# Pruebas del ciclo de mantenimiento MI

Fecha: 2026-10-30. Base Git: `180dfe28c`; cambios locales posteriores de pruebas y
publicación de estados terminales. El reemplazo individual y Actualizar todos
siguen pendientes. Este documento acredita los contratos existentes y sus límites,
no la actualización ni el rollback de una versión ya instalada.

## Qué conecta la suite

`tools/mi/test_maintenance.py` compila `tests/mi/maintenance_cli.cpp` con el parser,
instalador y consulta de actualizaciones reales. No sustituye busy/revisión por
stubs. Usa ZIP/Lua sintéticos servidos por HTTP loopback y raíces temporales.
El Lua de ejemplo omite su versión para comprobar que identidad/fechas provienen
del catálogo y del recibo, no del texto del script. El JSON es el contrato público;
no se necesitan datos de producción para reproducir los casos.

La suite contrasta archivos, directorios, enlaces y bytes de mods/recibos antes y
después de fallos. Una consulta debe dejar igual todo el árbol, no solo devolver
un resultado esperado. Comprueba hashes/tamaños reales del recibo recién creado.

El harness usa `--wrap=syscall` y `--wrap=rename` al enlazar únicamente el ejecutable
de pruebas Linux/Android. Interrumpe con `_exit(73)` en tres puntos o devuelve
ENOSPC en una publicación de metadatos. No se añaden hooks, variables de entorno
ni modos de fallo al motor del juego. La suite aborta si el syscall interceptado
no es el RENAME_NOREPLACE que espera: revisar el harness si cambia esa implementación.

## Cobertura ejecutada

| Escenario | Garantía observada |
|---|---|
| Instalación, recibo y consulta conectados | Candidato correcto por IDs/fechas; manifiesto coincide con bytes instalados |
| Repetir instalación e IDs iguales con remoto diferente | Mantiene versión local y no realiza una descarga nueva |
| Versión nueva con destino existente | Instalador ordinario rechaza sobrescribir; conserva versión y recibo anteriores |
| Archivo de otra versión con nombre diferente | Puede crear otra instalación; asociaciones duplicadas quedan inciertas |
| Edición local de igual tamaño | Hash la detecta al repetir instalación; no sobrescribe |
| Archivo local añadido | Manifiesto impide tratar la instalación como intacta; conserva el añadido |
| Archivo ausente | Reinstalación rechazada; consulta lo deja sin comprobar |
| Recibo con identidad cambiada | No reutiliza una asociación que no coincide con la solicitud/nombre del recibo |
| Destino con traversal en recibo | No accede/escribe fuera de la unidad instalada |
| Enlace local | Rechazo sin modificar el destino del enlace, cuando el filesystem permite crearlo |
| HTTP 404 y ZIP corrupto | Fallo conserva instalación y recibos previos |
| Instalación real concurrente con consulta | Consulta devuelve ocupado; cerrar cancela sin join y permite otra instalación al terminar |
| Salida de proceso antes de entrega | Diario preservado; siguiente proceso descarta preparación sin afirmar instalación |
| Salida después de entrega, antes del recibo | Diario bloquea consulta; recuperación verifica contenido y publica recibo |
| Salida después de guardar recibo | Recuperación conserva recibo existente y limpia diario, sin duplicar instalación |
| Edición local después de salida abrupta | Recuperación se detiene conservando mod y evidencia |
| ENOSPC simulado publicando diario | No entrega archivos; limpia preparación y permite reintentar |
| ENOSPC simulado publicando recibo | No anuncia éxito; conserva entrega/diario y recupera al reintentar |
| Destino creado entre precomprobación y entrega | RENAME_NOREPLACE conserva el destino concurrente y no publica recibo propio |
| Publicación de estados terminales | Una operación aceptada observada como finalizada ya liberó exclusión y actualizó revisión |

La salida agrupa estos contratos en 20 casos internos. La prueba de publicación
terminal se ejecuta en cada operación aceptada que termina normalmente en el harness.
Un handle rechazado por BUSY no afirma que la operación original haya terminado.

## Regresión encontrada y corregida

La primera ejecución falló al intentar una versión nueva sobre un destino existente:
`mi_install_poll` ya devolvía FAILED mientras `mi_install_is_busy()` seguía true.
Los catch publicaban el fallo antes de limpiar stage y liberar exclusión. La UI
podía permitir un reintento prematuro y recibir «otra instalación en curso».

`mi_install.cpp` ahora calcula el resultado en variables locales y publica todos
los terminales después de la limpieza/liberación, bajo el mismo mutex del snapshot.
`tests/mi/install_cli.cpp` elimina el sleep de gracia de 50 ms y exige exclusión
liberada/revisión incrementada al observar el terminal. El harness conectado hace
la misma comprobación, por lo que esta regresión no queda oculta por esperar.
El esquema de recibos, las APIs C y la política de no sobrescritura no cambian.

## Ejecución reproducible

Desde la raíz del fork, sin ROM:

```sh
python tools/mi/test_maintenance.py --bundled-curl
python tools/mi/test_install.py --bundled-curl
python tools/mi/test_updates.py
```

En Termux, datos en un hijo temporal del almacenamiento Android, binarios internos:

```sh
python tools/mi/test_maintenance.py --bundled-curl --storage-root /sdcard/Download
```

`--storage-root` es el padre de una carpeta temporal nueva que la herramienta
elimina al terminar; no usar la carpeta real de mods como padre. `--report RUTA`
guarda un JSON al finalizar todos los contratos; elegir una ruta temporal fuera
del repositorio. Sin `--bundled-curl` enlaza curl del host para el runner Linux.
No ejecutar con Python optimizado (`-O`): las aserciones son parte de las pruebas.

El workflow manual MI incorpora test_install.py y test_maintenance.py. No se lanzó
Actions, ni se hizo commit/push/release durante esta validación local. El éxito en
Termux no certifica el runner Ubuntu ni Gradle.

## Evidencia local

- Termux ARM64, Clang 21.1.8 (Android API 24), Python 3.14.8, curl incluido.
- Interno: 20 casos correctos, cero omitidos; 61.25 s incluyendo compilación.
- Almacenamiento emulado Download: 19 correctos, uno omitido por prohibición de
  enlaces simbólicos (errno 13); 71.87 s incluyendo compilación.
- Los tiempos describen esta ejecución; no son un benchmark ni límite de rendimiento.
- Suites anteriores de instalación, consulta de versiones, catálogo/idiomas/geometría
  y peticiones/caché también pasaron. Generador --check y diff --check correctos.
- Instalador corregido comprobado además por HTTPS con un Lua del catálogo público
  en raíz temporal de Download; no se tocó la instalación del usuario.
- APK local Makefile compilado; firma, alineación 4 KiB, CRC ZIP y coincidencia de
  libmain empaquetada con la recién compilada correctos.
- APK: `/sdcard/Download/sm64coopdx-MI-maintenance-local.apk`, 123906361 bytes.
- SHA-256: `c89219309e537e145d5cb202d2f14a476bd82e39a754ea8fd47e472a271e2890`.

## Lo que sigue pendiente

1. Reemplazo de versiones, respaldo de la anterior y rollback propio: aún no existen.
2. Revalidar hashes, inventario y propiedad inmediatamente antes de un reemplazo.
   La consulta actual comprueba presencia; archivos editados/añadidos pueden seguir
   produciendo candidatos. No confundir eso con permiso para reemplazarlos.
3. Detectar revisiones binarias bajo IDs remotos iguales requiere evidencia remota
   comparable; la repetición actual conserva los bytes instalados.
4. Resolver cambios de carpeta y archivos retirados sin mezclar versiones ni borrar
   contenido ajeno. El instalador ordinario no debe usarse como sustituto de esa lógica.
5. Disco realmente lleno durante descarga/extracción/fsync: ENOSPC aquí se simula
   al publicar metadatos, no llenando el teléfono.
6. Cortes de energía/durabilidad de directorios: `_exit` prueba muerte de proceso
   sin destructores y liberación de locks, no pérdida de energía ni fsync de directorio.
7. Pruebas físicas en el juego: UI de progreso/error/reintento, cancelación, refresco
   y activación nativos, minimizar/cerrar/reabrir y controles táctiles/mando.
8. Gradle y ejecución remota del workflow con la nueva suite.

No crear una promesa de actualización segura completa a partir de esta evidencia.
El siguiente desarrollo es un reemplazo individual transaccional con pruebas
propias; Actualizar todos debe reutilizarlo después de validarlo.


## Estado posterior: reemplazo implementado localmente

2026-10-30: las pruebas de instalación descritas arriba se conservan; las referencias
a reemplazo aún inexistente describen la etapa anterior. La implementación y
suite nueva están en [CATALOG_REPLACEMENT.md](CATALOG_REPLACEMENT.md). Las
utilidades del harness se comparten con test_replacement.py. Se mantienen como
pendientes físicos disco lleno, corte eléctrico, juego y CI remoto.
