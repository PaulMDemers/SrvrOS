/* Read-only host bridge to the kernel exFAT importer and allocation checker. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <srvros/block.h>
#include <srvros/exfat.h>
#include <srvros/vfs.h>

#define OUTPUT_LIMIT (1024u * 1024u)
#define HEAP_LIMIT (128u * 1024u * 1024u)
#define READ_LIMIT (2ULL * 1024 * 1024 * 1024)
#define CALL_LIMIT 1000000u
static int source_fd;
static uint64_t base, length, read_bytes, read_calls, live_bytes;
static unsigned writes, flushes, io_depth;
static bool read_failed;
struct allocation { max_align_t align; uint64_t size; };
void *kmalloc(uint64_t n) {
    if(n>SIZE_MAX-sizeof(struct allocation) || n>HEAP_LIMIT-live_bytes)return NULL;
    struct allocation *a=malloc(sizeof(*a)+(size_t)n); if(!a)return NULL;
    a->size=n;live_bytes+=n;return a+1;
}
void *kcalloc(uint64_t n,uint64_t s) {
    if(s && n>UINT64_MAX/s)return NULL;
    void *p=kmalloc(n*s);if(p)memset(p,0,(size_t)(n*s));return p;
}
void kfree(void *p) { if(p) { struct allocation *a=(struct allocation *)p-1;live_bytes-=a->size;free(a); } }
bool scheduler_io_begin(void) { io_depth++;return true; }
void scheduler_io_end(void) { assert(io_depth);io_depth--; }
bool scheduler_io_active(void) { return io_depth!=0; }
uint64_t timer_ticks(void) { return 100; }
int64_t timer_realtime_us(void) { return 0; }
void console_write(const char *s) { (void)s; }
void console_printf(const char *fmt,...) { (void)fmt; }
uint64_t block_device_size(const struct block_device *d) { return d->block_size*d->block_count; }
struct block_device *block_parent(struct block_device *d) { return d; }
bool block_read(struct block_device *d,uint64_t o,void *p,uint64_t n) {
    if(o>block_device_size(d) || n>block_device_size(d)-o || n>SSIZE_MAX ||
       n>READ_LIMIT-read_bytes || ++read_calls>CALL_LIMIT) { read_failed=true;return false; }
    read_bytes+=n;
    if(pread(source_fd,p,(size_t)n,(off_t)(base+o))!=(ssize_t)n) { read_failed=true;return false; }
    return true;
}
bool block_write(struct block_device *d,uint64_t o,const void *p,uint64_t n) {
    (void)d;(void)o;(void)p;(void)n;writes++;return false;
}
static bool refuse_flush(struct block_device *d) { (void)d;flushes++;return false; }
bool process_has_open_vfs_prefix(const char *p) { (void)p;return false; }
#define SRVROS_BLOCK_CLAIM_HOST_TEST
#include "../kernel/src/block_claim.c"

static char output[OUTPUT_LIMIT+1];
static size_t used;
static bool append(const char *fmt,...) {
    va_list ap;va_start(ap,fmt);int n=vsnprintf(output+used,sizeof(output)-used,fmt,ap);va_end(ap);
    if(n<0 || (size_t)n>=sizeof(output)-used)return false;
    used+=(size_t)n;return true;
}
static bool json_string(const char *s) {
    if(!append("\""))return false;
    for(const unsigned char *p=(const unsigned char *)s;*p;p++) {
        if(*p=='"' || *p=='\\') { if(!append("\\%c",*p))return false; }
        else if(*p<32) { if(!append("\\u%04x",*p))return false; }
        else if(!append("%c",*p))return false;
    }
    return append("\"");
}
static bool suffix_folded(const char *s,const char *suffix) {
    size_t n=strlen(s),m=strlen(suffix);if(n<=m)return false;s+=n-m;
    while(*suffix) { unsigned char a=(unsigned char)*s++,b=(unsigned char)*suffix++;
        if(a>='A'&&a<='Z')a+=(unsigned char)('a'-'A');
        if(a!=b)return false;
    }
    return true;
}
/* Return whether to emit a node and, when content is required, its size cap. */
static bool selected(const char *path,uint64_t *cap) {
    const char *root="/fat/home";size_t r=strlen(root);*cap=0;
    if(!strcmp(path,root))return true;
    if(strncmp(path,root,r) || path[r]!='/')return false;
    const char *a=path+r+1,*slash=strchr(a,'/');
    if(!slash) {
        if(!strcmp(a,".desktop-profile"))*cap=240;
        else if(suffix_folded(a,".profile-import.json"))*cap=4096;
        return true;
    }
    const char *b=slash+1,*next=strchr(b,'/');
    if(!next) {
        if(!strcmp(b,"profile.id"))*cap=80;
        else if(!strcmp(b,"identity.json") || !strcmp(b,"profile-local.json"))*cap=1024;
        else if(!strcmp(b,".profile-import-pending"))*cap=4096;
        return true;
    }
    if(!strcmp(b,".config/srvros") || !strcmp(b,".config/srvros/portable.json") ||
       !strcmp(b,".local/share")) {
        if(!strcmp(b,".config/srvros/portable.json"))*cap=512;
        return true;
    }
    return false;
}
static bool number(const char *s,uint64_t *v) {
    if(!*s)return false;
    uint64_t x=0;
    for(;*s;s++) { if(*s<'0'||*s>'9'||x>(UINT64_MAX-(unsigned)(*s-'0'))/10)return false;x=x*10+(unsigned)(*s-'0'); }
    *v=x;return true;
}
int main(int argc,char **argv) {
    uint64_t fdnum;struct stat st;
    const char *reason="native-fd-range";
    if(argc!=5 || strcmp(argv[1],"--fd") || !number(argv[2],&fdnum) || fdnum>INT_MAX ||
       !number(argv[3],&base) || !number(argv[4],&length) || !length || (base%512) || (length%512))goto fail;
    source_fd=(int)fdnum;int flags=fcntl(source_fd,F_GETFL);
    if(flags<0 || (flags&O_ACCMODE)!=O_RDONLY || fstat(source_fd,&st) || !S_ISREG(st.st_mode) ||
       st.st_nlink!=1 || st.st_size<0 || base>(uint64_t)st.st_size || length>(uint64_t)st.st_size-base)goto fail;
    struct block_device d={.used=true,.name="inspection",.block_size=512,.block_count=length/512,
        .read=block_read,.write=block_write,.flush=refuse_flush};
    reason="clean-admission";
    if(!exfat_mount_block_device_clean("/fat",&d))goto fail;
    struct exfat_check_result check={0};bool ok=exfat_check("/fat",&check);
    reason="allocation-inconsistent";
    if(!ok || read_failed || writes || flushes || vfs_count()>32768)goto unmount_fail;
    reason="namespace-targets";
    const char *targets[]={"/fat/bin","/fat/var","/fat/home"};
    for(size_t i=0;i<sizeof(targets)/sizeof(targets[0]);i++) {
        const struct vfs_node *target=vfs_lookup(targets[i]);
        if(!target || target->type!=VFS_NODE_DIRECTORY)goto unmount_fail;
    }
    reason="metadata-type-size-path-output-readbudget";
    if(!append("{\"version\":1,\"filesystem\":{\"clean_admission\":true,\"namespace_targets_valid\":true,\"allocation\":{\"files\":%llu,\"clusters\":%llu,\"leaked\":%llu,\"fat_bitmap_mismatches\":%llu,\"errors\":%llu},\"writes\":0,\"flushes\":0},\"entries\":[",
        (unsigned long long)check.files_checked,(unsigned long long)check.clusters_checked,
        (unsigned long long)check.leaked_clusters,(unsigned long long)check.fat_bitmap_mismatches,(unsigned long long)check.errors))goto unmount_fail;
    unsigned count=0;
    for(uint64_t i=0;i<vfs_count();i++) {
        const struct vfs_node *node=vfs_node_at(i);uint64_t cap;
        if(!node || !selected(node->path,&cap))continue;
        if(++count>4096 || strlen(node->path)>=160 ||
           (node->type!=VFS_NODE_FILE && node->type!=VFS_NODE_DIRECTORY) ||
           (cap && (node->type!=VFS_NODE_FILE || node->size>cap)))goto unmount_fail;
        if(!append("%s{\"path\":",count>1?",":"") || !json_string(node->path) ||
           !append(",\"type\":\"%s\",\"size\":%llu",node->type==VFS_NODE_DIRECTORY?"directory":"file",(unsigned long long)node->size))goto unmount_fail;
        if(cap) {
            const uint8_t *data=NULL;uint64_t size=0;
            if(!vfs_read_all(node,&data,&size) || size!=node->size)goto unmount_fail;
            bool fits=append(",\"data_hex\":\"");
            for(uint64_t j=0;fits && j<size;j++)fits=append("%02x",data[j]);
            vfs_release_data(node,data);if(!fits || !append("\""))goto unmount_fail;
        }
        if(!append("}"))goto unmount_fail;
    }
    if(!append("]}\n") || read_failed || !exfat_unmount("/fat") || writes || flushes || vfs_count())goto fail;
    if(fwrite(output,1,used,stdout)!=used || fflush(stdout) || ferror(stdout)) {
        fputs("installation inspection refused: metadata-type-size-path-output-readbudget\n",stderr);
        return 1;
    }
    return 0;
unmount_fail:
    (void)exfat_unmount("/fat");
fail:
    if(writes || flushes)reason="unexpected-write-flush";
    else if(read_failed)reason="metadata-type-size-path-output-readbudget";
    fprintf(stderr,"installation inspection refused: %s\n",reason);return 1;
}
