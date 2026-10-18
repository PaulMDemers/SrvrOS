#include <assert.h>
#include <stdio.h>
#include "../userspace/lib/src/desk_command_job.c"
static struct desk_command_job job;static char output[80],payload[20000];
static size_t cursor,total;static int pipes,closed,kills,reads,wait_ready=1,spawn_fail,pipe_fail,fcntl_fail,read_fail,wait_error,block;
long srv_pipe(int pair[2]) { pipes++;if(pipes==pipe_fail) return -1;pair[0]=pipes==1?10:12;pair[1]=pair[0]+1;return 0; }
long srv_fcntl(int fd,int command,uint64_t value) { assert(fd==12 && command==SRV_F_SETFL && value==SRV_FD_NONBLOCK);return fcntl_fail?-1:0; }
long srv_close(int fd) { assert(fd>=10 && fd<=13);closed++;return 0; }
long srv_exec(const struct srv_exec_request *q) { assert(!strcmp(q->path,"/fat/bin/ping") && !strcmp(q->argv[1],"10.0.2.2") && q->stdin_fd==10 && q->stdout_fd==13 && q->stderr_fd==13 && q->flags==SRV_EXEC_BACKGROUND);return spawn_fail?-1:42; }
long srv_kill(int64_t pid) { assert(pid==42);kills++;return 0; }
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) { assert(pid==42 && flags==SRV_WAIT_NOHANG);if(wait_error) return wait_error;*status=7;return wait_ready?42:0; }
long srv_read(int fd,void *buffer,size_t bytes) {
    assert(fd==12 && bytes<=4096);reads++;
    if(read_fail) return -1;
    if(block) return SRV_ERR_AGAIN;
    if(reads%3==0) return SRV_ERR_INTR;
    if(cursor==total) return 0;
    if(bytes>17) bytes=17;
    if(bytes>total-cursor) bytes=total-cursor;
    memcpy(buffer,payload+cursor,bytes);cursor+=bytes;return bytes;
}
static char *arguments[]={"ping","10.0.2.2",NULL};
static void reset(void) {
    wait_ready=1;wait_error=0;desk_command_job_close(&job);assert(!desk_command_job_init(&job,output,sizeof(output)));
    cursor=total=pipes=closed=kills=reads=spawn_fail=pipe_fail=fcntl_fail=read_fail=wait_error=block=0;
}
static int finish(void) { for(int i=0;i<3000;i++) { int result=desk_command_job_poll(&job,1);if(result) return result; }return 0; }
int main(void) {
    assert(desk_command_job_init(&job,output,1)<0);assert(!desk_command_job_init(&job,output,sizeof(output)));
    for(size_t i=0;i<sizeof(payload);i++) payload[i]='a'+i%26;
    reset();total=50;assert(!desk_command_job_start(&job,"/fat/bin/ping",arguments,0,1000000));assert(closed==3);
    assert(finish()==1 && job.exit_status==7 && job.length==50 && !job.truncated && !memcmp(output,payload,50) && !output[50] && closed==4 && !kills);
    reset();total=sizeof(payload);assert(!desk_command_job_start(&job,"/fat/bin/ping",arguments,0,1000000));
    assert(finish()==1 && job.truncated && job.length==79 && cursor==total && !memcmp(output,payload,79) && !output[79]);
    reset();wait_ready=0;assert(!desk_command_job_start(&job,"/fat/bin/ping",arguments,0,1000000));
    assert(!desk_command_job_poll(&job,1) && job.eof && job.busy && job.reader==-1);
    wait_error=SRV_ERR_INTR;assert(!desk_command_job_poll(&job,2));wait_error=0;wait_ready=1;assert(desk_command_job_poll(&job,3)==1);
    reset();block=1;wait_ready=0;assert(!desk_command_job_start(&job,"/fat/bin/ping",arguments,0,1000000));
    assert(!desk_command_job_poll(&job,999999));assert(desk_command_job_poll(&job,1000000)==-1 && job.timed_out && !job.canceled && !job.busy && job.retiring_pid==42 && kills==1);
    assert(desk_command_job_start(&job,"/fat/bin/ping",arguments,2,1000000)<0);wait_ready=1;desk_command_job_poll(&job,2);assert(!job.retiring_pid);
    reset();assert(!desk_command_job_start(&job,"/fat/bin/ping",arguments,0,1000000));desk_command_job_cancel(&job);assert(job.canceled && kills==1 && !job.busy);
    reset();spawn_fail=1;assert(desk_command_job_start(&job,"/fat/bin/ping",arguments,0,1000000)<0 && closed==4);
    reset();fcntl_fail=1;assert(desk_command_job_start(&job,"/fat/bin/ping",arguments,0,1000000)<0 && closed==4);
    reset();pipe_fail=2;assert(desk_command_job_start(&job,"/fat/bin/ping",arguments,0,1000000)<0 && closed==2);
    reset();read_fail=1;assert(!desk_command_job_start(&job,"/fat/bin/ping",arguments,0,1000000));assert(desk_command_job_poll(&job,1)<0 && kills==1);
    reset();wait_error=-1;assert(!desk_command_job_start(&job,"/fat/bin/ping",arguments,0,1000000));assert(desk_command_job_poll(&job,1)<0 && kills==1);
    reset();assert(desk_command_job_start(&job,"relative",arguments,0,1000000)<0 && !pipes);assert(desk_command_job_start(&job,"/fat/bin/ping",arguments,0,0)<0);
    puts("command-job: PASS fragmented bounded output, merged streams, EOF/exit ordering, truncation/drain, retry, timeout/cancel/reaping and failure cleanup");
}
