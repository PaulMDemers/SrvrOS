#include <srvros/desk_edit.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct srv_key_event key(unsigned usage,unsigned cp,unsigned mods) {
    return (struct srv_key_event){.usage=usage,.codepoint=cp,.modifiers=mods,.flags=SRV_KEY_DOWN_EVENT};
}
static unsigned press(struct desk_edit *e,unsigned usage,unsigned cp,unsigned mods) {
    struct srv_key_event k=key(usage,cp,mods); return desk_edit_key(e,&k);
}
int main(void) {
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
