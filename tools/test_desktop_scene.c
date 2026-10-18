#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/deskd/scene.h"

enum { W=640,H=480,S=647 };
static uint32_t full[S*H],tiles[S*H],front[310*260];
int main(void) {
    for (unsigned i=0;i<310*260;i++) front[i]=i;
    struct desktop_scene s={.focus=1,.cursor_x=195,.cursor_y=180};
    s.windows[0]=(struct desktop_window){.frame={-5,45,500,350},.pixels=front,
        .width=310,.height=260,.visible=1,.title="Behind"};
    s.windows[1]=(struct desktop_window){.frame={170,120,300,285},.pixels=front,
        .width=310,.height=260,.visible=1,.title="In front"};
    struct desk_canvas a,b;
    assert(desk_canvas_init(&a,full,W,H,S)==0);
    assert(desk_canvas_init(&b,tiles,W,H,S)==0);
    for (int pass=0;pass<6;pass++) {
        if(pass==3) {
            s.apps=1;desk_menu_init(&s.menu);
            const struct desk_menu_item menu[]={ {0,DESK_MENU_HEADING,"SrvrOS",""},{1,0,"Settings",""},
                {0,DESK_MENU_HEADING,"File",""},{2,0,"New document","Ctrl-N"},{3,DESK_MENU_DISABLED,"Save","Ctrl-S"} };
            assert(!desk_menu_configure(&s.menu,menu,5,(struct desk_rect){10,0,450,44},(struct desk_rect){0,0,W,H},&desk_font_16));
            struct srv_key_event k={.usage=SRV_KEY_F10,.flags=SRV_KEY_DOWN_EVENT};desk_menu_key(&s.menu,&k);
        }
        for (unsigned i=0;i<S*H;i++) full[i]=tiles[i]=0xdeadbeef;
        desk_clip(&a,(struct desk_rect){0,0,W,H}); desktop_draw(&a,&s);
        for (int y=0;y<H;y+=29) for (int x=0;x<W;x+=31) {
            desk_clip(&b,(struct desk_rect){x,y,31,29}); desktop_draw(&b,&s);
        }
        assert(memcmp(full,tiles,sizeof(full))==0);
        for (int y=0;y<H;y++) for (int x=W;x<S;x++) assert(full[y*S+x]==0xdeadbeef);
        s.focus=0; s.windows[1].frame.x+=250; s.windows[1].frame.y+=150;
    }
    puts("desktop-scene: PASS overlap, clipping, stale-size cache, focus, tiled/full equality, stride guards");
    return 0;
}
