#include <assert.h>
#include <stdlib.h>
#define main deskclient_guest_main
#include "../userspace/deskclient/deskclient.c"
#undef main
int main(void) {
    assert(scroll_clamp(INT64_MAX,320)==scroll_limit(320));
    assert(scroll_clamp(INT64_MIN,320)==0);
    assert(scroll_clamp(120,320)==120);
    for (int h=240;h<=640;h+=400) {
        uint32_t *pixels=malloc((size_t)(280*h+2)*4); assert(pixels);
        pixels[0]=pixels[280*h+1]=0x1234;
        struct desk_canvas canvas;
        assert(desk_canvas_init(&canvas,pixels+1,280,h,280)==0);
        draw(&canvas,"AaZz09",0,0); uint32_t hash=0;
        for (int i=0;i<280*h;i++) hash=hash*33+pixels[i+1];
        draw(&canvas,"AaZz09",0,120); uint32_t scrolled=0;
        for (int i=0;i<280*h;i++) scrolled=scrolled*33+pixels[i+1];
        assert(hash!=scrolled && pixels[0]==0x1234 && pixels[280*h+1]==0x1234);
        draw(&canvas,"AaZz09",1,scroll_limit(h));
        assert(pixels[0]==0x1234 && pixels[280*h+1]==0x1234); free(pixels);
    }
    puts("deskclient: PASS scroll bounds, resize limits, visible scroll change, viewport guards");
}
