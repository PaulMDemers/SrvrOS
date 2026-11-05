/*
 * Current SrvrOS signal boundary for the browser/JSC port (PR #310).
 * Guest probe only: SA_SIGINFO registration must fail without changing an
 * existing one-argument SIGUSR1 handler. It does not implement async delivery,
 * claim machine-context support, or exercise SIGBUS/SIGTRAP.
 *
 * To build after preparing a matched SDK:
 *   ports/srvros/webkit/cc -std=c11 -fno-builtin \
 *     tools/test_browser_signal_contract.c -o build/browser-runtime/signal-contract.elf
 * Run in a disposable SrvrOS guest and require status 0 plus marker below.
 *
 * Shared runtime owner handoff (not VMM PR #317): WTF's POSIX suspend path
 * installs SA_SIGINFO | SA_RESTART, uses pthread_kill on the target, publishes
 * captured register state with sem_post, and waits in sigsuspend for resume.
 * JSC's collector scans every suspended mutator's stack and registers. Safe
 * support needs asynchronously delivered per-thread signal/trampoline with a
 * valid interrupted register context, correct masks, atomic suspend/resume
 * handshake and signal-safe synchronization, nested-signal/stack bounds, and
 * exit/race handling. An alternative requires an independently proven safe
 * safepoint protocol. Never report success with uncaptured roots.
 * Source evidence: userspace/lib/src/posix_signal.c:92-135,163-173,223-239;
 * userspace/lib/src/posix_pthread.c:418-425; kernel/src/process.c:3520-3545;
 * upstream WTF/wtf/posix/ThreadingPOSIX.cpp:135-185,209-221,575-600;
 * JavaScriptCore/heap/MachineStackMarker.cpp:119-142,154-186.
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

static volatile sig_atomic_t seen;
static void simple_handler(int signum) { if (signum == SIGUSR1) ++seen; }
static void info_handler(int signum, siginfo_t *info, void *context) {
    (void)signum; (void)info; (void)context;
}
int main(void) {
    struct sigaction simple = {0}, before = {0}, after = {0}, requested = {0};
    struct sigaction untouched, sentinel;
    simple.sa_handler = simple_handler;
    if (sigemptyset(&simple.sa_mask) || sigaction(SIGUSR1, &simple, 0) ||
        sigaction(SIGUSR1, 0, &before)) return 1;
    requested.sa_sigaction = info_handler;
    requested.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigemptyset(&requested.sa_mask)) return 2;
    memset(&untouched, 0xa5, sizeof(untouched));
    memcpy(&sentinel, &untouched, sizeof(sentinel));
    errno = 0;
    if (sigaction(SIGUSR1, &requested, &untouched) != -1 || errno != EINVAL ||
        memcmp(&untouched, &sentinel, sizeof(sentinel))) return 3;
    if (sigaction(SIGUSR1, 0, &after) || after.sa_handler != before.sa_handler ||
        after.sa_flags != before.sa_flags || after.sa_mask != before.sa_mask) return 4;
    if (raise(SIGUSR1) || seen != 1) return 5;
    puts("browser-signal-contract: siginfo rejected; handler preserved");
    return 0;
}
