#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_ui.h>
static struct srv_key_event key(unsigned usage) { return (struct srv_key_event){.usage=usage,.flags=SRV_KEY_DOWN_EVENT}; }
static struct desk_action click(struct desk_ui *ui,int x,int y,uint64_t time) {
    desk_ui_pointer(ui,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=x,.y=y,.down=1,.pressed=1},time);
    return desk_ui_pointer(ui,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=x,.y=y},time+1000);
}
int main(void) {
    struct desk_table t;desk_table_init(&t);
    struct desk_table_column cols[]={{"Name",0,0,1},{"PID",64,1,1},{"State",96,0,0}};
    struct desk_table_row rows[]={{9,{"Alpha","9","Ready"}},{3,{"Beta","3","Ready"}},{7,{"Gamma","7","Ready"}}};
    assert(!desk_table_columns(&t,cols,3));assert(!desk_table_rows(&t,rows,3));
    struct desk_ui ui;desk_ui_init(&ui);assert(!desk_ui_add(&ui,19,DESK_CONTROL_TABLE,&t,NULL));
    desk_table_configure(&t,(struct desk_rect){10,10,400,120},1);desk_ui_layout_changed(&ui);
    assert(t.body.focused);struct srv_key_event down=key(SRV_KEY_DOWN);
    assert(desk_ui_key(&ui,&down).flags&DESK_WIDGET_CHANGED);assert(t.body.selected==0);
    desk_ui_key(&ui,&down);assert(t.body.selected==1);
    struct desk_table_row reordered[]={rows[1],rows[2],rows[0]};
    assert(!desk_table_rows(&t,reordered,3));assert(t.body.selected==0 && t.navigation[0].id==3);
    struct desk_table_row duplicate[]={rows[0],rows[0]};assert(desk_table_rows(&t,duplicate,2)<0 && t.body.count==3);
    assert(desk_table_rows(&t,NULL,1)<0);assert(desk_table_rows(&t,rows,257)<0);
    struct desk_action a=click(&ui,260,22,100000);assert(a.id==19 && (a.flags&DESK_TABLE_SORT) && t.sort_column==1 && !t.descending);
    a=click(&ui,260,22,200000);assert((a.flags&DESK_TABLE_SORT) && t.descending);
    a=click(&ui,330,22,300000);assert(!(a.flags&DESK_TABLE_SORT));
    struct srv_key_event left=key(SRV_KEY_LEFT),enter=key(SRV_KEY_ENTER),f6=key(0x3f);
    desk_ui_key(&ui,&left);assert(t.header_column==1);assert(desk_ui_key(&ui,&enter).flags&DESK_TABLE_SORT);
    enter.flags|=SRV_KEY_REPEAT;assert(!(desk_ui_key(&ui,&enter).flags&DESK_TABLE_SORT));
    desk_ui_key(&ui,&f6);assert(!t.header_focus);desk_ui_key(&ui,&down);assert(t.body.selected==1);
    a=click(&ui,40,80,500000);assert(!(a.flags&DESK_WIDGET_ACTIVATED));
    a=click(&ui,40,80,600000);assert(a.flags&DESK_WIDGET_ACTIVATED);
    desk_ui_pointer(&ui,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=260,.y=22,.down=1,.pressed=1},0);
    desk_ui_window_focus(&ui,0);desk_ui_window_focus(&ui,1);
    a=desk_ui_pointer(&ui,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=260,.y=22},0);assert(!(a.flags&DESK_TABLE_SORT));
    struct desk_table_row many[100];for(int i=0;i<100;i++) many[i]=(struct desk_table_row){.id=i,.cells={"Long long name","999","Background"}};
    assert(!desk_table_rows(&t,many,100));assert(desk_ui_scroll(&ui,40,80,0,200).flags&DESK_WIDGET_REDRAW);
    assert(t.body.scroll.offset==200);desk_list_select(&t.body,80);assert(t.body.scroll.offset>200);
    assert(!desk_table_rows(&t,many,100));assert(t.body.selected==80 && t.body.scroll.offset>200);
    uint32_t pixels[420*140];for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) pixels[i]=0x12345678;
    struct desk_canvas c;assert(!desk_canvas_init(&c,pixels,420,140,420));struct desk_rect clip={20,20,350,100};desk_clip(&c,clip);
    desk_ui_draw(&c,&ui,&desk_theme_quiet);assert(!memcmp(&clip,&c.clip,sizeof(clip)));
    for(int y=0;y<140;y++) for(int x=0;x<420;x++) if(x<20 || x>=370 || y<20 || y>=120) assert(pixels[y*420+x]==0x12345678);
    desk_table_configure(&t,(struct desk_rect){2,2,12,8},1);desk_table_draw(&c,&t,&desk_theme_quiet);
    desk_table_configure(&t,(struct desk_rect){2,2,80,90},0);desk_ui_layout_changed(&ui);
    assert(!t.body.focused);assert(!desk_table_key(&t,&down));assert(!desk_table_scroll(&t,10));
    assert(!desk_table_rows(&t,NULL,0));assert(t.body.selected==-1);
    puts("table: PASS stable IDs, validation, heading sort/cancel, keyboard, rows, double-click, scroll, clipping and narrow geometry");
}
