#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include "../userspace/lib/src/desk_capture_job.c"
static struct desk_capture_job job;
static unsigned char outgoing[200000];static struct desk_capture_response reply;
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
    memcpy(data,(char *)&reply+received,n);received+=n;return n;
}
static void reset(void) {
    wait_ready=1;desk_capture_job_close(&job);desk_capture_job_init(&job);
    sent=received=reads=writes=pipe_count=closes=spawn_fail=write_fail=eof=0;alive=1;
}
static int finish(void) { for(unsigned i=0;i<20000;i++) { int r=desk_capture_job_poll(&job);if(r) return r; }return 0; }
int main(void) {
    desk_capture_job_init(&job);uint32_t pixels[191*127];for(unsigned i=0;i<191*127;i++) pixels[i]=i;
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
    assert(desk_capture_bytes(1920,1080)==1920*1080*4 && !desk_capture_bytes(UINT32_MAX,UINT32_MAX));
    puts("capture-job: PASS immutable pixels/path, bounded fragmented transfers, retry, protocol failure, exited worker with buffered reply, EOF and nonblocking reaping without killing writes");
}
