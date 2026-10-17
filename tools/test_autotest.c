#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/autotest.c"
#define arch_reboot unused_hardware_reboot
#include "../kernel/src/arch/x86_64/reboot.c"
#undef arch_reboot

static struct block_device disks[2];
static unsigned disk_count, write_count, flush_count, resets, tests, fail_write, fail_flush;
static bool writable;
static bool storage_ok, shell_ok, files_open;
static unsigned storage_runs;
static uint64_t now, paint;
static uint8_t pages[16][4096];
static unsigned allocated;
struct file { struct vfs_node node; char path[80]; uint8_t bytes[2048]; };
static struct file files[8];
static unsigned file_count;
static const char *run = "0123456789abcdef";
static const char *cmdline = "srvros.storage=ram srvros.autotest-volume=37d8c5d0 srvros.autotest-run=0123456789abcdef srvros.autotest-release=abcdef0123456789";
void console_write(const char *s) { paint += strlen(s); }
void console_write_n(const char *s,size_t n) { (void)s; paint+=n; }
void console_printf(const char *f,...) { (void)f; }
bool console_sync(void) { return true; }
bool console_framebuffer_info(uint64_t *w,uint64_t *h,uint64_t *p) { *w=1440;*h=900;*p=6144;return true; }
bool console_set_serial_output(bool b) { return !b; }
void console_get_stats(struct console_stats *s) { memset(s,0,sizeof(*s));s->framebuffer_bytes_written=paint; }
bool timer_reference_ready(void) { return true; }
bool timer_wait_ticks_bounded(uint64_t ticks,uint64_t spins) { (void)ticks;(void)spins;return true; }
uint64_t timer_monotonic_us(void) { return ++now; }
bool timer_delay_us(uint64_t n) { (void)n;return true; }
bool process_has_open_vfs_prefix(const char *p) { (void)p;return files_open; }
bool autotest_shell_run(struct block_device *d,const char *r,struct autotest_shell_stats *s) {
    assert(d==disks && !strcmp(r,run)); *s=(struct autotest_shell_stats){.elapsed_us=10,.input_poll_count=1};return shell_ok;
}
bool fpu_selftest(void) { tests++;return true; }
bool autotest_storage_run(struct block_device *d, const char *id) {
    assert(d==disks && !strcmp(id,run) && writable && flush_count>=2);
    storage_runs++; return storage_ok;
}
uint64_t pmm_alloc_frame(void) { assert(allocated<16);return (uint64_t)pages[allocated++]; }
void *pmm_phys_to_virt(uint64_t n) { return (void *)n; }
void pmm_free_frame(uint64_t n) { (void)n; }
uint64_t xhci_device_count(void) { return 2; }
uint64_t xhci_keyboard_count(void) { return 1; }
bool arch_reboot(void) { resets++;return false; }
struct block_device *block_device_at(uint64_t i) { return i<disk_count?&disks[i]:NULL; }
struct block_device *block_parent(struct block_device *d) { return d; }
bool usb_storage_is_device(struct block_device *d) { return d!=NULL; }
bool usb_storage_enable_volume_writes(struct block_device *d) { assert(d==disks);writable=true;return true; }
bool block_read(struct block_device *d,uint64_t off,void *out,uint64_t n) {
    (void)d;assert(off==0 && n==512);memset(out,0,n);
    uint8_t *b=out;memcpy(b+3,"EXFAT   ",8);b[510]=0x55;b[511]=0xaa;
    b[100]=0xd0;b[101]=0xc5;b[102]=0xd8;b[103]=0x37;return true;
}
static bool flush_stub(struct block_device *d) { assert(d==disks);return ++flush_count!=fail_flush; }
bool exfat_mount_block_device(const char *path,struct block_device *d) { assert(!strcmp(path,"/testlog") && d==disks);return true; }
const struct vfs_node *vfs_lookup(const char *path) {
    for(unsigned i=0;i<file_count;i++) if(!strcmp(path,files[i].path)) return &files[i].node;
    return NULL;
}
bool vfs_read_all(const struct vfs_node *n,const uint8_t **data,uint64_t *size) {
    *data=n->private_data;*size=n->size;return true;
}
void vfs_release_data(const struct vfs_node *n,const uint8_t *d) { (void)n;(void)d; }
static void put_file(const char *path,const char *data,unsigned size) {
    unsigned i;
    for(i=0;i<file_count;i++) if(!strcmp(path,files[i].path)) break;
    if(i==file_count) file_count++;
    assert(i<8 && size<2048);strcpy(files[i].path,path);memcpy(files[i].bytes,data,size);files[i].bytes[size]=0;
    files[i].node=(struct vfs_node){.used=true,.path=files[i].path,.size=size,.type=VFS_NODE_FILE,.private_data=files[i].bytes};
}
bool exfat_write_file(const char *path,const uint8_t *data,uint64_t size) {
    assert(writable);write_count++;
    if(tests) assert(flush_count>=1); /* No tests before request durability. */
    if(write_count==fail_write) return false;
    put_file(path,(const char *)data,size);return true;
}
static void reset(void) {
    memset(files,0,sizeof(files));memset(disks,0,sizeof(disks));
    disks[0].flush=flush_stub;disk_count=1;file_count=0;
    write_count=flush_count=resets=tests=fail_write=fail_flush=allocated=0;
    writable=false;now=paint=0;
    storage_ok=shell_ok=true;files_open=false;storage_runs=0;
    char path[80],text[64];make_path(path,run,".req");
    snprintf(text,sizeof(text),"SRVROS-AUTOTEST-V3 %s\n",run);put_file(path,text,strlen(text));
}
static void test_reset_gas(void) {
    uint8_t table[244]={0},value=0;
    table[113]=4;table[116]=1;table[117]=8;table[120]=0xf9;table[121]=0x0c;table[128]=6;
    assert(fadt_reset_port(table,244,&value)==0xcf9 && value==6);
    for(unsigned i=0;i<129;i++) assert(!fadt_reset_port(table,i,&value));
    table[116]=0;assert(!fadt_reset_port(table,244,&value));table[116]=1;
    table[117]=32;assert(!fadt_reset_port(table,244,&value));table[117]=8;
    table[118]=1;assert(!fadt_reset_port(table,244,&value));table[118]=0;
    table[119]=2;assert(!fadt_reset_port(table,244,&value));table[119]=0;
    table[122]=1;assert(!fadt_reset_port(table,244,&value));table[122]=0;
    table[113]=0;assert(!fadt_reset_port(table,244,&value));
}
int main(void) {
    char out[17];
    assert(hex_option("x=0123456789abcdef", "x=",out,16));
    assert(!hex_option("ax=0123456789abcdef", "x=",out,16));
    assert(!hex_option("x=0123456789abcdeg", "x=",out,16));
    assert(!hex_option("x=0123456789abcdef0", "x=",out,16));
    assert(!hex_option("x=0123456789abcdef x=0123456789abcdef", "x=",out,16));
    assert(crc32((const uint8_t *)"123456789",9)==0xcbf43926);
    test_reset_gas();
    reset();autotest_run("srvros.storage=ram",true);assert(!writable && !tests && !resets);
    reset();autotest_run(cmdline,true);assert(tests==1 && resets==1 && write_count==4 && flush_count==4);
    assert(strstr(report,"status=PASS\n") && strstr(report,"physical_keypress=NOT_TESTED\n"));
    assert(storage_runs==1 && strstr(report,"usb_file_operations=PASS\n"));
    unsigned before=write_count;autotest_run(cmdline,true);assert(write_count==before && resets==1 && tests==1);
    reset();autotest_run(cmdline,false);assert(strstr(report,"status=FAIL\n") && resets==1);
    reset();disk_count=2;autotest_run(cmdline,true);assert(!writable && !tests && resets==1);
    reset();file_count=0;autotest_run(cmdline,true);assert(!writable && !tests && !resets);
    reset();fail_flush=1;autotest_run(cmdline,true);assert(!tests && resets==1 && write_count==1);
    reset();fail_write=3;autotest_run(cmdline,true);assert(tests==1 && resets==1 && write_count==3);
    char path[80];make_path(path,run,".done");assert(!vfs_lookup(path));
    reset();fail_flush=3;autotest_run(cmdline,true);assert(tests==1 && resets==1 && write_count==3 && !vfs_lookup(path));
    reset();storage_ok=false;autotest_run(cmdline,true);
    assert(storage_runs==1 && strstr(report,"usb_file_operations=FAIL\n") && strstr(report,"status=FAIL\n"));
    reset();shell_ok=false;autotest_run(cmdline,true);
    assert(strstr(report,"usb_shell_workspace=FAIL\n") && strstr(report,"status=FAIL\n"));
    reset();shell_ok=false;files_open=true;autotest_run(cmdline,true);
    assert(resets==0 && strstr(report,"status=FAIL\n"));
    reset();fail_write=2;autotest_run(cmdline,true);assert(!storage_runs && strstr(report,"status=FAIL\n"));
    puts("autotest-host: PASS (identity, replay, duplicate UUID, consumption-before-test, save/flush failures, CRC, FADT bounds)");
}
