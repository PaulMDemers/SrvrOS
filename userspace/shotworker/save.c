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
    struct stat st;
    if(stat(path,&st) || !S_ISREG(st.st_mode) || (uint64_t)st.st_size!=length)return DESK_CAPTURE_IO;
    /* Ordinary reopen/read can reuse a live shared RAM buffer. This operation
     * instead owns a fresh VFS media snapshot through complete comparison and
     * release, without changing any ordinary reader's shared-file semantics. */
    struct srv_file_verify_request request={.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(request),
        .path=path,.data=data,.length=length,.expected_inode=st.st_ino,.expected_mtime=st.st_mtime};
    if(srv_file_verify(&request))return DESK_CAPTURE_IO;
    if(progress)progress(context,phase,length);
    return DESK_CAPTURE_OK;
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
    /* Opt into bounded RAM-only appends without changing visible file length.
     * An old kernel or any refusal retains the ordinary checked write path. */
    (void)srv_file_reserve_append_ram(fd,length);
    unsigned result=DESK_CAPTURE_OK;size_t offset=0;
    if(progress)progress(context,CAPTURE_WRITE,0);
    while(offset<length) {
        /* Keep every checked write bounded to64KiB. The explicit reservation
         * may admit a proved private RAM append during foreign I/O; ordinary
         * fallbacks retain their gate. All persistence and both full byte
         * verification passes still follow below. */
        size_t bytes=length-offset;if(bytes>65536) bytes=65536;
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
