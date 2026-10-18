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
    f.button=1;assert(trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==1 && !m.dx && !m.wheel_x);
    f.contacts[0].x+=160;f.contacts[1].x+=160;
    assert(!trackpad_gesture_frame(&s,&f,false,&m));
    f.contacts[0].x+=160;f.contacts[1].x+=160;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && m.dx==10 && !m.wheel_x);
    assert(trackpad_pointer_cancel(&s,&m) && !m.buttons);
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && !m.buttons);
    f.button=0;trackpad_gesture_frame(&s,&f,false,&m);
    f.button=1;assert(trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==1);
    f.contacts[0].touch_major=601;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && !m.buttons && m.palm_suppressed);
    /* Converging contacts lose motion confidence, not the physical press. */
    s=(struct trackpad_pointer){0};f=frame(-100,0,2,1);f.contacts[1].x=100;
    trackpad_gesture_frame(&s,&f,false,&m);
    f.contacts[0].x=0;f.contacts[1].x=0;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==1 && m.drag_rebased);
    f.contacts[0].x=-30000;f.contacts[1].x=30000;
    assert(!trackpad_gesture_frame(&s,&f,false,&m) && m.buttons==1 && m.drag_rebased);
    f.count=0;f.button=0;
    assert(trackpad_gesture_frame(&s,&f,false,&m) && !m.buttons);
    assert(!trackpad_gesture_frame(&s,&f,false,&m));
    /* Steering uses the ordinary acceleration path, without centroid dilution. */
    s=(struct trackpad_pointer){0};f=frame(-1000,0,2,1);f.contacts[1].x=1000;
    trackpad_pointer_timed(&s,&f,true,true,1000,3,true,false,&m);
    f.contacts[1].x+=160;trackpad_pointer_timed(&s,&f,true,true,11000,3,true,false,&m);
    f.contacts[1].x+=160;
    assert(trackpad_pointer_timed(&s,&f,true,true,21000,3,true,false,&m) && m.dx==30 && m.buttons==1);
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
    gestures();acceleration();typing_policy();anchored_drag();
    puts("trackpad-pointer: PASS motion, fractional units, drag, lift/recontact, multitouch, anchored drag, handoffs, contact ordering, discontinuity, cancellation/release");
}
