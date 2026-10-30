# SM64CDPY: ideas para una edición integrada de CoopDX Android

Documento vivo de análisis y conversación.
Fecha inicial: 2026-10-29.
Estado: diseño y primer prototipo de catálogo DJUI implementado con autorización
del usuario. Las propuestas futuras no implican autorización para publicar ni distribuir.

Dirección más reciente del usuario: edición sm64coopdx-mi (Mods Integrations),
con administrador dentro de los menús del juego y sin Flutter en esa edición.
Las secciones 11, 12, 13 y 14 actualizan las alternativas iniciales; estas se conservan como contexto.
Alcance más reciente: MI presenta el catálogo remoto, instala y actualiza;
el juego conserva su biblioteca local, lectura de metadatos, refresco y activación.

## 1. Objetivo

Explorar una edición Android de CoopDX con catálogo, descargas, instalación,
activación, biblioteca y actualizaciones de mods integrados. Ofrecer una sola
experiencia al usuario conservando límites claros entre motor, administrador y
publicación del catálogo.

Repositorios locales de referencia:

- Administrador: ~/sm64cdpy.releases/
- Juego: ~/sm64coopdx/, rama android, origen ManIsCat2/sm64coopdx.

El análisis se basa en una inspección parcial de esos repositorios, no en una
prueba de compilación conjunta ni una auditoría completa de licencias.

## 2. Qué existe y qué potencial tiene

| Proyecto | Aportación actual observada |
|---|---|
| sm64cdpy.releases | Flutter, catálogo, descargas persistentes, instalación, biblioteca, actualizaciones y recuperación. |
| sm64coopdx, android | Motor, ejecución Lua, incompatibilidades, DynOS y multijugador. |

El contrato del catálogo ya separa identidad de mod, versión y archivo. La app
cuenta con Kotlin, WorkManager, extracción y seguimiento de operaciones. El
fork consume el catálogo publicado como contrato de datos externo.

Hallazgo favorable: platform/app/build.gradle del juego contempla compilar como
biblioteca mediante BUILD_AS_LIBRARY. Es un punto de partida para explorar la
integración, no una garantía de integración directa con Flutter y SDL.

El mayor beneficio de un fork conectado sería consultar las reglas del motor en
vez de replicarlas en el administrador.

### Ejemplo que motivó esta conversación

La Biblioteca mostraba cero mods activados y bloqueaba Bowser. Se inspeccionaron
los mods de /sdcard/com.maniscat2.sm64coopdx/user/mods/ y se encontró una línea de
816 caracteres en el encabezado de day-night-cycle/main.lua.

El administrador rechazaba líneas superiores a 800 caracteres y exigía que todos
los mods instalados fueran legibles para permitir nuevas activaciones. El juego
trunca líneas largas y comprueba incompatibilidades contra mods activados.

Se prepararon cambios en el administrador para corregir esa diferencia. Al
redactar este documento siguen pendientes generación l10n, compilación, ejecución
de pruebas y validación física por ausencia local de Flutter/Dart y wrapper de
Gradle. No se modificaron los archivos de mods ni la configuración del juego.

## 3. Opciones de producto

| Opción | Ventajas | Coste principal |
|---|---|---|
| Administrador independiente | Funciona con el juego existente y permite actualizaciones independientes. | Permisos, carpetas y sincronización indirecta. |
| Administrador y fork conectados | Estado confirmado por el motor y reutilización del administrador. | Diseñar y mantener un protocolo entre aplicaciones. |
| Un APK con administrador y juego | Instalación y experiencia más sencillas para el usuario. | Mayor complejidad de compilación, memoria y mantenimiento. |

Recomendación inicial: conservar los tres proyectos separados, mantener el
administrador independiente y experimentar con un fork pequeño conectado.
Después de demostrar la integración, evaluar empaquetar administrador y juego en
un solo APK. Una sola experiencia no exige un único repositorio.

## 4. Arquitectura propuesta

- El motor decide qué mods reconoce, cuáles están activados y cuáles son incompatibles.
- El instalador controla descargas, temporales, verificación y reemplazos.
- El catálogo informa versiones y archivos disponibles; no decide el estado real del juego.
- La interfaz solicita acciones y muestra resultados confirmados.

Un puente con versión de protocolo permitiría consultar mods y solicitar cambios.
Las operaciones del motor deben ejecutarse en su hilo y en momentos adecuados.
No se debe acceder arbitrariamente a estructuras C desde callbacks de Android.

Para aplicaciones separadas, habría que diseñar autenticación/autorización del
canal y compatibilidad de versiones. Todavía no se ha elegido entre servicio,
Binder, mensajes u otro mecanismo Android.

Para un APK integrado, la primera opción a probar sería Flutter para administrar
y una Activity SDL para jugar. No comenzar incrustando el juego en un widget
Flutter: primero resolver arranque, salida, audio, orientación y memoria.

Medir si conviene liberar el engine Flutter durante la partida. Mantener juego,
Flutter y engine del overlay simultáneamente podría perjudicar teléfonos modestos;
es una hipótesis de rendimiento pendiente de mediciones.

El juego consume únicamente el catálogo JSON publicado y sus URLs de descarga.
La producción de esos datos no forma parte de la arquitectura del fork.

## 5. Ideas de funciones

### 5.1 Perfiles de juego

Perfiles como MarioHunt, Bowser, Vanilla o Gráficos mejorados, con selección y
versiones de mods. Cambiar de perfil sin reinstalar todo. Definir posteriormente
si los ajustes y partidas guardadas se comparten o se separan, evitando pérdida
de datos.

### 5.2 Descargar y jugar

Elegir un mod, descargarlo e instalarlo, comprobar que el motor lo reconoce y
arrancar con el perfil correcto. No afirmar éxito antes de la confirmación real.

### 5.3 Actualizaciones reversibles

Conservar la versión anterior y permitir volver atrás. Si un mod está en uso,
preparar la actualización y aplicarla cuando sea seguro. El diseño de rollback,
espacio disponible y fallos durante reemplazos requiere un flujo específico.

### 5.4 Compatibilidad explicada

Mostrar conflictos concretos confirmados por el motor. Distinguir incompatibilidad
comprobada de compatibilidad desconocida. No convertir ausencia de datos en una
garantía de funcionamiento.

### 5.5 Colecciones compartibles

Exportar listas de mods, versiones, referencias y hashes para amigos. No asumir
permiso para redistribuir archivos. Estudiar luego su relación con salas y
requisitos de versiones del multijugador.

### 5.6 Diagnóstico útil

Recibir errores de carga del juego y mostrar el mod relacionado y una acción
concreta. Conservar detalle técnico copiable. Atribuir automáticamente cualquier
cierre a un mod necesitaría instrumentación y evidencia adicionales.

## 6. Mantenimiento y compatibilidad

Mantener el fork cerca de upstream. Priorizar integración, almacenamiento y
experiencia de usuario antes de cambiar red, Lua o formatos internos.

La compatibilidad multijugador se prueba; no se garantiza por conservar el nombre
del motor. Mantener un registro de versión upstream, cambios propios y protocolo
de integración.

Separar las actualizaciones del catálogo, de mods, del administrador y del motor.
Definir qué combinaciones son compatibles antes de automatizar actualizaciones.

La app independiente debe seguir siendo útil con el juego original mientras se
experimenta con la edición integrada.

## 7. Identidad, licencias y distribución

Presentación propuesta: una edición Android de CoopDX con la experiencia
integrada de SM64CDPY, conservando créditos y procedencia.

Crear un juego original independiente implicaría además recursos, contenido e
identidad propios; no basta con cambiar nombre o interfaz.

Se observó licencia MIT en el repositorio del administrador.
No se encontró una licencia general equivalente en la raíz del juego revisado.
Esto no es una auditoría completa: no asumir que el conjunto puede relicenciarse
como MIT. Revisar permisos, avisos y licencias de componentes, recursos y mods
antes de definir qué se distribuye. La ROM y los recursos del juego necesitan una
consideración separada de las herramientas propias.

GitHub distingue el permiso de ver y hacer fork de un repositorio público de los
derechos generales de reproducción, modificación y distribución.

Referencias consultadas:

- Rama Android del port: https://github.com/ManIsCat2/sm64coopdx/tree/android
- Licencias de repositorios: https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository
- Ports no oficiales, referencia upstream: https://github.com/coop-deluxe/sm64coopdx/discussions/1224

## 8. Primer prototipo recomendado

1. Arrancar el fork desde SM64CDPY.
2. Consultar al motor la lista real de mods y estado de selección.
3. Activar un mod mediante una solicitud confirmada por el motor.
4. Recibir el resultado de carga y mostrar errores comprensibles.
5. Probar vuelta al administrador, pausa, cierre y recuperación.
6. Medir consumo de memoria y tiempo de arranque antes de decidir un APK único.

Este prototipo comprobaría el beneficio esencial sin migrar todo el catálogo ni
reescribir la interfaz. Su alcance no incluye publicación ni integración completa.

## 9. Decisiones y preguntas abiertas

| Tema | Estado inicial |
|---|---|
| Mantener administrador independiente | Recomendado, pendiente de decisión de producto. |
| Fork pequeño conectado | Propuesta de primer experimento. |
| Un APK integrado | Evaluar después del prototipo y mediciones. |
| Protocolo entre administrador y motor | Sin diseñar. |
| Almacenamiento y migración de instalaciones | Sin diseñar; preservar datos existentes. |
| Perfiles y separación de partidas/ajustes | Idea pendiente de definición. |
| Compatibilidad multijugador | Pendiente de matriz de pruebas. |
| Licencias y paquete distribuible | Pendiente de revisión específica. |
| Nombre y marca de la edición | Sin decidir. |

## 10. Registro de conversación

### 2026-10-29 — Análisis inicial

El usuario plantea aprovechar los tres proyectos para crear un fork Android con
catálogo, activaciones, descargas y actualizaciones integradas. Se recomienda un
camino gradual: conservar proyectos separados, prototipo con comunicación real
con el motor y evaluación posterior del APK único.

El usuario pide conservar el análisis en /sdcard/ y continuar documentando ideas.
No se ha solicitado crear o publicar el fork en esta conversación.

### Próximas ideas

Agregar aquí nuevas propuestas, sus ventajas, costes, dudas y decisiones. Mantener
separadas ideas exploratorias, decisiones aceptadas y funciones implementadas.


## 11. Dirección propuesta por el usuario: sm64coopdx-mi

Fecha: 2026-10-29.
Nombre provisional: sm64coopdx-mi.
Significado propuesto de MI: Mods Integrations.
Estado: intención de producto expresada por el usuario; arquitectura por validar.

### 11.1 Experiencia buscada

El juego tendría su propia pantalla «Explorar mods» dentro de sus menús, con el
mismo sistema visual y controles del juego. El botón dejaría de abrir la web para
el flujo principal del catálogo y abriría una lista nativa con búsqueda, detalles,
descarga, instalación y acceso a los mods instalados.

El usuario aclara que no quiere Flutter en esta edición. El administrador actual
serviría como referencia funcional: adaptar y reescribir componentes al lenguaje
y arquitectura del juego, conservando el contrato JSON publicado.
La app independiente puede conservarse; no se ha decidido retirarla.

Esta dirección sustituye como propuesta principal la idea inicial de una pantalla
Flutter y otra Activity para jugar. La comparación anterior queda como historial.

### 11.2 Puntos reales de integración encontrados

- src/pc/djui/djui_panel_host_mods.c contiene djui_mod_website_open(), que llama a
  open_url("https://mods.sm64coopdx.com/mods/").
- En el mismo archivo, el botón BROWSE_MODS se conecta a ese callback. En el
  código revisado aparece cuando gNetworkType == NT_NONE; cualquier cambio para
  mostrarlo durante sesiones necesita una política explícita.
- Ese panel ya utiliza búsqueda, categorías y paginación DJUI. Son patrones
  nativos que pueden reutilizarse para diseñar el catálogo.
- src/pc/utils/json.hpp incluye nlohmann/json 3.12.0. Su presencia es una base
  posible para un módulo C++, pero no demuestra que el catálogo ya esté integrado
  ni que ese header se compile hoy en el destino Android.
- src/pc/update_checker.c contiene una implementación con libcurl fuera de
  Windows. Es una referencia de acceso HTTP; no constituye un gestor persistente
  de descargas y hay que comprobar su disponibilidad en la compilación Android.
- src/pc/mods/mods.c dispone de mods_refresh_local() y mods_enable();
  mods_utils.c implementa mods_update_selectable(). Su uso debe respetar el
  ciclo de vida del motor. Ninguna de estas funciones garantiza por sí sola
  recarga Lua segura durante una partida.

### 11.3 Arquitectura candidata sin Flutter

1. Interfaz DJUI en C: explorar, buscar, ver detalles, progreso, instalados y
   actualizaciones, usando el estilo y navegación del juego.
2. Servicio de catálogo en C++: leer JSON validado, proyectar identidades y
   versiones, mantener caché y entregar resultados acotados a DJUI mediante una
   interfaz C. Evitar cargar imágenes y todos los detalles simultáneamente.
3. Servicio de descargas e instalación: operaciones en background, progreso real,
   cancelación confirmada, temporales aislados, extracción validada y recuperación.
   Puede combinar C/C++ con Kotlin/Java para capacidades específicas de Android.
4. Motor: reconocimiento, selección e incompatibilidades, usando sus reglas y
   estados reales. Los cambios del motor se solicitan desde su hilo adecuado.
5. Fuente externa: el juego consume el catálogo JSON publicado sin depender de
   su proceso de producción.

«Nativo al juego» describe principalmente la interfaz y la integración con el
motor. No exige que todo sea C: servicios Kotlin/Java internos pueden conservar
valor para WorkManager, notificaciones y recuperación tras muerte del proceso,
sin mostrar una interfaz Android separada ni depender de Flutter.

Tampoco sería obligatorio compilar el juego como biblioteca para alojarlo en
Flutter: la opción BUILD_AS_LIBRARY era relevante para la alternativa anterior.
Para esta propuesta, el punto de partida es ampliar el propio juego Android.

### 11.4 Qué reutilizar y qué reescribir

| Componente actual | Tratamiento propuesto |
|---|---|
| JSON, IDs y contrato del catálogo | Reutilizar y versionar; conservar compatibilidad. |
| Fuente del catálogo | Conocer únicamente su contrato JSON publicado. |
| Pantallas Flutter, widgets y Riverpod | Reescribir como pantallas DJUI y estado propio del módulo. |
| Reglas de selección replicadas en el administrador | Usar las del motor, evitando otra copia. |
| Resolución de URLs en Dart | Adaptar al servicio nativo o ampliar el contrato con referencias estables resolubles; no asumir que URLs temporales pueden publicarse para siempre. |
| Workers, extracción y comprobaciones Kotlin | Evaluar reutilización quitando dependencias del plugin Flutter. |
| Recibos, migraciones y recuperación | Conservar conceptos y contratos; definir un formato que el fork pueda mantener. |
| Caché Hive y preferencias Flutter | No trasladarlas como dependencias; diseñar almacenamiento del fork y migración explícita si se necesita. |

No estimar esto como una conversión automática de lenguaje. La lógica existente
sirve de referencia, pero hacen falta nuevos límites entre UI, motor, red y disco.

### 11.5 Primer flujo vertical propuesto

Abrir «Explorar mods» desde el menú → leer una muestra o catálogo cacheado →
buscar y abrir detalles → descargar un ZIP soportado → instalar en ubicación
controlada → refrescar los mods reconocidos → permitir seleccionar con reglas
del motor → arrancar partida y confirmar resultado.

Primera etapa con juego fuera de sesión. Durante partidas se podría explorar o
preparar descargas más adelante, pero no reemplazar archivos de mods en uso ni
prometer activación en caliente. Definir cuándo se puede refrescar o requiere
reiniciar antes de ampliar ese comportamiento.

Las descargas HTTP del catálogo no deben confundirse con packet_download.c,
que pertenece al transporte del juego y requiere estudiar su finalidad antes de
cualquier reutilización.

### 11.6 Etapas sugeridas

1. Pantalla DJUI de catálogo de solo lectura con JSON de prueba, estados de
   carga/error/vacío y navegación táctil/mando.
2. Consulta remota y caché válida con versión de esquema.
3. Descarga e instalación de un formato soportado con progreso y cancelación.
4. Integración real con descubrimiento y selección del motor.
5. Recibos, actualizaciones reversibles y recuperación de operaciones.
6. Perfiles, colecciones y mejoras de experiencia posteriores.

### 11.7 Preguntas abiertas específicas

- ¿El menú principal tendrá entrada propia además del botón en Mods?
- ¿Qué parte de la app independiente se seguirá manteniendo?
- ¿Qué formatos y proveedores se soportarán en el primer prototipo?
- ¿Qué operaciones sobreviven a minimizar/cerrar el juego y cuáles se reanudan?
- ¿Qué almacenamiento evita permisos innecesarios y permite exportar o migrar?
- ¿Cómo diferenciar mods instalados por MI de mods copiados manualmente?
- ¿Cómo gestionar actualización del motor por separado del catálogo y mods?

### Registro de conversación

El usuario precisa que busca una integración en el menú real del juego, sin abrir
la web para explorar, con el JSON de su ecosistema y una reescritura nativa de la
experiencia del administrador. Propone el nombre sm64coopdx-mi. Se documenta esta
dirección sin modificar todavía el juego ni crear/publicar el fork.


## 12. Alcance refinado: obtener e instalar; el juego administra los mods

Fecha: 2026-10-29.
Estado: aclaración explícita del usuario sobre la dirección del producto.
Esta sección prevalece sobre propuestas previas que sugerían reconstruir una
biblioteca local o controles de activación dentro de MI.

### 12.1 División de responsabilidades

MI se enfoca en resolver cómo obtener los mods:

- Presentar el catálogo JSON remoto, con búsqueda y detalles.
- Identificar versiones y archivos instalables.
- Descargar, verificar e instalar automáticamente.
- Conservar recibos e información suficiente para reconocer actualizaciones.
- Actualizar con recuperación y sin reemplazar archivos en uso.
- Solicitar el refresco del juego cuando sea seguro o indicar que falta hacerlo.

El motor existente conserva sus responsabilidades:

- Descubrir y enumerar mods locales.
- Leer sus nombres, descripciones y demás datos que soporte.
- Presentar su lista y controles actuales.
- Activar y desactivar con sus propias reglas.
- Resolver incompatibilidades y cargar los mods.

No crear otra implementación de la biblioteca local, metadatos locales ni
selectores de activación. La interfaz de catálogo puede mostrar estados de
instalación/actualización, pero no se convierte en una segunda autoridad sobre
qué mods están activados. La disponibilidad remota y la selección local son
estados distintos.

### 12.2 Experiencia de instalación

El usuario pulsa «Instalar» y recibe un resultado comprensible. No tiene que
administrar ZIP, elegir un descompresor ni mover carpetas. La instalación es
automática como parte de la operación solicitada, no un segundo flujo manual.

Esto no implica que desaparezcan los comprimidos del transporte. Cuando el
proveedor entrega ZIP/7z u otro formato soportado, el instalador debe descargarlo,
validarlo y extraerlo internamente, limpiando temporales cuando sea seguro.
Si entrega un Lua suelto, se instala como archivo. No es necesario exponer el
archivo descargado como producto final en la interfaz.

No asumir que todos los catálogos o enlaces son instalables: formatos no
soportados, carpetas remotas sin inventario, descargas ambiguas y enlaces rotos
requieren un estado explícito y una salida útil, sin afirmar instalación exitosa.

### 12.3 Flujo vertical actualizado

Menú del juego → Explorar mods → Detalles → Instalar → Descargar/verificar →
Preparar archivos → Confirmar instalación → Refrescar lista local cuando sea
seguro → Abrir el menú existente de Mods para seleccionar y jugar.

La instalación no equivale a activación. Un mod recién instalado puede quedar
sin activar; el motor y sus controles existentes siguen decidiendo la selección.
Refrescar descubrimiento no garantiza recarga segura del código de una partida.

Para el primer prototipo, instalar desde un menú fuera de sesión. Estudiar el
contexto adecuado para llamar a mods_refresh_local() y reconstruir el panel
existente, evitando referencias a entradas anteriores durante el refresco.

### 12.4 Actualizaciones: responsabilidad propia de MI

Aunque el juego resuelve la gestión local, MI necesita conservar la procedencia:
ID de catálogo, versión, archivo y unidades/rutas instaladas. El nombre visible
no basta para reconocer una actualización.

Distinguir:

- Mod instalado por MI con recibo confirmado: actualización rastreable.
- Mod copiado manualmente: no asignarle versión o procedencia por parecido de título.
- Archivos locales modificados o compartidos: detectar conflictos antes de sobrescribir.

Una actualización puede cambiar nombres, añadir archivos o retirar otros. Definir
un manifiesto de propiedad, preparación aislada, recuperación ante fallo y
tratamiento de archivos del usuario. No borrar una carpeta completa suponiendo
que todo su contenido pertenece al catálogo.

Tras actualizar, dejar al juego refrescar y resolver la selección. Si cambia la
identidad física de un mod, comunicarlo y definir una migración explícita; no
prometer conservar la activación automáticamente sin comprobar ese caso.

### 12.5 Primer prototipo reducido

1. Pantalla DJUI que lee una muestra del JSON del catálogo.
2. Detalles y botón Instalar para un archivo soportado y conocido.
3. Operación real de descarga e instalación con salida de éxito/error/cancelación.
4. Refresco seguro y aparición del mod en el menú local existente.
5. Recibo mínimo que permita detectar una versión posterior.
6. Actualización controlada con recuperación de la versión anterior.

Quedan como ideas futuras, no como requisitos iniciales: perfiles, colecciones,
diagnóstico avanzado y reconstrucción de pantallas que el juego ya proporciona.

### Registro de conversación

El usuario precisa que el valor de MI está en presentar el catálogo e instalar y
actualizar automáticamente. El juego ya descubre mods, lee descripciones,
refresca y ofrece activación/desactivación. Se acepta esa separación como base
del diseño, evitando duplicar el motor y las pantallas de gestión local.


## 13. UX definida: instalar, asociar y actualizar sin gestionar comprimidos

Fecha: 2026-10-29.
Estado: aclaración explícita del usuario. Esta sección precisa y prevalece sobre
las propuestas anteriores respecto a comprimidos, asociación y acciones visibles.

### 13.1 Instalación como única acción de obtención

MI no ofrece «Descargar ZIP», «Guardar comprimido», exportación del archivo de
transporte ni un modo de descarga sin instalación. El usuario elige «Instalar».

Internamente, la fuente puede entregar un comprimido o un archivo suelto. MI:

1. Obtiene el archivo en un área temporal aislada por operación.
2. Comprueba la descarga y el tipo real del contenido.
3. Extrae de forma segura cuando corresponda o prepara el Lua/Luac suelto.
4. Valida la disposición que el motor reconoce, sin inventar una carpeta
   adicional que pueda ocultar el script al descubridor del juego.
5. Coloca la unidad en la ubicación correcta bajo user/mods.
6. Confirma los archivos y registra su asociación con el catálogo.
7. Elimina el comprimido y temporales cuando ya no sean necesarios, también
   ante fallos terminales o cancelación cuando sea seguro.

El resultado visible es el mod instalado. El comprimido no se conserva como
archivo del usuario. Si se ofrece recuperación de una actualización fallida,
puede conservarse temporalmente una copia interna de la instalación anterior;
esto es distinto de guardar el comprimido descargado.

ZIP/7z/RAR describen ejemplos de formatos de origen, no soporte ya implementado.
La app actual no tiene extracción RAR automática soportada según su contrato.
Para aceptar RAR en MI hay que integrar y validar un extractor compatible o
conseguir una fuente alternativa soportada. No prometer instalación de todos
los formatos por el mero hecho de listar un enlace.

El tratamiento de .luac requiere comprobar el descubrimiento del motor real:
no asumir que un .luac aislado aparece en el menú solo porque el juego pueda
usar bytecode dentro de otras estructuras. Definir los casos soportados antes
de modificar ubicación, extensiones o empaquetado.

### 13.2 MI registra instalaciones; no administra activación

La activación queda completamente en la pantalla existente del juego. No añadir
selectores, estados o avisos de activación a la UX de MI salvo una necesidad
concreta posterior. MI muestra instalación, asociación y actualización.

«Instalado frente a activado» sigue siendo una distinción interna necesaria para
no inferir estados ajenos, pero no es un problema de interfaz que MI deba volver
a presentar ni una pantalla que deba construir.

### 13.3 Recibos e identidad

Cada instalación del catálogo queda asociada a su unidad o unidades físicas:
carpeta, Lua suelto u otra estructura reconocida. Evitar asumir que un archivo
del catálogo siempre contiene una sola carpeta.

El registro interno debería conservar:

- Identidad estable del catálogo/mod, versión y archivo de origen.
- Rutas relativas y unidades instaladas, con un manifiesto de propiedad.
- Hashes de los archivos o manifiesto instalado para integridad y cambios locales.
- Fecha, estado confirmado de la operación y procedencia de la asociación.

Un hash ayuda a comprobar igualdad e integridad; por sí solo no demuestra qué
versión futura corresponde ni establece identidad universal entre catálogos.
Para buscar actualizaciones, usar los IDs y versiones publicados. Si la base
no ofrece información suficiente, representar esa limitación sin inventar una
versión ni usar únicamente el título visible.

El administrador Flutter sirve de referencia para recibos, IDs, manifiestos y
recuperación. Antes de reutilizar un mecanismo concreto, comprobar su
implementación: no asumir que todas las instalaciones actuales tienen hashes.

### 13.4 Asociar mods que el usuario ya tiene

Flujo propuesto: seleccionar una unidad local → buscar/elegir su entrada en el
catálogo → confirmar la asociación. No descargar, mover ni reemplazar archivos
solo por asociar.

La interfaz puede mostrar sugerencias por nombre o huella, pero una coincidencia
de título no basta para asociar automáticamente con certeza. Para esta función,
reutilizar el descubrimiento del juego al elegir unidades locales en vez de
construir otro escáner con reglas diferentes.

Separar dos hechos en el registro interno:

- Asociación confirmada por el usuario: «esta carpeta corresponde a este mod».
- Versión instalada comprobada: solo cuando existe evidencia suficiente.

Si no se conoce la versión local, no afirmar que está actualizado. Permitir
obtener la versión del catálogo con una sustitución explícita y protección de
archivos propios. La asociación manual nunca autoriza por sí sola a borrar
contenido no identificado ni a asumir propiedad de todos los archivos.

Ofrecer corregir o quitar una asociación sin borrar el mod. Si desaparece la
unidad local, reconciliar el registro en vez de seguir mostrando una instalación
confirmada inexistente.

### 13.5 Buscar y aplicar actualizaciones

Acciones principales propuestas:

- «Buscar actualizaciones»: consultar la base publicada y comparar instalaciones asociadas.
- «Actualizar» junto a cada mod con una versión disponible.
- «Actualizar todos» en el encabezado o pie de la pantalla.

Reutilizar el catálogo cacheado válido cuando la red falle, indicando cuándo fue
la última comprobación. No afirmar «Todo actualizado» si la consulta falló o
faltan versiones locales verificables.

«Actualizar todos» abarca las actualizaciones elegibles identificadas; los casos
ambiguos, archivos modificados o conflictos de propiedad requieren resolución
concreta. Mostrar progreso total y por mod, éxito/fallo individual y opción de
reintentar los fallidos. Una operación fallida no debe presentarse como éxito
global ni obligar a reinstalar los mods que ya se actualizaron correctamente.

Preparar cada actualización en aislamiento, evitar reemplazar archivos en uso y
revertir un reemplazo incompleto cuando sea posible. Guardar el nuevo recibo
solo tras confirmar el resultado. Comprobar nombres, rutas y propiedad también
cuando una versión cambia de carpeta o distribuye varias unidades.

La pantalla de actualizaciones no necesita duplicar la activación del juego.
Tras instalar o actualizar, integrar el refresco local en el contexto seguro del
motor para que su pantalla existente vea el resultado.

### 13.6 Dirección de interfaz

Una primera versión puede limitarse a dos áreas integradas con DJUI:

- «Explorar mods»: catálogo, búsqueda, detalles e Instalar.
- «Actualizaciones»: instalaciones asociadas, Buscar actualizaciones, Actualizar
  por mod, Actualizar todos y acceso a Asociar mod local.

Las etiquetas son propuestas en español; definir traducciones naturales antes
de implementar. Decidir la posición exacta de botones mediante un prototipo con
táctil y mando, evitando una pantalla recargada.

### Registro de conversación

El usuario confirma que los comprimidos son transporte temporal y no una función
expuesta. MI instala automáticamente y etiqueta las unidades con su procedencia
para futuras actualizaciones. Propone asociación manual de mods locales y una
pantalla con comprobación, actualización individual y actualización conjunta.
La activación y su pantalla permanecen enteramente a cargo del juego.


## 14. Reglas de instalación Lua/Luac y soporte RAR

Fecha: 2026-10-29.
Estado: reglas de producto aclaradas por el usuario; soporte RAR por implementar.

### 14.1 Lua suelto

Un archivo como boo2.lua se instala directamente en user/mods/boo2.lua.
No crear una carpeta adicional para ese caso: el juego reconoce el Lua suelto.
Preservar su contenido y encabezado. Una colisión con un archivo existente exige
consultar el recibo y la propiedad, sin sobrescribir silenciosamente un mod ajeno.

### 14.2 Paquetes que contienen Luac

Los .luac se conservan dentro de la estructura del paquete que entrega el autor,
con una carpeta de mod y main.lua como punto de descubrimiento. Ejemplo de
estructura, no un diseño que el instalador deba fabricar:

    user/mods/mi-mod/main.lua
    user/mods/mi-mod/logic.luac

main.lua contiene al inicio los metadatos del mod, como name y description,
según la estructura indicada por el usuario. Preservar el archivo y los demás
recursos/rutas del paquete. No generar main.lua, inventar metadatos ni renombrar
un .luac a .lua para forzar el descubrimiento. El punto de entrada real debe
conservar la lógica que el autor haya previsto; el encabezado por sí solo no
hace que el bytecode se ejecute.

Un .luac aislado que no venga con la estructura necesaria no se anuncia como mod
instalado y reconocido. Requiere una fuente/paquete válido o una explicación
accionable. No se introduce un tratamiento nuevo de activación: el juego sigue
a cargo de descubrir y gestionar el mod preparado correctamente.

### 14.3 RAR como formato de transporte soportado por MI

El usuario confirma que MI debe incorporar un extractor nativo con soporte RAR.
Esto es un requisito para el diseño, no una capacidad ya disponible ni una
selección de herramienta concreta.

Evaluar un componente mantenido y compatible con Android/NDK, su licencia de
integración y distribución, soporte RAR4/RAR5 y las variantes realmente admitidas.
Medir tamaño del binario, memoria y rendimiento antes de elegirlo. ZIP y 7z
mantienen sus extractores correspondientes o una implementación común validada.

El extractor se integra detrás del servicio de instalación, sin pedir que el
usuario instale una app externa. Extrae a un área temporal aislada, bloquea
rutas que salgan del destino y enlaces inseguros, aplica límites de espacio y
recursos, y permite cancelación. El destino final solo se confirma tras validar
el resultado. Limpiar el RAR y temporales al terminar cuando sea seguro.

Decidir explícitamente cómo se presentan archivos cifrados, multipartes,
corruptos o con métodos no soportados; no prometer soporte de todas las variantes
solo por aceptar la extensión .rar. Detectar el formato por contenido.

### Registro de conversación

El usuario confirma que boo2.lua va suelto en mods/, que los Luac deben llegar
como paquete con main.lua y sus metadatos, y que el fork debe integrar una
herramienta nativa para extraer RAR. Se documenta la regla sin modificar todavía
el juego ni elegir o instalar una biblioteca de extracción.


## 15. Mantenimiento del fork y sincronización con upstream

Fecha: 2026-10-29.
Estado: estrategia recomendada; no se han creado ramas, configurado automatismos
ni sincronizado el repositorio durante esta conversación.

### 15.1 Qué significa sincronizar

Una sincronización no rompe necesariamente MI. Puede producir conflictos de Git,
errores de compilación o cambios de comportamiento aunque el merge y la
compilación terminen bien. Git resuelve historia y texto; no certifica que el
catálogo, instalación, refresco o selección sigan funcionando.

Ejemplo: upstream conserva el nombre de una función de refresco pero cambia
cuándo puede ejecutarse. El adaptador puede compilar y aun así usarse en un
momento incorrecto. Otro caso: cambia la precedencia de rutas y el mod instalado
por MI queda oculto por una copia en otro origen.

### 15.2 Elegir una fuente primaria

La copia local usa origin=https://github.com/maniscat2/sm64coopdx.git y rama android.
Su historia reciente contiene merges de android-dev hacia android y de main
hacia android-dev. El usuario elige android como canal estable de referencia para MI. Esto no
sustituye la validación propia: registrar commits probados como base de cada edición.

Para este fork Android, tomar como upstream primario ManIsCat2/sm64coopdx, rama
android, mientras se mantenga esa elección de producto. Evitar integrar además
main/dev del proyecto principal por cuenta propia salvo necesidad deliberada:
el port ya incorpora cambios por su flujo y una segunda ruta aumenta la carga.

Registrar el SHA exacto de upstream usado en cada versión de MI, junto a la
versión propia y de los datos persistidos. Un nombre de rama mutable no basta.

### 15.3 Aislar las modificaciones propias

Crear un módulo propio, por ejemplo src/pc/mod_integrations/, con catálogo,
instalación, recibos y actualizaciones. DJUI sigue representando la interfaz.
No copiar el menú completo del juego ni reimplementar su escáner o activación.

Definir un adaptador pequeño hacia el juego para rutas de escritura, refresco en
contexto seguro, navegación y consulta de unidades reconocidas. La mayoría de
cambios upstream deberían afectar a ese adaptador y no a todo el catálogo.
No es una garantía: cambios de formatos o arquitectura pueden requerir más trabajo.

Puntos de contacto iniciales observados:

- djui_panel_host_mods.c: entrada de Explorar mods y navegación.
- mods.c/mods_utils.c: descubrimiento/refresco y acceso controlado al estado.
- fs/plataforma: rutas efectivas y permisos.
- Android.mk y Gradle: inclusión de fuentes, bibliotecas, empaquetado y ciclo de vida.
- Idiomas: nuevas claves MI con prefijo propio, conservando estructura upstream.

Android.mk enumera SRC_DIRS y obtiene fuentes de cada directorio. Un directorio
nuevo no se incorpora automáticamente sin ajustar la configuración. Proponer
un archivo de build propio incluido desde el build principal para reducir el
parche, verificando su integración real en Android/NDK.

Preferir un nombre/prefijo mi_ para símbolos propios y commits pequeños con una
sola finalidad. Evitar reformatear archivos de upstream o renombrarlos por marca.
Una opción de compilación MI permite comparar con/sin la integración, pero ambos
modos deben validarse; deshabilitar MI no garantiza compilación de la base.

### 15.4 Ramas propuestas

- upstream-android: referencia espejo de la rama fuente, sin desarrollo propio.
- mi-main: integración propia validada, protegida como rama de publicación.
- sync/android-<fecha>: rama desechable de integración del próximo cambio upstream.
- feature/mi-<tema>: trabajo funcional separado.

origin apuntaría al fork propio y upstream al port elegido. Estos nombres son
una propuesta futura; hoy origin del juego aún apunta a ManIsCat2.

Si la rama android del fork permanece como espejo sin modificaciones, Sync fork
puede usarse en ella. Integrar después hacia una rama de sincronización creada
desde mi-main. Evitar usar una sincronización forzada como procedimiento para
resolver conflictos de la rama que contiene MI.

Para la rama publicada, preferir merges de upstream: conservan la historia y
no obligan a reescribir commits públicos. Rebase puede usarse en ramas privadas
antes de compartirlas, con criterio; no es obligatorio para mantener un fork.

### 15.5 Flujo por actualización

1. Obtener cambios de upstream sin modificar la versión publicada.
2. Elegir y registrar un SHA candidato; leer cambios desde el SHA integrado anterior.
3. Crear sync/android-<fecha> desde mi-main e integrar ese candidato allí.
4. Resolver conflictos revisando intención y comportamiento, no aceptando todo
   «ours» o «theirs» automáticamente.
5. Revisar los puntos de contacto aunque no hayan generado conflictos.
6. Compilar la edición Android y, si existe, el modo sin MI.
7. Ejecutar pruebas de contrato y generar APK de prueba.
8. Probar flujos clave en dispositivo; solo después integrar en mi-main.
9. Publicar únicamente mediante decisión explícita, con registro de base upstream.

Git rerere puede recordar resoluciones anteriores de conflictos y reducir
repeticiones; las resoluciones reutilizadas siguen necesitando revisión y pruebas.
No se ha activado en el repositorio.

### 15.6 Qué automatizar

Un workflow puede detectar cambios y preparar una rama/PR de actualización,
mostrar archivos sensibles modificados y ejecutar builds/tests. No proponer
publicación ni merge automáticos del motor por el mero hecho de compilar.

Alertas por cambios en DJUI, mods, filesystem, configuración, Android/SDL,
compilación, versión de red y dependencias. Fijar toolchains y dependencias
reproducibles; evitar actualizar todo a latest durante una sincronización.

Una revisión periódica detecta cambios; el ritmo de adopción depende de pruebas,
importancia y recursos. Evitar acumular meses de divergencia y evitar prometer
compatibilidad el mismo día que upstream publica una versión.

### 15.7 Pruebas necesarias para detectar inconsistencias

Contratos automatizables con archivos de prueba:

- JSON soportado, campos opcionales, rechazo de esquema desconocido y caché válida.
- ZIP/7z/RAR soportados, Lua suelto, main.lua con recursos y rutas peligrosas.
- Cancelación, archivo truncado, espacio insuficiente, fallo de extracción y recuperación.
- Instalación/actualización, manifiesto de propiedad, archivos modificados y rollback.
- Recibos persistidos y migraciones desde versiones MI anteriores.
- Adaptador: destino real, descubrimiento y refresco del juego después de instalar.

Pruebas físicas de APK:

- Abrir/cerrar Explorar mods con táctil y mando, carga/error/red y navegación.
- Instalar Lua suelto y paquete, encontrarlos en la lista existente y seleccionarlos.
- Actualizar uno/todos, preservar unidades ajenas y comprobar recuperación tras fallo.
- Reiniciar/minimizar/cerrar proceso y revisar estado recuperado.
- Comprobar el comportamiento multijugador según las versiones soportadas.

### 15.8 Datos y versiones independientes

Separar versión del motor upstream, versión MI, esquema de catálogo y esquema de
recibos. El catálogo puede actualizarse sin recompilar el juego mientras conserve
el contrato. Un esquema futuro no se interpreta a ciegas: conservar la última
copia válida y comunicar el estado apropiado.

Los recibos y asociaciones requieren migraciones, no borrado al actualizar APK.
Permitir recuperar el software anterior no implica que pueda leer una base ya
migrada; planificar copia/compatibilidad de datos para rollback.

Mantener applicationId y certificado propios y estables desde la primera versión
pública para la continuidad de actualizaciones. No intentar sustituir el APK del
port original con otra firma. Diseñar migración de datos entre ambas instalaciones
si se quiere coexistencia, sin compartir simultáneamente archivos en uso.

### 15.9 Criterio de mantenimiento efectivo

Mantener pequeño el parche sobre upstream, concentrar compatibilidad en un
adaptador, revisar cambios semánticos y promover solo versiones probadas.
No existe la garantía de «sincronizar siempre sin romper nada». El objetivo es
que los cambios sean localizables, verificables y recuperables.

Referencias:

- https://docs.github.com/en/pull-requests/how-tos/work-with-forks/syncing-a-fork
- https://git-scm.com/docs/git-merge
- https://git-scm.com/docs/git-rerere


## 16. Decisión: fuente de mantenimiento de MI

Fecha: 2026-10-29.
Estado: dirección de mantenimiento confirmada por el usuario.

### Fuente elegida

Repositorio: https://github.com/ManIsCat2/sm64coopdx
Rama: android.

MI toma los cambios de esa rama como su fuente estable de referencia. No se
sincroniza directamente con android-dev ni con main/dev del repositorio PC.
Se espera a que el mantenedor del port incorpore y publique esos cambios en
android, y después se integran mediante el flujo de prueba descrito en la sección 15.

El usuario entiende android-dev como el espacio de experimentación y adaptación
previo a android. La historia local revisada muestra merges de android-dev hacia
android y de main hacia android-dev; eso es evidencia del flujo observado, no
una garantía formal sobre todas las decisiones del mantenedor.

### Política operativa acordada

- Vigilar android y registrar el SHA upstream integrado en cada versión MI.
- Mantener una rama espejo sin modificaciones MI y una rama propia validada.
- Integrar novedades en una rama sync; revisar, compilar y probar antes de promover.
- No publicar ni actualizar automáticamente la edición MI solo porque haya
  nuevos commits en android.
- Conservar módulos MI aislados y sus adaptadores pequeños hacia el juego.
- Revisar rutas, descubrimiento/refresco, DJUI, Android/SDL, dependencias y red
  cuando upstream los modifique, aunque el merge no produzca conflictos.
- Preservar recibos, asociaciones y datos del usuario mediante migraciones explícitas.

Cualquier incorporación excepcional desde android-dev u otra fuente se decide y
documenta por separado; no forma parte de la sincronización ordinaria.

Esta confirmación integra la estrategia al diseño del fork. No crea ni modifica
ramas, remotos, workflows o publicaciones de los repositorios actuales.


## 17. Opinión de producto, sugerencias y correcciones de alcance

Fecha: 2026-10-29.
Estado: sugerencias documentadas con aclaraciones expresas del usuario.
Esta sección precisa las anteriores cuando existe diferencia de enfoque.

### 17.1 Valor del producto

MI tiene un alcance concreto: obtener e instalar mods y mantenerlos actualizados
dentro del juego. El catálogo aporta los datos, MI entrega los archivos y el
motor conserva descubrimiento, selección y ejecución. El administrador Flutter
sirve de referencia y puede seguir siendo útil como producto independiente
mientras el fork madura.

El criterio de éxito propuesto: una persona puede instalar y actualizar varios
mods desde el juego, entender un fallo de instalación y recuperarse sin abrir un
gestor de archivos.

### 17.2 Actualizar es una decisión consciente del usuario

El usuario aclara que quien pulsa Actualizar o Actualizar todos es consciente del
reemplazo solicitado. MI no debe imponer una política que impida esa elección por
suponer que el usuario necesita conservar versiones para jugar con amigos.

No pedir confirmaciones repetitivas para el reemplazo normal ya solicitado ni
construir advertencias genéricas alrededor de cada actualización. Presentar
claramente la versión disponible y ejecutar la acción elegida con su progreso y
resultado. Esto no autoriza a borrar archivos ajenos a la operación solicitada.

La sugerencia previa de «actualizaciones conservadoras» queda ajustada: la
conservación se refiere a integridad técnica y recuperación ante fallos, no a
tutelar la decisión del usuario ni a exigirle justificar que desea actualizar.

Las acciones acordadas siguen siendo Buscar actualizaciones, Actualizar y
Actualizar todos. No se ha solicitado añadir actualización automática al iniciar;
si en el futuro se propone, tratarla como una preferencia de producto explícita.

Si el motor no permite reemplazar archivos en uso, respetar ese límite técnico y
preparar/aplicar la operación en un contexto seguro, sin presentar el problema
como una advertencia de compatibilidad entre amigos.

### 17.3 La compatibilidad de ejecución pertenece al juego

El usuario aclara que el juego ya gestiona incompatibilidades, carga de scripts,
errores y logs. MI no debe duplicar ese diagnóstico, certificar mods ni bloquear
instalaciones por una supuesta incompatibilidad inferida por el catálogo.

No construir un sistema adicional de validación de scripts ni hacer de MI el
responsable de que un mod de terceros funcione durante una partida. Reutilizar
los mecanismos existentes del motor y sus pantallas/logs para esa parte.

El límite operativo de MI es entregar correctamente lo solicitado: descarga
íntegra, extracción segura, destino adecuado, recibos fieles y actualizaciones
sin daños causados por el instalador. Comprobar eso no equivale a comprobar la
lógica Lua, conflictos de gameplay o compatibilidad multijugador.

Un error de red, extracción o escritura se comunica como fallo de instalación de
MI. Un error de script o ejecución se trata mediante el sistema existente del
juego. No convertir un error de ejecución posterior en una descarga fallida ni
prometer funcionamiento universal como resultado de instalar.

Esta división es de arquitectura y producto; no constituye una conclusión legal
sobre responsabilidad o distribución.

### 17.4 Sugerencias de interfaz conservadas

- Entrada «Explorar mods» integrada en DJUI.
- Catálogo con búsqueda y detalles, y área de Actualizaciones.
- Acciones Instalar, Instalado y Actualizar según información confirmada.
- Buscar actualizaciones y Actualizar todos con progreso total y por mod.
- Mensajes de fallo de instalación concretos y opción de Reintentar.
- Mantener intacta la función de la pantalla local del juego.
- Asociación manual accesible como herramienta secundaria, sin sobrecargar la
  instalación normal del catálogo.

Si una asociación manual no permite conocer la versión instalada, registrar el
vínculo sin inventar una versión. Eso no impide ofrecer obtener e instalar una
versión elegida del catálogo mediante una acción clara.

### 17.5 Riesgo técnico principal y secuencia sugerida

La mayor complejidad está en actualizar correctamente paquetes existentes: pueden
cambiar de carpeta, distribuir varias unidades o coexistir con archivos del
usuario. Priorizar manifiestos de propiedad, preparación aislada, recibos
confirmados y recuperación, sin añadir fricción al reemplazo normal.

Secuencia de desarrollo sugerida, todavía sin implementación autorizada:

1. Catálogo, búsqueda y detalles en DJUI.
2. Instalación real con progreso, cancelación y errores accionables.
3. Refresco seguro para encontrar el resultado en la lista existente del juego.
4. Actualización individual confiable y recuperación ante fallo.
5. Asociación manual y Actualizar todos.
6. Refinamientos de navegación e imágenes; perfiles y colecciones como ideas posteriores.

La secuencia ordena el trabajo; no elimina Actualizar todos ni asociación local
del alcance de producto ya discutido.

### Registro de conversación

El usuario pide conservar las sugerencias y corrige dos supuestos: actualizar es
una decisión consciente de reemplazo y MI no necesita resolver compatibilidad
ni errores de scripts que ya administra el juego. Se incorporan esas correcciones
como criterio vigente, sin cambios en el código de los repositorios.


## 18. Primer MVP del fork: catálogo, instalación y actualización individual

Fecha: 2026-10-29.
Estado: plan inicial documentado a petición del usuario. No implica que el fork,
las ramas, el build o las funciones descritas ya se hayan creado.

### 18.1 Principio confirmado: no duplicar lo que ya hace el juego

MI incorpora obtención e instalación de mods y gestión de sus actualizaciones.
Reutiliza el descubrimiento, metadatos locales, lista, refresco, selección,
activación/desactivación, incompatibilidades y errores de scripts del motor.

No construir una segunda pantalla de activación, un segundo escáner local ni
otra validación de gameplay. Comprobar en una prueba que el resultado aparece en
el menú existente es una verificación de integración, no una función nueva que
haya que implementar. Si el flujo actual ya refresca en el momento adecuado,
reutilizarlo sin añadir un refresco redundante. Si hace falta conectar el final
de instalación con ese flujo, hacerlo mediante el adaptador y en contexto seguro.

### 18.2 Objetivo del MVP

Desde una pantalla nativa del juego, explorar el catálogo, instalar un mod y
actualizarlo individualmente. El resto del uso del mod continúa en los menús
existentes del juego.

Hito demostrable: abrir MI, instalar un ZIP conocido, comprobar su aparición por
el flujo existente del juego y actualizarlo desde una segunda versión de prueba.
También verificar error, cancelación y recuperación, no solo el caso exitoso.

### 18.3 Etapa 0: establecer una base reproducible

Después de crear el fork en la cuenta del usuario:

1. Partir de ManIsCat2/sm64coopdx, rama android, y registrar el SHA exacto.
2. Compilar e instalar esa base sin modificaciones propias.
3. Registrar herramientas y procedimiento que produjeron el APK probado.
4. Conservar una rama espejo y una rama MI propia; aplicar la estrategia de
   sincronización de las secciones 15 y 16.
5. Separar desde el inicio la identidad del APK MI si debe coexistir con el port,
   comprobando rutas efectivas y migración de datos antes de dar por hecho que
   comparten ubicación. No tocar silenciosamente la instalación existente.

Criterio: poder reproducir el arranque del juego base antes de atribuir fallos a MI.

### 18.4 Etapa 1: pantalla DJUI con muestra del catálogo

Conectar la entrada Explorar mods con una pantalla propia, manteniendo el resto
del menú y sus responsabilidades.

Contenido mínimo:

- Lista paginada o acotada.
- Búsqueda.
- Detalles básicos.
- Botón Instalar y navegación Atrás.
- Estados de carga, lista vacía y error.

Usar primero una muestra pequeña del contrato JSON publicado.
Validar disposición, táctil y mando. No introducir Flutter. Las imágenes y el
acabado visual avanzado pueden esperar.

### 18.5 Etapa 2: consumir el catálogo publicado

Adaptar IDs, nombres, descripción, versiones y archivos al módulo C/C++.
Consultar el catálogo remoto sin bloquear el hilo del juego y conservar la
última copia local válida. Validar esquema antes de sustituir la caché; un fallo
de red o formato no debe destruirla ni producir una lista engañosamente vacía.

El contrato externo del APK es el JSON publicado. Reutilizar referencias estables
de descarga y adaptar los
resolutores necesarios, sin asumir que todos los enlaces del catálogo son URLs
directas listas para el instalador.

### 18.6 Etapa 3: instalación vertical real

Primeros casos controlados:

- Lua suelto: user/mods/archivo.lua.
- ZIP con carpeta de mod: estructura reconocida bajo user/mods/.

Flujo:

    Instalar
      → descargar a temporal aislado
      → verificar contenido y descarga
      → extraer si corresponde
      → preparar y validar rutas
      → colocar archivos en el destino efectivo del juego
      → confirmar instalación y guardar recibo
      → limpiar temporales
      → mostrar resultado

Incluir desde esta etapa progreso, exclusión de dobles pulsaciones, cancelación
confirmada, errores accionables y salida de todos los estados de carga.
No exponer Descargar comprimido ni conservar el ZIP como archivo del usuario.
Ante una colisión con archivos ajenos, no sobrescribir sin identificar propiedad.

ZIP es el primer formato del prototipo. 7z y RAR siguen en el alcance del producto
y se incorporan detrás del mismo servicio, con extractores probados. Luac se
mantiene dentro de paquetes con main.lua según la sección 14. No presentar los
formatos pendientes como soportados en la interfaz.

### 18.7 Etapa 4: integración mínima con los flujos existentes

En el primer MVP, instalar desde el menú fuera de sesión. Resolver el destino
mediante el adaptador del juego. Reutilizar el flujo existente de refresco y
reconocimiento, sin mantener una segunda lista local con reglas propias.

La validación física comprueba que el juego enumera la unidad instalada y que
sus controles existentes siguen funcionando. No añadir selectores MI ni convertir
esa prueba en una reimplementación del sistema de mods.

### 18.8 Etapa 5: actualización individual

Guardar desde la primera instalación:

- ID estable del mod, versión y archivo del catálogo.
- Unidades y rutas instaladas con propiedad identificable.
- Información de integridad cuando corresponda.
- Estado confirmado de la instalación.

Añadir Buscar actualizaciones y Actualizar por mod. Usar dos versiones
controladas para probar la sustitución. Preparar el reemplazo de forma aislada,
proteger archivos ajenos y confirmar el nuevo recibo solo al terminar.

Si falla, preservar o recuperar la instalación anterior cuando sea posible y
ofrecer reintento. No pedir confirmaciones reiteradas para el reemplazo normal
solicitado ni añadir validaciones de scripts que pertenecen al juego.

### 18.9 Criterios de aceptación del primer MVP

- La base Android y el APK MI se compilan con un procedimiento conocido.
- Explorar mods usa DJUI y datos del contrato JSON publicado.
- Catálogo remoto y caché funcionan con carga, error y ausencia de red.
- Un Lua suelto y un ZIP soportado se instalan en la ubicación correcta.
- El archivo de transporte no se conserva como descarga del usuario.
- Cancelar o fallar libera la interfaz y no deja una instalación anunciada como exitosa.
- El descubrimiento y controles existentes del juego funcionan con el resultado,
  sin pantallas ni lógica duplicadas de activación.
- Una segunda versión se detecta y actualiza individualmente mediante su recibo.
- Fallar durante actualización no borra archivos ajenos ni oculta el fallo.
- Reabrir conserva el registro confirmado y reconcilia operaciones interrumpidas.
- La lista de formatos/proveedores soportados coincide con lo probado.

### 18.10 Después del primer MVP

Siguiente etapa: Actualizar todos y asociación manual de mods locales, reutilizando
el instalador, recibos y actualización individual. Completar soporte 7z/RAR y
proveedores adicionales según las capacidades probadas. No mezclar variantes no
soportadas con instalaciones anunciadas como automáticas.

Después: imágenes, refinamientos de navegación y presentación. Perfiles,
colecciones y diagnóstico adicional siguen como ideas futuras, no requisitos
para demostrar el primer flujo vertical.

### Registro de conversación

El usuario pide incorporar el plan inicial del MVP y reafirma que no se debe
reinventar la gestión de mods del motor. Se documenta un recorrido de base
compilable, pantalla de catálogo, datos reales, instalación y actualización
individual, reutilizando los flujos del juego. No se ejecutan cambios en los
repositorios ni publicaciones como parte de esta documentación.


## 19. Ejecución: primer prototipo de catálogo DJUI

Fecha: 2026-10-29. Fork: ~/sm64coopdx-MI/, rama mi-main.

Tras confirmar el usuario que su APK base funciona, autorizó implementar el
siguiente paso. Ya existe una pantalla DJUI conectada al botón de explorar:
catálogo schema 2 de ocho muestras, búsqueda, paginación y detalles. Instalar
permanece deshabilitado y se identifica la muestra; no hay red, instalación ni
actualización en esta etapa. No se replica gestión local del juego.

El lector vive separado en src/pc/mod_integrations/, usa el JSON vendorizado y
expone una interfaz C. No tiene persistencia ni altera selección de mods. Los
controles y memoria pertenecen al panel y se liberan al salir.

Pruebas nativas, validación del contrato JSON de la muestra, compilación Termux y
verificación de firma/alineación/contenido del APK correctos. La navegación y
presentación en dispositivo siguen pendientes de probar. No se ha ejecutado
la vía Gradle ni se han creado commits/releases como parte de esta entrega.

Detalles y matriz manual: [mi-docs/CATALOG_PREVIEW.md](CATALOG_PREVIEW.md).
Siguiente paso: catálogo remoto y caché válida, después instalación Lua/ZIP.


## 20. Prueba del prototipo: idiomas existentes y escala del menú

Fecha: 2026-10-29.

El usuario confirmó que restaurar ajustes resolvió los controles táctiles del
juego. Se verificó que mi-main procede de android (99c8d21ab), sin incorporar
android-dev/newtouch. No se modificó el motor táctil como parte de esa revisión.

La prueba reveló dos problemas adicionales: TITLE por claves ausentes en los
idiomas externos antiguos y botones inferiores inaccesibles al ampliar DJUI.
Se añade respaldo de traducciones MI y desplazamiento reutilizable para menús
estándar, conservando la escala elegida y la gestión local del juego. No se
sobrescriben archivos de idioma o ajustes personales.

Las regresiones automatizadas de idiomas y geometría pasaron; la interacción
del parche en dispositivo sigue pendiente. El siguiente avance del catálogo
remoto permanece después de validar este prototipo.

Detalles: [mi-docs/DJUI_VIEWPORT_AND_TRANSLATIONS.md](DJUI_VIEWPORT_AND_TRANSLATIONS.md).


## 21. Confirmación del usuario: parche DJUI funcional

Fecha: 2026-10-29. El usuario confirma que funcionó el APK con respaldo de
traducciones MI y desplazamiento para los menús a escala aumentada. Se registra
validación general del parche en su teléfono; no se presume cobertura exhaustiva
de la matriz de idiomas, temas y mando.

Siguiente etapa acordada en el roadmap: consumir el catálogo remoto con caché
válida. La instalación y las actualizaciones siguen como etapas posteriores.


## 22. Integración del catálogo remoto — 2026-10-29

MI consume el JSON real mediante un servicio nativo asíncrono, con última copia
válida y respaldo, reutilización por 15 minutos y actualización manual. Conserva
la búsqueda y detalles dentro del juego. Instalar sigue deshabilitado hasta
completar el flujo de instalación con resultado real y recibos. No se duplica
la pantalla de activación.

Detalles y pruebas pendientes: [mi-docs/CATALOG_REMOTE.md](CATALOG_REMOTE.md).


## 23. Catálogo como pantalla amplia — 2026-10-29

Se aprovecha el espacio del dispositivo con cabecera y pie fijos, filas con
metadata y detalles amplios. La capacidad por página responde a la altura
DJUI disponible; no se altera la escala del usuario. Explorar mods se traslada
a la cabecera derecha del menú local. Se conservan sus flechas y motor local.

Diseño y matriz física pendiente: [mi-docs/CATALOG_WORKSPACE.md](CATALOG_WORKSPACE.md).


## 24. Revisión de capturas: accesibilidad visual

Se priorizan cuatro tarjetas grandes en cuadrícula 2x2, una columna si la escala
reduce el ancho útil, texto mayor y detalle alineado arriba. La escala DJUI se
respeta sin reducir títulos automáticamente por falta de altura. Se mantiene
la navegación nativa y el control local del juego.

Arquitectura y validación: [mi-docs/CATALOG_ACCESSIBILITY.md](CATALOG_ACCESSIBILITY.md).


## 25. Migración futura del formato del catálogo — 2026-10-29

El usuario quiere reducir el peso del catálogo y contempla sustituir el JSON
por un archivo .db u otro formato. El motivo principal es tamaño de descarga
y almacenamiento; el funcionamiento actual ya se percibe rápido. Esta decisión
se registra como trabajo futuro: no cambia el formato, endpoint ni código actual.

### Flujo vigente como punto de partida

El catálogo de producción no está empaquetado en el APK. Explorar mods obtiene
el JSON completo por HTTPS desde el repositorio sm64cdpy.releases, valida su
estructura y guarda una copia local con respaldo. Una copia de menos de 15 minutos
se reutiliza; Actualizar catálogo fuerza la consulta. Si hay copia válida se
conserva ante errores de red o publicación. Búsqueda y paginación son locales,
sin solicitudes por tarjeta o página. Descargar catálogo no descarga los mods.
La muestra sintética del proyecto es material de regresión, no la fuente de la
pantalla actual. La consulta verificada tenía 1307 mods y aproximadamente 6,2 MB;
son cifras históricas, no límites del catálogo futuro.

### Opciones a evaluar, sin asumir una reducción automática

La extensión .db por sí sola no reduce peso. SQLite es un candidato por sus
consultas e índices locales, pero el tamaño depende del esquema, textos e índices.
Comparar con datos equivalentes: JSON actual, JSON comprimido para transferencia,
una publicación que excluya campos innecesarios para el juego y SQLite, tanto
en tamaño de transferencia como en tamaño almacenado, tiempo de apertura y RAM.
Medir también búsqueda y paginación en un teléfono Android representativo.
Compresión de transferencia y formato local son decisiones independientes.

Una posibilidad es conservar el JSON como contrato externo y preparar una
representación local optimizada para el juego. Todavía no se elige esquema ni
tecnología; cualquier formato de consumo futuro debe mantener compatibilidad.
Las actualizaciones incrementales del catálogo son otra mejora posterior, con
mayor complejidad; no confundirlas con actualizar los archivos de los mods.

### Condiciones para una migración mantenible

Preservar IDs de mods/versiones/archivos, descripciones e idiomas y metadatos
necesarios para descargar, instalar, asociar y actualizar. Adaptar el acceso a
datos manteniendo el contrato de presentación del menú para evitar rehacer la UI.
Versionar formato y esquema; publicar metadatos de versión, tamaño y hash del
artefacto para verificarlo antes de reemplazar la copia local. Mantener HTTPS,
validación, trabajo fuera del hilo UI, reemplazo atómico y último respaldo válido.
Planear transición desde la caché JSON y convivencia temporal con clientes antiguos.
Si se elige SQLite, abrir el catálogo externo como datos de solo lectura y validar
esquema/integridad antes de adoptarlo. No ejecutar migraciones provenientes del
contenido descargado.

El cambio debe mantener la experiencia sin conexión y no duplicar activación o
lectura de mods locales del juego. Se realizará cuando corresponda en el roadmap;
no bloquea continuar el MVP de instalación con el catálogo JSON vigente.


### Confirmación del usuario — 2026-10-29

El usuario confirma que la nueva visualización funcionó a la perfección y
resultó muy accesible en su teléfono. Se registra aceptación general del diseño
de tarjetas y lectura. No se presume cobertura individual de toda la matriz
de mando, teclado, idiomas, temas y estados de red. Las notas anteriores de
prueba física pendiente reflejan el estado previo a esta confirmación.


## 26. Reglas operativas y mantenimiento reproducible — 2026-10-29

[MANTENER-UPDATES.md](MANTENER-UPDATES.md) define el flujo actual recomendado;
[RULES.md](RULES.md) registra contratos, puntos de contacto y los tres commits
MI publicados. Revisar comportamiento aunque Git no indique conflictos y aunque
compile: UI/input, rutas, ownership, TLS/caché, idiomas antiguos y empaquetado.

Las referencias a remotos/configuración en secciones anteriores son históricas.
Actualmente origin apunta al fork retired64/sm64coopdx-MI; upstream todavía no
está configurado. En esta tarea solo se documenta, no se sincroniza ni se publica.


## 27. AGENTS.md como entrada de trabajo — 2026-10-29

[AGENTS.md](../AGENTS.md) define cómo trabajar en todo el fork: lectura de reglas y
procedimiento, accesibilidad visual, UX, estructura nativa, estado/hilos, pruebas
y mantenimiento frente a Android estable. Enlaza los planes e historial existentes
sin reemplazarlos ni presentar funcionalidades pendientes como implementadas.
Su creación no configura hooks, CI ni remotos; esos pasos mantienen su propio
seguimiento y requieren comprobar su estado real.


## 28. Upstream preparado efectivamente — 2026-10-29

Se configura el clon con upstream/android estable, push predeterminado al fork,
bloqueo local de push upstream, pull ff-only y rerere sin autoupdate. Se verifica
por red que la estable continúa en 99c8d21ab: nuestra base ya la contiene y no
requiere merge. Se mantiene mi-main en 76b961277 durante la preparación.

[UPSTREAM_SETUP.md](UPSTREAM_SETUP.md) documenta el estado y la herramienta de
configuración por clon; el [registro](upstream/2026-10-29-setup.md) distingue
preparación de una sincronización real. CI y protecciones siguen pendientes.


## 29. Workflow Android sin baserom — 2026-10-29

Se prepara mi-actions-build.yml manual para APK ARM64 con firma de prueba y
artefactos, sin descargar ni extraer ROM. La rama carga los recursos originales
desde la ROM del usuario al arrancar; la explicación previa de necesitar extraer
ROM en CI no describe este port. Se corrige la ruta de copia de recursos Gradle.

[MI_ACTIONS_BUILD.md](MI_ACTIONS_BUILD.md) registra flujo y validaciones locales.
Primera ejecución Ubuntu/Gradle y prueba física pendientes; no se declara un build
CI exitoso antes de ejecutarlo. Publicación y rama predeterminada son pasos explícitos.


## 30. Primera compilación Android en Actions completada

2026-10-30 UTC. El fallo de :app se resolvió con entrada Gradle standalone y
preflight. Run 41013511475 sobre e7d0ac815 completó build, firma, validación y
subida del artefacto sin ROM. Quedan avisos heredados no bloqueantes de Makefile.

El usuario canceló la descarga local y pidió continuar la documentación; no se
reanudó ni se afirma una verificación física/local del APK nuevo.
[Registro completo](ci/2026-10-30-first-success.md).


## Instalación Lua/ZIP local — 2026-10-30

Se implementa el primer flujo directo del catálogo: selección de archivo/versión,
descarga temporal, extracción nativa con miniz existente, entrega sin reemplazo,
recibo y recuperación. Diseño de progreso reutiliza DjuiProgressBar; activación y
Refrescar permanecen en el juego. Alcance y límites: [CATALOG_INSTALLATION.md](CATALOG_INSTALLATION.md).
Se conserva el contrato JSON externo y la integración nativa con el motor.
Todavía no hay actualizaciones ni RAR/7z. Cambios solo locales por instrucción del
usuario: sin commit, push ni Actions. Prueba física del instalador pendiente.


## Actualizaciones internas sin saturar la interfaz — 2026-10-30

Decisión del usuario: no crear una lista de instalaciones MI; mantener versiones y
asociaciones internamente y mostrar solo las actualizaciones que correspondan.
La autoridad remota es el catálogo, sin depender de versiones escritas en main.lua.
Se documenta una primera implementación solo en el fork y basada en el JSON: [CATALOG_UPDATES_PLAN.md](CATALOG_UPDATES_PLAN.md). Incluye fechas
ambiguas, variantes, recibos antiguos, borrado manual y reemplazo con respaldo.
No detecta con certeza binarios reemplazados bajo los mismos IDs sin hash remoto.
El juego solo puede detectar cambios disponibles en el JSON publicado.
Todo este flujo de actualización sigue pendiente;
esta entrada no modifica código ni publica catálogos.


## Consulta interna y tarjetas con miniaturas — 2026-10-30

Se integra localmente Buscar actualizaciones con resultados en las tarjetas y
versión instalada/disponible en detalles. Sin lista de instalaciones ni reemplazo
activo todavía. Se incorporan miniaturas HTTPS de image_url/icon_url del JSON,
marcador si faltan, colores por tema y carga/caché acotadas. La descripción completa
permanece en detalles. No se cambia Refrescar/activación del juego.
Contratos y validación: [CATALOG_UPDATES.md](CATALOG_UPDATES.md),
[CATALOG_THUMBNAILS.md](CATALOG_THUMBNAILS.md). Trabajo local sin commit ni push;
prueba física del nuevo diseño pendiente. Solo contrato público del catálogo.
