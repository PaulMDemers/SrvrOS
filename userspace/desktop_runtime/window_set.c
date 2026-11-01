#include <srvros/desk_window_set.h>
#include <stdint.h>
#include <sys/mman.h>

void desk_window_set_init(struct desk_window_set *set) {
    *set=(struct desk_window_set){0};
    desk_connection_init(&set->connection);
    desk_runtime_init(&set->runtime,&set->connection);
}
struct desk_window_owner *desk_window_owner_new(struct desk_window_set *set,size_t bytes) {
    if(!set || set->failed || set->lifecycle.quitting || set->runtime.dispatching ||
       !bytes || bytes>SIZE_MAX-sizeof(struct desk_window_owner))return NULL;
    size_t length=sizeof(struct desk_window_owner)+bytes;
    struct desk_window_owner *owner=mmap(NULL,length,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(owner==MAP_FAILED)return NULL;
    owner->set=set;owner->mapping_bytes=length;owner->model_bytes=bytes;
    return owner;
}
int desk_window_owner_open(struct desk_window_owner *owner,struct desk_app *window,uint64_t requester,
                           const char *title,uint32_t width,uint32_t height) {
    if(!owner || !window || !title)return -1;
    struct desk_window_set *set=owner->set;
    uintptr_t base=(uintptr_t)owner->model,where=(uintptr_t)window;
    if(!set || set->failed || set->lifecycle.quitting || set->runtime.dispatching || owner->attached ||
       where<base || where-base>owner->model_bytes || sizeof(*window)>owner->model_bytes-(where-base))return -1;
    if(desk_window_open(&set->connection,window,title,width,height))return -1;
    owner->window=window;owner->requester=requester;owner->attached=1;
    owner->next=set->windows;set->windows=owner;
    return 0;
}
struct dispatch_context {struct desk_window_set *set;desk_window_event event;void *context;};
static int dispatch(void *context,struct desk_app *window,struct desk_packet packet) {
    struct dispatch_context *call=context;
    struct desk_window_set *set=call->set;
    struct desk_window_owner *owner=set->windows;
    while(owner && owner->window!=window && &owner->dialog.window!=window)owner=owner->next;
    if(!owner)return -1;
    if(window==owner->window) {
        if(packet.type==DESK_OPEN_REPLY) {
            if(packet.value) {owner->retired=1;set->refused++;}
            else {owner->ready=1;set->admitted++;}
        } else if(packet.type==DESK_CLOSE_ACK) {
            owner->retired=1;
            desk_application_closed(&set->lifecycle,window->window);
        }
    }
    return call->event(call->context,owner,window,packet);
}
int desk_window_set_dispatch(struct desk_window_set *set,desk_window_event event,void *context) {
    if(!set || !event || set->failed || set->runtime.dispatching)return -1;
    struct dispatch_context call={set,event,context};
    if(desk_runtime_dispatch(&set->runtime,dispatch,&call)<0) {
        set->failed=1;
        for(struct desk_window_owner *owner=set->windows;owner;owner=owner->next)owner->retired=1;
        return -1;
    }
    return 0;
}
struct desk_window_owner *desk_window_set_next_close(struct desk_window_set *set) {
    if(!set || set->failed || set->runtime.dispatching || !set->lifecycle.quitting || set->lifecycle.closing_window)return NULL;
    for(struct desk_window_owner *owner=set->windows;owner;owner=owner->next)
        if(owner->ready && !owner->retired && !owner->window->closing &&
           desk_application_close_next(&set->lifecycle,owner->window->window))return owner;
    return NULL;
}
int desk_window_owner_dispose_with(struct desk_window_owner *owner,int (*retire)(void *,void *,size_t),void *context) {
    if(!owner || !owner->set || owner->set->failed || owner->set->runtime.dispatching ||
       !owner->attached || !owner->retired || owner->dialog.window.connection)return -1;
    if(owner->disposed)return 0;
    if(retire?desk_window_dispose_with(owner->window,retire,context):desk_window_dispose(owner->window))return -1;
    owner->disposed=1;return 0;
}
int desk_window_owner_dispose(struct desk_window_owner *owner) {return desk_window_owner_dispose_with(owner,NULL,NULL);}
int desk_window_owner_release_with(struct desk_window_owner *owner,int work_drained,
    int (*retire)(void *,void *,size_t),void *context) {
    if(!owner || !owner->set || owner->set->failed || owner->set->runtime.dispatching ||
       !retire || !work_drained || owner->dialog.window.connection || (owner->attached && !owner->disposed))return -1;
    struct desk_window_set *set=owner->set;
    struct desk_window_owner **link=NULL,*next=owner->next;
    int counted=owner->attached && owner->ready;
    if(owner->attached) {
        link=&set->windows;
        while(*link && *link!=owner)link=&(*link)->next;
        if(!*link)return -1;
        *link=next;
    }
    if(retire(context,owner,owner->mapping_bytes)) {
        if(link)*link=owner;
        return -1;
    }
    if(counted)set->retired++;
    return 0;
}
static int release_mapping(void *context,void *address,size_t length) {
    (void)context;return munmap(address,length);
}
int desk_window_owner_release(struct desk_window_owner *owner,int work_drained) {
    return desk_window_owner_release_with(owner,work_drained,release_mapping,NULL);
}
