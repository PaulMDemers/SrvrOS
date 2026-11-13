# Opt-in native launch and fcntl gate observations

The existing `--capture-write-timing` desktop stress profile additionally enables
this read-only scheduler diagnostic. Ordinary logging does not enable it. Build
matched kernel/userspace images and follow the normal exact-stage, recovery,
preflight and one-shot collection workflow. It changes no admission predicate,
owner scope, callback, timeout, mapping bound, I/O policy or stress requirement.

The diagnostic records validated raw SYS_EXEC decisions and their first refusal.
The existing atomic predicate exposes its first refusal through a read-only
result; its boolean wrapper retains the identical conditions. The original
4096-mapping atomic-attempt bound, larger-image fallback, descriptor/authority
checks and original loader/retirement rules remain unchanged. No reason code
permits retrying with weaker checks.

Raw entry is observed before cancellation and the initial gate. `entries` counts
raw user exec entries while enabled; `decisions` counts those reaching the copied,
validated request decision. An early gate has reason0, before a validated decision.
Early invalid requests can return without a decision or wait. The twelve reason
counts sum to decisions. Handled and error count private results, including
allocation failure; neither implies a fallback wait. Typed inner calls are not
classified as raw entries. These counters establish coverage, not per-cycle
source histories or causal latency improvements.

| Code | First exec decision | Detail |
| --- | --- | --- |
| 0 | Raw entry before validation | Zero decision fields |
| 1 | Request unsupported by private route | Mask: scope1, flags2, stdio4, process-group8, foreground16, file-actions32 |
| 2 | Parent/result/liveness refused | Existing parent checks |
| 3 | Parent mapping count exceeds4096 | Exact entry mapping count |
| 4 | Absolute path precondition refused | No path bytes retained |
| 5 | Existing atomic scheduler proof refused | Atomic reason below |
| 6 | Inherited standard descriptor refused | Detail bits0–7:fd index; bits8–15:type; bit16:handle exceeds2 |
| 7 | Path/namespace resolution refused | Detail1:path context;2:system path |
| 8 | Immutable source unavailable | Detail1:no node;2:executable permission;3:read-only memory-view proof |
| 9 | Existing ELF validation refused | Original loader checks |
| 10 | Existing atomic image bound refused | Segment page count |
| 11 | Private attempt handled successfully | Coverage counter only |
| 12 | Private attempt returned error | Coverage counter only |

For reasons0–4 the atomic check has not run; its zero field is decoded as
`not_evaluated`, never success. After reaching that check, atomic codes are
0:success,1:state/capacity,2:no eligible foreign owner,
3:same-image/owner alias,4:caller,5:root,6:sibling state/safety,
7:sibling root. No process path, argument/environment bytes, address-space root,
user pointer or storage payload is recorded. Source reason3 is the composite
existing view proof, not a new inference about which internal clause failed.

Fcntl context retains the raw command, fd, descriptor type and flags from a
read-only pre-gate snapshot. The descriptor may change while waiting; ordinary
backend lookup/validation remains authoritative. Only SETFD, SETFL, DUPFD,
DUPFD_CLOEXEC and RESERVE_APPEND_RAM retain their scalar argument. Ignored,
unknown and pointer-valued arguments (including lock structures) are recorded
as scalar0/arg0. Nothing dereferences a user pointer for this diagnostic.

A scalar per-thread context is charged to the existing scheduler storage budget
and cleared at every configured raw syscall entry. The existing owner and gate
record caps remain unchanged. Two independent fixed records retain the longest
completed exec and fcntl waits at the existing100ms observation threshold, even
if old top-eight records are later evicted. They include the same stable caller,
initial owner sequence/identity and complete wait clocks. The initial owner is a
correlation anchor; ownership can change during the wait. Durations are software
wall intervals, not CPU or hardware-completion time.

Hooks allocate, print and schedule nothing. They change only scalar metadata
under IRQ exclusion; observation arithmetic overflow, missing context or stale
reset epochs invalidate evidence without altering execution. All whole-app and
workload clocks continue charging diagnostic overhead. No additional clock,
callback, transfer or service window is inserted into measured recording hooks.

Only `exit.log` emits version1 `launch-gate-summary`, `launch-gate-exec` and
`launch-gate-fcntl` records. The complete maximum encoded set is below2KiB;
the original64KiB tail, rotation/checkpoint caps and logger cadence are unchanged.
Missing target calls, counters, final rows, identity/clock joins, valid scalar
redaction or epoch binding fail strict parsing. Zero long waits are explicit
counter observations, not invented zero-duration records. If an exact legacy
`io-gate` record survives, its complete caller/owner/wait binding must agree.

`make test-launch-gate-trace` composes the real C scheduler formatter with the
strict parser and covers missing/malformed/crossed records, scalar redaction,
reset epochs and overflow. The lifecycle/IDT/logger fixtures retain the original
refusal, cancellation and final-only behavior checks. For the current positive
loaded guest, add `--require-launch-gate-trace` to
`tools/audio_io_stress_smoke.py`. The exact native stage still exercises foreign
hardware refusal only; that is not positive native diagnostic acceptance.
Original workload, byte/readback/flush/allocation and retirement checks remain
mandatory alongside these observations. Acceptance is per physical unit.
