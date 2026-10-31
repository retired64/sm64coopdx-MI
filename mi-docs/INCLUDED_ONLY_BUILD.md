# Build temporal: catálogo incluido como fuente exclusiva

Fecha: 2026-10-30. Decisión explícita del propietario. Supera temporalmente las
notas anteriores de apertura con caché y refresco remoto. Plan posterior:
[REMOTE_DATABASE_PLAN.md](REMOTE_DATABASE_PLAN.md).

## Comportamiento actual

- Explorar mods carga únicamente la DB SQLite incluida en el APK.
- No lee ni reemplaza cachés de catálogo descargadas por builds anteriores.
- No existe el botón Buscar novedades ni se consulta un catálogo remoto.
- El espacio de esa acción se dedica a una búsqueda más ancha.
- Se conserva Actualizaciones como filtro local: los recibos se comparan con la
  versión incluida al cargar y tras cambios de instalación. No implica revisar
  novedades en Internet ni instalar actualizaciones sin autorización del usuario.
- Miniaturas y descargas explícitas de mods siguen funcionando por sus fuentes
  declaradas. El build no se presenta como totalmente offline.

Los datos de catálogo cambian únicamente al instalar un APK con una DB incluida
nueva. No se borran cachés antiguas, ajustes, recibos ni mods para aplicar este modo.

## Entradas, hilos y recuperación

mi_catalog_request_start y mi_catalog_request_refresh activan bundledOnly y
terminan cargando SQLite antes de cualquier lectura de caché o fetch. Refresh se
conserva por compatibilidad de API, pero tampoco obtiene datos remotos. La entrada
start_url sigue reservada a pruebas explícitas de transporte y no tiene conexión
con el menú del producto. No confundir su suite HTTP con red de catálogo en este build.

El worker publica snapshots independientes; la UI los consume con el ownership y
cierre existentes. El escáner de recibos solo corre después de terminar la
recuperación y con instalador libre. Se invalidan sus resultados al cambiar la
revisión de instalación o el snapshot; ningún hilo guarda widgets.

La recuperación de instalaciones sigue al entrar al catálogo. Si falla, el usuario
puede volver y reabrir el menú después de resolver la causa; no existe un falso
reintento de red que oculte un error de almacenamiento local.

## Validación

La suite requests comprueba apertura con directorio nuevo, con caché remota válida,
con caché caducada y por la API refresh: siempre devuelve la copia incluida,
HTTP 0, sin crear ni modificar caché. Conserva las regresiones HTTP por el override
exclusivo de pruebas. Strings EN/ES/PT y geometría se verifican en test_catalog.

Prueba física pendiente: ausencia de Buscar novedades; búsqueda, filtros locales,
detalles y retorno; reapertura con caché anterior; instalación desde CDN y refresco
habitual del juego. No atribuir esas pruebas físicas a compilación o tests CLI.

## APK local

Archivo: /sdcard/Download/sm64coopdx-MI-included-only-local.apk.
Tamaño: 129087801 bytes.
SHA-256: a550e4a5bedf5e3e8b2633462a284ce318578bce4c6c07475572896b688c6f03.
Compilación, firma, alineación, CRC y correspondencia de libmain.so correctos;
los bytes de catalog.db están presentes en la biblioteca empaquetada.
Requests confirmó copia incluida incluso ante caché remota válida/caducada y
llamada refresh: HTTP 0, sin guardar ni modificar caché remota.
No se hizo commit, push ni publicación. Pendiente la prueba física del menú.
