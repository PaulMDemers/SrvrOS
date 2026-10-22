/* Explicit monitor diagnostic; never invoked by ordinary desktop startup. */
#include <srvros/process.h>
#include <srvros/console.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <stddef.h>

#define CHECK(x) do { if(!(x)) { console_printf("lifetime-test: FAIL line=%u\n",(uint64_t)__LINE__);goto failed; } } while(0)
static bool retire(uint64_t token) {
    if(!process_lifetime_revoke(token))return false;
    uint64_t end=timer_monotonic_us()+5000000;
    struct process_lifetime_status s;
    do {
        if(!process_lifetime_drain(token) || !process_lifetime_status(token,&s))return false;
        if(!s.members)return process_lifetime_release(token);
        timer_wait_ticks(1);
    } while(timer_monotonic_us()<end);
    return false;
}
static bool wait_exit(uint64_t pid,uint64_t *status,uint64_t us) {
    uint64_t end=timer_monotonic_us()+us;
    do {
        int64_t result=process_wait(pid,status,true);
        if(result==(int64_t)pid)return true;
        if(result<0)return false;
        timer_wait_ticks(1);
    }while(timer_monotonic_us()<end);
    return false;
}
void process_lifetime_test_run(void) {
    uint64_t token=0,unowned=0,status=0;
    struct process_lifetime_status s;
    token=process_lifetime_create();CHECK(token);
    CHECK(process_lifetime_spawn(token,"/fat/bin/no-such-lifetime-program","")<0);
    CHECK(process_lifetime_status(token,&s) && !s.members);
    CHECK(retire(token));token=0;
    console_write("lifetime-test: failed root retired\n");

    token=process_lifetime_create();CHECK(token);
    int64_t root=process_lifetime_spawn(token,"/fat/bin/lifetimeprobe","--churn");CHECK(root>0);
    CHECK(process_lifetime_of(root)==token);
    CHECK(wait_exit(root,&status,60000000) && status==0);
    CHECK(process_lifetime_status(token,&s) && !s.members);
    CHECK(retire(token));token=0;
    console_write("lifetime-test: churn retired\n");

    int64_t separate=process_spawn_background_elf_args_quiet("/fat/bin/lifetimeprobe","--hold");CHECK(separate>0);unowned=separate;
    CHECK(process_wait_startup(unowned,500) && !process_lifetime_of(unowned));
    token=process_lifetime_create();CHECK(token);
    root=process_lifetime_spawn(token,"/fat/bin/lifetimeprobe","--orphan");CHECK(root>0);
    CHECK(wait_exit(root,&status,10000000) && status==0);
    /* Observe only after the parent has been reaped, not a retained PID tree. */
    uint64_t end=timer_monotonic_us()+10000000;
    unsigned ready=0;
    do {
        ready=0;uint64_t index=0,pid;
        while((index=process_list(index,&pid,NULL,NULL))) {
            if(process_lifetime_of(pid)!=token)continue;
            if(process_wait_startup(pid,1) && process_group(pid)!=(uint64_t)root && process_session(pid)!=(uint64_t)root)ready++;
        }
        if(ready==2)break;
        timer_wait_ticks(1);
    }while(timer_monotonic_us()<end);
    CHECK(ready==2 && process_lifetime_status(token,&s) && s.members==2);
    CHECK(!process_lifetime_release(token));
    CHECK(retire(token));token=0;
    CHECK(process_signal_pid(unowned,0));
    CHECK(process_signal_pid(unowned,SRV_SIGNAL_KILL));
    CHECK(wait_exit(unowned,&status,5000000));unowned=0;
    console_write("lifetime-test: PASS orphan exec setsid group-change descendants=2 unrelated=preserved drained=yes\n");return;
failed:
    if(token && !retire(token))console_write("lifetime-test: ownership retained; retirement incomplete\n");
    if(unowned) { (void)process_signal_pid(unowned,SRV_SIGNAL_KILL);(void)wait_exit(unowned,&status,5000000); }
}
