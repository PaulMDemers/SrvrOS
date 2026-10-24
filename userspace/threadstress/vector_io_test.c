#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#define CHECK(x) do { if(!(x)) { printf("threadstress: vector-io line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
struct vector_writer { int fd,tag; };
static void *vector_writer(void *data) {
    struct vector_writer *w=data;
    char record[PIPE_BUF];memset(record,'A'+w->tag,sizeof(record));
    struct iovec v[]={{record,1023},{NULL,0},{record+1023,2048},{record+3071,1025}};
    for(int i=0;i<40;i++) if(writev(w->fd,v,4)!=PIPE_BUF) return (void *)1;
    return NULL;
}
static int read_exact(int fd,char *data,size_t size) {
    size_t n=0;
    while(n<size) { ssize_t r=read(fd,data+n,size-n);if(r<=0) return -1;n+=(size_t)r; }
    return 0;
}
static volatile sig_atomic_t got_signal;
static void handler(int sig) { got_signal=sig; }
struct vector_interrupt { pthread_t reader;int fd,restart; };
static void *interrupt_reader(void *data) {
    struct vector_interrupt *i=data;
    usleep(30000);
    if(pthread_kill(i->reader,SIGUSR1)) return (void *)1;
    if(i->restart) { usleep(30000);if(write(i->fd,"R",1)!=1) return (void *)1; }
    return NULL;
}

int posix_vector_io_test(void) {
    int p[2];char first[2],second[2];
    struct iovec input[]={{first,2},{NULL,0},{second,2}};
    CHECK(!pipe(p) && write(p[1],"ab",2)==2);
    /* The first vector fills, but the still-open writer has no more data.
     * A loop of independent reads would incorrectly block on the second. */
    memset(second,'?',sizeof(second));
    CHECK(readv(p[0],input,3)==2 && !memcmp(first,"ab",2) && second[0]=='?');
    CHECK(readv(p[0],NULL,0)==0 && writev(p[1],NULL,0)==0);
    CHECK(readv(-1,NULL,0)==-1 && errno==EBADF);
    CHECK(writev(p[1],input,-1)==-1 && errno==EINVAL);
    CHECK(readv(p[0],input,IOV_MAX+1)==-1 && errno==EINVAL);
    CHECK(writev(p[1],(const struct iovec *)(uintptr_t)1,1)==-1 && errno==EFAULT);
    struct iovec invalid[]={{first,1},{(void *)(uintptr_t)1,1}};
    CHECK(writev(p[1],invalid,2)==-1 && errno==EFAULT);
    CHECK(readv(p[0],invalid,2)==-1 && errno==EFAULT);
    struct iovec overflow[]={{first,(size_t)INT64_MAX},{first,1}};
    CHECK(writev(p[1],overflow,2)==-1 && errno==EINVAL);
    CHECK(preadv(p[0],input,3,0)==-1 && errno==ESPIPE);
    CHECK(pwritev(p[1],input,3,-1)==-1 && errno==EINVAL);
    CHECK(!fcntl(p[0],F_SETFL,O_NONBLOCK) && read(p[0],first,1)==-1 && errno==EAGAIN);
    CHECK(!fcntl(p[0],F_SETFL,0));

    struct sigaction action={.sa_handler=handler},old_action;
    CHECK(!sigaction(SIGUSR1,&action,&old_action));
    pthread_t worker;
    struct vector_interrupt interrupt={.reader=pthread_self(),.fd=p[1]};
    CHECK(!pthread_create(&worker,NULL,interrupt_reader,&interrupt));
    CHECK(readv(p[0],input,3)==-1 && errno==EINTR && got_signal==SIGUSR1);
    void *result;CHECK(!pthread_join(worker,&result) && !result);
    action.sa_flags=SA_RESTART;got_signal=0;interrupt.restart=1;
    CHECK(!sigaction(SIGUSR1,&action,NULL) && !pthread_create(&worker,NULL,interrupt_reader,&interrupt));
    CHECK(readv(p[0],input,3)==1 && first[0]=='R' && got_signal==SIGUSR1);
    CHECK(!pthread_join(worker,&result) && !result && !sigaction(SIGUSR1,&old_action,NULL));

    static char data[65536],out[65536];memset(data,'x',sizeof(data));
    CHECK(!fcntl(p[1],F_SETFL,O_NONBLOCK));
    CHECK(write(p[1],data,sizeof(data)-PIPE_BUF/2)==sizeof(data)-PIPE_BUF/2);
    struct iovec record[]={{data,PIPE_BUF/2},{data,PIPE_BUF/2}};
    CHECK(writev(p[1],record,2)==-1 && errno==EAGAIN);
    CHECK(!read_exact(p[0],out,sizeof(out)-PIPE_BUF/2));
    CHECK(!memcmp(data,out,sizeof(out)-PIPE_BUF/2));
    CHECK(!fcntl(p[0],F_SETFL,O_NONBLOCK) && read(p[0],out,1)==-1 && errno==EAGAIN);
    CHECK(!fcntl(p[0],F_SETFL,0) && !fcntl(p[1],F_SETFL,0));
    struct vector_writer writers[4];pthread_t tasks[4];unsigned counts[4]={0};
    for(int i=0;i<4;i++) {
        writers[i]=(struct vector_writer){p[1],i};
        CHECK(!pthread_create(&tasks[i],NULL,vector_writer,&writers[i]));
    }
    for(int i=0;i<160;i++) {
        CHECK(!read_exact(p[0],out,PIPE_BUF));
        int tag=out[0]-'A';CHECK(tag>=0 && tag<4);counts[tag]++;
        for(int j=1;j<PIPE_BUF;j++) CHECK(out[j]==out[0]);
    }
    for(int i=0;i<4;i++) CHECK(!pthread_join(tasks[i],&result) && !result && counts[i]==40);
    CHECK(!close(p[0]));
    action=(struct sigaction){.sa_handler=handler};got_signal=0;
    CHECK(!sigaction(SIGPIPE,&action,&old_action));
    CHECK(writev(p[1],record,2)==-1 && errno==EPIPE && got_signal==SIGPIPE);
    CHECK(!sigaction(SIGPIPE,&old_action,NULL) && !close(p[1]));

    const char *path="/fat/vector-io.txt";unlink(path);
    int fd=open(path,O_CREAT|O_TRUNC|O_RDWR,0600);CHECK(fd>=0);
    struct iovec text[]={{"ab",2},{NULL,0},{"cd",2}};
    CHECK(writev(fd,text,3)==4 && lseek(fd,0,SEEK_CUR)==4);
    CHECK(pwritev(fd,text,3,8)==4 && lseek(fd,0,SEEK_CUR)==4);
    CHECK(preadv(fd,input,3,8)==4 && !memcmp(first,"ab",2) && !memcmp(second,"cd",2));
    CHECK(lseek(fd,0,SEEK_CUR)==4);
    CHECK(preadv(fd,invalid,2,0)==-1 && errno==EFAULT && lseek(fd,0,SEEK_CUR)==4);
    CHECK(!fcntl(fd,F_SETFL,O_APPEND) && pwritev(fd,text,3,0)==4);
    CHECK(lseek(fd,0,SEEK_CUR)==4 && !fcntl(fd,F_SETFL,0));

    const size_t size=300000;
    char *large=malloc(size),*check=malloc(size);CHECK(large && check);
    for(size_t i=0;i<size;i++) large[i]=(char)(i*73u+19u);
    struct iovec big[]={{large,150001},{large+150001,size-150001}};
    CHECK(lseek(fd,0,SEEK_SET)==0);
    ssize_t n=writev(fd,big,2);CHECK(n>150001 && n<(ssize_t)size);
    struct iovec tail={large+n,size-(size_t)n};
    CHECK(writev(fd,&tail,1)==(ssize_t)tail.iov_len && lseek(fd,0,SEEK_CUR)==(off_t)size);
    CHECK(lseek(fd,0,SEEK_SET)==0);
    struct iovec big_in[]={{check,150001},{check+150001,size-150001}};
    ssize_t r=readv(fd,big_in,2);CHECK(r==n);
    tail=(struct iovec){check+r,size-(size_t)r};
    CHECK(readv(fd,&tail,1)==(ssize_t)tail.iov_len && !memcmp(large,check,size));
    CHECK(readv(fd,big_in,2)==0);
    free(large);free(check);CHECK(!close(fd) && !unlink(path));
    puts("threadstress: vector-io ok");return 0;
}
