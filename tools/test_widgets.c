#include <srvros/desk_widgets.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static struct desk_widget_event pointer(int x,int y,int down,int pressed) {
    return (struct desk_widget_event){DESK_WIDGET_POINTER,x,y,down,pressed};
}
static void scrollbar(void) {
    struct desk_scrollbar bar={0};
    desk_scrollbar_configure(&bar,(struct desk_rect){570,94,18,118},1152,118);
    struct desk_scrollbar_geometry g=desk_scrollbar_geometry(&bar);
    assert(g.limit==1034 && g.thumb.h==16 && g.travel==102);
    /* Hit area includes the track beside the visible thumb. */
    desk_scrollbar_event(&bar,pointer(571,101,1,1));
    assert(bar.dragging && bar.offset==0);
    desk_scrollbar_event(&bar,pointer(-500,INT32_MAX,1,0)); assert(bar.offset==1034);
    desk_scrollbar_event(&bar,pointer(-500,INT32_MIN,1,0)); assert(bar.offset==0);
    desk_scrollbar_event(&bar,pointer(-500,INT32_MAX,0,0));
    assert(!bar.dragging && bar.offset==1034); /* Release includes final position. */
    desk_scrollbar_event(&bar,pointer(571,94,1,0)); assert(bar.offset==1034);
    desk_scrollbar_event(&bar,pointer(571,94,1,1)); assert(bar.offset==916);
    desk_scrollbar_set(&bar,0);
    desk_scrollbar_event(&bar,pointer(571,211,1,1)); assert(bar.offset==118);
    desk_scrollbar_scroll(&bar,INT64_MIN); assert(bar.offset==0);
    desk_scrollbar_scroll(&bar,INT64_MAX); assert(bar.offset==1034);
    desk_scrollbar_set(&bar,INT64_MIN); assert(bar.offset==0);
    desk_scrollbar_event(&bar,pointer(571,101,1,1)); assert(bar.dragging);
    desk_scrollbar_event(&bar,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    desk_scrollbar_event(&bar,pointer(571,200,1,0)); assert(!bar.dragging && !bar.offset);
    desk_scrollbar_event(&bar,pointer(571,101,1,1));
    desk_scrollbar_configure(&bar,(struct desk_rect){570,94,18,200},1152,200);
    assert(!bar.dragging);
    desk_scrollbar_set(&bar,INT64_MAX);
    desk_scrollbar_configure(&bar,(struct desk_rect){570,94,18,200},100,200);
    assert(!bar.offset && !desk_scrollbar_limit(&bar));
    assert(desk_scrollbar_geometry(&bar).thumb.h==200);
    desk_scrollbar_event(&bar,pointer(571,101,1,1)); assert(!bar.dragging);
    desk_scrollbar_configure(&bar,(struct desk_rect){1,1,1,1},INT32_MAX,1);
    desk_scrollbar_set(&bar,INT64_MAX);
    assert(desk_scrollbar_geometry(&bar).travel==0);
    desk_scrollbar_event(&bar,pointer(1,1,1,1));
    desk_scrollbar_event(&bar,pointer(INT32_MIN,INT32_MAX,0,0));
    assert(!bar.dragging);
    desk_scrollbar_configure(&bar,(struct desk_rect){0,0,18,100},100,0);
    assert(!bar.offset && !desk_scrollbar_limit(&bar));
    desk_scrollbar_configure(&bar,(struct desk_rect){INT32_MAX,0,18,100},100,10);
    assert(!bar.track.w && !desk_scrollbar_geometry(&bar).thumb.w);
    /* Geometry arithmetic must remain bounded even with extreme dimensions. */
    desk_scrollbar_configure(&bar,(struct desk_rect){0,0,18,INT32_MAX},INT32_MAX,INT32_MAX-1);
    desk_scrollbar_set(&bar,1);
    g=desk_scrollbar_geometry(&bar);
    assert(g.thumb.y==1 && g.thumb.h==INT32_MAX-1);
    assert(!desk_widget_contains((struct desk_rect){0,0,10,10},10,0));
    assert(!desk_widget_inset((struct desk_rect){0,0,10,10},INT32_MAX).w);
}
static void button(void) {
    struct desk_button b={0};
    desk_button_configure(&b,(struct desk_rect){10,10,100,28},1);
    unsigned r=desk_button_event(&b,pointer(20,20,1,1));
    assert(r==DESK_WIDGET_REDRAW && b.armed);
    r=desk_button_event(&b,pointer(20,20,0,0));
    assert((r&DESK_WIDGET_ACTIVATED) && !b.armed);
    assert(!desk_button_event(&b,pointer(20,20,0,0))); /* No double activation. */
    desk_button_event(&b,pointer(20,20,1,1));
    r=desk_button_event(&b,pointer(-50,20,1,0)); assert(r==DESK_WIDGET_REDRAW);
    r=desk_button_event(&b,pointer(-50,20,0,0)); assert(!(r&DESK_WIDGET_ACTIVATED));
    desk_button_event(&b,pointer(20,20,1,1));
    desk_button_event(&b,pointer(-50,20,1,0));
    r=desk_button_event(&b,pointer(20,20,0,0)); assert(r&DESK_WIDGET_ACTIVATED);
    desk_button_event(&b,pointer(-50,20,1,1));
    assert(!desk_button_event(&b,pointer(20,20,0,0))); /* Press began outside. */
    desk_button_event(&b,pointer(20,20,1,1));
    desk_button_event(&b,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    assert(!desk_button_event(&b,pointer(20,20,0,0)));
    struct desk_widget_event activate={.type=DESK_WIDGET_ACTIVATE};
    assert(!desk_button_event(&b,activate));
    desk_button_event(&b,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});
    assert(desk_button_event(&b,activate)==DESK_WIDGET_ACTIVATED);
    desk_button_event(&b,pointer(20,20,1,1));
    desk_button_event(&b,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_OUT});
    assert(!b.armed && !b.focused && !desk_button_event(&b,pointer(20,20,0,0)));
    desk_button_event(&b,pointer(20,20,1,1));
    desk_button_configure(&b,b.frame,0);
    assert(!b.armed && !desk_button_event(&b,activate));
    assert(!desk_button_event(&b,pointer(20,20,1,1)));
    assert(!desk_button_event(&b,pointer(20,20,0,0)));
    desk_button_configure(&b,b.frame,1);
    assert(!desk_button_event(&b,pointer(20,20,0,0)));
    desk_button_event(&b,pointer(20,20,1,1));
    desk_button_configure(&b,(struct desk_rect){20,10,100,28},1);
    assert(!b.armed && !desk_button_event(&b,pointer(30,20,0,0)));
}
static void scene(struct desk_canvas *c) {
    struct desk_button b={0}; struct desk_scrollbar bar={0};
    desk_button_configure(&b,(struct desk_rect){-4,8,100,28},1);
    desk_button_event(&b,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});
    desk_button_draw(c,&b,"Aa Café",&desk_theme_quiet);
    desk_scrollbar_configure(&bar,(struct desk_rect){101,-5,18,100},500,100);
    desk_scrollbar_set(&bar,200);
    desk_scrollbar_draw(c,&bar,&desk_theme_quiet);
    desk_label_draw(c,(struct desk_rect){5,40,80,24},&desk_font_20,"Clipped label",0x292e2b);
}
static void clipping(void) {
    enum { W=128,H=80,S=135 };
    uint32_t full[S*H+2], tiled[S*H+2];
    for (unsigned i=0;i<S*H+2;i++) full[i]=tiled[i]=0x12345678;
    struct desk_canvas c; desk_canvas_init(&c,full+1,W,H,S); scene(&c);
    desk_canvas_init(&c,tiled+1,W,H,S);
    for (int y=0;y<H;y+=7) for (int x=0;x<W;x+=11) {
        desk_clip(&c,(struct desk_rect){x,y,11,7}); struct desk_rect clip=c.clip;
        scene(&c); assert(!memcmp(&clip,&c.clip,sizeof(clip)));
    }
    assert(!memcmp(full,tiled,sizeof(full)));
    assert(full[0]==0x12345678 && full[S*H+1]==0x12345678);
    for (int y=0;y<H;y++) for (int x=W;x<S;x++) assert(full[1+y*S+x]==0x12345678);
    /* Every control honors an empty ancestor clip. */
    desk_clip(&c,(struct desk_rect){0}); uint64_t before=c.pixel_writes;
    scene(&c); assert(c.pixel_writes==before);
}
static void ellipsis(void) {
    const char *samples[]={"Short","A long title with spaces and punctuation", "Café — mañana · résumé", "\xf0\x9f\x8c\xbf unsupported", "bad\xc0\x80 text", "First line\nsecond", ""};
    uint32_t actual[320*40],expected[320*40],tiled[320*40];
    for(unsigned f=0;f<2;f++) for(unsigned sample=0;sample<sizeof(samples)/sizeof(*samples);sample++) for(int width=0;width<280;width+=3) {
        const struct desk_font *font=f?&desk_font_20:&desk_font_16;const char *text=samples[sample];char reference[256]="";
        if(!strchr(text,'\n') && desk_text_width(font,text)<=width) strcpy(reference,text);
        else if(desk_text_width(font,"…")<=width) {
            strcpy(reference,"…");const char *p=text;
            while(*p && *p!='\n') {
                desk_utf8_next(&p);size_t count=p-text;char candidate[256];memcpy(candidate,text,count);strcpy(candidate+count,"…");
                if(desk_text_width(font,candidate)>width) break;
                strcpy(reference,candidate);
            }
        }
        memset(actual,0,sizeof(actual));memset(expected,0,sizeof(expected));memset(tiled,0,sizeof(tiled));struct desk_canvas a,b,c;
        desk_canvas_init(&a,actual,300,40,320);desk_canvas_init(&b,expected,300,40,320);desk_canvas_init(&c,tiled,300,40,320);
        struct desk_rect frame={7,4,width,30};desk_label_ellipsis(&a,frame,font,text,0xffffff);desk_label_draw(&b,frame,font,reference,0xffffff);
        assert(!memcmp(actual,expected,sizeof(actual)));
        for(int y=0;y<40;y+=9) for(int x=0;x<300;x+=31) { desk_clip(&c,(struct desk_rect){x,y,31,9});struct desk_rect clip=c.clip;desk_label_ellipsis(&c,frame,font,text,0xffffff);assert(!memcmp(&clip,&c.clip,sizeof(clip))); }
        assert(!memcmp(actual,tiled,sizeof(actual)));
    }
}
int main(void) {
    scrollbar(); button(); clipping(); ellipsis();
    puts("widgets: PASS capture/page/resize/clamp, button activation/cancel/focus/disable, nested clipping and stride guards");
    return 0;
}
