#define SRVROS_STREAM_PROCESS_HOST_TEST
#define main process_output_original_main
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#endif
#include "test_process_output.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef main
static bool busy,cleanup_ok=true;
static unsigned busy_calls,cleanup_calls;
static uint64_t cleanup_owner;
bool exfat_stream_busy(const char *path,bool subtree){(void)path;(void)subtree;busy_calls++;return busy;}
bool file_stream_cleanup(uint64_t owner){cleanup_calls++;cleanup_owner=owner;return cleanup_ok;}
int main(void){
    struct process subject;clear_process(&subject);subject.pid=731;
    assert(process_stream_identity(&subject)==0);
    subject.allocated=subject.active=true;
    uint64_t first=process_stream_identity(&subject);assert(first && process_stream_identity(&subject)==first);
    clear_process(&subject);subject.pid=731;subject.allocated=subject.active=true;
    uint64_t second=process_stream_identity(&subject);assert(second && second!=first);
    uint64_t saved_next=next_stream_instance;next_stream_instance=UINT64_MAX;
    struct process end;clear_process(&end);end.allocated=end.active=true;
    assert(process_stream_identity(&end)==UINT64_MAX);
    struct process exhausted;clear_process(&exhausted);exhausted.allocated=exhausted.active=true;
    assert(!process_stream_identity(&exhausted));next_stream_instance=saved_next;
    busy=true;opening=fixture_exists=true;
    assert(process_file_open_write_raw(&subject,"/test/output",SRV_OPEN_WRITE)==SRV_ERR_BUSY);
    path_fixture=true;assert(process_file_open_flags(&subject,"/test/output",SRV_OPEN_READ)==SRV_ERR_BUSY);path_fixture=false;
    assert(process_unlink_file("/test/output")==SRV_ERR_BUSY);
    assert(process_rename_file("/test/output","/test/new")==SRV_ERR_BUSY);
    assert(process_has_open_vfs_prefix("/test"));assert(busy_calls>=4);
    busy=false;assert(!process_has_open_vfs_prefix("/not-open"));
    assert(cleanup_process_files(&subject) && cleanup_owner==second && cleanup_calls==1);
    cleanup_ok=false;assert(finish_process_files(&subject,0)==1 && cleanup_calls==2);
    assert(finish_process_files(&subject,17)==17 && cleanup_calls==3);
    close_cloexec_files(&subject);assert(cleanup_calls==4 && cleanup_owner==second);
    cleanup_ok=true;current_subject=&subject;subject.fsize_limit.current=100;
    assert(process_stream_write_allowed(95,10)==5);
    assert(process_stream_write_allowed(0,0)==0);
    assert(process_stream_write_allowed(UINT64_MAX,1)==SRV_ERR_INVAL);
    puts("file stream process hooks: PASS");return 0;
}
