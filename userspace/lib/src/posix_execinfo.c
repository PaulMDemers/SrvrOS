#include <execinfo.h>
#include <stdio.h>
#include <unistd.h>

int backtrace(void **buffer, int size) {
    (void)buffer;
    (void)size;
    return 0;
}

char **backtrace_symbols(void *const *buffer, int size) {
    (void)buffer;
    (void)size;
    return 0;
}

void backtrace_symbols_fd(void *const *buffer, int size, int fd) {
    (void)buffer;
    (void)size;
    if (fd >= 0) {
        (void)write(fd, "backtrace unavailable\n", 22);
    }
}
