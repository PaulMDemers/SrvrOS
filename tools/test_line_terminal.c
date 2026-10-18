#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../ports/srvros/linenoise.c"
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
    puts("line-terminal: PASS escape continuation polls session stdin, standalone Escape, interruption, arrows/home/end/delete");
}
