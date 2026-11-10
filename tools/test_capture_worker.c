#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <srvros/sys.h>
#include <srvros/desk_capture_job.h>

static unsigned maps,unmaps,spawned,reaped,max_write,map_fail;
static unsigned snapshot_hold,snapshot_entered,snapshot_release;
/* A deterministic64KiB simple-pipe admission model. Host poll commonly asks
 * for a whole PIPE_BUF of free space, masking SrvrOS's any-free-byte POLLOUT.
 * Stage initial bytes while the peer is gated, then forward that exact prefix
 * to the real host pipe before allowing ordinary transport to resume. */
static pthread_mutex_t false_ready_lock=PTHREAD_MUTEX_INITIALIZER;
static unsigned false_ready_mode,false_ready_sleeps,false_ready_again,false_ready_probes;
static int false_ready_writer=-1;
static size_t false_ready_used;
static unsigned char false_ready_bytes[65536];
static pthread_mutex_t event_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t event_changed=PTHREAD_COND_INITIALIZER;
struct peer_control {unsigned request_seen,ready,accepted,persisted,final,allow_header,allow_read,allow_persist,allow_exit;};
static struct peer_control *peer;
static unsigned load(const unsigned *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void store(unsigned *p,unsigned v){__atomic_store_n(p,v,__ATOMIC_RELEASE);}
long srv_monotonic_us(void) {struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (long)t.tv_sec*1000000+t.tv_nsec/1000;}
long srv_futex_wait(uint32_t *word,uint32_t expected,uint64_t timeout) {
    assert(!timeout);assert(!pthread_mutex_lock(&event_lock));
    while(__atomic_load_n(word,__ATOMIC_ACQUIRE)==expected)assert(!pthread_cond_wait(&event_changed,&event_lock));
    assert(!pthread_mutex_unlock(&event_lock));return 0;
}
long srv_futex_wake(uint32_t *word,uint64_t count) {
    (void)word;assert(count==1);assert(!pthread_mutex_lock(&event_lock));
    assert(!pthread_cond_broadcast(&event_changed));assert(!pthread_mutex_unlock(&event_lock));return 1;
}
long srv_sleep_ticks(uint64_t ticks) {
    if(load(&false_ready_mode)==1)__atomic_add_fetch(&false_ready_sleeps,1,__ATOMIC_RELEASE);
    usleep(ticks*10000);return 0;
}
long srv_signal_config(uint64_t signal_number,uint64_t action) {(void)signal_number;(void)action;signal(SIGPIPE,SIG_IGN);return 0;}
long srv_pipe(int fds[2]) {return pipe(fds);}
long srv_fcntl(int fd,int command,uint64_t value) {assert(command==SRV_F_SETFL && value==SRV_FD_NONBLOCK);return fcntl(fd,F_SETFL,O_NONBLOCK);}
long srv_close(int fd) {return close(fd);}
static long io_result(ssize_t n) {return n<0?(errno==EAGAIN?SRV_ERR_AGAIN:errno==EINTR?SRV_ERR_INTR:-1):n;}
long srv_write(int fd,const void *data,size_t length) {
    assert(length<=4096);unsigned previous=load(&max_write);while(previous<length && !__atomic_compare_exchange_n(&max_write,&previous,(unsigned)length,0,__ATOMIC_RELEASE,__ATOMIC_ACQUIRE)){}
    if(load(&false_ready_mode)) {
        assert(!pthread_mutex_lock(&false_ready_lock));
        if(load(&false_ready_mode)==1) {
            if(false_ready_writer<0)false_ready_writer=fd;
            assert(false_ready_writer==fd);
            if(length>sizeof(false_ready_bytes)-false_ready_used) {
                __atomic_add_fetch(&false_ready_again,1,__ATOMIC_RELEASE);
                assert(!pthread_mutex_unlock(&false_ready_lock));return SRV_ERR_AGAIN;
            }
            memcpy(false_ready_bytes+false_ready_used,data,length);false_ready_used+=length;
            assert(!pthread_mutex_unlock(&false_ready_lock));return (long)length;
        }
        assert(!pthread_mutex_unlock(&false_ready_lock));
    }
    return io_result(write(fd,data,length));
}
long srv_read(int fd,void *data,size_t length) {assert(length<=32);return io_result(read(fd,data,length));}
long srv_poll(struct srv_pollfd *fds,uint64_t count,int64_t timeout) {
    assert(count>0 && count<=2 && (timeout==0 || timeout==10));struct pollfd host[2];
    if(load(&false_ready_mode)) {
        assert(!pthread_mutex_lock(&false_ready_lock));
        if(load(&false_ready_mode)==1) {
            int n=0;
            for(unsigned i=0;i<count;i++) {
                fds[i].revents=0;
                if(fds[i].fd==false_ready_writer && (fds[i].events&SRV_POLLOUT) &&
                   false_ready_used<sizeof(false_ready_bytes)) {fds[i].revents=SRV_POLLOUT;n++;}
            }
            if(timeout==0 && n)__atomic_add_fetch(&false_ready_probes,1,__ATOMIC_RELEASE);
            assert(!pthread_mutex_unlock(&false_ready_lock));return n;
        }
        assert(!pthread_mutex_unlock(&false_ready_lock));
    }
    for(unsigned i=0;i<count;i++)host[i]=(struct pollfd){.fd=fds[i].fd,.events=fds[i].events};
    int n=poll(host,(nfds_t)count,(int)timeout);
    for(unsigned i=0;i<count;i++)fds[i].revents=host[i].revents;
    return io_result(n);
}
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) {
    int child_status=0;pid_t n=waitpid((pid_t)pid,&child_status,flags==SRV_WAIT_NOHANG?WNOHANG:0);
    if(n>0){__atomic_add_fetch(&reaped,1,__ATOMIC_RELEASE);*status=(uint64_t)child_status;}
    return n<0?(errno==EINTR?SRV_ERR_INTR:-1):n;
}
static void *capture_map(void *address,size_t length,int prot,int flags,int fd,off_t offset) {
    if(load(&map_fail)){errno=ENOMEM;return MAP_FAILED;}
    void *p=mmap(address,length,prot,flags,fd,offset);if(p!=MAP_FAILED)__atomic_add_fetch(&maps,1,__ATOMIC_RELEASE);return p;
}
static int capture_unmap(void *p,size_t length) {__atomic_add_fetch(&unmaps,1,__ATOMIC_RELEASE);return munmap(p,length);}
#define mmap capture_map
#define munmap capture_unmap
#define desk_capture_job_snapshot actual_capture_snapshot
#include "../userspace/lib/src/desk_capture_job.c"
#undef desk_capture_job_snapshot
#undef mmap
#undef munmap
int desk_capture_job_snapshot(struct desk_capture_job *job,const uint32_t *pixels,uint32_t width,uint32_t height) {
    if(load(&snapshot_hold)) {
        store(&snapshot_entered,1);
        while(!load(&snapshot_release))usleep(1000);
    }
    return actual_capture_snapshot(job,pixels,width,height);
}
#include "../userspace/deskd/capture_worker.h"

static int child_transfer(int fd,void *p,size_t length,int writing) {
    while(length){ssize_t n=writing?write(fd,p,length):read(fd,p,length);if(n<0 && errno==EINTR)continue;if(n<=0)return -1;p=(char *)p+n;length-=n;}return 0;
}
static void child_frame(int fd,uint32_t magic,uint32_t id,unsigned phase,uint64_t bytes) {
    struct desk_capture_response r={.magic=magic,.id=id,.status=phase,.at_us=(uint64_t)srv_monotonic_us(),.bytes=bytes};
    if(child_transfer(fd,&r,sizeof(r),1))_exit(4);
}
static void child_main(int input,int output) {
    for(int fd=3;fd<1024;fd++)if(fd!=input && fd!=output)close(fd);
    while(!load(&peer->allow_header))usleep(1000);
    struct desk_capture_request q;if(child_transfer(input,&q,sizeof(q),0))_exit(0);
    if(q.magic!=DESK_CAPTURE_TRACE_MAGIC || !q.id || strcmp(q.path,"/proof.bmp"))_exit(5);
    store(&peer->request_seen,1);
    child_frame(output,DESK_CAPTURE_PROGRESS_MAGIC,q.id,CAPTURE_RECEIVE,0);
    child_frame(output,DESK_CAPTURE_READY_MAGIC,q.id,CAPTURE_RECEIVE,0);store(&peer->ready,1);
    while(!load(&peer->allow_read))usleep(1000);
    unsigned char buffer[4096];size_t length=desk_capture_bytes(q.width,q.height),offset=0;
    const unsigned char pattern[4]={0x78,0x56,0x34,0x12};
    while(offset<length) {
        size_t n=length-offset;if(n>sizeof(buffer))n=sizeof(buffer);
        if(child_transfer(input,buffer,n,0))_exit(0);
        for(size_t i=0;i<n;i++)if(buffer[i]!=pattern[(offset+i)%4])_exit(6);
        offset+=n;
    }
    child_frame(output,DESK_CAPTURE_PROGRESS_MAGIC,q.id,CAPTURE_ENCODE,length);
    store(&peer->accepted,1);
    while(!load(&peer->allow_persist))usleep(1000);
    store(&peer->persisted,1);
    child_frame(output,DESK_CAPTURE_PROGRESS_MAGIC,q.id,CAPTURE_RELEASE,length);
    child_frame(output,DESK_CAPTURE_MAGIC,q.id,DESK_CAPTURE_OK,0);store(&peer->final,1);
    while(!load(&peer->allow_exit))usleep(1000);
    _exit(0);
}
long srv_spawn_bg_args_fds(const char *program,const char *arguments,int input,int output) {
    assert(!strcmp(program,"/fat/bin/shotworker") && !*arguments);pid_t pid=fork();assert(pid>=0);
    if(!pid)child_main(input,output);
    __atomic_add_fetch(&spawned,1,__ATOMIC_RELEASE);return pid;
}
static void await_value(const unsigned *p,unsigned value) {
    long until=srv_monotonic_us()+3000000;
    while(load(p)!=value){assert(srv_monotonic_us()<until);usleep(1000);}
}
static void await_state(struct capture_worker *w,unsigned state) {await_value(&w->state,state);}
static void reset_peer(void) {*peer=(struct peer_control){.allow_header=1,.allow_read=1,.allow_persist=1,.allow_exit=1};}
static void submit(struct capture_worker *w,struct desk_capture_job *view,unsigned width,unsigned height) {
    desk_capture_job_init(view);assert(!desk_capture_job_trace(view,1));
    assert(capture_owner_submit(w,view,width,height,"/proof.bmp") && view->busy && !view->pixels && view->reader==-1);
    assert(!capture_owner_submit(w,view,width,height,"/proof.bmp"));
    await_state(w,CAPTURE_OWNER_PREPARED);
}
static void snapshot(struct capture_worker *w,struct desk_capture_job *view,unsigned width,unsigned height) {
    size_t bytes=desk_capture_bytes(width,height);uint32_t *pixels=malloc(bytes);assert(pixels);
    for(size_t i=0;i<bytes/4;i++)pixels[i]=0x12345678;
    assert(capture_owner_snapshot(w,view,pixels,width,height)==1);
    memset(pixels,0,bytes);free(pixels);assert(!view->pixels && view->reader==-1 && view->busy);
}
static int take(struct capture_worker *w,struct desk_capture_job *view) {
    await_state(w,CAPTURE_OWNER_DONE);uint64_t pid=0;int n=capture_owner_poll(w,view,&pid);
    assert(pid && !view->pid && !view->retiring_pid && !view->busy && !view->pixels && view->reader==-1 && view->writer==-1);
    assert(!capture_owner_poll(w,view,&pid));return n;
}
static unsigned unrelated_release,unrelated_complete;
static void *unrelated_work(void *unused) {
    (void)unused;while(!load(&unrelated_release))usleep(1000);store(&unrelated_complete,1);return NULL;
}
static void delayed_ui_and_unrelated_work(void) {
    reset_peer();struct capture_worker w;struct desk_capture_job view;assert(!capture_owner_start(&w));
    pthread_t unrelated;assert(!pthread_create(&unrelated,NULL,unrelated_work,NULL));
    submit(&w,&view,1366,768);snapshot(&w,&view,1366,768);
    /* No UI poll is executed while the actual job/pipe peer completes. */
    await_value(&peer->accepted,1);await_state(&w,CAPTURE_OWNER_DONE);assert(!load(&unrelated_complete));
    assert(take(&w,&view)==1 && view.transfer.valid && !view.transfer.invalid && view.transfer.write_ok==1026 &&
        view.transfer.write_bytes==4196528 && view.transfer.write_failed==0 && load(&max_write)==4096);
    assert(!capture_owner_stop(&w));store(&unrelated_release,1);assert(!pthread_join(unrelated,NULL));
}
static void false_ready_atomic_write(void) {
    reset_peer();store(&peer->allow_header,0);
    false_ready_used=0;false_ready_writer=-1;
    store(&false_ready_sleeps,0);store(&false_ready_again,0);store(&false_ready_probes,0);store(&false_ready_mode,1);
    struct capture_worker w;struct desk_capture_job view;assert(!capture_owner_start(&w));
    submit(&w,&view,1366,768);snapshot(&w,&view,1366,768);
    long until=srv_monotonic_us()+3000000;
    while(load(&false_ready_sleeps)<5){assert(srv_monotonic_us()<until);usleep(1000);}
    assert(!pthread_mutex_lock(&false_ready_lock));
    assert(false_ready_used==sizeof(struct desk_capture_request)+15*4096 &&
        sizeof(false_ready_bytes)-false_ready_used==3920 && !load(&peer->request_seen));
    unsigned sleeps=load(&false_ready_sleeps),again=load(&false_ready_again),probes=load(&false_ready_probes);
    /* One AGAIN can end the last productive admission; every subsequent
     * immediate-ready/no-progress admission must take a bounded backoff. */
    assert(sleeps>=5 && probes>=sleeps && probes<=sleeps+1 && again>=sleeps && again<=sleeps+2);
    store(&false_ready_mode,2);store(&peer->allow_header,1);
    size_t offset=0;
    while(offset<false_ready_used) {
        ssize_t n=write(false_ready_writer,false_ready_bytes+offset,false_ready_used-offset);
        if(n<0 && (errno==EAGAIN || errno==EINTR)) {
            struct pollfd fd={.fd=false_ready_writer,.events=POLLOUT};
            assert(poll(&fd,1,10)>=0);assert(srv_monotonic_us()<until);continue;
        }
        assert(n>0);offset+=(size_t)n;
    }
    store(&false_ready_mode,0);assert(!pthread_mutex_unlock(&false_ready_lock));
    assert(take(&w,&view)==1 && view.transfer.valid && view.transfer.write_ok==1026 &&
        view.transfer.write_bytes==4196528 && view.transfer.write_again>=again && load(&peer->persisted));
    assert(!capture_owner_stop(&w));
    printf("capture-owner-false-ready: PASS prefix=%zu free=3920 no_progress_again=%u immediate_probes=%u tick_backoffs=%u full_input=%llu\n",
        offset,again,probes,sleeps,(unsigned long long)view.transfer.write_bytes);
}
static void prepared_and_partial_cancel(void) {
    reset_peer();struct capture_worker w;struct desk_capture_job view;assert(!capture_owner_start(&w));
    submit(&w,&view,64,64);capture_owner_cancel(&w);assert(take(&w,&view)==-1 && !load(&peer->accepted));
    assert(!capture_owner_stop(&w));
    reset_peer();store(&peer->allow_read,0);assert(!capture_owner_start(&w));
    submit(&w,&view,1366,768);snapshot(&w,&view,1366,768);await_value(&peer->ready,1);
    capture_owner_cancel(&w);await_state(&w,CAPTURE_OWNER_RETIRING);assert(!load(&peer->accepted));
    uint64_t pid=0;assert(!capture_owner_poll(&w,&view,&pid) && view.busy && !view.pixels && view.reader==-1);
    store(&peer->allow_read,1);assert(take(&w,&view)==-1 && !load(&peer->accepted));assert(!capture_owner_stop(&w));
}
static void accepted_cancel_and_buffered_final(void) {
    reset_peer();store(&peer->allow_persist,0);struct capture_worker w;struct desk_capture_job view;assert(!capture_owner_start(&w));
    submit(&w,&view,64,64);snapshot(&w,&view,64,64);await_value(&peer->accepted,1);capture_owner_cancel(&w);
    usleep(20000);assert(capture_owner_state(&w)==CAPTURE_OWNER_TRANSFERRING && !load(&peer->persisted));
    store(&peer->allow_persist,1);assert(take(&w,&view)==1 && load(&peer->persisted));assert(!capture_owner_stop(&w));
    reset_peer();store(&peer->allow_exit,0);assert(!capture_owner_start(&w));
    submit(&w,&view,64,64);snapshot(&w,&view,64,64);await_value(&peer->final,1);await_state(&w,CAPTURE_OWNER_RETIRING);
    uint64_t pid=0;assert(!capture_owner_poll(&w,&view,&pid) && view.busy);
    store(&peer->allow_exit,1);assert(take(&w,&view)==1);assert(!capture_owner_stop(&w));
}
struct stop_request {struct capture_worker *w;unsigned done;};
static void *stop_owner(void *context) {struct stop_request *s=context;assert(!capture_owner_stop(s->w));store(&s->done,1);return NULL;}
static void stop_joins_and_failure(void) {
    reset_peer();struct capture_worker w;struct desk_capture_job view;assert(!capture_owner_start(&w));
    submit(&w,&view,64,64);assert(!capture_owner_stop(&w));assert(take(&w,&view)==-1);
    reset_peer();store(&peer->allow_persist,0);assert(!capture_owner_start(&w));
    submit(&w,&view,64,64);snapshot(&w,&view,64,64);await_value(&peer->accepted,1);
    struct stop_request stop={.w=&w};pthread_t joining;assert(!pthread_create(&joining,NULL,stop_owner,&stop));
    usleep(20000);assert(!load(&stop.done));store(&peer->allow_persist,1);assert(!pthread_join(joining,NULL));
    assert(load(&stop.done) && take(&w,&view)==1 && load(&peer->persisted));
    reset_peer();assert(!capture_owner_start(&w));desk_capture_job_init(&view);store(&map_fail,1);
    assert(capture_owner_submit(&w,&view,64,64,"/proof.bmp"));await_state(&w,CAPTURE_OWNER_DONE);
    uint64_t pid=99;assert(capture_owner_poll(&w,&view,&pid)==-1 && !pid && !view.busy && !view.pixels);
    store(&map_fail,0);assert(!capture_owner_stop(&w));
}
struct snapshot_request {struct capture_worker *w;struct desk_capture_job *view;};
static void *copy_snapshot(void *context) {
    struct snapshot_request *r=context;snapshot(r->w,r->view,64,64);return NULL;
}
static void cancel_during_snapshot(void) {
    for(unsigned stopping=0;stopping<2;stopping++) {
        reset_peer();struct capture_worker w;struct desk_capture_job view;assert(!capture_owner_start(&w));
        submit(&w,&view,64,64);store(&snapshot_hold,1);store(&snapshot_entered,0);store(&snapshot_release,0);
        struct snapshot_request request={.w=&w,.view=&view};pthread_t copying;assert(!pthread_create(&copying,NULL,copy_snapshot,&request));
        await_value(&snapshot_entered,1);assert(capture_owner_state(&w)==CAPTURE_OWNER_SNAPSHOTTING);
        struct stop_request stop={.w=&w};pthread_t joining;
        unsigned old_unmaps=load(&unmaps);
        if(stopping)assert(!pthread_create(&joining,NULL,stop_owner,&stop));else capture_owner_cancel(&w);
        usleep(20000);assert(capture_owner_state(&w)==CAPTURE_OWNER_SNAPSHOTTING && load(&unmaps)==old_unmaps && !load(&stop.done));
        store(&snapshot_release,1);assert(!pthread_join(copying,NULL));store(&snapshot_hold,0);
        if(stopping)assert(!pthread_join(joining,NULL));
        assert(take(&w,&view)==-1 && !load(&peer->accepted) && load(&unmaps)==old_unmaps+1);
        if(!stopping)assert(!capture_owner_stop(&w));
    }
}
static unsigned fd_count(void) {
    unsigned n=0;for(int fd=0;fd<1024;fd++)if(fcntl(fd,F_GETFD)>=0 || errno!=EBADF)n++;return n;
}
int main(void) {
    unsigned fds=fd_count();peer=mmap(NULL,sizeof(*peer),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);assert(peer!=MAP_FAILED);
    delayed_ui_and_unrelated_work();false_ready_atomic_write();prepared_and_partial_cancel();accepted_cancel_and_buffered_final();stop_joins_and_failure();cancel_during_snapshot();
    assert(load(&maps)==load(&unmaps) && load(&spawned)==load(&reaped) && fd_count()==fds);assert(!munmap(peer,sizeof(*peer)));
    puts("capture-owner: PASS real job/pipes/threads, full Air immutable input independent of delayed UI/unrelated worker, any-space false-ready atomic-write backoff, prepared/partial/accepted cancellation, cancel/stop during snapshot, buffered final/exit, stop joins, allocation failure and exact resource retirement");
}
