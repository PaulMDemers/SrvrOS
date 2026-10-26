# Portable C application profile

SrvrOS provides source compatibility for a selected POSIX-style C application
profile. It does not claim POSIX certification, Linux binary compatibility or
complete Unix semantics. Build programs against the matched SrvrOS libc and
kernel; installed headers and symbol availability alone are not a support test.
The [implementation audit](posix-compatibility.md) records fixes, evidence and
remaining work. The desktop overhaul is a separate milestone.

Status: the selected portable-application milestone is **implemented and
host/VM validated** at runtime source `7e2cf54` on 2026-10-24 (local date), in
[PR134](https://github.com/PaulMDemers/SrvrOS/pull/134), now merged. The later
bounded native regression is recorded below with its own source and payload;
VM evidence alone does not promote installed/recovery payloads.

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
| Error state and workers | Thread-local errno/TLS, mutexes, realtime/monotonic condition variables, read/write locks, thread keys/destructors | Bounded worker/key slots; checked supplied-stack bounds and semaphore counts; remaining restrictions below; no real-time scheduling or pthread cancellation API |
| Regular files | Shared contents across opens, separate open-description offsets, dup-shared offsets/status, positioned and vectored I/O, append/exclusive create, truncate, open-unlinked lifetime, record locks | exFAT metadata is root-only; filesystem size/allocation limits still apply |
| Descriptor lifecycle | Unified standard/native FD table, atomic pipe2/dup3/F_DUPFD_CLOEXEC, ordered spawn open/close/dup actions | 67 native descriptors; socket/eventfd pseudo-descriptors have separate namespaces and restrictions |
| Streams | Buffered file/pipe I/O, fdopen, global flush and concurrent retirement, descriptor printf, C-locale binary64 formatting | No complete locale/wide-character/extended-precision formatting; NULL-pathname freopen mode changes are rejected with EINVAL and close the stream |
| Namespace | Kernel process CWD, openat/fchdir, directory streams, file replacement and timestamps | No hard links or symlinks; directory subtree/replacement rename and open-directory lifetime are unfinished |
| Processes | POSIX spawn/exec with argument/environment budgets, single-thread CLI fork, wait selectors and exit/signal status, popen/system | Fork refuses live or unreaped worker slots and unsupported pseudo-FD cases; no shared mmap, dynamic linker or full shell job control |
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

The [October 25 synchronization audit](synchronization-audit-2026-10-25.md)
found six defects now corrected on the PR175 branch. Caller-supplied pthread
stacks retain their declared region, require a 16-byte-aligned base and at least
`PTHREAD_STACK_MIN` bytes, and are checked for address overflow before any write.
Only library-owned allocations are rounded upward, with checked size arithmetic.
The caller remains responsible for writable storage and its lifetime through
thread completion. `pthread_getattr_np(self)` reads worker metadata installed
before the callback, preserving exact supplied bounds or the actual rounded
owned allocation and current detach state. Initial-process inspection recognizes
the kernel's two fixed 2 MiB stack layouts by frame containment; unregistered
stacks outside those layouts return `ESRCH`. This adds no public layout change.
Workers publish their tid into a reserved record before entering callbacks, so
early self-detach succeeds and survives parent completion. A creator reservation
prevents premature cleanup; exclusive join/reap claims and a short table lock
serialize record ownership. Self-join returns `EDEADLK`. Failed/interrupted reaps
retain ownership for retry. Detached cleanup remains lazy, triggered by later
create/join/detach calls; there is no background collector. Mixing raw kernel
thread joins/detaches with libc-owned pthreads is outside this lifecycle contract.

The nonportable name APIs retain process-local libc names for the initial thread
and libc-created workers, with independent default `srvros-thread` values. Names
may be empty or contain at most 15 bytes plus NUL. Oversized input and a getter
buffer smaller than the stored name plus NUL return `ERANGE`, leaving the name
or output unchanged; null pointers return `EINVAL`. Unknown/reaped workers
return `ESRCH`; exited but unreaped records remain nameable. Error codes are
returned directly without changing `errno`. Protected copies prevent torn reads
under concurrent renaming. These calls do not rename kernel scheduler tasks or
add names to desktop/process diagnostics; raw syscall workers are unsupported.

Initial-thread `pthread_exit` now retires that user thread while joinable and
detached workers continue, including workers created by survivors. The kernel
retains the initial teardown continuation and shared process resources until no
workers remain active; completed, unreaped records do not prolong the process.
Libc then runs `exit(0)`, including `atexit` handlers, regardless of thread return
values. Explicit `exit`, `_exit` and fatal process signals still terminate the
whole process with their original status. TLS destructors run on thread exit;
shared files and mappings remain usable by surviving workers. Targeted signals
to the retired initial thread are not delivered, and it does not participate in
process-signal eligibility. The initial thread is joinable by default. Workers
can join it before or after retirement and retrieve its pointer return value,
or pass a null output pointer. Initial-thread detach (including self-detach)
uses the same lazy libc reaper policy as workers. Exclusive claims prevent a
second join/detach from collecting a record while a join is in progress;
interrupted kernel waits are retried without exposing `EINTR` to the caller.

Joining or reaping the initial thread invalidates its public handle but retains
the kernel continuation, stack and TLS for final process cleanup. Its separate
static libc record consumes no worker slot and is never freed/unmapped as an
allocated worker stack. Names return `ESRCH` after collection, as do kernel
signal lookups. Self-inspection reports initial-thread detach state; inspecting
the initial stack from a different thread remains unsupported (`ESRCH`).

This changes the `SYS_THREAD_EXIT` contract: workers never return; the initial
thread waits for worker retirement and returns zero solely to libc's immediate
`exit(0)` cleanup path. Use a matched kernel and libc rebuild. There is no new
public pthread layout change, and `pthread_exit` never returns to application
code. Raw syscall callers must perform the process-cleanup handoff themselves.

Condition variables retain `CLOCK_REALTIME` (including static/default init) or
`CLOCK_MONOTONIC`. Timed waits recheck their absolute deadline after early tick
wakes and signal interruption, and reacquire the mutex before returning. The
condition structure changed from 4 to 8 bytes: rebuild all consumers and use
matched library/header/application images. No compatibility with old objects
embedding `pthread_cond_t` is claimed. Absolute seconds are signed: negative
values with valid nanoseconds are accepted and compared with the selected clock.
Expired values return `ETIMEDOUT` through the ordinary mutex release/reacquisition
path. Nanoseconds outside `[0, 1000000000)` still return `EINVAL` before unlocking.
This validation correction adds no layout or syscall change. Host tests include
negative future values with modeled clocks; guest tests cover expired negative
values with the actual realtime/monotonic clocks.

`clock_getres` accepts an optional output pointer for realtime, monotonic and
process/thread CPU clocks. Null output still validates the clock and required
source: unknown clocks return `EINVAL`, unavailable realtime returns `EIO`, and
CPU usage errors propagate. Non-null output retains the existing 1 us reference
or 10 ms tick-fallback resolution. This correction changes no layout or syscall.

`times()` returns elapsed time in 100 Hz ticks using the reference clock when
available, otherwise the kernel tick counter directly. Its four CPU accounting
fields retain the same 100 Hz units. Errors from self/children usage queries
propagate before output is written; successful calls preserve errno. Null output
remains an SrvrOS extension. Host regressions cover both sources and large
counters; the guest group compares elapsed values against monotonic clock
brackets and checks CPU totals. The default VM exercises the reference path;
the missing-reference configuration is host-modeled, not VM/native accepted.
No layout or syscall change is introduced.

Relative `nanosleep`/`clock_nanosleep` capture the unslept interval before
post-wait handlers execute. A signal dispatched before any suspension preserves
the entire request. The captured value stays local until return, preserving
aliased request/output and nested-handler sleeps. Tick fallback splits elapsed
ticks into seconds and a bounded fraction before scaling, including extreme
representable intervals. `clock_nanosleep` keeps direct error returns and errno;
absolute calls leave remainder storage unchanged. Successful and non-interrupted
error paths retain their output behavior. These corrections add no layout or
syscall change. Reference/fallback and extreme boundaries are host-tested;
ordinary VM tests cover fast, long and nested-sleep handlers.

Kernel futex entries now remain bound to their process/address while any wait
syscall owns them, including before parking and after wake. Worker-initiated
process exit preserves those bindings until sibling wait continuations retire.
Wake lookup/count is IRQ-serialized. Deterministic host and guest regressions
cover interruption, wake ownership and reuse; the global 64-entry bound remains,
and this is not general SMP acceptance. Public pthread layouts and syscall
signatures are unchanged by this correction. Raw futex waits reject timeout
addition overflow with `SRV_ERR_INVAL` and return `SRV_ERR_FAULT` when a live
wait's mapping is removed or user access revoked. Word validation/load is
IRQ-serialized; restoring/replacing that address does not revive an old wait.
Readable protection changes preserve the wait. Applications must still retain
storage for synchronization objects while other threads use them.
The global 64-entry futex table returns `SRV_ERR_AGAIN` for new keys at saturation;
existing keys remain usable and owned entries cannot be stolen. Wake selection
uses scheduler-slot order, not FIFO; mutex/rwlock admission has no starvation-free
or priority-inheritance guarantee. Saturated libc callers retry while preemptible.

Once initializers abandoned through `pthread_exit` reset and wake their waiters,
including nested initializers, before thread-specific destructors run. Completed
controls stay completed. No cancellation API, public cleanup-handler stack or
nonlocal-jump recovery is added. Recursive mutex depth is bounded by `UINT_MAX`;
lock/trylock at that limit return `EAGAIN` without changing state or errno.

`SEM_VALUE_MAX` is `INT_MAX`. `sem_init` rejects larger counts with `EINVAL`;
`sem_post` uses a checked atomic increment and returns `EOVERFLOW` without changing
a full count. This does not complete the POSIX semaphore contract: `pshared` is
still ignored, blocking waits yield rather than parking on a futex, and no timed-
wait API is declared. The libuv adapter's separate mutex/condition semaphore does
not establish libc `sem_*` support. Process sharing and full blocking semantics
remain outside the validated profile.

The Linux-oriented `epoll`, `eventfd`, inotify and other Node compatibility
symbols remain experimental adapters. The [event-adapter audit](event-adapter-audit-2026-10-26.md)
corrects libc-managed epoll lifetime through close, native dup/dup2/dup3/F_DUPFD*,
final-alias retirement and descriptor reuse. Host/guest regressions cover native
alias watches and eventfd close; snapshot validation rejects stale wait results.
Eventfd duplication and nested epoll are refused. Adapter handles cannot be
exported through SCM_RIGHTS. Exec/spawn refuses live non-CLOEXEC adapters; close
them or mark every handle (including epoll aliases) close-on-exec first. Spawn
file actions cannot duplicate adapter handles into the child. Fork
continues to refuse live adapter state. Pseudo-socket epoll registration is
refused; native socketpairs remain supported. Raw `srv_*` descriptor mutation,
SCM_RIGHTS transfer of watched native descriptions, concurrent process inheritance
and cross-process adapter sharing remain outside this process-local contract.

Epoll waits now preserve zero timeouts, return already-ready events without
blocking on idle native descriptors, and recheck eventfd/control changes against
one monotonic deadline. Empty/infinite waits continue until readiness, interruption
or instance retirement. Synthetic state is checked through native poll intervals
of at most 10 ms, plus kernel clock rounding and scheduling delays; this is not a
kernel notification queue or a hard latency guarantee. Idle waits incur periodic
wakeups. Nonblocking eventfd writes now reject the reserved `UINT64_MAX` value
with `EINVAL` and additions exceeding `UINT64_MAX-1` with `EAGAIN`, preserving
the counter. EPOLLOUT requires room for at least one and returns after draining.
Zero writes remain valid at full capacity. `EFD_SEMAPHORE` reads return one and
subtract one, preserving remaining readiness; ordinary reads drain the counter.
Creation accepts only `EFD_SEMAPHORE`, `EFD_NONBLOCK` and `EFD_CLOEXEC`, rejects
unknown bits with `EINVAL` before allocation, and resets mode on slot reuse.
Changes to descriptor/status flags do not toggle semaphore mode; F_GETFL reports
O_RDWR. Blocking reads wait for a token/count; writes wait for sufficient capacity,
using interruptible 10 ms kernel poll slices without holding the lifecycle mutex.
SA_RESTART handlers retry the I/O; other interruption returns EINTR without
changing the counter/output. Close/reuse cancels a parked operation with EBADF.
F_SETFL changes are rechecked. Public poll/ppoll now recognize eventfds, including
mixed native/synthetic sets, monotonic finite deadlines and POLLNVAL on close/reuse.
The fixed FD_SETSIZE=1024 cannot represent eventfds numbered 3000..3015: use poll
or epoll for them; select/pselect are not a supported eventfd interface.
Epoll rotates delivery across stable ready sets using a per-instance cursor shared
by aliases, chosen under the lifecycle mutex after polling. All armed watches are
snapshotted; native HUP/ERR can be returned even with an empty interest mask.
This does not promise thread-scheduling fairness under arbitrary mutation.
One-shot registrations disable only on delivery (including HUP/ERR); successful
MOD rearms with its current mask/data. Overlapping waits cannot deliver the same
armed revision twice. Undelivered entries remain armed, and changing to ordinary
level mode removes the one-shot restriction. EPOLLPRI/POLLPRI conversion is
covered with modeled native readiness; no driver urgent-data support is implied.
Normal-data aliases EPOLLRDNORM/EPOLLWRNORM preserve the requested result bits.
Edge, half-close, message and band modes (EPOLLET, EPOLLRDHUP, EPOLLMSG,
EPOLLRDBAND, EPOLLWRBAND) return EOPNOTSUPP on ADD/MOD; unknown bits return EINVAL.
Failed MOD preserves mask/data and disabled one-shot state; DEL ignores its event
argument. epoll_pwait temporarily installs a supplied mask and restores it on
success, timeout and error, including pending signals at immediate-ready waits.
This follows the existing cooperative signal ABI, not asynchronous signal frames.
General concurrent pseudo-socket transitions remain outside acceptance.
Use the tested native poll/pipe path for new portable consumers. A successful Node link probe
does not establish Node support.

exFAT replacement is serialized during normal execution but is **not
power-loss atomic**. A failed write can quarantine the filesystem; a flush does
not add a journal. Recovery can preserve the source while truncating a duplicate
replacement target. Do not treat this profile as a durable transactional store.

The native desktop API, surfaces and worker-I/O ownership rules remain separate.
POSIX compatibility does not turn blocking filesystem work into safe UI-loop
work. See the [desktop API evolution plan](desktop-api-evolution.md) for the
implemented combined-wait contract and dynamic window registry.

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
cancellation is not implemented. The separate native GUI/nonblocking-pipe wait
is now implemented; see [desktop connections](desktop-connections.md). It does
not make GUI objects POSIX descriptors or establish native input-latency bounds.

A 150 ms idle wait used 5,536–7,968 us of process CPU over 153,900–160,017 us
elapsed in the corrected focused/full runs. The fixture also verifies 128
request/response wakeups and EOF readiness. An earlier 146,427 us return exposed
a real partial-tick deadline bug; that failed receipt is retained, along with
failed descriptor mapping and filesystem fault experiments. Passing reruns do
not erase the original evidence.

The Air agent retains exact image hashes, commands, exit statuses and raw logs
under `build/posix-core-20261024`, with a backup outside the disposable build
tree. The [machine record](machines/macbookair7-2.md) identifies that checkpoint.
The subsequent bounded native trial is recorded below. Broader API conformance
and desktop-runtime acceptance retain their own gates; neither follows from the
original portable-C milestone.

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


The completed bounded synchronization audit also corrects `sem_trywait` under
contention: it retries compare-exchange conflicts while tokens remain and returns
`EAGAIN` only after observing zero. See the [closeout](synchronization-audit-2026-10-25.md#final-synchronization-pass--2026-10-26)
for scope, validation and remaining broader work.

## Imported exFAT files

Imported file contents preserve the distinction between logical and initialized
length: reads return zeros beyond `ValidDataLength`. Rename/replacement preserve
that boundary, while a successful full overwrite initializes its new contents.
Both memory and block mounts validate geometry and stream extents; unsupported
revision/multiple-FAT images are refused. Dirty/media-error single-FAT media stay
read-only. See the [filesystem audit](filesystem-audit-2026-10-26.md) for host/VM
coverage and the remaining parser, ownership and crash-recovery work.

File entry imports now require checked entry/name hashes, one stream followed by
its exact consecutive ASCII name records, valid allocation flags and empty-file
fields. Unsupported non-ASCII names and secondary extensions are refused rather
than renamed or dropped. Directory scans reject traversed FAT cycles, invalid
successors and premature termination. This remains a bounded import profile;
automatic boot/upcase repair, full Unicode, complete allocation-format conformance
and crash safety remain separate work. Rebuild older generated images whose entry checksums were unset.

Imported duplicate/case-colliding paths and overlapping mount subtrees are
refused. Exact allocation aliases used by generated images retain their storage
until the last published owner retires; overwrite still uses a new allocation.
Block mounts quarantine inconsistent allocation maps or failed ownership checks
as read-only before sidecar recovery/mutation. This is not a hard-link API,
a media-repair facility or certification of corrupt-file contents. Mounts now
claim declared physical block-view ranges; overlapping views and active raw
transport leases refuse competing mounts. Raw diagnostics delegate only their
explicit inner image view and retain exclusion after I/O failure until reboot.
Unmount now holds storage ownership while rechecking linked process references
and refuses active backend operations. Runtime USB command failure and AHCI
uncertain completion invalidate the device and declared aliases; cached block
reads cannot bypass that state. No online rebind or media-generation recovery
is supported. Hot removal/replacement and power-loss recovery remain open.


exFAT sidecars are validated completely from one read buffer before applying
permissions, identities or times. Records must belong to that mount, contain
printable paths and bounded decimal fields, and end in newlines. Malformed
sidecars do not partially apply. Unreadable/invalid temporary files are retained
on mount; a later explicit sync may replace them. On quarantined media, no
sidecar is replayed: imported default identities/permissions and unknown times
are used because path-keyed metadata cannot identify the survivor of an
interrupted replacement. Inspect/repair such media offline before normal use.

Sidecar promotion replaces the committed record before retiring the temporary
record; it does not delete the committed file first. `chmod` and explicit time
updates propagate sidecar persistence failure as I/O failure and restore live
metadata. A failed write may have changed media, so this is not a durable rollback.
Sidecar v1 has no transaction generation or document checksum, and rename and
replacement are not journaled or crash-atomic. Prefix-write/flush fault tests
establish bounded quarantine behavior, not physical-drive power-loss guarantees.


Both mount paths validate full boot-region checksums (all repeated words),
mandatory boot fields and extended-sector signatures. The checked header is the
same snapshot used for geometry. A uniquely valid backup can provide read-only
access when the primary is invalid/unreadable, including when its sector-size
byte is damaged. Ambiguous or wholly invalid candidates are refused. A valid
primary with a damaged or disagreeing backup also stays read-only. Backup flags
and usage estimates do not override the primary; no automatic repair occurs.

Exactly one root allocation bitmap and one root upcase table are required.
Wrong bitmap identity, missing/duplicate/misplaced system descriptors, unknown
active critical entries and invalid system FAT chains are refused. The upcase
stream must be even-sized and at most 128 KiB, with a valid checksum, complete
compressed/uncompressed mapping coverage and the mandatory ASCII mappings.
Supported filenames remain ASCII. System allocation checking follows FAT chains
rather than assuming contiguous storage. Multi-cluster bitmaps remain read-only
and the allocation checker explicitly refuses their unsupported graph checks;
statfs counts their complete FAT chain under storage ownership. Rebuild old
repository-generated images with missing boot checksums or zero upcase tables.


Directory extension now selects space and initializes a new cluster before
retiring preceding end markers. Out-of-space or allocator refusal before that
point leaves directory bytes unchanged and the volume usable. When an end marker
is consumed, undefined trailing records are cleared, including unused later
clusters, so old names cannot reappear after remount. Payloads beyond the volume's
heap and entry sets too wide for one cluster are refused before dirty marking or
scratch allocation. On 512-byte clusters, a 211-character basename exceeds that
entry-set limit even though it is within the format's overall name-length limit.
Failures after allocation or metadata writes begin can still leave the volume
quarantined; compound file/sidecar operations do not reserve all future disk space
up front or offer a journaled rollback. Error reporting remains the existing
boolean/I/O-failure contract, not a new ENOSPC-specific syscall interface.


Truncation and replacement publish and flush the new stream before retiring the
old allocation, then persist sidecar metadata using the released space. Exact
allocation aliases still retain their clusters until the last owner retires.
Directory growth similarly defers sidecar persistence until its enclosing
operation finishes. Full-volume truncate-to-zero can therefore reclaim space;
nonzero replacement still needs free space for its new data first. A healthy
full-volume chmod/utime refusal before payload writes preserves live metadata
and disk bytes. If a later failure follows publication or retirement, the
existing dirty/quarantine contract applies. There is no compound reservation or
promise that every delete/truncate can finish on a full volume.
