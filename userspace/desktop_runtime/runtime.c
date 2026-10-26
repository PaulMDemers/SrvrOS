#include <srvros/desk_runtime.h>
#include <srvros/sys.h>
#include <string.h>
#include <limits.h>
static uint64_t heartbeat(uint64_t now) {
    return now>UINT64_MAX-DESK_RUNTIME_HEARTBEAT_US?UINT64_MAX:now+DESK_RUNTIME_HEARTBEAT_US;
}
void desk_runtime_init(struct desk_runtime *r,struct desk_connection *c) {
    *r=(struct desk_runtime){.connection=c};
}
void desk_runtime_retry(struct desk_runtime *r) { r->retry=1; }
void desk_runtime_deadline(struct desk_runtime *r,uint64_t deadline) {
    if(deadline<r->wait.deadline_us)r->wait.deadline_us=deadline;
}
int desk_runtime_watch(struct desk_runtime *r,int fd,unsigned events) {
    if(fd<0 || !events || (events&~(SRV_POLLIN|SRV_POLLOUT))) {r->retry=1;return -1;}
    for(unsigned i=0;i<r->wait.count;i++)if(r->wait.fds[i].fd==fd) {
        r->wait.fds[i].events|=events;return 0;
    }
    if(r->wait.count==SRV_GUI_WAIT_FDS) {r->retry=1;return -1;}
    r->wait.fds[r->wait.count++]=(struct srv_gui_wait_fd){.fd=fd,.events=events};return 0;
}
void desk_runtime_timer_cancel(struct desk_runtime_timer *t) {
    if(!t || !t->owner)return;
    struct desk_runtime_timer **link=&t->owner->timers;
    while(*link && *link!=t)link=&(*link)->next;
    if(*link)*link=t->next;
    t->owner=NULL;t->next=NULL;
}
int desk_runtime_timer_arm(struct desk_runtime *r,struct desk_runtime_timer *t,uint64_t at,
                           void (*fire)(void *,uint64_t),void *context) {
    if(!r || !t || !fire || (t->owner && t->owner!=r))return -1;
    desk_runtime_timer_cancel(t);t->owner=r;t->deadline_us=at;t->fire=fire;t->context=context;
    struct desk_runtime_timer **link=&r->timers;
    while(*link && (*link)->deadline_us<=at)link=&(*link)->next;
    t->next=*link;*link=t;return 0;
}
void desk_runtime_timers_clear(struct desk_runtime *r) {
    while(r->timers)desk_runtime_timer_cancel(r->timers);
}
int desk_runtime_dispatch(struct desk_runtime *r,desk_runtime_event event,void *context) {
    if(!r || !r->connection || !event || r->dispatching || r->failed)return -1;
    r->dispatching=1;r->retry=0;
    long clock=srv_monotonic_us();
    r->wait=(struct srv_gui_wait){.deadline_us=clock<0?0:heartbeat((uint64_t)clock)};
    if(clock<0)r->retry=1;
    unsigned batch;
    for(batch=0;batch<DESK_RUNTIME_BATCH;batch++) {
        struct desk_app *window;struct desk_packet p;
        int result=desk_connection_poll(r->connection,&window,&p);
        if(result<0) {r->failed=1;break;}
        if(!result)break;
        if(result==2)continue;
        int dispatched=event(context,window,p);
        if(dispatched<0) {r->failed=1;break;}
        if(dispatched) {r->retry=1;break;}
    }
    if(batch==DESK_RUNTIME_BATCH)r->retry=1;
    clock=srv_monotonic_us();if(clock<0)r->retry=1;
    for(unsigned n=0;!r->failed && clock>=0 && r->timers && r->timers->deadline_us<=(uint64_t)clock && n<DESK_RUNTIME_BATCH;n++) {
        struct desk_runtime_timer *t=r->timers;
        void (*fire)(void *,uint64_t)=t->fire;void *arg=t->context;
        desk_runtime_timer_cancel(t);fire(arg,(uint64_t)clock);
    }
    r->dispatching=0;return r->failed?-1:0;
}
void desk_runtime_idle(struct desk_runtime *r,int busy) {
    long clock=srv_monotonic_us();
    if(clock>=0) {
        desk_runtime_deadline(r,heartbeat((uint64_t)clock));
        if(r->timers)desk_runtime_deadline(r,r->timers->deadline_us);
    }
    if(busy || r->failed || r->retry || clock<0 || r->wait.deadline_us<=(uint64_t)clock ||
       desk_connection_wait_io(r->connection,&r->wait)<0)srv_sleep_ticks(1);
}
