#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FAIL() do { printf("positional: failed line=%d errno=%d\n",__LINE__,errno); return -1; } while(0)

static int position_fd;
static unsigned position_start;
static void *position_worker(void *arg) {
    unsigned id=(unsigned)(uintptr_t)arg;
    while(!__atomic_load_n(&position_start,__ATOMIC_ACQUIRE)) sched_yield();
    for(unsigned i=0;i<128;i++) {
        unsigned char value=(unsigned char)(id+32),readback=0;
        if(pwrite(position_fd,&value,1,32+id)!=1 ||
            pread(position_fd,&readback,1,32+id)!=1 || readback!=value)
            return (void *)1;
        sched_yield();
    }
    return NULL;
}
int posix_positional_test(void) {
    const char *path="/fat/positional-test.txt";
    unlink(path);
    int fd=open(path,O_CREAT|O_RDWR|O_TRUNC,0600);
    if(fd<0) FAIL();
    char data[64];memset(data,'x',sizeof(data));
    if(write(fd,data,sizeof(data))!=(ssize_t)sizeof(data)) FAIL();
    position_fd=dup(fd);
    if(position_fd<0 || lseek(fd,7,SEEK_SET)!=7) FAIL();
    pthread_t threads[4];__atomic_store_n(&position_start,0,__ATOMIC_RELAXED);
    for(unsigned i=0;i<4;i++)
        if(pthread_create(&threads[i],NULL,position_worker,(void *)(uintptr_t)i)) FAIL();
    __atomic_store_n(&position_start,1,__ATOMIC_RELEASE);
    for(unsigned i=0;i<128;i++) {
        if(lseek(fd,0,SEEK_CUR)!=7) FAIL();
        sched_yield();
    }
    for(unsigned i=0;i<4;i++) {
        void *result=NULL;
        if(pthread_join(threads[i],&result) || result) FAIL();
    }
    if(lseek(fd,0,SEEK_CUR)!=7 || pread(fd,data,4,32)!=4 || memcmp(data," !\"#",4)) FAIL();
    errno=0;if(pread(fd,data,1,-1)!=-1 || errno!=EINVAL) FAIL();
    errno=0;if(pread(-1,data,1,0)!=-1 || errno!=EBADF) FAIL();
    errno=0;if(pread(fd,(void *)(uintptr_t)1,1,0)!=-1 || errno!=EFAULT) FAIL();
    if(pwrite(fd,NULL,0,0)!=0 || pread(fd,data,1,INT64_MAX)!=0) FAIL();
    int pipes[2];if(pipe(pipes)) FAIL();
    errno=0;if(pread(pipes[0],data,1,0)!=-1 || errno!=ESPIPE) FAIL();
    errno=0;if(pwrite(pipes[1],data,1,0)!=-1 || errno!=ESPIPE) FAIL();
    if(close(pipes[0]) || close(pipes[1]) || close(position_fd) || close(fd)) FAIL();
    fd=open(path,O_WRONLY|O_APPEND);
    if(fd<0 || pwrite(fd,"A",1,0)!=1 || lseek(fd,0,SEEK_CUR)!=64) FAIL();
    errno=0;if(pread(fd,data,1,0)!=-1 || errno!=EBADF) FAIL();
    if(close(fd)) FAIL();
    fd=open(path,O_RDONLY);
    if(fd<0 || pread(fd,data,1,0)!=1 || data[0]!='A') FAIL();
    errno=0;if(pwrite(fd,"B",1,0)!=-1 || errno!=EBADF) FAIL();
    if(close(fd) || unlink(path)) FAIL();
    puts("threadstress: positional ok");
    return 0;
}
