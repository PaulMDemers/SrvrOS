#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/lib/src/desk_file_job.c"
#define JOBS 40
#define WORKERS 64
static struct desk_file_job jobs[JOBS];
static struct desk_file_pool pool;
static struct desk_file_channel channels[2];
struct worker {
    int reader,writer,alive,reaped,eof;
    size_t sent,received,total;
    unsigned char input[sizeof(struct desk_file_request)+DESK_FILE_BYTES];
    unsigned char output[sizeof(struct desk_file_response)+DESK_FILE_BYTES];
};
static struct worker workers[WORKERS];
static int next_fd,spawned,spawn_fail,pipe_fail,fcntl_fail,hold_exit,hold_response,bad_response;
static unsigned calls,admitted[256],admitted_count;
static int defer_nohang,require_all_eof;
static long final_wait_error;
static uint64_t final_wait_pid;
static unsigned blocking_waits;
static int closed[1024];
long srv_signal_config(uint64_t signal,uint64_t action) { assert(signal==SIGPIPE && action==SRV_SIGNAL_IGNORE);return 0; }
long srv_pipe(int fds[2]) {
    if(pipe_fail && --pipe_fail==0)return -1;
    fds[0]=next_fd++;fds[1]=next_fd++;return 0;
}
long srv_fcntl(int fd,int command,uint64_t flags) {
    assert(fd>=100 && command==SRV_F_SETFL && flags==SRV_FD_NONBLOCK);
    return fcntl_fail?-1:0;
}
long srv_spawn_bg_args_fds(const char *path,const char *args,int input,int output) {
    assert(!strcmp(path,"/fat/bin/deskfs") && !args[0]);
    if(spawn_fail)return -1;
    assert(spawned<WORKERS);
    struct worker *w=&workers[spawned++];
    *w=(struct worker){.reader=output-1,.writer=input+1,.alive=1};return 1000+spawned;
}
long srv_close(int fd) {
    assert(fd>=100 && fd<next_fd && !closed[fd]);closed[fd]=1;
    for(int i=0;i<spawned;i++)if(workers[i].writer==fd) {
        workers[i].eof=1;if(!hold_exit)workers[i].alive=0;
    }
    return 0;
}
int desk_process_alive(uint64_t pid) { assert(pid>1000 && pid<=1000+(unsigned)spawned);return workers[pid-1001].alive; }
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) {
    assert(flags==SRV_WAIT_NOHANG || flags==0);struct worker *w=&workers[pid-1001];
    if(flags==SRV_WAIT_NOHANG && defer_nohang)return 0;
    if(!flags) {
        blocking_waits++;
        if(require_all_eof)for(int i=0;i<spawned;i++)assert(workers[i].eof);
        if(final_wait_error && (!final_wait_pid || final_wait_pid==pid)) {long result=final_wait_error;final_wait_error=0;return result;}
    }
    if(w->alive)return 0;
    if(w->reaped)return -1;
    w->reaped=1;*status=0;return pid;
}
static struct worker *by_fd(int fd,int write) {
    assert(!closed[fd]);
    for(int i=0;i<spawned;i++)if((write?workers[i].writer:workers[i].reader)==fd)return &workers[i];
    assert(0);return NULL;
}
long srv_write(int fd,const void *data,size_t bytes) {
    struct worker *w=by_fd(fd,1);if(!w->alive)return -1;
    if(++calls%7==0)return SRV_ERR_AGAIN;
    if(w->total && w->received==w->total)w->sent=w->received=w->total=0;
    if(bytes>17)bytes=17;
    assert(w->sent+bytes<=sizeof(w->input));memcpy(w->input+w->sent,data,bytes);w->sent+=bytes;
    struct desk_file_request r;memcpy(&r,w->input,sizeof(r));
    if(w->sent>=sizeof(r) && w->sent==sizeof(r)+r.length) {
        assert(admitted_count<256);admitted[admitted_count++]=r.id;
        const void *reply=r.operation==DESK_FILE_READ?r.path:NULL;
        unsigned length=r.operation==DESK_FILE_READ?strlen(r.path):0;
        struct desk_file_response response={.magic=DESK_FILE_MAGIC,.id=r.id+(bad_response?1:0),.length=length,.crc=desk_file_crc(reply,length)};
        bad_response=0;memcpy(w->output,&response,sizeof(response));
        if(length)memcpy(w->output+sizeof(response),reply,length);
        w->total=sizeof(response)+length;
    }
    return bytes;
}
long srv_read(int fd,void *data,size_t bytes) {
    struct worker *w=by_fd(fd,0);
    if(hold_response || w->received==w->total || ++calls%11==0)return SRV_ERR_AGAIN;
    if(bytes>13)bytes=13;
    if(bytes>w->total-w->received)bytes=w->total-w->received;
    memcpy(data,w->output+w->received,bytes);w->received+=bytes;return bytes;
}
static void reset(void) {
    memset(jobs,0,sizeof(jobs));memset(workers,0,sizeof(workers));memset(closed,0,sizeof(closed));
    next_fd=100;spawned=spawn_fail=pipe_fail=fcntl_fail=hold_exit=hold_response=bad_response=0;calls=admitted_count=0;
    defer_nohang=require_all_eof=0;final_wait_error=0;final_wait_pid=0;blocking_waits=0;
    assert(!desk_file_pool_init(&pool,channels,2));
}
static void bind(unsigned index) { assert(!desk_file_job_bind(&jobs[index],&pool)); }
static int finish(unsigned index) {
    int result=0;for(int n=0;n<50000 && !result;n++)result=desk_file_job_poll(&jobs[index]);return result;
}
static void retired(void) {
    for(unsigned i=0;i<JOBS;i++)if(jobs[i].pool)assert(!desk_file_job_detach(&jobs[i]));
    assert(desk_file_pool_shutdown(&pool)==1 && !pool.live && pool.spawned==pool.reaped);
    assert(desk_file_pool_shutdown(&pool)==1);
    for(int fd=100;fd<next_fd;fd++)assert(closed[fd]);
}
static int open_state,open_calls;
static struct desk_file_transport prepared;
static int deferred_open(void *context,struct desk_file_channel *channel) {
    assert(context==&prepared && !channel->pid && !channel->owner);open_calls++;
    if(open_state>0) {channel->pid=prepared.pid;channel->reader=prepared.reader;channel->writer=prepared.writer;}
    return open_state;
}
int main(void) {
    reset();for(unsigned i=0;i<4;i++)bind(i);
    for(unsigned i=0;i<3;i++)assert(!desk_file_job_start(&jobs[i],i==1?DESK_FILE_READ:DESK_FILE_LIST,"/queued",NULL,0,0,0));
    assert(!desk_file_job_start(&jobs[3],DESK_FILE_WRITE,"/mutation",NULL,0,0,0));
    uint32_t canceled_id=jobs[1].request.id;
    assert(desk_file_job_cancel_unsubmitted(&jobs[1])==1 && jobs[0].queued_next==&jobs[2] && pool.tail==&jobs[3] && jobs[1].request.id==canceled_id && !jobs[1].busy);
    assert(desk_file_job_cancel_unsubmitted(&jobs[0])==1 && pool.head==&jobs[2]);
    assert(desk_file_job_cancel_unsubmitted(&jobs[2])==1 && pool.head==pool.tail && pool.head==&jobs[3]);
    assert(!desk_file_job_cancel_unsubmitted(&jobs[3]) && jobs[3].busy && !spawned);assert(finish(3)==1);retired();
    reset();bind(0);struct desk_file_probe probe={0};
    assert(!desk_file_job_start(&jobs[0],DESK_FILE_STAT,"/probe",&probe,sizeof(probe),0,0));
    canceled_id=jobs[0].request.id;assert(desk_file_job_cancel_unsubmitted(&jobs[0])==1 && !pool.head && !pool.tail);
    assert(!desk_file_job_start(&jobs[0],DESK_FILE_LIST,"/retry",NULL,0,0,0) && jobs[0].request.id==canceled_id+1);
    jobs[0].sent=1;assert(!desk_file_job_cancel_unsubmitted(&jobs[0]) && jobs[0].busy);jobs[0].sent=0;
    assert(desk_file_job_submit(&jobs[0])>=0 && jobs[0].channel && !desk_file_job_cancel_unsubmitted(&jobs[0]));
    assert(finish(0)==1);retired();
    reset();assert(!desk_file_pool_opener(&pool,deferred_open,&prepared));bind(0);bind(1);
    assert(desk_file_pool_opener(&pool,NULL,NULL)==-1);
    assert(!desk_file_job_start(&jobs[0],DESK_FILE_LIST,"/first",NULL,0,0,0));
    assert(!desk_file_job_start(&jobs[1],DESK_FILE_LIST,"/second",NULL,0,0,0));
    for(unsigned n=0;n<100;n++) {assert(!desk_file_job_submit(&jobs[0]) && !desk_file_job_submit(&jobs[1]));assert(pool.head==&jobs[0] && jobs[0].busy && !spawned && !calls);}
    open_state=-1;assert(desk_file_job_submit(&jobs[0])==-1 && !jobs[0].busy && pool.head==&jobs[1] && !spawned);
    assert(!desk_file_transport_open(&prepared));open_state=1;assert(finish(1)==1 && open_calls>100 && pool.spawned==1 && pool.live==1);
    retired();open_state=open_calls=0;
    reset();bind(0);
    assert(!desk_file_job_start(&jobs[0],DESK_FILE_READ,"/fat/snapshot-reuse",NULL,0,0,0));
    assert(finish(0)==1);
    assert(!desk_file_job_start(&jobs[0],DESK_FILE_WRITE,jobs[0].request.path,jobs[0].incoming,19,0,0));
    assert(!strcmp(jobs[0].request.path,"/fat/snapshot-reuse") && !memcmp(jobs[0].outgoing,"/fat/snapshot-reuse",19));
    assert(finish(0)==1);retired();
    reset();hold_response=1;
    for(unsigned i=0;i<JOBS;i++) {
        bind(i);char value=(char)i;
        assert(!desk_file_job_start(&jobs[i],DESK_FILE_WRITE,"/fat/snapshot",&value,1,0,0));
        value=99;assert(jobs[i].outgoing[0]==i && jobs[i].busy);
    }
    struct srv_gui_wait watch={0};
    assert(desk_file_pool_interests(&pool,&watch)==0 && !watch.count); /* A free lease needs dispatch. */
    assert(!spawned && pool.bound==JOBS && pool.head==jobs && pool.tail==jobs+JOBS-1);
    assert(desk_file_job_submit(&jobs[39])==0 && !spawned); /* FIFO despite reverse polling. */
    for(int i=0;i<100;i++) { (void)desk_file_job_submit(&jobs[0]);(void)desk_file_job_submit(&jobs[1]); }
    assert(spawned==2 && pool.peak==2 && jobs[0].channel && jobs[1].channel);
    assert(!desk_file_job_submit(&jobs[2]) && !jobs[2].channel);
    assert(desk_file_pool_interests(&pool,&watch)==1 && watch.count==2 && watch.fds[0].events==SRV_POLLIN && watch.fds[1].events==SRV_POLLIN);
    size_t sent_before=jobs[0].sent;jobs[0].sent=0;
    assert(desk_file_pool_interests(&pool,&watch)==1 && watch.fds[0].events==SRV_POLLOUT && watch.fds[0].fd==channels[0].writer);
    jobs[0].sent=sent_before;jobs[0].received=sizeof(jobs[0].response);
    assert(desk_file_pool_interests(&pool,&watch)==0); /* Complete header-only reply needs dispatch. */
    jobs[0].received=0;
    assert(desk_file_job_detach(&jobs[0])<0 && desk_file_job_detach(&jobs[39])<0);
    desk_file_job_close(&jobs[39]);assert(jobs[39].busy && jobs[39].pool==&pool);
    assert(desk_file_pool_shutdown(&pool)<0 && !pool.closing && !workers[0].eof);
    assert(desk_file_pool_finish(&pool)<0 && !pool.closing && !workers[0].eof);
    hold_response=0;
    for(unsigned i=0;i<JOBS;i++) {
        assert(finish(i)==1 && !jobs[i].busy && !jobs[i].channel && !jobs[i].pid);
        assert(jobs[i].request.id==i+1 && jobs[i].response.id==i+1);
    }
    assert(admitted_count==JOBS && !pool.head && !pool.tail && spawned==2);
    for(unsigned i=0;i<JOBS;i++)assert(admitted[i]==i+1);
    assert(!desk_file_job_start(&jobs[0],DESK_FILE_READ,"/fat/reused",NULL,0,0,0));
    assert(finish(0)==1 && jobs[0].request.id==JOBS+1 && !memcmp(jobs[0].incoming,"/fat/reused",11));
    assert(jobs[1].response.id==2); /* A sibling's retained reply is independent. */
    retired();

    /* Failed spawn/pipe/flags report on the owning poll, keep queued siblings,
     * then permit an explicit retry. No automatic reissue of the failed write. */
    for(int failure=0;failure<4;failure++) {
        reset();bind(0);bind(1);
        assert(!desk_file_job_start(&jobs[0],DESK_FILE_WRITE,"/fat/failed","x",1,0,0));
        assert(!desk_file_job_start(&jobs[1],DESK_FILE_READ,"/fat/next",NULL,0,0,0));
        spawn_fail=failure==0;pipe_fail=failure==1?1:failure==2?2:0;fcntl_fail=failure==3;
        assert(finish(0)<0 && jobs[0].broken && !jobs[0].busy && !spawned && pool.head==jobs+1);
        spawn_fail=pipe_fail=fcntl_fail=0;
        assert(finish(1)==1 && pool.spawned==1);
        assert(!desk_file_job_start(&jobs[0],DESK_FILE_READ,"/fat/retry",NULL,0,0,0));
        assert(finish(0)==1 && !jobs[0].broken && jobs[0].request.id==3);retired();
    }

    reset();bind(0);bind(1);bad_response=hold_exit=1;
    assert(!desk_file_job_start(&jobs[0],DESK_FILE_WRITE,"/fat/bad","x",1,0,0));
    assert(finish(0)<0 && channels[0].retiring && pool.live==1 && !pool.reaped);
    assert(desk_file_pool_interests(&pool,&watch)==0);
    assert(!desk_file_job_start(&jobs[1],DESK_FILE_READ,"/fat/other",NULL,0,0,0));
    assert(finish(1)==1 && pool.peak==2); /* The poisoned worker still occupies its lease. */
    assert(!desk_file_job_detach(&jobs[0]) && !desk_file_job_detach(&jobs[1]));
    assert(desk_file_pool_shutdown(&pool)==0 && pool.closing);
    assert(desk_file_job_bind(&jobs[0],&pool)<0 && pool.live==2);
    for(int i=0;i<spawned;i++)workers[i].alive=0;
    retired();

    /* A valid complete reply can be drained after worker death. The next job
     * reaps that transport before it starts a replacement. */
    reset();bind(0);bind(1);
    assert(!desk_file_job_start(&jobs[0],DESK_FILE_WRITE,"/fat/done",NULL,0,0,0));
    while(desk_file_job_submit(&jobs[0])!=1) {}
    calls=0;workers[0].alive=0;assert(finish(0)==1);
    assert(!desk_file_job_start(&jobs[1],DESK_FILE_READ,"/fat/new",NULL,0,0,0));
    assert(finish(1)==1 && pool.spawned==2 && pool.reaped==1 && pool.peak==1);retired();

    reset();bind(0);pool.next_id=UINT32_MAX;
    assert(desk_file_job_start(&jobs[0],DESK_FILE_READ,"/fat/wrap",NULL,0,0,0)<0 && !jobs[0].busy && !pool.head);retired();

    /* UI NOHANG calls may be deferred throughout another owner's I/O. Once
     * all jobs detach, the final owner can use kernel blocking admission.
     * EOF must reach both channels before the first wait; EINTR/error retain
     * exact PIDs/live counts, and a partial success is never reaped twice. */
    reset();bind(0);bind(1);hold_response=1;
    assert(!desk_file_job_start(&jobs[0],DESK_FILE_READ,"/fat/a",NULL,0,0,0));
    assert(!desk_file_job_start(&jobs[1],DESK_FILE_READ,"/fat/b",NULL,0,0,0));
    while(desk_file_job_submit(&jobs[0])!=1) {}
    while(desk_file_job_submit(&jobs[1])!=1) {}
    assert(spawned==2);hold_response=0;
    assert(finish(0)==1 && finish(1)==1);
    assert(!desk_file_job_detach(&jobs[0]) && !desk_file_job_detach(&jobs[1]));
    defer_nohang=1;require_all_eof=1;
    for(unsigned i=0;i<32;i++)assert(desk_file_pool_shutdown(&pool)==0 && pool.live==2 && !pool.reaped);
    final_wait_error=SRV_ERR_INTR;
    assert(desk_file_pool_finish(&pool)==0 && pool.live==2 && !pool.reaped && channels[0].pid==1001);
    final_wait_error=-1;
    assert(desk_file_pool_finish(&pool)==-1 && pool.live==2 && !pool.reaped && channels[0].pid==1001);
    final_wait_pid=1002;final_wait_error=SRV_ERR_INTR;
    assert(desk_file_pool_finish(&pool)==0 && pool.live==1 && pool.reaped==1 && !channels[0].pid && channels[1].pid==1002);
    assert(desk_file_pool_finish(&pool)==1 && !pool.live && pool.reaped==2 && blocking_waits==5);
    assert(desk_file_pool_finish(&pool)==1 && blocking_waits==5);
    for(int fd=100;fd<next_fd;fd++)assert(closed[fd]);
    puts("file-pool: PASS 40 FIFO jobs/two lazy workers, snapshots/replies, reuse, refusal, spawn/pipe failure, poison retention/reap, explicit retry and shutdown");
}
