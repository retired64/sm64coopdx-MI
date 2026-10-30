# MI: tarjetas y lectura accesible

> Ampliación local — 2026-10-30: tarjetas con miniaturas y consulta de versiones
> posteriores, sin reemplazo de archivos. Diseño y ciclo de vida vigentes:
> [CATALOG_THUMBNAILS.md](CATALOG_THUMBNAILS.md),
> [CATALOG_UPDATES.md](CATALOG_UPDATES.md). Las descripciones de tarjetas sin
> imágenes de este documento corresponden a la etapa anterior.


2026-10-29. Iteración basada en las cuatro capturas del usuario en Screenshots.
Sustituye las filas estrechas de texto de CATALOG_WORKSPACE.md.

## Presentación

Hasta cuatro tarjetas, dos columnas por dos filas, en dimensiones DJUI 1280x720.
Si el ancho útil baja de 1000 unidades, una columna; si la altura no permite dos
filas completas, una fila. Los textos no se encogen para meter más mods.
Las tarjetas miden entre 220 y 300 unidades; título al 125 % del tamaño base,
autor y categoría al 100 % (antes 65 %), en campos separados. Cuando la tarjeta
alcanza 288 unidades también muestra un extracto de descripción; la descripción
completa sigue en detalles. Texto que excede el área de una tarjeta puede quedar
recortado; el detalle permite leerlo mediante desplazamiento.

La escala DJUI global sigue aplicándose a texto y geometría. Cambiar la escala
puede reducir columnas/filas, conservando aproximadamente la posición del catálogo.
No se guarda otra preferencia ni se cambia la del juego. Las acciones permiten
etiquetas grandes con ajuste acotado para traducciones largas. El contador aumenta
del 65 al 95 % del tamaño base. Buscar mods tiene traducción propia EN/ES/PT,
incluso cuando el INI externo antiguo carece de las claves MI.

Se conserva el renderizador nativo de tres paneles, su reconocimiento de ancestros,
recorte, animaciones, flechas, rueda y exposición del foco con mando. Un hook de
cabecera ajusta solamente la geometría MI después de calcularla: contenido arriba,
acciones abajo. Así se elimina el vacío superior que mostraban los detalles.
El cuerpo del catálogo dispone celdas explícitas; el detalle conserva flujo vertical
con altura de texto medida y ancho de lectura máximo de 1000 unidades. Descripción
y metadata del detalle usan el 110 % del tamaño base; su título usa el 140 %.
El panel aumenta la opacidad mínima a 242/255 para reducir distracciones del fondo.

## Verificación de sincronización de estado

Red y caché no cambian: el servicio publica snapshots, el panel los consume; el
catálogo remoto validado y su último respaldo siguen siendo la fuente de datos.
El panel escribe y lee filtro, página y capacidad. Geometría se deriva de la raíz
DJUI en cada render; no hay nueva caché persistente ni invalidaciones adicionales.
No se agregan Futures ni hilos; el trabajador detached existente conserva su
cancelación y ownership. Lista/cuadrícula, búsqueda, detalle, pie de navegación,
idiomas, retorno a Mods y sus flechas se revisaron juntos. La integración no añade
activación ni modifica instalación o selección local del juego.

## Validación

Pruebas de geometría: anchos 640/1280/2048, alturas 480/720/1080 y escalas
0.5/0.75/1/1.5. Comprueban 2x2 a 1280x720, 1x2 a 640x720, capacidad válida,
altura mínima de tarjeta y desplazamiento cuando no cabe una fila completa.
Pruebas de idiomas comprueban Buscar mods con INIs externos antiguos.

Pendiente prueba física: cuadrícula y mando en ambos ejes, títulos/autores largos,
búsqueda y teclado, escala grande, detalles con párrafos largos, estados de red,
EN/ES/PT, temas y retorno a Mods. No se certifica accesibilidad física ni contraste
cuantitativo mediante compilación o geometría automatizada.


APK de esta iteración compilado con Makefile Termux; firma, alineación 4 KiB,
CRC ZIP y coincidencia de libmain correctos. Generador y git diff --check correctos.
No se ejecutó Gradle. Prueba física pendiente.
APK: build/us_pc/sm64coopdx.apk, 120158521 bytes.
SHA-256: 4e221f79ca1d537d25bfcc8f779624d09ae337a1c0d18c87ac4487ed0a2a4253.


### Confirmación del usuario — 2026-10-29

El usuario confirma que la nueva visualización funcionó a la perfección y
resultó muy accesible en su teléfono. Se registra aceptación general del diseño
de tarjetas y lectura. No se presume cobertura individual de toda la matriz
de mando, teclado, idiomas, temas y estados de red. Las notas anteriores de
prueba física pendiente reflejan el estado previo a esta confirmación.


### Detalles organizados — 2026-10-30

El detalle presenta título dorado, autor azul y categoría verde, con variantes
oscuras para superficies claras. El texto sigue identificando cada dato: el color
no es el único indicador. Las secciones «Elige una versión / archivo» y «Acerca
del mod» llevan separadores y encabezados azules. El selector precede a la
descripción para que una descripción extensa no oculte la elección de versión.
Se conservan ancho de lectura máximo de 1000 unidades lógicas, ajuste de altura
al envolver, escala DJUI, scroll y pie fijo con instalar/volver/progreso/estado.
No cambia el instalador, la consulta de actualizaciones ni la propiedad de datos.
Cambios en mi_catalog_panel.c, tres INIs y strings generados; sin cambios upstream.
Validación física pendiente: temas claros/oscuros, títulos largos, escalas grandes,
selección con mando y progreso real durante instalación. No se presume aceptación
por confirmaciones anteriores sobre otras pantallas.


### Consulta unificada de novedades — 2026-10-30

Esta nota supera el flujo anterior con dos acciones de descarga. «Buscar novedades»
es la única acción manual de red: fuerza una consulta del catálogo y, al terminar,
compara los recibos MI contra el snapshot válido. El botón queda deshabilitado
durante ambas fases. Abrir el catálogo conserva su política de caché habitual;
no implica reemplazar mods ni iniciar una actualización automática.

«Actualizaciones (N)» es únicamente un filtro local, habilitado cuando hay
candidatos comprobados. Alterna con «Ver todos los mods» sin red ni nueva lectura
de recibos. Conserva la búsqueda y vuelve a la primera página; terminar la consulta
no activa el filtro automáticamente. Catálogo nuevo o revisión de instalación
invalida candidatos y filtro. No existe otra caché ni otro trabajador.

El resumen distingue candidatos, ausencia de recibos MI, ausencia de novedades
y comprobaciones incompletas. Una ruta ausente se comunica como instalación sin
verificar, nunca como una eliminación realizada por MI. Los errores de red o de
guardado conservan su mensaje aunque se pueda comparar la copia disponible.
La vista previa de una actualización sigue sin habilitar el reemplazo de archivos.

Prueba física pendiente: tocar filtro repetidamente sin nueva consulta, búsqueda
sin coincidencias, cero recibos, candidatas, rutas ausentes, fallo de red con copia
guardada, cierre durante ambas fases y escala DJUI grande.


### Cabecera horizontal de detalles — 2026-10-30

Se revisaron las capturas de 16:11 y 16:12: el bloque vertical estrecho desplazaba
el selector y ocultaba información inicial. La identidad ahora ocupa el ancho
del cuerpo: miniatura a la izquierda, título y autor a su lado y versión/categoría
en una columna derecha cuando hay al menos 950 unidades lógicas disponibles.
Con menos ancho, versión y categoría pasan debajo del bloque imagen/título/autor.
La imagen mide 144 unidades, o 96 en anchos menores de 500. No se reduce el texto
para mantener columnas; la altura se calcula midiendo cada etiqueta envuelta.
Solo los párrafos mantienen el máximo de lectura de 1000 unidades. Selector,
separadores y pie fijo mantienen sus controles nativos y desplazamiento.

El detalle copia metadatos en widgets y conserva su propia solicitud de imagen
y lease del pool de texturas. El trabajador no conoce DJUI: el hilo principal
consume RGBA y publica en el slot estable. Se reutilizan descarga/caché de
miniaturas, sin otro formato persistido ni copia del catálogo. El cierre cancela
la solicitud y libera el lease; el pool conserva sus direcciones de memoria.
Fallo de imagen o agotamiento de slots muestra MOD sin bloquear la instalación.

Pendiente en teléfono: escala grande, títulos/autores/categorías largos, temas,
scroll completo, volver durante carga de imagen y reapertura repetida del detalle.
La versión de cabecera corresponde al catálogo; el selector indica la versión
del archivo elegido. La actualización por reemplazo sigue pendiente.


### Controles compactos del detalle — 2026-10-30

Instalar pasa a la cabecera, debajo de categoría, con ancho medido por la fuente
y traducción más márgenes. Conserva el estilo nativo de foco/hover y acento verde
adaptado a la superficie. La etiqueta de nueva versión continúa deshabilitada:
no se implementó reemplazo. Al desplazar la cabecera, el botón puede salir del
viewport; el pie conserva Volver/Cancelar y el estado de la operación.

El selector usa flechas ASCII < y > de 52 unidades con versión, posición y nombre
de archivo entre ellas. Las flechas desaparecen cuando solo hay un archivo.
La altura se mide al envolver el nombre. Los controles verticales nativos se
recolocan únicamente en workspaces MI: dos botones de 52 unidades separados,
agrupados a la derecha del pie, visibles solo en direcciones disponibles. No se
modifica djui_three_panel ni los otros menús del juego. No se incorporó WebP ni
un decodificador nuevo; las futuras texturas pueden sustituir el dibujo.

El pie normal mide 64 unidades lógicas. Su espacio de estado aparece ante una
operación, un error previo al inicio, archivo no admitido o aviso de actualización.
El mensaje listo para instalar deja de ocupar permanentemente espacio. En anchos
menores de 650, estado y Volver se distribuyen en filas; en anchos mayores quedan
uno al lado del otro. El progreso real permanece fijo mientras la cabecera se
desplaza; Cancelar se bloquea durante confirmación/finalización, como antes.

Se conserva el fallo de creación de operación en un flag de presentación hasta
reintentar o cambiar archivo: ya no desaparece en el siguiente frame. La operación
nativa sigue siendo la autoridad para estados, éxito y cancelación.

Prueba física pendiente: primer/último archivo, mod con un solo archivo, nombres
largos, foco con mando y direcciones ocultas, escala grande, hover/táctil, iniciar
desde cabecera y bajar/subir durante la instalación, cancelación y resultado.
