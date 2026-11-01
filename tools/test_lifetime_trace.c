#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_log.h>
#include "../userspace/deskd/operation_trace.h"
static char text[8192];static size_t used;
void srv_puts(const char *line) {size_t n=strlen(line);assert(n<sizeof(text)-used);memcpy(text+used,line,n+1);used+=n;}
int main(void) {
    struct operation_trace t={.at_us=UINT64_MAX};
    struct lifetime_trace q={.pid=42,.cycle=999,.begin_us=UINT64_MAX-100,.launch_begin_us=UINT64_MAX-90,.launch_end_us=UINT64_MAX-80,.ready_us=UINT64_MAX-70,.close_request_us=UINT64_MAX-60,.visual_us=UINT64_MAX-50,.exit_seen_us=UINT64_MAX-40,.reap_queued_us=UINT64_MAX-30,.reap_begin_us=UINT64_MAX-20,.reap_end_us=UINT64_MAX-10,.closed_us=UINT64_MAX-1};
    t.ready_worst=t.close_worst=q;
    operation_trace_report(&t,1);
    char *a=strstr(text,"desktop-life-worst: version=1 snapshot_us=18446744073709551615 kind=ready");
    char *b=strstr(text,"desktop-life-worst: version=1 snapshot_us=18446744073709551615 kind=close");
    assert(a && b && a<b && (size_t)(strchr(a,'\n')-a)>384);
    assert(strstr(a,"closed_us=18446744073709551614 final=1\n"));
    used=0;text[0]=0;lifetime_trace_report(&(struct lifetime_trace){0},"ready",1,1);assert(!used);
    puts("lifetime trace: PASS full-width retained clocks survive formatting; absent records are not fabricated");
}
