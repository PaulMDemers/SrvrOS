/* Execute the real filesystem and VFS on disposable host image files. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stddef.h>
#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif
#include <srvros/block.h>
#include <srvros/path_limits.h>
#include <srvros/exfat.h>
#include <srvros/vfs.h>
#include <srvros/autotest_storage.h>
static int fd;
static uint64_t read_calls,read_bytes,write_bytes;
static unsigned writes, flushes, fail_at, crash_at, fail_flush_at, crash_flush_at, tear_at;
static bool failed, foreign;
static unsigned io_depth;
bool scheduler_io_begin(void) { if(foreign) return false; io_depth++; return true; }
void scheduler_io_end(void) { assert(io_depth); io_depth--; }
bool scheduler_io_active(void) { return foreign || io_depth != 0; }
static uint64_t alloc_calls, fail_alloc_at, live_bytes, peak_bytes;
struct allocation { max_align_t alignment; uint64_t size; };
void *kmalloc(uint64_t n) {
    if(++alloc_calls==fail_alloc_at || n>SIZE_MAX-sizeof(struct allocation)) return NULL;
    struct allocation *p=malloc(sizeof(*p)+n);if(!p)return NULL;
    p->size=n;live_bytes+=n;if(live_bytes>peak_bytes)peak_bytes=live_bytes;return p+1;
}
void *kcalloc(uint64_t n, uint64_t size) {
    if(size && n>SIZE_MAX/size)return NULL;
    void *p=kmalloc(n*size);if(p)memset(p,0,n*size);return p;
}
void kfree(void *p) { if(p) { struct allocation *a=(struct allocation *)p-1;live_bytes-=a->size;free(a); } }
uint64_t timer_ticks(void) { return 100; }
static int64_t mock_realtime=1792326896000000LL;
int64_t timer_realtime_us(void) { return mock_realtime; }
void console_write(const char *s) { (void)s; }
void console_printf(const char *fmt, ...) { (void)fmt; }
uint64_t block_device_size(const struct block_device *d) { return d->block_size*d->block_count; }
struct block_device *block_parent(struct block_device *d) { return d; }
bool block_read(struct block_device *d, uint64_t o, void *p, uint64_t n) {
    read_calls++;read_bytes+=n;
    if(getenv("SRVROS_FAIL_READ_OFFSET") && o==strtoull(getenv("SRVROS_FAIL_READ_OFFSET"),NULL,10))return false;
    return o+n <= block_device_size(d) && pread(fd,p,n,o)==(ssize_t)n;
}
bool block_write(struct block_device *d, uint64_t o, const void *p, uint64_t n) {
    write_bytes+=n;
    if (failed || ++writes == fail_at) { failed=true; return false; }
    assert(o+n <= block_device_size(d));
    if(getenv("SRVROS_TRACE_WRITES"))printf("write-trace: call=%u offset=%llu bytes=%llu\n",writes,(unsigned long long)o,(unsigned long long)n);
    if(writes==tear_at) {
        assert(pwrite(fd,p,(n+1)/2,o)==(ssize_t)((n+1)/2));
        assert(fsync(fd)==0);_exit(79);
    }
    if(pwrite(fd,p,n,o)!=(ssize_t)n) return false;
    if(writes==crash_at) { fsync(fd); _exit(77); }
    return true;
}
static bool flush(struct block_device *d) { (void)d; flushes++; if(flushes==fail_flush_at) failed=true; if(failed || fsync(fd)) return false;
    if(flushes==crash_flush_at)_exit(78);
    return true; }
/* Advertise transport capabilities for the eligibility predicate, but never
 * mutate media. Any attempted mutation is also an assertion failure below. */
static bool refuse_write(struct block_device *d,uint64_t o,const void *p,uint64_t n) {
    (void)d;(void)o;(void)p;(void)n;writes++;return false;
}
static bool refuse_flush(struct block_device *d) { (void)d;flushes++;return false; }
static void equal(const char *path, const uint8_t *expected, uint64_t n) {
    const struct vfs_node *node=vfs_lookup(path); const uint8_t *data; uint64_t size;
    assert(node && vfs_read_all(node,&data,&size));
    assert(size==n && (!n || !memcmp(data,expected,n))); vfs_release_data(node,data);
}
int main(int argc, char **argv) {
    assert(argc>=3);
    bool image_range=!strcmp(argv[2],"image-range");
    bool headroom_only=!strcmp(argv[2],"headroom-check");
    bool writable_only=!strcmp(argv[2],"writable-check");
    bool check_only=writable_only||headroom_only||image_range||!strcmp(argv[2],"allocation-check");
    fd=open(argv[1],check_only ? O_RDONLY : O_RDWR); assert(fd>=0);
    struct stat st; assert(fstat(fd,&st)==0);
    uint64_t bytes=st.st_size;
    if(!S_ISREG(st.st_mode)) {
#ifdef __linux__
        assert(check_only && S_ISBLK(st.st_mode));
        assert(ioctl(fd,BLKGETSIZE64,&bytes)==0);
#else
        assert(false);
#endif
    }
    struct block_device d={.used=true,.name="fixture",.block_size=512,.block_count=bytes/512,
        .read=block_read,.write=writable_only ? refuse_write : check_only ? NULL : block_write,
        .flush=writable_only ? refuse_flush : check_only ? NULL : flush};
    if(getenv("SRVROS_TEST_BLOCK_BYTES")) {
        uint64_t block=strtoull(getenv("SRVROS_TEST_BLOCK_BYTES"),NULL,10);
        assert((block==512 || block==4096) && !(bytes%block));d.block_size=block;d.block_count=bytes/block;
    }
    if(check_only) {
        if(!exfat_mount_block_device("/vol",&d)) {
            fputs("allocation-check: unsupported or invalid filesystem; NOT CHECKED\n",stderr);
            close(fd);return 2;
        }
        struct exfat_check_result result={0};
        bool ok=exfat_check("/vol",&result);
        bool graph_ok=ok;
        if(writable_only) {
            bool writable=exfat_is_writable("/vol");
            printf("writable-check: %s zero_writes=yes\n",writable?"PASS":"REFUSED");
            ok=ok && writable;
        }
        if(ok&&image_range) {
            struct block_device *resolved=NULL;uint64_t offset=0;
            ok=exfat_exclusive_range("/vol/srvros-work/battery-fs.img",16u*1024u*1024u,&resolved,&offset)&&resolved==&d;
            printf("image-range: %s offset=%llu bytes=16777216\n",ok?"PASS":"REFUSED",(unsigned long long)offset);
        }
        if(headroom_only) {
            bool room=exfat_creation_capacity("/vol/new-trial",32,8);
            printf("headroom-check: %s reserve_files=32 reserve_directories=8 plus_metadata=yes\n",room?"PASS":"REFUSED");
            ok=ok && room;
        }
        assert(!writes && !flushes);
        printf("allocation-check: %s files=%llu clusters=%llu leaked=%llu fat_bitmap_mismatches=%llu errors=%llu\n",
            graph_ok ? "PASS" : "FAIL",(unsigned long long)result.files_checked,
            (unsigned long long)result.clusters_checked,(unsigned long long)result.leaked_clusters,
            (unsigned long long)result.fat_bitmap_mismatches,(unsigned long long)result.errors);
        close(fd);return ok ? 0 : 1;
    }
    if(!strcmp(argv[2],"memory-mount")) {
        assert(argc==4);fail_alloc_at=strtoull(argv[3],NULL,10);
        assert(!exfat_mount_block_device("/vol",&d));
        fail_alloc_at=0;assert(!vfs_count() && !vfs_lookup("/vol") && !writes && !flushes);
        assert(exfat_mount_block_device("/vol",&d));
        assert(exfat_unmount("/vol") && !vfs_count() && !writes && !flushes);
        uint64_t retained=live_bytes;
        assert(exfat_mount_block_device("/vol",&d));assert(exfat_unmount("/vol"));
        assert(live_bytes==retained); /* Only bounded VFS high-water blocks survive. */
        printf("memory mount rollback/retry PASS retained=%llu peak=%llu\n",(unsigned long long)retained,(unsigned long long)peak_bytes);
        close(fd);return 0;
    }
    if(!strcmp(argv[2],"noflush")) d.flush=NULL;
    if(!strcmp(argv[2],"snapshot-seed")) {
        assert(argc==4 && strlen(argv[3])==16);
        for(unsigned i=0;i<16;i++) assert(strchr("0123456789abcdef",argv[3][i]));
        assert(exfat_mount_block_device("/testlog",&d));
        for(unsigned i=0;i<5;i++) {
            char run[17],path[96],request[64]; strcpy(run,argv[3]);
            unsigned digit=(unsigned)(strchr("0123456789abcdef",run[15])-"0123456789abcdef");
            run[15]="0123456789abcdef"[digit^i];
            snprintf(path,sizeof(path),"/testlog/srvros-tests/%s.req",run);
            assert(!vfs_lookup(path));
            snprintf(request,sizeof(request),"SRVROS-AUTOTEST-V3 %s\n",run);
            assert(exfat_write_file(path,(const uint8_t *)request,strlen(request)));
        }
        assert(fsync(fd)==0);close(fd);puts("snapshot: five fresh requests prepared");return 0;
    }
    if(argc>3) fail_at=strtoul(argv[3],0,10);
    if(argc>4) crash_at=strtoul(argv[4],0,10);
    if(argc>5) fail_flush_at=strtoul(argv[5],0,10);
    if(argc>6) crash_flush_at=strtoul(argv[6],0,10);
    if(argc>7) tear_at=strtoul(argv[7],0,10);
    if(!strcmp(argv[2],"inspect-cut")) {
        bool mounted=exfat_mount_block_device("/vol",&d);
        assert(!writes && !flushes);
        if(mounted) {
            equal("/vol/anchor.txt",(const uint8_t *)"unchanged",9);
            assert(!exfat_is_writable("/vol"));
            unsigned before=writes;
            assert(!exfat_write_file("/vol/forbidden",NULL,0) && writes==before);
            assert(exfat_unmount("/vol"));
        }
        printf("interrupted media: %s, no recovery writes\n",mounted?"quarantined":"import refused");
        close(fd);return 0;
    }
    if(!strcmp(argv[2],"workload")) {
        assert(exfat_mount_block_device("/testlog",&d));
        assert(exfat_create_directory("/testlog/srvros-tests"));
        assert(autotest_storage_run(&d,"0123456789abcdef"));
        unsigned before=writes;
        assert(!autotest_storage_run(&d,"0123456789abcdef"));
        assert(!autotest_storage_run(&d,"../bad"));
        assert(before==writes);
        assert(fsync(fd)==0); close(fd);
        puts("hardware file workload/remount and existing-directory refusal PASS");
        return 0;
    }
    if(!strcmp(argv[2],"scratch-refuse")) {
        /* Strict import may reject malformed ownership/stream metadata before
         * a scratch extent is available. Neither refusal may mutate media. */
        bool mounted=exfat_mount_block_device("/vol",&d);
        assert(!writes && !flushes);
        struct block_device *resolved=NULL;uint64_t offset=UINT64_MAX;
        assert(!exfat_scratch_range("/vol/scratch.bin",&resolved,&offset));
        assert(!resolved && offset==UINT64_MAX && !writes && !flushes);
        printf("scratch refused: %s\n",mounted?"extent":"import");
        close(fd);return 0;
    }
    if(!strcmp(argv[2],"foreign")) {
        foreign=true;
        assert(!exfat_mount_block_device("/vol",&d) && !writes && !flushes && !vfs_count());
        foreign=false;
    }
    assert(exfat_mount_block_device("/vol",&d));
    if(!strcmp(argv[2],"foreign")) foreign=true;
    if(!strcmp(argv[2],"append-seed") || !strcmp(argv[2],"append-step") || !strcmp(argv[2],"append-repeat") || !strcmp(argv[2],"append-changed")) {
        struct exfat_fsinfo info;assert(exfat_statfs("/vol",&info));
        uint64_t initial=info.block_size*2,maximum=initial+info.block_size*3+512;
        uint8_t *data=malloc(maximum);assert(data);
        for(uint64_t i=0;i<maximum;i++)data[i]=(uint8_t)(i*17+37);
        if(!strcmp(argv[2],"append-seed")) {
            assert(exfat_write_file("/vol/keep.txt",data,initial));
            assert(exfat_write_file("/vol/anchor.txt",(const uint8_t *)"unchanged",9));
        } else if(!strcmp(argv[2],"append-step") || !strcmp(argv[2],"append-changed")) {
            if(!strcmp(argv[2],"append-changed"))data[0]^=1;
            bool ok=exfat_write_file("/vol/keep.txt",data,maximum);
            printf("operation=%s writes=%u flushes=%u\n",ok?"PASS":"FAIL",writes,flushes);
            if(failed) {unsigned before=writes;assert(!exfat_write_file("/vol/forbidden",data,1)&&before==writes);}
        } else {
            uint64_t step=info.block_size<16384?512:16384;
            for(uint64_t n=initial;n<maximum;) {
                n+=maximum-n<step?maximum-n:step;
                assert(exfat_write_file("/vol/keep.txt",data,n));equal("/vol/keep.txt",data,n);
            }
            assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
            equal("/vol/keep.txt",data,maximum);
        }
        free(data);assert(fsync(fd)==0);close(fd);
        printf("driver: write_bytes=%llu\n",(unsigned long long)write_bytes);
        printf("driver: complete writes=%u flushes=%u\n",writes,flushes);return 0;
    }
    if(!strcmp(argv[2],"bitmap-stats")) {
        struct exfat_fsinfo info;assert(exfat_statfs("/vol",&info));
        printf("bitmap-used=%llu\n",(unsigned long long)(info.blocks-info.blocks_free));
        assert(!writes && !flushes);close(fd);return 0;
    }
    if(!strcmp(argv[2],"retirement-seed")) {
        uint8_t data[1500];memset(data,'R',sizeof(data));
        assert(exfat_write_file("/vol/keep.txt",data,sizeof(data)));
        assert(exfat_create_directory("/vol/empty"));
        assert(fsync(fd)==0);close(fd);return 0;
    }
    if(!strcmp(argv[2],"growth-seed")) {
        assert(exfat_create_directory("/vol/grow"));
        assert(fsync(fd)==0);close(fd);return 0;
    }
    if(!strcmp(argv[2],"recover")) {
        /* The mount performs sidecar promotion. Faults can interrupt it. */
    } else if(!strncmp(argv[2],"verify-grow-",12)) {
        const char *path=!strcmp(argv[2],"verify-grow-root")?"/vol/new.txt":"/vol/grow/new.txt";
        equal(path,(const uint8_t *)"new-file-data",13);
        equal("/vol/anchor.txt",(const uint8_t *)"unchanged",9);
        struct exfat_check_result check;assert(exfat_check("/vol",&check));
    } else if(!strncmp(argv[2],"verify-retire-",14)) {
        assert(exfat_is_writable("/vol"));
        const char *operation=argv[2]+14;
        if(!strcmp(operation,"delete"))assert(!vfs_lookup("/vol/keep.txt"));
        else if(!strcmp(operation,"directory"))assert(!vfs_lookup("/vol/empty"));
        else if(!strcmp(operation,"truncate"))equal("/vol/keep.txt",NULL,0);
        else if(!strcmp(operation,"shrink"))equal("/vol/keep.txt",(const uint8_t *)"new",3);
        else if(!strcmp(operation,"chmod"))assert((vfs_lookup("/vol/keep.txt")->metadata.mode&07777)==0644);
        else if(!strcmp(operation,"utime"))assert(vfs_lookup("/vol/keep.txt")->metadata.mtime==12);
        else assert(false);
        if(strcmp(operation,"delete") && strcmp(operation,"chmod"))assert((vfs_lookup("/vol/keep.txt")->metadata.mode&07777)==0600);
        if(!strcmp(operation,"directory") || !strcmp(operation,"chmod") || !strcmp(operation,"utime")) {
            uint8_t data[1500];memset(data,'R',sizeof(data));equal("/vol/keep.txt",data,sizeof(data));
        }
        equal("/vol/anchor.txt",(const uint8_t *)"unchanged",9);
        struct exfat_check_result check;assert(exfat_check("/vol",&check));
    } else if(!strncmp(argv[2],"verify-",7)) {
        const char *path=!strcmp(argv[2],"verify-rename")?"/vol/renamed-with-a-longer-name.txt":"/vol/target.txt";
        equal(path,(const uint8_t *)"original",8);
        assert((vfs_lookup(path)->metadata.mode&07777)==0600);
        assert(!vfs_lookup("/vol/keep.txt"));
        equal("/vol/anchor.txt",(const uint8_t *)"unchanged",9);
        struct exfat_check_result check;assert(exfat_check("/vol",&check));
    } else if(!strcmp(argv[2],"seed-temp")) {
        const uint8_t *data;uint64_t size;
        const struct vfs_node *node=vfs_lookup("/vol/.srvros/meta");
        assert(node && vfs_read_all(node,&data,&size));
        assert(exfat_write_file("/vol/.srvros/meta.tmp",data,size));
        vfs_release_data(node,data);
    }
    if(!strcmp(argv[2],"recover") || !strcmp(argv[2],"seed-temp") || !strncmp(argv[2],"verify-",7)) {
        assert(fsync(fd)==0);close(fd);
        printf("driver: reads=%llu read_bytes=%llu\n",(unsigned long long)read_calls,(unsigned long long)read_bytes);
        printf("driver: write_bytes=%llu\n",(unsigned long long)write_bytes);
        printf("driver: complete writes=%u flushes=%u\n",writes,flushes);return 0;
    }
    if(!strcmp(argv[2],"memory-refusal")) {
        /* Empty-volume growth needs a file block and metadata scratch. Neither
         * failure may dirty VolumeFlags, publish nodes or quarantine the mount. */
        for(unsigned boundary=1;boundary<=2;boundary++) {
            uint64_t nodes=vfs_count();unsigned before=writes,flushed=flushes;
            fail_alloc_at=alloc_calls+boundary;
            assert(!exfat_write_file("/vol/refused",(const uint8_t *)"x",1));
            fail_alloc_at=0;
            assert(writes==before && flushes==flushed && vfs_count()==nodes && !vfs_lookup("/vol/refused"));
            assert(exfat_is_writable("/vol"));
            assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        }
        assert(exfat_write_file("/vol/resumed",(const uint8_t *)"saved",5));
        equal("/vol/resumed",(const uint8_t *)"saved",5);
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        equal("/vol/resumed",(const uint8_t *)"saved",5);
        puts("allocation refusal before writes and successful retry PASS");close(fd);return 0;
    } else if(!strcmp(argv[2],"scale")) {
        assert(vfs_count()>4096);
        const struct vfs_node *anchor=vfs_lookup("/vol/bulk/anchor.txt");assert(anchor);
        const void *private_data=anchor->private_data;const char *path=anchor->path;
        uint64_t inode=anchor->metadata.inode;
        for(unsigned i=0;i<96;i++) {
            char name[64];snprintf(name,sizeof(name),"/vol/bulk/new-%04u",i);
            assert(exfat_write_file(name,(const uint8_t *)"new",3));
        }
        for(unsigned i=0;i<320;i++) {
            char name[64];snprintf(name,sizeof(name),"/vol/dir-%04u",i);
            assert(exfat_create_directory(name));
        }
        assert(vfs_lookup("/vol/bulk/anchor.txt")==anchor && anchor->private_data==private_data && anchor->path==path);
        equal(anchor->path,(const uint8_t *)"anchor",6);
        for(unsigned i=0;i<96;i+=2) {
            char name[64];snprintf(name,sizeof(name),"/vol/bulk/new-%04u",i);
            assert(exfat_delete_file(name));assert(exfat_write_file(name,(const uint8_t *)"reused",6));
        }
        for(unsigned i=0;i<320;i+=2) {
            char name[64];snprintf(name,sizeof(name),"/vol/dir-%04u",i);
            assert(exfat_delete_directory(name));assert(exfat_create_directory(name));
        }
        assert(exfat_rename("/vol/bulk/f0042","/vol/bulk/renamed"));
        assert(exfat_write_file("/vol/bulk/f0043",(const uint8_t *)"edited",6));
        struct exfat_check_result result;assert(exfat_check("/vol",&result));
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        equal("/vol/bulk/anchor.txt",(const uint8_t *)"anchor",6);
        assert(vfs_lookup("/vol/bulk/anchor.txt")->metadata.inode==inode);
        assert(!vfs_lookup("/vol/bulk/f0042") && vfs_lookup("/vol/bulk/renamed"));
        equal("/vol/bulk/f0043",(const uint8_t *)"edited",6);
        for(unsigned i=0;i<96;i++) {
            char name[64];snprintf(name,sizeof(name),"/vol/bulk/new-%04u",i);
            equal(name,(const uint8_t *)(i%2?"new":"reused"),i%2?3:6);
        }
        assert(exfat_check("/vol",&result));
        printf("scale: PASS nodes=%llu files=%llu peak_heap=%llu\n",(unsigned long long)vfs_count(),(unsigned long long)result.files_checked,(unsigned long long)peak_bytes);
        assert(exfat_unmount("/vol"));close(fd);return 0;
    }
    if(!strcmp(argv[2],"scratch-seed")) {
        uint8_t data[4096];for(unsigned i=0;i<4096;i++)data[i]=(uint8_t)i;
        assert(exfat_write_file("/vol/scratch.bin",data,4096));
        assert(exfat_write_file("/vol/anchor.txt",(const uint8_t *)"unchanged",9));
        assert(exfat_write_file("/vol/target.txt",(const uint8_t *)"old-target",10));
        assert(exfat_unmount("/vol"));assert(fsync(fd)==0);close(fd);return 0;
    }
    if(!strcmp(argv[2],"scratch-range")) {
        struct block_device *resolved=NULL;uint64_t offset=UINT64_MAX;
        bool ok=exfat_scratch_range("/vol/scratch.bin",&resolved,&offset);
        assert(!writes);
        assert(ok&&resolved==&d);printf("scratch offset=%llu bytes=4096\n",(unsigned long long)offset);
        close(fd);return 0;
    }
    if(!strcmp(argv[2],"timestamps")) {
        assert(vfs_lookup("/vol")->metadata.mtime==0);
        assert(exfat_write_file("/vol/dated.txt",(const uint8_t *)"date",4));
        uint64_t created=vfs_lookup("/vol/dated.txt")->metadata.mtime;
        assert(created==(uint64_t)mock_realtime/1000000);
        assert(exfat_create_directory("/vol/dated-dir"));
        assert(vfs_lookup("/vol/dated-dir")->metadata.mtime==created);
        mock_realtime+=86400000000LL;
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        assert(vfs_lookup("/vol")->metadata.mtime==0);
        assert(vfs_lookup("/vol/dated.txt")->metadata.mtime==created);
        assert(vfs_lookup("/vol/dated-dir")->metadata.mtime==created);
        assert(fsync(fd)==0);close(fd);puts("unknown imported dates and persisted UTC creation dates across remount PASS");return 0;
    }
    if(!strcmp(argv[2],"capacity")) {
        assert(exfat_write_file("/vol/keep.txt",(const uint8_t *)"original",8));
        uint64_t available=vfs_free_nodes();
        char (*paths)[40]=calloc(available,sizeof(*paths)); assert(paths);
        unsigned count=0;
        while(vfs_free_nodes()>1) {
            snprintf(paths[count],40,"/unrelated-%u",count);
            assert(vfs_register_directory(paths[count])); count++;
        }
        /* One free slot is needed for the sidecar, not a new user name. */
        assert(!exfat_write_file("/vol/refused",(const uint8_t *)"x",1));
        assert(!exfat_create_directory("/vol/refused-dir"));
        assert(!vfs_lookup("/vol/refused") && !vfs_lookup("/vol/refused-dir"));
        assert(exfat_is_writable("/vol")); equal("/vol/keep.txt",(const uint8_t *)"original",8);
        assert(exfat_write_file("/vol/keep.txt",(const uint8_t *)"updated",7));
        snprintf(paths[count],40,"/unrelated-%u",count);
        assert(vfs_register_directory(paths[count])); count++;
        assert(vfs_free_nodes()==0);
        assert(!exfat_write_file("/vol/keep.txt",NULL,0));
        assert(exfat_is_writable("/vol")); equal("/vol/keep.txt",(const uint8_t *)"updated",7);
        assert(vfs_unregister_path(paths[--count]));
        assert(vfs_unregister_path(paths[--count]));
        assert(exfat_write_file("/vol/resumed",(const uint8_t *)"saved",5));
        for(unsigned i=0;i<count;i++) assert(vfs_unregister_path(paths[i]));
        free(paths);
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        equal("/vol/resumed",(const uint8_t *)"saved",5);
        equal("/vol/keep.txt",(const uint8_t *)"updated",7);
        puts("VFS exhaustion refuses before publication; overwrite reserve and recovery PASS");
        /* Failed mounts must not expose a partial tree or leave stale nodes. */
        assert(exfat_unmount("/vol"));
        available=vfs_free_nodes(); paths=calloc(available,sizeof(*paths)); assert(paths); count=0;
        while(vfs_free_nodes()>2) {
            snprintf(paths[count],40,"/unrelated-%u",count);
            assert(vfs_register_directory(paths[count])); count++;
        }
        uint64_t nodes_before=vfs_count(); unsigned writes_before=writes;
        assert(!exfat_mount_block_device("/vol",&d));
        assert(vfs_count()==nodes_before && writes==writes_before && !vfs_lookup("/vol"));
        for(unsigned i=0;i<count;i++) assert(vfs_unregister_path(paths[i]));
        free(paths);
        assert(exfat_mount_block_device("/vol",&d));
        equal("/vol/resumed",(const uint8_t *)"saved",5);
        puts("capacity-limited mount rollback and retry PASS");
    } else if(!strcmp(argv[2],"file-capacity")) {
        char path[160],last[160]; unsigned count=0;
        if(!vfs_lookup("/vol/bulk")) assert(exfat_create_directory("/vol/bulk"));
        for(;;) {
            snprintf(path,sizeof(path),"/vol/bulk/file-%04u-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwxyz.txt",count);
            if(!vfs_lookup(path) && !exfat_write_file(path,NULL,0)) break;
            strcpy(last,path); count++; assert(count<10000);
        }
        assert(count>512 && exfat_is_writable("/vol") && !vfs_lookup(path));
        unsigned before=writes;
        assert(!exfat_creation_capacity("/vol/new",1,0));
        assert(exfat_creation_capacity("/vol/new",0,1));
        assert(!exfat_creation_capacity("/vol/new",UINT64_MAX,0) && writes==before);
        assert(exfat_write_file(last,NULL,0));
        const struct vfs_node *meta=vfs_lookup("/vol/.srvros/meta"); assert(meta && meta->size>65536);
        assert(exfat_delete_file(last));
        assert(exfat_creation_capacity("/vol/new",1,0));
        assert(exfat_write_file(path,(const uint8_t *)"saved",5));
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        equal(path,(const uint8_t *)"saved",5);
        assert(exfat_is_writable("/vol"));
        printf("file table exhaustion and >64KiB sidecar remount PASS files=%u\n",count);
    } else if(!strcmp(argv[2],"dir-capacity")) {
        char path[40],last[40]; unsigned count=0;
        for(;;) {
            snprintf(path,sizeof(path),"/vol/directory-%04u",count);
            if(!exfat_create_directory(path)) break;
            strcpy(last,path); count++; assert(count<10000);
        }
        assert(count>128 && exfat_is_writable("/vol") && !vfs_lookup(path));
        assert(exfat_delete_directory("/vol/directory-0000"));
        assert(exfat_creation_capacity("/vol/reused-hole",0,1));
        assert(exfat_create_directory("/vol/reused-hole"));
        assert(exfat_delete_directory(last)); assert(exfat_create_directory(path));
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        assert(vfs_lookup(path) && exfat_is_writable("/vol"));
        printf("directory table exhaustion and recovery PASS directories=%u\n",count);
    } else if(!strcmp(argv[2],"replace-ops")) {
        assert(exfat_create_directory("/vol/from") && exfat_create_directory("/vol/to"));
        assert(exfat_write_file("/vol/from/source",(const uint8_t *)"source",6));
        assert(exfat_write_file("/vol/to/target",(const uint8_t *)"target",6));
        uint64_t inode=vfs_lookup("/vol/from/source")->metadata.inode;bool published;
        assert(!exfat_rename("/vol/from/source","/vol/to/target"));
        assert(exfat_replace_file("/vol/from/source","/vol/to/target",&published) && published);
        assert(!vfs_lookup("/vol/from/source") && vfs_lookup("/vol/to/target")->metadata.inode==inode);
        equal("/vol/to/target",(const uint8_t *)"source",6);
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        assert(!vfs_lookup("/vol/from/source") && vfs_lookup("/vol/to/target")->metadata.inode==inode);
        equal("/vol/to/target",(const uint8_t *)"source",6);
        puts("replacement rename preserves source inode and contents across remount PASS");
    } else if(!strcmp(argv[2],"creation-mode")) {
        assert(exfat_create_directory_mode("/vol/private",0700));
        assert(exfat_write_file_mode("/vol/private/file",(const uint8_t *)"mode",4,0640));
        assert(exfat_write_file_mode("/vol/zero",NULL,0,0));
        assert((vfs_lookup("/vol/private/file")->metadata.mode&07777)==0640);
        assert(exfat_write_file_mode("/vol/private/file",(const uint8_t *)"kept",4,0777));
        assert((vfs_lookup("/vol/private/file")->metadata.mode&07777)==0640);
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        equal("/vol/private/file",(const uint8_t *)"kept",4);
        assert((vfs_lookup("/vol/private")->metadata.mode&07777)==0700);
        assert((vfs_lookup("/vol/private/file")->metadata.mode&07777)==0640);
        assert((vfs_lookup("/vol/zero")->metadata.mode&07777)==0);
        puts("creation permissions survive remount; overwrite preserves mode PASS");
    } else if(!strcmp(argv[2],"long-path")) {
        char base[SRV_PATH_MAX]="/vol",path[SRV_PATH_MAX+1];
        for(unsigned level=0;level<4;level++) {
            size_t n=strlen(base);base[n++]='/';memset(base+n,'a'+level,200);base[n+200]=0;
            assert(exfat_create_directory(base));
        }
        size_t n=strlen(base);memcpy(path,base,n);path[n++]='/';
        memset(path+n,'z',SRV_PATH_MAX-1-n);path[SRV_PATH_MAX-1]=0;
        assert(exfat_write_file(path,(const uint8_t *)"long",4));
        uint64_t inode=vfs_lookup(path)->metadata.inode;
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        equal(path,(const uint8_t *)"long",4);assert(vfs_lookup(path)->metadata.inode==inode);
        path[SRV_PATH_MAX-1]='q';path[SRV_PATH_MAX]=0;
        assert(!exfat_write_file(path,(const uint8_t *)"bad",3));
        path[SRV_PATH_MAX-1]=0;equal(path,(const uint8_t *)"long",4);
        puts("maximum path survives remount; overlong writes do not alias PASS");
    } else if(!strcmp(argv[2],"cross-directory")) {
        assert(exfat_create_directory("/vol/from") && exfat_create_directory("/vol/to"));
        assert(exfat_write_file("/vol/from/long-source-name",(const uint8_t *)"move",4));
        assert(exfat_create_directory("/vol/from/long-directory-name"));
        assert(exfat_rename("/vol/from/long-source-name","/vol/to/a"));
        assert(exfat_rename("/vol/from/long-directory-name","/vol/to/b"));
        equal("/vol/to/a",(const uint8_t *)"move",4);
        assert(!vfs_lookup("/vol/from/long-source-name") && vfs_lookup("/vol/to/b"));
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        equal("/vol/to/a",(const uint8_t *)"move",4);
        assert(vfs_lookup("/vol/to/b") && !vfs_lookup("/vol/from/a") && !vfs_lookup("/vol/from/b"));
        assert(exfat_delete_directory("/vol/from"));
        puts("cross-directory short file/directory rename survives remount PASS");
    } else if(!strcmp(argv[2],"ops")) {
        uint8_t data[270000]; for(unsigned i=0;i<sizeof(data);i++) data[i]=(uint8_t)(i*17+3);
        assert(exfat_create_directory("/vol/Directory"));
        assert(exfat_write_file("/vol/Directory/MixedCase.txt",data,17));
        equal("/vol/Directory/MixedCase.txt",data,17);
        assert(exfat_write_file("/vol/Directory/MixedCase.txt",data,sizeof(data)));
        assert(exfat_write_file("/vol/Directory/MixedCase.txt",data,9));
        equal("/vol/Directory/MixedCase.txt",data,9);
        assert(exfat_write_file("/vol/empty",NULL,0));
        assert(exfat_write_file("/vol/truncated",data,100));
        assert(exfat_write_file("/vol/truncated",NULL,0));
        assert(exfat_rename("/vol/Directory/MixedCase.txt","/vol/A-much-longer-renamed-file.txt"));
        equal("/vol/A-much-longer-renamed-file.txt",data,9);
        assert(exfat_delete_directory("/vol/Directory"));
        assert(exfat_create_directory("/vol/Before"));
        assert(exfat_rename("/vol/Before","/vol/After"));
        assert(exfat_delete_directory("/vol/After"));
        assert(exfat_write_file("/vol/deleted",data,20000));
        assert(exfat_delete_file("/vol/deleted"));
        assert(!exfat_write_file("/vol/EMPTY",data,1));
        assert(!exfat_write_file("/vol/bad:name",data,1));
        assert(exfat_create_directory("/vol/Many"));
        for(unsigned i=0;i<70;i++) {
            char path[80]; snprintf(path,sizeof(path),"/vol/Many/file-%03u-with-a-long-name.txt",i);
            assert(exfat_write_file(path,data,17));
        }
        assert(exfat_unmount("/vol")); assert(exfat_mount_block_device("/vol",&d));
        equal("/vol/Many/file-069-with-a-long-name.txt",data,17);
        for(unsigned i=0;i<70;i++) {
            char path[80]; snprintf(path,sizeof(path),"/vol/root-%03u-with-a-long-name.txt",i);
            assert(exfat_write_file(path,data,17));
        }
        assert(exfat_unmount("/vol")); assert(exfat_mount_block_device("/vol",&d));
        equal("/vol/root-069-with-a-long-name.txt",data,17);
    } else if(!strcmp(argv[2],"fragmented")) {
        struct exfat_fsinfo info; assert(exfat_statfs("/vol",&info));
        uint8_t *data=malloc(info.block_size*24); assert(data); memset(data,0x5a,info.block_size*24);
        for(unsigned i=0;i<32;i++) { char path[40]; snprintf(path,sizeof(path),"/vol/gap-%02u",i); assert(exfat_write_file(path,data,info.block_size)); }
        assert(exfat_statfs("/vol",&info)); assert(info.blocks_free>64);
        uint64_t size=(info.blocks_free-16)*info.block_size;
        uint8_t *filler=calloc(1,size); assert(filler);
        assert(exfat_write_file("/vol/filler",filler,size)); free(filler);
        for(unsigned i=0;i<32;i+=2) { char path[40]; snprintf(path,sizeof(path),"/vol/gap-%02u",i); assert(exfat_delete_file(path)); }
        assert(exfat_write_file("/vol/fragmented",data,info.block_size*24));
        equal("/vol/fragmented",data,info.block_size*24); free(data);
    } else if(!strcmp(argv[2],"seed")) {
        assert(exfat_write_file("/vol/keep.txt",(const uint8_t *)"original",8));
        assert(exfat_write_file("/vol/anchor.txt",(const uint8_t *)"unchanged",9));
        assert(exfat_write_file("/vol/target.txt",(const uint8_t *)"old-target",10));
        assert(vfs_chmod("/vol/keep.txt",0600) && vfs_chmod("/vol/target.txt",0640));
    } else if(!strncmp(argv[2],"retire-",7)) {
        const char *operation=argv[2]+7;bool ok=false;
        if(!strcmp(operation,"delete"))ok=exfat_delete_file("/vol/keep.txt");
        else if(!strcmp(operation,"directory"))ok=exfat_delete_directory("/vol/empty");
        else if(!strcmp(operation,"truncate"))ok=exfat_write_file("/vol/keep.txt",NULL,0);
        else if(!strcmp(operation,"shrink"))ok=exfat_write_file("/vol/keep.txt",(const uint8_t *)"new",3);
        else if(!strcmp(operation,"chmod"))ok=vfs_chmod("/vol/keep.txt",0644);
        else if(!strcmp(operation,"utime"))ok=vfs_utime("/vol/keep.txt",11,12);
        else assert(false);
        printf("operation=%s writes=%u flushes=%u\n",ok?"PASS":"FAIL",writes,flushes);
        if(failed) { unsigned before=writes;assert(!exfat_write_file("/vol/after-failure",(const uint8_t *)"x",1));assert(before==writes); }
    } else if(!strcmp(argv[2],"grow-root") || !strcmp(argv[2],"grow-dir") || !strcmp(argv[2],"overwrite") || !strcmp(argv[2],"create") || !strcmp(argv[2],"delete") || !strcmp(argv[2],"rename") || !strcmp(argv[2],"replace")) {
        bool ok;
        if(!strcmp(argv[2],"grow-root") || !strcmp(argv[2],"grow-dir"))
            ok=exfat_write_file(!strcmp(argv[2],"grow-root")?"/vol/new.txt":"/vol/grow/new.txt",(const uint8_t *)"new-file-data",13);
        else if(!strcmp(argv[2],"create")) ok=exfat_write_file("/vol/new.txt",(const uint8_t *)"new-file-data",13);
        else if(!strcmp(argv[2],"delete")) ok=exfat_delete_file("/vol/keep.txt");
        else if(!strcmp(argv[2],"replace")) { bool published=false;ok=exfat_replace_file("/vol/keep.txt","/vol/target.txt",&published);
            assert(published==(!vfs_lookup("/vol/keep.txt")));
        }
        else if(!strcmp(argv[2],"rename")) ok=exfat_rename("/vol/keep.txt","/vol/renamed-with-a-longer-name.txt");
        else ok=exfat_write_file("/vol/keep.txt",(const uint8_t *)"replacement-data",16);
        printf("operation=%s writes=%u flushes=%u\n",ok?"PASS":"FAIL",writes,flushes);
        if(failed) { unsigned before=writes; assert(!exfat_write_file("/vol/after-failure",(const uint8_t *)"x",1)); assert(before==writes); }
    } else if(!strcmp(argv[2],"readonly") || !strcmp(argv[2],"noflush") || !strcmp(argv[2],"foreign")) {
        unsigned before=writes;
        assert(!exfat_write_file("/vol/refused",(const uint8_t *)"x",1));
        assert(before==writes);
    } else { return 2; }
    assert(fsync(fd)==0); close(fd);
    printf("driver: reads=%llu read_bytes=%llu\n",(unsigned long long)read_calls,(unsigned long long)read_bytes);
    printf("driver: write_bytes=%llu\n",(unsigned long long)write_bytes);
    printf("driver: complete writes=%u flushes=%u\n",writes,flushes);
    return 0;
}

#define SRVROS_BLOCK_CLAIM_HOST_TEST
#include "../kernel/src/block_claim.c"

bool process_has_open_vfs_prefix(const char *p) { (void)p;return false; }
