#include <srvros/desk_edit.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static struct srv_key_event key(unsigned usage,unsigned cp,unsigned mods) {
    return (struct srv_key_event){.usage=usage,.codepoint=cp,.modifiers=mods,.flags=SRV_KEY_DOWN_EVENT};
}
static unsigned press(struct desk_edit *e,unsigned usage,unsigned cp,unsigned mods) {
    struct srv_key_event k=key(usage,cp,mods); return desk_edit_key(e,&k);
}
static void scrollbars(void) {
    static char storage[2][16384],source[16384];size_t used=0;
    for(unsigned line=0;line<200;line++) used+=snprintf(source+used,sizeof(source)-used,"%03u A long line with words, café and more text for horizontal scrolling.\n",line);
    struct desk_text_buffer text;desk_text_buffer_init(&text,storage[0],storage[1],sizeof(storage[0]));assert(desk_text_buffer_set(&text,source,used)>0);
    struct desk_edit e;desk_edit_init(&e,&text,1);desk_edit_scrollbars(&e,1);desk_edit_configure(&e,(struct desk_rect){4,4,300,180},&desk_font_16,1);
    desk_text_buffer_select(&text,3,7);desk_edit_event(&e,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});desk_edit_reveal(&e);
    assert(e.show_vertical && e.show_horizontal);struct desk_rect view=desk_edit_viewport(&e);assert(view.w==270 && view.h==150);
    uint64_t revision=text.revision;struct desk_scrollbar_geometry g=desk_scrollbar_geometry(&e.vertical);
    struct desk_widget_event p={DESK_WIDGET_POINTER,g.thumb.x+1,g.thumb.y+1,1,1};desk_edit_event(&e,p);assert(e.scroll_capture==1 && !e.dragging);
    p.pressed=0;p.y=500;desk_edit_event(&e,p);assert(e.scroll_y==desk_scrollbar_limit(&e.vertical) && text.anchor==3 && text.caret==7 && text.revision==revision);
    p.down=0;desk_edit_event(&e,p);assert(!e.scroll_capture && !e.vertical.dragging);
    g=desk_scrollbar_geometry(&e.horizontal);p=(struct desk_widget_event){DESK_WIDGET_POINTER,g.thumb.x+1,g.thumb.y+1,1,1};desk_edit_event(&e,p);assert(e.scroll_capture==2);
    p.pressed=0;p.x=10000;p.down=0;desk_edit_event(&e,p);assert(e.scroll_x==desk_scrollbar_limit(&e.horizontal) && text.anchor==3 && text.caret==7 && text.revision==revision);
    desk_edit_scroll(&e,INT32_MIN,INT32_MIN);assert(!e.scroll_x && !e.scroll_y);
    p=(struct desk_widget_event){DESK_WIDGET_POINTER,e.vertical.track.x+1,e.vertical.track.y+e.vertical.track.h-1,1,1};
    unsigned result=desk_edit_event(&e,p);assert(result==DESK_WIDGET_REDRAW && e.scroll_y==view.h && text.anchor==3);
    desk_edit_event(&e,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_OUT});assert(!e.scroll_capture && !e.vertical.dragging);
    p.pressed=0;p.y=1000;desk_edit_event(&e,p);assert(e.scroll_y==view.h && text.caret==7);
    desk_edit_event(&e,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});press(&e,SRV_KEY_END,0,SRV_KEY_CTRL);
    assert(text.caret==text.length && e.scroll_y==desk_scrollbar_limit(&e.vertical));
    press(&e,SRV_KEY_HOME,0,SRV_KEY_CTRL);assert(!text.caret && !e.scroll_y && !e.scroll_x);
    p=(struct desk_widget_event){DESK_WIDGET_POINTER,e.vertical.track.x+1,e.horizontal.track.y+1,1,1};desk_edit_event(&e,p);assert(!e.dragging && !e.scroll_capture && !text.caret);
    /* Tiled/stride rendering must include both bars and the scrolled selection. */
    desk_edit_scroll(&e,113,1857);static uint32_t full[324*200],tiled[324*200];struct desk_canvas canvas;
    desk_canvas_init(&canvas,full,320,200,324);desk_edit_draw(&canvas,&e,&desk_theme_quiet);
    desk_canvas_init(&canvas,tiled,320,200,324);
    for(int y=0;y<200;y+=19) for(int x=0;x<320;x+=37) { desk_clip(&canvas,(struct desk_rect){x,y,37,19});desk_edit_draw(&canvas,&e,&desk_theme_quiet); }
    assert(!memcmp(full,tiled,sizeof(full)));
    desk_edit_configure(&e,e.frame,&desk_font_20,1);assert(e.measure_font==&desk_font_20 && e.content_height==201*desk_font_20.line_height);
    desk_text_buffer_set(&text,"small",5);desk_edit_reveal(&e);assert(!e.show_vertical && !e.show_horizontal && !e.scroll_x && !e.scroll_y);
    desk_edit_configure(&e,(struct desk_rect){4,4,30,30},e.font,1);desk_edit_draw(&canvas,&e,&desk_theme_quiet);
    desk_edit_configure(&e,e.frame,e.font,0);assert(!desk_edit_scroll(&e,100,100));
    e.multiline=0;desk_edit_configure(&e,(struct desk_rect){4,4,30,30},e.font,1);assert(!e.show_vertical && !e.show_horizontal);
}
int main(void) {
    scrollbars();
    char a[256],b[256]; struct desk_text_buffer text;
    assert(!desk_text_buffer_init(&text,a,b,sizeof(a)));
    struct desk_edit e; desk_edit_init(&e,&text,1);
    desk_edit_configure(&e,(struct desk_rect){4,4,180,65},&desk_font_16,1);
    assert(!press(&e,4,'a',0));
    desk_edit_event(&e,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});
    assert(press(&e,4,'a',0)&DESK_WIDGET_CHANGED);
    press(&e,0,0x00e9,0); press(&e,SRV_KEY_ENTER,0,0); press(&e,5,'b',0);
    assert(!strcmp(text.data,"aé\nb"));
    press(&e,SRV_KEY_HOME,0,0); assert(text.caret==4);
    press(&e,SRV_KEY_LEFT,0,SRV_KEY_SHIFT); assert(text.anchor==4 && text.caret==3);
    press(&e,SRV_KEY_LEFT,0,SRV_KEY_SHIFT); assert(text.caret==1);
    assert(press(&e,6,'c',SRV_KEY_CTRL)==DESK_EDIT_COPY);
    assert(press(&e,0x19,'v',SRV_KEY_CTRL)==DESK_EDIT_PASTE);
    press(&e,0x1b,'x',0); assert(!strcmp(text.data,"axb"));
    press(&e,0x1d,'z',SRV_KEY_CTRL); assert(!strcmp(text.data,"aé\nb"));
    press(&e,0x1c,'y',SRV_KEY_CTRL); assert(!strcmp(text.data,"axb"));
    press(&e,4,'a',SRV_KEY_CTRL); assert(text.anchor==0 && text.caret==3);
    press(&e,SRV_KEY_BACKSPACE,0,0); assert(!text.length);
    desk_text_buffer_set(&text,"first line\nsecond line\nthird line\nfourth line",45);
    press(&e,SRV_KEY_END,0,SRV_KEY_CTRL); assert(text.caret==text.length && e.scroll_y>0);
    press(&e,SRV_KEY_HOME,0,SRV_KEY_CTRL); assert(!text.caret && !e.scroll_y);
    press(&e,SRV_KEY_DOWN,0,0); assert(text.caret==11);
    press(&e,SRV_KEY_END,0,0); assert(text.caret==22);
    press(&e,SRV_KEY_LEFT,0,SRV_KEY_CTRL); assert(text.caret==18);
    press(&e,SRV_KEY_LEFT,0,SRV_KEY_CTRL); assert(text.caret==11);
    struct desk_widget_event pointer={.type=DESK_WIDGET_POINTER,.x=10,.y=10,.down=1,.pressed=1};
    desk_edit_event(&e,pointer); assert(e.dragging && !text.caret);
    pointer.pressed=0;pointer.x=500;pointer.y=500;
    desk_edit_event(&e,pointer); assert(text.caret==text.length && text.anchor==0);
    desk_edit_event(&e,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    pointer.x=10;pointer.y=10;desk_edit_event(&e,pointer); assert(text.caret==text.length);
    desk_edit_configure(&e,e.frame,e.font,0); assert(!press(&e,4,'a',0));
    desk_edit_configure(&e,e.frame,e.font,1);
    desk_edit_event(&e,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});
    e.multiline=0; assert(press(&e,SRV_KEY_ENTER,0,0)==DESK_EDIT_SUBMIT);
    /* Tiled drawing must honor ancestor clipping, including selection/caret. */
    e.multiline=1; e.scroll_x=e.scroll_y=0;
    uint32_t full[200*100],tiled[200*100];
    for(unsigned i=0;i<200*100;i++) full[i]=tiled[i]=0x12345678;
    struct desk_canvas c;desk_canvas_init(&c,full,200,100,200);desk_edit_draw(&c,&e,&desk_theme_quiet);
    desk_canvas_init(&c,tiled,200,100,200);
    for(int y=0;y<100;y+=7) for(int x=0;x<200;x+=13) {
        desk_clip(&c,(struct desk_rect){x,y,13,7});struct desk_rect clip=c.clip;
        desk_edit_draw(&c,&e,&desk_theme_quiet);assert(!memcmp(&clip,&c.clip,sizeof(clip)));
    }
    assert(!memcmp(full,tiled,sizeof(full)));
    puts("widget-edit: PASS UTF-8 editing, selection, shortcuts, vertical/word movement, reveal, drag/cancel, focus and tiled drawing");
}
