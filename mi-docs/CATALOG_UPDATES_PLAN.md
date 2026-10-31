# Actualizaciones internas usando el catálogo existente

Fecha: 2026-10-30. Estado original: propuesta. Consulta de solo lectura implementada
localmente; ver [CATALOG_UPDATES.md](CATALOG_UPDATES.md). Reemplazo, limpieza/migración
de recibos y Actualizar todos siguen pendientes.
Alcance autorizado: documentación del fork. No modificar el esquema del JSON publicado ni código del juego en esta tarea.
Relacionado: [instalador vigente](CATALOG_INSTALLATION.md),
[catálogo remoto](CATALOG_REMOTE.md) y [mantenimiento](MANTENER-UPDATES.md).

## Decisión de producto

No crear una pantalla/lista de «Instalaciones MI», insignias permanentes ni otro
control de activación. MI mantiene internamente asociaciones y versiones. El
usuario pide Buscar actualizaciones y solo aparecen acciones para actualizaciones
identificadas. La presentación exacta se diseñará al implementar, reutilizando el
catálogo/detalle y evitando llenar todas las tarjetas con información adicional.

El usuario puede borrar mods desde su explorador. El juego conserva descubrimiento,
Refrescar, metadatos, activación y ejecución. MI responde por archivos que administra,
no interpreta main.lua ni --description como autoridad de versión instalada.

## Frontera pública: el contrato JSON

El fork consume un catálogo JSON publicado y URLs de descarga declaradas en él.
Su implementación y documentación no dependen de cómo se producen esos datos.
No incluir rutas, repositorios, herramientas, procesos ni configuraciones privadas
de la fuente. Las identidades se tratan como valores opacos del contrato.

- mod_id/version_id/file_id identifican contenido en su ámbito. No son números
  ordenables por antigüedad; un ID diferente no demuestra una actualización.
- Fechas de publicación y procedencia son opcionales. No asumir precisión de hora
  cuando el catálogo solo aporta fecha de día.
- El contrato actual no garantiza un orden universal de versions[]. No tomar el
  primer elemento como la publicación vigente sin evidencia adicional.
- recommended identifica un archivo principal dentro de una versión, no cuál
  versión es posterior. role/requires_primary distinguen variantes y dependencias.
- Una URL de mirror no implica que haya un hash binario remoto disponible.

La copia local del JSON revisada contenía 1307 mods, 2406 versiones y 2603 archivos;
las versiones tenían release_date, pero no release_timestamp, y los archivos no
aportaban sha256 ni mirrors disponibles. Esa observación se limita a la copia
revisada, no certifica el contenido remoto actual.

La disponibilidad de actualizaciones depende del catálogo publicado en el endpoint
que consulta MI. Un catálogo preparado pero no publicado no puede ser descubierto
por el juego. MI no consulta infraestructuras privadas ni incorpora credenciales
para conseguir datos que no estén disponibles en su contrato público.

## Implementación futura solo dentro del fork

### 1. Preservar metadata ya disponible

Ampliar la proyección nativa de versiones/archivos para conservar fechas, fuente
de versión y rol, sin añadir campos obligatorios al JSON externo. MiCatalogFile
actual no conserva estas fechas ni el rol: no afirmar que ya puede decidirlo.
Ausencia o dato inválido debe significar información insuficiente, no una fecha
inventada. Fechas completas se normalizan a UTC; fechas sin hora conservan esa
menor precisión. No usar generated_at del catálogo como fecha del mod.

### 2. Reconciliar recibos y presencia local internamente

Los recibos schema 1 ya guardan mod_id, version_id, file_id, etiqueta, destino y
manifiesto de archivos instalados. Al comprobar actualizaciones:

1. Resolver exactamente el recibo por IDs contra el catálogo validado.
2. Comprobar el destino guardado, sin buscar por nombre visible ni main.lua.
3. Si desapareció, marcar la asociación internamente como ausente y excluirla de
   actualizaciones; no recrear el mod automáticamente ni afectar otros destinos.
4. Si existe, comprobar tipo/rutas y, antes del reemplazo, el manifiesto de hashes.
   Distinguir contenido intacto, modificado e instalación incompleta.
5. Si hay varias asociaciones para el mismo destino, tratarlo como conflicto de
   propiedad; no elegir arbitrariamente una y borrar la otra.

Un borrado manual no debe convertirse en «recuperación fallida» permanente ni
impedir una instalación posterior explícita. Ese ajuste de reconciliación queda
pendiente: hoy el instalador detecta recibos cuyos archivos ya no coinciden.
No eliminar evidencia automáticamente ante un diario de entrega pendiente.

Para nuevas instalaciones, una evolución versionada del recibo podría guardar
fecha de publicación, rol/variante, procedencia y hash remoto disponible. Mantener
lectura de schema 1. En recibos antiguos recuperar metadata por IDs cuando exista;
si el registro desapareció, conservar asociación/datos y declarar comparación
insuficiente. No adivinar versiones leyendo scripts ni exigir reinstalar todo.

### 3. Decidir si hay una actualización con evidencia suficiente

| Evidencia | Decisión propuesta |
|---|---|
| Mismos IDs y sin hash remoto comparable | No hay cambio demostrable por identidad; no garantiza igualdad binaria |
| Otra publicación con fecha inequívocamente posterior y variante equivalente | Puede ofrecerse Actualizar |
| Misma fecha de día para dos publicaciones sin hora | Ambiguo; la posición del JSON o un ID mayor no resuelven la duda |
| Fecha ausente, inconsistente o candidato no identificado como vigente | No anunciar automáticamente una actualización |
| Publicación vigente sin descarga soportada/resuelta | No ofrecer una versión antigua como si fuera la actualización vigente |
| Etiquetas distintas pero sin orden fiable | Cambio de etiqueta, insuficiente por sí solo |
| Archivo instalado borrado manualmente | No ofrecer actualización de esa instalación |
| Archivos locales modificados | Requerir decisión explícita antes de reemplazar |

Política inicial: resolver primero la publicación vigente con el overview del mod
(campo version) y los registros de historial; si coincide con varias publicaciones,
usar fechas válidas con precisión suficiente para desambiguar. Para registros sin
etiqueta utilizable, considerar las fechas coherentes del historial, siempre dentro
de la misma procedencia/línea. Solo ofrecer cuando tanto el candidato vigente como
su posterioridad respecto a lo instalado sean inequívocos. No hacer consultas web
extra desde el juego para reconstruir lo que falte en el JSON.

No ordenar cadenas alfabéticamente: 1.10 no es anterior a 1.9. Un comparador de
versiones solo puede ser apoyo si ambas etiquetas tienen una sintaxis estrictamente
comparable y política definida para prereleases; no extraer dígitos de nombres
arbitrarios. En el MVP conservador, una duda de fecha/identidad queda sin oferta
automática. No mostrar «todo actualizado» si quedaron asociaciones sin comparar;
un aviso agregado breve puede explicar esa limitación sin una pantalla nueva.

Conservar variante: el recomendado de una publicación no autoriza cambiar Lite
por completa, API por mod principal ni otro paquete distinto. Vincular roles y
procedencias; si hay varios candidatos equivalentes, pedir selección en detalles.
Los artefactos requires_primary siguen fuera del instalador actual; detectar una
actualización no elimina esa restricción.

### 4. Límites de hashes y revisiones silenciosas

El manifiesto del recibo verifica archivos locales descomprimidos. No puede
compararse directamente con el SHA-256 del ZIP remoto: son contenidos distintos.
Cambios de URL, mirror o tamaño no demuestran una nueva versión.

Si el autor reemplaza el binario manteniendo IDs y el catálogo no publica un hash
comparable, no puede detectarse de forma fiable descargando solo el JSON. No
prometer cobertura de esas revisiones. Sería otra funcionalidad descargar una
copia temporal y comparar contenido; no hacerlo silenciosamente para todos los mods.
Un hash calculado por MI identifica lo descargado, no descubre por sí solo si el
servidor tiene otro archivo. No tratar ETag como hash binario universal.

### 5. Reemplazo individual seguro

El catálogo es autoridad de publicación remota; el recibo/manifiesto identifica
qué instalación administra MI. La carpeta se mantiene aunque el archivo remoto
se llame distinto en la nueva versión, salvo una migración de destino diseñada.

```text
comprobar asociación y archivos locales
 -> descargar nueva versión a staging
 -> extraer y verificar
 -> revalidar destino/propiedad y reservar transacción
 -> apartar versión anterior a respaldo fuera de mods/
 -> colocar nueva versión en el destino original
 -> publicar nuevo recibo y retirar asociación sustituida
 -> eliminar respaldo tras confirmar resultado durable
```

No borrar primero la versión anterior, ni mezclar carpetas archivo por archivo.
Si algo falla, restaurar la anterior; el diario debe permitir recuperar tras cierre
abrupto. Si falla el rollback, comunicar recuperación pendiente y conservar evidencia.
Si había cambios locales, informar que se reemplazará ese contenido y pedir decisión.
No intentar sustituir un mod en uso durante una partida: ofrecer la operación en
un contexto de menú seguro, conservando Refrescar y activación del juego.

El instalador vigente rechaza destinos existentes. Este reemplazo requiere una
transacción nueva y pruebas de interrupción; no basta quitar la protección EXISTS.
No trasladar backups a mods/, donde el escáner podría descubrir ambas versiones.
No conservar comprimidos como resultado ni reemplazar mods que MI no tenga asociados.

## Orden de trabajo y aceptación

1. Fixtures de comparación con el JSON actual: etiquetas repetidas, fechas de día,
   GitHub sin versión explícita, variantes y registros no resueltos.
2. Proyección interna y reconciliación compatible con recibos antiguos/borrados.
3. Buscar actualizaciones sin escribir mods: ofrecer solo candidatos inequívocos;
   sin lista de instalaciones y sin sustituciones automáticas al abrir el juego.
4. Actualización individual con staging, respaldo, recibo y recuperación.
5. Probar sobre filesystem externo real: fcntl, renameat2, falta de espacio, red,
   cancelación, cierre antes/después de cada entrega y modificaciones locales.
6. Más adelante, Actualizar todos reutiliza la misma transacción mediante cola
   con progreso por mod y resultado final; no implementar otra lógica de reemplazo.

Fixtures deben incluir dos recibos para un mismo destino, archivo/carpeta borrado,
renombrado por el usuario, cambio de nombre del ZIP remoto, variante retirada,
rollback fallido y actualización de recibo pendiente. Validación sin mutaciones
para la búsqueda; éxito de actualización solo tras escritura/recibo confirmados.

No se ejecutan builds ni pruebas de C/C++ por añadir esta documentación. La
implementación, migración de recibos, diseño final de botones y publicación de un
catálogo nuevo quedan pendientes. Este plan utiliza únicamente el contrato JSON disponible.


La consulta inicial exige etiqueta overview fiable: no infiere la versión vigente
solo por fecha máxima cuando la etiqueta falta. Es una restricción conservadora
frente a las posibilidades de este plan. No se limpian ni migran recibos.


## Implementación posterior — 2026-10-30

La actualización individual de este plan ya está implementada localmente y
validada con fallos sintéticos y raíces temporales. Contrato efectivo y límites
en [CATALOG_REPLACEMENT.md](CATALOG_REPLACEMENT.md). Actualizar todos y asociación
de mods previos siguen pendientes; la validación física del reemplazo es independiente.
