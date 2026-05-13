#include <srvros/bmp.h>

static uint16_t read16(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read32(const uint8_t *data) {
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static int32_t read32s(const uint8_t *data) {
    return (int32_t)read32(data);
}

static void write16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void write32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

int bmp_decode_rgba(const uint8_t *bmp, size_t bmp_size, uint32_t *pixels,
    uint64_t pixel_capacity, uint64_t *width_out, uint64_t *height_out) {
    if (bmp == 0 || pixels == 0 || bmp_size < 54 ||
        bmp[0] != 'B' || bmp[1] != 'M') {
        return -1;
    }

    uint32_t pixel_offset = read32(bmp + 10);
    uint32_t dib_size = read32(bmp + 14);
    if (dib_size < 40 || pixel_offset >= bmp_size) {
        return -1;
    }

    int32_t signed_width = read32s(bmp + 18);
    int32_t signed_height = read32s(bmp + 22);
    uint16_t planes = read16(bmp + 26);
    uint16_t bpp = read16(bmp + 28);
    uint32_t compression = read32(bmp + 30);
    if (signed_width <= 0 || signed_height == 0 || planes != 1 ||
        (bpp != 24 && bpp != 32) || compression != 0) {
        return -1;
    }

    uint64_t width = (uint64_t)signed_width;
    uint64_t height = signed_height < 0 ? (uint64_t)-signed_height : (uint64_t)signed_height;
    if (width == 0 || height == 0 || width > UINT64_MAX / height ||
        width * height > pixel_capacity) {
        return -1;
    }

    uint64_t bytes_per_pixel = bpp / 8;
    uint64_t row_bytes = ((width * bytes_per_pixel) + 3) & ~(uint64_t)3;
    if (row_bytes > (uint64_t)bmp_size ||
        row_bytes * height > (uint64_t)bmp_size - pixel_offset) {
        return -1;
    }

    for (uint64_t y = 0; y < height; y++) {
        uint64_t source_y = signed_height > 0 ? (height - 1 - y) : y;
        const uint8_t *row = bmp + pixel_offset + source_y * row_bytes;
        for (uint64_t x = 0; x < width; x++) {
            const uint8_t *px = row + x * bytes_per_pixel;
            pixels[y * width + x] = ((uint32_t)px[2] << 16) |
                ((uint32_t)px[1] << 8) |
                (uint32_t)px[0];
        }
    }

    if (width_out != 0) {
        *width_out = width;
    }
    if (height_out != 0) {
        *height_out = height;
    }
    return 0;
}

int bmp_encode_rgba32(const uint32_t *pixels, uint64_t width, uint64_t height,
    uint8_t *bmp, size_t bmp_capacity, size_t *size_out) {
    if (pixels == 0 || bmp == 0 || width == 0 || height == 0 ||
        width > UINT32_MAX || height > UINT32_MAX) {
        return -1;
    }

    uint64_t row_bytes = width * 4;
    uint64_t image_size = row_bytes * height;
    uint64_t file_size = 54 + image_size;
    if (file_size > bmp_capacity || file_size > UINT32_MAX) {
        return -1;
    }

    for (uint64_t i = 0; i < file_size; i++) {
        bmp[i] = 0;
    }

    bmp[0] = 'B';
    bmp[1] = 'M';
    write32(bmp + 2, (uint32_t)file_size);
    write32(bmp + 10, 54);
    write32(bmp + 14, 40);
    write32(bmp + 18, (uint32_t)width);
    write32(bmp + 22, (uint32_t)height);
    write16(bmp + 26, 1);
    write16(bmp + 28, 32);
    write32(bmp + 34, (uint32_t)image_size);

    for (uint64_t y = 0; y < height; y++) {
        uint8_t *row = bmp + 54 + (height - 1 - y) * row_bytes;
        for (uint64_t x = 0; x < width; x++) {
            uint32_t color = pixels[y * width + x];
            row[x * 4 + 0] = (uint8_t)color;
            row[x * 4 + 1] = (uint8_t)(color >> 8);
            row[x * 4 + 2] = (uint8_t)(color >> 16);
            row[x * 4 + 3] = 0;
        }
    }

    if (size_out != 0) {
        *size_out = (size_t)file_size;
    }
    return 0;
}
