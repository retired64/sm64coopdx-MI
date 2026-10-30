# Mantener MI actualizado con Android estable

> Estado local posterior — 2026-10-30: instalación Lua/ZIP y recibos schema 1
> implementados y con funcionamiento confirmado por el usuario tras corregir
> bloqueo y progreso. El usuario autorizó commit/push a mi-main. Las referencias históricas
> a Instalar deshabilitado quedan superadas por [CATALOG_INSTALLATION.md](CATALOG_INSTALLATION.md).
> Reemplazo/actualizaciones de mods, asociaciones previas y RAR/7z siguen pendientes.
> El usuario confirmó funcionamiento del APK CI anterior; el instalador nuevo
> requiere su propia prueba física.


Fecha de referencia: 2026-10-29. Alcance: fork `retired64/sm64coopdx-MI`, rama
`mi-main`. Reglas de implementación e inventario: [RULES.md](RULES.md).
Este documento prescribe el procedimiento; no acredita que se haya ejecutado.
Los comandos Git se ejecutan desde la raíz del repositorio, no desde mi-docs/.

## 1. Estado comprobado y objetivo

- Base integrada: `99c8d21abe4d24649b508d0f216c4359ed5d2a7a`, de `android`.
- HEAD MI al preparar upstream: `76b9612775d0e0e13b9aeec716d93c8a77b97242`.
- `origin`: `git@github.com:retired64/sm64coopdx-MI.git`.
- Fuente elegida: `https://github.com/ManIsCat2/sm64coopdx.git`, rama `android`.
- `upstream` configurado y consultado el 2026-10-29: android estable sigue en
  `99c8d21ab`, igual a la base integrada; no hay novedades pendientes.
- Configuración efectiva por clon: [UPSTREAM_SETUP.md](UPSTREAM_SETUP.md).
- rerere local activado, con autoupdate=false para revisar resoluciones.
- No se han configurado protección de ramas, CI MI, sincronización programada,
  identidad pública propia del APK ni publicación automática.
- El diseño accesible tiene aceptación general del usuario en su teléfono;
  no implica haber probado cada idioma, tema, mando o condición de red.

Objetivo: integrar nuevas bases Android sin perder catálogo, accesibilidad,
caché, traducciones ni comportamiento nativo. Conservar historial y una versión
probada recuperable. Ningún merge se considera compatible solamente por compilar.
La rama `android` es el canal estable elegido; no una garantía de ausencia de bugs.

## 2. Tres actualizaciones diferentes

| Operación | Qué cambia | Control actual |
|---|---|---|
| Sincronizar upstream | Código del motor y port Android | Desarrollador, mediante este flujo |
| Actualizar catálogo | Datos remotos y caché local | Botón del catálogo; no reemplaza el APK |
| Actualizar mods instalados | Archivos concretos y recibos | Funcionalidad futura, todavía no implementada |

Actualizar el JSON no incorpora una nueva versión del juego. Sincronizar el
juego tampoco autoriza reemplazar automáticamente mods. Los futuros botones de
actualización de mods deben usar identidades y recibos propios, no la paginación
ni el estado de descarga del catálogo como prueba de instalación.

## 3. Organización de ramas y responsabilidad

| Referencia | Propósito | Regla |
|---|---|---|
| `origin/mi-main` | Código MI integrado y validado | Sin force push ni rebase de historia pública |
| `upstream/android` | Referencia remota del port elegido | Se obtiene con fetch; no desarrollar encima directamente |
| `android` / `origin/android` | Espejo opcional de la base | Sin commits MI; solo avance directo cuando sea posible |
| `feature/mi-<tema>` | Cambios funcionales | Una finalidad, pruebas y documentación |
| `sync/android-<fecha>-<intento>` | Integración de un SHA candidato | Resolver y probar aquí antes de promover |
| `checkpoint/mi-<fecha>-<sha>` | Retención local de un punto probado | No es release ni sustituye respaldo de datos |

No tomar `android-dev`, `newtouch` ni ramas PC como fuente ordinaria. Que Android
estable integre internamente esas ramas no obliga a incorporarlas por otra ruta.
Si la fuente cambia, documentar la decisión y la nueva matriz antes de adoptarla.

## 4. Preparación inicial, una vez por clon

Primero leer RULES.md. La preparación reproducible, validada y recomendada es:

```sh
python tools/mi/configure_upstream.py setup
python tools/mi/configure_upstream.py check
```

Ver [UPSTREAM_SETUP.md](UPSTREAM_SETUP.md) para alcance, valores y errores. setup
no mezcla código; check es offline. El fetch no actualiza una rama local existente.

Si se prepara manualmente, comprobar árbol de trabajo y remotos:

```sh
git status --short --branch
git remote -v
git branch -vv
```

Con cambios locales: guardarlos en su rama/commit correspondiente; no ejecutar
reset ni sobrescribirlos para despejar el merge. Verificar la URL si `upstream`
ya existe. Solo si no existe:

```sh
git remote add -t android --no-tags upstream https://github.com/ManIsCat2/sm64coopdx.git
```

Obtener referencias sin mezclar código:

```sh
git fetch origin
git fetch upstream
```

`upstream` es configuración local en `.git/config`: no viaja en los commits ni
aparece automáticamente al clonar el fork en otro teléfono. Este paso se repite
por clon. Los cambios en código, pruebas y documentación sí viajan con Git.

La configuración local aplicada incluye rerere.enabled=true y
rerere.autoupdate=false. Una resolución recordada debe revisarse; no prueba
corrección. El bloque manual anterior no establece por sí solo todos los valores
de protección local: usar setup o completar la tabla de UPSTREAM_SETUP.md.

## 5. Elegir y fijar un candidato

Con `mi-main` limpia:

```sh
git switch mi-main
git pull --ff-only origin mi-main
git rev-parse HEAD
git rev-parse upstream/android
git merge-base HEAD upstream/android
git log --oneline HEAD..upstream/android
git diff --stat HEAD...upstream/android
```

Registrar el HEAD MI anterior y el SHA candidato completo. Comparar también el
SHA upstream integrado anterior con el candidato para revisar cambios del motor,
no solamente diferencias frente a MI. En la primera sincronización la base es
`99c8d21abe4d24649b508d0f216c4359ed5d2a7a`.

Si `git merge-base --is-ancestor <BASE_UPSTREAM_ANTERIOR> <SHA_CANDIDATO>` falla,
investigar reescritura o cambio de línea de upstream. No forzar el espejo ni
suponer que la nueva punta es una continuación normal. Los valores entre `< >`
son marcadores: sustituirlos antes de ejecutar; no pegarlos como comandos.

Elegir el SHA probado, no una referencia móvil durante toda la revisión. Un
fetch posterior no debe cambiar silenciosamente el candidato de la misma prueba.

## 6. Integración aislada

Ejemplo de nombre; usar fecha/intento no existentes:

```sh
git branch checkpoint/mi-2026-11-01-02964f435
git switch -c sync/android-2026-11-01-01
```

Después ejecutar `git merge --no-ff --no-commit <SHA_CANDIDATO>` sustituyendo el
marcador por el SHA elegido. `--no-commit` permite revisar antes de crear el merge;
`--no-ff` evita que un avance directo salte esa pausa. Si no hay novedades, Git
puede indicar que ya está actualizado: no crear un registro falso de integración.

No re-aplicar los tres commits MI mediante cherry-pick sobre cada nueva base:
el merge conserva el historial y combina desde el ancestro común. No mantener
un ZIP de código propio como mecanismo principal de actualización.

## 7. Resolver conflictos con intención

```sh
git status
git diff --name-only --diff-filter=U
git diff --cc
```

Por cada archivo: identificar intención upstream, intención MI, contrato común y
solución elegida. Revisar la versión base y ambos lados cuando sea necesario.
`ours` en este merge es la rama sync que parte de MI; `theirs` es el candidato
upstream. No usar esas opciones para aceptar carpetas enteras sin análisis.

- Si upstream ya implementó nuestro arreglo, adaptar o retirar la duplicación
  verificando comportamiento equivalente, no conservar dos manejadores.
- Si cambió una API, revisar todos sus lectores/escritores y su ciclo de vida.
- Si ambos crearon un mismo archivo, comparar responsabilidades e identidad;
  no reemplazarlo solo porque el prefijo parece propio.
- Si un `.inc` generado entra en conflicto, resolver primero sus fuentes y
  regenerarlo; no editar manualmente la salida como solución definitiva.
- Añadir con `git add` solo archivos resueltos. Revisar el diff preparado antes
  de crear el commit de merge.

Para abandonar una integración no terminada existe `git merge --abort`; comprobar
antes el estado y conservar cualquier trabajo que no perteneciera al merge.
No utilizar reset destructivo como solución habitual. Si el merge ya se creó,
es preferible conservar/recrear una rama de prueba que reescribir historia pública.

## 8. Revisión obligatoria aunque no haya conflictos

Usar la matriz de RULES.md. Comprobar especialmente:

1. Makefile y Android.mk siguen compilando el módulo MI real y sus dependencias.
2. El APK incluye el `libmain.so` recién compilado, no un artefacto viejo.
3. La entrada de Mods conserva contexto seguro, visibilidad, retorno y tooltip.
4. Render y callbacks mantienen reconocimiento de ancestros, clip y foco;
   una función con igual firma puede haber cambiado de significado o momento.
5. La escala sigue aumentando texto; la cuadrícula responde con menos celdas.
6. Los INI externos antiguos conservan fallback y nunca son sobrescritos.
7. Worker/cancelación/snapshots y generación de caché mantienen ownership.
8. El curl incluido sigue verificando HTTPS con la confianza del sistema.
9. Rutas, configuración, escáner y ciclo de vida siguen siendo los reales del port.
10. Cambios de protocolo o versión de red se contrastan con la compatibilidad
    multijugador declarada; no prometerla por conservar el nombre del juego.

Un defecto silencioso debe tener disparador, resultado esperado/observado,
causa, solución, prueba y riesgo residual registrados. Si no puede comprobarse
un contrato crítico, el candidato permanece en sync; no promoverlo por prisa.

## 9. Validación por capas

### Código y contratos

```sh
python tools/mi/embed_catalog.py --check
python tools/mi/test_catalog.py
python tools/mi/test_requests.py --bundled-curl
python tools/mi/test_requests.py --bundled-curl --live
git diff --check
```

La prueba `--live` requiere red y depende del estado de la publicación. Registrar
una indisponibilidad externa por separado de una regresión, sin marcarla como
aprobada. Los tests usan temporales y no deben tocar mods del usuario. Si el
sistema no tiene el entorno Android/Termux, registrar qué variante se ejecutó;
probar curl del host no sustituye probar el curl del APK.

### Compilación Android en Termux

Preparación actualmente usada (no instalar automáticamente por leer la guía):

```sh
pkg install git wget make python getconf zip apksigner clang binutils libglvnd-dev aapt libandroid-execinfo which x11-repo sdl2
make -j$(nproc)
```

En el entorno de desarrollo se retiró `libglvnd` para usar `libglvnd-dev`;
comprobar paquetes/conflictos del teléfono antes de cambiar su instalación.
La baserom necesaria se aporta localmente, nunca se añade a Git ni al APK público
por este procedimiento. Revisar requisitos y toolchain si upstream cambia.

En la sesión Codex se necesitó, solo por interferencia de bibliotecas del entorno:

```sh
env LD_LIBRARY_PATH=/data/data/com.termux/files/usr/lib make -j$(nproc)
```

No convertir ese workaround en cambio global de bibliotecas. El artefacto actual
es `build/us_pc/sm64coopdx.apk`. Ante cambios de build/dependencias o evidencia de
objetos obsoletos, realizar una compilación limpia/aislada siguiendo la base nueva;
no tratar un build incremental como prueba de reproducibilidad en otro teléfono.
Android.mk está adaptado, pero la ruta Gradle no se ha validado en esta sesión.

### Verificación del artefacto

```sh
apksigner verify build/us_pc/sm64coopdx.apk
zipalign -c -p 4 build/us_pc/sm64coopdx.apk
sha256sum build/us_pc/sm64coopdx.apk
```

Comprobar además CRC del ZIP y que `lib/arm64-v8a/libmain.so` empaquetado coincide
con `build/us_pc/libmain.so`. Registrar tamaño/hash del APK y toolchain utilizados.
La firma válida no demuestra identidad de actualización ni instalación exitosa:
comparar certificado, package ID y versión cuando cambie el empaquetado.

### Matriz física mínima

| Flujo | Evidencia esperada |
|---|---|
| Inicio y partida base | Controles, sonido y render operativos |
| Mods local | Lista, búsqueda, selección y refresco originales operativos |
| Acceso a MI y vuelta | Botón en contexto correcto, tooltip y foco restaurados |
| Cuadrícula | Táctil y mando en ambos ejes; página/query coherentes |
| Escala/tema | Sin acciones inaccesibles; una columna cuando falta ancho |
| Detalles largos | Lectura completa mediante flechas, sin vacío superior artificial |
| Idiomas | EN/ES/PT y archivos externos antiguos; sin TITLE/SEARCH literales |
| Red/caché | Primera carga, copia reciente, refresco, offline, error y recuperación |
| Cierre durante consulta | Sin bloqueo, crash ni callback a UI destruida |
| Android | Minimizar/reabrir y actualizar APK preservando configuración/datos |
| Multijugador | Comprobar lo que se declare soportado con la nueva base |

Instalación/recibos/Update all se incorporarán a esta matriz cuando existan;
actualmente Instalar debe continuar deshabilitado. Marcar cada prueba como
aprobada, fallida o pendiente, con dispositivo/versión/escala cuando corresponda.

## 10. Commit, promoción y publicación

Crear el merge después de revisar los archivos preparados; registrar pruebas
posteriores contra ese resultado. Si hay correcciones adicionales, incluirlas
en commits claros y repetir las pruebas afectadas. Antes de promover:

```sh
git status --short
git log --oneline mi-main..HEAD
git diff --stat mi-main..HEAD
```

Opcional: subir la rama sync para revisión mediante PR. Un check verde no es
permiso para publicar. Con candidato aprobado y `mi-main` sin avances paralelos:

```sh
git switch mi-main
git merge --ff-only sync/android-2026-11-01-01
git push origin mi-main
```

Si el avance directo falla, `mi-main` avanzó o la historia difiere: incorporar
su estado nuevo en sync, reconciliar y repetir checks. No eliminar `--ff-only`
automáticamente para saltar esa comprobación. Un push rechazado requiere fetch
y revisión; nunca force push por comodidad.

Promover código no crea una release. Tags, assets y publicación del APK requieren
la autorización correspondiente. El botón Sync fork puede mantener un espejo
limpio; no debe forzar ni sustituir `mi-main` por Android oficial.

## 11. Registro obligatorio por sincronización

Añadir un registro versionado, por ejemplo `mi-docs/sync/AAAA-MM-DD-01.md`.
Ese directorio/archivo se creará al ejecutar una sincronización, no existe aún.
Usar este esquema:

```text
Fecha / responsable / rama sync:
SHA MI anterior:
SHA upstream anterior y candidato:
Versión motor / versión MI / esquema catálogo / esquema recibos si existe:
Cambios upstream relevantes:
Archivos en conflicto y solución razonada:
Revisión semántica sin conflictos (contratos examinados y resultados):
Archivos propios adaptados o retirados por equivalencia upstream:
Toolchain, comandos, resultados y logs:
APK: nombre, tamaño, SHA-256, package ID y certificado comprobados:
Dispositivos, escalas, idiomas, entrada y escenarios probados:
Pruebas pendientes y riesgo residual:
Compatibilidad de datos / respaldo / recuperación:
SHA del merge probado y correcciones posteriores:
Decisión de promoción / revisión / publicación:
```

El SHA del merge puede anotarse en un commit de documentación posterior para
no intentar insertar dentro de un commit su propio hash. Dejar explícito cuál
fue el código probado y si solo cambió documentación después de probarlo.

## 12. Recuperación y escalabilidad

Antes de probar una actualización respaldar los datos relevantes. Una rama o
APK anterior no restaura automáticamente caché, ajustes ni futuros recibos.
Un downgrade puede ser imposible o incompatible con datos migrados. Ante un
fallo publicado, detener promoción/publicación, conservar logs y preparar una
corrección hacia delante; revertir un merge público exige decidir el parent y
planear cómo se reintegrará después. No recetar `revert -m` sin revisar historia.

Revisar upstream periódicamente; automatizar detección y preparación de sync/PR
solo después de validar el flujo manual. Alertar cambios en puntos sensibles,
fijar herramientas/dependencias y requerir revisión humana y pruebas físicas.
No programar merges, upgrades de dependencias ni releases automáticos por el
mero hecho de que exista una nueva punta Android.

Preparación aplicada en este clon; repetir setup en clones nuevos. Pendientes:
protecciones y checks
requeridos en GitHub, CI Android dedicado a MI, identidad/versionado público,
registro reproducible de entorno y pruebas en otro teléfono. No afirmar que esos
controles existen hasta verificar su configuración efectiva.


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
