#include <srvros/desk_terminal_view.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define WIDTH 240
#define HEIGHT 150
#define STRIDE 247
static struct desk_terminal terminal;
static struct desk_term_line lines[80],alternate[DESK_TERM_ROWS];
static struct desk_terminal_view view;
static uint32_t full[STRIDE*HEIGHT+16],tiled[STRIDE*HEIGHT+16];
static void feed(const char *text) { desk_terminal_feed(&terminal,text,strlen(text));desk_terminal_view_sync(&view); }
static void key(unsigned usage,unsigned modifiers,unsigned cp,const char *expected,size_t length) {
    struct srv_key_event k={.usage=usage,.modifiers=modifiers,.codepoint=cp,.flags=SRV_KEY_DOWN_EVENT};char bytes[32];
    memset(bytes,0x55,sizeof(bytes));assert(desk_terminal_key(&terminal,&k,bytes,sizeof(bytes))==length);
    assert(!memcmp(bytes,expected,length));
    if(length) { memset(bytes,0x55,sizeof(bytes));assert(!desk_terminal_key(&terminal,&k,bytes,length-1) && bytes[0]==0x55); }
    k.flags=0;assert(!desk_terminal_key(&terminal,&k,bytes,sizeof(bytes)));
}
int main(void) {
    for(unsigned i=0;i<desk_font_mono_16.count;i++) assert(desk_font_mono_16.glyphs[i].advance==10);
    assert(!desk_terminal_init(&terminal,lines,80,alternate,20,1));
    desk_terminal_view_init(&view,&terminal);desk_terminal_view_configure(&view,(struct desk_rect){5,5,230,140},1);
    assert(view.columns==20 && view.rows>0);
    feed("Hello\r\nWorld");view.anchor=0;view.caret=25;
    char text[100];assert(desk_terminal_view_copy(&view,text,sizeof(text))==11 && !strcmp(text,"Hello\nWorld"));
    char small[4]="old";assert(desk_terminal_view_copy(&view,small,sizeof(small))<0 && !strcmp(small,"old"));
    desk_terminal_reset(&terminal);feed("abcdefghijklmnopqrstUVW");view.anchor=0;view.caret=23;
    assert(desk_terminal_view_copy(&view,text,sizeof(text))==23 && !strcmp(text,"abcdefghijklmnopqrstUVW"));
    for(unsigned i=0;i<100;i++) feed("\r\nline");
    assert(view.scroll.offset==(int)(terminal.history*view.cell_height));
    desk_terminal_view_scroll(&view,-3*(int)view.cell_height);int before=view.scroll.offset;
    feed("\r\nmore");assert(view.scroll.offset==before-(int)view.cell_height); /* One retained row evicted. */
    desk_terminal_view_follow(&view);assert(view.scroll.offset==(int)(terminal.history*view.cell_height));
    desk_terminal_view_event(&view,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});
    assert(view.focused);
    desk_terminal_view_event(&view,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=view.content.x,.y=view.content.y,.down=1,.pressed=1});
    assert(view.dragging);
    desk_terminal_view_event(&view,(struct desk_widget_event){.type=DESK_WIDGET_POINTER,.x=view.content.x+30,.y=view.content.y,.down=0});
    assert(!view.dragging && view.caret-view.anchor==3);
    desk_terminal_view_event(&view,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_OUT});assert(!view.focused);
    key(4,0,'a',"a",1);key(4,SRV_KEY_CTRL,'a',"\001",1);key(SRV_KEY_SPACE,SRV_KEY_CTRL,' ',"\0",1);
    key(SRV_KEY_ENTER,0,0,"\r",1);key(SRV_KEY_BACKSPACE,0,0,"\177",1);
    key(SRV_KEY_UP,0,0,"\033[A",3);key(SRV_KEY_LEFT,SRV_KEY_CTRL,0,"\033[1;5D",6);
    key(SRV_KEY_TAB,SRV_KEY_SHIFT,0,"\033[Z",3);key(SRV_KEY_DELETE,0,0,"\033[3~",4);
    key(SRV_KEY_F1,0,0,"\033OP",3);key(SRV_KEY_F12,0,0,"\033[24~",5);
    key(4,SRV_KEY_ALT,'a',"\033a",2);key(4,0,0xe9,"\xc3\xa9",2);key(4,SRV_KEY_META,'a',"",0);
    feed("\033[?1h");key(SRV_KEY_UP,0,0,"\033OA",3);
    /* Full and tiled draws are identical; guard stride and incoming clip stay intact. */
    feed("\033[31;1mred\033[4;7mreverse\033[0m\033[2;3H\xc3\xa9");view.anchor=0;view.caret=42;
    for(unsigned i=0;i<sizeof(full)/sizeof(*full);i++) full[i]=tiled[i]=0xdeadbeef;
    struct desk_canvas a,b;assert(!desk_canvas_init(&a,full,WIDTH,HEIGHT,STRIDE));assert(!desk_canvas_init(&b,tiled,WIDTH,HEIGHT,STRIDE));
    desk_terminal_view_draw(&a,&view,&desk_theme_quiet,1);
    for(int y=0;y<HEIGHT;y+=17) for(int x=0;x<WIDTH;x+=19) {
        desk_clip(&b,(struct desk_rect){x,y,19,17});struct desk_rect original=b.clip;
        desk_terminal_view_draw(&b,&view,&desk_theme_quiet,1);assert(!memcmp(&b.clip,&original,sizeof(original)));
    }
    assert(!memcmp(full,tiled,sizeof(full)));
    for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=WIDTH;x<STRIDE;x++) assert(full[y*STRIDE+x]==0xdeadbeef);
    for(unsigned i=STRIDE*HEIGHT;i<sizeof(full)/sizeof(*full);i++) assert(full[i]==0xdeadbeef);
    desk_terminal_view_configure(&view,(struct desk_rect){0,0,1,1},1);desk_terminal_view_draw(&a,&view,&desk_theme_quiet,1);
    assert(view.columns==1 && view.rows==1 && !view.dragging && !view.caret);
    puts("terminal-view: PASS selection/UTF-8 copy, soft wraps, scroll following/eviction, focus/capture, key encoding, clipped/tiled drawing and guards");
}
