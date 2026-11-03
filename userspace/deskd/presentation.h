#ifndef DESKD_PRESENTATION_H
#define DESKD_PRESENTATION_H
#include <stdint.h>
#include <srvros/desk_render.h>

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
        uint64_t x=(uint64_t)r.x*scale,y=(uint64_t)r.y*scale;
        uint64_t w=(uint64_t)r.w*scale,h=(uint64_t)r.h*scale;
        if(w>physical_width-x)w=physical_width-x;
        if(h>physical_height-y)h=physical_height-y;
        *bytes+=w*h*4;
    }
    return failed;
}
#endif
