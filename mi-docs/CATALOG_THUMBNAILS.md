# Tarjetas con miniaturas

2026-10-30: implementación local para pruebas. Consume image_url o, si está vacío,
icon_url del JSON público. No depende de cómo se producen esos datos.

## Diseño

Se conserva cuadrícula adaptativa de hasta 2x2 y la escala del usuario. Tarjeta
completa táctil: imagen cuadrada izquierda; título, autor y categoría a la derecha.
La descripción completa sigue en detalles. No se reduce la cuadrícula para añadir
una fila de controles: búsqueda, refresco y consulta de actualizaciones comparten
la cabecera existente.

Títulos dorados, autor azul y categoría verde, con tonos oscuros para fondos claros
y claros para fondos oscuros. Se adapta al color real del botón, también al enfocar.
Se conservan borde/estilo/foco y navegación nativos. Fondo del botón opaco para
lectura; no se cambian preferencias globales de fuente, escala ni tema. El cuerpo
y las flechas mantienen su contrato de desplazamiento.

mi_card_layout calcula regiones sin solapamientos; mi_card_render restaura posición
absoluta de título después de cambios del estilo de botón. Los textos largos
pueden recortarse dentro de la tarjeta y se leen completos en detalles. No se usa
el desplazamiento relativo de texto que antes hacía desaparecer controles.

El marcador MOD aparece si no hay URL, el formato no se admite o falla la imagen;
... indica carga. La miniatura nunca bloquea catálogo, selección ni instalación.
Este cambio muestra miniaturas en tarjetas, no una galería de capturas en detalles.

## Descarga, decodificación y caché

mi_thumbnail.h/.cpp: dos workers detached de proceso y cola de hasta 16 solicitudes
pendientes. Se eliminan solicitudes canceladas de la cola; cambio de página/cierre
libera referencias sin join bloqueante. Solo se solicitan las tarjetas visibles,
no los miles de iconos del catálogo. Los workers no conservan widgets.

HTTPS con certificados del sistema y validación de hostname; HTTP loopback solo
para pruebas. Hasta 3 redirects, conexión 8 s, total 20 s, baja velocidad 128 bytes/s
por 8 s. Cuerpo máximo 2 MiB; PNG/JPEG, hasta 1024x1024 antes de decodificar. Otros
formatos (incluidos WebP/GIF) quedan con marcador; no se adivina la extensión.

Decodificador privado basado en el header ya incluido en el motor: sin compartir
settings/globales del decodificador del juego. Decodificación serializada entre
los dos workers. Normalización RGBA128x128 con recorte central sin deformación y
reescalado bilineal. No se modifican los archivos de imagen del usuario.

Caché opcional: fs_writepath/mi/thumbnails/<hash-url>.rgba. Magic/version y SHA-256
local del bloque RGBA detectan corrupción. No autentican al autor. Caducidad 7 días;
retención de hasta 64 entradas conocidas por antigüedad de escritura, alrededor de
4.1 MiB, con poda acotada de directorio. No es LRU por acceso. No se borran archivos
ajenos al patrón de caché; el límite es por proceso y una concurrencia entre procesos
puede superarlo temporalmente. Una URL nueva obtiene otra clave.

Temporal exclusivo, fsync del archivo y rename. Caché corrupta/caducada se vuelve
a descargar; fallo al guardar no impide mostrar una imagen válida en memoria.
No se garantiza fsync de directorio ni limpieza de temporales por pérdida eléctrica.
La caché de imágenes es independiente de catálogo, recibos y datos de mods.

## CPU, GPU y ciclo de vida

mi_thumbnail_take copia una sola vez a memoria de la UI. Cada catálogo alquila
hasta cuatro de dieciséis slots estables de 64 KiB, 1 MiB global reservado. Al
cambiar contenido se oculta la imagen anterior hasta publicar la nueva. Al cerrar
se libera el alquiler, nunca la dirección de píxeles, porque display lists con
interpolación podrían conservarla. Más paneles que slots usan marcador.

mi_thumbnail_texture.c/.h encapsula el pool en hilo principal. No mantiene página,
foco ni estado de instalación globales. Renderer y widgets nunca leen el vector
del worker. Al reutilizar una dirección se marca únicamente esa textura como
modificada; gfx_texture_cache_lookup reimporta datos en el GPU ID existente.
No se vacía toda la caché gráfica ni se crea otra textura por cada imagen/cambio.

Puntos de contacto upstream: gfx_pc.c/.h añade gfx_texture_mark_dirty y refresco
opt-in por dirección, acotado a 16 pendientes. El comportamiento de otras texturas
sigue su ruta habitual. Revisar la semántica del caché/override DJUI/DynOS al
sincronizar upstream, aunque no haya conflictos. No mover el upload a workers.

## Validación

```sh
python tools/mi/test_thumbnails.py --bundled-curl --live
python tools/mi/test_catalog.py
python tools/mi/test_updates.py
make -j$(nproc)
```

Tests sintéticos: PNG/normalización RGBA, HTTP y truncamiento, límites de bytes y
dimensiones, corrupción/caducidad/poda de caché, almacenamiento opcional, cierre y
cambios rápidos de cola; slots acotados, direcciones estables y publicación que
solicita refresco gráfico. La opción live consulta un icono del catálogo público
por HTTPS en una raíz temporal. Pruebas de parser cubren URL HTTPS exacta y fallback.
Geometría prueba región de imagen y texto en varios anchos/altos de tarjeta.

Estas pruebas no acreditan render GPU ni input táctil. Falta prueba física de
miniaturas correctas al paginar/filtrar/reabrir, imágenes ausentes, offline, escala
grande, temas claros/oscuros, mando y fluidez. No se afirma contraste certificado
ni memoria GPU medida. El checklist registra build/APK y pendientes reales.
