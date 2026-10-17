#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_WORKQUEUE_HOST_TEST
#include "../kernel/src/workqueue.c"
static unsigned order[100], ran, wakes;
void console_printf(const char *fmt, ...) { (void)fmt; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { assert(q == &wait_queue); wakes++; }
static void simple(void *arg) { order[ran++] = (unsigned)(uintptr_t)arg; }
static void repeat(void *arg) { simple(arg); assert(workqueue_schedule(repeat,arg)); }
static void run_one(void) {
    workqueue_fn fn; void *arg;
    assert(take_work(&fn,&arg)); fn(arg); callback_active=false;
}
int main(void) {
    assert(!workqueue_schedule(simple,0));
    initialized=true; assert(!workqueue_schedule(simple,0));
    worker_started=true; assert(!workqueue_schedule(NULL,0));
    assert(workqueue_schedule(repeat,(void *)1));
    assert(workqueue_schedule(simple,(void *)2));
    run_one(); run_one();
    assert(order[0]==1 && order[1]==2); /* Requeued callback must not starve peer. */
    workqueue_fn fn; void *arg; assert(take_work(&fn,&arg) && fn==repeat);
    for(unsigned round=0;round<4;round++) {
        for(unsigned i=0;i<16;i++) assert(workqueue_schedule(simple,(void *)(uintptr_t)i));
        uint64_t old=coalesced_count;
        assert(workqueue_schedule(simple,(void *)5)); assert(coalesced_count==old+1);
        assert(!workqueue_schedule(repeat,0));
        for(unsigned i=0;i<16;i++) {
            assert(take_work(&fn,&arg) && fn==simple && (uintptr_t)arg==i);
        }
        assert(!work_pending(NULL) && !take_work(&fn,&arg));
    }
    assert(dropped_count==4 && wakes==67);
    callback_active=false;
    assert(!workqueue_io_service_ready(NULL));
    assert(workqueue_schedule(simple, (void *)1));
    assert(!workqueue_io_service_ready(NULL));
    assert(take_work(&fn, &arg) && !workqueue_io_service_ready(NULL));
    callback_active=false;
    assert(workqueue_schedule_io_safe(simple, (void *)2));
    assert(workqueue_io_service_ready(NULL));
    assert(take_work(&fn, &arg) && workqueue_io_service_ready(NULL));
    /* An unsafe item queued behind a safe active callback cannot become eligible. */
    assert(workqueue_schedule(simple, (void *)3));
    assert(workqueue_io_service_ready(NULL));
    callback_active=false; assert(!workqueue_io_service_ready(NULL));
    assert(take_work(&fn,&arg)); callback_active=false;
    assert(workqueue_schedule_io_safe(simple, (void *)4));
    assert(workqueue_schedule(simple, (void *)4));
    assert(!workqueue_io_service_ready(NULL)); /* Conservative duplicate downgrade. */
    puts("workqueue-host: PASS (FIFO fairness, coalescing, full queue, wrap, failed worker)");
}
