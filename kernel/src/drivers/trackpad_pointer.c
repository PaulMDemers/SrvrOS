#include <srvros/trackpad_pointer.h>

static void tap_reject(struct trackpad_pointer *s,enum trackpad_tap_rejection reason,
    uint64_t now,unsigned travel) {
    if(!s->tap_armed) return;
    struct trackpad_tap_rejections *r=&s->tap_rejections;
    r->count[reason]++;r->two_finger[reason]+=s->tap_peak==2;
    uint64_t age=now>=s->tap_start_us?now-s->tap_start_us:0;
    if(age>r->max_age_us[reason]) r->max_age_us[reason]=age;
    if(travel>r->max_travel[reason]) r->max_travel[reason]=travel;
    s->tap_armed=false;
}

bool trackpad_pointer_cancel(struct trackpad_pointer *s,struct trackpad_motion *out) {
    bool release=s->buttons!=0;
    tap_reject(s,TAP_REJECT_CANCEL,s->tap_last_us,0);
    struct trackpad_tap_rejections rejected=s->tap_rejections;
    *s=(struct trackpad_pointer){.blocked=true,.tap_inhibit=true,.tap_rejections=rejected};
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
    /* Choose a physical button once, at the press. Adding a steering finger
     * to a primary drag must not turn it into a secondary click; lifting one
     * contact from a secondary press must not turn it into a primary click.
     * Cancellation/palm suppression blocks rearming until physical release.
     * This is independent of tap preferences and preserves legacy pointer mode. */
    out->buttons=!f->button || s->blocked?0:
        s->buttons?s->buttons:(gestures && f->count==2?2:1);
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

/* Provisional policy in native units, independent of pointer speed/gain.
 * Allow staggered finger contact/lift without extending single-finger taps.
 * Both contacts must lift before a click is emitted.
 * No timer callback is needed: expiry only disarms, never synthesizes input. */
#define TAP_MIN_US 10000u
#define TAP_MAX_US 200000u
#define TAP_SECONDARY_MAX_US 300000u
#define TAP_CHORD_US 150000u
#define TAP_RELEASE_US 150000u
#define TAP_SLOP 64
static int tap_distance(const struct trackpad_pointer *s,unsigned old,
    const struct apple_trackpad_frame *f,unsigned next) {
    return distance(f->contacts[next].x-s->tap_x[old],f->contacts[next].y-s->tap_y[old]);
}
static void tap_policy(struct trackpad_pointer *s,const struct apple_trackpad_frame *f,
    bool enabled,uint64_t now,unsigned options,struct trackpad_motion *out) {
    options &= SRV_POINTER_TAPS;
    unsigned previous=s->tap_contacts;
    if(s->tap_options!=options && previous) s->tap_inhibit=true;
    s->tap_options=options;
    bool invalid=!enabled || !options || f->button || s->blocked ||
        out->palm_suppressed || out->typing_suppressed || f->count>2 ||
        !now || (s->tap_last_us && now<=s->tap_last_us);
    enum trackpad_tap_rejection reason=
        !now || (s->tap_last_us && now<=s->tap_last_us)?TAP_REJECT_CLOCK:
        f->count>2?TAP_REJECT_CONTACTS:out->palm_suppressed?TAP_REJECT_PALM:
        out->typing_suppressed?TAP_REJECT_TYPING:
        f->button || s->blocked?TAP_REJECT_PHYSICAL:TAP_REJECT_POLICY;
    s->tap_last_us=now;
    if(invalid) s->tap_inhibit=true;
    if(s->tap_inhibit) tap_reject(s,reason,now,0);
    if(s->tap_armed && now-s->tap_start_us>(s->tap_peak==2?TAP_SECONDARY_MAX_US:TAP_MAX_US))
        tap_reject(s,TAP_REJECT_DURATION,now,0);
    if(s->tap_armed && s->tap_releasing && now-s->tap_release_us>TAP_RELEASE_US)
        tap_reject(s,TAP_REJECT_LIFT,now,0);
    if(!f->count) {
        if(s->tap_armed && now-s->tap_start_us>=TAP_MIN_US) {
            if(s->tap_peak==1 && (options&SRV_POINTER_TAP_CLICK)) out->tap_buttons=1;
            if(s->tap_peak==2 && (options&SRV_POINTER_TWO_FINGER_TAP)) out->tap_buttons=2;
        }
        if(s->tap_armed && now-s->tap_start_us<TAP_MIN_US) tap_reject(s,TAP_REJECT_SHORT,now,0);
        s->tap_armed=s->tap_inhibit=s->tap_releasing=false;
    } else if(!previous && !s->tap_inhibit) {
        s->tap_armed=true;s->tap_releasing=false;s->tap_peak=f->count;s->tap_start_us=now;
        for(unsigned i=0;i<f->count;i++) { s->tap_x[i]=f->contacts[i].x;s->tap_y[i]=f->contacts[i].y; }
    } else if(s->tap_armed) {
        if(f->count>previous) {
            if(s->tap_peak!=1 || s->tap_releasing) tap_reject(s,TAP_REJECT_RECONTACT,now,0);
            else if(now-s->tap_start_us>TAP_CHORD_US) tap_reject(s,TAP_REJECT_JOIN,now,0);
            else {
                unsigned old=tap_distance(s,0,f,1)<tap_distance(s,0,f,0);
                if(tap_distance(s,0,f,old)>TAP_SLOP) tap_reject(s,TAP_REJECT_MOVEMENT,now,tap_distance(s,0,f,old));
                else { s->tap_x[1]=f->contacts[1-old].x;s->tap_y[1]=f->contacts[1-old].y;s->tap_peak=2; }
            }
        } else if(f->count==2) {
            unsigned order=tap_distance(s,0,f,1)+tap_distance(s,1,f,0)<
                tap_distance(s,0,f,0)+tap_distance(s,1,f,1);
            unsigned a=tap_distance(s,0,f,order),b=tap_distance(s,1,f,1-order);
            if(a>TAP_SLOP || b>TAP_SLOP) tap_reject(s,TAP_REJECT_MOVEMENT,now,a>b?a:b);
        } else {
            unsigned old=previous==2 && tap_distance(s,1,f,0)<tap_distance(s,0,f,0);
            if(tap_distance(s,old,f,0)>TAP_SLOP) tap_reject(s,TAP_REJECT_MOVEMENT,now,tap_distance(s,old,f,0));
            if(previous==2) {
                s->tap_x[0]=s->tap_x[old];s->tap_y[0]=s->tap_y[old];
                s->tap_releasing=true;s->tap_release_us=now;
            }
        }
    }
    if(s->tap_peak==2 && !(options&SRV_POINTER_TWO_FINGER_TAP)) tap_reject(s,TAP_REJECT_SECONDARY_OFF,now,0);
    /* A quiet two-finger tap must not scroll and then open a context menu.
     * Crossing the displacement/time limit gives subsequent motion to scrolling. */
    if(s->tap_armed && s->tap_peak==2) {
        out->wheel_x=out->wheel_y=0;s->remainder_x=s->remainder_y=0;
    }
    s->tap_contacts=f->count;
}
bool trackpad_pointer_configured(struct trackpad_pointer *s,const struct apple_trackpad_frame *f,
    bool gestures,bool typing,uint64_t now,unsigned speed,unsigned options,struct trackpad_motion *out) {
    uint8_t previous=s->buttons;
    trackpad_pointer_timed(s,f,gestures,typing,now,speed,(options&SRV_POINTER_ACCELERATION)!=0,
        (options&SRV_POINTER_DISABLE_WHILE_TYPING)!=0,out);
    tap_policy(s,f,gestures && !(typing && (options&SRV_POINTER_DISABLE_WHILE_TYPING)),now,options,out);
    return previous!=out->buttons || out->dx || out->dy || out->wheel_x || out->wheel_y || out->tap_buttons;
}
