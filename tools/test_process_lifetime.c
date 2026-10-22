/* Exercise the real allocator/release paths, including preemption windows. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_PROCESS_HOST_TEST
#include "../kernel/src/process.c"
static struct process *subject;
static uint64_t tested;
static unsigned hook;
void *scheduler_current_user_context(void) { return subject; }
void *scheduler_current_user_thread_context(void) { return NULL; }
void fpu_init_state(struct fpu_state *p) { memset(p,0,sizeof(*p)); }
static uint64_t members(uint64_t token) {
    struct process *saved=subject;subject=NULL;
    struct process_lifetime_status s;assert(process_lifetime_status(token,&s));
    subject=saved;return s.members;
}
uint64_t gdt_default_kernel_stack_top(void) {
    if(hook==1) {
        hook=0;assert(members(tested)==1);
        assert(process_lifetime_revoke(tested));
        assert(!process_lifetime_release(tested));
        assert(process_lifetime_drain(tested));
        assert(members(tested)==1); /* Reserved but not published. */
    }
    return 0;
}
void pmm_free_frame_range(uint64_t address,uint64_t count) {
    assert(address==0x1000 && count==1 && hook==2);hook=0;
    assert(members(tested)==1 && !process_lifetime_release(tested));
    assert(process_lifetime_drain(tested));
    assert(members(tested)==1); /* Unpublished but resources still owned. */
}
void net_process_wake(struct process *p) { (void)p; }
void keyboard_wake_waiters(void) {}
void pty_wake(void) {}
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q; }
int main(void) {
    tested=process_lifetime_create();assert(tested);
    assert(!process_lifetime_release(tested) && !process_lifetime_drain(tested));
    struct process *root=alloc_process_lifetime("root",true,tested);assert(root);
    assert(!alloc_process_lifetime("second-root",true,tested));
    subject=root;
    assert(!process_lifetime_create() && !process_lifetime_revoke(tested));
    assert(!process_lifetime_release(tested) && !process_lifetime_drain(tested));
    assert(!alloc_process_lifetime("override",true,tested));
    /* Far more than the live slot bound, with no historical membership list. */
    for(unsigned i=0;i<512;i++) {
        struct process *p=alloc_process("app",true);assert(p && p->lifetime==tested);
        assert(members(tested)==2);release_process(p);assert(members(tested)==1);
    }
    struct process *orphan=alloc_process("job",true);assert(orphan);
    assert(process_set_group(orphan->pid,orphan->pid));
    subject=orphan;assert(process_set_group(orphan->pid,root->pid));
    uint64_t session;assert(process_create_session_current(&session));
    assert(session==orphan->pid && orphan->lifetime==tested);
    subject=NULL;release_process(root);assert(members(tested)==1);
    subject=orphan;
    struct process *leaf=alloc_process("leaf-after-parent-reaped",true);assert(leaf && leaf->lifetime==tested);
    subject=NULL;assert(process_lifetime_revoke(tested));
    subject=orphan;assert(!alloc_process("late",true));subject=NULL;
    orphan->signal_ignore_mask=orphan->main_signal_mask=~0ull;
    assert(process_lifetime_drain(tested));
    assert(orphan->killed && leaf->killed && !process_lifetime_release(tested));
    orphan->active=false;orphan->reapable=true;leaf->active=false;leaf->reapable=true;
    assert(process_lifetime_drain(tested) && members(tested)==0);
    assert(process_lifetime_release(tested));
    uint64_t old=tested;tested=process_lifetime_create();assert(tested && tested!=old);
    assert(!process_lifetime_revoke(old) && !alloc_process_lifetime("stale",true,old));
    hook=1;root=alloc_process_lifetime("revoked-during-prepare",true,tested);assert(root && !hook);
    root->kernel_stack_physical=0x1000;root->kernel_stack_frames=1;hook=2;
    release_process(root);assert(!hook && members(tested)==0 && process_lifetime_release(tested));
    /* Bounded owner table and fail-closed token exhaustion. */
    uint64_t tokens[PROCESS_MAX_PROCESSES];
    for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) { tokens[i]=process_lifetime_create();assert(tokens[i]); }
    assert(!process_lifetime_create());
    for(unsigned i=0;i<PROCESS_MAX_PROCESSES;i++) assert(process_lifetime_revoke(tokens[i]) && process_lifetime_release(tokens[i]));
    next_lifetime=UINT64_MAX;tested=process_lifetime_create();assert(tested==UINT64_MAX && !process_lifetime_create());
    assert(process_lifetime_revoke(tested) && process_lifetime_release(tested) && !process_lifetime_create());
    puts("process-lifetime: PASS churn, orphans, job control, admission, preparation/release races, stale tokens, bounds");
}
