#include <string.h>
#include "mi_thumbnail_texture.h"
#include "mi_thumbnail.h"
#include "pc/gfx/gfx_pc.h"
static uint8_t sPixels[16][MI_THUMB_BYTES] __attribute__((aligned(8)));
static bool sLeased[16];
int mi_thumbnail_texture_acquire(void) {
    for(int i=0;i<16;i++)if(!sLeased[i]){sLeased[i]=true;return i;}
    return -1;
}
const uint8_t* mi_thumbnail_texture_pixels(int slot) {
    return slot>=0&&slot<16&&sLeased[slot]?sPixels[slot]:NULL;
}
bool mi_thumbnail_texture_publish(int slot,const uint8_t* pixels) {
    if(slot<0||slot>=16||!sLeased[slot]||!pixels||!gfx_texture_mark_dirty(sPixels[slot]))return false;
    memcpy(sPixels[slot],pixels,MI_THUMB_BYTES);return true;
}
void mi_thumbnail_texture_release(int slot) {if(slot>=0&&slot<16)sLeased[slot]=false;}
