#ifndef SRVROS_DESK_LAUNCH_OWNER_H
#define SRVROS_DESK_LAUNCH_OWNER_H
#include <srvros/sys.h>
#include <srvros/profile_session.h>
#include <signal.h>
#include <string.h>
/* One private request pipe and one reply pipe; neither is inherited by apps.
 * The small server is the actual parent and reaps only its recorded children.
 * 64 is the existing global process-table capacity, not a new app quota. */
#define DESK_LAUNCH_CHILDREN 64
#define DESK_LAUNCH_MAGIC 0x4c41554e434831ull
struct desk_launch_request {
    uint64_t magic,sequence,operation,pid,flags;
    char program[160],role[32],path[160];
    struct srv_profile_session profile;
};
_Static_assert(sizeof(struct desk_launch_request)<=4096,"one existing atomic pipe write");
struct desk_launch_reply { uint64_t magic,sequence,status;int64_t result; };
struct desk_launch_owner {
    long pid;int reader,writer,failed;
    uint64_t sequence,children[DESK_LAUNCH_CHILDREN],launches,reaps;
};
static inline int desk_launch_exact(int fd,void *data,size_t bytes,int writing) {
    size_t done=0;
    while(done<bytes) {
        long n=writing?srv_write(fd,(char *)data+done,bytes-done):srv_read(fd,(char *)data+done,bytes-done);
        if(n==SRV_ERR_INTR)continue;
        if(n==SRV_ERR_AGAIN) {
            struct srv_pollfd ready={.fd=fd,.events=writing?SRV_POLLOUT:SRV_POLLIN};
            long polled=srv_poll(&ready,1,-1);
            if(polled>0 || polled==SRV_ERR_INTR)continue;
            return -1;
        }
        if(n<=0 || (size_t)n>bytes-done)return -1;
        done+=(size_t)n;
    }
    return 0;
}
static inline int desk_launch_index(const uint64_t *children,uint64_t pid) {
    for(unsigned i=0;i<DESK_LAUNCH_CHILDREN;i++)if(__atomic_load_n(&children[i],__ATOMIC_ACQUIRE)==pid)return (int)i;
    return -1;
}
static inline int desk_launch_rpc(struct desk_launch_owner *o,struct desk_launch_request *q,struct desk_launch_reply *a) {
    if(o->failed || !o->pid || o->sequence==UINT64_MAX)return -1;
    q->magic=DESK_LAUNCH_MAGIC;q->sequence=++o->sequence;
    if(desk_launch_exact(o->writer,q,sizeof(*q),1) || desk_launch_exact(o->reader,a,sizeof(*a),0) ||
       a->magic!=DESK_LAUNCH_MAGIC || a->sequence!=q->sequence) {o->failed=1;return -1;}
    return 0;
}
static inline int desk_launch_owner_start(struct desk_launch_owner *o) {
    *o=(struct desk_launch_owner){.reader=-1,.writer=-1};
    int in[2]={-1,-1},out[2]={-1,-1};long pid=-1;
    if(srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE) || srv_pipe(in) || srv_pipe(out))goto fail;
    for(unsigned i=0;i<2;i++)if(srv_fcntl(in[i],SRV_F_SETFL,SRV_FD_NONBLOCK)<0 ||
        srv_fcntl(out[i],SRV_F_SETFL,SRV_FD_NONBLOCK)<0)goto fail;
    struct srv_spawn_file_action actions[2]={
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=in[0],.new_fd=3},
        {.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=out[1],.new_fd=4}};
    char *argv[]={"/fat/bin/deskclient","--launch-owner",NULL};
    struct srv_exec_request r={.path=argv[0],.argv=argv,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1,.file_actions=actions,.file_action_count=2};
    pid=srv_exec(&r);
    if(pid<=0)goto fail;
    o->pid=pid;o->reader=out[0];o->writer=in[1];out[0]=in[1]=-1;
fail:
    for(unsigned i=0;i<2;i++){if(in[i]>=0)srv_close(in[i]);if(out[i]>=0)srv_close(out[i]);}
    if(pid<=0)return -1;
    struct desk_launch_request q={0};struct desk_launch_reply a;
    if(desk_launch_rpc(o,&q,&a) || a.result!=pid) {o->failed=1;return -1;}
    return 0;
}
static inline long desk_launch_owner_launch(struct desk_launch_owner *o,const char *program,const char *role,const char *path,const struct srv_profile_session *profile) {
    int slot=desk_launch_index(o->children,0);
    if(slot<0 || strlen(program)>=160 || strlen(role)>=32 || strlen(path)>=160)return SRV_ERR_INVAL;
    struct desk_launch_request q={.operation=1,.profile=*profile};struct desk_launch_reply a;
    strcpy(q.program,program);strcpy(q.role,role);strcpy(q.path,path);
    if(desk_launch_rpc(o,&q,&a))return SRV_ERR_IO;
    if(a.result>0){__atomic_store_n(&o->children[slot],(uint64_t)a.result,__ATOMIC_RELEASE);o->launches++;}
    return a.result;
}
/* Called synchronously only by the worker, or after its join at shutdown. */
static inline long desk_launch_owner_wait(struct desk_launch_owner *o,uint64_t pid,uint64_t *status,uint64_t flags) {
    int slot=desk_launch_index(o->children,pid);
    if(slot<0)return srv_wait(pid,status,flags);
    struct desk_launch_request q={.operation=2,.pid=pid,.flags=flags};struct desk_launch_reply a;
    if(desk_launch_rpc(o,&q,&a))return SRV_ERR_IO;
    if(a.result==(int64_t)pid){__atomic_store_n(&o->children[slot],0,__ATOMIC_RELEASE);o->reaps++;if(status)*status=a.status;}
    return a.result;
}
static inline int desk_launch_owner_finish(struct desk_launch_owner *o) {
    if(!o->pid)return 0;
    int clean=!o->failed;
    for(unsigned i=0;i<DESK_LAUNCH_CHILDREN;i++)if(o->children[i])clean=0;
    if(clean) {
        struct desk_launch_request q={.operation=3};struct desk_launch_reply a;
        if(desk_launch_rpc(o,&q,&a) || a.result)clean=0;
    }
    /* EOF also handles a lost reply: the server owns and drains even a child
     * whose PID was never delivered. Never retry an ambiguous launch. */
    if(o->writer>=0)srv_close(o->writer);
    if(o->reader>=0)srv_close(o->reader);
    o->writer=o->reader=-1;
    uint64_t status=1;long waited;
    do {waited=srv_wait((uint64_t)o->pid,&status,0);}while(waited==SRV_ERR_INTR);
    if(waited!=o->pid || status)clean=0;
    o->pid=0;return clean?0:-1;
}
static inline int desk_launch_owner_serve(void) {
    uint64_t children[DESK_LAUNCH_CHILDREN]={0},sequence=0;
    int result=1;
    if(srv_signal_config(SIGPIPE,SRV_SIGNAL_IGNORE))return 1;
    for(;;) {
        struct desk_launch_request q;struct desk_launch_reply a={.magic=DESK_LAUNCH_MAGIC,.result=SRV_ERR_INVAL};
        if(desk_launch_exact(3,&q,sizeof(q),0))break;
        if(q.magic!=DESK_LAUNCH_MAGIC || sequence==UINT64_MAX || q.sequence!=++sequence)break;
        a.sequence=q.sequence;
        if(q.operation==0)a.result=srv_getpid();
        else if(q.operation==1) {
            int slot=desk_launch_index(children,0);
            struct srv_profile_environment env;
            if(slot>=0 && q.program[0]=='/' && memchr(q.program,0,sizeof(q.program)) &&
               memchr(q.role,0,sizeof(q.role)) && memchr(q.path,0,sizeof(q.path)) &&
               memchr(q.profile.home,0,sizeof(q.profile.home)) && memchr(q.profile.id,0,sizeof(q.profile.id)) &&
               srv_profile_environment_init(&env,&q.profile,"/fat/bin:/:/fat")) {
                char *argv[]={q.program,q.role,q.path,NULL};
                struct srv_exec_request r={.path=q.program,.argv=argv,.envp=env.envp,.flags=SRV_EXEC_BACKGROUND,
                    .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
                a.result=srv_exec(&r);
                if(a.result>0)children[slot]=(uint64_t)a.result;
            }
        } else if(q.operation==2 && !(q.flags&~SRV_WAIT_NOHANG)) {
            int slot=desk_launch_index(children,q.pid);
            if(q.pid && slot>=0) {
                a.result=srv_wait(q.pid,&a.status,q.flags);
                if(a.result==(int64_t)q.pid)children[slot]=0;
            }
        } else if(q.operation==3) {
            a.result=0;
            for(unsigned i=0;i<DESK_LAUNCH_CHILDREN;i++)if(children[i])a.result=SRV_ERR_BUSY;
        }
        if(desk_launch_exact(4,&a,sizeof(a),1))break;
        if(q.operation==3 && !a.result){result=0;break;}
    }
    /* Broken caller/channel: keep real child ownership until final wait. */
    for(unsigned i=0;i<DESK_LAUNCH_CHILDREN;i++)if(children[i])srv_kill((int64_t)children[i]);
    for(unsigned i=0;i<DESK_LAUNCH_CHILDREN;i++)if(children[i]) {
        uint64_t status;long waited;
        do {waited=srv_wait(children[i],&status,0);}while(waited==SRV_ERR_INTR);
        if(waited!=(long)children[i])result=1;
    }
    srv_close(3);srv_close(4);return result;
}
#endif
