/* Actual process ownership/actions; paging and scheduler remain explicit mocks. */
#define main frame_copy_fixture_main
#include "test_process_user_frame_copy.c"
#undef main

static unsigned exec_checks;
static uint64_t deny_exec;
void *scheduler_current_user_context(void) {return observation.process;}
void *scheduler_current_user_thread_context(void) {return observation.user_thread;}
bool vmm_user_exec_current(uint64_t address,uint64_t root) {
    assert(!(test_flags&0x200) && root==0x2000);exec_checks++;
    return address!=deny_exec;
}
static struct signal_context_record record(uint64_t token,uint64_t frame) {
    struct signal_context_record r={0};
    r.frame=(struct isr_frame){.cs=0x23,.ss=0x1b,.rip=0x500000,.rsp=0x900000,.rflags=0x202};
    r.image_generation=1;r.thread_generation=7;r.token=token;r.frame_identity=frame;
    return r;
}
static void refused_action(struct process *p,const struct signal_action_descriptor *next) {
    memcpy(&original,p,sizeof(original));
    struct signal_action_descriptor old;memset(&old,0xa5,sizeof(old));
    struct signal_action_descriptor output=old;uint64_t generation=123,flags=test_flags;
    assert(!process_signal_action_exchange(next,&output,&generation));
    assert(!memcmp(&original,p,sizeof(original)) && !memcmp(&old,&output,sizeof(old)));
    assert(generation==123 && flags==test_flags);
}
int main(void) {
    struct process *p=&processes[0];setup(p);test_flags=0x200;
    struct signal_action_descriptor info={SIGNAL_ACTION_INFO,SIGNAL_ACTION_RESTART,0x500000,0x500100,1ull<<12};
    struct signal_action_descriptor old={0};uint64_t generation=99;
    assert(process_signal_action_exchange(&info,&old,&generation));
    assert(!old.mode && generation==1 && p->signal_catch_mask==(1ull<<10) && !p->signal_ignore_mask && test_flags==0x200);
    assert(exec_checks==2 && !memcmp(&p->usr1_action,&info,sizeof(info)));
    unsigned before=exec_checks;
    struct signal_action_descriptor queried;
    assert(process_signal_action_exchange(NULL,&queried,&generation) && generation==1 && exec_checks==before);
    assert(!memcmp(&queried,&info,sizeof(info)));
    struct signal_action_descriptor bad=info;bad.flags=4;refused_action(p,&bad);
    bad=info;bad.mask|=1;refused_action(p,&bad);
    bad=info;bad.handler=1;refused_action(p,&bad);
    deny_exec=info.handler;refused_action(p,&info);deny_exec=info.trampoline;refused_action(p,&info);deny_exec=0;
    p->killed=true;refused_action(p,&info);p->killed=false;
    p->exiting=true;refused_action(p,&info);p->exiting=false;
    p->main_thread_exited=true;refused_action(p,&info);p->main_thread_exited=false;
    void *saved=observation.process;observation.process=(void *)1;refused_action(p,&info);observation.process=saved;
    observation.image_generation=2;refused_action(p,&info);observation.image_generation=1;
    observation.user_thread=(void *)1;refused_action(p,&info);observation.user_thread=NULL;

    /* Cooperative poll/consume never steal the separately owned INFO bit. */
    p->pending_signals=p->main_pending_signals=1ull<<10;
    memcpy(&original,p,sizeof(original));assert(!process_signal_poll_current());
    assert(!process_signal_consume_current(UINT64_MAX) && !memcmp(p,&original,sizeof(original)));
    p->pending_signals|=1ull<<12;assert(process_signal_poll_current()==12);
    assert(p->pending_signals==(1ull<<10) && p->main_pending_signals==(1ull<<10));
    assert(process_signal_config_current(10,SRV_SIGNAL_CATCH));
    assert(p->usr1_action.mode==SIGNAL_ACTION_LEGACY && !p->usr1_action.handler && !p->usr1_action.trampoline && !p->usr1_action.flags && !p->usr1_action.mask);
    assert(process_signal_consume_current(1ull<<10)==10 && p->pending_signals==(1ull<<10) && !p->main_pending_signals);
    assert(process_signal_poll_current()==10 && !p->pending_signals);
    assert(process_signal_action_exchange(&info,NULL,NULL));
    p->pending_signals=p->main_pending_signals=(1ull<<10)|(1ull<<12);
    p->user_threads[0].pending_signals=(1ull<<10)|(1ull<<12);
    struct signal_action_descriptor ignore={.mode=SIGNAL_ACTION_IGNORE};
    assert(process_signal_action_exchange(&ignore,&ignore,&generation)); /* aliased old output */
    assert(ignore.mode==SIGNAL_ACTION_INFO && p->usr1_action.mode==SIGNAL_ACTION_IGNORE);
    assert(!p->signal_catch_mask && p->signal_ignore_mask==(1ull<<10));
    assert(p->pending_signals==(1ull<<12) && p->main_pending_signals==(1ull<<12) && p->user_threads[0].pending_signals==(1ull<<12));
    assert(process_signal_config_current(10,SRV_SIGNAL_DEFAULT) && !p->signal_ignore_mask && !p->usr1_action.mode);
    p->usr1_action_generation=UINT64_MAX;refused_action(p,&info);
    memcpy(&original,p,sizeof(original));assert(!process_signal_config_current(10,SRV_SIGNAL_CATCH));
    assert(!memcmp(p,&original,sizeof(original)) && test_flags==0x200);

    setup(p);test_flags=0x200;
    for(unsigned i=0;i<4;i++) {
        struct signal_context_record r=record(i+1,0x800000+i*1024);
        assert(process_signal_record_push(&r,(struct signal_context_range){r.frame_identity,r.frame_identity+768}));
    }
    assert(p->main_signal_state.identity.public_tid==1 && p->main_signal_state.stack.depth==4);
    memcpy(&original,p,sizeof(original));struct signal_context_record extra=record(5,0x810000);
    assert(!process_signal_record_push(&extra,(struct signal_context_range){0x810000,0x810300}) && !memcmp(p,&original,sizeof(original)));
    struct signal_context_record out;memset(&out,0xa5,sizeof(out));
    struct signal_context_record out_before=out;struct signal_context_range range={123,456};
    assert(!process_signal_record_pop(1,0x800000,&out,&range));
    assert(!memcmp(p,&original,sizeof(original)) && !memcmp(&out,&out_before,sizeof(out)) && range.start==123 && range.end==456);
    for(unsigned i=4;i;i--) assert(process_signal_record_pop(i,0x800000+(i-1)*1024,&out,&range));
    assert(!p->main_signal_state.stack.depth && p->main_signal_state.stack.last_token==4);
    struct process_user_thread *w=&p->user_threads[0];
    *w=(struct process_user_thread){.used=true,.active=true,.owner=p,.tid=2};
    observation.user_thread=w;observation.user_fpu=&w->fpu;observation.kernel_stack_top=0;
    extra=record(1,0x820000);
    assert(process_signal_record_push(&extra,(struct signal_context_range){0x820000,0x820300}));
    assert(w->signal_state.identity.public_tid==2 && w->signal_state.stack.depth==1);
    memcpy(&original,p,sizeof(original));w->tid=3;struct signal_thread_state saved_state=w->signal_state;
    assert(!process_signal_record_pop(1,0x820000,&out,&range) && !memcmp(&w->signal_state,&saved_state,sizeof(saved_state)));
    w->signal_state=(struct signal_thread_state){0}; /* Constructor retirement; actual paths tested separately. */
    assert(process_signal_record_push(&extra,(struct signal_context_range){0x820000,0x820300}));
    assert(w->signal_state.identity.public_tid==3);
    process_signal_retire_image(p);
    assert(!process_signal_records_active(p) && !p->usr1_action_generation && test_flags==0x200);
    printf("process-signal-state: action/identity/rollback/INFO isolation pass; state=%zu process=%zu static_extra=%zu\n",
        sizeof(struct signal_thread_state),sizeof(struct process),
        (sizeof(struct signal_thread_state)*(PROCESS_MAX_USER_THREADS+1)+sizeof(struct signal_action_descriptor)+sizeof(uint64_t))*PROCESS_MAX_PROCESSES);
    return 0;
}
