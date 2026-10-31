# MI Android APK: workflow manual sin ROM

Workflow: [.github/workflows/mi-actions-build.yml](../.github/workflows/mi-actions-build.yml).
Fecha: 2026-10-29. Alcance: APK ARM64 de prueba como artefacto; no release.

## Flujo implementado

1. Checkout limpio del commit de la rama seleccionada, sin credenciales persistidas.
2. Java 17 y dependencias Ubuntu de herramientas/compilación.
3. Tests MI de parser/idiomas/geometría, HTTP local y puerta de validación del APK.
4. SDK/NDK fijados, sin descarga ni uso de baserom o secrets.
5. make del host prepara fuentes/headers generados y sus dependencias. También
   construye el ejecutable del host, que no se sube ni se ejecuta como juego.
6. Gradle/NDK compila y empaqueta ARM64 con controles táctiles.
7. zipalign y apksigner producen dist/sm64coopdx-MI-arm64.apk con firma de prueba.
8. Verifica CRC, ABI, recursos, bibliotecas y símbolos MI, lector ROM y SHA-256.
9. Sube APK, build-info.json y SHA256SUMS.txt. Retención de siete días.

Solo workflow_dispatch; no push/PR automáticos, ni actualizaciones de upstream,
ni publicación de tags/assets/releases. Concurrencia por rama cancela builds
anteriores al lanzar otro de la misma rama. Timeout: 90 minutos.

## Herramientas fijadas y parámetros

- Runner ubuntu-22.04; JDK Temurin 17.
- Gradle 8.9 (wrapper existente), AGP 8.7.2 (existente).
- compileSdk 33, Build Tools 34.0.0 y NDK 27.0.12077973.
- Acciones oficiales checkout v6, setup-java v5 y upload-artifact v7 fijadas por SHA.
- Gradle acepta -PmiAbis=arm64-v8a y -PmiJobs=2. Sin propiedad conserva ambos ABIs
  heredados; paralelismo predeterminado dos y validación de entradas.
- Android.mk explicita C++17. No cambia el build Termux ni la identidad del APK.

Compatibilidad AGP:
https://developer.android.com/build/releases/agp-8-7-0-release-notes
Los paquetes apt y actualizaciones menores del JDK/runner no se fijan por digest;
este workflow no promete reproducibilidad binaria bit a bit.

## Por qué no usa ROM

Esta rama valida una ROM al arrancar con main_rom_handler y carga recursos con
rom_assets_load. Las macros ROM_ASSET_LOAD reservan buffers que se llenan desde
la ROM del usuario. El workflow no invoca extract_assets.py, no descarga ROM y
comprueba ausencia de baserom.us.z64 antes y después de make. El APK no debe
contener archivos .z64/.n64/.v64 ni material privado de firma.

El verificador no es un detector universal de contenido protegido o ROMs
renombradas. Comprueba las rutas y contratos del empaquetado diseñado; la revisión
del código sigue siendo necesaria. El APK sí incluye recursos personalizados,
idiomas, mods/packs auxiliares del port y bibliotecas. Seleccionar ROM en una
instalación nueva sigue siendo responsabilidad del usuario.

## Corrección del empaquetado Gradle

copyGameAssets buscaba lang/mods/dynos/palettes dentro de platform/, pero las
fuentes viven en la raíz del repositorio. Ahora usa el directorio padre del
proyecto Gradle y falla si falta una carpeta. Antes podía omitirla silenciosamente.

verify_ci_apk.py compara las cuatro bibliotecas incluidas con las del directorio
stripped_native_libs de AGP, no con objetos previos al strip ni el ejecutable del
host. Los símbolos exportados del libmain.so se comprueban con llvm-nm. Los INIs
EN/ES/PT deben incluir MI_CATALOG y la búsqueda traducida. Se comprueban las cuatro
carpetas de recursos y se rechazan ABI inesperados o entradas ZIP duplicadas.

La ruta stripped_native_libs es parte del contrato de la versión AGP fijada:
revisarla al actualizar el plugin; no omitir la comparación para hacer pasar CI.

## Firma e identidad

Se conserva com.maniscat2.sm64coopdx, versionCode 42 y versionName 1.5.1. Se usa
la clave/certificado públicos heredados del repositorio, solo como firma de prueba.
No se generan nuevas claves privadas ni se usa una identidad exclusiva de MI.
Antes de publicación propia debe diseñarse identidad/versionado y continuidad de
actualizaciones. Firma válida no equivale a permiso de distribución ni validación
funcional. El artefacto de un repo público es accesible a lectores del repo.

## Publicar y ejecutar manualmente

El workflow y sus archivos deben estar committed y subidos a mi-main antes de
estos pasos. El fork tenía android como rama predeterminada al consultarlo.
workflow_dispatch requiere el archivo en la rama predeterminada. No poner este
workflow en android solo para habilitar el botón, pues esa rama es nuestro espejo.

Tras revisar/publicar el commit, si se decide usar mi-main como rama predeterminada:

```sh
gh repo edit retired64/sm64coopdx-MI --default-branch mi-main
gh workflow run mi-actions-build.yml --repo retired64/sm64coopdx-MI --ref mi-main
```

La primera orden cambia configuración remota; no se ejecutó automáticamente durante
esta preparación. No hace falta baserom ni configurar ningún secret.
Consultar ejecución y descargar artefactos:

```sh
gh run list --repo retired64/sm64coopdx-MI --workflow mi-actions-build.yml --branch mi-main --limit 5
```

Con el ID que muestre esa lista (sustituir RUN_ID):

```sh
gh run watch RUN_ID --repo retired64/sm64coopdx-MI --exit-status
gh run download RUN_ID --repo retired64/sm64coopdx-MI --dir ./mi-artifacts
```

Para fallo: gh run view RUN_ID --repo retired64/sm64coopdx-MI --log-failed.
El APK se sube solo si todo el job previo pasa. No subir el workspace, ROM, objetos,
caches o logs completos como alternativa cuando falle la verificación.

Referencia del disparador:
https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow

## Validación registrada y límites

Estado actual: el [run 41013511475](https://github.com/retired64/sm64coopdx-MI/actions/runs/41013511475)
completó el build y la subida del artefacto. Ver el
[registro de primera compilación correcta](ci/2026-10-30-first-success.md). Las
notas iniciales siguientes conservan el contexto previo a esa ejecución.

- actionlint 1.7.12: correcto; shellcheck y pyflakes desactivados porque no estaban
  instalados en el entorno local. Expresiones, sintaxis y estructura de Actions validadas.
- test_catalog.py y test_requests.py: correctos en el entorno local. La prueba HTTP
  aquí usa curl del host, no acredita TLS del curl incluido en Android.
- apk_test.py: acepta el contrato y rechaza recursos/bibliotecas ausentes, ABI no
  esperado, ROM/clave empaquetadas y discrepancia con el libmain compilado.
- verify_ci_apk.py contra el APK ARM64 histórico local: correcto. Esto valida el
  verificador con un artefacto real, no demuestra un nuevo build Gradle.
- Enlaces, sintaxis Python y git diff --check: correctos.

Pendiente: primera ejecución Ubuntu/Gradle en Actions, APK descargado y prueba
física en instalación nueva con selector de ROM, idiomas y catálogo MI. El entorno
local es Termux ARM64; no se ejecutó el toolchain Linux x86_64 del runner aquí.
No se recompiló el APK Termux porque el cambio de build afecta Android.mk/Gradle
para NDK y no el empaquetado Termux del Makefile. No se afirma build Gradle exitoso
hasta comprobar una ejecución real. Cualquier fallo del primer run debe resolverse
con sus logs y repetir validación; no saltar pasos para producir un artefacto verde.

## Verificación de sincronización de estado

No cambia estado runtime, caché ni workers del juego. Gradle lee fuentes de recursos
de la raíz y genera assets; NDK genera bibliotecas; verificador lee APK y bibliotecas
stripped; upload-artifact publica únicamente dist. No se agregan caches persistentes,
hilos ni Futures propios. Revisados Makefile preparatorio, Android.mk, Gradle,
idiomas, librerías y artefactos. Persisten las pruebas físicas pendientes.


## Corrección de primera ejecución — 2026-10-30

Run 41012336610 falló antes de compilar Android: :app no existía en el grafo
heredado, definido por settings.gradle raíz como :platform:app. El make del host
y las pruebas previas sí terminaron. Se añade platform/settings.gradle como
entrada standalone para -p platform, con módulo :app. El settings de raíz se
conserva; la ruta de recursos se deriva de projectDir del módulo y sirve para
ambas entradas. Se valida :app:tasks antes de la compilación del host.

El primer fallo no fue falta de ROM. La confirmación del build Android completo
se registrará tras la ejecución corregida, sin inferirla de una validación estática.


## Resultado posterior: ejecución completa correcta

Run 41013511475, commit e7d0ac815: job Android correcto en 8 min 48 s. Se completaron
Gradle/NDK, firma, comprobaciones del APK y subida del artefacto sin ROM ni secrets.
La descarga local se interrumpió a petición del usuario y no se reanudó. Por tanto,
no se afirma verificación local independiente ni instalación física del APK nuevo.

Evidencia, corrección y pendientes: [registro CI](ci/2026-10-30-first-success.md).
Los avisos heredados sobre recetas duplicadas del Makefile permanecen y no fueron
la causa del fallo de selección de módulo.


### Nuevas suites de consulta e imágenes

El job de contratos incluye test_updates.py y test_thumbnails.py: fixtures y HTTP
locales, sin ROM ni catálogos privados. Se mantiene el workflow manual de build MI;
no se ejecutó Actions por estos cambios. La opción --live es una validación local
separada y no es requisito de red de esas suites en CI.


### Puerta de mantenimiento MI — 2026-10-30

La validación manual incluye test_install.py y test_maintenance.py además de las
suites previas. La segunda enlaza parser, consulta e instalador reales y usa HTTP
loopback, raíces temporales e interrupciones de procesos de prueba. Los wrappers
de fallos están únicamente en el ejecutable de tests: no se añaden al APK.
No requiere ROM ni secretos. Incorporar estas líneas al workflow no ejecuta Actions
ni acredita su resultado en Ubuntu; se verificó localmente en Termux ARM64.
Detalle: [MI_MAINTENANCE_TESTS.md](MI_MAINTENANCE_TESTS.md).


### Suite de reemplazo individual — 2026-10-30

La puerta manual añade python3 tools/mi/test_replacement.py. Usa raíces y servidor
HTTP temporales, enlaza módulos reales y simula fallos solo en su ejecutable de
tests. No requiere ROM ni acceso al catálogo público. Configurar el paso no
ejecuta Actions: la comprobación Ubuntu/Gradle remota sigue pendiente.


## Versionado MI y catálogo incluido

Regla vigente: [VERSIONING_AND_RELEASES.md](VERSIONING_AND_RELEASES.md). Conservar versionCode y firma upstream;
personalizar solo versionName. Catálogo incluido actualizado: 1,311 mods.


## Workflow manual de releases

Preparado: [RELEASE_WORKFLOW.md](RELEASE_WORKFLOW.md). Reutiliza el build verificado
y publica solo al ejecutarlo explícitamente. Ejecución remota pendiente.
