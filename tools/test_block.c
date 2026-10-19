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
static uint8_t medium[65536];
static bool fail_read, fail_write, fail_flush;
static unsigned reads, writes, flushes;
static bool partial_failure;
void console_printf(const char *fmt, ...) { (void)fmt; }
bool scheduler_io_begin(void) { if(foreign_service) return false; io_depth++; return true; }
bool scheduler_io_active(void) { return io_depth != 0 || foreign_service; }
void scheduler_io_end(void) { assert(io_depth); io_depth--; }
static uint64_t clock_us;
static bool clock_ready = true;
static unsigned service_yields;
uint64_t timer_monotonic_us(void) { return clock_us; }
bool timer_reference_ready(void) { return clock_ready; }
bool scheduler_io_yield(void) {
    assert(io_depth && !irq_enabled && !foreign_service);
    service_yields++;
    foreign_service = true;
    uint8_t byte = 0x25;
    assert(!block_read(&devices[0], 0, &byte, 1) && byte == 0x25);
    assert(!block_write(&devices[0], 0, &byte, 1));
    foreign_service = false;
    return true;
}
static bool fake_read(struct block_device *d, uint64_t off, void *buf, uint64_t len) {
    assert(off+len<=sizeof(medium)); reads++; memcpy(buf,medium+off,len);
    if (fail_read) return false;
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
    (void)d; assert(off+len<=sizeof(medium)); writes++;
    if(fail_write) {
        if(partial_failure) memcpy(medium+off,buf,len/2);
        return false;
    }
    memcpy(medium+off,buf,len); return true;
}
static bool fake_flush(struct block_device *d) { (void)d; flushes++; return !fail_flush; }
static void mbr_entry(unsigned slot, uint8_t type, uint32_t start, uint32_t count) {
    uint8_t *p=medium+446+slot*16; p[4]=type;
    for(unsigned i=0;i<4;i++) { p[8+i]=start>>(8*i); p[12+i]=count>>(8*i); }
}
static struct block_device *mbr_disk(void) {
    block_init(); return block_register("usb0",512,8,fake_read,NULL,NULL);
}
static void test_mbr(void) {
    memset(medium,0,sizeof(medium)); medium[510]=0x55; medium[511]=0xaa;
    mbr_entry(0,7,1,4);
    struct block_device *d=mbr_disk();
    assert(block_register_mbr_partitions(d)==1);
    struct block_device *p=block_find("usb0p1"); assert(p && !p->write && p->block_count==4);
    uint8_t byte=0; medium[512]=0x91;
    assert(block_read(p,0,&byte,1) && byte==0x91);
    assert(block_register_mbr_partitions(d)==0 && partition_count==1);
    assert(block_register_mbr_partitions(p)==0);
    assert(block_device_at(0)==d && block_parent(p)==d && !block_device_at(BLOCK_MAX_DEVICES));
    struct block_device *raw=NULL;uint64_t offset=UINT64_MAX;
    assert(block_raw_range(p,512,512,&raw,&offset)&&raw==d&&offset==1024);
    assert(!block_raw_range(p,2048,1,&raw,&offset));
    assert(!block_raw_range(p,UINT64_MAX,512,&raw,&offset));
    assert(!block_raw_range(p,0,0,&raw,&offset));
    assert(block_raw_range(d,0,4096,&raw,&offset)&&raw==d&&offset==0);
    medium[512]=0x93;block_forget(p);
    assert(block_read(p,0,&byte,1)&&byte==0x93);
    assert(!block_enable_partition_writes(p));
    d->write=fake_write; assert(block_enable_partition_writes(p) && p->write==partition_write);
    byte=0x72; assert(block_write(p,0,&byte,1) && medium[512]==0x72);
    mbr_entry(1,7,4,2); assert(block_register_mbr_partitions(mbr_disk())==0); /* overlap */
    mbr_entry(1,0,0,0); mbr_entry(0,7,7,2); assert(block_register_mbr_partitions(mbr_disk())==0);
    mbr_entry(0,7,1,UINT32_MAX); assert(block_register_mbr_partitions(mbr_disk())==0);
    mbr_entry(0,7,0,4); assert(block_register_mbr_partitions(mbr_disk())==0);
    mbr_entry(0,0xee,1,4); assert(block_register_mbr_partitions(mbr_disk())==0);
    mbr_entry(0,5,1,4); assert(block_register_mbr_partitions(mbr_disk())==0);
    mbr_entry(0,7,1,4); medium[510]=0; assert(block_register_mbr_partitions(mbr_disk())==0);
}
static void put32(uint8_t *p, uint32_t n) { for (unsigned i=0;i<4;i++) p[i]=n>>(8*i); }
static void put64(uint8_t *p, uint64_t n) { for (unsigned i=0;i<8;i++) p[i]=n>>(8*i); }
static void gpt_checksums(void) {
    uint8_t *h=medium+512;
    put32(h+88,~gpt_crc32(UINT32_MAX,medium+1024,128*128));
    put32(h+16,0);put32(h+16,~gpt_crc32(UINT32_MAX,h,92));
}
static void gpt_fixture(void) {
    memset(medium,0,sizeof(medium));medium[510]=0x55;medium[511]=0xaa;
    mbr_entry(0,0xee,1,127);
    uint8_t *h=medium+512;memcpy(h,"EFI PART",8);put32(h+8,0x10000);put32(h+12,92);
    put64(h+24,1);put64(h+32,127);put64(h+40,34);put64(h+48,94);
    put64(h+72,2);put32(h+80,128);put32(h+84,128);
    uint8_t *e=medium+1024;e[0]=1;e[16]=2;put64(e+32,40);put64(e+40,47);
    gpt_checksums();
}
static struct block_device *gpt_disk(void) {
    block_init();return block_register("usb0",512,128,fake_read,NULL,NULL);
}
static void reject_gpt(void) {
    assert(block_register_gpt_partitions(gpt_disk())==0);
    assert(partition_count==0 && device_count==1);
}
static void test_gpt(void) {
    assert(~gpt_crc32(UINT32_MAX,(const uint8_t *)"123456789",9)==0xcbf43926u);
    gpt_fixture(); struct block_device *d=gpt_disk();
    assert(block_register_gpt_partitions(d)==1);
    struct block_device *p=block_find("usb0p1");assert(p && p->block_count==8 && !p->write);
    uint8_t byte;medium[40*512]=0x5a;assert(block_read(p,0,&byte,1) && byte==0x5a);
    assert(block_register_gpt_partitions(d)==0 && partition_count==1);
    assert(block_register_gpt_partitions(p)==0);
    d->write=fake_write;assert(block_enable_partition_writes(p));byte=0x71;
    assert(block_write(p,0,&byte,1) && medium[40*512]==0x71);
    assert(!block_write(p,8*512,&byte,1));
    gpt_fixture();medium[512+16]^=1;reject_gpt(); /* Header CRC. */
    gpt_fixture();medium[1024+100]^=1;reject_gpt(); /* Entry table CRC. */
    gpt_fixture();mbr_entry(1,7,40,8);reject_gpt(); /* Hybrid protective MBR. */
    gpt_fixture();put64(medium+1024+32,1);gpt_checksums();reject_gpt();
    gpt_fixture();put64(medium+1024+40,UINT64_MAX);gpt_checksums();reject_gpt();
    gpt_fixture();put64(medium+512+72,UINT64_MAX);gpt_checksums();reject_gpt();
    gpt_fixture();put32(medium+512+80,UINT32_MAX);gpt_checksums();reject_gpt();
    gpt_fixture();put32(medium+512+84,129);gpt_checksums();reject_gpt();
    gpt_fixture();put64(medium+512+40,2);gpt_checksums();reject_gpt();
    gpt_fixture();put64(medium+512+48,126);gpt_checksums();reject_gpt();
    gpt_fixture();put64(medium+1024+48,1ull<<60);gpt_checksums();reject_gpt();
    gpt_fixture();memcpy(medium+1152,medium+1024,128);medium[1152+16]=3;
    put64(medium+1152+32,47);put64(medium+1152+40,50);gpt_checksums();reject_gpt();
    /* Late invalid entry must not publish the earlier valid partition. */
    gpt_fixture();memcpy(medium+1024+127*128,medium+1024,128);gpt_checksums();reject_gpt();
    gpt_fixture();memcpy(medium+1152,medium+1024,128);medium[1152+16]=3;
    put64(medium+1152+32,64);put64(medium+1152+40,71);gpt_checksums();
    assert(block_register_gpt_partitions(gpt_disk())==2);
    gpt_fixture();fail_read=true;reject_gpt();fail_read=false;
    puts("gpt-host: PASS CRCs, bounded map, metadata exclusion, overlap/duplicate rejection, no partial publication and partition I/O");
}

static void test_batches(void) {
    static uint8_t input[sizeof(medium)], output[sizeof(medium)], expected[sizeof(medium)];
    const unsigned offsets[]={0,1,510,512,4095,4096};
    const unsigned lengths[]={0,1,511,512,513,4095,4096,4097,32768};
    for (unsigned a=0;a<sizeof(offsets)/sizeof(*offsets);a++) {
        for (unsigned b=0;b<sizeof(lengths)/sizeof(*lengths);b++) {
            block_init(); memset(medium,0x69,sizeof(medium));
            struct block_device *d=block_register("batch",512,sizeof(medium)/512,fake_read,fake_write,NULL);
            d->max_transfer_bytes=4096;
            unsigned off=offsets[a], n=lengths[b];
            memcpy(expected,medium,sizeof(medium));
            for(unsigned i=0;i<n;i++) input[i]=(uint8_t)(i*37+11);
            memcpy(expected+off,input,n);
            assert(block_write(d,off,input,n) && !memcmp(medium,expected,sizeof(medium)));
            assert(block_read(d,off,output,n) && !memcmp(output,input,n));
        }
    }
    block_init(); reads=writes=0; memset(medium,0x69,sizeof(medium));
    struct block_device *d=block_register("batch",512,sizeof(medium)/512,fake_read,fake_write,NULL);
    d->max_transfer_bytes=4096;
    assert(block_write(d,0,input,8192) && reads==0 && writes==2);
    assert(block_read(d,0,output,8192) && reads==0 && !memcmp(input,output,8192));
    cache_invalidate_range(d,0,16);
    assert(block_read(d,0,output,8192) && reads==2 && !memcmp(input,output,8192));
    cache_invalidate_range(d,0,16); fail_read=true; reads=0;
    assert(!block_read(d,0,output,8192) && reads==1);
    fail_read=false;
    assert(block_read(d,0,output,4096) && reads==2); /* Failure published no cache entries. */
    /* A cached middle sector splits a miss run without rereading that sector. */
    cache_invalidate_range(d,0,16); reads=0;
    assert(block_read(d,1024,output,512) && reads==1);
    assert(block_read(d,0,output,4096) && reads==3 && !memcmp(input,output,4096));
    /* All sectors potentially touched by a failed write must be reloaded. */
    memset(input,0xab,4096); fail_write=partial_failure=true;
    unsigned before=writes;
    assert(!block_write(d,0,input,8192) && writes==before+1);
    fail_write=partial_failure=false; reads=0;
    assert(block_read(d,0,output,4096) && reads==1 && !memcmp(output,medium,4096));
    for(unsigned i=0;i<2048;i++) assert(output[i]==0xab);
    /* Partition aliases share the parent's batching and invalidation. */
    struct partition_block_device part={.parent=d,.offset=512,.size=8192};
    struct block_device *p=block_register("part",512,16,partition_read,partition_write,&part);
    reads=writes=0;
    assert(block_write(p,0,input,4096) && reads==0 && writes==1);
    assert(block_read(d,512,output,4096) && reads==0 && !memcmp(input,output,4096));
    assert(block_read(p,0,output,4096) && reads==0 && !memcmp(input,output,4096));
    assert(!block_write(p,8191,input,2) && writes==1);
    assert(!block_read(p,8191,output,2) && reads==0);
    /* Batching must retain service opportunities through long cache operations. */
    service_blocks=0; last_service_us=0; clock_us=1000;
    unsigned yields_before=service_yields;
    assert(block_read(d,0,output,32768));
    assert(service_yields>yields_before && io_depth==0);
    clock_us=0;
}
int main(void) {
    test_batches(); service_yields=0;
    test_gpt();
    test_mbr();
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
    /* Cache hits must offer services without yielding on every tiny read. */
    service_blocks=0; last_service_us=0; clock_us=999;
    for (unsigned i=0; i<64; i++) assert(block_read(d,0,&c,1) && c=='a');
    assert(service_yields==0);
    clock_us=1000;
    before=reads;
    for (unsigned i=0; i<32; i++) assert(block_read(d,0,&c,1) && c=='a');
    assert(service_yields==1 && reads==before && !irq_enabled && io_depth==0);
    clock_ready=false; service_blocks=0;
    for (unsigned i=0; i<256; i++) assert(block_write(d,0,&c,1));
    assert(service_yields==2 && !irq_enabled && io_depth==0);
    puts("block-host: PASS (bounded batching, failed batch invalidation/publication, unaligned I/O, cache publication isolation, cached I/O service windows, service rejection, failed writes, partition coherence, MBR bounds/overlap/protection, flush errors)");
}
