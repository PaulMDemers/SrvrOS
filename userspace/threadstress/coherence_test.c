#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { printf("threadstress: coherence line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static const char *path="/fat/posix-shared-content.tmp";
static unsigned ready,start;
struct record { uint32_t writer,sequence; };
static void *append_worker(void *arg) {
    unsigned id=(unsigned)(uintptr_t)arg;
    int fd=open(path,O_WRONLY|O_APPEND);
    __atomic_add_fetch(&ready,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&start,__ATOMIC_ACQUIRE)) sched_yield();
    if(fd<0) return (void *)1;
    for(unsigned i=0;i<100;i++) {
        struct record record={id,i};
        if(write(fd,&record,sizeof(record))!=sizeof(record)) { close(fd);return (void *)2; }
        sched_yield();
    }
    return close(fd)?(void *)3:NULL;
}
int posix_coherence_test(void) {
    unlink(path);
    int first=open(path,O_RDWR|O_CREAT|O_TRUNC,0600);CHECK(first>=0 && write(first,"seed",4)==4);
    int reader=open(path,O_RDONLY),second=open(path,O_RDWR);CHECK(reader>=0 && second>=0);
    char data[16];struct stat status;
    CHECK(pread(reader,data,16,0)==4 && !memcmp(data,"seed",4));
    CHECK(write(second,"ONE",3)==3 && pread(first,data,16,0)==4 && !memcmp(data,"ONEd",4));
    CHECK(pwrite(first,"two",3,4)==3);
    CHECK(stat(path,&status)==0 && status.st_size==7 && fstat(reader,&status)==0 && status.st_size==7);
    CHECK(lseek(first,0,SEEK_CUR)==4 && lseek(second,0,SEEK_CUR)==3);
    CHECK(ftruncate(second,2)==0 && pread(reader,data,16,0)==2 && !memcmp(data,"ON",2));
    CHECK(ftruncate(first,5)==0 && pread(second,data,16,0)==5 && !memcmp(data,"ON\0\0\0",5));
    CHECK(fsync(reader)==0 && close(first)==0 && close(second)==0);
    CHECK(pread(reader,data,16,0)==5 && !memcmp(data,"ON\0\0\0",5));
    first=open(path,O_WRONLY|O_TRUNC);CHECK(first>=0 && read(reader,data,1)==0 && close(first)==0);
    CHECK(lseek(reader,123,SEEK_SET)==123 && read(reader,data,1)==0 && close(reader)==0);
    pthread_t workers[4];ready=start=0;
    for(unsigned i=0;i<4;i++) CHECK(pthread_create(&workers[i],NULL,append_worker,(void *)(uintptr_t)i)==0);
    while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=4) sched_yield();
    __atomic_store_n(&start,1,__ATOMIC_RELEASE);
    for(unsigned i=0;i<4;i++) { void *result;CHECK(pthread_join(workers[i],&result)==0 && !result); }
    reader=open(path,O_RDONLY);CHECK(reader>=0 && fstat(reader,&status)==0 && status.st_size==400*sizeof(struct record));
    unsigned char seen[4][100]={{0}};
    for(unsigned i=0;i<400;i++) {
        struct record record;CHECK(read(reader,&record,sizeof(record))==sizeof(record));
        CHECK(record.writer<4 && record.sequence<100 && !seen[record.writer][record.sequence]);
        seen[record.writer][record.sequence]=1;
    }
    CHECK(read(reader,data,1)==0 && close(reader)==0 && unlink(path)==0);
    puts("threadstress: coherence ok");return 0;
}
