#define main image_worker_main
#include "../userspace/imageworker/imageworker.c"
#undef main
#include <assert.h>
static unsigned reads;
long srv_read(int fd,void *data,size_t size) {
    if(++reads%4==0) return SRV_ERR_INTR;
    if(size>71) size=71;
    return read(fd,data,size);
}
long srv_write(int fd,const void *data,size_t size) { assert(fd==2);(void)data;return size; }
int main(void) {
    const char *paths[]={"tools/fixtures/images/pattern.png","tools/fixtures/images/pattern.jpg","tools/fixtures/images/pattern.bmp","tools/fixtures/images/large.png"};
    for(unsigned i=0;i<4;i++) {
        uint32_t *p=NULL,w=0,h=0;assert(!load(paths[i],&p,&w,&h));assert(p && w==(i==3?1600u:19u) && h==(i==3?1000u:13u));image_decode_free(p);
    }
    uint32_t *p=NULL,w=0,h=0;
    assert(load("tools/fixtures/images/absent",&p,&w,&h)==DESK_IMAGE_NOT_FOUND);
    assert(load("tools/fixtures/images",&p,&w,&h)==DESK_IMAGE_FORMAT);
    char path[]="/tmp/srvros-image-worker-XXXXXX";int fd=mkstemp(path);assert(fd>=0);assert(!ftruncate(fd,SRV_IMAGE_MAX_FILE+1u));close(fd);
    assert(load(path,&p,&w,&h)==DESK_IMAGE_LIMIT);unlink(path);
    assert(!p && !w && !h && reads>100);
    puts("image-worker: PASS mapped source reads, short reads/EINTR, all formats, large image, absent/directory/oversized input and ownership cleanup");
}
