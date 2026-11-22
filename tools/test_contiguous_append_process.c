/* Actual process content operations and VFS/exFAT global writeback. Reuse the
 * existing host adapters; rename only their unrelated public test entry. */
#define test_sync_content_case contiguous_append_existing_case
#include "test_process_writeback.c"
#undef test_sync_content_case
unsigned contiguous_append_round;
unsigned contiguous_append_read_role(void) {
    return sync_fixture_content_active?sync_fixture_content_role:UINT32_MAX;
}
void test_sync_content_case(const char *mode,struct block_device *device) {
    assert(!strcmp(mode,"sync-content-contiguous-growth"));
    enum { BYTES=1366*768*4+54, CHUNK=65536 };
    assert(sync_timing_enable());
    struct process subject={0};subject.fsize_limit.current=UINT64_MAX;
    struct process_file_content *image=sync_fixture_content("/vol/work/capture.tmp");
    struct process_file_content *other=sync_fixture_content("/vol/work/other.txt");
    assert(!image->size && !other->size && image!=other);
    uint64_t identity=image->diagnostic_identity;
    image->refs+=2; /* second independent writer and surviving reader */
    write_files[0]=(struct process_write_file){.used=true,.refs=1,.content=image,.open_flags=SRV_OPEN_WRITE};
    write_files[1]=(struct process_write_file){.used=true,.refs=1,.content=other,.open_flags=SRV_OPEN_WRITE};
    write_files[2]=(struct process_write_file){.used=true,.refs=1,.content=image,.open_flags=SRV_OPEN_WRITE};
    read_files[0]=(struct process_read_file){.used=true,.refs=1,.content=image};
    subject.files[0]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS_WRITE,.handle=1};
    subject.files[1]=(struct process_file){.used=true,.type=PROCESS_FILE_VFS_WRITE,.handle=2};
    unsigned char *expected=kmalloc(BYTES);assert(expected);
    for(unsigned i=0;i<BYTES;i++)expected[i]=(uint8_t)(i*37+11);
    unsigned char marker[17];uint64_t sum=0,prefix=0;
    for(uint64_t at=0;at<BYTES;) {
        uint64_t old=at,n=BYTES-at;if(n>CHUNK)n=CHUNK;
        contiguous_append_round++;
        assert(process_file_write(&subject,0,expected+at,n)==(int64_t)n);at+=n;
        memset(marker,(int)contiguous_append_round,sizeof(marker));write_files[1].offset=0;
        assert(process_file_write(&subject,1,marker,sizeof(marker))==(int64_t)sizeof(marker));
        assert(image->dirty && other->dirty && image->refs==3 && write_files[2].content==image);
        assert(sync_fixture_flush(device,true,2,at+sizeof(marker))==0);
        assert(!image->dirty && !other->dirty && !image->failed && image->diagnostic_identity==identity);
        sync_fixture_media("/vol/work/capture.tmp",expected,at);
        sum+=at;prefix+=old;
        printf("contiguous-growth: round=%u write_bytes=%llu size=%llu\n",contiguous_append_round,
            (unsigned long long)n,(unsigned long long)at);
    }
    assert(contiguous_append_round==65 && sum==140511286 && prefix==136314880);
    assert(write_file_close(1)==0 && write_file_close(3)==0 && image->refs==1);
    assert(process_file_sync_all()==0 && !image->dirty);assert(read_file_close(1)==0 && write_file_close(2)==0);
    /* Independent fresh media reads before and after namespace publication.
     * The actual checked-rename/SYS_FILE_VERIFY wire remains covered by the
     * existing capture-media integration; this fixture isolates writeback. */
    sync_fixture_media("/vol/work/capture.tmp",expected,BYTES);
    assert(exfat_rename("/vol/work/capture.tmp","/vol/work/capture.bmp"));
    assert(device->flush(device));sync_fixture_media("/vol/work/capture.bmp",expected,BYTES);
    sync_fixture_media("/vol/work/anchor.txt",(const uint8_t *)"unchanged",9);
    struct exfat_check_result check={0};assert(exfat_check("/vol",&check));
    assert(!check.errors && !check.leaked_clusters && !check.fat_bitmap_mismatches);
    assert(exfat_unmount("/vol"));kfree(expected);
    printf("contiguous-growth-result: rounds=65 bytes=%u committed_bytes=%llu prefix_bytes=%llu shared_reader=1 allocation_clean=1\n",
        BYTES,(unsigned long long)sum,(unsigned long long)prefix);
}
