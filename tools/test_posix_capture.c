/* Fault-injected orchestration: never let a missing exit or retained tree pass. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <srvros/process.h>
#include <srvros/timer.h>
#include <srvros/console.h>
#include "../kernel/src/posix_capture.c"
static uint64_t now;
static unsigned spawned,released,revoked,mode;
uint64_t timer_monotonic_us(void) { return now; }
void timer_wait_ticks(uint64_t ticks) { now+=ticks*10000; }
void console_write(const char *text) { (void)text; }
void console_printf(const char *fmt,...) { (void)fmt; }
uint64_t process_lifetime_create(void) { return mode==4?0:1; }
int64_t process_lifetime_spawn(uint64_t token,const char *path,const char *args) {
    (void)path;(void)args;assert(token==1);spawned++;return mode==5?-1:100;
}
int64_t process_wait(uint64_t pid,uint64_t *status,bool nohang) {
    assert(pid==100 && nohang);now+=10000;
    if(mode==2)return 0;
    *status=mode==1?7:0;return pid;
}
bool process_lifetime_revoke(uint64_t token) { assert(token==1);revoked++;return true; }
bool process_lifetime_drain(uint64_t token) { assert(token==1);return true; }
bool process_lifetime_status(uint64_t token,struct process_lifetime_status *s) {
    assert(token==1);s->members=mode==3?1:0;return true;
}
bool process_lifetime_release(uint64_t token) { assert(token==1 && mode!=3);released++;return true; }
int main(void) {
    for(mode=0;mode<=5;mode++) {
        now=0;spawned=released=revoked=0;
        bool ok=posix_capture_run();assert(ok==(mode==0));
        assert(spawned==(mode==0?5:mode==4?0:1));
        assert(released==(mode==0?5:(mode==3 || mode==4)?0:1));
        if(mode==2)assert(now>=180000000 && now<181000000);
        if(mode==3)assert(now>=10000000 && now<11000000);
    }
    puts("posix-capture: PASS success, nonzero exit, deadline, retained tree, allocation and spawn failure");
}
