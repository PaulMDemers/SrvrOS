/* Only the console formatter is a host adapter. The production static
 * report_sync_content and its command formatter run unchanged on the C
 * observer's retained snapshot; unrelated logger code is link-time collected. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "test_workspace_sync_content_wire.h"
#define SRVROS_WORKSPACE_LOG_HOST_TEST
#include "../kernel/src/workspace_log.c"

/* SrvrOS console_printf consumes uint64_t for %u/%x. Passing the production
 * format directly to libc printf would silently truncate/reinterpret values. */
void console_printf(const char *format,...) {
    va_list args;va_start(args,format);
    while(*format) {
        if(*format!='%') {putchar(*format++);continue;}
        format++;
        if(*format=='u')printf("%llu",(unsigned long long)va_arg(args,uint64_t));
        else if(*format=='x')printf("%llx",(unsigned long long)va_arg(args,uint64_t));
        else if(*format=='s')fputs(va_arg(args,const char *),stdout);
        else if(*format=='%')putchar('%');
        else assert(!"unsupported production console conversion");
        if(*format)format++;
    }
    va_end(args);
}

void workspace_sync_content_wire(uint64_t at,const struct sync_timing_snapshot *snapshot) {
    assert(snapshot && snapshot->enabled && !snapshot->active && !snapshot->invalid);
    report_sync_content(at,snapshot);
}
void workspace_commit_wait_wire(uint64_t at,const struct commit_wait_snapshot *snapshot) {
    assert(snapshot);report_commit_wait(at,snapshot);
}
