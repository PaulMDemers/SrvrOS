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
