#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_ui.h>
static struct srv_key_event key(unsigned usage,unsigned flags) {
    return (struct srv_key_event){.usage=usage,.flags=flags};
}
int main(void) {
    struct desk_check box={0},radio={0};struct desk_ui ui;
    desk_check_configure(&box,(struct desk_rect){10,10,120,28},1,0,0);
    desk_check_configure(&radio,(struct desk_rect){10,44,120,28},1,0,1);
    desk_ui_init(&ui);assert(!desk_ui_add(&ui,1,DESK_CONTROL_CHECK,&box,"Date"));
    assert(!desk_ui_add(&ui,2,DESK_CONTROL_CHECK,&radio,"24 hour"));
    desk_ui_layout_changed(&ui);assert(box.button.focused);
    struct srv_key_event space=key(SRV_KEY_SPACE,SRV_KEY_DOWN_EVENT);
    struct desk_action a=desk_ui_key(&ui,&space);assert(a.id==1 && (a.flags&DESK_WIDGET_CHANGED) && box.checked);
    a=desk_ui_key(&ui,&space);assert((a.flags&DESK_WIDGET_CHANGED) && !box.checked);
    space.flags|=SRV_KEY_REPEAT;a=desk_ui_key(&ui,&space);assert(!(a.flags&DESK_WIDGET_ACTIVATED));space.flags&=~SRV_KEY_REPEAT;
    struct srv_key_event tab=key(SRV_KEY_TAB,SRV_KEY_DOWN_EVENT);desk_ui_key(&ui,&tab);assert(radio.button.focused);
    a=desk_ui_key(&ui,&space);assert((a.flags&DESK_WIDGET_CHANGED) && radio.checked);
    a=desk_ui_key(&ui,&space);assert((a.flags&DESK_WIDGET_ACTIVATED) && !(a.flags&DESK_WIDGET_CHANGED) && radio.checked);
    struct desk_widget_event down={.type=DESK_WIDGET_POINTER,.x=120,.y=24,.down=1,.pressed=1};
    desk_ui_pointer(&ui,down,0);assert(box.button.armed);
    struct desk_widget_event up=down;up.down=up.pressed=0;
    a=desk_ui_pointer(&ui,up,0);assert((a.flags&DESK_WIDGET_CHANGED) && box.checked);
    desk_ui_pointer(&ui,down,0);up.x=140;a=desk_ui_pointer(&ui,up,0);assert(!(a.flags&DESK_WIDGET_ACTIVATED) && box.checked);
    desk_ui_pointer(&ui,down,0);desk_ui_window_focus(&ui,0);up.x=120;
    a=desk_ui_pointer(&ui,up,0);assert(!(a.flags&DESK_WIDGET_ACTIVATED) && !box.button.armed);
    desk_ui_window_focus(&ui,1);desk_ui_pointer(&ui,down,0);
    desk_check_configure(&box,box.button.frame,1,0,0);assert(!box.button.armed);desk_ui_pointer(&ui,up,0);assert(!box.checked);
    desk_ui_pointer(&ui,down,0);desk_check_configure(&box,box.button.frame,0,0,0);desk_ui_layout_changed(&ui);
    assert(radio.button.focused && !box.button.armed && !box.button.focused);
    uint32_t pixels[140*90];for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) pixels[i]=0x12345678;
    struct desk_canvas c;assert(!desk_canvas_init(&c,pixels,140,90,140));
    struct desk_rect clip={15,15,80,50};desk_clip(&c,clip);desk_ui_draw(&c,&ui,&desk_theme_quiet);
    assert(!memcmp(&c.clip,&clip,sizeof(clip)));
    for(int y=0;y<90;y++) for(int x=0;x<140;x++) if(x<15 || x>=95 || y<15 || y>=65) assert(pixels[y*140+x]==0x12345678);
    desk_check_configure(&box,(struct desk_rect){INT32_MAX-1,INT32_MIN,1,1},1,1,0);
    desk_check_draw(&c,&box,"tiny",&desk_theme_quiet);
    puts("check: PASS checkbox/radio state, keyboard repeat, label click, capture cancellation, disable, clipping and tiny geometry");
}
