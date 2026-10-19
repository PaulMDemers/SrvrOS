#define SRVROS_IRQ_HANDLERS_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include "../kernel/src/irq_handlers.c"
static unsigned calls;
static uint64_t own;
static bool claim(void *p) { calls++;return p!=NULL; }
static bool mutate(void *p) {
    (void)p;assert(!irq_handler_remove(own));assert(!irq_handler_register(9,claim,(void *)3));return true;
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
    next_token=UINT64_MAX;assert(!irq_handler_register(9,claim,NULL));
    puts("irq-handlers: PASS shared dispatch, duplicate/capacity/reserved-line refusal, mutation exclusion, stale tokens and teardown");
}
