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

/* Coordinates inside a card; text and image never overlap, independent of scale. */
struct MiCardLayout { float imageX,imageY,imageSize,textX,textWidth,textY; };
static inline struct MiCardLayout mi_card_layout(float width,float height) {
    float edge=fmaxf(0,fminf(160,fminf(height-32,width*0.30f)));
    float textX=16+edge+16;
    return (struct MiCardLayout){16,fmaxf(8,(height-edge)/2),edge,textX,fmaxf(0,width-textX-16),fmaxf(8,(height-208)/2)};
}
