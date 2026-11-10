/* Actual fixed-state counter helpers through the production workspace formatter.
 * Clocks/gate scheduling are host adapters; this is not admission authority. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/capture_write_timing_state.h"
#include "../kernel/src/capture_gate_timing_state.h"
#define SRVROS_WORKSPACE_LOG_HOST_TEST
#include "../kernel/src/workspace_log.c"
static struct capture_write_snapshot fixture;
void process_capture_write_snapshot(struct capture_write_snapshot *out) {*out=fixture;}
uint64_t timer_monotonic_us(void) {return 1000;}
void console_printf(const char *format,...) {
    va_list args;va_start(args,format);
    while(*format) {
        if(*format!='%') {putchar(*format++);continue;}
        format++;
        if(*format=='u')printf("%llu",(unsigned long long)va_arg(args,uint64_t));
        else if(*format=='x')printf("%llx",(unsigned long long)va_arg(args,uint64_t));
        else if(*format=='s')fputs(va_arg(args,const char *),stdout);
        else if(*format=='%')putchar('%');
        else assert(!"unsupported kernel conversion");
        if(*format)format++;
    }
    va_end(args);
}
int main(int argc,char **argv) {
    assert(argc==2);bool all_fast=!strcmp(argv[1],"all-fast");
    assert(all_fast || !strcmp(argv[1],"mixed"));
    fixture.enabled=1;struct capture_write_record *r=capture_write_touch(&fixture,42,100);assert(r);
    r->attempts=2;
    if(all_fast) {
        capture_write_sample(r,CAP_WRITE_RESERVE,101,102);
        capture_write_sample(r,CAP_WRITE_COPY,103,105);
        capture_write_sample(r,CAP_WRITE_HANDLER,100,106);
        capture_write_fast_result(r,65536,65536);
        capture_write_result(r,100,107,65536,65536);
    } else {
        struct capture_gate_wait gate={.start_us=101,.end_us=104};
        gate.counts.running_us=3;capture_gate_complete(&r->gate,&gate);
        capture_write_sample(r,CAP_WRITE_GATE,101,104);
        capture_write_sample(r,CAP_WRITE_RESERVE,105,106);
        capture_write_sample(r,CAP_WRITE_COPY,107,109);
        capture_write_sample(r,CAP_WRITE_HANDLER,104,110);
        capture_write_sample(r,CAP_WRITE_HANDOFF,110,111);
        capture_write_result(r,100,112,65536,65536);
    }
    uint64_t at=r->last_us+1;
    capture_write_sample(r,CAP_WRITE_RESERVE,at+1,at+2);
    capture_write_sample(r,CAP_WRITE_COPY,at+3,at+5);
    capture_write_sample(r,CAP_WRITE_HANDLER,at,at+6);
    capture_write_fast_result(r,5464,5464);
    capture_write_result(r,at,at+7,5464,5464);
    r->retired=1;assert(!r->invalid && !fixture.invalid);
    capture_write_checkpoint(true);
    return 0;
}
