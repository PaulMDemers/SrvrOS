/* Real userspace shell/editor tests in an isolated per-run USB directory. */
#include <srvros/autotest_shell.h>
#include <srvros/block.h>
#include <srvros/console.h>
#include <srvros/exfat.h>
#include <srvros/latency.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <srvros/vfs.h>
#include <stddef.h>

static void join(char out[160], const char *a, const char *b) {
    unsigned n = 0;
    while (*a) out[n++] = *a++;
    while (*b) out[n++] = *b++;
    out[n] = 0; /* Validated 16-digit run plus fixed suffixes only. */
}
static bool put(const char *root, const char *name, const char *text) {
    char path[160]; join(path, root, name);
    unsigned n = 0; while (text[n]) n++;
    return exfat_write_file(path, (const uint8_t *)text, n);
}
static bool equal(const char *root, const char *name, const char *expected) {
    char path[160]; join(path, root, name);
    const struct vfs_node *node = vfs_lookup(path);
    unsigned n = 0; while (expected[n]) n++;
    if (!node || node->type != VFS_NODE_FILE || node->size != n) return false;
    const uint8_t *data; uint64_t size;
    if (!vfs_read_all(node, &data, &size)) return false;
    bool ok = size == n;
    for (unsigned i = 0; ok && i < n; i++) ok = data[i] == (uint8_t)expected[i];
    vfs_release_data(node, data); return ok;
}
static bool run_script(const char *root, const char *script) {
    char args[320], path[160];
    join(path, root, script);
    unsigned n = 0;
    const char *parts[] = {"--workspace ", root, " ", path};
    for (unsigned i = 0; i < 4; i++) for (const char *p = parts[i]; *p; p++) args[n++] = *p;
    args[n] = 0;
    int64_t pid = process_spawn_background_elf_args_quiet("/fat/bin/sh", args);
    if (pid < 1) return false;
    process_set_group((uint64_t)pid, (uint64_t)pid);
    uint64_t start = timer_monotonic_us();
    for (;;) {
        uint64_t status;
        int64_t waited = process_wait((uint64_t)pid, &status, true);
        if (waited != 0) return waited == pid && status == 0;
        if (timer_monotonic_us() - start >= 120000000ull) {
            process_signal_target(-pid, 9);
            console_write("autotest: shell deadline exceeded\n");
            return false;
        }
        scheduler_yield();
    }
}
static const char edited[] = "Edited AaZz\nKeep me\nAppended 123\n";
static const char driver[] =
    "set -e\n"
    "test \"$WORKSPACE_CHECK\" = persisted-settings\n"
    "echo \"$WORKSPACE_CHECK\" > settings.txt\n"
    "ed -s document.txt < edit.in\n"
    "sh --workspace \"$HOME\" < session.in\n"
    "dd if=/dev/zero of=load.bin bs=4096 count=128\n"
    "seq 1 500\n"
    "sync\n"
    "echo shell-first-pass > first.done\n";
static const char verify[] =
    "set -e\n"
    "test \"$WORKSPACE_CHECK\" = persisted-settings\n"
    "ed -s document.txt < view.in > reopened.txt\n"
    "cmp document.txt reopened.txt\n"
    "sh --workspace \"$HOME\" < recall.in > recalled.txt\n"
    "grep workspace-history-sentinel recalled.txt > history-proof.txt\n"
    "sync\n"
    "echo shell-remount-pass > verified.done\n";

bool autotest_shell_run(struct block_device *device, const char *run,
    struct autotest_shell_stats *stats) {
    *stats = (struct autotest_shell_stats){0};
    if (!run) return false;
    for (unsigned i = 0; i < 16; i++)
        if (!((run[i] >= '0' && run[i] <= '9') || (run[i] >= 'a' && run[i] <= 'f'))) return false;
    if (run[16]) return false;
    char root[160], prefix[160];
    join(prefix, "/testlog/srvros-tests/", run); join(root, prefix, ".shell");
    if (vfs_lookup(root) || !exfat_create_directory(root)) return false;
    if (!put(root, "/.profile", "WORKSPACE_CHECK=persisted-settings\nHISTSIZE=32\n") ||
        !put(root, "/document.txt", "Draft AaZz\nKeep me\n") ||
        !put(root, "/edit.in", "1s/Draft/Edited/\n$a\nAppended 123\n.\nw\nq\n") ||
        !put(root, "/view.in", "1,$p\nq\n") ||
        !put(root, "/session.in", "echo workspace-history-sentinel\nexit\n") ||
        !put(root, "/recall.in", "history\nexit\n") ||
        !put(root, "/driver.sh", driver) || !put(root, "/verify.sh", verify)) return false;
    if (!latency_reset()) return false;
    uint64_t start = timer_monotonic_us();
    bool ok = run_script(root, "/driver.sh");
    struct block_device *parent = block_parent(device);
    if (ok) ok = !process_has_open_vfs_prefix("/testlog") && process_file_sync_all() >= 0 &&
        parent && parent->flush && parent->flush(parent) &&
        exfat_unmount("/testlog") && exfat_mount_block_device("/testlog", device);
    if (ok) ok = equal(root, "/document.txt", edited) &&
        equal(root, "/.history", "echo workspace-history-sentinel\nexit\n") &&
        run_script(root, "/verify.sh") &&
        equal(root, "/reopened.txt", edited) &&
        equal(root, "/.history", "echo workspace-history-sentinel\nexit\nhistory\nexit\n") &&
        equal(root, "/settings.txt", "persisted-settings\n") &&
        equal(root, "/verified.done", "shell-remount-pass\n");
    stats->elapsed_us = timer_monotonic_us() - start;
    struct latency_stats latency; latency_snapshot(&latency); latency_stop();
    stats->input_poll_count = latency.input_poll_gap.count;
    stats->input_poll_max_us = latency.input_poll_gap.max_us;
    stats->tick_max_us = latency.tick_gap.max_us;
    return ok && stats->input_poll_count > 0;
}
