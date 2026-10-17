#include <srvros/desk_render.h>
#include <limits.h>

static int64_t min64(int64_t a, int64_t b) { return a < b ? a : b; }
static int64_t max64(int64_t a, int64_t b) { return a > b ? a : b; }

struct desk_rect desk_intersect(struct desk_rect a, struct desk_rect b) {
    int64_t x = max64(a.x, b.x), y = max64(a.y, b.y);
    int64_t right = min64((int64_t)a.x + a.w, (int64_t)b.x + b.w);
    int64_t bottom = min64((int64_t)a.y + a.h, (int64_t)b.y + b.h);
    if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0 || right <= x || bottom <= y)
        return (struct desk_rect){0};
    return (struct desk_rect){(int32_t)x, (int32_t)y, (int32_t)(right-x), (int32_t)(bottom-y)};
}

int desk_canvas_init(struct desk_canvas *c, uint32_t *pixels,
    int32_t width, int32_t height, int32_t stride) {
    if (!c || !pixels || width <= 0 || height <= 0 || stride < width ||
        width > 8192 || height > 8192 || stride > 8192) return -1;
    *c = (struct desk_canvas){pixels, width, height, stride, {0,0,width,height}, 0};
    return 0;
}

void desk_clip(struct desk_canvas *c, struct desk_rect clip) {
    c->clip = desk_intersect(clip, (struct desk_rect){0,0,c->width,c->height});
}

void desk_fill(struct desk_canvas *c, struct desk_rect r, uint32_t color) {
    r = desk_intersect(r, c->clip);
    for (int32_t y = r.y; y < r.y+r.h; y++) {
        uint32_t *row = c->pixels + (size_t)y*c->stride + r.x;
        for (int32_t x = 0; x < r.w; x++) row[x] = color & 0xffffff;
    }
    c->pixel_writes += (uint64_t)r.w*r.h;
}

uint32_t desk_utf8_next(const char **text) {
    const unsigned char *p = (const unsigned char *)*text;
    if (!*p) return 0;
    uint32_t cp = *p++;
    unsigned count;
    uint32_t minimum;
    if (cp < 128) { *text = (const char *)p; return cp; }
    if (cp >= 0xc2 && cp <= 0xdf) { count=1; minimum=0x80; cp &= 31; }
    else if (cp >= 0xe0 && cp <= 0xef) { count=2; minimum=0x800; cp &= 15; }
    else if (cp >= 0xf0 && cp <= 0xf4) { count=3; minimum=0x10000; cp &= 7; }
    else { *text = (const char *)p; return 0xfffd; }
    for (unsigned i=0; i<count; i++) {
        if ((p[i] & 0xc0) != 0x80) { *text = (const char *)p; return 0xfffd; }
        cp = (cp << 6) | (p[i] & 63);
    }
    *text = (const char *)p + count;
    if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return 0xfffd;
    return cp;
}

static const struct desk_glyph *glyph(const struct desk_font *f, uint32_t cp) {
    unsigned lo=0, hi=f->count;
    while (lo < hi) {
        unsigned mid = lo+(hi-lo)/2;
        if (f->glyphs[mid].codepoint < cp) lo=mid+1; else hi=mid;
    }
    if (lo < f->count && f->glyphs[lo].codepoint == cp) return &f->glyphs[lo];
    return &f->glyphs[f->count-1]; /* Generated atlas ends in U+FFFD. */
}

int32_t desk_text_width(const struct desk_font *font, const char *text) {
    int64_t line=0, width=0;
    uint32_t cp;
    while ((cp=desk_utf8_next(&text))) {
        if (cp=='\n') { width=max64(width,line); line=0; }
        else line=min64(INT32_MAX,line+glyph(font,cp)->advance);
    }
    return (int32_t)max64(width,line);
}

static uint32_t blend(uint32_t fg, uint32_t bg, unsigned alpha) {
    uint32_t rgb=0;
    for (unsigned shift=0; shift<24; shift+=8) {
        unsigned a=(fg>>shift)&255, b=(bg>>shift)&255;
        rgb |= ((a*alpha+b*(255-alpha)+127)/255)<<shift;
    }
    return rgb;
}

void desk_text(struct desk_canvas *c, const struct desk_font *font,
    int32_t x, int32_t y, const char *text, uint32_t color) {
    int64_t pen=x, baseline=(int64_t)y+font->ascent;
    uint32_t cp;
    while ((cp=desk_utf8_next(&text))) {
        if (cp=='\n') { pen=x; baseline+=font->line_height; continue; }
        const struct desk_glyph *g=glyph(font,cp);
        int64_t left=pen+g->left, top=baseline+g->top;
        int64_t x0=max64(left,c->clip.x), y0=max64(top,c->clip.y);
        int64_t x1=min64(left+g->width,(int64_t)c->clip.x+c->clip.w);
        int64_t y1=min64(top+g->height,(int64_t)c->clip.y+c->clip.h);
        for (int64_t py=y0; py<y1; py++) {
            for (int64_t px=x0; px<x1; px++) {
                unsigned a=font->alpha[g->offset+(py-top)*g->width+px-left];
                if (!a) continue;
                uint32_t *pixel=c->pixels+(size_t)py*c->stride+px;
                *pixel=blend(color,*pixel,a);
                c->pixel_writes++;
            }
        }
        pen+=g->advance;
        /* Bounded coordinates even for extremely long hostile strings. */
        if (pen > INT32_MAX || baseline > INT32_MAX) return;
    }
}

void desk_damage_init(struct desk_damage *d, int32_t width, int32_t height) {
    *d=(struct desk_damage){.width=width,.height=height};
}

void desk_damage_add(struct desk_damage *d, struct desk_rect r) {
    r=desk_intersect(r,(struct desk_rect){0,0,d->width,d->height});
    if (!r.w || !r.h) return;
    /* Merge overlap/touch, restarting because the union can reach earlier rects. */
    for (unsigned i=0;i<d->count;) {
        struct desk_rect a=d->rects[i];
        if (r.x<=a.x+a.w && a.x<=r.x+r.w && r.y<=a.y+a.h && a.y<=r.y+r.h) {
            int32_t x=(int32_t)min64(r.x,a.x), y=(int32_t)min64(r.y,a.y);
            r=(struct desk_rect){x,y,(int32_t)max64(r.x+r.w,a.x+a.w)-x,
                (int32_t)max64(r.y+r.h,a.y+a.h)-y};
            d->rects[i]=d->rects[--d->count];
            i=0;
        } else i++;
    }
    if (d->count==DESK_DAMAGE_MAX) {
        d->count=1;
        d->rects[0]=(struct desk_rect){0,0,d->width,d->height};
    } else d->rects[d->count++]=r;
}
