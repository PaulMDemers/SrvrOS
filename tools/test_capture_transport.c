#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#define main shotworker_main
#include "../userspace/shotworker/shotworker.c"
#undef main
static unsigned calls,polls,mode;static size_t offset;static int direction;
static unsigned char expected[12345];
static long io(void *data,size_t n,int writing) {
    assert(writing==direction && n && n<=SRV_PIPE_BUF);calls++;
    if(mode==1)return 0;
    if(mode==2)return SRV_ERR_PIPE;
    if(mode==3)return n+1;
    if(calls==1)return SRV_ERR_INTR;
    if(calls==2 || calls==3)return SRV_ERR_AGAIN;
    if(n>997)n=997;
    assert(offset+n<=sizeof(expected));
    if(writing)assert(!memcmp(data,expected+offset,n));else memcpy(data,expected+offset,n);
    offset+=n;return n;
}
long srv_read(int fd,void *data,size_t n) {assert(fd==0);return io(data,n,0);}
long srv_write(int fd,const void *data,size_t n) {assert(fd==1);return io((void *)data,n,1);}
long srv_poll(struct srv_pollfd *fds,size_t count,int64_t timeout) {
    assert(count==1 && timeout==-1 && fds->fd==direction);
    assert(fds->events==(direction?SRV_POLLOUT:SRV_POLLIN));polls++;
    if(polls==1)return SRV_ERR_INTR;
    fds->revents=SRV_POLLHUP|fds->events;return 1;
}
long srv_monotonic_us(void) {return 123;}
int main(void) {
    unsigned char actual[sizeof(expected)];for(size_t i=0;i<sizeof(expected);i++)expected[i]=(unsigned char)(i*37);
    for(direction=0;direction<=1;direction++) {
        mode=calls=polls=0;offset=0;memset(actual,0,sizeof(actual));
        assert(!transfer(direction,direction?expected:actual,sizeof(actual),direction));
        assert(offset==sizeof(actual) && polls==2 && (direction || !memcmp(actual,expected,sizeof(actual))));
        for(mode=1;mode<=3;mode++){calls=polls=0;assert(transfer(direction,actual,1,direction)==-1 && calls==1 && !polls);}
    }
    direction=1;mode=2;calls=0;struct progress_writer writer={.id=1};
    progress(&writer,CAPTURE_WRITE,0);assert(writer.broken && calls==1);
    progress(&writer,CAPTURE_FINAL_VERIFY,0);assert(calls==1);
    puts("capture-transport: PASS bounded chunks, exact partial transfer, EINTR/EAGAIN/poll retry, HUP drain, EOF/EPIPE/oversize and broken progress reader");
}
