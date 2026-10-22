#include <srvros/irq_handlers.h>
#include <stddef.h>
#define IRQ_HANDLER_SLOTS 16
struct registration { uint64_t token;uint8_t line;irq_handler_fn fn;void *context;irq_completion_fn complete;bool pending; };
static struct registration handlers[IRQ_HANDLER_SLOTS];
static uint64_t next_token=1;
static unsigned dispatch_depth,completion_pending;
static uint64_t active_token;
static uint8_t active_line;
static struct { uint64_t token;uint8_t line;irq_completion_fn fn;void *context;bool ready; } deferred_exit;
static uint64_t lock(void) {
#ifdef SRVROS_IRQ_HANDLERS_HOST_TEST
    return 0;
#else
    uint64_t f;__asm__ volatile("pushfq;popq %0;cli":"=r"(f)::"memory");return f;
#endif
}
static void unlock(uint64_t f) {
#ifdef SRVROS_IRQ_HANDLERS_HOST_TEST
    (void)f;
#else
    if(f&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
}
uint64_t irq_handler_register(uint8_t line,irq_handler_fn fn,void *context) {
    return irq_handler_register_observed(line,fn,NULL,context);
}
uint64_t irq_handler_register_observed(uint8_t line,irq_handler_fn fn,irq_completion_fn complete,void *context) {
    if(line>=16 || line==0 || line==1 || line==4 || line==12 || !fn)return 0;
    uint64_t f=lock(),token=0;
    if(dispatch_depth || completion_pending || deferred_exit.token || next_token==UINT64_MAX)goto out;
    unsigned free_slot=IRQ_HANDLER_SLOTS;
    for(unsigned i=0;i<IRQ_HANDLER_SLOTS;i++) {
        if(!handlers[i].token) { if(free_slot==IRQ_HANDLER_SLOTS)free_slot=i; }
        else if(handlers[i].line==line && handlers[i].fn==fn && handlers[i].context==context)goto out;
    }
    if(free_slot<IRQ_HANDLER_SLOTS) {
        token=next_token++;handlers[free_slot]=(struct registration){.token=token,.line=line,.fn=fn,.context=context,.complete=complete};
    }
out:
    unlock(f);return token;
}
bool irq_handler_remove(uint64_t token) {
    uint64_t f=lock();bool found=false;
    if(token && !dispatch_depth && !completion_pending && !deferred_exit.token)for(unsigned i=0;i<IRQ_HANDLER_SLOTS;i++)if(handlers[i].token==token) {
        handlers[i]=(struct registration){0};found=true;break;
    }
    unlock(f);return found;
}
bool irq_handlers_dispatch(uint8_t line) {
    if(line>=16)return false;
    uint64_t f=lock();bool claimed=false;
    if(dispatch_depth || completion_pending || deferred_exit.token){unlock(f);return false;}
    dispatch_depth++;
    for(unsigned i=0;i<IRQ_HANDLER_SLOTS;i++)if(handlers[i].token && handlers[i].line==line) {
        active_token=handlers[i].token;active_line=line;
        claimed=handlers[i].fn(handlers[i].context)||claimed;
        active_token=0;
        if(handlers[i].complete){handlers[i].pending=true;completion_pending++;}
    }
    dispatch_depth--;unlock(f);return claimed;
}

void irq_handlers_complete(uint8_t line) {
    if(line>=16)return;
    uint64_t f=lock();
    if(dispatch_depth || !completion_pending){unlock(f);return;}
    dispatch_depth++;
    for(unsigned i=0;i<IRQ_HANDLER_SLOTS;i++)if(handlers[i].pending && handlers[i].line==line) {
        handlers[i].pending=false;completion_pending--;
        active_token=handlers[i].token;active_line=line;
        handlers[i].complete(handlers[i].context);
        active_token=0;
    }
    dispatch_depth--;
    if(deferred_exit.token && deferred_exit.line==line && !completion_pending)deferred_exit.ready=true;
    unlock(f);
}

/* A single token-bound request per delivery. Observed callbacks only: their
 * completion establishes the barrier, including requests made by completion. */
bool irq_handler_defer_exit(uint64_t token,irq_completion_fn fn,void *context) {
    uint64_t f=lock();bool ok=false;
    if(token && token==active_token && dispatch_depth==1 && fn && !deferred_exit.token) {
        for(unsigned i=0;i<IRQ_HANDLER_SLOTS;i++)if(handlers[i].token==token && handlers[i].complete) {
            deferred_exit.token=token;deferred_exit.line=active_line;
            deferred_exit.fn=fn;deferred_exit.context=context;deferred_exit.ready=false;ok=true;break;
        }
    }
    unlock(f);return ok;
}
void irq_handlers_finish(uint8_t line) {
    uint64_t f=lock();
    if((f&(1ull<<9)) || dispatch_depth || completion_pending || !deferred_exit.ready ||
       !deferred_exit.token || deferred_exit.line!=line) { unlock(f);return; }
    irq_completion_fn fn=deferred_exit.fn;void *context=deferred_exit.context;
    /* Clear EVERYTHING before calling out. A terminal hook may retain this
     * stack forever and switch tasks; no registry lifetime may depend on return. */
    deferred_exit.token=0;deferred_exit.fn=NULL;deferred_exit.context=NULL;
    deferred_exit.ready=false;deferred_exit.line=0;
    fn(context);
    unlock(f);
}
