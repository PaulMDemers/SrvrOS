#include <assert.h>
#include <stdlib.h>
#define main deskclient_guest_main
#include "../userspace/deskclient/deskclient.c"
#undef main
int main(void) {
    struct desk_packet p={.type=DESK_POINTER,.value=DESK_POINTER_LEFT|DESK_POINTER_PRESS,.x=42,.y=-12};
    struct desk_widget_event event=pointer_event(&p);
    assert(event.type==DESK_WIDGET_POINTER && event.down && event.pressed && event.x==42 && event.y==-12);
    p.value=DESK_POINTER_CANCEL;
    assert(pointer_event(&p).type==DESK_WIDGET_CANCEL);
    for (int h=240;h<=640;h+=400) {
        uint32_t *pixels=malloc((size_t)(280*h+2)*4); assert(pixels);
        pixels[0]=pixels[280*h+1]=0x1234;
        struct desk_canvas canvas;
        assert(desk_canvas_init(&canvas,pixels+1,280,h,280)==0);
        struct desk_scrollbar bar={0}; struct desk_button button={0};
        layout(&bar,&button,280,h);
        draw(&canvas,"AaZz09",0,&bar,&button,0); uint32_t hash=0;
        for (int i=0;i<280*h;i++) hash=hash*33+pixels[i+1];
        desk_scrollbar_set(&bar,120);
        draw(&canvas,"AaZz09",0,&bar,&button,0); uint32_t scrolled=0;
        for (int i=0;i<280*h;i++) scrolled=scrolled*33+pixels[i+1];
        assert(hash!=scrolled && pixels[0]==0x1234 && pixels[280*h+1]==0x1234);
        desk_scrollbar_set(&bar,INT64_MAX);
        draw(&canvas,"AaZz09",1,&bar,&button,0);
        assert(pixels[0]==0x1234 && pixels[280*h+1]==0x1234); free(pixels);
    }
    /* A tap's ordered edges may arrive before any draw. Activation must be
     * immediate and unique; only its visual feedback survives the batch. */
    struct desk_button b={0};struct desk_scrollbar bar={0};
    layout(&bar,&b,280,240);
    struct desk_widget_event tap={.type=DESK_WIDGET_POINTER,.x=b.frame.x+3,.y=b.frame.y+3,.down=1,.pressed=1};
    assert(!(desk_button_event(&b,tap)&DESK_WIDGET_ACTIVATED));
    tap.down=tap.pressed=0;
    assert(desk_button_event(&b,tap)&DESK_WIDGET_ACTIVATED);
    assert(!b.armed && !(desk_button_event(&b,tap)&DESK_WIDGET_ACTIVATED));
    struct button_feedback f={.active=1};
    assert(!feedback_expire(&f,5000)); /* Busy surface cannot consume the pulse. */
    feedback_commit(&f,4);feedback_release(&f,3,6000);
    assert(!feedback_expire(&f,7000)); /* Stale release cannot expire it. */
    feedback_release(&f,4,7000);assert(!feedback_expire(&f,7009));
    uint32_t pixels[280*240];struct desk_canvas c;
    assert(desk_canvas_init(&c,pixels,280,240,280)==0);
    draw(&c,"",0,&bar,&b,f.active);
    assert(pixels[(b.frame.y+3)*280+b.frame.x+3]==desk_theme_quiet.selected);
    assert(!b.armed); /* Drawing the pulse cannot hold a logical drag. */
    assert(feedback_expire(&f,7010));assert(!feedback_expire(&f,7011));
    draw(&c,"",0,&bar,&b,f.active);
    assert(pixels[(b.frame.y+3)*280+b.frame.x+3]==desk_theme_quiet.surface);
    f=(struct button_feedback){.active=1};feedback_commit(&f,5);
    f=(struct button_feedback){.active=1}; /* New activation while a frame is pending. */
    feedback_release(&f,5,8000);assert(!f.until);
    feedback_commit(&f,6);feedback_release(&f,6,8000);assert(f.until==8010);
    f=(struct button_feedback){0}; /* Cancel/resize clears visual feedback. */
    assert(!feedback_expire(&f,9000));
    puts("deskclient: PASS protocol event adapter, shared control layout, scrolling/text-size drawing, viewport guards, batched tap feedback, frame-release expiry and repeat/cancel");
}
