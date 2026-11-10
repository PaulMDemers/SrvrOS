#include <srvros/desk_capture_job.h>
#include <srvros/sys.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
static void reap(struct desk_capture_job *j) {
    if(j->retiring_pid) {
        uint64_t status;long result=srv_wait(j->retiring_pid,&status,SRV_WAIT_NOHANG);
        if(result==(long)j->retiring_pid || result==-1) j->retiring_pid=0;
    }
}
void desk_capture_job_init(struct desk_capture_job *j) { *j=(struct desk_capture_job){.reader=-1,.writer=-1}; }
int desk_capture_job_trace(struct desk_capture_job *j,int enabled) {
    if(j->busy || j->pid || j->retiring_pid || j->pixels || j->reader>=0 || j->writer>=0)return -1;
    j->transfer.enabled=!!enabled;return 0;
}
static void transfer_bad(struct desk_capture_job *j) {j->transfer.valid=0;j->transfer.invalid=1;}
static void transfer_add(struct desk_capture_job *j,uint64_t *sum,uint64_t value) {
    if(value>UINT64_MAX-*sum)transfer_bad(j);else *sum+=value;
}
static uint64_t transfer_clock(struct desk_capture_job *j) {
    long now=srv_monotonic_us();if(now<=0){transfer_bad(j);return 0;}return (uint64_t)now;
}
static int transfer_return(struct desk_capture_job *j,int result,uint64_t start) {
    if(j->transfer.enabled) {
        uint64_t end=result?j->finished_us:transfer_clock(j);
        if(!end || end<start)transfer_bad(j);
        else transfer_add(j,&j->transfer.poll_us,end-start);
        j->transfer.end_us=end;
    }
    return result;
}
void desk_capture_job_close(struct desk_capture_job *j) {
    uint64_t start=(uint64_t)srv_monotonic_us(),at=start;
    if(j->reader>=0) srv_close(j->reader);
    if(j->writer>=0) srv_close(j->writer);
    j->reader=j->writer=-1;j->busy=0;
    j->close_us+=(uint64_t)srv_monotonic_us()-at;at=(uint64_t)srv_monotonic_us();
    if(j->pixels) munmap(j->pixels,desk_capture_bytes(j->request.width,j->request.height));
    j->unmap_us+=(uint64_t)srv_monotonic_us()-at;at=(uint64_t)srv_monotonic_us();
    j->pixels=NULL;
    if(j->pid) { j->retiring_pid=j->pid;j->pid=0; }
    reap(j);j->reap_us+=(uint64_t)srv_monotonic_us()-at;
    j->cleanup_us+=(uint64_t)srv_monotonic_us()-start;
}
static int spawn(struct desk_capture_job *j) {
    int input[2],output[2];
    if(srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE) || srv_pipe(input)) return -1;
    if(srv_pipe(output)) { srv_close(input[0]);srv_close(input[1]);return -1; }
    j->reader=output[0];j->writer=input[1];
    if(srv_fcntl(j->reader,SRV_F_SETFL,SRV_FD_NONBLOCK)<0 || srv_fcntl(j->writer,SRV_F_SETFL,SRV_FD_NONBLOCK)<0) {
        srv_close(input[0]);srv_close(output[1]);return -1;
    }
    long pid=srv_spawn_bg_args_fds("/fat/bin/shotworker","",input[0],output[1]);
    srv_close(input[0]);srv_close(output[1]);
    if(pid<=0) return -1;
    j->pid=pid;return 0;
}
int desk_capture_job_prepare(struct desk_capture_job *j,uint32_t width,uint32_t height,const char *path) {
    reap(j);uint32_t bytes=desk_capture_bytes(width,height);
    if(j->busy || j->retiring_pid || !bytes || !path || path[0]!='/' || strlen(path)>=DESK_CAPTURE_PATH) return -1;
    uint64_t enabled=j->transfer.enabled;
    j->transfer=(struct desk_capture_transfer_timing){.enabled=enabled,.valid=enabled,
        .total=enabled?sizeof(j->request)+bytes:0};j->transfer_ready_received=0;
    j->request=(struct desk_capture_request){.magic=enabled?DESK_CAPTURE_TRACE_MAGIC:DESK_CAPTURE_MAGIC,.id=++j->next_id,.width=width,.height=height};
    long now=srv_monotonic_us();j->started_us=now>0?(uint64_t)now:0;
    j->finished_us=j->last_progress_us=j->progress_bytes=0;j->phase=j->progress_frames=0;
    memset(j->phase_us,0,sizeof(j->phase_us));
    j->cleanup_us=j->close_us=j->unmap_us=j->reap_us=0;
    strcpy(j->request.path,path);j->response=(struct desk_capture_response){0};j->sent=j->received=0;j->snapshotted=0;
    j->pixels=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(j->pixels==MAP_FAILED) { j->pixels=NULL;return -1; }
    if(spawn(j)) { desk_capture_job_close(j);return -1; }
    j->busy=1;return 0;
}
int desk_capture_job_snapshot(struct desk_capture_job *j,const uint32_t *pixels,uint32_t width,uint32_t height) {
    if(!j->busy || j->snapshotted || !j->pixels || !pixels || j->sent || j->received ||
       j->request.width!=width || j->request.height!=height)return -1;
    memcpy(j->pixels,pixels,desk_capture_bytes(width,height));j->snapshotted=1;return 0;
}
int desk_capture_job_start(struct desk_capture_job *j,const uint32_t *pixels,uint32_t width,uint32_t height,const char *path) {
    if(!pixels || desk_capture_job_prepare(j,width,height,path))return -1;
    return desk_capture_job_snapshot(j,pixels,width,height);
}
int desk_capture_job_poll_transfer(struct desk_capture_job *j) {
    if(!j->busy || !j->snapshotted) return 0;
    struct desk_capture_transfer_timing *t=&j->transfer;
    uint64_t poll_start=0;
    if(t->enabled) {
        poll_start=transfer_clock(j);
        if(t->polls) {
            if(poll_start<t->end_us)transfer_bad(j);
            else {uint64_t gap=poll_start-t->end_us;transfer_add(j,&t->gap_us,gap);if(gap>t->gap_max_us)t->gap_max_us=gap;}
        } else t->begin_us=poll_start;
        transfer_add(j,&t->polls,1);
    }
    size_t total=sizeof(j->request)+desk_capture_bytes(j->request.width,j->request.height);
    unsigned batch=0;
    for(;batch<4 && j->sent<total;batch++) {
        const void *data;size_t bytes;
        if(j->sent<sizeof(j->request)) { data=(char *)&j->request+j->sent;bytes=sizeof(j->request)-j->sent; }
        else { data=(char *)j->pixels+j->sent-sizeof(j->request);bytes=total-j->sent; }
        if(bytes>4096) bytes=4096;
        uint64_t start=0;
        if(t->enabled) {
            start=transfer_clock(j);
            if(t->write_calls) {
                if(start<t->write_last_us)transfer_bad(j);
                else {uint64_t gap=start-t->write_last_us;transfer_add(j,&t->write_gap_us,gap);if(gap>t->write_gap_max_us)t->write_gap_max_us=gap;}
            } else t->write_first_us=start;
            transfer_add(j,&t->write_calls,1);
        }
        long n=srv_write(j->writer,data,bytes);
        if(t->enabled) {
            uint64_t end=transfer_clock(j);
            if(end<start)transfer_bad(j);else transfer_add(j,&t->write_us,end-start);
            t->write_last_us=end;
        }
        if(n==SRV_ERR_AGAIN) {if(t->enabled)transfer_add(j,&t->write_again,1);break;}
        if(n==SRV_ERR_INTR) {if(t->enabled)transfer_add(j,&t->write_intr,1);break;}
        if(n<=0 || (size_t)n>bytes) {if(t->enabled)transfer_add(j,&t->write_failed,1);goto failed;}
        if(t->enabled) {
            transfer_add(j,&t->write_ok,1);transfer_add(j,&t->write_bytes,(uint64_t)n);
            if((size_t)n<bytes)transfer_add(j,&t->short_writes,1);
            if(j->sent<sizeof(j->request) && j->sent+(size_t)n>=sizeof(j->request))t->request_done_us=t->write_last_us;
            if(j->sent+(size_t)n==total)t->pixels_done_us=t->write_last_us;
        }
        j->sent+=n;
    }
    if(t->enabled) {
        if(batch==4 && j->sent<total)transfer_add(j,&t->budget_polls,1);
        transfer_add(j,&t->read_calls,1);
    }
    long n=srv_read(j->reader,(char *)&j->response+j->received,sizeof(j->response)-j->received);
    /* The worker may publish its response and exit between an empty read and
     * any process-status query. Drain the pipe; EOF, not process liveness, is
     * the authoritative end of this one-shot stream. */
    if(n==SRV_ERR_AGAIN) {if(t->enabled)transfer_add(j,&t->read_again,1);return transfer_return(j,0,poll_start);}
    if(n==SRV_ERR_INTR) {if(t->enabled)transfer_add(j,&t->read_intr,1);return transfer_return(j,0,poll_start);}
    if(n<=0 || (size_t)n>sizeof(j->response)-j->received) {if(t->enabled)transfer_add(j,&t->read_failed,1);goto failed;}
    if(t->enabled)transfer_add(j,&t->read_bytes,(uint64_t)n);
    j->received+=n;
    if(j->received<sizeof(j->response)) return transfer_return(j,0,poll_start);
    if(j->response.magic==DESK_CAPTURE_READY_MAGIC) {
        const struct desk_capture_response *r=&j->response;
        if(!t->enabled || j->request.magic!=DESK_CAPTURE_TRACE_MAGIC || j->transfer_ready_received ||
           r->id!=j->request.id || r->status!=CAPTURE_RECEIVE || r->reserved || r->bytes || j->phase!=CAPTURE_RECEIVE)goto failed;
        j->transfer_ready_received=1;t->worker_allocated_us=r->at_us;
        if(!r->at_us || r->at_us<j->last_progress_us)transfer_bad(j);
        /* Auxiliary observation: preserve the existing phase accounting. */
        j->received=0;j->response=(struct desk_capture_response){0};return transfer_return(j,0,poll_start);
    }
    if(j->response.magic==DESK_CAPTURE_PROGRESS_MAGIC) {
        const struct desk_capture_response *r=&j->response;
        if(r->id!=j->request.id || r->reserved || !r->status || r->status>=CAPTURE_PHASE_COUNT ||
           r->status<j->phase || r->bytes>DESK_CAPTURE_MAX_BYTES || r->at_us<j->last_progress_us)goto failed;
        if(t->enabled) {
            if(r->status==CAPTURE_RECEIVE && !t->worker_receive_us) {
                t->worker_receive_us=r->at_us;if(!r->at_us)transfer_bad(j);
            }
            if(r->status==CAPTURE_ENCODE) {
                if(!j->transfer_ready_received)goto failed;
                t->worker_read_done_us=r->at_us;
                if(!r->at_us || r->at_us<t->worker_allocated_us)transfer_bad(j);
            }
        }
        if(j->phase)j->phase_us[j->phase]+=r->at_us-j->last_progress_us;
        j->phase=r->status;j->last_progress_us=r->at_us;j->progress_bytes=r->bytes;j->progress_frames++;
        j->received=0;j->response=(struct desk_capture_response){0};return transfer_return(j,0,poll_start);
    }
    if(j->response.at_us<j->last_progress_us)goto failed;
    if(j->phase)j->phase_us[j->phase]+=j->response.at_us-j->last_progress_us;
    if(j->response.magic!=DESK_CAPTURE_MAGIC || j->response.id!=j->request.id || j->response.reserved || j->response.bytes ||
       j->response.status>DESK_CAPTURE_EXISTS || (!j->response.status && j->sent!=total)) goto failed;
    if(t->enabled) {
        t->worker_final_us=j->response.at_us;
        if(!j->response.at_us || (t->worker_read_done_us && j->response.at_us<t->worker_read_done_us))transfer_bad(j);
    }
    {long now=srv_monotonic_us();j->finished_us=now>0?(uint64_t)now:0;}
    return transfer_return(j,1,poll_start);
failed:
    {long now=srv_monotonic_us();j->finished_us=now>0?(uint64_t)now:0;}
    return transfer_return(j,-1,poll_start);
}

int desk_capture_job_poll(struct desk_capture_job *j) {
    reap(j);int result=desk_capture_job_poll_transfer(j);
    if(result)desk_capture_job_close(j);
    return result;
}
