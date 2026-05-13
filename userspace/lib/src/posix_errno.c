#include <errno.h>

static int srvros_errno;

int *__errno_location(void) {
    return &srvros_errno;
}
