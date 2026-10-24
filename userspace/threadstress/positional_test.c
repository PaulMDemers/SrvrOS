#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
    if(fd<0) return -1;
    char data[64];memset(data,'x',sizeof(data));
    if(write(fd,data,sizeof(data))!=(ssize_t)sizeof(data)) return -1;
    position_fd=dup(fd);
    if(position_fd<0 || lseek(fd,7,SEEK_SET)!=7) return -1;
    pthread_t threads[4];__atomic_store_n(&position_start,0,__ATOMIC_RELAXED);
    for(unsigned i=0;i<4;i++)
        if(pthread_create(&threads[i],NULL,position_worker,(void *)(uintptr_t)i)) return -1;
    __atomic_store_n(&position_start,1,__ATOMIC_RELEASE);
    for(unsigned i=0;i<128;i++) {
        if(lseek(fd,0,SEEK_CUR)!=7) return -1;
        sched_yield();
    }
    for(unsigned i=0;i<4;i++) {
        void *result=NULL;
        if(pthread_join(threads[i],&result) || result) return -1;
    }
    if(lseek(fd,0,SEEK_CUR)!=7 || pread(fd,data,4,32)!=4 || memcmp(data," !\"#",4)) return -1;
    errno=0;if(pread(fd,data,1,-1)!=-1 || errno!=EINVAL) return -1;
    errno=0;if(pread(-1,data,1,0)!=-1 || errno!=EBADF) return -1;
    errno=0;if(pread(fd,(void *)(uintptr_t)1,1,0)!=-1 || errno!=EFAULT) return -1;
    if(pwrite(fd,NULL,0,0)!=0 || pread(fd,data,1,INT64_MAX)!=0) return -1;
    int pipes[2];if(pipe(pipes)) return -1;
    errno=0;if(pread(pipes[0],data,1,0)!=-1 || errno!=ESPIPE) return -1;
    errno=0;if(pwrite(pipes[1],data,1,0)!=-1 || errno!=ESPIPE) return -1;
    if(close(pipes[0]) || close(pipes[1]) || close(position_fd) || close(fd)) return -1;
    fd=open(path,O_WRONLY|O_APPEND);
    if(fd<0 || pwrite(fd,"A",1,0)!=1 || lseek(fd,0,SEEK_CUR)!=64) return -1;
    errno=0;if(pread(fd,data,1,0)!=-1 || errno!=EBADF) return -1;
    if(close(fd)) return -1;
    fd=open(path,O_RDONLY);
    if(fd<0 || pread(fd,data,1,0)!=1 || data[0]!='A') return -1;
    errno=0;if(pwrite(fd,"B",1,0)!=-1 || errno!=EBADF) return -1;
    if(close(fd) || unlink(path)) return -1;
    puts("threadstress: positional ok");
    return 0;
}
