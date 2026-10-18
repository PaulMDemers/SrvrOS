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
static int32_t distance(int32_t x,int32_t y) {
    return (x<0?-x:x)+(y<0?-y:y);
}
static void drag_start(struct trackpad_pointer *s,const struct apple_trackpad_frame *f) {
    s->drag_count=2;s->drag_active=0;
    for(unsigned i=0;i<2;i++) {
        s->drag_x[i]=s->drag_anchor_x[i]=f->contacts[i].x;
        s->drag_y[i]=s->drag_anchor_y[i]=f->contacts[i].y;
    }
}
/* Only two contacts are eligible here. Choose a unique bounded assignment,
 * never an array slot or the as-yet unverified report origin field. Ambiguous
 * contact crossing/replacement pauses motion without changing the held button. */
static bool drag_point(struct trackpad_pointer *s,const struct apple_trackpad_frame *f,
    int32_t *x,int32_t *y,struct trackpad_motion *out) {
    bool reset=s->drag_count!=2;
    int cost[2][2];
    for(unsigned old=0;old<2;old++) for(unsigned next=0;next<2;next++)
        cost[old][next]=distance(f->contacts[next].x-s->drag_x[old],f->contacts[next].y-s->drag_y[old]);
    int straight=cost[0][0]+cost[1][1],swapped=cost[0][1]+cost[1][0];
    unsigned order=swapped<straight;
    /* Four base-scale pixels of assignment separation; bounded to the same
     * discontinuity scale as ordinary pointing. All arithmetic fits int32. */
    if(distance(straight-swapped,0)<=64 || cost[0][order]>2048 || cost[1][1-order]>2048 ||
       distance(f->contacts[0].x-f->contacts[1].x,f->contacts[0].y-f->contacts[1].y)<=64) reset=true;
    if(reset) {
        drag_start(s,f);*x=s->drag_x[0];*y=s->drag_y[0];return true;
    }
    int32_t px[2],py[2],travel[2];
    for(unsigned i=0;i<2;i++) {
        unsigned next=i?1-order:order;
        px[i]=f->contacts[next].x;py[i]=f->contacts[next].y;
        travel[i]=distance(px[i]-s->drag_anchor_x[i],py[i]-s->drag_anchor_y[i]);
    }
    unsigned active=s->drag_active;
    if(!active) {
        unsigned best=travel[1]>travel[0];
        /* Two base-scale pixels of deliberate travel, with a clear moving finger.
         * A stationary anchor contributes no motion and cannot halve the gain. */
        if(travel[best]>=32 && travel[best]>=2*travel[1-best]+16) active=best+1;
        else if(travel[0]>=32 && travel[1]>=32 &&
            distance((px[0]-s->drag_anchor_x[0])-(px[1]-s->drag_anchor_x[1]),
                     (py[0]-s->drag_anchor_y[0])-(py[1]-s->drag_anchor_y[1]))<=16)
            active=1; /* Both contacts moving together: stable logical owner. */
    } else {
        unsigned owner=active-1,other=1-owner;
        /* A larger four-pixel threshold for switching prevents owner chatter.
         * Evidence accumulates only while the current owner stays within one
         * base-scale pixel of its anchor. */
        if(travel[owner]<=16 && travel[other]>=64) active=other+1;
    }
    bool handoff=active!=s->drag_active;
    if(handoff || (active && travel[active-1]>16)) {
        for(unsigned i=0;i<2;i++) { s->drag_anchor_x[i]=px[i];s->drag_anchor_y[i]=py[i]; }
    }
    out->drag_handoff=handoff && s->drag_active!=0;
    s->drag_active=active;
    for(unsigned i=0;i<2;i++) { s->drag_x[i]=px[i];s->drag_y[i]=py[i]; }
    *x=px[active?active-1:0];*y=py[active?active-1:0];
    return handoff || !active;
}
static uint32_t root(uint32_t n) {
    uint32_t result=0,bit=1u<<30;
    while(bit>n) bit>>=2;
    while(bit) { if(n>=result+bit) { n-=result+bit;result=(result>>1)+bit; } else result>>=1;bit>>=2; }
    return result;
}
static bool frame(struct trackpad_pointer *s,const struct apple_trackpad_frame *f,
    bool gestures,bool typing,uint64_t now,unsigned speed,bool acceleration,struct trackpad_motion *out) {
    *out=(struct trackpad_motion){0};
    if(speed<1 || speed>5) speed=3;
    uint64_t elapsed=now>s->sample_us?now-s->sample_us:0;
    if(s->speed!=speed || s->accelerated!=acceleration) { rebase(s);elapsed=0; }
    s->speed=speed;s->accelerated=acceleration;s->sample_us=now;
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
            s->drag_count=s->drag_active=0;
            s->buttons=0; rebase(s);
            if (s->palm_blocked && f->button) s->blocked=true;
            return release;
        }
    }
    out->buttons=f->button && !s->blocked;
    bool changed=out->buttons!=s->buttons;
    s->buttons=out->buttons;
    bool two_drag=gestures && f->count==2 && out->buttons;
    unsigned mode=f->count==1 || two_drag?1:(gestures && f->count==2 && !f->button?2:0);
    if(!two_drag && s->drag_count) { s->drag_count=s->drag_active=0;rebase(s);elapsed=0; }
    if (!mode) { rebase(s); return changed; }
    int32_t x=f->contacts[0].x,y=f->contacts[0].y;
    if(two_drag) {
        out->two_finger_drag=true;
        if(drag_point(s,f,&x,&y,out)) { rebase(s);elapsed=0;out->drag_rebased=true; }
    }
    if (mode==2) { x+=f->contacts[1].x; y+=f->contacts[1].y; }
    if (s->mode!=mode) rebase(s);
    if (s->baseline) {
        int32_t dx=x-s->x,dy=s->y-y;
        int32_t limit=2048*(int)mode,scale=16*(int)mode;
        /* Summed two-contact coordinates avoid rounding the centroid. */
        if (dx>=-limit && dx<=limit && dy>=-limit && dy<=limit) {
            if(mode==1) {
                unsigned gain=128+64*(speed-1); /* Q8: 0.5, 0.75, 1, 1.25, 1.5. */
                if(acceleration && elapsed>=1000 && elapsed<=100000) {
                    unsigned distance=root((uint32_t)(dx*dx+dy*dy));
                    uint64_t velocity=(uint64_t)distance*1000000/(16*elapsed);
                    /* Gain rises continuously from 1x at 60 px/s to 3x at 600. */
                    unsigned boost=velocity<=60?256:velocity>=600?768:256+(unsigned)(velocity-60)*512/540;
                    gain=gain*boost/256;
                }
                dx=dx*(int)gain+s->remainder_x;dy=dy*(int)gain+s->remainder_y;
                scale=16*256;out->dx=dx/scale;out->dy=dy/scale;
            } else {
                dx+=s->remainder_x;dy+=s->remainder_y;
                out->wheel_x=-dx/scale;out->wheel_y=-dy/scale;
            }
            s->remainder_x=dx%scale;s->remainder_y=dy%scale;
        } else s->remainder_x=s->remainder_y=0;
    }
    s->x=x; s->y=y; s->baseline=true; s->mode=mode;
    return changed || out->dx || out->dy || out->wheel_x || out->wheel_y;
}
bool trackpad_pointer_frame(struct trackpad_pointer *s,
    const struct apple_trackpad_frame *f,struct trackpad_motion *out) {
    return frame(s,f,false,false,0,3,false,out);
}
bool trackpad_gesture_frame(struct trackpad_pointer *s,
    const struct apple_trackpad_frame *f,bool typing,struct trackpad_motion *out) {
    return frame(s,f,true,typing,0,3,false,out);
}

bool trackpad_pointer_timed(struct trackpad_pointer *s,const struct apple_trackpad_frame *f,
    bool gestures,bool typing,uint64_t now,unsigned speed,bool acceleration,bool disable_while_typing,struct trackpad_motion *out) {
    if(!disable_while_typing && s->typing_blocked) {
        s->typing_blocked=false;rebase(s); /* Resume without replaying suppressed travel. */
    }
    return frame(s,f,gestures,typing && disable_while_typing,now,speed,acceleration,out);
}
