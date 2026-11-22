/* Host GLib behavior probe; does not qualify the SrvrOS port or GUI integration. */
#include <glib.h>
#include <errno.h>
#include <stdio.h>

struct probe {
    GMainContext *context;
    GThread *owner;
    gint idle_count;
    gint timer_count;
};

static gboolean idle_callback(gpointer data) {
    struct probe *p = data;
    g_assert_true(g_thread_self() == p->owner);
    p->idle_count++;
    return G_SOURCE_REMOVE;
}

static gboolean timer_callback(gpointer data) {
    struct probe *p = data;
    g_assert_true(g_thread_self() == p->owner);
    p->timer_count++;
    return G_SOURCE_REMOVE;
}

static gpointer attach_from_worker(gpointer data) {
    struct probe *p = data;
    g_usleep(30000);
    GSource *s = g_idle_source_new();
    g_source_set_callback(s, idle_callback, p, NULL);
    g_assert_cmpuint(g_source_attach(s, p->context), >, 0);
    g_source_unref(s);
    return NULL;
}

/* The caller owns the context. Mirror the documented prepare/query/poll/check
 * sequence; clamp only an otherwise longer wait to this fixture's deadline. */
static int cycle(GMainContext *context, int max_ms, gint64 *poll_us) {
    gint priority, timeout_ms;
    gboolean ready = g_main_context_prepare(context, &priority);
    int n = g_main_context_query(context, priority, &timeout_ms, NULL, 0);
    g_assert_cmpint(n, >=, 0);
    GPollFD *fds = g_new0(GPollFD, n > 0 ? n : 1);
    int actual = g_main_context_query(context, priority, &timeout_ms, fds, n);
    g_assert_cmpint(actual, ==, n);
    int wait_ms = ready ? 0 : timeout_ms < 0 || timeout_ms > max_ms ? max_ms : timeout_ms;
    gint64 start = g_get_monotonic_time();
    int ret = g_poll(fds, n, wait_ms);
    *poll_us = g_get_monotonic_time() - start;
    if (ret < 0) g_error("g_poll failed: %d", errno);
    gboolean dispatch = g_main_context_check(context, priority, fds, n);
    if (dispatch) g_main_context_dispatch(context);
    g_free(fds);
    return ret;
}

int main(void) {
    g_assert_cmpuint(glib_major_version, ==, 2);
    g_assert_cmpuint(glib_minor_version, ==, 90);
    g_assert_cmpuint(glib_micro_version, ==, 1);
    struct probe p = { .context = g_main_context_new(), .owner = g_thread_self() };
    g_assert_true(g_main_context_acquire(p.context));

    /* Drain any initial wake notification before measuring a quiet wait. */
    while (g_main_context_pending(p.context)) g_main_context_iteration(p.context, FALSE);
    gint64 quiet_us = 0;
    int quiet_ready = cycle(p.context, 80, &quiet_us);
    g_assert_cmpint(quiet_ready, ==, 0);
    g_assert_cmpint(quiet_us, >=, 30000);
    g_assert_cmpint(quiet_us, <, 1000000);

    GSource *timer = g_timeout_source_new(150);
    g_source_set_callback(timer, timer_callback, &p, NULL);
    g_assert_cmpuint(g_source_attach(timer, p.context), >, 0);
    g_source_unref(timer);
    GThread *worker = g_thread_new("glib-probe", attach_from_worker, &p);
    gint64 deadline = g_get_monotonic_time() + G_TIME_SPAN_SECOND * 3;
    unsigned cycles = 0, sleeping_cycles = 0;
    while ((!p.idle_count || !p.timer_count) && g_get_monotonic_time() < deadline) {
        gint64 elapsed = 0;
        cycle(p.context, 250, &elapsed);
        cycles++;
        if (elapsed >= 10000) sleeping_cycles++;
        g_assert_cmpuint(cycles, <, 100);
    }
    g_thread_join(worker);
    g_assert_cmpint(p.idle_count, ==, 1);
    g_assert_cmpint(p.timer_count, ==, 1);
    g_assert_cmpuint(sleeping_cycles, >=, 1);
    g_main_context_release(p.context);
    g_main_context_unref(p.context);
    printf("glib-main-context-host: 2.90.1 quiet_us=%" G_GINT64_FORMAT " cycles=%u sleeping=%u wake+timer ok\n", quiet_us, cycles, sleeping_cycles);
    return 0;
}
