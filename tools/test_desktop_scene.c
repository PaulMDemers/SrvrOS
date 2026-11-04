#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/deskd/scene.h"
#include <srvros/desktop_protocol.h>

enum { W=640,H=480,S=647 };
static uint32_t full[S*H],tiles[S*H],front[310*260];
static uint32_t other_front[310*260];
static struct desk_rect paint_bounds(struct desk_rect frame) {
    frame.w+=6;frame.h+=8;return frame;
}
static void draw_damage(struct desk_canvas *canvas,const struct desktop_scene *scene,struct desk_damage *damage) {
    int order[DESKD_WINDOWS];desktop_stack(scene,order);
    for(unsigned i=0;i<damage->count;i++) {
        desk_clip(canvas,damage->rects[i]);desktop_draw_ordered(canvas,scene,order,1);
    }
}
static void dynamic_damage(void) {
    for(unsigned style=0;style<3;style++) for(unsigned glass=0;glass<3;glass++) {
        for(unsigned i=0;i<310*260;i++) { front[i]=(i*37u)&0xffffff;other_front[i]=(i*139u)&0xffffff; }
        struct desktop_scene s={.focus=1,.cursor_x=500,.cursor_y=450};
        assert(!desk_theme_configure_style(&s.theme,style,0,glass,16));
        s.windows[0]=(struct desktop_window){.frame={35,55,312,305},.pixels=front,
            .width=310,.height=260,.visible=1,.title="Lower"};
        s.windows[1]=(struct desktop_window){.frame={145,115,302,305},.pixels=other_front,
            .width=300,.height=260,.visible=1,.title="Foreground"};
        struct desk_canvas fresh,partial;
        assert(!desk_canvas_init(&fresh,full,W,H,S));assert(!desk_canvas_init(&partial,tiles,W,H,S));
        for(unsigned i=0;i<S*H;i++)full[i]=tiles[i]=0xdeadbeef;
        desktop_draw(&partial,&s);
        struct desk_damage damage;desk_damage_init(&damage,W,H);
        desk_damage_add(&damage,paint_bounds(s.windows[1].frame));
        s.windows[1].frame.x+=55;s.windows[1].frame.y-=25;
        desk_damage_add(&damage,paint_bounds(s.windows[1].frame));
        draw_damage(&partial,&s,&damage);desktop_draw(&fresh,&s);
        assert(!memcmp(full,tiles,sizeof(full)));
        /* Redraw an unchanged translucent preview over changing content, then
         * switch and remove it. Each partial frame must equal a fresh frame. */
        s.snap_preview=DESKTOP_TILE_LEFT;s.snap_frame=(struct desk_rect){8,52,300,348};
        desk_damage_init(&damage,W,H);desk_damage_add(&damage,s.snap_frame);
        draw_damage(&partial,&s,&damage);desktop_draw(&fresh,&s);assert(!memcmp(full,tiles,sizeof(full)));
        for(unsigned i=0;i<310*260;i++)front[i]=(i*71u+33u)&0xffffff;
        desk_damage_init(&damage,W,H);desk_damage_add(&damage,paint_bounds(s.windows[0].frame));
        draw_damage(&partial,&s,&damage);desktop_draw(&fresh,&s);assert(!memcmp(full,tiles,sizeof(full)));
        desk_damage_init(&damage,W,H);desk_damage_add(&damage,s.snap_frame);
        s.snap_preview=DESKTOP_TILE_RIGHT;s.snap_frame=(struct desk_rect){332,52,300,348};desk_damage_add(&damage,s.snap_frame);
        draw_damage(&partial,&s,&damage);desktop_draw(&fresh,&s);assert(!memcmp(full,tiles,sizeof(full)));
        desk_damage_init(&damage,W,H);desk_damage_add(&damage,s.snap_frame);s.snap_preview=0;
        draw_damage(&partial,&s,&damage);desktop_draw(&fresh,&s);assert(!memcmp(full,tiles,sizeof(full)));
        /* A stationary pointer changing shape must repaint both old and new pixels. */
        const int shapes[]={1,2,4,8,5,6,9,10,0};
        for(unsigned k=0;k<sizeof(shapes)/sizeof(shapes[0]);k++) {
            desk_damage_init(&damage,W,H);
            desk_damage_add(&damage,(struct desk_rect){s.cursor_x-8,s.cursor_y-8,25,25});
            s.cursor_shape=shapes[k];draw_damage(&partial,&s,&damage);desktop_draw(&fresh,&s);
            assert(!memcmp(full,tiles,sizeof(full)));
        }
        /* Replace the lower window's cached pixels while foreground glass
         * overlaps it, as when the application scrolls and commits a frame. */
        for(unsigned i=0;i<310*260;i++)front[i]=(i*101u+0x123456u)&0xffffff;
        desk_damage_init(&damage,W,H);desk_damage_add(&damage,paint_bounds(s.windows[0].frame));
        draw_damage(&partial,&s,&damage);desktop_draw(&fresh,&s);
        assert(!memcmp(full,tiles,sizeof(full)));
    }
}
/* Physical-stride scene composition must remain identical for clipped redraws
 * while cached client frames independently transition between 1x and 2x. */
static void native_density_scene(void) {
    enum { PS=W*2+11, PH=H*2, GUARD=19, FW=320, FH=240 };
    const size_t count=(size_t)PS*PH, source_count=(size_t)FW*FH*4;
    uint32_t *a=malloc((count+2*GUARD)*sizeof(*a)),*b=malloc((count+2*GUARD)*sizeof(*b));
    uint32_t *source_a=malloc((source_count+2*GUARD)*sizeof(*source_a));
    uint32_t *source_b=malloc((source_count+2*GUARD)*sizeof(*source_b));
    assert(a && b && source_a && source_b);
    for(size_t i=0;i<source_count+2*GUARD;i++)source_a[i]=source_b[i]=0xdeadbeef;
    for(size_t i=0;i<source_count;i++) {
        source_a[GUARD+i]=(uint32_t)(0x100000u | ((i*37u)&0x0fffffu));
        source_b[GUARD+i]=(uint32_t)(0x800000u | ((i*91u)&0x0fffffu));
    }
    for(unsigned style=0;style<3;style++)for(unsigned mixed=0;mixed<3;mixed++) {
        unsigned lower_scale=mixed==1?1:2,upper_scale=mixed==2?1:2;
        struct desktop_scene s={.focus=1,.cursor_x=600,.cursor_y=450,.cursor_shape=5,
            .snap_preview=DESKTOP_TILE_RIGHT,.snap_frame={330,52,300,348}};
        assert(!desk_theme_configure_style(&s.theme,style,1,2,16));
        s.windows[0]=(struct desktop_window){.frame={20,60,322,285},.pixels=source_a+GUARD,
            .width=FW,.height=FH,.scale=lower_scale,.visible=1,.title="Lower"};
        s.windows[1]=(struct desktop_window){.frame={200,160,322,285},.pixels=source_b+GUARD,
            .width=FW,.height=FH,.scale=upper_scale,.visible=1,.title="Upper"};
        for(size_t i=0;i<count+2*GUARD;i++)a[i]=b[i]=0xdeadbeef;
        struct desk_canvas full2,tiled2;
        assert(!desk_canvas_init_scaled(&full2,a+GUARD,W,H,PS,2));
        assert(!desk_canvas_init_scaled(&tiled2,b+GUARD,W,H,PS,2));
        desk_clip(&full2,(struct desk_rect){0,0,W,H});desktop_draw(&full2,&s);
        for(int y=0;y<H;y+=29)for(int x=0;x<W;x+=31) {
            desk_clip(&tiled2,(struct desk_rect){x,y,31,29});desktop_draw(&tiled2,&s);
        }
        assert(!memcmp(a,b,(count+2*GUARD)*sizeof(*a)));
        /* Two unoccluded samples verify native source pixels and nearest 1x
         * expansion, rather than merely comparing two identical draw paths. */
        for(unsigned dy=0;dy<2;dy++)for(unsigned dx=0;dx<2;dx++) {
            unsigned lx=50,ly=120,ux=250,uy=220;
            size_t li=((ly-104)*lower_scale+dy*lower_scale/2)*(FW*lower_scale)+(lx-21)*lower_scale+dx*lower_scale/2;
            size_t ui=((uy-204)*upper_scale+dy*upper_scale/2)*(FW*upper_scale)+(ux-201)*upper_scale+dx*upper_scale/2;
            assert(a[GUARD+(ly*2+dy)*PS+lx*2+dx]==source_a[GUARD+li]);
            assert(a[GUARD+(uy*2+dy)*PS+ux*2+dx]==source_b[GUARD+ui]);
        }
        for(size_t i=0;i<GUARD;i++) {
            assert(a[i]==0xdeadbeef && a[GUARD+count+i]==0xdeadbeef);
            assert(source_a[i]==0xdeadbeef && source_a[GUARD+source_count+i]==0xdeadbeef);
            assert(source_b[i]==0xdeadbeef && source_b[GUARD+source_count+i]==0xdeadbeef);
        }
        for(int y=0;y<PH;y++)for(int x=W*2;x<PS;x++)assert(a[GUARD+(size_t)y*PS+x]==0xdeadbeef);
        assert(full2.pixel_writes && full2.pixel_writes<(uint64_t)W*H*4*100);
        assert(tiled2.pixel_writes && tiled2.pixel_writes<(uint64_t)W*H*4*100);
        struct desk_damage damage;desk_damage_init(&damage,W,H);
        desk_damage_add(&damage,paint_bounds(s.windows[1].frame));
        s.windows[1].frame.x-=43;s.windows[1].frame.y-=21;
        desk_damage_add(&damage,paint_bounds(s.windows[1].frame));
        draw_damage(&tiled2,&s,&damage);
        desk_clip(&full2,(struct desk_rect){0,0,W,H});desktop_draw(&full2,&s);
        assert(!memcmp(a,b,(count+2*GUARD)*sizeof(*a)));
    }
    free(source_b);free(source_a);free(b);free(a);
    puts("desktop-scene-density: PASS 2x tiled and partial redraw, mixed fronts, physical stride guards");
}
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
        desk_theme_configure_style(&s.theme,(unsigned)(pass%3),(unsigned)(pass%3),(unsigned)(pass%3),16);
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
    assert(desktop_snap_trigger(1280,800,0,52)==DESKTOP_MAXIMIZE_PLACEMENT);
    assert(desktop_snap_trigger(1280,800,7,53)==DESKTOP_TILE_LEFT);
    assert(desktop_snap_trigger(1280,800,1272,53)==DESKTOP_TILE_RIGHT);
    assert(!desktop_snap_trigger(1280,800,8,53));
    assert(!desktop_snap_trigger(1280,800,0,720));
    assert(desktop_window_placement(1280,800,original,DESKTOP_MAXIMIZE_PLACEMENT,&center));
    assert(center.x==8 && center.y==52 && center.w==1264 && center.h==668);
    struct desk_rect moved;
    assert(desktop_drag_restore(1280,800,center,original,632,20,640,200,&moved));
    assert(moved.x==279 && moved.y==180 && moved.w==722 && moved.h==525);
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
    struct desk_rect resized,base={100,100,500,400};
    const unsigned directions[]={1,2,4,8,5,6,9,10};
    const int points[][2]={{99,200},{600,200},{300,99},{300,500},{99,99},{600,99},{99,500},{600,500}};
    for(unsigned i=0;i<8;i++) {
        unsigned e=directions[i];
        assert(desktop_resize_hit(base,points[i][0],points[i][1])==e);
        assert(desktop_window_resize(1280,800,base,e,0,0,&resized));
        assert(!memcmp(&base,&resized,sizeof(base)));
        assert(desktop_window_resize(1280,800,base,e,35,30,&resized));
        if(e&1)assert(resized.x+resized.w==base.x+base.w);
        if(e&2)assert(resized.x==base.x);
        if(e&4)assert(resized.y+resized.h==base.y+base.h);
        if(e&8)assert(resized.y==base.y);
        assert(desktop_window_resize(1280,800,base,e,100000,-100000,&resized));
        assert(resized.w>=282 && resized.h>=285 && resized.w<=DESK_MAX_WIDTH+2 && resized.h<=DESK_MAX_HEIGHT+45);
    }
    assert(desktop_window_resize(1280,800,base,5,INT64_MIN,INT64_MAX,&resized));
    assert(resized.x+resized.w==600 && resized.y+resized.h==500 && resized.w<=DESK_MAX_WIDTH+2 && resized.h>=285);
    struct desk_rect offscreen={-400,100,600,400};
    assert(desktop_window_resize(1280,800,offscreen,2,-1000,0,&resized));
    assert(resized.x+resized.w>=160);
    assert(!desktop_resize_hit(base,599,499)); /* client never becomes a resize handle */
    assert(!desktop_resize_hit(base,50,50));
    resized=unchanged;assert(!desktop_window_resize(1280,800,base,3,0,0,&resized));
    assert(!memcmp(&resized,&unchanged,sizeof(resized)));
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
    dynamic_damage();
    native_density_scene();
    puts("desktop-scene: PASS overlap, clipping, dynamic drag/scroll damage, stale-size cache, tiled/full equality, stride guards and bounded placement");
    return 0;
}
