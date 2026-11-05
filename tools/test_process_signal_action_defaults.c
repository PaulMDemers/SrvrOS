/* Actual process action changes; scheduler and paging are host models. */
#define main signal_state_fixture_unused_main
#include "test_process_user_frame_copy.c"
#undef main

void *scheduler_current_user_context(void) {return observation.process;}
void *scheduler_current_user_thread_context(void) {return observation.user_thread;}
bool vmm_user_exec_current(uint64_t address,uint64_t root) {(void)address;assert(!(test_flags&0x200));return root==0x2000;}
void net_process_wake(struct process *p) {(void)p;}
void scheduler_wake_all(struct scheduler_wait_queue *q) {(void)q;}
void keyboard_wake_waiters(void) {}
void pty_wake(void) {}

static struct process *subject(uint64_t signal) {
    struct process *p=&processes[0];setup(p);test_flags=0x200;
    p->main_signal_mask=1ull<<signal;
    p->user_threads[0]=(struct process_user_thread){.used=true,.active=true,.owner=p,.tid=2};
    assert(process_signal_config_current(signal,SRV_SIGNAL_CATCH));
    return p;
}
static void default_action(uint64_t signal) {
    uint64_t flags=test_flags;
    assert(process_signal_config_current(signal,SRV_SIGNAL_DEFAULT));
    assert(flags==test_flags);
}
static void fatal_matrix(void) {
    for(unsigned sig=SRV_SIGNAL_USR1;sig<=SRV_SIGNAL_USR2;sig+=2)for(unsigned shared=0;shared<2;shared++) {
        struct process *p=subject(sig);uint64_t bit=1ull<<sig;
        if(shared)p->pending_signals=bit;else p->user_threads[0].pending_signals=bit;
        default_action(sig);
        assert(p->killed && p->kill_signal==sig);
        assert(!(shared?p->pending_signals:p->user_threads[0].pending_signals));
    }
}
int main(void) {
    fatal_matrix();
    for(unsigned sig=SRV_SIGNAL_USR1;sig<=SRV_SIGNAL_USR2;sig+=2) {
        uint64_t bit=1ull<<sig;struct process *p=subject(sig);
        p->user_threads[0].pending_signals=bit;p->pending_signals=bit;
        default_action(sig);assert(p->killed && p->kill_signal==sig);
        assert(!p->user_threads[0].pending_signals && p->pending_signals==bit);

        p=subject(sig);p->main_signal_mask=0;p->main_pending_signals=bit;p->pending_signals=bit;
        default_action(sig);assert(p->killed && p->kill_signal==sig && !p->main_pending_signals && p->pending_signals==bit);

        p=subject(sig);p->user_threads[0].tid=4;p->user_threads[0].pending_signals=bit;
        p->user_threads[1]=(struct process_user_thread){.used=true,.active=true,.owner=p,.tid=2,.pending_signals=bit};
        default_action(sig);assert(p->killed && p->kill_signal==sig && p->user_threads[0].pending_signals==bit && !p->user_threads[1].pending_signals);

        p=subject(sig);p->user_threads[0].signal_mask=bit;
        p->user_threads[0].pending_signals=bit;
        p->user_threads[1]=(struct process_user_thread){.used=true,.active=true,.owner=p,.tid=3};
        p->pending_signals=bit;default_action(sig);
        assert(p->killed && p->kill_signal==sig && p->user_threads[0].pending_signals==bit && !p->pending_signals);

        p=subject(sig);p->user_threads[0].signal_mask=bit;
        p->user_threads[0].pending_signals=p->pending_signals=bit;
        default_action(sig);assert(!p->killed && p->pending_signals==bit && p->user_threads[0].pending_signals==bit);

        p=subject(sig);p->main_thread_exited=true;p->main_pending_signals=bit;
        p->user_threads[0].active=false;p->user_threads[0].pending_signals=bit;
        p->user_threads[1]=(struct process_user_thread){.used=false,.active=true,.owner=p,.tid=3,.pending_signals=bit};
        p->user_threads[2]=(struct process_user_thread){.used=true,.active=true,.owner=&processes[1],.tid=4,.pending_signals=bit};
        default_action(sig);assert(!p->killed && p->main_pending_signals==bit &&
            p->user_threads[0].pending_signals==bit && p->user_threads[1].pending_signals==bit && p->user_threads[2].pending_signals==bit);

        p=subject(sig);p->killed=true;p->kill_signal=SRV_SIGNAL_TERM;
        p->pending_signals=p->user_threads[0].pending_signals=bit;
        default_action(sig);assert(p->killed && p->kill_signal==SRV_SIGNAL_TERM &&
            p->pending_signals==bit && p->user_threads[0].pending_signals==bit);

        p=subject(sig);p->pending_signals=p->main_pending_signals=bit;
        for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)p->user_threads[i].pending_signals=bit;
        assert(process_signal_config_current(sig,SRV_SIGNAL_IGNORE));
        assert(!p->pending_signals && !p->main_pending_signals);
        for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)assert(!p->user_threads[i].pending_signals);
    }
    for(unsigned i=0;i<2;i++) {
        uint64_t sig=i?SRV_SIGNAL_WINCH:SRV_SIGNAL_CHLD,bit=1ull<<sig;
        struct process *ignored=subject(sig);ignored->main_pending_signals=ignored->pending_signals=bit;
        ignored->user_threads[0].pending_signals=bit;
        assert(process_signal_config_current(sig,SRV_SIGNAL_DEFAULT));
        assert(!ignored->killed && !ignored->main_pending_signals && !ignored->pending_signals && !ignored->user_threads[0].pending_signals);
    }
    struct process *p=subject(SRV_SIGNAL_USR1);
    struct signal_action_descriptor info={.mode=SIGNAL_ACTION_INFO,.handler=0x500000,.trampoline=0x500100};
    assert(process_signal_action_exchange(&info,NULL,NULL));
    p->user_threads[0].pending_signals=1ull<<SRV_SIGNAL_USR1;
    struct signal_action_descriptor def={.mode=SIGNAL_ACTION_DEFAULT};
    assert(process_signal_action_exchange(&def,NULL,NULL));
    assert(p->killed && p->kill_signal==SRV_SIGNAL_USR1 && !p->user_threads[0].pending_signals);
    p=subject(SRV_SIGNAL_USR1);
    p->usr1_action_generation=UINT64_MAX;
    p->user_threads[0].pending_signals=p->pending_signals=1ull<<SRV_SIGNAL_USR1;
    memcpy(&original,p,sizeof(original));
    assert(!process_signal_config_current(SRV_SIGNAL_USR1,SRV_SIGNAL_DEFAULT));
    assert(!memcmp(p,&original,sizeof(original)) && test_flags==0x200);
    assert(!process_signal_config_current(63,SRV_SIGNAL_DEFAULT));
    assert(!memcmp(p,&original,sizeof(original)) && test_flags==0x200);
    puts("process-signal-defaults: peer-local/shared eligibility and ignore pass");return 0;
}
