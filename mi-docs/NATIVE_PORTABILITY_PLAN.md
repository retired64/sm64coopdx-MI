# Plan de portabilidad: explorador MI independiente en C++

Fecha: 2026-10-30. Estado: propuesta futura solicitada por el propietario;
no implementada. Este documento no cambia el alcance Android vigente ni inicia
un port. La prioridad actual es validar físicamente la actualización individual
dentro del juego, según [CATALOG_REPLACEMENT.md](CATALOG_REPLACEMENT.md).

## Objetivo

Extraer el motor de catálogo, descarga, instalación y actualización para usarlo
en una aplicación nativa independiente: primero Linux y después Windows.
Conservar la claridad visual de MI: tarjetas legibles, colores, detalles
organizados, imágenes y progreso real, con controles sencillos.

El resultado deseado es un motor compartido y dos interfaces: DJUI dentro del
juego y una aplicación de escritorio. El juego conserva descubrimiento local,
refresco, activación y ejecución. La aplicación externa selecciona el destino,
presenta el catálogo e instala o actualiza archivos; no duplica la activación.

## Viabilidad y piezas existentes

Gran parte del motor ya está escrita en C++; no se trata de convertir todo desde
otro lenguaje. La separación principal es entre lógica reutilizable, integración
con el juego y operaciones específicas de plataforma.

| Componente actual | Reutilización prevista | Trabajo necesario |
|---|---|---|
| mi_catalog.cpp/.h | Parseo, validación, búsqueda y DTO | Separar cualquier dependencia del entorno del juego |
| mi_catalog_request.cpp/.h | Red, caché y snapshots | Inyectar rutas, configuración y confianza TLS de plataforma |
| mi_updates.cpp/.h | Comparación con recibos y candidatos | Mantener IDs y contratos persistidos |
| mi_install.cpp/.h | Instalación, reemplazo y recuperación | Aislar bloqueo, movimientos y garantías del sistema de archivos |
| mi_network.h y mi_sha256.h | Transporte compartido y huellas | Auditar dependencias y empaquetado |
| mi_catalog_panel.c | Referencia de diseño y flujo | Crear otra interfaz; los widgets DJUI dependen del juego |
| tools/mi/ y tests/mi/ | Pruebas de contratos y recuperación | Ejecutar variantes reales para cada plataforma |

Estas rutas pertenecen a src/pc/mod_integrations salvo las pruebas. El inventario
es punto de partida, no certifica que los módulos compilen ya de forma autónoma.
El contrato externo continúa siendo únicamente el JSON público y sus URLs.

## Arquitectura propuesta

1. Biblioteca C++ del motor, sin widgets ni acceso implícito al estado del juego.
2. Capa de plataforma para rutas, almacenamiento, bloqueo, movimientos seguros,
   certificados y directorios de caché/configuración.
3. Adaptador del juego que conserva la interfaz C para DJUI y traduce su contexto.
4. Adaptador de escritorio con ventana, configuración de destino y presentación.

Definir una API pequeña con ownership, snapshots, cancelación y estados terminales
documentados. Los trabajadores no llaman widgets. El hilo de interfaz consume
estado; cerrar una ventana no debe causar callbacks a objetos destruidos ni
interrumpir una fase de reemplazo de forma insegura. Conservar las garantías de
exclusión, publicación terminal, revisión e invalidación de la implementación.

Evitar dos copias del instalador. Elegir después si la biblioteca vive en este
repositorio o en uno independiente, con versiones fijadas y pruebas compartidas;
no copiar archivos manualmente como estrategia de mantenimiento.

## Interfaz independiente

Qt Widgets es la opción inicial a evaluar para una aplicación C++ con teclado,
ratón, escalado y controles accesibles. Es una recomendación de diseño pendiente
de prototipo, revisión de licencias, dependencias y distribución, no una decisión
de framework ya adoptada. SDL es otra alternativa si se prioriza apariencia de
juego, con mayor trabajo propio de controles y accesibilidad.

Reproducir la jerarquía visual aprobada, no trasladar coordenadas rígidas del
teléfono. Adaptar columnas al ancho y escala, conservar navegación por teclado,
foco visible, lectura de textos largos y mensajes que no dependan solo del color.
La aplicación puede elegir carpeta de mods sin exponer rutas técnicas en cada
tarjeta. No añadir listas o etiquetas que saturen la vista por defecto.

## Riesgos de portabilidad y datos

- El reemplazo actual usa mecanismos Linux/Android, incluyendo fcntl y
  renameat2 con RENAME_NOREPLACE. Windows requiere equivalentes comprobados;
  cambiar nombres de funciones o recompilar no demuestra las mismas garantías.
- Revisar archivos abiertos por el juego, nombres reservados, Unicode, longitud
  de rutas, mayúsculas/minúsculas, enlaces y puntos de reanálisis en Windows.
- Respetar permisos, destinos ajenos y modificaciones locales. Conservar evidencia
  cuando restaurar o limpiar no sea seguro; nunca forzar sobrescritura para acabar.
- Si aplicación y juego apuntan a los mismos mods, ambos deben respetar el mismo
  bloqueo y formato de recibos/diarios. Definir ubicación y migración antes de
  compartirlos; no dar por hecho que basta copiar la caché.
- Evitar reemplazar mods mientras el juego los está usando. Definir el mecanismo
  para coordinar o rechazar esa situación antes de ofrecer el flujo externo.
- Mantener esquema versionado, recuperación tras cierre y pruebas en el sistema
  de archivos real. No prometer resistencia a pérdida de energía sin comprobarla.
- Revisar licencias y atribuciones del código y dependencias antes de redistribuir.

La recuperación actual se inicia al entrar al catálogo del juego. El ejecutable
independiente necesitará su propio punto de recuperación previo a permitir
mutaciones; no se considera ya resuelto por existir ese hook en DJUI.

## Secuencia recomendada

- [ ] Completar la validación física Android de instalación, actualización,
  cancelación, interrupción, protección de cambios locales y refresco del juego.
- [ ] Auditar dependencias y diseñar el contrato mínimo del motor y plataforma.
- [ ] Extraer la biblioteca y un ejecutable de prueba sin render, preservando los
  resultados de las suites existentes y sin romper el APK.
- [ ] Reconectar DJUI a esa biblioteca y verificar que mantiene su comportamiento.
- [ ] Prototipar la aplicación Linux: carpeta de destino, catálogo, búsqueda,
  detalles, Lua/ZIP, progreso, cancelación, actualización y recuperación.
- [ ] Validar Linux con pruebas de fallos, concurrencia y uso real junto al juego.
- [ ] Implementar la capa Windows y repetir la matriz en Windows real.
- [ ] Preparar builds reproducibles, empaquetado, licencias y distribución por
  plataforma, con publicación autorizada por separado.

No incluir incidentalmente migración a base de datos, RAR/7z, Actualizar todos,
asociación manual ni OTA. Son capacidades independientes con su propio alcance.

## Criterios de aceptación

El mismo contrato JSON produce resultados coherentes en juego y escritorio.
Instalar no sobrescribe contenido ajeno. Actualizar conserva el destino,
retira archivos obsoletos y mantiene recibos coherentes. Fallos y cancelación
dejan una salida accionable; un cierre permite recuperar sin perder evidencia.

Probar por plataforma: Lua/ZIP, hashes y tamaños incorrectos, 404 y red caída,
espacio insuficiente, permisos, cambios locales, dos procesos, destino ocupado,
cierre durante reemplazo y recuperación. Las pruebas omitidas por limitaciones
del entorno se registran, no se cuentan como aprobadas.

La UI debe demostrar lectura y acciones accesibles con distintas escalas,
ventanas pequeñas, teclado y ratón. Compilar no acredita instalación funcional
ni equivalencia de garantías entre sistemas. Registrar evidencia y pendientes
antes de considerar cada port estable.

## Documentación relacionada

- [Reglas del proyecto](RULES.md).
- [Mantenimiento de upstream Android](MANTENER-UPDATES.md).
- [Instalación actual](CATALOG_INSTALLATION.md).
- [Reemplazo y recuperación actuales](CATALOG_REPLACEMENT.md).
- [Accesibilidad del catálogo](CATALOG_ACCESSIBILITY.md).

El port futuro necesita su propio flujo de mantenimiento; no sustituye el
seguimiento de Android estable ni autoriza incorporar ramas experimentales.
