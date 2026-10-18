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
        draw(&canvas,"AaZz09",0,&bar,&button); uint32_t hash=0;
        for (int i=0;i<280*h;i++) hash=hash*33+pixels[i+1];
        desk_scrollbar_set(&bar,120);
        draw(&canvas,"AaZz09",0,&bar,&button); uint32_t scrolled=0;
        for (int i=0;i<280*h;i++) scrolled=scrolled*33+pixels[i+1];
        assert(hash!=scrolled && pixels[0]==0x1234 && pixels[280*h+1]==0x1234);
        desk_scrollbar_set(&bar,INT64_MAX);
        draw(&canvas,"AaZz09",1,&bar,&button);
        assert(pixels[0]==0x1234 && pixels[280*h+1]==0x1234); free(pixels);
    }
    puts("deskclient: PASS protocol event adapter, shared control layout, scrolling/text-size drawing, viewport guards");
}
