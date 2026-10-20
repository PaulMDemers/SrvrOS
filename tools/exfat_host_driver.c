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
#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif
#include <srvros/block.h>
#include <srvros/exfat.h>
#include <srvros/vfs.h>
#include <srvros/autotest_storage.h>
static int fd;
static unsigned writes, flushes, fail_at, crash_at, fail_flush_at;
static bool failed, foreign;
static unsigned io_depth;
bool scheduler_io_begin(void) { if(foreign) return false; io_depth++; return true; }
void scheduler_io_end(void) { assert(io_depth); io_depth--; }
bool scheduler_io_active(void) { return foreign || io_depth != 0; }
void *kmalloc(uint64_t n) { return malloc(n); }
void *kcalloc(uint64_t n, uint64_t size) { return calloc(n,size); }
void kfree(void *p) { free(p); }
uint64_t timer_ticks(void) { return 100; }
static int64_t mock_realtime=1792326896000000LL;
int64_t timer_realtime_us(void) { return mock_realtime; }
void console_write(const char *s) { (void)s; }
void console_printf(const char *fmt, ...) { (void)fmt; }
uint64_t block_device_size(const struct block_device *d) { return d->block_size*d->block_count; }
struct block_device *block_parent(struct block_device *d) { return d; }
bool block_read(struct block_device *d, uint64_t o, void *p, uint64_t n) {
    return o+n <= block_device_size(d) && pread(fd,p,n,o)==(ssize_t)n;
}
bool block_write(struct block_device *d, uint64_t o, const void *p, uint64_t n) {
    if (failed || ++writes == fail_at) { failed=true; return false; }
    assert(o+n <= block_device_size(d));
    if(pwrite(fd,p,n,o)!=(ssize_t)n) return false;
    if(writes==crash_at) { fsync(fd); _exit(77); }
    return true;
}
static bool flush(struct block_device *d) { (void)d; flushes++; if(flushes==fail_flush_at) failed=true; return !failed && fsync(fd)==0; }
static void equal(const char *path, const uint8_t *expected, uint64_t n) {
    const struct vfs_node *node=vfs_lookup(path); const uint8_t *data; uint64_t size;
    assert(node && vfs_read_all(node,&data,&size));
    assert(size==n && (!n || !memcmp(data,expected,n))); vfs_release_data(node,data);
}
int main(int argc, char **argv) {
    assert(argc>=3);
    bool check_only=!strcmp(argv[2],"allocation-check");
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
    struct block_device d={.name="fixture",.block_size=512,.block_count=bytes/512,
        .read=block_read,.write=check_only ? NULL : block_write,.flush=check_only ? NULL : flush};
    if(check_only) {
        if(!exfat_mount_block_device("/vol",&d)) {
            fputs("allocation-check: unsupported or invalid filesystem; NOT CHECKED\n",stderr);
            close(fd);return 2;
        }
        struct exfat_check_result result={0};
        bool ok=exfat_check("/vol",&result);
        assert(!writes && !flushes);
        printf("allocation-check: %s files=%llu clusters=%llu leaked=%llu fat_bitmap_mismatches=%llu errors=%llu\n",
            ok ? "PASS" : "FAIL",(unsigned long long)result.files_checked,
            (unsigned long long)result.clusters_checked,(unsigned long long)result.leaked_clusters,
            (unsigned long long)result.fat_bitmap_mismatches,(unsigned long long)result.errors);
        close(fd);return ok ? 0 : 1;
    }
    if(!strcmp(argv[2],"noflush")) d.flush=NULL;
    if(!strcmp(argv[2],"foreign")) foreign=true;
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
    assert(exfat_mount_block_device("/vol",&d));
    if(!strcmp(argv[2],"scratch-seed")) {
        uint8_t data[4096];for(unsigned i=0;i<4096;i++)data[i]=(uint8_t)i;
        assert(exfat_write_file("/vol/scratch.bin",data,4096));
        assert(exfat_write_file("/vol/anchor.txt",(const uint8_t *)"unchanged",9));
        assert(exfat_unmount("/vol"));assert(fsync(fd)==0);close(fd);return 0;
    }
    if(!strcmp(argv[2],"scratch-range")||!strcmp(argv[2],"scratch-refuse")) {
        struct block_device *resolved=NULL;uint64_t offset=UINT64_MAX;
        bool ok=exfat_scratch_range("/vol/scratch.bin",&resolved,&offset);
        assert(!writes);
        if(!strcmp(argv[2],"scratch-refuse"))assert(!ok&&resolved==NULL&&offset==UINT64_MAX);
        else {assert(ok&&resolved==&d);printf("scratch offset=%llu bytes=4096\n",(unsigned long long)offset);}
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
        for(;;) {
            snprintf(path,sizeof(path),"/vol/file-%04u-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwxyz.txt",count);
            if(!exfat_write_file(path,NULL,0)) break;
            strcpy(last,path); count++; assert(count<10000);
        }
        assert(count>512 && exfat_is_writable("/vol") && !vfs_lookup(path));
        const struct vfs_node *meta=vfs_lookup("/vol/.srvros/meta"); assert(meta && meta->size>65536);
        assert(exfat_delete_file(last));
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
        assert(exfat_delete_directory(last)); assert(exfat_create_directory(path));
        assert(exfat_unmount("/vol") && exfat_mount_block_device("/vol",&d));
        assert(vfs_lookup(path) && exfat_is_writable("/vol"));
        printf("directory table exhaustion and recovery PASS directories=%u\n",count);
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
    } else if(!strcmp(argv[2],"overwrite") || !strcmp(argv[2],"create") || !strcmp(argv[2],"delete") || !strcmp(argv[2],"rename")) {
        bool ok;
        if(!strcmp(argv[2],"create")) ok=exfat_write_file("/vol/new.txt",(const uint8_t *)"new-file-data",13);
        else if(!strcmp(argv[2],"delete")) ok=exfat_delete_file("/vol/keep.txt");
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
    printf("driver: complete writes=%u flushes=%u\n",writes,flushes);
    return 0;
}
