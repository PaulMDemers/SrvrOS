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

## Event-adapter completion target (PR179)

The bounded process-local event-adapter audit is complete. All 46 host modes and
sanitizers, ten negative controls, native poll/select regressions, the matched
build, targeted guest, complete thread suite and libuv suite pass. Delivery is
[PR179](https://github.com/PaulMDemers/SrvrOS/pull/179); GitHub records its merge
state and SHA. The user authorized merge after these gates.

| Contract area | Current disposition |
| --- | --- |
| Close, duplication, replacement and reuse | Identity/revision tracking, final-owner retirement and stale-result rejection covered |
| Waits, deadlines and interruption | Immediate/finite/infinite waits and bounded synthetic rechecks covered |
| Counters, flags and semaphore mode | Bounds/readiness, token reads, flag validation and O_RDWR reporting corrected |
| Delivery order, one-shot and native events | Stable-set rotation, HUP/ERR, rearming, priority and normal-data conversion covered |
| Temporary signal masks | epoll_pwait mask installation/restoration and pending/arriving signal handling implemented for the cooperative signal ABI |
| Blocking I/O and poll | Blocking reads/writes, SA_RESTART/EINTR, close/reuse cancellation and public eventfd poll/ppoll integration implemented |
| Unsupported modes | Edge/half-close/message/band modes refused; unknown bits rejected. Pseudo-socket epoll transitions refused; native socketpair coverage retained |
| Ownership boundaries | Adapter-handle rights export refused; exec/spawn requires CLOEXEC handles and refuses duplicating them into a child; live-adapter fork remains refused |

The [event-adapter audit](event-adapter-audit-2026-10-26.md) records original
failures and correction evidence. The current gate has 46 host modes and eight
event-adapter guest groups, plus complete thread/libuv consumers. Counts describe
executable cases, not whole-OS audit completion.

### Explicit limits and future capabilities

Actual edge notification, kernel-driven synthetic wakeups, cross-process adapter
sharing and full Linux conformance remain future capabilities. Eventfd numbers
3000..3015 exceed FD_SETSIZE=1024, so use poll/epoll, not select/pselect. Native
watched descriptions transferred via SCM_RIGHTS, raw descriptor syscalls,
concurrent process-inheritance mutations and general pseudo-socket transitions
remain outside the process-local contract. Priority conversion does not prove a
native urgent-data source. The implementation has 8 instances, 64 watches per
instance and 16 eventfds, with periodic polling and no hard wake-latency bound.

These limits do not reopen the completed target once the specified gates pass.
They require a separately scoped implementation and acceptance pass if broader
compatibility is wanted.

## Broader audit: filesystem checkpoints

The [exFAT import audit](filesystem-audit-2026-10-26.md) corrects unchecked boot
geometry and stale-byte exposure beyond imported `ValidDataLength`. The first
checkpoint's sixteen host modes cover mount validation, initialized
reads and preservation through rename/replacement/overwrite/remount. The next
checkpoint expands to 29 modes and corrects entry checksums/layouts, ASCII name
identity and directory-chain termination/cycles. Generator and parser changes
land together; seven new negative controls and a recursive generated-image check
cover this pass. Dedicated disposable VMs exercise valid imports and corruption
refusal. A third checkpoint expands to 47 modes and protects exact allocation
aliases, rejects duplicate/overlapping namespaces and quarantines inconsistent
allocation maps before mutation. Normal teardown busy guards and process snapshot
ownership were traced without confirming an additional open-reference defect.
A fourth checkpoint expands import coverage to 51 modes and adds physical-range
mount claims, exclusive raw-transport leases, delegated inner-image admission
and persistent failure quarantine. Block/workspace/USB and seven synthetic
image-adapter runs cover handoff and retirement. The next checkpoint expands to 61 modes, serializes backend operations against
teardown, rechecks process busy state under storage ownership, and propagates
terminal USB/AHCI identity loss through block/cache/claim admission. Live
unregister/rebind and crash recovery remain open. The rename/sidecar checkpoint
expands to 72 modes and adds 144 write/flush/half-write fault boundaries. It fixes
partial or cross-volume sidecar application, malformed fields, stale replay on
dirty media, destructive temporary cleanup, delete-first promotion and hidden
metadata persistence failures. Interrupted states remain quarantined; journaled
rename, controller reordering and physical power-loss recovery are not accepted.
Delivery is [draft PR182](https://github.com/PaulMDemers/SrvrOS/pull/182).
This checkpoint is separate from the completed event-adapter target.

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

The documented, regression-tested **bounded event adapter profile** is complete.
Unsupported capabilities are explicitly rejected or identified above. This is
not complete Linux API parity or certification of the entire OS.
