#include <srvros/desk_terminal.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct desk_terminal t,u;
static struct desk_term_line lines[80],alt[DESK_TERM_ROWS],other[80],other_alt[DESK_TERM_ROWS];
static void init(unsigned columns,unsigned rows) { assert(!desk_terminal_init(&t,lines,80,alt,columns,rows)); }
static void feed(const char *text) { desk_terminal_feed(&t,text,strlen(text)); }
static const struct desk_term_cell *cell(unsigned x,unsigned y) { return &desk_terminal_line(&t,desk_terminal_history(&t)+y)->cells[x]; }
static void row(unsigned y,const char *text) {
    for(unsigned x=0;text[x];x++) assert(cell(x,y)->codepoint==(unsigned char)text[x]);
}
static void check(struct desk_terminal *p) {
    assert(p->columns && p->columns<=DESK_TERM_COLUMNS && p->rows && p->rows<=DESK_TERM_ROWS);
    assert(p->head<p->capacity && p->history+p->rows<=p->capacity && p->x<p->columns && p->y<p->rows);
    assert(p->top<=p->bottom && p->bottom<p->rows && p->param_count<=16 && p->reply_size<=DESK_TERM_REPLY);
}
int main(void) {
    init(8,3);feed("abcdefgh");assert(t.x==7 && t.y==0 && t.wrap_pending);
    feed("I");row(0,"abcdefgh");row(1,"I");assert(lines[0].wrapped);
    feed("\rX\n\rthird\n\rlast");assert(t.history==1);row(0,"X");row(1,"third");row(2,"last");
    assert(desk_terminal_line(&t,0)->cells[0].codepoint=='a');
    init(12,4);feed("abcdef\r\033[2C\033[2P");row(0,"abef  ");
    feed("\033[2@Z");row(0,"abZ ef  ");
    feed("\033[2;4Hword\033[1K");row(1,"       ");
    feed("\033[3;2H\033[31;44;1;4mR\033[0mN");
    assert(cell(1,2)->codepoint=='R' && cell(1,2)->style==(DESK_TERM_BOLD|DESK_TERM_UNDERLINE));
    assert(cell(2,2)->style==0 && cell(1,2)->foreground!=cell(2,2)->foreground);
    feed("\033[38;2;10;20;30mT");assert(cell(3,2)->foreground==0x0a141e);
    feed("\033[48;5;196mB");assert(cell(4,2)->background==0xff0000);
    feed("\033[6n");char reply[80];size_t count=desk_terminal_reply(&t,reply,sizeof(reply));
    assert(count==6 && !memcmp(reply,"\033[3;6R",6));
    feed("\033[2J");for(unsigned y=0;y<4;y++) for(unsigned x=0;x<12;x++) assert(cell(x,y)->codepoint==' ');
    init(10,4);feed("one\r\ntwo\r\nthree\r\nfour");
    feed("\033[2;3r\033[3;1H\n");row(0,"one");row(1,"three");row(2," ");row(3,"four");assert(!t.history);
    feed("\033[2;1H\033M");row(1," ");row(2,"three");
    feed("\033[?6h\033[1;1Hx");assert(cell(0,1)->codepoint=='x');
    init(8,3);feed("main\033[31m\033[?1049hALT");assert(t.alt_active && !desk_terminal_history(&t));row(0,"ALT");
    feed("\033[32m\0337\033[?1049l");assert(!t.alt_active);row(0,"main");assert(t.x==4);
    feed("R");assert(cell(4,0)->foreground==0xb23a36);
    feed("\033[?25l\033[?1h\033[?2004h");assert(!t.cursor_visible && t.application_cursor && t.bracketed_paste);
    init(10,3);feed("a\xc3");assert(t.utf_remaining);feed("\xa9\xf0\x9f\x98\x80");
    assert(cell(1,0)->codepoint==0xe9 && cell(2,0)->codepoint==0x1f600);
    feed("\xe2X\xc0");assert(cell(3,0)->codepoint==0xfffd && cell(4,0)->codepoint=='X' && cell(5,0)->codepoint==0xfffd);
    feed("\033]52;c;ignored\007Z");assert(cell(6,0)->codepoint=='Z');
    feed("\033(0q\033(B");assert(cell(7,0)->codepoint==0x2500);
    init(10,4);feed("one\r\ntwo\r\nthree\r\nfour");
    assert(!desk_terminal_resize(&t,6,2) && t.history==2 && t.y==1);row(0,"three");row(1,"four");
    assert(!desk_terminal_resize(&t,12,4) && !t.history && t.y==3);row(0,"one");row(3,"four");
    assert(cell(7,3)->codepoint==' ');
    for(unsigned i=0;i<200;i++) feed("\r\nline");
    assert(t.history==76 && t.discarded>1);check(&t);
    assert(!desk_terminal_resize(&t,80,64));check(&t);assert(!desk_terminal_resize(&t,1,1));check(&t);
    /* Fragmentation cannot change interpretation, including UTF-8, strings,
     * palette sequences and cursor addressing. */
    const char *stream="start\033[2;3H\xc3\xa9\033[38;2;12;13;14mX\033]0;ignored\033\\Y\033[6n\033[?1049halt\033[?1049l!";
    init(40,10);assert(!desk_terminal_init(&u,other,80,other_alt,40,10));
    desk_terminal_feed(&t,stream,strlen(stream));
    for(unsigned i=0;stream[i];i++) desk_terminal_feed(&u,stream+i,1);
    assert(t.x==u.x && t.y==u.y && t.history==u.history && t.foreground==u.foreground && t.reply_size==u.reply_size);
    assert(!memcmp(lines,other,sizeof(lines)) && !memcmp(alt,other_alt,sizeof(alt)) && !memcmp(t.reply,u.reply,t.reply_size));
    /* Arbitrary output plus adversarial geometry always stays in storage. */
    uint32_t random=0x31415926;
    for(unsigned i=0;i<200000;i++) {
        random=random*1664525+1013904223;uint8_t c=random>>24;desk_terminal_feed(&t,&c,1);
        if(!(i%997)) desk_terminal_resize(&t,1+random%160,1+(random>>8)%64);
        check(&t);
    }
    puts("terminal: PASS wrap/scrollback, ANSI cursor/erase/insert/color/margins, alternate screen, UTF-8 fragmentation, ignored strings, replies, resize and fuzz bounds");
}
