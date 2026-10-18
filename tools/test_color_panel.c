#include <srvros/desk_color_panel.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct srv_key_event key(unsigned usage,unsigned mods) { return (struct srv_key_event){.flags=SRV_KEY_DOWN_EVENT,.usage=usage,.modifiers=mods}; }
int main(void) {
    struct desk_color_panel p;desk_color_panel_init(&p);desk_color_panel_configure(&p,(struct desk_rect){0,0,640,480});desk_color_panel_open(&p,0xab38644f);
    assert(p.color==0x38644f && !strcmp(p.hex,"#38644F") && p.channels[0].value==56 && p.channels[1].value==100 && p.channels[2].value==79);
    struct srv_key_event k=key(SRV_KEY_END,0);struct desk_action a=desk_color_panel_key(&p,&k);assert(a.flags&DESK_WIDGET_CHANGED);assert(p.color==0xff644f && !strcmp(p.hex,"#FF644F"));
    k=key(SRV_KEY_TAB,0);desk_color_panel_key(&p,&k);k=key(SRV_KEY_HOME,0);desk_color_panel_key(&p,&k);assert(p.color==0xff004f);
    k=key(SRV_KEY_ENTER,0);a=desk_color_panel_key(&p,&k);assert(a.id==DESK_COLOR_ACCEPT && (a.flags&DESK_WIDGET_ACTIVATED));
    k=key(SRV_KEY_ESCAPE,0);a=desk_color_panel_key(&p,&k);assert(a.id==DESK_COLOR_CANCEL && p.color==0xff004f);
    desk_color_panel_open(&p,0x123456);assert(!strcmp(p.hex,"#123456"));
    struct desk_rect r=p.swatches[1].frame;struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=r.x+5,.y=r.y+5,.down=1,.pressed=1};
    desk_color_panel_pointer(&p,e,0);e.down=e.pressed=0;a=desk_color_panel_pointer(&p,e,1);assert((a.flags&DESK_WIDGET_CHANGED) && p.color==0xffffff);
    r=p.copy.frame;e=(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=r.x+5,.y=r.y+5,.down=1,.pressed=1};desk_color_panel_pointer(&p,e,0);e.down=e.pressed=0;a=desk_color_panel_pointer(&p,e,1);assert(a.id==DESK_COLOR_COPY && (a.flags&DESK_WIDGET_ACTIVATED));
    desk_color_panel_focus(&p,0);assert(!desk_color_panel_key(&p,&k).flags);desk_color_panel_focus(&p,1);
    static uint32_t full[660*500],tiled[660*500];
    for(unsigned mode=0;mode<8;mode++) {
        int width=mode<2?640:mode<4?280:mode<6?160:16,height=mode<2?480:mode<4?240:120;
        desk_color_panel_configure(&p,(struct desk_rect){0,0,width,height});
        assert((width>=280)==!!p.accept.enabled);
        memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));struct desk_canvas a,b;desk_canvas_init(&a,full,width,height,660);desk_canvas_init(&b,tiled,width,height,660);
        struct desk_theme t=desk_theme_quiet;if(mode%2) t.body=&desk_font_20;
        desk_color_panel_draw(&a,&p,&t);
        for(int y=0;y<height;y+=43) for(int x=0;x<width;x+=59) { desk_clip(&b,(struct desk_rect){x,y,59,43});struct desk_rect clip=b.clip;desk_color_panel_draw(&b,&p,&t);assert(!memcmp(&clip,&b.clip,sizeof(clip))); }
        assert(!memcmp(full,tiled,sizeof(full)));
        for(unsigned y=0;y<500;y++) for(unsigned x=0;x<660;x++) if(x>=(unsigned)width || y>=(unsigned)height) assert(!full[y*660+x]);
    }
    puts("color-panel: PASS independent RGB draft, exact hex, keyboard/channel traversal, swatches, use/cancel/copy requests, focus and compact/tiled/stride rendering");
}
