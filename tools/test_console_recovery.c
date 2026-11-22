#include <assert.h>
#include <string.h>
#include <stdio.h>
#define main guifail_unused_main
#include "../userspace/guifail/guifail.c"
#undef main

static char output[1024];
static unsigned queries,sleeps,clocks,ready_after,clock_error,rollback;
static int query_error,sleep_error;
static long tick,step;
void srv_puts(const char *s){assert(strlen(output)+strlen(s)<sizeof(output));strcat(output,s);}
long srv_ticks(void){unsigned n=++clocks;if(n==clock_error)return -1;if(n==rollback)return tick-2;return tick;}
long srv_sleep_ticks(uint64_t n){assert(n==1);sleeps++;if(sleep_error)return -1;tick+=step;return 0;}
int gfx_console_muted(void){unsigned n=queries++;if(query_error && n==(unsigned)(query_error-1))return -1;return n<ready_after;}
static void reset(void){memset(output,0,sizeof(output));queries=sleeps=clocks=0;ready_after=0;clock_error=rollback=0;query_error=sleep_error=0;tick=10;step=1;}
static void failed(int status){assert(console_recovery_probe(1)==status);assert(!strstr(output,"console-muted=0"));}
int main(void){
    reset();assert(!console_recovery_probe(0) && queries==1 && !clocks && !sleeps);
    reset();ready_after=1;assert(console_recovery_probe(0)==1 && queries==1 && !sleeps);
    reset();query_error=1;assert(console_recovery_probe(0)==2 && !strstr(output,"console-muted=0"));
    reset();assert(!console_recovery_probe(1) && queries==1 && !sleeps);
    assert(strstr(output,"ticks=0 polls=1"));
    reset();ready_after=3;assert(!console_recovery_probe(1) && queries==4 && sleeps==3);
    assert(strstr(output,"ticks=3 polls=4"));
    reset();ready_after=200;assert(!console_recovery_probe(1) && queries==201 && sleeps==200);
    reset();ready_after=201;failed(1);assert(queries==201 && sleeps==200);
    reset();ready_after=201;step=0;failed(2);assert(queries==201 && sleeps==200 && strstr(output,"stalled-clock"));
    for(unsigned i=1;i<=2;i++){reset();clock_error=i;failed(2);assert(!queries && !sleeps);}
    reset();ready_after=201;rollback=3;failed(2);assert(queries==1 && sleeps==1);
    reset();ready_after=201;step=201;failed(1);assert(queries==1 && sleeps==1);
    reset();ready_after=201;query_error=4;failed(2);assert(queries==4 && sleeps==3);
    reset();ready_after=201;sleep_error=1;failed(2);assert(queries==1 && sleeps==1);
    puts("console-recovery: PASS actual read-only producer, immediate/delayed/boundary restore, deadline, clock rollback/stall, query and sleep errors");
}
