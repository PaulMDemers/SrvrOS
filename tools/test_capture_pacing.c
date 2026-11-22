#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/lib/src/desk_capture_job.c"

/* Actual sender, deterministic reader and a bounded64KiB input pipe. No wall
 * sleeps or filesystem stand-ins: this fixture proves transfer/cadence and
 * ownership mechanics, not native saving or hardware performance. */
enum { WIDTH=1366,HEIGHT=768,PIXELS=WIDTH*HEIGHT,PIXEL_BYTES=PIXELS*4,
    PIPE_BYTES=65536,STREAM_BYTES=PIXEL_BYTES+sizeof(struct desk_capture_request),
    MAX_FRAMES=16 };
static struct desk_capture_job job;
static uint32_t canvas[PIXELS];
static unsigned char accepted[STREAM_BYTES],consumed[STREAM_BYTES],baseline[STREAM_BYTES];
static unsigned char input_pipe[PIPE_BYTES],output_pipe[MAX_FRAMES*sizeof(struct desk_capture_response)];
static size_t input_at,input_size,accepted_size,consumed_size,output_at,output_size;
static uint64_t now_us,period_us,clock_reads,input_hash,control_hash;
static unsigned polls,send_polls,poll_writes,max_poll_writes,write_calls,read_calls;
static unsigned read_ok,read_again_count,read_intr_count,read_failed_count;
static uint64_t read_bytes;
static unsigned pipes,closes,wait_calls,again_count,intr_count,short_count;
static unsigned fragment,read_fragment,read_faults,write_faults,blocked,write_pipe_error;
static unsigned force_eof,hold_final,worker_reap_ready,worker_received,worker_accepted;
static unsigned reader_closed,writer_closed,header_seen,body_seen,final_queued;
static const uint32_t trace_magic=0x32544853u,ready_magic=0x32524853u;

static void mix(uint64_t *hash,uint64_t value) {
    for(unsigned i=0;i<8;i++) {*hash^=(unsigned char)(value>>(i*8));*hash*=1099511628211ull;}
}
static void frame(uint32_t magic,unsigned status,uint64_t bytes) {
    assert(output_size+sizeof(struct desk_capture_response)<=sizeof(output_pipe));
    struct desk_capture_response response={.magic=magic,.id=job.request.id,
        .status=status,.at_us=now_us,.bytes=bytes};
    memcpy(output_pipe+output_size,&response,sizeof(response));output_size+=sizeof(response);
}
static void queue_final(void) {
    assert(worker_accepted && !final_queued);
    frame(DESK_CAPTURE_MAGIC,DESK_CAPTURE_OK,0);final_queued=1;
}
static void reader_drain(void) {
    if(blocked)return;
    while(input_size) {
        assert(consumed_size<sizeof(consumed));
        consumed[consumed_size++]=input_pipe[input_at];
        input_at=(input_at+1)%sizeof(input_pipe);input_size--;
        if(!header_seen && consumed_size==sizeof(struct desk_capture_request)) {
            struct desk_capture_request request;memcpy(&request,consumed,sizeof(request));
            assert((request.magic==DESK_CAPTURE_MAGIC || request.magic==trace_magic) &&
                request.id==job.request.id && request.width==WIDTH && request.height==HEIGHT);
            assert(!strcmp(request.path,"/fat/home/Screenshots/full-size.bmp"));
            header_seen=worker_received=1;
            frame(DESK_CAPTURE_PROGRESS_MAGIC,CAPTURE_RECEIVE,0);
            if(request.magic==trace_magic)frame(ready_magic,CAPTURE_RECEIVE,0);
        }
        if(!body_seen && consumed_size==sizeof(consumed)) {
            body_seen=worker_accepted=1;
            /* A burst is intentionally buffered: the actual sender must drain
             * validated progress and final frames rather than infer liveness. */
            frame(DESK_CAPTURE_PROGRESS_MAGIC,CAPTURE_ENCODE,PIXEL_BYTES);
            frame(DESK_CAPTURE_PROGRESS_MAGIC,CAPTURE_WRITE,PIXEL_BYTES+54);
            frame(DESK_CAPTURE_PROGRESS_MAGIC,CAPTURE_TEMP_VERIFY,PIXEL_BYTES+54);
            if(!hold_final)queue_final();
        }
    }
}
long srv_monotonic_us(void) {clock_reads++;return (long)now_us;}
long srv_signal_config(uint64_t signal,uint64_t action) {
    assert(signal==SIGPIPE && action==SRV_SIGNAL_IGNORE);mix(&control_hash,1);return 0;
}
long srv_pipe(int pair[2]) {
    assert(pipes<2);pair[0]=pipes?10:12;pair[1]=pipes?13:11;pipes++;
    mix(&control_hash,2);return 0;
}
long srv_fcntl(int fd,int command,uint64_t value) {
    assert((fd==10 || fd==11) && command==SRV_F_SETFL && value==SRV_FD_NONBLOCK);
    mix(&control_hash,3);mix(&control_hash,(uint64_t)fd);return 0;
}
long srv_spawn_bg_args_fds(const char *path,const char *args,int input,int output) {
    assert(!strcmp(path,"/fat/bin/shotworker") && !*args && input==12 && output==13);
    mix(&control_hash,4);return 42;
}
long srv_close(int fd) {
    assert(fd>=10 && fd<=13);closes++;mix(&control_hash,5);mix(&control_hash,(uint64_t)fd);
    if(fd==10) {assert(!reader_closed);reader_closed=1;}
    if(fd==11) {assert(!writer_closed);writer_closed=1;}
    return 0;
}
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) {
    assert(pid==42 && flags==SRV_WAIT_NOHANG);wait_calls++;*status=0;
    mix(&control_hash,6);return worker_reap_ready?42:0;
}
long srv_write(int fd,const void *bytes,size_t size) {
    assert(fd==11 && !writer_closed && size && size<=4096);
    write_calls++;poll_writes++;assert(poll_writes<=4);
    mix(&input_hash,size);
    long result;
    if(write_pipe_error)result=SRV_ERR_PIPE;
    else if(write_faults && write_calls%13==0) {intr_count++;result=SRV_ERR_INTR;}
    else if(write_faults && write_calls%7==0) {again_count++;result=SRV_ERR_AGAIN;}
    else if(size>sizeof(input_pipe)-input_size) {again_count++;result=SRV_ERR_AGAIN;}
    else {
        size_t count=size;if(fragment && count>fragment)count=fragment;
        if(count<size)short_count++;
        assert(accepted_size+count<=sizeof(accepted));
        memcpy(accepted+accepted_size,bytes,count);accepted_size+=count;
        for(size_t i=0;i<count;i++)input_pipe[(input_at+input_size+i)%sizeof(input_pipe)]=((const unsigned char *)bytes)[i];
        input_size+=count;assert(input_size<=sizeof(input_pipe));reader_drain();result=(long)count;
    }
    mix(&input_hash,(uint64_t)result);return result;
}
long srv_read(int fd,void *bytes,size_t size) {
    assert(fd==10 && !reader_closed && size && size<=sizeof(struct desk_capture_response));read_calls++;
    if(force_eof) {read_failed_count++;return 0;}
    if(read_faults && read_calls%11==0) {read_intr_count++;return SRV_ERR_INTR;}
    if(read_faults && read_calls%7==0) {read_again_count++;return SRV_ERR_AGAIN;}
    if(output_at==output_size) {read_again_count++;return SRV_ERR_AGAIN;}
    if(size>output_size-output_at)size=output_size-output_at;
    if(read_fragment && size>read_fragment)size=read_fragment;
    memcpy(bytes,output_pipe+output_at,size);output_at+=size;
    read_ok++;read_bytes+=size;return (long)size;
}
static void reset(unsigned traced) {
    assert(!job.busy && !job.pixels && !job.retiring_pid);
    desk_capture_job_init(&job);
    input_at=input_size=accepted_size=consumed_size=output_at=output_size=0;
    now_us=1000000;period_us=10000;clock_reads=0;
    input_hash=control_hash=1469598103934665603ull;
    polls=send_polls=poll_writes=max_poll_writes=write_calls=read_calls=0;
    read_ok=read_again_count=read_intr_count=read_failed_count=0;read_bytes=0;
    pipes=closes=wait_calls=again_count=intr_count=short_count=0;
    fragment=read_fragment=read_faults=write_faults=blocked=write_pipe_error=0;
    force_eof=hold_final=worker_received=worker_accepted=0;worker_reap_ready=1;
    reader_closed=writer_closed=header_seen=body_seen=final_queued=0;
    assert(!desk_capture_job_trace(&job,(int)traced));
    for(size_t i=0;i<PIXELS;i++)canvas[i]=(uint32_t)(i*2654435761u)^0xa55a33ccu;
    char path[DESK_CAPTURE_PATH]="/fat/home/Screenshots/full-size.bmp";
    assert(!desk_capture_job_start(&job,canvas,WIDTH,HEIGHT,path));
    assert(desk_capture_job_trace(&job,0)<0); /* Never alter active protocol. */
    path[1]='X';canvas[0]^=0xffffffffu;canvas[PIXELS-1]^=0xffffffffu;
}
static int step(void) {
    assert(job.busy);now_us=1000000+(uint64_t)polls*period_us;
    poll_writes=0;if(job.sent<STREAM_BYTES)send_polls++;
    int result=desk_capture_job_poll_transfer(&job);
    if(poll_writes>max_poll_writes)max_poll_writes=poll_writes;
    assert(poll_writes<=4);
    assert(read_calls==read_ok+read_again_count+read_intr_count+read_failed_count);
    polls++;return result;
}
static int finish(void) {
    for(unsigned i=0;i<20000;i++) {int result=step();if(result)return result;}
    assert(!"bounded fixture did not finish");return 0;
}
static void close_and_reap(void) {
    worker_reap_ready=1;desk_capture_job_close(&job);
    assert(!job.busy && !job.pixels && job.reader==-1 && job.writer==-1 && !job.pid && !job.retiring_pid);
    assert(reader_closed && writer_closed && closes==4);
}
static void exact_stream(unsigned traced) {
    assert(accepted_size==STREAM_BYTES && consumed_size==STREAM_BYTES && !input_size);
    assert(!memcmp(accepted,consumed,sizeof(accepted)));
    struct desk_capture_request request;memcpy(&request,accepted,sizeof(request));
    assert(request.magic==(traced?trace_magic:DESK_CAPTURE_MAGIC));
    assert(request.id==job.request.id && request.width==WIDTH && request.height==HEIGHT);
    assert(!strcmp(request.path,"/fat/home/Screenshots/full-size.bmp"));
    for(size_t i=0;i<PIXELS;i++) {
        uint32_t pixel;memcpy(&pixel,accepted+sizeof(request)+i*sizeof(pixel),sizeof(pixel));
        assert(pixel==((uint32_t)(i*2654435761u)^0xa55a33ccu));
    }
}
static void canonical_stream(unsigned char *out) {
    memcpy(out,accepted,sizeof(accepted));
    uint32_t magic=DESK_CAPTURE_MAGIC;memcpy(out,&magic,sizeof(magic));
}
static void full_size_cadence(void) {
    uint64_t baseline_input=0,baseline_control=0,baseline_read_bytes=0;
    unsigned baseline_writes=0,baseline_read_ok=0;
    for(unsigned traced=0;traced<2;traced++) {
        reset(traced);assert(finish()==1 && !job.response.status && job.busy && job.pixels);
        exact_stream(traced);assert(write_calls==1026 && send_polls==257 && max_poll_writes==4);
        assert(job.phase==CAPTURE_TEMP_VERIFY && job.progress_frames==4 && final_queued && output_at==output_size);
        if(traced) {
            assert(job.transfer.valid && !job.transfer.invalid && job.transfer.enabled);
            assert(job.transfer.write_calls==1026 && job.transfer.write_ok==1026 && job.transfer.write_bytes==STREAM_BYTES);
            assert(job.transfer.budget_polls==256 && job.transfer.write_again==0 && job.transfer.write_intr==0);
            assert(job.transfer.write_first_us==1000000 && job.transfer.write_last_us==3560000);
            assert(job.transfer.write_last_us-job.transfer.write_first_us==256*period_us);
            assert(job.transfer.write_gap_us==256*period_us && !job.transfer.write_us);
            assert(job.transfer.request_done_us==1000000 && job.transfer.pixels_done_us==3560000);
            assert(job.transfer.worker_receive_us==1000000 && job.transfer.worker_allocated_us==1000000);
            assert(job.transfer.worker_read_done_us==3560000 && job.transfer.worker_final_us==3560000);
            assert(write_calls==baseline_writes && input_hash==baseline_input);
            /* READY contributes one complete frame, but can replace an earlier
             * EAGAIN turn; it need not add a terminal poll/read syscall. */
            assert(read_bytes==baseline_read_bytes+sizeof(struct desk_capture_response) && read_ok==baseline_read_ok+1);
            assert(job.transfer.read_calls==read_calls && job.transfer.read_bytes==read_bytes);
            assert(job.transfer.read_again==read_again_count && job.transfer.read_intr==read_intr_count && job.transfer.read_failed==read_failed_count);
            static unsigned char canonical[STREAM_BYTES];canonical_stream(canonical);assert(!memcmp(canonical,baseline,sizeof(baseline)));
        } else {
            canonical_stream(baseline);baseline_input=input_hash;baseline_writes=write_calls;
            baseline_read_ok=read_ok;baseline_read_bytes=read_bytes;
        }
        close_and_reap();
        if(!traced)baseline_control=control_hash;else assert(control_hash==baseline_control);
    }
}
static void fragmented_and_backpressured(void) {
    uint64_t baseline_input=0;unsigned baseline_writes=0;
    for(unsigned traced=0;traced<2;traced++) {
        reset(traced);fragment=127;read_fragment=3;write_faults=read_faults=1;
        assert(finish()==1 && !job.response.status);exact_stream(traced);
        assert(again_count && intr_count && short_count && max_poll_writes<=4);
        if(traced) {
            assert(job.transfer.valid && !job.transfer.invalid);
            assert(job.transfer.write_bytes==STREAM_BYTES && job.transfer.write_again==again_count && job.transfer.write_intr==intr_count);
            assert(job.transfer.short_writes==short_count && job.transfer.write_calls==write_calls);
            assert(job.transfer.write_us+job.transfer.write_gap_us==job.transfer.write_last_us-job.transfer.write_first_us);
            assert(write_calls==baseline_writes && input_hash==baseline_input);
            static unsigned char canonical[STREAM_BYTES];canonical_stream(canonical);assert(!memcmp(canonical,baseline,sizeof(baseline)));
        } else {canonical_stream(baseline);baseline_input=input_hash;baseline_writes=write_calls;}
        close_and_reap();
    }
    reset(1);blocked=1;
    for(unsigned i=0;i<40;i++)assert(!step());
    assert(input_size<=PIPE_BYTES && input_size>0 && consumed_size==0 && !worker_received && !worker_accepted);
    size_t before=job.sent;assert(before<STREAM_BYTES && again_count && !job.progress_frames);
    for(unsigned i=0;i<40;i++)assert(!step() && job.sent==before);
    assert(job.transfer.write_bytes==before && !job.transfer.pixels_done_us && !job.transfer.worker_read_done_us);
    blocked=0;reader_drain();assert(finish()==1);exact_stream(1);close_and_reap();
}
static void failure_and_retirement(void) {
    reset(1);assert(!step() && job.sent);write_pipe_error=1;
    assert(step()==-1 && job.busy && job.pixels && job.transfer.write_failed==1);
    worker_reap_ready=0;desk_capture_job_close(&job);
    assert(!job.busy && !job.pixels && job.retiring_pid==42 && job.reader==-1 && job.writer==-1);
    assert(desk_capture_job_prepare(&job,WIDTH,HEIGHT,"/fat/home/Screenshots/second.bmp")<0);
    assert(!desk_capture_job_poll(&job) && job.retiring_pid==42);
    worker_reap_ready=1;assert(!desk_capture_job_poll(&job) && !job.retiring_pid && closes==4);

    reset(1);assert(!step());force_eof=1;
    assert(step()==-1 && job.transfer.read_failed==1 && job.busy && job.pixels);close_and_reap();

    reset(1);assert(!step() && !worker_accepted);worker_reap_ready=0;
    desk_capture_job_close(&job);assert(!job.pixels && job.retiring_pid==42 && !worker_accepted);
    worker_reap_ready=1;assert(!desk_capture_job_poll(&job) && !job.retiring_pid);

    reset(1);hold_final=1;
    while(!worker_accepted)assert(!step());
    assert(job.sent==STREAM_BYTES && worker_accepted && !final_queued);
    /* Closing the response/input descriptors cannot kill or revoke a worker
     * that has accepted persistence. Its lifetime remains pending until wait. */
    worker_reap_ready=0;desk_capture_job_close(&job);
    assert(worker_accepted && job.retiring_pid==42 && !job.pixels && !job.busy);
    assert(desk_capture_job_trace(&job,0)<0);
    worker_reap_ready=1;assert(!desk_capture_job_poll(&job) && !job.retiring_pid);

    reset(1);hold_final=1;
    while(!worker_accepted)assert(!step());
    queue_final();worker_reap_ready=1; /* Exited with final bytes still buffered. */
    assert(finish()==1 && !job.response.status && output_at==output_size && job.busy);
    close_and_reap();
}
int main(void) {
    _Static_assert(sizeof(struct desk_capture_request)==176,"wire request changed");
    _Static_assert(PIXEL_BYTES==4196352 && STREAM_BYTES==4196528,"Air fixture changed");
    desk_capture_job_init(&job);
    full_size_cadence();fragmented_and_backpressured();failure_and_retirement();
    puts("capture-pacing: PASS actual full-size sender, 64KiB pipe, 1026 writes/257 polls/256 synthetic cadence gaps, trace on-off input/syscall equivalence, immutable bytes, partial/EAGAIN/EINTR, progress burst, EOF/EPIPE, buffered final and ownership retirement");
    return 0;
}
