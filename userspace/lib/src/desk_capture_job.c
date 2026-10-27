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
void desk_capture_job_close(struct desk_capture_job *j) {
    if(j->reader>=0) srv_close(j->reader);
    if(j->writer>=0) srv_close(j->writer);
    j->reader=j->writer=-1;j->busy=0;
    if(j->pixels) munmap(j->pixels,desk_capture_bytes(j->request.width,j->request.height));
    j->pixels=NULL;
    if(j->pid) { j->retiring_pid=j->pid;j->pid=0; }
    reap(j);
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
int desk_capture_job_start(struct desk_capture_job *j,const uint32_t *pixels,uint32_t width,uint32_t height,const char *path) {
    reap(j);uint32_t bytes=desk_capture_bytes(width,height);
    if(j->busy || j->retiring_pid || !bytes || !pixels || !path || path[0]!='/' || strlen(path)>=DESK_CAPTURE_PATH) return -1;
    j->request=(struct desk_capture_request){.magic=DESK_CAPTURE_MAGIC,.id=++j->next_id,.width=width,.height=height};
    long now=srv_monotonic_us();j->started_us=now>0?(uint64_t)now:0;
    j->finished_us=j->last_progress_us=j->progress_bytes=0;j->phase=j->progress_frames=0;
    memset(j->phase_us,0,sizeof(j->phase_us));
    strcpy(j->request.path,path);j->response=(struct desk_capture_response){0};j->sent=j->received=0;
    j->pixels=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(j->pixels==MAP_FAILED) { j->pixels=NULL;return -1; }
    memcpy(j->pixels,pixels,bytes);
    if(spawn(j)) { desk_capture_job_close(j);return -1; }
    j->busy=1;return 0;
}
int desk_capture_job_poll(struct desk_capture_job *j) {
    reap(j);if(!j->busy) return 0;
    size_t total=sizeof(j->request)+desk_capture_bytes(j->request.width,j->request.height);
    for(unsigned batch=0;batch<4 && j->sent<total;batch++) {
        const void *data;size_t bytes;
        if(j->sent<sizeof(j->request)) { data=(char *)&j->request+j->sent;bytes=sizeof(j->request)-j->sent; }
        else { data=(char *)j->pixels+j->sent-sizeof(j->request);bytes=total-j->sent; }
        if(bytes>4096) bytes=4096;
        long n=srv_write(j->writer,data,bytes);
        if(n==SRV_ERR_AGAIN || n==SRV_ERR_INTR) break;
        if(n<=0 || (size_t)n>bytes) goto failed;
        j->sent+=n;
    }
    long n=srv_read(j->reader,(char *)&j->response+j->received,sizeof(j->response)-j->received);
    /* The worker may publish its response and exit between an empty read and
     * any process-status query. Drain the pipe; EOF, not process liveness, is
     * the authoritative end of this one-shot stream. */
    if(n==SRV_ERR_AGAIN || n==SRV_ERR_INTR) return 0;
    if(n<=0 || (size_t)n>sizeof(j->response)-j->received) goto failed;
    j->received+=n;
    if(j->received<sizeof(j->response)) return 0;
    if(j->response.magic==DESK_CAPTURE_PROGRESS_MAGIC) {
        const struct desk_capture_response *r=&j->response;
        if(r->id!=j->request.id || r->reserved || !r->status || r->status>=CAPTURE_PHASE_COUNT ||
           r->status<j->phase || r->bytes>DESK_CAPTURE_MAX_BYTES || r->at_us<j->last_progress_us)goto failed;
        if(j->phase)j->phase_us[j->phase]+=r->at_us-j->last_progress_us;
        j->phase=r->status;j->last_progress_us=r->at_us;j->progress_bytes=r->bytes;j->progress_frames++;
        j->received=0;j->response=(struct desk_capture_response){0};return 0;
    }
    if(j->response.at_us<j->last_progress_us)goto failed;
    if(j->phase)j->phase_us[j->phase]+=j->response.at_us-j->last_progress_us;
    if(j->response.magic!=DESK_CAPTURE_MAGIC || j->response.id!=j->request.id || j->response.reserved || j->response.bytes ||
       j->response.status>DESK_CAPTURE_EXISTS || (!j->response.status && j->sent!=total)) goto failed;
    {long now=srv_monotonic_us();j->finished_us=now>0?(uint64_t)now:0;}
    desk_capture_job_close(j);return 1;
failed:
    {long now=srv_monotonic_us();j->finished_us=now>0?(uint64_t)now:0;}
    desk_capture_job_close(j);return -1;
}
