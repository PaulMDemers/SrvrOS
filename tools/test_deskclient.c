#include <assert.h>
#include <stdlib.h>
#define main deskclient_guest_main
#include "../userspace/deskclient/deskclient.c"
#undef main
int main(void) {
    struct scrollbar_drag drag={0};
    struct desk_packet p={.type=DESK_POINTER,.value=DESK_POINTER_LEFT|DESK_POINTER_PRESS};
    int offset=0;
    struct scrollbar bar=scrollbar_geometry(600,320,offset);
    p.x=bar.track.x+1; p.y=bar.thumb.y+7; /* Generous hit area beside visible thumb. */
    scrollbar_pointer(&drag,600,320,&offset,&p);
    assert(drag.active && offset==0);
    p.value=DESK_POINTER_LEFT; p.x=-500; p.y=INT32_MAX;
    scrollbar_pointer(&drag,600,320,&offset,&p); assert(offset==scroll_limit(320));
    p.y=INT32_MIN; scrollbar_pointer(&drag,600,320,&offset,&p); assert(offset==0);
    p.value=0; scrollbar_pointer(&drag,600,320,&offset,&p); assert(!drag.active);
    p.value=DESK_POINTER_LEFT; p.y=200;
    scrollbar_pointer(&drag,600,320,&offset,&p); assert(offset==0); /* No capture after release. */
    p.value=DESK_POINTER_LEFT|DESK_POINTER_PRESS; p.x=bar.track.x; p.y=bar.track.y+bar.track.h-1;
    scrollbar_pointer(&drag,600,320,&offset,&p); assert(offset==bar.track.h && !drag.active);
    p.y=bar.track.y; scrollbar_pointer(&drag,600,320,&offset,&p); assert(offset==0);
    p.y=bar.thumb.y+7; scrollbar_pointer(&drag,600,320,&offset,&p); assert(drag.active);
    p.value=DESK_POINTER_CANCEL; scrollbar_pointer(&drag,600,320,&offset,&p); assert(!drag.active);
    p.value=DESK_POINTER_LEFT; p.y=200;
    scrollbar_pointer(&drag,600,320,&offset,&p); assert(offset==0);
    p.value=DESK_POINTER_LEFT|DESK_POINTER_PRESS; p.y=bar.thumb.y+7;
    scrollbar_pointer(&drag,600,320,&offset,&p); assert(drag.active);
    p.value=0; p.y=INT32_MAX;
    scrollbar_pointer(&drag,600,320,&offset,&p);
    assert(!drag.active && offset==scroll_limit(320)); /* Release carries final position. */
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
    puts("deskclient: PASS thumb grab, outside drag, clamp, page clicks, release/cancel, scroll bounds, viewport guards");
}
