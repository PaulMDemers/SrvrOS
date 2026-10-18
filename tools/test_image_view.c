#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_ui.h>
#include "image_view_reference.h"
static uint32_t source[256*128];
int main(void) {
    for(unsigned y=0;y<128;y++) for(unsigned x=0;x<256;x++) source[y*256+x]=((x%3?255u:128u)<<24)|(x<<16)|(y<<8)|17;
    struct desk_image v;desk_image_init(&v);desk_image_configure(&v,(struct desk_rect){10,10,102,82},1);
    assert(!desk_image_set(&v,source,256,128));assert(v.fit && v.scale==25600 && !v.offset_x && !v.offset_y);
    struct desk_image saved=v;assert(desk_image_set(&v,source,4096,4096)==-1 && !memcmp(&v,&saved,sizeof(v)));
    assert(desk_image_set(&v,NULL,1,1)==-1 && desk_image_set(&v,source,0,0)==-1);
    desk_image_actual(&v);assert(v.scale==65536 && v.offset_x==78 && v.offset_y==24);
    desk_image_scroll(&v,2147483647,2147483647);assert(v.offset_x==156 && v.offset_y==48);
    desk_image_scroll(&v,-2147483647,-2147483647);assert(!v.offset_x && !v.offset_y);
    struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=90,.y=60,.down=1,.pressed=1};desk_image_event(&v,e);
    e.x=-100;e.y=-100;e.pressed=e.down=0;assert(desk_image_event(&v,e)&DESK_WIDGET_CHANGED);assert(v.offset_x==156 && v.offset_y==48 && !v.dragging);
    e.x=20;e.y=20;e.pressed=e.down=1;desk_image_event(&v,e);desk_image_configure(&v,(struct desk_rect){10,10,120,90},1);assert(!v.dragging);
    struct desk_ui ui;struct desk_button button={0};desk_ui_init(&ui);desk_ui_add(&ui,1,DESK_CONTROL_IMAGE,&v,NULL);desk_button_configure(&button,(struct desk_rect){10,120,100,28},1);desk_ui_add(&ui,2,DESK_CONTROL_BUTTON,&button,"Fit");desk_ui_focus(&ui,1);
    struct srv_key_event key={.usage=SRV_KEY_HOME,.flags=SRV_KEY_DOWN_EVENT};desk_ui_key(&ui,&key);assert(!v.offset_x && !v.offset_y);
    key.codepoint='+';assert(desk_ui_key(&ui,&key).flags&DESK_WIDGET_CHANGED);assert(v.scale==131072);
    key.codepoint='0';desk_ui_key(&ui,&key);assert(v.fit);
    key.codepoint=0;key.usage=SRV_KEY_TAB;desk_ui_key(&ui,&key);assert(button.focused && !v.focused);
    key.modifiers=SRV_KEY_SHIFT;desk_ui_key(&ui,&key);assert(v.focused);
    static uint32_t a[224*180],b[224*180];struct desk_canvas full,tiles;
    for(unsigned mode=0;mode<5;mode++) {
        if(mode==1) desk_image_actual(&v);
        if(mode==2) { desk_image_zoom(&v,1);desk_image_scroll(&v,33,11); }
        if(mode==3) desk_image_configure(&v,(struct desk_rect){-10,5,180,140},1);
        if(mode==4) desk_image_configure(&v,(struct desk_rect){1,1,1,1},1);
        memset(a,0,sizeof(a));memset(b,0,sizeof(b));desk_canvas_init(&full,a,200,160,224);desk_canvas_init(&tiles,b,200,160,224);
        desk_image_draw(&full,&v,&desk_theme_quiet);
        for(int y=0;y<160;y+=17) for(int x=0;x<200;x+=19) { desk_clip(&tiles,(struct desk_rect){x,y,19,17});struct desk_rect clip=tiles.clip;desk_image_draw(&tiles,&v,&desk_theme_quiet);assert(!memcmp(&clip,&tiles.clip,sizeof(clip))); }
        assert(!memcmp(a,b,sizeof(a)));
        for(int y=0;y<180;y++) for(int x=0;x<224;x++) if(x>=200 || y>=160) assert(!a[y*224+x]);
    }
    unsigned random=0x972134u;
    for(unsigned trial=0;trial<400;trial++) {
        random=random*1664525u+1013904223u;
        desk_image_set(&v,source,256,128);
        desk_image_configure(&v,(struct desk_rect){(int)(random%80)-40,(int)((random>>8)%60)-30,3+(random>>16)%350,3+(random>>24)%170},1);
        if(trial%3) { desk_image_actual(&v);for(unsigned n=0;n<trial%5;n++) desk_image_zoom(&v,trial%2?1:-1);desk_image_scroll(&v,random%100,(random>>8)%80); }
        memset(a,0,sizeof(a));memset(b,0,sizeof(b));desk_canvas_init(&full,a,200,160,224);desk_canvas_init(&tiles,b,200,160,224);
        struct desk_rect clip={(int)(random%70),(int)((random>>8)%50),80+(random>>16)%120,60+(random>>24)%100};
        desk_clip(&full,clip);desk_clip(&tiles,clip);desk_image_draw(&full,&v,&desk_theme_quiet);image_reference(&tiles,&v,&desk_theme_quiet);
        assert(!memcmp(a,b,sizeof(a)) && full.pixel_writes==tiles.pixel_writes);
    }
    desk_canvas_init(&full,a,200,160,224);
    uint32_t small[]={0xffff0000,0x000000ff,0x8000ff00,0xff0000ff};
    desk_image_configure(&v,(struct desk_rect){0,0,6,6},1);desk_image_set(&v,small,2,2);desk_image_draw(&full,&v,&desk_theme_quiet);
    assert(a[2*224+2]==0xff0000 && a[2*224+3]==0xf1f0eb && a[3*224+3]==0x0000ff);
    assert(a[3*224+2]==0x78f875); /* Independent rounded alpha blend over checker. */
    puts("image-view: PASS bounded source geometry, fit/zoom/pan, pointer cancellation, focus, alpha, clipping, tiled rendering and stride guards");
}
