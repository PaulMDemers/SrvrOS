/* Explicit known-failure audit, excluded from the positive all-tests suite. */
#include <errno.h>
#include <stdio.h>
#include <time.h>

int posix_clock_resolution_audit(void) {
    const int clocks[] = {CLOCK_REALTIME, CLOCK_MONOTONIC,
        CLOCK_PROCESS_CPUTIME_ID, CLOCK_THREAD_CPUTIME_ID};
    unsigned failures = 0;
    for (unsigned i = 0; i < sizeof(clocks) / sizeof(*clocks); i++) {
        struct timespec resolution = {0};
        int stored = clock_getres(clocks[i], &resolution);
        errno = 0;
        int probe = clock_getres(clocks[i], NULL);
        printf("threadstress: clock-resolution-audit clock=%d stored=%d null=%d errno=%d\n",
            clocks[i], stored, probe, errno);
        if (stored != 0 || probe != 0) failures++;
    }
    errno = 0;
    if (clock_getres(99, NULL) != -1 || errno != EINVAL) failures++;
    if (failures) { puts("threadstress: clock-resolution-audit failed"); return 1; }
    puts("threadstress: clock-resolution-audit ok\nthreadstress: ok");
    return 0;
}
