#include <srvros/irq_handlers.h>
#include <stddef.h>
#define IRQ_HANDLER_SLOTS 16
struct registration { uint64_t token;uint8_t line;irq_handler_fn fn;void *context; };
static struct registration handlers[IRQ_HANDLER_SLOTS];
static uint64_t next_token=1;
static unsigned dispatch_depth;
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
    if(line>=16 || line==0 || line==1 || line==4 || line==12 || !fn)return 0;
    uint64_t f=lock(),token=0;
    if(dispatch_depth || next_token==UINT64_MAX)goto out;
    unsigned free_slot=IRQ_HANDLER_SLOTS;
    for(unsigned i=0;i<IRQ_HANDLER_SLOTS;i++) {
        if(!handlers[i].token) { if(free_slot==IRQ_HANDLER_SLOTS)free_slot=i; }
        else if(handlers[i].line==line && handlers[i].fn==fn && handlers[i].context==context)goto out;
    }
    if(free_slot<IRQ_HANDLER_SLOTS) {
        token=next_token++;handlers[free_slot]=(struct registration){token,line,fn,context};
    }
out:
    unlock(f);return token;
}
bool irq_handler_remove(uint64_t token) {
    uint64_t f=lock();bool found=false;
    if(token && !dispatch_depth)for(unsigned i=0;i<IRQ_HANDLER_SLOTS;i++)if(handlers[i].token==token) {
        handlers[i]=(struct registration){0};found=true;break;
    }
    unlock(f);return found;
}
bool irq_handlers_dispatch(uint8_t line) {
    if(line>=16)return false;
    uint64_t f=lock();bool claimed=false;dispatch_depth++;
    for(unsigned i=0;i<IRQ_HANDLER_SLOTS;i++)if(handlers[i].token && handlers[i].line==line)
        claimed=handlers[i].fn(handlers[i].context)||claimed;
    dispatch_depth--;unlock(f);return claimed;
}
