# Catálogo personalizado y descargas exclusivas del CDN

Fecha: 2026-10-30. Sustituye la política anterior de origen oficial con fallback.

## Fuente única

El catálogo remoto y la consulta de actualizaciones usan exclusivamente:

https://raw.githubusercontent.com/retired64/sm64cdpy.releases/r2-phase6-flutter-fallback/assets/db/database_sm64coopdx.json

No se empaqueta una copia completa del JSON. Se conserva la caché validada y el
último respaldo de esta fuente para lectura offline. El nuevo namespace
catalog-cdn-v2.cache / catalog-cdn-v2.last-good impide reutilizar por accidente
la caché anterior de main/db. No se borran recibos, mods ni ajustes existentes.

El fork solo consume el contrato JSON público; no conoce su proceso de producción.

## Descarga

Cada archivo debe tener mirror_status=available y mirror_url bajo
https://mods-cdn.sm64cdpy.org/. Ese enlace se convierte en la única URL de descarga,
independientemente de mirror_priority. download_url no se usa como alternativa.
Sin mirror válido, se permite leer detalles pero no instalar ese archivo.
El tamaño esperado procede de mirror_size_bytes; sha256 se verifica si está
publicado. IDs de mod, versión y archivo se conservan: cambiar de transporte no
crea una versión ni una asociación nueva.

El instalador vuelve a comprobar el dominio exacto, sin aceptar dominios parecidos,
HTTP ni credenciales que cambien el host. Las descargas de producción no siguen
redirecciones: un 3xx se comunica como error, sin contactar otro destino. HTTP
loopback permanece reservado a fixtures de pruebas. Se elimina el segundo intento
por una fuente alternativa; nunca se vuelve a la descarga oficial.

Esta política se refiere a archivos instalables. Miniaturas y enlaces informativos
siguen usando sus campos del catálogo y pueden pertenecer a otros hosts.

## Peppino y DynOS

El enlace CDN de Peppino respondió HTTP 200, 42136 bytes y ZIP sin errores CRC.
Contiene Peppino/texture_hud_char_wario_head.tex y Peppino/wario_geo.bin;
no es un mod Lua. El instalador actual exige Lua o main.lua para los ZIP y escribe
en mods, por lo que no puede instalar correctamente ese paquete DynOS.
Los archivos de categoría DynOS quedan deshabilitados hasta implementar su destino,
validación, recibos y recuperación propios. No declarar instalación de Peppino
resuelta solo por haber solucionado su descarga HTTP 409.

## Estado y mantenimiento

La comparación de actualizaciones sigue leyendo únicamente este catálogo y los
recibos locales. No consulta versiones en la web oficial ni en main/db.
Revisar la URL si la rama publicada cambia de nombre o se elimina. Un fallo de
red conserva la copia válida de esta fuente, sin cambiar a otro catálogo.

Pruebas: selección exclusiva, mirror ausente/no disponible, host engañoso, HTTP,
URL larga, tamaño CDN, categoría DynOS y conservación del origen solo como metadata;
además de las suites existentes de red, instalación, mantenimiento y reemplazo.
Registrar resultados efectivos antes de considerar terminada la validación.

## Evidencia local

- Catálogo personalizado por HTTPS: HTTP 200, 1308 mods; 2588 archivos con mirror
  disponible bajo el CDN esperado al consultar el JSON publicado.
- Parser, idiomas, geometría y selección exclusiva: correctos.
- Consulta de red/caché/último respaldo y descarga real del catálogo: correctas.
- Instalador con curl incluido: rechazo de hosts no permitidos, sin fallback,
  Lua/ZIP y errores existentes correctos. Lua real no_bljs.lua instalado en
  directorio temporal: HTTP 200, 369 bytes. No se tocaron mods del usuario.
- Ciclo de mantenimiento conectado: 20 escenarios correctos.
- Reemplazo/rollback/recuperación: 50 escenarios correctos, ninguno omitido.
- Comparación de actualizaciones: correcta.
- APK local compilado; firma, alineación, CRC y coincidencia de libmain.so correctos.
- APK: /sdcard/Download/sm64coopdx-MI-cdn-local.apk; 124373305 bytes.
- SHA-256: 68e70958ebe2ea10e15bf6b75e6f7104a945cb23b28e59d6d9cb723cf2cb2edf.

Pendiente de prueba física en el juego y de ejecución remota de CI. No se publica
release ni se cambia el catálogo externo por esta integración.


## Catálogo incluido — 2026-10-30

La apertura usa caché o catálogo incluido sin descargar JSON; solo Buscar novedades
consulta el repositorio. Contrato: [BUNDLED_CATALOG.md](BUNDLED_CATALOG.md).
