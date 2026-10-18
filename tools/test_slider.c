#include <srvros/desk_ui.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct srv_key_event key(unsigned usage,unsigned modifiers) { return (struct srv_key_event){.flags=SRV_KEY_DOWN_EVENT,.usage=usage,.modifiers=modifiers}; }
int main(void) {
    struct desk_slider s;desk_slider_init(&s);desk_slider_configure(&s,(struct desk_rect){10,20,218,28},1);
    assert(s.enabled && !desk_slider_range(&s,0,255,1));assert(desk_slider_set(&s,123)&DESK_WIDGET_CHANGED);
    struct desk_rect thumb=desk_slider_thumb(&s);assert(thumb.w==14 && thumb.h==18);
    assert(desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=thumb.x+3,.y=thumb.y+4,.down=1,.pressed=1})==DESK_WIDGET_REDRAW && s.value==123 && s.dragging);
    desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=INT32_MAX,.y=20,.down=1});assert(s.value==255);
    unsigned result=desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=INT32_MIN,.y=20});assert(s.value==0 && !s.dragging && (result&DESK_WIDGET_ACTIVATED));
    desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});
    struct srv_key_event k=key(SRV_KEY_END,0);desk_slider_key(&s,&k);assert(s.value==255);
    k=key(SRV_KEY_LEFT,SRV_KEY_SHIFT);desk_slider_key(&s,&k);assert(s.value==245);
    k=key(SRV_KEY_PAGEDOWN,0);desk_slider_key(&s,&k);assert(s.value==235);
    k=key(SRV_KEY_HOME,SRV_KEY_CTRL);assert(!desk_slider_key(&s,&k) && s.value==235);
    struct desk_slider saved=s;assert(desk_slider_range(&s,10,0,1)<0 && !memcmp(&s,&saved,sizeof(s)));assert(desk_slider_range(&s,0,10,0)<0);
    assert(!desk_slider_range(&s,INT32_MIN,INT32_MAX,1));desk_slider_set(&s,INT64_MAX);assert(s.value==INT32_MAX);
    k=key(SRV_KEY_LEFT,0);desk_slider_key(&s,&k);assert(s.value==INT32_MAX-1);
    desk_slider_set(&s,INT64_MIN);assert(s.value==INT32_MIN);
    desk_slider_range(&s,-100,100,7);desk_slider_set(&s,-90);assert(s.value==-93);desk_slider_set(&s,99);assert(s.value==96);desk_slider_set(&s,100);assert(s.value==100);
    desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=20,.y=25,.down=1,.pressed=1});assert(s.dragging);
    desk_slider_configure(&s,(struct desk_rect){10,20,219,28},1);assert(!s.dragging);
    desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=20,.y=25,.down=1,.pressed=1});
    desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_OUT});assert(!s.dragging && !s.focused);
    desk_slider_configure(&s,s.frame,0);saved=s;desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=20,.y=25,.down=1,.pressed=1});assert(!memcmp(&s,&saved,sizeof(s)));
    /* Full signed range mapping remains monotonic, clamped and exact at ends. */
    desk_slider_range(&s,INT32_MIN,INT32_MAX,1);desk_slider_configure(&s,(struct desk_rect){0,0,218,28},1);
    int32_t previous=INT32_MIN;
    for(int x=-20;x<250;x++) {
        desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=2,.y=10,.down=1,.pressed=1});s.grab=7;
        desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=x,.y=10,.down=1});assert(s.value>=previous);previous=s.value;
        desk_slider_event(&s,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    }
    assert(s.value==INT32_MAX);
    static uint32_t full[300*80],tiled[300*80];
    for(int width=0;width<=280;width+=7) {
        memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));desk_slider_configure(&s,(struct desk_rect){3,4,width,28},1);s.focused=1;
        struct desk_canvas a,b;desk_canvas_init(&a,full,288,64,300);desk_canvas_init(&b,tiled,288,64,300);desk_slider_draw(&a,&s,&desk_theme_quiet);
        for(int y=0;y<64;y+=13) for(int x=0;x<288;x+=17) { desk_clip(&b,(struct desk_rect){x,y,17,13});struct desk_rect clip=b.clip;desk_slider_draw(&b,&s,&desk_theme_quiet);assert(!memcmp(&clip,&b.clip,sizeof(clip))); }
        assert(!memcmp(full,tiled,sizeof(full)));
        for(unsigned y=0;y<80;y++) for(unsigned x=0;x<300;x++) if(y>=64 || x>=288) assert(!full[y*300+x]);
    }
    struct desk_ui ui;desk_ui_init(&ui);desk_slider_range(&s,0,10,1);desk_slider_configure(&s,(struct desk_rect){0,0,100,28},1);desk_slider_set(&s,5);
    assert(!desk_ui_add(&ui,1,DESK_CONTROL_SLIDER,&s,NULL));desk_ui_focus(&ui,1);k=key(SRV_KEY_RIGHT,0);assert(desk_ui_key(&ui,&k).flags&DESK_WIDGET_CHANGED);assert(s.value==6);
    assert(!desk_ui_scroll(&ui,10,10,0,120).flags && s.value==6);desk_ui_window_focus(&ui,0);assert(!s.focused);
    puts("slider: PASS full-range arithmetic, snapping, capture/resize/focus, keyboard steps, no wheel mutation, UI routing, clipped/tiled/stride rendering");
}
