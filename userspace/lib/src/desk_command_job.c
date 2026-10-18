#include <srvros/desk_command_job.h>
#include <srvros/sys.h>
#include <string.h>
static void reap(struct desk_command_job *j) {
    if(j->retiring_pid) {
        uint64_t status;long result=srv_wait(j->retiring_pid,&status,SRV_WAIT_NOHANG);
        if(result==(long)j->retiring_pid || result==-1) j->retiring_pid=0;
    }
}
int desk_command_job_init(struct desk_command_job *j,char *output,size_t capacity) {
    if(!j || !output || capacity<2 || capacity>65537) return -1;
    *j=(struct desk_command_job){.reader=-1,.output=output,.capacity=capacity};output[0]=0;return 0;
}
void desk_command_job_cancel(struct desk_command_job *j) {
    if(j->reader>=0) srv_close(j->reader);
    j->reader=-1;j->busy=0;j->canceled=1;
    if(j->pid) { srv_kill(j->pid);j->retiring_pid=j->pid;j->pid=0; }
    reap(j);
}
void desk_command_job_close(struct desk_command_job *j) { desk_command_job_cancel(j); }
int desk_command_job_start(struct desk_command_job *j,const char *path,char *const argv[],uint64_t now,uint64_t timeout) {
    reap(j);
    if(j->busy || j->retiring_pid || !j->output || j->capacity<2 || !path || path[0]!='/' || strlen(path)>=160 ||
       !argv || !argv[0] || !timeout || timeout>60000000) return -1;
    unsigned count=0;
    for(;count<16 && argv[count];count++) if(strlen(argv[count])>=256) return -1;
    if(count==16) return -1;
    int input[2],output[2];if(srv_pipe(input)) return -1;
    if(srv_pipe(output)) { srv_close(input[0]);srv_close(input[1]);return -1; }
    if(srv_fcntl(output[0],SRV_F_SETFL,SRV_FD_NONBLOCK)<0) {
        srv_close(input[0]);srv_close(input[1]);srv_close(output[0]);srv_close(output[1]);return -1;
    }
    char *environment[]={"PATH=/fat/bin","HOME=/fat/home",NULL};
    struct srv_exec_request request={.path=path,.argv=argv,.envp=environment,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=input[0],.stdout_fd=output[1],.stderr_fd=output[1],.process_group=UINT64_MAX};
    long pid=srv_exec(&request);srv_close(input[0]);srv_close(input[1]);srv_close(output[1]);
    if(pid<=0) { srv_close(output[0]);return -1; }
    j->pid=pid;j->reader=output[0];j->started_us=now;j->timeout_us=timeout;j->busy=1;
    j->eof=j->truncated=j->timed_out=j->canceled=0;j->length=0;j->output[0]=0;j->exit_status=0;return 0;
}
int desk_command_job_poll(struct desk_command_job *j,uint64_t now) {
    reap(j);if(!j->busy) return 0;
    if(now-j->started_us>=j->timeout_us) { j->timed_out=1;goto failed; }
    for(unsigned batch=0;batch<4 && !j->eof;batch++) {
        char data[4096];long n=srv_read(j->reader,data,sizeof(data));
        if(n==SRV_ERR_AGAIN || n==SRV_ERR_INTR) break;
        if(n<0 || n>(long)sizeof(data)) goto failed;
        if(!n) { j->eof=1;srv_close(j->reader);j->reader=-1;break; }
        size_t keep=n;if(keep>j->capacity-1-j->length) { keep=j->capacity-1-j->length;j->truncated=1; }
        if(keep) memcpy(j->output+j->length,data,keep);
        j->length+=keep;j->output[j->length]=0;
    }
    if(j->eof) {
        long result=srv_wait(j->pid,&j->exit_status,SRV_WAIT_NOHANG);
        if(result==(long)j->pid) { j->busy=0;j->pid=0;return 1; }
        if(result<0 && result!=SRV_ERR_INTR && result!=SRV_ERR_AGAIN) goto failed;
    }
    return 0;
failed:
    desk_command_job_cancel(j);j->canceled=0;return -1;
}
