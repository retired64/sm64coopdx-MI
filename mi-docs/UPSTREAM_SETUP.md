# Upstream preparado: configuración por clon

Preparación ejecutada el 2026-10-29. Procedimiento general:
[MANTENER-UPDATES.md](MANTENER-UPDATES.md). Evidencia de esta sesión:
[upstream/2026-10-29-setup.md](upstream/2026-10-29-setup.md).

## 1. Remotos y seguimiento efectivos

| Elemento | Valor |
|---|---|
| origin fetch/push | git@github.com:retired64/sm64coopdx-MI.git |
| upstream fetch | https://github.com/ManIsCat2/sm64coopdx.git |
| upstream push | disabled://upstream-read-only |
| upstream fetch refspec | refs/heads/android:refs/remotes/upstream/android |
| mi-main tracking | origin/mi-main |
| android tracking | upstream/android |
| destino de push por defecto | origin |
| push.default | simple |
| pull.ff | only |
| upstream.tagOpt | --no-tags |
| upstream.prune | true |
| rerere.enabled | true |
| rerere.autoupdate | false |

branch.mi-main.pushRemote y branch.android.pushRemote apuntan a origin. La rama
android continúa como espejo sin commits MI; una nueva punta de upstream no la
mueve automáticamente. Las referencias experimentales ya existentes en origin
no se borraron, pero upstream solo sigue android estable.

El bloqueo de push upstream es una protección local mediante URL no operativa,
no un permiso de servidor ni una protección GitHub. Un usuario puede cambiarla;
no garantiza impedir comandos deliberados con una URL explícita. pull.ff=only
impide merges implícitos por pull, no revisa semántica ni sustituye ramas sync.
rerere recuerda resoluciones; autoupdate=false exige revisar y añadir a mano.

No se cambiaron configuración global, código del juego, dependencias, firma,
identidad del APK, protecciones GitHub ni workflows. Tampoco se hicieron merges,
pushes ni releases en esta preparación.

## 2. Configurar otro teléfono de forma reproducible

La herramienta versionable es
[tools/mi/configure_upstream.py](../tools/mi/configure_upstream.py).
Requiere Python, Git y un clon íntegro de MI con mi-main local; setup necesita red
y acceso al fork. Ejecutar desde la raíz del repositorio:

```sh
python tools/mi/configure_upstream.py setup
python tools/mi/configure_upstream.py check
```

En un clon nuevo, primero seleccionar mi-main. La herramienta valida origin y
upstream antes de mutar configuración; rehúsa URLs inesperadas. Acepta origin SSH
o HTTPS del fork exacto. Configura solo .git/config, hace fetch de origin/upstream
sin importar tags y muestra SHAs/divergencia. Si no existe android local, la crea
siguiendo upstream/android; si ya existe, no mueve su punta. No hace checkout,
merge, reset ni push y puede repetirse sin duplicar valores de configuración.

Si se usa otro fork o un remote con URL distinta, revisar/adaptar explícitamente
las identidades esperadas; no quitar la validación solo para silenciar un error.
setup detecta cambios no fast-forward comparando la referencia previa con la
obtenida. Si upstream reescribe historia, se detiene: la referencia remota pudo
actualizarse con fetch, pero las ramas locales no se integran ni se fuerzan.
La ausencia de '+' en un refspec remoto no sustituye esa comprobación explícita.
En el primer fetch no hay punta previa local; se verifica al menos historia común.

check no usa red ni modifica configuración; audita valores y compara referencias
locales del último fetch. Un check correcto no afirma que GitHub no avanzó desde
entonces. setup obtiene referencias recientes, pero tampoco certifica estabilidad
ni compatibilidad de la nueva base. Un fetch fallido puede dejar configuración
aplicada y referencias parcialmente obtenidas: revisar y repetir tras resolverlo.

## 3. Uso cotidiano

```sh
python tools/mi/configure_upstream.py check
git fetch --no-tags upstream
git rev-parse upstream/android
git log --oneline mi-main..upstream/android
git rev-list --left-right --count mi-main...upstream/android
```

El primer número cuenta commits solo en MI; el segundo, commits upstream aún no
incluidos en MI. Registrar el SHA integrado anterior y comprobar que es ancestro
del candidato aunque fetch funcione. El comando fetch aislado no ejecuta la
comprobación de reescritura del setup. Si hay novedades, usar MANTENER-UPDATES.md
para crear sync, fijar SHA, revisar conflictos y contratos, probar y promover.
No ejecutar pull upstream android dentro de mi-main como sustituto de ese flujo.

El espejo local android puede avanzar solo después de revisar su divergencia y
mediante fast-forward explícito en su propio contexto. No forzarlo ni copiarle
commits MI. Mantenerlo actualizado no integra novedades en mi-main.

## 4. Persistencia y alcance

.git/config, referencias remotas, caché rerere y checkpoints locales no viajan
con los commits. El script y estas guías sí viajan cuando se versionan y suben.
Por eso setup se ejecuta una vez por clon. Esta sesión dejó
checkpoint/mi-2026-10-29-76b961277 apuntando al HEAD previo probado; no se publicó
esa rama ni es respaldo de partidas/configuración/mods del teléfono.

El estado upstream vigente se consulta en Git; el registro fechado es evidencia
histórica, no una referencia mutable de última versión. Actualizar el registro
de base integrada después de cada promoción real, separándolo del último fetch.

## 5. Preparación que sigue siendo independiente

- CI específico MI y checks de build/tests exigidos en GitHub.
- Protección de ramas y elección/verificación de rama predeterminada en GitHub.
- Identidad pública propia y proceso de publicación del APK.
- Primera sincronización real en sync cuando android estable avance.

Nada de lo anterior se considera configurado por este script. No bloquea continuar
el MVP de instalación con la base actual, que coincide con la estable consultada.
