#include <srvros/desk_file_job.h>
#include <srvros/sys.h>
#include <srvros/desktop.h>
#include <string.h>
#include <signal.h>

int desk_file_job_init(struct desk_file_job *j) {
    *j=(struct desk_file_job){.reader=-1,.writer=-1};
    int input[2],output[2];
    if(srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE)!=0 || srv_pipe(input)) return -1;
    if(srv_pipe(output)) { srv_close(input[0]);srv_close(input[1]);return -1; }
    j->reader=output[0];j->writer=input[1];
    if(srv_fcntl(j->reader,SRV_F_SETFL,SRV_FD_NONBLOCK)<0 || srv_fcntl(j->writer,SRV_F_SETFL,SRV_FD_NONBLOCK)<0) {
        srv_close(input[0]);srv_close(output[1]);desk_file_job_close(j);return -1;
    }
    long pid=srv_spawn_bg_args_fds("/fat/bin/deskfs","",input[0],output[1]);
    srv_close(input[0]);srv_close(output[1]);
    if(pid<=0) { desk_file_job_close(j);return -1; }
    j->pid=pid;return 0;
}
int desk_file_job_start(struct desk_file_job *j,unsigned operation,const char *path,
    const void *data,size_t length,unsigned flags,uint32_t expected_crc) {
    if(j->busy || j->broken || !j->pid || !path || path[0]!='/' || strlen(path)>=DESK_FILE_PATH ||
       operation<DESK_FILE_READ || operation>DESK_FILE_MKDIR || length>DESK_FILE_BYTES ||
       (length && !data) || (operation!=DESK_FILE_WRITE && length) || flags>DESK_FILE_EXPECT_EXISTING) return -1;
    j->request=(struct desk_file_request){.magic=DESK_FILE_MAGIC,.id=++j->next_id,
        .operation=operation,.length=length,.flags=flags,.expected_crc=expected_crc};
    strcpy(j->request.path,path);
    if(length) memcpy(j->outgoing,data,length);
    j->response=(struct desk_file_response){0};j->sent=j->received=0;j->busy=1;return 0;
}
int desk_file_job_poll(struct desk_file_job *j) {
    if(j->broken) return -1;
    if(!j->busy) return 0;
    for(unsigned batch=0;batch<4 && j->sent<sizeof(j->request)+j->request.length;batch++) {
        const void *data;size_t bytes;
        if(j->sent<sizeof(j->request)) { data=(char *)&j->request+j->sent;bytes=sizeof(j->request)-j->sent; }
        else { data=j->outgoing+j->sent-sizeof(j->request);bytes=sizeof(j->request)+j->request.length-j->sent; }
        if(bytes>4096) bytes=4096;
        long n=srv_write(j->writer,data,bytes);
        if(n==SRV_ERR_AGAIN || n==SRV_ERR_INTR) break;
        if(n<=0 || (size_t)n>bytes) goto failed;
        j->sent+=n;
    }
    for(unsigned batch=0;batch<4;batch++) {
        void *data;size_t bytes;
        if(j->received<sizeof(j->response)) { data=(char *)&j->response+j->received;bytes=sizeof(j->response)-j->received; }
        else { data=j->incoming+j->received-sizeof(j->response);bytes=sizeof(j->response)+j->response.length-j->received; }
        if(!bytes) {
            if(j->sent!=sizeof(j->request)+j->request.length) goto failed;
            if(desk_file_crc(j->incoming,j->response.length)!=j->response.crc) goto failed;
            j->busy=0;return 1;
        }
        if(bytes>4096) bytes=4096;
        long n=srv_read(j->reader,data,bytes);
        if(n==SRV_ERR_AGAIN || n==SRV_ERR_INTR) break;
        if(n<=0 || (size_t)n>bytes) goto failed;
        j->received+=n;
        if(j->received==sizeof(j->response)) {
            if(j->response.magic!=DESK_FILE_MAGIC || j->response.id!=j->request.id ||
               j->response.length>DESK_FILE_BYTES || j->response.count>DESK_FILE_ENTRIES ||
               j->response.status>DESK_FILE_NOT_FOUND || j->response.truncated>1 ||
               (j->request.operation==DESK_FILE_LIST && j->response.length!=j->response.count*sizeof(struct desk_file_entry))) goto failed;
        }
    }
    if(j->pid && !desk_process_alive(j->pid)) goto failed;
    return 0;
failed:
    j->busy=0;j->broken=1;return -1;
}
void desk_file_job_close(struct desk_file_job *j) {
    if(j->writer>=0) srv_close(j->writer);
    if(j->reader>=0) srv_close(j->reader);
    j->writer=j->reader=-1;
    /* Closing input lets an idle worker exit. A busy writer is allowed to
     * finish its already accepted operation; never terminate a file mutation. */
    if(j->pid) { uint64_t status;srv_wait(j->pid,&status,SRV_WAIT_NOHANG); }
    j->pid=0;
}
