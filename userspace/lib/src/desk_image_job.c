#include <srvros/desk_image_job.h>
#include <srvros/sys.h>
#include <srvros/desktop.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>

void desk_image_pixels_release(uint32_t *pixels,uint32_t width,uint32_t height) {
    if(pixels && width && height) munmap(pixels,(size_t)width*height*4);
}
static void reap(struct desk_image_job *j) {
    if(j->retiring_pid) {
        uint64_t status;long result=srv_wait(j->retiring_pid,&status,SRV_WAIT_NOHANG);
        if(result==(long)j->retiring_pid || result==-1) j->retiring_pid=0;
    }
}
void desk_image_job_init(struct desk_image_job *j) { *j=(struct desk_image_job){.reader=-1,.writer=-1}; }
void desk_image_job_cancel(struct desk_image_job *j) {
    if(j->reader>=0) srv_close(j->reader);
    if(j->writer>=0) srv_close(j->writer);
    j->reader=j->writer=-1;j->busy=j->ready=0;desk_image_pixels_release(j->pixels,j->response.width,j->response.height);j->pixels=NULL;
    /* This worker never mutates a file, so abandoning an image decode is safe.
     * Retain its PID until a subsequent nonblocking wait confirms collection. */
    if(j->pid) { srv_kill(j->pid);j->retiring_pid=j->pid;j->pid=0; }
    reap(j);
}
void desk_image_job_close(struct desk_image_job *j) { desk_image_job_cancel(j); }
static int spawn(struct desk_image_job *j) {
    int input[2],output[2];
    if(srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE)!=0 || srv_pipe(input)) return -1;
    if(srv_pipe(output)) { srv_close(input[0]);srv_close(input[1]);return -1; }
    j->reader=output[0];j->writer=input[1];
    if(srv_fcntl(j->reader,SRV_F_SETFL,SRV_FD_NONBLOCK)<0 || srv_fcntl(j->writer,SRV_F_SETFL,SRV_FD_NONBLOCK)<0) {
        srv_close(input[0]);srv_close(output[1]);desk_image_job_cancel(j);return -1;
    }
    long pid=srv_spawn_bg_args_fds("/fat/bin/imageworker","",input[0],output[1]);
    srv_close(input[0]);srv_close(output[1]);
    if(pid<=0) { desk_image_job_cancel(j);return -1; }
    j->pid=pid;return 0;
}
int desk_image_job_start(struct desk_image_job *j,const char *path,uint64_t now) {
    reap(j);
    if(j->busy || j->retiring_pid || !path || path[0]!='/' || strlen(path)>=DESK_IMAGE_PATH) return -1;
    if(!j->pid && spawn(j)) return -1;
    desk_image_pixels_release(j->pixels,j->response.width,j->response.height);j->pixels=NULL;j->ready=0;
    if(!++j->next_id) ++j->next_id;
    j->request=(struct desk_image_request){.magic=DESK_IMAGE_MAGIC,.id=j->next_id};strcpy(j->request.path,path);
    j->response=(struct desk_image_response){0};j->sent=j->received=0;j->hash=DESK_IMAGE_HASH_INITIAL;j->busy=1;j->started=now;return 0;
}
static int header(struct desk_image_job *j) {
    const struct desk_image_response *r=&j->response;
    if(r->magic!=DESK_IMAGE_MAGIC || r->id!=j->request.id || r->reserved || r->status>DESK_IMAGE_INVALID) return -1;
    if(r->status) return r->width || r->height || r->length || r->checksum!=DESK_IMAGE_HASH_INITIAL?-1:0;
    if(!r->width || !r->height || r->width>SRV_IMAGE_MAX_DIMENSION || r->height>SRV_IMAGE_MAX_DIMENSION ||
       (uint64_t)r->width*r->height>SRV_IMAGE_MAX_PIXELS || r->length!=(uint64_t)r->width*r->height*4) return -1;
    j->pixels=mmap(NULL,r->length,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(j->pixels==MAP_FAILED) { j->pixels=NULL;return -1; }return 0;
}
int desk_image_job_poll(struct desk_image_job *j,uint64_t now) {
    reap(j);if(!j->busy) return 0;
    if(now-j->started>=3000) goto failed;
    if(j->sent<sizeof(j->request)) {
        size_t size=sizeof(j->request)-j->sent;long n=srv_write(j->writer,(char *)&j->request+j->sent,size);
        if(n!=SRV_ERR_AGAIN && n!=SRV_ERR_INTR) { if(n<=0 || (size_t)n>size) goto failed;j->sent+=n; }
    }
    if(j->sent<sizeof(j->request)) goto pending;
    for(unsigned batch=0;batch<16;batch++) {
        void *data;size_t size;int payload=j->received>=sizeof(j->response);
        if(!payload) { data=(char *)&j->response+j->received;size=sizeof(j->response)-j->received; }
        else {
            size=sizeof(j->response)+j->response.length-j->received;
            if(!size) { if(j->hash!=j->response.checksum) goto failed;j->busy=0;j->ready=1;return 1; }
            data=(char *)j->pixels+j->received-sizeof(j->response);
        }
        if(size>4096) size=4096;
        long n=srv_read(j->reader,data,size);
        if(n==SRV_ERR_AGAIN || n==SRV_ERR_INTR) break;
        if(n<=0 || (size_t)n>size) goto failed;
        if(payload) j->hash=desk_image_hash(j->hash,data,(size_t)n);
        j->received+=n;
        if(j->received==sizeof(j->response) && header(j)) goto failed;
    }
pending:
    if(!desk_process_alive(j->pid)) goto failed;
    return 0;
failed:
    desk_image_job_cancel(j);return -1;
}
uint32_t *desk_image_job_take(struct desk_image_job *j) {
    if(!j->ready || j->response.status) return NULL;
    uint32_t *pixels=j->pixels;j->pixels=NULL;j->ready=0;return pixels;
}
