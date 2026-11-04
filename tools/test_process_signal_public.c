#define SRVROS_SIGNAL_PUBLIC_GUEST_TEST
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
bool scheduler_observe_user_entry(struct scheduler_user_return_observation *out) {
    assert(!post_store);if(!observation_ok || (test_flags&0x200))return false;*out=observation;return true;
}
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
    assert(!post_store && root==0x2000 && frame && length && length<=1024 && dst>=0x1000 && dst+length<=0x3000);
    assert(stack->frames==32 && user==observation.user_fpu && kernel==&kernel_fpu && count>=1 && count<=2);
    struct process *p=observation.process;struct process_user_thread *worker=observation.user_thread;
    assert(n==10 && ex[0].address==(uint64_t)(uintptr_t)(worker?&worker->signal_state:&p->main_signal_state));
    assert(ex[0].length==sizeof(struct signal_thread_state));
    assert(ex[1].address==(uint64_t)(uintptr_t)p && ex[1].length==offsetof(struct process,main_signal_state));
    for(size_t i=0;i<8;i++)assert(ex[i+2].address==(uint64_t)(uintptr_t)&p->user_threads[i].signal_mask && ex[i+2].length==16);
    (void)pages;copies++;if(!copy_ok)return false;
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
static bool input_ok=true,range_ok=true;static unsigned input_reads;
bool syscall_signal_copy_input(void *out,uint64_t addr,size_t length) {
    assert(!post_store && length<=1024);input_reads++;if(!length)return true;
    if(!input_ok || addr<0x1000 || addr+length>0x3000)return false;
    memcpy(out,destination_bytes+(addr-0x1000),length);return true;
}
bool vmm_user_range_mapped(uint64_t addr,uint64_t length,bool writable) {
    assert(!post_store && !writable && length==1);return range_ok && addr>=0x1000 && addr<0x3000;
}
static unsigned wait_calls,yield_calls,io_wait_calls,safe_calls;
bool scheduler_wait(struct scheduler_wait_queue *queue,scheduler_wait_condition_fn condition,void *arg) {
    assert(queue && condition && !post_store);wait_calls++;
    struct process *p=observation.process;
    if(observation.user_thread)((struct process_user_thread *)observation.user_thread)->pending_signals|=UINT64_C(1)<<SRV_SIGNAL_USR1;
    else p->main_pending_signals|=UINT64_C(1)<<SRV_SIGNAL_USR1;
    assert(condition(arg));return true;
}
void scheduler_yield(void) { assert(!post_store);yield_calls++; }
void scheduler_io_set_user_safe(bool safe) {assert(!post_store && !safe);safe_calls++;}
void scheduler_io_wait(void) {assert(!post_store);io_wait_calls++;}
static int64_t suspend_request(struct isr_frame *f,uint64_t op,uint64_t a,uint64_t b) {
    uint64_t args[3]={op,a,b};uint8_t wire[128]={0};memcpy(wire,args,sizeof args);
    post_store=false;assert(process_signal_public_stage(f,SIGNAL_REQUEST_SUSPEND,op,wire,0,0)==0);
    assert(process_signal_public_complete(f)==0);post_store=false;return (int64_t)f->rax;
}
static void suspend_cases(bool worker) {
    const uint64_t usr1=UINT64_C(1)<<SRV_SIGNAL_USR1;
    struct process *p=&processes[0];init_case(p,worker);struct isr_frame frame=make_frame();
    uint64_t *mask=worker?&p->user_threads[0].signal_mask:&p->main_signal_mask;
    uint64_t *pending=worker?&p->user_threads[0].pending_signals:&p->main_pending_signals;
    *mask=0x20;unsigned oldwait=wait_calls;
    int64_t ticket=suspend_request(&frame,SRV_SIGNAL_SUSPEND_BEGIN,0,frame.rsp+16);
    assert(ticket>0 && *mask==0 && wait_calls==oldwait && io_wait_calls && safe_calls);
    assert(process_signal_deliver_private(&frame)==PROCESS_SIGNAL_DELIVERED);
    struct signal_thread_state *state=worker?&p->user_threads[0].signal_state:&p->main_signal_state;
    assert(state->stack.records[0].wait_mode==SIGNAL_WAIT_MASK_OVERRIDE && state->stack.records[0].mask==0x20);
    uint64_t address=frame.rsp;post_store=consumed=false;
    assert(process_signal_return_private(&frame,1,address)==PROCESS_SIGNAL_RETURN_RESTORED && *mask==0x20);
    post_store=consumed=false;assert(suspend_request(&frame,SRV_SIGNAL_SUSPEND_FINISH,(uint64_t)ticket,0)==SRV_ERR_INTR);
    assert(*mask==0x20 && !state->waits[0].ticket);

    /* An initial non-INFO wake becomes LEGACY_READY; later INFO preserves
     * the interrupted temporary mask and FINISH owns the final restoration. */
    init_case(p,worker);frame=make_frame();*mask=0x20;*pending=UINT64_C(1)<<SRV_SIGNAL_USR2;
    p->signal_catch_mask|=UINT64_C(1)<<SRV_SIGNAL_USR2;
    ticket=suspend_request(&frame,SRV_SIGNAL_SUSPEND_BEGIN,0,frame.rsp+16);
    assert(ticket>0 && (state->waits[0].phase_flags&255)==SIGNAL_WAIT_LEGACY_READY);
    *pending=usr1;assert(process_signal_deliver_private(&frame)==PROCESS_SIGNAL_DELIVERED);
    assert(state->stack.records[0].wait_mode==SIGNAL_WAIT_COMPLETION_ONLY && state->stack.records[0].mask==0);
    address=frame.rsp;post_store=consumed=false;
    assert(process_signal_return_private(&frame,1,address)==PROCESS_SIGNAL_RETURN_RESTORED && *mask==0);
    post_store=consumed=false;assert(suspend_request(&frame,SRV_SIGNAL_SUSPEND_FINISH,(uint64_t)ticket,0)==SRV_ERR_INTR && *mask==0x20);

    /* FINISH(false) atomically rearms and the scheduler mock supplies a new
     * pending INFO before its condition completes. */
    init_case(p,worker);frame=make_frame();*mask=0x20;*pending=UINT64_C(1)<<SRV_SIGNAL_USR2;
    p->signal_catch_mask|=UINT64_C(1)<<SRV_SIGNAL_USR2;
    ticket=suspend_request(&frame,SRV_SIGNAL_SUSPEND_BEGIN,0,frame.rsp+16);assert(ticket>0);
    *pending=0;oldwait=wait_calls;
    assert(suspend_request(&frame,SRV_SIGNAL_SUSPEND_FINISH,(uint64_t)ticket,0)==ticket && wait_calls==oldwait+1 && *mask==0);
    assert((state->waits[0].phase_flags&255)==SIGNAL_WAIT_RETURNING && (*pending&usr1));
}
static void nested_constraints(void) {
    struct process *p=&processes[0];init_case(p,false);struct isr_frame frame=make_frame();
    assert(process_signal_deliver_private(&frame)==PROCESS_SIGNAL_DELIVERED);post_store=consumed=false;
    p->usr1_action.flags|=SIGNAL_ACTION_RESET;
    assert(suspend_request(&frame,SRV_SIGNAL_SUSPEND_BEGIN,0,frame.rsp+16)==SRV_ERR_NOTSUP);
    p->usr1_action.flags&=~SIGNAL_ACTION_RESET;p->signal_catch_mask|=UINT64_C(1)<<SRV_SIGNAL_USR2;
    assert(suspend_request(&frame,SRV_SIGNAL_SUSPEND_BEGIN,0,frame.rsp+16)==SRV_ERR_NOTSUP);
    p->signal_catch_mask&=~(UINT64_C(1)<<SRV_SIGNAL_USR2);p->main_pending_signals|=UINT64_C(1)<<SRV_SIGNAL_USR1;
    int64_t ticket=suspend_request(&frame,SRV_SIGNAL_SUSPEND_BEGIN,0,frame.rsp+16);assert(ticket>0);
    assert((p->main_signal_state.waits[1].phase_flags&SIGNAL_WAIT_INFO_ONLY)!=0);
    struct srv_signal_control_v1 w={.size=96,.version=1,.operation=SRV_SIGNAL_CONTROL_EXCHANGE_ACTION,
        .signum=SRV_SIGNAL_USR1,.action={.handler=0}};
    uint8_t wire[128]={0};memcpy(wire,&w,sizeof w);
    assert(process_signal_public_stage(&frame,SIGNAL_REQUEST_CONTROL,w.operation,wire,0,0)==0);
    assert(process_signal_public_complete(&frame)==0 && (int64_t)frame.rax==SRV_ERR_BUSY);
    assert(p->usr1_action.mode==SIGNAL_ACTION_INFO && p->main_signal_state.waits[1].ticket==(uint64_t)ticket);
    w.action.handler=0x400000;w.action.flags=SRV_SIGNAL_NATIVE_INFO|SRV_SIGNAL_NATIVE_RESET;w.trampoline=0x500000;
    memset(wire,0,sizeof wire);memcpy(wire,&w,sizeof w);
    assert(process_signal_public_stage(&frame,SIGNAL_REQUEST_CONTROL,w.operation,wire,0,0)==0);
    assert(process_signal_public_complete(&frame)==0 && (int64_t)frame.rax==SRV_ERR_BUSY);
    assert(!(p->usr1_action.flags&SIGNAL_ACTION_RESET));
}
static void stage_control(struct isr_frame *f,struct srv_signal_control_v1 *w,uint64_t dst,uint64_t size) {
    uint8_t bytes[128]={0};memcpy(bytes,w,sizeof(*w));post_store=false;
    assert(process_signal_public_stage(f,SIGNAL_REQUEST_CONTROL,w->operation,bytes,dst,size)==0);
}
static struct srv_signal_control_v1 control(unsigned op) {
    return (struct srv_signal_control_v1){.size=96,.version=1,.operation=op};
}
static void finish(struct isr_frame *f,int64_t result,uint64_t status) {
    assert(process_signal_public_complete(f)==status);assert((int64_t)f->rax==result);
    post_store=false;
}
static int64_t io_request(struct isr_frame *f,struct srv_signal_io_v1 *w,uint64_t output,uint64_t size) {
    post_store=false;assert(process_signal_public_stage(f,SIGNAL_REQUEST_IO,w->operation,(const uint8_t *)w,output,size)==0);
    assert(process_signal_public_complete(f)==0);post_store=false;return (int64_t)f->rax;
}
static struct srv_signal_io_v1 io_wire(unsigned op) {
    return (struct srv_signal_io_v1){.size=128,.version=1,.operation=op};
}
static void io_cases(void) {
    struct process *p=&processes[0];init_case(p,false);struct isr_frame f=make_frame();
    p->files[3].used=true;p->file_generations[3]=7;
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    pipes[0]=(struct process_pipe){.used=true,.refs=2,.read_refs=1,.write_refs=1};
    p->files[3].type=PROCESS_FILE_PIPE_READ;p->files[3].handle=1;
#endif
    struct srv_signal_descriptor_v1 vector[2]={{.fd=3},{.fd=-1}};
    memcpy(destination_bytes+4080,vector,sizeof vector);
    struct srv_signal_io_v1 w=io_wire(SRV_SIGNAL_IO_BEGIN);w.args[0]=f.rsp+16;w.args[2]=SRV_SIGNAL_ROOT_READ;w.args[3]=SYS_READ;
    w.deadline=UINT64_MAX;w.descriptor_vector=0x1ff0;w.descriptor_count=1;
    uint64_t seq=p->main_signal_state.io_sequence;copy_ok=false;
    assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_FAULT && p->main_signal_state.io_sequence==seq && !p->main_signal_state.io_top);
    copy_ok=true;input_ok=false;assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_FAULT && p->main_signal_state.io_sequence==seq);input_ok=true;
    vector[0].value=1;memcpy(destination_bytes+4080,vector,16);
    assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_INVAL && p->main_signal_state.io_sequence==seq);
    vector[0].value=0;vector[0].fd=17;memcpy(destination_bytes+4080,vector,16);
    assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_BADF);
    vector[0].fd=3;memcpy(destination_bytes+4080,vector,16);p->file_generations[3]=UINT64_MAX;
    assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_OVERFLOW);p->file_generations[3]=7;
    int64_t ticket=io_request(&f,&w,0x1ff0,16);assert(ticket>0);
    memcpy(vector,destination_bytes+4080,16);assert(vector[0].fd==3 && vector[0].value==7);
    struct signal_state_identity id={1,1,7};uint64_t cookie;
    assert(signal_io_prepare_outcome(&p->main_signal_state,id,0,ticket,SYS_READ,&cookie));
    assert(signal_io_commit_outcome(&p->main_signal_state,id,0,ticket,cookie,SYS_READ,9,4,2,true,true,true));
    w=io_wire(SRV_SIGNAL_IO_RESULT);w.ticket=ticket;
    assert(io_request(&f,&w,0x1ff0,64)==0);struct srv_signal_io_result_v1 result;
    memcpy(&result,destination_bytes+4080,sizeof result);assert(result.cookie==cookie && result.result==9 && result.syscall_number==SYS_READ && result.elapsed_us==4 && result.elapsed_ticks==2 && result.validity==(3|SRV_SIGNAL_IO_RESULT_COMPLETED_NATIVE));
    assert(io_request(&f,&w,0x1ff0,64)==0);struct srv_signal_io_result_v1 again;memcpy(&again,destination_bytes+4080,sizeof again);assert(!memcmp(&result,&again,sizeof result));
    w=io_wire(SRV_SIGNAL_IO_END);w.ticket=ticket;assert(io_request(&f,&w,0,0)==0 && io_request(&f,&w,0,0)==SRV_ERR_INVAL);
    w=io_wire(SRV_SIGNAL_IO_BEGIN);w.args[0]=f.rsp+16;w.args[2]=SRV_SIGNAL_ROOT_POLL;w.args[3]=SYS_POLL;w.flags=SRV_SIGNAL_IO_POLL_LIST;
    w.descriptor_vector=0x1ff0;w.descriptor_count=2;vector[0]=(struct srv_signal_descriptor_v1){.fd=17};vector[1]=(struct srv_signal_descriptor_v1){.fd=-1};
    memcpy(destination_bytes+4080,vector,sizeof vector);ticket=io_request(&f,&w,0x1ff0,sizeof vector);assert(ticket>0);
    memcpy(vector,destination_bytes+4080,sizeof vector);assert(vector[0].value==UINT64_MAX && vector[1].value==UINT64_MAX);
    w=io_wire(SRV_SIGNAL_IO_LOCAL_BEGIN);w.ticket=ticket;int64_t guard=io_request(&f,&w,0,0);assert(guard>0 && (p->main_signal_mask&(1ull<<SRV_SIGNAL_USR1)));
    unsigned previous_policies=policies,previous_proofs=proofs,previous_wait=io_wait_calls;
    struct isr_frame entry=make_frame();entry.vector=0x20;
    observation.thread_identity=8;assert(process_signal_public_entry(&entry)==139 && p->main_signal_state.local.cookie==(uint64_t)guard);
    observation.thread_identity=7;assert(process_signal_public_entry(&entry)==4);
    entry.vector=0x80;entry.rax=SYS_THREAD_SELF;entry.rdi=entry.rsi=entry.rdx=entry.rcx=entry.r8=entry.r9=0;assert(process_signal_public_entry(&entry)==2);
    entry.rax=SYS_FUTEX_WAKE;entry.rdi=0x1000;entry.rsi=1;assert(process_signal_public_entry(&entry)==2);
    entry.rsi=2;assert(process_signal_public_entry(&entry)==3 && (int64_t)entry.rax==SRV_ERR_INVAL);
    entry=make_frame();entry.vector=0x80;entry.rax=SYS_EXIT;assert(process_signal_public_entry(&entry)==2);
    entry.rax=SYS_SIGNAL_IO;entry.r9=1;entry.rdi=SRV_SIGNAL_IO_LOCAL_END;entry.rsi=ticket;entry.rdx=guard+1;entry.rcx=entry.r8=0;
    assert(process_signal_public_entry(&entry)==3 && (int64_t)entry.rax==SRV_ERR_INVAL && p->main_signal_state.local.cookie==(uint64_t)guard);
    entry.rax=SYS_SIGNAL_IO;entry.rdx=guard;assert(process_signal_public_entry(&entry)==1 && !p->main_signal_state.local.flags);
    assert(policies==previous_policies && proofs==previous_proofs && io_wait_calls==previous_wait);
    w=io_wire(SRV_SIGNAL_IO_END);w.ticket=ticket;assert(io_request(&f,&w,0,0)==0);
}
static void io_mask_unwind_cases(void) {
    struct process *p=&processes[0];init_case(p,false);p->usr1_action=(struct signal_action_descriptor){0};
    p->signal_catch_mask=0;p->main_pending_signals=0;p->main_signal_mask=0x20;
    struct isr_frame f=make_frame();struct srv_signal_io_v1 w=io_wire(SRV_SIGNAL_IO_BEGIN);
    w.args[0]=0x2800;w.args[1]=0x40;w.args[2]=SRV_SIGNAL_ROOT_POLL;w.args[3]=SYS_POLL;w.flags=SRV_SIGNAL_IO_LEGACY_MASKED;
    int64_t outer=io_request(&f,&w,0,0);assert(outer>0 && p->main_signal_mask==0x40);
    w=io_wire(SRV_SIGNAL_IO_LOCAL_BEGIN);w.ticket=outer;int64_t guard=io_request(&f,&w,0,0);assert(guard>0 && p->main_signal_mask==0x40);
    struct isr_frame e=make_frame();e.vector=0x80;e.rax=SYS_SIGNAL_IO;e.rdi=SRV_SIGNAL_IO_LOCAL_END;e.rsi=outer;e.rdx=guard;e.rcx=e.r8=0;e.r9=1;
    assert(process_signal_public_entry(&e)==1 && p->main_signal_mask==0x40);
    w=io_wire(SRV_SIGNAL_IO_END);w.ticket=outer;assert(io_request(&f,&w,0,0)==0 && p->main_signal_mask==0x20);
    w=io_wire(SRV_SIGNAL_IO_BEGIN);w.args[0]=0x2800;w.args[2]=SRV_SIGNAL_ROOT_POLL;w.args[3]=SYS_POLL;
    outer=io_request(&f,&w,0,0);assert(outer>0);w.args[0]=0x2600;int64_t inner=io_request(&f,&w,0,0);assert(inner>0);
    w=io_wire(SRV_SIGNAL_IO_END);w.ticket=outer;assert(io_request(&f,&w,0,0)==SRV_ERR_INVAL);
    w=io_wire(SRV_SIGNAL_IO_UNWIND);w.args[0]=0x2700;assert(io_request(&f,&w,0,0)==0 && p->main_signal_state.io_top==(uint64_t)outer);
    w.args[0]=0x2900;assert(io_request(&f,&w,0,0)==0 && !p->main_signal_state.io_top);
    w.args[0]=0x3000;assert(io_request(&f,&w,0,0)==SRV_ERR_INVAL);
}
static void take_cases(void) {
    struct process *p=&processes[0];init_case(p,false);struct isr_frame f=make_frame();
    p->usr1_action=(struct signal_action_descriptor){.mode=SIGNAL_ACTION_LEGACY,.handler=0x400000,.flags=SIGNAL_ACTION_RESET};
    p->user_threads[1]=(struct process_user_thread){.used=true,.active=true,.tid=3,.owner=p,.pending_signals=UINT64_C(1)<<SRV_SIGNAL_USR1};
    struct srv_signal_control_v1 w=control(SRV_SIGNAL_CONTROL_LEGACY_TAKE);w.signum=SRV_SIGNAL_USR1;
    stage_control(&f,&w,0x1ff0,64);copy_ok=false;finish(&f,SRV_ERR_FAULT,0);copy_ok=true;
    assert(p->usr1_action.mode==SIGNAL_ACTION_LEGACY && (p->main_pending_signals&(1ull<<SRV_SIGNAL_USR1)) && !p->killed);
    stage_control(&f,&w,0x1ff0,64);finish(&f,0,138);assert(p->killed && p->kill_signal==SRV_SIGNAL_USR1);
    struct srv_signal_legacy_take_v1 take;memcpy(&take,destination_bytes+4080,sizeof take);
    assert(take.signum==SRV_SIGNAL_USR1 && take.action.handler==0x400000 && take.generation==4);
    init_case(p,false);f=make_frame();p->main_signal_mask|=1ull<<SRV_SIGNAL_USR1;
    w=control(SRV_SIGNAL_CONTROL_BLOCKED_TAKE);w.mask=1ull<<SRV_SIGNAL_USR1;
    stage_control(&f,&w,0x1ff0,4);copy_ok=false;finish(&f,SRV_ERR_FAULT,0);copy_ok=true;
    assert(p->main_pending_signals&(1ull<<SRV_SIGNAL_USR1));
    stage_control(&f,&w,0x1ff0,4);finish(&f,0,0);int32_t signum;memcpy(&signum,destination_bytes+4080,sizeof signum);
    assert(signum==SRV_SIGNAL_USR1 && !(p->main_pending_signals&(1ull<<SRV_SIGNAL_USR1)));
}
static void capture_descriptor_cases(void) {
    struct process *p=&processes[0];init_case(p,false);p->main_pending_signals=0;
    struct isr_frame f=make_frame();p->files[3].used=p->files[4].used=true;
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    pipes[0]=(struct process_pipe){.used=true,.refs=2,.read_refs=1,.write_refs=1};
    p->files[3].type=p->files[4].type=PROCESS_FILE_PIPE_READ;p->files[3].handle=p->files[4].handle=1;
#endif
    p->file_generations[3]=7;p->file_generations[4]=11;
    struct srv_signal_io_v1 w=io_wire(SRV_SIGNAL_IO_BEGIN);w.args[0]=0x2800;
    w.args[2]=SRV_SIGNAL_ROOT_EPOLL;w.args[3]=SYS_POLL;
    int64_t root=io_request(&f,&w,0,0);assert(root>0);
    w=io_wire(SRV_SIGNAL_IO_CAPTURE_DESCRIPTORS);w.ticket=root;
    assert(io_request(&f,&w,0,0)==0);
    struct srv_signal_io_v1 inner=io_wire(SRV_SIGNAL_IO_BEGIN);inner.args[0]=0x2700;
    inner.args[2]=SRV_SIGNAL_ROOT_EPOLL;inner.args[3]=SYS_POLL;
    int64_t inner_ticket=io_request(&f,&inner,0,0);assert(inner_ticket>0);
    assert(io_request(&f,&w,0,0)==SRV_ERR_INVAL);
    inner=io_wire(SRV_SIGNAL_IO_END);inner.ticket=inner_ticket;assert(io_request(&f,&inner,0,0)==0);
    struct signal_state_identity identity={1,1,7};struct signal_context_record rec={.frame=make_frame(),
        .image_generation=1,.thread_generation=7,.token=1,.frame_identity=0x1000};
    struct signal_context_range active={0x1000,0x1300},removed;struct signal_context_record restored;
    assert(signal_thread_state_push(&p->main_signal_state,identity,&rec,active));
    assert(io_request(&f,&w,0,0)==SRV_ERR_INVAL);
    assert(signal_thread_state_pop(&p->main_signal_state,identity,1,0x1000,&restored,&removed));
    struct srv_signal_descriptor_v1 vector={.fd=3};memcpy(destination_bytes+4080,&vector,sizeof vector);
    w.descriptor_vector=0x1ff0;w.descriptor_count=1;
    uint64_t sequence=p->main_signal_state.io_sequence,mask=p->main_signal_mask;
    unsigned reads=input_reads;memcpy(bytes_before,destination_bytes,sizeof bytes_before);copy_ok=false;
    assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_FAULT);copy_ok=true;
    assert(!memcmp(bytes_before,destination_bytes,sizeof bytes_before));
    assert(p->main_signal_state.io_sequence==sequence && p->main_signal_state.io_top==(uint64_t)root && p->main_signal_mask==mask);
    assert(input_reads==reads+1);assert(io_request(&f,&w,0x1ff0,16)==0);
    memcpy(&vector,destination_bytes+4080,sizeof vector);assert(vector.fd==3 && vector.value==7);
    assert(p->main_signal_state.io_sequence==sequence && p->main_signal_state.io_top==(uint64_t)root && p->main_signal_mask==mask);
    vector=(struct srv_signal_descriptor_v1){.fd=4};memcpy(destination_bytes+4080,&vector,sizeof vector);
    p->file_generations[4]=UINT64_MAX;assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_OVERFLOW);p->file_generations[4]=11;
    struct signal_state_identity id={1,1,7};assert(signal_io_mark_interrupted(&p->main_signal_state,id));
    w.descriptor_vector=0x4000;reads=input_reads;
    assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_INTR && input_reads==reads);
    w=io_wire(SRV_SIGNAL_IO_END);w.ticket=root;assert(io_request(&f,&w,0,0)==0);
    w=io_wire(SRV_SIGNAL_IO_BEGIN);w.args[0]=0x2800;w.args[2]=SRV_SIGNAL_ROOT_POLL;w.args[3]=SYS_POLL;
    root=io_request(&f,&w,0,0);assert(root>0);w=io_wire(SRV_SIGNAL_IO_CAPTURE_DESCRIPTORS);w.ticket=root;
    assert(io_request(&f,&w,0,0)==SRV_ERR_INVAL);
    w=io_wire(SRV_SIGNAL_IO_END);w.ticket=root;assert(io_request(&f,&w,0,0)==0);
    w=io_wire(SRV_SIGNAL_IO_BEGIN);w.args[0]=0x2800;w.args[2]=SRV_SIGNAL_ROOT_READ;w.args[3]=SYS_READ;
    root=io_request(&f,&w,0,0);assert(root>0);
    struct srv_signal_io_v1 call=io_wire(SRV_SIGNAL_IO_CALL);call.ticket=root;call.raw_syscall=SYS_READ;
    call.args[0]=3;call.args[1]=0x1000;call.args[2]=1;call.descriptor_count=1;call.descriptor_vector=0x1ff0;
    struct srv_signal_descriptor_v1 captured={3,7};p->file_generations[3]=8;uint64_t cookie=0,req=0;
    assert(process_signal_io_reserve(&f,&call,&captured,1,&cookie,&req)==SRV_ERR_BADF && !cookie && !req);
    w=io_wire(SRV_SIGNAL_IO_END);w.ticket=root;assert(io_request(&f,&w,0,0)==0);
}
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
static void experimental_policy_cases(void) {
    struct process *p=&processes[0];init_case(p,false);p->main_pending_signals=0;struct isr_frame f=make_frame();
    struct srv_signal_io_v1 w=io_wire(SRV_SIGNAL_IO_BEGIN);w.args[0]=0x2800;w.args[2]=SRV_SIGNAL_ROOT_RAW;w.args[3]=SYS_NET_CONNECT;
    uint64_t seq=p->main_signal_state.io_sequence;unsigned reads=input_reads,copied=copies;
    assert(io_request(&f,&w,0,0)==SRV_ERR_NOTSUP && p->main_signal_state.io_sequence==seq && input_reads==reads && copies==copied);
    p->files[3].used=true;p->files[3].type=PROCESS_FILE_NET_CONNECTION;p->file_generations[3]=5;
    struct srv_signal_descriptor_v1 v={3,0};memcpy(destination_bytes+4080,&v,sizeof v);
    w.args[2]=SRV_SIGNAL_ROOT_READ;w.args[3]=SYS_READ;w.descriptor_vector=0x1ff0;w.descriptor_count=1;
    assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_NOTSUP && !p->main_signal_state.io_top && p->main_signal_state.io_sequence==seq);
    const enum process_file_type denied[]={PROCESS_FILE_VFS,PROCESS_FILE_PTY_MASTER,PROCESS_FILE_PIPE_DUPLEX};
    for(unsigned i=0;i<sizeof(denied)/sizeof(denied[0]);i++) {
        p->files[3].type=denied[i];assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_NOTSUP && !p->main_signal_state.io_top && p->main_signal_state.io_sequence==seq);
    }
    pipes[0]=(struct process_pipe){.used=true,.refs=2,.read_refs=1,.write_refs=1};pipes[0].rights[0].used=true;
    p->files[3].type=PROCESS_FILE_PIPE_READ;p->files[3].handle=1;
    assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_NOTSUP && !p->main_signal_state.io_top);
    pipes[0].rights[0].used=false;
    const uint64_t bad_primary[]={SYS_PREAD,SYS_FCNTL,SYS_PIPE_MESSAGE};
    for(unsigned i=0;i<sizeof(bad_primary)/sizeof(bad_primary[0]);i++) {
        struct srv_signal_io_v1 bad=io_wire(SRV_SIGNAL_IO_BEGIN);bad.args[0]=0x2800;bad.args[2]=SRV_SIGNAL_ROOT_RAW;bad.args[3]=bad_primary[i];
        assert(io_request(&f,&bad,0,0)==SRV_ERR_NOTSUP && !p->main_signal_state.io_top);
    }
    p->files[3].type=PROCESS_FILE_NET_CONNECTION;
    w=io_wire(SRV_SIGNAL_IO_BEGIN);w.args[0]=0x2800;w.args[2]=SRV_SIGNAL_ROOT_EPOLL;w.args[3]=SYS_POLL;
    int64_t root=io_request(&f,&w,0,0);assert(root>0);
    w=io_wire(SRV_SIGNAL_IO_CAPTURE_DESCRIPTORS);w.ticket=root;w.descriptor_vector=0x1ff0;w.descriptor_count=1;
    assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_NOTSUP && p->main_signal_state.io_top==(uint64_t)root);
    for(unsigned i=0;i<sizeof(denied)/sizeof(denied[0]);i++) {
        p->files[3].type=denied[i];assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_NOTSUP && p->main_signal_state.io_top==(uint64_t)root);
    }
    p->files[3].type=PROCESS_FILE_PIPE_READ;pipes[0].rights[0].used=true;
    assert(io_request(&f,&w,0x1ff0,16)==SRV_ERR_NOTSUP && p->main_signal_state.io_top==(uint64_t)root);
    pipes[0].rights[0].used=false;
    w=io_wire(SRV_SIGNAL_IO_END);w.ticket=root;assert(io_request(&f,&w,0,0)==0);
    struct signal_state_identity id={1,1,7};uint64_t network_root=0;
    assert(signal_io_begin(&p->main_signal_state,id,0,SRV_SIGNAL_ROOT_RAW,SYS_NET_CONNECT,0,0x2800,0,0,0,0,&network_root));
    struct srv_signal_io_v1 call=io_wire(SRV_SIGNAL_IO_CALL);call.ticket=network_root;call.raw_syscall=SYS_NET_CONNECT;
    struct signal_io_slot out;assert(process_signal_io_check(&f,&call,&out)==SRV_ERR_NOTSUP);
}
#endif
int main(void) {
    struct process *p=&processes[0];init_case(p,false);struct isr_frame frame=make_frame();
    struct srv_signal_control_v1 w=control(SRV_SIGNAL_CONTROL_FEATURES);
    stage_control(&frame,&w,0x1ff0,64);finish(&frame,0,0);
    struct srv_signal_features_v1 feat;memcpy(&feat,destination_bytes+4080,sizeof feat);
    assert(feat.size==64 && feat.version==1);
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    assert(feat.capabilities==1 && feat.info_signals==(1ull<<SRV_SIGNAL_USR1) && feat.context_bytes==688);
#else
    assert(!feat.capabilities);
#endif
    w=control(SRV_SIGNAL_CONTROL_QUERY_ACTION);w.signum=SRV_SIGNAL_USR1;
    stage_control(&frame,&w,0x1ff0,24);finish(&frame,0,0);
    struct srv_signal_action_v1 old;memcpy(&old,destination_bytes+4080,sizeof old);
    assert(old.handler==0x400000 && old.mask==(1ull<<12) && (old.flags&SRV_SIGNAL_NATIVE_INFO));
    w=control(SRV_SIGNAL_CONTROL_EXCHANGE_ACTION);w.signum=SRV_SIGNAL_USR1;w.action.handler=1;
    stage_control(&frame,&w,0x1ff0,24);copy_ok=false;struct signal_action_descriptor action=p->usr1_action;
    memcpy(bytes_before,destination_bytes,sizeof bytes_before);finish(&frame,SRV_ERR_FAULT,0);copy_ok=true;
    assert(!memcmp(&action,&p->usr1_action,sizeof action) && !memcmp(bytes_before,destination_bytes,sizeof bytes_before));
    stage_control(&frame,&w,0x1ff0,24);finish(&frame,0,0);assert(p->usr1_action.mode==SIGNAL_ACTION_IGNORE);
    w.action.handler=0x400000;w.action.flags=SRV_SIGNAL_NATIVE_INFO|SRV_SIGNAL_NATIVE_RESTART;w.trampoline=0x500000;
    stage_control(&frame,&w,0,0);finish(&frame,SRV_ERR_NOTSUP,0);assert(p->usr1_action.mode==SIGNAL_ACTION_IGNORE);
    p->usr1_action=(struct signal_action_descriptor){.mode=SIGNAL_ACTION_LEGACY};
    w=control(SRV_SIGNAL_CONTROL_QUERY_ACTION);w.signum=SRV_SIGNAL_USR1;
    stage_control(&frame,&w,0x1ff0,24);finish(&frame,SRV_ERR_NOTSUP,0);
    w=control(SRV_SIGNAL_CONTROL_FEATURES);w.reserved[3]=1;
    stage_control(&frame,&w,0x1ff0,64);finish(&frame,SRV_ERR_INVAL,0);
    init_case(p,false);frame=make_frame();w=control(SRV_SIGNAL_CONTROL_EXCHANGE_ACTION);w.signum=SRV_SIGNAL_USR1;
    p->user_threads[1]=(struct process_user_thread){.used=true,.active=true,.tid=3,.owner=p,.pending_signals=1ull<<SRV_SIGNAL_USR1};
    stage_control(&frame,&w,0,0);finish(&frame,0,138);assert(p->killed && p->kill_signal==SRV_SIGNAL_USR1);
    init_case(p,false);frame=make_frame();p->main_signal_mask=1ull<<SRV_SIGNAL_USR2;p->main_pending_signals=1ull<<SRV_SIGNAL_USR2;
    w=control(SRV_SIGNAL_CONTROL_MASK);w.mask_how=SRV_SIGNAL_UNBLOCK;w.mask=1ull<<SRV_SIGNAL_USR2;
    stage_control(&frame,&w,0x1ff0,8);finish(&frame,0,140);assert(p->killed && p->kill_signal==SRV_SIGNAL_USR2);
    init_case(p,false);frame=make_frame();w=control(SRV_SIGNAL_CONTROL_FEATURES);uint8_t bytes[128]={0};memcpy(bytes,&w,sizeof w);
    assert(process_signal_public_stage(&frame,SIGNAL_REQUEST_CONTROL,w.operation,bytes,0x1ff0,64)==0);
    struct isr_frame wrong=frame;assert(process_signal_public_complete(&wrong)==UINT64_MAX);
    assert(p->main_signal_state.request.sequence);assert(process_signal_public_complete(&frame)==0);post_store=false;
    assert(process_signal_public_complete(&frame)==0);
    p->main_signal_state.request_sequence=INT64_MAX;
    assert(process_signal_public_stage(&frame,SIGNAL_REQUEST_CONTROL,w.operation,bytes,0x1ff0,64)==SRV_ERR_OVERFLOW);
    /* Request is normalized at stage even when caller later overwrites its
     * input, including when the modeled output names the same user address. */
    init_case(p,false);frame=make_frame();w=control(SRV_SIGNAL_CONTROL_EXCHANGE_ACTION);
    w.signum=SRV_SIGNAL_USR1;w.action.handler=1;memcpy(destination_bytes+4080,&w,sizeof w);
    memset(bytes,0,sizeof bytes);memcpy(bytes,destination_bytes+4080,sizeof w);
    assert(process_signal_public_stage(&frame,SIGNAL_REQUEST_CONTROL,w.operation,bytes,0x1ff0,24)==0);
    memset(bytes,0xff,sizeof bytes);memset(destination_bytes+4080,0xee,sizeof w);
    finish(&frame,0,0);assert(p->usr1_action.mode==SIGNAL_ACTION_IGNORE);
    memcpy(&old,destination_bytes+4080,sizeof old);assert(old.handler==0x400000);
    init_case(p,true);frame=make_frame();w=control(SRV_SIGNAL_CONTROL_FEATURES);
    stage_control(&frame,&w,0x1ff0,64);finish(&frame,0,0);
    uint8_t empty[128]={0};
    assert(process_signal_public_stage(&frame,SIGNAL_REQUEST_SUSPEND,1,empty,0,0)==0);finish(&frame,SRV_ERR_INVAL,0);
    assert(process_signal_public_stage(&frame,SIGNAL_REQUEST_IO,1,empty,0,0)==0);finish(&frame,SRV_ERR_INVAL,0);
    init_case(p,false);frame=make_frame();w=control(SRV_SIGNAL_CONTROL_FEATURES);
    stage_control(&frame,&w,0x1ff0,64);uint64_t staged=p->main_signal_state.request.sequence;
    observation.thread_identity=8;assert(process_signal_public_complete(&frame)==UINT64_MAX);
    assert(p->main_signal_state.request.sequence==staged);observation.thread_identity=7;
    finish(&frame,0,0);
    init_case(p,false);frame=make_frame();assert(process_signal_deliver_private(&frame)==PROCESS_SIGNAL_DELIVERED);
    uint64_t address=frame.rsp;post_store=consumed=false;memset(bytes,0,sizeof bytes);uint64_t args[2]={1,address};memcpy(bytes,args,sizeof args);
    assert(process_signal_public_stage(&frame,SIGNAL_REQUEST_RETURN,1,bytes,0,0)==0);
    assert(process_signal_public_complete(&frame)==PROCESS_SIGNAL_RETURN_RESTORED && consumed);post_store=false;
    init_case(p,false);frame=make_frame();memset(bytes,0,sizeof bytes);args[0]=4;args[1]=0x2000;memcpy(bytes,args,sizeof args);
    assert(process_signal_public_stage(&frame,SIGNAL_REQUEST_RETURN,1,bytes,0,0)==0);
    assert(process_signal_public_complete(&frame)==UINT64_MAX);
    suspend_cases(false);suspend_cases(true);nested_constraints();io_cases();io_mask_unwind_cases();take_cases();capture_descriptor_cases();
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    experimental_policy_cases();
#endif
    puts("process-signal-public: staged host model PASS");return 0;
}
