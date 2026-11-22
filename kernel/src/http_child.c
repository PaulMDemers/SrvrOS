#include <srvros/http_child.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <limits.h>
#include <stddef.h>
static bool owner_valid(const struct http_child_owner *o) {
    return o && o->self==(uintptr_t)o && !o->retired && !process_current();
}
static bool owned(const struct http_child_owner *o) {
    return owner_valid(o) && o->token &&
        o->pipe.self==(uintptr_t)&o->pipe && o->pipe.token==o->token && !process_current();
}
static bool recipient(const struct http_child_owner *o) {
    struct process_pipe_identity identity={0};
    return owned(o) && process_pipe_handoff_bound(&o->pipe,&identity) &&
        identity.pid==o->pid && identity.lifetime==o->token &&
        identity.pid==o->identity.pid && identity.lifetime==o->identity.lifetime &&
        identity.image_generation==o->identity.image_generation;
}
static void put(uint8_t *p,uint64_t value,unsigned bytes) {
    for(unsigned i=0;i<bytes;i++)p[i]=(uint8_t)(value>>(i*8));
}
bool http_child_prepare(struct http_child_owner *o,bool preflight) {
    if(!o || o->self || o->token || o->pipe.self || o->pipe.token || process_current())return false;
    o->self=(uintptr_t)o;o->token=process_lifetime_create();
    if(!o->token)return false;
    int64_t pid=process_lifetime_spawn_pipe(o->token,"/httpget",
        preflight?"--wifi-preflight 3":"--wifi-exact 3",&o->pipe);
    if(pid<=0)return false;
    o->pid=(uint64_t)pid;
    if(!process_pipe_handoff_bound(&o->pipe,&o->identity))return false;
    if(process_lifetime_of(o->pid)!=o->token || !process_wait_startup(o->pid,500) ||
       !process_pipe_handoff_identity(&o->pipe,&o->identity) ||
       o->identity.pid!=o->pid || o->identity.lifetime!=o->token || !o->identity.image_generation ||
       !process_lifetime_revoke(o->token))return false;
    o->ready_us=timer_monotonic_us();o->prepared=true;return true;
}
bool http_child_make_go(struct http_child_owner *o,uint64_t epoch,uint64_t start,
    uint64_t deadline,uint32_t ip,uint8_t record[HTTP_CHILD_GO_BYTES]) {
    if(!record || !recipient(o) || !o->prepared || o->published || !epoch ||
       start<o->ready_us || start>INT64_MAX-HTTP_CHILD_WINDOW_US ||
       deadline!=start+HTTP_CHILD_WINDOW_US || !ip || !(ip>>24) ||
       (ip>>24)==127 || (ip>>24)>=224 || !process_pipe_handoff_current(&o->pipe))return false;
    uint64_t now=timer_monotonic_us();if(now<start || now>=deadline)return false;
    struct process_pipe_identity identity={0};
    if(!process_pipe_handoff_identity(&o->pipe,&identity) || identity.pid!=o->identity.pid ||
       identity.lifetime!=o->identity.lifetime || identity.image_generation!=o->identity.image_generation)return false;
    uint8_t bytes[HTTP_CHILD_GO_BYTES]={0};const uint8_t magic[8]={'S','R','V','R','H','T','P','1'};
    for(unsigned i=0;i<8;i++)bytes[i]=magic[i];
    put(bytes+8,1,4);put(bytes+12,HTTP_CHILD_GO_BYTES,4);put(bytes+16,o->pid,8);
    put(bytes+24,o->token,8);put(bytes+32,o->identity.image_generation,8);
    put(bytes+40,epoch,8);put(bytes+48,start,8);put(bytes+56,deadline,8);
    put(bytes+64,ip,4);put(bytes+68,80,2);
    for(unsigned i=0;i<sizeof bytes;i++)record[i]=bytes[i];
    return true;
}
bool http_child_publish(struct http_child_owner *o,uint64_t epoch,uint64_t start,
    uint64_t deadline,uint32_t ip) {
    uint8_t record[HTTP_CHILD_GO_BYTES];
    if(!http_child_make_go(o,epoch,start,deadline,ip,record))return false;
    bool good=process_pipe_handoff_publish(&o->pipe,record,sizeof record)==(int64_t)sizeof record;
    for(unsigned i=0;i<sizeof record;i++)record[i]=0;
    if(good){o->published=true;o->epoch=epoch;o->start=start;o->deadline=deadline;}
    return good;
}
bool http_child_observe(struct http_child_owner *o,uint64_t *status) {
    if(!status || !recipient(o) || !o->pid || o->reaped)return false;
    uint64_t value=0;if(!process_pipe_handoff_retired(&o->pipe,&value))return false;
    o->observed=true;o->status=value;*status=value;return true;
}
bool http_child_cancel(struct http_child_owner *o) {
    if(!recipient(o))return false;
    bool sealed=process_pipe_handoff_finish(&o->pipe) && process_lifetime_revoke(o->token);
    if(o->pid && !o->observed)(void)process_signal_pid(o->pid,SRV_SIGNAL_KILL);
    return sealed;
}
bool http_child_reap(struct http_child_owner *o) {
    uint64_t status;
    if(!recipient(o) || o->reaped || !http_child_observe(o,&status))return false;
    uint64_t waited=UINT64_MAX;int64_t result=process_wait(o->pid,&waited,true);
    if(result!=(int64_t)o->pid || waited!=status)return false;
    o->reaped=true;return true;
}
bool http_child_retire(struct http_child_owner *o) {
    if(!owner_valid(o))return false;
    if(!o->token){o->retired=true;return true;}
    if(o->pipe.token && !owned(o))return false;
    /* Failed prelive spawn may lack a claimed pipe but still owns lifetime. */
    bool pipe_done=!o->pipe.token || process_pipe_handoff_finish(&o->pipe);
    struct process_lifetime_status s;
    if(!pipe_done || !process_lifetime_revoke(o->token) ||
       !process_lifetime_drain(o->token) || !process_lifetime_status(o->token,&s) || s.members ||
       !process_lifetime_release(o->token))return false;
    o->retired=true;return true;
}
