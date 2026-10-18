#include <srvros/trackpad_pointer.h>

bool trackpad_pointer_cancel(struct trackpad_pointer *s,struct trackpad_motion *out) {
    bool release=s->buttons!=0;
    *s=(struct trackpad_pointer){.blocked=true};
    *out=(struct trackpad_motion){0};
    return release;
}
static void rebase(struct trackpad_pointer *s) {
    s->baseline=false; s->mode=0; s->remainder_x=s->remainder_y=0;
}
static bool frame(struct trackpad_pointer *s,const struct apple_trackpad_frame *f,
    bool gestures,bool typing,struct trackpad_motion *out) {
    *out=(struct trackpad_motion){0};
    if (!f->button) s->blocked=false;
    if (gestures) {
        if (!f->count) s->palm_blocked=s->typing_blocked=false;
        bool broad=f->count>2;
        for (unsigned i=0;i<f->count && i<APPLE_TRACKPAD_CONTACTS;i++)
            broad |= f->contacts[i].touch_major>600 || f->contacts[i].touch_minor>600;
        if (broad) s->palm_blocked=true;
        if (typing && f->count && !f->button && !s->buttons) s->typing_blocked=true;
        /* Physical press explicitly overrides typing suppression, not a palm. */
        if (f->button) s->typing_blocked=false;
        out->palm_suppressed=s->palm_blocked;
        out->typing_suppressed=s->typing_blocked;
        if (s->palm_blocked || s->typing_blocked) {
            bool release=s->buttons!=0;
            s->buttons=0; rebase(s);
            if (s->palm_blocked && f->button) s->blocked=true;
            return release;
        }
    }
    out->buttons=f->button && !s->blocked;
    bool changed=out->buttons!=s->buttons;
    s->buttons=out->buttons;
    unsigned mode=f->count==1?1:(gestures && f->count==2 && !f->button?2:0);
    if (!mode) { rebase(s); return changed; }
    int32_t x=f->contacts[0].x,y=f->contacts[0].y;
    if (mode==2) { x+=f->contacts[1].x; y+=f->contacts[1].y; }
    if (s->mode!=mode) rebase(s);
    if (s->baseline) {
        int32_t dx=x-s->x,dy=s->y-y;
        int32_t limit=2048*(int)mode,scale=16*(int)mode;
        /* Summed two-contact coordinates avoid rounding the centroid. */
        if (dx>=-limit && dx<=limit && dy>=-limit && dy<=limit) {
            dx+=s->remainder_x; dy+=s->remainder_y;
            if (mode==1) { out->dx=dx/scale; out->dy=dy/scale; }
            else { out->wheel_x=-dx/scale; out->wheel_y=-dy/scale; }
            s->remainder_x=dx%scale; s->remainder_y=dy%scale;
        } else s->remainder_x=s->remainder_y=0;
    }
    s->x=x; s->y=y; s->baseline=true; s->mode=mode;
    return changed || out->dx || out->dy || out->wheel_x || out->wheel_y;
}
bool trackpad_pointer_frame(struct trackpad_pointer *s,
    const struct apple_trackpad_frame *f,struct trackpad_motion *out) {
    return frame(s,f,false,false,out);
}
bool trackpad_gesture_frame(struct trackpad_pointer *s,
    const struct apple_trackpad_frame *f,bool typing,struct trackpad_motion *out) {
    return frame(s,f,true,typing,out);
}
