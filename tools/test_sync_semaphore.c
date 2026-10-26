/* Compile the real semaphore adapter with a deterministic atomic boundary.
 * Inject another thread's post/consume immediately before its first CAS. */
#include <stdbool.h>
unsigned sync_sem_cas_posts, sync_sem_cas_consume;
static bool sync_sem_compare_exchange(unsigned *word, unsigned *expected,
    unsigned desired, bool weak, int success, int failure) {
    if (sync_sem_cas_posts) {
        sync_sem_cas_posts--;
        __atomic_add_fetch(word, 1, __ATOMIC_RELEASE);
    } else if (sync_sem_cas_consume) {
        sync_sem_cas_consume=0;
        __atomic_store_n(word, 0, __ATOMIC_RELEASE);
    }
    return __atomic_compare_exchange_n(word, expected, desired, weak, success, failure);
}
#define __atomic_compare_exchange_n sync_sem_compare_exchange
#include "../userspace/lib/src/posix_semaphore.c"
#undef __atomic_compare_exchange_n
