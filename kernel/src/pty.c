#include <srvros/pty.h>
#include <stddef.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#define PTY_MAX 16
struct pty_slot { struct pty_model model;uint64_t generation,masters,slaves,master_flags,slave_flags; };
static struct pty_slot slots[PTY_MAX];
static uint64_t generation;
static struct scheduler_wait_queue waiters;
static uint64_t lock(void) {
#ifdef SRVROS_PTY_HOST_TEST
    return 0;
#else
    uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");return flags;
#endif
}
static void unlock(uint64_t flags) {
#ifdef SRVROS_PTY_HOST_TEST
    (void)flags;
#else
    if(flags&(1ull<<9)) __asm__ volatile("sti":::"memory");
#endif
}
static struct pty_slot *slot(uint64_t handle) {
    unsigned i=handle&255;
    if(!i || i>PTY_MAX) return NULL;
    struct pty_slot *p=&slots[i-1];
    return (p->masters || p->slaves) && p->generation==(handle>>8)?p:NULL;
}
struct pty_model *pty_state(uint64_t handle) { struct pty_slot *p=slot(handle);return p?&p->model:NULL; }
void pty_wake(void) { scheduler_wake_all(&waiters);process_file_poll_wake(); }
uint64_t pty_create(void) {
    uint64_t flags=lock(),id=0;
    for(unsigned i=0;i<PTY_MAX;i++) if(!slots[i].masters && !slots[i].slaves) {
        struct pty_slot *p=&slots[i];pty_model_init(&p->model);p->masters=p->slaves=1;
        p->master_flags=p->slave_flags=0;
        p->generation=++generation;if(!p->generation) p->generation=++generation;
        id=(p->generation<<8)|(i+1);break;
    }
    unlock(flags);return id;
}
bool pty_ref(uint64_t handle,bool master) {
    uint64_t flags=lock();struct pty_slot *p=slot(handle);bool ok=p && (master?p->masters:p->slaves);
    if(ok) { if(master) p->masters++;else p->slaves++; }
    unlock(flags);return ok;
}
int64_t pty_get_flags(uint64_t handle,bool master) {
    struct pty_slot *p=slot(handle);
    return p?(int64_t)(master?p->master_flags:p->slave_flags):SRV_ERR_BADF;
}
int64_t pty_set_flags(uint64_t handle,bool master,uint64_t status) {
    uint64_t flags=lock();struct pty_slot *p=slot(handle);
    if(p) { if(master) p->master_flags=status;else p->slave_flags=status; }
    unlock(flags);return p?0:SRV_ERR_BADF;
}
void pty_close(uint64_t handle,bool master) {
    uint64_t flags=lock();struct pty_slot *p=slot(handle);
    if(p) {
        if(master && p->masters && !--p->masters) { p->model.master_closed=true;process_tty_hangup(handle); }
        if(!master && p->slaves && !--p->slaves) p->model.slave_closed=true;
        pty_wake();
    }
    unlock(flags);
}
struct wait_request { uint64_t handle,length;bool master,write;uint8_t byte; };
static bool ready(void *argument) {
    struct wait_request *w=argument;struct pty_model *p=pty_state(w->handle);
    if(!p || process_should_exit_current() || process_signal_pending_current()) return true;
    if(w->master?p->slave_closed:p->master_closed) return true;
    if(!w->write) {
        if(w->master) return p->output.size>0;
        if(p->attributes.lflag&SRV_TTY_LFLAG_ICANON) return p->record_count>0;
        uint64_t minimum=p->attributes.cc[SRV_TTY_VMIN];if(minimum>w->length) minimum=w->length;
        return p->input.size>=minimum;
    }
    return pty_model_write_ready(p,w->master,w->byte);
}
int64_t pty_read(uint64_t handle,bool master,uint8_t *data,uint64_t length,bool nonblocking) {
    uint64_t flags=lock();int64_t result=-1;
    struct wait_request w={.handle=handle,.length=length,.master=master};
    for(;;) {
        struct pty_model *p=pty_state(handle);if(!p || process_should_exit_current()) break;
        result=pty_model_read(p,master,data,length,nonblocking);
        if(result!=SRV_ERR_AGAIN || nonblocking) break;
        if(process_signal_pending_current() || !scheduler_wait(&waiters,ready,&w)) { result=SRV_ERR_INTR;break; }
    }
    if(result>=0) pty_wake();
    unlock(flags);return result;
}
int64_t pty_write(uint64_t handle,bool master,const uint8_t *data,uint64_t length,bool nonblocking) {
    if(!data && length) return -1;
    uint64_t flags=lock(),done=0;int64_t error=-1;
    while(done<length) {
        struct pty_model *p=pty_state(handle);if(!p || process_should_exit_current()) break;
        int64_t result=pty_model_write(p,master,data+done,length-done);
        if(result>0) {
            done+=result;uint64_t signals=p->signals;p->signals=0;
            for(unsigned signal=1;signal<64;signal++) if(signals&(1ull<<signal)) process_tty_signal(handle,signal);
            pty_wake();continue;
        }
        error=result;if(result!=SRV_ERR_AGAIN || nonblocking) break;
        struct wait_request w={.handle=handle,.master=master,.write=true,.byte=data[done]};
        if(process_signal_pending_current() || !scheduler_wait(&waiters,ready,&w)) { error=SRV_ERR_INTR;break; }
    }
    unlock(flags);return done?(int64_t)done:!length?0:error;
}
/* Capture the descriptor, not just the shared PTY handle: close/dup2 may
 * replace it while a sibling is asleep, even with another reference alive. */
struct checked_wait {
    struct process *process;uint64_t fd,generation,handle,length;
    enum process_file_type type;bool master,write;uint8_t byte;
};
static bool checked_current(struct checked_wait *w) {
    struct process_file *f=process_file_at(w->process,w->fd);
    return f && w->generation!=UINT64_MAX && process_fd_generation(w->process,w->fd)==w->generation &&
        f->type==w->type && f->handle==w->handle && pty_state(w->handle);
}
static bool checked_ready(void *argument) {
    struct checked_wait *w=argument;
    if(!checked_current(w)) return true;
    struct wait_request r={.handle=w->handle,.length=w->length,.master=w->master,.write=w->write,.byte=w->byte};
    return ready(&r);
}
static bool checked_init(struct checked_wait *w,struct process *process,uint64_t fd) {
    struct process_file *f=process_file_at(process,fd);
    if(!f || (f->type!=PROCESS_FILE_PTY_MASTER && f->type!=PROCESS_FILE_PTY_SLAVE)) return false;
    *w=(struct checked_wait){.process=process,.fd=fd,.generation=process_fd_generation(process,fd),
        .handle=f->handle,.type=f->type,.master=f->type==PROCESS_FILE_PTY_MASTER};
    return checked_current(w);
}
int64_t process_file_pty_read_checked(struct process *process,uint64_t fd,uint64_t length,process_pipe_copyout copyout,void *context) {
    uint64_t flags=lock();struct checked_wait w;int64_t result=SRV_ERR_BADF;
    if(!copyout || !checked_init(&w,process,fd)) goto out;
    w.length=length;
    for(;;) {
        if(!checked_current(&w)) { result=SRV_ERR_BADF;break; }
        if(process_should_exit_current()) { result=-1;break; }
        bool nonblocking=process_file_nonblocking(process,fd);
        result=pty_model_read_checked(pty_state(w.handle),w.master,length,nonblocking,copyout,context);
        if(result!=SRV_ERR_AGAIN || nonblocking) break;
        if(process_signal_pending_current()) { result=SRV_ERR_INTR;break; }
        bool woke=scheduler_wait(&waiters,checked_ready,&w);
        if(!checked_current(&w)) { result=SRV_ERR_BADF;break; }
        if(!woke || process_signal_pending_current()) { result=SRV_ERR_INTR;break; }
    }
    if(result>=0) pty_wake();
out: unlock(flags);return result;
}
int64_t process_file_pty_write_checked(struct process *process,uint64_t fd,uint64_t length,process_pipe_copyin copyin,void *context) {
    uint64_t flags=lock(),done=0;struct checked_wait w;int64_t error=SRV_ERR_BADF;
    if(!copyin || !checked_init(&w,process,fd)) goto out;
    w.write=true;
    while(done<length) {
        if(!checked_current(&w)) { error=SRV_ERR_BADF;break; }
        if(process_should_exit_current()) { error=-1;break; }
        /* Copy at most one small bounded batch while mappings are stable.
         * If a batch crosses an inaccessible mapping, retain the accessible
         * prefix so the caller sees the same short-write behavior as before. */
        uint8_t bytes[256];
        uint64_t count=length-done;
        if(count>sizeof(bytes)) count=sizeof(bytes);
        if(!copyin(context,done,bytes,count,NULL,0)) {
            uint64_t prefix=0;
            while(prefix<count && copyin(context,done+prefix,bytes+prefix,1,NULL,0)) prefix++;
            if(!prefix) { error=SRV_ERR_FAULT;break; }
            count=prefix;
        }
        struct pty_model *p=pty_state(w.handle);
        int64_t result=pty_model_write(p,w.master,bytes,count);
        if(result>0) {
            done+=(uint64_t)result;uint64_t signals=p->signals;p->signals=0;
            for(unsigned signal=1;signal<64;signal++) if(signals&(1ull<<signal)) process_tty_signal(w.handle,signal);
            pty_wake();continue;
        }
        error=result;if(result!=SRV_ERR_AGAIN || process_file_nonblocking(process,fd)) break;
        w.byte=bytes[0];
        if(process_signal_pending_current()) { error=SRV_ERR_INTR;break; }
        bool woke=scheduler_wait(&waiters,checked_ready,&w);
        if(!checked_current(&w)) { error=SRV_ERR_BADF;break; }
        if(!woke || process_signal_pending_current()) { error=SRV_ERR_INTR;break; }
    }
    if(!length) error=0;
out: unlock(flags);return done?(int64_t)done:error;
}
uint16_t pty_poll(uint64_t handle,bool master,uint16_t events) {
    struct pty_model *p=pty_state(handle);return p?pty_model_poll(p,master,events):SRV_POLLNVAL;
}
struct attr_wait { uint64_t handle;unsigned action; };
static bool attr_ready(void *arg) {
    struct attr_wait *w=arg;
    struct pty_model *p=pty_state(w->handle);
    return !p || p->master_closed || process_signal_pending_current() || process_should_exit_current() ||
        ((w->action==SRV_TCSANOW || !p->output.size) && PTY_BYTES-p->input.size>=p->line_size);
}
struct checked_attr_wait { struct checked_wait *descriptor;struct attr_wait attr; };
static bool checked_attr_ready(void *arg) {
    struct checked_attr_wait *w=arg;
    return !checked_current(w->descriptor) || attr_ready(&w->attr);
}
int64_t process_file_pty_setattr(struct process *process,uint64_t fd,const struct srv_termios *attributes,unsigned action) {
    uint64_t flags=lock();struct checked_wait descriptor;int64_t result=SRV_ERR_BADF;
    if(!checked_init(&descriptor,process,fd)) goto out;
    struct checked_attr_wait w={&descriptor,{descriptor.handle,action}};
    for(;;) {
        if(!checked_current(&descriptor)) { result=SRV_ERR_BADF;break; }
        struct pty_model *p=pty_state(descriptor.handle);
        if(p->master_closed || process_should_exit_current()) { result=-1;break; }
        result=pty_model_attributes(p,attributes,action);if(result!=SRV_ERR_AGAIN) break;
        if(process_signal_pending_current()) { result=SRV_ERR_INTR;break; }
        bool woke=scheduler_wait(&waiters,checked_attr_ready,&w);
        if(!checked_current(&descriptor)) { result=SRV_ERR_BADF;break; }
        if(!woke || process_signal_pending_current()) { result=SRV_ERR_INTR;break; }
    }
    if(!result) pty_wake();
out: unlock(flags);return result;
}
int64_t pty_setattr(uint64_t handle,const struct srv_termios *attributes,unsigned action) {
    uint64_t flags=lock();int64_t result=-1;
    struct attr_wait w={handle,action};
    for(;;) {
        struct pty_model *p=pty_state(handle);if(!p || p->master_closed || process_should_exit_current()) break;
        result=pty_model_attributes(p,attributes,action);if(result!=SRV_ERR_AGAIN) break;
        if(process_signal_pending_current() || !scheduler_wait(&waiters,attr_ready,&w)) { result=SRV_ERR_INTR;break; }
    }
    if(!result) pty_wake();
    unlock(flags);return result;
}
