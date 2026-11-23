/* Observe the diagnostic softmagic marker, interrupt the active matcher, and
 * check a real write failure. The marker is emitted within the rule path. */
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("file-magic-probe: FAIL line %d: %s\n", __LINE__, #x);                          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static char *env[] = {"PATH=/fat/bin", "LC_ALL=C", NULL};
static char *repeat_env[] = {"PATH=/fat/bin", "LC_ALL=C", "SRV_FILE_MAGIC_REPEAT=1", NULL};
static char *normal[] = {"/fat/bin/file-magic", "-b", "--", "/fat/file-magic-test/png", NULL};
static char *diagnostic[] = {"/fat/file-magic-test/file-magic-diagnostic", "-b", "--",
                             "/fat/file-magic-test/png", NULL};
static long start(char **argv, char **environment, int out, int err) {
    struct srv_exec_request req = {.path = argv[0],
                                   .argv = argv,
                                   .envp = environment,
                                   .flags = SRV_EXEC_BACKGROUND,
                                   .stdin_fd = 0,
                                   .stdout_fd = out,
                                   .stderr_fd = err,
                                   .process_group = UINT64_MAX};
    return srv_exec(&req);
}
static long wait_bounded(long pid, uint64_t *status) {
    for (int i = 0; i < 1500; i++) {
        long n = srv_wait((uint64_t)pid, status, SRV_WAIT_NOHANG);
        if (n)
            return n;
        srv_sleep_ticks(1);
    }
    return 0;
}
static int alive(long pid) {
    struct srv_process_info info;
    return srv_proc_list(SRV_PROC_LIST_BY_PID | (uint64_t)pid, &info) > 0 &&
           info.pid == (uint64_t)pid && strcmp(info.state, "exited");
}
static int interrupted(void) {
    int p[2];
    CHECK(srv_pipe(p) == 0);
    long pid = start(diagnostic, repeat_env, 1, p[1]);
    srv_close(p[1]);
    CHECK(pid > 0);
    char buf[128] = {0};
    size_t used = 0;
    int found = 0;
    for (int i = 0; i < 1500 && used < sizeof(buf) - 1; i++) {
        struct srv_pollfd poll = {.fd = p[0], .events = SRV_POLLIN};
        if (srv_poll(&poll, 1, 0) > 0 && (poll.revents & SRV_POLLIN)) {
            long n = srv_read(p[0], buf + used, sizeof(buf) - 1 - used);
            if (n > 0) {
                used += (size_t)n;
                buf[used] = 0;
                if (strstr(buf, "FILE_MAGIC_MATCH_READY\n")) {
                    found = 1;
                    break;
                }
            }
        }
        if (!alive(pid))
            break;
        srv_sleep_ticks(1);
    }
    uint64_t status = ~0ULL;
    long done = 0;
    int ok = found && alive(pid) && srv_kill_signal(pid, SRV_SIGNAL_INT) == 0;
    if (ok) {
        done = wait_bounded(pid, &status);
        if (done == pid) {
            /* With the child reaped there is no remaining writer. */
            long count = srv_read(p[0], buf + used, sizeof(buf) - 1 - used);
            if (count > 0) buf[used + (size_t)count] = 0;
        }
        ok = done == pid && status == 130 && !strstr(buf, "FILE_MAGIC_MATCH_FINISHED");
    }
    if (done != pid) {
        srv_kill_signal(pid, SRV_SIGNAL_KILL);
        wait_bounded(pid, &status);
    }
    srv_close(p[0]);
    printf("file-magic-probe: marker=%d wait=%ld status=%llu\n", found, done,
           (unsigned long long)status);
    CHECK(ok);
    return 0;
}
static int output_error(void) {
    int fd = (int)srv_open("/fat/file-magic-test/png");
    CHECK(fd >= 0);
    long pid = start(normal, env, fd, 2);
    srv_close(fd);
    CHECK(pid > 0);
    uint64_t status = ~0ULL;
    long done = wait_bounded(pid, &status);
    if (done != pid) {
        srv_kill_signal(pid, SRV_SIGNAL_KILL);
        wait_bounded(pid, &status);
    }
    printf("file-magic-probe: readonly output wait=%ld status=%llu\n", done,
           (unsigned long long)status);
    CHECK(done == pid && status == 2);
    return 0;
}
static int retry(void) {
    long pid = start(normal, env, 1, 2);
    CHECK(pid > 0);
    uint64_t status = ~0ULL;
    long done = wait_bounded(pid, &status);
    if (done != pid) {
        srv_kill_signal(pid, SRV_SIGNAL_KILL);
        wait_bounded(pid, &status);
    }
    CHECK(done == pid && status == 0);
    return 0;
}
int main(void) {
    CHECK(!output_error());
    CHECK(!interrupted());
    CHECK(!retry());
    puts("file-magic-probe: PASS");
    return 0;
}
