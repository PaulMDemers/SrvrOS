#define SRVROS_ACPI_WORKER_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../ports/srvros/uacpi/runtime/work_worker.c"

uint64_t acpi_worker_test_flags = 1ull << 9;
static uint64_t now, identity = 1;
static bool spawn_ok = true, wait_ok = true, clock_ok = true;
static scheduler_thread_fn spawned;
static void *spawn_arg;
static struct acpi_work_worker worker;
static struct acpi_work_item *item;
static unsigned callbacks, yields, waits;
static bool finish_on_wait, stop_on_yield;

uint64_t scheduler_current_thread_id(void) { return identity; }
bool scheduler_spawn(const char *name, scheduler_thread_fn fn, void *arg) {
    assert(name && !(acpi_worker_test_flags & (1ull << 9)));
    spawned = fn; spawn_arg = arg; return spawn_ok;
}
void scheduler_wake_all(struct scheduler_wait_queue *queue) {
    assert(queue && !(acpi_worker_test_flags & (1ull << 9)));
}
bool timer_reference_ready(void) { return clock_ok; }
uint64_t timer_monotonic_us(void) { return now; }
uint64_t timer_ticks(void) { return now / 10000; }
bool scheduler_wait_timeout(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition, void *arg, uint64_t due) {
    assert(queue && (acpi_worker_test_flags & (1ull << 9)) && due > timer_ticks());
    waits++;
    if (!wait_ok) return false;
    if (finish_on_wait) {
        struct acpi_work_call call;
        assert(acpi_work_take(&worker.work, &call));
        assert(acpi_work_complete(&worker.work));
        finish_on_wait = false;
        assert(condition(arg));
    }
    now += 10000;
    return true;
}
bool scheduler_wait(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition, void *arg) {
    (void)queue; (void)condition; (void)arg;
    assert(!"test worker should always have pending work or stop request");
    return false;
}
void scheduler_yield(void) {
    assert(acpi_worker_test_flags & (1ull << 9));
    yields++;
    if (stop_on_yield && acpi_work_idle(&worker.work)) {
        identity = 1;
        assert(!acpi_worker_stop(&worker, 0)); /* Asked; worker must publish exit. */
        identity = 2;
    }
}
static void noop(void *arg) { (void)arg; }
static void callback(void *arg) {
    assert(arg == &worker && (acpi_worker_test_flags & (1ull << 9)));
    assert(!acpi_worker_drain(&worker, 0) && !acpi_worker_stop(&worker, 0));
    callbacks++;
    if (callbacks < 1000) {
        assert(acpi_worker_enqueue(&worker, item, callbacks & 1 ?
            UACPI_WORK_NOTIFICATION : UACPI_WORK_GPE_EXECUTION, callback, &worker));
    } else {
        free(item); item = NULL; /* ASan checks no worker access after callback. */
    }
}
int main(void) {
    struct acpi_work_item pending = {0};
    assert(!acpi_worker_enqueue(&worker, &pending, UACPI_WORK_NOTIFICATION, noop, NULL));
    assert(!acpi_worker_drain(&worker, 0) && !acpi_worker_stop(&worker, 0));
    identity = 0; assert(!acpi_worker_start(&worker)); identity = 1;
    spawn_ok = false; assert(!acpi_worker_start(&worker) && !worker.started);
    spawn_ok = true; assert(acpi_worker_start(&worker) && !acpi_worker_start(&worker));
    assert(acpi_worker_drain(&worker, 0));
    acpi_worker_test_flags = 0;
    assert(acpi_worker_enqueue(&worker, &pending, UACPI_WORK_NOTIFICATION, noop, NULL));
    assert(!acpi_worker_test_flags); /* IRQ producer cannot re-enable interrupts. */
    assert(!acpi_worker_enqueue(&worker, &pending, UACPI_WORK_NOTIFICATION, noop, NULL));
    assert(!acpi_worker_drain(&worker, 100) && !waits);
    acpi_worker_test_flags = 1ull << 9;
    assert(!acpi_worker_stop(&worker, 0) && !worker.stopping);
    assert(!acpi_worker_drain(&worker, 0) && !waits);
    assert(!acpi_worker_drain(&worker, UINT64_MAX) && !waits);
    now = UINT64_MAX - 100; assert(!acpi_worker_drain(&worker, 1) && !waits); now = 0;
    clock_ok = false; assert(!acpi_worker_drain(&worker, 1) && !waits); clock_ok = true;
    assert(!acpi_worker_drain(&worker, 15) && now >= 15000);
    wait_ok = false; assert(!acpi_worker_drain(&worker, 100)); wait_ok = true;
    finish_on_wait = true; assert(acpi_worker_drain(&worker, 100));
    item = calloc(1, sizeof(*item)); assert(item);
    assert(acpi_worker_enqueue(&worker, item, UACPI_WORK_GPE_EXECUTION, callback, &worker));
    stop_on_yield = true; identity = 2; spawned(spawn_arg); identity = 1;
    assert(callbacks == 1000 && yields == 1000 && worker.completed == 1000 && !item);
    assert(acpi_worker_stop(&worker, 0));
    assert(!acpi_worker_enqueue(&worker, &pending, UACPI_WORK_NOTIFICATION, noop, NULL));
    assert(acpi_worker_start(&worker) && !worker.stopped && !worker.completed);
    assert(!acpi_worker_stop(&worker, 0));
    identity = 3; spawned(spawn_arg); identity = 1;
    assert(acpi_worker_stop(&worker, 0));
    assert(acpi_worker_test_flags == (1ull << 9));
    puts("acpi-worker: PASS IRQ admission, duplicate refusal, timeout/overflow/failure, self-reschedule/free, yielding, stop/restart and flags");
}
