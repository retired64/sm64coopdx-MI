# Catálogo SQLite incluido: apertura rápida y contrato JSON remoto

Fecha: 2026-10-30. Alcance: primera migración del catálogo incluido en el APK.
No cambia instalación, recibos, activación ni la fuente remota publicada.

## Arquitectura

- assets/mi/catalog.json conserva el contrato público completo como fuente de build.
- tools/mi/build_catalog_db.py genera assets/mi/catalog.db, schema nativo 1.
- tools/mi/embed_catalog.py genera mi_catalog_bundled.inc con los bytes de esa DB.
- mi_catalog_bundled(language) abre esos bytes en memoria y solo lectura, con
  SQLite compilado dentro del juego. No depende de APIs privadas Android ni de
  instalar SQLite en el teléfono.
- El lector produce los mismos DTOs que el parser JSON. Se cierra la conexión
  después de materializar el snapshot. Búsqueda, paginación, detalles y comparación
  de actualizaciones conservan el trabajo en memoria; no hay consultas SQL por frame.

La DB es una proyección de los campos usados actualmente por el motor nativo;
no sustituye el JSON público completo ni es una base de datos de instalaciones.
EN/ES/PT comparten datos y conservan el fallback de descripción original.

## Frontera de esta etapa

La apertura sigue usando primero una caché CDN válida; sin ella utiliza la DB
incluida. Buscar novedades sigue descargando el JSON del único endpoint configurado,
validándolo y guardándolo con la recuperación existente. Esa caché todavía es JSON:
reabrir con copia remota guardada conserva su coste de parseo anterior.

Por tanto, la aceleración medida de SQLite se aplica a la ruta del catálogo
incluido. No se declara una migración completa de la caché ni de la distribución
remota. Cambiar esas rutas requiere importación transaccional, esquema/migración,
invalidation y pruebas de recuperación propias. No convertirlas incidentalmente
ni borrar la caché del usuario para forzar resultados favorables.

Miniaturas e instalación siguen haciendo sus solicitudes explícitas/habituales.
La política de apertura local se refiere al catálogo JSON, no a todas las imágenes.

## Contrato y validación

La base usa application_id MIDB y user_version=1. Tablas: metadata, mods,
translations, versions y files. Ordinales preservan el orden original; IDs de
mod/versión/archivo no se regeneran. Las asociaciones y rutas instaladas no cambian.

El generador comprueba schema JSON, estado completo, conteo y catalog_id antes de
crear un temporal y reemplazar la base generada. --check del generador embebido
comprueba ID de fuente, integridad y bytes del include. La DB y el include son
salidas generadas: no se editan manualmente. JSON y DB se versionan juntos.

El lector usa deserialize de solo lectura, consultas fijas, query_only, modo
Defensive, trusted_schema desactivado y límites de tamaño/SQL/columnas. Comprueba
firma SQLite, versión, application_id, quick_check, metadatos, índices/ordinales,
IDs duplicados, límites, filas huérfanas y versiones resueltas sin archivos.
Valida de nuevo URLs, hashes, tamaños, dependencias y formatos admitidos al formar
los DTOs. Un fallo no publica un catálogo parcial ni escribe sobre la entrada.
La base incluida no es una firma criptográfica ni certifica compatibilidad jugable.

Dependencia y procedencia: [SQLITE_DEPENDENCY.md](SQLITE_DEPENDENCY.md).
El wrapper mi_sqlite.c vive en mod_integrations, ya enumerado por Makefile y
Android.mk. Los tests enlazan ese mismo wrapper mediante native_sqlite.py.

## Pruebas y benchmark

```sh
python tools/mi/embed_catalog.py --check
python tools/mi/test_catalog.py
python tools/mi/test_requests.py --bundled-curl --live
python tools/mi/test_updates.py
python tools/mi/test_maintenance.py --bundled-curl
python tools/mi/test_replacement.py --bundled-curl
```

El contrato de DB se ejecuta dentro de test_catalog.py: compara todos los campos
DTO EN/ES/PT con el JSON, y prueba truncamiento, corrupción, versión desconocida,
conteo inválido, huérfanos e identidad vacía. No usa mods reales del usuario.

Benchmark opcional Linux/Termux; necesita headers/biblioteca SQLite de desarrollo
solo para su ejecutable experimental, no para el APK:

```sh
python tools/mi/benchmark_catalog_storage.py --rounds 9 --output-dir /tmp/mi-storage-results
```

En esta sesión Termux fue necesario anteponer env LD_LIBRARY_PATH con la ruta
habitual de las bibliotecas Termux para evitar interferencias del entorno de enlace.
No cambiar bibliotecas globales por este workaround. El benchmark verifica el hash
del catálogo y compara procesos nuevos con caché del sistema caliente y orden
aleatorio reproducible. VmHWM de /proc/self/status evita confundir el pico heredado
del proceso Python con el consumo del lector nativo.

Medición previa de nueve rondas en este dispositivo, orientativa:

| Ruta | Apertura mediana | Pico proceso | Búsqueda/página en memoria |
|---|---:|---:|---:|
| JSON incluido | 246,4 ms | 39820 KiB | 2,99 ms |
| Prototipo SQLite con snapshot completo | 85,6 ms | 31188 KiB | 3,00 ms |
| SQL bajo demanda experimental | 1,10 ms inicial | 7320 KiB | 10,03 ms |

La tercera ruta no se adopta: modifica la arquitectura y no acredita equivalencia
completa de búsquedas con texto saneado/recortado. El lector de producción añade
validaciones, por lo que se mide separadamente y no se le atribuyen automáticamente
los tiempos del prototipo. Estas cifras no miden FPS ni certifican otro teléfono.

JSON: 3239458 bytes. DB inicial: 2994176 bytes. Gzip favoreció al JSON (867516 frente
a aproximadamente 919635 bytes de DB). SQLite no es un compresor. En el APK actual
libmain.so se almacena sin compresión; el tamaño final depende también del motor
SQLite añadido y se verifica contra el APK anterior, sin prometer reducción.

## Mantenimiento y pendientes

Conservar pruebas de igualdad de DTOs al modificar el parser JSON, el esquema o
los límites. Añadir un campo al contrato requiere decidir si forma parte de la
proyección SQL y actualizar generador, lector y test de equivalencia juntos.
Una versión de esquema desconocida se rechaza; no reinterpretar columnas antiguas.

Pendiente físico: abrir sin caché y sin conexión, leer detalles, cambiar idioma,
Buscar novedades y reabrir con caché, instalar/actualizar y refrescar Mods. El
resultado de un benchmark CLI no sustituye esta validación ni la ejecución NDK CI.

## Resultado del lector integrado con SQLite del juego

Nueve rondas adicionales enlazaron el objeto mi_sqlite.o realmente compilado por
el juego, con el lector final y sus validaciones. Medianas locales:

| Ruta | Apertura | Pico del proceso | Búsqueda/paginación |
|---|---:|---:|---:|
| JSON en memoria | 252.22 ms | 39824 KiB | 3.032 ms |
| SQLite incluido | 89.79 ms | 33700 KiB | 3.027 ms |

Esto favorece la carga completa desde SQLite, manteniendo navegación en memoria.
No se adopta SQL por cada interacción. Informe con muestras y límites:
[benchmarks/2026-10-30-catalog-storage.json](benchmarks/2026-10-30-catalog-storage.json).

La generación se comprobó dos veces con igualdad byte a byte; el JSON fuente no
cambia. test_catalog.py y la prueba final contra el objeto SQLite del juego
confirmaron todos los DTOs EN/ES/PT y rechazos de corrupción, truncamiento,
esquema, estado parcial, conteo, huérfanos e identidad vacía.

La regla específica de Makefile excluye -g solo del objeto SQLite con -g0. No
cambia su optimización ni retira debug del juego. Revisar esta regla en syncs;
Android.mk ya enumera el wrapper en mod_integrations y el pipeline NDK release
usa su propio paso strip. La validación NDK remota queda pendiente.

## Artefacto final y regresiones

APK: /sdcard/Download/sm64coopdx-MI-sqlite-local.apk.
Bytes: 129091897.
SHA-256: 5f692f4362b29b49591207777696716504e45b6b4f9ac7f5f9344900ef54dee2.
Incremento frente al APK con JSON incluido: 1474560 bytes. La base es menor,
pero la dependencia aumenta el APK; el beneficio buscado aquí es apertura/memoria.

Firma, alineación, CRC y coincidencia del libmain compilado: correctos. Los bytes
de la DB están en la biblioteca empaquetada, sin duplicar el JSON fuente completo.
El objeto SQLite pasó de aproximadamente 7,7 a 1,6 MB al retirar solo su debug.

Requests/caché/apertura local/refresco explícito con curl incluido y consulta real:
correctos. Parser, DB EN/ES/PT, cadenas y geometría: correctos. Consulta de
actualizaciones: correcta. Mantenimiento: 20 escenarios correctos. Reemplazo y
rollback: 50 escenarios correctos, ninguno omitido. Generación DB determinista,
fuente original intacta, generador --check y git diff --check: correctos.

No se repiten las pruebas de miniaturas sin cambios de su motor: su validación
previa sigue siendo la referencia, y la igualdad de DTOs incluye las URLs de imagen.
Pendiente: prueba física del APK y build NDK remoto. No se hizo commit ni push.


## Modo incluido exclusivo — 2026-10-30

Este build ignora cachés remotas y no ofrece Buscar novedades. Fuente única: DB
incluida en el APK. Estado vigente: [INCLUDED_ONLY_BUILD.md](INCLUDED_ONLY_BUILD.md).
Plan posterior: [REMOTE_DATABASE_PLAN.md](REMOTE_DATABASE_PLAN.md). Las notas
anteriores de refresco remoto describen una etapa previa.
