#include <srvros/desk_ui.h>
#include <srvros/desk_flow.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct desk_document document;
static char prose[3000];
int main(void) {
    for(unsigned i=0;i<48;i++) strcat(prose,"A quiet desktop with clear controls. Words wrap naturally. ");
    struct desk_document_block blocks[]={
        {1,DESK_DOCUMENT_TITLE,"Welcome to SrvrOS"},
        {2,DESK_DOCUMENT_PARAGRAPH,prose},
        {3,DESK_DOCUMENT_HEADING,"Keyboard"},
        {4,DESK_DOCUMENT_CODE,"Ctrl-O\tOpen\nCtrl-S\tSave\nUTF-8: café — π"},
        {5,DESK_DOCUMENT_NOTE,"Keep a copy of important work on another volume."},
    };
    desk_document_init(&document);desk_document_configure(&document,(struct desk_rect){10,10,340,240},&desk_font_16,1);
    assert(!desk_document_set(&document,blocks,5));assert(document.count==5 && document.scroll.content>1000);
    struct desk_document saved=document;blocks[1].id=1;assert(desk_document_set(&document,blocks,5)<0 && !memcmp(&saved,&document,sizeof(saved)));blocks[1].id=2;
    blocks[4].text="bad\033";assert(desk_document_set(&document,blocks,5)<0);blocks[4].text="bad\xc0\x80";assert(desk_document_set(&document,blocks,5)<0);
    blocks[4].text="Good";blocks[4].kind=(enum desk_document_kind)99;assert(desk_document_set(&document,blocks,5)<0);
    char text[4096];memset(text,0x4a,sizeof(text));assert(desk_document_text(&document,text,10)<0 && text[0]==0x4a);
    long length=desk_document_text(&document,NULL,0);assert(length>2000 && length<4096);assert(desk_document_text(&document,text,sizeof(text))==length);
    assert(!strncmp(text,"Welcome to SrvrOS\n\n",18) && strstr(text,"Ctrl-O\tOpen\nCtrl-S\tSave\nUTF-8: café — π\n\n"));
    assert(desk_document_jump(&document,3));assert(document.scroll.offset==(document.layout[2].y<desk_scrollbar_limit(&document.scroll)?document.layout[2].y:desk_scrollbar_limit(&document.scroll)));
    desk_document_scroll(&document,INT32_MAX);assert(document.scroll.offset==desk_scrollbar_limit(&document.scroll));
    desk_document_scroll(&document,INT32_MIN);assert(!document.scroll.offset);
    struct desk_ui ui;struct desk_button button={0};desk_ui_init(&ui);assert(!desk_ui_add(&ui,1,DESK_CONTROL_DOCUMENT,&document,NULL));
    desk_button_configure(&button,(struct desk_rect){10,260,80,28},1);desk_ui_add(&ui,2,DESK_CONTROL_BUTTON,&button,"Topics");desk_ui_focus(&ui,1);
    struct srv_key_event key={.usage=SRV_KEY_PAGEDOWN,.flags=SRV_KEY_DOWN_EVENT};assert(desk_ui_key(&ui,&key).flags&DESK_WIDGET_REDRAW);assert(document.scroll.offset>0);
    key.usage=SRV_KEY_TAB;desk_ui_key(&ui,&key);assert(button.focused && !document.focused);key.modifiers=SRV_KEY_SHIFT;desk_ui_key(&ui,&key);assert(document.focused);
    struct desk_scrollbar_geometry geometry=desk_scrollbar_geometry(&document.scroll);
    struct desk_widget_event event={.type=DESK_WIDGET_POINTER,.x=geometry.thumb.x+2,.y=geometry.thumb.y+2,.down=1,.pressed=1};desk_ui_pointer(&ui,event,0);assert(document.scroll.dragging);
    event.y+=60;event.pressed=0;desk_ui_pointer(&ui,event,0);assert(document.scroll.offset>200);event.down=0;desk_ui_pointer(&ui,event,0);assert(!document.scroll.dragging);
    desk_document_event(&document,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_OUT});assert(!document.focused);
    static uint32_t a[420*340],b[420*340];struct desk_canvas full,tiled;
    for(unsigned mode=0;mode<5;mode++) {
        struct desk_rect frame=mode==3?(struct desk_rect){-15,-10,370,280}:mode==4?(struct desk_rect){1,1,8,8}:(struct desk_rect){10,10,mode==2?180:340,240};
        desk_document_configure(&document,frame,mode==1?&desk_font_20:&desk_font_16,1);
        if(mode==1) desk_document_jump(&document,3);else if(mode==2) desk_document_scroll(&document,-INT32_MAX);
        memset(a,0,sizeof(a));memset(b,0,sizeof(b));desk_canvas_init(&full,a,400,320,420);desk_canvas_init(&tiled,b,400,320,420);desk_document_draw(&full,&document,&desk_theme_quiet);
        for(int y=0;y<320;y+=39) for(int x=0;x<400;x+=43) { desk_clip(&tiled,(struct desk_rect){x,y,43,39});struct desk_rect clip=tiled.clip;desk_document_draw(&tiled,&document,&desk_theme_quiet);assert(!memcmp(&clip,&tiled.clip,sizeof(clip))); }
        assert(!memcmp(a,b,sizeof(a)));
        for(unsigned y=0;y<340;y++) for(unsigned x=0;x<420;x++) if(x>=400 || y>=320) assert(!a[y*420+x]);
    }
    assert(desk_text_block_height(&desk_font_16,"a\tb",500,10)==desk_font_16.line_height);
    static char maximum[DESK_DOCUMENT_BLOCK_BYTES+1];memset(maximum,'x',sizeof(maximum));maximum[DESK_DOCUMENT_BLOCK_BYTES-1]=0;
    struct desk_document_block limits[9];for(unsigned i=0;i<9;i++) limits[i]=(struct desk_document_block){i+1,DESK_DOCUMENT_PARAGRAPH,maximum};
    assert(!desk_document_set(&document,limits,8));saved=document;
    assert(desk_document_set(&document,limits,9)<0 && !memcmp(&saved,&document,sizeof(saved)));
    maximum[DESK_DOCUMENT_BLOCK_BYTES-1]='x';maximum[DESK_DOCUMENT_BLOCK_BYTES]=0;
    assert(desk_document_set(&document,limits,1)<0);maximum[DESK_DOCUMENT_BLOCK_BYTES-1]=0;
    limits[0].id=0;assert(desk_document_set(&document,limits,1)<0);limits[0].id=1;
    assert(desk_document_set(&document,NULL,1)<0 && desk_document_set(&document,limits,DESK_DOCUMENT_BLOCKS+1)<0);
    desk_document_configure(&document,(struct desk_rect){10,10,64,32},&desk_font_20,1);assert(document.scroll.content>100000);
    desk_document_scroll(&document,INT32_MAX);desk_canvas_init(&full,a,400,320,420);desk_document_draw(&full,&document,&desk_theme_quiet);
    assert(!desk_document_set(&document,NULL,0) && !desk_document_text(&document,text,sizeof(text)) && !text[0]);
    puts("document-view: PASS atomic validation, plain text, wrapping/reflow, anchors, scroll bounds/capture, focus, clipping, tiled rendering and stride guards");
}
