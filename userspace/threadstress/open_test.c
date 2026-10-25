#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *path="/fat/posix-open-contract.txt";
static const char *exclusive="/fat/posix-exclusive-contract.txt";
static unsigned ready,start,winners;
static void *create_worker(void *unused) {
    (void)unused;__atomic_add_fetch(&ready,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&start,__ATOMIC_ACQUIRE)) sched_yield();
    int fd=open(exclusive,O_CREAT|O_EXCL|O_WRONLY,0600);
    if(fd<0) return errno==EEXIST?NULL:(void *)1;
    __atomic_add_fetch(&winners,1,__ATOMIC_RELAXED);
    return write(fd,"x",1)==1 && close(fd)==0?NULL:(void *)2;
}
#define CHECK(x) do { if(!(x)) { printf("threadstress: open line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_open_test(void) {
    unlink(path);unlink(exclusive);
    int fd=open(path,O_CREAT|O_EXCL|O_RDONLY|O_CLOEXEC,0600);char data[16]={0};
    CHECK(fd>=0 && read(fd,data,1)==0 && (fcntl(fd,F_GETFL)&O_ACCMODE)==O_RDONLY);
    CHECK(fcntl(fd,F_GETFD)==FD_CLOEXEC);
    CHECK(write(fd,"x",1)==-1 && errno==EBADF && pwrite(fd,"x",1,0)==-1 && errno==EBADF);
    CHECK(ftruncate(fd,1)==-1 && errno==EBADF);
    CHECK(close(fd)==0);
    fd=open(path,O_WRONLY);CHECK(fd>=0 && write(fd,"seed",4)==4);
    CHECK(read(fd,data,1)==-1 && errno==EBADF && close(fd)==0);
    fd=open(path,O_WRONLY);CHECK(fd>=0 && lseek(fd,0,SEEK_CUR)==0 && write(fd,"X",1)==1 && close(fd)==0);
    fd=open(path,O_RDWR|O_APPEND|O_CLOEXEC);CHECK(fd>=0);
    int alias=dup(fd);CHECK(alias>=0 && (fcntl(alias,F_GETFL)&(O_ACCMODE|O_APPEND))==(O_RDWR|O_APPEND));
    CHECK(fcntl(alias,F_GETFD)==0 && fcntl(fd,F_GETFD)==FD_CLOEXEC);
    CHECK(lseek(fd,0,SEEK_SET)==0 && write(alias,"Y",1)==1 && lseek(fd,0,SEEK_CUR)==5);
    CHECK(fcntl(alias,F_SETFL,O_NONBLOCK)==0 && fcntl(fd,F_GETFL)==(O_RDWR|O_NONBLOCK));
    CHECK(lseek(fd,0,SEEK_SET)==0 && write(fd,"Z",1)==1);
    CHECK(pread(alias,data,5,0)==5 && !memcmp(data,"ZeedY",5));
    CHECK(close(alias)==0 && close(fd)==0);
    fd=open(path,O_WRONLY|O_APPEND|O_TRUNC);CHECK(fd>=0 && lseek(fd,0,SEEK_CUR)==0 && write(fd,"T",1)==1 && close(fd)==0);
    fd=open(path,O_RDONLY);CHECK(fd>=0 && read(fd,data,sizeof(data))==1 && data[0]=='T');
    alias=dup(fd);CHECK(alias>=0 && fcntl(alias,F_SETFL,O_NONBLOCK)==0 && fcntl(fd,F_GETFL)==O_NONBLOCK);
    CHECK(close(alias)==0 && close(fd)==0);
    CHECK(open(path,O_CREAT|O_EXCL|O_WRONLY,0600)==-1 && errno==EEXIST);
    CHECK(open(path,O_ACCMODE)==-1 && errno==EINVAL);
    pthread_t workers[4];ready=start=winners=0;
    for(unsigned i=0;i<4;i++) CHECK(pthread_create(&workers[i],NULL,create_worker,NULL)==0);
    while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=4) sched_yield();
    __atomic_store_n(&start,1,__ATOMIC_RELEASE);
    for(unsigned i=0;i<4;i++) { void *result=NULL;CHECK(pthread_join(workers[i],&result)==0 && !result); }
    CHECK(winners==1);
    fd=open(exclusive,O_RDONLY);CHECK(fd>=0 && read(fd,data,2)==1 && data[0]=='x' && close(fd)==0);
    CHECK(unlink(path)==0 && unlink(exclusive)==0);
    puts("threadstress: open ok");return 0;
}
