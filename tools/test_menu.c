#include <srvros/desk_menu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const struct desk_menu_item items[]={
    {0,DESK_MENU_HEADING,"File",""},{1,0,"New","Ctrl-N"},{2,DESK_MENU_DISABLED,"Save","Ctrl-S"},
    {0,DESK_MENU_SEPARATOR,"",""},{3,0,"Close","Ctrl-W"},
    {0,DESK_MENU_HEADING,"View",""},{4,DESK_MENU_CHECKED,"Details",""},{5,0,"Refresh","Ctrl-R"}
};
static struct desk_menu m;
static struct desk_menu_result pointer(int x,int y,int down,int pressed) {
    return desk_menu_pointer(&m,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=x,.y=y,.down=down,.pressed=pressed});
}
static struct desk_menu_result key(unsigned usage) {
    struct srv_key_event k={.usage=usage,.flags=SRV_KEY_DOWN_EVENT};return desk_menu_key(&m,&k);
}
#define W 420
#define H 280
#define STRIDE 427
static uint32_t pixels[STRIDE*H+16],tiles[STRIDE*H+16];
int main(void) {
    assert(desk_menu_valid(items,8));struct desk_menu_item bad[8];memcpy(bad,items,sizeof(bad));
    bad[2].id=1;assert(!desk_menu_valid(bad,8));bad[2]=items[2];bad[0].label[0]='\n';assert(!desk_menu_valid(bad,8));
    bad[0]=items[0];memset(bad[1].label,'x',sizeof(bad[1].label));assert(!desk_menu_valid(bad,8));
    bad[1]=items[1];strcpy(bad[1].label,"\xc0\xaf");assert(!desk_menu_valid(bad,8));
    assert(!desk_menu_valid(items,DESK_MENU_ITEMS+1));assert(!desk_menu_valid(items+1,7));
    desk_menu_init(&m);assert(!desk_menu_configure(&m,items,8,(struct desk_rect){10,0,W-10,44},(struct desk_rect){0,0,W,H},&desk_font_16));
    assert(pointer(20,20,1,1).consumed && m.open==0);pointer(20,20,0,0);
    struct desk_rect row=desk_menu_row(&m,1);assert(!pointer(row.x+30,row.y+10,0,0).action && m.open==0);
    pointer(row.x+30,row.y+10,1,1);assert(pointer(row.x+30,row.y+10,0,0).action==1 && m.open<0);
    /* Press-drag from heading to item releases into a single activation. */
    pointer(20,20,1,1);row=desk_menu_row(&m,4);pointer(row.x+20,row.y+10,1,0);
    assert(pointer(row.x+20,row.y+10,0,0).action==3);
    pointer(20,20,1,1);pointer(20,20,0,0);row=desk_menu_row(&m,2);
    pointer(row.x+20,row.y+10,1,1);assert(!pointer(row.x+20,row.y+10,0,0).action);
    assert(pointer(410,260,1,1).consumed && m.open<0);assert(!pointer(410,260,0,0).consumed);
    assert(key(SRV_KEY_F10).consumed && m.selected==1);key(SRV_KEY_DOWN);assert(m.selected==4);
    key(SRV_KEY_DOWN);assert(m.selected==1);key(SRV_KEY_END);assert(m.selected==4);
    key(SRV_KEY_RIGHT);assert(m.open==1 && m.selected==6);assert(key(SRV_KEY_ENTER).action==4);
    key(SRV_KEY_F10);key(SRV_KEY_ESCAPE);assert(m.open<0);
    key(SRV_KEY_F10);struct srv_key_event cancel={.flags=SRV_KEY_CANCEL};desk_menu_key(&m,&cancel);assert(m.open<0);
    key(SRV_KEY_F10);
    memcpy(bad,items,sizeof(bad));bad[6].flags=0;
    assert(!desk_menu_replace(&m,bad,8) && m.open==0 && m.selected==1); /* Unrelated group update. */
    bad[1].flags=DESK_MENU_DISABLED;
    assert(!desk_menu_replace(&m,bad,8) && m.open==0 && m.selected==-1 && !m.armed);
    assert(!key(SRV_KEY_ENTER).action && m.open==0); /* No fallthrough into app. */
    assert(!desk_menu_replace(&m,items,8));key(SRV_KEY_HOME);
    key(SRV_KEY_RIGHT);assert(m.open==1);
    assert(!desk_menu_replace(&m,items,5) && m.open==0 && m.selected==-1 && !key(SRV_KEY_ENTER).action);
    assert(!desk_menu_replace(&m,items,8));key(SRV_KEY_HOME);
    for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) pixels[i]=tiles[i]=0xdeadbeef;
    struct desk_canvas a,b;assert(!desk_canvas_init(&a,pixels,W,H,STRIDE));assert(!desk_canvas_init(&b,tiles,W,H,STRIDE));
    desk_menu_bar_draw(&a,&m,&desk_theme_quiet);desk_menu_popup_draw(&a,&m,&desk_theme_quiet);
    for(int y=0;y<H;y+=19) for(int x=0;x<W;x+=23) {
        desk_clip(&b,(struct desk_rect){x,y,23,19});struct desk_rect clip=b.clip;
        desk_menu_bar_draw(&b,&m,&desk_theme_quiet);desk_menu_popup_draw(&b,&m,&desk_theme_quiet);
        assert(!memcmp(&clip,&b.clip,sizeof(clip)));
    }
    assert(!memcmp(pixels,tiles,sizeof(pixels)));
    for(int y=0;y<H;y++) for(int x=W;x<STRIDE;x++) assert(pixels[y*STRIDE+x]==0xdeadbeef);
    for(unsigned i=STRIDE*H;i<sizeof(pixels)/sizeof(*pixels);i++) assert(pixels[i]==0xdeadbeef);
    assert(!desk_menu_configure(&m,NULL,0,(struct desk_rect){0,0,W,44},(struct desk_rect){0,0,W,H},&desk_font_16));
    assert(m.open<0 && !key(SRV_KEY_F10).consumed);
    struct desk_menu_item many[25]={{.flags=DESK_MENU_HEADING,.label="Window"}};
    for(unsigned i=1;i<25;i++) { many[i].id=i;snprintf(many[i].label,sizeof(many[i].label),"Window %u",i); }
    assert(!desk_menu_configure(&m,many,25,(struct desk_rect){0,0,400,44},(struct desk_rect){0,0,640,480},&desk_font_16));
    key(SRV_KEY_F10);key(SRV_KEY_END);
    struct desk_rect last=desk_menu_row(&m,24),popup=desk_menu_popup(&m);
    assert(m.selected==24 && m.scroll>0 && last.h==30 && last.y+last.h<=popup.y+popup.h);
    assert(!desk_menu_replace(&m,many,25) && desk_menu_row(&m,24).h==30);
    assert(desk_menu_scroll(&m,-4096).consumed && !m.scroll && desk_menu_row(&m,1).h==30);
    assert(desk_menu_scroll(&m,4096).redraw && desk_menu_row(&m,24).h==30);
    desk_menu_cancel(&m);
    puts("menu: PASS snapshot validation, pointer release/drag/cancel, disabled rows, keyboard traversal, clipping/tiled rendering and guards");
}
