#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { printf("threadstress: stdio-registry line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
struct stream_writer { int fd,tag; };
static unsigned finished,flushes,flusher_ready;
static void *flush_worker(void *unused) {
    (void)unused;__atomic_store_n(&flusher_ready,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&finished,__ATOMIC_ACQUIRE)) {
        if(fflush(NULL)) return (void *)1;
        __atomic_add_fetch(&flushes,1,__ATOMIC_RELAXED);sched_yield();
    }
    return NULL;
}
static void *stream_worker(void *data) {
    struct stream_writer *w=data;char record[64];memset(record,'A'+w->tag,sizeof(record));
    for(unsigned i=0;i<100;i++) {
        int fd=dup(w->fd);if(fd<0) return (void *)1;
        FILE *s=fdopen(fd,"w");if(!s) { close(fd);return (void *)2; }
        flockfile(s);flockfile(s);
        if(fwrite(record,1,sizeof(record),s)!=sizeof(record)) { fclose(s);return (void *)3; }
        /* A global flush can pin this stream and block behind our explicit
         * lock. Closing must retire it without waiting for that blocked pin. */
        usleep(1000);
        if(fclose(s)) return (void *)4;
    }
    return NULL;
}
int posix_stdio_registry_test(void) {
    int pipes[3][2];pthread_t workers[3],flusher;struct stream_writer writers[3];
    finished=flushes=flusher_ready=0;
    CHECK(!pthread_create(&flusher,NULL,flush_worker,NULL));
    while(!__atomic_load_n(&flusher_ready,__ATOMIC_ACQUIRE)) sched_yield();
    for(int i=0;i<3;i++) { CHECK(!pipe(pipes[i]));writers[i]=(struct stream_writer){pipes[i][1],i};CHECK(!pthread_create(&workers[i],NULL,stream_worker,&writers[i])); }
    void *result;
    for(int i=0;i<3;i++) CHECK(!pthread_join(workers[i],&result) && !result);
    __atomic_store_n(&finished,1,__ATOMIC_RELEASE);
    CHECK(!pthread_join(flusher,&result) && !result && flushes>0);
    char text[6400];
    for(int i=0;i<3;i++) {
        CHECK(!close(pipes[i][1]));size_t done=0;
        while(done<sizeof(text)) { ssize_t n=read(pipes[i][0],text+done,sizeof(text)-done);CHECK(n>0);done+=(size_t)n; }
        for(size_t j=0;j<sizeof(text);j++) CHECK(text[j]=='A'+i);
        CHECK(read(pipes[i][0],text,1)==0 && !close(pipes[i][0]));
    }
    CHECK(!fflush(NULL));puts("threadstress: stdio-registry ok");return 0;
}
