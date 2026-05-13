#ifndef SRVROS_USER_BMP_H
#define SRVROS_USER_BMP_H

#include <stddef.h>
#include <stdint.h>

int bmp_decode_rgba(const uint8_t *bmp, size_t bmp_size, uint32_t *pixels,
    uint64_t pixel_capacity, uint64_t *width_out, uint64_t *height_out);
int bmp_encode_rgba32(const uint32_t *pixels, uint64_t width, uint64_t height,
    uint8_t *bmp, size_t bmp_capacity, size_t *size_out);

#endif
