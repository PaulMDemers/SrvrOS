#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
static int injected_write,injected_read,injected_sync,injected_rename;
static unsigned full_write,write_calls,read_calls,sync_calls;static size_t max_write,read_bytes;
static unsigned reserve_calls;static long reserve_result=-1;
static ssize_t test_write(int fd,const void *data,size_t n) { write_calls++;if(n>max_write)max_write=n;if(injected_write) return -1;return write(fd,data,!full_write && n>311?311:n); }
static ssize_t test_read(int fd,void *data,size_t n) { read_calls++;ssize_t result=read(fd,data,n>137?137:n);if(result>0)read_bytes+=(size_t)result;if(injected_read && result>0) ((char *)data)[0]^=1;return result; }
#define write test_write
#define read test_read
#include "../userspace/shotworker/save.c"
#undef write
#undef read
long srv_file_reserve_append_ram(int fd,uint64_t capacity) {
    struct stat st;assert(!fstat(fd,&st) && S_ISREG(st.st_mode) && st.st_size==0);
    assert(capacity>=54 && capacity<=DESK_CAPTURE_MAX_BYTES);reserve_calls++;
    return reserve_result;
}
long srv_file_verify(const struct srv_file_verify_request *q) {
    struct stat st;
    assert(q->abi_version==SRV_ABI_VERSION && q->struct_size==sizeof(*q) && !q->reserved);
    int fd=open(q->path,O_RDONLY);if(fd<0)return SRV_ERR_IO;
    int result=0;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || (uint64_t)st.st_size!=q->length ||
       (uint64_t)st.st_ino!=q->expected_inode || (uint64_t)st.st_mtime!=q->expected_mtime)result=-1;
    unsigned char bytes[4096];uint64_t offset=0;
    while(!result && offset<q->length) {
        size_t length=q->length-offset;if(length>sizeof(bytes))length=sizeof(bytes);
        ssize_t n=test_read(fd,bytes,length);
        if(n<=0 || (size_t)n>length || memcmp(bytes,(const unsigned char *)q->data+offset,(size_t)n))result=-1;
        else offset+=(size_t)n;
    }
    if(close(fd))result=-1;
    return result;
}
long srv_getpid(void) { return getpid(); }
long srv_ticks(void) { return 12345; }
long srv_sync(void) { sync_calls++;return injected_sync && sync_calls==(unsigned)injected_sync?-1:0; }
long srv_file_change(const struct srv_file_change *q) {
    struct stat st;if(injected_rename) return SRV_FILE_EXISTS;
    assert(q->operation==SRV_FILE_RENAME);
    if(stat(q->path,&st) || (uint64_t)st.st_ino!=q->expected_inode || (uint64_t)st.st_size!=q->expected_size || (uint64_t)st.st_mtime!=q->expected_mtime) return SRV_FILE_CHANGED;
    if(!stat(q->destination,&st)) return SRV_FILE_EXISTS;
    return rename(q->path,q->destination);
}
static unsigned phases,last_phase;static uint64_t progress_bytes;
static void progress(void *context,unsigned phase,uint64_t bytes) {
    assert(context==&phases && phase>=last_phase && phase<CAPTURE_PHASE_COUNT);
    last_phase=phase;phases|=1u<<phase;progress_bytes=bytes;
}
int main(void) {
    char dir[]="/tmp/srvros-capture-test-XXXXXX";assert(mkdtemp(dir));char path[160],folder[80],temp[160];
    snprintf(folder,sizeof(folder),"%s/Screenshots",dir);snprintf(path,sizeof(path),"%s/proof.bmp",folder);
    snprintf(temp,sizeof(temp),"%s/.capture-%llu-12345.tmp",folder,(unsigned long long)getpid());
    unsigned char data[71000];for(size_t i=0;i<sizeof(data);i++) data[i]=(i*73+i/31)&255;
    assert(capture_save_progress(path,data,sizeof(data),progress,&phases)==DESK_CAPTURE_OK);
    assert(reserve_calls==1 && reserve_result<0); /* Old-kernel refusal is harmless. */
    assert((phases&(1u<<CAPTURE_TEMP_VERIFY)) && (phases&(1u<<CAPTURE_FINAL_VERIFY)) && (phases&(1u<<CAPTURE_RENAME)) && progress_bytes==sizeof(data));struct stat st;assert(!stat(path,&st) && st.st_size==sizeof(data));
    assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_EXISTS);assert(!unlink(path));
    injected_write=1;assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_IO && stat(path,&st)<0 && stat(temp,&st)<0);injected_write=0;
    injected_read=1;assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_IO && stat(path,&st)<0 && stat(temp,&st)<0);injected_read=0;
    sync_calls=0;injected_sync=1;assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_IO && stat(path,&st)<0 && stat(temp,&st)<0);injected_sync=0;
    injected_rename=1;assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_EXISTS && stat(path,&st)<0 && stat(temp,&st)<0);injected_rename=0;
    /* A capture-sized admission batch must stay bounded and retain both full
     * byte-for-byte verification passes. Real short-I/O coverage above stays. */
    unsigned char *large=malloc(200000);assert(large);
    for(size_t i=0;i<200000;i++)large[i]=(unsigned char)(i*17+i/97);
    full_write=1;reserve_result=0;reserve_calls=0;write_calls=read_calls=sync_calls=0;max_write=read_bytes=0;
    assert(capture_save(path,large,200000)==DESK_CAPTURE_OK);
    assert(reserve_calls==1 && write_calls==4 && max_write==65536 && read_calls>0 && read_bytes==400000 && sync_calls==2);
    assert(!unlink(path));
    /* An uncertain post-rename sync retains the published destination for
     * inspection, while never reporting a durable successful save. */
    sync_calls=0;injected_sync=2;
    assert(capture_save(path,large,200000)==DESK_CAPTURE_IO);
    assert(!stat(path,&st) && st.st_size==200000 && stat(temp,&st)<0);
    injected_sync=0;assert(!unlink(path));free(large);
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0 && write(fd,"keep",4)==4 && !close(fd));
    assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_IO && !stat(temp,&st) && st.st_size==4);assert(!unlink(temp));
    assert(capture_save(path,data,DESK_CAPTURE_MAX_BYTES+1)==DESK_CAPTURE_INVALID);
    assert(!rmdir(folder) && !rmdir(dir));
    puts("capture-save: PASS new folder, complete verified save, short IO, existing destination/temp preserved, bounded batched admission, both full readbacks, write/sync/readback/rename failures, uncertain publication retention and cleanup");
}
