/* Explicit monitor diagnostic; never part of ordinary desktop startup. */
#include <srvros/semaphore.h>
#include <srvros/console.h>
#include <srvros/timer.h>
#include <stddef.h>
static struct {
    struct kernel_semaphore gate,started,progress,done;
    uint64_t owner,worker;
    bool busy,worker_ok;
} test;
static void worker(void *arg) {
    (void)arg;test.worker=scheduler_current_thread_id();
    test.worker_ok=test.worker && test.worker!=test.owner;
    semaphore_signal(&test.started);
    test.worker_ok&=semaphore_wait(&test.gate,SEMAPHORE_WAIT_FOREVER)==SEMAPHORE_OK;
    semaphore_signal(&test.progress);
    test.worker_ok&=semaphore_wait(&test.gate,SEMAPHORE_WAIT_FOREVER)==SEMAPHORE_CLOSED;
    semaphore_signal(&test.done); /* No test-state access after publishing done. */
}
void semaphore_test_run(void) {
    if(test.busy) { console_write("semaphore-test: FAIL previous worker not drained\n");return; }
    test.busy=true;test.owner=scheduler_current_thread_id();test.worker=0;test.worker_ok=false;
    semaphore_init(&test.gate,0);semaphore_init(&test.started,0);
    semaphore_init(&test.progress,0);semaphore_init(&test.done,0);
    bool spawned=scheduler_spawn("semaphore-test",worker,NULL),ok=spawned;
    if(ok)ok=semaphore_wait(&test.started,1000)==SEMAPHORE_OK;
    if(ok)ok=semaphore_signal(&test.gate) && semaphore_wait(&test.progress,1000)==SEMAPHORE_OK;
    semaphore_close(&test.gate);
    bool drained=!spawned || semaphore_wait(&test.done,1000)==SEMAPHORE_OK;
    ok=ok && drained && test.worker_ok && semaphore_drained(&test.gate);
    struct kernel_semaphore empty;semaphore_init(&empty,0);
    uint64_t start=timer_monotonic_us();
    enum semaphore_result result=semaphore_wait(&empty,15);
    uint64_t elapsed=timer_monotonic_us()-start;
    semaphore_close(&empty);
    ok=ok && result==SEMAPHORE_TIMEOUT && elapsed>=15000 && semaphore_drained(&empty);
    if(drained)test.busy=false;
    console_printf("semaphore-test: %s owner=%u worker=%u timeout_us=%u drained=%s\n",ok?"PASS":"FAIL",test.owner,test.worker,elapsed,drained?"yes":"NO");
}
