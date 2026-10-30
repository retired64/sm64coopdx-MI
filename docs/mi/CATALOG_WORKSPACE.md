# Catálogo MI: espacio amplio y navegación accesible

Fecha: 2026-10-29. Sustituye el diseño estrecho de la etapa 2.

## Decisiones

Se revisaron djui_panel_menu, djui_three_panel, el panel de descripción de Mods,
flow layouts, búsqueda, estilos de botones, cursor y la pila/animaciones DJUI.
El catálogo y sus detalles conservan esos componentes nativos y el tema activo,
pero ocupan el 96 % del ancho y toda la altura útil. El fondo tiene una opacidad
mínima de 220/255 para legibilidad; se conservan márgenes y el juego detrás.
No se cambia la escala persistida del usuario ni los demás menús.

Cabecera: título, búsqueda, actualizar/reintentar y estado. Pie: volver,
anterior, página/resultados y siguiente. Estas acciones están fuera del cuerpo
que puede desplazarse. Las filas muestran título hasta dos líneas y autor /
categoría. Los detalles usan el mismo espacio amplio y mantienen volver e
instalar (todavía deshabilitado) en el pie; el contenido largo usa las flechas.

La cantidad de filas se calcula con la altura lógica DJUI, incorporando la escala
del usuario: de una a cinco por página. Se preserva aproximadamente la posición
al cambiar esa capacidad. Por debajo de 480 unidades se compactan espacios y se
oculta la fecha secundaria. No se reduce toda la interfaz para meter más filas.
Los botones traducidos permiten dos líneas con ajuste acotado de tipografía.
Las dimensiones son unidades DJUI, no una certificación de tamaño físico en dp.

En Mods el acceso al catálogo está en la cabecera del panel derecho existente,
fuera de la lista local desplazable. Sigue limitado al contexto original sin
partida de red. Se oculta/desactiva cuando Mods no está activo o durante un
refresco local. Las flechas de desplazamiento originales se conservan.

## Verificación de sincronización de estado

El servicio de catálogo sigue escribiendo snapshots/caché y DJUI leyéndolos;
no cambia la fuente de verdad ni se agrega persistencia. El tamaño de página,
filtro y selección pertenecen al panel y se liberan al cerrarlo. La geometría
se recalcula desde la raíz DJUI; no se cachea una medida del dispositivo.

El acceso derecho lee visibilidad/habilitación del panel Mods y el bloqueo de
navegación del juego en cada render; se liberan sus referencias con el propietario.
La descripción se oculta al abrir MI y se restaura al regresar. No se agregan
Futures ni hilos; permanece el trabajador detached ya documentado en
[CATALOG_REMOTE.md](CATALOG_REMOTE.md). Se revisaron lista, detalles, búsqueda,
paginación, Mods local, tooltip, temas e idiomas EN/ES/PT.

## Validación y límites

La regresión geométrica usa alturas 480/720/1080 y escalas 0.5/0.75/1/1.5:
filas completas, cabecera y pie reservados, sin necesitar desplazar la lista del
catálogo en esa matriz. También se mantienen las pruebas de desbordamiento
para menús y detalles. Se comprueban parser, traducciones y generadores.

Pendiente prueba física: táctil y mando, entrada superior derecha, retorno,
EN/ES/PT, títulos largos, tema claro/oscuro, teclado de búsqueda, escala grande,
primera carga, caché y errores. Los tamaños extremos fuera de la matriz pueden
requerir flechas. No se certifica interacción ni apariencia por compilar.

La instalación de mods continúa pendiente; este cambio adapta la presentación.


Validación registrada: pruebas de parser, idiomas y geometría correctas;
generador sin diferencias; APK Termux compilado y verificado (firma, alineación
4 KiB, CRC y libmain coincidente). No se ejecutó Gradle. Prueba física pendiente.
APK: build/us_pc/sm64coopdx.apk, 120154425 bytes.
SHA-256: 6db65de2ac6df1aa598bd45d8addc4813fe034b710a3c2f44f5fc60939a7d870.


## Iteración posterior: tarjetas grandes

El diseño actual reemplaza las filas de una columna por una cuadrícula adaptable
y aumenta tipografía y contraste. Consulta [CATALOG_ACCESSIBILITY.md](CATALOG_ACCESSIBILITY.md).
Las cifras y el APK anteriores se conservan como registro histórico.
