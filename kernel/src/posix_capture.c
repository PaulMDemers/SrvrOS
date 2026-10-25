#include <srvros/posix_capture.h>
#include <srvros/process.h>
#include <srvros/console.h>
#include <srvros/timer.h>
#include <srvros/scheduler.h>
#include <stdint.h>

/* Tests write to the disposable RAM root. Only the existing desktop screenshot
 * and workspace logger publish new files to the selected Home volume. */
static const struct capture_case {
    const char *name, *path, *args;
    uint64_t seconds;
} cases[] = {
    {"threads", "/fat/bin/threadstress", "", 180},
    {"posix", "/fat/bin/posixdemo", "", 90},
    {"tls", "/fat/bin/tlsprobe", "", 60},
    {"sqlite", "/fat/bin/sqlitedemo", "", 60},
    {"desktop", "/fat/bin/deskd", "--idle-capture", 120},
};

static bool tick_due(void *arg) { return timer_ticks() >= *(uint64_t *)arg; }
static bool wait_tick(void) {
    static struct scheduler_wait_queue wake;
    uint64_t due=timer_ticks()+1;
    return scheduler_wait_timeout(&wake,tick_due,&due,due);
}

static bool retire(uint64_t token) {
    if (!process_lifetime_revoke(token)) return false;
    uint64_t until = timer_monotonic_us() + 10000000;
    do {
        struct process_lifetime_status state;
        if (!process_lifetime_drain(token) || !process_lifetime_status(token, &state)) return false;
        if (!state.members) return process_lifetime_release(token);
        if(!wait_tick()) return false;
    } while (timer_monotonic_us() < until);
    return false; /* Retain ownership; never release live descendants. */
}

bool posix_capture_run(void) {
    enum { COUNT = sizeof(cases) / sizeof(cases[0]) };
    struct { uint64_t status, elapsed; bool exited, retired; } results[COUNT] = {0};
    unsigned ran = 0;
    bool ok = true;
    console_write("posix-capture: begin version=1 tests=5; automatic, no input required\n");
    for (unsigned i = 0; i < COUNT; i++) {
        const struct capture_case *test = &cases[i];
        uint64_t begin = timer_monotonic_us();
        console_printf("posix-capture: starting name=%s deadline_seconds=%u\n", test->name, test->seconds);
        uint64_t token = process_lifetime_create();
        int64_t pid = token ? process_lifetime_spawn(token, test->path, test->args) : -1;
        results[i].status = UINT64_MAX;
        if (pid > 0) {
            do {
                int64_t waited = process_wait((uint64_t)pid, &results[i].status, true);
                if (waited == pid) { results[i].exited = true; break; }
                if (waited < 0) break;
                if(!wait_tick()) break;
            } while (timer_monotonic_us() - begin < test->seconds * 1000000);
        }
        results[i].retired = token && retire(token);
        results[i].elapsed = timer_monotonic_us() - begin;
        ran++;
        ok = results[i].exited && results[i].status == 0 && results[i].retired;
        console_printf("posix-capture: finished name=%s ok=%s\n", test->name, ok ? "yes" : "NO");
        if (!ok) break;
    }
    /* Repeat compact receipts at the end so a verbose child cannot push earlier
     * outcomes out of the logger's bounded tail. Missing entries never pass. */
    for (unsigned i = 0; i < ran; i++)
        console_printf("posix-capture: case=%s exited=%u status=%u retired=%u elapsed_us=%u\n",
            cases[i].name, (uint64_t)results[i].exited, results[i].status,
            (uint64_t)results[i].retired, results[i].elapsed);
    console_printf("posix-capture: complete=%s tests=%u expected=%u\n",
        ok && ran == COUNT ? "yes" : "NO", (uint64_t)ran, (uint64_t)COUNT);
    return ok && ran == COUNT;
}
