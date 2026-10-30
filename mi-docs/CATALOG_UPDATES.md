# Consulta de actualizaciones

2026-10-30: implementación local de solo lectura. Reemplazo individual y Actualizar
todos siguen pendientes. Plan: [CATALOG_UPDATES_PLAN.md](CATALOG_UPDATES_PLAN.md).
El fork conoce únicamente el JSON público, sus URLs y sus recibos propios.

## Flujo de usuario

Buscar actualizaciones comparte la cabecera existente con búsqueda y refresco.
Fuerza consulta del catálogo mediante el servicio actual y, al terminar, compara
recibos en segundo plano. Una copia válida anterior puede usarse con indicación
explícita de copia guardada si falla la red.

Si hay candidatos, las tarjetas/paginación existentes muestran solo esos mods.
Ver todos los mods desactiva ese filtro; otra búsqueda realiza una consulta nueva.
Se conserva el texto de búsqueda y se aplica también a los resultados. No se
crea una lista de instalaciones ni insignias permanentes en todas las tarjetas.

El resumen muestra nuevas / sin comprobar / borradas. Nunca dice «todo actualizado»
cuando falta información. Con cero candidatos conserva el catálogo normal.
Errores de lectura y una instalación/recuperación pendiente permiten reintentar.

Abrir un resultado selecciona el archivo candidato y muestra versión instalada
-> disponible. Nueva versión encontrada permanece deshabilitado y explica que el
reemplazo aún no está habilitado. No inicia una segunda instalación ni quita la
protección de destinos existentes. El juego conserva Refrescar y activación.

## Decisión conservadora

- IDs opacos mod/versión/archivo identifican el recibo; no son números ordenables.
- Metadata pública opcional: release_date, release_timestamp, provider_release_date
  y role. También se conservan publicaciones sin archivos resueltos.
- Se exige una etiqueta overview fiable que coincida con el historial, normalizando
  espacios, mayúsculas y prefijo v numérico. Si falta, el MVP deja sin comprobar;
  no infiere la versión vigente solo por fecha máxima.
- Entre etiquetas repetidas se exige un máximo único por fecha. Un día representa
  todo ese día; hora y zona se convierten a UTC. Fechas inválidas, empates y rangos
  superpuestos son ambiguos. Fracciones conservan precisión de segundo.
- La publicación debe estar resuelta y ser inequívocamente posterior. Un overview
  más antiguo que lo instalado no produce downgrade. No se ordenan arrays ni
  cadenas arbitrarias para elegir la versión.
- El archivo debe ser único, soportado y del mismo rol; primary también debe ser
  recommended. No se cambia Lite por completa ni se sustituye una publicación
  vigente no resuelta por otra vieja que sí descargue.
- No detecta cambios binarios bajo los mismos IDs sin evidencia remota comparable.

## Recibos, presencia y límites

Lectura de fs_writepath/mi/receipts, sin crear carpetas, escribir mods, migrar recibos
ni eliminar evidencia. Schema 1, identidad/nombre de recibo, tipo y rutas seguros.
Límites: 4096 recibos; 4 MiB por recibo; 32 MiB de lectura agregada; 4096 rutas de
manifiesto por recibo; 24 niveles. Catálogo: 20000 versiones y 20000 archivos.

Si desapareció el destino, se cuenta como borrado y no se ofrece actualización.
Si faltan archivos internos, hay enlaces o asociaciones duplicadas para destino/mod,
queda sin comprobar. Dos instalaciones del mismo mod son ambiguas para esta UI
por mod, aunque estén en carpetas diferentes. Un recibo dañado no certifica la
situación de sus archivos y cuenta como no comprobado.

Se comprueba presencia/tipo/rutas, no hashes del contenido local. Ofrecer una
publicación nueva no certifica integridad: un futuro reemplazo debe verificar otra
vez el manifiesto y tratar modificaciones locales. Recibos de destinos borrados
se conservan; ajustar la reinstalación explícita de esas asociaciones sigue pendiente.
Una transacción pendiente bloquea la consulta hasta que el instalador la recupere.

## Ownership y vigencia

mi_updates_start copia raíz y snapshot profundo del catálogo. Worker detached solo
lee y publica resultados bajo mutex; poll/get copian datos. Al terminar libera el
catálogo grande y conserva resultados. close cancela/libera UI sin bloquear; la
referencia del worker impide acceso a objetos liberados. Ningún worker llama DJUI.

La revisión del instalador cambia al comenzar/terminar cada operación aceptada.
Se comprueba ocupado/revisión antes y después de leer; una instalación concurrente
descarta la lectura. No se toma un lock POSIX de lectura que pueda rebajar el lock
de escritura de otro thread del proceso. El panel invalida resultados al refrescar,
recibir otro snapshot, cambiar la revisión o cerrarse. No hay caché persistente de
actualizaciones. Detalles copia el candidato, sin punteros prestados al padre.
Los cambios externos posteriores requieren comprobar otra vez; el reemplazo futuro
revalidará propiedad y archivos al actuar.

## Inventario y pruebas

- mi_updates.h/.cpp: comparación pura y worker de consulta.
- mi_catalog.h/.cpp: fechas/roles opcionales y clonación independiente.
- mi_install.h/.cpp: revisión/ocupado para invalidación; no amplía el reemplazo.
- mi_catalog_panel.c: control, filtro, resumen y detalle informativo.
- EN/ES/PT y strings compiladas: etiquetas y estados.
- tests/mi/updates_test.cpp y tools/mi/test_updates.py: fixtures sintéticas.

```sh
python tools/mi/test_updates.py
python tools/mi/test_catalog.py
python tools/mi/test_requests.py --bundled-curl --live
python tools/mi/test_install.py --bundled-curl
```

Cobertura: orden invertido, fechas/UTC/precisión, etiquetas repetidas/ausentes,
variantes, vigente no resuelta, formato no soportado, borrados, conflictos, JSON
roto, diario pendiente, ocupado, cancelación y destrucción con worker activo.
Solo temporales. Prueba física pendiente: escala/idiomas, filtro/volver, red/copia
guardada, cierre y nueva instalación posterior. APK y evidencia en el checklist.
