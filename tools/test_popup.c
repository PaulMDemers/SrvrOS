#include <srvros/desk_popup.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
static struct desk_menu_result key(struct desk_popup *p,unsigned usage,unsigned cp) {
    struct srv_key_event k={.usage=usage,.codepoint=cp,.flags=SRV_KEY_DOWN_EVENT};return desk_popup_key(p,&k);
}
static struct desk_widget_event mouse(int x,int y,int down,int press) { return (struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=x,.y=y,.down=down,.pressed=press}; }
int main(void) {
    struct desk_popup p;desk_popup_init(&p);assert(!desk_popup_active(&p));
    struct desk_menu_item rows[32]={ {1,0,"Open","Enter"},{2,DESK_MENU_DISABLED,"Rename","F2"},{0,DESK_MENU_SEPARATOR,"",""},{3,DESK_MENU_CHECKED,"Copy path","Ctrl-C"},{4,0,"Delete","Del"} };
    struct desk_rect bounds={20,30,280,240};
    assert(!desk_popup_open(&p,rows,5,INT_MAX,INT_MIN,bounds,&desk_font_16,1));
    assert(p.frame.x>=20 && p.frame.y==30 && p.frame.x+p.frame.w<=300 && p.selected==0);
    rows[0].label[0]='X';assert(p.items[0].label[0]=='O');rows[0].label[0]='O';
    assert(key(&p,SRV_KEY_DOWN,0).redraw && p.selected==3);key(&p,SRV_KEY_UP,0);assert(p.selected==0);
    key(&p,SRV_KEY_END,0);assert(p.selected==4);key(&p,0,'c');assert(p.selected==3);
    assert(key(&p,SRV_KEY_ENTER,0).action==3 && !desk_popup_active(&p));
    assert(!desk_popup_open(&p,rows,5,25,35,bounds,&desk_font_16,0));
    struct desk_rect row=desk_popup_row(&p,0);
    assert(!desk_popup_pointer(&p,mouse(row.x+2,row.y+2,0,0)).action);
    desk_popup_pointer(&p,mouse(row.x+2,row.y+2,1,1));
    assert(desk_popup_pointer(&p,mouse(row.x+2,row.y+2,0,0)).action==1 && !desk_popup_active(&p));
    assert(!desk_popup_open(&p,rows,5,25,35,bounds,&desk_font_16,0));
    desk_popup_pointer(&p,mouse(0,0,1,1));assert(!p.opened && desk_popup_active(&p));
    assert(desk_popup_pointer(&p,mouse(0,0,1,0)).consumed);assert(desk_popup_pointer(&p,mouse(0,0,0,0)).consumed && !desk_popup_active(&p));
    assert(!desk_popup_open(&p,rows,5,25,35,bounds,&desk_font_16,0));
    row=desk_popup_row(&p,1);desk_popup_pointer(&p,mouse(row.x+2,row.y+2,1,1));assert(!desk_popup_pointer(&p,mouse(row.x+2,row.y+2,0,0)).action && p.opened);
    row=desk_popup_row(&p,0);desk_popup_pointer(&p,mouse(row.x+2,row.y+2,1,1));key(&p,SRV_KEY_ESCAPE,0);assert(p.swallow_release && !p.opened);desk_popup_pointer(&p,mouse(0,0,0,0));assert(!desk_popup_active(&p));
    for(unsigned i=0;i<32;i++) { rows[i]=(struct desk_menu_item){.id=i+1};snprintf(rows[i].label,sizeof(rows[i].label),"Item %u with a long name",i);snprintf(rows[i].shortcut,sizeof(rows[i].shortcut),"Ctrl-%u",i); }
    assert(!desk_popup_open(&p,rows,32,280,230,bounds,&desk_font_20,1));assert(desk_scrollbar_limit(&p.scroll)>0);
    key(&p,SRV_KEY_END,0);assert(p.selected==31 && p.scroll.offset>0);row=desk_popup_row(&p,31);assert(row.h==(int)p.font->line_height+10 && row.y+row.h<=p.frame.y+p.frame.h-4);
    key(&p,SRV_KEY_HOME,0);assert(p.selected==0 && p.scroll.offset==0);
    desk_popup_scroll(&p,INT64_MAX);assert(p.scroll.offset==desk_scrollbar_limit(&p.scroll));
    desk_popup_scroll(&p,INT64_MIN);assert(p.scroll.offset==0);
    struct desk_scrollbar_geometry geometry=desk_scrollbar_geometry(&p.scroll);int x=geometry.thumb.x+2,y=geometry.thumb.y+2;
    desk_popup_pointer(&p,mouse(x,y,1,1));assert(p.scroll_capture);desk_popup_pointer(&p,mouse(x,y+100,1,0));assert(p.scroll.offset>0);desk_popup_pointer(&p,mouse(x,y+100,0,0));assert(!p.scroll_capture && !p.armed);
    struct desk_popup saved=p;struct desk_menu_item bad[32];memcpy(bad,rows,sizeof(bad));bad[2].id=1;
    assert(desk_popup_open(&p,bad,32,0,0,bounds,&desk_font_16,0)<0 && !memcmp(&saved,&p,sizeof(p)));
    bad[2]=rows[2];bad[2].label[0]=(char)0xff;assert(desk_popup_open(&p,bad,32,0,0,bounds,&desk_font_16,0)<0);
    assert(desk_popup_open(&p,rows,33,0,0,bounds,&desk_font_16,0)<0);
    assert(desk_popup_open(&p,rows,32,0,0,(struct desk_rect){0,0,8,8},&desk_font_16,0)<0);
    static uint32_t full[400*320],tiled[400*320];
    for(unsigned font=0;font<2;font++) for(unsigned count=1;count<=32;count+=3) for(int width=16;width<=280;width+=33) {
        assert(!desk_popup_open(&p,rows,count,300,300,(struct desk_rect){20,30,width,240},font?&desk_font_20:&desk_font_16,1));
        if(count>10) key(&p,SRV_KEY_END,0);
        memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));struct desk_canvas a,b;desk_canvas_init(&a,full,360,300,400);desk_canvas_init(&b,tiled,360,300,400);desk_popup_draw(&a,&p,&desk_theme_quiet);
        for(int y=0;y<300;y+=37) for(int x=0;x<360;x+=43) { desk_clip(&b,(struct desk_rect){x,y,43,37});desk_popup_draw(&b,&p,&desk_theme_quiet); }
        assert(!memcmp(full,tiled,sizeof(full)));
        for(unsigned y=0;y<320;y++) for(unsigned x=0;x<400;x++) if(x>=360 || y>=300) assert(!full[y*400+x]);
    }
    puts("popup: PASS owned/atomic validation, bounded anchors, disabled/separators, keyboard/typeahead, release activation/cancellation, outside-release isolation, overflow reveal/wheel/drag and tiled rendering");
}
