#ifndef DESK_IMAGE_DECODE_H
#define DESK_IMAGE_DECODE_H
#include <srvros/desktop_image.h>
/* Returns a desk_image_status; success owns packed 0xAARRGGBB pixels. */
unsigned image_decode(const void *data,size_t length,uint32_t **pixels,uint32_t *width,uint32_t *height);
const char *image_decode_reason(void);
void image_decode_free(uint32_t *pixels);
#endif
