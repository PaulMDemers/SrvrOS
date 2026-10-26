#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/deskd/scene.h"
#include <srvros/desktop_protocol.h>

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
    for (int pass=0;pass<7;pass++) {
        if(pass==3) {
            s.apps=1;s.windows[0].application=DESK_APP_TEXT;s.windows[1].application=DESK_APP_PREVIEW;desk_menu_init(&s.menu);
            const struct desk_menu_item menu[]={ {0,DESK_MENU_HEADING,"SrvrOS",""},{1,0,"Settings",""},
                {0,DESK_MENU_HEADING,"File",""},{2,0,"New document","Ctrl-N"},{3,DESK_MENU_DISABLED,"Save","Ctrl-S"} };
            assert(!desk_menu_configure(&s.menu,menu,5,(struct desk_rect){10,0,450,44},(struct desk_rect){0,0,W,H},&desk_font_16));
            struct srv_key_event k={.usage=SRV_KEY_F10,.flags=SRV_KEY_DOWN_EVENT};desk_menu_key(&s.menu,&k);
        }
        if(pass==4) {
            desk_palette_init(&s.launcher);struct desk_palette_item entries[]={{1,"Files","Application"},{5000000000ULL,"Text — Notes","Window"}};
            assert(!desk_palette_open(&s.launcher,"Open application or window",entries,2));
            desk_palette_configure(&s.launcher,(struct desk_rect){0,44,W,H-120});s.launcher_open=1;
        }
        if(pass==5) {
            s.launcher_open=0;s.calendar_open=1;s.calendar_frame=(struct desk_rect){268,52,360,416};
            desk_calendar_init(&s.calendar);desk_calendar_select(&s.calendar,(struct desk_date){2026,10,18});desk_calendar_today(&s.calendar,s.calendar.selected);
            desk_calendar_configure(&s.calendar,(struct desk_rect){284,100,328,272},1,0);desk_ui_init(&s.calendar_ui);
            desk_ui_add(&s.calendar_ui,1,DESK_CONTROL_CALENDAR,&s.calendar,NULL);desk_ui_focus(&s.calendar_ui,1);
            strcpy(s.calendar_date,"Sunday · 2026-10-18");strcpy(s.calendar_zone,"UTC");strcpy(s.clock,"12:34 UTC");
        }
        if(pass==6) {
            s.calendar_open=0;s.windows[0].minimized=1;desk_menu_cancel(&s.menu);desk_popup_init(&s.context);
            const struct desk_menu_item items[]={{1,0,"Open Files",""},{2,0,"Settings",""},{3,0,"SrvrOS Help","F1"}};
            assert(!desk_popup_open(&s.context,items,3,630,470,(struct desk_rect){0,44,W,H-124},&desk_font_16,1));
        }
        for (unsigned i=0;i<S*H;i++) full[i]=tiles[i]=0xdeadbeef;
        int order[DESKD_WINDOWS];desktop_stack(&s,order);
        desk_clip(&a,(struct desk_rect){0,0,W,H}); desktop_draw_ordered(&a,&s,order,0);
        for (int y=0;y<H;y+=29) for (int x=0;x<W;x+=31) {
            desk_clip(&b,(struct desk_rect){x,y,31,29}); desktop_draw(&b,&s);
        }
        assert(memcmp(full,tiles,sizeof(full))==0);
        for (int y=0;y<H;y++) for (int x=W;x<S;x++) assert(full[y*S+x]==0xdeadbeef);
        s.focus=0; s.windows[1].frame.x+=250; s.windows[1].frame.y+=150;
    }
    struct desk_rect frame={32,64,562,365};
    for(unsigned control=DESKTOP_CLOSE;control<=DESKTOP_MINIMIZE;control++) {
        struct desk_rect r=desktop_window_control_rect(frame,control);
        assert(desktop_titlebar_hit(frame,1,r.x+16,r.y+21)==control);
        assert(desktop_titlebar_hit(frame,0,r.x+16,r.y+21)==(control==DESKTOP_CLOSE?control:0));
        assert(!desktop_titlebar_hit(frame,1,r.x+16,r.y+r.h));
    }
    assert(!desktop_titlebar_hit(frame,1,frame.x+frame.w-1,frame.y+20));
    struct desk_rect left,right,center,original={32,64,722,525},unchanged={1,2,3,4};
    for(int width=200;width<4100;width+=37) for(int height=200;height<2200;height+=41) {
        left=unchanged;int possible=desktop_window_placement(width,height,original,DESKTOP_TILE_LEFT,&left);
        if(!possible) { assert(!memcmp(&left,&unchanged,sizeof(left)));continue; }
        assert(desktop_window_placement(width,height,original,DESKTOP_TILE_RIGHT,&right));
        assert(left.x>=8 && left.y>=52 && left.w>=282 && left.h>=285 && left.w<=DESK_MAX_WIDTH+2 && left.h<=DESK_MAX_HEIGHT+45);
        assert(left.x+left.w+8<=right.x && right.x+right.w<=width-8 && right.y+right.h<=height-80);
        assert(desktop_window_placement(width,height,original,DESKTOP_CENTER,&center));
        assert(center.x>=8 && center.y>=52 && center.x+center.w<=width-8 && center.y+center.h<=height-80);
    }
    assert(!desktop_window_placement(20000,800,original,DESKTOP_CENTER,&center));
    assert(!desktop_window_placement(800,400,original,DESKTOP_TILE_LEFT,&center));
    assert(!desktop_window_placement(800,600,original,0,&center));
    struct desk_rect moved;
    assert(desktop_window_drag(1280,800,original,-400,700,&moved));
    assert(moved.x==-400 && moved.y==688 && moved.w==original.w && moved.h==original.h);
    for(int x=-4000;x<=4000;x+=137)for(int y=-4000;y<=4000;y+=271) {
        assert(desktop_window_drag(1280,800,original,x,y,&moved));
        assert(moved.x+original.w>=160 && moved.x<=1120 && moved.y>=44 && moved.y<=688);
        assert(moved.w==original.w && moved.h==original.h);
    }
    assert(desktop_window_drag(1280,800,original,INT64_MIN,INT64_MAX,&moved));
    assert(moved.x==160-original.w && moved.y==688);
    moved=unchanged;assert(!desktop_window_drag(128,100,original,0,0,&moved));assert(!memcmp(&moved,&unchanged,sizeof(moved)));
    struct desktop_scene dock_scene={.focus=2};
    dock_scene.windows[0]=(struct desktop_window){.visible=1,.application=DESK_APP_TEXT,.minimized=1};
    dock_scene.windows[1]=(struct desktop_window){.visible=1,.application=DESK_APP_TEXT};
    dock_scene.windows[2]=(struct desktop_window){.visible=1,.application=DESK_APP_TEXT,.dialog=1};
    dock_scene.windows[3]=(struct desktop_window){.visible=1,.application=DESK_APP_PREVIEW,.minimized=1};
    dock_scene.windows[4]=(struct desktop_window){.visible=1,.application=DESK_APP_HELP};
    struct desktop_dock_entry entries[DESK_APP_COUNT];
    unsigned count=desktop_dock_entries(&dock_scene,1280,800,entries);assert(count==8);
    assert(entries[1].application==DESK_APP_TEXT && entries[1].windows==2 && entries[1].minimized==1 && entries[1].active);
    assert(entries[6].application==DESK_APP_PREVIEW && entries[6].windows==1 && entries[6].minimized==1);
    assert(entries[7].application==DESK_APP_HELP);
    for(unsigned i=0;i<count;i++) {
        struct desk_rect r=entries[i].frame;assert(r.x>=0 && r.x+r.w<=1280);
        assert(desktop_dock_hit(&dock_scene,1280,800,r.x+r.w/2,r.y+20)==entries[i].application);
        if(i)assert(entries[i-1].frame.x+entries[i-1].frame.w==r.x);
    }
    dock_scene.windows[3].visible=0;assert(desktop_dock_entries(&dock_scene,640,480,entries)==7);
    assert(entries[6].application==DESK_APP_HELP);
    for(unsigned app=1;app<=DESK_APP_COUNT;app++)dock_scene.windows[app-1]=(struct desktop_window){.visible=1,.application=app};
    assert(desktop_dock_entries(&dock_scene,640,480,entries)==DESK_APP_COUNT);
    assert(entries[0].frame.x>=8 && entries[DESK_APP_COUNT-1].frame.x+entries[DESK_APP_COUNT-1].frame.w<=632);
    puts("desktop-scene: PASS overlap, clipping, stale-size cache, focus, tiled/full equality, stride guards and bounded window placement");
    return 0;
}
