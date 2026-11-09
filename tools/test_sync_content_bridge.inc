/* Real RAM content writes and global writeback against the real image-backed
 * exFAT/VFS. Only IRQ, clock, unrelated process branches and USB observer seams
 * are host adapters; there is no USB controller in this fixture. */
#include <stdio.h>
#include <srvros/sync_timing.h>
static uint64_t sync_test_irq_save(void) { return 0; }
static void sync_test_irq_restore(uint64_t f) { (void)f; }
#define SRVROS_SYNC_TIMING_HOST_TEST
#include "../kernel/src/sync_timing.c"
static bool sync_fixture_usb_busy;
bool usb_storage_timing_aux_begin(void) {
    assert(!sync_fixture_usb_busy);sync_fixture_usb_busy=true;return true;
}
void usb_storage_timing_aux_end(struct usb_storage_timing *out) {
    assert(sync_fixture_usb_busy);sync_fixture_usb_busy=false;
    *out=(struct usb_storage_timing){.valid=true};
}
void test_sync_content_arm_failure(void);

static struct process_file_content *sync_fixture_content(const char *path) {
    const struct vfs_node *node=vfs_lookup(path);const uint8_t *data;uint64_t size;
    assert(node && vfs_read_all(node,&data,&size));
    struct process_file_content *content=content_acquire(node,path,data,size);
    assert(content);return content;
}
static void sync_fixture_media(const char *path,const uint8_t *expected,uint64_t bytes) {
    const struct vfs_node *node=vfs_lookup(path);const uint8_t *data;uint64_t size;
    assert(node && vfs_read_all(node,&data,&size));
    assert(size==bytes && (!bytes || !memcmp(data,expected,(size_t)bytes)));
    vfs_release_data(node,data);
}
static int64_t sync_fixture_flush(struct block_device *device,bool enabled,unsigned dirty,uint64_t bytes) {
    bool timed=sync_timing_begin(7);assert(timed==enabled);
    int64_t result=process_file_sync_all();
    if(timed) {
        assert(current.content.calls==dirty && current.content.bytes==bytes);
        assert(current.content.worst.identity && current.content.worst.inode);
        assert(current.content.worst.start_us>=current.start_us);
        assert(current.content.worst.end_us>=current.content.worst.start_us);
        assert(current.content.worst.size>0 && current.content.worst.slot<PROCESS_MAX_WRITE_FILES+PROCESS_MAX_READ_FILES);
        assert((current.content.failures!=0)==(result<0));
        assert((current.content.worst.ok!=0)==(result==0));
        uint64_t io_us=0;
        for(unsigned role=0;role<SYNC_CONTENT_ROLES;role++) {
            const struct sync_content_io *io=&current.content.worst.io[role];
            assert(io->read_us==io->reads*10 && io->write_us==io->writes*20 && io->flush_us==io->flushes*30);
            io_us+=io->read_us+io->write_us+io->flush_us;
        }
        assert(io_us<=current.content.worst.end_us-current.content.worst.start_us);
        if(result==0)assert(io_us>0);
        sync_timing_files_done(result>=0);
    }
    bool flushed=device->flush(device);
    if(timed)sync_timing_end(flushed);
    return result<0 || !flushed?SRV_ERR_IO:0;
}

void test_sync_content_case(const char *mode,struct block_device *device) {
    enum { IMAGE_BYTES=1366*768*4+54, CHUNK=65536 };
    bool enabled=!strcmp(mode,"sync-content-on") || !strcmp(mode,"sync-content-fail-on");
    bool fault=strstr(mode,"-fail-")!=NULL;
    assert(enabled || !strcmp(mode,"sync-content-off") || !strcmp(mode,"sync-content-fail-off"));
    if(enabled)assert(sync_timing_enable());
    struct process subject={0};subject.fsize_limit.current=UINT64_MAX;
    struct process_file_content *image=sync_fixture_content("/vol/work/capture.tmp");
    struct process_file_content *other=sync_fixture_content("/vol/work/other.txt");
    assert(image!=other);
    uint64_t image_identity=image->diagnostic_identity,other_identity=other->diagnostic_identity;
    assert(image_identity && other_identity && image_identity!=other_identity);
    image->refs+=2; /* independent second writer and a retained reader */
    write_files[0]=(struct process_write_file){.used=true,.refs=1,.content=image,.open_flags=SRV_OPEN_WRITE};
    write_files[1]=(struct process_write_file){.used=true,.refs=1,.content=other,.open_flags=SRV_OPEN_WRITE};
    write_files[2]=(struct process_write_file){.used=true,.refs=1,.content=image,.open_flags=SRV_OPEN_WRITE};
    read_files[0]=(struct process_read_file){.used=true,.refs=1,.content=image};
    subject.files[0]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS_WRITE,.handle=1};
    subject.files[1]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS_WRITE,.handle=2};
    unsigned char *expected=kmalloc(IMAGE_BYTES);assert(expected);
    for(unsigned i=0;i<IMAGE_BYTES;i++)expected[i]=(uint8_t)(i*37+11);
    unsigned char marker[17];unsigned rounds=0;uint64_t last_size=0;
    for(uint64_t at=0;at<IMAGE_BYTES;) {
        uint64_t n=IMAGE_BYTES-at;if(n>CHUNK)n=CHUNK;
        assert(process_file_write(&subject,0,expected+at,n)==(int64_t)n);at+=n;rounds++;
        memset(marker,(int)rounds,sizeof(marker));write_files[1].offset=0;
        assert(process_file_write(&subject,1,marker,sizeof(marker))==(int64_t)sizeof(marker));
        assert(image->dirty && other->dirty && write_files[2].content==image);
        if(fault && rounds==33)test_sync_content_arm_failure();
        int64_t result=sync_fixture_flush(device,enabled,2,at+sizeof(marker));
        if(fault && rounds==33) {
            assert(result<0 && image->dirty && image->failed && other->dirty && other->failed);
            assert(image->refs==3 && !memcmp(image->data,expected,(size_t)at));
            assert(write_file_close(1)==SRV_ERR_IO && write_file_close(3)==SRV_ERR_IO);
            assert(image->refs==1 && read_files[0].content==image);
            assert(sync_fixture_flush(device,enabled,2,at+sizeof(marker))<0);
            assert(read_file_close(1)==SRV_ERR_IO && write_file_close(2)==SRV_ERR_IO);
            assert(!content_find("/vol/work/capture.tmp") && !content_find("/vol/work/other.txt"));
            assert(!exfat_is_writable("/vol"));break;
        }
        assert(result==0 && !image->dirty && !other->dirty);
        assert(!image->failed && !other->failed && image->size==at);
        assert(image->diagnostic_identity==image_identity && other->diagnostic_identity==other_identity);
        sync_fixture_media("/vol/work/capture.tmp",expected,at);
        sync_fixture_media("/vol/work/other.txt",marker,sizeof(marker));
        last_size=at;
    }
    if(!fault) {
        assert(rounds==65 && last_size==IMAGE_BYTES);
        /* Close both writers while the reader keeps the same content alive. */
        assert(write_file_close(1)==0 && write_file_close(3)==0 && image->refs==1);
        assert(!image->dirty && read_files[0].content==image);
        assert(process_file_sync_all()==0); /* clean reader is not rewritten */
        assert(read_file_close(1)==0 && write_file_close(2)==0);
        assert(!content_find("/vol/work/capture.tmp") && !content_find("/vol/work/other.txt"));
        /* Slot reuse receives a new identity, even with the same inode/path. */
        struct process_file_content *again=sync_fixture_content("/vol/work/capture.tmp");
        assert(again->diagnostic_identity!=image_identity && again->diagnostic_identity!=other_identity);
        assert(content_release(again)==0);
        sync_fixture_media("/vol/work/anchor.txt",(const uint8_t *)"unchanged",9);
        struct exfat_check_result check={0};assert(exfat_check("/vol",&check));
        assert(!check.leaked_clusters && !check.fat_bitmap_mismatches && !check.errors);
        assert(exfat_unmount("/vol"));
    }
    if(enabled) {
        struct sync_timing_snapshot summary;sync_timing_snapshot(&summary);
        assert(!summary.active && !summary.invalid && !summary.dropped);
        assert(summary.records[0].pid==7 && !summary.records[0].invalid);
        assert(summary.records[0].calls==(fault?34u:65u));
        assert(summary.records[0].errors==(fault?2u:0u));
        assert(summary.records[0].worst.content.calls==2 && summary.records[0].worst.content.worst.identity);
        printf("sync-content-result: enabled=1 rounds=%u calls=%llu worst_contents=%llu worst_bytes=%llu identity=%llu faults=%llu\n",
            rounds,(unsigned long long)summary.records[0].calls,(unsigned long long)summary.records[0].worst.content.calls,
            (unsigned long long)summary.records[0].worst.content.bytes,(unsigned long long)summary.records[0].worst.content.worst.identity,
            (unsigned long long)summary.records[0].errors);
    } else printf("sync-content-result: enabled=0 rounds=%u\n",rounds);
    kfree(expected);
}
