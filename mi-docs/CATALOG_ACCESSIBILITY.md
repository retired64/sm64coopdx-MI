# MI: tarjetas y lectura accesible

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
