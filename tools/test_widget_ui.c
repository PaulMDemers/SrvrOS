#include <srvros/desk_ui.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct srv_key_event key(unsigned usage,unsigned mods) {
    return (struct srv_key_event){.usage=usage,.modifiers=mods,.flags=SRV_KEY_DOWN_EVENT};
}
int main(void) {
    char data[64],undo[64];struct desk_text_buffer text;
    desk_text_buffer_init(&text,data,undo,64);
    struct desk_edit edit;desk_edit_init(&edit,&text,0);
    desk_edit_configure(&edit,(struct desk_rect){10,10,200,32},&desk_font_16,1);
    struct desk_button button={0};desk_button_configure(&button,(struct desk_rect){10,50,100,28},1);
    struct desk_button disabled={0};desk_button_configure(&disabled,(struct desk_rect){120,50,100,28},0);
    struct desk_list list;desk_list_init(&list);
    struct desk_list_item items[]={{"Apple","Folder",1,1},{"Beta","4 bytes",2,0},{"Gamma","5 bytes",3,0},{"Delta","6 bytes",4,0}};
    assert(!desk_list_set_items(&list,items,4));desk_list_configure(&list,(struct desk_rect){10,90,200,60},1);
    struct desk_ui ui;desk_ui_init(&ui);
    assert(!desk_ui_add(&ui,1,DESK_CONTROL_EDIT,&edit,NULL));
    assert(!desk_ui_add(&ui,2,DESK_CONTROL_BUTTON,&button,"Open"));
    assert(!desk_ui_add(&ui,3,DESK_CONTROL_BUTTON,&disabled,"Disabled"));
    assert(!desk_ui_add(&ui,4,DESK_CONTROL_LIST,&list,NULL));
    assert(desk_ui_add(&ui,1,DESK_CONTROL_LIST,&list,NULL)==-1);
    desk_ui_layout_changed(&ui);assert(ui.focus==0 && edit.focused);
    struct srv_key_event tab=key(SRV_KEY_TAB,0),backtab=key(SRV_KEY_TAB,SRV_KEY_SHIFT),enter=key(SRV_KEY_ENTER,0);
    desk_ui_key(&ui,&tab);assert(ui.focus==1 && button.focused && !edit.focused);
    struct desk_action a=desk_ui_key(&ui,&enter);assert(a.id==2 && (a.flags&DESK_WIDGET_ACTIVATED));
    desk_ui_key(&ui,&tab);assert(ui.focus==3 && list.focused);
    desk_ui_key(&ui,&tab);assert(ui.focus==0);
    desk_ui_key(&ui,&backtab);assert(ui.focus==3);
    struct srv_key_event down=key(SRV_KEY_DOWN,0),end=key(SRV_KEY_END,0);
    a=desk_ui_key(&ui,&down);assert(a.id==4 && (a.flags&DESK_WIDGET_CHANGED) && list.selected==0);
    desk_ui_key(&ui,&end);assert(list.selected==3 && list.scroll.offset>0);
    assert(desk_ui_key(&ui,&enter).flags&DESK_WIDGET_ACTIVATED);
    struct srv_key_event letter=key(4,0);letter.codepoint='A';desk_ui_key(&ui,&letter);assert(list.selected==0);
    /* Double click uses caller timestamps and the same row, never a stale press. */
    struct desk_widget_event p={.type=DESK_WIDGET_POINTER,.x=30,.y=100,.down=1,.pressed=1};
    desk_ui_pointer(&ui,p,100);p.down=p.pressed=0;
    assert(!(desk_ui_pointer(&ui,p,200).flags&DESK_WIDGET_ACTIVATED));
    p.down=p.pressed=1;desk_ui_pointer(&ui,p,1000);p.down=p.pressed=0;
    assert(desk_ui_pointer(&ui,p,2000).flags&DESK_WIDGET_ACTIVATED);
    /* Tab cancels a captured button, including when the pointer remains held. */
    p.x=30;p.y=60;p.down=p.pressed=1;desk_ui_pointer(&ui,p,3000);assert(button.armed && ui.capture==1);
    desk_ui_key(&ui,&tab);assert(!button.armed && ui.capture==-1 && ui.focus==3);
    p.down=p.pressed=0;assert(!(desk_ui_pointer(&ui,p,4000).flags&DESK_WIDGET_ACTIVATED));
    desk_ui_focus(&ui,1);desk_ui_window_focus(&ui,0);assert(!edit.focused);
    assert(!desk_ui_key(&ui,&letter).flags && !text.length);
    desk_ui_window_focus(&ui,1);assert(edit.focused);
    letter.codepoint='a';desk_ui_key(&ui,&letter);assert(!strcmp(text.data,"a"));
    desk_edit_configure(&edit,edit.frame,edit.font,0);desk_ui_layout_changed(&ui);assert(ui.focus==1);
    /* Wheel scroll follows the hit control without changing keyboard focus. */
    desk_ui_scroll(&ui,30,100,0,50);assert(ui.focus==1 && list.scroll.offset>0);
    assert(desk_list_set_items(&list,items,DESK_LIST_MAX_ITEMS+1)==-1 && list.count==4);
    assert(!desk_list_set_items(&list,NULL,0) && list.selected==-1 && !list.scroll.offset);
    assert(!desk_list_key(&list,&enter));
    struct desk_list_item names[]={{.label="Desktop"},{.label="Denim"},{.label="Delta"},{.label="Alpha"}};
    assert(!desk_list_set_items(&list,names,4));list.focused=1;
    letter.time_us=100;letter.codepoint='d';desk_list_key(&list,&letter);assert(list.selected==0);
    letter.time_us=200;letter.codepoint='e';desk_list_key(&list,&letter);assert(list.selected==0);
    letter.time_us=300;letter.codepoint='n';desk_list_key(&list,&letter);assert(list.selected==1);
    letter.time_us=1000000;letter.codepoint='d';desk_list_key(&list,&letter);assert(list.selected==2);
    letter.time_us=1000100;desk_list_key(&list,&letter);assert(list.selected==0);
    desk_list_event(&list,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_OUT},0);assert(!list.typeahead_length);
    /* Composite rendering preserves every damage clip. */
    uint32_t full[240*180],tiled[240*180];for(unsigned i=0;i<240*180;i++) full[i]=tiled[i]=0x12345678;
    struct desk_canvas c;desk_canvas_init(&c,full,240,180,240);desk_ui_draw(&c,&ui,&desk_theme_quiet);
    desk_canvas_init(&c,tiled,240,180,240);
    for(int y=0;y<180;y+=13) for(int x=0;x<240;x+=17) {
        desk_clip(&c,(struct desk_rect){x,y,17,13});struct desk_rect clip=c.clip;
        desk_ui_draw(&c,&ui,&desk_theme_quiet);assert(!memcmp(&clip,&c.clip,sizeof(clip)));
    }
    assert(!memcmp(full,tiled,sizeof(full)));
    puts("widget-ui: PASS focus traversal/restore, disabled skipping, capture cancellation, list navigation/double-click/scroll and composite clipping");
}
