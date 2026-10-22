/* Exercise the real registry and IRQ dispatcher without hardware I/O. */
#define SRVROS_IRQ_HANDLERS_HOST_TEST
#define SRVROS_X86_64_IO_H
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static unsigned order[16],count;
static void mark(unsigned n){assert(count<16);order[count++]=n;}
static void outb(uint16_t port,uint8_t value){assert(value==0x20);mark(port==0xa0?2:3);}
static void io_wait(void){}
#include "../kernel/src/irq_handlers.c"
#include "../kernel/src/arch/x86_64/irq.c"
void lapic_send_eoi(void){mark(4);}
void console_printf(const char *fmt,...){(void)fmt;mark(7);}
void timer_handle_tick(void){}
uint64_t timer_ticks(void){return 0;}
void net_timer_tick(void){}
void scheduler_preempt_tick(void){}
void keyboard_handle_irq(void){}
void serial_handle_irq(void){}
void mouse_handle_irq(void){}
uint8_t e1000_irq_line(void){return 255;}
bool e1000_handle_interrupt(void){return false;}
static uint64_t exit_token;
static void terminal(void *p) {
    assert(p==(void *)42 && !dispatch_depth && !completion_pending && !deferred_exit.token);
    mark(8);assert(irq_handler_remove(exit_token));
}
static bool exit_request(void *p) {
    (void)p;mark(1);assert(irq_handler_defer_exit(exit_token,terminal,(void *)42));return true;
}
static bool handler(void *p){mark(1);return p!=0;}
static void after(void *p){(void)p;mark(5);}
int main(void){
    for(unsigned claimed=0;claimed<2;claimed++){
        uint64_t token=irq_handler_register_observed(9,handler,after,(void *)(uintptr_t)claimed);
        assert(token);count=0;irq_dispatch(41);
        unsigned want_claimed[]={1,2,3,4,5},want_unclaimed[]={1,7,2,3,4,5};
        unsigned *want=claimed?want_claimed:want_unclaimed;
        assert(count==(claimed?5:6));
        for(unsigned i=0;i<count;i++)assert(order[i]==want[i]);
        assert(irq_handler_remove(token));
        irq_handlers_complete(9);assert(order[count-1]==5);
    }
    exit_token=irq_handler_register_observed(9,exit_request,after,NULL);
    uint64_t peer=irq_handler_register_observed(9,handler,after,(void *)1);assert(exit_token && peer);
    count=0;irq_dispatch(41);
    unsigned drained[]={1,1,2,3,4,5,5,8};assert(count==8);
    for(unsigned i=0;i<count;i++)assert(order[i]==drained[i]);
    assert(irq_handler_remove(peer));
    count=0;irq_dispatch(255);assert(!count);
    puts("irq-completion: PASS handler/result, original PIC then LAPIC EOI, all observers before terminal hook, removal and invalid vector");
}
