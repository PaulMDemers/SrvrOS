#include <errno.h>

/* The ELF loader creates a separate TLS block for the main thread and every
 * user thread. No allocation or pthread-key bootstrap is needed here. */
static _Thread_local int srvros_errno;

int *__errno_location(void) {
    return &srvros_errno;
}
