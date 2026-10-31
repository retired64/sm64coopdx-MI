# AGENTS.md — Desarrollo y mantenimiento de SM64coopdx-MI

## 1. Alcance y forma de aplicar estas instrucciones

Este archivo se aplica a todo este repositorio. Leerlo completo antes de modificar
código, documentación, build o configuración. Es la entrada operativa para agentes
y colaboradores; los contratos detallados están en [RULES.md](mi-docs/RULES.md) y el flujo
de sincronización en [MANTENER-UPDATES.md](mi-docs/MANTENER-UPDATES.md), ambos de lectura
obligatoria al inicio del trabajo en el proyecto. No confundir instrucciones de
repositorios hermanos con las de este fork.

Las instrucciones explícitas del usuario y las reglas de mayor prioridad del
entorno prevalecen. Un AGENTS.md más específico puede detallar un subdirectorio.
Estas reglas no añaden aprobaciones para cambios reversibles ya autorizados.
Continuar hasta completar el flujo solicitado; no limitarse a proponer un plan ni
pedir reiteradamente permiso. Respetar las restricciones reales del entorno.

Las cifras y el estado histórico de otros documentos pueden quedar obsoletos.
Contrastar código, Git y notas fechadas antes de actuar; corregir contradicciones
con evidencia, conservando el historial. No interpretar una propuesta como trabajo
hecho ni una casilla pendiente como funcionalidad disponible.

## 2. Producto: responsabilidad de MI y del juego

SM64coopdx-MI es un fork Android con integración nativa de catálogo y, como etapas
posteriores, instalación y actualización de mods. Priorizar teléfonos Android,
lectura clara, uso táctil y mando, y continuidad con la interfaz del juego.

- MI obtiene y presenta catálogo; posteriormente entregará archivos instalados,
  asociaciones y actualizaciones con resultado verificable.
- El juego descubre mods locales, lee sus metadatos, refresca, activa/desactiva y
  aplica sus reglas de ejecución. No duplicar su escáner ni crear otra activación.
- No incorporar Flutter ni copiar WorkManager/SAF/engines de la aplicación hermana
  al juego. Las referencias de otras aplicaciones no son dependencias implícitas.
- Mantener separación entre actualizar catálogo, actualizar mods y actualizar APK.
- No certificar compatibilidad jugable por listar un mod. Sí responder por el
  comportamiento de nuestra descarga, escritura, extracción y mensajes de resultado.

Estado de referencia: catálogo remoto, caché, búsqueda, detalle, cuadrícula y
accesibilidad están implementados. Instalación Lua/ZIP y recibos schema 1 están implementados localmente para pruebas.
Consulta de actualizaciones de solo lectura y miniaturas implementadas localmente:
ver mi-docs/CATALOG_UPDATES.md y mi-docs/CATALOG_THUMBNAILS.md.
Actualización individual transaccional y recuperación implementadas localmente:
ver mi-docs/CATALOG_REPLACEMENT.md. Asociaciones de mods previos, Update all, SQLite
y OTA propia siguen pendientes. Ver mi-docs/CATALOG_INSTALLATION.md. Verificar ese
estado al retomar trabajo y actualizarlo cuando realmente cambie.

### Frontera pública de datos

El fork conoce únicamente el contrato JSON publicado y las URLs declaradas en él.
No incorporar ni documentar procesos, herramientas, repositorios, rutas locales,
workflows o configuraciones privadas que produzcan esos datos. Esta regla se aplica
al código, pruebas, fixtures, documentación y mensajes de commit/PR. Ejemplos deben
ser sintéticos o proceder del contrato público, sin credenciales ni información
interna. Los workflows propios de compilación MI sí pertenecen a este repositorio.

## 3. Mapa de documentación y orden de consulta

Todas estas rutas son relativas a la raíz del repositorio. La documentación propia
de MI se centraliza en mi-docs/; AGENTS.md permanece en la raíz:

| Ruta | Para qué se consulta |
|---|---|
| [CATALOG_REPLACEMENT.md](mi-docs/CATALOG_REPLACEMENT.md) | Actualización individual, diario, respaldo, rollback y recuperación al entrar al catálogo |
| [MI_MAINTENANCE_TESTS.md](mi-docs/MI_MAINTENANCE_TESTS.md) | Pruebas del ciclo inicial e interrupciones; reemplazo en CATALOG_REPLACEMENT |
| [CATALOG_UPDATES.md](mi-docs/CATALOG_UPDATES.md) | Consulta de versiones posteriores, recibos e invalidación |
| [CATALOG_THUMBNAILS.md](mi-docs/CATALOG_THUMBNAILS.md) | Tarjetas, imágenes, caché y slots de texturas |
| [CATALOG_INSTALLATION.md](mi-docs/CATALOG_INSTALLATION.md) | Instalador local Lua/ZIP, propiedad, progreso y recuperación |
| [RULES.md](mi-docs/RULES.md) | Contratos, revisión semántica, inventario de archivos y tres commits iniciales |
| [MI_ACTIONS_BUILD.md](mi-docs/MI_ACTIONS_BUILD.md) | Workflow manual, toolchain, artefactos y comandos gh; comprobar estado real del primer run |
| [UPSTREAM_SETUP.md](mi-docs/UPSTREAM_SETUP.md) | Configuración efectiva por clon y herramienta setup/check |
| [MANTENER-UPDATES.md](mi-docs/MANTENER-UPDATES.md) | Upstream estable, ramas sync, conflictos, pruebas, promoción y recuperación |
| [MI_INITIAL_PREPARATION_CHECKLIST.md](mi-docs/MI_INITIAL_PREPARATION_CHECKLIST.md) | Preparación, seguimiento, artefactos y pendientes reales |
| [SM64CDPY_IDEAS_Y_FORK.md](mi-docs/SM64CDPY_IDEAS_Y_FORK.md) | Producto, decisiones e historial; secciones 15–16 mantenimiento, 25 formato futuro, 26 reglas operativas |
| [mi-docs/CATALOG_REMOTE.md](mi-docs/CATALOG_REMOTE.md) | Descarga del catálogo, TLS, caché, ownership y límites actuales |
| [mi-docs/CATALOG_ACCESSIBILITY.md](mi-docs/CATALOG_ACCESSIBILITY.md) | Diseño vigente de tarjetas, lectura, escala y aceptación del usuario |
| [mi-docs/DJUI_VIEWPORT_AND_TRANSLATIONS.md](mi-docs/DJUI_VIEWPORT_AND_TRANSLATIONS.md) | Desplazamiento de menús y fallback de INIs externos antiguos |
| [mi-docs/CATALOG_PREVIEW.md](mi-docs/CATALOG_PREVIEW.md) | Registro histórico del prototipo con muestra; no describe la fuente actual |
| [mi-docs/CATALOG_WORKSPACE.md](mi-docs/CATALOG_WORKSPACE.md) | Registro histórico del primer menú ancho, anterior al grid |

El README heredado describe además requisitos del juego; no renombrar ni reescribir
su documentación por marca sin necesidad. Crear un índice o documento nuevo solo
si mejora una navegación real; mantener enlaces y evitar copias contradictorias.
El inventario exhaustivo y los SHAs históricos se mantienen en RULES.md, no se
replican manualmente aquí. Registros de sync futuros usarán mi-docs/sync/ según
MANTENER-UPDATES.md; no afirmar que existen antes de crearlos.

## 4. Protocolo antes de editar

1. Comprobar `git status --short --branch`, `git remote -v` y la rama actual.
   Conservar cambios del usuario. No cambiar de rama ni limpiar archivos a ciegas.
2. Leer los documentos aplicables y el flujo completo del código afectado.
3. Buscar con `rg` / `rg --files` todos los lectores, escritores, callbacks y
   llamadas. Revisar las variantes hermanas, no solo el primer resultado.
4. Identificar fuente de verdad, propietario de cada objeto y momento/hilo de
   ejecución. Definir las salidas de éxito, error, cancelación y falta de datos.
5. Elegir el cambio más pequeño que complete el objetivo. No rehacer componentes
   nativos si pueden adaptarse. Evitar dependencias nuevas sin un beneficio concreto.
6. Verificar el estado real de herramientas y archivos; no asumir que rutas de
   otro teléfono o configuración de .git/config viajan con el repositorio.

Explicar brevemente al usuario qué se hará. Durante trabajos largos comunicar
hallazgos y pendientes, sin afirmar una validación que no se haya ejecutado.
Preguntar solo por datos o decisiones necesarios que no puedan resolverse con
el contexto; continuar mientras tanto con trabajo independiente autorizado.

## 5. Estructura y fronteras de código

| Ruta | Responsabilidad / regla |
|---|---|
| src/pc/mod_integrations/mi_catalog.cpp y .h | Parseo validado, proyección inmutable, búsqueda e interfaz C |
| src/pc/mod_integrations/mi_catalog_request.cpp y .h | IO asíncrono, TLS, caché, revisión de estado y cancelación |
| src/pc/mod_integrations/mi_catalog_panel.c y .h | Widgets DJUI, polling, navegación, snapshots y cierre del panel |
| src/pc/mod_integrations/mi_catalog_layout.h | Geometría pura del grid derivada de dimensiones lógicas |
| src/pc/mod_integrations/mi_catalog_strings.c y .h | Lookup MI y fallback respetando traducción externa |
| src/pc/djui/djui_panel_host_mods.c | Adaptador de entrada/salida a Mods y coordinación del tooltip |
| src/pc/djui/djui_three_panel.c y .h | Infraestructura de viewport/scroll; cambio afecta otros menús |
| src/pc/djui/djui_panel_layout.h | Geometría compartida comprobable sin render |
| src/pc/djui/djui_cursor.c, djui_interactable.c, djui_panel_menu.c | Foco, entrada y opt-in al desplazamiento |
| lang/English.ini, Spanish.ini, Portuguese.ini | Fuentes de etiquetas MI_CATALOG |
| tools/mi/ y tests/mi/ | Generadores y regresiones independientes del ROM/UI |
| Makefile y Android.mk | Inclusión efectiva de fuentes, dependencias y build |

Concentrar implementación propia en mod_integrations con prefijo mi_. Mantener
interfaces C entre C++ y DJUI y documentar asignación/liberación en los headers.
El parser no conoce widgets ni modifica el juego. La UI representa estado y
solicita acciones; no realiza red, parseo grande ni extracción bloqueante.
No copiar el menú upstream completo para evitar diseñar un adaptador pequeño.
No reformatear, renombrar ni modificar archivos ajenos al alcance.

## 6. Diseño, accesibilidad y experiencia de usuario

### Lectura y jerarquía

- Priorizar texto legible y acciones claras sobre densidad de resultados.
- Respetar la escala DJUI elegida: cuando falte espacio reducir columnas/filas,
  no encoger automáticamente todo el contenido para conservar la densidad.
- Mantener la cuadrícula 2x2 cuando quepa; una columna o menos filas cuando haga
  falta. Los umbrales actuales son implementación revisable, no medidas físicas dp.
- Diferenciar título, autor, categoría, resumen y estado mediante espacio y
  jerarquía coherentes. No relegar datos útiles a tamaños ilegibles.
- Mantener título completo y descripción accesibles en detalles aunque una tarjeta
  use un extracto acotado. No confundir texto recortado con datos ausentes.
- Limitar ancho de lectura de párrafos y medir altura al envolver texto. No dejar
  vacíos grandes por centrar verticalmente contenido corto de una página de lectura.
- Conservar fondo del juego sin sacrificar legibilidad; usar superficie/contraste
  adecuados al tema. No afirmar contraste certificado sin medirlo.
- Textos largos, nombres reales del catálogo y traducciones deben formar parte
  de la revisión, no diseñar solo con títulos de dos palabras.

### Controles e interacción

- Cabecera con búsqueda/actualización; pie con vuelta y paginación accesibles.
  Solo el contenido necesita desplazarse. Mantener flechas donde haya overflow.
- La zona táctil debe coincidir con lo visible. Botones separados y suficientemente
  grandes; no convertir metadatos pequeños en el único acceso a una acción.
- Conservar navegación espacial con mando en ambos ejes, foco visible y reveal de
  la selección. Revisar rueda, táctil, teclado de búsqueda y botón de retorno.
- No comunicar estado solo con color, hover o una animación. Usar texto claro y
  distinguir acción deshabilitada, trabajo activo, error y resultado.
- Mantener filtro y posición razonables al entrar/salir de detalles. Recalcular
  geometría al cambiar escala sin IDs de contenido obsoletos ni divisiones por cero.
- Una solicitud de red activa no debe impedir volver; reintentos no deben generar
  solicitudes paralelas involuntarias. Mostrar copia guardada como tal.
- No exponer JSON, SHA, ZIP, rutas o detalles técnicos como requisito para usar el
  flujo normal. Conservar diagnóstico útil separado del mensaje principal.

### Reutilización DJUI: riesgos obligatorios

El render nativo identifica ancestros desplazables. Sustituirlo por un wrapper
puede romper scroll/reveal sin error de compilación. La geometría MI actual usa
un hook de cabecera y conserva dicho render. Los estilos de botón escriben offsets
absolutos +/-1 al texto: no convertirlos en posiciones relativas. El grid informa
bodySize y no usa flujo vertical; detalles sí usa flujo con altura medida.
Verificar render, clip, hit testing y foco juntos ante cualquier modificación.

## 7. Estado, memoria, hilos y recuperación

- El trabajador no retiene ni llama widgets DJUI. Todo cambio visual ocurre en
  el hilo principal a través de snapshots/estado sincronizado.
- poll transfiere snapshot al panel; liberar el anterior. Detalles copia texto,
  no conserva punteros prestados al DTO del panel padre.
- El hilo detached mantiene una referencia propia; close cancela/libera la UI
  sin join bloqueante. Capturar errores y evitar escritura/publicación obsoleta.
- Conservar mutex, revisiones y generaciones; cualquier sustitución requiere
  demostrar ownership y orden de publicación en cierre/reapertura.
- No crear singletons globales de página/foco que colisionen con Mods vivo debajo.
- Distinguir fuente de datos, cache y estado visual. El juego es autoridad sobre
  mods locales; el catálogo validado es autoridad de metadata remota.
- Error/cancelación deben dejar una salida accionable. Nunca afirmar instalación
  por terminar descarga ni éxito de almacenamiento por mostrar datos en memoria.

Si se toca estado compartido, seguir su recorrido completo y reportar al final
“Verificación de sincronización de estado”: lectores/escritores, autoridad,
cache e invalidación, paso entre hilos, tareas independientes intencionales,
liberación y consumidores hermanos. No atribuir Futures/engines Flutter al C/C++.

## 8. Red, datos externos y persistencia

Aplicar límites y garantías de RULES.md/CATALOG_REMOTE.md. No desactivar TLS,
aceptar esquema desconocido ni reemplazar caché válida con respuesta parcial.
Comprobar curl incluido y certificados reales del sistema en Android. No confundir
headers nuevos con funcionalidades disponibles en la biblioteca empaquetada.

Mantener caché dedicada bajo fs_writepath/mi, validación al leer, respaldo y
reemplazo atómico. El CRC32 actual detecta corrupción local; no es autenticación.
El hash de catalog_id no se recalcula actualmente: no prometer lo contrario.
Ningún cambio de datos persistidos se resuelve borrando ajustes o mods del usuario.

La futura migración JSON/.db exige mediciones, IDs estables, esquema versionado,
transición y compatibilidad. Reducir transferencia y optimizar consultas son
objetivos distintos. Conservar contrato de presentación y último respaldo válido.
No implementar esa migración incidentalmente durante un cambio visual.

## 9. Instalación, asociaciones y actualizaciones futuras

No habilitar controles antes de completar el flujo vertical y probar resultados.
El producto busca instalación directa: el comprimido es transporte temporal, no
el resultado que debe gestionar el usuario. Respetar Lua suelto y estructuras
con metadatos según lo que detecta realmente el juego; RAR/7z necesitan soporte
nativo comprobado, no asumirlo por la extensión.

Diseñar propiedad de archivos/recibos y asociación mediante IDs estables, no por
título. Validar extracción, rutas, límites, cancelación, espacio y fallo parcial.
No sobrescribir unidades ajenas ni borrarlas para completar una actualización.
Actualización explícita debe explicar el resultado sin confirmaciones repetidas
innecesarias. No introducir reemplazos automáticos al arrancar como efecto colateral.
El refresco del juego se invoca en un contexto seguro; no reinventar activación.

## 10. Localización y archivos generados

- Toda etiqueta MI nueva va a MI_CATALOG en English.ini, Spanish.ini y Portuguese.ini.
- Usar mi_catalog_text para respetar overrides externos y fallback EN/ES/PT. Probar
  INIs antiguos preservados en el teléfono; empaquetar un INI nuevo no los reemplaza.
- Regenerar con `python tools/mi/embed_catalog.py`; comprobar `--check`.
- mi_catalog_strings.inc y mi_catalog_sample.inc son salidas generadas: resolver
  conflictos en los INIs/fixture y volver a generar; no mantener parches a mano.
- tests/mi/catalog_sample.json es fixture, no catálogo de producción ni URL real.
- Mantener mensajes naturales y consistentes entre lenguas. No mostrar claves
  literales TITLE/SEARCH ni excepciones técnicas como explicación principal.

## 11. Validación proporcional, concreta y reproducible

Siempre, después de cambios: `git diff --check`.

Para catálogo/UI/idiomas y en sincronizaciones:

```sh
python tools/mi/embed_catalog.py --check
python tools/mi/test_catalog.py
python tools/mi/test_updates.py
python tools/mi/test_thumbnails.py --bundled-curl
```

Para red, caché, hilos, dependencias de transporte o sincronización del motor:

```sh
python tools/mi/test_install.py --bundled-curl
python tools/mi/test_maintenance.py --bundled-curl
python tools/mi/test_replacement.py --bundled-curl
python tools/mi/test_requests.py --bundled-curl
python tools/mi/test_requests.py --bundled-curl --live
```

La prueba live requiere red: distinguir fallo externo de regresión y registrar
lo pendiente. Las pruebas deben usar temporales, no el directorio de mods real.
Crear pruebas con valor de contrato/recuperación, no réplicas de la implementación.
No ampliar/repetir suites sin una razón tras resultados correctos.

Para C/C++ de juego, DJUI, build o empaquetado:

```sh
make -j$(nproc)
apksigner verify build/us_pc/sm64coopdx.apk
zipalign -c -p 4 build/us_pc/sm64coopdx.apk
```

Comprobar CRC ZIP y coincidencia de lib/arm64-v8a/libmain.so con la biblioteca
compilada, y registrar SHA-256/tamaño. Si la sesión impone una biblioteca C++
incompatible a zipalign, el workaround conocido es ejecutar make con
`env LD_LIBRARY_PATH=/data/data/com.termux/files/usr/lib`; no cambiar paquetes
ni variables globales a ciegas. Preparación Termux en MANTENER-UPDATES.md.

Un cambio de headers/build/dependencias puede requerir reconstrucción limpia o
checkout aislado. Un build incremental no prueba reproducibilidad en otro teléfono.
Makefile validado no acredita Gradle validado. La baserom es local, nunca se agrega
a Git ni se publica por este flujo. Solo documentación: validar enlaces, bloques,
consistencia y diff; no recompilar el juego sin necesidad.

Prueba física según alcance: táctil/mando, escala, temas, idiomas, títulos largos,
teclado, búsqueda/páginas/detalle/retorno, carga inicial/offline/refresco, cierre
mientras consulta y reapertura. Para cambios del motor añadir partida base,
controles, audio y compatibilidad multijugador declarada. Registrar dispositivo y
resultados reales. Compilar o una confirmación general no cubre toda la matriz.

## 12. Mantenimiento de upstream y prevención de fallos silenciosos

Seguir MANTENER-UPDATES.md, no improvisar un pull sobre mi-main. Para un clon
nuevo usar python tools/mi/configure_upstream.py setup; para auditar sin red ni
mutaciones usar check. UPSTREAM_SETUP.md describe valores y alcance. La auditoría
no certifica frescura remota ni integración; setup tampoco hace merge ni push:


1. Inspeccionar remotos por clon y obtener referencias sin mezclar código.
2. Registrar SHA upstream anterior/candidato y HEAD MI; crear rama sync aislada.
3. Integrar el SHA fijado preservando historia pública mediante merge.
4. Resolver intención upstream/MI por archivo; nunca aceptar ours/theirs masivamente.
5. Revisar contratos aunque Git no señale conflictos. Examinar también dependencias
   no modificadas por MI: fs, mods, root/scale, idioma, SDL/táctil, red y plataforma.
6. Probar, documentar evidencia/puntos pendientes y promover solo el resultado validado.

Una API con igual firma puede cambiar hilo, orden, ownership o rutas. Un APK puede
compilar sin incluir el módulo nuevo. Un botón puede verse y no responder. Estos
son fallos de integración, no excepciones aceptables por ausencia de conflictos.
No promover un candidato con contratos críticos sin verificar. Retirar parches
redundantes si upstream ya ofrece comportamiento equivalente probado.

Mantener pequeño el delta upstream, actualizar inventario y registrar cada sync
por SHA. Separar versión de motor, MI, catálogo y futuros recibos. No declarar
configurados upstream, protecciones o CI sin comprobarlos; .git/config no viaja
con los commits. No importar experimentos de Android-dev para resolver una sync.

## 13. Git, cambios del usuario y publicación

- No usar reset destructivo, descartar cambios ajenos ni sobrescribir datos externos.
- Revisar diff y staged diff; añadir archivos concretos, evitando ROM, APK, claves,
  credenciales, temporales o material de referencia de otros repositorios.
- Commits con problema y resultado claros; registrar pruebas y limitaciones.
- Push, merge remoto y releases se realizan dentro de la autorización del usuario.
  No crear tags, subir assets ni publicar releases por inferencia de «compilar».
- Para mi-main pública no force push/rebase. Un rechazo exige revisar divergencia.
- Protección de ramas, CI y automatismos son configuración real a verificar; este
  AGENTS.md no los instala. Automatizar preparación de sync/PR, no promoción ciega.
- Recuperación de código no garantiza recuperación de datos. Antes de pruebas de
  actualización considerar backups; no recomendar downgrade sin revisar esquema/firma.

## 14. Documentación y forma de entregar resultados

Crear toda documentación nueva propia de MI dentro de mi-docs/. Mantener este
AGENTS.md en la raíz y actualizar el índice mi-docs/README.md. Los registros de
sincronización irán en mi-docs/sync/. No volver a crear Markdown MI dispersos
en la raíz ni en docs/mi/. La documentación heredada del juego permanece en sus
rutas upstream para evitar cambios innecesarios al sincronizar.

Si cambia comportamiento/arquitectura/contrato, actualizar el documento técnico
aplicable, checklist y decisión/historial cuando corresponda. Registrar pruebas,
artefacto y pendientes. Los tres commits iniciales y archivos están en RULES.md;
añadir futuros puntos de contacto para que el inventario siga siendo útil.
No reescribir hechos históricos: marcar un estado superado con nota posterior.

Comunicar en español claro, con resultado primero. Explicar qué cambió, por qué,
cómo se verificó y qué queda pendiente. No usar implementación como jerga de la
interfaz del producto. Dar enlaces a archivos/APK cuando ayuden a revisar.
No atribuir pruebas físicas a análisis estático. Si solo se documentó, decirlo;
si no hubo push, no afirmar que está en GitHub.

## 15. Definición de terminado

Una tarea se considera terminada cuando el alcance autorizado está resuelto:

- Flujo completo y variantes hermanas relevantes revisados, no parche de una tarjeta.
- Lectura, navegación, escala y entrada siguen coherentes para el usuario.
- Sin éxito falso, bloqueo permanente ni acceso a objetos ya destruidos.
- Estado/ownership, errores y cierre tienen contratos y salidas claras.
- Validaciones aplicables pasaron o la limitación externa/física está explícita.
- Cadenas y generados alineados; documentación e inventario actuales.
- Cambios ajenos y datos preservados; diff revisable y artefacto identificado.
- No se adelantaron funcionalidades futuras ni publicaciones no autorizadas.
- Si hubo estado compartido, incluye “Verificación de sincronización de estado”.

Si un entorno impide una comprobación, continuar el resto del trabajo útil y
reportar exactamente qué no se verificó. No rebautizar trabajo incompleto como
validado, ni detener una implementación autorizada en una propuesta superficial.
