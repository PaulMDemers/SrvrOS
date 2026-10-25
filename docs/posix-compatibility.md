# POSIX compatibility audit and priorities

Reviewed 2026-10-24 against `bc0edb9` (PR130). This is a source and targeted-test
audit, not a complete standards conformance assessment. SrvrOS has a useful
POSIX-shaped C library and native syscall ABI. It is **not POSIX-conformant or
Linux-binary-compatible**, and exporting a header or passing a link probe does
not establish the function's semantics.

Use POSIX.1-2024 / Issue 8 as the reference for new compatibility contracts;
document selected features, restrictions and extensions explicitly. The standard
covers system interfaces **and shell/utilities**, so a libc function count cannot
measure overall conformance. See the official
[scope](https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap01.html)
and [general interface requirements](https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html).
This reference choice is a development target, not certification.

For porting guidance, start with the [supported application profile](posix-supported-profile.md).

## Implementation progress after the audit

Native duplex socketpair creation now installs nonblocking/CLOEXEC status and
copies both results in one IRQ-excluded operation, with rollback on output
failure. Legacy native pair creation retains zero flags. Libuv's internal duplex
and socket pairs use this atomic path. Full core and focused/full guest tests
pass, including alternating pipe/socketpair publishers concurrent with child
inheritance scans. This does not add pseudo-socket inheritance.

`freopen` now flushes before a truncating reopen, preserves its native descriptor
number through atomic replacement, clears EOF/error/read state on success and
closes/unregisters the original stream on failure. Reopen may require one spare
FD. NULL-pathname mode changes are explicitly rejected with EINVAL and close the
stream. Focused and full guest tests cover same-path truncation, flags, retained
FD identity, failed paths/modes and safe global flushing after failure.

The libuv adapter now snapshots low-numbered spawn sources before child actions
can replace them, and closes extra originals only after all mappings. A guest
swap of two inherited files failed before the correction and passes afterward,
with shared offsets and parent CLOEXEC flags retained. The first fixture exceeded
its low-FD precondition after earlier consumer tests; the isolated early fixture
reproduced the actual mapping bug. Both receipts are retained.

Pipe, PTY and native Unix-listener nonblocking status now belongs to the shared
endpoint rather than each FD. Duplicates, fork and spawn see the same status;
CLOEXEC remains per descriptor and opposite endpoints remain independent.
Unix listeners retain references across duplication/inheritance and survive
closing the original FD. Full core and focused/full guest suites pass, including
child status changes and close-before-accept. Native IPv4 status sharing and
pseudo-descriptor inheritance remain separate gaps.

Seek overflow now returns EOVERFLOW without changing the offset; nonseekable
endpoints report ESPIPE. File flush/close failures retain EIO rather than becoming
EBADF. The descriptor is still retired after a failed close. TTY operations
distinguish EBADF/ENOTTY, and stat/path helpers preserve path errors. Host injected
flush failures and focused/full guest error tests pass.

`pipe2`, `dup3` and F_DUPFD_CLOEXEC now publish descriptors and flags in one
kernel operation. Minimum-FD duplication does not temporarily occupy or close
lower numbers; dup3 publishes its replacement before retiring an old endpoint.
Core and focused/full guest tests pass, including concurrent pipe/duplicate
creation while twelve spawned children inspect their inherited tables. CLOFORK
and pseudo-socket/eventfd duplication variants remain outside this profile.
`dprintf`/`vdprintf` borrow the descriptor through a private stack buffer, without
allocating a spare FD or closing/changing the caller's descriptor. Full-table,
format/count, nonblocking failure and broken-pipe tests pass. The normal-deadline
libuv regression also passes this candidate.

Descriptors 0, 1 and 2 now occupy the same kernel table as other descriptors.
Close releases their numbers for reuse; dup/dup2 copy endpoint ownership rather
than retaining a reference to a mutable standard-stream slot. Replacement is
published under IRQ exclusion before retiring the previous endpoint, and native
standard-stream swaps snapshot all sources first. Fcntl, stat, positional I/O,
TTY identity and close-on-exec use the real entries. Host pipe/PTY lifetime tests,
focused guest reuse/redirection/close checks and the full thread suite pass.
POSIX spawn now inherits the complete native descriptor table and applies close,
dup2 and open actions in their recorded order before closing CLOEXEC entries.
Child opens use the child's directory and creation mask without allocating a
temporary descriptor in the parent. A closed source cannot be resurrected from
the parent by a later action. Native GUI spawn retains its explicit-transfer
profile; socket/eventfd adapters remain separate. Focused and full guest tests
cover inherited files/pipes, shared offsets, CLOEXEC, closed standard descriptors,
ordered redirects, failed actions and a full parent descriptor table.

`popen` creates temporary pipes with CLOEXEC and closes prior popen streams in
each child. Creation and pclose descriptor retirement are serialized, with the
lock released before waiting for a child. Two simultaneous writable popen streams
verify that closing the first can receive EOF and finish while the second stays
open. These semantics do not provide general pseudo-descriptor inheritance.

Global `fflush(NULL)` now snapshots and pins the stream registry before taking
individual FILE locks. Concurrent fclose removes the stream from that registry
and defers allocation release until pinned flushers retire; closing under an
explicit recursive flockfile cannot strand them. Registry locks never span FILE
locks or I/O. FILE/pthread mutex owner fields use atomic access. The three-writer
plus global-flusher guest race and full thread suite pass. The subsequent descriptor block makes fclose of standard streams close their
real descriptor, and permits a successful global flush with an empty registry.

Small pipe writes through `write` are atomic up to `PIPE_BUF=4096`: blocking
writers wait for the whole record, nonblocking writers return EAGAIN without
publishing a prefix, and a missing reader generates SIGPIPE/EPIPE. Larger writes
retain partial-transfer behavior. Host queue models and full guest thread tests
cover concurrent record writers, requested-space wakeups and signal dispositions.
Vectored I/O now snapshots and validates the vector in the kernel, gathers or
scatters through an owned buffer, and issues one underlying transfer. Combined
pipe records obey PIPE_BUF atomicity; readv returns the available prefix without
blocking again on the next element. Positional vectors leave the shared offset
unchanged. Requests above 256 KiB may return a short prefix; IOV_MAX is 16.
Pseudo-eventfd vectored operations return EOPNOTSUPP. The libuv adapter now marks
its internal wakeup and process pipes CLOEXEC. The new spawn inheritance exposed
an input-writer leak that kept `cat` alive; both failing runs are retained and the
corrected normal-deadline libuv suite passes. Lua upstream files/process tests
also pass with inherited descriptors. The native GUI I/O admission
rules remain unchanged; vector calls are not admitted during another I/O owner.
Core/build and focused/full guest thread tests pass: concurrent split-vector
records, nonblocking no-prefix refusal, EINTR/restart, SIGPIPE, invalid vectors,
short-read completion, positioned offsets and large-transfer continuation.

`umask` is kernel process state, shared by threads and inherited by fork/spawn.
Open/mkdir creation modes are installed before publication can yield and persist
through exFAT metadata/remount; opening an existing file with O_CREAT preserves
its mode. Trailing-slash mkdir/mkdirat resolution now accepts a missing final
directory. Host/core, guest inheritance tests and exFAT creation-mode/remount and
fault-matrix checks pass. This remains root-only metadata, not multi-user access
control or power-loss-atomic namespace operations.


Binary64 printf now emits `%a/A`, `%e/E`, `%f/F`, and `%g/G` using exact
expansion and nearest/ties-even rounding, with sign/padding, alternate form,
subnormals and large precision. Integer precision is no longer silently limited
to a 95-character temporary. Host edge cases and 12,000 random comparisons,
guest formatting tests and the unmodified Lua strings/UTF-8/bitwise portable/soft
suites pass. Two explicit carry fixtures use the standard significant-digit rule
rather than glibc 2.43's lost alternate-form zeroes (`%#.2g` of 99.5 and `%#.3g`
of 999.5). See the [WG14 clarification](https://open-std.org/JTC1/SC22/WG14/issues/c99/issue0233.html).
Non-default rounding, full long-double precision, wide-character/positional
formats and locale-dependent formatting remain unaccepted. A libuv run exceeded
the short harness deadline; the retained 60-second diagnostic and a subsequent
normal-deadline run both pass, without relaxing the default.


Work is on `device/macbookair7-2/posix-core`, draft PR134. Desktop overhaul is
explicitly deferred while this compatibility milestone is implemented. The
original audit below describes `bc0edb9`; the following fixes supersede its
corresponding defect descriptions. This is still a partial compatibility
profile, not completed POSIX conformance.

| Contract now implemented | Regression evidence | Remaining boundary |
| --- | --- | --- |
| Per-thread `errno`, initialized/zero TLS, reclaimed worker TLS | `threadstress/errno_test.c`, `test_process_loader.c`, guest `tlsprobe`; simultaneous workers and 16 reuse rounds with stable mapped bytes | Existing bounded worker slots remain |
| Kernel `pread`/`pwrite` without offset mutation, including `dup` and append handles | `threadstress/positional_test.c`, `test_process_output.c`; concurrent regions, sparse writes, EOF, bad access, allocation failure preserving previous data | Separate opens now share contents; private offsets remain in each open description |
| Regular-file access modes, no implicit append, shared append/nonblocking flags through `dup`, atomic exclusive create, close-on-exec publication and safe truncation | `threadstress/open_test.c`, host process-output fixtures, full core and thread VM suite | Cross-open content coherence is implemented; directory descriptors now work; unsupported open flags fail explicitly |
| `select` ready-bit counts, masks, EBADF, timeout validation, IPv4 socket FD range | `test_posix_poll.c`, `libcprobe/readiness.c`; guest uses an explicit supported e1000 | `FD_SETSIZE=1024` accommodates current sockets, not all pseudo-FDs; poll remains bounded to 64 entries |
| Interrupted sleep remainder, absolute realtime/monotonic deadlines, direct `clock_nanosleep` errors | `threadstress/sleep_test.c`; aliased remainder, signal interruption, no early completion | Timing remains subject to timer resolution and scheduling |
| Thread-directed signals, blocked `raise`, per-thread dispatch guard, one-shot handler reset | `threadstress/targeted_signal_test.c`, host process-output tests, full thread and libuv guest suites | Handlers are still dispatched at libc checkpoints; no general asynchronous signal trampoline, altstack or siginfo frames |
| Process/thread CPU clocks, `clock`, `times`, reaped-child CPU totals and peak mapped memory | `threadstress/usage_test.c`, scheduler and process host fixtures; busy/sleeping workers, child wait and peak retention | CPU0 accounting uses the monotonic reference; auxiliary rusage counters remain zero, and this is not an energy measurement |
| Shared-reader/exclusive-writer locks, thread-local key values, key generation reuse and repeated destructors | `threadstress/pthread_contract_test.c`; concurrent readers, blocked writer, destructor reinstallation and deleted/recreated keys | Bounded keys/workers; reader-preference locking, no real-time scheduling policy |
| Kernel-owned file-size limits inherited by spawn and retained by exec; short writes at the boundary, EFBIG and thread-directed SIGXFSZ | `threadstress/resource_test.c`, full thread VM suite and local core tests | Other finite policy limits are explicitly unsupported; stack/core report fixed platform values, unspecified policies report infinity. Backend file capacity is separate from the policy limit. |
| Wait status distinguishes exit values 0–255 from signals; any-child/current-group/specific-group selectors and `wait()` | `threadstress/wait_test.c`; high exit values, self-signal, nohang, group isolation, reaping and system status | Stop/continue events and waitid remain unsupported; native monitor wait status stays unchanged |
| `pselect`/`ppoll` temporary masks, error restoration and wide poll deadlines | `threadstress/masked_poll_test.c`, host poll/core and full thread VM suite | Atomicity relies on the current cooperative handler ABI; an asynchronous signal-frame implementation must introduce a kernel masked wait first. Poll still has a 64-entry bound. |

Separate opens now retain one in-memory content object, with independent offsets
and status flags. Readers observe writes and truncation from other opens before
flush; stat/fstat report the live size. Read-only fsync flushes shared dirty data,
and closing one writer cannot overwrite another writer's changes. Bounded reads
remain allocation-free under IRQ exclusion during owned I/O. Host failure/lifetime
tests and a full guest thread suite, including four independent atomic appenders,
pass. Native whole-file replacement/append refuses a live open path to avoid
invalidating that content. Open-file unlink now removes the name while retaining contents, inode metadata
and record locks until last close. Recreating the name creates a distinct object;
closing the deleted file cannot overwrite it. `tmpfile` uses exclusive creation
followed by unlink. Record-lock range changes split/merge correctly and preserve
the original locks when the bounded lock table is exhausted. Retaining an
unlinked read-only snapshot currently requires memory within the 16 MiB file
buffer bound; allocation failure leaves the name intact. Regular-file rename now follows open content identities, including replacement: the
displaced target becomes an orphan, and the source keeps its inode and locks. This does not implement power-loss atomicity or a
general inode filesystem.

The orphan/lock/tempfile block passes host failure tests, full local core and
thread/ports/process/libuv/utility/libc VM suites. The stricter exec permission
check exposed missing initramfs executable metadata; boot archives now preserve
tar modes and runtime ELF staging sets execute bits without marking data files
executable.

Native `fork()` now implements a bounded single-thread CLI profile: eager private
pages with copied TLS/FPU/register continuation, inherited regular-file and pipe
open descriptions, empty child pending signals, inherited limits/masks and ordered
`pthread_atfork` callbacks. Host tests cover partial-copy rollback and parent-page
preservation; the full guest thread suite covers private memory, shared offsets,
repeated fork/reap and exec. Active/unjoined workers and libc-owned socket/eventfd/
epoll adapters currently return `EOPNOTSUPP`; this is not general multithreaded fork.

The exec family now includes `execv`, `execl`, `execle` and `execlp`, with `PATH`
search in execvp/spawnp including empty components. Argument overflow returns
`E2BIG`; invalid ELF, missing file and execute permissions have distinct errors.
Exec preserves ignored dispositions, pending signals and masks, resets caught
handlers and closes CLOEXEC descriptors. Replacing a process with workers is
explicitly refused until worker teardown can be implemented safely. `execvp` and
`execlp` now invoke `/fat/bin/sh` for executable text rejected with ENOEXEC,
preserving the script path and remaining arguments. Exec now accepts a shared
32 KiB budget for argument/environment strings, NULs and pointer vectors, with
no separate 256-byte string cap. `ARG_MAX`/sysconf report that budget. The native
CLI profile additionally limits argv and environment vectors to 128 entries each;
this additional count restriction is not a full POSIX exec capacity contract.
Exact-budget, oversized, wide-vector, 8 KiB string, fork/exec and spawn guest
tests pass. Mutating an inherited full environment fails without dropping its
last entry; failed setenv insertion releases its temporary allocation. Environment
storage remains bounded and is not a general thread-safe environment service.

The working directory is now kernel process state: shared by a process's threads,
inherited by fork/spawn and preserved by exec independently of `PWD`. The shell
and Terminal explicitly select their child directories. Checked traversal rejects
oversized paths, missing intermediate directories and regular-file `/..` traversal
rather than silently truncating/collapsing them. `chdir` leaves environment updates
to the shell. Full host/core tests, focused guest inheritance/failure tests and
process/libuv VM regressions pass. Active working directories currently prevent
namespace removal/rename; general directory inode lifetime is still pending.

Directory streams now own real descriptors. `open(O_DIRECTORY)`, `fdopendir`,
`dirfd`, directory cookies, `rewinddir`/`seekdir`/`telldir`, `fchdir` and descriptor-
relative open/stat/chmod/access/mkdir/unlink/rename adapters are implemented.
Dup/fork share a directory's position; readdir reports real inode numbers and
`.`/`..`, preserves errno at EOF, and propagates backend failures. Directory
handles pin the current pathname against rename/removal; open-directory orphan
identities remain a restriction. Directory replacement/subtree rename is not
implemented by the descriptor-relative `renameat` adapter.

`futimens`, `utimensat`, `utime` and `utimes` now update metadata rather than
returning success without a change. NOW/OMIT are resolved atomically in the kernel,
including unlinked open files; invalid subsecond fields fail. Persistent metadata
rounds down to whole seconds and cannot store dates before 1970 (EOVERFLOW).
Host tests, full core, focused and full thread tests, ports, process, libuv,
utilities, libc and directory VM regressions pass. One process run did not leave
the bootloader; its log is retained. Serial harnesses now run headlessly and the
process harness refuses to send commands before the monitor is ready; rerun passed.

Regular-file replacement rename now publishes the source over an existing target
without first unlinking the target. Open source handles follow the new name;
open target handles retain their old data and zero link count until last close.
Allocation failure leaves names intact; an I/O error after publication reconciles
the live identities even though the syscall fails. Native guarded file-manager
rename remains non-overwriting. Cross-directory moves of short names now allocate
records in the destination directory, fixing a move that disappeared on remount.
Directory subtree/replacement rename and open-directory lifetime remain pending.

This is serialized runtime namespace publication, **not power-loss atomicity**.
An injected failure between writing the destination record and retiring the source
can leave duplicate cluster references on dirty media. The writer quarantines
further mutations. Offline `fsck.exfat` can keep the source and truncate the
replacement target to zero; it must not be described as old-or-new atomic
recovery. The fault fixture retains this distinction and checks source contents,
unrelated data, dirty admission and clean post-repair allocation separately.
A journal or recovery transaction is still needed for stronger durability.

The Lua consumer exposed missing `_setjmp`/`_longjmp` and `difftime`, unsupported
hexadecimal number parsing, buffered writes incorrectly accepted on read-only
streams, and shell status text contaminating `popen` output. These are corrected.
C-locale `strtod`/`strtof` use an exact integer ratio and final nearest/ties-even
rounding, including hex input, subnormals, infinity/NaN, long inputs and end
pointers; exact host comparisons and sanitizers pass. `strtold` still widens
binary64, and alternate rounding modes/extended precision remain gaps.
Descriptor-backed stdio validates access at admission, preserves fdopen offsets,
sets append status, and uses fstat for renamed/unlinked streams. Formatted output
propagates errors. Noninteractive shell commands no longer append native status messages. The subsequent descriptor and POSIX-spawn blocks remove the private standard-stream
slots and implement ordered inherited descriptors; native GUI spawn keeps its
explicit-transfer contract.
Pinned Lua 5.4.8's unmodified `files.lua` passes its portable/soft profile, followed
by explicit read/write popen, EOF and exit-status checks. This is a selected
consumer gate, not all upstream tests or a completed compatibility milestone.

`abort()` now raises SIGABRT, including when the signal was ignored or a handler
returns; wait status identifies signal termination. Explicit SIGSEGV/SIGPROF
signals have catchable dispositions, but this does not add hardware exception
recovery or a profiling timer. Unsupported stop/continue requests fail instead
of accidentally terminating the target. Stop/resume job control remains open.
Thread-specific storage supports 128 keys, with exhaustion and repeated-use tests.
The `_POSIX_*` minimum constants no longer alias local limits. Filesystem/process
paths now use a shared 1,024-byte capacity (including NUL), and components and
descriptor directory entries support 255-byte names. `PATH_MAX`, `FILENAME_MAX`,
CLI/libuv buffers and spawn-action paths agree; native GUI message payloads retain
their separate bounds. The larger directory-read structure requires matched
kernel/userspace builds. This is still a bound on the resolved absolute path,
not unbounded traversal through directory descriptors.

`pathconf` and `fpathconf` validate the target and report the selected path/name,
link and option limits rather than confusing `_PC_PATH_MAX` with page size.
`OPEN_MAX` now matches the 67 descriptor slots reported by sysconf. The exFAT
importer rejects an overlong child path without registering a truncated alias.
Guest tests exercise exact-boundary paths, long names, streams, rename and
directory enumeration; host tests check remount persistence and bounded import.
The exec fallback regression also corrected quoted `exit` arguments in the shell;
negative statuses wrap to eight bits and invalid numeric operands fail.

Resource limit changes are process-wide, including all workers. The current
root-only profile may raise a previously lowered hard file-size limit. Unsupported
finite policies return `EOPNOTSUPP` instead of changing an unenforced libc array.
Priority queries validate process identity and report the supported zero priority;
nonzero changes and group/user priority operations are explicitly unsupported.
The `nice` utility and libuv adapter propagate those errors instead of claiming
an adjustment happened. This is a restricted scheduler, not a nice-aware one.

`sigaction` refuses unsupported delivery flags rather than accepting them as
working. Pending thread signals are private to their target; process-directed
signals remain shared and consumable by an eligible thread. Ignoring a signal
clears pending instances across all threads. A joinable exited thread retains a
valid ID but receives no signal, matching the
[Issue 8 pthread_kill contract](https://pubs.opengroup.org/onlinepubs/9799919799.2024edition/functions/pthread_kill.html).

`clock()` now counts process CPU microseconds (`CLOCKS_PER_SEC=1000000`);
`times()` uses `_SC_CLK_TCK=100`. Consumers needing elapsed time must use
`CLOCK_MONOTONIC`; the native `time`/`timeout` utilities have been corrected.
CPU charging follows userspace/kernel transitions and scheduler switches, so
sleeping time and another worker's execution are not charged to the caller.

Pthread scheduling queries report the supported `SCHED_OTHER` / priority-zero
contract; unsupported policies or nonzero priorities fail instead of succeeding
without effect. The stress expansion also exposed a scheduler address-space bug:
a same-process switch could inherit a temporary kernel CR3. Suspended threads
now retain their actual address space, independently of process identity, with
a deterministic host fixture and passing full thread/libc guest regressions.

Local logs are under `build/posix-core-20261024/`; Actions remain disabled.
The initial socket-readiness fixture failed because Q35's default NIC was not
supported; its failed log is retained, and the corrected explicit-e1000 run
passed. These VM results do not accept a new native payload. Installed hardware
and recovery releases remain unchanged.

## Original audit baseline (bc0edb9)

Paths below are relative to `userspace/lib/src/` unless stated otherwise.
“Implemented subset” means real operations exist, not that every corner case
has passed a conformance suite. Limits may be valid implementation choices;
silent semantic substitutions are a different problem.

| Area | Evidence in current implementation | Boundary / missing contract |
| --- | --- | --- |
| C strings, conversion, formatting, allocation, stdio | Library sources; `libcprobe`, `nodeprobe`, thread stdio/heap tests | Useful tested subset; no complete ISO C or POSIX libc certification |
| File descriptors and regular files | `posix_file.c`, `posix_dir.c`; kernel VFS/open-file descriptions, dup, pipe, metadata and exFAT | Bounded descriptors/paths; positional I/O race below; broader file/flag/error semantics need explicit tests |
| Directories and metadata | Real traversal/mkdir/rename/unlink/stat and persistent sidecar metadata | No journal/power-loss atomicity; `link`/`symlink` return ENOSYS; single-user metadata does not establish permission enforcement |
| Spawn/exec/wait | `posix_process.c`, kernel process lifetime and descriptor actions | Real native child processes; `fork()` in `posix_node_compat.c` returns ENOSYS; no general fork-based Unix execution model |
| Pipes, PTYs and terminal shell | Kernel pipe and `pty.c`, termios/ioctl adapters; native Terminal uses PTY shell | PTYs exist; complete job-control/session/terminal semantics require a separate matrix and tests |
| Threads and synchronization | `posix_pthread.c`: real thread creation/join, atomics/futex mutexes, conditions, once, rwlocks, key storage | Bounded slots; shared errno, thread-directed signal gap and compatibility placeholders below; not all pthread semantics implemented |
| Signals | `posix_signal.c`, kernel pending signals and **per-thread masks** | Handler dispatch occurs at libc checkpoints; accepted flags exceed implemented delivery semantics; no general asynchronous userspace signal frame |
| Poll/readiness | `posix_poll.c`, kernel poll and wait queues | Real readiness; select, interruption and descriptor namespace gaps below; no `pselect`/`ppoll` implementation found |
| Network / local sockets | `posix_socket.c`: IPv4 TCP/UDP, AF_UNIX stream/socketpair and bounded FD passing | No IPv6; pseudo-descriptor table and partial options/ancillary semantics; not a complete BSD socket implementation |
| Memory mapping | `posix_mman.c`: private anonymous and file-snapshot mappings, protection/unmap | MAP_SHARED rejected; no general shared-file mapping/writeback; demand paging not implemented |
| Clocks and sleeping | `posix_time.c`: realtime and monotonic clock queries, tick-based sleeps | CPU-time accounting substituted with uptime in some APIs; interrupted-sleep remainder missing; absolute clock_nanosleep rejected |
| Identity and resource controls | Root-only passwd/group/UID/GID adapters; `posix_resource.c` | No multi-user protection model; reported limits/accounting partly library placeholders |
| Dynamic loading | `posix_dlfcn.c` exports the interface | dlopen/dlsym/dlclose return ENOSYS; dladdr placeholder; not a dynamic linker |
| Locale and text | C locale adapter and bounded UTF-8/wide-character helpers | No general locale database, full Unicode collation or complete text semantics |
| Shell and utilities | Native shell plus selected ports, `ports_smoke.py`, `posixutils_smoke.py` | Useful commands do not establish POSIX shell grammar, expansion, job-control or complete utility conformance |
| Linux extensions | `posix_node_compat.c`: eventfd and poll-backed epoll adapters | Separate portability surface, not POSIX requirements; inotify ENOSYS, no Linux syscall ABI |
| libuv / Node | `ports/srvros/uv.c` adapter and selected upstream source | Adapter is not full upstream libuv; historical Node execution is not a current reproduced full runtime port |

## Original semantic findings and remaining priorities

### P0. Thread-local error state

[`posix_errno.c`](../userspace/lib/src/posix_errno.c) stores a single static
integer and returns its address from `__errno_location()`. All threads in a
process share it. A worker can overwrite the UI thread's error before it is read.
An audit-only host-thread fixture linked the actual source with its accessor
renamed to avoid replacing Ubuntu libc's errno: both threads received the same
address and the worker's assignment changed the main thread's value. This
reproduces the storage defect; it is not a guest TLS acceptance test.
POSIX requires one thread's errno assignments not to change another's value
([error numbers](https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html)).

Implement thread-lifetime error storage, including main thread, startup, detached
exit and slot reuse. Avoid a bootstrap recursion through allocation or pthread
functions which themselves set errno. Test simultaneous distinct errors with
barriers and repeated thread creation; ensure the probe is linked to SrvrOS libc,
not the Ubuntu host library.

### P0. Positional I/O changes the shared offset

[`posix_file.c`](../userspace/lib/src/posix_file.c) implements pread/pwrite as
save-offset, seek, read/write, restore-offset. Another thread or descriptor that
shares the open-file description can observe/interfere with the temporary offset.
Restoring it later does not make the operation atomic. Add a kernel positional
operation that does not mutate the shared offset, with checked ranges, partial
transfer/error handling and tests through dup/shared descriptors. Verify append
semantics against the selected standard rather than inheriting an accidental
host behavior. The standard's
[read/pread contract](https://pubs.opengroup.org/onlinepubs/9799919799.2024edition/functions/read.html)
is the reference.

### P0. select and descriptor readiness

[`posix_poll.c`](../userspace/lib/src/posix_poll.c) ignores `exceptfds`, returns
poll's count of ready descriptors instead of counting all selected output bits,
does not turn POLLNVAL into select's EBADF, and maps negative timeval fields to
an infinite wait. Error/HUP handling can also set result bits that the caller did
not request. Poll itself has 64 entries and collapses negative kernel errors to
EINVAL; EINTR delivery requires review alongside signal dispatch.

`FD_SETSIZE` is 128, whereas socket() returns library pseudo-descriptors starting
at 1000 from an eight-entry table. These cannot be placed in the current fd_set;
callers must not use FD_SET on them. Poll has a translation path, select does not
offer a general socket-compatible descriptor namespace. Unify descriptor identity
or provide an explicitly safe transitional contract before marketing generic
event-loop compatibility.

Add fixtures for a descriptor ready in both sets, exceptional conditions,
invalid descriptors, ignored negative poll FDs, zero/invalid timeouts, HUP/EOF,
nonblocking connect, signals and concurrent close/reuse. Check all return values
and masks, not just whether a loop woke up.

### P1. Signal and thread semantics

The kernel has per-thread masks; do not replace that fact with the outdated claim
that all signal state is process-global. However libc has a process-global
dispatch-in-progress flag and delivers handlers only at its checkpoints.
sigaction accepts SA_SIGINFO, SA_ONSTACK and SA_RESETHAND, while the dispatcher
calls the one-argument handler without implementing those contracts. pthread_kill
checks the thread then calls kill(getpid(), sig), so it is not thread-directed.

Reject unsupported modes clearly or implement them fully; never advertise them
through a successful registration alone. Specify pending-signal selection,
thread-directed delivery, handler masks, interruption/restart and wait races.
Test masked/unmasked threads, targeted delivery, handler registration changes and
blocking calls. Full asynchronous signal delivery is a distinct kernel project.

### P1. Successful placeholders and misleading accounting

- `posix_resource.c`: setrlimit updates a libc array without installing kernel
  enforcement; get/setpriority return zero; getrusage uses global uptime and
  global used memory instead of process CPU time/peak resident usage.
- `posix_time.c`: clock() returns system ticks; nanosleep ignores its remaining
  interval argument even on interruption. POSIX requires the remaining interval
  when requested after EINTR ([nanosleep](https://pubs.opengroup.org/onlinepubs/9799919799/functions/nanosleep.html)).
- `posix_node_compat.c`: pthread_atfork discards callbacks and returns success;
  pthread_getschedparam/setschedparam do not implement scheduling policy.
- `posix_pthread.c`: thread naming setters validate but do not retain a name;
  getters return a constant. These `_np` extensions need honest contracts too.

Replace unsupported success with a documented supported restriction/error where
the API allows it, or implement the operation. Separate diagnostic estimates from
standard accounting APIs. Revisit probes which currently accept placeholders;
a compatibility fix may intentionally make an old optimistic probe fail until
its required semantics are implemented.

### P1. Linux adapter behavior, separately tracked

The existing epoll adapter is real code, so “epoll absent” is inaccurate. It is
also not a drop-in Linux epoll implementation: epoll_wait changes timeout zero to
a 10 ms poll, returns after a short sleep for an indefinitely waiting eventfd-only
set, and epoll_pwait ignores its signal mask. Registration stores event flags
without implementing edge-triggered/one-shot lifetime rules. Audit/reject those
modes, fix ready-event/timeout behavior and test fairness and close/reuse before
using this as a general upstream runtime backend. These are Linux compatibility
issues, not missing POSIX requirements.

## Desktop dependencies

The [desktop evolution plan](desktop-api-evolution.md) needs dependable worker
error state, file operations and event dispatch now. It does not need fork,
dynamic linking or every standard utility before supporting multiple windows.

Provide a single UI connection dispatcher which can wait on GUI events, timer
deadlines, file/socket readiness and worker completion. Decide how GUI readiness
enters the kernel wait model; repeatedly polling a separate GUI loop is not the
intended permanent solution. Preserve native GUI authentication, surface seals
and cancellation ownership while making the userspace loop reusable.

For future ports, publish a small capability/limits manifest and test it against
the actual implementation. Audit sysconf/pathconf/feature-test macros before
advertising support. A function returning ENOSYS can be a useful explicit stub;
a function returning success with different semantics is a porting trap.

## Implementation sequence and completion criteria

| Block | Deliverable | Acceptance |
| --- | --- | --- |
| C0: honest baseline | This matrix, documented unsupported modes, recorded existing smoke results | No claim of certification or complete libuv/Node based on symbols |
| C1: worker correctness | Thread-local errno; kernel positional I/O; audit shared libc state used by jobs | Deterministic guest tests with barriers, dup/shared offsets, cancellation and thread-slot reuse |
| C2: portable event loop | select/poll semantics, descriptor namespace, errors/timeouts; combined GUI/FD wait design | Ready/masked/invalid FD cases, EINTR races, no lost wakeups, bounded idle CPU, input under I/O |
| C3: truthful process APIs | Reject/fix unsupported signal flags and scheduling/resource placeholders; proper interrupted sleeps | Negative tests and positive semantics, including resource enforcement if advertised |
| C4: chosen portable consumer | One pinned real CLI/event-driven C application, its explicit supported feature profile and upstream tests | Build/link/run on SrvrOS; files/network/shutdown verified, not just version output |
| C5: broader Unix semantics | Separate designs for fork, shared mmap, dynamic linking, permissions/security, full shell/job control | Per-feature design and tests; no blanket readiness inferred from C4 |

Each block should land as reviewable changes with failing-before/passing-after
tests for the actual bug. Run tests locally while repository Actions are disabled.
Do not lower unsupported results into successful no-ops to keep a port compiling.
Keep API errors explicit and update callers and the support matrix together.

## Validation and evidence limits

This audit uses current source plus the existing guest harnesses:

```sh
make -q all usb-image
tools/host-env.sh python3 tools/libc_smoke.py
tools/host-env.sh python3 tools/thread_smoke.py
tools/host-env.sh python3 tools/libuv_smoke.py
```

Run them sequentially in one checkout. Each harness uses a disposable disk copy;
these are VM tests, not native device tests. The local audit receipt records the
exact image hashes, command exit statuses and output under
`build/desktop-api-audit-20261024/`. None is a comprehensive POSIX test suite.
All three guest harnesses passed on October 24; `make -q all usb-image` confirmed
the existing images were current for the unchanged runtime source. The audit
changed documentation only. The host errno and clipboard fixtures deliberately
reproduced known gaps; their zero exit statuses mean the defects were observed,
not that the affected semantics passed. Their sources and logs are retained with
the local receipt for conversion into passing-after-fix regressions.
In particular, libcprobe/nodeprobe passing does not disprove the source findings
above, and a “resource ok” marker does not prove kernel-enforced rlimits.

For future conformance work, first select the claimed profile/options, then add
system-interface, shell and utility tests against the standard. The Open Group's
[test-suite overview](https://posix.opengroup.org/testsuites.html) is a starting
point for scope and requirements, not evidence that SrvrOS has run those suites.
Keep unsupported, expected-failing, untested and passed cases separate; do not
publish a POSIX percentage without a defined denominator and semantic coverage.

See [porting](ports.md), [libuv](libuv-port.md) and [Node recovery](node-port.md)
for build instructions and their narrower current guarantees.
