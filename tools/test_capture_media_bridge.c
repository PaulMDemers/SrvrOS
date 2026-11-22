#define _POSIX_C_SOURCE 200809L
/* The complete screenshot save wrapper runs against real process contents,
 * exFAT and VFS. Only namespace/syscall admission, user copy and unrelated
 * process branches are host adapters; no native scheduler/controller is modeled. */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <srvros/file_verify_kernel.h>
#include "../userspace/shotworker/save.h"
#define test_sync_content_case unused_test_sync_content_case
#include "test_process_writeback.c"
#undef test_sync_content_case

static struct process *capture_subject;
static struct block_device *capture_device;
static const unsigned char *capture_expected;
static size_t capture_length;
static const char *capture_case;
static int held_temp=-1,held_final=-1;
static unsigned rename_attempts,rename_successes,verify_calls,verify_successes;
static uint64_t verify_compared;
static int64_t posix_result(int64_t result) {
    if(result<0) {errno=(int)-result;return -1;}return result;
}
/* Exact read-only-open core used by process_file_open_flags: the fresh VFS
 * bytes deliberately flow through ordinary shared-content acquisition. */
static int capture_read_open(const char *path) {
    const struct vfs_node *node=vfs_lookup(path);const uint8_t *bytes;uint64_t length;
    if(!node) {errno=ENOENT;return -1;}
    if(node->type!=VFS_NODE_FILE || !vfs_read_all(node,&bytes,&length)) {errno=EIO;return -1;}
    return (int)posix_result(process_file_alloc(capture_subject,node,bytes,length));
}
static int capture_open(const char *path,int flags,...) {
    if((flags&O_ACCMODE)==O_RDONLY)return capture_read_open(path);
    assert((flags&O_ACCMODE)==O_WRONLY && (flags&O_CREAT) && (flags&O_EXCL));
    uint64_t mode=SRV_OPEN_WRITE|SRV_OPEN_CREATE|SRV_OPEN_EXCL|SRV_OPEN_PERMISSIONS|
        ((uint64_t)0600<<SRV_OPEN_MODE_SHIFT);
    return (int)posix_result(process_file_open_write_raw(capture_subject,path,mode));
}
static int capture_close(int fd) {
    struct process_file *file=process_file_at(capture_subject,(uint64_t)fd);assert(file);
    int64_t result;
    if(file->type==PROCESS_FILE_VFS_WRITE)result=write_file_close(file->handle);
    else {assert(file->type==PROCESS_FILE_VFS);result=read_file_close(file->handle);}
    *file=(struct process_file){0};return (int)posix_result(result);
}
static ssize_t capture_write(int fd,const void *bytes,size_t length) {
    return (ssize_t)posix_result(process_file_write(capture_subject,fd,bytes,length));
}
static ssize_t __attribute__((unused)) capture_read(int fd,void *bytes,size_t length) {
    return (ssize_t)posix_result(process_file_read(capture_subject,fd,bytes,length));
}
static int capture_fsync(int fd) {return (int)posix_result(process_file_flush(capture_subject,fd));}
static void capture_stat_value(const struct vfs_node *node,uint64_t length,struct stat *out) {
    memset(out,0,sizeof(*out));out->st_mode=(mode_t)node->metadata.mode;
    out->st_size=(off_t)length;out->st_ino=(ino_t)node->metadata.inode;out->st_mtime=(time_t)node->metadata.mtime;
}
static int capture_stat(const char *path,struct stat *out) {
    const struct vfs_node *node=vfs_lookup(path);
    if(!node){errno=ENOENT;return -1;}capture_stat_value(node,node->size,out);return 0;
}
static int __attribute__((unused)) capture_fstat(int fd,struct stat *out) {
    struct process_file *file=process_file_at(capture_subject,fd);assert(file && file->type==PROCESS_FILE_VFS);
    struct process_read_file *reader=read_file_at(file->handle);assert(reader && reader->node);
    capture_stat_value(reader->node,reader->content->size,out);return 0;
}
static int capture_mkdir(const char *path,mode_t mode) {
    (void)mode;return exfat_create_directory(path)?0:(errno=EIO,-1);
}
static int capture_unlink(const char *path) {return (int)posix_result(process_unlink_file(path));}
long srv_getpid(void) {return 777;}
/* This fixture isolates the original save/media proofs and old-kernel fallback.
 * Actual reserve/overlapping-copy ownership is exercised by its own fixture. */
long srv_file_reserve_append_ram(int fd,uint64_t capacity) {
    (void)fd;assert(capacity>=54 && capacity<=DESK_CAPTURE_MAX_BYTES);return SRV_ERR_NOTSUP;
}
long srv_ticks(void) {return 12345;}
long srv_sync(void) {
    int64_t result=process_file_sync_all();bool flushed=capture_device->flush(capture_device);
    return result<0 || !flushed?SRV_ERR_IO:0;
}
long srv_file_change(const struct srv_file_change *q) {
    rename_attempts++;
    assert(q->abi_version==SRV_ABI_VERSION && q->struct_size==sizeof(*q) && q->operation==SRV_FILE_RENAME);
    /* Preserve the checked syscall's guards, especially its open-prefix refusal.
     * The permissive legacy process_rename_file primitive is NOT substituted. */
    const struct vfs_node *node=vfs_lookup(q->path);
    if(!node || node->metadata.inode!=q->expected_inode || node->size!=q->expected_size ||
       node->metadata.mtime!=q->expected_mtime || (uint64_t)node->type!=q->expected_type)return SRV_FILE_CHANGED;
    if(vfs_lookup(q->destination))return SRV_FILE_EXISTS;
    if(vfs_namespace_system_protects(q->path) || vfs_namespace_system_protects(q->destination) ||
       process_has_open_vfs_prefix(q->path) || process_has_open_vfs_prefix(q->destination))return SRV_FILE_BUSY;
    if(!exfat_rename(q->path,q->destination))return -1;
    rename_successes++;return 0;
}
static bool capture_copy_expected(void *context,void *destination,uint64_t offset,size_t length) {
    const struct srv_file_verify_request *q=context;
    assert(offset<=q->length && length<=q->length-offset && length<=SRV_FILE_VERIFY_CHUNK);
    memcpy(destination,(const unsigned char *)q->data+offset,length);verify_compared+=length;return true;
}
long srv_file_verify(const struct srv_file_verify_request *q) {
    verify_calls++;int64_t result=file_verify(q->path,q,capture_copy_expected,(void *)q);
    if(!result)verify_successes++;
    return result;
}
#define open capture_open
#define close capture_close
#define read capture_read
#define write capture_write
#define fsync capture_fsync
#define stat(...) capture_stat(__VA_ARGS__)
#define fstat(...) capture_fstat(__VA_ARGS__)
#define mkdir capture_mkdir
#define unlink capture_unlink
#ifndef CAPTURE_MEDIA_SAVE_SOURCE
#define CAPTURE_MEDIA_SAVE_SOURCE "../userspace/shotworker/save.c"
#endif
#include CAPTURE_MEDIA_SAVE_SOURCE
#undef open
#undef close
#undef read
#undef write
#undef fsync
#undef stat
#undef fstat
#undef mkdir
#undef unlink

void capture_media_corrupt_payload(const unsigned char *,size_t);
static const char *temp_path="/vol/work/Screenshots/.capture-777-12345.tmp";
static const char *final_path="/vol/work/Screenshots/proof.bmp";
static bool retained(void) {return strstr(capture_case,"retained")!=NULL || strstr(capture_case,"busy")!=NULL;}
static void retained_original(int fd) {
    assert(fd>=0 && process_file_seek(capture_subject,fd,0,0)==0);
    unsigned char buffer[4096];size_t offset=0;
    while(offset<capture_length) {
        size_t bytes=capture_length-offset;if(bytes>sizeof(buffer))bytes=sizeof(buffer);
        assert(process_file_read(capture_subject,fd,buffer,bytes)==(int64_t)bytes);
        assert(!memcmp(buffer,capture_expected+offset,bytes));offset+=bytes;
    }
    assert(process_file_seek(capture_subject,fd,0,0)==0);
}
static void media_is_corrupt(const char *path) {
    const struct vfs_node *node=vfs_lookup(path);const uint8_t *bytes;uint64_t length;
    assert(node && vfs_read_all(node,&bytes,&length) && length==capture_length);
    assert(bytes[0]==(unsigned char)(capture_expected[0]^0x80));
    assert(!memcmp(bytes+1,capture_expected+1,capture_length-1));vfs_release_data(node,bytes);
}
static void capture_progress(void *context,unsigned phase,uint64_t bytes) {
    assert(context==capture_subject);
    if(phase==CAPTURE_FSYNC && retained()) {
        held_temp=capture_read_open(temp_path);assert(held_temp>=0);retained_original(held_temp);
    }
    if(phase==CAPTURE_TEMP_VERIFY && !bytes && strstr(capture_case,"temp-corrupt")) {
        capture_media_corrupt_payload(capture_expected,capture_length);media_is_corrupt(temp_path);
        if(held_temp>=0)retained_original(held_temp);
    }
    if(phase==CAPTURE_RENAME && held_temp>=0 && !strstr(capture_case,"busy")) {
        retained_original(held_temp);assert(capture_close(held_temp)==0);held_temp=-1;
    }
    if(phase==CAPTURE_FINAL_SYNC && retained() && !strstr(capture_case,"temp-corrupt")) {
        held_final=capture_read_open(final_path);assert(held_final>=0);
    }
    if(phase==CAPTURE_FINAL_VERIFY && !bytes && strstr(capture_case,"final-corrupt")) {
        capture_media_corrupt_payload(capture_expected,capture_length);media_is_corrupt(final_path);
        if(held_final>=0)retained_original(held_final);
    }
}
void test_sync_content_case(const char *mode,struct block_device *device) {
    enum { BYTES=200000 };
    capture_case=mode;capture_device=device;
    struct process subject={0};subject.fsize_limit.current=UINT64_MAX;
    strcpy(subject.cwd,"/");strcpy(subject.cwd_storage,"/");capture_subject=&subject;
    unsigned char expected[BYTES];for(unsigned i=0;i<BYTES;i++)expected[i]=(uint8_t)(i*37+11);
    capture_expected=expected;capture_length=BYTES;
    unsigned result=capture_save_progress(final_path,expected,sizeof(expected),capture_progress,&subject);
    bool temp_fault=strstr(mode,"temp-corrupt")!=NULL,final_fault=strstr(mode,"final-corrupt")!=NULL;
    bool busy=strstr(mode,"busy")!=NULL,masked_final=final_fault && retained();
#ifdef CAPTURE_MEDIA_EXPECT_LEGACY
    assert(result==(masked_final || (!temp_fault && !final_fault && !busy)?DESK_CAPTURE_OK:DESK_CAPTURE_IO));
    assert(!verify_calls); /* Actual historical save wrapper uses reopen/read. */
#else
    assert(result==(!temp_fault && !final_fault && !busy?DESK_CAPTURE_OK:DESK_CAPTURE_IO));
    if(temp_fault)assert(!rename_attempts && verify_calls==1 && !verify_successes);
    else if(busy)assert(rename_attempts==1 && !rename_successes && verify_calls==1 && verify_successes==1);
    else assert(rename_successes==1 && verify_calls==2 && verify_successes==(final_fault?1u:2u));
    if(result==DESK_CAPTURE_OK)assert(verify_compared==BYTES*2u);
#endif
    (void)masked_final;
    if(held_temp>=0){retained_original(held_temp);assert(capture_close(held_temp)==0);held_temp=-1;}
    if(held_final>=0){retained_original(held_final);assert(capture_close(held_final)==0);held_final=-1;}
    assert(!vfs_lookup(temp_path));
    if(rename_successes) {
        assert(vfs_lookup(final_path));
        if(temp_fault || final_fault)media_is_corrupt(final_path);
        else sync_fixture_media(final_path,expected,sizeof(expected));
    } else assert(!vfs_lookup(final_path));
    for(unsigned i=0;i<PROCESS_MAX_OPEN_FILES;i++)assert(!subject.files[i].used);
    assert(!content_find(temp_path) && !content_find(final_path));
    sync_fixture_media("/vol/work/anchor.txt",(const uint8_t *)"unchanged",9);
    struct exfat_check_result check={0};assert(exfat_check("/vol",&check));
    assert(!check.leaked_clusters && !check.fat_bitmap_mismatches && !check.errors);
    printf("capture-media: case=%s result=%u rename_attempts=%u rename_successes=%u fresh_calls=%u fresh_successes=%u compared=%llu ordinary_shared_reader_preserved=1 allocation_clean=1\n",
        mode,result,rename_attempts,rename_successes,verify_calls,verify_successes,(unsigned long long)verify_compared);
    assert(exfat_unmount("/vol"));
}
