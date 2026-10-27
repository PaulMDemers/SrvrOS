#include "save.h"
#include <srvros/sys.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
static unsigned verify(const char *path,const unsigned char *data,size_t length,capture_progress_fn progress,void *context,unsigned phase) {
    if(progress)progress(context,phase,0);
    int fd=open(path,O_RDONLY);if(fd<0) return DESK_CAPTURE_IO;
    struct stat st;unsigned result=DESK_CAPTURE_OK;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || (uint64_t)st.st_size!=length) result=DESK_CAPTURE_IO;
    unsigned char buffer[4096];size_t offset=0;
    while(!result && offset<length) {
        size_t bytes=length-offset;if(bytes>sizeof(buffer)) bytes=sizeof(buffer);
        ssize_t n=read(fd,buffer,bytes);
        if(n<=0 || (size_t)n>bytes || memcmp(buffer,data+offset,n)) result=DESK_CAPTURE_IO;
        else { offset+=n;if(progress && (!(offset%65536) || offset==length))progress(context,phase,offset); }
    }
    if(close(fd)) result=DESK_CAPTURE_IO;
    return result;
}
unsigned capture_save_progress(const char *path,const unsigned char *data,size_t length,capture_progress_fn progress,void *context) {
    if(progress)progress(context,CAPTURE_PREPARE,0);
    if(!path || path[0]!='/' || !data || length<54 || length>DESK_CAPTURE_MAX_BYTES) return DESK_CAPTURE_INVALID;
    char temporary[DESK_CAPTURE_PATH],folder[DESK_CAPTURE_PATH];
    if(strlen(path)>=sizeof(folder)) return DESK_CAPTURE_INVALID;
    strcpy(folder,path);char *slash=strrchr(folder,'/');if(!slash || slash==folder) return DESK_CAPTURE_INVALID;*slash=0;
    struct stat st;
    if(stat(folder,&st)) {
        if(errno!=ENOENT || mkdir(folder,0755)) return DESK_CAPTURE_IO;
    } else if(!S_ISDIR(st.st_mode)) return DESK_CAPTURE_IO;
    if(!stat(path,&st)) return DESK_CAPTURE_EXISTS;
    if(errno!=ENOENT) return DESK_CAPTURE_IO;
    int n=snprintf(temporary,sizeof(temporary),"%s/.capture-%llu-%llu.tmp",folder,(unsigned long long)srv_getpid(),(unsigned long long)srv_ticks());
    if(n<0 || (size_t)n>=sizeof(temporary)) return DESK_CAPTURE_INVALID;
    int fd=open(temporary,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0) return DESK_CAPTURE_IO;
    unsigned result=DESK_CAPTURE_OK;size_t offset=0;
    if(progress)progress(context,CAPTURE_WRITE,0);
    while(offset<length) {
        size_t bytes=length-offset;if(bytes>16384) bytes=16384;
        ssize_t written=write(fd,data+offset,bytes);
        if(written<=0 || (size_t)written>bytes) { result=DESK_CAPTURE_IO;break; }offset+=written;
        if(progress && (!(offset%65536) || offset==length))progress(context,CAPTURE_WRITE,offset);
    }
    if(progress)progress(context,CAPTURE_FSYNC,offset);
    if(!result && fsync(fd)) result=DESK_CAPTURE_IO;
    if(close(fd)) result=DESK_CAPTURE_IO;
    if(progress)progress(context,CAPTURE_TEMP_SYNC,offset);
    if(!result && (srv_sync() || verify(temporary,data,length,progress,context,CAPTURE_TEMP_VERIFY))) result=DESK_CAPTURE_IO;
    if(!result) {
        if(progress)progress(context,CAPTURE_RENAME,length);
        if(stat(temporary,&st) || !S_ISREG(st.st_mode) || (uint64_t)st.st_size!=length) result=DESK_CAPTURE_IO;
        else {
            struct srv_file_change change={.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(change),.operation=SRV_FILE_RENAME,
                .expected_inode=st.st_ino,.expected_size=st.st_size,.expected_mtime=st.st_mtime,.path=temporary,.destination=path};
            long moved=srv_file_change(&change);
            if(moved==SRV_FILE_EXISTS) result=DESK_CAPTURE_EXISTS;
            else if(moved) result=DESK_CAPTURE_IO;
            else {
                if(progress)progress(context,CAPTURE_FINAL_SYNC,length);
                if(srv_sync() || verify(path,data,length,progress,context,CAPTURE_FINAL_VERIFY))result=DESK_CAPTURE_IO;
            }
        }
    }
    /* Only our exclusive temporary name is eligible for cleanup; the destination
     * may exist after an uncertain sync and must be left available for inspection. */
    if(progress)progress(context,CAPTURE_CLEANUP,length);
    if(result) unlink(temporary);
    return result;
}

unsigned capture_save(const char *path,const unsigned char *data,size_t length) {
    return capture_save_progress(path,data,length,NULL,NULL);
}
