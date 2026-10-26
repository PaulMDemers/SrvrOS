#include <srvros/desk_runtime.h>
#include <srvros/desk_file_job.h>
#include <srvros/desk_image_job.h>
#include <srvros/sys.h>
void desk_runtime_file_pool(struct desk_runtime *r,const struct desk_file_pool *pool) {
    struct srv_gui_wait interests={0};
    if(desk_file_pool_interests(pool,&interests)!=1) {desk_runtime_retry(r);return;}
    for(unsigned i=0;i<interests.count;i++)desk_runtime_watch(r,(int)interests.fds[i].fd,interests.fds[i].events);
}
void desk_runtime_file_job(struct desk_runtime *r,const struct desk_file_job *j) {
    if(j->pool) {desk_runtime_file_pool(r,j->pool);return;}
    if(!j->busy)return;
    if(j->broken || (j->received>=sizeof(j->response) && j->received==sizeof(j->response)+j->response.length)) {desk_runtime_retry(r);return;}
    int writing=j->sent<sizeof(j->request)+j->request.length;
    desk_runtime_watch(r,writing?j->writer:j->reader,writing?SRV_POLLOUT:SRV_POLLIN);
}
void desk_runtime_image_job(struct desk_runtime *r,const struct desk_image_job *j) {
    if(j->retiring_pid)desk_runtime_retry(r);
    if(!j->busy)return;
    if(j->received>=sizeof(j->response) && j->received==sizeof(j->response)+j->response.length) {desk_runtime_retry(r);return;}
    int writing=j->sent<sizeof(j->request);
    desk_runtime_watch(r,writing?j->writer:j->reader,writing?SRV_POLLOUT:SRV_POLLIN);
}
