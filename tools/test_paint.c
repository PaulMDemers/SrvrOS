#include <srvros/desk_ui.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    size_t bytes=desk_bitmap_storage_bytes(100,80,4);void *arena=malloc(bytes);assert(arena);struct desk_bitmap b;assert(!desk_bitmap_init(&b,arena,bytes,100,80,4,NULL));
    struct desk_paint p;desk_paint_init(&p);assert(!desk_paint_bind(&p,&b));desk_paint_configure(&p,(struct desk_rect){0,0,202,122},1);
    int32_t x=-1,y=-1;assert(desk_image_point(&p.view,51,21,0,&x,&y) && x==0 && y==0);
    assert(!desk_image_point(&p.view,0,0,0,&x,&y));assert(desk_image_point(&p.view,INT32_MAX,INT32_MIN,1,&x,&y) && x==99 && y==0);
    struct desk_ui ui;desk_ui_init(&ui);assert(!desk_ui_add(&ui,1,DESK_CONTROL_PAINT,&p,NULL));desk_ui_focus(&ui,1);
    desk_paint_tool(&p,DESK_PAINT_PEN,0xff0000,1);
    struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=61,.y=31,.down=1,.pressed=1};desk_ui_pointer(&ui,e,0);assert(p.drawing && b.active);
    e.x=81;e.pressed=0;desk_ui_pointer(&ui,e,1);e.down=0;struct desk_action a=desk_ui_pointer(&ui,e,2);assert((a.flags&DESK_WIDGET_ACTIVATED) && !p.drawing && !b.active && b.index==1);
    for(unsigned i=10;i<=30;i++) assert(b.pixels[10*100+i]==0xffff0000u);
    assert(desk_bitmap_undo(&b));for(unsigned i=0;i<8000;i++) assert(b.pixels[i]==0xffffffffu);
    desk_paint_tool(&p,DESK_PAINT_LINE,0,1);e=(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=61,.y=31,.down=1,.pressed=1};desk_ui_pointer(&ui,e,0);
    e.x=91;e.y=61;e.pressed=0;desk_ui_pointer(&ui,e,1);assert(b.pixels[40*100+40]==0xff000000u);
    e.y=31;desk_ui_pointer(&ui,e,2);assert(b.pixels[40*100+40]==0xffffffffu);e.down=0;desk_ui_pointer(&ui,e,3);assert(b.index==1);
    e.down=e.pressed=1;desk_ui_pointer(&ui,e,4);desk_ui_window_focus(&ui,0);assert(!p.drawing && !b.active && b.index==1);desk_ui_window_focus(&ui,1);
    desk_ui_pointer(&ui,e,5);desk_paint_configure(&p,(struct desk_rect){0,0,203,122},1);desk_ui_layout_changed(&ui);assert(!p.drawing && !b.active);
    desk_paint_tool(&p,DESK_PAINT_PAN,0,1);desk_image_zoom(&p.view,1);int old=p.view.offset_y;
    e=(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=80,.y=80,.down=1,.pressed=1};desk_ui_pointer(&ui,e,1);e.y=40;e.pressed=0;desk_ui_pointer(&ui,e,2);e.down=0;desk_ui_pointer(&ui,e,3);assert(p.view.offset_y>old && b.index==1);
    static uint32_t full[240*160],tiled[240*160];struct desk_canvas c,d;desk_canvas_init(&c,full,220,140,240);desk_canvas_init(&d,tiled,220,140,240);desk_paint_draw(&c,&p,&desk_theme_quiet);
    for(int yy=0;yy<140;yy+=19) for(int xx=0;xx<220;xx+=23) { desk_clip(&d,(struct desk_rect){xx,yy,23,19});desk_paint_draw(&d,&p,&desk_theme_quiet); }
    assert(!memcmp(full,tiled,sizeof(full)));free(arena);
    puts("paint: PASS centered/zoomed coordinates, continuous strokes, preview replacement, undo, focus/resize cancellation, pan-only input and tiled rendering");
}
