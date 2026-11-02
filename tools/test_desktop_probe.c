#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/lib/src/desktop.c"
static unsigned calls;
static int result=1;
static const char *state="background";
long srv_proc_list(uint64_t selector,struct srv_process_info *out) {
    calls++;assert(selector==(SRV_PROC_LIST_BY_PID|987654));
    *out=(struct srv_process_info){.pid=987654};strcpy(out->state,state);return result;
}
int main(void) {
    assert(desk_process_alive(987654) && calls==1);
    state="stopping";assert(desk_process_alive(987654) && calls==2);
    state="exited";assert(!desk_process_alive(987654) && calls==3);
    result=0;assert(!desk_process_alive(987654) && calls==4);
    result=-1;assert(!desk_process_alive(987654) && calls==5);
    assert(!desk_process_alive(0) && !desk_process_alive(SRV_PROC_LIST_BY_PID) && calls==5);
    puts("desktop-probe: PASS one metadata syscall, active/stopping/exited/missing/error and invalid PID");
}
