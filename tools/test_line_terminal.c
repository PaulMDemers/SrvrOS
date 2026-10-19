#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_terminal.h>
#define tcgetattr test_tcgetattr
#define tcsetattr test_tcsetattr
#define clrscr test_clrscr
#include "../ports/srvros/linenoise.c"
static struct desk_terminal terminal;
static struct desk_term_line lines[80],alternate[DESK_TERM_ROWS];
int test_tcgetattr(int fd,struct termios *t) { (void)fd;(void)t;return -1; }
int test_tcsetattr(int fd,int action,const struct termios *t) { (void)fd;(void)action;(void)t;return 0; }
int test_clrscr(void) { desk_terminal_feed(&terminal,"\033[2J\033[H",7);return 0; }
long srv_write(int fd,const void *data,size_t size) {
    assert(fd==SRV_STDOUT);
    /* PTY output normally translates LF to CRLF. Feed the real terminal
     * model to check displayed cells as well as the returned command. */
    const char *p=data;
    for(size_t i=0;i<size;i++) {
        if(p[i]=='\n') desk_terminal_feed(&terminal,"\r",1);
        desk_terminal_feed(&terminal,p+i,1);
    }
    return size;
}
static const char *input;static size_t cursor;static int poll_error;static unsigned polls;
long srv_poll(struct srv_pollfd *fds,size_t count,int timeout) {
    assert(count==1 && fds[0].fd==SRV_STDIN && fds[0].events==SRV_POLLIN && timeout==120);polls++;
    if(poll_error) return SRV_ERR_INTR;
    fds[0].revents=input[cursor]?SRV_POLLIN:0;return input[cursor]?1:0;
}
long srv_read(int fd,void *data,size_t size) {
    assert(fd==SRV_STDIN && size==1);if(!input[cursor]) return 0;
    *(char *)data=input[cursor++];return 1;
}
static void keys(const char *sequence,char *buffer,size_t *length,size_t *position) {
    input=sequence;cursor=0;size_t index=0;char draft[40]={0},kill[40]={0};int saved=0,pending=-1;
    consume_escape(buffer,40,length,position,&index,draft,&saved,kill,sizeof(kill),&pending);
}
static void edit(const char *sequence,const char *expected) {
    assert(!desk_terminal_init(&terminal,lines,80,alternate,80,4));
    input=sequence;cursor=0;
    char *result=linenoise("/work/srvros-work $ ");
    assert(result && !strcmp(result,expected));free(result);
    char displayed[80];snprintf(displayed,sizeof(displayed),"/work/srvros-work $ %s",expected);
    const struct desk_term_line *line=desk_terminal_line(&terminal,0);
    for(unsigned i=0;i<80;i++) assert(line->cells[i].codepoint==(i<strlen(displayed)?(unsigned char)displayed[i]:' '));
}
int main(void) {
    input="";assert(read_byte_scan()==-1);input="x";assert(read_byte_scan()=='x');
    poll_error=1;input="y";cursor=0;assert(read_byte_scan()==-1 && !cursor);poll_error=0;
    char buffer[40]="ac";size_t length=2,position=2;
    keys("[D",buffer,&length,&position);assert(position==1);
    insert_at(buffer,sizeof(buffer),&length,&position,'b');assert(!strcmp(buffer,"abc"));
    keys("[F",buffer,&length,&position);assert(position==3);
    keys("[H",buffer,&length,&position);assert(!position);
    keys("[3~",buffer,&length,&position);assert(!strcmp(buffer,"bc"));
    assert(polls>=7);
    /* Erase padding must not move the next appended byte beyond the editor's
     * remembered extent, or another backspace leaves it behind permanently. */
    edit("abc\177/\177\n","ab");
    edit("abc\177\177/\177\177\n","");
    edit("abc\177/\n","ab/");
    edit("abcdef\001\006\006\013/\177\n","ab");
    edit("abcdef\025/\177\n","");
    edit("alpha beta\027/\177\n","alpha ");
    linenoiseHistoryAdd("a much longer previous command");
    edit("\020\016/\177\n","");
    edit("abc\002\004/\177\n","ab");
    puts("line-terminal: PASS escape continuation polls session stdin, standalone Escape, interruption, arrows/home/end/delete; real terminal cells after backspace/kill/history shortening and appended slash");
}
