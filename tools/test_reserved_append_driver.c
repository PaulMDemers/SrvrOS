/* Reuse real disposable image backend and its ordered cost fingerprint. Only
 * block callback boundaries/current-context seams are modeled, not USB time. */
#define main reserved_append_unused_driver_main
#define process_has_open_vfs_prefix reserved_append_unused_busy
#define block_read reserved_append_backend_read
#define block_write reserved_append_backend_write
#include "exfat_host_driver.c"
#undef main
#undef process_has_open_vfs_prefix
#undef block_read
#undef block_write
#include "test_reserved_append_io.h"
void reserved_append_backend_step(void);
void reserved_append_read_observed(uint64_t,bool);
void reserved_append_note_target_io(bool,uint64_t,uint64_t,uint64_t);
void reserved_append_case(const char *,struct block_device *);
bool block_read(struct block_device *d,uint64_t o,void *p,uint64_t n) {
    reserved_append_backend_step();bool ok=reserved_append_backend_read(d,o,p,n);
    reserved_append_read_observed(n,ok);return ok;
}
bool block_write(struct block_device *d,uint64_t o,const void *p,uint64_t n) {
    reserved_append_backend_step();reserved_append_note_target_io(true,writes+1,o,n);return reserved_append_backend_write(d,o,p,n);
}
static bool reserved_append_flush(struct block_device *d) {reserved_append_backend_step();reserved_append_note_target_io(false,flushes+1,0,0);return flush(d);}
void reserved_append_io_snapshot(struct reserved_append_io *out) {
    *out=(struct reserved_append_io){sync_content_callbacks,read_calls,read_bytes,writes,write_bytes,flushes};
}
void reserved_append_model_delay_us(uint64_t usec) {
    assert(usec<=2000000 && sync_content_us<=UINT64_MAX-usec);
    sync_content_us+=usec;
}
uint64_t reserved_append_allocations(void) {return alloc_calls;}
unsigned reserved_append_owner_depth(void) {return io_depth;}
int main(int argc,char **argv) {
    assert(argc==3);fd=open(argv[1],O_RDWR);assert(fd>=0);
    if(getenv("SRVROS_RESERVED_FAIL_WRITE"))fail_at=(unsigned)strtoul(getenv("SRVROS_RESERVED_FAIL_WRITE"),NULL,10);
    if(getenv("SRVROS_RESERVED_FAIL_FLUSH"))fail_flush_at=(unsigned)strtoul(getenv("SRVROS_RESERVED_FAIL_FLUSH"),NULL,10);
    struct stat st;assert(fstat(fd,&st)==0 && S_ISREG(st.st_mode));
    struct block_device d={.used=true,.name="fixture",.block_size=512,.block_count=(uint64_t)st.st_size/512,
        .read=block_read,.write=block_write,.flush=reserved_append_flush};
    assert(exfat_mount_block_device("/vol",&d));
    reserved_append_case(argv[2],&d);
    assert(fsync(fd)==0);close(fd);
    printf("reserved-io: callbacks=%llu hash=%016llx reads=%llu read_bytes=%llu writes=%u write_bytes=%llu flushes=%u\n",
        (unsigned long long)sync_content_callbacks,(unsigned long long)sync_content_trace,
        (unsigned long long)read_calls,(unsigned long long)read_bytes,writes,
        (unsigned long long)write_bytes,flushes);
    return 0;
}
