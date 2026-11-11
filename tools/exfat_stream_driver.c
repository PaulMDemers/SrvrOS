/* Real exFAT backend against a disposable synthetic volume. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
static size_t measured_max_malloc;
static void *measured_malloc(size_t n) {
    if(n>measured_max_malloc)measured_max_malloc=n;
    return malloc(n);
}
#define malloc measured_malloc
#define block_read stream_mock_read
#define block_write stream_mock_write
#define main exfat_host_legacy_main
#include "exfat_host_driver.c"
#undef main
#undef malloc
#undef block_read
#undef block_write
#include <srvros/exfat_stream.h>
#include <srvros/syscall_numbers.h>
static uint64_t max_read_transfer,max_write_transfer;
static bool fail_after_mutation_write;
bool block_read(struct block_device *d,uint64_t o,void *p,uint64_t n) {
    if(n>max_read_transfer)max_read_transfer=n;
    assert(n<=65536);return stream_mock_read(d,o,p,n);
}
bool block_write(struct block_device *d,uint64_t o,const void *p,uint64_t n) {
    if(n>max_write_transfer)max_write_transfer=n;
    assert(n<=65536);
    bool ok=stream_mock_write(d,o,p,n);
    /* Fault by publication event, not a fragile heap-call ordinal. */
    if(ok && fail_after_mutation_write && o!=106) {
        fail_after_mutation_write=false;fail_alloc_at=alloc_calls+1;
    }
    return ok;
}

static unsigned ram_reads;
static bool ram_read(const struct vfs_node *n,const uint8_t **p,uint64_t *size) {
    (void)n;(void)p;(void)size;ram_reads++;return false;
}
static unsigned char pattern(uint64_t i) { return (unsigned char)((i*29u+(i>>13)+17u)&255u); }
int main(int argc,char **argv) {
    assert(argc==4);fd=open(argv[1],O_RDWR);assert(fd>=0);
    struct stat st;assert(fstat(fd,&st)==0);
    struct block_device d={.used=true,.name="stream-fixture",.block_size=512,.block_count=(uint64_t)st.st_size/512,
        .read=block_read,.write=block_write,.flush=flush};
    assert(exfat_mount_block_device("/vol",&d));
    assert(vfs_register_file("/ram-file",0,NULL,ram_read));
    assert(exfat_stream_open_read("/ram-file")==SRV_ERR_NOTSUP && !ram_reads);
    assert(exfat_stream_open_read("/absent")==SRV_ERR_NOENT);
    if(!strcmp(argv[3],"read-fragment") || !strcmp(argv[3],"read-zero") || !strcmp(argv[3],"read-alias") || !strcmp(argv[3],"read-nlink") || !strcmp(argv[3],"read-fault")) {
        bool alias=!strcmp(argv[3],"read-alias"), zero=!strcmp(argv[3],"read-zero");
        if(!strcmp(argv[3],"read-nlink")) {
            const struct vfs_node *node=vfs_lookup("/vol/fixture/input");assert(node);
            struct vfs_metadata metadata=node->metadata;metadata.nlink=2;
            assert(vfs_set_metadata("/vol/fixture/input",&metadata));
            assert(exfat_stream_open_read("/vol/fixture/input")<0);
            printf("nlink PASS\n");close(fd);return 0;
        }
        int64_t rh=exfat_stream_open_read("/vol/fixture/input");
        if(alias) { assert(rh<0);printf("alias PASS\n");close(fd);return 0; }
        assert(rh>0);unsigned char bytes[8192];
        if(!strcmp(argv[3],"read-fault")) {
            const char *target=getenv("STREAM_READ_FAULT_OFFSET");assert(target);
            assert(setenv("SRVROS_FAIL_READ_OFFSET",target,1)==0);
            assert(exfat_stream_pread((uint64_t)rh,bytes,100,0)<0);
            assert(unsetenv("SRVROS_FAIL_READ_OFFSET")==0);
        }
        uint64_t size=strtoull(argv[2],NULL,10);
        assert(size<=sizeof(bytes));
        assert(exfat_stream_pread((uint64_t)rh,bytes,size,0)==(int64_t)size);
        for(uint64_t j=0;j<size;j++)assert(bytes[j]==(zero && j>=1000?0:pattern(j)));
        assert(exfat_stream_pread((uint64_t)rh,bytes,100,4000)==100);
        for(uint64_t j=0;j<100;j++)assert(bytes[j]==(zero?0:pattern(4000+j)));
        assert(exfat_stream_pread((uint64_t)rh,bytes,10,10)==10);
        for(uint64_t j=0;j<10;j++)assert(bytes[j]==pattern(10+j));
        assert(exfat_stream_close((uint64_t)rh)==0);assert(exfat_unmount("/vol"));
        printf("%s PASS\n",argv[3]);close(fd);return 0;
    }
    bool fragmented=!strcmp(argv[3],"fragment-write");
    if(fragmented) {
        unsigned char *fill=calloc(1,65536);assert(fill);
        for(unsigned i=0;i<30;i++) { char path[40];snprintf(path,sizeof(path),"/vol/gap-%02u",i);assert(exfat_write_file(path,fill,65536)); }
        for(unsigned i=0;i<30;i+=2) { char path[40];snprintf(path,sizeof(path),"/vol/gap-%02u",i);assert(exfat_delete_file(path)); }
        free(fill);
    }
    uint64_t baseline=live_bytes;peak_bytes=live_bytes;measured_max_malloc=0;
    uint64_t count=strtoull(argv[2],NULL,10);const char *mode=fragmented?"ok":argv[3];
    int64_t h=exfat_stream_create("/vol/stream.bin",0640);assert(h>0);
    assert(exfat_stream_append((uint64_t)h,NULL,0)==0);
    assert(exfat_stream_busy("/VOL/STREAM.BIN",false));
    const uint8_t *snapshot=NULL;uint64_t snapshot_size=0;
    assert(!vfs_read_all(vfs_lookup("/vol/stream.bin"),&snapshot,&snapshot_size));
    assert(exfat_stream_open_read("/vol/.srvros/meta")<0);assert(exfat_stream_busy("/vol",true));assert(exfat_stream_busy("/",true));
    assert(!exfat_write_file("/vol/stream.bin",(const uint8_t *)"x",1));
    assert(!exfat_delete_file("/vol/stream.bin"));assert(!exfat_rename("/vol/stream.bin","/vol/renamed"));
    assert(!exfat_unmount("/vol"));
    unsigned char *buf=malloc(65536);assert(buf);
    uint64_t at=0;
    while(at<count) {
        uint64_t n=count-at;if(n>65536)n=65536;if(!at && n>24)n=24;
        for(uint64_t j=0;j<n;j++)buf[j]=pattern(at+j);
        assert(exfat_stream_append((uint64_t)h,buf,n)==(int64_t)n);at+=n;
    }
    if(!strcmp(mode,"sync-fault") || !strcmp(mode,"close-fault")) {
        fail_flush_at=flushes+1;
        if(!strcmp(mode,"sync-fault")) {assert(exfat_stream_sync((uint64_t)h)<0);
            assert(exfat_stream_append((uint64_t)h,NULL,0)<0); }
        assert(exfat_stream_close((uint64_t)h)<0);
        assert(!exfat_stream_busy("/vol/stream.bin",false));assert(!exfat_is_writable("/vol"));
        printf("fault=%s PASS\n",mode);free(buf);close(fd);return 0;
    }
    if(!strcmp(mode,"probe")) {
        unsigned w=writes,f=flushes;buf[0]=pattern(count);
        assert(exfat_stream_append((uint64_t)h,buf,1)==1);count++;
        printf("append-writes=%u append-flushes=%u\n",writes-w,flushes-f);mode="ok";
    }
    if(!strcmp(mode,"read-clean")) {
        unsigned char boot[512];assert(pread(fd,boot,sizeof(boot),0)==sizeof(boot));
        uint32_t heap;memcpy(&heap,boot+88,4);uint64_t cluster=(uint64_t)1<<(boot[108]+boot[109]);
        uint64_t bitmap=(uint64_t)heap*(1u<<boot[108])+cluster;
        char text[32];snprintf(text,sizeof(text),"%llu",(unsigned long long)bitmap);
        assert(setenv("SRVROS_FAIL_READ_OFFSET",text,1)==0);unsigned w=writes,f=flushes;
        assert(exfat_stream_append((uint64_t)h,buf,1)<0);
        assert(w==writes && f==flushes && exfat_is_writable("/vol"));
        assert(unsetenv("SRVROS_FAIL_READ_OFFSET")==0);mode="ok";
    }
    if(!strcmp(mode,"lowspace")) {
        unsigned char boot[512];assert(pread(fd,boot,sizeof(boot),0)==sizeof(boot));
        uint32_t heap;memcpy(&heap,boot+88,4);uint64_t cluster=(uint64_t)1<<(boot[108]+boot[109]);
        uint64_t bitmap=(uint64_t)heap*(1u<<boot[108])+cluster;
        unsigned char *saved=malloc(cluster),*full=malloc(cluster);assert(saved && full);
        assert(pread(fd,saved,cluster,bitmap)==(ssize_t)cluster);memset(full,255,cluster);
        assert(pwrite(fd,full,cluster,bitmap)==(ssize_t)cluster);
        unsigned w=writes,f=flushes;assert(exfat_stream_append((uint64_t)h,buf,1)==-28);
        assert(w==writes && f==flushes && exfat_is_writable("/vol"));
        assert(pwrite(fd,saved,cluster,bitmap)==(ssize_t)cluster);free(saved);free(full);
        mode="ok";
    }
    if(!strcmp(mode,"alloc-dirty"))fail_after_mutation_write=true;
    if(!strncmp(mode,"write-fault",11))fail_at=writes+strtoul(mode+11,NULL,10);
    if(!strncmp(mode,"flush-fault",11))fail_flush_at=flushes+strtoul(mode+11,NULL,10);
    if(!strcmp(mode,"alloc-clean")) {
        fail_alloc_at=alloc_calls+1;unsigned w=writes,f=flushes;
        assert(exfat_stream_append((uint64_t)h,buf,1)<0);
        assert(w==writes && f==flushes && exfat_is_writable("/vol"));fail_alloc_at=0;mode="ok";
    }
    if(strcmp(mode,"ok")) {
        assert(exfat_stream_append((uint64_t)h,buf,1)<0);
        assert(exfat_stream_append((uint64_t)h,buf,1)<0);
        assert(exfat_stream_sync((uint64_t)h)<0);assert(exfat_stream_close((uint64_t)h)<0);
        assert(!exfat_stream_busy("/vol/stream.bin",false));
        assert(!exfat_is_writable("/vol"));
        printf("fault=%s PASS\n",mode);free(buf);close(fd);return 0;
    }
    assert(exfat_stream_sync((uint64_t)h)==0);assert(exfat_stream_close((uint64_t)h)==0);
    assert(exfat_stream_close((uint64_t)h)<0);
    h=exfat_stream_open_read("/vol/stream.bin");assert(h>0);
    struct srv_file_stream_stat info;assert(exfat_stream_stat((uint64_t)h,&info)==0 && info.size==count && (info.mode&0777)==0640);
    for(at=0;at<count;) {
        uint64_t n=count-at;if(n>65536)n=65536;
        assert(exfat_stream_pread((uint64_t)h,buf,n,at)==(int64_t)n);
        for(uint64_t j=0;j<n;j++)assert(buf[j]==pattern(at+j));
        at+=n;
    }
    assert(exfat_stream_pread((uint64_t)h,buf,1,count)==0);
    int64_t stale=h;assert(exfat_stream_close((uint64_t)h)==0);
    h=exfat_stream_open_read("/vol/stream.bin");assert(h>0 && h!=stale);
    assert(exfat_stream_pread((uint64_t)stale,buf,1,0)<0);assert(exfat_stream_close((uint64_t)h)==0);
    assert(exfat_unmount("/vol"));
    assert(exfat_mount_block_device("/vol",&d));h=exfat_stream_open_read("/vol/stream.bin");assert(h>0);
    for(at=0;at<count;) {
        uint64_t n=count-at;if(n>65536)n=65536;
        assert(exfat_stream_pread((uint64_t)h,buf,n,at)==(int64_t)n);
        for(uint64_t j=0;j<n;j++)assert(buf[j]==pattern(at+j));
        at+=n;
    }
    assert(exfat_stream_close((uint64_t)h)==0);
    struct exfat_check_result check={0};assert(exfat_check("/vol",&check));
    assert(exfat_unmount("/vol"));
    printf("bytes=%llu kernel_baseline=%llu kernel_peak=%llu kernel_increment=%llu max_allocation=%llu max_read=%llu max_write=%llu writes=%u flushes=%u PASS\n",
        (unsigned long long)count,(unsigned long long)baseline,(unsigned long long)peak_bytes,
        (unsigned long long)(peak_bytes-baseline),
        (unsigned long long)(measured_max_malloc?measured_max_malloc-sizeof(struct allocation):0),
        (unsigned long long)max_read_transfer,(unsigned long long)max_write_transfer,writes,flushes);
    free(buf);close(fd);return 0;
}
