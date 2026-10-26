#include <errno.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("threadstress: times line=%d errno=%d\n", __LINE__, errno); return -1; } } while (0)
static clock_t elapsed_ticks(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value)) return -1;
    return value.tv_sec*100 + value.tv_nsec/10000000;
}
int posix_times_test(void) {
    CHECK(sysconf(_SC_CLK_TCK) == 100);
    struct tms first, last;
    clock_t lower = elapsed_ticks();
    errno = EDOM;
    clock_t start = times(&first);
    CHECK(errno == EDOM);
    clock_t upper = elapsed_ticks();
    CHECK(lower >= 0 && start >= lower && start <= upper);
    CHECK(usleep(100000) == 0);
    lower = elapsed_ticks(); errno = EDOM;
    clock_t end = times(&last);
    CHECK(errno == EDOM);
    upper = elapsed_ticks();
    CHECK(end >= lower && end <= upper && end-start >= 10);
    CHECK(last.tms_utime >= first.tms_utime && last.tms_stime >= first.tms_stime);
    CHECK(last.tms_cutime >= first.tms_cutime && last.tms_cstime >= first.tms_cstime);
    lower = elapsed_ticks(); errno = EDOM;
    clock_t no_output = times(NULL); /* Existing SrvrOS extension. */
    CHECK(errno == EDOM);
    upper = elapsed_ticks();
    CHECK(no_output >= lower && no_output <= upper);
    printf("threadstress: times source=%s elapsed=%ld\n", srv_monotonic_us() >= 0 ? "reference" : "ticks", end-start);
    puts("threadstress: times ok");
    return 0;
}
