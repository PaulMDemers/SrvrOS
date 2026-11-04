/* Force a real GOT entry and exercise initialized and zero TLS per thread. */
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

static _Thread_local int zero_tls[16];
#ifdef INITIALIZED_TLS
static _Thread_local int initialized_tls = 0x12345678;
#endif
int tls_got_global = 42;
static int errors;
static void *worker(void *unused) {
    (void)unused;
    if (zero_tls[0] != 0) ++errors;
#ifdef INITIALIZED_TLS
    if (initialized_tls != 0x12345678) ++errors;
    initialized_tls = 123;
#endif
    zero_tls[0] = 5;
    return 0;
}
int main(void) {
    void *address;
    __asm__ volatile ("mov tls_got_global@GOTPCREL(%%rip), %0" : "=r"(address));
    if (address != &tls_got_global || *(int *)address != 42 || zero_tls[0] != 0) return 1;
#ifdef INITIALIZED_TLS
    if (initialized_tls != 0x12345678) return 2;
    initialized_tls = 99;
#endif
    zero_tls[0] = 7;
    pthread_t thread;
    if (pthread_create(&thread, 0, worker, 0) || pthread_join(thread, 0)) return 3;
    if (errors || zero_tls[0] != 7) return 4;
#ifdef INITIALIZED_TLS
    if (initialized_tls != 99) return 5;
#endif
    puts("tls-got: ok");
    return 0;
}
