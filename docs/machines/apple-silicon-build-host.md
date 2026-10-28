# Apple Silicon macOS build host

Updated: 2026-10-25. This is a development host running an emulated x86_64
guest, not a native SrvrOS hardware target. Observed host model: MacBookPro18,2.

## Active scope

Branch `device/apple-silicon/macos-build-docs`, based on freshly fetched
`origin/main` at `eda8bfbc`. Move the working workspace macOS helpers into
`tools/macos/`, make installation and VM state paths portable, and audit the
current documentation against implementation and named acceptance records.
Touched areas: host tool bootstrap/checks, macOS VM tooling, current guides and
documentation audit notes. No kernel, driver, firmware or application behavior
changes are planned. The only open PR at scope inspection was Pro11,2 PR19,
which retains USB Wi-Fi ownership and does not overlap this host-tool work.

## Initial evidence

At source `46f6212f`, the complete ordinary image built with native ARM64 Zig
0.15.2, GNU Make 4.4.1, GNU tar and xorriso. QEMU 11.1.2 TCG ran one x86_64 CPU,
1 GiB RAM, Q35, AHCI, USB input, e1000 and HDA/CoreAudio at 1440×900. The desktop
ready marker appeared 6.61 seconds after launch. Rendering and injected typing
were observed; the user subsequently reported opening, running and testing it
successfully. That report is qualitative and does not establish a performance,
filesystem durability or audio-endurance bound. The automated Save As check was
not completed; it is not a save/readback pass.

This host has no native SrvrOS installation, physical USB acceptance or recovery
payload. No reboot is needed for this scope. Prior guest disks and evidence must
be retained during migration and excluded from Git; required VM state must
survive `make distclean`. Local logs identify their original source and paths.

## Completed host validation

The repo now owns the build/VM helpers and the [reproduction guide](../macos-development.md).
Previous workspace entry points forward to them. Existing VM disks moved to
`.local/macos-vm/` with before/after SHA-256 equality; historical manifests were
preserved. This state survives `distclean` and is ignored by Git.

Validation on baseline `eda8bfbc` with local host/docs changes:

- Fresh checksum-pinned ARM64 Zig bootstrap into an isolated test directory: pass.
- `sh tools/macos/build.sh check-host`: pass.
- `sh tools/macos/build.sh -j2 all`: pass; matched manifest written.
- `python3 tools/test_macos_host.py -v`: five tests pass, including provenance,
  disk preservation/failure, PID refusal and simulated-build refusal.
- `sh tools/macos/build.sh check-docs` and `git diff --check`: pass.
- Separate headless Q35/TCG VM with a long, spaced state path: console at 4.88 s,
  desktop/first frame at 5.13 s, QMP running, 1440×900 Text/desktop screenshot
  visually inspected. That disposable VM was stopped via its own QMP socket.

Local bootstrap/build logs, VM manifest and screenshot are under
`build/review/macos-host/` and `build/macos-build.log`. The session manifest
contains artifact hashes; runtime source and later documentation changes are
not conflated with another machine's native acceptance. The prior interactive
VM was already stopped before migration. No VM is intentionally left running
by this validation, and no native boot/install was performed.

## Audit checkpoint and next scope

The [first-pass audit](../code-docs-audit-2026-10-25.md) records current-guide
corrections and source references. Unresolved work includes condition-variable
clock attributes, Air6,1 native lifecycle latency, Node source-history recovery,
macOS test-suite portability and deeper subsystem review. The Linux host/core,
full desktop/VM suites, physical save durability, UEFI boot and audio endurance
were not accepted by this Mac build/boot check. Keep those gates separate.

## Continued audit scope — 2026-10-25 evening

PR175 publishes the first pass. The continued audit consumes `origin/main`
`aa7b7392` (PR173 adds three failed/incomplete Air6,1 timing attempts; no runtime
change). PR174 owns process startup/worker retirement under disk contention;
PR172 owns shared desktop application dispatch; PR19 retains Wi-Fi diagnostics.
This pass audits libc synchronization/stack contracts, creates bounded host
reproducers, and corrects current documentation. It does not implement changes
in those active runtime scopes or claim new native acceptance.

The continued pass confirms three defect categories in the
[synchronization audit](../synchronization-audit-2026-10-25.md): supplied-stack
bounds (two cases), selected condition clocks and semaphore count overflow.
`tools/audit_sync_contracts.py` compiles real library sources with prefixed
symbols and modeled OS services; no threads or VM are started. Default mode
exits 1 for the known defects; `--expect-known-defects` exits 0 for all four
observations. Separate JSON receipts under `build/review/sync-audit/default`
and `build/review/sync-audit/known` retain source/header hashes and output.
The exact-size stack and ordinary semaphore operations are positive controls.
This is source-level host evidence, not a full thread/concurrency suite. Current
guides exclude the affected contracts and distinguish libc from libuv semaphore
coverage. Documentation links and whitespace validation pass. Runtime corrections
remain follow-up work; no kernel/library implementation was changed.

## Synchronization corrections — active scope

The user authorized fixes for the three confirmed findings and continued audit.
Base is `a90ac149` with `origin/main` still at `aa7b7392`; inspected PR172/174/19
retain desktop dispatch, process admission and Wi-Fi ownership respectively.
This block owns libc supplied-stack bounds, condition clock retention/deadlines,
semaphore count bounds, focused host and guest regressions, and current-guide/PR
updates. Rebuild matched userspace after the condition-variable layout change.
No native hardware trial or expansion of those independent PR scopes is planned.

The three fixes are implemented. The positive host suite passes; all three
isolated reintroduced-defect controls fail as intended. Matched `all` rebuild,
targeted live guest synchronization cases, the complete thread suite and complete
libuv consumer suite pass. Logs and hashes are retained under
`build/review/sync-fixes/`; no physical boot occurred. `pthread_cond_t` is now
8 bytes and all consumers were rebuilt. Supplied stacks preserve exact bounds;
condition clocks/deadlines are honored; semaphore counts use an atomic checked
maximum. Process-sharing/timed semaphore support is not added.

Continued guest audit confirms a separate `pthread_getattr_np(self)` metadata
failure: a 64 KiB supplied stack is reported as a guessed 2 MiB region. The
explicit `stack-inspection-audit` mode returns a failure receipt and is excluded
from the passing `all` suite. See the synchronization report for exact output,
next correction scope and remaining publication/lifetime questions. No claim of
fixing that fourth finding accompanies the three completed corrections.

## Self-stack inspection correction — active scope

The user authorized the fourth confirmed fix, regressions, documentation/PR
updates and continued audit. This block consumes main `1eb60b8f` (merged
PR174/176 scheduler retirement work) into this branch. PR172/19 retain shared
desktop dispatch and Wi-Fi scope. Work is bounded to libc thread metadata,
focused host/guest inspection tests and current documentation. Worker metadata
must exist before the user callback, including when the child runs before
`pthread_create` returns. Initial-process stack reporting retains the kernel's
two fixed layouts; unknown raw-thread stacks must not receive guessed bounds.
Next checkpoint is a matched build, sequential guest thread/libuv checks and a
bounded audit of adjacent thread lifecycle contracts. No physical boot is armed.

The self-stack correction is implemented with private TLS start metadata and
atomic detach-state reads/updates. Host early-child regressions pass; removing
metadata installation in an isolated negative control produces exit 1. Matched
`all` build, targeted inspection, final complete thread suite and full libuv
consumer suite pass on this ARM64 host's x86_64 QEMU. The merged timing parser's
three Python tests also pass. Documentation validation checks 634 local links
with zero errors; whitespace checks pass. The complete Linux/core suite was not
run, and no native acceptance is added. Evidence, exact build manifest and source/
log hashes are under `build/review/stack-inspection/`. Disposable guests exited.

Continued audit's deterministic host model finds early self-detach returning
`EINVAL` before the parent publishes its record. That separate explicit audit
exits 1 and remains outside the passing regression gate; no live race frequency
is inferred. Next scope is record publication/lifecycle ownership, including
reaping. Current status also reconciles PR176's failed Air6,1 native trial; worker
pipe admission is still only a candidate explanation. This block is complete;
no VM or physical boot is left running/armed.

## Early self-detach correction — active scope

The user authorized the publication-order fix, regressions, docs/PR updates and
continued audit. Base is `d9b1e710`, consuming main `aa931093` (merged PR172 shared
Text/Preview runtime). PR19 retains Wi-Fi diagnostics; no overlapping libc edits
were found. This block owns libc pthread record publication and cleanup ownership,
focused host/guest lifecycle tests and documentation. Install the record before
callbacks, retain creator ownership until create returns, and serialize join/reap
claims without holding the table lock across kernel waits or memory teardown.
Next checkpoint is deterministic early-child/parent-resume coverage, matched
build and sequential thread/libuv guest suites. No native trial is armed.

The early-detach fix is complete. Child publication precedes callbacks; creator
reservations prevent early cleanup, and exclusive join/reap claims serialize
teardown without blocking under the table lock. Self-join returns `EDEADLK`.
Failed queries/joins retain ownership for retry. No public ABI change is added.

Matched `all`, positive host regressions (40 forced early detached lifetimes plus
ordinary join failure/interruption retry), ASan/UBSan, targeted guest self-detach
(96 lifetimes/three concurrent creators), full thread suite and full libuv suite
pass. The newly merged desktop-runtime host target passes on this Mac as well.
Four isolated lifecycle negative controls each fail with exit 1. Documentation
checks 639 local links with zero errors and whitespace checks pass. Host native
pthread execution forces ordering; it does not replace guest kernel validation.
Raw logs, manifests and source hashes remain under `build/review/self-detach/`.
All disposable VMs stopped; no native boot was performed or armed.

Continued audit confirms nonportable thread-name APIs are stubs: setting
`audit-worker` succeeds but reading returns `srvros-thread`. A separate explicit
host audit exits 1; current guides exclude real naming. Source review also
corrected the fork limit to include exited-but-unreaped workers. The next
bounded work can address naming storage/error semantics; abnormal once-initializer
exit remains an unconfirmed audit area. Detached cleanup is still lazy and raw
kernel lifecycle operations must not be mixed with libc-owned workers.

## Thread-name correction — active scope

The user authorized name storage/error semantics, regression tests, docs/PR
updates and continued audit. Base is `b095d9b4`, consuming main `fa5a6cc1`'s
Air7,2 shared-runtime acceptance documentation; runtime remains `aa931093`.
PR19 retains Wi-Fi scope. This block owns libc pthread names, focused host/guest
regressions and current docs. Use protected record storage plus an initial-thread
name, bounded 15-byte payloads, explicit range errors and lifecycle-aware lookup.
This does not add kernel scheduler/UI naming. Next checkpoint is deterministic
pre-parent worker naming, concurrent guest round trips, matched build and the
thread/libuv suites. No physical boot is armed.

The thread-name fix is complete: independent protected initial-thread/worker
storage, 15-byte maximum plus NUL, explicit range errors without partial writes,
errno preservation and cleanup-aware lookup. Default names reset with each record;
exited-but-unreaped workers retain their names. Kernel task labels are unchanged.

Matched build, default host regressions, ASan/UBSan and targeted name VM tests
pass. Three isolated negative controls each fail with exit 1. Host tests force
naming before parent completion and across 40 worker lifetimes; the guest covers
24 alternating joinable/detached lifetimes and 600 concurrent naming operations.
The final complete thread and libuv suites pass sequentially. Documentation
checks 641 local links with zero errors; whitespace checks pass. Exact manifests,
source/log hashes and receipts are under `build/review/thread-names/`. No full
Linux/core or physical acceptance is claimed; disposable VMs have stopped.

Continued audit confirms initial-thread `pthread_exit` kills a live worker:
`main-exit-audit` logs the initial exit, then process status 0 without the worker
survival/completion markers; harness exit 3. This separate failed audit is
excluded from `all`. The next correction needs kernel initial-thread/process
lifetime and last-thread retirement ownership. Once-initializer abnormal exit
remains unconfirmed. Current guides also consume Air7,2's new shared-runtime
acceptance without extending its scope to native playback or all UI edge cases.
No VM or physical boot is left running/armed.

## Initial-thread exit correction — active scope

The user authorized the initial-thread exit fix, regressions, docs/PR updates
and continued audit. Base is `2ba7edce`; fetched main remains `fa5a6cc1`.
PR19's current Wi-Fi branch is `58f68fe4`, with no overlapping pthread scope.
This block owns kernel initial-thread retirement, the libc last-thread cleanup
handoff, focused host/guest tests and current documentation. Retain the initial
kernel continuation and process resources while workers live; preserve explicit
process exit/fatal signals and run normal libc cleanup after the last thread.
Next checkpoint is matched-image subprocess regression coverage and sequential
full thread/libuv suites. No native boot is armed.


The initial-thread exit correction is complete. The retained kernel continuation
waits for active workers, then returns only to libc's `exit(0)` cleanup path.
Initial-thread pointer values do not become process status; explicit process
exit and fatal signals retain their behavior. Dead initial-thread signal targets
cannot terminate survivors. This syscall handoff requires matched kernel/libc.

Matched build, host synchronization regressions, ASan/UBSan, targeted 30-lifetime
initial-exit guest coverage, final full thread suite and final full libuv suite
pass. Kernel premature-teardown and libc cleanup-skipping negative controls
fail with exit 3 and 1 respectively. The corrected kernel was restored and
rebuilt before final suites. Documentation checks 641 local links with zero
errors; whitespace checks pass. Logs, source hashes and manifests are retained
in `build/review/initial-exit/`. No full Linux/core or native acceptance is claimed.

Continued guest audit confirms that joining the initial thread returns `EINVAL`
(22), with no return value, then the audit exits status 1 (harness exit 3).
`main-join-audit` is excluded from `all`; initial-thread join/detach remains
explicitly unsupported. The next bounded work is initial-thread joinability and
return-value/cleanup ownership. Once-initializer abnormal exit remains
unconfirmed. All disposable VMs have stopped; no native boot is armed.

## Initial-thread join correction — active scope

The user authorized initial-thread join, regression tests, docs/PR updates and
continued audit. Base is `b75fc79a`; fetched main remains `fa5a6cc1`, and PR19
remains at `58f68fe4` with Wi-Fi scope. This block owns initial-thread lifecycle
records in libc/kernel, retained return values, join/detach claims, wakeups and
focused tests. Preserve the initial kernel continuation and last-thread cleanup
even after its public pthread handle is collected. Next checkpoint is waiting
and late joins, detach/error ownership and matched full thread/libuv VM suites.
No physical trial is armed.


The initial-thread join correction is complete. A separate static libc record
uses exclusive claims and lazy detached collection. The kernel retains the
pointer return value and wakes waiting joiners; collecting the public handle
preserves the process-owned stack/TLS and last-thread cleanup continuation.
Self-inspection reports detach state, and names/signals reject collected handles.

Matched build, final host regressions and ASan/UBSan, targeted 24-lifetime
initial-join guest coverage, complete final thread suite and complete final
libuv suite pass. The libc initial-join refusal and kernel discarded-value
negative controls fail with exit 1 and 3; the fixed kernel was restored before
final suites. Documentation checks 641 local links with zero errors; whitespace
checks pass. Evidence is retained in `build/review/initial-join/`. No full
Linux/core, general SMP or native acceptance is claimed.

Continued source/host audit confirms that a valid negative absolute condition
deadline returns `EINVAL` (22) rather than `ETIMEDOUT` (110). The explicit
`--audit-negative-deadline` mode fails with exit 1 and is excluded from default
positive tests. Current guides state this edge case; the next bounded work is
absolute-deadline validation and timeout-path regressions. Once-initializer
abnormal exit remains unconfirmed. Disposable VMs stopped; no physical boot is
armed.

## Negative absolute condition deadline correction — active scope

The user authorized deadline validation, regression tests, docs/PR updates and
continued audit. Base is `cc955159`; fetched main remains `fa5a6cc1`. PR19 has
advanced to `b68df8cd` with bounded Wi-Fi diagnostics, without overlapping libc
scope. This block owns absolute condition-deadline validation and focused
host/guest tests. Preserve signed seconds, reject invalid nanoseconds, and route
expired deadlines through mutex release/reacquisition. Next checkpoint is
boundary/control cases, matched build and sequential thread/libuv VM suites.
No physical boot is armed.

The negative absolute condition deadline correction is complete. Signed seconds
reach the selected-clock comparison and normal mutex release/reacquisition path;
invalid nanoseconds remain rejected before unlocking. No layout/syscall change
is introduced.

Matched build, host regressions, ASan/UBSan, targeted guest coverage (15 expired
and six invalid deadlines), final full thread suite and final full libuv suite
pass. Two isolated host negative controls fail with exit 1: restored negative
seconds rejection and a timeout shortcut that skips the mutex transition.
Documentation checks 641 local links with zero errors; whitespace checks pass.
Evidence is retained in `build/review/absolute-deadlines/`. No full Linux/core,
general SMP or native acceptance is claimed.

Continued guest audit confirms that `clock_getres(clock, NULL)` incorrectly
returns `EINVAL` for realtime/monotonic clocks; CPU-clock probes and non-null
outputs succeed. The explicit `clock-resolution-audit` fails with guest status 1
and harness exit 3 and is excluded from `all`. Current guides document a real
output buffer as the workaround. Optional-output validation is the next bounded
correction. Once-initializer abnormal exit remains unconfirmed. All disposable
VMs have stopped; no physical boot is armed.

## Clock resolution optional output correction — active scope

The user authorized null-output clock resolution, regression tests, docs/PR
updates and continued audit. Base is `86498be3`; fetched main remains `fa5a6cc1`
and PR19 remains `b68df8cd` with separate Wi-Fi scope. This block owns libc
`clock_getres` validation and focused host/guest timing tests. Preserve unknown
clock errors, realtime/CPU source failures and existing resolution values. The
next checkpoint is modeled source/fallback tests, matched build and sequential
thread/libuv VM suites. No physical boot is armed.

The optional-output correction is complete. Realtime/monotonic resolution
queries accept null output while retaining unknown-clock and source errors;
CPU queries and resolution values are unchanged. No layout/syscall change.

Matched build, host timing/synchronization targets, final timing ASan/UBSan,
targeted clock-resolution guest group, complete thread suite and full libuv
suite pass. Three source-copy negative controls fail with exit 1 (original
rejection, unconditional null success, skipped realtime validation). Docs check
641 local links with zero errors; whitespace checks pass. Evidence, source
hashes and manifests are retained in `build/review/clock-resolution/`. No full
Linux/core, general SMP or native acceptance is claimed.

Continued source/host audit confirms a `times()` fallback defect: with the
high-resolution reference unavailable, 50 advancing modeled ticks produce a
zero elapsed return delta. Reference-enabled `times()` and fallback monotonic
`clock_gettime` controls pass. The explicit `--audit-times-fallback` fails with
exit 1 and is excluded from default positive checks. This configuration has
not been reproduced in the default VM or on native hardware. Current guides
state the monotonic-clock workaround. The next bounded correction is elapsed
source selection/fallback. Once-initializer abnormal exit remains unconfirmed.
Disposable VMs stopped; no physical boot is armed.

## times elapsed fallback correction — active scope

The user authorized `times()` fallback, regression tests, docs/PR updates and
continued audit. Base is `cb8985e6`; fetched main remains `fa5a6cc1` and PR19
remains `b68df8cd` with separate Wi-Fi scope. This block owns libc elapsed source
selection and focused host/guest timing tests. Preserve CPU accounting units,
usage errors and output ownership. Next checkpoint is reference/fallback and
boundary controls, matched build and sequential thread/libuv VM suites. No
physical boot is armed.

The `times()` elapsed fallback correction is complete. Valid reference readings
convert to 100 Hz ticks; unavailable reference time selects kernel ticks
directly. CPU accounting fields, optional output, error propagation and failure
output ownership are preserved. No layout/syscall or kernel change.

Matched build, host timing/synchronization targets, timing ASan/UBSan, targeted
`times` guest group, complete thread suite and full libuv suite pass. Three
source-copy negative controls fail with exit 1 (frozen return, wrong fallback
scaling, wrong CPU units). Docs check 641 local links with zero errors;
whitespace checks pass. Source/log hashes, manifests and receipts are retained
in `build/review/times-fallback/`. Guest targeted/full runs report the reference
source; absent-reference tests remain host-modeled. No full Linux/core, general
SMP, native or fallback-VM acceptance is claimed.

The bounded adjacent timing/accounting and timestamp-conversion review adds no
confirmed finding. Next audit scope is deterministic sleep interruption and
remainder behavior with unavailable clocks and boundary values. Abnormal
once-initializer exit remains unconfirmed; broader profile exclusions remain.
All disposable VMs stopped; no physical boot is armed.

## Sleep interruption and remainder audit — active scope

The user requested an audit, regression tests for confirmed issues, docs/PR
updates and continued review. Base is `820c1c3e`; fetched main remains `fa5a6cc1`
and PR19 remains `b68df8cd` with separate Wi-Fi scope. This block owns host/guest
sleep probes and documentation of observed contracts. Trace signal dispatch,
aliasing, remaining-time arithmetic, source fallback and boundary errors through
libc/kernel. Keep confirmed failing probes separate from positive default gates.
Next checkpoint is deterministic host cases and a matched guest reproduction for
any practical failure. No physical boot is armed.

The sleep audit confirms two pending remainder defects. P2: handler execution
is deducted from relative unslept time (24/24 modeled cases and six guest cases).
Guest signals arrived 25.553–32.760 ms into 500 ms requests; 600 ms handlers
left zero remainder. P3: extreme fallback tick scaling wraps (six modeled cases,
roughly 584,000 years; no VM/native occurrence). Libc/kernel runtime is unchanged.

Matched build, final host timing/synchronization tests, final timing ASan/UBSan
and complete positive thread suite pass, including six fast-handler remainder
controls. Explicit host handler/overflow audits fail with exit 1; the guest
handler audit returns status 1/harness exit 3. These are failure receipts and
are excluded from positive defaults. Docs check 641 local links with zero
errors; whitespace checks pass. Evidence is retained in `build/review/sleep-audit/`.
Prior libuv validation remains historical; no new full Linux/core, general SMP,
native or absent-reference VM acceptance is claimed.

Continued kernel wait/source and boundary review adds no third confirmed issue.
Next corrections are capturing suspension end before handlers, then splitting
fallback ticks before scaling. Once-initializer abnormal exit remains unconfirmed.
All disposable VMs stopped; no physical boot is armed.

## Sleep remainder corrections — active scope

The user authorized the signal-handler remainder fix and the P3 arithmetic fix
if bounded, regression tests, docs/PR updates and continued audit. Base is
`bd300e28`; fetched main remains `fa5a6cc1` and PR19 remains `b68df8cd` with
separate Wi-Fi scope. This block owns libc suspension-end capture and remainder
conversion plus focused timing tests. Preserve aliasing, pre-wait interruption,
error/errno and absolute-output semantics. The P3 fix is a small quotient/
remainder conversion in the same path. Next checkpoint is positive former
reproducers, boundary/reentrant controls and matched sequential guest suites.
No physical boot is armed.


Both sleep remainder corrections are complete. `nanosleep` captures unslept time
before post-wait handlers and retains the full request on pre-wait interruption;
a local snapshot survives nested sleeps and handler buffer changes. Tick fallback
splits seconds/fractions before scaling. Alias/error/absolute-output behavior
is preserved, with no layout/syscall or kernel change.


Validation of both corrections on ARM64 macOS/x86_64 QEMU TCG: matched build,
host timing/synchronization checks, timing ASan/UBSan, all 18 targeted sleep
remainder cases, the complete thread suite on retry and full libuv suite pass.
The targeted long/nested handlers retained 461818000–472246000 ns from interrupted
500 ms requests. Three isolated source-copy negative controls each exit 1:
restored old runtime, overflowing conversion and pre-wait handler subtraction.

The first full thread run failed before the sleep cases: an AHCI read command
reported the disk offline and the rename test returned EIO. A fresh disposable
VM using the same build passed the full suite. Both logs are retained; the
storage failure's cause is unresolved and a retry does not erase that failure.
Docs check 641 local links with zero errors; whitespace checks pass. Source/log
hashes, build manifest and test receipts are in `build/review/sleep-fixes/`.
No full Linux/core, general SMP, native or absent-reference VM acceptance is
claimed. All disposable VMs stopped; no physical boot is armed.

Continued bounded review covered wrapper rounding/errors, nested dispatch,
source/output ownership and initial futex wait/wake paths. No additional defect
was confirmed. Next scope is deterministic futex interruption/wake race coverage;
abnormal once-initializer exit remains unconfirmed. The observed AHCI failure
also remains an unresolved validation event, with both failure and retry receipts.

## Deterministic futex interruption and wake audit — active scope

User authorized deterministic race coverage, confirmed fixes, docs/PR updates and
continued review. Base `5503804e`; fetched main remains `fa5a6cc1`, and PR19's
`b68df8cd` retains separate Wi-Fi scope. Own the process futex wait/wake lifetime
and focused kernel/libc/guest regressions; inspect scheduler membership without
assuming general SMP acceptance. Next checkpoint is deterministic interruption,
pre-enqueue/re-wait queue reuse and wake ownership probes. No physical boot armed.


The bounded futex correction is complete. The deterministic fixture confirms
entry reuse could change the process/address binding while a syscall owned its
queue. Counted reservations now cover pre-enqueue and post-wake gaps, all return
paths and process-exit propagation. Final cleanup follows forced sibling
retirement. Wake lookup/count is IRQ-serialized as defensive hardening; the
current syscall gate already masks IRQs. No public layout/syscall change.

Full validation also confirmed a harness defect: child exit messages prematurely
ended the enclosing suite. The reader now waits for test completion/failure or
the monitor's top-level run receipt; five host regressions protect that behavior.


Final validation: matched build, host futex/synchronization/timing targets, five
smoke-reader tests, futex ASan/UBSan, targeted futex and main-join VM groups,
complete thread suite with the corrected harness and full libuv suite pass.
Three kernel source-copy controls abort at the intended assertions (old runtime,
restored wake gap and Boolean reservation); the old smoke reader fails its host
regressions. Docs check 641 local inline links with zero errors; whitespace
checks pass. Source/log hashes and build/test receipts are retained in
`build/review/futex-audit/`.

Retained unsuccessful runs: two full captures were truncated by the old harness;
a subsequent corrected-harness run encountered an AHCI read failure/disk offline
during fork, before the futex group. The fresh-VM retry passed. The storage
failure remains unresolved, consistent with the preceding sleep-fix checkpoint;
a retry is not evidence that it is fixed. Earlier test-development build and
signal-poll expectation errors are retained locally with the final passing runs.
No full Linux/core, general SMP or physical-hardware acceptance is claimed.
All disposable VMs stopped; no physical boot is armed.

Continued review traced syscall entry, condition recomputation, scheduler queue
removal and process-exit ownership without another confirmed runtime finding.
Next scope is raw futex timeout overflow and mapped-address lifetime. Abnormal
once-initializer exit and broader exclusions remain open; the AHCI fault is still
an unresolved validation event.

## Futex deadline and mapped-address audit — active scope

User authorized overflow/lifetime checks, confirmed fixes, tests and docs/PR
updates. Base `cca88cc0`; fetched main remains `fa5a6cc1`. PR19 advanced to
`44f41e72` with separate channel1 Wi-Fi candidate scope. Own futex deadline
arithmetic and safe word access/invalidation across mapping changes, plus focused
host/guest tests. Next checkpoint is boundary and removal/reprotection/remap
reproductions. Keep the current CPU0 IRQ model explicit; no physical boot armed.


Completed scope: reject overflowing relative futex deadlines before reservation;
validate/load words with CPU0 IRQ exclusion; invalidate and wake old bindings on
page removal or successful user-access revocation. Old calls fault even after
address replacement/restoration; readable or failed protection changes preserve
waits. No public layout/syscall-number change. Host regressions cover boundary
arithmetic and exact mutation interleavings; guest tests cover 20 mapping lifetimes.


Final validation for the deadline/lifetime correction: matched kernel/userspace
build; host futex, synchronization and timing contracts; five smoke-reader tests;
futex ASan/UBSan; targeted futex-lifetime guest; complete thread suite; and full
libuv consumer suite all pass. The full thread suite passed on its first run,
without recurrence of the historical AHCI failure. Both pre-fix probes fail and
all five source-copy controls compile then abort at their intended assertions.
Docs check 641 local inline links with zero errors; whitespace checks pass.
Build/test receipts and source/log hashes remain in `build/review/futex-lifetime/`.
No full Linux/core, general SMP or physical-hardware acceptance is claimed.
All disposable guests stopped; no physical boot is armed.

Continued review covered page removal/protection callers, mapping refresh and
rollback, MAP_FIXED refusal of live-region replacement, libc storage assumptions
and exec's refusal while worker records exist. No further confirmed defect.
Next checkpoint: futex table saturation/fairness and once-initializer exit.
The prior AHCI disk-offline event remains unresolved.


## Synchronization audit closeout — active scope

User authorizes completion of the documented synchronization audit, confirmed
fixes/regressions, docs/PR updates and merge of PR175 after validation. Base
`f5f6328f`; fetched main remains `fa5a6cc1`, Pro PR19 remains `44f41e72` with
separate Wi-Fi scope. Own futex saturation/fairness verification, once-initializer
exit recovery and a final pass over supported pthread/timing contracts and the
PR diff. Broader filesystem/network/native investigations remain separate from
this bounded audit. Next checkpoint: failing once-exit probe, saturation and
scheduler wake-policy evidence. No physical boot armed.


Completed closeout: fixed once-initializer exit with a private TLS frame chain,
reset/wake before destructors and nested/initial-thread coverage. Final review
also confirmed and fixed recursive mutex depth overflow and false semaphore
try-wait failure after a concurrent post. Capacity tests preserve existing futex
bindings across process pressure; scheduler tests characterize slot-order wakes
without promising FIFO or starvation freedom. No new public layout/syscall ABI.


Final closeout validation: matched build, host synchronization/futex/timing
contracts, five smoke-reader tests, synchronization/futex ASan+UBSan, x86_64
scheduler fixture under Rosetta, targeted once-exit guest, complete thread suite
and full libuv suite all pass. Five source-copy negative controls compile and
fail their intended assertions. Three pre-fix probes reproduce the new findings.
The complete thread suite passed first time; the historical AHCI disk-offline
failure did not recur and remains unresolved. Docs check 645 local inline links
with zero errors; whitespace checks pass. Source/log hashes, matched image hashes
and receipts remain under `build/review/sync-closeout/`.

Retained unsuccessful attempts: the first once guest used an incorrect subprocess
path (fixed to `/fat/bin/threadstress`); native ARM64 scheduler compilation rejected
existing x86 instructions, and an initial x86 command omitted the Limine include
path. The explicit x86_64/Rosetta fixture passes. None is recorded as a guest
runtime regression or full Mac test-core acceptance. All disposable VMs stopped.
No full Linux/core, general SMP or physical-hardware acceptance is claimed.

The bounded build/docs and synchronization/stack/timing audit is complete, with
nineteen corrected runtime findings and the smoke-reader correction. Broader
filesystem/network/native issues, Node recovery and Mac test-core portability
remain separate tracked work, as specified in the audit closeout matrix. PR175
is ready for the user-authorized merge after publishing this validated commit.
No physical boot or installation is armed; Actions remain disabled by policy.


## Historical AHCI disk-offline investigation — active scope

User authorizes reproductions, confirmed fixes, regression coverage, docs and a
new draft PR. Branch `device/apple-silicon/ahci-offline` starts at merged main
`f0cd6409`; PR19 at `44f41e72` retains separate Wi-Fi ownership. Review the two
retained READ DMA EXT failures during full thread runs, AHCI polling/deadlines,
service handoff and completion/error ownership. Use disposable x86_64 QEMU guests
and host driver models; do not change native gates or persistent VM disks.
Next checkpoint: classify the historical evidence and obtain a directed failing
reproduction before changing runtime behavior. No physical boot armed.


Two diagnostic-only full-thread runs passed without spontaneous offline. The
host driver model confirmed a stale completion/readiness decision at bounds and
carryover of the pre-submission service budget. The candidate rechecks phase
state once at bounds, preserves the final TFES check and resets the completion
phase's budget. Genuine failures keep the existing quarantine/pinned DMA policy;
limits and native gates are unchanged. New diagnostics record phase, reason,
sector, last elapsed sample, counters and port registers.

AHCI and block/cache host tests and AHCI ASan/UBSan pass under x86_64 Rosetta.
Four source-copy controls compile and fail the intended assertions. The
storage-service VM passes throttled I/O, USB input, cancellation, copy comparison,
fsck, three executable relaunch cycles and forced timeout/quarantine. Its genuine
pending write times out at 2,000,000 us with CI=1, IS=0, TFD=0x50 and SERR=0;
one failure is counted and later storage is refused. Historical READ DMA EXT
failures remain unattributed; these reproductions do not establish their cause.
Evidence is retained in `build/review/ahci-offline/`; no physical trial is armed.


Final candidate full-thread suite passes on its first run, with no spontaneous
AHCI failure. Documentation checks 648 local inline links with zero errors;
whitespace checks pass. Matched build, AHCI/block host checks, sanitizer and
storage-service receipts are saved with source/image/log hashes. The initial
diagnostic build rejected duplicate unused local declarations introduced while
adding diagnostics; those were removed before VM tests. That failed attempt is
retained alongside the passing build. All disposable VMs are stopped.


## AHCI repeat campaign — active scope (2026-10-26)

User authorizes repeated diagnostic VM tests and confirmed fixes on draft PR178.
Source is `8f7a9434`, based on `origin/main` `f0cd6409`; fetched remote refs and
open PRs show PR19 retains separate Wi-Fi work. Run six full-thread guests, two
complete storage-service guests (including deliberate timeout injection) and one
ten-cycle fsck/relaunch/comparison guest, sequentially on disposable disks.
Retain per-run logs and source/image hashes under `build/review/ahci-repeat/`.
Stop to classify any failure before changing code. If all runs stay clean, shelve
the historical incident as non-reproduced, preserve confirmed fixes and evidence,
and update investigation/status docs and draft PR178. No native trial is armed.

Repeat campaign complete: all nine fresh guests pass (six full-thread, two full
storage-service, one ten-cycle relaunch). No spontaneous offline event occurred.
Both deliberate write timeouts have completion/deadline receipts, pending CI=1,
IS=0, TFD=0x50, SERR=0 and exactly one failure each; subsequent sync is refused
and the shell remains responsive. All guests stopped and template hashes remain
unchanged. No new runtime finding or additional regression case was confirmed.

Active historical investigation is shelved as non-reproduced at the user's
direction, with cause still unknown. Preserve the existing corrections/tests,
original failed runs and the new per-run evidence under `build/review/ahci-repeat/`.
Update draft PR178 with this documentation-only checkpoint; no merge requested.
Next step is other project work unless new diagnostics justify reopening AHCI.
No physical trial is armed; Actions remain disabled.


## Eventfd/epoll audit — active scope (2026-10-26)

PR178 merged as `8bd43adc`; the clean checkout fast-forwarded to that baseline.
Branch `device/apple-silicon/eventfd-epoll-audit` takes the deferred experimental
descriptor-adapter review. PR19 retains independent Wi-Fi ownership. Scope is
`posix_node_compat.c`, libc descriptor close integration, focused host probes and
current-profile documentation: first establish lifecycle, wait and counter-bound
behavior with deterministic executable evidence. This is an audit checkpoint,
not acceptance of the full Linux API or Node port. Preserve native and Node
recovery work as separate follow-ups. No physical trial or Actions run is armed.

Audit checkpoint: three unfixed finding groups are confirmed by seven directed
host probes (all exit 1 at the intended assertion): epoll lifetime/reuse, immediate
wait timeouts and eventfd counter bounds. Real adapter code and public close are
compiled with modeled native services. Positive controls and ASan/UBSan pass;
no runtime changed and no new guest acceptance is claimed. Current profile/status
and the dedicated audit report now state these concrete limitations. Next bounded
correction is epoll ownership across close, duplication and descriptor reuse.
Evidence is in `build/review/event-adapters{,-audit,-sanitized}/`. No VM is running
from this work, no physical boot is armed, and Actions remain disabled.

Validation: `sh tools/macos/build.sh test-event-adapters` passes; sanitized
controls pass without diagnostics; explicit `--audit all` exits 1 with seven
intended failed contract assertions. Documentation checks 653 local inline links
with zero errors, and whitespace checks pass. Publish this audit-only checkpoint
in a new draft PR; it does not contain runtime corrections.


## Epoll lifetime correction — active scope (2026-10-26)

User authorizes close/duplication/reuse fixes, regression tests, docs and draft
PR179 update, then continued audit. Baseline is `526b6353` on merged main
`8bd43adc`; fetched PR179 matches and PR19 retains separate Wi-Fi ownership.
Scope: libc event adapter and close/dup/F_DUPFD integration, identity/alias
tracking and wait snapshots, host and matched-guest regressions. No new public
syscall ABI is planned. Preserve the separate unfixed wait/counter findings.
Next checkpoint: positive lifetime tests including duplicate-before-registration,
replacement, failure rollback and concurrent wait/control interleavings.

Lifetime correction implemented: protected libc identities cover native dup,
dup2, dup3 and F_DUPFD*, final-alias retirement, close-error cleanup and number
reuse. Wait snapshots validate instance/watch/descriptor identity after polling.
The original three lifetime probes and four broader lifetime modes pass with
ordinary controls and ASan/UBSan. Targeted and full-thread guests pass, including
concurrent lifetimes and post-cleanup fork. Four source-copy negative controls
compile and fail the intended assertions. Continued audit adds a fifth remaining
wait/counter failure: an eventfd post during the modeled sleep is missed.
No kernel/syscall ABI or native gate change; raw descriptor mutations, rights
transfer and general Linux epoll semantics are outside this bounded acceptance.


Final revision validation: matched build, eight positive host modes and their
ASan/UBSan runs, complete thread guest (including epoll lifetime), libuv guest
and five smoke-reader tests all pass. Four negative controls compile and fail
the intended assertions. The five explicit remaining wait/counter probes fail
as documented. Documentation checks 653 local inline links with zero errors;
whitespace checks pass. Source/build/log hashes and all directed receipts are
retained under `build/review/epoll-lifetime/`. All disposable guests stopped.
No physical trial is armed and Actions remain disabled. Draft PR179 now carries
the lifetime correction; the next bounded item is wait/notification handling.

## Epoll wait correction — active scope (2026-10-26)

User authorized proceeding with the wait/notification findings. Baseline is
`b51143cb`; fetched main remains `8bd43adc`, PR179 is draft at this branch head,
and PR19 retains separate Wi-Fi ownership. This block owns the libc epoll wait
loop, monotonic timeout accounting, rechecking synthetic readiness and control
changes, focused host/guest regressions and current docs/PR updates. Preserve
identity/revision validation and keep signal delivery outside the lifecycle lock.
Use bounded native poll intervals for synthetic state, since it has no kernel
notification source; document this latency/efficiency limitation. Counter bounds,
full Linux epoll semantics and native acceptance remain separate. Next checkpoint
is deterministic wait/deadline/interruption coverage, then a matched build and
sequential guest thread/libuv checks. No physical trial or Actions run is armed.


Final wait validation: matched `all` build, sixteen positive host modes and
ASan/UBSan, targeted epoll-wait guest, complete thread guest, full libuv consumer
guest and five smoke-reader tests pass. Five compiled negative controls restore
zero-timeout sleeping, ready-event delay, premature infinite-wait return, missing
deadline clamping or stale counter readiness; each fails its intended assertion.
Three explicit counter probes fail as documented, including the new full-counter
writable case. Local manifests, source/log hashes and receipts are retained under
`build/review/epoll-wait/`. All disposable VMs stopped. No kernel/syscall ABI,
physical acceptance or Actions setting changed. PR179 remains draft; next bounded
correction is eventfd counter bounds and matching writable readiness.

## Eventfd counter correction — active scope (2026-10-26)

User authorized proceeding with counter bounds and writable readiness. Base is
`1f83d29f`; fetched main remains `8bd43adc`, PR179 is draft at this branch head,
and PR19 retains separate Wi-Fi ownership. Scope: checked eventfd addition,
reserved-value rejection and matching EPOLLOUT capacity, host boundary/error
regressions, concurrent public-API guest coverage and current docs/PR updates.
Keep the existing lifecycle mutex and wait behavior. Blocking I/O, semaphore
mode and flag semantics remain separate audit areas; no new kernel ABI or native
acceptance is planned. Next checkpoint is preservation/readiness boundary tests,
then matched build and sequential thread/libuv VMs. Actions remain disabled.


Final counter validation: matched `all` build, twenty-one positive host modes
and ASan/UBSan, targeted eventfd-counter VM, complete thread suite and full libuv
consumer VM pass. Four compiled negative controls break reserved-value rejection,
capacity enforcement, writable readiness or failed-write preservation; all fail
their intended assertions. Five smoke-reader tests pass. Documentation checks
655 local links with zero errors; whitespace checks pass. Baseline three-counter
failures and two newly confirmed semaphore/flag failures are preserved separately.
Local source/build/log hashes and receipts are under `build/review/eventfd-counter/`.
All disposable guests stopped; no physical trial, ABI or Actions setting changed.
PR179 remains draft. Next bounded scope is eventfd semaphore mode and creation
flag validation; blocking I/O and broader Linux adapter semantics remain separate.

## Eventfd semaphore/flags correction — active scope (2026-10-26)

User authorized proceeding with semaphore reads and creation-flag validation.
Base is `e37213bf`; fetched main remains `8bd43adc`, PR179 matches and stays draft,
and PR19 retains Wi-Fi ownership. Scope: private eventfd mode storage, one-token
reads under the existing mutex, validation before slot allocation, host tests for
flag combinations/lifetime/readiness and concurrent guest consumption. Update
current guides and PR179, then continue the bounded adapter audit. Blocking I/O
and kernel notification remain separate; no new ABI or physical acceptance.
Next checkpoint is host regressions and matched sequential thread/libuv VMs.
Actions remain disabled and historical failed evidence must be retained.


Final mode/flag validation: matched `all` build, twenty-five positive host modes
and ASan/UBSan, targeted eventfd-modes VM, complete thread suite and full libuv
consumer VM pass. Four compiled negative controls break flag rejection, mode
storage, one-token subtraction or fcntl mode independence; each fails its intended
assertion. Five smoke-reader tests pass. Documentation checks 655 local links with
zero errors; whitespace checks pass. The baseline's two semaphore/flag failures
and the two new fairness/hangup failures are retained separately, under
`build/review/eventfd-modes/`, with source/build/log hashes. All disposable guests
stopped. PR179 remains draft; next bounded scope is epoll ready-set rotation and
zero-interest native error/hangup reporting. Blocking I/O, kernel notification,
physical acceptance and Actions settings remain unchanged.

## Epoll rotation/hangup correction — active scope (2026-10-26)

User authorized proceeding with ready-set fairness and zero-interest hangups.
Base is `87c48440`; fetched main remains `8bd43adc`, draft PR179 matches, and PR19
retains separate Wi-Fi scope. Own libc epoll delivery rotation and unconditional
native HUP/ERR observation, host stable-set/alias/control regressions and matched
guest coverage. Keep lifetime/revision validation and monotonic wait deadlines.
Use a per-instance cursor shared by aliases, advance only on delivered events,
and select against its current value under the lifecycle mutex. Snapshot all
watches so native/synthetic ordering cannot bias delivery. No new ABI, hardware
trial or Actions run. Next checkpoint: deterministic regressions, then matched
build and sequential thread/libuv VMs; continue auditing adjacent adapter modes.


Final rotation/hangup validation: matched `all` build, twenty-nine positive host
modes and ASan/UBSan, targeted epoll-rotation VM, complete thread suite and full
libuv consumer VM pass. Four compiled negative controls break cursor advancement,
full snapshot scanning, empty-mask polling or current-cursor selection; each
fails its intended assertion. Five smoke-reader tests pass. Documentation checks
655 local links with zero errors; whitespace checks pass. Baseline failures,
initial fixture corrections and two new one-shot/priority failures are retained
under `build/review/epoll-rotation/` with source/build/log hashes. All disposable
VMs stopped. PR179 remains draft. Next bounded scope is one-shot disabling/rearm
and native priority-bit conversion; broader event modes, signal masks and blocking
eventfd I/O remain separate. No physical acceptance or Actions setting changed.

## Epoll one-shot/priority correction — active scope (2026-10-26)

User authorized proceeding with one-shot disabling/rearming and priority mapping.
Base is `9a0f51a9`; fetched main remains `8bd43adc`, draft PR179 matches, and PR19
retains separate Wi-Fi scope. Own private per-registration armed state, delivery
serialization with overlapping waits, MOD rearming/revision checks, and both
EPOLLPRI/POLLPRI conversions. Add deterministic host and matched guest regressions,
update current docs/PR179, then continue the adapter audit. Preserve rotation,
timeouts and descriptor identities. Priority mapping tests model a native source;
no unsupported device/urgent-data capability is implied. Edge delivery, signal
masks and blocking eventfd I/O remain separate. No ABI/hardware/Actions changes.


Final one-shot/priority validation: matched `all` build, thirty-three positive
host modes and ASan/UBSan, targeted epoll-oneshot VM, complete thread suite and
full libuv consumer VM pass. Six compiled negative controls break disarming,
MOD rearming, disabled snapshot/delivery checks or either priority conversion;
each fails its intended assertion. Five smoke-reader tests pass. Documentation
checks 655 local links with zero errors; whitespace checks pass. Baseline failures
and the new EPOLLET failure remain separate under `build/review/epoll-oneshot/`,
with source/build/log hashes. All disposable VMs stopped. PR179 remains draft.
Next bounded scope is an honest edge-mode contract; blocking eventfd I/O, signal
masks, kernel notifications and native priority-source support remain separate.
No public ABI, physical acceptance or Actions setting changed.

## Epoll edge-mode contract — active scope (2026-10-26)

User authorized fixing the confirmed EPOLLET defect, regressions, docs/PR179 and
continued audit with a coverage inventory. Base `aba0999a`; fetched main remains
`8bd43adc`, draft PR179 matches and PR19 retains separate Wi-Fi ownership. Reject
EPOLLET on ADD/MOD with EOPNOTSUPP before mutating identity/watch state; preserve
level and one-shot behavior, including failed MOD on a disabled watch. Own only
the adapter, host/guest regressions and current audit documentation. Validate
sequentially with host/sanitizers, matched build and disposable thread/libuv VMs.
Actual edge notifications, signal-mask waits and blocking eventfd remain separate.
No ABI, hardware acceptance or Actions changes.

Final edge-contract validation: all 34 host modes pass normally and with
ASan/UBSan. Four compiled negative controls fail the intended refusal or
state-preservation assertions. Matched `all` build, targeted epoll-edge VM,
complete thread suite and full libuv consumer VM pass sequentially. Five
smoke-reader tests pass; docs check 664 local links with zero errors and
whitespace checks pass. Baseline failure, source/build hashes and logs are
retained under `build/review/epoll-edge/`. All disposable guests stopped.
The current audit inventory separates completed bounded work from the event
adapter queue and broader OS reviews. Next scope is epoll_pwait mask semantics;
edge delivery remains explicitly unsupported. PR179 stays draft; no physical
acceptance, public ABI or Actions setting changed.

## Event-adapter completion and merge — active scope (2026-10-26)

User authorizes completing the bounded event-adapter audit and merging PR179.
Base `c98dcded`; fetched main remains `8bd43adc`. PR179 is mergeable with no
required checks reported; Actions remain disabled. PR19 retains separate Wi-Fi
ownership and is not part of this merge. Use the WNP development completion
criteria in the existing audit inventory. Required: correct epoll_pwait masks,
blocking eventfd and native poll integration; check remaining event/control
contracts and ownership boundaries, rejecting unsupported modes explicitly.
Own libc event/poll glue, regression fixtures/guests and current docs. Preserve
public syscall ABI and device gates. Sequential host/sanitizer, matched build,
thread/libuv VM gates and final PR review precede merge. Actual edge notifications,
cross-process adapter sharing and full Linux parity are outside this target.

Closeout ownership review also covers process/rights glue: refuse adapter-handle
SCM_RIGHTS export and exec/spawn inheritance of non-CLOEXEC adapters, rather than
allowing a kernel placeholder with lost libc state. Pseudo-socket epoll registration
is explicitly refused until its connect/listener transitions share lifecycle
serialization; native socketpair readiness remains covered. Raw syscalls and
native watched descriptors transferred before/after registration remain outside
the process-local ownership contract. No general transfer-tracking ABI is added.

## Closeout validation and delivery

The bounded process-local event-adapter audit is complete. All 46 host modes pass
normally and under ASan/UBSan. Ten compiled negative controls fail their intended
mask entry/restoration, blocking retry/generation, signal dispatch/restart, poll
identity/immediacy, normal-event-bit or export-guard assertions. Native poll/select
host regressions and five smoke-reader tests pass. The matched x86_64 build,
corrected targeted event-adapter-final guest, complete thread suite (all eight
event-adapter groups) and full libuv consumer suite pass sequentially. Docs check
664 local links with zero errors; whitespace checks pass. No unresolved confirmed
defect remains within this selected contract. Failed diagnostic runs remain
separate evidence, not passes.

Source/build/log hashes and the validation receipt are retained under
`build/review/event-adapter-closeout/`. All disposable guests stopped. Delivery is
[PR179](https://github.com/PaulMDemers/SrvrOS/pull/179), whose GitHub record is the
source of the final merge state/SHA. The user authorized merge after these gates;
PR19 remains separate Wi-Fi work. No kernel/syscall ABI, native-hardware acceptance
or Actions setting changed. Broader filesystem/network/process/security reviews
and optional adapter capabilities remain in the current audit inventory.

## Broader audit: exFAT import contracts — active scope (2026-10-26)

User requests resuming the broader system audit. Main is `dfd63fef`; PR181 owns
desktop lifecycle work, PR180 owns Air6,1 retest/poll evidence and PR19 Wi-Fi.
This branch owns kernel exFAT import validation and initialized-data reads, host
fixtures, current docs and disposable VM validation. Source review identifies
unchecked boot shift/geometry fields and ignored ValidDataLength. First checkpoint:
reproduce both, correct confirmed defects with mount/read/rename/remount coverage,
then publish a draft PR. Preserve mutation ordering/quarantine and existing
hardware gates. No changes to the other agents' poll/UI/device implementations.
Acceptance: bounded source-level host tests/sanitizers, matched build and relevant
thread/libuv VMs, documentation checks and explicit remaining audit queue. Full
filesystem/security/crash consistency certification and native trials are excluded.

### Import-contract checkpoint results

The [filesystem audit](../filesystem-audit-2026-10-26.md) now records two fixes:
checked single-FAT boot geometry and zero-filled reads beyond imported
`ValidDataLength`, including rename/replacement preservation and overwrite cache
publication. Baseline: one control passed and thirteen directed modes failed.
Final: sixteen host modes pass normally and under ASan/UBSan; six independently
compiled negative controls fail their intended runtime assertions. The existing
entry-layout test passes with ARM host-test/Darwin linker options.

The matched build, dedicated imported-data VM and complete thread suite pass.
The libuv run with its default ten-second completion budget ended after
`fs poll ok` (exit 3); the failed log is retained. A fresh 60-second-budget run
passed all required markers. Evidence, source/output hashes and receipts live in
`build/review/filesystem-audit/`. No physical trial or Linux exfatprogs crash
campaign is claimed. GitHub Actions remain disabled.

During closeout PR180 merged into `main` (`afc8bb67`); its header/test/documentation
changes do not overlap this scope and were fast-forwarded into this branch.
PR181 and PR19 remain separate. The next checkpoint is entry integrity and
parser structure: source inspection plus a read-only fixture check confirms that
the generated disk's 169 root entries have unset checksums, while mount accepts
them. Allocation ownership, unmount lifetime and crash recovery follow; the
broader OS audit remains open.

The post-PR180 matched rebuild and all three VMs pass: dedicated import (16.47 s),
complete threads (39.69 s), libuv with the 60-second completion budget (10.01 s
including boot). `integrated-vms.json`, the integrated build receipt and runtime
hash inventory distinguish these runs from the earlier baseline. Documentation
link/gallery and whitespace checks pass. This checkpoint is ready for draft PR
review; the listed follow-up defects are not represented as fixed.

Published runtime commit `a8d1617b` in [draft PR182](https://github.com/PaulMDemers/SrvrOS/pull/182).
The broader audit remains active at the next entry-integrity checkpoint.

### Entry-integrity audit — active scope (2026-10-26)

User authorized the next checkpoint on PR182. Base is `352f9b0d` with main at
`afc8bb67`; refreshed PR181 remains desktop lifecycle and PR19 Wi-Fi. This pass
owns exFAT entry-set integrity/structure, bounded ASCII name import, generated
image entries and directed host/VM fixtures. Reproduce corrupt-checksum acceptance,
malformed stream/name layouts and unsafe names before correction. Fix the packer
and parser together; retain explicit unsupported-format refusal and document the
need to rebuild older generated disks. Acceptance: actual driver regressions and
sanitizers, independent generated-entry checks, matched build, import/thread/libuv
VMs and docs. Allocation cross-link ownership, unmount lifetime, full Unicode,
boot-region recovery and crash consistency remain subsequent checkpoints.

### Entry-integrity checkpoint results

The generator now emits correct file-entry checksums, name hashes and allocation
flags, and avoids allocating empty streams. The driver checks basic stream/name
layout, checksum, ASCII name identity/hash and empty-stream flags before
publication. Additional secondary extensions and unsupported names are refused.
Directory scans now reject traversed cycles, invalid FAT successors and chains
shorter than their declared directory data. Older generated disks must be rebuilt
with the matching kernel; persistent workspace/recovery media were not modified.

Ten new modes failed on the previous implementation. All 29 current host modes
pass normally and under ASan/UBSan; seven new compiled negative controls are
caught. Four generator groups and an independent recursive check of all 372 sets
in 16 directories pass. The existing entry-layout test, matched build and all
four VMs pass sequentially: import 16.48 s, corrupt-entry/fallback 16.35 s,
complete threads 40.39 s, libuv 10.22 s (60-second completion budget).

The first corruption harness wrongly assumed refusal prevented all application
startup. Its retained failure showed the intended clean initramfs fallback. The
corrected `exfat-refused` guest test verifies rejected paths are absent and the
fallback remains readable, alongside explicit mount-refusal/fallback diagnostics.
No timeout is counted as a pass. Evidence and tested source/output hashes are in
`build/review/exfat-entry-audit/`; documentation and whitespace checks pass.

PR182 remains the delivery branch. Next: allocation/cross-link ownership,
duplicate paths, bitmap/FAT disagreement and unmount/open-reference lifetime.
Source review confirms monitor and workspace callers have busy guards; trace
those against the direct filesystem teardown and other callers before declaring
a lifetime defect. Boot/upcase recovery, full Unicode and power-loss atomicity
remain outside this checkpoint. No physical tests or Actions dispatch occurred.

### Allocation/namespace audit — active scope (2026-10-26)

User authorized the next PR182 checkpoint. Base is `0ba50fe9`; refreshed main
`9f18d7d6` adds the Air6,1 failed stress report only. PR184 owns worker-pipe
admission, PR181 desktop lifecycle and PR19 Wi-Fi. This pass owns exFAT namespace
admission, imported allocation checks and exact-alias release, plus focused
host/VM evidence and docs. Reproduce duplicate-name and overlapping-mount damage,
shared-allocation release and writable acceptance of inconsistent ownership.
Preserve generated exact aliases; quarantine unsupported/corrupt allocation
layouts before mutation. Inspect open-reference teardown callers without assuming
a defect merely because the low-level unmount API lacks its own process check.
Acceptance: directed actual-driver tests, sanitizers/negative controls, matched
build and disposable guest consumers. Cross-device physical aliasing, general
hot-unplug, power-loss recovery and other agents' process admission code are out
of scope. No hardware or Actions dispatch is requested.

### Allocation/namespace checkpoint results

All thirteen initial modes reproduced defects at the previous runtime: duplicate
names were accepted, overlapping/occupied mounts could remove unrelated VFS
entries, surviving exact aliases lost data/allocation ownership, and inconsistent
allocation maps remained writable. The driver now admits exclusive canonical
mount namespaces, rejects duplicate imported names, retains published aliases'
extents until last-owner retirement, and checks supported allocation maps before
allowing mutation. Checker failures/inconsistencies latch read-only mode.

All 47 host modes pass normally and under ASan/UBSan; eight new compiled negative
controls are detected. Fault cases cover mount allocation retry, checker I/O/OOM,
pre-publication overwrite cleanup and uncertain-write quarantine. The initial
fault test's clean-map expectation after a metadata-write failure was incorrect:
existing quarantine deliberately retains the uncertain new allocation. Its failed
log is preserved; separate read/write failure tests now verify both contracts.

The matched build, independent 372-entry image check and existing entry-layout
test pass. Six sequential disposable VMs pass: exact aliases 16.52 s, corrupted
bitmap quarantine 16.43 s, import 16.53 s, corrupt-entry fallback 16.41 s, complete
threads 40.35 s, and libuv 10.17 s (60-second completion budget). Evidence and
source/output hashes are in `build/review/exfat-ownership-audit/`. Docs/whitespace
checks pass. Main's documentation-only PR183 was merged into this branch before
implementation; no other agent's runtime changes were incorporated.

The normal monitor/workspace/workspace-image busy guards cover retained linked
file/directory descriptions and current directories. Unlinked/replaced snapshots
are promoted to process-owned storage and detached from backend nodes. No new
open-reference defect was confirmed by this trace; no process-admission edits
were made. Caller serialization, overlapping physical device views, raw leases
and removal remain next, followed by recovery/crash consistency. This is bounded
source/host/VM acceptance, not a complete filesystem or hardware audit.

### Allocation checkpoint integration verification

After checkpoint `28bdb374` was pushed, main `0858e8e6` incorporated PR181's
desktop lifecycle and PR185's UART/startup trace updates. The only merge conflict
was the appended Makefile test targets; both target groups were preserved.
No filesystem implementation/test source changed during integration. The matched
build, 47 exFAT host modes, four generator groups, 40-case shared-window host test
and 372-entry image check pass against that baseline.

Six integrated VMs pass sequentially: aliases 16.46 s, bitmap quarantine 16.39 s,
import 16.45 s, corrupt-entry fallback 16.33 s, complete threads 39.89 s and libuv
10.06 s. Integrated logs, VM receipts and source/output hashes are retained in
`build/review/exfat-ownership-audit/`. PR184/19 runtime work remains separate.
No desktop UI, physical hardware or Actions acceptance is claimed by these runs.
The remaining queue is unchanged: overlapping device views, raw leases,
serialization/removal and recovery/crash consistency.

### Device-view/raw-lease audit — active scope (2026-10-26)

Continue PR182 from `0654615b`, integrating actual main `5bcd061c` (PR184
worker-pipe admission) before implementation. Own block range identity/claims,
exFAT mount admission and workspace scratch/image lease handoff, with directed
host regressions and matched disposable VMs. PR19 Wi-Fi work stays separate.
Reproduce overlapping mounts and raw-lease remounts, preserve disjoint partition
mounts and the authorized inner-image mount, and test release/failure/retry.
No hardware, Actions dispatch, general hot-unplug or crash-repair work is requested.

### Device-view/raw-lease checkpoint results

Confirmed overlapping block mounts and a parked outer-volume remount during an
active image lease against the previous runtime. Declared physical view identity
and range claims now reject overlap before media reads. Raw scratch/image leases
reserve the entire transport, delegate only the designated image view, reject
sibling-mount handoff and retain poisoned exclusion until reboot. Clean
refusal/close releases claims; stale tokens cannot release later owners.

All 51 import modes pass normally and under ASan/UBSan. The real block/partition,
workspace and USB callback tests pass; seven synthetic real image-adapter runs
cover successful mutation, two pause points, partial-write/flush quarantine and
two clean refusals/retries with outer-byte preservation. Block/workspace/image
adapters pass sanitizers. Six compiled negative controls are detected. Existing
AHCI host tests pass under x86_64 host execution. The first alias probe had an
incorrect assertion placement; only the corrected overlap failure is counted.
Unrelated aggregate Mac linker/assembly/sync-command failures are retained; the
focused Darwin/x86 wrapper recipe is in the Mac guide. No full Linux host-suite
or exfatprogs fault-sweep acceptance is claimed.

Matched build and 372-entry image validation pass. Six sequential VMs pass:
aliases 16.57 s, bitmap quarantine 16.42 s, imports 16.43 s, corrupt-entry fallback
16.40 s, complete threads 39.75 s and libuv 10.13 s. Evidence is retained in
`build/review/storage-claims/`, including tested hashes and build/VM receipts.
The checkout includes main `5bcd061c`/PR184. PR19's workspace-log changes were
inspected for overlap and remain separate; no logging files were changed here.

Follow-on source tracing found registered USB/AHCI devices are quarantined and
retained on failure, with no general runtime unregister/reuse path. No additional
device use-after-free was confirmed. Hot removal/generation changes, caller
serialization and mutation recovery/crash consistency remain next. No native
hardware, battery/EC trial or Actions dispatch occurred.

### Identity invalidation/teardown audit — active scope (2026-10-26)

Continue PR182 from `a35e2c33`; integrate main `19a6fe05` (PR186's Air6,1
documentation) before implementation. Inspect runtime media identity failures,
cache admission after quarantine, and exFAT operation/teardown serialization.
Registered-device reuse/hot-plug remains unsupported; add no recovery/rebind API
without a confirmed need. Own block/USB/AHCI health propagation, filesystem
lifecycle guards and focused host/VM evidence. PR187 desktop timing and PR19
Wi-Fi/workspace logging are separate. No hardware or Actions dispatch.

### Identity invalidation/teardown checkpoint results

Five original probes confirmed teardown during foreign ownership, read/write
callbacks, mount scans and checks. The filesystem now holds CPU0 storage
ownership for each complete backend operation, rejects same-owner callback
teardown and rechecks process files/directories/cwds under the teardown owner.
Boot-time fallback and normal nested metadata operations remain supported.

A published USB failed-command status previously allowed later writes using the
old geometry. Read/write/flush probes reproduce it. Runtime status/transport
failures now invalidate the block lineage, as do AHCI uncertain completions.
Cached aliases, writes/flushes, claims and parked-workspace admission cannot bypass
terminal invalidation. Published USB storage objects also refuse repeated attach
before any probe; the old entry point accepted unsupported republication. No
runtime rebind/generation recovery is added; descriptors/DMA stay pinned.

All 61 import/lifecycle modes pass normally and under ASan/UBSan. Actual block,
USB, AHCI and workspace tests cover invalidation and balanced admission; seven
synthetic real image-adapter cases still pass. Block/workspace/image fixtures
also pass sanitizers. Eight compiled negative controls are detected. The matched
build, four generator groups and 372-entry image check pass. Six sequential VMs
pass: aliases 16.55 s, bitmap quarantine 16.40 s, imports 16.55 s, corrupt-entry
fallback 16.40 s, complete threads 40.08 s and libuv 10.06 s. Evidence, tested
hashes and build/VM receipts are in `build/review/storage-lifetime/`.

The checkout includes main `19a6fe05`; PR187 desktop timing and PR19 Wi-Fi/logging
remain separate. Further tracing confirms dirty marking and uncertain-write
quarantine are not a journal. Next: namespace/sidecar interruption and durable
rename/replacement recovery. Live media replacement, arbitrary retained kernel
pointers and hardware hot-unplug remain unaccepted. No physical trial or Actions
dispatch occurred.

### Lifetime checkpoint integration verification

Checkpoint `801ac9ca` was then integrated with main `4d195e25`/PR187's desktop
loop diagnostics. The merge was clean; all 19 storage source/test manifest files
are unchanged. The integrated storage host gate, 61 modes, three loop-timing
tests, matched build and 372-entry generated-image check pass. Six VMs pass again:
aliases 16.52 s, bitmap quarantine 16.46 s, imports 16.45 s, corrupt-entry fallback
16.44 s, complete threads 40.03 s and libuv 10.06 s. Integrated logs, tested hashes
and build/VM receipts remain under `build/review/storage-lifetime/`. PR19 remains
separate. These runs do not extend native desktop or hardware acceptance.

### Rename/sidecar audit — active scope (2026-10-26)

Continue PR182 from `3c0c84a7`, including main `4d195e25`. Review namespace
replacement ordering, sidecar validation/promotion and interrupted mutation
quarantine. Add portable deterministic write/flush interruption probes and
focused parser regressions; retain the distinction between quarantine and
journaled recovery. PR19 Wi-Fi/workspace logging has no storage-file overlap.
Own exFAT, VFS metadata persistence results, host fixtures and related documentation. No physical trial,
Actions dispatch, live media rebind or automatic offline repair is in scope.


### Rename/sidecar checkpoint results

Nine directed before/after probes reproduce partial or out-of-mount sidecar
application, numeric overflow, truncated/NUL records, dirty-media replay,
destructive cleanup after temporary-read failure, and hidden chmod/time sync
failures. Sidecars now validate wholly from one buffer, stay within the mount,
and are not replayed on quarantined media. Unreadable temporary files survive
mount for retry. VFS metadata mutations propagate persistence failure and restore
live metadata. The original file-backed source also fails the committed-sidecar
presence assertion; sync/promotion now replace the committed file before retiring
the temporary record.

All 72 modes pass normally and under ASan/UBSan. The portable synthetic-volume
sweep passes 144 write/flush/failure/half-write boundaries across rename,
replacement and promotion. Storage/lease/VFS host tests, the matched build and
372-entry generator check pass. Six VMs pass: aliases 17.20 s, quarantine
16.71 s, imports 17.16 s, corrupt entry 16.70 s, complete threads 40.77 s and
libuv 10.74 s. Logs, source hashes, original failures, harness corrections and
build/VM receipts are retained in `build/review/rename-sidecar/`.

Main remains `4d195e25`; PR19 was refreshed at `35041be0` and its Wi-Fi/logging
and separate Make targets do not overlap exFAT/VFS. No native media or Actions
were used. The fault model covers ordered persisted prefixes; it does not accept
controller reordering, sector atomicity, journaled rename or physical power-loss
recovery. Follow-on tracing identifies boot-region/backup and system-stream
validation as the next bounded audit; full-format conformance remains open.

### Boot/system-stream audit — active scope (2026-10-26)

Continue PR182 from `bc024735`, including main `4d195e25`. Validate boot checksums
and backup handling before trusting geometry, then root bitmap/upcase descriptors
and their streams before metadata recovery or mutation. Own exFAT, generated
images, portable fixtures and related documentation. Refresh PR19 at `f70ee542`:
its Wi-Fi/logging changes and separate Make targets do not overlap this runtime
scope. Preserve offline recovery boundaries; no automatic boot repair, native
media trial, Actions dispatch or new Unicode filename support is planned.


### Boot/system-stream checkpoint results

Nineteen initial directed probes reproduce missing boot-region validation and
system-stream defects; the subsequent unknown-critical-entry probe also fails
against the previous behavior. Boot checks now cover complete regions and all
checksum words, required fields/signatures and exact header snapshots. A unique
valid backup can provide read-only fallback even with a damaged primary sector
size; ambiguous candidates are refused. A damaged or disagreeing backup also
prevents writes through a valid primary. No automatic repair is added.

Imports require single root bitmap/upcase descriptors, the correct bitmap
identity, exact system FAT chains, and valid upcase checksum, coverage and ASCII
mappings. Fragmented system allocations are followed correctly. Generated images
now contain both boot regions and a complete compressed upcase table; older
unchecked images must be rebuilt. Follow-on probes reproduce teardown during
statfs and truncated multi-cluster bitmap counts. Statistics now hold storage
ownership and traverse complete chains. Multi-cluster mutation/graph checking
remain unsupported. A suspected bitmap overread was ruled out and is not counted
as a confirmed defect; that evidence is retained.

All 100 directed modes and the 29-case sector-size/backup/bitmap matrix pass
normally and under ASan/UBSan. The 144 interruption boundaries, storage/lease host
gates, matched build, six generator groups and 372-entry image check pass. Six
VMs pass: aliases 16.44 s, quarantine 16.51 s, imports 16.67 s, corrupt entry
16.53 s, complete threads 41.49 s and libuv 10.84 s. Evidence and tested hashes
are retained under `build/review/boot-system/`.

The final refresh still has main `4d195e25` and PR19 `f70ee542`; no new runtime
overlap or integration dependency appeared. No Actions, native media test or
physical recovery was performed. Follow-on tracing reaches allocation exhaustion
and directory-growth rollback; full Unicode, automatic repair and physical
power-loss guarantees remain outside the accepted profile.

### Allocation/growth audit — active scope (2026-10-26)

Continue PR182 from `16b81077`, including main `4d195e25`. Inspect allocation
exhaustion, size bounds, directory extension and failure ordering. Own exFAT,
focused host fault fixtures and related documentation. PR19 remains `f70ee542`;
its Wi-Fi/logging and separate Make targets do not overlap this runtime scope.
Distinguish clean refusal before mutation from uncertain-write quarantine; add
no journal, native-media trial, Actions dispatch or automatic repair.


### Allocation/growth checkpoint results

Out-of-space and allocator-refusal probes reproduced premature directory tail
writes and quarantine. Oversized payloads/entry sets also reached mutation work.
The writer now selects space and initializes an extension before retiring end
markers; impossible requests are refused before dirty marking or scratch
allocation. Cluster rounding is overflow-safe and allocator counts are bounded.

Stale `ghost` records following an end marker reappeared after insertion and
remount, both within the same cluster and in later allocated directory clusters.
The writer now respects the logical end across the chain and clears undefined
records before publication. UBSan additionally caught signed overflow in the
long-name hash rotation; its bit-15 rotation is now bounded.

All 108 directed modes pass normally and under ASan/UBSan. The 193 root/subdirectory
growth fault boundaries pass normally and under sanitizers, alongside the
existing 144 rename/sidecar boundaries, 29 boot-sector cases, storage/lease gates,
six generator groups and 372-entry image check. The matched build and six VMs
pass: aliases 16.96 s, quarantine 16.98 s, imports 17.02 s, corrupt entry 16.63 s,
complete threads 40.39 s and libuv 10.72 s. Evidence, original failures, corrected
harness assumptions and tested hashes remain in `build/review/allocation-growth/`.

The publication refresh found main `ee5468f6` (PR188/189), changing only the Air6,1
machine record. The runtime was tested above on the `4d195e25` integration base;
the clean documentation merge and refreshed-build check are recorded next.
PR19 remains separate. Follow-on tracing reaches deletion/truncation retirement
and low-space sidecar updates. Partial mutations still quarantine; this adds no
compound space reservation, journal, new errno contract or native acceptance.


### Allocation checkpoint integration verification

Runtime checkpoint `497e2e6d` was merged cleanly with main `ee5468f6` (PR188/189).
The incoming change is confined to the Air6,1 machine record. Every tested source
and output hash is unchanged; `all` needed no recompilation and refreshed the
matched-build receipt. A fresh disposable import VM passes in 17.00 s. The full
six-VM results above remain evidence for the identical runtime artifacts.
`integrated-build.log`, `integrated-macos-build.json` and `integrated-result.json`
retain this verification under `build/review/allocation-growth/`.

## Deletion/truncation retirement — active scope

Continue PR182 from `91a4f594` on integration base `ee5468f6`. Audit exFAT
retirement ordering and low-space sidecar updates using healthy allocation
fixtures and write/flush interruption sweeps. Touched areas are exFAT, its host
fixtures, test targets and current storage guides; process unlink reconciliation
is inspected with its existing regressions. PR190 (`4c0b3fb7`) owns scheduler
wait attribution; PR19 (`3cb0a338`) owns USB Wi-Fi. Their actual diffs have no
exFAT implementation overlap; preserve their separate Makefile targets. Next
checkpoint is reproduced defects, bounded fixes, sanitized host checks and
matched disposable VM results. No physical install, native acceptance or merge
is planned.

### Retirement checkpoint verification

All 116 import modes pass normally and under ASan/UBSan; all 323 retirement fault
boundaries pass normally and under ASan/UBSan. The current 161 growth boundaries,
144 rename/promotion boundaries, 29 boot matrix cases, storage/lease tests and
process open-content/orphan tests pass. The matched build, six generator groups
and recursive 372-entry image check pass. Six sequential disposable VMs pass:
aliases 17.06 s, quarantine 16.60 s, imports 16.99 s, corrupt-entry 16.68 s, threads 40.18 s, libuv 10.61 s.

Evidence and source/artifact hashes are retained under
`build/review/retirement-sidecar/`; the two original regression failures match
`91a4f594` byte-for-byte for the backend source. The compiler-stub failure is
retained separately and is not a test pass. Main remains `ee5468f6`; refreshed
PR190 (`f1587a2f`) and PR19 (`9a50f4dc`) remain separate with no exFAT overlap.
Documentation/link and whitespace checks pass. No native hardware test or
Actions dispatch was performed.

The confirmed full-volume truncate/shrink defect is corrected and PR182 remains
a draft. Follow-on snapshot/teardown tracing confirmed no further defect. Next
bounded question: process writeback retry after a clean backend space refusal,
which currently shares the failed-content latch used for uncertain I/O. Native
unregister/rebind, physical power-loss recovery and broader subsystem coverage
remain open.


### Retirement checkpoint integration — active scope

After publishing `d34fcc97`, main advanced to `4ee18f69` with PR190's scheduler
wait attribution and desktop diagnostic ABI. The only textual conflict is the
Makefile tail: preserve both `test-storage-leases` and `test-scheduler-trace`.
No exFAT implementation conflict exists. Rebuild matched artifacts and rerun
the affected scheduler/syscall/desktop host checks, storage gates and six VMs
before republishing. The pre-integration evidence above remains tied to its
original runtime. PR19 remains separate at `9a50f4dc`.

### Retirement checkpoint integrated verification

Integration `9510ce8c` includes main `4ee18f69` (PR190). The Makefile conflict
was resolved by preserving both test targets; exFAT source was unchanged.
The scheduler admission/trace/syscall gates, five loop-timing tests, six scheduler
activity tests and desktop-session fixture pass. The latter uses the documented
Darwin wrapper with `-Wno-error=array-bounds` for only
`build/tests/desktop-session-host`: Clang diagnoses the unselected inline-array
branch at index 31 after asserted dynamic growth to 32/128 slots. Both warnings
remain visible, and the original strict compile failure is retained; no desktop
implementation was changed to silence it.

The rebuilt 116-mode import suite, 323 retirement boundaries, storage/lease and
process tests pass. The matched full build, six generator groups and recursive
372-entry image check pass. A fresh six-VM pass on the integrated artifacts gives:
aliases 16.53 s, quarantine 16.44 s, imports 16.51 s, corrupt-entry 16.38 s, threads 40.02 s, libuv 10.11 s.

Commands, failures, passing reruns, build receipt and tested source/artifact
hashes are under `build/review/retirement-sidecar/integration/`. The earlier
ASan/UBSan evidence still covers the unchanged exFAT implementation and fixtures;
it is distinct from the new integrated guest evidence. This final record changes
documentation only. PR182 remains draft; the next audit question is process
writeback retry after clean space refusal.

## Process writeback retry — active scope

Continue PR182 from `834fbff0` on main `4ee18f69`. Audit clean writeback refusal
versus irreversible/uncertain failure, including retained content, shared opens,
retry, close and process-exit reporting. Touched areas: process writeback, an
exFAT outcome contract, focused host fixtures and storage guides. Refreshed PR19
at `e04e2dc0` retains Wi-Fi ownership and has no process/exFAT/VFS overlap; keep
its Makefile additions separate. Next checkpoint is an original-source retry
reproducer, atomic backend outcome coverage, process regressions and matched
VM validation. No native install or PR merge is planned.

### Writeback findings and correction

Two confirmed defects are corrected: clean exFAT refusal no longer permanently
poisons shared process content, and global sync includes dirty reader-only
survivors after failed writer close, once per content object. Retry eligibility
is captured under filesystem ownership after proven clean refusal; partial or
uncertain mutations remain sticky. Original process source `834fbff0` fails the
real full-volume bridge, and a separate process regression reproduces the sync
omission. The new backend tests include reservation OOM and failed clean-marker
write/flush controls. Final-close and exit failure reporting remain intact.

### Writeback checkpoint verification

All 123 import modes, the real process/exFAT bridge and public process-output
regressions pass normally and under ASan/UBSan. The 323 retirement, 161 growth,
144 rename/promotion and 29 boot-matrix boundaries/cases pass normally, along
with storage/lease gates. The matched build, six generator groups and recursive
372-entry image check pass. Six sequential disposable VMs pass:
aliases 16.53 s, quarantine 16.47 s, imports 16.50 s, corrupt-entry 16.41 s, threads 40.53 s, libuv 10.18 s.

Evidence, original failures, harness corrections and tested hashes remain under
`build/review/writeback-retry/`. Main remains `4ee18f69`; PR19 at `e04e2dc0` has
no process/exFAT/VFS overlap. Documentation/link and whitespace checks pass.
No native install, Actions dispatch or PR merge occurred. Follow-on syscall/libc
tracing confirmed no further defect; descriptor inheritance and delayed-error
ownership across process exit are next.

## Descriptor inheritance and exit errors — active scope (2026-10-28)

Continue PR182 from `2a091597`, integrating fetched main `5a94d755` through
PR230 before implementation. Audit inherited file references, close-on-exec,
spawn/fork setup rollback and ownership of delayed writeback errors at exit.
Touched areas: process lifecycle/file descriptions, focused host regressions,
current storage/process guides and this host record. Main's spawn-image,
directory and scheduler changes are included rather than duplicated. Air6,1 and
Air7,2 branches are merged; PR19 at `114dee34` retains separate Wi-Fi scope with
no process/exFAT/VFS changes. Preserve its Makefile work. Next checkpoint is
bounded reproducers, fixes for confirmed defects and matched host/VM evidence.
No native install, hardware acceptance or PR merge is planned.
