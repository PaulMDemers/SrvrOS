#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
static bool irq_enabled = true, foreign_service, simulate_preemption;
static unsigned io_depth, preemptions;
static uint64_t block_test_irq_save(void) { bool old=irq_enabled; irq_enabled=false; return old; }
static void block_test_irq_restore(uint64_t flags) { irq_enabled=flags != 0; }
#define SRVROS_BLOCK_HOST_TEST
#include "../kernel/src/block.c"
static uint8_t medium[4096];
static bool fail_write, fail_flush;
static unsigned reads, flushes;
void console_printf(const char *fmt, ...) { (void)fmt; }
bool scheduler_io_begin(void) { if(foreign_service) return false; io_depth++; return true; }
bool scheduler_io_active(void) { return io_depth != 0 || foreign_service; }
void scheduler_io_end(void) { assert(io_depth); io_depth--; }
static bool fake_read(struct block_device *d, uint64_t off, void *buf, uint64_t len) {
    assert(off+len<=sizeof(medium)); reads++; memcpy(buf,medium+off,len);
    /* Reproduce a supervisor read at the driver's IRQ-restore boundary. With
     * only driver serialization it reuses the unpublished victim and replaces
     * this read's data. Block-level ownership must close that gap. */
    if (simulate_preemption && irq_enabled) {
        simulate_preemption=false; preemptions++;
        uint8_t other; assert(block_read(d,512,&other,1));
    }
    return true;
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
    block_init(); memset(medium,'a',512); memset(medium+512,0xff,512);
    d=block_register("race",512,8,fake_read,fake_write,NULL);
    simulate_preemption=true;
    assert(read_locked(d,0,&c,1) && c==0xff && preemptions==1);
    /* Reset the intentionally corrupted cache, then exercise the public API. */
    block_init(); d=block_register("race",512,8,fake_read,fake_write,NULL);
    preemptions=0; simulate_preemption=true;
    assert(block_read(d,0,&c,1) && c=='a');
    assert(preemptions==0 && irq_enabled && io_depth==0);
    /* A service must also be rejected on cache hits, before touching data. */
    foreign_service=true; c='q';
    assert(!block_read(d,0,&c,1) && c=='q');
    assert(!block_write(d,0,&c,1) && medium[0]=='a');
    foreign_service=false;
    irq_enabled=false; assert(block_read(d,0,&c,1) && !irq_enabled);
    assert(io_depth==0);
    puts("block-host: PASS (cache publication isolation, service rejection, failed writes, partition coherence, bounds, flush errors)");
}
