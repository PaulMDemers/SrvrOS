#include <srvros/desk_terminal.h>
#include <string.h>
/* Sequence meanings follow https://invisible-island.net/xterm/ctlseqs/ctlseqs.html.
 * Only the explicitly implemented subset below is advertised by the app. */
#define DEFAULT_FG 0x303a34u
#define DEFAULT_BG 0xf9f7f1u
static unsigned minimum(unsigned a,unsigned b) { return a<b?a:b; }
static struct desk_term_line *line(struct desk_terminal *t,unsigned row) {
    return t->alt_active?&t->alternate[row]:&t->lines[(t->head+t->history+row)%t->capacity];
}
unsigned desk_terminal_history(const struct desk_terminal *t) { return t->alt_active?0:t->history; }
const struct desk_term_line *desk_terminal_line(const struct desk_terminal *t,unsigned row) {
    if(row>=desk_terminal_history(t)+t->rows) return NULL;
    return t->alt_active?&t->alternate[row]:&t->lines[(t->head+row)%t->capacity];
}
static struct desk_term_cell blank(const struct desk_terminal *t) {
    return (struct desk_term_cell){' ',t->foreground,t->background,0};
}
static void erase(struct desk_terminal *t,unsigned row,unsigned first,unsigned end) {
    struct desk_term_line *l=line(t,row);
    for(unsigned x=first;x<end;x++) l->cells[x]=blank(t);
    l->wrapped=0;
}
static void clear_line(struct desk_terminal *t,unsigned row) { erase(t,row,0,DESK_TERM_COLUMNS); }
static void defaults(struct desk_terminal *t) {
    t->foreground=DEFAULT_FG;t->background=DEFAULT_BG;t->style=0;
}
void desk_terminal_reset(struct desk_terminal *t) {
    t->head=t->history=t->x=t->y=t->main_x=t->main_y=t->saved_x=t->saved_y=0;
    t->alt_active=t->wrap_pending=t->origin=t->insert=t->application_cursor=t->bracketed_paste=0;
    t->state=t->private_mode=t->param_count=t->param_overflow=t->charset=t->charset_target=t->utf_remaining=0;
    t->reply_size=0;t->wrap=t->cursor_visible=1;t->top=0;t->bottom=t->rows-1;defaults(t);
    t->saved_foreground=t->foreground;t->saved_background=t->background;t->saved_style=0;
    for(unsigned x=0;x<DESK_TERM_COLUMNS;x++) t->tabs[x]=x && !(x%8);
    for(unsigned y=0;y<t->capacity;y++) {
        t->lines[y].wrapped=0;
        for(unsigned x=0;x<DESK_TERM_COLUMNS;x++) t->lines[y].cells[x]=blank(t);
    }
    for(unsigned y=0;y<DESK_TERM_ROWS;y++) {
        t->alternate[y].wrapped=0;
        for(unsigned x=0;x<DESK_TERM_COLUMNS;x++) t->alternate[y].cells[x]=blank(t);
    }
    t->discarded++;t->revision++;
}
int desk_terminal_init(struct desk_terminal *t,struct desk_term_line *lines,unsigned capacity,
    struct desk_term_line *alternate,unsigned columns,unsigned rows) {
    if(!t || !lines || !alternate || capacity<DESK_TERM_ROWS || capacity>4096 ||
       !columns || columns>DESK_TERM_COLUMNS || !rows || rows>DESK_TERM_ROWS) return -1;
    *t=(struct desk_terminal){.lines=lines,.alternate=alternate,.capacity=capacity,.columns=columns,.rows=rows};
    desk_terminal_reset(t);return 0;
}
int desk_terminal_resize(struct desk_terminal *t,unsigned columns,unsigned rows) {
    if(!columns || columns>DESK_TERM_COLUMNS || !rows || rows>DESK_TERM_ROWS) return -1;
    if(columns==t->columns && rows==t->rows) return 0;
    if(columns>t->columns) {
        for(unsigned y=0;y<t->capacity;y++) for(unsigned x=t->columns;x<columns;x++) t->lines[y].cells[x]=blank(t);
        for(unsigned y=0;y<DESK_TERM_ROWS;y++) for(unsigned x=t->columns;x<columns;x++) t->alternate[y].cells[x]=blank(t);
    }
    /* Keep the cursor visible, moving top rows into history only when needed.
     * Widening does not reflow history. */
    unsigned *cursor=t->alt_active?&t->main_y:&t->y;
    if(rows<t->rows) { unsigned shifted=*cursor>=rows?*cursor-rows+1:0;
        t->history+=shifted;*cursor-=shifted; }
    else {
        unsigned extra=rows-t->rows,pull=minimum(extra,t->history);
        t->history-=pull;*cursor+=pull;
        unsigned old_alt=t->alt_active;t->alt_active=0;
        for(unsigned y=t->rows+pull;y<rows;y++) clear_line(t,y);
        t->alt_active=old_alt;
    }
    if(t->alt_active && rows>t->rows) for(unsigned y=t->rows;y<rows;y++) clear_line(t,y);
    t->columns=columns;t->rows=rows;t->x=minimum(t->x,columns-1);t->y=minimum(t->y,rows-1);
    t->main_x=minimum(t->main_x,columns-1);t->main_y=minimum(t->main_y,rows-1);
    t->top=0;t->bottom=rows-1;t->wrap_pending=0;t->revision++;return 0;
}
static void scroll_up(struct desk_terminal *t,unsigned top,unsigned bottom,unsigned count) {
    count=minimum(count,bottom-top+1);
    for(unsigned n=0;n<count;n++) {
        if(!top && bottom==t->rows-1 && !t->alt_active) {
            if(t->history+t->rows<t->capacity) t->history++;
            else { t->head=(t->head+1)%t->capacity;t->discarded++; }
        } else for(unsigned y=top;y<bottom;y++) *line(t,y)=*line(t,y+1);
        clear_line(t,bottom);
    }
}
static void scroll_down(struct desk_terminal *t,unsigned top,unsigned bottom,unsigned count) {
    count=minimum(count,bottom-top+1);
    for(unsigned n=0;n<count;n++) {
        for(unsigned y=bottom;y>top;y--) *line(t,y)=*line(t,y-1);
        clear_line(t,top);
    }
}
static void newline(struct desk_terminal *t) {
    t->wrap_pending=0;
    if(t->y==t->bottom) scroll_up(t,t->top,t->bottom,1);
    else if(t->y+1<t->rows) t->y++;
}
static uint32_t graphic(uint32_t c) {
    switch(c) {
    case 'j':return 0x2518;case 'k':return 0x2510;case 'l':return 0x250c;case 'm':return 0x2514;
    case 'n':return 0x253c;case 'q':return 0x2500;case 't':return 0x251c;case 'u':return 0x2524;
    case 'v':return 0x2534;case 'w':return 0x252c;case 'x':return 0x2502;default:return c;
    }
}
static void character(struct desk_terminal *t,uint32_t cp) {
    if(t->wrap_pending) { line(t,t->y)->wrapped=1;t->x=0;newline(t); }
    if(t->charset && cp<128) cp=graphic(cp);
    struct desk_term_line *l=line(t,t->y);
    if(t->insert) for(unsigned x=t->columns-1;x>t->x;x--) l->cells[x]=l->cells[x-1];
    l->cells[t->x]=(struct desk_term_cell){cp,t->foreground,t->background,t->style};
    if(t->x+1<t->columns) t->x++;else t->wrap_pending=t->wrap;
}
static unsigned argument(const struct desk_terminal *t,unsigned i,unsigned fallback) {
    return i<t->param_count && t->params[i]?t->params[i]:fallback;
}
static void move(struct desk_terminal *t,unsigned x,unsigned y) {
    t->x=minimum(x,t->columns-1);t->y=minimum(y,t->rows-1);t->wrap_pending=0;
}
static uint32_t palette(unsigned index) {
    static const uint32_t colors[]={0x202820,0xb23a36,0x397344,0x927019,0x365ea1,0x885a95,0x297d7a,0xe2e0d8,
        0x687268,0xdb5750,0x66a06a,0xc6a24c,0x7196d1,0xb087bd,0x70adaa,0xffffff};
    if(index<16) return colors[index];
    if(index<232) { unsigned n=index-16,r=n/36,g=n/6%6,b=n%6;return (r?55+40*r:0)<<16 | (g?55+40*g:0)<<8 | (b?55+40*b:0); }
    unsigned gray=8+10*minimum(index-232,23);return gray<<16|gray<<8|gray;
}
static void rendition(struct desk_terminal *t) {
    for(unsigned i=0;i<t->param_count;i++) {
        unsigned p=t->params[i];
        if(!p) defaults(t);
        else if(p==1) t->style|=DESK_TERM_BOLD;
        else if(p==4) t->style|=DESK_TERM_UNDERLINE;
        else if(p==7) t->style|=DESK_TERM_REVERSE;
        else if(p==22) t->style&=~DESK_TERM_BOLD;
        else if(p==24) t->style&=~DESK_TERM_UNDERLINE;
        else if(p==27) t->style&=~DESK_TERM_REVERSE;
        else if(p>=30 && p<=37) t->foreground=palette(p-30);
        else if(p>=40 && p<=47) t->background=palette(p-40);
        else if(p>=90 && p<=97) t->foreground=palette(p-90+8);
        else if(p>=100 && p<=107) t->background=palette(p-100+8);
        else if(p==39) t->foreground=DEFAULT_FG;
        else if(p==49) t->background=DEFAULT_BG;
        else if((p==38 || p==48) && i+1<t->param_count) {
            uint32_t color;unsigned mode=t->params[++i];
            if(mode==5 && i+1<t->param_count) color=palette(minimum(t->params[++i],255));
            else if(mode==2 && i+3<t->param_count) {
                color=minimum(t->params[i+1],255)<<16|minimum(t->params[i+2],255)<<8|minimum(t->params[i+3],255);i+=3;
            } else break;
            if(p==38) t->foreground=color;else t->background=color;
        }
    }
}
static void save_cursor(struct desk_terminal *t) {
    t->saved_x=t->x;t->saved_y=t->y;t->saved_foreground=t->foreground;t->saved_background=t->background;t->saved_style=t->style;
}
static void restore_cursor(struct desk_terminal *t) {
    move(t,t->saved_x,t->saved_y);t->foreground=t->saved_foreground;t->background=t->saved_background;t->style=t->saved_style;
}
static void alternate(struct desk_terminal *t,unsigned enabled,unsigned clear) {
    if(enabled==t->alt_active) return;
    if(enabled) {
        t->main_x=t->x;t->main_y=t->y;
        t->main_foreground=t->foreground;t->main_background=t->background;t->main_style=t->style;
        t->alt_active=1;t->x=t->y=0;
        if(clear) for(unsigned y=0;y<t->rows;y++) clear_line(t,y);
    } else { t->alt_active=0;move(t,t->main_x,t->main_y);
        t->foreground=t->main_foreground;t->background=t->main_background;t->style=t->main_style; }
    t->top=0;t->bottom=t->rows-1;t->wrap_pending=0;
}
static void reply_text(struct desk_terminal *t,const char *text) {
    size_t length=strlen(text);if(length>DESK_TERM_REPLY-t->reply_size) return;
    memcpy(t->reply+t->reply_size,text,length);t->reply_size+=length;
}
static unsigned decimal(char *out,unsigned number) {
    char reverse[10];unsigned count=0;do { reverse[count++]='0'+number%10;number/=10; } while(number);
    for(unsigned i=0;i<count;i++) out[i]=reverse[count-1-i];
    return count;
}
static void sequence(struct desk_terminal *t,uint8_t final) {
    if(t->param_overflow) return;
    unsigned n=argument(t,0,1),p=t->params[0],low=t->origin?t->top:0,high=t->origin?t->bottom:t->rows-1;
    if(final=='h' || final=='l') {
        for(unsigned i=0;i<t->param_count;i++) {
            unsigned mode=t->params[i],set=final=='h';
            if(!t->private_mode) { if(mode==4) t->insert=set;continue; }
            if(mode==1) t->application_cursor=set;
            else if(mode==6) { t->origin=set;move(t,0,set?t->top:0); }
            else if(mode==7) { t->wrap=set;t->wrap_pending=0; }
            else if(mode==25) t->cursor_visible=set;
            else if(mode==47 || mode==1047 || mode==1049) {
                alternate(t,set,mode!=47);
            } else if(mode==2004) t->bracketed_paste=set;
        }
        return;
    }
    if(t->private_mode) return;
    switch(final) {
    case 'A':move(t,t->x,t->y>low+n?t->y-n:low);break;
    case 'B':case 'e':move(t,t->x,minimum(high,t->y+n));break;
    case 'C':case 'a':move(t,t->x+n,t->y);break;
    case 'D':move(t,t->x>n?t->x-n:0,t->y);break;
    case 'E':move(t,0,minimum(high,t->y+n));break;
    case 'F':move(t,0,t->y>low+n?t->y-n:low);break;
    case 'G':case '`':move(t,n-1,t->y);break;
    case 'd':move(t,t->x,minimum(high,low+n-1));break;
    case 'H':case 'f':move(t,argument(t,1,1)-1,minimum(high,low+n-1));break;
    case 'J':
        if(p==0) { erase(t,t->y,t->x,t->columns);for(unsigned y=t->y+1;y<t->rows;y++) clear_line(t,y); }
        else if(p==1) { for(unsigned y=0;y<t->y;y++) clear_line(t,y);erase(t,t->y,0,t->x+1); }
        else if(p==2) for(unsigned y=0;y<t->rows;y++) clear_line(t,y);
        else if(p==3 && !t->alt_active) { t->head=(t->head+t->history)%t->capacity;t->history=0;t->discarded++; }
        break;
    case 'K':if(p==0) erase(t,t->y,t->x,t->columns);else if(p==1) erase(t,t->y,0,t->x+1);else if(p==2) clear_line(t,t->y);break;
    case 'm':rendition(t);break;
    case 'r': {
        unsigned top=argument(t,0,1)-1,bottom=argument(t,1,t->rows)-1;
        if(top<bottom && bottom<t->rows) { t->top=top;t->bottom=bottom;move(t,0,t->origin?top:0); }break;
    }
    case 'S':scroll_up(t,t->top,t->bottom,n);break;
    case 'T':scroll_down(t,t->top,t->bottom,n);break;
    case 'L':if(t->y>=t->top && t->y<=t->bottom) scroll_down(t,t->y,t->bottom,n);break;
    case 'M':if(t->y>=t->top && t->y<=t->bottom) scroll_up(t,t->y,t->bottom,n);break;
    case '@': {
        n=minimum(n,t->columns-t->x);struct desk_term_line *l=line(t,t->y);
        for(unsigned x=t->columns;x>t->x+n;x--) l->cells[x-1]=l->cells[x-n-1];
        erase(t,t->y,t->x,t->x+n);break;
    }
    case 'P': {
        n=minimum(n,t->columns-t->x);struct desk_term_line *l=line(t,t->y);
        for(unsigned x=t->x;x<t->columns-n;x++) l->cells[x]=l->cells[x+n];
        erase(t,t->y,t->columns-n,t->columns);break;
    }
    case 'X':erase(t,t->y,t->x,minimum(t->columns,t->x+n));break;
    case 's':save_cursor(t);break;case 'u':restore_cursor(t);break;
    case 'g':if(!p) t->tabs[t->x]=0;else if(p==3) memset(t->tabs,0,sizeof(t->tabs));break;
    case 'n':
        if(p==5) reply_text(t,"\033[0n");
        else if(p==6) { char result[32]="\033[";unsigned size=2;size+=decimal(result+size,t->y-low+1);result[size++]=';';size+=decimal(result+size,t->x+1);result[size++]='R';result[size]=0;reply_text(t,result); }
        break;
    case 'c':if(!p) reply_text(t,"\033[?1;0c");break;
    default:break;
    }
}
/* ESC/CSI/string states are incremental across arbitrary read boundaries.
 * Overlong strings are consumed without storing them; malformed CSI is ignored
 * through its final byte. OSC never executes clipboard/title/file operations. */
static void byte(struct desk_terminal *t,uint8_t c) {
again:
    if(t->state==3) { if(c==7) t->state=0;else if(c==27) t->state=4;return; }
    if(t->state==4) { if(c=='\\') t->state=0;else t->state=c==27?4:3;return; }
    if(c==24 || c==26) { t->state=t->utf_remaining=0;return; }
    if(c==27) { if(t->utf_remaining) { t->utf_remaining=0;character(t,0xfffd); }t->state=1;return; }
    if(t->state==1) {
        t->state=0;
        if(c=='[') { t->state=2;t->param_count=1;t->private_mode=t->param_overflow=0;memset(t->params,0,sizeof(t->params)); }
        else if(c==']' || c=='P' || c=='^' || c=='_' || c=='X') t->state=3;
        else if(c=='(' || c==')' || c=='%' || c=='#') { t->state=5;t->charset_target=c; }
        else if(c=='7') save_cursor(t);else if(c=='8') restore_cursor(t);
        else if(c=='D') newline(t);else if(c=='E') { t->x=0;newline(t); }
        else if(c=='M') { if(t->y==t->top) scroll_down(t,t->top,t->bottom,1);else if(t->y) t->y--;t->wrap_pending=0; }
        else if(c=='H') t->tabs[t->x]=1;
        else if(c=='c') desk_terminal_reset(t);
        return;
    }
    if(t->state==5) { if(t->charset_target=='(') t->charset=c=='0';t->state=0;return; }
    if(t->state==2) {
        if(c>='0' && c<='9') { unsigned *p=&t->params[t->param_count-1];*p=minimum(65535,*p*10+c-'0'); }
        else if(c==';') { if(t->param_count<16) t->param_count++;else t->param_overflow=1; }
        else if(c=='?' && t->param_count==1 && !t->params[0]) t->private_mode=1;
        else if(c>=0x40 && c<=0x7e) { sequence(t,c);t->state=0; }
        else if(c>=0x20) t->param_overflow=1;
        return;
    }
    if(t->utf_remaining) {
        if((c&0xc0)==0x80) {
            t->utf_value=(t->utf_value<<6)|(c&63);
            if(!--t->utf_remaining) {
                uint32_t cp=t->utf_value;if(cp<t->utf_minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) cp=0xfffd;
                character(t,cp);
            }return;
        }
        t->utf_remaining=0;character(t,0xfffd);goto again;
    }
    if(c<32 || c==127) {
        if(c==7) t->bells++;
        else if(c==8) { if(t->x) t->x--;t->wrap_pending=0; }
        else if(c==9) { unsigned x=t->x+1;while(x<t->columns-1 && !t->tabs[x]) x++;move(t,minimum(x,t->columns-1),t->y); }
        else if(c==10 || c==11 || c==12) newline(t);
        else if(c==13) { t->x=0;t->wrap_pending=0; }
        return;
    }
    if(c<128) character(t,c);
    else if(c>=0xc2 && c<=0xdf) { t->utf_remaining=1;t->utf_value=c&31;t->utf_minimum=0x80; }
    else if(c>=0xe0 && c<=0xef) { t->utf_remaining=2;t->utf_value=c&15;t->utf_minimum=0x800; }
    else if(c>=0xf0 && c<=0xf4) { t->utf_remaining=3;t->utf_value=c&7;t->utf_minimum=0x10000; }
    else character(t,0xfffd);
}
void desk_terminal_feed(struct desk_terminal *t,const void *data,size_t length) {
    if(!t || (!data && length)) return;
    const uint8_t *bytes=data;for(size_t i=0;i<length;i++) byte(t,bytes[i]);
    if(length) t->revision++;
}
size_t desk_terminal_reply(struct desk_terminal *t,void *data,size_t capacity) {
    size_t count=minimum(t->reply_size,capacity>DESK_TERM_REPLY?DESK_TERM_REPLY:(unsigned)capacity);
    if(!data) return 0;
    memcpy(data,t->reply,count);memmove(t->reply,t->reply+count,t->reply_size-count);t->reply_size-=count;return count;
}

size_t desk_terminal_paste_normalize(const struct desk_terminal *terminal,char *text,size_t size) {
    if(terminal->bracketed_paste) return size;
    size_t out=0;
    for(size_t i=0;i<size;i++) {
        char c=text[i];
        if(c=='\r' || c=='\n') {
            if(c=='\r' && i+1<size && text[i+1]=='\n') i++;
            text[out++]=' ';
        } else text[out++]=c;
    }
    return out;
}
