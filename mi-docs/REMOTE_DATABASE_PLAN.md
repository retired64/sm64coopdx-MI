# Plan futuro: catálogo SQLite remoto en el CDN

Fecha: 2026-10-30. Propuesta de contrato público, no funcionalidad habilitada.
Estado actual: [INCLUDED_ONLY_BUILD.md](INCLUDED_ONLY_BUILD.md). Este build solo
usa la base incluida y no ofrece Buscar novedades.

## Objetivo y recursos públicos

Permitir en una etapa futura actualizar el catálogo sin recompilar el APK,
conservando la apertura local y la decisión explícita del usuario. El juego
conocerá únicamente el contrato público y URLs HTTPS; nunca credenciales de R2,
nombres privados de infraestructura ni procesos internos de producción.

Propuesta de dos recursos:

- catalog/stable/latest.json: manifiesto pequeño del catálogo vigente.
- catalog/releases/<hash-del-archivo>/catalog.db: una DB inmutable por revisión.

El manifiesto no es el catálogo JSON grande. Campos propuestos: schema_version,
database_schema_version, catalog_id, database_url, size_bytes, sha256 y generated_at.
Definir sha256 como 64 caracteres hexadecimales minúsculos del archivo DB recibido;
catalog_id conserva el prefijo sha256: y la identidad lógica del catálogo.
El tamaño corresponde a los bytes de la DB, no a una representación comprimida.
El esquema de transferencia y cualquier Content-Encoding se probarán por separado.

Los nombres/rutas son propuestas, no endpoints ya publicados. Una futura versión
de esquema incompatible requerirá canal o contrato compatible para clientes antiguos;
no sustituir silenciosamente el significado de columnas o campos existentes.

## Publicación y promoción

Preparar la revisión antes de hacerla vigente: validar estructura, límites,
integridad SQLite, IDs, idiomas, conteos y equivalencia con el contrato público.
Generar tamaño y SHA-256 de los bytes definitivos. Publicar primero la revisión
inmutable y comprobar su descarga desde el dominio público. Solo entonces promover
el manifiesto vigente. Un fallo previo conserva la revisión anterior.

Conservar revisiones previas para recuperación. La DB inmutable admite caché larga;
el manifiesto necesita TTL corto o revalidación por ETag. ETag sirve para evitar
transferencias redundantes, no sustituye SHA-256. Evitar que dos promociones
simultáneas o una ejecución antigua hagan retroceder accidentalmente el catálogo.
La publicación automatizada no se considera configurada por existir este plan.

## Flujo futuro del cliente

1. Abrir: DB local validada o incluida, sin consulta remota automática.
2. Buscar novedades explícitamente: descargar y validar el manifiesto acotado.
3. Si la revisión y DB local ya coinciden, no descargar de nuevo el catálogo.
4. Si cambian, descargar a un temporal aislado con progreso real y cancelación.
5. Comprobar HTTPS/host, tamaño, hash, schema/application_id/user_version,
   integridad, referencias, límites y DTOs antes de promover datos.
6. Guardar la revisión completa y cambiar atómicamente un puntero local vigente.
   DB y metadata no deben quedar desparejadas tras cierre: probar un protocolo de
   promoción/recuperación, no asumir que dos renames son una transacción conjunta.
7. Publicar el snapshot en la UI e invalidar resultados de comparación antiguos.
8. Comparar recibos locales con el nuevo catálogo, sin actualizar mods automáticamente.

Ante red caída, 404, hash/tamaño incorrecto, base corrupta, esquema desconocido,
cancelación, falta de espacio o cierre, conservar la DB anterior o la incluida.
El modo de diagnóstico no debe ocultar esos fallos detrás de una falsa afirmación
de catálogo actualizado. No realizar modificaciones sobre la base recibida.

## Prerrequisitos y validación

El lector actual se diseñó para una copia generada e incluida y ya tiene controles,
pero habilitar DB remota requiere una revisión adicional de entrada no confiable:
consultas/vistas/estructuras maliciosas, límites de tiempo/memoria y progresos,
esquemas inesperados y ataques a SQLite. Actualizar la dependencia fijada mediante
procedencia verificada y pruebas, no descargar bibliotecas en el dispositivo.

Probar además: manifiesto inválido, 304 sin copia local usable, cambio de esquema,
reducción inesperada de catálogo, revisión repetida, publicación interrumpida,
cierre antes/después de promoción, cancelación, disco lleno y lectores simultáneos.
Mantener IDs de mod/versión/archivo y recibos existentes; no derivarlos del nombre,
de la ruta CDN ni del hash global del catálogo.

## Etapas propuestas

- [ ] Formalizar y probar el manifiesto público y el esquema DB remoto.
- [ ] Validar revisiones antes de promoción, conservando evidencias y anterior vigente.
- [ ] Probar promoción manual; automatizar generación y validación primero.
- [ ] Incorporar descarga, cache transaccional y recuperación al cliente.
- [ ] Validar físicamente y habilitar Buscar novedades en un build posterior.

No se modifica ni publica infraestructura por esta documentación. La base actual
es aproximadamente 2,99 MB; no es un formato de compresión. El beneficio medido
fue apertura y memoria. Ver [CATALOG_DATABASE.md](CATALOG_DATABASE.md).
