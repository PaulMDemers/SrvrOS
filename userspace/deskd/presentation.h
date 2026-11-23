#ifndef DESKD_PRESENTATION_H
#define DESKD_PRESENTATION_H
#include <stddef.h>
#include <stdint.h>
#include <srvros/desktop_scale.h>
#include <srvros/desk_render.h>

enum { DESKTOP_OUTPUT_WIDTH_MAX=3840, DESKTOP_OUTPUT_BAND_ROWS=16 };

static inline struct desk_rect desktop_output_rect(struct desk_rect r,unsigned scale,
    unsigned physical_width,unsigned physical_height) {
    unsigned quarters=desk_scale_quarters(scale);
    unsigned x=(unsigned)r.x*quarters/4,y=(unsigned)r.y*quarters/4;
    unsigned right=((unsigned)(r.x+r.w)*quarters+3)/4;
    unsigned bottom=((unsigned)(r.y+r.h)*quarters+3)/4;
    if(x>=physical_width || y>=physical_height)return (struct desk_rect){0};
    if(right>physical_width)right=physical_width;
    if(bottom>physical_height)bottom=physical_height;
    return (struct desk_rect){x,y,right-x,bottom-y};
}

/* Box-filter from the 2x backing raster to the output. In this coordinate
 * system an output pixel spans eight units and each source pixel spans q.
 * Damage includes both intersecting edge pixels so incremental and full
 * redraws produce the same output. The rounded-up canvas covers the last
 * physical sample even when the display size is not an exact multiple. */
static inline void desktop_fractional_band(const struct desk_canvas *c,struct desk_rect out,
    unsigned scale,uint32_t *pixels) {
    unsigned q=desk_scale_quarters(scale);
    for(unsigned y=out.y;y<(unsigned)(out.y+out.h);y++) {
        unsigned top=y*8,bottom=top+8,sy=top/q;
        for(unsigned x=out.x;x<(unsigned)(out.x+out.w);x++) {
            unsigned left=x*8,right=left+8,sx=left/q;
            unsigned red=0,green=0,blue=0;
            for(unsigned y1=sy;y1*q<bottom;y1++) {
                unsigned y0=y1*q>top?y1*q:top;
                unsigned y2=(y1+1)*q<bottom?(y1+1)*q:bottom;
                const uint32_t *row=c->pixels+(size_t)y1*c->stride;
                for(unsigned x1=sx;x1*q<right;x1++) {
                    unsigned x0=x1*q>left?x1*q:left;
                    unsigned x2=(x1+1)*q<right?(x1+1)*q:right;
                    unsigned weight=(x2-x0)*(y2-y0),p=row[x1];
                    red+=((p>>16)&255)*weight;green+=((p>>8)&255)*weight;blue+=(p&255)*weight;
                }
            }
            *pixels++=(red/64<<16)|(green/64<<8)|(blue/64);
        }
    }
}

/* The caller freezes scene and stacking order until this returns. Compose all
 * clips before uploading any: the framebuffer is still updated in pieces and
 * may be scanned while writes occur. Upload failures do not skip later clips. */
struct desktop_present_ops {
    void (*compose)(void *context,struct desk_rect rect);
    int (*upload)(void *context,struct desk_rect rect);
};
static inline int desktop_present(const struct desk_damage *damage,unsigned scale,
    uint64_t physical_width,uint64_t physical_height,const struct desktop_present_ops *ops,
    void *context,uint64_t *bytes) {
    int failed=0;
    for(unsigned i=0;i<damage->count;i++)ops->compose(context,damage->rects[i]);
    for(unsigned i=0;i<damage->count;i++) {
        struct desk_rect r=damage->rects[i];
        if(ops->upload(context,r))failed=1;
        struct desk_rect p=desktop_output_rect(r,scale,physical_width,physical_height);
        *bytes+=(uint64_t)p.w*p.h*4;
    }
    return failed;
}
#endif
