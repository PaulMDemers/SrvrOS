# Code and documentation audit progress — 2026-10-26

This is a scope inventory, not a whole-OS completion percentage. No exhaustive
file-by-file, security or standards-conformance audit has been completed. Passing
regressions establish the listed contracts on their recorded models/VMs; they do
not accept other hardware or every concurrent execution.

## Completed bounded work

| Area | Reviewed and corrected | Evidence / limits |
| --- | --- | --- |
| ARM64 Mac development | Repo-owned build/VM helpers, reproducible toolchain setup and current build/docs reconciliation | [Mac guide](macos-development.md), [initial audit](code-docs-audit-2026-10-25.md); x86_64 QEMU TCG, not native ARM64 OS support |
| Synchronization, stacks and timing | Nineteen runtime defects corrected: stack bounds/inspection, thread lifecycle/names/initial-thread exit and join, condition clocks/deadlines, semaphore bounds/contention, clock resolution/times fallback, sleep remainders/overflow, futex races/deadlines/mapping lifetime, once exit and recursive depth | [Synchronization closeout](synchronization-audit-2026-10-25.md), merged PR175; bounded host and guest contracts, not complete pthread/POSIX conformance |
| AHCI failure investigation | Deadline-edge false-offline decisions and independent phase budgets corrected; diagnostics and regressions added | [AHCI report](ahci-offline-investigation-2026-10-26.md), merged PR178; nine fresh stress VMs passed. Historical spontaneous incidents remain unattributed and shelved pending recurrence |

## Current draft PR179

The [event-adapter audit](event-adapter-audit-2026-10-26.md) now covers seven
correction areas: descriptor lifetime across close/dup/reuse; wait/deadline and
interruption handling; eventfd counter bounds/readiness; semaphore and creation
flags; ready-set rotation and empty-mask HUP/ERR; one-shot rearming and priority
conversion; explicit refusal of unsupported edge mode. The positive suite has
34 host modes and seven event-adapter guest groups, plus the complete thread and
libuv consumer gates. Test counts measure executable cases, not audit completion.

EPOLLET now returns EOPNOTSUPP on ADD/MOD. This closes false success; it does not
implement edge-triggered notifications. Native priority sources are not accepted
merely because modeled bit conversion passes.

## Remaining event-adapter work, in priority order

1. **Signal-mask waits:** source review confirms epoll_pwait discards its nonnull
   sigmask and calls epoll_wait. Next bounded task: deterministic pending-signal,
   mask restoration and interruption regressions, followed by an honest supported
   contract. Review the existing ppoll/pselect approach: it bypasses handler
   dispatch during mask entry and relies on cooperative delivery plus kernel
   pending checks. A plain sigprocmask swap may consume the signal before the
   wait. This turn adds no signal-mask runtime acceptance.
2. **Blocking eventfd:** reads/writes currently return EAGAIN when they should
   block without EFD_NONBLOCK. Define wake/interruption/close behavior and test it;
   align native poll/select treatment of synthetic descriptors.
3. **Remaining event/error contracts:** audit alternate event bits, RDHUP, control
   validation/error precedence, socket shutdown/readiness and concurrent socket
   lifecycle. Header constants do not establish implemented semantics.
4. **Ownership beyond the tested libc path:** descriptor transfer (SCM_RIGHTS),
   raw syscall mutations, cross-process and exec state. Nested epoll is refused;
   general kernel notification and actual edge support require additional work.

Items 1 and 2 are known implementation gaps. Items 3 and 4 combine known limits
with paths still requiring directed review; they are not claims that every path
contains a confirmed defect.

## Broader work still outside exhaustive coverage

- Filesystem failure/ownership paths and crash consistency. Existing exFAT tests
  and serialized replacement do not establish power-loss atomicity.
- Networking/socket lifecycle, process/resource boundaries, VM/syscall validation,
  security and isolation need dedicated bounded passes. Existing feature tests
  are not substitutes for those audits.
- Terminal/session/job-control semantics, cancellation/asynchronous signals,
  complete upstream libuv and Node reproducibility remain outside the accepted
  portable profile; the missing historical Node-port provenance remains unresolved.
- Native drivers, graphics/audio/power and device acceptance remain machine
  specific. Air6,1 input-under-I/O latency needs its own hardware evidence;
  passing emulated guests cannot close it.
- Broader host-suite portability and documentation reconciliation should continue
  with each subsystem. Historical journals remain evidence, while the supported
  profile and this inventory carry current disposition.

The next completion target is a documented, regression-tested **bounded event
adapter profile**, with every unsupported mode explicitly rejected or clearly
identified. It is not complete Linux API parity or certification of the entire OS.
