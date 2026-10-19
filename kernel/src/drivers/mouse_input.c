#include <srvros/mouse.h>
#include <limits.h>

/* One CPU currently owns input. IRQ exclusion also covers producers reached
 * from storage service hooks. Keep button edges even if userspace is delayed. */
#define MOUSE_QUEUE_SIZE 64
static struct mouse_event queue[MOUSE_QUEUE_SIZE];
static unsigned head,count;
static uint64_t overflows;
static uint8_t source_buttons[2],blocked[2];
static uint64_t lock(void) {
#ifndef SRVROS_MOUSE_TEST_HOST
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
#else
    return 0;
#endif
}
static void unlock(uint64_t flags) {
#ifndef SRVROS_MOUSE_TEST_HOST
    if (flags&(1ull<<9)) __asm__ volatile("sti" ::: "memory");
#else
    (void)flags;
#endif
}
static int32_t sum(int32_t a,int32_t b) {
    int64_t n=(int64_t)a+b;
    return n>INT32_MAX?INT32_MAX:n<INT32_MIN?INT32_MIN:(int32_t)n;
}
static void inject(unsigned source,int32_t dx,int32_t dy,int32_t wx,int32_t wy,uint8_t buttons) {
    uint64_t flags=lock();
    uint8_t previous=(source_buttons[0]&~blocked[0])|(source_buttons[1]&~blocked[1]);
    source_buttons[source]=buttons&7;
    blocked[source]&=buttons;
    buttons=(source_buttons[0]&~blocked[0])|(source_buttons[1]&~blocked[1]);
    if (!dx && !dy && !wx && !wy && buttons==previous) { unlock(flags); return; }
    /* Never coalesce movement into a button edge: its click position matters. */
    if (count && previous==buttons) {
        struct mouse_event *last=&queue[(head+count-1)%MOUSE_QUEUE_SIZE];
        bool wheel=wx || wy,prior_wheel=last->wheel_x || last->wheel_y;
        bool same_direction=(!wx || !last->wheel_x || (wx>0)==(last->wheel_x>0)) &&
            (!wy || !last->wheel_y || (wy>0)==(last->wheel_y>0));
        if (last->buttons==buttons && !last->reserved && !(last->flags&MOUSE_CANCEL) &&
            wheel==prior_wheel && (!wheel || same_direction)) {
            last->dx=sum(last->dx,dx); last->dy=sum(last->dy,dy);
            last->wheel_x=sum(last->wheel_x,wx); last->wheel_y=sum(last->wheel_y,wy);
            unlock(flags); return;
        }
    }
    if (count==MOUSE_QUEUE_SIZE) {
        /* Drop the ambiguous gesture, release all buttons and require a real
         * release before accepting another press from either source. */
        overflows++;
        head=0; count=1;
        blocked[0]=source_buttons[0]; blocked[1]=source_buttons[1];
        queue[0]=(struct mouse_event){.flags=MOUSE_CANCEL};
        unlock(flags); return;
    }
    queue[(head+count)%MOUSE_QUEUE_SIZE]=(struct mouse_event){
        .dx=dx,.dy=dy,.wheel_x=wx,.wheel_y=wy,.buttons=buttons,.flags=buttons,
        .reserved=previous!=buttons}; /* Internal edge marker, stripped on read. */
    count++;
    unlock(flags);
}
void mouse_inject_event(int32_t dx,int32_t dy,uint8_t buttons) { inject(0,dx,dy,0,0,buttons); }
void mouse_inject_trackpad(int32_t dx,int32_t dy,uint8_t buttons) { inject(1,dx,dy,0,0,buttons); }
bool mouse_scan(struct mouse_event *out) {
    if (!out) return false;
    uint64_t flags=lock();
    if (!count) { unlock(flags); return false; }
    *out=queue[head]; out->reserved=0;
    head=(head+1)%MOUSE_QUEUE_SIZE; count--;
    unlock(flags); return true;
}

uint64_t mouse_overflow_count(void) {
    uint64_t flags=lock(),value=overflows;
    unlock(flags); return value;
}

void mouse_inject_trackpad_scroll(int32_t dx,int32_t dy,int32_t wx,int32_t wy,uint8_t buttons) {
    inject(1,dx,dy,wx,wy,buttons);
}

/* Only the owning display server changes these. Fixed mapping outside desktop. */
static unsigned trackpad_speed=3;
static unsigned trackpad_options;
void mouse_trackpad_settings(unsigned speed,unsigned options) {
    uint64_t flags=lock();trackpad_speed=speed;trackpad_options=options;unlock(flags);
}
void mouse_trackpad_get_settings(unsigned *speed,unsigned *options) {
    uint64_t flags=lock();*speed=trackpad_speed;*options=trackpad_options;unlock(flags);
}
/* Keep both edges at the same position, even when userspace is busy. Do not
 * combine a tap with a held external-mouse button or an overflow-blocked drag. */
void mouse_inject_trackpad_tap(uint8_t button) {
    uint64_t flags=lock();
    if((button==1 || button==2) && !(source_buttons[0]|source_buttons[1]|blocked[0]|blocked[1])) {
        inject(1,0,0,0,0,button);inject(1,0,0,0,0,0);
    }
    unlock(flags);
}

void mouse_cancel_trackpad(void) {
    uint64_t flags=lock();
    if(source_buttons[1]) {
        source_buttons[1]=blocked[1]=0;
        head=0;count=1;
        uint8_t buttons=source_buttons[0]&~blocked[0];
        queue[0]=(struct mouse_event){.buttons=buttons,.flags=MOUSE_CANCEL|buttons};
    }
    unlock(flags);
}
