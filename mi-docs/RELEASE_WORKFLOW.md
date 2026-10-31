# Publicar una release Android MI

El workflow manual `.github/workflows/mi-actions-release.yml` publica la versión
que declaran Gradle y el manifiesto. Primera versión: `v1.5.1-mi.1`. Solo admite
la rama `mi-main`; no publica por push, PR ni por ejecutar el build de artefactos.

## Flujo

1. Valida nombre visible, código y correspondencia con la versión del motor.
2. Reutiliza `mi-actions-build.yml` del mismo commit: contratos, compilación ARM64,
   firma heredada, alineación y verificación del APK sin ROM.
3. Recupera únicamente el artefacto de ese run y commit; verifica hash y tamaño.
4. Rechaza tags/releases existentes; nunca sobrescribe una versión publicada.
5. Crea un borrador con APK, build-info.json y SHA256SUMS.txt. Solo después de
   subir todos los archivos publica la release. Si falla, revisar el borrador
   antes de reintentar; no se elimina automáticamente evidencia ni tags.

La opción `prerelease` permite marcar una versión de prueba. Por defecto es una
release normal. No requiere PAT, ROM ni secretos propios: utiliza GITHUB_TOKEN
y la clave pública heredada. Solo el job de publicación tiene contents:write.
Las reglas del repositorio deben permitirle crear el tag.

## Ejecutar desde tu clon

Después de subir ambos workflows a GitHub:

```sh
gh workflow run mi-actions-release.yml --ref mi-main -f prerelease=false
gh run list --workflow mi-actions-release.yml --branch mi-main --limit 5
```

GitHub CLI usa el repositorio del clon. En Actions también se puede seleccionar
MI Android Release → Run workflow → mi-main. Si el workflow nuevo no figura
en la interfaz, comprobar que está en la rama predeterminada de GitHub; no cambiar
upstream ni ramas para intentar resolverlo sin revisar esa configuración.

Antes de ejecutarlo, comprobar la prueba física del APK y las notas de release
del workflow. Para otra revisión editar el versionName en ambos archivos Android
según VERSIONING_AND_RELEASES.md; no aumentar el código upstream por una revisión MI.

Preparado localmente; crear el workflow no acredita una ejecución remota ni
publica la primera release. El resultado de Gradle y publicación queda pendiente
hasta su ejecución manual.

Referencias: [workflows reutilizables](https://docs.github.com/en/actions/how-tos/reuse-automations/reuse-workflows),
[gh release create](https://cli.github.com/manual/gh_release_create).
