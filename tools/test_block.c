#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/block.c"
static uint8_t medium[4096];
static bool fail_write, fail_flush;
static unsigned reads, flushes;
void console_printf(const char *fmt, ...) { (void)fmt; }
static bool fake_read(struct block_device *d, uint64_t off, void *buf, uint64_t len) {
    (void)d; assert(off+len<=sizeof(medium)); reads++; memcpy(buf,medium+off,len); return true;
}
static bool fake_write(struct block_device *d, uint64_t off, const void *buf, uint64_t len) {
    (void)d; assert(off+len<=sizeof(medium)); if(fail_write) return false; memcpy(medium+off,buf,len); return true;
}
static bool fake_flush(struct block_device *d) { (void)d; flushes++; return !fail_flush; }
int main(void) {
    block_init(); memset(medium,'a',sizeof(medium));
    struct block_device *d=block_register("disk",512,8,fake_read,fake_write,NULL);
    assert(d); assert(!block_register("overflow",512,UINT64_MAX,fake_read,fake_write,NULL));
    uint8_t c;
    assert(block_read(d,10,&c,1) && c=='a');
    unsigned before=reads;
    fail_write=true; c='z'; assert(!block_write(d,10,&c,1));
    assert(block_read(d,10,&c,1) && c=='a' && reads>before);
    fail_write=false; c='b'; assert(block_write(d,10,&c,1));
    assert(block_read(d,10,&c,1) && c=='b' && medium[10]=='b');
    struct partition_block_device partition={.parent=d,.offset=512,.size=512};
    struct block_device *p=block_register("part",512,1,partition_read,partition_write,&partition);
    assert(block_read(p,0,&c,1) && c=='a'); c='c'; assert(block_write(d,512,&c,1));
    assert(block_read(p,0,&c,1) && c=='c'); c='d'; assert(block_write(p,0,&c,1));
    assert(block_read(d,512,&c,1) && c=='d');
    assert(!block_write(d,UINT64_MAX,&c,1)); assert(!block_flush_all());
    d->flush=fake_flush; assert(block_flush_all() && flushes==1);
    fail_flush=true; assert(!block_flush_all() && flushes==2);
    puts("block-host: PASS (failed-write invalidation, partition coherence, bounds, flush errors)");
}
