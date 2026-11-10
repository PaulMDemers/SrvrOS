#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include "../userspace/lib/src/desk_capture_job.c"
static struct desk_capture_job job;
static unsigned char outgoing[200000];static struct desk_capture_response reply;
static struct desk_capture_response frames[8];static unsigned frame_count,frame_index;
static uint64_t clock_reads;static long clock_at=1000000;static unsigned clock_regress_on;
long srv_monotonic_us(void) {clock_reads++;if(clock_reads==clock_regress_on)return clock_at-10;return clock_at++;}
static size_t sent,received,total;static int reads,writes,alive=1,wait_ready=1,pipe_count,closes,spawn_fail,write_fail,eof;
long srv_signal_config(uint64_t signal,uint64_t action) { assert(signal==SIGPIPE && action==SRV_SIGNAL_IGNORE);return 0; }
long srv_pipe(int pair[2]) { pair[0]=pipe_count?10:12;pair[1]=pipe_count?13:11;pipe_count++;return 0; }
long srv_fcntl(int fd,int command,uint64_t value) { assert((fd==10 || fd==11) && command==SRV_F_SETFL && value==SRV_FD_NONBLOCK);return 0; }
long srv_spawn_bg_args_fds(const char *path,const char *args,int input,int output) { assert(!strcmp(path,"/fat/bin/shotworker") && !*args && input==12 && output==13);return spawn_fail?-1:42; }
long srv_close(int fd) { assert(fd>=10 && fd<=13);closes++;return 0; }
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) { assert(pid==42 && flags==SRV_WAIT_NOHANG);*status=0;return wait_ready==2?SRV_ERR_INTR:wait_ready?42:0; }
int desk_process_alive(uint64_t pid) { assert(pid==42);return alive; }
long srv_write(int fd,const void *data,size_t n) {
    assert(fd==11 && n<=4096);if(write_fail) return -1;if(++writes%3==0) return SRV_ERR_INTR;
    if(n>127) n=127;
    assert(sent+n<=sizeof(outgoing));memcpy(outgoing+sent,data,n);sent+=n;return n;
}
long srv_read(int fd,void *data,size_t n) {
    assert(fd==10);if(eof) return 0;if(++reads%4==0 || sent<total) return SRV_ERR_AGAIN;
    if(n>3) n=3;
    memcpy(data,(char *)(frame_count?&frames[frame_index]:&reply)+received,n);received+=n;
    if(frame_count && received==sizeof(reply)){received=0;frame_index++;assert(frame_index<=frame_count);}
    return n;
}
static void reset(void) {
    wait_ready=1;desk_capture_job_close(&job);desk_capture_job_init(&job);
    frame_count=frame_index=0;sent=received=reads=writes=pipe_count=closes=spawn_fail=write_fail=eof=0;alive=1;
    clock_reads=clock_regress_on=0;clock_at=1000000;
}
static int finish(void) { for(unsigned i=0;i<20000;i++) { int r=desk_capture_job_poll(&job);if(r) return r; }return 0; }
static void timing_frames(int traced) {
    frame_count=traced?4:3;
    frames[0]=(struct desk_capture_response){.magic=DESK_CAPTURE_PROGRESS_MAGIC,.id=job.request.id,.status=CAPTURE_RECEIVE,.at_us=1000010};
    frames[1]=(struct desk_capture_response){.magic=DESK_CAPTURE_READY_MAGIC,.id=job.request.id,.status=CAPTURE_RECEIVE,.at_us=1000020};
    frames[2]=(struct desk_capture_response){.magic=DESK_CAPTURE_PROGRESS_MAGIC,.id=job.request.id,.status=CAPTURE_ENCODE,.at_us=1000030,.bytes=sizeof(uint32_t)*191*127};
    frames[3]=(struct desk_capture_response){.magic=DESK_CAPTURE_MAGIC,.id=job.request.id,.at_us=1000050};
    if(!traced){frames[1]=frames[2];frames[2]=frames[3];}
}
static void timing_tests(uint32_t *pixels,const char *path) {
    static unsigned char baseline[sizeof(outgoing)];int baseline_writes=0;
    for(unsigned traced=0;traced<=1;traced++) {
        reset();assert(!desk_capture_job_trace(&job,traced));
        assert(!desk_capture_job_start(&job,pixels,191,127,path));
        assert(job.request.magic==(traced?DESK_CAPTURE_TRACE_MAGIC:DESK_CAPTURE_MAGIC));
        assert(desk_capture_job_trace(&job,!traced)<0 && job.transfer.enabled==traced);
        total=sizeof(job.request)+sizeof(uint32_t)*191*127;timing_frames(traced);
        assert(finish()==1 && job.progress_frames==2);
        assert(job.phase_us[CAPTURE_RECEIVE]==20 && job.phase_us[CAPTURE_ENCODE]==20);
        if(!traced) {
            memcpy(baseline,outgoing,total);baseline_writes=writes;
            assert(clock_reads==9 && !job.transfer.enabled && !job.transfer.polls);
        } else {
            const struct desk_capture_transfer_timing *t=&job.transfer;
            assert(t->enabled && t->valid && !t->invalid && t->total==total);
            assert(t->write_bytes==total && t->write_ok && t->short_writes && t->write_intr && !t->write_failed);
            assert(t->write_calls==t->write_ok+t->write_again+t->write_intr+t->write_failed);
            assert(t->write_first_us<=t->request_done_us && t->request_done_us<=t->pixels_done_us && t->pixels_done_us<=t->write_last_us);
            assert(t->end_us==job.finished_us && t->poll_us+t->gap_us==t->end_us-t->begin_us);
            assert(t->write_us+t->write_gap_us==t->write_last_us-t->write_first_us);
            assert(t->read_calls==t->polls && t->read_bytes==(job.progress_frames+2)*sizeof(struct desk_capture_response));
            assert(t->worker_receive_us==1000010 && t->worker_allocated_us==1000020 && t->worker_read_done_us==1000030 && t->worker_final_us==1000050);
            assert(clock_reads>9 && writes==baseline_writes);
            assert(!memcmp(outgoing+sizeof(uint32_t),baseline+sizeof(uint32_t),total-sizeof(uint32_t)));
            pipe_count=0;assert(!desk_capture_job_prepare(&job,191,127,path));
            assert(job.transfer.enabled && job.transfer.valid && !job.transfer.polls && !job.transfer.write_calls && !job.transfer.worker_receive_us && !job.transfer_ready_received);
        }
    }
    for(unsigned bad=0;bad<6;bad++) {
        reset();assert(!desk_capture_job_trace(&job,1));assert(!desk_capture_job_start(&job,pixels,191,127,path));timing_frames(1);
        if(bad==0)frames[1].id++;
        if(bad==1){frames[4]=frames[3];frames[3]=frames[2];frames[2]=frames[1];frame_count=5;}
        if(bad==2){frames[0]=frames[1];frame_count=1;}
        if(bad==3)frames[1].status=CAPTURE_ENCODE;
        if(bad==4)frames[1].reserved=1;
        if(bad==5)frames[1].bytes=1;
        assert(finish()==-1 && !job.pixels);
    }
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));timing_frames(1);assert(finish()==-1); /* Unrequested auxiliary frame. */
    reset();assert(!desk_capture_job_trace(&job,1));assert(!desk_capture_job_start(&job,pixels,191,127,path));timing_frames(1);
    frames[1].at_us=frames[0].at_us-1;assert(finish()==1 && !job.transfer.valid && job.transfer.invalid && job.phase_us[CAPTURE_RECEIVE]==20);
    reset();assert(!desk_capture_job_trace(&job,1));assert(!desk_capture_job_start(&job,pixels,191,127,path));timing_frames(1);
    clock_reads=0;clock_regress_on=3;assert(finish()==1 && !job.transfer.valid && job.transfer.invalid);
    reset();assert(!desk_capture_job_trace(&job,1));assert(!desk_capture_job_start(&job,pixels,191,127,path));timing_frames(1);
    job.transfer.write_calls=UINT64_MAX;assert(finish()==1 && !job.transfer.valid && job.transfer.invalid);
    reset();assert(!desk_capture_job_trace(&job,1));assert(!desk_capture_job_start(&job,pixels,191,127,path));timing_frames(1);
    job.transfer.poll_us=UINT64_MAX;assert(finish()==1 && !job.transfer.valid && job.transfer.invalid);
    reset();assert(!desk_capture_job_trace(&job,1));assert(!desk_capture_job_start(&job,pixels,191,127,path));write_fail=1;
    assert(finish()==-1 && job.transfer.write_calls==1 && job.transfer.write_failed==1 && !job.transfer.read_calls && !job.transfer.worker_final_us);
    reset();assert(!desk_capture_job_trace(&job,1));assert(!desk_capture_job_start(&job,pixels,191,127,path));eof=1;
    assert(finish()==-1 && job.transfer.read_calls==1 && job.transfer.read_failed==1);
    reset();puts("capture-transfer-timing: PASS ordinary clock/callback equivalence, exact attempt/poll partitions, split worker allocation receipt, unchanged phases, malformed auxiliary frames, reset and diagnostic-only overflow/clock faults");
}
int main(void) {
    desk_capture_job_init(&job);uint32_t pixels[191*127];for(unsigned i=0;i<191*127;i++) pixels[i]=i;
    assert(!desk_capture_job_prepare(&job,191,127,"/fat/home/Screenshots/prepared.bmp"));
    assert(!desk_capture_job_poll_transfer(&job) && !writes && !reads && !job.sent);
    assert(desk_capture_job_snapshot(&job,pixels,190,127)<0 && !job.sent);
    assert(!desk_capture_job_snapshot(&job,pixels,191,127));
    assert(!memcmp(job.pixels,pixels,sizeof(pixels)));
    job.sent=1;assert(desk_capture_job_snapshot(&job,pixels,191,127)<0);job.sent=0;
    reset();
    char path[160]="/fat/home/Screenshots/test.bmp";reset();
    assert(!desk_capture_job_start(&job,pixels,191,127,path));path[5]='X';pixels[0]=0xff;
    total=sizeof(job.request)+sizeof(pixels);reply=(struct desk_capture_response){.magic=DESK_CAPTURE_MAGIC,.id=job.request.id};
    assert(desk_capture_job_start(&job,pixels,191,127,path)<0);wait_ready=0;
    assert(finish()==1 && !job.busy && !job.pixels && job.retiring_pid==42 && closes==4);
    assert(!strcmp(((struct desk_capture_request *)outgoing)->path,"/fat/home/Screenshots/test.bmp"));
    assert(!memcmp(outgoing+sizeof(job.request)+4,pixels+1,sizeof(pixels)-4) && *(uint32_t *)(outgoing+sizeof(job.request))==0);
    assert(desk_capture_job_start(&job,pixels,191,127,path)<0);wait_ready=2;desk_capture_job_poll(&job);assert(job.retiring_pid==42);
    wait_ready=1;desk_capture_job_poll(&job);assert(!job.retiring_pid);
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));reply.id=job.request.id+1;assert(finish()==-1 && !job.pixels);
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));reply.id=job.request.id;reply.status=DESK_CAPTURE_IO;assert(finish()==1 && job.response.status==DESK_CAPTURE_IO);
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));alive=0;reply.status=0;assert(!desk_capture_job_poll(&job));assert(finish()==1 && !job.pixels);
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));eof=1;assert(desk_capture_job_poll(&job)==-1 && !job.pixels);
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));write_fail=1;assert(desk_capture_job_poll(&job)==-1 && !job.pixels);
    reset();assert(desk_capture_job_start(&job,pixels,4096,4096,path)<0 && !pipe_count);assert(desk_capture_job_start(&job,pixels,191,127,"relative")<0);
    spawn_fail=1;assert(desk_capture_job_start(&job,pixels,191,127,path)<0 && !job.pixels && closes==4);
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));
    frame_count=3;frames[0]=(struct desk_capture_response){.magic=DESK_CAPTURE_PROGRESS_MAGIC,.id=job.request.id,.status=CAPTURE_WRITE,.at_us=10,.bytes=100};
    frames[1]=frames[0];frames[1].status=CAPTURE_TEMP_VERIFY;frames[1].at_us=1010;frames[1].bytes=200;
    frames[2]=(struct desk_capture_response){.magic=DESK_CAPTURE_MAGIC,.id=job.request.id,.at_us=3010};
    assert(finish()==1 && job.progress_frames==2 && job.phase==CAPTURE_TEMP_VERIFY);
    assert(job.phase_us[CAPTURE_WRITE]==1000 && job.phase_us[CAPTURE_TEMP_VERIFY]==2000 && job.progress_bytes==200);
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));frame_count=2;
    frames[0].id=frames[1].id=job.request.id;frames[1].at_us=9;assert(finish()==-1); /* Clock regression. */
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));frame_count=2;
    frames[0].id=frames[1].id=job.request.id;frames[1].at_us=1010;frames[1].status=CAPTURE_RECEIVE;assert(finish()==-1);
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));frame_count=1;frames[0].id=job.request.id;frames[0].reserved=1;assert(finish()==-1);
    reset();assert(!desk_capture_job_start(&job,pixels,191,127,path));
    reply=(struct desk_capture_response){.magic=DESK_CAPTURE_MAGIC,.id=job.request.id};
    int completion=0;for(unsigned n=0;n<10000 && !completion;n++)completion=desk_capture_job_poll_transfer(&job);
    assert(completion==1 && job.busy && job.pixels && job.reader==10 && job.writer==11 && closes==2);
    desk_capture_job_close(&job);assert(!job.busy && !job.pixels && closes==4);
    assert(desk_capture_scale(1440,900)==1 && desk_capture_scale(1920,1080)==1);
    assert(desk_capture_scale(2880,1800)==2 && desk_capture_scale(3840,2160)==2);
    assert(!desk_capture_scale(4096,4096) && !desk_capture_scale(UINT32_MAX,1));
    assert(!desk_capture_scale(0,1080));
    assert(desk_capture_bytes(1920,1080)==1920*1080*4 && !desk_capture_bytes(UINT32_MAX,UINT32_MAX));
    timing_tests(pixels,path);
    puts("capture-job: PASS immutable pixels/path, bounded fragmented transfers, retry, protocol failure, exited worker with buffered reply, EOF and nonblocking reaping without killing writes");
}
