#include <assert.h>
#include <stdio.h>
#define SRVROS_MOUSE_TEST_HOST
#include "../kernel/src/drivers/mouse_input.c"
static unsigned wakes,poll_wakes;
void process_file_poll_wake(void) { poll_wakes++; }
static uint64_t test_us;
uint64_t timer_monotonic_us(void) { return test_us; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { assert(q==mouse_wait_queue());wakes++; }
static struct mouse_event next(int x,int y,int buttons) {
    struct mouse_event e;
    assert(mouse_scan(&e)); assert(e.dx==x && e.dy==y && e.buttons==buttons && !e.reserved);
    return e;
}
int main(void) {
    struct input_timing oldest,latest;
    test_us=100;mouse_inject_trackpad(1,0,0);
    test_us=200;mouse_inject_trackpad(2,0,0);
    test_us=500;next(3,0,0);mouse_queue_timing(&oldest,&latest);
    assert(oldest.count==1 && oldest.total_us==400 && latest.total_us==300);
    test_us=600;mouse_inject_trackpad(1,0,0);
    test_us=550;next(1,0,0);mouse_queue_timing(&oldest,&latest);assert(oldest.count==1);
    test_us=700;mouse_inject_trackpad(1,0,0);mouse_inject_event(1,0,0);
    test_us=800;next(2,0,0);mouse_queue_timing(&oldest,&latest);assert(oldest.count==1);
    struct input_timing saturated={.count=UINT64_MAX};
    input_timing_add(&saturated,1,2);assert(saturated.count==UINT64_MAX && !saturated.total_us);
    saturated=(struct input_timing){.total_us=UINT64_MAX};
    input_timing_add(&saturated,1,2);assert(!saturated.count);
    test_us=0;wakes=poll_wakes=0;
    assert(!mouse_pending());
    mouse_inject_trackpad(1,0,0);assert(mouse_pending() && wakes==1 && poll_wakes==1);
    mouse_inject_trackpad(1,0,0);assert(wakes==1 && poll_wakes==1); /* Existing readiness needs no second notification. */
    next(2,0,0);assert(!mouse_pending());
    unsigned speed,options;
    mouse_trackpad_get_settings(&speed,&options);assert(speed==3 && options==0);
    mouse_trackpad_settings(5,15);mouse_trackpad_get_settings(&speed,&options);assert(speed==5 && options==15);
    mouse_trackpad_settings(3,0);mouse_trackpad_get_settings(&speed,&options);assert(speed==3 && !options);
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
    /* Tap edges are adjacent, at a fixed position, and preserve physical ownership. */
    mouse_inject_trackpad(7,9,0);mouse_inject_trackpad_tap(1);mouse_inject_trackpad(3,4,0);
    next(7,9,0);next(0,0,1);next(0,0,0);next(3,4,0);
    mouse_inject_trackpad_tap(2);next(0,0,2);next(0,0,0);
    mouse_inject_event(0,0,1);next(0,0,1);mouse_inject_trackpad_tap(2);
    assert(!mouse_scan(&e));mouse_inject_event(0,0,0);next(0,0,0);
    mouse_inject_trackpad(0,0,1);next(0,0,1);mouse_inject_trackpad_tap(1);
    assert(!mouse_scan(&e));mouse_inject_trackpad(0,0,0);next(0,0,0);
    for(unsigned i=0;i<32;i++) mouse_inject_trackpad_tap(1);
    mouse_inject_trackpad_tap(2);e=next(0,0,0);
    assert((e.flags&MOUSE_CANCEL) && !mouse_scan(&e));
    mouse_inject_trackpad_tap(2);next(0,0,2);next(0,0,0);
    /* A device/palm cancellation must not look like release-to-activate.
     * Discard pending pointer work but retain an external held button. */
    mouse_inject_event(0,0,1);next(0,0,1);
    mouse_cancel_trackpad();assert(!mouse_scan(&e));
    mouse_inject_trackpad(0,0,2);mouse_inject_trackpad(10,20,2);
    uint64_t overflow_before=mouse_overflow_count();
    mouse_cancel_trackpad();e=next(0,0,1);
    assert((e.flags&MOUSE_CANCEL) && !mouse_scan(&e) && mouse_overflow_count()==overflow_before);
    mouse_inject_trackpad(0,0,0);assert(!mouse_scan(&e));
    mouse_inject_event(0,0,0);e=next(0,0,0);assert(!(e.flags&MOUSE_CANCEL));
    mouse_inject_trackpad(0,0,1);next(0,0,1);
    mouse_cancel_trackpad();e=next(0,0,0);assert(e.flags&MOUSE_CANCEL);
    mouse_cancel_trackpad();assert(!mouse_scan(&e));
    mouse_inject_trackpad_tap(2);next(0,0,2);next(0,0,0);
    test_us=1000;mouse_inject_trackpad(0,0,1);mouse_cancel_trackpad();
    test_us=2000;e=next(0,0,0);assert(e.flags&MOUSE_CANCEL);
    mouse_queue_timing(&oldest,&latest);assert(oldest.count==1);
    mouse_inject_trackpad(0,0,0);assert(!mouse_scan(&e));
    for(unsigned i=0;i<65;i++)mouse_inject_trackpad(0,0,(i&1)?0:1);
    e=next(0,0,0);assert(e.flags&MOUSE_CANCEL);
    mouse_queue_timing(&oldest,&latest);assert(oldest.count==1);
    assert(wakes==poll_wakes); /* Enqueue, overflow and cancellation notify both paths. */
    puts("mouse-input: PASS ordered edges, click positions, coalescing, source buttons, saturation, overflow cancellation, wrap");
}
