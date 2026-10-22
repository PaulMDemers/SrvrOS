#define SRVROS_IRQ_HANDLERS_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include "../kernel/src/irq_handlers.c"
static unsigned calls,completed;
static uint64_t own;
static bool claim(void *p) { calls++;return p!=NULL; }
static bool mutate(void *p) {
    (void)p;assert(!irq_handler_remove(own));assert(!irq_handler_register(9,claim,(void *)3));return true;
}
static void complete(void *p) {
    (void)p;completed++;
    assert(!irq_handler_remove(own) && !irq_handler_register(9,claim,(void *)5));
    assert(!irq_handlers_dispatch(9));irq_handlers_complete(9);
}
static unsigned exits,exit_completions;
static bool request_in_completion;
static void exit_hook(void *p) {
    assert(p==(void *)42 && !dispatch_depth && !completion_pending && !active_token && !deferred_exit.token);
    assert(exit_completions==2);exits++;
    /* Simulate work on another stack: registration/dispatch/removal can no
     * longer be stranded behind this delivery's deferred bookkeeping. */
    uint64_t t=irq_handler_register(10,claim,(void *)9);assert(t);
    assert(irq_handlers_dispatch(10));irq_handlers_complete(10);irq_handlers_finish(10);
    assert(irq_handler_remove(t));
}
static void request_exit(void) {
    assert(!irq_handler_defer_exit(0,exit_hook,(void *)42));
    assert(!irq_handler_defer_exit(own+100,exit_hook,(void *)42));
    assert(!irq_handler_defer_exit(own,NULL,(void *)42));
    assert(irq_handler_defer_exit(own,exit_hook,(void *)42));
    assert(!irq_handler_defer_exit(own,exit_hook,(void *)42));
    irq_handlers_finish(9);assert(!exits);
}
static bool deferred_handler(void *p) {
    (void)p;if(!request_in_completion)request_exit();return false;
}
static void deferred_completion(void *p) {
    if(p && request_in_completion)request_exit();
    exit_completions++;irq_handlers_finish(9);assert(!exits);
}
static bool unobserved_handler(void *p) {
    (void)p;assert(!irq_handler_defer_exit(own,exit_hook,NULL));return false;
}
int main(void) {
    for(unsigned i=0;i<16;i++)assert(!irq_handlers_dispatch(i));
    for(unsigned i=0;i<4;i++)assert(!irq_handler_register((unsigned[]){0,1,4,12}[i],claim,NULL));
    assert(!irq_handler_register(16,claim,NULL) && !irq_handler_register(9,NULL,NULL));
    uint64_t a=irq_handler_register(9,claim,NULL),b=irq_handler_register(9,claim,(void *)1);
    assert(a && b && a!=b && !irq_handler_register(9,claim,NULL));
    assert(irq_handlers_dispatch(9) && calls==2);assert(!irq_handlers_dispatch(10) && calls==2);
    assert(irq_handler_remove(b) && !irq_handler_remove(b));
    assert(!irq_handlers_dispatch(9) && calls==3);
    own=irq_handler_register(9,mutate,NULL);assert(own>b && irq_handlers_dispatch(9));
    assert(irq_handler_remove(own) && irq_handler_remove(a));
    uint64_t tokens[IRQ_HANDLER_SLOTS];
    for(unsigned i=0;i<IRQ_HANDLER_SLOTS;i++) { tokens[i]=irq_handler_register(9,claim,(void *)(uintptr_t)(i+1));assert(tokens[i]); }
    assert(!irq_handler_register(10,claim,NULL));
    for(unsigned i=0;i<IRQ_HANDLER_SLOTS;i++)assert(irq_handler_remove(tokens[i]));
    assert(!irq_handler_remove(a) && !irq_handler_remove(0) && !irq_handlers_dispatch(9));
    own=irq_handler_register_observed(9,claim,complete,(void *)1);
    uint64_t other=irq_handler_register_observed(9,claim,complete,NULL);
    assert(own && other);
    irq_handlers_complete(9);assert(!completed);
    assert(irq_handlers_dispatch(9) && completion_pending==2);
    assert(!irq_handler_remove(own) && !irq_handler_register(10,claim,NULL));
    assert(!irq_handlers_dispatch(9));irq_handlers_complete(10);irq_handlers_complete(255);
    assert(!completed && completion_pending==2);
    irq_handlers_complete(9);assert(completed==2 && !completion_pending);
    irq_handlers_complete(9);assert(completed==2);
    assert(irq_handler_remove(own) && irq_handler_remove(other));
    irq_handlers_complete(9);assert(completed==2);
    assert(!irq_handler_defer_exit(own,exit_hook,(void *)42));
    own=irq_handler_register(9,unobserved_handler,NULL);assert(own);
    irq_handlers_dispatch(9);assert(irq_handler_remove(own));
    for(unsigned mode=0;mode<2;mode++) {
        request_in_completion=mode;exits=exit_completions=0;
        own=irq_handler_register_observed(9,deferred_handler,deferred_completion,(void *)1);
        other=irq_handler_register_observed(9,claim,deferred_completion,NULL);assert(own && other);
        assert(!irq_handlers_dispatch(9));
        irq_handlers_finish(9);assert(!exits);
        irq_handlers_complete(10);irq_handlers_finish(10);assert(!exits);
        irq_handlers_complete(9);assert(exit_completions==2 && !exits);
        assert(!irq_handler_remove(own) && !irq_handler_register(10,claim,NULL));
        assert(!irq_handlers_dispatch(9));irq_handlers_finish(10);assert(!exits);
        irq_handlers_finish(9);assert(exits==1);
        irq_handlers_finish(9);irq_handlers_complete(9);assert(exits==1);
        assert(irq_handler_remove(own) && irq_handler_remove(other));
    }
    puts("irq-exit: PASS token scope, one request, all-completions barrier, registry drained before hook, no repeat");
    next_token=UINT64_MAX;assert(!irq_handler_register(9,claim,NULL));
    puts("irq-handlers: PASS shared dispatch, duplicate/capacity/reserved-line refusal, mutation exclusion, stale tokens and teardown");
}
