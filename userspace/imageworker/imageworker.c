#include "decode.h"
#include <srvros/sys.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>

static int transfer(int fd,void *data,size_t size,int writing) {
    unsigned char *p=data;
    while(size) {
        size_t chunk=size>16384?16384:size;
        long n=writing?srv_write(fd,p,chunk):srv_read(fd,p,chunk);
        if(n==SRV_ERR_INTR) continue;
        if(n<=0 || (size_t)n>chunk) return -1;
        p+=n;size-=n;
    }
    return 0;
}
static unsigned load(const char *path,uint32_t **pixels,uint32_t *width,uint32_t *height) {
    struct stat st;
    if(stat(path,&st)) return errno==ENOENT?DESK_IMAGE_NOT_FOUND:DESK_IMAGE_IO;
    if(!S_ISREG(st.st_mode) || st.st_size<=0) return DESK_IMAGE_FORMAT;
    if((uint64_t)st.st_size>SRV_IMAGE_MAX_FILE) return DESK_IMAGE_LIMIT;
    int fd=open(path,O_RDONLY);if(fd<0) return DESK_IMAGE_IO;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<=0 || (uint64_t)st.st_size>SRV_IMAGE_MAX_FILE) {
        close(fd);return DESK_IMAGE_LIMIT;
    }
    size_t size=(size_t)st.st_size;unsigned char *data=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(data==MAP_FAILED) { close(fd);return DESK_IMAGE_MEMORY; }
    int failed=transfer(fd,data,size,0);if(close(fd)) failed=1;
    unsigned result=failed?DESK_IMAGE_IO:image_decode(data,size,pixels,width,height);
    if(result) {
        char message[256];int n=snprintf(message,sizeof(message),"imageworker: status=%u bytes=%zu reason=%s\n",result,size,failed?"read":image_decode_reason());
        if(n>0) srv_write(2,message,(size_t)n<sizeof(message)?(size_t)n:sizeof(message)-1);
    }
    munmap(data,size);return result;
}
int main(void) {
    srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE);
    for(;;) {
        struct desk_image_request q;
        if(transfer(0,&q,sizeof(q),0)) break;
        struct desk_image_response r={.magic=DESK_IMAGE_MAGIC,.id=q.id,.checksum=DESK_IMAGE_HASH_INITIAL};
        uint32_t *pixels=NULL;
        if(q.magic!=DESK_IMAGE_MAGIC || !q.id || q.path[0]!='/' || !memchr(q.path,0,sizeof(q.path))) r.status=DESK_IMAGE_INVALID;
        else r.status=load(q.path,&pixels,&r.width,&r.height);
        if(!r.status) { r.length=r.width*r.height*4;r.checksum=desk_image_hash(r.checksum,pixels,r.length); }
        int failed=transfer(1,&r,sizeof(r),1) || (r.length && transfer(1,pixels,r.length,1));
        image_decode_free(pixels);if(failed) break;
    }
    return 0;
}
