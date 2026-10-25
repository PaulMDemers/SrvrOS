#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: readiness line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
static uint64_t stamp(int clock_id) {
    struct timespec t;if(clock_gettime(clock_id,&t)) return 0;
    return (uint64_t)t.tv_sec*1000000+(uint64_t)t.tv_nsec/1000;
}
struct channels { int request,response; };
static void *exchange(void *arg) {
    struct channels *c=arg;
    for(unsigned i=0;i<128;i++) {
        unsigned char byte;
        if(read(c->request,&byte,1)!=1 || byte!=(unsigned char)i) return (void *)1;
        if((i&1) && usleep(1000)) return (void *)2;
        if(write(c->response,&byte,1)!=1) return (void *)3;
    }
    return NULL;
}
int posix_readiness_wait_test(void) {
    int data[2],request[2];CHECK(!pipe(data) && !pipe(request));
    struct pollfd interest={.fd=data[0],.events=POLLIN};
    uint64_t wall=stamp(CLOCK_MONOTONIC),cpu=stamp(CLOCK_PROCESS_CPUTIME_ID);
    CHECK(poll(&interest,1,150)==0 && !interest.revents);
    cpu=stamp(CLOCK_PROCESS_CPUTIME_ID)-cpu;wall=stamp(CLOCK_MONOTONIC)-wall;
    printf("threadstress: readiness idle wall_us=%llu cpu_us=%llu\n",(unsigned long long)wall,(unsigned long long)cpu);
    CHECK(wall>=150000 && cpu<wall/2+1000);
    struct channels c={request[0],data[1]};pthread_t worker;
    CHECK(!pthread_create(&worker,NULL,exchange,&c));
    for(unsigned i=0;i<128;i++) {
        unsigned char byte=(unsigned char)i,reply=0;
        CHECK(write(request[1],&byte,1)==1);
        CHECK(poll(&interest,1,1000)==1 && (interest.revents&POLLIN));
        CHECK(read(data[0],&reply,1)==1 && reply==byte);
        CHECK(poll(&interest,1,0)==0 && !interest.revents);
    }
    void *result;CHECK(!pthread_join(worker,&result) && !result);
    CHECK(!close(data[1]));
    CHECK(poll(&interest,1,0)==1 && (interest.revents&POLLHUP));
    CHECK(!close(data[0]) && !close(request[0]) && !close(request[1]));
    puts("threadstress: readiness ok");return 0;
}
