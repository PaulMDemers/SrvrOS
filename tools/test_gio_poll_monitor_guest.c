/* Exercise the real GIO polling fallback on the private exFAT test volume. */
#include <gio/gio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <srvros/sys.h>

#define DIR "/fat/gio-poll-monitor"
#define PATH DIR "/watch"
#define CHECK(phase, expr) do { if (!(expr)) { \
    printf("gio-poll-monitor: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
    return 1; \
} } while (0)

struct events { unsigned created, changed, deleted, done, other; };

static void changed(GFileMonitor *monitor, GFile *file, GFile *other,
                    GFileMonitorEvent event, gpointer data) {
    struct events *e = data;
    (void)monitor; (void)file; (void)other;
    switch (event) {
    case G_FILE_MONITOR_EVENT_CREATED: ++e->created; break;
    case G_FILE_MONITOR_EVENT_CHANGED: ++e->changed; break;
    case G_FILE_MONITOR_EVENT_DELETED: ++e->deleted; break;
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT: ++e->done; break;
    default: ++e->other; break;
    }
}

static int64_t now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -1;
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int pump(int64_t duration_ms, const unsigned *first, unsigned first_target,
                const unsigned *second, unsigned second_target) {
    int64_t start = now_ms();
    if (start < 0) return 0;
    for (;;) {
        while (g_main_context_iteration(NULL, FALSE)) { }
        if (first && *first >= first_target && (!second || *second >= second_target)) return 1;
        int64_t now = now_ms();
        if (now < start) return 0;
        if (now - start >= duration_ms) return first == NULL;
        struct timespec pause = { .tv_sec = 0, .tv_nsec = 20000000 };
        if (nanosleep(&pause, NULL) != 0 && errno != EINTR) return 0;
    }
}

static int write_file(const char *text, int create) {
    int fd = open(PATH, O_WRONLY | (create ? O_CREAT | O_EXCL : 0), 0600);
    if (fd < 0) return 0;
    size_t length = strlen(text), done = 0;
    while (done < length) {
        ssize_t n = write(fd, text + done, length - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(fd); return 0; }
        done += (size_t)n;
    }
    return close(fd) == 0;
}

int main(void) {
    CHECK("mkdir-exclusive", mkdir(DIR, 0700) == 0);
    struct srv_fsinfo fs = {0};
    CHECK("exfat-mount", srv_statfs(DIR, &fs) == 0 &&
          strcmp(fs.filesystem, "exfat") == 0 && strcmp(fs.mountpoint, "/fat") == 0);
    GFile *file = g_file_new_for_path(PATH);
    CHECK("local-file-object", file && strcmp(G_OBJECT_TYPE_NAME(file), "GLocalFile") == 0);
    GError *error = NULL;
    GFile *directory = g_file_new_for_path(DIR);
    CHECK("directory-object", directory != NULL);
    GFileMonitor *dir_monitor = g_file_monitor_directory(directory, G_FILE_MONITOR_NONE, NULL, &error);
    CHECK("directory-unsupported", dir_monitor == NULL && error != NULL &&
          error->domain == G_IO_ERROR && error->code == G_IO_ERROR_FAILED);
    g_clear_error(&error);
    GFileMonitor *monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, NULL, &error);
    CHECK("monitor-create", monitor != NULL && error == NULL);
    CHECK("poll-backend", strcmp(G_OBJECT_TYPE_NAME(monitor), "GPollFileMonitor") == 0);
    struct events events = {0};
    g_signal_connect(monitor, "changed", G_CALLBACK(changed), &events);
    CHECK("initial-settle", pump(7000, NULL, 0, NULL, 0));
    CHECK("initial-quiet", events.created == 0 && events.changed == 0 &&
          events.deleted == 0 && events.done == 0 && events.other == 0);
    CHECK("create-file", write_file("first data\n", 1));
    CHECK("create-event", pump(15000, &events.created, 1, &events.done, 1));
    struct stat initial, updated;
    CHECK("initial-stat", stat(PATH, &initial) == 0);
    int64_t start = now_ms();
    CHECK("rewrite-clock", start >= 0);
    while (time(NULL) <= initial.st_mtime) {
        CHECK("rewrite-wait", pump(250, NULL, 0, NULL, 0));
        CHECK("rewrite-deadline", now_ms() >= start && now_ms() - start < 10000);
    }
    do {
        CHECK("rewrite-file", write_file("other data\n", 0));
        CHECK("rewrite-stat", stat(PATH, &updated) == 0);
        CHECK("equal-size", updated.st_size == initial.st_size);
        if (updated.st_mtime > initial.st_mtime) break;
        CHECK("rewrite-wait", pump(250, NULL, 0, NULL, 0));
        CHECK("rewrite-deadline", now_ms() >= start && now_ms() - start < 10000);
    } while (1);
    CHECK("rewrite-event", pump(15000, &events.changed, 1, &events.done, 2));
    CHECK("unlink", unlink(PATH) == 0);
    CHECK("delete-event", pump(15000, &events.deleted, 1, NULL, 0));
    CHECK("events", events.created == 1 && events.changed == 1 && events.deleted == 1 &&
          events.done == 2 && events.other == 0);
    CHECK("cancel", g_file_monitor_cancel(monitor));
    CHECK("cancelled-state", g_file_monitor_is_cancelled(monitor));
    struct events before = events;
    CHECK("recreate", write_file("after stop\n", 1));
    CHECK("cancel-wait", pump(7000, NULL, 0, NULL, 0));
    CHECK("cancel-quiet", memcmp(&before, &events, sizeof(events)) == 0);
    CHECK("cleanup-file", unlink(PATH) == 0);
    g_object_unref(monitor);
    g_object_unref(file);
    g_object_unref(directory);
    CHECK("cleanup-dir", rmdir(DIR) == 0);
    printf("gio-poll-monitor: ok\n");
    return 0;
}
