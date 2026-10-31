# Versionado y catálogo incluido

## Política permanente

- Mantener el applicationId, namespace, carpetas y firma heredados del upstream Android estable. No generar otra identidad de aplicación para MI.
- Conservar exactamente el versionCode de la versión upstream integrada. No calcular contadores Android propios para cada revisión MI.
- Personalizar únicamente versionName con `<versión upstream>-mi.<revisión>`. Primera referencia: `1.5.1-mi.1`, versionCode `42`.
- Mantener alineados platform/app/build.gradle y platform/app/src/main/AndroidManifest.xml: Gradle y la compilación local deben mostrar la misma versión.
- No modificar SM64COOPDX_VERSION, VERSION_NUMBER ni MINOR_VERSION_NUMBER del protocolo para introducir la etiqueta MI.
- Al sincronizar upstream, revisar ambos archivos Android, conservar su nuevo versionCode y actualizar la base del nombre visible. La revisión MI empieza en 1 para cada nueva base.
- El tag de una futura release será, por ejemplo, `v1.5.1-mi.1`. Publicarlo requiere autorización explícita; compilar no publica una release.
- Las revisiones MI de la misma base comparten versionCode. No usar ese entero para distinguirlas en un futuro actualizador propio: deberá considerar la etiqueta MI.
- Las claves heredadas son públicas: no constituyen una identidad privada exclusiva de MI. No cambiar firma, paquete o rutas durante una actualización sin una decisión expresa.

## Catálogo incluido actualizado — 2026-10-30

La nueva fuente pública contiene 1,311 mods. Se conserva únicamente el contrato
público y las URLs mirror permitidas. assets/mi/catalog.json es la entrada
reproducible; tools/mi/embed_catalog.py genera assets/mi/catalog.db y el bloque
binario incluido. La DB se abre en solo lectura y se verifica contra el JSON
en inglés, español y portugués. No se habilita descarga remota del catálogo.
Los benchmarks anteriores conservan sus datos históricos; no son mediciones
de este nuevo conjunto.

El workflow manual existente compila artefactos. No añade publicación de releases
ni se ejecuta automáticamente por este cambio. Las pruebas físicas y el resultado
Gradle/NDK en GitHub deben confirmarse después de la compilación solicitada.
