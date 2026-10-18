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
    puts("trackpad-pointer: PASS motion, fractional units, drag, lift/recontact, multitouch, discontinuity, cancellation/release");
}
