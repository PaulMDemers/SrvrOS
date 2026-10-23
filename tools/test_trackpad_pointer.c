#include <assert.h>
#include <stdio.h>
#include <srvros/trackpad_pointer.h>
static struct apple_trackpad_frame frame(int x,int y,int count,int button) {
    struct apple_trackpad_frame f={.count=count,.button=button};
    f.contacts[0].x=x; f.contacts[0].y=y;
    return f;
}
static bool feed(struct trackpad_pointer *s,struct trackpad_motion *m,int x,int y,int n,int b) {
    struct apple_trackpad_frame f=frame(x,y,n,b);
    return trackpad_pointer_frame(s,&f,m);
}
static void gestures(void) {
    struct trackpad_pointer s={0}; struct trackpad_motion m;
    struct apple_trackpad_frame f=frame(100,200,2,0);
    f.contacts[1].x=300; f.contacts[1].y=400;
    assert(!trackpad_gesture_frame(&s,&f,false,&m));
    f.contacts[0].y+=160; f.contacts[1].y+=160;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.wheel_y==10 && !m.dx && !m.dy);
    f.contacts[0].y-=80; f.contacts[1].y-=80;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.wheel_y==-5);
    struct apple_trackpad_contact temp=f.contacts[0]; f.contacts[0]=f.contacts[1]; f.contacts[1]=temp;
    assert(!trackpad_gesture_frame(&s,&f,false,&m)); /* Slot ordering is irrelevant to centroid. */
    f.count=1; assert(!trackpad_gesture_frame(&s,&f,false,&m));
    f.contacts[0].x+=160;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.dx==10 && !m.wheel_y);
    assert(!trackpad_gesture_frame(&s,&f,true,&m) && m.typing_suppressed);
    f.contacts[0].x+=160;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.typing_suppressed); /* Latched until lift. */
    f.button=1;
    assert(trackpad_gesture_frame(&s,&f,true,&m) && m.buttons && !m.typing_suppressed && !m.dx);
    f.contacts[0].x+=160;
    assert(trackpad_gesture_frame(&s,&f,true,&m) && m.dx==10 && m.buttons); /* Explicit drag survives typing. */
    f.contacts[0].touch_major=601;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && !m.buttons && m.palm_suppressed);
    f.contacts[0].touch_major=50; f.button=0;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.palm_suppressed);
    f.count=0; assert(!trackpad_gesture_frame(&s,&f,false,&m));
    f.count=1; f.contacts[0].x=-30000;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && !m.palm_suppressed);
    f.contacts[0].x+=16;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.dx==1);
    f.count=3; assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.palm_suppressed);
}
static int timed_path(int dx,int dt,int steps,unsigned speed,bool accel) {
    struct trackpad_pointer s={0};struct trackpad_motion m;
    struct apple_trackpad_frame f=frame(0,0,1,0);uint64_t now=1000000;int total=0;
    assert(!trackpad_pointer_timed(&s,&f,true,false,now,speed,accel,true,&m));
    for(int n=0;n<steps;n++) { now+=dt;f.contacts[0].x+=dx;
        trackpad_pointer_timed(&s,&f,true,false,now,speed,accel,true,&m);total+=m.dx;assert(!m.dy && !m.wheel_y); }
    return total;
}
static void acceleration(void) {
    assert(timed_path(8,10000,40,3,true)==20); /* Precision below threshold. */
    assert(timed_path(160,10000,50,3,false)==500);
    assert(timed_path(160,10000,50,3,true)==1500);
    assert(timed_path(16,1000,500,3,true)==1500); /* Same trajectory at 10x sample rate. */
    int middle=timed_path(160,50000,20,3,true);
    assert(middle>200 && middle<600);
    assert(timed_path(80,25000,40,3,true)==middle); /* Also within the ramp. */
    int previous=0;
    for(unsigned speed=1;speed<=5;speed++) {
        int travel=timed_path(160,50000,20,speed,true);
        assert(travel>previous);previous=travel;
    }
    assert(timed_path(-160,10000,50,3,true)==-1500);
    assert(timed_path(160,10000,50,1,false)==250);
    assert(timed_path(160,10000,50,5,false)==750);
    assert(timed_path(160,0,20,3,true)==200); /* Bad clock cannot amplify. */
    assert(timed_path(160,200000,20,3,true)==200); /* Pause resets velocity. */
    struct trackpad_pointer s={0};struct trackpad_motion m;
    struct apple_trackpad_frame f=frame(0,0,1,0);
    trackpad_pointer_timed(&s,&f,true,false,10000,3,true,true,&m);
    f.contacts[0].x=160;f.contacts[0].y=-160;
    assert(trackpad_pointer_timed(&s,&f,true,false,20000,3,true,true,&m) && m.dx==30 && m.dy==30);
    f.count=2;f.contacts[1].x=300;f.contacts[1].y=400;
    assert(!trackpad_pointer_timed(&s,&f,true,false,30000,3,true,true,&m));
    f.contacts[0].y+=160;f.contacts[1].y+=160;
    assert(trackpad_pointer_timed(&s,&f,true,false,31000,5,true,true,&m)==false); /* Setting change rebases. */
    f.contacts[0].y+=160;f.contacts[1].y+=160;
    assert(trackpad_pointer_timed(&s,&f,true,false,32000,5,true,true,&m) && m.wheel_y==10 && !m.dx);
    f.count=0;trackpad_pointer_timed(&s,&f,true,false,33000,5,true,true,&m);
    f.count=1;f.contacts[0].x=-2000;
    assert(!trackpad_pointer_timed(&s,&f,true,false,34000,5,true,true,&m));
    trackpad_pointer_cancel(&s,&m);f.contacts[0].x=2000;
    assert(!trackpad_pointer_timed(&s,&f,true,false,35000,5,true,true,&m));
    /* Equal-distance axis/diagonal moves use the same velocity gain, even
     * inside the ramp; output rounding differs by less than one pixel. */
    struct trackpad_pointer axis={0},diagonal={0};struct trackpad_motion a,b;
    struct apple_trackpad_frame af=frame(0,0,1,0),bf=af;
    trackpad_pointer_timed(&axis,&af,true,false,1000,3,true,true,&a);
    trackpad_pointer_timed(&diagonal,&bf,true,false,1000,3,true,true,&b);
    af.contacts[0].x=800;bf.contacts[0].x=480;bf.contacts[0].y=-640;
    trackpad_pointer_timed(&axis,&af,true,false,101000,3,true,true,&a);
    trackpad_pointer_timed(&diagonal,&bf,true,false,101000,3,true,true,&b);
    int ax=5*b.dx-3*a.dx,ay=5*b.dy-4*a.dx;
    assert(ax>=-5 && ax<=5 && ay>=-5 && ay<=5);
}
static void typing_policy(void) {
    struct trackpad_pointer s={0};struct trackpad_motion m;
    struct apple_trackpad_frame f=frame(0,0,1,0);
    assert(!trackpad_pointer_timed(&s,&f,true,true,1000,3,false,false,&m));
    f.contacts[0].x=160;
    assert(trackpad_pointer_timed(&s,&f,true,true,11000,3,false,false,&m) && m.dx==10 && !m.typing_suppressed);
    f.contacts[0].x+=160;
    assert(!trackpad_pointer_timed(&s,&f,true,true,21000,3,false,true,&m) && m.typing_suppressed);
    f.contacts[0].x+=160;
    assert(!trackpad_pointer_timed(&s,&f,true,false,31000,3,false,true,&m) && m.typing_suppressed);
    /* Switching off clears the latch immediately, rebasing rather than jumping. */
    assert(!trackpad_pointer_timed(&s,&f,true,true,41000,3,false,false,&m) && !m.typing_suppressed);
    f.contacts[0].x+=160;
    assert(trackpad_pointer_timed(&s,&f,true,true,51000,3,false,false,&m) && m.dx==10);
    f.contacts[0].touch_major=601;
    assert(!trackpad_pointer_timed(&s,&f,true,true,61000,3,false,false,&m) && m.palm_suppressed);
}
static void swap_contacts(struct apple_trackpad_frame *f) {
    struct apple_trackpad_contact t=f->contacts[0];f->contacts[0]=f->contacts[1];f->contacts[1]=t;
}
static void anchored_drag(void) {
    struct trackpad_pointer s={0};struct trackpad_motion m;
    struct apple_trackpad_frame f=frame(-1000,0,1,1);
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==1);
    f.count=2;f.contacts[1].x=1000;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==1 && m.drag_rebased);
    f.contacts[1].x+=160;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.drag_rebased && !m.dx);
    f.contacts[1].x+=160;
    assert(trackpad_gesture_frame(&s,&f,true,&m) && m.dx==10 && m.buttons==1 && !m.wheel_x);
    /* Report order and raw origin values do not define finger identity. */
    swap_contacts(&f);f.contacts[0].origin=65535;f.contacts[1].origin=65535;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && !m.drag_rebased);
    f.contacts[0].x+=160;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.dx==10 && m.buttons==1);
    /* Stationary steering finger, then deliberate movement of the old anchor. */
    f.contacts[1].x+=16;assert(!trackpad_gesture_frame(&s,&f,false,&m));
    f.contacts[1].x+=16;assert(!trackpad_gesture_frame(&s,&f,false,&m));
    f.contacts[1].x+=32;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.drag_handoff && m.drag_rebased && m.buttons==1);
    f.contacts[1].x+=160;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.dx==10 && m.buttons==1);
    /* Lift the selected finger: do not jump to the remaining distant finger. */
    f.count=1;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==1 && !m.dx);
    f.contacts[0].x+=160;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.dx==10 && m.buttons==1);
    /* Replace it on the other side, then start dragging in that direction. */
    f.count=2;f.contacts[1].x=-3000;
    assert(!trackpad_gesture_frame(&s,&f,false,&m));
    f.contacts[1].x-=160;assert(!trackpad_gesture_frame(&s,&f,false,&m));
    f.contacts[1].x-=160;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.dx==-10 && m.buttons==1);
    f.button=0;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && !m.buttons && !m.dx && !m.wheel_x);
    f.contacts[0].x+=160;f.contacts[1].x+=160;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.wheel_x==-10 && !m.dx);
    /* A new physical press must not replay scroll as drag motion. */
    f.button=1;assert(trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==2 && !m.dx && !m.wheel_x);
    f.contacts[0].x+=160;f.contacts[1].x+=160;
    assert(!trackpad_gesture_frame(&s,&f,false,&m));
    f.contacts[0].x+=160;f.contacts[1].x+=160;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.dx==10 && !m.wheel_x);
    assert(trackpad_pointer_cancel(&s,&m) && !m.buttons);
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && !m.buttons);
    f.button=0;trackpad_gesture_frame(&s,&f,false,&m);
    f.button=1;assert(trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==2);
    f.contacts[0].touch_major=601;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && !m.buttons && m.palm_suppressed);
    /* Converging contacts lose motion confidence, not the physical press. */
    s=(struct trackpad_pointer){0};f=frame(-100,0,1,1);f.contacts[1].x=100;
    trackpad_gesture_frame(&s,&f,false,&m);
    f.count=2;trackpad_gesture_frame(&s,&f,false,&m);
    f.contacts[0].x=0;f.contacts[1].x=0;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==1 && m.drag_rebased);
    f.contacts[0].x=-30000;f.contacts[1].x=30000;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==1 && m.drag_rebased);
    f.count=0;f.button=0;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && !m.buttons);
    assert(!trackpad_gesture_frame(&s,&f,false,&m));
    /* Steering uses the ordinary acceleration path, without centroid dilution. */
    s=(struct trackpad_pointer){0};f=frame(-1000,0,1,1);f.contacts[1].x=1000;
    trackpad_pointer_timed(&s,&f,true,true,500,3,true,false,&m);
    f.count=2;trackpad_pointer_timed(&s,&f,true,true,1000,3,true,false,&m);
    f.contacts[1].x+=160;trackpad_pointer_timed(&s,&f,true,true,11000,3,true,false,&m);
    f.contacts[1].x+=160;
    assert(trackpad_pointer_timed(&s,&f,true,true,21000,3,true,false,&m) && m.dx==30 && m.buttons==1);
}
static struct trackpad_motion tap_feed(struct trackpad_pointer *s,struct apple_trackpad_frame *f,
    uint64_t now,unsigned options,bool typing) {
    struct trackpad_motion m;
    trackpad_pointer_configured(s,f,true,typing,now,3,options,&m);
    assert(!(m.tap_buttons && (m.buttons || f->count || m.dx || m.dy || m.wheel_x || m.wheel_y)));
    return m;
}
static void tapping(void) {
    const unsigned taps=SRV_POINTER_TAPS;
    struct trackpad_pointer s={0};struct apple_trackpad_frame f=frame(-1000,0,1,0);
    struct trackpad_motion m=tap_feed(&s,&f,1000,taps,false);assert(!m.tap_buttons);
    f.count=0;m=tap_feed(&s,&f,51000,taps,false);assert(m.tap_buttons==1);
    assert(!tap_feed(&s,&f,61000,taps,false).tap_buttons);
    f.count=1;tap_feed(&s,&f,71000,taps,false);f.count=0;
    assert(tap_feed(&s,&f,121000,taps,false).tap_buttons==1); /* Rapid repeated taps. */
    s=(struct trackpad_pointer){0};f=frame(-1000,0,2,0);f.contacts[1].x=1000;
    tap_feed(&s,&f,1000,taps,false);swap_contacts(&f);f.contacts[0].origin=9;
    m=tap_feed(&s,&f,20000,taps,false);assert(!m.tap_buttons && !m.buttons);
    f.count=1;tap_feed(&s,&f,40000,taps,false);f.count=0;
    assert(tap_feed(&s,&f,60000,taps,false).tap_buttons==2);
    /* Staggered arrival, reordered chord, staggered departure. */
    s=(struct trackpad_pointer){0};f=frame(-1000,0,1,0);
    tap_feed(&s,&f,1000,taps,false);f.count=2;f.contacts[1].x=1000;swap_contacts(&f);
    tap_feed(&s,&f,30000,taps,false);f.count=1;tap_feed(&s,&f,50000,taps,false);f.count=0;
    assert(tap_feed(&s,&f,70000,taps,false).tap_buttons==2);
    /* A human chord may arrive/release across several reports. Cover both
     * contact orders and either surviving finger, including the exact timing
     * boundaries. A rejected chord must never fall back to a primary click. */
    const unsigned joins[]={0,30000,80000,100000,130000,150000,150001};
    const unsigned lifts[]={1,50000,100000,150000,150001};
    for(unsigned j=0;j<sizeof(joins)/sizeof(joins[0]);j++)
    for(unsigned l=0;l<sizeof(lifts)/sizeof(lifts[0]);l++)
    for(unsigned swap=0;swap<2;swap++) for(unsigned remaining=0;remaining<2;remaining++) {
        s=(struct trackpad_pointer){0};f=frame(-1000,0,joins[j]?1:2,0);
        f.contacts[1].x=1000;
        if(!joins[j] && swap) swap_contacts(&f);
        assert(!tap_feed(&s,&f,1000,taps,false).tap_buttons);
        if(joins[j]) {
            f.count=2;
            if(swap) swap_contacts(&f);
            assert(!tap_feed(&s,&f,1000+joins[j],taps,false).tap_buttons);
        }
        f.contacts[0]=f.contacts[remaining];f.count=1;
        uint64_t first_lift=1000+joins[j]+20000;
        assert(!tap_feed(&s,&f,first_lift,taps,false).tap_buttons);
        f.count=0;
        unsigned total=joins[j]+20000+lifts[l];
        unsigned expected=joins[j]<=150000 && lifts[l]<=150000 && total<=300000?2:0;
        assert(tap_feed(&s,&f,first_lift+lifts[l],taps,false).tap_buttons==expected);
        assert(!tap_feed(&s,&f,first_lift+lifts[l]+1000,taps,false).tap_buttons);
        unsigned rejected=0;
        for(unsigned reason=0;reason<TAP_REJECT_COUNT;reason++) rejected+=s.tap_rejections.count[reason];
        assert(rejected==(expected?0u:1u));
        if(!expected) {
            unsigned reason=joins[j]>150000?TAP_REJECT_JOIN:total>300000?TAP_REJECT_DURATION:TAP_REJECT_LIFT;
            assert(s.tap_rejections.count[reason]==1);
            assert(s.tap_rejections.two_finger[reason]==(reason==TAP_REJECT_JOIN?0u:1u));
        }
    }
    /* Two-finger jitter is quiet. Deliberate scroll cancels tapping permanently. */
    s=(struct trackpad_pointer){0};f=frame(-1000,0,2,0);f.contacts[1].x=1000;
    tap_feed(&s,&f,1000,taps,false);f.contacts[0].y=f.contacts[1].y=96;
    m=tap_feed(&s,&f,20000,taps,false);assert(!m.wheel_y);
    f.contacts[0].y=f.contacts[1].y=144;m=tap_feed(&s,&f,30000,taps,false);assert(m.wheel_y==3);
    f.count=0;assert(!tap_feed(&s,&f,50000,taps,false).tap_buttons);
    /* Secondary settling allowance covers reordered contacts at join, with both
     * down, and either surviving contact after first lift. Diagonal movement
     * uses the same Manhattan metric. Crossing the limit cannot rearm on return. */
    const unsigned travels[]={64,65,127,128,129,256};
    for(unsigned phase=0;phase<3;phase++) for(unsigned swap=0;swap<2;swap++)
    for(unsigned survivor=0;survivor<2;survivor++)
    for(unsigned t=0;t<sizeof(travels)/sizeof(travels[0]);t++) {
        s=(struct trackpad_pointer){0};f=frame(-1000,0,phase==0?1:2,0);
        f.contacts[1].x=1000;tap_feed(&s,&f,1000,taps,false);
        f.count=2;
        unsigned moving=phase==0?0:survivor;
        f.contacts[moving].x+=(int)(travels[t]/2);
        f.contacts[moving].y+=(int)(travels[t]-travels[t]/2);
        if(phase==2) { f.contacts[0]=f.contacts[survivor];f.count=1; }
        else if(swap) swap_contacts(&f);
        m=tap_feed(&s,&f,21000,taps,false);
        if(travels[t]<=128 && f.count==2) assert(!m.wheel_x && !m.wheel_y);
        /* Returning to the original coordinate cannot revive a rejected tap. */
        if(phase==2) f.contacts[0]=frame(survivor?1000:-1000,0,1,0).contacts[0];
        else { f=frame(-1000,0,2,0);f.contacts[1].x=1000;if(swap) swap_contacts(&f); }
        tap_feed(&s,&f,31000,taps,false);f.count=0;
        assert(tap_feed(&s,&f,51000,taps,false).tap_buttons==(travels[t]<=128?2:0));
        assert(s.tap_rejections.count[TAP_REJECT_MOVEMENT]==(travels[t]>128));
    }
    /* A one-finger move remains limited to 64 even with both preferences on.
     * Adding a second finger after rejection must not revive the candidate. */
    for(unsigned travel=64;travel<=65;travel++) for(unsigned join=0;join<2;join++) {
        s=(struct trackpad_pointer){0};f=frame(-1000,0,1,0);
        tap_feed(&s,&f,1000,taps,false);f.contacts[0].x+=(int)travel;
        tap_feed(&s,&f,21000,taps,false);
        if(join) { f.count=2;f.contacts[1].x=1000;tap_feed(&s,&f,31000,taps,false); }
        f.count=0;assert(tap_feed(&s,&f,51000,taps,false).tap_buttons==(travel==64?(join?2:1):0));
    }
    /* Independent options: a secondary tap never falls back to primary. */
    for(unsigned opts=0;opts<=taps;opts+=4) for(int count=1;count<=2;count++) {
        s=(struct trackpad_pointer){0};f=frame(-1000,0,count,0);f.contacts[1].x=1000;
        tap_feed(&s,&f,1000,opts,false);f.count=0;m=tap_feed(&s,&f,51000,opts,false);
        assert(m.tap_buttons==(count==1?(opts&SRV_POINTER_TAP_CLICK?1:0):(opts&SRV_POINTER_TWO_FINGER_TAP?2:0)));
    }
    /* Long hold, short noise, late join, replacement, broad contact, backwards
     * clock, configuration changes and canceled transport must not click. */
    for(unsigned reason=0;reason<12;reason++) {
        s=(struct trackpad_pointer){0};f=frame(-1000,0,1,0);
        tap_feed(&s,&f,10000,taps,false);uint64_t end=100000;
        unsigned opts=taps;
        switch(reason) {
        case 0:end=220000;break;
        case 1:end=15000;break;
        case 2:f.count=2;f.contacts[1].x=1000;tap_feed(&s,&f,161000,taps,false);end=180000;break;
        case 3:f.contacts[0].x=2000;tap_feed(&s,&f,20000,taps,false);break;
        case 4:f.contacts[0].touch_major=601;tap_feed(&s,&f,20000,taps,false);break;
        case 5:f.count=3;tap_feed(&s,&f,20000,taps,false);break;
        case 6:tap_feed(&s,&f,9000,taps,false);break;
        case 7:tap_feed(&s,&f,20000,0,false);break;
        case 8:trackpad_pointer_cancel(&s,&m);break;
        case 9:f.button=1;assert(tap_feed(&s,&f,20000,taps,false).buttons==1);
            f.button=0;assert(!tap_feed(&s,&f,30000,taps,false).buttons);break;
        case 10:opts=taps|SRV_POINTER_DISABLE_WHILE_TYPING;break;
        case 11:f.count=2;f.contacts[1].x=1000;tap_feed(&s,&f,20000,taps,false);
            f.count=1;tap_feed(&s,&f,30000,taps,false);end=190000;break;
        }
        f.count=0;assert(!tap_feed(&s,&f,end,opts,reason==10).tap_buttons);
        const unsigned expected[]={TAP_REJECT_DURATION,TAP_REJECT_SHORT,TAP_REJECT_JOIN,
            TAP_REJECT_MOVEMENT,TAP_REJECT_PALM,TAP_REJECT_CONTACTS,TAP_REJECT_CLOCK,
            TAP_REJECT_POLICY,TAP_REJECT_CANCEL,TAP_REJECT_PHYSICAL,TAP_REJECT_POLICY,TAP_REJECT_LIFT};
        unsigned total=0;
        for(unsigned i=0;i<TAP_REJECT_COUNT;i++) total+=s.tap_rejections.count[i];
        assert(total==1 && s.tap_rejections.count[expected[reason]]==1);
    }
    s=(struct trackpad_pointer){0};f=frame(-1000,0,1,0);
    tap_feed(&s,&f,1000,0,false);tap_feed(&s,&f,20000,taps,false);f.count=0;
    assert(!tap_feed(&s,&f,40000,taps,false).tap_buttons); /* Enabling during touch. */
    f.count=1;tap_feed(&s,&f,50000,taps,true);f.count=0;
    assert(tap_feed(&s,&f,80000,taps,true).tap_buttons==1); /* Typing opt-out. */
    s=(struct trackpad_pointer){0};f=frame(-1000,0,2,0);f.contacts[1].x=1000;
    tap_feed(&s,&f,1000,taps,false);f.count=1;tap_feed(&s,&f,20000,taps,false);
    f.count=2;tap_feed(&s,&f,30000,taps,false);f.count=0;
    assert(!tap_feed(&s,&f,40000,taps,false).tap_buttons); /* Finger replacement after release began. */
    assert(s.tap_rejections.count[TAP_REJECT_RECONTACT]==1);
    /* First rejection is retained across later invalid frames and transport cancel. */
    s=(struct trackpad_pointer){0};f=frame(-1000,0,2,0);f.contacts[1].x=1000;
    tap_feed(&s,&f,1000,taps,false);f.contacts[1].x+=129;
    tap_feed(&s,&f,21000,taps,false);f.contacts[1].x+=100;
    tap_feed(&s,&f,31000,taps,false);trackpad_pointer_cancel(&s,&m);
    assert(s.tap_rejections.count[TAP_REJECT_MOVEMENT]==1);
    assert(s.tap_rejections.two_finger[TAP_REJECT_MOVEMENT]==1);
    assert(s.tap_rejections.max_age_us[TAP_REJECT_MOVEMENT]==20000);
    assert(s.tap_rejections.max_travel[TAP_REJECT_MOVEMENT]==129);
    assert(!s.tap_rejections.count[TAP_REJECT_CANCEL]);
    s=(struct trackpad_pointer){0};f=frame(-1000,0,1,0);
    unsigned typing_opts=taps|SRV_POINTER_DISABLE_WHILE_TYPING;
    tap_feed(&s,&f,1000,typing_opts,false);tap_feed(&s,&f,21000,typing_opts,true);
    assert(s.tap_rejections.count[TAP_REJECT_TYPING]==1);
    s=(struct trackpad_pointer){0};f=frame(-1000,0,2,0);f.contacts[1].x=1000;
    tap_feed(&s,&f,1000,SRV_POINTER_TAP_CLICK,false);
    assert(s.tap_rejections.count[TAP_REJECT_SECONDARY_OFF]==1);
}

static void secondary_click(void) {
    /* Physical button identity must survive every contact-count/order change,
     * independently of tap settings. Enumerate both initial press types. */
    for(unsigned options=0;options<=SRV_POINTER_TAPS;options+=4)
    for(unsigned first=1;first<=2;first++) {
        struct trackpad_pointer s={0};
        struct apple_trackpad_frame f=frame(-1000,0,first,0);
        f.contacts[1].x=1000;
        uint64_t now=1000;
        tap_feed(&s,&f,now,options,false);
        f.button=1;
        struct trackpad_motion m=tap_feed(&s,&f,now+=20000,options,false);
        assert(m.buttons==first && !m.tap_buttons && !m.wheel_x && !m.wheel_y);
        for(unsigned n=0;n<12;n++) {
            f.count=n%3; /* Zero contacts with physical button still held too. */
            if(f.count==2) swap_contacts(&f);
            m=tap_feed(&s,&f,now+=20000,options,false);
            assert(m.buttons==first && !m.tap_buttons && !m.wheel_x && !m.wheel_y);
        }
        f.button=0;f.count=0;
        m=tap_feed(&s,&f,now+=20000,options,false);
        assert(!m.buttons && !m.tap_buttons);
        f.count=2;f.button=1;
        m=tap_feed(&s,&f,now+=20000,options,false);assert(m.buttons==2);
        assert(trackpad_pointer_cancel(&s,&m) && !m.buttons);
        assert(!tap_feed(&s,&f,now+=20000,options,false).buttons);
        f.button=0;tap_feed(&s,&f,now+=20000,options,false);
        f.button=1;m=tap_feed(&s,&f,now+=20000,options,false);assert(m.buttons==2);
        f.contacts[0].touch_major=601;
        m=tap_feed(&s,&f,now+=20000,options,false);assert(!m.buttons && m.palm_suppressed);
        f.count=0;assert(!tap_feed(&s,&f,now+=20000,options,false).buttons);
        f.count=1;f.contacts[0].touch_major=10;
        assert(!tap_feed(&s,&f,now+=20000,options,false).buttons);
        f.button=0;tap_feed(&s,&f,now+=20000,options,false);
        f.button=1;assert(tap_feed(&s,&f,now+=20000,options,false).buttons==1);
    }
    struct trackpad_pointer s={0};struct trackpad_motion m;
    assert(feed(&s,&m,0,0,2,1) && m.buttons==1); /* Legacy pointer mode unchanged. */
}

int main(void) {
    struct trackpad_pointer s={0}; struct trackpad_motion m;
    assert(!feed(&s,&m,-2635,3858,1,0)); /* Real capture starting coordinate. */
    assert(feed(&s,&m,-2475,3698,1,0) && m.dx==10 && m.dy==10);
    assert(feed(&s,&m,-2475,3698,1,1) && m.buttons==1 && !m.dx && !m.dy);
    assert(feed(&s,&m,-2635,3858,1,1) && m.dx==-10 && m.dy==-10 && m.buttons==1);
    assert(feed(&s,&m,0,0,0,0) && !m.buttons); /* Lift/release. */
    assert(!feed(&s,&m,3000,-2000,1,0)); /* Re-contact never jumps. */
    for (int i=1;i<16;i++) assert(!feed(&s,&m,3000+i,-2000,1,0));
    assert(feed(&s,&m,3016,-2000,1,0) && m.dx==1); /* Fractional accumulation. */
    assert(feed(&s,&m,3000,-2000,1,0) && m.dx==-1);
    assert(!feed(&s,&m,2900,-1800,2,0)); /* Multitouch pauses motion. */
    assert(!feed(&s,&m,-3000,3000,1,0));
    assert(!feed(&s,&m,32767,-32768,1,0)); /* Discontinuity is rebased. */
    assert(feed(&s,&m,32751,-32752,1,1) && m.dx==-1 && m.dy==-1 && m.buttons);
    assert(trackpad_pointer_cancel(&s,&m) && !m.buttons);
    assert(!feed(&s,&m,0,0,1,1)); /* Held button cannot become a new press after error. */
    assert(!feed(&s,&m,0,0,1,0));
    assert(feed(&s,&m,0,0,1,1) && m.buttons);
    assert(feed(&s,&m,0,0,0,0) && !m.buttons);
    assert(!trackpad_pointer_cancel(&s,&m));
    gestures();acceleration();typing_policy();anchored_drag();tapping();secondary_click();
    puts("trackpad-pointer: PASS motion, fractional units, drag, lift/recontact, multitouch, anchored drag, handoffs, contact ordering, tap arbitration, discontinuity, cancellation/release");
}
