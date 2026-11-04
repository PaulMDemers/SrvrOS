/* Actual process transaction; scheduler/FPU/VMM are explicit models. */
#define main copy_fixture_unused_main
#define SRVROS_PROCESS_COPY_CUSTOM_OBSERVERS
#include "test_process_user_frame_copy.c"
#undef main

static bool post_store,exec_ok=true,commit_prepare_ok=true,snapshot_ok=true,consumed;
static unsigned commits;
static struct fpu_user_trap_proof mock_proof;
static struct process full_before;
static uint8_t bytes_before[sizeof(destination_bytes)];
void *scheduler_current_user_context(void) {assert(!post_store);return observation.process;}
void *scheduler_current_user_thread_context(void) {assert(!post_store);return observation.user_thread;}
bool scheduler_observe_user_return(struct scheduler_user_return_observation *out) {
    assert(!post_store);if(!observation_ok || (test_flags&0x200))return false;*out=observation;return true;
}
bool scheduler_observe_user_worker_stack(struct scheduler_user_worker_stack *out) {
    assert(!post_store);worker_queries++;if(!worker_ok)return false;*out=worker_stack;return true;
}
bool vmm_user_frame_copy_policy_current(uint64_t root) {
    assert(!post_store && root==0x2000 && !(test_flags&0x200));policies++;policy_success_pending=policy_ok;return policy_ok;
}
bool fpu_validate_user_trap(const struct isr_frame *f) {
    assert(!post_store && f && policy_success_pending);policy_success_pending=false;proofs++;return proof_ok;
}
bool vmm_user_exec_current(uint64_t address,uint64_t root) {assert(!post_store && root==0x2000);return exec_ok && (address==0x400000 || address==0x500000 || address==0x600000);}
bool fpu_snapshot_user_trap(const struct isr_frame *f,struct fpu_user_snapshot *out) {
    assert(!post_store && f);if(!snapshot_ok || consumed)return false;
    for(unsigned i=0;i<512;i++)out->bytes[i]=(uint8_t)(i^0x5a);return true;
}
bool fpu_prepare_user_trap_commit(const struct isr_frame *f,const struct fpu_user_snapshot *restore,struct fpu_user_trap_commit *out) {
    assert(!post_store && f);if(!commit_prepare_ok || consumed)return false;
    *out=(struct fpu_user_trap_commit){.proof=&mock_proof,.user_fpu=observation.user_fpu,.restore=restore};return true;
}
void fpu_commit_user_trap_prepared(const struct fpu_user_trap_commit *commit) {
    assert(commit->proof==&mock_proof && !consumed);
    if(commit->restore)memcpy(commit->user_fpu->fxsave_area,commit->restore->bytes,512);
    consumed=true;commits++;post_store=true;
}
bool vmm_copy_user_frame_excluding_current(uint64_t root,uint64_t dst,const struct user_frame_source *source,size_t length,
    const struct user_frame_stack *stack,const struct user_frame_page pages[2],size_t count,const struct isr_frame *frame,
    const struct fpu_state *user,const struct fpu_state *kernel,const struct user_frame_exclusion *ex,size_t n) {
    assert(!post_store && root==0x2000 && frame && length==760 && dst>=0x1000 && dst+length<=0x3000);
    assert(stack->frames==32 && user==observation.user_fpu && kernel==&kernel_fpu && count==2);
    assert(pages[0].virtual_address==0x1000 && pages[1].virtual_address==0x2000);
    struct process *p=observation.process;struct process_user_thread *worker=observation.user_thread;
    assert(n==(worker?3u:2u));
    assert(ex[0].address==(uint64_t)(uintptr_t)(worker?&worker->signal_state:&p->main_signal_state));
    assert(ex[0].length==sizeof(struct signal_thread_state));
    assert(ex[1].address==(uint64_t)(uintptr_t)&p->signal_catch_mask &&
        ex[1].length==offsetof(struct process,main_signal_state)-offsetof(struct process,signal_catch_mask));
    if(worker)assert(ex[2].address==(uint64_t)(uintptr_t)&worker->signal_mask && ex[2].length==16);
    copies++;if(!copy_ok)return false;
    memcpy(destination_bytes+(dst-0x1000),source->bytes,length);post_store=true;return true;
}
bool vmm_copy_user_frame_extended_current(uint64_t root,uint64_t dst,const struct user_frame_source *source,size_t length,
    const struct user_frame_stack *stack,const struct user_frame_page pages[2],size_t count,const struct isr_frame *frame,
    const struct fpu_state *user,const struct fpu_state *kernel,const struct user_frame_exclusion *ex,size_t n) {
    assert(!post_store && root==0x2000 && frame && length==760 && dst>=0x1000 && dst+length<=0x3000);
    assert(stack->frames==32 && user==observation.user_fpu && kernel==&kernel_fpu && count==2);
    assert(pages[0].virtual_address==0x1000 && pages[1].virtual_address==0x2000);
    struct process *p=observation.process;struct process_user_thread *worker=observation.user_thread;
    assert(n==10 && ex[0].address==(uint64_t)(uintptr_t)(worker?&worker->signal_state:&p->main_signal_state));
    assert(ex[0].length==sizeof(struct signal_thread_state));
    assert(ex[1].address==(uint64_t)(uintptr_t)p && ex[1].length==offsetof(struct process,main_signal_state));
    for(size_t i=0;i<8;i++) {
        assert(ex[i+2].address==(uint64_t)(uintptr_t)&p->user_threads[i].signal_mask);
        assert(ex[i+2].length==16);
    }
    copies++;if(!copy_ok)return false;
    memcpy(destination_bytes+(dst-0x1000),source->bytes,length);post_store=true;return true;
}
static struct isr_frame make_frame(void) {
    return (struct isr_frame){.cs=0x23,.ss=0x1b,.rip=0x600000,.rsp=0x2300,.rflags=0x202,
        .rax=0x1234,.rdi=0x5678,.r15=0xdeadbeef};
}
static void init_case(struct process *p,bool worker) {
    setup(p);post_store=consumed=false;exec_ok=commit_prepare_ok=snapshot_ok=true;
    p->usr1_action=(struct signal_action_descriptor){.mode=SIGNAL_ACTION_INFO,.flags=SIGNAL_ACTION_RESTART,
        .handler=0x400000,.trampoline=0x500000,.mask=1ull<<12};
    p->usr1_action_generation=4;p->signal_catch_mask=1ull<<SRV_SIGNAL_USR1;
    if(worker) {
        struct process_user_thread *w=&p->user_threads[0];*w=(struct process_user_thread){.used=true,.active=true,.tid=2,.owner=p};
        observation.user_thread=w;observation.user_fpu=&w->fpu;
        worker_stack=(struct scheduler_user_worker_stack){7,0xffff800004000000ull,0x4000000,32,observation.kernel_stack_top};
        w->pending_signals=1ull<<SRV_SIGNAL_USR1;w->signal_mask=1ull<<5;
    } else {p->main_pending_signals=1ull<<SRV_SIGNAL_USR1;p->main_signal_mask=1ull<<5;}
}
static void reject_delivery(struct process *p,struct isr_frame *frame,enum process_signal_delivery_result expected) {
    memcpy(&full_before,p,sizeof(*p));memcpy(bytes_before,destination_bytes,sizeof(bytes_before));
    struct isr_frame old=*frame;struct fpu_user_trap_proof old_proof=mock_proof;
    struct fpu_state old_user=*observation.user_fpu,old_kernel=kernel_fpu;
    bool old_consumed=consumed;unsigned old_commits=commits;
    assert(process_signal_deliver_private(frame)==expected);
    assert(!memcmp(p,&full_before,sizeof(*p)) && !memcmp(frame,&old,sizeof(old)) &&
        !memcmp(bytes_before,destination_bytes,sizeof(bytes_before)) && !memcmp(&old_proof,&mock_proof,sizeof(old_proof)) &&
        !memcmp(&old_user,observation.user_fpu,sizeof(old_user)) && !memcmp(&old_kernel,&kernel_fpu,sizeof(old_kernel)) && consumed==old_consumed && commits==old_commits && !post_store);
}
static void reject_return(struct process *p,struct isr_frame *frame,uint64_t token,uint64_t identity) {
    memcpy(&full_before,p,sizeof(*p));memcpy(bytes_before,destination_bytes,sizeof(bytes_before));
    struct isr_frame old=*frame;struct fpu_user_trap_proof old_proof=mock_proof;
    struct fpu_state old_user=*observation.user_fpu,old_kernel=kernel_fpu;
    bool old_consumed=consumed;unsigned old_commits=commits;
    assert(!process_signal_return_private(frame,token,identity));
    assert(!memcmp(p,&full_before,sizeof(*p)) && !memcmp(frame,&old,sizeof(old)) &&
        !memcmp(bytes_before,destination_bytes,sizeof(bytes_before)) && !memcmp(&old_proof,&mock_proof,sizeof(old_proof)) &&
        !memcmp(&old_user,observation.user_fpu,sizeof(old_user)) && !memcmp(&old_kernel,&kernel_fpu,sizeof(old_kernel)) && consumed==old_consumed && commits==old_commits && !post_store);
}
static void return_pending_cases(struct process *p,bool worker) {
    const uint64_t usr1=1ull<<SRV_SIGNAL_USR1,usr2=1ull<<SRV_SIGNAL_USR2;
    const uint64_t ignored=(1ull<<SRV_SIGNAL_CHLD)|(1ull<<SRV_SIGNAL_WINCH);
    for(unsigned scenario=0;scenario<5;scenario++) {
        init_case(p,worker);struct isr_frame frame=make_frame(),initial=frame;
        if(scenario==1 || scenario==2)p->usr1_action.flags|=SIGNAL_ACTION_RESET;
        if(scenario==3)p->usr1_action.mask|=ignored;
        assert(process_signal_deliver_private(&frame)==PROCESS_SIGNAL_DELIVERED);
        uint64_t address=frame.rsp;consumed=post_store=false;
        uint64_t *local=worker?&p->user_threads[0].pending_signals:&p->main_pending_signals;
        uint64_t *mask=worker?&p->user_threads[0].signal_mask:&p->main_signal_mask;
        p->user_threads[1]=(struct process_user_thread){.used=true,.active=true,.tid=3,.owner=p};
        uint64_t *other=&p->user_threads[1].pending_signals;
        *other=ignored;
        if(scenario==0) {*local|=usr2;p->pending_signals|=usr2;}
        if(scenario==1) p->pending_signals|=usr1;
        if(scenario==2) {*local|=usr1;p->pending_signals|=usr1;}
        if(scenario==3) {*local|=ignored;p->pending_signals|=ignored;}
        if(scenario==4) {*local|=usr2;p->pending_signals|=usr2;p->signal_catch_mask|=usr2;}
        commit_prepare_ok=false;reject_return(p,&frame,1,address);commit_prepare_ok=true;
        uint64_t result=process_signal_return_private(&frame,1,address);
        assert(result==(scenario==0?140u:(scenario==1 || scenario==2)?138u:PROCESS_SIGNAL_RETURN_RESTORED));
        assert(consumed && post_store && !memcmp(&frame,&initial,sizeof(frame)) && *mask==(1ull<<5));
        for(unsigned i=0;i<512;i++)assert(observation.user_fpu->fxsave_area[i]==(uint8_t)(i^0x5a));
        assert(*other==ignored);
        if(scenario==0)assert(p->killed && p->kill_signal==SRV_SIGNAL_USR2 && !(*local&usr2) && (p->pending_signals&usr2));
        if(scenario==1)assert(p->killed && p->kill_signal==SRV_SIGNAL_USR1 && !(p->pending_signals&usr1));
        if(scenario==2)assert(p->killed && p->kill_signal==SRV_SIGNAL_USR1 && !(*local&usr1) && (p->pending_signals&usr1));
        if(scenario==1 || scenario==2)assert(p->usr1_action.mode==SIGNAL_ACTION_DEFAULT);
        if(scenario==3)assert(!p->killed && !(*local&ignored) && !(p->pending_signals&ignored));
        if(scenario==4)assert(!p->killed && (*local&usr2) && (p->pending_signals&usr2));
    }
}
/* RESET stages the selected consumption and the handler's self-mask before
 * finding a default-fatal peer. No handler may run after an exit result. */
static void reset_peer_cases(struct process *p,bool worker) {
    const uint64_t bit=1ull<<SRV_SIGNAL_USR1;
    for(unsigned scenario=0;scenario<10;scenario++) {
        init_case(p,worker);p->usr1_action.flags|=SIGNAL_ACTION_RESET;
        struct isr_frame frame=make_frame(),initial=frame;
        uint64_t *local=worker?&p->user_threads[0].pending_signals:&p->main_pending_signals;
        uint64_t *mask=worker?&p->user_threads[0].signal_mask:&p->main_signal_mask;
        struct process_user_thread *a=&p->user_threads[1],*b=&p->user_threads[2];
        *a=(struct process_user_thread){.used=true,.active=true,.tid=7,.owner=p};
        *b=(struct process_user_thread){.used=true,.active=true,.tid=3,.owner=p};
        if(worker)p->main_signal_mask|=bit;
        if(scenario==0) {a->pending_signals=bit;b->pending_signals=bit;p->pending_signals=bit;}
        if(scenario==1) p->pending_signals=bit;
        if(scenario==2) {a->pending_signals=bit;a->signal_mask=bit;p->pending_signals=bit;b->signal_mask=bit;}
        if(scenario==3) {a->pending_signals=bit;a->active=false;b->pending_signals=bit;b->owner=&processes[1];}
        if(scenario==4) {*local=0;p->pending_signals=bit;}
        if(scenario==5) {a->pending_signals=bit;b->pending_signals=bit;b->signal_mask=bit;}
        if(scenario==6) {a->pending_signals=bit;b->pending_signals=bit;if(!worker)p->main_pending_signals=bit;}
        if(scenario==7) {p->pending_signals=bit;a->signal_mask=bit;b->signal_mask=bit;}
        if(scenario==8 && worker) {p->main_signal_mask&=~bit;p->main_pending_signals=bit;a->pending_signals=bit;}
        if(scenario==9) {*local=0;p->pending_signals=bit;a->pending_signals=bit;}
        commit_prepare_ok=false;reject_delivery(p,&frame,PROCESS_SIGNAL_FATAL);commit_prepare_ok=true;
        copy_ok=false;reject_delivery(p,&frame,PROCESS_SIGNAL_FATAL);copy_ok=true;
        enum process_signal_delivery_result result=process_signal_deliver_private(&frame);
        assert(consumed && post_store && p->usr1_action.mode==SIGNAL_ACTION_DEFAULT && p->usr1_action_generation==5);
        assert((*mask&bit) && !(*local&bit));
        if(scenario==0 || scenario==1 || scenario==5 || scenario==6 || (scenario==8 && worker) || scenario==9) {
            assert(result==PROCESS_SIGNAL_DELIVERED_EXIT_USR1 && p->killed && p->kill_signal==SRV_SIGNAL_USR1);
            if(scenario==0)assert(!(b->pending_signals&bit) && (a->pending_signals&bit) && (p->pending_signals&bit));
            if(scenario==1)assert(!(p->pending_signals&bit));
            if(scenario==5)assert(!(a->pending_signals&bit) && (b->pending_signals&bit));
            if(scenario==6)assert(!(b->pending_signals&bit) && (a->pending_signals&bit));
            if(scenario==8 && worker)assert(!(p->main_pending_signals&bit) && (a->pending_signals&bit));
            if(scenario==9)assert(!(a->pending_signals&bit) && !(p->pending_signals&bit));
        } else {
            assert(result==PROCESS_SIGNAL_DELIVERED && !p->killed);
            if(scenario==3)assert((a->pending_signals&bit) && (b->pending_signals&bit));
            if(scenario==2 || scenario==7) {
                assert(p->pending_signals&bit);
                uint64_t address=frame.rsp;consumed=post_store=false;
                assert(process_signal_return_private(&frame,1,address)==138 && p->killed);
                assert(!memcmp(&frame,&initial,sizeof(frame)));
            }
        }
    }
    init_case(p,worker);p->usr1_action.flags|=SIGNAL_ACTION_RESET;p->killed=true;
    struct isr_frame frame=make_frame();reject_delivery(p,&frame,PROCESS_SIGNAL_DEFERRED);
}
int main(void) {
    struct process *p=&processes[0];
    for(unsigned worker=0;worker<2;worker++) {
        init_case(p,worker);struct isr_frame frame=make_frame(),initial=frame;
        uint64_t bit=1ull<<SRV_SIGNAL_USR1;p->pending_signals=bit;
        assert(process_signal_deliver_private(&frame)==PROCESS_SIGNAL_DELIVERED && consumed && post_store);
        struct signal_thread_state *state=worker?&p->user_threads[0].signal_state:&p->main_signal_state;
        assert(state->stack.depth==1 && state->stack.records[0].token==1);
        assert((worker?p->user_threads[0].pending_signals:p->main_pending_signals)==0 && p->pending_signals==bit);
        assert((worker?p->user_threads[0].signal_mask:p->main_signal_mask)==((1ull<<5)|(1ull<<12)|bit));
        uint64_t address=frame.rsp;assert(frame.rip==0x500000 && frame.rcx==0x400000 && frame.r8==1 && frame.r9==address);
        assert(address+760<=initial.rsp-128 && address>=0x1000 && address+760<=0x3000);
        size_t offset=(size_t)(address-0x1000);
        for(size_t i=0;i<offset;i++)assert(destination_bytes[i]==0xa5);
        for(size_t i=offset+760;i<sizeof(destination_bytes);i++)assert(destination_bytes[i]==0xa5);
        for(unsigned i=0;i<8;i++)assert(destination_bytes[offset+i]==0);
        struct signal_context_snapshot context;memcpy(&context,destination_bytes+offset+8,sizeof(context));
        assert(context.size==sizeof(context) && context.r15==initial.r15 && context.rax==initial.rax && context.rsp==initial.rsp && context.token==1);
        for(unsigned i=0;i<512;i++)assert(context.fpu[i]==(uint8_t)(i^0x5a));
        struct process_signal_private_info info;memcpy(&info,destination_bytes+offset+8+sizeof(context),sizeof(info));
        struct process_signal_private_info expected_info={.size=sizeof(info),.version=1,.signo=SRV_SIGNAL_USR1,
            .valid=PROCESS_SIGNAL_INFO_SIGNO|PROCESS_SIGNAL_INFO_TARGET_TID,.target_tid=worker?2u:1u};
        assert(!memcmp(&info,&expected_info,sizeof(info)));
        memset(destination_bytes+offset,0xcc,760);consumed=post_store=false;
        reject_return(p,&frame,2,address);reject_return(p,&frame,1,address+16);
        assert(process_signal_return_private(&frame,1,address)==PROCESS_SIGNAL_RETURN_RESTORED && consumed && !memcmp(&frame,&initial,sizeof(frame)));
        assert((worker?p->user_threads[0].signal_mask:p->main_signal_mask)==(1ull<<5));
        for(unsigned i=0;i<512;i++)assert(observation.user_fpu->fxsave_area[i]==(uint8_t)(i^0x5a));
        consumed=post_store=false;reject_return(p,&frame,1,address);
    }
    return_pending_cases(p,false);return_pending_cases(p,true);
    reset_peer_cases(p,false);reset_peer_cases(p,true);
    init_case(p,true);struct isr_frame stale=make_frame();
    assert(process_signal_deliver_private(&stale)==PROCESS_SIGNAL_DELIVERED);consumed=post_store=false;
    consumed=true;reject_return(p,&stale,1,stale.rsp);consumed=false;
    p->user_threads[0].signal_state.ranges[0].end--;
    reject_return(p,&stale,1,stale.rsp);p->user_threads[0].signal_state.ranges[0].end++;
    p->user_threads[0].tid=3;reject_return(p,&stale,1,stale.rsp);p->user_threads[0].tid=2;
    observation.image_generation=2;reject_return(p,&stale,1,stale.rsp);observation.image_generation=1;
    commit_prepare_ok=false;reject_return(p,&stale,1,stale.rsp);commit_prepare_ok=true;
    init_case(p,false);struct isr_frame frame=make_frame();
    exec_ok=false;reject_delivery(p,&frame,PROCESS_SIGNAL_FATAL);exec_ok=true;
    snapshot_ok=false;reject_delivery(p,&frame,PROCESS_SIGNAL_FATAL);snapshot_ok=true;
    commit_prepare_ok=false;reject_delivery(p,&frame,PROCESS_SIGNAL_FATAL);commit_prepare_ok=true;
    copy_ok=false;reject_delivery(p,&frame,PROCESS_SIGNAL_FATAL);copy_ok=true;
    struct signal_state_identity id={1,1,7};assert(signal_thread_state_bind(&p->main_signal_state,id));
    for(unsigned i=0;i<4;i++) {
        struct signal_context_record record={.frame=frame,.image_generation=1,.thread_generation=7,
            .token=i+1,.frame_identity=0x10000+i*0x400};
        assert(signal_thread_state_push(&p->main_signal_state,id,&record,
            (struct signal_context_range){record.frame_identity,record.frame_identity+760}));
    }
    reject_delivery(p,&frame,PROCESS_SIGNAL_DEFERRED);
    signal_thread_state_reset(&p->main_signal_state);
    p->main_signal_state.identity=id;p->main_signal_state.stack.last_token=UINT64_MAX;
    reject_delivery(p,&frame,PROCESS_SIGNAL_FATAL);
    signal_thread_state_reset(&p->main_signal_state);p->usr1_action.flags|=SIGNAL_ACTION_RESET;p->usr1_action_generation=UINT64_MAX;
    reject_delivery(p,&frame,PROCESS_SIGNAL_FATAL);
    p->usr1_action_generation=4;assert(process_signal_deliver_private(&frame)==PROCESS_SIGNAL_DELIVERED);
    assert(p->usr1_action.mode==SIGNAL_ACTION_DEFAULT && p->usr1_action_generation==5 && !(p->signal_catch_mask&(1ull<<SRV_SIGNAL_USR1)));
    puts("process-signal-delivery: modeled private delivery and return");return 0;
}
