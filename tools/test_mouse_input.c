#include <assert.h>
#include <stdio.h>
#define SRVROS_MOUSE_TEST_HOST
#include "../kernel/src/drivers/mouse_input.c"
static struct mouse_event next(int x,int y,int buttons) {
    struct mouse_event e;
    assert(mouse_scan(&e)); assert(e.dx==x && e.dy==y && e.buttons==buttons && !e.reserved);
    return e;
}
int main(void) {
    unsigned speed;bool acceleration,typing;
    mouse_trackpad_get_settings(&speed,&acceleration,&typing);
    assert(speed==3 && !acceleration && !typing);
    mouse_trackpad_settings(5,true,true);mouse_trackpad_get_settings(&speed,&acceleration,&typing);
    assert(speed==5 && acceleration && typing);
    mouse_trackpad_settings(3,false,false);mouse_trackpad_get_settings(&speed,&acceleration,&typing);
    assert(speed==3 && !acceleration && !typing);
    struct mouse_event e;
    assert(!mouse_scan(NULL) && !mouse_scan(&e));
    mouse_inject_trackpad(10,20,0); mouse_inject_trackpad(5,-5,0); next(15,15,0);
    mouse_inject_trackpad(0,0,1); mouse_inject_trackpad(10,0,1); mouse_inject_trackpad(0,0,0);
    next(0,0,1); next(10,0,1); next(0,0,0); assert(!mouse_scan(&e));
    /* Device ownership: releasing the pad must not release another mouse. */
    mouse_inject_event(0,0,1); mouse_inject_trackpad(0,0,1); mouse_inject_trackpad(0,0,0);
    next(0,0,1); assert(!mouse_scan(&e)); mouse_inject_event(0,0,0); next(0,0,0);
    mouse_inject_event(INT32_MAX,INT32_MIN,0); mouse_inject_event(1,-1,0);
    next(INT32_MAX,INT32_MIN,0);
    for (int i=0;i<65;i++) mouse_inject_trackpad(0,0,(i&1)?0:1);
    assert(mouse_overflow_count()==1);
    e=next(0,0,0); assert(e.flags&MOUSE_CANCEL); assert(!mouse_scan(&e));
    mouse_inject_trackpad(0,0,1); assert(!mouse_scan(&e)); /* Suppress until released. */
    mouse_inject_trackpad(0,0,0); assert(!mouse_scan(&e));
    mouse_inject_trackpad(0,0,1); mouse_inject_trackpad(0,0,0);
    next(0,0,1); next(0,0,0);
    for (int i=0;i<200;i++) { mouse_inject_event(1,0,0); next(1,0,0); }
    mouse_inject_trackpad_scroll(0,0,0,10,0); mouse_inject_trackpad_scroll(0,0,0,20,0);
    mouse_inject_trackpad_scroll(0,0,0,-5,0); mouse_inject_trackpad(16,0,0);
    e=next(0,0,0); assert(e.wheel_y==30);
    e=next(0,0,0); assert(e.wheel_y==-5);
    e=next(16,0,0); assert(!e.wheel_y);
    puts("mouse-input: PASS ordered edges, click positions, coalescing, source buttons, saturation, overflow cancellation, wrap");
}
