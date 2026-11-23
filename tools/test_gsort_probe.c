/* Process-level sort interruption and output-error checks. */
#include <srvros/sys.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { \
    if (!(x)) { \
        printf("gsort-probe: FAIL line %d: %s\n", __LINE__, #x); \
        return 1; \
    } \
} while (0)

static char *env[] = {"PATH=/fat/bin", "LC_ALL=C", NULL};

static long start(char **argv, char **environment, int in, int out, int err) {
    struct srv_exec_request req = {
        .path = argv[0], .argv = argv, .envp = environment,
        .flags = SRV_EXEC_BACKGROUND, .stdin_fd = in, .stdout_fd = out,
        .stderr_fd = err, .process_group = UINT64_MAX
    };
    return srv_exec(&req);
}

static long wait_child(long pid, uint64_t *status) {
    for (int i = 0; i < 1500; i++) {
        long n = srv_wait(pid, status, SRV_WAIT_NOHANG);
        if (n) return n;
        srv_sleep_ticks(1);
    }
    return 0;
}

static int alive(long pid) {
    struct srv_process_info info;
    return srv_proc_list(SRV_PROC_LIST_BY_PID | (uint64_t)pid, &info) > 0 &&
        info.pid == (uint64_t)pid && strcmp(info.state, "exited");
}

static void kill_child(long pid) {
    uint64_t status;
    srv_kill_signal(pid, SRV_SIGNAL_KILL);
    wait_child(pid, &status);
}

static int retry(void) {
    char *args[] = {"/fat/bin/gsort", "/fat/gsort-test/tiny", NULL};
    int out = open("/fat/gsort-test/retry.out", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    CHECK(out >= 0);
    long pid = start(args, env, 0, out, 2);
    srv_close(out);
    CHECK(pid > 0);
    uint64_t status = ~0ULL;
    CHECK(wait_child(pid, &status) == pid && status == 0);
    return 0;
}

static int input_interrupt(void) {
    int p[2];
    CHECK(srv_pipe(p) == 0);
    char *args[] = {"/fat/bin/gsort", "-", NULL};
    long pid = start(args, env, p[0], 1, 2);
    srv_close(p[0]);
    CHECK(pid > 0);
    CHECK(srv_write(p[1], "z\n", 2) == 2);
    int progress = 0;
    for (int i = 0; i < 1500; i++) {
        struct srv_stat st;
        if (srv_fstat(p[1], &st) == 0 && st.size == 0 && alive(pid)) {
            progress = 1;
            break;
        }
        if (!alive(pid)) break;
        srv_sleep_ticks(1);
    }
    if (!progress) {
        kill_child(pid);
        srv_close(p[1]);
        CHECK(progress);
    }
    CHECK(srv_kill_signal(pid, SRV_SIGNAL_INT) == 0);
    uint64_t status = ~0ULL;
    long done = wait_child(pid, &status);
    srv_close(p[1]);
    printf("gsort-probe: held stdin consumed=2 wait=%ld status=%llu\n",
           done, (unsigned long long)status);
    CHECK(done == pid && status == 130);
    return retry();
}

static int repeat_interrupt(void) {
    int p[2];
    CHECK(srv_pipe(p) == 0);
    char *args[] = {"/fat/gsort-test/gsort-diagnostic", "/fat/gsort-test/bytes", NULL};
    char *diagnostic_env[] = {"PATH=/fat/bin", "LC_ALL=C", "SRV_GSORT_REPEAT=1", NULL};
    int out = open("/fat/gsort-test/repeat.out", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    CHECK(out >= 0);
    long pid = start(args, diagnostic_env, 0, out, p[1]);
    srv_close(out);
    srv_close(p[1]);
    CHECK(pid > 0);
    long flags = srv_fcntl(p[0], SRV_F_GETFL, 0);
    CHECK(flags >= 0 && srv_fcntl(p[0], SRV_F_SETFL, (uint64_t)flags | SRV_FD_NONBLOCK) >= 0);
    char buf[4096];
    size_t used = 0;
    int ready = 0;
    for (int i = 0; i < 1500 && used < sizeof(buf) - 1; i++) {
        long n = srv_read(p[0], buf + used, sizeof(buf) - 1 - used);
        if (n > 0) {
            used += (size_t)n;
            buf[used] = 0;
            if (strstr(buf, "GSORT_SORT_READY")) {
                ready = 1;
                break;
            }
        }
        if (!alive(pid)) break;
        srv_sleep_ticks(1);
    }
    if (!ready) {
        kill_child(pid);
        srv_close(p[0]);
        CHECK(ready);
    }
    CHECK(srv_kill_signal(pid, SRV_SIGNAL_INT) == 0);
    uint64_t status = ~0ULL;
    long done = wait_child(pid, &status);
    while (used < sizeof(buf) - 1) {
        long n = srv_read(p[0], buf + used, sizeof(buf) - 1 - used);
        if (n <= 0) break;
        used += (size_t)n;
    }
    buf[used] = 0;
    srv_close(p[0]);
    printf("gsort-probe: repeat ready=%d finished=%d wait=%ld status=%llu\n",
           ready, strstr(buf, "GSORT_SORT_FINISHED") != NULL, done,
           (unsigned long long)status);
    CHECK(done == pid && status == 130 && !strstr(buf, "GSORT_SORT_FINISHED"));
    return retry();
}

static int output_error(void) {
    char *args[] = {"/fat/bin/gsort", "/fat/gsort-test/tiny", NULL};
    int fd = open("/fat/gsort-test/tiny", O_RDONLY);
    CHECK(fd >= 0);
    long pid = start(args, env, 0, fd, 2);
    srv_close(fd);
    CHECK(pid > 0);
    uint64_t status = ~0ULL;
    long done = wait_child(pid, &status);
    printf("gsort-probe: readonly output wait=%ld status=%llu\n",
           done, (unsigned long long)status);
    CHECK(done == pid && status == 2);
    return retry();
}

int main(void) {
    CHECK(!input_interrupt());
    CHECK(!repeat_interrupt());
    CHECK(!output_error());
    puts("gsort-probe: PASS");
    return 0;
}
