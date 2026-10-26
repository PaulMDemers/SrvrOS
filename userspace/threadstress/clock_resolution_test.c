#include <errno.h>
#include <stdio.h>
#include <time.h>

#define CHECK(x) do { if (!(x)) { printf("threadstress: clock-resolution line=%d errno=%d\n", __LINE__, errno); return -1; } } while (0)
int posix_clock_resolution_test(void) {
    const int clocks[] = {CLOCK_REALTIME, CLOCK_MONOTONIC,
        CLOCK_PROCESS_CPUTIME_ID, CLOCK_THREAD_CPUTIME_ID};
    for (unsigned i = 0; i < sizeof(clocks) / sizeof(*clocks); i++) {
        struct timespec resolution = {.tv_sec = -7, .tv_nsec = -9};
        errno = EDOM;
        CHECK(clock_getres(clocks[i], &resolution) == 0 && errno == EDOM);
        CHECK(resolution.tv_sec >= 0 && resolution.tv_nsec >= 0 && resolution.tv_nsec < 1000000000L);
        CHECK(resolution.tv_sec > 0 || resolution.tv_nsec > 0);
        CHECK(clock_getres(clocks[i], NULL) == 0 && errno == EDOM);
    }
    const int invalid[] = {-1, 99};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        struct timespec resolution = {.tv_sec = 7, .tv_nsec = 9};
        errno = 0;
        CHECK(clock_getres(invalid[i], &resolution) == -1 && errno == EINVAL);
        CHECK(resolution.tv_sec == 7 && resolution.tv_nsec == 9);
        errno = 0;
        CHECK(clock_getres(invalid[i], NULL) == -1 && errno == EINVAL);
    }
    puts("threadstress: clock-resolution ok");
    return 0;
}
