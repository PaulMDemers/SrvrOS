/* Offline GPT selection: actual native parser, no writable capabilities. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
static uint64_t block_test_irq_save(void) { return 0; }
static void block_test_irq_restore(uint64_t flags) { (void)flags; }
#define SRVROS_BLOCK_HOST_TEST
#include "../kernel/src/block.c"
#define SRVROS_BLOCK_CLAIM_HOST_TEST
#include "../kernel/src/block_claim.c"
static int source_fd;
static unsigned depth, calls;
static uint64_t read_bytes;
static bool read_failed;
void console_printf(const char *fmt,...) { (void)fmt; }
bool scheduler_io_begin(void) { depth++;return true; }
void scheduler_io_end(void) { assert(depth);depth--; }
bool scheduler_io_active(void) { return depth!=0; }
bool scheduler_io_yield(void) { return false; }
uint64_t timer_monotonic_us(void) { return 0; }
bool timer_reference_ready(void) { return false; }
static bool source_read(struct block_device *d,uint64_t off,void *buf,uint64_t n) {
    if(off>block_device_size(d) || n>block_device_size(d)-off || n>SSIZE_MAX ||
       n>4*1024*1024-read_bytes || ++calls>8192) {read_failed=true;return false;}
    read_bytes+=n;
    if(pread(source_fd,buf,(size_t)n,(off_t)off)!=(ssize_t)n) {read_failed=true;return false;}
    return true;
}
static bool number(const char *s,uint64_t *out) {
    uint64_t n=0;if(!*s)return false;
    for(;*s;s++) {if(*s<'0'||*s>'9'||n>(UINT64_MAX-(unsigned)(*s-'0'))/10)return false;n=n*10+(unsigned)(*s-'0');}
    *out=n;return true;
}
/* Supplement native primary validation; never reconstruct either map. */
static bool backup_agrees(struct block_device *d,const uint8_t primary[512]) {
    uint8_t backup[512],a[512],b[512];uint64_t end=d->block_count-1;
    if(!block_read(d,end*512,backup,512))return false;
    uint32_t size=read_le32(primary+12),crc=read_le32(backup+16);
    if(size<92||size>512)return false;
    if(memcmp(primary,backup,16)||read_le32(backup+20)||read_le64(backup+24)!=end||read_le64(backup+32)!=1||
       memcmp(primary+40,backup+40,32)||memcmp(primary+80,backup+80,12)||
       memcmp(primary+92,backup+92,size-92))return false;
    memset(backup+16,0,4);
    if(~gpt_crc32(UINT32_MAX,backup,size)!=crc)return false;
    uint64_t length=(uint64_t)read_le32(primary+80)*read_le32(primary+84);
    uint64_t at=read_le64(backup+72),blocks=(length+511)/512;
    if(at<=read_le64(primary+48)||at>=end||blocks>end-at)return false;
    uint32_t check=UINT32_MAX;
    for(uint64_t off=0;off<length;) {
        uint64_t n=length-off;if(n>512)n=512;
        if(!block_read(d,read_le64(primary+72)*512+off,a,n)||!block_read(d,at*512+off,b,n)||memcmp(a,b,n))return false;
        check=gpt_crc32(check,b,n);off+=n;
    }
    return ~check==read_le32(primary+88);
}
static void guid_hex(const uint8_t *g,char out[33]) {
    static const char hex[]="0123456789abcdef";
    for(unsigned i=0;i<16;i++){out[i*2]=hex[g[i]>>4];out[i*2+1]=hex[g[i]&15];}out[32]=0;
}
int main(int argc,char **argv) {
    uint64_t fdnum,base,length;struct stat st;const char *reason="native-fd-range";
    if(argc!=5||strcmp(argv[1],"--fd")||!number(argv[2],&fdnum)||fdnum>INT_MAX||
       !number(argv[3],&base)||base||!number(argv[4],&length)||!length||length%512||length>8ULL*1024*1024*1024)goto fail;
    source_fd=(int)fdnum;int flags=fcntl(source_fd,F_GETFL);
    if(flags<0||(flags&O_ACCMODE)!=O_RDONLY||fstat(source_fd,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1||
       st.st_size<0||(uint64_t)st.st_size!=length)goto fail;
    block_init();struct block_device *d=block_register("image",512,length/512,source_read,NULL,NULL);
    struct block_gpt_map map={0};reason="primary-gpt";
    /* Prefetch the primary header used by native validation; keep its bounds
     * after the table scan evicts it from the cache. Never reread new bounds. */
    uint8_t primary[512];
    if(!d||!block_read(d,512,primary,512)||!block_gpt_scan(d,&map)||read_failed)goto fail;
    reason="backup-gpt";if(!backup_agrees(d,primary)||read_failed)goto fail;
    static const uint8_t basic[16]={0xa2,0xa0,0xd0,0xeb,0xe5,0xb9,0x33,0x44,0x87,0xc0,0x68,0xb6,0xb7,0x26,0x99,0xc7};
    struct block_gpt_partition *selected=NULL;unsigned candidates=0;
    for(unsigned i=0;i<map.count;i++) {
        struct block_device *v=map.partitions[i].view;
        reason="writable-view";if(!v||v->write||v->flush||d->write||d->flush)goto fail;
        if(!memcmp(map.partitions[i].type,basic,16)){selected=&map.partitions[i];candidates++;}
    }
    reason="workspace-candidate-count";if(candidates!=1)goto fail;
    struct block_device *v=selected->view;
    reason="workspace-range";if(!block_device_size(v)||block_device_size(v)>2ULL*1024*1024*1024)goto fail;
    char disk[33],part[33];guid_hex(map.disk_guid,disk);guid_hex(selected->guid,part);
    printf("{\"version\":1,\"kind\":\"workspace-gpt-selection\",\"primary_validated\":true,\"backup_agrees\":true,\"writable_admission\":false,\"disk_guid_le\":\"%s\",\"partition_guid_le\":\"%s\",\"partitions\":%u,\"candidates\":1,\"offset_bytes\":%llu,\"size_bytes\":%llu}\n",disk,part,map.count,(unsigned long long)v->backing_offset,(unsigned long long)block_device_size(v));
    return fflush(stdout)||ferror(stdout)?1:0;
fail:
    fprintf(stderr,"workspace GPT selection refused: %s\n",reason);return 1;
}
