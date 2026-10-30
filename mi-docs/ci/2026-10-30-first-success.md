# Primera compilación Android MI verificada en Actions

Fecha: 2026-10-30 UTC. Rama: mi-main. Tipo: corrección del grafo Gradle y
validación remota completa; no sincronización del motor ni publicación de release.

## Ejecuciones y causa

| Ejecución | Commit | Resultado |
|---|---|---|
| [41012336610](https://github.com/retired64/sm64coopdx-MI/actions/runs/41012336610) | b7b0503ce145d42c817724ac73e941ec11c68170 | Falló al seleccionar el módulo Gradle |
| [41013511475](https://github.com/retired64/sm64coopdx-MI/actions/runs/41013511475) | e7d0ac815221b66a15812dfc19d89ec2291011e0 | Correcta, job Android en 8 min 48 s |

La primera ejecución pidió :app:assembleRelease, pero Gradle encontraba el
settings.gradle heredado de la raíz, que incluye :platform:app. No era falta
de ROM ni fallo de un script de mod. Tests y make del host habían terminado.

## Corrección versionada

Commit: [e7d0ac815](https://github.com/retired64/sm64coopdx-MI/commit/e7d0ac815221b66a15812dfc19d89ec2291011e0).

- platform/settings.gradle define el proyecto Android standalone con módulo :app.
- El workflow usa -p platform y comprueba :app:tasks antes del make del host.
- La copia de recursos calcula la raíz desde projectDir del módulo app, lo que
  funciona con la entrada standalone y la estructura heredada.
- El settings.gradle de la raíz se conserva. No se cambia la base upstream ni
  el código runtime del catálogo/juego.

## Evidencia del run exitoso

Se observó gh run watch --exit-status terminar con código 0 y job Android correcto.
Los pasos requeridos del workflow completaron:

1. Pruebas de catálogo, idiomas, geometría, solicitudes HTTP y validación de APK.
2. Instalación del SDK/NDK fijados y comprobación del grafo standalone.
3. Compilación preparatoria del host y fuentes generadas sin baserom.
4. Gradle/NDK ARM64 release.
5. Alineación, firma de prueba heredada y comprobación de firma.
6. Verificación de CRC, bibliotecas empaquetadas frente a stripped_native_libs,
   recursos EN/ES/PT, carpetas auxiliares y símbolos MI/carga ROM.
7. Generación del informe y SHA-256, y subida del artefacto.

Artefacto esperado por la configuración ejecutada:
sm64coopdx-MI-arm64-e7d0ac815221b66a15812dfc19d89ec2291011e0.
Contiene sm64coopdx-MI-arm64.apk, build-info.json y SHA256SUMS.txt; retención 7 días.
La disponibilidad posterior depende de expiración o eliminación del artefacto.
No se transcribe un hash ni ID de artefacto no consultados/verificados localmente.

## Límite de la validación y decisión del usuario

El usuario interrumpió la descarga local y pidió continuar solo la documentación.
No se reanudó la descarga. No se comprobó localmente el APK nuevo ni se contrastó
su hash/certificado fuera del runner. Esto no invalida las verificaciones que el
workflow sí completó, pero son evidencias distintas.

Pendiente prueba física del APK de Actions: instalación nueva, selector de ROM,
arranque, catálogo, idiomas, escala, táctil/mando y actualización sobre instalación
anterior compatible. No afirmar que la experiencia física quedó validada por CI.

No se usó ROM ni secrets en el runner. Se conserva firma pública heredada y paquete
com.maniscat2.sm64coopdx; una identidad/firma propia sigue siendo una decisión futura.
No hubo release, tag ni publicación de APK como release. La corrección sí se subió
a origin/mi-main para poder ejecutar el run.

## Advertencias restantes

El run conserva avisos heredados de Makefile sobre recetas duplicadas de
build/us_pc/ (overriding recipe / ignoring old recipe). No impidieron el build.
No se cambiaron esas reglas incidentalmente para resolver la selección de Gradle.

## Prevención y seguimiento

Al cambiar estructura Gradle revisar settings, nombre del módulo, -p, SDK local,
rutas de recursos y rutas de bibliotecas de AGP como un mismo contrato. El preflight
permite detectar un grafo roto antes de la compilación costosa. actionlint comprueba
el workflow, pero no sustituye configurar Gradle ni ejecutar el build real.

Los siguientes commits exclusivamente documentales no necesitan otro build: este
registro identifica el SHA de código realmente probado. Cualquier cambio posterior
al build/script/NDK requiere su propia validación proporcional y evidencia.

## Verificación de sincronización de estado

Gradle produce recursos y bibliotecas desde el checkout; el verificador compara
el APK con las bibliotecas stripped del mismo job y upload-artifact sube dist.
Git conserva la corrección y Actions el resultado del run. Este documento es
registro histórico, no una caché del último build. Sin cambios de estado runtime,
workers, Futures o persistencia del usuario. La verificación física sigue pendiente.
