/* Included by process.c: staged source, opt-in guest kernel only. */
#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
#include <srvros/vector_io.h>
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
bool process_signal_experimental_fd_allowed(uint64_t fd) {
    struct process_file *file=process_file_at(process_current(),fd);
    return !file || file->type==PROCESS_FILE_STDIO ||
        ((file->type==PROCESS_FILE_PIPE_READ || file->type==PROCESS_FILE_PIPE_WRITE) &&
        process_experimental_pipe_simple(pipe_at(file->handle)));
}
static bool signal_experimental_primary(uint64_t number) {
    switch(number) {
    case SYS_READ:case SYS_WRITE:case SYS_VECTOR_IO:case SYS_POLL:
    case SYS_WAIT:case SYS_SLEEP_TICKS:case SYS_YIELD:return true;
    default:return false;
    }
}
#endif
bool process_signal_info_pending_current(void) {
    uint64_t irq=process_irq_save();struct process *p=process_current();
    bool pending=p && p->usr1_action.mode==SIGNAL_ACTION_INFO &&
        (process_visible_pending(p)&~process_current_signal_mask(p)&(UINT64_C(1)<<SRV_SIGNAL_USR1));
    process_irq_restore(irq);return pending;
}
static bool signal_public_zero(const void *p,size_t n) {
    const uint8_t *b=p;for(size_t i=0;i<n;i++)if(b[i])return false;return true;
}
int64_t process_signal_public_stage(struct isr_frame *frame,uint64_t kind,uint64_t operation,
    const uint8_t wire[128],uint64_t output,uint64_t output_size) {
    uint64_t irq=process_irq_save();struct process_signal_owner owner;
    int64_t result=SRV_ERR_INVAL;
    if(!frame || frame->cs!=0x23 || frame->ss!=0x1b || !wire ||
       !process_signal_observe_owner(&owner,true))goto done;
    struct signal_thread_state next=*owner.state;
    if(!signal_thread_state_bind(&next,owner.identity))goto done;
    if(next.request_sequence>=INT64_MAX){result=SRV_ERR_OVERFLOW;goto done;}
    uint64_t sequence=0;
    if(!signal_request_stage(&next,owner.identity,(uint64_t)frame,operation,
        output,output_size,wire,kind,0,&sequence))goto done;
    *owner.state=next;result=0;
done:
    process_irq_restore(irq);return result;
}

/* A complete fixed output is admitted before any metadata publication. All
 * exclusions come from authenticated static owner storage, never user input. */
static bool signal_public_copy(struct isr_frame *frame,const struct process_signal_owner *owner,
    uint64_t destination,const void *bytes,size_t length) {
    if(!length)return !destination;
    if(!bytes || length>USER_FRAME_COPY_LIMIT)return false;
    struct user_frame_source source={0};
    for(size_t i=0;i<length;i++)source.bytes[i]=((const uint8_t *)bytes)[i];
    struct process_frame_copy_plan plan;
    if(!process_prepare_frame_copy(frame,&source,destination,length,&plan))return false;
    struct user_frame_exclusion excluded[PROCESS_MAX_USER_THREADS+2]={
        {(uint64_t)owner->state,sizeof(*owner->state)},
        {(uint64_t)owner->process,offsetof(struct process,main_signal_state)}};
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)excluded[i+2]=
        (struct user_frame_exclusion){(uint64_t)&owner->process->user_threads[i].signal_mask,16};
    return vmm_copy_user_frame_extended_current(plan.root,destination,&source,length,
        &plan.stack,plan.pages,plan.page_count,frame,plan.user_fpu,plan.kernel_fpu,
        excluded,PROCESS_MAX_USER_THREADS+2);
}
static bool signal_public_action_wire(const struct signal_action_descriptor *a,
    struct srv_signal_action_v1 *out) {
    if(a->mode==SIGNAL_ACTION_LEGACY && !a->handler)return false;
    *out=(struct srv_signal_action_v1){
        .handler=a->mode==SIGNAL_ACTION_IGNORE?1:a->handler,.mask=a->mask,
        .flags=(a->mode==SIGNAL_ACTION_INFO?SRV_SIGNAL_NATIVE_INFO:0)|
            ((a->flags&SIGNAL_ACTION_RESTART)?SRV_SIGNAL_NATIVE_RESTART:0)|
            ((a->flags&SIGNAL_ACTION_RESET)?SRV_SIGNAL_NATIVE_RESET:0)};
    return true;
}
static int64_t signal_public_action_parse(const struct srv_signal_control_v1 *wire,
    struct signal_action_descriptor *out) {
    const struct srv_signal_action_v1 *a=&wire->action;
    if(a->reserved || (a->flags&~(SRV_SIGNAL_NATIVE_INFO|SRV_SIGNAL_NATIVE_RESTART|SRV_SIGNAL_NATIVE_RESET)))return SRV_ERR_NOTSUP;
    if((a->flags&SRV_SIGNAL_NATIVE_INFO) && (a->flags&SRV_SIGNAL_NATIVE_RESTART))return SRV_ERR_NOTSUP;
    bool info=(a->flags&SRV_SIGNAL_NATIVE_INFO)!=0;
    if((info && a->handler<=1) || (!info && wire->trampoline))return SRV_ERR_INVAL;
    *out=(struct signal_action_descriptor){
        .mode=info?SIGNAL_ACTION_INFO:a->handler==0?SIGNAL_ACTION_DEFAULT:
            a->handler==1?SIGNAL_ACTION_IGNORE:SIGNAL_ACTION_LEGACY,
        .handler=a->handler>1?a->handler:0,.trampoline=wire->trampoline,
        .flags=((a->flags&SRV_SIGNAL_NATIVE_RESTART)?SIGNAL_ACTION_RESTART:0)|
            ((a->flags&SRV_SIGNAL_NATIVE_RESET)?SIGNAL_ACTION_RESET:0),
        .mask=a->mask&~(UINT64_C(1)|(UINT64_C(1)<<9)|(UINT64_C(1)<<19))};
    return signal_action_descriptor_valid(out)?0:SRV_ERR_INVAL;
}
struct signal_public_mask_plan {uint64_t mask,local,shared,fatal;};
static struct signal_public_mask_plan signal_public_prepare_mask(
    const struct process_signal_owner *owner,uint64_t next) {
    const struct process *p=owner->process;
    struct signal_public_mask_plan plan={.mask=next&~((UINT64_C(1)<<9)|(UINT64_C(1)<<19)),
        .local=owner->worker?owner->worker->pending_signals:p->main_pending_signals,.shared=p->pending_signals};
    uint64_t pending=(plan.local|plan.shared)&~plan.mask&~(p->signal_catch_mask|p->signal_ignore_mask);
    uint64_t ignored=pending&((UINT64_C(1)<<SRV_SIGNAL_CHLD)|(UINT64_C(1)<<SRV_SIGNAL_WINCH));
    plan.local&=~ignored;plan.shared&=~ignored;pending&=~ignored;
    if(!p->killed)for(uint64_t s=1;s<64;s++)if(pending&(UINT64_C(1)<<s)) {
        plan.fatal=s;
        if(plan.local&(UINT64_C(1)<<s))plan.local&=~(UINT64_C(1)<<s);
        else plan.shared&=~(UINT64_C(1)<<s);
        break;
    }
    return plan;
}
static void signal_public_commit_mask(const struct process_signal_owner *owner,
    struct signal_public_mask_plan plan) {
    owner->process->pending_signals=plan.shared;
    if(owner->worker){owner->worker->signal_mask=plan.mask;owner->worker->pending_signals=plan.local;}
    else {owner->process->main_signal_mask=plan.mask;owner->process->main_pending_signals=plan.local;}
    if(plan.fatal){owner->process->killed=true;owner->process->kill_signal=plan.fatal;}
}
uint64_t process_signal_public_entry(struct isr_frame *frame) {
    struct process_signal_owner owner;
    if(!frame || frame->cs!=0x23 || frame->ss!=0x1b || !process_signal_observe_owner(&owner,true))
        return 128+SRV_SIGNAL_SEGV;
    if(!owner.state->local.flags)return 0;
    /* v1 never allocates FD/event tickets in INFO. At depth0 validation does
     * not inspect or copy any active FP context, and this path uses no FPU API. */
    if(owner.state->stack.depth || !signal_state_validate(owner.state,owner.identity))return 128+SRV_SIGNAL_SEGV;
    if(frame->vector!=0x80)return 4;
    if(frame->rax==SYS_SIGNAL_IO && frame->r9==1 && frame->rdi==SRV_SIGNAL_IO_LOCAL_END &&
        !frame->rcx && !frame->r8) {
        bool no_mask=(owner.state->local.flags&SIGNAL_LOCAL_NO_MASK)!=0;
        struct signal_public_mask_plan plan={0};
        if(!no_mask)plan=signal_public_prepare_mask(&owner,owner.state->local.previous_mask);
        uint64_t old;bool unchanged;
        if(!signal_io_local_end(owner.state,owner.identity,0,frame->rsi,frame->rdx,&old,&unchanged)) {
            frame->rax=(uint64_t)SRV_ERR_INVAL;return 3;
        }
        if(!no_mask)signal_public_commit_mask(&owner,plan);
        frame->rax=0;
        return plan.fatal?128+plan.fatal:1;
    }
    if(frame->rax==SYS_EXIT)return 2;
    if(frame->rax==SYS_THREAD_SELF && !frame->rdi && !frame->rsi && !frame->rdx &&
        !frame->rcx && !frame->r8 && !frame->r9)return 2;
    if(frame->rax==SYS_FUTEX_WAKE && frame->rsi==1 && !frame->rdx && !frame->rcx && !frame->r8 && !frame->r9)return 2;
    frame->rax=(uint64_t)SRV_ERR_INVAL;return 3;
}

static int64_t signal_public_control(struct isr_frame *frame,const struct process_signal_owner *owner,
    const struct signal_request_slot *request,struct signal_thread_state *next,uint64_t *exit_status) {
    struct srv_signal_control_v1 wire;
    for(size_t i=0;i<sizeof(wire);i++)((uint8_t *)&wire)[i]=request->wire[i];
    if(wire.size!=sizeof(wire) || wire.version!=1 || wire.operation!=request->operation ||
       !signal_public_zero(wire.reserved,sizeof(wire.reserved)) ||
       !signal_public_zero(request->wire+sizeof(wire),128-sizeof(wire)))return SRV_ERR_INVAL;
    uint64_t output=request->output_address,size=request->output_size;
    struct process *p=owner->process;
    switch(wire.operation) {
    case SRV_SIGNAL_CONTROL_FEATURES: {
        if(!signal_public_zero((const uint8_t *)&wire+12,sizeof(wire)-12) || size!=64)return SRV_ERR_INVAL;
        /* Zero capabilities until the complete kernel/libc/guest gate closes. */
        struct srv_signal_features_v1 features={.size=64,.version=1};
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
        features.capabilities=1;features.info_signals=UINT64_C(1)<<SRV_SIGNAL_USR1;
        features.info_flags=SRV_SIGNAL_NATIVE_INFO|SRV_SIGNAL_NATIVE_RESET;
        features.context_bytes=688;features.info_bytes=64;features.max_depth=4;
#endif
        if(!signal_public_copy(frame,owner,output,&features,sizeof(features)))return SRV_ERR_FAULT;
        *owner->state=*next;return 0;
    }
    case SRV_SIGNAL_CONTROL_QUERY_ACTION:
    case SRV_SIGNAL_CONTROL_EXCHANGE_ACTION: {
        bool exchange=wire.operation==SRV_SIGNAL_CONTROL_EXCHANGE_ACTION;
        if(wire.signum!=SRV_SIGNAL_USR1 || wire.mask_how || wire.mask ||
           (exchange?(size && size!=24):size!=24) ||
           (!exchange && (!signal_public_zero(&wire.action,sizeof(wire.action)) || wire.trampoline)))return SRV_ERR_INVAL;
        struct srv_signal_action_v1 old;
        if(size && !signal_public_action_wire(&p->usr1_action,&old))return SRV_ERR_NOTSUP;
        struct signal_action_descriptor action={0};
        struct signal_default_decision decision={0};
        if(exchange) {
            int64_t result=signal_public_action_parse(&wire,&action);if(result)return result;
            if(p->usr1_action_generation==UINT64_MAX)return SRV_ERR_OVERFLOW;
            if((action.handler && !vmm_user_exec_current(action.handler,owner->root)) ||
               (action.trampoline && !vmm_user_exec_current(action.trampoline,owner->root)))return SRV_ERR_FAULT;
            /* Active unassociated INFO-only waits cannot lose their wakeup. */
            if(!process_usr1_waits_permit_action(p,&action))return SRV_ERR_BUSY;
            if(action.mode==SIGNAL_ACTION_DEFAULT)decision=process_prepare_signal_default(p,SRV_SIGNAL_USR1);
        }
        if(size && !signal_public_copy(frame,owner,output,&old,sizeof(old)))return SRV_ERR_FAULT;
        *owner->state=*next;
        if(exchange) {
            uint64_t bit=UINT64_C(1)<<SRV_SIGNAL_USR1;
            p->usr1_action=action;p->usr1_action_generation++;
            p->signal_catch_mask&=~bit;p->signal_ignore_mask&=~bit;
            if(action.mode==SIGNAL_ACTION_INFO || action.mode==SIGNAL_ACTION_LEGACY)p->signal_catch_mask|=bit;
            if(action.mode==SIGNAL_ACTION_IGNORE){p->signal_ignore_mask|=bit;process_discard_signals(p,bit);}
            process_commit_signal_default(p,decision);
            if(decision.pending)*exit_status=128+decision.signal;
        }
        return 0;
    }
    case SRV_SIGNAL_CONTROL_LEGACY_TAKE: {
        if(wire.signum!=SRV_SIGNAL_USR1 || !signal_public_zero(&wire.action,sizeof(wire.action)) ||
            wire.trampoline || wire.mask_how || wire.mask || size!=64)return SRV_ERR_INVAL;
        uint64_t bit=UINT64_C(1)<<SRV_SIGNAL_USR1;
        uint64_t *mask=owner->worker?&owner->worker->signal_mask:&p->main_signal_mask;
        uint64_t *local=owner->worker?&owner->worker->pending_signals:&p->main_pending_signals;
        bool selected=p->usr1_action.mode==SIGNAL_ACTION_LEGACY && !(*mask&bit) && ((*local|p->pending_signals)&bit);
        struct srv_signal_legacy_take_v1 output={.size=64,.version=1};
        struct signal_default_decision decision={0};
        uint64_t handler_mask=0;uint64_t *pending=NULL;bool reset=false;
        if(selected) {
            if(!signal_public_action_wire(&p->usr1_action,&output.action))return SRV_ERR_NOTSUP;
            output.signum=SRV_SIGNAL_USR1;output.previous_mask=*mask;output.generation=p->usr1_action_generation;
            pending=(*local&bit)?local:&p->pending_signals;
            handler_mask=(*mask|p->usr1_action.mask|bit)&~((UINT64_C(1)<<9)|(UINT64_C(1)<<19));
            reset=(p->usr1_action.flags&SIGNAL_ACTION_RESET)!=0;
            if(reset) {
                if(p->usr1_action_generation==UINT64_MAX)return SRV_ERR_OVERFLOW;
                struct signal_default_view view={.worker=owner->worker,.mask=handler_mask,
                    .local_pending=*local&~(pending==local?bit:0),
                    .shared_pending=p->pending_signals&~(pending==&p->pending_signals?bit:0)};
                decision=process_prepare_signal_default_view(p,SRV_SIGNAL_USR1,&view);
            }
        }
        if(!signal_public_copy(frame,owner,request->output_address,&output,sizeof(output)))return SRV_ERR_FAULT;
        *owner->state=*next;
        if(selected) {
            *pending&=~bit;*mask=handler_mask;
            if(reset) {
                p->usr1_action=(struct signal_action_descriptor){0};p->usr1_action_generation++;
                p->signal_catch_mask&=~bit;p->signal_ignore_mask&=~bit;
            }
            process_commit_signal_default(p,decision);
            if(decision.pending)*exit_status=128+decision.signal;
        }
        return 0;
    }
    case SRV_SIGNAL_CONTROL_BLOCKED_TAKE: {
        if(wire.signum || !signal_public_zero(&wire.action,sizeof(wire.action)) || wire.trampoline ||
            wire.mask_how || size!=4)return SRV_ERR_INVAL;
        uint64_t *local=owner->worker?&owner->worker->pending_signals:&p->main_pending_signals;
        uint64_t blocked=owner->worker?owner->worker->signal_mask:p->main_signal_mask;
        uint64_t available=(*local|p->pending_signals)&wire.mask&blocked;
        int32_t signum=0;uint64_t *pending=NULL;
        for(int32_t s=1;s<64;s++)if(available&(UINT64_C(1)<<s)) {
            signum=s;pending=(*local&(UINT64_C(1)<<s))?local:&p->pending_signals;break;
        }
        if(!signal_public_copy(frame,owner,request->output_address,&signum,sizeof(signum)))return SRV_ERR_FAULT;
        *owner->state=*next;
        if(pending)*pending&=~(UINT64_C(1)<<signum);
        return 0;
    }
    case SRV_SIGNAL_CONTROL_MASK: {
        if(wire.signum || !signal_public_zero(&wire.action,sizeof(wire.action)) || wire.trampoline ||
           wire.mask_how>SRV_SIGNAL_SETMASK || (size && size!=8))return SRV_ERR_INVAL;
        uint64_t old=owner->worker?owner->worker->signal_mask:p->main_signal_mask;
        uint64_t value=wire.mask_how==SRV_SIGNAL_BLOCK?old|wire.mask:
            wire.mask_how==SRV_SIGNAL_UNBLOCK?old&~wire.mask:wire.mask;
        struct signal_public_mask_plan plan=signal_public_prepare_mask(owner,value);
        if(size && !signal_public_copy(frame,owner,output,&old,sizeof(old)))return SRV_ERR_FAULT;
        *owner->state=*next;signal_public_commit_mask(owner,plan);
        if(plan.fatal)*exit_status=128+plan.fatal;
        return 0;
    }
    default:return SRV_ERR_NOTSUP;
    }
}
static bool signal_public_wait_ready(void *unused) {
    (void)unused;
    return process_should_exit_current() || process_signal_pending_current();
}
static bool signal_public_primary_valid(uint64_t kind,uint64_t number,uint64_t selector) {
    if(kind<1 || kind>8)return false;
    bool native=false;
    switch(number) {
    case SYS_WRITE:case SYS_READ:case SYS_PREAD:case SYS_PWRITE:case SYS_POLL:
    case SYS_FCNTL:case SYS_NET_ACCEPT:case SYS_NET_CONNECT:case SYS_NET_UDP_SENDTO:
    case SYS_NET_UDP_RECVFROM:case SYS_NET_PEEK:case SYS_UNIX_CONNECT:case SYS_UNIX_ACCEPT:
    case SYS_WAIT:case SYS_SLEEP_TICKS:native=!selector;break;
    case SYS_VECTOR_IO:native=selector<=SRV_VECTOR_PWRITE;break;
    case SYS_SOCKET_TRY_IO:native=selector>=SRV_SOCKET_RECV && selector<=SRV_SOCKET_UDP_SEND;break;
    case SYS_PIPE_MESSAGE:native=selector<=SRV_PIPE_MESSAGE_RECV;break;
    case SYS_YIELD:return kind==SRV_SIGNAL_ROOT_SEM_WAIT && !selector;
    default:return false;
    }
    if(!native)return false;
    switch(kind) {
    case SRV_SIGNAL_ROOT_RAW:return true;
    case SRV_SIGNAL_ROOT_READ:return number==SYS_READ || number==SYS_NET_PEEK || number==SYS_NET_UDP_RECVFROM ||
        (number==SYS_VECTOR_IO && selector==SRV_VECTOR_READ) ||
        (number==SYS_SOCKET_TRY_IO && (selector==SRV_SOCKET_RECV || selector==SRV_SOCKET_PEEK || selector==SRV_SOCKET_UDP_RECV)) ||
        (number==SYS_PIPE_MESSAGE && selector==SRV_PIPE_MESSAGE_RECV);
    case SRV_SIGNAL_ROOT_WRITE:return number==SYS_WRITE || number==SYS_NET_UDP_SENDTO ||
        (number==SYS_VECTOR_IO && selector==SRV_VECTOR_WRITE) ||
        (number==SYS_SOCKET_TRY_IO && (selector==SRV_SOCKET_SEND || selector==SRV_SOCKET_UDP_SEND)) ||
        (number==SYS_PIPE_MESSAGE && selector==SRV_PIPE_MESSAGE_SEND);
    case SRV_SIGNAL_ROOT_POLL:case SRV_SIGNAL_ROOT_EPOLL:return number==SYS_POLL;
    case SRV_SIGNAL_ROOT_WAITPID:return number==SYS_WAIT;
    case SRV_SIGNAL_ROOT_SLEEP:return number==SYS_SLEEP_TICKS;
    default:return false;
    }
}
static struct signal_io_slot *signal_public_top(struct signal_thread_state *state,
    struct signal_state_identity id,uint64_t ticket) {
    if(!signal_state_validate(state,id) || !ticket || state->io_top!=ticket)return NULL;
    for(unsigned i=0;i<4;i++)if((state->io[i].flags&SIGNAL_IO_LIVE) && state->io[i].ticket==ticket &&
        state->io[i].depth==state->stack.depth)return &state->io[i];
    return NULL;
}
static bool signal_public_call_shape(const struct srv_signal_io_v1 *wire,const struct signal_io_slot *root) {
    if(wire->size!=128 || wire->version!=1 || wire->operation!=SRV_SIGNAL_IO_CALL ||
       wire->flags || wire->deadline || wire->reserved[0] || wire->reserved[1] ||
       wire->descriptor_count>64 || (!!wire->descriptor_vector!=!!wire->descriptor_count))return false;
    uint64_t kind=root->flags&255,primary=(root->flags>>8)&UINT16_MAX,selector=(root->flags>>24)&255;
    uint64_t number=wire->raw_syscall,actual_selector=0;unsigned arity;
    switch(number) {
    case SYS_WRITE:case SYS_READ:case SYS_NET_CONNECT:case SYS_NET_PEEK:case SYS_WAIT:case SYS_FCNTL:arity=3;break;
    case SYS_PREAD:case SYS_PWRITE:case SYS_NET_ACCEPT:case SYS_PIPE_MESSAGE:arity=4;break;
    case SYS_VECTOR_IO:case SYS_SOCKET_TRY_IO:case SYS_NET_UDP_SENDTO:case SYS_NET_UDP_RECVFROM:arity=5;break;
    case SYS_POLL:arity=3;break;
    case SYS_UNIX_CONNECT:case SYS_UNIX_ACCEPT:case SYS_SLEEP_TICKS:arity=1;break;
    case SYS_YIELD:arity=0;break;
    default:return false;
    }
    for(unsigned i=arity;i<6;i++)if(wire->args[i])return false;
    if(number==SYS_VECTOR_IO)actual_selector=wire->args[3];
    else if(number==SYS_SOCKET_TRY_IO)actual_selector=wire->args[1];
    else if(number==SYS_PIPE_MESSAGE)actual_selector=wire->args[3];
    if(number==SYS_FCNTL && wire->args[1]!=SRV_F_SETLKW)return false;
    if(number==SYS_WAIT && (wire->args[2]&~(uint64_t)(SRV_WAIT_NOHANG|SRV_WAIT_POSIX)))return false;
    if(number==primary)return actual_selector==selector && signal_public_primary_valid(kind,primary,selector);
    return number==SYS_POLL && (kind==SRV_SIGNAL_ROOT_READ || kind==SRV_SIGNAL_ROOT_WRITE || kind==SRV_SIGNAL_ROOT_EPOLL);
}
int64_t process_signal_io_check(struct isr_frame *frame,const struct srv_signal_io_v1 *wire,struct signal_io_slot *out) {
    struct process_signal_owner owner;
    if(!frame || !wire || !out || !process_signal_observe_owner(&owner,true))return SRV_ERR_INVAL;
    struct signal_thread_state next=*owner.state;
    if(!signal_thread_state_bind(&next,owner.identity) || next.local.flags || next.request.sequence || next.stack.depth)return SRV_ERR_NOTSUP;
    struct signal_io_slot *root=signal_public_top(&next,owner.identity,wire->ticket);
    if(!root || !signal_public_call_shape(wire,root))return SRV_ERR_INVAL;
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    if(!signal_experimental_primary(wire->raw_syscall))return SRV_ERR_NOTSUP;
#endif
    if(root->flags&SIGNAL_IO_INTERRUPTED)return SRV_ERR_INTR;
    if(root->flags&SIGNAL_IO_COMPLETED)return SRV_ERR_INVAL;
    *out=*root;return 0;
}
int64_t process_signal_io_reserve(struct isr_frame *frame,const struct srv_signal_io_v1 *wire,
    const struct srv_signal_descriptor_v1 *expected,size_t count,uint64_t *cookie,uint64_t *sequence) {
    struct signal_io_slot root;
    int64_t result=process_signal_io_check(frame,wire,&root);if(result)return result;
    if(!cookie || !sequence || count!=wire->descriptor_count || count>64 || (count && !expected))return SRV_ERR_INVAL;
    struct process_signal_owner owner;if(!process_signal_observe_owner(&owner,true))return SRV_ERR_INVAL;
    for(size_t i=0;i<count;i++) {
        int64_t fd=expected[i].fd;
        if(fd<0)return SRV_ERR_INVAL;
        for(size_t j=0;j<i;j++)if(expected[j].fd==fd && expected[j].value!=expected[i].value)return SRV_ERR_INVAL;
        uint64_t actual=process_fd_generation(owner.process,(uint64_t)fd);
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
        if(!process_signal_experimental_fd_allowed((uint64_t)fd))return SRV_ERR_NOTSUP;
#endif
        if(wire->raw_syscall!=SYS_POLL && (actual==UINT64_MAX || expected[i].value==UINT64_MAX || actual!=expected[i].value))return SRV_ERR_BADF;
    }
    struct signal_thread_state next=*owner.state;
    if(!signal_thread_state_bind(&next,owner.identity))return SRV_ERR_INVAL;
    if(next.io_sequence>=INT64_MAX || next.request_sequence>=INT64_MAX)return SRV_ERR_OVERFLOW;
    uint64_t seq=0,value=0;
    if(!signal_request_stage(&next,owner.identity,(uint64_t)frame,SRV_SIGNAL_IO_CALL,0,0,
        (const uint8_t *)wire,SIGNAL_REQUEST_IO,0,&seq) ||
        !signal_io_prepare_outcome(&next,owner.identity,next.stack.depth,wire->ticket,wire->raw_syscall,&value))return SRV_ERR_INVAL;
    *owner.state=next;*cookie=value;*sequence=seq;return 0;
}
bool process_signal_io_capture(struct isr_frame *frame,const struct srv_signal_io_v1 *wire,uint64_t cookie,uint64_t sequence,
    int64_t result,uint64_t us,uint64_t ticks,bool mono,bool tickvalid) {
    struct process_signal_owner owner;
    if(!frame || !wire || !process_signal_observe_owner(&owner,true))return false;
    struct signal_thread_state next=*owner.state;
    const struct signal_request_slot *request=&next.request;
    if(request->kind!=SIGNAL_REQUEST_IO || request->operation!=SRV_SIGNAL_IO_CALL || request->sequence!=sequence ||
        request->trap!=(uint64_t)frame)return false;
    for(size_t i=0;i<128;i++)if(request->wire[i]!=((const uint8_t *)wire)[i])return false;
    struct signal_io_slot *root=NULL;
    for(unsigned i=0;i<4;i++)if(next.io[i].ticket==wire->ticket && (next.io[i].flags&SIGNAL_IO_LIVE))root=&next.io[i];
    if(!root)return false;
    uint64_t kind=root->flags&255,primary=(root->flags>>8)&UINT16_MAX;
    bool completed=false;
    if(result>=0) {
        if(kind==SRV_SIGNAL_ROOT_RAW || kind==SRV_SIGNAL_ROOT_WAITPID || kind==SRV_SIGNAL_ROOT_SLEEP)completed=true;
        else if((kind==SRV_SIGNAL_ROOT_READ || kind==SRV_SIGNAL_ROOT_WRITE) && wire->raw_syscall==primary)completed=true;
        else if(kind==SRV_SIGNAL_ROOT_POLL)completed=result>0 || root->deadline==0 ||
            (root->deadline!=UINT64_MAX && timer_reference_ready() && timer_monotonic_us()>=root->deadline);
    }
    if(!signal_io_commit_outcome(&next,owner.identity,next.stack.depth,wire->ticket,cookie,wire->raw_syscall,
        result,us,ticks,mono,tickvalid,completed) ||
        !signal_request_clear(&next,owner.identity,(uint64_t)frame,sequence))return false;
    *owner.state=next;return true;
}
static int64_t signal_public_io(struct isr_frame *frame,const struct process_signal_owner *owner,
    const struct signal_request_slot *request,const struct signal_thread_state *cleared,uint64_t *exit_status) {
    struct srv_signal_io_v1 wire;
    for(size_t i=0;i<sizeof(wire);i++)((uint8_t *)&wire)[i]=request->wire[i];
    if(wire.size!=sizeof(wire) || wire.version!=1 || wire.operation!=request->operation ||
       wire.reserved[0] || wire.reserved[1])return SRV_ERR_INVAL;
    struct signal_thread_state next=*cleared;
    uint64_t depth=next.stack.depth;
    if(wire.operation==SRV_SIGNAL_IO_CAPTURE_DESCRIPTORS) {
        struct signal_io_slot *slot=signal_public_top(&next,owner->identity,wire.ticket);
        if(!slot || (slot->flags&255)!=SRV_SIGNAL_ROOT_EPOLL || wire.raw_syscall || wire.deadline ||
            !signal_public_zero(wire.args,sizeof(wire.args)) || (wire.flags&~SRV_SIGNAL_IO_POLL_LIST) ||
            wire.descriptor_count>64 || (!!wire.descriptor_vector!=!!wire.descriptor_count) ||
            request->output_size!=wire.descriptor_count*16 || (!!request->output_address!=!!wire.descriptor_count))return SRV_ERR_INVAL;
        if(slot->flags&SIGNAL_IO_INTERRUPTED)return SRV_ERR_INTR;
        if(slot->flags&SIGNAL_IO_COMPLETED)return SRV_ERR_INVAL;
        struct srv_signal_descriptor_v1 vector[64];
        if(!syscall_signal_copy_input(vector,wire.descriptor_vector,wire.descriptor_count*16))return SRV_ERR_FAULT;
        for(uint64_t i=0;i<wire.descriptor_count;i++) {
            if(vector[i].value)return SRV_ERR_INVAL;
            struct process_file *file=vector[i].fd>=0?process_file_at(owner->process,(uint64_t)vector[i].fd):NULL;
            if(!file) {
                if(!(wire.flags&SRV_SIGNAL_IO_POLL_LIST))return SRV_ERR_BADF;
                vector[i].value=UINT64_MAX;
            } else {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
                if(!process_signal_experimental_fd_allowed((uint64_t)vector[i].fd))return SRV_ERR_NOTSUP;
#endif
                vector[i].value=owner->process->file_generations[vector[i].fd];
                if(vector[i].value==UINT64_MAX)return SRV_ERR_OVERFLOW;
            }
        }
        if(!signal_public_copy(frame,owner,request->output_address,vector,wire.descriptor_count*16))return SRV_ERR_FAULT;
        *owner->state=next;return 0;
    }
    if(wire.operation==SRV_SIGNAL_IO_BEGIN) {
        if(wire.ticket || wire.raw_syscall || (wire.flags&~UINT64_C(3)) || wire.args[5] ||
           wire.descriptor_count>64 || (!!wire.descriptor_vector!=!!wire.descriptor_count) ||
           request->output_size!=wire.descriptor_count*16 || (!!request->output_address!=!!wire.descriptor_count) ||
           !signal_public_primary_valid(wire.args[2],wire.args[3],wire.args[4]) ||
           wire.args[0]<frame->rsp || wire.args[0]>=(UINT64_C(1)<<47) ||
           (!(wire.flags&SRV_SIGNAL_IO_LEGACY_MASKED) && wire.args[1]))return SRV_ERR_INVAL;
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
        if(!signal_experimental_primary(wire.args[3]))return SRV_ERR_NOTSUP;
        if(wire.args[3]==SYS_VECTOR_IO && wire.args[4]>SRV_VECTOR_WRITE)return SRV_ERR_NOTSUP;
#endif
        if((wire.flags&3) && wire.args[2]!=SRV_SIGNAL_ROOT_POLL && wire.args[2]!=SRV_SIGNAL_ROOT_EPOLL)return SRV_ERR_INVAL;
        if(depth)return SRV_ERR_NOTSUP; /* No FD/event/locking I/O inside INFO v1. */
        bool masked=(wire.flags&SRV_SIGNAL_IO_LEGACY_MASKED)!=0;
        if(masked && owner->process->usr1_action.mode==SIGNAL_ACTION_INFO)return SRV_ERR_NOTSUP;
        if(next.io_sequence>=INT64_MAX)return SRV_ERR_OVERFLOW;
        bool capacity=false;for(unsigned i=0;i<4;i++)if(!next.io[i].ticket)capacity=true;
        if(!capacity)return SRV_ERR_AGAIN;
        struct srv_signal_descriptor_v1 vector[64];
        if(!syscall_signal_copy_input(vector,wire.descriptor_vector,wire.descriptor_count*16))return SRV_ERR_FAULT;
        for(uint64_t i=0;i<wire.descriptor_count;i++) {
            if(vector[i].value)return SRV_ERR_INVAL;
            int64_t fd=vector[i].fd;
            struct process_file *file=fd>=0?process_file_at(owner->process,(uint64_t)fd):NULL;
            if(!file) {
                if(!(wire.flags&SRV_SIGNAL_IO_POLL_LIST))return SRV_ERR_BADF;
                vector[i].value=UINT64_MAX;
            } else {
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
                if(!process_signal_experimental_fd_allowed((uint64_t)fd))return SRV_ERR_NOTSUP;
#endif
                uint64_t generation=owner->process->file_generations[fd];
                if(generation==UINT64_MAX)return SRV_ERR_OVERFLOW;
                vector[i].value=generation;
            }
        }
        uint64_t old=owner->worker?owner->worker->signal_mask:owner->process->main_signal_mask;
        uint64_t temp=wire.args[1]&~((UINT64_C(1)<<9)|(UINT64_C(1)<<19));
        uint64_t ticket;
        if(!signal_io_begin(&next,owner->identity,depth,wire.args[2],wire.args[3],wire.args[4],
            wire.args[0],wire.deadline,masked?SIGNAL_IO_LEGACY_MASKED:0,masked?old:0,masked?temp:0,&ticket))return SRV_ERR_INVAL;
        struct signal_public_mask_plan plan={0};if(masked)plan=signal_public_prepare_mask(owner,temp);
        if(!signal_public_copy(frame,owner,request->output_address,vector,wire.descriptor_count*16))return SRV_ERR_FAULT;
        *owner->state=next;if(masked)signal_public_commit_mask(owner,plan);
        if(plan.fatal)*exit_status=128+plan.fatal;
        return (int64_t)ticket;
    }
    if(wire.operation==SRV_SIGNAL_IO_UNWIND) {
        if(wire.ticket || wire.raw_syscall || wire.descriptor_vector || wire.descriptor_count || wire.flags ||
            wire.deadline || request->output_size || !signal_public_zero(wire.args+1,5*8) ||
            depth || next.local.flags || !wire.args[0] || wire.args[0]>=(UINT64_C(1)<<47) ||
            !vmm_user_range_mapped(wire.args[0],1,false))return SRV_ERR_INVAL;
        for(unsigned n=0;n<4 && next.io_top;n++) {
            struct signal_io_slot *top=signal_public_top(&next,owner->identity,next.io_top);
            if(!top)return SRV_ERR_INVAL;
            if(top->anchor>=wire.args[0])break;
            if(!signal_io_end(&next,owner->identity,0,top->ticket))return SRV_ERR_INVAL;
        }
        struct signal_wait_slot *wait=&next.waits[0];
        if(wait->ticket && wait->anchor<wire.args[0]) {
            uint64_t phase=wait->phase_flags&255;
            if(phase!=SIGNAL_WAIT_LEGACY_READY && phase!=SIGNAL_WAIT_INFO_DONE)return SRV_ERR_INVAL;
            *wait=(struct signal_wait_slot){0};
        }
        if(!signal_state_validate(&next,owner->identity))return SRV_ERR_INVAL;
        *owner->state=next;return 0;
    }
    struct signal_io_slot *slot=signal_public_top(&next,owner->identity,wire.ticket);
    if(!slot || wire.raw_syscall || wire.descriptor_vector || wire.descriptor_count || wire.deadline)return SRV_ERR_INVAL;
    if(wire.operation==SRV_SIGNAL_IO_LOCAL_BEGIN) {
        if(wire.flags>1 || request->output_size || !signal_public_zero(wire.args+1,5*8))return SRV_ERR_INVAL;
        uint64_t mask=owner->worker?owner->worker->signal_mask:owner->process->main_signal_mask,cookie;
        if(next.io_sequence>=INT64_MAX)return SRV_ERR_OVERFLOW;
        if(!signal_io_local_begin(&next,owner->identity,depth,wire.ticket,mask,wire.flags!=0,wire.args[0],&cookie))
            return (slot->flags&SIGNAL_IO_INTERRUPTED)?SRV_ERR_INTR:SRV_ERR_INVAL;
        bool no_mask=(next.local.flags&SIGNAL_LOCAL_NO_MASK)!=0;
        *owner->state=next;
        if(!no_mask) {
            if(owner->worker)owner->worker->signal_mask=mask|(UINT64_C(1)<<SRV_SIGNAL_USR1);
            else owner->process->main_signal_mask=mask|(UINT64_C(1)<<SRV_SIGNAL_USR1);
        }
        return (int64_t)cookie;
    }
    if(wire.flags || !signal_public_zero(wire.args,sizeof(wire.args)))return SRV_ERR_INVAL;
    if(wire.operation==SRV_SIGNAL_IO_RESULT) {
        if(request->output_size!=64)return SRV_ERR_INVAL;
        struct srv_signal_io_result_v1 output={.size=64,.version=1,.cookie=slot->cookie,
            .syscall_number=(slot->flags>>32)&UINT16_MAX,.result=slot->result,
            .elapsed_us=slot->elapsed_us,.elapsed_ticks=slot->elapsed_ticks,
            .validity=((slot->flags&SIGNAL_IO_MONO_VALID)?1:0)|((slot->flags&SIGNAL_IO_TICKS_VALID)?2:0)|
                ((slot->flags&SIGNAL_IO_COMPLETED)?SRV_SIGNAL_IO_RESULT_COMPLETED_NATIVE:0)};
        if(!signal_public_copy(frame,owner,request->output_address,&output,sizeof(output)))return SRV_ERR_FAULT;
        *owner->state=next;return 0;
    }
    if(wire.operation==SRV_SIGNAL_IO_END) {
        if(request->output_size)return SRV_ERR_INVAL;
        bool masked=(slot->flags&SIGNAL_IO_LEGACY_MASKED)!=0;
        struct signal_public_mask_plan plan={0};if(masked)plan=signal_public_prepare_mask(owner,slot->previous_mask);
        if(!signal_io_end(&next,owner->identity,depth,wire.ticket))return SRV_ERR_INVAL;
        *owner->state=next;if(masked)signal_public_commit_mask(owner,plan);
        if(plan.fatal)*exit_status=128+plan.fatal;
        return 0;
    }
    return SRV_ERR_NOTSUP;
}
static int64_t signal_public_suspend(struct isr_frame *frame,struct process_signal_owner *owner,
    const struct signal_request_slot *request,struct signal_thread_state *next,uint64_t *exit_status) {
    uint64_t args[3];for(size_t i=0;i<sizeof(args);i++)((uint8_t *)args)[i]=request->wire[i];
    if(request->output_address || request->output_size || !signal_public_zero(request->wire+24,104) ||
       args[0]!=request->operation)return SRV_ERR_INVAL;
    uint64_t ticket=0,depth=next->stack.depth;
    if(depth>=4)return SRV_ERR_AGAIN;
    if(args[0]==SRV_SIGNAL_SUSPEND_BEGIN) {
        uint64_t mask=args[1]&~((UINT64_C(1)<<9)|(UINT64_C(1)<<19));
        bool info_only=depth!=0;
        if(next->waits[depth].ticket)return SRV_ERR_BUSY;
        if(next->io_sequence>=INT64_MAX)return SRV_ERR_OVERFLOW;
        if(args[2]<frame->rsp || args[2]>=(UINT64_C(1)<<47))return SRV_ERR_INVAL;
        if(info_only && (owner->process->usr1_action.mode!=SIGNAL_ACTION_INFO ||
            (owner->process->usr1_action.flags&SIGNAL_ACTION_RESET) ||
            (owner->process->signal_catch_mask&~mask&~(UINT64_C(1)<<SRV_SIGNAL_USR1))))return SRV_ERR_NOTSUP;
        uint64_t old=owner->worker?owner->worker->signal_mask:owner->process->main_signal_mask;
        if(!signal_wait_begin(next,owner->identity,args[2],old,mask,request->sequence,info_only,&ticket))return SRV_ERR_INVAL;
        struct signal_public_mask_plan plan=signal_public_prepare_mask(owner,mask);
        *owner->state=*next;signal_public_commit_mask(owner,plan);
        if(plan.fatal){*exit_status=128+plan.fatal;return 0;}
    } else if(args[0]==SRV_SIGNAL_SUSPEND_FINISH) {
        ticket=args[1];
        if(args[2]>1 || !ticket || next->waits[depth].ticket!=ticket)return SRV_ERR_INVAL;
        uint64_t previous=0;bool restore=false;
        if(signal_wait_finish(next,owner->identity,ticket,args[2]!=0,&previous,&restore)) {
            struct signal_public_mask_plan plan={0};
            if(restore)plan=signal_public_prepare_mask(owner,previous);
            *owner->state=*next;
            if(restore)signal_public_commit_mask(owner,plan);
            if(plan.fatal)*exit_status=128+plan.fatal;
            return SRV_ERR_INTR;
        }
        if(args[2] || !signal_wait_rearm(next,owner->identity,ticket))return SRV_ERR_INVAL;
        *owner->state=*next;
    } else return SRV_ERR_INVAL;

    /* Mask installation and queue condition are serialized by scheduler_wait's
     * IF0 condition/register transition. Wakeups before registration are seen
     * by that condition. No user instruction or new BEGIN occurs on rearm. */
    struct signal_state_identity identity=owner->identity;
    process_irq_restore(UINT64_C(1)<<9);
    while(!signal_public_wait_ready(NULL)) {
        if(!scheduler_wait(&process_wait_queue,signal_public_wait_ready,NULL))scheduler_yield();
    }
    (void)process_irq_save();
    scheduler_io_set_user_safe(false);scheduler_io_wait();
    (void)process_irq_save();
    if(process_should_exit_current()){*exit_status=process_current_kill_status();return 0;}
    struct process_signal_owner resumed;
    if(!process_signal_current_owner(&resumed) || resumed.identity.image_generation!=identity.image_generation ||
       resumed.identity.public_tid!=identity.public_tid || resumed.identity.scheduler_identity!=identity.scheduler_identity ||
       resumed.state!=owner->state){*exit_status=128+SRV_SIGNAL_SEGV;return 0;}
    *owner=resumed;*next=*owner->state;
    if(!signal_wait_wake(next,identity,ticket)){*exit_status=128+SRV_SIGNAL_SEGV;return 0;}
    uint64_t bit=UINT64_C(1)<<SRV_SIGNAL_USR1;
    uint64_t mask=owner->worker?owner->worker->signal_mask:owner->process->main_signal_mask;
    uint64_t local=owner->worker?owner->worker->pending_signals:owner->process->main_pending_signals;
    bool info=owner->process->usr1_action.mode==SIGNAL_ACTION_INFO && !(mask&bit) &&
        ((local|owner->process->pending_signals)&bit);
    if(!info && !signal_wait_no_delivery(next,identity,ticket)){*exit_status=128+SRV_SIGNAL_SEGV;return 0;}
    *owner->state=*next;
    return (int64_t)ticket;
}
uint64_t process_signal_public_complete(struct isr_frame *frame) {
    struct process_signal_owner owner;
    if(!process_signal_current_owner(&owner))return UINT64_MAX;
    if(!owner.state->request.sequence)return 0;
    struct signal_request_slot request=owner.state->request;
    struct signal_thread_state next=*owner.state;
    if(!signal_request_clear(&next,owner.identity,(uint64_t)frame,request.sequence))return UINT64_MAX;
    uint64_t exit_status=0;int64_t result=SRV_ERR_NOTSUP;
    if(request.kind==SIGNAL_REQUEST_RETURN) {
        if(request.operation!=1 || request.output_address || request.output_size ||
            !signal_public_zero(request.wire+16,112))return UINT64_MAX;
        uint64_t args[2];for(size_t i=0;i<16;i++)((uint8_t *)args)[i]=request.wire[i];
        *owner.state=next;
        uint64_t restored=process_signal_return_private(frame,args[0],args[1]);
        return restored?restored:UINT64_MAX;
    }
    if(request.kind==SIGNAL_REQUEST_CONTROL)result=signal_public_control(frame,&owner,&request,&next,&exit_status);
    if(request.kind==SIGNAL_REQUEST_SUSPEND)result=signal_public_suspend(frame,&owner,&request,&next,&exit_status);
    if(request.kind==SIGNAL_REQUEST_IO)result=signal_public_io(frame,&owner,&request,&next,&exit_status);
    /* A refusal publishes only the cleared association and negative RAX. */
    if(result<0)*owner.state=next;
    frame->rax=(uint64_t)result;
    return exit_status;
}
#endif
