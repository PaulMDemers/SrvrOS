#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
static int injected_write,injected_read,injected_sync,injected_rename;
static ssize_t test_write(int fd,const void *data,size_t n) { if(injected_write) return -1;return write(fd,data,n>311?311:n); }
static ssize_t test_read(int fd,void *data,size_t n) { ssize_t result=read(fd,data,n>137?137:n);if(injected_read && result>0) ((char *)data)[0]^=1;return result; }
#define write test_write
#define read test_read
#include "../userspace/shotworker/save.c"
#undef write
#undef read
long srv_getpid(void) { return getpid(); }
long srv_ticks(void) { return 12345; }
long srv_sync(void) { return injected_sync?-1:0; }
long srv_file_change(const struct srv_file_change *q) {
    struct stat st;if(injected_rename) return SRV_FILE_EXISTS;
    assert(q->operation==SRV_FILE_RENAME);
    if(stat(q->path,&st) || (uint64_t)st.st_ino!=q->expected_inode || (uint64_t)st.st_size!=q->expected_size || (uint64_t)st.st_mtime!=q->expected_mtime) return SRV_FILE_CHANGED;
    if(!stat(q->destination,&st)) return SRV_FILE_EXISTS;
    return rename(q->path,q->destination);
}
int main(void) {
    char dir[]="/tmp/srvros-capture-test-XXXXXX";assert(mkdtemp(dir));char path[160],folder[80],temp[160];
    snprintf(folder,sizeof(folder),"%s/Screenshots",dir);snprintf(path,sizeof(path),"%s/proof.bmp",folder);
    snprintf(temp,sizeof(temp),"%s/.capture-%llu-12345.tmp",folder,(unsigned long long)getpid());
    unsigned char data[71000];for(size_t i=0;i<sizeof(data);i++) data[i]=(i*73+i/31)&255;
    assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_OK);struct stat st;assert(!stat(path,&st) && st.st_size==sizeof(data));
    assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_EXISTS);assert(!unlink(path));
    injected_write=1;assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_IO && stat(path,&st)<0 && stat(temp,&st)<0);injected_write=0;
    injected_read=1;assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_IO && stat(path,&st)<0 && stat(temp,&st)<0);injected_read=0;
    injected_sync=1;assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_IO && stat(path,&st)<0 && stat(temp,&st)<0);injected_sync=0;
    injected_rename=1;assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_EXISTS && stat(path,&st)<0 && stat(temp,&st)<0);injected_rename=0;
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0 && write(fd,"keep",4)==4 && !close(fd));
    assert(capture_save(path,data,sizeof(data))==DESK_CAPTURE_IO && !stat(temp,&st) && st.st_size==4);assert(!unlink(temp));
    assert(capture_save(path,data,DESK_CAPTURE_MAX_BYTES+1)==DESK_CAPTURE_INVALID);
    assert(!rmdir(folder) && !rmdir(dir));
    puts("capture-save: PASS new folder, complete verified save, short IO, existing destination/temp preserved, write/sync/readback/rename failures and cleanup");
}
