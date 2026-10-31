# Reglas de desarrollo y mantenimiento de SM64coopdx-MI

> Estado local posterior — 2026-10-30: instalación Lua/ZIP y recibos schema 1
> implementados y con funcionamiento confirmado por el usuario tras corregir
> bloqueo y progreso. El usuario autorizó commit/push a mi-main. Las referencias históricas
> a Instalar deshabilitado quedan superadas por [CATALOG_INSTALLATION.md](CATALOG_INSTALLATION.md).
> Reemplazo/actualizaciones de mods, asociaciones previas y RAR/7z siguen pendientes.
> El usuario confirmó funcionamiento del APK CI anterior; el instalador nuevo
> requiere su propia prueba física.


Referencia: 2026-10-29. Este documento debe leerse antes de modificar o sincronizar
el fork. Procedimiento operativo: [MANTENER-UPDATES.md](MANTENER-UPDATES.md).
Las instrucciones explícitas del propietario y las reglas aplicables del entorno
prevalecen. [AGENTS.md](../AGENTS.md) es la entrada operativa en la raíz y exige leer
este documento. RULES.md no instala hooks ni checks de CI; esos controles deben
configurarse y verificarse por separado.

## 1. Propósito y alcance

MI integra obtención de mods dentro del juego Android: catálogo, instalación y
actualizaciones futuras. El juego sigue siendo responsable de descubrir, describir,
seleccionar, activar y desactivar mods locales. No duplicar ese motor ni sus reglas.

El contrato externo del fork es el catálogo JSON publicado. No incorporar Flutter
ni copiar arquitecturas de engines/Workers al proceso C/C++ del juego.
Conservar el JSON como contrato vigente hasta aprobar una migración medida.
La extensión .db no garantiza reducción de peso; véase sección 25 del historial.

La fuente ordinaria del motor es ManIsCat2/sm64coopdx, rama android. No incorporar
por cuenta propia android-dev/newtouch o ramas PC para completar una sincronización
estable. La historia pública MI se conserva; no rebase/force push de mi-main.

## 2. Estado real y resultado esperado

Implementado y versionado al inicio de este documento:

- Catálogo remoto schema 2 con carga asíncrona, caché y último respaldo válido.
- Búsqueda y paginación locales, detalles, fallback EN/ES/PT y controles nativos.
- Cuadrícula hasta 2x2, texto aumentado y lectura de detalles limitada en ancho.
- Acceso desde la cabecera derecha de Mods, navegación y tooltip restaurados.
- Desplazamiento reutilizable de menús estándar, conservando la escala elegida.
- Pruebas de parser, red/caché/cierre, idiomas y geometría, y build APK Termux.

No implementado: instalar desde catálogo, recibos/asociaciones, actualización
individual/Update all, SQLite ni actualización automática del APK. El workflow
MI completó su primera ejecución remota correcta; prueba física del artefacto
pendiente (MI_ACTIONS_BUILD.md). Instalar debe continuar deshabilitado hasta que exista el flujo completo.
No describir pruebas de planes futuros como resultados actuales.

Resultado esperado de trabajo futuro: integración pequeña, localizable, con
contratos explícitos, datos recuperables y una experiencia comprensible. Un
usuario debe distinguir una consulta del catálogo de una instalación real.
No certificar compatibilidad de un mod por estar listado; el juego conserva sus
comprobaciones y logs de ejecución. Mostrar fallos propios de descarga/instalación
con precisión y conservar detalle útil para diagnóstico.

## 3. Antes de editar

1. Leer este archivo, MANTENER-UPDATES.md y documentación del flujo afectado.
2. Comprobar status, rama y remotos; preservar cambios ajenos/locales.
3. Identificar emisor, consumidores, propietarios y momento de ejecución de cada
   API/estado tocado, buscando referencias con rg en todo el repositorio.
4. Revisar módulos hermanos: lista, detalle, búsqueda, paginación, retorno a Mods,
   idiomas, tema, escala, input, red/caché y empaquetado según el cambio.
5. Separar hechos observados de hipótesis; registrar limitaciones verificables.
6. No ampliar a plataformas fuera de Android ni reformatear código ajeno al objetivo.

## 4. Fronteras arquitectónicas que deben conservarse

### Módulo MI

- Concentrar lógica propia en src/pc/mod_integrations, con prefijo mi_.
- mi_catalog.cpp valida/proyecta datos; no manipula widgets ni el estado del juego.
- mi_catalog_request.cpp controla IO, caché, cancelación y publicación de snapshots.
- mi_catalog_panel.c interpreta snapshots y presenta la interfaz en el hilo principal.
- Mantener la interfaz C entre C++ y DJUI; documentar ownership en headers.
- No copiar pantallas completas del upstream para evitar un adaptador pequeño.
- Agregar archivos/directorios exige revisar inclusión real en ambos build paths.

### Hilos, memoria y ciclo de vida

- El trabajador nunca guarda un DjuiBase ni llama APIs de widgets.
- Los snapshots son inmutables; poll transfiere ownership al panel. Liberar el
  snapshot anterior y el último al destruir. Detalles copia texto; no retiene DTO.
- close solicita cancelación y libera la referencia UI sin esperar red/DNS/IO.
  El hilo detached existente conserva su referencia hasta terminar.
- Mantener mutex, revisiones y generaciones que evitan publicaciones/commits
  obsoletos. No reemplazarlos por un bool global sin analizar carreras.
- Toda nueva tarea independiente necesita propietario, cierre, captura de errores
  y prueba de salida durante operación. No introducir callbacks tardíos a UI destruida.
- Worker en ejecución no significa catálogo válido ni instalación terminada.

### Red, validación y caché

- Producción usa HTTPS con certificado y hostname verificados; nunca desactivar TLS
  para hacer pasar una prueba. El override HTTP loopback es soporte de tests.
- Verificar con el libcurl incluido, no solo con headers o curl del host. La base
  comprobada incluye libcurl 7.79 y usa certificados PEM del sistema vía CAINFO_BLOB.
- Límites vigentes: 16 MiB, schema 2 completo, hasta 5000 mods, 32 niveles de
  anidamiento; identidades coherentes y únicas en su ámbito. Revisar código al
  actualizar esos límites, no modificar solo la documentación.
- Hasta tres redirecciones, conexión 10 s y total 40 s, límite de baja velocidad;
  conservar límites y detección de truncamiento al cambiar transporte.
- Caché dedicada bajo fs_writepath/mi, archivos catalog-v2.cache y
  catalog-v2.last-good. Revalidar al leer. Respuesta inválida no sustituye copia válida.
- Una copia de menos de 15 minutos se reutiliza al abrir; refresco manual fuerza
  consulta. Búsqueda/páginas no vuelven a descargar el catálogo.
- Rechazo preventivo de reducción superior al 25 % respecto al catálogo válido:
  revisar publicaciones legítimas reducidas con criterio; no eliminar respaldo
  automáticamente para saltarse la protección.
- Escribir temporal, fsync del archivo y rename atómico. El código actual no
  garantiza fsync de directorio ni limpieza de temporales tras muerte abrupta.
- CRC32 detecta corrupción local; no autentica datos. catalog_id se valida por
  formato, no se recalcula su hash. No afirmar verificación criptográfica inexistente.
- Un fallo al guardar permite mostrar datos remotos válidos con aviso; no declarar
  que están disponibles offline si no se guardaron. No registrar secretos.

### Interfaz, accesibilidad y foco

- La escala global DJUI afecta también a MI. No reducir títulos al aumentar escala
  para conservar artificialmente el mismo número de tarjetas: reducir celdas.
- Mantener acciones fuera del cuerpo desplazable y acceso a detalles largos.
- Cambios de geometría deben revisar render, clip, hit testing, foco y navegación
  espacial juntos; la apariencia sola no demuestra que el área táctil coincida.
- El reconocimiento de paneles desplazables depende del renderizador nativo;
  sustituirlo por un wrapper puede romper búsqueda de ancestros y reveal silenciosamente.
  El diseño actual conserva ese renderizador y ajusta geometría desde la cabecera.
- Los estilos de botones escriben offsets absolutos +/-1 al texto. No convertir
  esos offsets en coordenadas relativas que expulsen el texto al enfocar/tocar.
- El cuerpo grid no usa el flujo vertical de hijos; su altura se informa mediante
  bodySize. Los detalles sí usan flujo y altura medida. No aplicar una medición
  propia de filas verticales a las celdas de dos columnas.
- No usar controles de paginación globales del juego para dos pantallas coexistentes;
  la pantalla local queda viva debajo de MI. Mantener estado por panel.
- El botón de entrada del panel derecho sigue habilitación/visibilidad de Mods y
  bloqueo de navegación. Mantener el contexto original NT_NONE salvo cambio diseñado.
- No sobrescribir ajustes, partidas, idiomas externos o mods del usuario para
  arreglar una regresión de UI. Un reset manual histórico no es política de migración.

### Traducciones y generados

- Cadenas MI visibles en sección MI_CATALOG de English.ini, Spanish.ini y
  Portuguese.ini. Otras lenguas usan fallback inglés; respetar overrides externos.
- El juego puede conservar INIs antiguos al actualizar APK. Añadir claves solo a
  assets no basta: mantener fallback compilado mediante mi_catalog_text.
- tools/mi/embed_catalog.py genera mi_catalog_strings.inc y mi_catalog_sample.inc;
  sus fuentes son los INIs y tests/mi/catalog_sample.json. Regenerar y usar --check.
- No editar las salidas generadas como fuente principal, ni usar URLs de la muestra
  example.invalid como archivos reales. La muestra no alimenta el catálogo actual.
- No reutilizar literalmente SEARCH/TITLE como texto final por falta de traducción.

## 5. Matriz de riesgo semántico: revisar sin conflicto textual

| Zona | Error silencioso posible | Comprobación obligatoria |
|---|---|---|
| Makefile / Android.mk | Carpeta MI omitida o fuente vieja empaquetada | Revisar enumeración, link y hash del libmain en APK |
| host_mods | Botón oculto, activo durante refresco o tooltip perdido | Abrir/volver, refrescar local y probar contexto de red |
| three_panel / layout | Flechas dibujadas pero contenido inaccesible | Alcanzar última acción con escala grande y detalles largos |
| cursor / interactable | Mando no revela selección, rueda deja de desplazar | Táctil, rueda y mando; foco en ambos ejes del grid |
| panel_menu / temas | Opt-in se pierde o custom panels cambian accidentalmente | Menús estándar y un custom panel, claro/oscuro |
| Root / gfx scale | Texto se reduce o celdas/zonas táctiles se solapan | Revisar unidades lógicas y matriz de escala |
| language / assets | APK nuevo muestra claves en instalación antigua | INIs antiguos, fallback y overrides personalizados |
| fs / Android paths | Caché en ruta distinta o persistencia inaccesible | Primera carga, reapertura, actualización de APK |
| curl / TLS / certificados | HTTPS falla solo en dispositivo | Request test con curl incluido y consulta real |
| request / lifecycle | Cierre provoca UAF, espera o escritura obsoleta | Cerrar durante descarga, reabrir, error y cancelación |
| parser / schema | Datos descartados o proyección incorrecta | Fixtures válidas/rotas, identidades, idiomas y catálogo real |
| mods / mods_utils | Futuro instalador escribe donde el scanner no busca | Revisar ruta efectiva y refresco seguro antes de integrar instalación |
| config / touch / SDL | Ajustes viejos alteran controles o input de búsqueda | Ajustes existentes y prueba base; no reset automático |
| red / protocolo | Compila pero ya no conecta con versiones esperadas | Verificar versión/protocolo y prueba multijugador declarada |
| Firma / package ID | APK no actualiza o pierde acceso a datos | Comparar identidad, certificado y persistencia |

Esta matriz es una lista mínima de búsqueda, no un permiso para ignorar otras
zonas cambiadas por upstream. Una API puede conservar su nombre y cambiar su
contrato, orden de llamada, hilo o ownership. Buscar usos y revisar la implementación.

## 6. Disciplina de cambios y resolución

- Commits enfocados, nombres claros y explicación del problema/resultado/prueba.
- Para conflictos: revisar base, MI y upstream; no aceptar ours/theirs masivamente.
- Si upstream resuelve el mismo problema, adoptar su solución si es equivalente y
  retirar el parche redundante después de demostrar equivalencia funcional.
- No imponer estructuras/ABI antiguos solo para evitar adaptar MI al nuevo motor.
- Conservar cambio pequeño en upstream; un helper nuevo necesita responsabilidad
  clara, consumidores reales y pruebas útiles, no capas especulativas.
- Ningún dato persistido se borra para ocultar un fallo. Versionar y migrar cuando
  cambie su contrato; planear recuperación de datos por separado del rollback de código.
- Actualizar inventario y matriz cuando se añade/retira un punto de contacto.
- Mantener las interfaces hermanas coherentes y terminar estados de error/cierre,
  no solo el caso exitoso visible.

## 7. Reglas para instalación y actualizaciones futuras

Antes de habilitar Instalar, completar selección de archivo, descarga, extracción/
copia, salida de error/cancelación y comprobación del resultado. Admitir según el
contrato real del juego: Lua suelto puede ir directamente en mods; los paquetes
luac requieren estructura y metadatos apropiados, no tratar todo luac como Lua
suelto. RAR/7z requieren soporte de extracción nativo real; no simularlo por extensión.
Validar contenido y rutas, tamaños extraídos y propiedad de archivos.

Los comprimidos son transporte temporal: el producto entrega el mod instalado,
no ofrece descargar ZIP como resultado principal. Limpiar temporales cuando sea
seguro. No declarar éxito al llegar a 100 % de descarga ni al iniciar extracción.

Recibos deben asociar IDs del catálogo con unidades/rutas instaladas, versión y
huellas pertinentes; nombre visible no es identidad. La asociación manual de un
mod local es una decisión del usuario, no una coincidencia de título automática.
Actualizar no debe borrar archivos/mods ajenos a su propiedad. Definir conflictos
con cambios locales, fallo parcial y recuperación antes de ofrecer Update all.

El usuario solicita explícitamente una actualización y entiende el reemplazo;
no imponer confirmaciones repetidas sin motivo. Diferenciar actualizar catálogo,
mod y APK. No introducir actualización de mods silenciosa durante arranque o
entrada a partida sin un diseño y autorización específicos.

El juego gestiona activación y sus fallos de scripts. MI debe conectar la entrega
con el refresco real en un contexto seguro, no implementar otra pantalla de activación.
La futura migración JSON/.db conserva IDs, asociación, idiomas y rollback de datos;
medir peso/tiempos antes de elegir. Mantener el catálogo externo como datos, no código.

## 8. Validación y definición de terminado

Obligatorio según alcance: generador --check, test_catalog.py, test_requests.py con
curl incluido cuando cambie el contrato de red/caché o al sincronizar motor,
consulta real diferenciada de test offline y git diff --check. Comandos y matriz
física en MANTENER-UPDATES.md. Cambios solo documentales no requieren recompilar.

Cambios de UI/DJUI/build requieren compilar APK y verificar firma, alineación,
CRC y libmain. Cambios de build/dependencias exigen descartar objetos obsoletos y
validar un build limpio/aislado. La ruta Gradle no se presume validada por make.
No modificar dependencias globales para corregir un problema local del entorno.

Una tarea termina cuando:

- Contratos afectados y lectores/escritores están identificados y revisados.
- Caso feliz, error, cierre y configuración faltante tienen salida coherente.
- No hay éxito falso, spinner perpetuo ni callback a objetos destruidos.
- Escala, foco, idiomas antiguos y navegación siguen operativos según alcance.
- Pruebas aplicables pasaron; pendientes físicas/externas se declaran exactamente.
- Documentación describe el estado actual y separa planes de funcionalidades.
- Se preservaron cambios del usuario y no se publicaron releases sin autorización.
- El resultado puede revisarse mediante diff, logs y artefacto identificado.

Una confirmación general del teléfono es evidencia valiosa, pero no completa
por inferencia toda la matriz. Registros históricos deben conservar su fecha;
una nota posterior puede corregir su vigencia sin borrar el contexto anterior.

## 9. Verificación de sincronización de estado

Todo cambio de estado compartido, persistencia, hilos o ciclo de vida debe explicar:
quién escribe/lee, fuente de verdad, paso entre hilos, caches e invalidación,
operaciones independientes intencionales, liberación y consumidores hermanos.
En el resumen usar el título “Verificación de sincronización de estado”.

Estado de referencia: servicio escribe caché y publica snapshots; panel consume
snapshots y controla página/filtro; juego controla mods locales. Caché válida
reciente se reutiliza y se renueva por vencimiento o petición; errores no reemplazan
respaldo válido. No hay engines Flutter ni Futures en este módulo C/C++.
El hilo detached tiene referencia propia y cancelación al cerrar el panel.

## 10. Documentación y precedencia temporal

- [AGENTS.md](../AGENTS.md): instrucciones operativas para todo el repositorio; enlaza
  reglas, procedimiento y documentación de cada flujo.

- MANTENER-UPDATES.md: ejecución de sincronizaciones, promoción y recuperación.
- RULES.md: contratos, riesgos, inventario e historial técnico verificable.
- MI_INITIAL_PREPARATION_CHECKLIST.md: preparación y seguimiento; casillas pendientes
  no se convierten en hechas por crear estas reglas.
- SM64CDPY_IDEAS_Y_FORK.md: decisiones de producto, historial y planes; sus secciones
  antiguas pueden describir un estado previo del repo (por ejemplo remotos antiguos).
- mi-docs/CATALOG_PREVIEW.md: etapa 1 histórica, muestra embebida.
- mi-docs/DJUI_VIEWPORT_AND_TRANSLATIONS.md: overflow general y fallback de idiomas.
- mi-docs/CATALOG_REMOTE.md: transporte y persistencia de etapa 2.
- mi-docs/CATALOG_WORKSPACE.md: primer diseño amplio, histórico.
- mi-docs/CATALOG_ACCESSIBILITY.md: cuadrícula y lectura actuales, con aceptación física.

Ante contradicción, contrastar código/commits y notas fechadas; corregir documentos
con evidencia. No borrar historia ni presentar un plan como regla ya implementada.
Los dos archivos nuevos se crean en esta tarea documental; no pertenecen a los tres
commits de implementación descritos abajo. Actualizar este inventario en siguientes
cambios sin intentar incrustar el hash de un commit dentro de sí mismo.


## 11. Inventario acumulado de archivos de los tres commits

Base comparada: `99c8d21ab`; punta: `02964f435`. Datos extraídos de Git, no de
una lista recordada. A = creado en algún commit MI; M = archivo existente en la
base, modificado por MI. El estado por commit se detalla en la sección 12.

| Archivo | Origen acumulado | Responsabilidad / revisión |
|---|---|---|
| [Android.mk](../Android.mk) | M | Incluye el directorio MI en la enumeración Android/NDK; ruta Gradle pendiente de validar. |
| [MI_INITIAL_PREPARATION_CHECKLIST.md](MI_INITIAL_PREPARATION_CHECKLIST.md) | A | Preparación, progreso, hashes de APK y confirmaciones; conservar pendientes reales. |
| [Makefile](../Makefile) | M | Incluye fuentes MI en la compilación nativa/Termux y su link. |
| [SM64CDPY_IDEAS_Y_FORK.md](SM64CDPY_IDEAS_Y_FORK.md) | A | Decisiones de producto, mantenimiento e historial; incluye migración futura JSON. |
| [docs/mi/CATALOG_ACCESSIBILITY.md](CATALOG_ACCESSIBILITY.md) | A | Diseño actual, grid y lectura; evidencia de aceptación física general. |
| [docs/mi/CATALOG_PREVIEW.md](CATALOG_PREVIEW.md) | A | Registro histórico de etapa 1 y muestra, no contrato remoto actual. |
| [docs/mi/CATALOG_REMOTE.md](CATALOG_REMOTE.md) | A | Servicio remoto, TLS, caché, límites y validación. |
| [docs/mi/CATALOG_WORKSPACE.md](CATALOG_WORKSPACE.md) | A | Primer espacio amplio; diseño anterior al grid accesible. |
| [docs/mi/DJUI_VIEWPORT_AND_TRANSLATIONS.md](DJUI_VIEWPORT_AND_TRANSLATIONS.md) | A | Contrato overflow DJUI, escala y fallback de idiomas externos antiguos. |
| [lang/English.ini](../lang/English.ini) | M | Referencia EN de MI_CATALOG; conserva otras secciones upstream. |
| [lang/Portuguese.ini](../lang/Portuguese.ini) | M | Traducción PT de todas las etiquetas MI. |
| [lang/Spanish.ini](../lang/Spanish.ini) | M | Traducción ES de todas las etiquetas MI. |
| [src/pc/djui/djui_cursor.c](../src/pc/djui/djui_cursor.c) | M | Reveal al cambiar selección con mando; depende de ancestros y geometría. |
| [src/pc/djui/djui_interactable.c](../src/pc/djui/djui_interactable.c) | M | Integración de rueda/scroll con interacción; preservar handlers existentes. |
| [src/pc/djui/djui_panel_host_mods.c](../src/pc/djui/djui_panel_host_mods.c) | M | Entrada MI, visibilidad/foco/bloqueo y tooltip; no duplicar scanner local. |
| [src/pc/djui/djui_panel_layout.h](../src/pc/djui/djui_panel_layout.h) | A | Geometría pura compartida para overflow y clamp. |
| [src/pc/djui/djui_panel_menu.c](../src/pc/djui/djui_panel_menu.c) | M | Opt-in de menús estándar al desplazamiento. |
| [src/pc/djui/djui_three_panel.c](../src/pc/djui/djui_three_panel.c) | M | Viewport, medición, scroll, flechas, clip y reveal; afecta menús estándar. |
| [src/pc/djui/djui_three_panel.h](../src/pc/djui/djui_three_panel.h) | M | Campos y API del desplazamiento; revisar layout/ABI y recompilar consumidores. |
| [src/pc/mod_integrations/mi_catalog.cpp](../src/pc/mod_integrations/mi_catalog.cpp) | A | Parser validado, snapshots y búsqueda; muestra solo para regresión. |
| [src/pc/mod_integrations/mi_catalog.h](../src/pc/mod_integrations/mi_catalog.h) | A | Contrato C, DTO acotado, índices y ownership del catálogo. |
| [src/pc/mod_integrations/mi_catalog_layout.h](../src/pc/mod_integrations/mi_catalog_layout.h) | A | Capacidad/columnas/altura derivadas de dimensiones lógicas DJUI. |
| [src/pc/mod_integrations/mi_catalog_panel.c](../src/pc/mod_integrations/mi_catalog_panel.c) | A | Hilo UI, grid, detalle, polling y cierre; acoplamientos DJUI explícitos. |
| [src/pc/mod_integrations/mi_catalog_panel.h](../src/pc/mod_integrations/mi_catalog_panel.h) | A | Entrada pública de creación de panel usada por Mods. |
| [src/pc/mod_integrations/mi_catalog_request.cpp](../src/pc/mod_integrations/mi_catalog_request.cpp) | A | Worker pthread, HTTP/TLS, caché, respaldo, cancelación y generaciones. |
| [src/pc/mod_integrations/mi_catalog_request.h](../src/pc/mod_integrations/mi_catalog_request.h) | A | Contrato C de inicio/refresco/poll/cierre y estados de consulta. |
| [src/pc/mod_integrations/mi_catalog_sample.inc](../src/pc/mod_integrations/mi_catalog_sample.inc) | A | Generado desde fixture; regenerar, no resolver manualmente. |
| [src/pc/mod_integrations/mi_catalog_strings.c](../src/pc/mod_integrations/mi_catalog_strings.c) | A | Lookup fallback EN/ES/PT respetando traducción externa. |
| [src/pc/mod_integrations/mi_catalog_strings.h](../src/pc/mod_integrations/mi_catalog_strings.h) | A | Contrato público de etiquetas MI. |
| [src/pc/mod_integrations/mi_catalog_strings.inc](../src/pc/mod_integrations/mi_catalog_strings.inc) | A | Generado desde INIs; revisar fuente y --check. |
| [tests/mi/catalog_sample.json](../tests/mi/catalog_sample.json) | A | Fixture sintética schema 2, URLs inválidas deliberadamente. |
| [tests/mi/catalog_test.cpp](../tests/mi/catalog_test.cpp) | A | Contrato del parser, IDs, errores, búsqueda y límites UTF-8. |
| [tests/mi/panel_layout_test.c](../tests/mi/panel_layout_test.c) | A | Geometría overflow y grid con alturas/anchos/escalas variados. |
| [tests/mi/request_cli.cpp](../tests/mi/request_cli.cpp) | A | Ejecutable de prueba de requests/poll/cierre, sin widgets. |
| [tests/mi/strings_test.c](../tests/mi/strings_test.c) | A | Fallback con INIs antiguos, overrides y búsqueda traducida. |
| [tests/mi/stubs/pc/configfile.h](../tests/mi/stubs/pc/configfile.h) | A | Stub mínimo para pruebas de idioma sin enlazar el juego. |
| [tests/mi/stubs/pc/djui/djui_language.h](../tests/mi/stubs/pc/djui/djui_language.h) | A | Stub del contrato de idioma para pruebas standalone. |
| [tools/mi/embed_catalog.py](../tools/mi/embed_catalog.py) | A | Genera fixture .inc y tabla de idiomas; --check detecta drift. |
| [tools/mi/test_catalog.py](../tools/mi/test_catalog.py) | A | Orquesta C++ parser y C idiomas/geometría con temporales. |
| [tools/mi/test_requests.py](../tools/mi/test_requests.py) | A | HTTP local, fallos/caché/cierre y opción curl incluido/HTTPS real. |

### Dependencias observadas que NO fueron modificadas por estos tres commits

`src/pc/mods/`, `src/pc/fs/`, `src/pc/djui/djui_root.c`,
`src/pc/djui/djui_gfx.c`, `src/pc/djui/djui_button.c`,
`src/pc/djui/djui_searchbox.c`, cargador de idioma, configuración, control táctil,
SDL, plataforma Android, red y curl son dependencias a revisar aunque no aparezcan
en el diff MI. No confundir «revisado» con «modificado». No se editó el motor táctil
para solucionar los ajustes históricos: el usuario los restableció manualmente.

Las herramientas compartidas de GitHub Actions y la identidad pública del APK
no se configuraron en estos commits. No hay evidencia de build Gradle ejecutado.

## 12. Historial exacto de los tres cambios publicados

Los cambios documentales que originan este archivo no están incluidos aquí.
Los enlaces apuntan a commits inmutables del fork. A/M describen cada commit,
no el origen acumulado de la sección anterior.

### 12.1. Prototipo nativo y corrección general DJUI

Commit: [`5b3ae5c50434b9f3277d2605b71ddb8902295551`](https://github.com/retired64/sm64coopdx-MI/commit/5b3ae5c50434b9f3277d2605b71ddb8902295551).

Mensaje: Add native MI catalog preview and fix DJUI menu overflow.

Entrada de catálogo con muestra, parser/contrato C, fallback de idiomas y scroll de menús estándar. Se añadieron fuentes MI a Makefile y Android.mk. El usuario confirmó funcionamiento del parche tras restablecer ajustes táctiles por su cuenta.

32 archivos: 21 creados y 11 modificados.

| Estado | Archivo |
|---|---|
| M | [Android.mk](../Android.mk) |
| A | [MI_INITIAL_PREPARATION_CHECKLIST.md](MI_INITIAL_PREPARATION_CHECKLIST.md) |
| M | [Makefile](../Makefile) |
| A | [SM64CDPY_IDEAS_Y_FORK.md](SM64CDPY_IDEAS_Y_FORK.md) |
| A | [docs/mi/CATALOG_PREVIEW.md](CATALOG_PREVIEW.md) |
| A | [docs/mi/DJUI_VIEWPORT_AND_TRANSLATIONS.md](DJUI_VIEWPORT_AND_TRANSLATIONS.md) |
| M | [lang/English.ini](../lang/English.ini) |
| M | [lang/Portuguese.ini](../lang/Portuguese.ini) |
| M | [lang/Spanish.ini](../lang/Spanish.ini) |
| M | [src/pc/djui/djui_cursor.c](../src/pc/djui/djui_cursor.c) |
| M | [src/pc/djui/djui_interactable.c](../src/pc/djui/djui_interactable.c) |
| M | [src/pc/djui/djui_panel_host_mods.c](../src/pc/djui/djui_panel_host_mods.c) |
| A | [src/pc/djui/djui_panel_layout.h](../src/pc/djui/djui_panel_layout.h) |
| M | [src/pc/djui/djui_panel_menu.c](../src/pc/djui/djui_panel_menu.c) |
| M | [src/pc/djui/djui_three_panel.c](../src/pc/djui/djui_three_panel.c) |
| M | [src/pc/djui/djui_three_panel.h](../src/pc/djui/djui_three_panel.h) |
| A | [src/pc/mod_integrations/mi_catalog.cpp](../src/pc/mod_integrations/mi_catalog.cpp) |
| A | [src/pc/mod_integrations/mi_catalog.h](../src/pc/mod_integrations/mi_catalog.h) |
| A | [src/pc/mod_integrations/mi_catalog_panel.c](../src/pc/mod_integrations/mi_catalog_panel.c) |
| A | [src/pc/mod_integrations/mi_catalog_panel.h](../src/pc/mod_integrations/mi_catalog_panel.h) |
| A | [src/pc/mod_integrations/mi_catalog_sample.inc](../src/pc/mod_integrations/mi_catalog_sample.inc) |
| A | [src/pc/mod_integrations/mi_catalog_strings.c](../src/pc/mod_integrations/mi_catalog_strings.c) |
| A | [src/pc/mod_integrations/mi_catalog_strings.h](../src/pc/mod_integrations/mi_catalog_strings.h) |
| A | [src/pc/mod_integrations/mi_catalog_strings.inc](../src/pc/mod_integrations/mi_catalog_strings.inc) |
| A | [tests/mi/catalog_sample.json](../tests/mi/catalog_sample.json) |
| A | [tests/mi/catalog_test.cpp](../tests/mi/catalog_test.cpp) |
| A | [tests/mi/panel_layout_test.c](../tests/mi/panel_layout_test.c) |
| A | [tests/mi/strings_test.c](../tests/mi/strings_test.c) |
| A | [tests/mi/stubs/pc/configfile.h](../tests/mi/stubs/pc/configfile.h) |
| A | [tests/mi/stubs/pc/djui/djui_language.h](../tests/mi/stubs/pc/djui/djui_language.h) |
| A | [tools/mi/embed_catalog.py](../tools/mi/embed_catalog.py) |
| A | [tools/mi/test_catalog.py](../tools/mi/test_catalog.py) |

### 12.2. Catálogo remoto y respaldo válido

Commit: [`c633ad43ea76c5cdaebbcbfdf9b6dd94dae69a09`](https://github.com/retired64/sm64coopdx-MI/commit/c633ad43ea76c5cdaebbcbfdf9b6dd94dae69a09).

Mensaje: Integrate native remote mod catalog with validated cache.

Sustituye la muestra en pantalla por catálogo real asíncrono, validación de versiones/archivos, TLS y caché con respaldo. Agrega pruebas de red y cierre. Se comprobó HTTPS con el curl incluido y 1307 mods en esa consulta histórica.

16 archivos: 5 creados y 11 modificados.

| Estado | Archivo |
|---|---|
| M | [MI_INITIAL_PREPARATION_CHECKLIST.md](MI_INITIAL_PREPARATION_CHECKLIST.md) |
| M | [SM64CDPY_IDEAS_Y_FORK.md](SM64CDPY_IDEAS_Y_FORK.md) |
| M | [docs/mi/CATALOG_PREVIEW.md](CATALOG_PREVIEW.md) |
| A | [docs/mi/CATALOG_REMOTE.md](CATALOG_REMOTE.md) |
| M | [lang/English.ini](../lang/English.ini) |
| M | [lang/Portuguese.ini](../lang/Portuguese.ini) |
| M | [lang/Spanish.ini](../lang/Spanish.ini) |
| M | [src/pc/mod_integrations/mi_catalog.cpp](../src/pc/mod_integrations/mi_catalog.cpp) |
| M | [src/pc/mod_integrations/mi_catalog.h](../src/pc/mod_integrations/mi_catalog.h) |
| M | [src/pc/mod_integrations/mi_catalog_panel.c](../src/pc/mod_integrations/mi_catalog_panel.c) |
| A | [src/pc/mod_integrations/mi_catalog_request.cpp](../src/pc/mod_integrations/mi_catalog_request.cpp) |
| A | [src/pc/mod_integrations/mi_catalog_request.h](../src/pc/mod_integrations/mi_catalog_request.h) |
| M | [src/pc/mod_integrations/mi_catalog_strings.inc](../src/pc/mod_integrations/mi_catalog_strings.inc) |
| M | [tests/mi/catalog_test.cpp](../tests/mi/catalog_test.cpp) |
| A | [tests/mi/request_cli.cpp](../tests/mi/request_cli.cpp) |
| A | [tools/mi/test_requests.py](../tools/mi/test_requests.py) |

### 12.3. Accesibilidad, cuadrícula e historial futuro

Commit: [`02964f435e898c19c5831c2f9d9a10c20ccb1f10`](https://github.com/retired64/sm64coopdx-MI/commit/02964f435e898c19c5831c2f9d9a10c20ccb1f10).

Mensaje: Improve MI catalog accessibility with responsive cards and readable details.

Espacio amplio, entrada arriba a la derecha, grid adaptable, tipografía mayor, detalle arriba y fallback de búsqueda. Incluye el plan futuro JSON/.db, no una migración implementada. El usuario confirmó que la visualización resultó muy accesible en su teléfono.

14 archivos: 3 creados y 11 modificados.

| Estado | Archivo |
|---|---|
| M | [MI_INITIAL_PREPARATION_CHECKLIST.md](MI_INITIAL_PREPARATION_CHECKLIST.md) |
| M | [SM64CDPY_IDEAS_Y_FORK.md](SM64CDPY_IDEAS_Y_FORK.md) |
| A | [docs/mi/CATALOG_ACCESSIBILITY.md](CATALOG_ACCESSIBILITY.md) |
| M | [docs/mi/CATALOG_REMOTE.md](CATALOG_REMOTE.md) |
| A | [docs/mi/CATALOG_WORKSPACE.md](CATALOG_WORKSPACE.md) |
| M | [lang/English.ini](../lang/English.ini) |
| M | [lang/Portuguese.ini](../lang/Portuguese.ini) |
| M | [lang/Spanish.ini](../lang/Spanish.ini) |
| M | [src/pc/djui/djui_panel_host_mods.c](../src/pc/djui/djui_panel_host_mods.c) |
| A | [src/pc/mod_integrations/mi_catalog_layout.h](../src/pc/mod_integrations/mi_catalog_layout.h) |
| M | [src/pc/mod_integrations/mi_catalog_panel.c](../src/pc/mod_integrations/mi_catalog_panel.c) |
| M | [src/pc/mod_integrations/mi_catalog_strings.inc](../src/pc/mod_integrations/mi_catalog_strings.inc) |
| M | [tests/mi/panel_layout_test.c](../tests/mi/panel_layout_test.c) |
| M | [tests/mi/strings_test.c](../tests/mi/strings_test.c) |

### Cómo verificar o extender este historial

```sh
git show --stat 5b3ae5c50
git show --name-status c633ad43e
git show --name-status 02964f435
git diff --name-status 99c8d21ab..02964f435
```

Para cada futuro commit de integración registrar SHA, base upstream, intención,
archivos creados/modificados/retirados, contratos afectados, pruebas y pendientes.
Si se mueve o elimina un archivo, actualizar enlaces e inventario actual y
conservar su referencia histórica al commit que lo contenía.


## 13. Organización documental vigente

Toda documentación propia de MI vive en mi-docs/; AGENTS.md permanece en la raíz.
Crear futuros planes, registros y guías dentro de esta carpeta y enlazarlos desde
[README.md](README.md). Las tablas históricas anteriores conservan los nombres
y rutas que tenían los archivos en sus commits originales; sus enlaces llevan a
la ubicación vigente. No mover documentación heredada del juego por esta regla.


## 14. Herramienta de preparación por clon

[tools/mi/configure_upstream.py](../tools/mi/configure_upstream.py) configura y
audita remotos/seguimiento y valores locales con setup/check. No pertenece a los
tres commits iniciales. Detalles: [UPSTREAM_SETUP.md](UPSTREAM_SETUP.md).
No confundir check offline con frescura remota ni configuración con integración.


## 15. Build CI preparado, pendiente de ejecución remota

[MI_ACTIONS_BUILD.md](MI_ACTIONS_BUILD.md) documenta el workflow
[mi-actions-build.yml](../.github/workflows/mi-actions-build.yml). Nuevos archivos:
[verify_ci_apk.py](../tools/mi/verify_ci_apk.py) y
[apk_test.py](../tests/mi/apk_test.py). Adaptados Android.mk (C++17) y
platform/app/build.gradle (NDK/Build Tools, ABI/jobs y rutas de recursos).
No forman parte de los tres commits iniciales. Nunca atribuir a un verificador
local el éxito del build Gradle remoto. La ROM sigue siendo un dato del usuario
solicitado al arrancar, no una entrada de este workflow.


### Grafo Gradle Android

platform/settings.gradle define :app al usar -p platform. El settings.gradle raíz
heredado define :platform:app. No intercambiar nombres de tareas sin comprobar la
entrada efectiva. La ruta de assets se deriva del directorio del módulo app, no
de asumir dónde está rootProject. CI debe comprobar tareas antes del make del host.


### Resultado CI posterior al primer fallo

[Run y evidencia](ci/2026-10-30-first-success.md): e7d0ac815 compiló, verificó y
subió el APK. El resultado remoto no acredita una descarga contrastada localmente
ni pruebas físicas; la descarga se detuvo por petición del usuario. Registrar
ambos niveles por separado y conservar el SHA del código realmente probado.


## Instalador MI local — puntos de contacto nuevos

Contrato e inventario: [CATALOG_INSTALLATION.md](CATALOG_INSTALLATION.md).
Nuevos mi_install.cpp/.h, mi_network.h y mi_sha256.h en src/pc/mod_integrations;
nuevos tests/mi/install_cli.cpp y tools/mi/test_install.py. Modificados parser/DTO,
request TLS compartido, detalles DJUI, idiomas/generado y tests de catálogo.
Sin cambios al escáner, activación, importador original ni carga inicial.
En cada sync revisar semántica de fs_writepath, fcntl/renameat2, miniz/curl,
manifest/recibos, lifetime y callbacks DJUI. No basta resolver conflictos textuales.
Añadir prueba Lua/ZIP/cancelación/recuperación y detección mediante Refrescar.
No promover esta implementación antes de completar pruebas locales autorizadas.


## Consulta de actualizaciones y miniaturas — implementación local

Contratos: [CATALOG_UPDATES.md](CATALOG_UPDATES.md) y
[CATALOG_THUMBNAILS.md](CATALOG_THUMBNAILS.md). Nuevos módulos mi_updates,
mi_thumbnail y mi_thumbnail_texture; tests/mi/updates_test.cpp, thumbnail_cli.cpp,
thumbnail_texture_test.c y herramientas test_updates.py/test_thumbnails.py.
Metadata/clone del catálogo, revisión del instalador, UI/idiomas y geometría se
adaptan. gfx_pc.c/.h añade refresco opt-in de textura por dirección, sin vaciado
global ni cambio del contrato de las demás texturas. Incluir estos puntos en la
revisión semántica de upstream. No hay reemplazo de mods ni limpieza de recibos.
Solo JSON público como contrato; no conocimiento de producción de datos.


## Contratos de mantenimiento conectados — 2026-10-30

Inventario nuevo: tests/mi/maintenance_cli.cpp y tools/mi/test_maintenance.py;
contrato y matriz en [MI_MAINTENANCE_TESTS.md](MI_MAINTENANCE_TESTS.md). El harness
intercepta rename/syscall únicamente en su link de pruebas, no en el juego. El
workflow MI manual invoca las suites de instalación y mantenimiento.

Punto de contacto corregido: mi_install.cpp publica todos los estados terminales
de una operación aceptada después de limpieza/liberación de exclusión, no solo
el éxito. install_cli.cpp comprueba esa garantía sin una espera de gracia. Busy
por rechazo de una segunda solicitud no implica que otra operación haya terminado.
Al sincronizar upstream, conservar orden de publicación, mutex, revisión y
liberación de referencias. No confundir candidato de actualización con integridad
local ni instalación ordinaria con reemplazo: este último sigue pendiente.


## Actualización individual transaccional — 2026-10-30

La implementación local supera el estado pendiente de reemplazo. Contrato y
matriz: [CATALOG_REPLACEMENT.md](CATALOG_REPLACEMENT.md). Nuevos puntos de revisión:
API mi_install_update_start/mi_install_recover_start, IDs instalados en candidato,
registro opcional directories en recibos schema 1, diarios schema 2 y sus estados
prepared/committed/cleaning/rolled_back. Preservar recuperación bajo el mismo lock,
publicación terminal después de liberar recursos y evidencia cuando no sea seguro
restaurar. test_replacement.py y maintenance_cli.cpp cubren fallos sin hooks de
producción. No se amplía al escáner ni a activación ni a Actualizar todos.


## Fuente CDN exclusiva — 2026-10-30

Se sustituye el origen anterior y la política de fallback por el catálogo
personalizado y mirror como única descarga. Contrato y límite DynOS:
[CDN_ONLY_CATALOG.md](CDN_ONLY_CATALOG.md). Las notas anteriores conservan historia.


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


## Versionado MI y catálogo incluido

Regla vigente: [VERSIONING_AND_RELEASES.md](VERSIONING_AND_RELEASES.md). Conservar versionCode y firma upstream;
personalizar solo versionName. Catálogo incluido actualizado: 1,311 mods.
