/* Actual fixed-state counter helpers through the production workspace formatter.
 * Clocks/gate scheduling are host adapters; this is not admission authority. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/capture_write_timing_state.h"
#include "../kernel/src/capture_gate_timing_state.h"
#define SRVROS_WORKSPACE_LOG_HOST_TEST
#ifdef SRVROS_CAPTURE_GATE_SYNC_WIRE
#define timer_monotonic_us workspace_capture_wire_clock
#endif
#include "../kernel/src/workspace_log.c"
#ifndef SRVROS_CAPTURE_GATE_SYNC_WIRE
/* This formatter-only fixture has no private wait or live sync producer. */
void sync_commit_wait_snapshot(struct commit_wait_snapshot *out) {
    *out=(struct commit_wait_snapshot){.enabled=1};
}
#endif
uint64_t timer_monotonic_us(void) {return 1000;}
#ifdef SRVROS_CAPTURE_GATE_SYNC_WIRE
#undef timer_monotonic_us
#endif
static struct capture_write_snapshot fixture;
void process_capture_write_snapshot(struct capture_write_snapshot *out) {*out=fixture;}
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
static int emit(bool all_fast,const struct sync_gate_context *context,bool final) {
    fixture=(struct capture_write_snapshot){0};
    fixture.enabled=1;struct capture_write_record *r=capture_write_touch(&fixture,42,100);assert(r);
    r->attempts=2;
    if(all_fast) {
        capture_write_sample(r,CAP_WRITE_RESERVE,101,102);
        capture_write_sample(r,CAP_WRITE_COPY,103,105);
        capture_write_sample(r,CAP_WRITE_HANDLER,100,106);
        capture_write_fast_result(r,65536,65536);
        capture_write_result(r,100,107,65536,65536);
    } else if(context) {
        struct capture_gate_wait gate={.start_us=115,.end_us=170};
        gate.counts.running_us=55;gate.counts.owned_us=40;gate.counts.scopes=1;
        gate.owner=(struct capture_gate_owner){.thread=99,.pid=7,.syscall=87,
            .scope_start_us=120,.scope_end_us=160,.overlap_start_us=120,.overlap_end_us=160,.sync=*context};
        capture_gate_complete(&r->gate,&gate);
        capture_write_sample(r,CAP_WRITE_GATE,115,170);
        capture_write_sample(r,CAP_WRITE_RESERVE,171,172);
        capture_write_sample(r,CAP_WRITE_COPY,173,174);
        capture_write_sample(r,CAP_WRITE_HANDLER,170,175);
        capture_write_sample(r,CAP_WRITE_HANDOFF,175,177);
        capture_write_result(r,100,180,65536,65536);
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
    capture_write_checkpoint(final);
    return 0;
}

#ifdef SRVROS_CAPTURE_GATE_SYNC_WIRE
void workspace_gate_fixture(const struct sync_gate_context *context) {assert(emit(false,context,true)==0);}
#else
int main(int argc,char **argv) {
    assert(argc==2);bool all_fast=!strcmp(argv[1],"all-fast");
    bool periodic=!strcmp(argv[1],"periodic");
    assert(all_fast || periodic || !strcmp(argv[1],"mixed"));return emit(all_fast,NULL,!periodic);
}
#endif
