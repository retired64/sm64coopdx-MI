# Documentación de SM64coopdx-MI

La documentación propia del fork se concentra aquí. Las instrucciones operativas
para agentes permanecen en [AGENTS.md](../AGENTS.md), en la raíz. Los comandos de
build/Git de estas guías se ejecutan desde la raíz salvo indicación expresa.
La documentación heredada del juego permanece en sus rutas originales.

## Reglas y mantenimiento

- [Primera compilación CI correcta](ci/2026-10-30-first-success.md): causa del fallo,
  corrección, run exitoso y pruebas físicas pendientes.

- [MI_ACTIONS_BUILD.md](MI_ACTIONS_BUILD.md): workflow manual Android sin ROM,
  artefactos, firma de prueba y comandos gh para ejecución/descarga.

- [UPSTREAM_SETUP.md](UPSTREAM_SETUP.md): preparación efectiva y reproducible por clon.
- [Registro inicial de upstream](upstream/2026-10-29-setup.md): SHAs y auditoría.

- [RULES.md](RULES.md): contratos, errores silenciosos, inventario e historial de commits.
- [MANTENER-UPDATES.md](MANTENER-UPDATES.md): actualización desde Android estable,
  resolución de conflictos, validación, promoción y recuperación.
- [MI_INITIAL_PREPARATION_CHECKLIST.md](MI_INITIAL_PREPARATION_CHECKLIST.md):
  preparación, progreso, artefactos y pendientes.
- [SM64CDPY_IDEAS_Y_FORK.md](SM64CDPY_IDEAS_Y_FORK.md): decisiones, ideas,
  roadmap e historial; incluye la futura migración del catálogo.

## Integración vigente

- [CATALOG_REPLACEMENT.md](CATALOG_REPLACEMENT.md): actualización individual,
  respaldo, rollback y recuperación de diarios.

- [MI_MAINTENANCE_TESTS.md](MI_MAINTENANCE_TESTS.md): instalación/recibos/consulta
  conectados, procesos interrumpidos, fallos de publicación y límites pendientes.

- [CATALOG_UPDATES.md](CATALOG_UPDATES.md): consulta de solo lectura y resultados
  integrados en el catálogo, sin reemplazo todavía.
- [CATALOG_THUMBNAILS.md](CATALOG_THUMBNAILS.md): diseño de tarjetas, carga de
  imágenes, caché acotada y ciclo de vida de texturas.

- [CATALOG_INSTALLATION.md](CATALOG_INSTALLATION.md): instalación Lua/ZIP, progreso,
  recibos, recuperación, límites y pruebas locales.

- [CATALOG_REMOTE.md](CATALOG_REMOTE.md): red, TLS, caché y ownership.
- [CATALOG_ACCESSIBILITY.md](CATALOG_ACCESSIBILITY.md): tarjetas, accesibilidad
  y lectura actuales.
- [DJUI_VIEWPORT_AND_TRANSLATIONS.md](DJUI_VIEWPORT_AND_TRANSLATIONS.md):
  desplazamiento de menús y respaldo de traducciones.

## Planes pendientes

- [NATIVE_PORTABILITY_PLAN.md](NATIVE_PORTABILITY_PLAN.md): propuesta de motor
  C++ compartido y aplicación independiente para Linux y Windows, fases y pruebas.

- [CATALOG_UPDATES_PLAN.md](CATALOG_UPDATES_PLAN.md): detección interna y reemplazo
  seguro usando únicamente el contrato JSON existente, sin añadir lista
  de instalaciones.

## Etapas históricas

- [CATALOG_PREVIEW.md](CATALOG_PREVIEW.md): prototipo con muestra embebida.
- [CATALOG_WORKSPACE.md](CATALOG_WORKSPACE.md): primer diseño ancho anterior al grid.

## Convención para documentos nuevos

Crear nuevas guías, planes y registros MI dentro de mi-docs/ y añadir su enlace
a este índice. Los registros de futuras sincronizaciones se crearán en sync/.
Conservar evidencia fechada y distinguir planes de funcionalidades implementadas.
Los inventarios históricos pueden mencionar rutas anteriores: sus enlaces deben
llevar al documento vigente o al commit histórico correspondiente.

Reorganización: 2026-10-29. Se trasladan los cuatro documentos MI de la raíz y los
cinco documentos de docs/mi/ a esta carpeta. No cambia código ni configuración.
