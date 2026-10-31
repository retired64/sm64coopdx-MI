# Instalación nativa del catálogo MI

Implementación local: 2026-10-30. Se prepara para pruebas en teléfono, sin commit,
push, Actions ni publicación. Complementa [CATALOG_REMOTE.md](CATALOG_REMOTE.md).
El usuario confirmó que el APK CI anterior funcionó; esa confirmación no prueba
este instalador nuevo.

## Flujo disponible

Detalles copia los archivos y sus identidades desde el catálogo. Selecciona un
archivo recomendado soportado inicialmente, permite recorrer archivos/versiones
con botones y muestra versión y nombre antes de instalar. No toma el título como
identidad. Archivos no soportados siguen visibles con explicación.

Instalar inicia una sola operación para todo el proceso. La UI consulta snapshots
mediante mutex; el trabajador detached conserva su propia referencia, nunca widgets.
Una segunda operación recibe un resultado ocupado, sin colisiones de temporales.

```text
preparando -> descargando -> extrayendo (ZIP) -> verificando -> finalizando
                                                    -> completado / error
antes de finalizando: cancelar -> cancelando -> cancelado
```

El progreso corresponde a la etapa indicada, no a una estimación global. Descarga
al 100 % no es éxito. Se reutiliza DjuiProgressBar, el componente de la carga inicial,
con valores propiedad de la UI y una barra animada para total desconocido. No se
usan sus globals ni la pantalla de carga del juego.

Volver durante una operación solicita cancelación y mantiene la pantalla hasta
confirmación terminal. Durante entrega atómica no se cancela. Destruir el panel
libera su referencia sin esperar red; el trabajador limpia y termina por su cuenta.
Cerrar el proceso abruptamente requiere reconciliación posterior, no es cancelación
confirmada. No se implementa un servicio Android persistente en segundo plano.

Después del éxito: volver a Mods y pulsar **Refrescar**, usando el control original.
No se llama al refresco desde el trabajador ni se toca la activación. El juego
sigue leyendo metadatos, seleccionando mods, aplicando reglas y mostrando errores
Lua. Instalar no certifica que el script funcione en una partida.

## Alcance de formatos

- Lua suelto: entrega directa `mods/nombre.lua`.
- ZIP con `main.lua` en raíz: entrega una carpeta cuyo nombre deriva del ZIP.
- ZIP con una única cadena de carpetas envolventes: encuentra el directorio con
  `main.lua`, sin conservar wrappers innecesarios.
- ZIP que contiene únicamente un Lua: entrega ese Lua directamente.
- `.luac` solo como recurso dentro de un mod con `main.lua`.
- No instala RAR, 7z, Lua compilado suelto, paquetes ambiguos de varios mods ni
  archivos marcados `requires_primary`. RAR/7z necesitan otra etapa nativa probada.
- La detección de formato/estructura es conservadora; no ejecuta Lua para validar.
  Comprueba contenido básico, ZIP/CRC y metadatos de transporte disponibles.

No reemplaza destinos existentes, aunque correspondan a otra versión del mismo
mod. Actualizar, asociar mods previos y Update all permanecen pendientes. Repetir
la misma instalación con su recibo intacto verifica archivos y devuelve éxito sin
volver a descargarlos. Archivos modificados o ausentes no se declaran originales.

## Transporte y límites

HTTPS con certificado/hostname verificados, certificados Android compartidos con
el catálogo; HTTP loopback está reservado a pruebas. Cinco redirecciones máximo,
conexión 15 s, total 30 min, corte por baja velocidad 128 bytes/s durante 30 s.
Mirror disponible marcado fallback se intenta ante fallo de red/HTTP/TLS de la
fuente primaria. No se parsean páginas HTML de proveedores ni se resuelven enlaces
web indirectos: el catálogo debe entregar una URL descargable.

No hay reintentos automáticos indefinidos. El usuario puede reintentar. El tamaño
y SHA-256 del catálogo se verifican cuando existen; sin hash remoto, los hashes
locales sirven para identificar contenido instalado, no para autenticar al autor.
Se detecta truncamiento informado por HTTP. Nunca se conserva el comprimido como
resultado ofrecido al usuario.

Límites: descarga 512 MiB; archivo individual 256 MiB; expansión total 1 GiB;
4096 entradas ZIP; profundidad 24; rutas 512 bytes; componentes 240 bytes;
ratio máximo 200 para entradas expandidas de más de 2 MiB. Son límites técnicos
iniciales, no promesas de espacio suficiente. Los errores de escritura se capturan.

Se rechazan rutas absolutas, traversal, barras invertidas, controles, enlaces,
archivos especiales, cifrado, métodos ZIP no soportados y duplicados de ruta
incluyendo diferencias ASCII de mayúsculas. CRC se valida durante extracción.
Se ignoran recursos auxiliares `__MACOSX` y `.DS_Store` después de validar rutas.

## Almacenamiento, recibos y recuperación

La raíz es la misma `fs_writepath` del juego; no se codifica el package Android.

| Ruta relativa | Propósito |
|---|---|
| `mods/` | Resultado final reconocido por el juego |
| `mi/stage-XXXXXX/` | Descarga y extracción aisladas fuera del escáner |
| `mi/install.lock` | Exclusión entre procesos mediante fcntl F_SETLK |
| `mi/receipts/<clave>.json` | Recibo schema 1 |
| `mi/transactions/<stage>.json` | Diario de entrega pendiente |

Clave: SHA-256 de `mod_id + salto de línea + version_id + salto de línea + file_id`.
Recibo: IDs, versión, destino, fecha y manifiesto de rutas/tamaños/SHA-256.
Los IDs no se truncan para instalar: fuentes que exceden las capacidades del DTO
quedan no soportadas. El catálogo limita la proyección a 20000 archivos.

Se prepara y verifica todo fuera de mods. Antes de entregar se escribe el diario
con candidato, inode/device, destino y recibo esperado. La entrega usa renameat2
RENAME_NOREPLACE; si el filesystem no lo soporta, falla sin reemplazar datos.
No hay fallback inseguro a rename con sobrescritura. Ambas carpetas deben residir
en el mismo filesystem. Esta capacidad debe probarse en el almacenamiento Android
real del usuario, además del entorno temporal de las pruebas.

Éxito requiere recibo publicado después de la entrega. Si falla el recibo, se
conserva el diario y se informa recuperación pendiente; no se declara éxito.
La siguiente instalación, bajo lock, reconcilia primero los diarios: descarta
preparaciones no entregadas o completa el recibo de una entrega cuyo inode y
manifiesto coinciden. Un diario corrupto o un destino cambiado detiene la operación
y conserva evidencia. No elimina mods del usuario para forzar la recuperación.
También elimina stages propios abandonados sin diario bajo exclusión.

Los archivos persistidos usan temporal, fsync del archivo y rename. No se garantiza
resistencia a pérdida eléctrica sin fsync de directorios, ni recuperación frente
al usuario moviendo archivos externamente. Errores persistentes de recuperación
requieren diagnóstico conservando diario, recibos y mods. No recomendar borrar
`mi/` indiscriminadamente: contiene propiedad y asociaciones futuras.

## Archivos y mantenimiento

- `mi_install.h/.cpp`: API C, trabajador, límites, descarga, extracción, entrega,
  recibos y recuperación. `mi_sha256.h`: hash incremental con vectores de prueba.
- `mi_network.h`: inicialización curl y confianza Android compartida; revisar
  tanto catálogo como instalador al cambiar certificados o curl.
- `mi_catalog.h/.cpp`: DTO inmutable de fuentes seleccionables y sus identidades.
- `mi_catalog_panel.c`: ownership por detalle, selección, polling, cancelación,
  mensajes y barras. Mantiene stack/render/scroll nativos.
- INIs EN/ES/PT y strings compiladas: estado, acciones y errores localizados.
- `tests/mi/install_cli.cpp`, `tools/mi/test_install.py`: pruebas aisladas.
- `tests/mi/catalog_test.cpp`: contrato de fuentes soportadas y límites de copia.

Makefile y Android.mk enumeran automáticamente los nuevos .cpp en mod_integrations;
comprobar link y libmain empaquetada en cada sincronización. El escáner, importador,
activación y globals de la carga inicial no se modifican. El catálogo JSON es el
contrato externo de datos; su producción no forma parte del fork.

## Validación y prueba física pendiente

```sh
python tools/mi/embed_catalog.py --check
python tools/mi/test_catalog.py
python tools/mi/test_requests.py --bundled-curl --live
python tools/mi/test_install.py --bundled-curl --live
make -j$(nproc)
git diff --check
```

La opción --live instaló noclip.lua desde el catálogo real por HTTPS, usando el
curl incluido y una raíz temporal. La suite del instalador cubre vectores SHA, Lua/ZIP/wrappers, manifiestos y recibos,
repetición, traversal/enlaces/duplicados, estructura ambigua, HTML/corrupción,
404/truncamiento, hash, destino existente, cancelación, ocupado, modificación local
y recuperación antes/después de entrega. Usa raíces temporales, nunca mods reales.

En el teléfono falta probar: Lua y ZIP reales del catálogo, detectar con Refrescar,
activar mediante controles originales, cancelar y reintentar, pérdida de red,
espacio insuficiente, minimizar/reabrir, cierre abrupto, filesystem externo,
escala grande, mando y EN/ES/PT. Compilar y probar el backend no acredita esta matriz.
El APK y resultados locales se registran en el checklist al finalizar la validación.


## Corrección visual posterior a prueba física — 2026-10-30

Las tres capturas 09:22–09:23 mostraban Archivo siguiente sin su pareja e Instalar
invisible incluso con un archivo soportado (Flood). Causa: right_create fija
HALIGN_RIGHT, mientras mi_place proporcionaba x=0.515 desde la izquierda.
DJUI calcula right como ancho - anchoBotón - x; por tanto ambos botones acababan
en x=0. Archivo siguiente tapaba Anterior y Volver, creado después, tapaba Instalar.
Se fija HALIGN_LEFT explícitamente para los dos controles derechos del detalle.
No cambia la activación, soporte de formatos, recibos ni el diseño del catálogo.
También se sustituye el separador Unicode no disponible en la fuente por |.
Comprobar físicamente los dos pares visibles, selección y pulsación por separado,
con y sin scroll y a escala grande. Capturas identifican la causa; el APK corregido
requiere una nueva comprobación física. Sin commit, push ni Actions.


## Corrección de instalación/progreso sobre almacenamiento Android

2026-10-30: el usuario informó carga breve sin resultado y posteriores mensajes
«otra instalación». Inspección de su raíz real: install.lock existía, pero no había
recibos ni stages. La prueba no destructiva del lock devolvió ENOSYS (38) para
flock en /sdcard. El código anterior convertía cualquier error de flock en BUSY,
por lo que fallaba antes de iniciar la descarga y mostraba una causa incorrecta.

Se sustituye flock por locks POSIX fcntl F_SETLK, comprobados en este filesystem.
Solo EACCES/EAGAIN significan ocupado; otros errores son almacenamiento. El
kernel libera el lock al cerrar descriptor/morir proceso; la exclusión en memoria
protege threads del mismo proceso. No se ignora el fallo del lock ni se borra un
archivo de lock para simular desbloqueo. Éxito se publica después de terminar
limpieza y liberar exclusión, evitando que una operación completa siga bloqueando.

La comparación con ModInstallWorker/ModDownloadWorker de SM64CDPY conserva el
contrato: bytes reales, estados separados y éxito después de escritura verificada.
El pie de detalles ahora reserva 200 unidades lógicas: botones, barra y estado
permanente. Muestra archivo, bytes/total KiB durante descarga y destino mods/ al
terminar. Descripción y selector siguen desplazables, el estado ya no desaparece
al desplazarlos. Extracción publica bytes dentro de cada archivo, no solo al final.
Verificación sin denominador calculado usa barra indeterminada. No hay demoras
artificiales para fingir trabajo. Un Lua pequeño puede terminar rápido de verdad.
Fallo de red sin respuesta HTTP se comunica como red, no HTTP 0.

Pruebas: test_install.py --bundled-curl --live --storage-root /sdcard/Download
correcto en una raíz temporal nueva: Lua noclip.lua real por HTTPS, ZIP/wrappers,
recibos, errores, recuperación, cancelación, transferencia por bloques con bytes
observados y exclusión entre procesos seguida de liberación del lock. Ejecutables
en temp interno, datos en carpeta temporal externa; no se tocan mods del usuario.
La variante sin --live verifica la versión final de los callbacks. --storage-root
no debe apuntar a mods reales: la herramienta crea y elimina su propia subcarpeta.
Nueva prueba física pendiente: observar progreso/resultado, comprobar el destino,
volver a Mods y usar Refrescar. El escáner y la activación del juego no cambiaron.


## Confirmación del usuario y autorización de publicación — 2026-10-30

El usuario confirmó que el instalador corregido funciona y autorizó commit y push
hacia mi-main. Corresponde al APK SHA-256
11440a7dd784fa178f57a7fe0ad3e9faf4401c305f3f850c745d582374cbbc61.
Esta confirmación general no acredita todos los escenarios de la matriz física;
los casos no informados y la ruta Gradle con el instalador siguen pendientes.
Las notas anteriores de «sin commit/push» describen el desarrollo local previo.
No se autoriza ni ejecuta una release, subida de APK ni Actions por este registro.


### Presentación compacta — 2026-10-30

El botón Instalar se sitúa debajo de categoría en la cabecera del mod. El selector
usa flechas pequeñas junto a los datos del archivo; no hay flechas para un único
archivo. El pie fijo conserva Volver/Cancelar y aparece el estado/progreso cuando
corresponde, sin reservar permanentemente una franja para el mensaje de listo.
Un fallo al crear la operación se conserva en la proyección UI hasta reintentar
o elegir otro archivo. No cambia la máquina de estados ni la escritura nativa.
Ver [CATALOG_ACCESSIBILITY.md](CATALOG_ACCESSIBILITY.md) para geometría y pruebas.


### Estados terminales después de liberar recursos — 2026-10-30

La suite de mantenimiento detectó que FAILED/CANCELLED podían publicarse desde
catch antes de limpiar temporales y liberar la exclusión. Un consumidor podía
reintentar al ver el fallo y recibir BUSY por la operación anterior aún finalizando.
Se calcula el resultado en variables locales; después de salir del lock del
filesystem y completar limpieza se incrementa la revisión, se libera busy y se
publica el resultado bajo mutex. Éxito conserva ese mismo orden. No hay espera
artificial ni cambio de formatos/recibos.

Un handle rechazado con BUSY representa otra operación activa y no promete que
el instalador global esté libre. Los tests de una operación aceptada verifican
liberación inmediata al observar su terminal, sin sleep de gracia.
Pruebas conectadas y limitaciones: [MI_MAINTENANCE_TESTS.md](MI_MAINTENANCE_TESTS.md).


### Actualización individual implementada — 2026-10-30

El motor añade sustitución explícita con recibo previo, respaldo, diario y rollback;
la instalación inicial conserva su política de no sobrescritura. Los recibos
nuevos registran directories como extensión opcional de schema 1. Recuperación
al abrir el catálogo no descarga archivos. Contrato vigente y limitaciones:
[CATALOG_REPLACEMENT.md](CATALOG_REPLACEMENT.md). Las notas anteriores que dejaban
pendiente todo reemplazo corresponden a la etapa previa.
