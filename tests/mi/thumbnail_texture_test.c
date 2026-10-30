#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pc/mod_integrations/mi_thumbnail.h"
#include "pc/mod_integrations/mi_thumbnail_texture.h"
static unsigned marks;
bool gfx_texture_mark_dirty(const void* address){assert(address);marks++;return true;}
int main(void){
    int slots[16];const uint8_t* pointers[16];
    for(int i=0;i<16;i++){slots[i]=mi_thumbnail_texture_acquire();assert(slots[i]>=0);pointers[i]=mi_thumbnail_texture_pixels(slots[i]);assert(pointers[i]);for(int j=0;j<i;j++)assert(pointers[i]!=pointers[j]);}
    assert(mi_thumbnail_texture_acquire()==-1);
    uint8_t pixels[MI_THUMB_BYTES];memset(pixels,123,sizeof(pixels));
    assert(mi_thumbnail_texture_publish(slots[0],pixels));assert(marks==1);
    assert(!memcmp(pointers[0],pixels,sizeof(pixels)));
    mi_thumbnail_texture_release(slots[0]);assert(!mi_thumbnail_texture_pixels(slots[0]));
    assert(!mi_thumbnail_texture_publish(slots[0],pixels));
    int reused=mi_thumbnail_texture_acquire();assert(reused==slots[0]);assert(mi_thumbnail_texture_pixels(reused)==pointers[0]);
    memset(pixels,27,sizeof(pixels));assert(mi_thumbnail_texture_publish(reused,pixels));assert(marks==2);
    assert(pointers[0][0]==27&&pointers[0][MI_THUMB_BYTES-1]==27);
    for(int i=0;i<16;i++)mi_thumbnail_texture_release(slots[i]);
    assert(!mi_thumbnail_texture_publish(-1,pixels));
    puts("MI thumbnail textures: bounded leases, stable addresses and targeted refresh passed.");
}
