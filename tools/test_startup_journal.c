#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <srvros/startup_journal.h>
#include <srvros/block.h>
#include <srvros/bootlog.h>
#include <srvros/initramfs.h>
#include <srvros/sha256.h>
#include "../kernel/src/startup_journal.c"

static uint8_t medium[3*1024*1024], before[sizeof medium], descriptor[128];
static struct block_device devices[2];
static struct initramfs_file module;
static unsigned writes,flushes,fail_write,fail_flush;
static uint64_t fail_read_offset,reason_mask;
static bool claim_refused,corrupt_readback;
static const uint64_t base=1048576+8192;
void console_write(const char *s) {(void)s;}
void console_printf(const char *s,...) {
    if(!strstr(s,"startup-journal-refused:"))return;
    va_list args;va_start(args,s);uint64_t reason=va_arg(args,uint64_t);va_end(args);
    assert(reason<64);reason_mask|=1ull<<reason;
}
const struct initramfs_file *initramfs_find(const char *p) {assert(!strcmp(p,"/startup-journal.bin"));return &module;}
struct block_device *block_device_at(uint64_t i) {return i<2?&devices[i]:NULL;}
uint64_t block_claim_raw(struct block_device *d,struct block_device *view) {assert(d==&devices[0] && !view);return claim_refused?0:1;}
bool block_claim_release(uint64_t token) {assert(token==1);return true;}
void block_forget(struct block_device *d) {assert(d==&devices[0]);}
uint64_t bootlog_snapshot(char *out,uint64_t cap,uint64_t *total) {
    const char *s="workspace: USB volume absent, ambiguous or already writable\n";
    assert(cap>=strlen(s));memcpy(out,s,strlen(s));*total=strlen(s)+10000;return strlen(s);
}
static bool read_disk(struct block_device *d,uint64_t offset,void *p,uint64_t size) {
    (void)d;assert(offset<=sizeof medium && size<=sizeof medium-offset);
    if(fail_read_offset && offset==fail_read_offset)return false;
    memcpy(p,medium+offset,size);
    if(corrupt_readback && writes && offset>=base+SECTOR)((uint8_t *)p)[0]^=1;
    return true;
}
static bool write_disk(struct block_device *d,uint64_t offset,const void *p,uint64_t size) {
    assert(d==&devices[0] && offset>=base+SECTOR && offset+size<=base+FILE_BYTES);
    writes++;if(writes==fail_write)return false;
    memcpy(medium+offset,p,size);return true;
}
static bool flush_disk(struct block_device *d) {(void)d;flushes++;return flushes!=fail_flush;}
static void setup(void) {
    memset(medium,0xa5,sizeof medium);memset(descriptor,0,sizeof descriptor);
    memcpy(descriptor,"SRVRSTARTDESC1",14);put64(descriptor+16,sizeof medium/SECTOR);
    put64(descriptor+24,base);put64(descriptor+32,1048576);put64(descriptor+40,sizeof medium-1048576);
    memset(nonce,0x37,16);memcpy(descriptor+48,nonce,16);
    sha256_digest(medium+SECTOR,SECTOR,descriptor+64);
    sha256_digest(medium+1048576,SECTOR,descriptor+96);
    header(medium+base,"SRVRSTARTFILE1");put64(medium+base+32,FILE_BYTES);
    for(unsigned i=0;i<SLOT_COUNT;i++) {
        uint8_t *slot=medium+base+SECTOR+i*SLOT_BYTES;
        memset(slot,0,SLOT_BYTES);header(slot,"SRVRSTARTEMPTY1");put32(slot+32,i);
    }
    memcpy(before,medium,sizeof medium);
    devices[0]=(struct block_device){.used=true,.name="ahci0",.block_size=SECTOR,.block_count=sizeof medium/SECTOR,
        .read=read_disk,.write=write_disk,.flush=flush_disk};
    module=(struct initramfs_file){"/startup-journal.bin",descriptor,sizeof descriptor,0400};
}
static void verify_outside(void) {
    assert(!memcmp(medium,before,base+SECTOR));
    assert(!memcmp(medium+base+FILE_BYTES,before+base+FILE_BYTES,sizeof medium-base-FILE_BYTES));
}
static void run(unsigned test,const char *output) {
    setup();
    switch(test) {
    case 1:descriptor[0]^=1;break;
    case 2:put64(descriptor+24,UINT64_MAX-511);break;
    case 3:descriptor[64]^=1;break;
    case 4:descriptor[96]^=1;break;
    case 5:medium[base]^=1;break;
    case 6:devices[1]=devices[0];break;
    case 7:claim_refused=true;break;
    case 8:devices[0].name="usb0";break;
    case 9:devices[0].backing=&devices[1];break;
    case 10:devices[0].block_count--;break;
    case 11:module.size--;break;
    case 12:memset(descriptor+48,0,16);break;
    case 13:devices[0].flush=NULL;break;
    case 21:fail_read_offset=512;break;
    case 22:fail_read_offset=1048576;break;
    case 23:fail_read_offset=base;break;
    default:break;
    }
    bool opened=startup_journal_open();assert(!startup_journal_open());
    if((test>=1 && test<=13) || test>=21) {
        const unsigned reasons[]={0,3,3,6,8,10,11,12,13,13,13,2,4,13};
        unsigned reason=test<=13?reasons[test]:test==21?5:test==22?7:9;
        assert(!opened && !writes && !flushes && (reason_mask&(1ull<<reason)));
        return;
    }
    assert(opened);
    if(test==14)medium[base+SECTOR+40]^=1;
    if(test==15)fail_write=1;
    if(test==16)fail_flush=1;
    if(test==17)corrupt_readback=true;
    if(test==18)fail_write=2;
    if(test==19)fail_flush=2;
    if(test==20){assert(!startup_journal_checkpoint(2,0));assert(!startup_journal_checkpoint(1,64));assert(!writes);}
    if(test>=14 && test<=19){assert(!startup_journal_checkpoint(1,0));assert(!startup_journal_checkpoint(1,0));assert(!startup_journal_close());verify_outside();return;}
    for(unsigned phase=1;phase<=7;phase++)assert(startup_journal_checkpoint(phase,phase>=3?12:0));
    assert(!startup_journal_checkpoint(8,0));assert(writes==14 && flushes==14);
    verify_outside();
    if(output) {
        FILE *f=fopen(output,"wb");assert(f && fwrite(medium+base,1,FILE_BYTES,f)==FILE_BYTES && !fclose(f));
        char name[4096];assert(snprintf(name,sizeof name,"%s.desc",output)<(int)sizeof name);
        f=fopen(name,"wb");assert(f && fwrite(descriptor,1,sizeof descriptor,f)==sizeof descriptor && !fclose(f));
    }
    assert(startup_journal_close());assert(!startup_journal_close());
}
int main(int argc,char **argv) {
    for(unsigned test=0;test<=23;test++) {
        pid_t pid=fork();assert(pid>=0);
        if(!pid){run(test,test==0 && argc==2?argv[1]:NULL);_exit(0);}
        int status;assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    }
    puts("startup-journal: PASS 24 actual writer scenarios, bounded writes, readback/commit ordering, identity and partial-I/O refusals");
}
