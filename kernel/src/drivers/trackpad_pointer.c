#include <srvros/trackpad_pointer.h>

bool trackpad_pointer_cancel(struct trackpad_pointer *s,struct trackpad_motion *out) {
    bool release=s->buttons!=0;
    *s=(struct trackpad_pointer){.blocked=true};
    *out=(struct trackpad_motion){0};
    return release;
}
bool trackpad_pointer_frame(struct trackpad_pointer *s,
    const struct apple_trackpad_frame *f,struct trackpad_motion *out) {
    *out=(struct trackpad_motion){0};
    if (!f->button) s->blocked=false;
    out->buttons=f->button && !s->blocked;
    bool changed=out->buttons!=s->buttons;
    s->buttons=out->buttons;
    if (f->count!=1) {
        s->baseline=false; s->remainder_x=s->remainder_y=0;
        return changed;
    }
    int32_t x=f->contacts[0].x,y=f->contacts[0].y;
    if (s->baseline) {
        int32_t dx=x-s->x,dy=s->y-y;
        /* Reject discontinuities; never turn a re-contact or corrupt jump into
         * a screen-wide motion. The current point becomes the new baseline. */
        if (dx>=-2048 && dx<=2048 && dy>=-2048 && dy<=2048) {
            dx+=s->remainder_x; dy+=s->remainder_y;
            out->dx=dx/16; out->dy=dy/16;
            s->remainder_x=dx%16; s->remainder_y=dy%16;
        } else s->remainder_x=s->remainder_y=0;
    }
    s->x=x; s->y=y; s->baseline=true;
    return changed || out->dx || out->dy;
}
