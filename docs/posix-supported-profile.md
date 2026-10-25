# Portable C application profile

SrvrOS provides source compatibility for a selected POSIX-style C application
profile. It does not claim POSIX certification, Linux binary compatibility or
complete Unix semantics. Build programs against the matched SrvrOS libc and
kernel; installed headers and symbol availability alone are not a support test.
The [implementation audit](posix-compatibility.md) records fixes, evidence and
remaining work. The desktop overhaul is a separate milestone.

Status: the selected portable-application milestone is **implemented and
host/VM validated** at runtime source `7e2cf54` on 2026-10-24 (local date), in
[PR134](https://github.com/PaulMDemers/SrvrOS/pull/134). It is ready for integration
review and discussion of the desktop migration. The laptop's installed/recovery
payloads have not been promoted by these VM results.

## Intended applications

The current target is statically linked CLI tools and bounded event-driven C
applications using regular files, directories, pipes, PTYs, worker threads,
`poll`/`select`, native process creation and the supported IPv4/local-socket
adapters. Lua 5.4.8 is the pinned portable consumer: its unmodified upstream
strings, UTF-8, bitwise and files tests run with the documented portable/soft
switches. Additional subprocess tests check pipe EOF and exit status. The SrvrOS
libuv adapter exercises timers, workers, files, network connections and process
shutdown; it is not the full upstream libuv implementation or test suite.

## Contracts and bounds

| Area | Implemented profile | Boundary to account for |
| --- | --- | --- |
| Error state and workers | Thread-local errno/TLS, mutexes, condition variables, semaphores, read/write locks, thread keys/destructors | Bounded worker/key slots; no real-time scheduling or pthread cancellation API |
| Regular files | Shared contents across opens, separate open-description offsets, dup-shared offsets/status, positioned and vectored I/O, append/exclusive create, truncate, open-unlinked lifetime, record locks | exFAT metadata is root-only; filesystem size/allocation limits still apply |
| Descriptor lifecycle | Unified standard/native FD table, atomic pipe2/dup3/F_DUPFD_CLOEXEC, ordered spawn open/close/dup actions | 67 native descriptors; socket/eventfd pseudo-descriptors have separate namespaces and restrictions |
| Streams | Buffered file/pipe I/O, fdopen, global flush and concurrent retirement, descriptor printf, C-locale binary64 formatting | No complete locale/wide-character/extended-precision formatting; NULL-pathname freopen mode changes are rejected with EINVAL and close the stream |
| Namespace | Kernel process CWD, openat/fchdir, directory streams, file replacement and timestamps | No hard links or symlinks; directory subtree/replacement rename and open-directory lifetime are unfinished |
| Processes | POSIX spawn/exec with argument/environment budgets, single-thread CLI fork, wait selectors and exit/signal status, popen/system | Fork refuses live-worker/unsupported pseudo-FD cases; no shared mmap, dynamic linker or full shell job control |
| Signals and time | Thread/process pending masks, supported dispositions at libc checkpoints, interrupted sleeps, absolute deadlines, CPU accounting | No general asynchronous signal frames, altstack or siginfo; stop/continue and unsupported flags fail explicitly |
| Readiness | Native poll and select masks, invalid-FD/error handling, interruption and checked timeouts | At most 64 poll interests; select FD_SETSIZE=1024; separate native GUI wait accepts up to eight nonblocking pipe/PTY interests; GUI is not a POSIX descriptor |
| Sockets | Selected IPv4 TCP/UDP and local stream/socketpair operations | No IPv6 or complete BSD socket semantics; pseudo-socket inheritance and duplicate operations remain restricted |
| Limits | sysconf/pathconf report selected actual bounds; file-size limits are enforced | Unsupported finite resource/scheduler policies fail; not a multi-user permission model |

Public compile-time bounds include PATH_MAX=1024 (resolved absolute path,
including NUL), NAME_MAX=255, PIPE_BUF=4096, IOV_MAX=16 and an aggregate 32 KiB
exec string budget. Argument and environment vectors each have 128-entry caps.
Native socketpair flags are published atomically with both endpoints. Reopening
a stream preserves its descriptor number on success, flushes before truncation,
and closes the original on failure. It may need one spare descriptor during
replacement.

Large vectored transfers may return a 256 KiB prefix; callers must handle short
I/O. These are implementation limits, not claims that every optional POSIX
facility is supported.

## Deliberate exclusions

Condition-variable attribute clocks are not complete: the current initializer
ignores `pthread_condattr_setclock(CLOCK_MONOTONIC)`, so only the default realtime
condition deadline is in the validated profile. This remains a compatibility gap;
attribute-setter success must not be treated as proof of that clock's behavior.

The Linux-oriented `epoll`, `eventfd`, inotify and other Node compatibility
symbols remain experimental adapters. In particular, eventfd blocking/overflow,
epoll edge/one-shot delivery, mask replacement and descriptor reuse semantics
are not accepted by this profile. Use the tested native poll/pipe path for new
portable consumers. A successful Node link probe does not establish Node support.

exFAT replacement is serialized during normal execution but is **not
power-loss atomic**. A failed write can quarantine the filesystem; a flush does
not add a journal. Recovery can preserve the source while truncating a duplicate
replacement target. Do not treat this profile as a durable transactional store.

The native desktop API, surfaces and worker-I/O ownership rules remain separate.
POSIX compatibility does not turn blocking filesystem work into safe UI-loop
work. See the [desktop API evolution plan](desktop-api-evolution.md) for the
combined-wait contract and future dynamic window registry.

## Reproduce the selected gates

Run one build or VM harness at a time, from the checkout root. Harnesses use
throwaway VM disk copies. Save exact source/image hashes alongside results.

```sh
tools/host-env.sh make -j2 all build/srvros.exfat test-core
tools/host-env.sh python3 tools/thread_smoke.py --boot-wait 60 --run-wait 120
tools/host-env.sh python3 tools/libc_smoke.py --boot-wait 60
tools/host-env.sh python3 tools/ports_smoke.py --boot-wait 60
tools/host-env.sh python3 tools/process_smoke.py --boot-wait 60
tools/host-env.sh python3 tools/libuv_smoke.py --boot-wait 60
tools/host-env.sh python3 tools/lua_upstream_smoke.py --test files
tools/host-env.sh python3 tools/lua_upstream_smoke.py --test strings
tools/host-env.sh python3 tools/lua_upstream_smoke.py --test utf8
tools/host-env.sh python3 tools/lua_upstream_smoke.py --test bitwise
```

The libc networking harness selects the supported e1000 NIC explicitly. Terminal and Files regression
harnesses check that changes to native descriptor ownership preserve desktop
consumers. Passing these gates provides VM evidence for this selected profile;
it does not accept a new physical payload or every API combination.

## Milestone evidence

The final runtime source is `7e2cf5473e529559f48feb4ddf1750636057a8d6`.
The full core suite, forced partial-tick deadline fixture, focused readiness
fixture and complete thread suite pass. The final combined guest gate passes
libc, ports, process/shell, libuv, utilities, Lua files/strings/UTF-8/bitwise,
Terminal and Files. Lua is pinned at
`6e22fedb74cf0c9b6656e9fce8b7331db847c605`; its portable/soft tests are unmodified.
The libuv adapter uses the pinned 1.52.1 headers and its SrvrOS integration
consumer; this is not a claim to pass upstream libuv's entire test suite.

The C0–C4 plan now has a truthful supported profile, worker/file semantics,
checked readiness and monotonic timeouts, supported process contracts and real
consumer gates. Its broader Unix work remains separate. In particular, pthread
cancellation is not implemented, and the combined GUI/FD wait remains a design;
no native input-latency acceptance is inferred from the VM readiness test.

A 150 ms idle wait used 5,536–7,968 us of process CPU over 153,900–160,017 us
elapsed in the corrected focused/full runs. The fixture also verifies 128
request/response wakeups and EOF readiness. An earlier 146,427 us return exposed
a real partial-tick deadline bug; that failed receipt is retained, along with
failed descriptor mapping and filesystem fault experiments. Passing reruns do
not erase the original evidence.

The Air agent retains exact image hashes, commands, exit statuses and raw logs
under `build/posix-core-20261024`, with a backup outside the disposable build
tree. The [machine record](machines/macbookair7-2.md) identifies that checkpoint.
A new native trial, broader API conformance suite and the desktop overhaul are
separate next steps, not implied acceptance from this milestone.

## Unattended native regression

After preparing the host's own exFAT workspace, stage the explicit capture:

```sh
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR-EXFAT-UUID --posix-capture
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --boot-wait 600
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace
```

Follow the [device guide](ubuntu-device-agent-setup.md) for preflight, recovery,
installation, run recording and one-shot boot. The separate `v1-posix` recipe
creates a private, volatile copy of the 128 MiB boot filesystem, then runs
threadstress, posixdemo, TLS and SQLite against that copy, followed by the existing
60-second desktop/Text screenshot exercise. Ordinary RAM boot remains read-only;
only this explicit diagnostic requests the writable copy. It does not enable battery/AML,
audio or network providers and needs no human input. Each test has a deadline;
its owned process tree is revoked and drained before proceeding. Nonzero exits,
timeouts or failed retirement fail the run. Handled failures save what they can
and return to Ubuntu; a missing completion record is never a pass.

The normal workspace logger saves a compact five-case receipt in `exit.log`.
`hardware_desktop.py` exposes `posix_capture` only when every case exited with
status zero, all process trees retired, and desktop screenshot/completion markers
are present. Verify the saved screenshot bytes, prior-file preservation and both
offline filesystem checks after return. This is a bounded runtime regression,
not physical input, battery, audio, networking or full POSIX acceptance.

The subsequent unattended regression uses runtime `1882029` and release
`284e53065081672f`. Its exact-stage VM passes all five suites, saved screenshot
readback, clean exFAT and reset. Preparation caught a completed-join signal race;
`dddb07f` fixes delivery after retiring the joined record, with a directed
before/after regression and libuv rerun. The same release passes the bounded native Air7,2 capture: all five zero exits
and retired trees, saved logs/screenshot, prior-file preservation and clean offline
filesystem/allocation checks. See the machine record for the exact session and
limits; the original eleven-consumer gate above identifies its own source.
