/* Real process content snapshots + VFS/exFAT. The host callback models an
 * already audited CPU0 private process turn; actual scheduler admission and
 * IDT/cancellation are exercised by their separate production-code fixtures. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/file_verify_kernel.h>
#include "test_reserved_append_io.h"
static uint64_t reserved_process_flags;
static uint64_t process_test_irq_save(void) {uint64_t f=reserved_process_flags;reserved_process_flags=0;return f;}
static void process_test_irq_restore(uint64_t f) {reserved_process_flags=f;}
static uint64_t process_test_rflags(void) {return reserved_process_flags;}
#define SRVROS_PROCESS_IRQ_HOST_TEST
#define SRVROS_PROCESS_HOST_TEST
#include "../kernel/src/process.c"
static struct process reserved_writer,reserved_foreign;
static struct process *reserved_current;
void *scheduler_current_user_context(void) {return reserved_current;}
void *scheduler_current_user_thread_context(void) {return NULL;}
void net_process_wake(struct process *p) {(void)p;}
void keyboard_wake_waiters(void) {assert(!"unexpected signal branch");}
void scheduler_wake_all(struct scheduler_wait_queue *q) {(void)q;}
void pty_wake(void) {assert(!"unexpected signal branch");}
int64_t pty_read(uint64_t h,bool m,uint8_t *b,uint64_t n,bool nonblock) {(void)h;(void)m;(void)b;(void)n;(void)nonblock;assert(0);return SRV_ERR_IO;}
int64_t pty_write(uint64_t h,bool m,const uint8_t *b,uint64_t n,bool nonblock) {(void)h;(void)m;(void)b;(void)n;(void)nonblock;assert(0);return SRV_ERR_IO;}
int64_t net_close(uint64_t h) {(void)h;assert(0);return SRV_ERR_IO;}
void pty_close(uint64_t h,bool m) {(void)h;(void)m;assert(0);}
int64_t pty_get_flags(uint64_t h,bool m) {(void)h;(void)m;assert(0);return SRV_ERR_IO;}
#define test_sync_content_case reserved_append_unused_sync_case
#include "test_sync_content_bridge.inc"
#undef test_sync_content_case
uint64_t reserved_append_allocations(void);
unsigned reserved_append_owner_depth(void);
static bool reserved_enabled,reserved_barrier,reserved_queued,reserved_turn,reserved_sequential,reserved_failure_mode;
static unsigned reserved_chunks,reserved_fast,reserved_ordinary,reserved_misses,reserved_foreign_syncs;
static uint64_t reserved_bytes,reserved_snapshot_calls,reserved_snapshot_bytes,reserved_prefix_bytes,reserved_actual_prefix_bytes;
static struct reserved_append_io reserved_source_io;
void reserved_append_read_observed(uint64_t bytes,bool ok) {
    if(reserved_barrier && sync_fixture_content_active && sync_fixture_content_role==3 && ok)reserved_actual_prefix_bytes+=bytes;
}
static uint8_t *reserved_expected,*reserved_shadow;
static struct process_file_content *reserved_image;
static uint64_t reserved_fd;
void reserved_append_note_target_io(bool writing,uint64_t absolute,uint64_t offset,uint64_t bytes) {
    if(reserved_barrier && reserved_image && reserved_image->writeback.active)
        printf("reserved-target-io: barrier=%u write=%u absolute=%llu role=%u offset=%llu bytes=%llu\n",
            reserved_foreign_syncs,writing,(unsigned long long)absolute,sync_fixture_content_role,
            (unsigned long long)offset,(unsigned long long)bytes);
}
static uint64_t next_chunk(void) {uint64_t left=1366u*768u*4u+54u-reserved_bytes;return left>65536?65536:left;}
bool scheduler_io_entry_process_independent(void *p) {
    return !reserved_process_flags && p==&reserved_writer && reserved_current==p;
}
bool scheduler_io_process_independent(void *p) {
    return reserved_enabled && !reserved_process_flags && reserved_turn && reserved_barrier &&
        reserved_append_owner_depth() && p==&reserved_writer && reserved_current==p;
}
static bool reserved_expected_copy(void *context,void *destination,uint64_t count,uint64_t requested) {
    uint64_t offset=(uintptr_t)context;
    assert(!reserved_process_flags && count==requested && requested==next_chunk());
    assert(offset==reserved_bytes && offset+requested<=1366u*768u*4u+54u);
    memcpy(destination,reserved_expected+offset,(size_t)count);return true;
}
void reserved_append_backend_step(void) {
    if(!reserved_barrier || !reserved_image || !reserved_image->writeback.active || reserved_queued ||
       reserved_bytes==1366u*768u*4u+54u || reserved_turn)return;
    assert(reserved_current==&reserved_foreign && reserved_append_owner_depth());
    if(reserved_sequential) {reserved_queued=true;return;}
    uint64_t prefix=reserved_image->writeback.size,capacity=reserved_image->capacity;
    const uint8_t *data=reserved_image->data;
    memcpy(reserved_shadow,data,(size_t)prefix);
    reserved_turn=true;reserved_current=&reserved_writer;
    for(unsigned n=0;n<5 && reserved_bytes<1366u*768u*4u+54u;n++) {
        uint64_t count=next_chunk(),allocation=reserved_append_allocations();unsigned depth=reserved_append_owner_depth();
        struct process_file_content before=*reserved_image;
        struct process_write_file description=write_files[reserved_writer.files[reserved_fd].handle-1];
        int64_t result=12345;
        bool handled=process_file_try_reserved_append(&reserved_writer,reserved_fd,count,reserved_expected_copy,(void *)(uintptr_t)reserved_bytes,&result);
        assert(allocation==reserved_append_allocations() && depth==reserved_append_owner_depth());
        assert(data==reserved_image->data && capacity==reserved_image->capacity && !memcmp(reserved_shadow,data,(size_t)prefix));
        if(!handled) {
            assert(result==12345 && !memcmp(&before,reserved_image,sizeof(before)) &&
                !memcmp(&description,&write_files[reserved_writer.files[reserved_fd].handle-1],sizeof(description)));
            reserved_queued=true;reserved_misses++;break;
        }
        assert(result==(int64_t)count);reserved_fast++;reserved_chunks++;reserved_bytes+=count;
    }
    reserved_current=&reserved_foreign;reserved_turn=false;
}
static void ordinary_chunk(void) {
    assert(reserved_bytes<1366u*768u*4u+54u && !reserved_image->writeback.active);
    reserved_current=&reserved_writer;uint64_t count=next_chunk();
    assert(process_file_write(&reserved_writer,reserved_fd,reserved_expected+reserved_bytes,count)==(int64_t)count);
    assert(!reserved_enabled || write_files[reserved_writer.files[reserved_fd].handle-1].append.valid);
    reserved_bytes+=count;reserved_chunks++;reserved_ordinary++;
}
static int64_t reserved_sync(struct block_device *device,unsigned dirty,uint64_t bytes) {
    assert(sync_timing_begin(7));int64_t result=process_file_sync_all();
    assert(current.content.calls==dirty && current.content.bytes==bytes);
    assert(result==0?!current.content.failures:reserved_failure_mode && current.content.failures);
    /* New exclusive content deliberately has no retained node pointer; inode0
     * is diagnostic, never authority. Identity/slot and captured prefix remain
     * fully bound by the production begin/end snapshot records. */
    assert(current.content.worst.identity && current.content.worst.prefix.valid &&
        current.content.worst.start_us>=current.start_us && current.content.worst.end_us>=current.content.worst.start_us);
    const struct sync_content_call *worst=&current.content.worst;
    assert(worst->prefix.end_generation-worst->prefix.start_generation==worst->prefix.append_calls &&
        worst->prefix.live_size==worst->size+worst->prefix.append_bytes);
    assert(result==0?worst->prefix.committed_size==worst->size:worst->prefix.committed_size<=worst->size);
    sync_timing_files_done(result==0);bool flushed=device->flush(device);sync_timing_end(flushed);
    return result==0 && flushed?0:SRV_ERR_IO;
}
static bool verify_expected(void *context,void *destination,uint64_t offset,size_t length) {
    const uint8_t *expected=context;assert(offset+length<=1366u*768u*4u+54u);
    memcpy(destination,expected+offset,length);return true;
}
static void fresh_verify(const char *path) {
    const struct vfs_node *node=vfs_lookup(path);assert(node);
    struct srv_file_verify_request request={.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(request),
        .path=path,.data=reserved_expected,.length=1366u*768u*4u+54u,
        .expected_inode=node->metadata.inode,.expected_mtime=node->metadata.mtime};
    assert(file_verify(path,&request,verify_expected,reserved_expected)==0);
}
static void reserved_failed_writeback(uint64_t captured,uint64_t previous) {
    assert(reserved_failure_mode && reserved_foreign_syncs==2 && reserved_process_flags==0x200 &&
        !reserved_image->writeback.active && reserved_image->failed && reserved_image->dirty &&
        reserved_image->committed_size==previous && reserved_image->size==reserved_bytes && reserved_bytes>captured);
    assert(reserved_image->dirty_generation-reserved_image->writeback.generation==reserved_image->writeback.append_calls &&
        reserved_image->writeback.append_bytes==reserved_bytes-captured && !memcmp(reserved_image->data,reserved_expected,(size_t)reserved_bytes));
    reserved_current=&reserved_writer;int64_t result=12345;
    assert(!process_file_try_reserved_append(&reserved_writer,reserved_fd,next_chunk(),reserved_expected_copy,(void *)(uintptr_t)reserved_bytes,&result) && result==12345);
    const struct vfs_node *node=vfs_lookup("/vol/work/capture.tmp");const uint8_t *media;uint64_t media_size;
    assert(node && vfs_read_all(node,&media,&media_size) && previous<=media_size && media_size<=captured &&
        !memcmp(media,reserved_expected,(size_t)media_size));
    int64_t reader=process_file_alloc(&reserved_foreign,node,media,media_size);assert(reader>=0 && reserved_image->refs==2);
    assert(process_file_close(&reserved_writer,reserved_fd)==SRV_ERR_IO && reserved_image->refs==1 && reserved_image->dirty && reserved_image->failed);
    reserved_current=&reserved_foreign;uint8_t buffer[16384];uint64_t read=0;
    while(read<reserved_bytes) {
        int64_t n=process_file_read(&reserved_foreign,(uint64_t)reader,buffer,sizeof(buffer));
        assert(n>0 && (uint64_t)n<=reserved_bytes-read && !memcmp(buffer,reserved_expected+read,(size_t)n));read+=(uint64_t)n;
    }
    assert(process_file_read(&reserved_foreign,(uint64_t)reader,buffer,1)==0);
    uint64_t live=reserved_bytes;reserved_image=NULL;
    assert(process_file_close(&reserved_foreign,(uint64_t)reader)==SRV_ERR_IO);
    assert(process_file_close(&reserved_foreign,3)==SRV_ERR_IO && !exfat_is_writable("/vol"));
    struct exfat_check_result check={0};bool allocation=exfat_check("/vol",&check);
    printf("reserved-failure: foreign_syncs=%u captured=%llu prior_committed=%llu live=%llu media=%llu retained_read=%llu fast=%u ordinary=%u close_error=1 quarantined=1 allocation_ok=%u leaks=%llu errors=%llu\n",
        reserved_foreign_syncs,(unsigned long long)captured,(unsigned long long)previous,(unsigned long long)live,
        (unsigned long long)media_size,(unsigned long long)read,reserved_fast,reserved_ordinary,allocation,
        (unsigned long long)check.leaked_clusters,(unsigned long long)check.errors);
    free(reserved_expected);free(reserved_shadow);
}
void reserved_append_case(const char *mode,struct block_device *device) {
    reserved_failure_mode=!strcmp(mode,"failure");
    reserved_sequential=!strcmp(mode,"sequential");
    reserved_enabled=reserved_failure_mode || reserved_sequential || !strcmp(mode,"overlap");
    assert(reserved_enabled || !strcmp(mode,"baseline"));
    enum { BYTES=1366*768*4+54 };
    reserved_expected=malloc(BYTES);reserved_shadow=malloc(BYTES);assert(reserved_expected && reserved_shadow);
    for(unsigned i=0;i<BYTES;i++)reserved_expected[i]=(uint8_t)(i*37+11);
    reserved_writer=(struct process){.allocated=true,.active=true,.pid=11,.image_generation=17};
    reserved_foreign=(struct process){.allocated=true,.active=true,.pid=7,.image_generation=18};
    reserved_writer.fsize_limit.current=reserved_foreign.fsize_limit.current=UINT64_MAX;
    for(unsigned i=0;i<3;i++)reserved_writer.files[i]=(struct process_file){.used=true,.type=PROCESS_FILE_STDIO,.handle=i};
    reserved_current=&reserved_writer;
    int64_t opened=process_file_open_write_raw(&reserved_writer,"/vol/work/capture.tmp",SRV_OPEN_WRITE|SRV_OPEN_CREATE|SRV_OPEN_EXCL);
    assert(opened==3);reserved_fd=(uint64_t)opened;
    reserved_image=write_files[reserved_writer.files[reserved_fd].handle-1].content;
    assert(!reserved_image->node && !reserved_image->size);
    if(reserved_enabled)assert(process_file_reserve_append_ram(&reserved_writer,reserved_fd,BYTES)==0);
    else assert(!write_files[reserved_writer.files[reserved_fd].handle-1].append.valid);
    struct process_file_content *other=sync_fixture_content("/vol/work/other.txt");
    uint64_t other_handle=write_file_alloc();assert(other_handle);
    write_files[other_handle-1]=(struct process_write_file){.used=true,.refs=1,.open_flags=SRV_OPEN_WRITE,.content=other};
    reserved_foreign.files[3]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS_WRITE,.handle=other_handle};
    assert(sync_timing_enable());ordinary_chunk();
    for(unsigned round=1;round<=65;round++) {
        reserved_current=&reserved_foreign;
        unsigned char marker[17];memset(marker,(int)round,sizeof(marker));write_files[other_handle-1].offset=0;
        assert(process_file_write(&reserved_foreign,3,marker,sizeof(marker))==(int64_t)sizeof(marker));
        uint64_t entry=reserved_image->size;bool dirty=reserved_image->dirty;
        uint64_t previous=reserved_image->committed_size;
        struct reserved_append_io before,after;reserved_append_io_snapshot(&before);
        reserved_process_flags=0x200;reserved_barrier=true;reserved_foreign_syncs++;
        int64_t sync_result=reserved_sync(device,1+(dirty?1:0),sizeof(marker)+(dirty?entry:0));
        assert(reserved_process_flags==0x200);reserved_barrier=false;reserved_append_io_snapshot(&after);
        reserved_source_io.callbacks+=after.callbacks-before.callbacks;
        reserved_source_io.reads+=after.reads-before.reads;reserved_source_io.read_bytes+=after.read_bytes-before.read_bytes;
        reserved_source_io.writes+=after.writes-before.writes;reserved_source_io.write_bytes+=after.write_bytes-before.write_bytes;
        reserved_source_io.flushes+=after.flushes-before.flushes;
        assert(!reserved_image->writeback.active);
        if(sync_result<0) {reserved_failed_writeback(entry,previous);return;}
        if(dirty) {
            reserved_snapshot_calls++;reserved_snapshot_bytes+=entry;reserved_prefix_bytes+=previous;
            assert(reserved_image->committed_size==entry && reserved_image->dirty==(reserved_image->size>entry));
            assert(reserved_image->writeback.size==entry &&
                reserved_image->dirty_generation-reserved_image->writeback.generation==reserved_image->writeback.append_calls);
        }
        sync_fixture_media("/vol/work/capture.tmp",reserved_expected,reserved_image->committed_size);
        sync_fixture_media("/vol/work/other.txt",marker,sizeof(marker));
        uint64_t start_generation=dirty?reserved_image->writeback.generation:0;
        uint64_t end_generation=dirty?reserved_image->dirty_generation:0;
        uint64_t live_at_end=reserved_image->size,append_calls=dirty?reserved_image->writeback.append_calls:0;
        uint64_t append_bytes=dirty?reserved_image->writeback.append_bytes:0;
        bool dirty_at_end=reserved_image->dirty;
        assert(!dirty || live_at_end==entry+append_bytes);
        if(reserved_queued) {ordinary_chunk();reserved_queued=false;}
        printf("reserved-barrier: round=%u captured=%llu live=%llu committed=%llu dirty=%u prefix_valid=%u start_generation=%llu end_generation=%llu live_at_end=%llu append_calls=%llu append_bytes=%llu dirty_at_end=%u\n",round,
            (unsigned long long)(dirty?entry:0),(unsigned long long)reserved_image->size,
            (unsigned long long)reserved_image->committed_size,reserved_image->dirty,dirty,
            (unsigned long long)start_generation,(unsigned long long)end_generation,(unsigned long long)live_at_end,
            (unsigned long long)append_calls,(unsigned long long)append_bytes,dirty_at_end);
    }
    assert(!reserved_failure_mode);
    assert(reserved_chunks==65 && reserved_bytes==BYTES && !reserved_image->dirty && reserved_image->committed_size==BYTES);
    assert(reserved_foreign_syncs==65 && reserved_ordinary+reserved_fast==65);
    assert(reserved_actual_prefix_bytes==reserved_prefix_bytes);
    if(reserved_enabled && !reserved_sequential)assert(reserved_fast==52 && reserved_ordinary==13 && reserved_snapshot_calls==14);
    else assert(!reserved_fast && reserved_ordinary==65 && reserved_snapshot_calls==65);
    reserved_current=&reserved_writer;
    assert(process_file_flush(&reserved_writer,reserved_fd)==0);
    reserved_image=NULL;
    assert(process_file_close(&reserved_writer,reserved_fd)==0);
    assert(process_file_sync_all()==0 && device->flush(device));fresh_verify("/vol/work/capture.tmp");
    const struct vfs_node *node=vfs_lookup("/vol/work/capture.tmp");assert(node && node->size==BYTES);
    /* Exact checked-rename guards; unlike permissive POSIX replacement, the
     * screenshot operation refuses an existing destination or an open prefix. */
    assert(!vfs_lookup("/vol/work/capture.bmp") && !vfs_namespace_system_protects("/vol/work/capture.tmp") &&
        !vfs_namespace_system_protects("/vol/work/capture.bmp") &&
        !process_has_open_vfs_prefix("/vol/work/capture.tmp") && !process_has_open_vfs_prefix("/vol/work/capture.bmp"));
    assert(exfat_rename("/vol/work/capture.tmp","/vol/work/capture.bmp"));
    assert(process_file_sync_all()==0 && device->flush(device));fresh_verify("/vol/work/capture.bmp");
    assert(process_file_close(&reserved_foreign,3)==0);
    sync_fixture_media("/vol/work/anchor.txt",(const uint8_t *)"unchanged",9);
    struct exfat_check_result check={0};assert(exfat_check("/vol",&check));
    assert(!check.errors && !check.leaked_clusters && !check.fat_bitmap_mismatches && exfat_unmount("/vol"));
    printf("reserved-result: mode=%s writes=%u bytes=%u foreign_syncs=%u fast=%u ordinary=%u misses=%u snapshots=%llu snapshot_bytes=%llu old_prefix_bytes=%llu fresh_verifications=2 allocation_clean=1\n",
        mode,reserved_chunks,BYTES,reserved_foreign_syncs,reserved_fast,reserved_ordinary,reserved_misses,
        (unsigned long long)reserved_snapshot_calls,(unsigned long long)reserved_snapshot_bytes,(unsigned long long)reserved_prefix_bytes);
    printf("reserved-source-io: callbacks=%llu reads=%llu read_bytes=%llu writes=%llu write_bytes=%llu flushes=%llu prefix_read_bytes=%llu\n",
        (unsigned long long)reserved_source_io.callbacks,(unsigned long long)reserved_source_io.reads,
        (unsigned long long)reserved_source_io.read_bytes,(unsigned long long)reserved_source_io.writes,
        (unsigned long long)reserved_source_io.write_bytes,(unsigned long long)reserved_source_io.flushes,
        (unsigned long long)reserved_actual_prefix_bytes);
    free(reserved_expected);free(reserved_shadow);
}
