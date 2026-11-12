/* Actual workspace/exFAT/VFS/BOT/block-claim path on disposable regular media. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <srvros/workspace.h>
#include <srvros/block.h>
#include <srvros/usb_storage.h>
#include <srvros/vfs.h>
#include <srvros/exfat.h>
static int fd;static uint64_t image_size,clock_us=100,irq=0x200,raw_offset;
static unsigned depth,phase,done,command_length,command_tag,raw_leases,raw_bytes;
static bool busy,unavailable,clock_jump;static const char *mode;
static struct usb_storage storage;static uint8_t cdb[16];
static struct usb_bulk_timing sample;
static uint8_t *raw_before;static unsigned sectorsize=512;
uint64_t block_test_irq_save(void){uint64_t f=irq;irq=0;return f;}
void block_test_irq_restore(uint64_t f){irq=f;}
uint64_t usb_test_irq_save(void){return block_test_irq_save();}
void usb_test_irq_restore(uint64_t f){block_test_irq_restore(f);}
void *kmalloc(uint64_t n){return malloc(n);}
void *kcalloc(uint64_t n,uint64_t size){return calloc(n,size);}
void kfree(void *p){free(p);}
uint64_t timer_monotonic_us(void){return clock_us;}
uint64_t timer_ticks(void){return clock_us/10000+1;}
int64_t timer_realtime_us(void){return 1794438000000000LL+(int64_t)clock_us;}
bool timer_reference_ready(void){return !unavailable;}
bool scheduler_io_begin(void){depth++;return true;}
void scheduler_io_end(void){assert(depth);depth--;}
bool scheduler_io_active(void){return depth!=0;}
bool scheduler_io_yield(void){return true;}
void scheduler_io_checkpoint(void){}
bool process_has_open_vfs_prefix(const char *p){(void)p;return busy;}
int64_t process_file_sync_all(void){return 0;}
void process_wait_notify(void){}
uint64_t initramfs_file_count(void){return 0;}
const struct initramfs_file *initramfs_file_at(uint64_t i){(void)i;return NULL;}
void latency_usb(uint64_t bytes,uint8_t opcode){(void)bytes;(void)opcode;}
bool workspace_image_busy(void){return false;}
bool workspace_log_pause(void){assert(workspace_active());return true;}
/* Optional content attribution is inactive in this dedicated USB observer. */
bool sync_timing_content_io_active(void){return false;}
void sync_timing_content_io(unsigned r,unsigned op,uint64_t n,uint64_t us,bool ok){(void)r;(void)op;(void)n;(void)us;(void)ok;}
unsigned sync_timing_content_role_begin(unsigned r){(void)r;return USB_STORAGE_CONTENT_NO_ROLE;}
void sync_timing_content_role_end(unsigned r){(void)r;}
static uint32_t le32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t be32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void putle(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void putbe(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>((3-i)*8));}
static void raw_contained(void) {
    if(!raw_before)return;
    uint8_t *after=malloc(image_size);assert(after&&pread(fd,after,image_size,0)==(ssize_t)image_size);
    for(uint64_t i=0;i<image_size;i++)if(i<raw_offset||i>=raw_offset+32768)assert(after[i]==raw_before[i]);
    free(after);free(raw_before);raw_before=NULL;
}
void console_write(const char *s){if(!strcmp(s,"workspace: ready at /work/srvros-work\n"))raw_contained();}
void console_printf(const char *fmt,...) {
    if(strncmp(fmt,"storage-only",12)&&strncmp(fmt,"Storage-only",12))return;
    va_list args;va_start(args,fmt);
    if(!strncmp(fmt,"storage-only-lease:",19)) {
        va_list copied;va_copy(copied,args);(void)va_arg(copied,uint64_t);raw_offset=va_arg(copied,uint64_t);va_end(copied);
        assert(!raw_before && storage.block && !storage.block->write && !storage.block->flush);
        assert(!block_claim_mount(storage.block)&&!block_claim_raw(storage.block,NULL));
        assert(raw_offset<=image_size&&32768<=image_size-raw_offset);
        raw_before=malloc(image_size);assert(raw_before&&pread(fd,raw_before,image_size,0)==(ssize_t)image_size);raw_leases++;
    }
    for(const char *p=fmt;*p;p++) {
        if(*p!='%'){putchar(*p);continue;}p++;
        if(*p=='u')printf("%"PRIu64,va_arg(args,uint64_t));
        else if(*p=='x')printf("%"PRIx64,va_arg(args,uint64_t));
        else if(*p=='s')fputs(va_arg(args,const char *),stdout);
        else {assert(*p=='%');putchar('%');}
    }
    va_end(args);
}
static bool bulk(void *ctx,bool in,void *buffer,uint32_t n,uint32_t *actual) {
    assert(ctx==&storage&&n<=4096&&n);uint64_t start=clock_us;unsigned prior=phase;
    bool direct=raw_before!=NULL;*actual=n;clock_us+=10;
    if(phase==0) {
        uint8_t *p=buffer;assert(!in&&n==31&&le32(p)==0x43425355);
        command_tag=le32(p+4);command_length=le32(p+8);memcpy(cdb,p+15,16);done=0;
        assert(command_length<=16384);phase=command_length?1:2;
    } else if(phase==1) {
        assert(n==(command_length-done>4096?4096:command_length-done));
        if(cdb[0]==0x12)memset(buffer,0,n);
        else if(cdb[0]==0x25){putbe(buffer,(uint32_t)(image_size/sectorsize-1));putbe((uint8_t *)buffer+4,sectorsize);}
        else {
            assert((cdb[0]==0x28&&in)||(cdb[0]==0x2a&&!in));
            uint64_t offset=(uint64_t)be32(cdb+2)*sectorsize+done;assert(offset<=image_size&&n<=image_size-offset);
            if(direct&&cdb[0]==0x2a) {
                assert(offset>=raw_offset&&n<=raw_offset+32768-offset);raw_bytes+=n;
                if(!strcmp(mode,"partial")&&raw_leases==1) {
                    assert(pwrite(fd,buffer,512,offset)==512);*actual=512;return false;
                }
            }
            assert((in?pread(fd,buffer,n,offset):pwrite(fd,buffer,n,offset))==(ssize_t)n);
            if(direct&&in&&!strcmp(mode,"mismatch"))((uint8_t *)buffer)[0]^=1;
        }
        done+=n;phase=done==command_length?2:1;
    } else {
        assert(in&&n==13);memset(buffer,0,n);putle(buffer,0x53425355);putle((uint8_t *)buffer+4,command_tag);
        if(cdb[0]==0x35) {assert(fsync(fd)==0);if(direct&&!strcmp(mode,"flush"))((uint8_t *)buffer)[12]=1;}
        if(cdb[0]==0x2a&&command_length==16384)clock_us+=350000;
        phase=0;
    }
    if(direct&&!strcmp(mode,"deadline")&&!clock_jump){clock_us+=180000001;clock_jump=true;}
    if(direct&&!strcmp(mode,"clock")&&!clock_jump){clock_us=1;clock_jump=true;}
    sample=(struct usb_bulk_timing){.submitted=start+1,.observed=clock_us-1,.resumed=clock_us,.seen=true,.service_valid=true,
        .service_checks=1,.service_samples=1,.service_gap_max_us=clock_us-start-2,.completion_gap_us=clock_us-start-2,.event_checks=1};
    (void)prior;return true;
}
static void timing(void *ctx,struct usb_bulk_timing *out){assert(ctx==&storage);*out=sample;}
int main(int argc,char **argv) {
    assert(argc==3);mode=argv[2];fd=open(argv[1],O_RDWR);assert(fd>=0);struct stat st;assert(!fstat(fd,&st)&&S_ISREG(st.st_mode));image_size=st.st_size;
    uint8_t boot[512];assert(pread(fd,boot,512,0)==512);sectorsize=1u<<boot[108];
    assert(usb_storage_attach(&storage,&storage,bulk,true));storage.transfer_timing=timing;
    vfs_init();assert(usb_storage_disable_volume_writes(storage.block));assert(workspace_mount("5a1b2c3d"));
    clock_us=100000;busy=!strcmp(mode,"busy");unavailable=!strcmp(mode,"timer");
    uint64_t held=!strcmp(mode,"held")?workspace_mount_hold_acquire():0;
    bool passed=workspace_storage_compare();raw_contained();
    if(!strcmp(mode,"workload")) {
        assert(passed&&raw_leases==6&&raw_bytes==6*32768&&workspace_active());
        unsigned prior=raw_bytes;assert(!workspace_storage_compare()&&raw_bytes==prior);assert(workspace_unmount());
    } else {
        assert(!passed);
        if(!strcmp(mode,"partial")||!strcmp(mode,"mismatch")||!strcmp(mode,"flush")||!strcmp(mode,"clock")||!strcmp(mode,"deadline")) {
            assert(raw_leases==1&&!workspace_parked_device()&&!workspace_mount("5a1b2c3d"));
            assert(!block_device_online(storage.block)&&!block_claim_mount(storage.block));
        } else {assert(!raw_leases);if(held)assert(workspace_mount_hold_valid(held)&&workspace_mount_hold_release(held));}
    }
    assert(depth==0);assert(fsync(fd)==0);close(fd);puts("workspace-storage-control-host: PASS real stack containment/refusal/retirement");return 0;
}
