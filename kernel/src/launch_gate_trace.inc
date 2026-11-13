/* Fixed observations; no allocation, callbacks, user pointers, waits or grants.
 * Existing gate/owner capacities and their selection remain unchanged. */
_Static_assert(LAUNCH_TRACE_REASON_COUNT==13,"update versioned launch reason wire together");
_Static_assert(SRV_F_GETFD==1 && SRV_F_SETFD==2 && SRV_F_GETFL==3 && SRV_F_SETFL==4 &&
    SRV_F_GETLK==5 && SRV_F_SETLK==6 && SRV_F_SETLKW==7 && SRV_F_DUPFD==8 &&
    SRV_F_DUPFD_CLOEXEC==9 && SRV_F_RESERVE_APPEND_RAM==10,"update versioned fcntl wire together");
static bool launch_trace_configured;
struct launch_gate_worst {struct io_gate_sample gate;struct launch_gate_context context;};
static struct {
    uint64_t epoch,entries,decisions,fcntl_calls,invalid,reasons[LAUNCH_TRACE_REASON_COUNT],waits[2];
    struct launch_gate_worst worst[2];
} launch_trace;
static void launch_trace_invalid(void) {if(launch_trace.invalid!=UINT64_MAX)launch_trace.invalid++;}
static void launch_trace_count(uint64_t *n) {if(*n==UINT64_MAX)launch_trace_invalid();else (*n)++;}
static void launch_trace_reset(void) {
    uint64_t previous=launch_trace.epoch;
    launch_trace=(__typeof__(launch_trace)){.epoch=previous==UINT64_MAX?UINT64_MAX:previous+1,.invalid=previous==UINT64_MAX};
}
bool scheduler_launch_trace_enable(void) {
    uint64_t flags=irq_save();bool ok=!launch_trace_configured && !switching && !io_depth && timer_reference_ready();
    if(ok)launch_trace_configured=true;
    irq_restore(flags);return ok;
}
bool scheduler_launch_trace_enabled(void) {return launch_trace_configured && initialized && io_observed.enabled;}
static struct thread *launch_trace_current(uint64_t syscall) {
    if(!scheduler_launch_trace_enabled())return NULL;
    if(switching || current_thread>=thread_capacity) {launch_trace_invalid();return NULL;}
    struct thread *t=thread_at(current_thread);
    if(t->io_syscall!=syscall)return NULL; /* Typed inner calls are not raw entries. */
    if(t->state!=THREAD_RUNNING) {launch_trace_invalid();return NULL;}
    return t;
}
void scheduler_launch_trace_entry(void) {
    uint64_t flags=irq_save();struct thread *t=launch_trace_current(SYS_EXEC);
    if(t) {
        launch_trace_count(&launch_trace.entries);
        t->launch_context=(struct launch_gate_context){.kind=1,.epoch=launch_trace.epoch};
    }
    irq_restore(flags);
}
void scheduler_launch_trace_exec(uint64_t reason,uint64_t request_mask,uint64_t mappings,uint64_t atomic,uint64_t detail) {
    uint64_t flags=irq_save();struct thread *t=launch_trace_current(SYS_EXEC);
    if(t) {
        if(!reason || reason>=LAUNCH_TRACE_REASON_COUNT)launch_trace_invalid();
        else {
            launch_trace_count(&launch_trace.decisions);launch_trace_count(&launch_trace.reasons[reason]);
            t->launch_context=(struct launch_gate_context){.kind=1,.epoch=launch_trace.epoch,
                .exec={reason,request_mask,mappings,atomic,detail,0}};
        }
    }
    irq_restore(flags);
}
void scheduler_launch_trace_fcntl(uint64_t fd,uint64_t command,uint64_t arg,uint64_t type,uint64_t file_flags) {
    uint64_t flags=irq_save();struct thread *t=launch_trace_current(SYS_FCNTL);
    if(t) {
        bool scalar=command==SRV_F_SETFD || command==SRV_F_SETFL || command==SRV_F_DUPFD ||
            command==SRV_F_DUPFD_CLOEXEC || command==SRV_F_RESERVE_APPEND_RAM;
        launch_trace_count(&launch_trace.fcntl_calls);
        t->launch_context=(struct launch_gate_context){.kind=2,.epoch=launch_trace.epoch,
            .fcntl={fd,command,scalar,scalar?arg:0,type,file_flags}};
    }
    irq_restore(flags);
}
static void launch_trace_gate(struct io_gate_sample gate,struct launch_gate_context context) {
    if(!scheduler_launch_trace_enabled() || (gate.syscall!=SYS_EXEC && gate.syscall!=SYS_FCNTL))return;
    unsigned index=gate.syscall==SYS_FCNTL;
    if(gate.end<gate.start || context.kind!=index+1 || context.epoch!=launch_trace.epoch) {launch_trace_invalid();return;}
    if(gate.end-gate.start<IO_OBSERVE_MIN_US)return;
    launch_trace_count(&launch_trace.waits[index]);
    struct launch_gate_worst *w=&launch_trace.worst[index];
    if(!w->gate.end || gate.end-gate.start>w->gate.end-w->gate.start)*w=(struct launch_gate_worst){gate,context};
}
void scheduler_print_launch_trace(void) {
    uint64_t flags=irq_save();
    if(!scheduler_launch_trace_enabled() || io_depth) {irq_restore(flags);return;}
    uint64_t at=timer_monotonic_us();__typeof__(launch_trace) snapshot=launch_trace;
    irq_restore(flags);
    const uint64_t *c=snapshot.reasons;
    console_printf("launch-gate-summary: version=1 snapshot_us=%u epoch=%u entries=%u decisions=%u fcntl_calls=%u invalid=%u exec_waits=%u fcntl_waits=%u min_us=%u request=%u parent=%u mappings=%u path=%u atomic=%u stdio=%u resolve=%u source=%u elf=%u image_bound=%u handled=%u error=%u final=1\n",
        at,snapshot.epoch,snapshot.entries,snapshot.decisions,snapshot.fcntl_calls,snapshot.invalid,snapshot.waits[0],snapshot.waits[1],(uint64_t)IO_OBSERVE_MIN_US,
        c[1],c[2],c[3],c[4],c[5],c[6],c[7],c[8],c[9],c[10],c[11],c[12]);
    for(unsigned i=0;i<2;i++) {
        const struct launch_gate_worst *w=&snapshot.worst[i];const struct io_gate_sample *g=&w->gate;
        const struct launch_gate_context *x=&w->context;if(!g->end)continue;
        if(!i)console_printf("launch-gate-exec: version=1 snapshot_us=%u epoch=%u pid=%u thread=%u owner_pid=%u owner_thread=%u owner_seq=%u start_us=%u end_us=%u elapsed_us=%u reason=%u request_mask=%u mappings=%u atomic=%u detail=%u final=1\n",
            at,x->epoch,g->pid,g->identity,g->owner_pid,g->owner_identity,g->owner_seq,g->start,g->end,g->end-g->start,
            x->exec.reason,x->exec.request_mask,x->exec.mappings,x->exec.atomic,x->exec.detail);
        else console_printf("launch-gate-fcntl: version=1 snapshot_us=%u epoch=%u pid=%u thread=%u owner_pid=%u owner_thread=%u owner_seq=%u start_us=%u end_us=%u elapsed_us=%u fd=%u command=%u scalar=%u arg=%u type=%u flags=%u final=1\n",
            at,x->epoch,g->pid,g->identity,g->owner_pid,g->owner_identity,g->owner_seq,g->start,g->end,g->end-g->start,
            x->fcntl.fd,x->fcntl.command,x->fcntl.scalar,x->fcntl.arg,x->fcntl.type,x->fcntl.flags);
    }
}
