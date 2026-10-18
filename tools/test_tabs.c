#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_ui.h>
static struct srv_key_event key(unsigned usage,unsigned modifiers) { return (struct srv_key_event){.usage=usage,.modifiers=modifiers,.flags=SRV_KEY_DOWN_EVENT}; }
int main(void) {
    struct desk_tabs t;desk_tabs_init(&t);
    struct desk_tab items[]={{1,"Processes",0},{2,"Disabled",1},{3,"Network",0}};
    assert(!desk_tabs_items(&t,items,3));desk_tabs_configure(&t,(struct desk_rect){10,10,301,32},1);
    assert(t.enabled && desk_tabs_selected(&t)==1);
    assert(desk_tabs_select(&t,2)==-1 && desk_tabs_select(&t,99)==-1);
    struct desk_tabs saved=t;items[1].id=1;assert(desk_tabs_items(&t,items,3)==-1 && !memcmp(&saved,&t,sizeof(t)));
    items[1].id=2;strcpy(items[1].label,"bad\nlabel");assert(desk_tabs_items(&t,items,3)==-1);strcpy(items[1].label,"Disabled");
    assert(!desk_tabs_select(&t,1));assert(desk_tabs_select(&t,3)==1);
    struct desk_tab reorder[]={items[2],items[0],items[1]};assert(!desk_tabs_items(&t,reorder,3));assert(t.selected==0 && desk_tabs_selected(&t)==3);
    assert(!desk_tabs_items(&t,items,3));assert(t.selected==2);
    desk_tabs_event(&t,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});
    struct srv_key_event k=key(SRV_KEY_RIGHT,0);assert(desk_tabs_key(&t,&k)&DESK_WIDGET_CHANGED);assert(desk_tabs_selected(&t)==1);
    k.usage=SRV_KEY_LEFT;assert(desk_tabs_key(&t,&k)&DESK_WIDGET_CHANGED);assert(desk_tabs_selected(&t)==3);
    k.usage=SRV_KEY_HOME;desk_tabs_key(&t,&k);assert(desk_tabs_selected(&t)==1);k.usage=SRV_KEY_END;desk_tabs_key(&t,&k);assert(desk_tabs_selected(&t)==3);
    k=key(SRV_KEY_LEFT,SRV_KEY_CTRL);assert(!desk_tabs_key(&t,&k));
    k=key(SRV_KEY_ENTER,0);assert(desk_tabs_key(&t,&k)&DESK_WIDGET_ACTIVATED);k.flags|=SRV_KEY_REPEAT;assert(!desk_tabs_key(&t,&k));
    struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=30,.y=25,.pressed=1,.down=1};
    desk_tabs_event(&t,e);e.x=140;e.pressed=e.down=0;assert(!(desk_tabs_event(&t,e)&DESK_WIDGET_ACTIVATED));assert(desk_tabs_selected(&t)==3);
    e.x=30;e.pressed=e.down=1;desk_tabs_event(&t,e);e.pressed=e.down=0;assert(desk_tabs_event(&t,e)&DESK_WIDGET_ACTIVATED);assert(desk_tabs_selected(&t)==1);
    e.pressed=e.down=1;desk_tabs_event(&t,e);desk_tabs_event(&t,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_OUT});e.pressed=e.down=0;assert(!desk_tabs_event(&t,e));
    e.pressed=e.down=1;desk_tabs_event(&t,e);desk_tabs_configure(&t,(struct desk_rect){10,10,300,32},1);e.pressed=e.down=0;assert(!desk_tabs_event(&t,e));
    e.pressed=e.down=1;desk_tabs_event(&t,e);desk_tabs_event(&t,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});
    k=key(SRV_KEY_ENTER,0);assert(desk_tabs_key(&t,&k)&DESK_WIDGET_ACTIVATED);assert(t.pressed<0);e.pressed=e.down=0;assert(!desk_tabs_event(&t,e));
    saved=t;desk_tabs_configure(&t,(struct desk_rect){2147483647,0,100,30},1);assert(!memcmp(&saved,&t,sizeof(t)));
    /* Shared router owns one focus stop for the whole strip. */
    struct desk_ui ui;struct desk_button b={0};desk_ui_init(&ui);desk_button_configure(&b,(struct desk_rect){10,60,100,28},1);
    assert(!desk_ui_add(&ui,11,DESK_CONTROL_TABS,&t,NULL));assert(!desk_ui_add(&ui,12,DESK_CONTROL_BUTTON,&b,"Next"));desk_ui_focus(&ui,11);
    k=key(SRV_KEY_RIGHT,0);struct desk_action action=desk_ui_key(&ui,&k);assert(action.id==11 && action.flags&DESK_WIDGET_CHANGED && desk_tabs_selected(&t)==3);
    k.usage=SRV_KEY_TAB;desk_ui_key(&ui,&k);assert(b.focused && !t.focused);k.modifiers=SRV_KEY_SHIFT;desk_ui_key(&ui,&k);assert(t.focused && !b.focused);
    static uint32_t a[320*100],z[320*100];struct desk_canvas full,tiles;desk_canvas_init(&full,a,320,100,320);desk_canvas_init(&tiles,z,320,100,320);
    desk_tabs_draw(&full,&t,&desk_theme_quiet);
    for(int y=0;y<100;y+=11) for(int x=0;x<320;x+=13) { struct desk_rect clip={x,y,13,11};desk_clip(&tiles,clip);clip=tiles.clip;desk_tabs_draw(&tiles,&t,&desk_theme_quiet);assert(!memcmp(&clip,&tiles.clip,sizeof(clip))); }
    assert(!memcmp(a,z,sizeof(a)));
    for(int y=0;y<100;y++) for(int x=0;x<320;x++) if(x<10 || x>=310 || y<10 || y>=42) assert(!a[y*320+x]);
    desk_tabs_configure(&t,(struct desk_rect){10,10,1,1},1);assert(!t.enabled && !t.focused);desk_tabs_draw(&full,&t,&desk_theme_quiet);
    items[0].disabled=items[2].disabled=1;assert(!desk_tabs_items(&t,items,3));assert(t.selected==-1 && !t.enabled);
    assert(!desk_tabs_items(&t,NULL,0) && !desk_tabs_selected(&t));
    puts("tabs: PASS stable identities, atomic validation, disabled skipping, keyboard/pointer selection, cancellation, shared focus and tiled/clipped drawing");
}
