#ifndef MI_THUMBNAIL_H
#define MI_THUMBNAIL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MI_THUMB_EDGE 128
#define MI_THUMB_BYTES (MI_THUMB_EDGE * MI_THUMB_EDGE * 4)
#ifdef __cplusplus
extern "C" {
#endif
struct MiThumbnail;
enum MiThumbnailState { MI_THUMB_LOADING, MI_THUMB_READY, MI_THUMB_FAILED, MI_THUMB_CANCELLED };
/* Two bounded background workers; copies root/URL. HTTPS in production.
 * Decorative failure never changes catalog/install state. No DJUI access. */
struct MiThumbnail* mi_thumbnail_start(const char* userRoot,const char* url);
enum MiThumbnailState mi_thumbnail_state(struct MiThumbnail* request);
/* Copies RGBA128 once. Caller owns pixels; never points into worker memory. */
bool mi_thumbnail_take(struct MiThumbnail* request,uint8_t* pixels,size_t capacity);
void mi_thumbnail_close(struct MiThumbnail* request);
#ifdef __cplusplus
}
#endif
#endif
