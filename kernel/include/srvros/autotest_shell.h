#ifndef SRVROS_AUTOTEST_SHELL_H
#define SRVROS_AUTOTEST_SHELL_H
#include <stdbool.h>
#include <stdint.h>
struct block_device;
struct autotest_shell_stats {
    uint64_t elapsed_us, input_poll_count, input_poll_max_us, tick_max_us;
};
bool autotest_shell_run(struct block_device *device, const char *run,
    struct autotest_shell_stats *stats);
#endif
