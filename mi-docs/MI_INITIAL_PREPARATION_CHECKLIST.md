# MI — Checklist de preparación inicial

Fecha: 2026-10-29.
Proyecto: sm64coopdx-MI, edición Android con Mods Integrations.
Estado: preparación y primer prototipo DJUI implementado; las casillas pendientes
no representan trabajo ejecutado. Ver progreso del MVP al final del documento.

## Objetivo y alcance

Preparar un fork mantenible y una compilación Android reproducible antes de
implementar el catálogo. MI obtiene, instala y actualiza mods. El juego conserva
su descubrimiento, metadatos locales, lista, refresco, activación e incompatibilidades.

Referencia de producto: [SM64CDPY_IDEAS_Y_FORK.md](SM64CDPY_IDEAS_Y_FORK.md),
especialmente secciones 15–18. Ambos documentos están en la raíz del fork.

No forman parte de esta preparación: implementar el catálogo, modificar el motor,
publicar releases, subir assets ni sincronizar ramas de desarrollo experimentales.

## 0. Punto de partida verificado

- [x] Repositorio local: ~/sm64coopdx-MI/.
- [x] origin: git@github.com:retired64/sm64coopdx-MI.git.
- [x] Rama local inicial: android, siguiendo origin/android.
- [x] Árbol de trabajo limpio antes de crear este documento.
- [x] Base local registrada: 99c8d21abe4d24649b508d0f216c4359ed5d2a7a.
- [x] Referencias remotas locales observadas: android, android-dev y newtouch.
- [x] Wrapper presente: platform/gradlew y platform/gradle/wrapper/gradle-wrapper.jar.
- [x] Workflow heredado inspeccionado: .github/workflows/build-coop.yaml.
- [x] Confirmar por fetch si la fuente remota avanzó; el SHA anterior describe la copia inspeccionada, no la última versión remota garantizada.

## 1. Remotos y ramas de mantenimiento

- [x] Añadir upstream apuntando a https://github.com/ManIsCat2/sm64coopdx.git.
- [x] Obtener referencias de upstream y comparar android con upstream/android.
- [x] Conservar android como espejo sin desarrollo propio de MI.
- [x] Crear mi-main desde el SHA de android elegido y registrado.
- [x] Llevar este checklist a mi-main; pendiente de commit junto con el documento de ideas.
- [ ] Crear ramas feature/mi-<tema> para funciones y sync/android-<fecha> para sincronizaciones.
- [ ] Definir mi-main como rama principal del fork cuando esté preparada.
- [x] Documentar que android-dev, newtouch y ramas PC no son fuentes ordinarias de sincronización.
- [x] Acordar que ninguna sincronización fuerza el reemplazo de la historia de MI.

Comandos orientativos; ejecutar según el estado real, no como bloque ciego:

```sh
git remote add upstream https://github.com/ManIsCat2/sm64coopdx.git
git fetch upstream
git log --oneline android..upstream/android
git switch -c mi-main android
```

Si upstream ya existe, comprobar su URL antes de modificarlo. Si el SHA base
cambia, actualizar su registro y repetir la validación de base. No sincronizar
ni crear ramas adicionales solo para marcar este checklist.

## 2. Instrucciones y documentación del fork

- [x] Crear AGENTS.md propio con alcance Android y fuente upstream/android.
- [x] Límites documentados en mi-docs/CATALOG_PREVIEW.md: DJUI y catálogo separados; el motor conserva gestión local. Red/disco pendientes.
- [x] Alcance documentado: el prototipo no duplica escáner, activación, incompatibilidades ni diagnóstico Lua.
- [x] Mapa del primer módulo y conexión DJUI documentados en mi-docs/CATALOG_PREVIEW.md.
- [ ] Registrar SHA base, versión MI, herramientas y procedimiento de build en documentación propia.
- [ ] Mantener README heredado y créditos; agregar instrucciones MI sin atribuirnos código ajeno.
- [ ] Registrar decisiones técnicas y pruebas físicas pendientes sin marcarlas como completadas.

## 3. Identidad y convivencia Android

Separar la prueba de base sin modificaciones del cambio de identidad de MI.

- [ ] Definir applicationId propio y estable para MI antes de distribuir APK a usuarios.
- [ ] Definir nombre visible e identificación de versión propia y base upstream.
- [ ] Revisar Activity, manifest, namespace, referencias Java/JNI y recursos antes de renombrar paquetes.
- [ ] Evitar renombrar todo el código Java solo por marca si basta con aislar applicationId y etiqueta; verificar las referencias concretas.
- [ ] Definir firma propia para futuras actualizaciones; no usar la firma heredada como identidad de publicación MI.
- [ ] Guardar claves privadas fuera del repositorio y logs; usar secretos del entorno de publicación.
- [ ] Verificar coexistencia con com.maniscat2.sm64coopdx y rutas efectivas de datos.
- [ ] Decidir cómo seleccionar/migrar datos existentes sin sobrescribirlos ni compartir archivos en uso.
- [ ] Mantener permisos alineados con las rutas realmente elegidas; no añadir accesos por suposición.

## 4. Compilación Android de referencia

Valores observados en los archivos actuales, no toolchain ya validada:

| Componente | Referencia inspeccionada |
|---|---|
| Java del workflow | 17 |
| Gradle wrapper | 8.9 |
| Android Gradle Plugin | 8.7.2 |
| compileSdk / targetSdk | 33 / 33 |
| minSdk | 24 |
| ABI declaradas | arm64-v8a, x86_64 |
| Controles táctiles del workflow | TOUCH_CONTROLS=1 |

- [x] Entorno inicial elegido y probado por el usuario: Termux, compilación nativa mediante Makefile. No requiere usar la ruta Gradle para este build.
- [ ] Identificar y fijar la versión NDK y cualquier herramienta adicional que use la base.
- [ ] Revisar platform/app/jni/Android.mk y sus referencias hacia el build nativo.
- [ ] Confirmar disponibilidad de bibliotecas precompiladas y recursos requeridos.
- [ ] Determinar pasos de generación/extracción de recursos necesarios y sus entradas; no asumir que basta con Gradle.
- [ ] Revisar el prebuild make del workflow: qué genera y qué necesita el APK.
- [x] Build local completado por el usuario en Termux con su baserom mediante make -j$(nproc); APK presente. Versiones de herramientas pendientes de registrar.
- [ ] Guardar logs y APK de prueba con SHA y entorno identificables.
- [ ] Repetir desde un checkout/entorno limpio para comprobar reproducibilidad.
- [ ] No mezclar actualización general de herramientas con la primera compilación de referencia salvo que un fallo lo exija; documentar cualquier ajuste.

Actualización: el usuario confirmó compilación local exitosa en Termux con su
baserom. Se verificó la presencia del APK y la integridad ZIP de sus entradas.
La observación inicial de herramientas no disponibles no describe el entorno
actual del build. Pendiente registrar la orden exacta y versiones utilizadas,
repetibilidad desde un entorno limpio y pruebas físicas; no se volvió a compilar
durante esta verificación.

## 5. CI de prueba Android para MI

El workflow heredado dispara push/PR sobre android-dev y contiene jobs de varias
plataformas. No sirve todavía como política de validación propia de mi-main.

- [x] Crear workflow propio Android con ejecución manual.
- [ ] Añadir validación automática de PR hacia mi-main después del primer build validado.
- [ ] Evitar ejecutar builds de escritorio para validar el MVP Android.
- [x] Mantener el workflow heredado intacto cuando sea viable; revisar sus disparadores antes de decidir si deshabilitarlo en el fork.
- [x] Fijar Java, SDK, NDK y dependencias según el build validado.
- [x] Compilar inicialmente APK de prueba; no crear release, tag ni publicación automática.
- [x] Usar firma de prueba adecuada sin claves privadas de publicación.
- [x] Producir artifact APK instalable y logs; identificar SHA MI y base upstream.
- [x] Establecer permisos mínimos de Actions, sin acceso a secretos en PR no confiables.
- [x] Comprobar el workflow en una ejecución real y corregir el procedimiento según evidencia.
- [ ] Documentar una orden de build local equivalente al flujo validado.

## 6. Validación física de la base

- [x] Usuario confirma instalación/prueba y funcionamiento del juego. No se verificó por separado el procedimiento de convivencia con otra instalación.
- [x] Arranque y funcionamiento general confirmados por el usuario con su baserom; no se incorporan esos recursos al control de versiones.
- [ ] Comprobar menús, Atrás, táctil y mando cuando esté disponible.
- [ ] Confirmar acceso a datos y carpeta efectiva user/mods.
- [ ] Comprobar descubrimiento, descripción, refresco y activación existentes.
- [ ] Minimizar, reabrir y cerrar/arrancar el juego.
- [ ] Registrar dispositivo, Android, ABI, SHA, resultado y fallos conocidos.
- [ ] Repetir comprobaciones necesarias después de aplicar identidad/rutas MI.

No dar por validado el juego solo porque Gradle genera un APK.

## 7. Preparar los límites del módulo MI

- [x] Directorio src/pc/mod_integrations/ elegido; lector y pantalla de muestra implementados, instalador pendiente.
- [x] Interfaz C del catálogo hacia C++ definida en mi_catalog.h; interfaz de instalación pendiente.
- [x] Botón de explorar conectado al panel MI; tooltip existente se oculta y restaura al regresar.
- [ ] Identificar cómo solicitar trabajo y entregar resultados al hilo de UI sin acceder a objetos DJUI destruidos.
- [ ] Encapsular resolución de rutas y conexión al refresco existente.
- [ ] Evitar referencias directas a estructuras internas del motor desde cada parte del instalador.
- [x] Directorio incluido en Makefile y Android.mk. Build Termux validado; Gradle no ejecutado.
- [ ] Planificar bibliotecas HTTP/JSON/extracción verificando su disponibilidad real en el target Android.
- [ ] Definir almacenamiento MI separado para caché y recibos, sin interferir con archivos de configuración del juego.
- [ ] Versionar recibos y prever recuperación/migración desde el primer diseño.

## 8. Preparar datos y casos de prueba del MVP

- [x] tests/mi/catalog_sample.json: 8 entradas sintéticas schema 2 con IDs estables; validada por el scraper y embebida para el prototipo.
- [ ] Seleccionar Lua suelto y ZIP de prueba con distribución permitida o generarlos como fixtures propios.
- [ ] Preparar dos versiones de un mismo mod para probar actualización individual.
- [ ] Preparar casos de red fallida, descarga incompleta, cancelación, extracción fallida y colisión con archivos ajenos.
- [ ] Definir proveedores y formatos iniciales probados; no anunciar RAR/7z hasta incorporar y validar sus extractores.
- [ ] Dejar RAR nativo como requisito posterior identificado, con evaluación de biblioteca/licencia y variantes soportadas.
- [ ] Definir qué recibo asocia IDs del catálogo, versión, archivo y unidades/rutas instaladas.
- [ ] No usar un hash o parecido de título como único identificador universal del mod.

## 9. Mantenimiento preparado antes de ampliar funciones

- [ ] Documentar revisión periódica de upstream/android y promoción mediante ramas sync.
- [ ] Enumerar puntos sensibles: DJUI, mods, fs/plataforma, Android/SDL, build y red.
- [ ] Exigir revisión semántica aunque Git no encuentre conflictos.
- [ ] Definir pruebas mínimas por actualización y registro del último SHA upstream probado.
- [ ] Mantener catálogo, versión del motor, versión MI y esquema de recibos independientes.
- [ ] Posponer automatismos de publicación; automatizar primero detección y validación.

## 10. Puerta de entrada al desarrollo del MVP

La preparación queda lista cuando:

- [ ] mi-main está separada del espejo android y upstream está documentado.
- [ ] Existe un build Android reproducible con APK de referencia probado.
- [ ] La identidad MI y las rutas tienen una decisión explícita y comprobada.
- [ ] CI genera un APK de prueba y logs identificables.
- [ ] Se conocen los límites del módulo y los puntos de contacto con el motor.
- [ ] Están preparados JSON y archivos de prueba controlados.

Entonces comenzar, en orden:

1. Pantalla DJUI y muestra JSON.
2. Catálogo remoto y caché válida.
3. Instalación Lua/ZIP con progreso, cancelación, recibo y limpieza.
4. Integración mínima con los flujos existentes del juego, sin duplicarlos.
5. Actualización individual con recuperación.
6. Después: asociación local, Actualizar todos y formatos/proveedores adicionales.

## Registro de ejecución

| Fecha | Acción / SHA / entorno | Evidencia | Pendientes |
|---|---|---|---|
| 2026-10-29 | Inspección inicial, base 99c8d21abe4d24649b508d0f216c4359ed5d2a7a | Ramas, remotos, build y workflow leídos; sin build ejecutado | Preparación técnica por realizar |


### Build local confirmado por el usuario — 2026-10-29

- Rama inspeccionada: mi-main.
- HEAD: 99c8d21abe4d24649b508d0f216c4359ed5d2a7a; sin cambios rastreados.
- Origen: build realizado por el usuario en Termux con su baserom.
- APK: build/us_pc/sm64coopdx.apk.
- Tamaño: 118880569 bytes.
- SHA-256: e3eab952fd55cd0d513457c4b8e8874d9bd60cb6479859d3fae9b3b89b4cf8b0.
- Verificación del agente: manifest y bibliotecas principales enumerados;
  integridad CRC de las entradas ZIP correcta. Esto no valida firma, instalación,
  ejecución ni comportamiento en Android.
- Comando de compilación y arranque confirmados posteriormente por el usuario;
  pendientes versiones de herramientas y matriz detallada de pruebas.
- La baserom no se agrega al repositorio ni se copia como parte de este registro.


### Procedimiento Termux y funcionamiento confirmados por el usuario

Fecha: 2026-10-29. Ejecutar desde la raíz del fork para los pasos de compilación.
Estos comandos documentan lo que realizó el usuario; el agente no ha quitado ni
instalado paquetes ni repetido la compilación durante este registro.

Preparación reportada:

```sh
pkg remove libglvnd
pkg install git wget make python getconf zip \
  apksigner clang binutils libglvnd-dev aapt libandroid-execinfo which x11-repo sdl2
```

Compilación y copia del APK reportadas:

```sh
make -j$(nproc)
cp build/us_pc/sm64coopdx.apk /storage/emulated/0
```

El usuario confirma que el juego funciona. Esta confirmación cubre funcionamiento
general; no se marcan pruebas individuales de mando, recuperación, rutas o
multijugador que no haya descrito expresamente.

La preparación corresponde al entorno existente del usuario. No asumir que la
misma orden de paquetes configura desde cero todos los repositorios Termux ni
que la baserom fue preparada automáticamente por esos comandos.

#### Diferencia importante frente al build Gradle

El Makefile detecta Termux y selecciona TARGET_ANDROID. Su ruta Android empaqueta
la biblioteca nativa y recursos usando platform/android como plantilla, alinea el
APK y firma usando las referencias ya presentes en el Makefile. No es el mismo
recorrido que platform/app de Gradle.

Para el MVP, conservar esta vía local comprobada. Al añadir un módulo C/C++,
revisar su inclusión en el Makefile y, cuando se valide la otra vía, Android.mk.
Si se cambia applicationId, manifest, recursos o Java/Kotlin, comprobar cómo se
regenera la plantilla Android usada por make: no suponer que editar solo los
archivos de Gradle cambia el APK producido en Termux.

La firma y la identidad actuales sirven como evidencia del build base, no como
decisión de identidad de publicación MI. Su separación sigue pendiente según la
sección 3. La comprobación de APK registrada antes no verificó la firma.

Próximo paso funcional: pantalla DJUI de Explorar mods con muestra del contrato
JSON, reutilizando el build Termux. CI sigue pendiente; no es necesario bloquear
el prototipo local por esa tarea. Antes de distribuir una edición MI, resolver
identidad, rutas y firma propias.


## Progreso MVP — etapa 1: catálogo de muestra DJUI

Fecha: 2026-10-29. Implementación autorizada por el usuario después de confirmar
que su build base funciona. La identidad propia, CI y preparación restante
siguen pendientes; no se declaran requisitos ya completados por tener este prototipo.

- [x] Entrada Explorar mods abre DJUI en lugar de abrir la web.
- [x] Lista de ocho muestras, cinco por página, con búsqueda y detalles.
- [x] Estado de muestra explícito y botón Instalar deshabilitado; sin instalación simulada.
- [x] Estados de catálogo inválido/vacío y búsqueda sin resultados; reintento ante error.
- [x] Estado por panel y liberación al salir; sin tareas asíncronas ni nueva persistencia.
- [x] Textos EN/ES/PT y fallback inglés nativo para otros idiomas.
- [x] Pruebas C++ de contrato, identidad, búsqueda y límites de texto correctas.
- [x] Muestra aceptada por el validador del scraper.
- [x] APK Termux compilado; firma, alineación y contenido verificados.
- [ ] Prueba física de la nueva pantalla, búsqueda desde página 2, detalles y retorno.
- [ ] Confirmar que paginación, tooltip y selección del menú original permanecen operativos.
- [ ] Verificar táctil/mando, temas, idiomas y tamaños en dispositivo.

APK: build/us_pc/sm64coopdx.apk, 119888185 bytes.
SHA-256: 87fc1a866998c79f58ada237fbf1c829484b941a89080eabb4331750477e35ef.

En esta sesión se compiló con la biblioteca C++ de Termux para evitar que la
biblioteca vendor de Codex interfiriera con zipalign. Ver comando, arquitectura,
limitaciones y matriz manual en [mi-docs/CATALOG_PREVIEW.md](CATALOG_PREVIEW.md).

Siguiente etapa del roadmap: catálogo remoto y última caché válida fuera del hilo
de UI. Luego instalación Lua/ZIP y recibos; actualización individual después.


## Seguimiento de prueba física y correcciones DJUI

Fecha: 2026-10-29.

- [x] Usuario confirmó que restablecer los ajustes solucionó controles táctiles
  invisibles y movimientos extraños. El agente no modificó su configuración.
- [x] Origen de mi-main verificado con reflog: android, base 99c8d21ab;
  sin integración propia de android-dev/newtouch.
- [x] Causa de TITLE identificada: archivos de idioma externos antiguos sin MI_CATALOG.
- [x] Respaldo nativo EN/ES/PT generado desde los INI MI, conservando los externos.
- [x] Desplazamiento implementado en menús estándar cuando no cabe el contenido:
  flechas táctiles, rueda y exposición de la selección del mando.
- [x] Geometría probada en alturas 480/720/1080 y escalas 0.5/0.75/1/1.5.
- [x] Regresión de idiomas antiguos y traducciones personalizadas probada.
- [ ] Prueba física del parche con escala grande, idiomas externos antiguos,
  flechas, mando y retorno entre Mods/MI/detalles.

Arquitectura y matriz: [mi-docs/DJUI_VIEWPORT_AND_TRANSLATIONS.md](DJUI_VIEWPORT_AND_TRANSLATIONS.md).
La escala persistente sigue siendo del juego; el desplazamiento se guarda solo
por panel y se libera al salir. No hay tareas asíncronas ni cachés persistentes
nuevas. La compilación y verificación del APK de este parche se registran en
ese documento al completarse.


APK del parche DJUI compilado y verificado: build/us_pc/sm64coopdx.apk,
119908665 bytes; SHA-256 7fb291e742f3daaecac48151d3965e4bb9bfb014937bd9da28033ecc8939920b.
Pruebas de catálogo, idiomas y geometría correctas; firma/alineación/CRC
correctos. Prueba física del parche pendiente. No se ejecutó Gradle.


### Confirmación física del parche — 2026-10-29

- [x] El usuario confirma que el APK con las correcciones funcionó en su teléfono,
  después de reportar TITLE y botones inferiores ocultos con escala DJUI aumentada.
- [ ] Sigue pendiente registrar por separado resultados de cada idioma, tema,
  mando y tamaño de la matriz; no se infieren de una confirmación general.

Próximo paso del roadmap: catálogo remoto y última caché válida. Esta confirmación
no implica que descarga/instalación o actualización de mods estén implementadas.


## Etapa 2 — catálogo remoto y caché

- [x] Catálogo real, consulta fuera del hilo UI y textos EN/ES/PT.
- [x] Caché válida de 15 minutos, actualización manual y respaldo ante errores.
- [x] HTTPS verificado con certificados del sistema y libcurl del APK.
- [x] Pruebas de parser, red/caché/cierre y endpoint real (1307 mods).
- [ ] Prueba física del catálogo remoto y estados sin conexión.

Arquitectura, límites y validación: [mi-docs/CATALOG_REMOTE.md](CATALOG_REMOTE.md).
Instalación y actualización de mods continúan pendientes.

- [x] APK de etapa 2 compilado; firma, alineación, CRC y libmain verificados.
- [x] Generadores sin diferencias y git diff --check correcto.

APK: build/us_pc/sm64coopdx.apk (120138041 bytes).
SHA-256: 117ad63202cfd2c396679e331403231b64b4fe8593cdce7c632d1b8ff1c8961d.


## Diseño amplio del catálogo — 2026-10-29

- [x] Catálogo y detalles al 96 % del ancho; navegación fuera del cuerpo desplazable.
- [x] Filas legibles con autor/categoría y capacidad según altura/escala DJUI.
- [x] Acceso al catálogo en cabecera derecha de Mods; flechas conservadas.
- [x] Anterior/siguiente EN/ES/PT y respaldo nativo generado.
- [ ] Validación física del diseño, teclado, táctil, mando y temas.

Detalles: [mi-docs/CATALOG_WORKSPACE.md](CATALOG_WORKSPACE.md).


Validación registrada: pruebas de parser, idiomas y geometría correctas;
generador sin diferencias; APK Termux compilado y verificado (firma, alineación
4 KiB, CRC y libmain coincidente). No se ejecutó Gradle. Prueba física pendiente.
APK: build/us_pc/sm64coopdx.apk, 120154425 bytes.
SHA-256: 6db65de2ac6df1aa598bd45d8addc4813fe034b710a3c2f44f5fc60939a7d870.


## Tarjetas y accesibilidad visual — 2026-10-29

- [x] Cuadrícula adaptable 2x2, con una columna cuando falta ancho.
- [x] Título, autor, categoría y contador mayores; escala DJUI respetada.
- [x] Detalles alineados arriba, mayor texto y ancho de lectura limitado.
- [x] Buscar mods EN/ES/PT con fallback para idiomas externos antiguos.
- [ ] Prueba física de esta iteración con táctil, mando, teclado y escala grande.

Diseño actual: [mi-docs/CATALOG_ACCESSIBILITY.md](CATALOG_ACCESSIBILITY.md).


APK de esta iteración compilado con Makefile Termux; firma, alineación 4 KiB,
CRC ZIP y coincidencia de libmain correctos. Generador y git diff --check correctos.
No se ejecutó Gradle. Prueba física pendiente.
APK: build/us_pc/sm64coopdx.apk, 120158521 bytes.
SHA-256: 4e221f79ca1d537d25bfcc8f779624d09ae337a1c0d18c87ac4487ed0a2a4253.


## Plan futuro: migración del catálogo JSON — 2026-10-29

- [x] Intención registrada: reducir principalmente tamaño, sin un problema actual
  de fluidez percibida; .db/SQLite es candidato, todavía no formato decidido.
- [x] Flujo actual documentado: JSON completo remoto, copia local válida de
  15 minutos, refresco manual, respaldo y búsqueda/paginación locales.
- [ ] Medir con el mismo catálogo: JSON, transferencia comprimida, publicación
  reducida para el juego y SQLite; bytes transferidos/almacenados, apertura y RAM.
- [ ] Definir formato/esquema con base en esas mediciones y consultas Android.
- [ ] Decidir si el scraper conserva JSON canónico y genera un artefacto adicional.
- [ ] Preservar identidades, idiomas y datos de instalación/asociación/actualización.
- [ ] Versionar publicación y validar tamaño/hash, estructura e integridad antes
  de adoptar el nuevo catálogo; mantener HTTPS y carga fuera del hilo UI.
- [ ] Conservar reemplazo atómico, último respaldo válido y funcionamiento offline.
- [ ] Planear transición de caché JSON y compatibilidad con clientes anteriores.
- [ ] Adaptar la fuente de datos conservando el contrato del menú y validar en
  teléfono: primer inicio, actualización, fallo, recuperación, búsqueda y paginación.
- [ ] Evaluar incrementos del catálogo como mejora separada y posterior.

Todos los puntos pendientes son planificación, no funcionalidades implementadas.
No cambia el JSON actual ni bloquea el MVP de instalación. El detalle de la
decisión queda en la sección 25 de
[SM64CDPY_IDEAS_Y_FORK.md](SM64CDPY_IDEAS_Y_FORK.md).


### Confirmación del usuario — 2026-10-29

El usuario confirma que la nueva visualización funcionó a la perfección y
resultó muy accesible en su teléfono. Se registra aceptación general del diseño
de tarjetas y lectura. No se presume cobertura individual de toda la matriz
de mando, teclado, idiomas, temas y estados de red. Las notas anteriores de
prueba física pendiente reflejan el estado previo a esta confirmación.


## Procedimiento y reglas de mantenimiento — 2026-10-29

- [x] [MANTENER-UPDATES.md](MANTENER-UPDATES.md): configuración recomendada,
  sincronización por SHA en rama aislada, revisión semántica, pruebas y promoción.
- [x] [RULES.md](RULES.md): contratos, riesgos silenciosos, inventario e historial
  exacto de los tres commits publicados sobre Android estable.
- [x] Configurar upstream en este clon y comprobar la punta remota candidata.
- [ ] Configurar CI/protecciones y ejecutar una sincronización real probada.

Crear la documentación no ejecuta esos pasos ni habilita actualizaciones de mods.


## Guía operativa del repositorio — 2026-10-29

- [x] [AGENTS.md](../AGENTS.md) creado en la raíz con alcance global del fork.
- [x] Define lectura obligatoria de RULES.md y MANTENER-UPDATES.md y rutas a planes.
- [x] Incluye accesibilidad, UX, arquitectura nativa, ownership, validación,
  traducciones, protección de datos y revisión semántica tras sincronizaciones.

Documentación solamente: no configura upstream, CI, protección de ramas ni instala
funcionalidades futuras. Revisar el estado efectivo por clon antes de afirmar
que esos controles están habilitados.


## Preparación real de upstream — 2026-10-29

- [x] Configuración local aplicada y auditada; solo android estable en upstream.
- [x] Fetch de ambos remotos y SHA estable confirmado; 0 commits upstream pendientes.
- [x] Tracking MI/origin y espejo/upstream, push a origin y bloqueo local upstream.
- [x] Pull ff-only; rerere con revisión explícita (autoupdate=false).
- [x] Herramienta reproducible setup/check para futuros clones y checkpoint local.
- [ ] Primera sincronización de motor cuando exista una punta estable nueva.

Guía: [UPSTREAM_SETUP.md](UPSTREAM_SETUP.md). Registro:
[upstream/2026-10-29-setup.md](upstream/2026-10-29-setup.md).
No se configuraron CI/protecciones GitHub ni se promovió un motor nuevo.


## Preparación del workflow MI — 2026-10-29

- [x] mi-actions-build.yml manual, sin ROM ni secrets, artefacto ARM64 y retención 7 días.
- [x] SDK/NDK fijados, C++17 y parámetros ABI/paralelismo Gradle.
- [x] Corregida fuente de recursos Gradle a raíz; fallo explícito si faltan carpetas.
- [x] Verificador APK, pruebas de artefactos incompletos y actionlint correctos.
- [x] Publicar el commit y decidir cambio de rama predeterminada a mi-main.
- [x] Primera ejecución real en Actions correcta tras corregir el grafo Gradle.
- [ ] Prueba física del APK de Gradle en dispositivo.

El make del host prepara fuentes generadas; no se publica un artefacto de escritorio.
Detalle, comandos y limitaciones: [MI_ACTIONS_BUILD.md](MI_ACTIONS_BUILD.md).


### Primera ejecución CI: grafo Gradle — 2026-10-30

Run 41012336610: tests y make host correctos; Gradle rechazó :app porque la entrada
heredada era :platform:app. Se prepara settings Android standalone y preflight de
tareas. Reintento real pendiente al crear esta nota; no marcar APK CI completado.


### Primera compilación CI completa correcta — 2026-10-30 UTC

- [x] Run 41013511475 correcto con e7d0ac815; job Android en 8 min 48 s.
- [x] Compilación, firma, verificación y subida de artefacto completadas sin ROM.
- [x] Corrección del grafo documentada, con preflight para evitar repetición.
- [ ] Descarga y contraste local independiente: interrumpidos por el usuario.
- [ ] Instalación y prueba física del nuevo APK; no se infiere de CI.

[Registro de evidencia y límites](ci/2026-10-30-first-success.md). Las notas previas
de ejecución pendiente describen el estado anterior a este resultado.


## Instalador Lua/ZIP local — 2026-10-30

- [x] Usuario confirma funcionamiento del APK CI anterior en su teléfono.
- [x] Selección de versión/archivo en detalles e instalación directa Lua/ZIP.
- [x] Progreso nativo por etapa, cancelación y errores localizados EN/ES/PT.
- [x] Recibos schema 1, manifiesto SHA-256 y recuperación de entrega interrumpida.
- [x] Preservación de destinos existentes; activación/refresco originales intactos.
- [x] test_catalog.py: parser/selección/idiomas/layout correctos.
- [x] test_requests.py --bundled-curl --live: correcto; catálogo HTTPS de 1307 mods.
- [x] test_install.py --bundled-curl --live: correcto, incluida recuperación;
  noclip.lua real instalado por HTTPS en una raíz temporal, fuera de mods reales.
- [x] make Termux local correcto, firma válida, zipalign correcto, CRC y libmain
  empaquetada coincidentes con la compilación.
- [ ] Probar físicamente el instalador nuevo: Lua, ZIP, cancelación, errores,
  almacenamiento Android real, Refrescar/activación original, escala y navegación.
- [ ] Validar ruta Gradle con este instalador en una sesión futura autorizada.
- [ ] Commit/push/publicación: no realizados por instrucción del usuario.

APK local: build/us_pc/sm64coopdx.apk; 121956665 bytes.
SHA-256: 2f242cd1ae0ef555fca84eb4d00120cecf8ffdf3b36d9f464259b4b15d478084.
libmain.so SHA-256: 154b0259649b9724306a5e08734b43ab7d531112b98165340180c63cf7fe78e0.
Log de build local: /data/data/com.termux/files/usr/tmp/mi-preview-build.log.
El build fue incremental; no demuestra un clon limpio en otro dispositivo.
Advertencias observadas: recetas repetidas heredadas del Makefile y aviso miniz
sobre IO de archivos grandes en la suite; sin errores de compilación.
Contrato, archivos, límites y matriz física: [CATALOG_INSTALLATION.md](CATALOG_INSTALLATION.md).


### Corrección de botones superpuestos tras capturas 09:22–09:23

- Causa confirmada: alineación derecha combinada con offsets desde izquierda;
  Instalar coincidía con Volver y Siguiente con Anterior. Corregidos los dos pares.
- Sustituido separador no disponible en la fuente del juego por |.
- Suite catálogo/idiomas/geometría, build local, firma, zipalign, CRC y coincidencia
  de libmain correctos. Backend de instalación sin cambios en esta corrección.
- APK corregido: /sdcard/Download/sm64coopdx-MI-install-local.apk; 121956665 bytes.
- SHA-256: 39e38025a27286feb07b74c58dbe056fde08cc8f21c6406850d5f6750a325181.
- Pendiente: nueva prueba física de los cuatro botones, scroll, escala e instalación.
- Sin commit, push ni Actions. El hash del apartado anterior corresponde al APK previo.


### Corrección de bloqueo falso y progreso oculto — 2026-10-30

- [x] Causa reproducida sin alterar mods: flock sobre install.lock del usuario
  devuelve ENOSYS (38); se comunicaba incorrectamente como otra instalación.
- [x] Sustituido por fcntl F_SETLK; solo conflictos reales producen BUSY.
- [x] Estado, archivo, bytes descargados y resultado siempre visibles en el pie;
  progreso de extracción dentro de cada archivo y verificación indeterminada.
- [x] Éxito publicado después de liberar exclusión y completar limpieza.
- [x] Suite --bundled-curl --live --storage-root /sdcard/Download correcta;
  noclip.lua descargado por HTTPS e instalado en una carpeta temporal externa.
- [x] Suite final sin --live sobre /sdcard correcta, incluyendo progreso observado,
  bloqueo entre procesos, liberación, Lua/ZIP, cancelación, errores y recuperación.
- [x] Suite de catálogo/idiomas/geometría, generados y diff --check correctos.
- [x] Build Termux, firma, zipalign, CRC y libmain empaquetada coincidentes.
- [ ] Prueba física del menú corregido y descubrimiento con Refrescar.
- [ ] Ruta Gradle/CI con este instalador: no ejecutada en este cambio.

APK actualizado: /sdcard/Download/sm64coopdx-MI-install-local.apk; 121956665 bytes.
SHA-256: 11440a7dd784fa178f57a7fe0ad3e9faf4401c305f3f850c745d582374cbbc61.
Sin commit, push ni Actions; los hashes anteriores identifican APK previos.
Detalle de causa y contratos: [CATALOG_INSTALLATION.md](CATALOG_INSTALLATION.md).


## Confirmación del usuario y autorización de publicación — 2026-10-30

El usuario confirmó que el instalador corregido funciona y autorizó commit y push
hacia mi-main. Corresponde al APK SHA-256
11440a7dd784fa178f57a7fe0ad3e9faf4401c305f3f850c745d582374cbbc61.
Esta confirmación general no acredita todos los escenarios de la matriz física;
los casos no informados y la ruta Gradle con el instalador siguen pendientes.
Las notas anteriores de «sin commit/push» describen el desarrollo local previo.
No se autoriza ni ejecuta una release, subida de APK ni Actions por este registro.
