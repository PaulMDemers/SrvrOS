/* Real target GLib GChildWatchSource, using the native spawn/wait interfaces. */
#include <glib.h>
#include <glib-unix.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <srvros/sys.h>

#define CHECK(phase, condition) do { \
    if (!(condition)) { \
        printf("glib-linked-child-watch: failure %s line=%d errno=%d\n", (phase), __LINE__, errno); \
        return 1; \
    } \
} while (0)

struct watch_result {
    GPid observed;
    gint status;
    unsigned callbacks;
};

static void watched_exited(GPid pid, gint status, gpointer data) {
    struct watch_result *result = data;
    result->observed = pid;
    result->status = status;
    result->callbacks++;
}

static gboolean watch_timeout(gpointer data) {
    gboolean *expired = data;
    *expired = TRUE;
    return G_SOURCE_REMOVE;
}

static int64_t now_ms(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) return -1;
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static int child(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "unwatched") == 0) return 42;
    if (argc != 4 || strcmp(argv[1], "watched") != 0) return 90;
    int read_fd = atoi(argv[2]), write_fd = atoi(argv[3]);
    if (read_fd < 0 || write_fd < 0 || close(write_fd) != 0) return 91;
    struct pollfd p = { .fd = read_fd, .events = POLLIN };
    int result;
    do { result = poll(&p, 1, 4000); } while (result < 0 && errno == EINTR);
    if (result != 1 || !(p.revents & POLLIN)) return 92;
    char byte = 0;
    if (read(read_fd, &byte, 1) != 1 || byte != 'g' || close(read_fd) != 0) return 93;
    return 17;
}

int main(int argc, char **argv) {
    if (argc > 1) return child(argc, argv);
    CHECK("argc", argc == 1);

    _Static_assert(_Generic((GPid *)0, pid_t *: 1, default: 0), "GPid must match pid_t");
    GPid large = (GPid)UINT64_C(4294967297);
    gchar *formatted = g_strdup_printf("%" G_PID_FORMAT, large);
    CHECK("pid-format", formatted != NULL && strcmp(formatted, "4294967297") == 0);
    g_free(formatted);

    int fd = open("/init", O_RDONLY);
    CHECK("fd-open", fd >= 0);
    GError *error = NULL;
    gchar *path = g_unix_fd_query_path(fd, &error);
    CHECK("fd-nosys", path == NULL && error != NULL && error->domain == G_FILE_ERROR &&
        error->code == G_FILE_ERROR_NOSYS);
    g_clear_error(&error);
    CHECK("fd-nosys-ignored", g_unix_fd_query_path(fd, NULL) == NULL);
    CHECK("fd-close", close(fd) == 0);

    int gate[2];
    CHECK("gate-pipe", pipe(gate) == 0);
    CHECK("gate-inherited", (fcntl(gate[0], F_GETFD) & FD_CLOEXEC) == 0 &&
        (fcntl(gate[1], F_GETFD) & FD_CLOEXEC) == 0);
    char *environment[] = { NULL };
    char *other_args[] = { "/glib-linked-child-watch", "unwatched", NULL };
    pid_t other = 0;
    CHECK("spawn-other", posix_spawn(&other, other_args[0], NULL, NULL, other_args, environment) == 0 && other > 0);
    int64_t deadline = now_ms();
    CHECK("clock-other", deadline >= 0);
    deadline += 5000;
    int exited = 0;
    for (unsigned attempt = 0; attempt < 1000; ++attempt) {
        struct srv_process_info info = { .abi_version = SRV_ABI_VERSION, .struct_size = sizeof(info) };
        long result = srv_proc_list(SRV_PROC_LIST_BY_PID | (uint64_t)other, &info);
        if (result > 0 && info.pid == (uint64_t)other && info.parent_pid == (uint64_t)getpid() &&
            strcmp(info.state, "exited") == 0) { exited = 1; break; }
        int64_t now = now_ms();
        if (now < 0 || now >= deadline) break;
        (void)poll(NULL, 0, 10);
    }
    CHECK("other-exited-not-reaped", exited);

    char read_arg[24], write_arg[24];
    CHECK("gate-format", snprintf(read_arg, sizeof(read_arg), "%d", gate[0]) > 0 &&
        snprintf(write_arg, sizeof(write_arg), "%d", gate[1]) > 0);
    char *watch_args[] = { "/glib-linked-child-watch", "watched", read_arg, write_arg, NULL };
    GPid watched = 0;
    CHECK("spawn-watched", posix_spawn(&watched, watch_args[0], NULL, NULL, watch_args, environment) == 0 && watched > 0 && watched != other);
    CHECK("gate-close-read", close(gate[0]) == 0);
    struct watch_result result = {0};
    GMainContext *context = g_main_context_new();
    CHECK("context-create", context != NULL);
    GSource *watch = g_child_watch_source_new(watched);
    CHECK("watch-create", watch != NULL);
    g_source_set_callback(watch, G_SOURCE_FUNC(watched_exited), &result, NULL);
    CHECK("watch-attach", g_source_attach(watch, context) != 0);
    /* Consume the initial maybe-exited check while the child is held at the gate. */
    (void)g_main_context_iteration(context, FALSE);
    CHECK("no-early-callback", result.callbacks == 0);
    CHECK("gate-release", write(gate[1], "g", 1) == 1 && close(gate[1]) == 0);

    gboolean expired = FALSE;
    GSource *timer = g_timeout_source_new(5000);
    CHECK("timer-create", timer != NULL);
    g_source_set_callback(timer, watch_timeout, &expired, NULL);
    CHECK("timer-attach", g_source_attach(timer, context) != 0);
    while (result.callbacks == 0 && !expired)
        (void)g_main_context_iteration(context, TRUE);
    CHECK("callback", !expired && result.callbacks == 1 && result.observed == watched &&
        WIFEXITED(result.status) && WEXITSTATUS(result.status) == 17);
    (void)g_main_context_iteration(context, FALSE);
    CHECK("callback-once", result.callbacks == 1);
    int status = 0;
    errno = 0;
    CHECK("watched-reaped", waitpid(watched, &status, WNOHANG) == -1 && errno == ECHILD);
    CHECK("unwatched-reap", waitpid(other, &status, WNOHANG) == other &&
        WIFEXITED(status) && WEXITSTATUS(status) == 42);
    g_source_destroy(timer);
    g_source_unref(timer);
    g_source_destroy(watch);
    g_source_unref(watch);
    g_main_context_unref(context);
    puts("glib-linked-child-watch: ok");
    return 0;
}
