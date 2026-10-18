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
    gestures();
    puts("trackpad-pointer: PASS motion, fractional units, drag, lift/recontact, multitouch, discontinuity, cancellation/release");
}
