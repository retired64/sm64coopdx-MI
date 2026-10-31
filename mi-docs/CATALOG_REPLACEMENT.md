# Actualización individual y recuperación MI

2026-10-30. Implementación local posterior a `180dfe28c`. Sustituye las notas
históricas que describían Actualizar deshabilitado. Actualizar todos, asociación
de instalaciones previas y formatos RAR/7z siguen fuera de esta etapa.

## Flujo del usuario

Buscar novedades actualiza el catálogo y compara recibos MI. El filtro local
Actualizaciones lleva al detalle del candidato, donde se muestra instalada ->
disponible y Actualizar. Se reutilizan selección, progreso real, cancelación y
resultado del instalador. No hay descarga de comprimidos como producto final.
La activación y Refrescar continúan en la pantalla original del juego.

El candidato copia también los IDs instalados de versión/archivo. El motor copia
esos IDs, el destino y la fuente nueva antes de iniciar el worker; no conserva
widgets ni punteros a otro panel. La selección de versión/rol se valida mediante
mi_updates_compare: IDs opacos, fechas y catálogo validado; el servicio de escritura
no consulta otra fuente ni ordena etiquetas para decidir si una versión es posterior.

Al abrir el catálogo se comprueban y recuperan diarios locales en segundo plano,
sin descargar mods. Mientras tanto se bloquean acciones sobre archivos y el retorno
a Mods, para no refrescar durante un movimiento. Fuera de esa recuperación local,
la red no impide Volver; destruir la UI no espera un join. Si la recuperación falla, se conserva
la evidencia y Reintentar vuelve a solicitarla. Se permite leer los detalles, pero
su botón de instalación/actualización queda bloqueado con la causa; volver al
catálogo y reintentar permite comprobar otra vez antes de abrir una acción nueva. Una comprobación sin diarios no
crea carpetas. No se añade un hook de arranque al juego ni una recuperación antes
de su primer escaneo local: tras un cierre en plena entrega, entrar al catálogo
recupera antes de volver a Mods y pulsar Refrescar. No se realizan actualizaciones
nuevas automáticamente al arrancar ni al entrar a partidas.

## Propiedad y validación

Se exige un recibo schema 1 que coincida con mod/versión/archivo/destino y con el
hash usado para su nombre. Se revalidan todos los archivos, tamaños y SHA-256,
enlaces, inventario y asociación única bajo el lock del instalador, antes de la
descarga y otra vez antes de mover la instalación. Un cambio local durante la
descarga se conserva y detiene la actualización. Un candidato visible no certifica
integridad: esta se comprueba al actuar, sin recalcular hashes al dibujar tarjetas.

El listado de recibos está acotado a 4096 entradas, 4 MiB por recibo y 32 MiB
agregados. Un recibo malformado o una asociación ambigua bloquea el reemplazo;
no se adivina su propietario ni se borra metadata para forzar el éxito.

Se conserva el destino instalado aunque el ZIP, su wrapper o el Lua remoto cambie
de nombre. Se sustituye la unidad completa; archivos retirados de la nueva versión
no quedan mezclados con ella. Se admite carpeta -> carpeta y archivo -> archivo;
cambiar el tipo requiere resolución explícita y se rechaza en esta etapa.

Los recibos nuevos añaden el campo opcional directories manteniendo schema 1,
para registrar también carpetas vacías propias. Los lectores antiguos ignoran ese
campo. Los recibos previos siguen usando sus archivos y los directorios ascendientes
como evidencia de propiedad. Una carpeta vacía extra cuya propiedad no esté
registrada se conserva y bloquea la sustitución; no se borra por suponer que es ajena
a la instalación. No se migra ni modifica un recibo antiguo al consultarlo.

## Transacción

Se reutilizan transporte HTTPS/TLS, límites, hash, validación Lua/ZIP y extracción
en un directorio temporal único. La versión instalada permanece intacta durante
esa preparación. El backup es un rename, no una copia adicional del mod.

Diarios de actualización: schema 2, kind update, en mi/transactions; temporales
update-XXXXXX separados de los stage-XXXXXX de instalación inicial. El diario
incluye recibos anterior/nuevo, identidades, destino, candidato e inodos/dispositivos
de unidades y del directorio privado. Su tamaño total se limita a 4 MiB antes de
mover el original, para no crear un diario que el lector no pueda recuperar.

| Estado persistido | Interpretación de recuperación |
|---|---|
| prepared | No hay confirmación: conservar/restaurar la anterior y retirar solamente el recibo nuevo exacto de esa transacción |
| committed | Nueva versión y recibo comprobados: finalizar la retirada del recibo anterior y del respaldo |
| cleaning | Limpieza privada ya autorizada: puede continuar si el respaldo se eliminó parcialmente |
| rolled_back | Versión anterior ya restaurada: terminar limpieza privada sin reinterpretar el cambio como éxito |

Antes de entregar se escribe prepared. La versión anterior pasa a backup y la
nueva al destino mediante RENAME_NOREPLACE; cada movimiento individual es atómico,
pero la secuencia completa no es un rename atómico único. Una interrupción entre
movimientos puede dejar temporalmente el destino ausente hasta recuperar el diario.
Nunca se reemplaza un destino que un tercero haya creado mientras tanto.

Después se comprueban destino y respaldo, se publica el recibo nuevo y se escribe
committed. Solo entonces se retira el recibo viejo. La intención de limpieza se
persiste antes de eliminar recursivamente temporales, haciendo esa limpieza
reanudable. El éxito se publica al terminar y liberar exclusión, no al descargar.

Si algo falla antes de committed, se intenta restaurar la anterior dentro del
mismo lock. La UI indica restauración con progreso indeterminado y luego un fallo
que explica que se conservó la anterior. Si el rollback encuentra archivos
editados, destinos ajenos o metadata inconsistente, no los elimina: deja backup,
diario y unidades disponibles y muestra recuperación pendiente. Si el commit ya
está confirmado, una recuperación termina la nueva versión en lugar de deshacerla.

Una repetición tras recuperar un commit reconoce el recibo nuevo y no descarga
ni crea una segunda instalación. Cancelar funciona durante comprobación/descarga/
preparación; desde commit o rollback se bloquea para no interrumpir el cambio a
mitad. Cerrar la UI libera su referencia y la tarea conserva la suya hasta terminar.

Se sincronizan archivos y, cuando está soportado, directorios. EINVAL/ENOTSUP en
fsync de directorio conservan compatibilidad Android, pero no acreditan durabilidad
frente a corte eléctrico. Los bloqueos excluyen instaladores MI cooperativos;
no son una barrera contra otro proceso que altere archivos en cualquier instante.
Los chequeos de hash/inodo permiten detener recuperaciones inseguras sin sobrescribir.

## Estado e integración

- mi_install.cpp/.h: actualización explícita y recuperación sin descarga, diario,
  validación, publicación terminal y revisión global compartidas con instalación.
- mi_updates.cpp/.h: candidato con identidades instaladas exactas, copiado al detalle.
- mi_catalog_panel.c: Actualizar, restauración, cancelación, recuperación al abrir,
  bloqueo contextual y reintento; conserva propietario por panel y callbacks nativos.
- English.ini, Spanish.ini y Portuguese.ini: claves nuevas con fallback compilado.
- tests/mi/maintenance_cli.cpp: wrappers de fallos solo en el ejecutable de pruebas.
- tools/mi/test_replacement.py: ciclo conectado con reinicio de procesos.
- tools/mi/test_maintenance.py: utilidades compartidas de compilación/fixtures y
  regresiones de instalación inicial. El juego no incluye el harness ni sus fallos.
- Workflow MI manual: incorpora la nueva suite; no se inicia una ejecución remota.

La revisión aumenta al iniciar/terminar cada operación aceptada, incluida recuperación.
El panel invalida candidatos al cambiar esa revisión o el catálogo; los detalles
conservan copias y el backend revalida recibos al actuar. No hay caché persistente
de resultados de actualización ni otra fuente de verdad sobre operaciones activas.

## Validación y pendientes

```sh
python tools/mi/test_replacement.py --bundled-curl
python tools/mi/test_replacement.py --bundled-curl --storage-root /sdcard/Download
python tools/mi/test_maintenance.py --bundled-curl
python tools/mi/test_install.py --bundled-curl
python tools/mi/test_updates.py
```

Las raíces siempre son temporales y no se modifican mods reales. La suite de
reemplazo cubre instalación previa real, archivos retirados, nombres distintos,
Lua, actualizaciones sucesivas, carpetas vacías, recibos legacy, cambios locales,
ambigüedad, límites, cancelación/close, fallos de entrega/publicación, interrupciones
antes/después de commit, rollback fallido, limpieza parcial, destinos concurrentes,
diarios alterados y preservación de evidencia. Se conservan las suites anteriores.

Los resultados definitivos y APK se registran en el checklist al finalizar.
Pendientes físicos: UI/táctil/mando/escala/idiomas, cancelar, minimizar/cerrar/reabrir,
volver a Mods/Refrescar y activar. También quedan pruebas con disco realmente lleno,
corte eléctrico y Gradle/CI remoto. Los fallos ENOSPC y salidas de proceso del
harness no sustituyen esas comprobaciones.


## Resultado de la validación local

- Reemplazo: 50 escenarios correctos internos; 49 correctos en Download y uno
  omitido porque ese filesystem no permite enlaces simbólicos (errno 13). Ese
  caso sí pasó internamente. Raíces temporales, sin tocar mods del usuario.
- Suites de instalación inicial/mantenimiento, comparación de versiones, catálogo,
  idiomas antiguos/fallback, viewport y miniaturas correctas con curl incluido.
- Catálogo público HTTPS: 1307 mods; instalación HTTPS de un Lua público en raíz
  temporal de Download correcta. Estas pruebas verifican transporte compartido,
  no sustituyen la prueba física de actualizar un mod del usuario.
- Generador --check, diff --check y enlaces Markdown locales correctos.
- Build Makefile Termux correcto; firma, alineación 4 KiB, CRC y coincidencia de
  libmain empaquetada con la compilada correctos. Las APIs nuevas aparecen en
  los símbolos del libmain.so. Gradle y Actions no se ejecutaron.
- APK: /sdcard/Download/sm64coopdx-MI-updates-local.apk, 124369209 bytes.
- SHA-256: 4daefaf42f9a99b1b2c02b0569f3d902480b85c6fcb99c76ea6cbdac3590be3d.

No hubo commit/push. Pendiente aceptación física del nuevo flujo. Un APK anterior
puede no entender diarios schema 2 pendientes: conservar evidencia y recuperar
con una versión MI que los soporte antes de considerar un downgrade.
