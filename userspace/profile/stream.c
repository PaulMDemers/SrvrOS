#include "stream.h"
#include "codec.h"
#include "memory.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef SRVROS_PROFILE_HOST_TEST
static int fault(const char *name){const char *value=getenv("SRVROS_PROFILE_TEST_IO");return value&&!strcmp(value,name);}
#else
static int fault(const char *name){(void)name;return 0;}
#endif
int profile_stream_sync(int fd){if(fault("fsync-error")){errno=EIO;return -1;}return fsync(fd);}
static int read_exact(int fd,void *buffer,size_t n,uint64_t offset){
    size_t done=0;while(done<n){if(fault("read-error")){errno=EIO;return -1;}size_t want=n-done;if(fault("read-short")&&want>4096)want=4096;ssize_t r=pread(fd,(char *)buffer+done,want,(off_t)(offset+done));if(r<0&&errno==EINTR)continue;if(r<=0)return -1;done+=(size_t)r;}return 0;
}
static int write_exact(int fd,const void *buffer,size_t n){
    size_t done=0;while(done<n){if(fault("write-error")){errno=ENOSPC;return -1;}size_t want=n-done;if(fault("write-short")&&want>4096)want=4096;ssize_t r=write(fd,(const char *)buffer+done,want);if(r<0&&errno==EINTR)continue;if(r<=0)return -1;done+=(size_t)r;}return 0;
}
static int stream_copy(int input,uint64_t offset,uint64_t size,int output,char hash[65],int checkpoint_fd,struct profile_sha256_ctx *whole){
    unsigned char *buffer=malloc(PROFILE_STREAM_BUFFER);if(!buffer)return -1;
    struct profile_sha256_ctx c;profile_sha256_init(&c);uint64_t done=0;
    while(done<size){size_t n=size-done>PROFILE_STREAM_BUFFER?PROFILE_STREAM_BUFFER:(size_t)(size-done);
        if(read_exact(input,buffer,n,offset+done)||(output>=0&&write_exact(output,buffer,n))){free(buffer);return -1;}
        profile_sha256_update(&c,buffer,n);if(whole)profile_sha256_update(whole,buffer,n);done+=n;
        if(checkpoint_fd>=0 && done==PROFILE_STREAM_BUFFER){if(profile_stream_sync(checkpoint_fd)){free(buffer);return -1;}const char *stop=getenv("SRVROS_PROFILE_STOP_AFTER");if(stop && !strcmp(stop,"copy-partial")){profile_memory_report("copy-partial");free(buffer);_exit(86);}checkpoint_fd=-1;}
    }
    profile_sha256_final(&c,hash);free(buffer);return 0;
}
int profile_stream_copy(int fd,uint64_t offset,uint64_t size,int out,char hash[65],int checkpoint_fd){return stream_copy(fd,offset,size,out,hash,checkpoint_fd,NULL);}
int profile_stream_append(int fd,uint64_t offset,uint64_t size,int out,char hash[65],struct profile_sha256_ctx *whole){return stream_copy(fd,offset,size,out,hash,-1,whole);}
int profile_stream_hash(int fd,uint64_t offset,uint64_t size,char hash[65]){return stream_copy(fd,offset,size,-1,hash,-1,NULL);}
