#pragma once
#include <math.h>
struct MiCatalogLayout { unsigned columns, rows, pageSize; float cardHeight; };
static inline struct MiCatalogLayout mi_catalog_layout(float width, float height) {
    float available = height - 32 - 156 - 72;
    unsigned columns = width * 0.96f - 32 >= 1000 ? 2 : 1;
    unsigned rows = available >= 452 ? 2 : 1;
    float card = fmaxf(220, fminf(300, (available - (rows - 1) * 12) / rows));
    return (struct MiCatalogLayout){ columns, rows, columns * rows, card };
}
