#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
static uint64_t block_test_irq_save(void) { return 0; }
static void block_test_irq_restore(uint64_t x) { (void)x; }
#define SRVROS_BLOCK_HOST_TEST
#include "../kernel/src/block.c"
#include "../kernel/src/installed_root.c"
void console_printf(const char *fmt, ...) { (void)fmt; }
bool scheduler_io_begin(void) { return true; }
bool scheduler_io_active(void) { return false; }
void scheduler_io_end(void) {}
bool scheduler_io_yield(void) { return false; }
uint64_t timer_monotonic_us(void) { return 0; }
bool timer_reference_ready(void) { return false; }
static uint8_t media[2][512*256];
static unsigned writes, flushes;
static bool io_fail;
static bool rd(struct block_device *d,uint64_t off,void *buf,uint64_t n) {
    if (io_fail) return false;
    memcpy(buf,(uint8_t *)d->private_data+off,n); return true;
}
static bool wr(struct block_device *d,uint64_t off,const void *buf,uint64_t n) {
    (void)d;(void)off;(void)buf;(void)n; writes++;return false;
}
static bool fl(struct block_device *d) { (void)d;flushes++;return false; }
static void p32(uint8_t *p,uint32_t x) { for(unsigned i=0;i<4;i++)p[i]=x>>(8*i); }
static void p64(uint8_t *p,uint64_t x) { for(unsigned i=0;i<8;i++)p[i]=x>>(8*i); }
static void checksum(uint8_t *m) {
    uint8_t *h=m+512;p32(h+88,~gpt_crc32(UINT32_MAX,m+1024,128*128));
    p32(h+16,0);p32(h+16,~gpt_crc32(UINT32_MAX,h,92));
}
static void fixture(unsigned disk,unsigned id) {
    uint8_t *m=media[disk];memset(m,0,sizeof(media[disk]));
    m[510]=0x55;m[511]=0xaa;m[450]=0xee;p32(m+454,1);p32(m+458,255);
    uint8_t *h=m+512;memcpy(h,"EFI PART",8);p32(h+8,0x10000);p32(h+12,92);
    p64(h+24,1);p64(h+32,255);p64(h+40,34);p64(h+48,222);h[56]=disk+1;
    p64(h+72,2);p32(h+80,128);p32(h+84,128);
    uint8_t *e=m+1024;const uint8_t type[16]={0xa2,0xa0,0xd0,0xeb,0xe5,0xb9,0x33,0x44,0x87,0xc0,0x68,0xb6,0xb7,0x26,0x99,0xc7};
    memcpy(e,type,16);e[16]=id;p64(e+32,40);p64(e+40,190);
    const char *name="srvros-system-v1";for(unsigned i=0;name[i];i++)e[56+2*i]=name[i];
    uint8_t *b=m+40*512;memcpy(b+3,"EXFAT   ",8);p64(b+72,151);p32(b+80,24);p32(b+84,2);
    p32(b+88,26);p32(b+92,100);p32(b+96,2);b[105]=1;b[108]=9;b[110]=1;b[510]=0x55;b[511]=0xaa;
    checksum(m);
}
static void setup(bool reverse) {
    block_init();writes=flushes=0;io_fail=false;
    struct block_device *a=block_register(reverse?"disk1":"disk0",512,256,rd,wr,media[reverse?1:0]);a->flush=fl;
    struct block_device *b=block_register(reverse?"disk0":"disk1",512,256,rd,wr,media[reverse?0:1]);b->flush=fl;
}
static const char *options="srvros.root=00000001-0000-0000-0000-000000000000 srvros.root-format=exfat-v1 srvros.root-role=system";
static void expect(const char *opts,enum installed_root_result result,const char *why) {
    struct block_device *out=(void *)1;const char *reason=(void *)1;
    assert(installed_root_select(opts,&out,&reason)==result);
    if (result==INSTALLED_ROOT_SELECTED) assert(out && out->backing->private_data==media[0]);
    else assert(!out);
    if (why) assert(reason && !strcmp(reason,why)); else assert(!reason);
    assert(!writes && !flushes);
}
int main(void) {
    fixture(0,1);fixture(1,2);setup(false);
    expect("quiet",INSTALLED_ROOT_LEGACY,NULL);expect(options,INSTALLED_ROOT_SELECTED,NULL);
    expect(options,INSTALLED_ROOT_SELECTED,NULL);setup(true);expect(options,INSTALLED_ROOT_SELECTED,NULL);
    setup(false);expect("srvros.root=00000001-0000-0000-0000-000000000000",INSTALLED_ROOT_INVALID,"bad-options");
    expect("srvros.root-format=exfat-v1 srvros.root-format=exfat-v1",INSTALLED_ROOT_INVALID,"bad-options");
    expect("srvros.root=0000000X-0000-0000-0000-000000000000 srvros.root-format=exfat-v1 srvros.root-role=system",INSTALLED_ROOT_INVALID,"bad-options");
    expect("srvros.root=00000003-0000-0000-0000-000000000000 srvros.root-format=exfat-v1 srvros.root-role=system",INSTALLED_ROOT_INVALID,"missing");
    fixture(1,1);setup(false);expect(options,INSTALLED_ROOT_INVALID,"ambiguous");
    fixture(1,2);media[1][512+56]=1;checksum(media[1]);setup(false);expect(options,INSTALLED_ROOT_INVALID,"ambiguous");
    fixture(1,2);media[1][512+16]^=1;setup(false);expect(options,INSTALLED_ROOT_INVALID,"invalid-map");
    fixture(1,2);setup(false);io_fail=true;expect(options,INSTALLED_ROOT_INVALID,"invalid-map");
    fixture(0,1);fixture(1,2);media[0][1024]=0;checksum(media[0]);setup(false);expect(options,INSTALLED_ROOT_INVALID,"role");
    fixture(0,1);media[0][1024+56]='X';checksum(media[0]);setup(false);expect(options,INSTALLED_ROOT_INVALID,"role");
    fixture(0,1);media[0][40*512+3]='X';setup(false);expect(options,INSTALLED_ROOT_INVALID,"format");
    puts("installed-root: PASS");return 0;
}
