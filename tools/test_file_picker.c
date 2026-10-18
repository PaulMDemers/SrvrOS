#include <srvros/desk_file_picker.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct desk_file_picker p;
static const struct desk_list_item listing[]={ {"Folder","Folder",1,1},{"notes.txt","12 bytes",2,0},{"café.txt","20 bytes",3,0} };
static struct desk_picker_result key(unsigned usage,unsigned mods,unsigned cp) {
    struct srv_key_event k={.usage=usage,.modifiers=mods,.codepoint=cp,.flags=SRV_KEY_DOWN_EVENT};return desk_picker_key(&p,&k);
}
static void name(const char *text) { desk_text_buffer_set(&p.name_text,text,strlen(text));desk_ui_focus(&p.ui,DESK_PICKER_FILENAME); }
#define W 640
#define H 480
#define S 647
static uint32_t pixels[S*H+16],tiles[S*H+16];
int main(void) {
    desk_picker_init(&p);desk_picker_configure(&p,(struct desk_rect){0,0,W,H});
    assert(!desk_picker_open(&p,DESK_PICKER_OPEN,"/fat/",""));assert(!strcmp(p.directory,"/fat"));
    desk_picker_loading(&p);assert(p.busy && !p.location.enabled && !p.list.enabled);
    assert(key(SRV_KEY_ESCAPE,0,0).operation==DESK_PICKER_CANCEL);
    desk_picker_focus(&p,0);assert(key(SRV_KEY_ESCAPE,0,0).operation==DESK_PICKER_NONE);desk_picker_focus(&p,1);
    assert(!desk_picker_listing(&p,"/fat",listing,3));
    key(SRV_KEY_HOME,0,0);struct desk_picker_result a=key(SRV_KEY_ENTER,0,0);
    assert(a.operation==DESK_PICKER_BROWSE && !strcmp(p.target,"/fat/Folder"));
    desk_picker_loading(&p);assert(!desk_picker_listing(&p,"/fat/Folder",listing,3) && !p.name_text.length);
    a=key(SRV_KEY_UP,SRV_KEY_ALT,0);assert(a.operation==DESK_PICKER_BROWSE && !strcmp(p.target,"/fat"));
    key(SRV_KEY_END,0,0);a=key(SRV_KEY_ENTER,0,0);assert(a.operation==DESK_PICKER_ACCEPT && !strcmp(p.target,"/fat/Folder/café.txt"));
    key(0x0f,SRV_KEY_CTRL,0);assert(p.location_text.anchor==0 && p.location_text.caret==p.location_text.length);
    desk_text_buffer_set(&p.location_text,"/a/../another//./",17);a=key(SRV_KEY_ENTER,0,0);assert(a.operation==DESK_PICKER_BROWSE && !strcmp(p.target,"/another"));
    desk_picker_loading(&p);desk_picker_error(&p,"Folder is unavailable.");assert(!p.busy && !strcmp(p.location_text.data,"/fat/Folder"));
    assert(!desk_picker_open(&p,DESK_PICKER_SAVE,"/","Untitled.txt"));assert(!desk_picker_listing(&p,"/",listing,3));
    assert(p.name_text.anchor==0 && p.name_text.caret==p.name_text.length);
    name("saved.txt");a=key(SRV_KEY_ENTER,0,0);assert(a.operation==DESK_PICKER_ACCEPT && !strcmp(p.target,"/saved.txt"));
    name("../escape.txt");assert(key(SRV_KEY_ENTER,0,0).operation==DESK_PICKER_NONE);
    name("..");assert(key(SRV_KEY_ENTER,0,0).operation==DESK_PICKER_NONE);
    char long_path[160];memset(long_path,'a',159);long_path[0]='/';long_path[159]=0;
    assert(!desk_picker_open(&p,DESK_PICKER_SAVE,long_path,"notes.txt"));name("notes.txt");assert(key(SRV_KEY_ENTER,0,0).operation==DESK_PICKER_NONE);
    assert(desk_picker_open(&p,DESK_PICKER_SAVE,"relative","x")<0);
    assert(desk_picker_open(&p,DESK_PICKER_SAVE,"/fat","a/b")<0);
    assert(!desk_picker_open(&p,DESK_PICKER_SAVE,"/fat","notes.txt"));assert(!desk_picker_listing(&p,"/fat",listing,3));
    desk_ui_focus(&p.ui,DESK_PICKER_FILENAME);desk_text_buffer_select(&p.name_text,0,p.name_text.length);
    a=key(6,SRV_KEY_CTRL,0);assert(a.field==DESK_PICKER_FILENAME && (a.flags&DESK_EDIT_COPY));
    for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) pixels[i]=tiles[i]=0xdeadbeef;
    struct desk_canvas full,tile;assert(!desk_canvas_init(&full,pixels,W,H,S));assert(!desk_canvas_init(&tile,tiles,W,H,S));
    desk_picker_draw(&full,&p,&desk_theme_quiet);
    for(int y=0;y<H;y+=19) for(int x=0;x<W;x+=23) {
        desk_clip(&tile,(struct desk_rect){x,y,23,19});struct desk_rect clip=tile.clip;
        desk_picker_draw(&tile,&p,&desk_theme_quiet);assert(!memcmp(&clip,&tile.clip,sizeof(clip)));
    }
    assert(!memcmp(pixels,tiles,sizeof(pixels)));
    for(int y=0;y<H;y++) for(int x=W;x<S;x++) assert(pixels[y*S+x]==0xdeadbeef);
    for(unsigned i=S*H;i<sizeof(pixels)/sizeof(*pixels);i++) assert(pixels[i]==0xdeadbeef);
    desk_picker_configure(&p,(struct desk_rect){0,0,1,1});desk_picker_draw(&full,&p,&desk_theme_quiet);
    assert(!p.name.enabled && !p.list.enabled && key(SRV_KEY_ESCAPE,0,0).operation==DESK_PICKER_CANCEL);
    puts("file-picker: PASS async browse/cancel, parent/location, open/save paths, UTF-8 names, lexical bounds, clipboard actions and clipped/tiled rendering");
}
