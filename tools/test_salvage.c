#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static unsigned char source[2300],destination[2300];
static size_t position,total,source_length;static int failure,closed,opened,close_calls;static unsigned interrupted;
static int once(unsigned bit) {if(!(interrupted&bit)){interrupted|=bit;errno=EINTR;return 1;}return 0;}
static off_t mock_seek(int fd,off_t off,int whence) {
    assert(fd==7 && !closed);
    if(failure==10 && once(whence==SEEK_CUR?1:off==0?2:4))return -1;
    if(failure==1 && whence==SEEK_CUR){errno=ESPIPE;return -1;}
    if(failure==2 && whence==SEEK_SET && off==0){errno=EIO;return -1;}
    if((failure==8 || failure==9) && whence==SEEK_SET && off==37){errno=EIO;return -1;}
    if(whence==SEEK_SET)position=(size_t)off;
    return (off_t)position;
}
static int mock_open(const char *path,int flags,...) {
    assert(!strcmp(path,"/recovered") && (flags&O_EXCL) && (flags&O_CREAT));
    if(failure==10 && once(8))return -1;
    if(failure==3){errno=EEXIST;return -1;}opened=1;return 9;
}
static ssize_t mock_read(int fd,void *out,size_t capacity) {
    assert(fd==7 && !closed);
    if(failure==10 && once(16))return -1;
    if((failure==4 || failure==9) && position>=17){errno=EIO;return -1;}
    size_t n=source_length-position;if(n>capacity)n=capacity;if(n>17)n=17;
    memcpy(out,source+position,n);position+=n;return (ssize_t)n;
}
static ssize_t mock_write(int fd,const void *in,size_t length) {
    assert(fd==9 && opened);
    if(failure==10 && once(32))return -1;
    if(failure==5 && total>=7){errno=ENOSPC;return -1;}
    if(failure==6)return 0;
    if(length>7)length=7;memcpy(destination+total,in,length);total+=length;return (ssize_t)length;
}
static int mock_flush(int fd) {
    assert(fd==9 && opened);
    if(failure==10 && once(64))return -1;
    if(failure==7){errno=EIO;return -1;}return 0;
}
static int mock_close(int fd) {
    assert(fd==9 && opened);close_calls++;opened=0;
    if(failure==9 || failure==11){errno=EINTR;return -1;}return 0;
}
#define lseek mock_seek
#define open mock_open
#define read mock_read
#define write mock_write
#define fsync mock_flush
#define close mock_close
#include "../userspace/lib/src/salvage.c"
int main(void) {
    for(unsigned i=0;i<sizeof(source);i++)source[i]=(unsigned char)(i*13);
    for(failure=0;failure<=12;failure++) {
        source_length=failure==12?0:sizeof(source);position=37;total=closed=opened=close_calls=0;interrupted=0;memset(destination,0xcc,sizeof(destination));
        struct srv_salvage_result r;int status=srv_salvage_copy(7,"/recovered",&r);
        assert(!closed && !opened && close_calls<=1 && r.bytes_written==total);
        if(failure==0 || failure==10 || failure==12) {assert(!status && !r.error && total==source_length && !memcmp(source,destination,total));}
        else assert(status==-1 && r.error && errno==r.error);
        if(failure!=8 && failure!=9)assert(position==37);
        if(failure==1 || failure==2 || failure==3)assert(!r.destination_opened && !close_calls);
        if(failure==3)assert(destination[0]==0xcc && r.error==EEXIST);
        if(failure==6)assert(r.error==EIO && r.stage==SRV_SALVAGE_WRITE);
        if(failure==8)assert(r.restore_error==EIO && r.stage==SRV_SALVAGE_RESTORE);
        if(failure==9)assert(r.stage==SRV_SALVAGE_READ && r.close_error==EINTR && r.restore_error==EIO);
        if(failure==11)assert(r.stage==SRV_SALVAGE_CLOSE && r.close_error==EINTR && close_calls==1);
    }
    assert(srv_salvage_copy(7,NULL,NULL)==-1 && errno==EINVAL);
    puts("salvage: PASS short I/O, EINTR, exclusive open, all failure stages, source position/ownership and secondary errors");
}
