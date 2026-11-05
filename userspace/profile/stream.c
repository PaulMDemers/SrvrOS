#include "stream.h"
#include "memory.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#ifdef SRVROS_PROFILE_HOST_TEST
#include <fcntl.h>
#include <sys/stat.h>
static int fault(const char *name){const char *value=getenv("SRVROS_PROFILE_TEST_IO");return value&&!strcmp(value,name);}
profile_stream_handle profile_stream_open_read(const char *path){if(fault("unsupported")){errno=EOPNOTSUPP;return -1;}return open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);}
profile_stream_handle profile_stream_open_new(const char *path){return open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);}
int64_t profile_stream_pread(profile_stream_handle h,void *buffer,size_t n,uint64_t offset){return pread((int)h,buffer,n,(off_t)offset);}
int64_t profile_stream_write(profile_stream_handle h,const void *buffer,size_t n){return write((int)h,buffer,n);}
int profile_stream_stat(profile_stream_handle h,struct profile_stream_info *out){struct stat s;if(fstat((int)h,&s))return -1;*out=(struct profile_stream_info){(uint64_t)s.st_size,(uint64_t)s.st_ino,(uint64_t)s.st_mode,(uint64_t)s.st_nlink,(uint64_t)s.st_mtime,(uint64_t)s.st_ctime,(uint64_t)s.st_dev};return 0;}
int profile_stream_sync(profile_stream_handle h){if(fault("fsync-error")){errno=EIO;return -1;}return fsync((int)h);}
int profile_stream_close(profile_stream_handle h){return close((int)h);}
int profile_stream_publish(const char *source,const char *destination,const struct profile_stream_info *expected){
    struct stat s,d;if(lstat(source,&s)||!S_ISREG(s.st_mode)||s.st_nlink!=1||(uint64_t)s.st_dev!=expected->dev||(uint64_t)s.st_ino!=expected->inode||(uint64_t)s.st_size!=expected->size||(uint64_t)s.st_mtime!=expected->mtime){errno=EIO;return -1;}
    if(!lstat(destination,&d)){errno=EEXIST;return -1;}if(errno!=ENOENT)return -1;return rename(source,destination);
}
#else
#include <srvros/sys.h>
#include <srvros/file_stream.h>
static int fault(const char *name){(void)name;return 0;}
static int64_t call(unsigned op,profile_stream_handle handle,const char *path,void *buffer,size_t length,uint64_t offset,uint64_t mode){
    struct srv_file_stream_request request={.version=SRV_FILE_STREAM_VERSION,.size=sizeof(request),.operation=op,.handle=(uint64_t)handle,.path=(uint64_t)(uintptr_t)path,.buffer=(uint64_t)(uintptr_t)buffer,.length=length,.offset=offset,.mode=mode};
    long result=srv_file_stream(&request);if(result<0){errno=(int)-result;return -1;}return result;
}
profile_stream_handle profile_stream_open_read(const char *path){return call(SRV_FILE_STREAM_OPEN_READ,0,path,NULL,0,0,0);}
profile_stream_handle profile_stream_open_new(const char *path){return call(SRV_FILE_STREAM_CREATE_APPEND,0,path,NULL,0,0,0600);}
int64_t profile_stream_pread(profile_stream_handle h,void *buffer,size_t n,uint64_t offset){return call(SRV_FILE_STREAM_PREAD,h,NULL,buffer,n,offset,0);}
int64_t profile_stream_write(profile_stream_handle h,const void *buffer,size_t n){return call(SRV_FILE_STREAM_APPEND,h,NULL,(void *)buffer,n,0,0);}
int profile_stream_stat(profile_stream_handle h,struct profile_stream_info *out){struct srv_file_stream_stat s={0};if(call(SRV_FILE_STREAM_STAT,h,NULL,&s,sizeof(s),0,0)<0)return -1;if(s.abi_version!=SRV_ABI_VERSION||s.struct_size!=sizeof(s)||s.type!=0){errno=EIO;return -1;}*out=(struct profile_stream_info){s.size,s.inode,s.mode,s.nlink,s.mtime,s.ctime,1};return 0;}
int profile_stream_sync(profile_stream_handle h){return (int)call(SRV_FILE_STREAM_SYNC,h,NULL,NULL,0,0,0);}
int profile_stream_close(profile_stream_handle h){return (int)call(SRV_FILE_STREAM_CLOSE,h,NULL,NULL,0,0,0);}
int profile_stream_publish(const char *source,const char *destination,const struct profile_stream_info *expected){
    struct srv_file_change request={.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(request),.operation=SRV_FILE_RENAME,.expected_inode=expected->inode,.expected_size=expected->size,.expected_mtime=expected->mtime,.expected_type=0,.path=source,.destination=destination};
    if(srv_file_change(&request)!=0){errno=EIO;return -1;}return 0;
}
#endif
static int read_exact(profile_stream_handle h,void *buffer,size_t n,uint64_t offset){
    size_t done=0;while(done<n){if(fault("read-error")){errno=EIO;return -1;}size_t want=n-done;if(fault("read-short")&&want>4096)want=4096;int64_t r=profile_stream_pread(h,(char *)buffer+done,want,offset+done);if(r<0&&errno==EINTR)continue;if(r<=0)return -1;done+=(size_t)r;}return 0;
}
static int write_exact(profile_stream_handle h,const void *buffer,size_t n){
    size_t done=0;while(done<n){if(fault("write-error")){errno=ENOSPC;return -1;}size_t want=n-done;if(fault("write-short")&&want>4096)want=4096;int64_t r=profile_stream_write(h,(const char *)buffer+done,want);if(r<0&&errno==EINTR)continue;if(r<=0)return -1;done+=(size_t)r;}return 0;
}
static int stream_copy(profile_stream_handle input,uint64_t offset,uint64_t size,profile_stream_handle output,char hash[65],profile_stream_handle checkpoint,struct profile_sha256_ctx *whole){
    unsigned char *buffer=malloc(PROFILE_STREAM_BUFFER);if(!buffer)return -1;struct profile_sha256_ctx c;profile_sha256_init(&c);uint64_t done=0;
    while(done<size){size_t n=size-done>PROFILE_STREAM_BUFFER?PROFILE_STREAM_BUFFER:(size_t)(size-done);
        if(read_exact(input,buffer,n,offset+done)||(output>=0&&write_exact(output,buffer,n))){free(buffer);return -1;}
        profile_sha256_update(&c,buffer,n);if(whole)profile_sha256_update(whole,buffer,n);done+=n;
        if(checkpoint>=0&&done==PROFILE_STREAM_BUFFER){if(profile_stream_sync(checkpoint)){free(buffer);return -1;}const char *stop=getenv("SRVROS_PROFILE_STOP_AFTER");if(stop&&!strcmp(stop,"copy-partial")){profile_memory_report("copy-partial");free(buffer);_exit(86);}checkpoint=-1;}
    }
    profile_sha256_final(&c,hash);free(buffer);return 0;
}
int profile_stream_copy(profile_stream_handle fd,uint64_t offset,uint64_t size,profile_stream_handle out,char hash[65],profile_stream_handle checkpoint){return stream_copy(fd,offset,size,out,hash,checkpoint,NULL);}
int profile_stream_append(profile_stream_handle fd,uint64_t offset,uint64_t size,profile_stream_handle out,char hash[65],struct profile_sha256_ctx *whole){return stream_copy(fd,offset,size,out,hash,-1,whole);}
int profile_stream_hash(profile_stream_handle fd,uint64_t offset,uint64_t size,char hash[65]){return stream_copy(fd,offset,size,-1,hash,-1,NULL);}
