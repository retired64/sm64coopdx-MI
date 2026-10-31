# Catálogo remoto MI — etapa 2

> Ampliación local — 2026-10-30: tarjetas con miniaturas y consulta de versiones
> posteriores, sin reemplazo de archivos. Diseño y ciclo de vida vigentes:
> [CATALOG_THUMBNAILS.md](CATALOG_THUMBNAILS.md),
> [CATALOG_UPDATES.md](CATALOG_UPDATES.md). Las descripciones de tarjetas sin
> imágenes de este documento corresponden a la etapa anterior.


Implementado el 2026-10-29 sobre mi-main, basado en ManIsCat2/android.

## Comportamiento

Explorar mods consume directamente el JSON publicado por SM64CDPY:
https://raw.githubusercontent.com/retired64/sm64cdpy.releases/main/db/database_sm64coopdx.json

La consulta y lectura de caché ocurren fuera del hilo DJUI. La pantalla mantiene
búsqueda, paginación y detalles; selecciona descripciones es/pt cuando existen.
Una copia válida de menos de 15 minutos se reutiliza al abrir. Actualizar catálogo
fuerza una consulta. Sin copia inicial y sin conexión se muestra un error con
reintento. Si existe una copia válida se conserva ante fallos de actualización.
El botón Instalar sigue deshabilitado: esta etapa no instala ni actualiza mods.
La lista y activación locales siguen siendo responsabilidad del juego.

## Contrato y límites

Se aceptan catálogos completos schema 2, hasta 16 MiB y 5000 mods, con identidades
únicas de mods/versiones/archivos y URLs HTTP(S) en archivos resueltos. No se
expone una respuesta parcialmente válida. Una reducción superior al 25 % frente
a la copia válida se rechaza preventivamente. Las proyecciones de texto son
acotadas: la descripción visible conserva hasta 2048 bytes y no interpreta
comandos de formato DJUI del contenido externo.

El catalog_id se valida por formato; NO se recalcula su hash de contenido.
La confianza remota depende de HTTPS y de la validación estructural. No se
promete compatibilidad de ejecución del mod por aparecer en el catálogo.

## Red y almacenamiento

HTTPS verifica certificado y nombre de servidor, con hasta tres redirecciones,
10 segundos para conectar y 40 segundos totales; también limita transferencias
lentas y tamaño. Detecta truncamiento cuando hay Content-Length. El libcurl
7.79 incluido necesita certificados PEM del sistema: se leen desde Conscrypt
o desde /system/etc/security/cacerts y se pasan mediante CAINFO_BLOB, sin
desactivar TLS ni modificar certificados del usuario.

Dentro de fs_writepath/mi se guardan catalog-v2.cache y catalog-v2.last-good.
La envoltura registra fecha, longitud y CRC32; CRC32 detecta corrupción local,
no autentica el contenido. Cada lectura vuelve a validar el JSON. Una copia
principal corrupta no reemplaza el respaldo válido. Se escribe un temporal
privado, fsync del archivo y rename atómico. No se garantiza fsync del directorio
ni eliminación de temporales huérfanos si el proceso muere abruptamente.
Un fallo de almacenamiento permite mostrar el catálogo remoto válido con aviso.

## Verificación de sincronización de estado

El catálogo JSON publicado es la fuente externa; el servicio nativo lee red/caché y escribe las
copias válidas; DJUI recibe snapshots inmutables mediante polling con mutex.
La fuente es el catálogo remoto validado, con la última copia válida como respaldo.
La caché se renueva por antigüedad o petición explícita; respuestas inválidas
no la sustituyen. No hay caché Flutter ni comunicación entre engines.

El hilo es intencionalmente detached: referencias compartidas mantienen viva
la solicitud hasta terminar. Cerrar el panel solicita cancelación y libera su
referencia sin bloquear DJUI. El trabajador no accede a widgets; captura errores.
Las revisiones sincronizan publicación y lectura; generaciones evitan que una
solicitud anterior sustituya la caché de una posterior. Lista, búsqueda, detalles,
retorno a Mods e idiomas EN/ES/PT fueron revisados como flujo relacionado.

## Validación

```sh
python tools/mi/embed_catalog.py --check
python tools/mi/test_catalog.py
python tools/mi/test_requests.py --bundled-curl --live
```

Las pruebas usan carpetas temporales y un servidor local, sin tocar mods del
usuario. Cubren redirección, caché reciente, respaldo, corrupción, JSON inválido,
404, truncamiento, límite de tamaño, reducción, almacenamiento fallido y cierre
durante descarga. La prueba HTTPS usa el mismo libcurl incluido en Android.

Pendiente en teléfono: primera apertura online/offline, reapertura con copia,
actualización manual, cierre durante consulta, búsqueda/paginación, idioma y
retorno a Mods con escala aumentada. Compilar no sustituye estas pruebas.

Siguiente etapa: selección del archivo e instalación Lua/ZIP con recibos,
cancelación y resultado real; después actualización individual y Update all.

### Resultado registrado

Pruebas de parser, idiomas, geometría, solicitudes con libcurl incluido y HTTPS
real: correctas (1307 mods, HTTP 200, caché guardada). APK Termux compilado;
firma, alineación 4 KiB, CRC ZIP y coincidencia de libmain.so: correctos.
No se ejecutó Gradle ni se publicó release. Prueba física remota pendiente.

APK: build/us_pc/sm64coopdx.apk — 120138041 bytes.
SHA-256: 117ad63202cfd2c396679e331403231b64b4fe8593cdce7c632d1b8ff1c8961d.


### Presentación actual

El catálogo remoto usa ahora un espacio amplio, navegación fija y filas por
página adaptadas a la altura. Véase [CATALOG_WORKSPACE.md](CATALOG_WORKSPACE.md).
El servicio de red, la caché y las reglas de instalación no cambian.


## Fuente CDN exclusiva — 2026-10-30

Se sustituye el origen anterior y la política de fallback por el catálogo
personalizado y mirror como única descarga. Contrato y límite DynOS:
[CDN_ONLY_CATALOG.md](CDN_ONLY_CATALOG.md). Las notas anteriores conservan historia.


## Catálogo incluido — 2026-10-30

La apertura usa caché o catálogo incluido sin descargar JSON; solo Buscar novedades
consulta el repositorio. Contrato: [BUNDLED_CATALOG.md](BUNDLED_CATALOG.md).


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
