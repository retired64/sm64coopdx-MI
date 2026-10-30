#ifndef MI_THUMBNAIL_TEXTURE_H
#define MI_THUMBNAIL_TEXTURE_H
#include <stdbool.h>
#include <stdint.h>
/* Main thread only. Sixteen stable pixel slots bound CPU/GPU texture identity.
 * Release frees a lease, never memory referenced by an interpolated display list. */
int mi_thumbnail_texture_acquire(void);
const uint8_t* mi_thumbnail_texture_pixels(int slot);
bool mi_thumbnail_texture_publish(int slot,const uint8_t* pixels);
void mi_thumbnail_texture_release(int slot);
#endif
