#include <srvros/salvage.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static off_t salvage_seek(int fd,off_t offset,int whence) {
    off_t value;
    do { value=lseek(fd,offset,whence); } while(value==(off_t)-1 && errno==EINTR);
    return value;
}
static void salvage_error(struct srv_salvage_result *r,enum srv_salvage_stage stage,int error) {
    if(!r->error) { r->stage=stage;r->error=error; }
}
int srv_salvage_copy(int source_fd,const char *destination,struct srv_salvage_result *result) {
    struct srv_salvage_result r={0};
    off_t original=(off_t)-1;int fd=-1;
    if(!destination || !*destination) {salvage_error(&r,SRV_SALVAGE_DESTINATION_OPEN,EINVAL);goto done;}
    original=salvage_seek(source_fd,0,SEEK_CUR);
    if(original==(off_t)-1) {salvage_error(&r,SRV_SALVAGE_SOURCE_POSITION,errno);goto done;}
    if(salvage_seek(source_fd,0,SEEK_SET)==(off_t)-1) {
        salvage_error(&r,SRV_SALVAGE_SOURCE_REWIND,errno);goto done;
    }
    do {fd=open(destination,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);} while(fd<0 && errno==EINTR);
    if(fd<0) {salvage_error(&r,SRV_SALVAGE_DESTINATION_OPEN,errno);goto done;}
    r.destination_opened=1;
    uint8_t buffer[1024];
    for(;;) {
        ssize_t n;
        do {n=read(source_fd,buffer,sizeof(buffer));} while(n<0 && errno==EINTR);
        if(n<0) {salvage_error(&r,SRV_SALVAGE_READ,errno);break;}
        if(!n) break;
        size_t offset=0;
        while(offset<(size_t)n) {
            ssize_t count;
            do {count=write(fd,buffer+offset,(size_t)n-offset);} while(count<0 && errno==EINTR);
            if(count<=0) {salvage_error(&r,SRV_SALVAGE_WRITE,count<0?errno:EIO);break;}
            offset+=(size_t)count;r.bytes_written+=(uint64_t)count;
        }
        if(r.error) break;
    }
    if(!r.error) {
        int status;do {status=fsync(fd);} while(status<0 && errno==EINTR);
        if(status<0) salvage_error(&r,SRV_SALVAGE_FLUSH,errno);
    }
done:
    if(fd>=0 && close(fd)<0) {r.close_error=errno;salvage_error(&r,SRV_SALVAGE_CLOSE,errno);}
    if(original!=(off_t)-1 && salvage_seek(source_fd,original,SEEK_SET)==(off_t)-1) {
        r.restore_error=errno;salvage_error(&r,SRV_SALVAGE_RESTORE,errno);
    }
    if(result) *result=r;
    if(r.error) {errno=r.error;return -1;}
    return 0;
}
