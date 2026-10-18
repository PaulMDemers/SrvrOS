#include <assert.h>
#include <stdio.h>
#include "../userspace/lib/src/desk_image_job.c"
static struct desk_image_job job;
static unsigned char wire[200000],out[sizeof(struct desk_image_request)];
static size_t wire_size,position,sent;static int alive,reads,writes,closes,kills,waits,wait_ready,pipe_count,spawn_fail;
long srv_signal_config(uint64_t signal,uint64_t action) { assert(signal==SIGPIPE && action==SRV_SIGNAL_IGNORE);return 0; }
long srv_pipe(int pair[2]) { pair[0]=pipe_count?10:12;pair[1]=pipe_count?13:11;pipe_count++;return 0; }
long srv_fcntl(int fd,int command,uint64_t value) { assert((fd==10 || fd==11) && command==SRV_F_SETFL && value==SRV_FD_NONBLOCK);return 0; }
long srv_spawn_bg_args_fds(const char *path,const char *args,int input,int output) { assert(!strcmp(path,"/fat/bin/imageworker") && !*args && input==12 && output==13);return spawn_fail?-1:42; }
long srv_close(int fd) { assert(fd>=10 && fd<=13);closes++;return 0; }
long srv_kill(int64_t pid) { assert(pid==42);kills++;alive=0;return 0; }
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) { assert(pid==42 && flags==SRV_WAIT_NOHANG);*status=0;waits++;return wait_ready==2?SRV_ERR_INTR:wait_ready?42:0; }
int desk_process_alive(uint64_t pid) { assert(pid==42);return alive; }
long srv_write(int fd,const void *data,size_t n) {
    assert(fd==11);if(++writes%3==0) return SRV_ERR_INTR;if(n>7) n=7;assert(sent+n<=sizeof(out));memcpy(out+sent,data,n);sent+=n;return n;
}
long srv_read(int fd,void *data,size_t n) {
    assert(fd==10);if(++reads%4==0 || position==wire_size) return SRV_ERR_AGAIN;
    assert(sent==sizeof(out));if(n>37) n=37;if(n>wire_size-position) n=wire_size-position;
    memcpy(data,wire+position,n);position+=n;return n;
}
static void reset(void) {
    desk_image_job_cancel(&job);desk_image_job_init(&job);
    alive=1;wire_size=position=sent=reads=writes=closes=kills=waits=wait_ready=pipe_count=spawn_fail=0;
}
static void response(unsigned status,unsigned w,unsigned h) {
    struct desk_image_response r={.magic=DESK_IMAGE_MAGIC,.id=job.request.id,.status=status,.width=w,.height=h,.length=w*h*4,.checksum=DESK_IMAGE_HASH_INITIAL};
    assert(r.length+sizeof(r)<=sizeof(wire));for(unsigned i=0;i<r.length;i++) wire[sizeof(r)+i]=(unsigned char)(i*17);
    r.checksum=desk_image_hash(r.checksum,wire+sizeof(r),r.length);memcpy(wire,&r,sizeof(r));wire_size=sizeof(r)+r.length;
}
static int finish(void) { int result=0;for(unsigned i=0;i<20000 && !result;i++) result=desk_image_job_poll(&job,1);return result; }
int main(void) {
    desk_image_job_init(&job);reset();char path[160]="/fat/sample.png";
    assert(!desk_image_job_start(&job,path,0));path[5]='X';assert(!strcmp(job.request.path,"/fat/sample.png"));assert(desk_image_job_start(&job,"/fat/other",0)<0);
    response(0,191,127);assert(finish()==1 && job.ready && !job.busy);uint32_t *pixels=desk_image_job_take(&job);assert(pixels && !job.pixels && !job.ready);assert(!memcmp(pixels,wire+sizeof(job.response),191*127*4));desk_image_pixels_release(pixels,191,127);assert(!desk_image_job_take(&job));
    reset();assert(!desk_image_job_start(&job,"/fat/absent",0));response(DESK_IMAGE_NOT_FOUND,0,0);assert(finish()==1 && job.response.status==DESK_IMAGE_NOT_FOUND && !desk_image_job_take(&job));
    reset();assert(!desk_image_job_start(&job,"/fat/image",0));response(0,2,2);wire[sizeof(job.response)]^=1;assert(finish()==-1 && !job.pixels && !job.busy && kills==1);
    reset();assert(!desk_image_job_start(&job,"/fat/image",0));response(0,2,2);((struct desk_image_response *)wire)->width=4097;assert(finish()==-1 && !job.pixels);
    reset();assert(!desk_image_job_start(&job,"/fat/image",0));response(DESK_IMAGE_IO,1,1);assert(finish()==-1);
    reset();assert(!desk_image_job_start(&job,"/fat/image",0));response(0,2,2);((struct desk_image_response *)wire)->id++;assert(finish()==-1);
    reset();assert(!desk_image_job_start(&job,"/fat/image",0));response(0,2,2);alive=0;assert(desk_image_job_poll(&job,1)==-1 && kills==1 && job.retiring_pid==42);
    assert(desk_image_job_start(&job,"/fat/image",1)==-1);wait_ready=2;desk_image_job_poll(&job,1);assert(job.retiring_pid==42);wait_ready=1;desk_image_job_poll(&job,1);assert(!job.retiring_pid);
    reset();assert(!desk_image_job_start(&job,"/fat/image",7));assert(desk_image_job_poll(&job,3007)==-1 && !reads);
    reset();assert(!desk_image_job_start(&job,"/fat/image",0));response(0,191,127);
    while(position<sizeof(job.response)+20) assert(!desk_image_job_poll(&job,1));
    assert(job.pixels);desk_image_job_cancel(&job);assert(!job.pixels && !job.busy && !job.ready && kills==1);
    reset();assert(desk_image_job_start(&job,"relative",0)==-1 && !pipe_count);spawn_fail=1;assert(desk_image_job_start(&job,"/fat/image",0)==-1 && closes==4 && !job.pid);
    desk_image_job_close(&job);
    puts("image-job: PASS bounded fragmented transfer, retry, checked payload ownership, failures, timeout, cancellation and nonblocking worker reaping");
}
