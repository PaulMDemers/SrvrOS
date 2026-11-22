/* Explicit actual-initramfs ELF/READY/GO/EOF diagnostic. No sockets/provider. */
#include <srvros/http_child_preflight.h>
#include <srvros/http_child.h>
#include <srvros/console.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <limits.h>
#include <stddef.h>
#define PREFLIGHT_LIMIT 4u
static bool enabled,running;
static unsigned issued;
static struct http_child_owner owners[PREFLIGHT_LIMIT];
static struct scheduler_wait_queue poll_queue;
void http_child_preflight_enable(bool isolated_init_ready) {
    if(!running && !issued)enabled=isolated_init_ready;
}
static bool poll_due(void *arg) {return timer_ticks()>=*(uint64_t *)arg;}
static void pause_poll(void) {
    uint64_t tick=timer_ticks();if(tick==UINT64_MAX)return;
    tick++;if(!scheduler_wait_timeout(&poll_queue,poll_due,&tick,tick))scheduler_yield();
}
static bool child_exit(struct http_child_owner *o,uint64_t end,uint64_t *status) {
    uint64_t last=timer_monotonic_us();
    for(unsigned i=0;i<2000;i++) {
        uint64_t now=timer_monotonic_us();if(now<last || now>=end)return false;
        last=now;
        if(http_child_observe(o,status)) {
            uint64_t after=timer_monotonic_us();return after>=now && after<end && http_child_reap(o);
        }
        pause_poll();
    }
    return false;
}
static bool retire(struct http_child_owner *o) {
    (void)http_child_cancel(o);
    uint64_t start=timer_monotonic_us(),last=start;if(start>UINT64_MAX-5000000)return false;
    for(unsigned i=0;i<1000;i++) {
        if(http_child_retire(o))return true;
        uint64_t now=timer_monotonic_us();if(now<last || now-start>=5000000)return false;
        last=now;pause_poll();
    }
    return false;
}
static void put(uint8_t *p,uint64_t value,unsigned bytes) {
    for(unsigned i=0;i<bytes;i++)p[i]=(uint8_t)(value>>(i*8));
}
static bool one(unsigned variant,uint64_t expected) {
    struct http_child_owner *o=&owners[issued++];
    uint64_t status=UINT64_MAX;bool good=false,retired=false,published=false;
    uint8_t record[HTTP_CHILD_GO_BYTES+1]={0};size_t bytes=HTTP_CHILD_GO_BYTES;
    console_write("http-child-test: preparing actual /httpget preflight ELF; no sockets or input needed\n");
    if(!http_child_prepare(o,true))goto finish;
    uint64_t start=timer_monotonic_us();if(start>INT64_MAX-HTTP_CHILD_WINDOW_US)goto finish;
    /* token is a preflight identity, never a provider epoch/RF permission. */
    if(!variant)published=http_child_publish(o,o->token,start,start+HTTP_CHILD_WINDOW_US,UINT32_C(0xc0000201));
    else {
        if(!http_child_make_go(o,o->token,start,start+HTTP_CHILD_WINDOW_US,UINT32_C(0xc0000201),record))goto finish;
        if(variant==1)put(record+16,o->pid+1,8);
        if(variant==2)bytes=HTTP_CHILD_GO_BYTES-1;
        if(variant==3)bytes=HTTP_CHILD_GO_BYTES+1;
        published=process_pipe_handoff_publish(&o->pipe,record,bytes)==(int64_t)bytes;
    }
    if(!published)goto finish;
    console_write("http-child-test: READY bound; numeric GO and real pipe EOF published\n");
    good=child_exit(o,start+HTTP_CHILD_WINDOW_US,&status) && status==expected;
finish:
    retired=retire(o);
    for(unsigned i=0;i<sizeof record;i++)record[i]=0;
    console_printf("http-child-case: variant=%u complete=%u ready=%u go=%u status=%u expected=%u retired=%u sockets=0 native_wifi=0\n",
        (uint64_t)variant,(uint64_t)(good && retired),(uint64_t)o->prepared,
        (uint64_t)published,status,expected,(uint64_t)retired);
    return good && retired;
}
bool http_child_preflight_run(void) {
    if(!enabled || running || issued || process_current()) {
        console_write("http-child-test: REFUSED explicit isolated-init permit required or finite tickets exhausted\n");return false;
    }
    running=true;const uint64_t expected[PREFLIGHT_LIMIT]={0,16,15,15};bool good=true;
    for(unsigned i=0;i<PREFLIGHT_LIMIT;i++)if(!one(i,expected[i])) {good=false;break;}
    running=false;
    if(good)console_write("http-child-test: PASS actual ELF READY numericGO EOF exit reap lifetime cases=4; network untested\n");
    else console_write("http-child-test: FAIL no process/network acceptance; retained ownership if retirement incomplete\n");
    return good;
}
