# Catálogo incluido y consulta remota explícita

Fecha: 2026-10-30. Contrato público; no contiene instrucciones de producción privada.

## Flujo

1. El APK incluye el JSON público de assets/mi/catalog.json dentro de libmain.so.
2. Al abrir Explorar mods, un trabajador carga la caché CDN validada o su último
   respaldo. Sin copia válida, parsea el catálogo incluido según el idioma.
3. La apertura nunca descarga el JSON, aunque la caché tenga más de 15 minutos.
4. Buscar novedades consulta el endpoint público configurado, valida el resultado,
   guarda la caché y después compara recibos para detectar actualizaciones.
5. Una consulta fallida no elimina el catálogo mostrado ni cambia a otra fuente.

Las miniaturas siguen su carga bajo demanda y una instalación explícita descarga
su archivo del CDN. Apertura local se refiere al JSON; no significa que todas las
imágenes estén empaquetadas ni que se puedan descargar mods sin conexión.

La copia guardada tiene prioridad sobre la incluida. Una nueva versión del APK
no borra ni reemplaza automáticamente esa copia: Buscar novedades renueva el
catálogo cuando el usuario lo decide. No se certifica la frescura de una copia
local por estar incluida o por proceder de caché.

## Archivos y generación

assets/mi/catalog.json es la entrada pública versionada. tools/mi/embed_catalog.py
comprueba schema, conteo y catalog_id, y genera mi_catalog_bundled.inc. El generador
existente --check también exige que esta salida esté actualizada. No editar el
.inc a mano. mi_catalog_bundled(language) devuelve un snapshot independiente,
validado por el mismo parser que se usa para el JSON remoto.

La representación literal compilada sirve para ambos build paths sin permisos ni
extracción de assets Android, y el APK comprime la biblioteca según su empaquetado.
No se copian los datos completos privados ni herramientas ajenas al contrato público.

El catálogo incluido conserva IDs, títulos, autores, categorías, descripciones,
traducciones, imágenes, versiones, roles, dependencias y metadatos CDN necesarios.
No exige download_url: la única descarga instalable sigue siendo mirror_url bajo
el CDN permitido. Se recalcula catalog_id al cambiar los datos públicos.

## Ownership y estado

El worker conserva sus referencias; la UI recibe snapshots y los libera como antes.
fromCache=true con savedAt=0 identifica la copia incluida y muestra Catálogo incluido,
sin afirmar una consulta remota ni un guardado en disco. Una caché validada conserva
su fecha de consulta. No se genera una caché artificial al abrir por primera vez.
Revisiones e invalidación de resultados de actualización siguen el flujo existente.

El endpoint remoto no cambia en esta etapa; el parser admite el catálogo público
mínimo y el JSON enriquecido actualmente publicado. Promover un nuevo archivo
remoto es una operación separada de construir el APK.

## Verificación

Entrada pública incluida: 1308 mods, 2409 versiones, 2606 archivos, 2588 mirrors;
3239458 bytes. Comparar identidades con la entrada al preparar cada revisión.

Pruebas nativas cubren parseo del catálogo incluido, idioma/fallback, primera
apertura sin caché y sin escritura/red del catálogo, caché caducada sin consulta,
refresco explícito, errores, copia de respaldo y cancelación. Mantener la suite
de actualizaciones y thumbnails. Validar APK y correspondencia de biblioteca.

Pendiente físico: primera apertura sin conexión, búsqueda/paginación/detalles,
reapertura con caché antigua y Buscar novedades con y sin red. Conservar las
pruebas de instalación y límites de [CDN_ONLY_CATALOG.md](CDN_ONLY_CATALOG.md).

## Resultado de validación local

Parser/idiomas/viewport, requests con curl incluido y consulta real explícita,
actualizaciones y thumbnails: correctos. El test de primera apertura comprueba
1308 mods, HTTP 0 y ausencia de caché artificial; el de caché caducada conserva
la copia sin consulta. Se comprobaron también los IDs de 2409 versiones y
2606 archivos, sin cambios, y la ausencia de campos técnicos fuera del contrato.

APK compilado, firma y alineación válidas; CRC y libmain.so coinciden. El ID del
catálogo público incluido está presente en la biblioteca empaquetada.
Archivo: /sdcard/Download/sm64coopdx-MI-bundled-catalog-local.apk.
Tamaño: 127617337 bytes.
SHA-256: 96f2c74eccd04197b3d6ab7663b43922a69543b2d48f26f75c5f9a20c1ca7e5f.
Pendiente prueba física y CI remoto. No se hizo commit, push ni publicación remota.


## Catálogo incluido SQLite — 2026-10-30

La copia incluida pasa a una base SQLite de solo lectura. JSON sigue siendo el
contrato remoto y el formato de caché actual. Arquitectura, límites y pruebas:
[CATALOG_DATABASE.md](CATALOG_DATABASE.md). Esta nota supera la representación
JSON literal incluida descrita anteriormente, sin cambiar sus IDs.


## Modo incluido exclusivo — 2026-10-30

Este build ignora cachés remotas y no ofrece Buscar novedades. Fuente única: DB
incluida en el APK. Estado vigente: [INCLUDED_ONLY_BUILD.md](INCLUDED_ONLY_BUILD.md).
Plan posterior: [REMOTE_DATABASE_PLAN.md](REMOTE_DATABASE_PLAN.md). Las notas
anteriores de refresco remoto describen una etapa previa.
