/* Host throughput comparison using the actual console and a RAM framebuffer.
 * This is deliberately not a laptop/device-memory performance prediction. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <srvros/workqueue.h>
#define SRVROS_CONSOLE_HOST_TEST
#ifdef SRVROS_BENCH_BASELINE
#include "baseline_console.c"
#else
#include "../kernel/src/console.c"
#endif
void serial_putc(char c) { (void)c; }
void bootlog_putc(char c) { (void)c; }
static workqueue_fn pending;
bool workqueue_schedule_io_safe(workqueue_fn fn, void *arg) { (void)arg; pending=fn; return true; }
bool workqueue_schedule(workqueue_fn fn, void *arg) { return workqueue_schedule_io_safe(fn, arg); }
void scheduler_yield(void) {}
static uint64_t nanos(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;
}
int main(void) {
    struct limine_framebuffer f={.address=calloc(1440*900,4),.width=1440,.height=900,
        .pitch=1440*4,.bpp=32,.memory_model=LIMINE_FRAMEBUFFER_RGB,
        .red_mask_size=8,.red_mask_shift=16,.green_mask_size=8,.green_mask_shift=8,.blue_mask_size=8};
    if(!f.address) return 1;
    console_init(&f);
#ifndef SRVROS_BENCH_BASELINE
    console_enable_deferred();
    struct console_stats before = stats;
#endif
    uint64_t start=nanos();
    for(unsigned i=0;i<1000;i++)
        console_write("AaZz 0123456789 []{} !@#$%^&*() The quick brown fox jumps over the lazy dog. 123\n");
    while(pending) { workqueue_fn fn=pending; pending=NULL; fn(NULL); }
    uint64_t elapsed=nanos()-start;
    printf("1000x80 1440x900 RAM framebuffer serial disabled: %llu us",(unsigned long long)(elapsed/1000));
#ifndef SRVROS_BENCH_BASELINE
    printf(" presents=%llu fb_write_bytes=%llu", (unsigned long long)(stats.presents-before.presents),
        (unsigned long long)(stats.framebuffer_bytes_written-before.framebuffer_bytes_written));
#endif
    puts(""); free(f.address);
}
