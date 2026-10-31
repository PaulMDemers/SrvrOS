#include <srvros/desk_file_job.h>
#include <srvros/sys.h>
#include <srvros/desktop.h>
#include <string.h>
#include <signal.h>
#include <limits.h>

static int transport_cancelled(const unsigned *cancel) {return cancel && __atomic_load_n(cancel,__ATOMIC_ACQUIRE);}
int desk_file_transport_open_cancelable(struct desk_file_transport *t,const unsigned *cancel) {
    if(!t)return -1;
    *t=(struct desk_file_transport){.reader=-1,.writer=-1};
    int input[2]={-1,-1},output[2]={-1,-1},result=-1;
    if(transport_cancelled(cancel))return DESK_FILE_TRANSPORT_CANCELLED;
    if(srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE)!=0)goto done;
    if(transport_cancelled(cancel)){result=DESK_FILE_TRANSPORT_CANCELLED;goto done;}
    if(srv_pipe(input))goto done;
    if(transport_cancelled(cancel)){result=DESK_FILE_TRANSPORT_CANCELLED;goto done;}
    if(srv_pipe(output))goto done;
    if(transport_cancelled(cancel)){result=DESK_FILE_TRANSPORT_CANCELLED;goto done;}
    if(srv_fcntl(output[0],SRV_F_SETFL,SRV_FD_NONBLOCK)<0)goto done;
    if(transport_cancelled(cancel)){result=DESK_FILE_TRANSPORT_CANCELLED;goto done;}
    if(srv_fcntl(input[1],SRV_F_SETFL,SRV_FD_NONBLOCK)<0)goto done;
    if(transport_cancelled(cancel)){result=DESK_FILE_TRANSPORT_CANCELLED;goto done;}
    long pid=srv_spawn_bg_args_fds("/fat/bin/deskfs","",input[0],output[1]);
    if(pid<=0)goto done;
    /* Spawn may have completed after cancellation. Publish the owned child;
     * its receiver must close/reap it rather than pretend it never existed. */
    t->pid=pid;t->reader=output[0];t->writer=input[1];output[0]=input[1]=-1;result=0;
done:
    for(unsigned i=0;i<2;i++){if(input[i]>=0)srv_close(input[i]);if(output[i]>=0)srv_close(output[i]);}
    return result;
}
int desk_file_transport_open(struct desk_file_transport *t) {return desk_file_transport_open_cancelable(t,NULL);}
int desk_file_job_init(struct desk_file_job *j) {
    struct desk_file_transport t;int result=desk_file_transport_open(&t);
    *j=(struct desk_file_job){.reader=t.reader,.writer=t.writer,.pid=t.pid};return result;
}
int desk_file_pool_init(struct desk_file_pool *p,struct desk_file_channel *channels,unsigned count) {
    if(!p || !channels || !count || count>DESK_FILE_POOL_MAX_CHANNELS)return -1;
    *p=(struct desk_file_pool){.channels=channels,.count=count};
    for(unsigned i=0;i<count;i++)channels[i]=(struct desk_file_channel){.reader=-1,.writer=-1};
    return 0;
}
int desk_file_pool_opener(struct desk_file_pool *p,int (*opener)(void *,struct desk_file_channel *),void *context) {
    if(!p || !p->channels || p->bound || p->live || p->head || p->closing)return -1;
    p->opener=opener;p->opener_context=context;return 0;
}
int desk_file_job_bind(struct desk_file_job *j,struct desk_file_pool *p) {
    if(!j || !p || !p->channels || !p->count || p->closing || p->bound==UINT_MAX || j->pool || j->pid || j->busy)return -1;
    *j=(struct desk_file_job){.reader=-1,.writer=-1,.pool=p};p->bound++;return 0;
}
int desk_file_job_detach(struct desk_file_job *j) {
    if(!j || !j->pool || j->busy || j->channel)return -1;
    j->pool->bound--;j->pool=NULL;return 0;
}
static void channel_reap(struct desk_file_pool *p,struct desk_file_channel *c) {
    if(!c->retiring || !c->pid)return;
    uint64_t status=0;long result=srv_wait(c->pid,&status,SRV_WAIT_NOHANG);
    if(result==(long)c->pid || (result<0 && !desk_process_alive(c->pid))) {
        c->pid=0;c->retiring=0;p->reaped++;p->live--;
    }
}
static void channel_close(struct desk_file_channel *c) {
    if(!c->retiring) {
        if(c->writer>=0)srv_close(c->writer);
        if(c->reader>=0)srv_close(c->reader);
        c->writer=c->reader=-1;c->owner=NULL;c->retiring=c->pid!=0;
    }
}
static void channel_retire(struct desk_file_pool *p,struct desk_file_channel *c) {
    channel_close(c);channel_reap(p,c);
}
int desk_file_pool_shutdown(struct desk_file_pool *p) {
    if(!p || !p->channels || p->bound || p->head)return -1;
    p->closing=1;
    for(unsigned i=0;i<p->count;i++)channel_retire(p,&p->channels[i]);
    return p->live?0:1;
}
int desk_file_pool_finish(struct desk_file_pool *p) {
    if(!p || !p->channels || p->bound || p->head)return -1;
    p->closing=1;
    /* Final owner only: publish EOF on every idle channel before waiting for
     * any child. No accepted job remains and no GUI dispatch depends on us.
     * Blocking wait gets normal kernel admission instead of repeated NOHANG
     * deferrals while a different process owns I/O. */
    for(unsigned i=0;i<p->count;i++)channel_close(&p->channels[i]);
    for(unsigned i=0;i<p->count;i++) {
        struct desk_file_channel *c=&p->channels[i];
        if(!c->pid)continue;
        uint64_t status=0;long result=srv_wait(c->pid,&status,0);
        if(result==(long)c->pid) {c->pid=0;c->retiring=0;p->reaped++;p->live--;}
        else return result==0 || result==SRV_ERR_INTR?0:-1;
    }
    return p->live?0:1;
}
int desk_file_pool_interests(const struct desk_file_pool *p,struct srv_gui_wait *r) {
    if(!p || !p->channels || !r)return -1;
    r->count=0;
    for(unsigned i=0;i<p->count;i++) {
        const struct desk_file_channel *c=&p->channels[i];
        const struct desk_file_job *j=c->owner;
        if(c->retiring || (p->head && !j))return 0;
        if(!j)continue;
        if(j->received>=sizeof(j->response) && j->received==sizeof(j->response)+j->response.length)return 0;
        if(r->count==SRV_GUI_WAIT_FDS)return -1;
        int writing=j->sent<sizeof(j->request)+j->request.length;
        r->fds[r->count++]=(struct srv_gui_wait_fd){.fd=writing?c->writer:c->reader,.events=writing?SRV_POLLOUT:SRV_POLLIN};
    }
    return 1;
}
static void dequeue(struct desk_file_job *j) {
    struct desk_file_pool *p=j->pool;p->head=j->queued_next;
    if(!p->head)p->tail=NULL;
    j->queued_next=NULL;
}
/* Only the FIFO head may acquire a lease. Owner polling drives bounded progress;
 * no job's completion is consumed by a sibling document's dispatcher turn. */
static int acquire(struct desk_file_job *j) {
    struct desk_file_pool *p=j->pool;
    if(j->channel)return 1;
    if(p->head!=j)return 0;
    for(unsigned i=0;i<p->count;i++) {
        struct desk_file_channel *c=&p->channels[i];
        channel_reap(p,c);
        if(c->owner || c->retiring)continue;
        if(c->pid && !desk_process_alive(c->pid))channel_retire(p,c);
        if(c->retiring)continue;
        if(!c->pid) {
            if(p->opener) {
                int result=p->opener(p->opener_context,c);
                if(result<0)dequeue(j);
                if(result<=0)return result;
            } else {
                struct desk_file_transport t;
                if(desk_file_transport_open(&t)) {dequeue(j);return -1;}
                c->pid=t.pid;c->reader=t.reader;c->writer=t.writer;
            }
            p->spawned++;p->live++;if(p->live>p->peak)p->peak=p->live;
        }
        j->pid=c->pid;j->reader=c->reader;j->writer=c->writer;
        c->owner=j;j->channel=c;dequeue(j);return 1;
    }
    return 0;
}
static void release(struct desk_file_job *j,int failed) {
    if(!j->pool || !j->channel)return;
    if(failed)channel_retire(j->pool,j->channel);else j->channel->owner=NULL;
    j->channel=NULL;j->pid=0;j->reader=j->writer=-1;
}
static int fail(struct desk_file_job *j) {
    j->busy=0;j->broken=1;release(j,1);return -1;
}
int desk_file_job_start(struct desk_file_job *j,unsigned operation,const char *path,
    const void *data,size_t length,unsigned flags,uint32_t expected_crc) {
    if(j->busy || (j->pool?(j->pool->closing || j->pool->next_id==UINT32_MAX):(j->broken || !j->pid || j->next_id==UINT32_MAX)) || !path || path[0]!='/' || strlen(path)>=DESK_FILE_PATH ||
       length>DESK_FILE_BYTES || (length && !data) || !desk_file_payload_valid(operation,(unsigned)length,flags)) return -1;
    /* Reusing a prior request path or incoming/outgoing slice is valid. Snapshot
     * before resetting headers; memmove also covers overlapping output slices. */
    char saved_path[DESK_FILE_PATH];strcpy(saved_path,path);
    if(length)memmove(j->outgoing,data,length);
    j->request=(struct desk_file_request){.magic=DESK_FILE_MAGIC,.id=j->pool?++j->pool->next_id:++j->next_id,
        .operation=operation,.length=length,.flags=flags,.expected_crc=expected_crc};
    strcpy(j->request.path,saved_path);
    j->response=(struct desk_file_response){0};j->sent=j->received=0;j->busy=1;j->broken=0;
    if(j->pool) {
        j->queued_next=NULL;
        if(j->pool->tail)j->pool->tail->queued_next=j;else j->pool->head=j;
        j->pool->tail=j;
    }
    return 0;
}
int desk_file_job_cancel_unsubmitted(struct desk_file_job *j) {
    if(!j || !j->busy || !j->pool || j->pool->closing || j->channel || j->sent || j->received ||
       (j->request.operation!=DESK_FILE_READ && j->request.operation!=DESK_FILE_LIST && j->request.operation!=DESK_FILE_STAT))return 0;
    struct desk_file_pool *p=j->pool;struct desk_file_job **link=&p->head,*previous=NULL;
    for(unsigned n=0;n<p->bound && *link && *link!=j;n++){previous=*link;link=&previous->queued_next;}
    if(*link!=j)return 0;
    *link=j->queued_next;if(p->tail==j)p->tail=previous;
    j->queued_next=NULL;j->busy=0;return 1;
}
int desk_file_job_submit(struct desk_file_job *j) {
    if(j->broken) return -1;
    if(!j->busy) return 0;
    if(j->pool) { int result=acquire(j);if(result<0)return fail(j);if(!result)return 0; }
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
    /* A completed reply can outlive its worker. Let poll validate/drain it
     * before declaring worker death, as it did before split submission. */
    if(j->sent<sizeof(j->request)+j->request.length && j->pid && !desk_process_alive(j->pid))goto failed;
    return j->sent==sizeof(j->request)+j->request.length;
failed:return fail(j);
}
int desk_file_job_poll(struct desk_file_job *j) {
    if(desk_file_job_submit(j)<0)return -1;
    if(!j->busy || (j->pool && !j->channel))return 0;
    size_t received_before=j->received;
    for(unsigned batch=0;batch<4;batch++) {
        void *data;size_t bytes;
        if(j->received<sizeof(j->response)) { data=(char *)&j->response+j->received;bytes=sizeof(j->response)-j->received; }
        else { data=j->incoming+j->received-sizeof(j->response);bytes=sizeof(j->response)+j->response.length-j->received; }
        if(!bytes) {
            if(j->sent!=sizeof(j->request)+j->request.length) goto failed;
            if(desk_file_crc(j->incoming,j->response.length)!=j->response.crc) goto failed;
            j->busy=0;release(j,0);return 1;
        }
        if(bytes>4096) bytes=4096;
        long n=srv_read(j->reader,data,bytes);
        if(n==SRV_ERR_AGAIN || n==SRV_ERR_INTR) break;
        if(n<=0 || (size_t)n>bytes) goto failed;
        j->received+=n;
        if(j->received==sizeof(j->response)) {
            if(j->response.magic!=DESK_FILE_MAGIC || j->response.id!=j->request.id ||
               j->response.length>DESK_FILE_BYTES || j->response.count>DESK_FILE_ENTRIES ||
               j->response.status>DESK_FILE_BUSY || j->response.truncated>1 ||
               (j->request.operation==DESK_FILE_LIST && j->response.length!=j->response.count*sizeof(struct desk_file_entry)) ||
               (j->request.operation==DESK_FILE_STAT && (j->response.count || j->response.truncated || j->response.length!=(j->response.status?0:sizeof(struct desk_file_info))))) goto failed;
        }
    }
    /* A validated reply may span several bounded batches after its writer
     * exits. Continue draining actual progress; a dead, stalled or truncated
     * stream still fails without treating an uncertain write as retryable. */
    if(j->received==received_before && j->pid && !desk_process_alive(j->pid)) goto failed;
    return 0;
failed:
    return fail(j);
}
void desk_file_job_close(struct desk_file_job *j) {
    if(j->pool) { (void)desk_file_job_detach(j);return; }
    if(j->writer>=0) srv_close(j->writer);
    if(j->reader>=0) srv_close(j->reader);
    j->writer=j->reader=-1;
    /* Closing input lets an idle worker exit. A busy writer is allowed to
     * finish its already accepted operation; never terminate a file mutation. */
    if(j->pid) { uint64_t status;srv_wait(j->pid,&status,SRV_WAIT_NOHANG); }
    j->pid=0;
}

int desk_file_job_retire(struct desk_file_job *j) {
    if(!j)return -1;
    if(j->busy)return 0;
    if(j->pool)return desk_file_job_detach(j)?-1:1;
    if(j->writer>=0)srv_close(j->writer);
    if(j->reader>=0)srv_close(j->reader);
    j->writer=j->reader=-1;j->broken=1;
    if(j->pid) {
        uint64_t status=0;long result=srv_wait(j->pid,&status,SRV_WAIT_NOHANG);
        if(result==(long)j->pid || (result<0 && !desk_process_alive(j->pid)))j->pid=0;
        else return 0;
    }
    return 1;
}
