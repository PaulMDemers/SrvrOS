#ifndef SRVROS_DESK_RENDER_H
#define SRVROS_DESK_RENDER_H

#include <stddef.h>
#include <stdint.h>

/* Rectangles are half-open. Canvas pixels are packed 0x00RRGGBB in RAM. */
struct desk_rect { int32_t x, y, w, h; };
struct desk_canvas {
    uint32_t *pixels;
    int32_t width, height, stride;
    struct desk_rect clip;
    uint64_t pixel_writes;
};
struct desk_glyph {
    uint32_t codepoint, offset;
    uint16_t width, height;
    int16_t left, top, advance;
};
struct desk_font {
    const struct desk_glyph *glyphs;
    const uint8_t *alpha;
    uint16_t count, ascent, line_height;
};
extern const struct desk_font desk_font_16, desk_font_20, desk_font_mono_16, desk_font_digits_32;
extern const char desk_font_license[];

#define DESK_DAMAGE_MAX 16
struct desk_damage {
    struct desk_rect rects[DESK_DAMAGE_MAX];
    unsigned count;
    int32_t width, height;
};

int desk_canvas_init(struct desk_canvas *c, uint32_t *pixels,
    int32_t width, int32_t height, int32_t stride);
struct desk_rect desk_intersect(struct desk_rect a, struct desk_rect b);
void desk_clip(struct desk_canvas *c, struct desk_rect clip);
void desk_fill(struct desk_canvas *c, struct desk_rect r, uint32_t color);
/* NUL-terminated UTF-8. Unsupported/malformed code points use U+FFFD.
 * Coordinates use a top-of-line origin. Newlines advance by line_height.
 * No shaping or kerning in this first atlas implementation. */
int32_t desk_text_width(const struct desk_font *font, const char *text);
void desk_text(struct desk_canvas *c, const struct desk_font *font,
    int32_t x, int32_t y, const char *text, uint32_t color);
uint32_t desk_utf8_next(const char **text);
void desk_damage_init(struct desk_damage *damage, int32_t width, int32_t height);
void desk_damage_add(struct desk_damage *damage, struct desk_rect r);

#endif
