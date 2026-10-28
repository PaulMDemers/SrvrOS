#include "save.h"
#include <srvros/bmp.h>
#include <srvros/sys.h>
#include <srvros/io_limits.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
static int transfer(int fd,void *data,size_t length,int writing) {
    unsigned char *p=data;
    while(length) {
        size_t count=length>SRV_PIPE_BUF?SRV_PIPE_BUF:length;
        long n=writing?srv_write(fd,p,count):srv_read(fd,p,count);
        if(n==SRV_ERR_INTR)continue;
        if(n==SRV_ERR_AGAIN) {
            struct srv_pollfd wait={.fd=fd,.events=writing?SRV_POLLOUT:SRV_POLLIN};
            long ready=srv_poll(&wait,1,-1);
            if(ready==SRV_ERR_INTR)continue;
            if(ready<=0 || (wait.revents&SRV_POLLNVAL))return -1;
            /* Retry even on HUP/ERR: drain remaining data or obtain EPIPE. */
            continue;
        }
        if(n<=0 || (size_t)n>count)return -1;
        p+=n;length-=n;
    }
    return 0;
}
struct progress_writer { uint32_t id;int broken; };
static void progress(void *context,unsigned phase,uint64_t bytes) {
    struct progress_writer *writer=context;if(writer->broken)return;
    long now=srv_monotonic_us();
    struct desk_capture_response frame={.magic=DESK_CAPTURE_PROGRESS_MAGIC,.id=writer->id,
        .status=phase,.at_us=now>0?(uint64_t)now:0,.bytes=bytes};
    /* A vanished reader cannot cancel an accepted file write or verification. */
    if(transfer(1,&frame,sizeof(frame),1))writer->broken=1;
}
int main(void) {
    if(srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE) ||
       srv_fcntl(0,SRV_F_SETFL,SRV_FD_NONBLOCK)<0 || srv_fcntl(1,SRV_F_SETFL,SRV_FD_NONBLOCK)<0)return 1;
    struct desk_capture_request q;
    if(transfer(0,&q,sizeof(q),0)) return 1;
    struct desk_capture_response r={.magic=DESK_CAPTURE_MAGIC,.id=q.id,.status=DESK_CAPTURE_INVALID};
    uint32_t bytes=desk_capture_bytes(q.width,q.height);
    if(q.magic!=DESK_CAPTURE_MAGIC || !q.id || !bytes || q.path[0]!='/' || !memchr(q.path,0,sizeof(q.path))) goto reply;
    struct progress_writer writer={.id=q.id};progress(&writer,CAPTURE_RECEIVE,0);
    uint32_t *pixels=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    unsigned char *bmp=mmap(NULL,bytes+54,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(pixels==MAP_FAILED || bmp==MAP_FAILED) r.status=DESK_CAPTURE_MEMORY;
    else if(transfer(0,pixels,bytes,0)) r.status=DESK_CAPTURE_IO;
    else {
        progress(&writer,CAPTURE_ENCODE,bytes);
        size_t length=0;
        if(!bmp_encode_rgba32(pixels,q.width,q.height,bmp,bytes+54,&length)) r.status=capture_save_progress(q.path,bmp,length,progress,&writer);
    }
    progress(&writer,CAPTURE_RELEASE,bytes);
    if(pixels!=MAP_FAILED) munmap(pixels,bytes);
    if(bmp!=MAP_FAILED) munmap(bmp,bytes+54);
reply:
    { long now=srv_monotonic_us();r.at_us=now>0?(uint64_t)now:0; }
    return transfer(1,&r,sizeof(r),1)?1:0;
}
