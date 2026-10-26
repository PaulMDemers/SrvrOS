# Synchronization and stack contract audit — 2026-10-25

## Current disposition

The three original findings below are fixed on PR175: supplied storage is never
expanded, condition variables retain their selected clock and recheck absolute
deadlines, and semaphore increments enforce `SEM_VALUE_MAX=INT_MAX` atomically.
The fourth finding, worker self-stack inspection, is also corrected with
pre-callback metadata and positive host/guest tests. The fifth finding, early
self-detach, is now corrected with creator reservations and serialized lifecycle
claims. The sixth finding, thread-name round trips, is corrected with bounded
record storage and range/lifetime checks. The seventh finding, initial-thread
`pthread_exit` killing live workers, is corrected with retained kernel teardown
ownership and a last-thread libc cleanup handoff. The eighth finding,
initial-thread join, is corrected with retained return values and exclusive
join/detach ownership. The ninth finding, negative absolute condition deadlines,
is corrected by accepting signed seconds and using the existing timeout path.
The tenth finding, null-output clock resolution, is corrected while preserving
clock/source validation. The eleventh finding, frozen `times()` elapsed return
without a reference clock, is corrected with direct tick fallback and positive
regressions. The twelfth and thirteenth findings, handler-time subtraction from
sleep remainders and extreme fallback conversion overflow, are now corrected
with pre-handler capture and quotient/remainder conversion. The failed sleep
audit below is historical at `bd300e28`; current positive coverage is described
in the correction record.
See the correction/validation record below. The original findings and receipts
are retained as history at source `a90ac149`; they do not describe the corrected
implementation. The original audit-only runner was replaced by positive
`tools/test_sync_contracts.py` regressions, integrated into `test-core`.

**Matched rebuild required:** `pthread_cond_t` now includes its clock (8 bytes,
previously 4). Static initializers retain realtime. Rebuild embedded users such
as the libuv adapter and all apps; no binary compatibility with old objects is
claimed. The initial-thread exit correction also changes `SYS_THREAD_EXIT`:
only the initial caller can return, after all workers retire, solely to run
libc `exit(0)`. Keep kernel and libc matched for this cleanup handoff.

## Original scope and evidence

Continuation of the [first code/docs audit](code-docs-audit-2026-10-25.md), on
draft [PR175](https://github.com/PaulMDemers/SrvrOS/pull/175). Baseline is
`origin/main` at `aa7b7392`: the runtime is unchanged from `eda8bfbc`; PR173 adds
Air6,1's three failed/incomplete native timing attempts. PR174 owns process
startup/worker retirement and PR172 owns shared desktop dispatch. This pass
does not modify those implementations or accept a new physical payload.

The probe compiles the actual `posix_pthread.c` and `posix_semaphore.c` with
their SrvrOS public headers. Symbols are prefixed to avoid replacing the host's
libc/thread interfaces. OS services are modeled: thread creation records the
requested stack and returns an error without starting a thread; clocks have
deliberately distinct values; futex calls are counted. A larger allocation holds
sentinels beyond the declared stack region, so the bad write can be observed
without writing outside the probe's own allocation. The model checks library
decisions; it does not test kernel scheduling or real concurrent execution.

## Original findings

### P1 — Caller-supplied stack size is enlarged before an out-of-bounds write

In [posix_pthread.c](../userspace/lib/src/posix_pthread.c), `pthread_attr_setstack`
accepts a non-null region as small as `PTHREAD_STACK_MIN` (16 KiB).
`pthread_create` then takes the larger of the requested size and its 2 MiB
default, rounds that size upward to a page boundary and writes a zero return
address at the calculated stack top. This happens before `srv_thread_create`.

The host probe gives a 16,384-byte contract inside guarded storage: the zero is
written at offset **2,097,144**, well outside that contract. A declared
2,097,153-byte region is rounded upward and written at offset **2,101,240**.
The exact 2 MiB control remains inside its region. Both defective cases occur
even when the modeled kernel refuses creation. A real smaller allocation could
fault or corrupt adjacent memory before the caller receives an error.

The [application-managed stack contract](https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html)
assigns a region using its supplied address and size. Fixing this needs bounds
and overflow validation before any write; supplied storage must not be expanded
as if it were library-allocated storage. Reject unsupported sizes/alignment or
compute a stack pointer entirely inside the supplied region. Add guarded-region
tests for minimum/odd/huge sizes and failure rollback. Current consumers should
use library-owned stacks until this is fixed; no unsafe workaround is required.

The existing `threadstress` compatibility case checks stack attribute setters
and getters, but never creates a thread with a supplied stack. Passing that test
does not cover this defect. No runtime fix is included in this audit.

### P2 — A successfully selected condition clock is discarded

`pthread_condattr_setclock` accepts `CLOCK_MONOTONIC`, but `pthread_cond_init`
discards its attributes and `pthread_cond_timedwait` uses the realtime helper.
With monotonic time 100 s, realtime 10,000 s and a selected monotonic deadline
of 101 s, the actual implementation queries realtime and returns `ETIMEDOUT`
without making a futex wait call. The future monotonic deadline is interpreted
as an expired realtime deadline.

This was already excluded by the [supported profile](posix-supported-profile.md);
the new probe makes the failure reproducible. Retain and use the selected clock,
or refuse unsupported selection explicitly. The default realtime wait is a
separate contract. The [condition-wait reference](https://pubs.opengroup.org/onlinepubs/9799919799/functions/pthread_cond_clockwait.html)
explains the distinction between realtime and monotonic deadlines. A fix must
also preserve mutex reacquisition and interruption behavior; this model does
not validate actual timer precision.

### P2 — POSIX semaphore overflow silently destroys the count

In [posix_semaphore.c](../userspace/lib/src/posix_semaphore.c), `sem_init` accepts
the full unsigned count without a bound; `sem_post` performs an unchecked atomic
increment and always returns success. The probe initializes `UINT32_MAX`, posts
once and observes count zero with both calls succeeding. Tokens are silently
lost, so subsequent waits can block instead of reporting an overflow condition.

`sem_init` should validate a documented maximum, and `sem_post` should check
that maximum atomically before incrementing. Publishing a consistent maximum
also matters for `sem_getvalue`'s signed result. The
[initialization reference](https://pubs.opengroup.org/onlinepubs/9799919799.2024edition/functions/sem_init.html)
requires rejection of a value beyond the semaphore maximum. Positive controls
verify ordinary decrement and empty `EAGAIN` before exercising the bad boundary.

Source inspection also shows ignored `pshared`, yielding waits and no public
timed-wait API. These are additional scope limits, not demonstrated interprocess
or scheduler failures in this probe. `uv_sem_*` uses its own mutex/condition
implementation in `ports/srvros/uv.c`; its passing consumer tests cannot be used
as evidence for libc `sem_*` overflow semantics.

## Reproduce the historical failures

At commit `a90ac149`, from the repo root with the macOS command-line C compiler:

```sh
python3 tools/audit_sync_contracts.py --output build/review/sync-audit/default
# Expected exit 1: known defects, not a failed invocation.
python3 tools/audit_sync_contracts.py --expect-known-defects --output build/review/sync-audit/known
# Expected exit 0: all four known observations reproduced, NOT conformance.
```

The original receipt was:

```text
stack declared=2097152 submitted_offset=2097144 out_of_contract_write=0
stack declared=16384 submitted_offset=2097144 out_of_contract_write=1
stack declared=2097153 submitted_offset=2101240 out_of_contract_write=1
condition monotonic_selected=1 realtime_queried=1 futex_calls=0 early_timeout=1
semaphore init_uint_max=0 post=0 silent_wrap=1
audit: 4 known-defect observations; this is not a conformance PASS
```

Each output directory contains the binary and `result.json`: source commit,
dirty state, source/header hashes, compile command, platform, output and exit
status. The two stack cases are one defect category. Exit 2 means a probe
precondition or expected-defect receipt changed; inspect it rather than treating
it as a pass. Convert these cases to positive regression tests with each fix.
Only ARM64 macOS execution was validated in this pass; the runner also accepts
Linux toolchains, without claiming a Linux run here. It is separate from
`test-core` and does not rebuild or launch SrvrOS.

## Original documentation reconciliation

The supported profile now excludes custom-stack bounds and full POSIX semaphore
edge semantics, and the libuv guide distinguishes its semaphore implementation.
The port inventory's **Current Limits** section also had stale statements about
missing `fork`, final-state stdio spawn actions, `FD_SETSIZE=128`, relative-only
sleep and uptime-only time. Source and published headers establish restricted
single-thread fork, ordered native spawn actions (maximum 32), `FD_SETSIZE=1024`,
absolute sleep support and hardware-seeded UTC. Those statements are corrected;
that source review does not rerun their prior guest acceptance suites.

The October 24 audit's historical matrix is explicitly labelled as its original
baseline, so already-fixed findings are not read as current defects. Current
Air6,1 status incorporates PR173's failure evidence; ongoing PR174 is a proposed
correction, not native acceptance. Kernel lifetime/concurrency, filesystem
failure paths and the experimental Linux adapters remain further audit work.

## Corrections and regression evidence

The user authorized corrections after the initial audit. The PR175 implementation
now validates supplied bases (16-byte alignment), minimum sizes and pointer
addition before touching storage. Supplied lengths are preserved, with the entry
stack aligned downward inside the region. Library-owned stacks retain the 2 MiB
minimum and checked page rounding; failed creation releases only owned storage.
The setters and create path both validate, so inconsistent attributes cannot
bypass checks by reaching creation directly.

Condition initialization validates and retains the chosen clock. Timed waits
recompute from the absolute deadline after interrupts and early tick wakes,
using at most one second per relative futex wait to avoid arithmetic overflow
for distant dates. Failed mutex release returns its error. Timeout and clock
errors reacquire the mutex before returning. Static/default initialization is
still realtime; the layout change requires a matched rebuild.

Semaphore initialization rejects counts above `SEM_VALUE_MAX` with `EINVAL` and
leaves the object untouched on that error. Posting uses a compare/exchange loop:
a full count returns `EOVERFLOW` unchanged; concurrent posters cannot both take
the final available count. The signed `sem_getvalue` result can represent the
published maximum. Process sharing and broader wait semantics are not added.

Validated on the ARM64 Mac with rebuilt x86_64 artifacts:

- `sh tools/macos/build.sh test-sync-contracts`: pass. Real library sources with
  modeled OS calls cover small/odd/exact supplied regions, sentinels, invalid
  ranges/alignment, huge-size refusal, failed allocation/creation cleanup,
  realtime/monotonic/static-init deadlines, early wakes, interruption, mutex
  reacquisition, invalid clocks/times and semaphore boundaries/recovery.
- Three isolated negative controls reintroduce stack enlargement, discarded
  clock selection or unchecked posting. Each makes the positive host suite fail
  with exit 1. These mutated copies exist only under ignored local evidence.
- `sh tools/macos/build.sh -j2 all`: pass, rebuilding consumers of `pthread_cond_t`.
- `python3 tools/thread_smoke.py --test sync-contracts --boot-wait 60 --run-wait 120`:
  pass. Live workers use 16 KiB, 16 KiB+1 and 2 MiB+1 supplied stacks with guard
  pages/sentinels; both clocks time out without early completion and wake on
  signals; twelve rounds of three competing posters admit exactly one final
  increment and two overflow failures.
- `python3 tools/thread_smoke.py --boot-wait 60 --run-wait 300`: complete suite pass,
  including existing lifecycle, mutex/condition, signal, fork, descriptor and
  storage tests plus the new group.
- `python3 tools/libuv_smoke.py --boot-wait 60`: complete consumer suite pass,
  including workers, thread synchronization, async work, IPC and process tests.

Host receipts are under `build/review/sync-regression/`; guest logs, negative
controls and matched build hashes are under `build/review/sync-fixes/`. Guest
harnesses terminate their own disposable VMs. The full Linux host/core suite and
physical hardware acceptance are not claimed. The original failing probe remains
in Git history; the positive regression runner replaces expected-defect success.

## Historical fourth finding — P2: self-stack inspection reports a guessed region

The following receipt is from `bc0d6418`, before the correction below.

The corrected stack allocation exposed an independent metadata defect in
`pthread_getattr_np(pthread_self(), ...)`: its self branch guesses one of two
fixed address ranges from a local frame address, with a fixed 2 MiB size, instead
of reporting the actual worker stack. The other-thread path reads a thread
record. This does not undo the supplied-stack bounds fix, but consumers must not
use the self result for stack limits or memory ownership.

A separate guest mode waits until the parent's create call has returned before
the worker inspects itself, removing initial record-publication timing from this
reproduction:

```sh
python3 tools/thread_smoke.py --test stack-inspection-audit --boot-wait 60 --run-wait 30
```

Observed: requested `0x10000000/65536`, reported `0xfdff000/2097152`, error 0,
mismatch 1; guest exits 1 and the harness exits 3 for missing success markers.
This is a retained **failed audit case**, explicitly excluded from the passing
`all` suite. A correction should obtain worker metadata with defined publication/
lifetime ownership and separately define main-thread inspection; merely replacing
one guessed constant is insufficient. Actual worker scheduling in this test is
VM evidence, not physical/native acceptance. This was the next confirmed finding at that checkpoint; broader record-
publication/reaping races were not established by this case.


## Self-stack inspection correction

This pass consumes main `1eb60b8f` (PR174/176 scheduler work). The trampoline
installs a thread-local pointer to the start record before calling user code,
so self-inspection does not wait for the parent to publish its tid/record.
The record remains owned until kernel exit and join/reaping permit cleanup.
Supplied stack bounds are exact; owned bounds describe the actual allocation.
Detach state is initialized before creation and atomically updated by successful
`pthread_detach`, then read by self and other-thread inspection. No public ABI
layout changes accompany this correction; the earlier condition ABI requirement
still applies to the combined PR.

Without worker metadata, inspection recognizes only a frame inside either of
the kernel's two initial-process stack ranges (2 MiB ending at `0x1000000` or
`0x0ffff000`). Other unregistered stacks return `ESRCH`. This remains coupled to
`process.c`'s fixed layouts, not a general kernel stack-query API. The guest
initial-process test exercised the lower layout; the relocated layout is source-
reviewed, not separately runtime-qualified here.

The positive host runner executes the real trampoline inside its modeled create
call and terminates that dedicated test process from modeled thread exit, before
parent publication can run. Supplied odd-size, owned and initially detached cases
pass. Unknown host-stack and invalid-thread/null-output checks also pass. A
negative control omitting the TLS installation fails the supplied-stack case
with exit 1, demonstrating that the test detects the startup dependency.

`threadstress stack-inspection` replaces the historical failing guest mode and
is included in `all`. It covers 16 KiB, 16 KiB+1, 64 KiB and 2 MiB+1 supplied
stacks; owned minimum and rounded allocations; immediate self-inspection;
parent agreement; initially detached and later-detached workers; local-address
containment; initial-process inspection; and rejection after record cleanup.
The host ordering test is deterministic; the VM test does not force the early
scheduling window. Evidence is retained under `build/review/stack-inspection/`.

## Historical fifth finding — P2: early self-detach depends on parent publication

The following receipt is from `d9b1e710`, before the correction below.

`pthread_detach(pthread_self())` still looks up `pthread_records` by tid. When a
joinable child calls it before the parent returns from `srv_thread_create` and
publishes `record->tid/start`, lookup fails and returns `EINVAL`. The new TLS
metadata fixes inspection only; it does not publish or serialize lifecycle
records. A worker that expects to detach itself can remain joinable and retain
its stack/record until explicitly joined.

Reproduce with real libc and the deterministic child-before-parent host model:

```sh
python3 tools/test_sync_contracts.py --audit-self-detach --output build/review/thread-publication-audit
# Expected exit 1: known defect; not part of the positive default suite.
```

Observed: `immediate self-detach returned=22 expected=0`. The modeled syscall
invokes the actual trampoline and callback before returning; no actual host or
guest scheduling race is claimed by this receipt. It demonstrates a permitted
ordering the library does not handle. Parent-side detach after publication is
covered by the passing VM regression. The next correction should establish
record publication and lifecycle ownership before callbacks, including cleanup
and concurrent reaping, rather than special-case a successful return while
leaving the record joinable. Wider reaper races remain review questions without
a separate reproduced failure in this pass.

Validation for the final inspection correction: matched `all` build and positive
host suite pass; targeted guest inspection and the final full thread/libuv suites
pass sequentially on ARM64 macOS/x86_64 QEMU. The isolated negative control and
separate self-detach audit fail as intended. The merged app-timing parser's three
Python tests pass. Documentation checks and `git diff --check` pass. No full
Linux/core or native-hardware test is claimed. Final source/log hashes and the
matched build manifest are retained in `build/review/stack-inspection/`.


## Early self-detach correction

This pass consumes main `aa931093`, including the shared Text/Preview runtime.
The only merge conflict appended independent Make targets; both the existing
synchronization target and new desktop runtime/command targets are retained.
No kernel or public pthread layout changes accompany this fix.

A record is reserved with its start metadata before the kernel can run a child.
The trampoline publishes the child's tid under the record lock before invoking
its callback. Parent completion publishes the returned handle and releases the
creation reservation without overwriting the child's detach decision. The
reservation prevents reaping even if the child exits before create returns.

Joiners and detached reapers claim a record exclusively under the same short
lock. Kernel status/join calls, yielding waits, stack unmapping and freeing run
outside that lock. Other-thread attribute inspection copies metadata while
locked. Failed/interrupted reaps release the claim but retain the record for a
later attempt. Joining a child still being created waits for the creator to
release its reservation; self-join returns `EDEADLK` before such a wait. After
cleanup detaches a record from the table, only the cleanup owner frees its saved
start metadata and owned stack. Supplied storage is never freed by libc.

Validation:

- Default host regressions pass. A separately compiled native pthread shim runs
  the actual libc trampoline to completion before the modeled create returns.
  Across 40 alternating supplied/owned lifetimes it verifies early self-detach,
  repeated-detach refusal, self-join refusal, self inspection, creator pinning,
  parent preservation, status-error/interrupted-join retries, a reentrant reaper,
  one cleanup per worker and record-slot reuse. An ordinary failed join releases
  its claim; a subsequent interrupted join retries and cleans up once. Other
  existing host cases remain.
- AddressSanitizer/UndefinedBehaviorSanitizer host runs pass with no diagnostics.
- Four isolated negative controls remove child publication, restore the parent's
  detach overwrite, remove the creator reservation check, or remove the reap
  claim check. Each fails the positive lifecycle test with exit 1.
- Matched `all` build and targeted `threadstress self-detach` pass. The guest
  completes 96 child lifetimes across three concurrent creators, alternating
  supplied/owned stacks, checking immediate self-detach, metadata, duplicate
  refusal and detached join refusal, followed by a fresh create/join.
- Complete thread and libuv guest suites pass sequentially on ARM64 macOS using
  x86_64 QEMU. This is bounded concurrency coverage, not a general SMP proof or
  physical-device acceptance. Full Linux/core coverage is not claimed.

The old failing self-detach option is retained at `d9b1e710`; the current default
host suite and `--test self-detach` guest group are positive regression gates.
Logs, negative controls, hashes and build manifest are retained under
`build/review/self-detach/`. Detached cleanup remains lazy until a subsequent
create/join/detach call; mixing raw kernel lifecycle operations with libc-owned
pthread records is unsupported. There is no new background collector.

## Historical sixth finding — P3: thread-name success discards the requested name

The following receipt is from `b095d9b4`, before the correction below.

`pthread_setname_np` validates the target and pointer, then returns zero without
storing anything. `pthread_getname_np` always copies `srvros-thread` and returns
zero, even when truncating to a short buffer. These nonportable entry points do
not implement a name round trip, which can mislabel diagnostic workers.

```sh
python3 tools/test_sync_contracts.py --audit-thread-names --output build/review/thread-name-audit
# Expected exit 1: known stub behavior, excluded from the positive default suite.
```

Observed with the real libc: `set=0 get=0 requested=audit-worker
reported=srvros-thread`. This is host evidence for library behavior; no kernel
or VM name-display behavior is inferred. The supported profile now explicitly
excludes real thread naming. A future correction should define storage, lifetime,
length and truncation/error semantics, or refuse unsupported setting explicitly.

Adjacent source review also verified that `process_fork` refuses every used
kernel worker slot, including an exited but unreaped worker; the current profile
previously said only live workers. That wording is corrected. No new fork
runtime behavior is claimed. Once-initializer abnormal exit and broader raw
kernel lifecycle interactions remain future audit areas, not confirmed failures
from this pass.


## Thread-name correction

This pass consumes main `fa5a6cc1` (Air7,2 acceptance documentation; runtime is
unchanged from `aa931093`). Each reserved pthread record has its own name and
the initial thread (kernel tid 1) has separate process-local storage. The record
lock protects lookup and complete copies, including against reaping. A worker
can name itself before the parent returns; parent completion does not replace
it. New records start as `srvros-thread`, independently of the parent's name.
Exited records retain names until join/reaping; cleaned-up handles return `ESRCH`.
Raw syscall-created workers have no libc naming record.

The chosen payload limit is 15 bytes plus NUL, matching the bound described by
the [Linux man-pages reference](https://www.man7.org/linux/man-pages/man3/pthread_setname_np.3.html).
Empty names are allowed. Setters scan at most 16 input bytes and reject a missing
terminator within that bound with `ERANGE`; failed setters preserve the old name.
Getters require enough space for the actual name and NUL; shorter buffers,
including zero length, receive `ERANGE` without any output write. Null pointers
receive `EINVAL`; errors are returned directly and preserve `errno`. This defines
SrvrOS's libc contract, not all GNU/Linux process-name or procfs behavior. No
kernel scheduler label, diagnostic UI or public struct layout is changed.

The positive host suite replaces the old expected-failure name option. It covers
initial/default/empty/maximum names, oversized and 16-byte nonterminated input,
exact-fit buffers, unchanged short-buffer output, invalid targets/pointers,
errno preservation and naming across 40 lifecycle/reuse cases. The native shim
forces self-naming before parent completion, worker access to the initial-thread
name, naming after child exit, preservation when the parent resumes and refusal
after cleanup. ASan/UBSan passes without diagnostics. Three isolated negative
controls (discard accepted names, truncate short output, or share worker/main
storage) each fail the positive suite with exit 1.

Guest `threadstress thread-names` is now part of `all`. It checks range errors,
unchanged output, independent defaults across 24 alternating joinable/detached
lifetimes, parent/worker renaming, exited and stale handles, then two writers and
one reader performing 200 operations each on one target. Readers accept only
complete old/new names. The targeted guest test passes. Logs, negative controls,
source hashes and matched build manifest are retained in
`build/review/thread-names/`; no physical hardware trial is claimed.

## Historical finding — P2: initial-thread exit kills live workers

At `2ba7edce`, `pthread_exit` calls `srv_thread_exit`. In `process_thread_exit`, an initial
thread has no `process_user_thread` record, so the kernel calls `process_exit`.
That path kills the process's user workers and releases the process. This loses
outstanding work when an application exits its initial thread expecting its
workers to keep running. The [POSIX.1-2024 termination definitions](https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap03.html)
distinguish thread termination from process termination after the last thread
exits.

```sh
python3 tools/thread_smoke.py --test main-exit-audit --boot-wait 60 --run-wait 15
# Expected harness exit 3: missing worker-survival/completion markers.
```

The audit starts a worker, waits for its ready flag, then calls `pthread_exit`
from the initial thread while the worker delays for one second. The worker would
print survival/completion markers after waking. Observed: `initial thread exiting
with live worker`, followed by `run: pid=4 exited status=0`; no worker-survival or
success marker. The harness fails with exit 3. A zero process status does not
turn this missing completion into a pass. This explicit mode is excluded from
the positive `all` suite and retained as a failed guest receipt.

A correction needs initial-thread lifetime, process resource ownership and
last-thread retirement in the kernel; a libc-only successful return would not
preserve the workers. The profile at that checkpoint excluded initial-thread
exit with continuing workers. Worker-side exits retain their passing regression coverage.
Once-initializer abnormal-exit recovery remains a separate unconfirmed audit
area; this observation does not establish that behavior.

Final validation for the naming correction: matched `all`, positive host suite,
ASan/UBSan, targeted name guest test, complete thread suite and complete libuv
suite pass. The name negative controls and separate main-exit audit fail as
intended. Documentation validates 641 local links with zero errors, and whitespace
checks pass. The failed audit does not count as a passing conformance test.
No Linux/core or physical hardware run is claimed.


## Initial-thread exit correction

The kernel now marks the initial user thread retired and parks its kernel
continuation on the process thread wait queue until no worker is active. This
retains the original launch/teardown context, address space, initial TLS, shared
files and mappings. Dead, unreaped joinable slots do not keep a process alive.
Workers can create descendants after initial-thread exit. Worker retirement
wakes the continuation; explicit process exit and fatal signals wake/terminate
it with the existing process status. The initial thread's pointer return value
never becomes an integer process exit status.

After ordinary last-worker retirement, `SYS_THREAD_EXIT` returns zero only to
the initial libc caller, which immediately runs `exit(0)`. This executes process
`atexit` handlers before the existing kernel teardown. Worker syscall exits
remain nonreturning, and `pthread_exit` never returns to application code.
The initial user thread's TLS destructors have already run before its syscall;
worker destructors run before worker retirement. Targeted signals to the dead
initial thread do not deliver or kill survivors; process signal eligibility
considers live workers. A successful exec resets initial-thread retirement.

This kernel/libc handoff requires matched images, despite adding no pthread
structure layout change. The last-thread process-cleanup behavior follows the
[thread-exit contract](https://pubs.opengroup.org/onlinepubs/009696899/functions/pthread_exit.html)
(the accessible Issue 6 page; Issue 8 remains the project's target). Raw
`SYS_THREAD_EXIT` callers must honor the documented cleanup handoff themselves.

### Regression evidence

On the ARM64 macOS build host with x86_64 QEMU TCG:

- Matched `sh tools/macos/build.sh -j2 all` succeeds.
- The positive host suite and ASan/UBSan pass. The new `initial-exit` case runs
  real libc TLS destructor/exit code against a modeled kernel return and
  requires an `atexit` receipt; a zero exit status alone cannot pass.
- Guest `--test main-exit` passes 30 subprocess lifetimes (ten cases, three
  repetitions). It covers empty initial exits with null/nonzero values,
  joinable and detached workers, worker return and explicit pthread exit,
  post-initial-exit descendants, completed unreaped workers, `exit(23)`,
  `_exit(37)`, fatal SIGTERM and blocked SIGTERM. A pipe back to the parent
  verifies main/worker TLS destruction, survivor progress, process cleanup
  ordering, descriptor lifetime and child status. Callbacks also check shared
  heap data and independent TLS while the initial user thread is dead.
- Restoring premature kernel process teardown with live workers fails case 2:
  status 256, main destructor 1, worker/destructor/atexit receipts all 0;
  harness exit 3. The corrected image was restored and rebuilt afterward.
- An isolated host negative control restoring `_exit(0)` fails with exit 1
  because the required `atexit` receipt is absent.

The former `main-exit-audit` remains historical at `2ba7edce`; positive
`main-exit` replaces it in `all`. Raw logs, source hashes, sanitizer results and
build manifests remain in ignored `build/review/initial-exit/`. Host modeling
is not kernel scheduling evidence, and VM results are not native acceptance.

## Historical finding — P2: initial-thread join is rejected

At `b75fc79a`, with survival fixed, a worker trying to join the initial thread still gets
`EINVAL` (22), and cannot collect its `pthread_exit` value. The initial tid 1
has no libc `pthread_record`, so `pthread_join` rejects it before reaching the
kernel; the kernel join lookup also rejects tids below 2. Initial-thread detach
has the same source-level lookup restriction. This is a separate pre-existing
join/detach gap; the current profile now states it explicitly.

```sh
python3 tools/thread_smoke.py --test main-join-audit --boot-wait 60 --run-wait 30
# Harness exit 3; guest reports join=22 value=0x0, expected=0/0x9876.
```

The initial thread creates a worker and exits with `0x9876`; the worker attempts
to join it, reports the unexpected error and exits the process with status 1.
This explicit audit is excluded from `all`. The
[POSIX thread lifecycle contract](https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html)
requires a successful join to make the terminated thread's exit value available;
no initial-thread join/detach support is claimed by this correction. The next
bounded work is initial-thread joinability, return-value retention and exclusive
join/detach ownership while preserving the new teardown continuation. Abnormal
once-initializer exit remains unconfirmed; this pass does not promote it to a
finding.


Final validation for this correction: the restored matched image passes the
complete thread suite (including `main-exit`) and complete libuv suite. Host
regressions and ASan/UBSan pass; docs check 641 local links with zero errors and
whitespace checks pass. The two negative controls and separate initial-join audit
are failed evidence by design, not conformance passes. Disposable VMs stopped;
no full Linux/core or physical hardware acceptance is claimed.


## Initial-thread join correction

Libc now has a separate static initial-thread record using the same lock,
exclusive claims and lazy detached reaper as worker records. It does not consume
one of the sixteen worker slots. Collecting it clears only the public pthread
record: its static start metadata is not freed, and its process-owned stack/TLS
is not unmapped. Names follow the record lifetime; self-inspection reports its
actual detach state, while foreign initial-stack inspection remains unsupported.

The kernel retains the initial pointer return value before publishing exit and
wakes blocked joiners. Its initial-thread join path waits for that retirement,
collects the value once, and rejects self-join, duplicate claims and detached or
joined handles. Failed/interrupted waits release the kernel claim; libc retains
its own claim across signal dispatch/retry, so reentrant joins/detaches cannot
steal ownership. Initial-thread kernel detach/status/signal handling follows the
same public lifetime. Exec resets that state. The parked initial continuation
still owns process cleanup after the last worker; joining or detaching never
frees the process resources needed by survivors or `atexit`.

The former `main-join-audit` failure is historical at `b75fc79a`. Positive
`--test main-join` replaces it in `all` and covers eight scenarios repeated three
times: waiting/late joins, null output, self/early/late detach, an interrupted
join with reentrant signal-handler calls, and two workers competing for
join/detach ownership. Parent pipe receipts verify main/worker TLS destruction,
completion, process cleanup and status 0. Workers verify the return value,
retired-handle rejection and shared storage surviving collection.

The host fixture adds four positive modes for join/error/interruption retry,
null output and early/late detach. It verifies unchanged error output, exclusive
reentrant claims, name invalidation and no mapping teardown. ASan/UBSan also
checks that static initial metadata is never freed. Host OS services remain
modeled; actual kernel waiting/retirement is verified in QEMU.

## Historical finding — P2: negative absolute condition deadlines return EINVAL

At `cc955159`, `pthread_cond_timedwait` rejects `abstime->tv_sec < 0` before entering the wait
path, even with a valid nanosecond value. This is a valid expired absolute time,
not a malformed relative interval. The
[POSIX.1-2024 condition wait contract](https://pubs.opengroup.org/onlinepubs/9799919799/functions/pthread_cond_clockwait.html)
requires `ETIMEDOUT` for a passed deadline; its invalid-time check concerns the
nanosecond range. The timeout path must also release and reacquire the mutex.
The current early rejection instead reports `EINVAL` and skips that transition.

```sh
python3 tools/test_sync_contracts.py --audit-negative-deadline \
  --output build/review/negative-deadline-audit
# Expected current failure: returned=22, expected=110; exit 1.
```

The explicit host audit calls the real libc with a locked initialized mutex,
a default realtime condition, modeled current time 100 seconds and deadline
`{-1, 0}`. It observes `EINVAL` (22), rather than `ETIMEDOUT` (110), and confirms
the caller can still unlock. This audit is separate from the passing default
suite; it does not claim guest scheduling coverage or prove the mutex transition.
The next bounded correction is absolute-deadline validation and timeout-path
regressions, including unchanged rejection of invalid nanoseconds. Once
initializer abnormal-exit recovery remains unconfirmed. No extra defect is
inferred from the ordinary mutex/rwlock/once source review in this pass.


Final initial-join validation: matched build, positive host suite, ASan/UBSan,
targeted 24-lifetime guest group, final complete thread suite and final complete
libuv suite pass. An isolated libc negative control restores initial-thread join
refusal and fails with exit 1; a kernel negative control discards the exit value
and fails case 10 with guest status 91/harness exit 3. The fixed kernel was
restored and rebuilt before final suites. Documentation checks 641 local links
with zero errors; whitespace checks pass. Logs, source hashes, sanitizer records
and build manifests remain in ignored `build/review/initial-join/`. The deadline
audit failure is separate evidence, not a passing conformance result. All
disposable guests stopped; no full Linux/core or physical run is claimed.


## Negative absolute condition deadline correction

`pthread_cond_timedwait` now accepts signed absolute seconds, leaving the valid
nanosecond range unchanged. The existing signed comparison determines whether
the selected clock has passed the deadline. Expired negative values enter the
ordinary wait path, release the mutex, evaluate the clock, reacquire the mutex
and return `ETIMEDOUT`. This also avoids treating every negative value as an
immediate timeout: a negative deadline can be in the future relative to a
modeled negative clock value. Bounded tick conversion remains unchanged. No
public layout or syscall changes are introduced.

Positive host coverage replaces the old explicit failing audit at `cc955159`.
It exercises static/default, explicit realtime and monotonic conditions, each
with five expired values (`-1`, `INT64_MIN` and zero with boundary nanoseconds),
two malformed nanosecond values, two negative/current-zero crossings with a
nanosecond borrow, and a future interval spanning the signed seconds range.
The latter uses a signal wake and verifies the existing bounded 100-tick slice.
A clock hook acquires/releases the real error-checking mutex while the condition
wait has released it; the return check verifies caller ownership. Invalid
nanoseconds cause neither a clock query nor an unlock. Successful timeout/error
checks also verify errno preservation. Clocks and futex services are modeled.

The positive guest `--test absolute-deadlines` group covers the 15 expired and
six malformed cases using actual guest clocks and checks the returned error,
mutex ownership and errno. It is included in `all`. Existing guest future wait,
signal, lifecycle and libuv coverage remains relevant. Only the host clock hook
provides deterministic evidence for the transient mutex release; the guest
ownership check alone does not prove that transition.

Two isolated host negative controls fail with exit 1: restoring the rejected
negative-seconds check, and returning `ETIMEDOUT` early without the mutex
transition. Both use copies of the real sources; the checkout remains fixed.
Evidence is retained in ignored `build/review/absolute-deadlines/`.

Final validation on ARM64 macOS with x86_64 QEMU TCG passes: matched build,
host regressions, ASan/UBSan, targeted guest deadline group, complete thread
suite and full libuv suite. Documentation checks 641 local links with zero
errors; whitespace checks pass. Logs, manifests and source hashes are retained
with the evidence above. No full Linux/core, general SMP or native hardware
acceptance is claimed.


## Historical P2 finding: clock_getres rejects an optional output pointer

The following reproduction describes `86498be3`, before the correction below.

For `CLOCK_REALTIME` and `CLOCK_MONOTONIC`, `clock_getres(clock, NULL)` returns
`-1`/`EINVAL`, although the same clocks succeed with a non-null output pointer.
The generic branch in `posix_time.c` treats a null output as an invalid request;
its CPU-clock branch already accepts it. A portable caller probing a known
clock without requesting the resolution therefore receives a false failure.
The [clock resolution contract](https://pubs.opengroup.org/onlinepubs/000095399/functions/clock_getres.html)
explicitly makes this output optional (accessible Issue 6 reference; Issue 8
remains the project's target).

```sh
python3 tools/thread_smoke.py --test clock-resolution-audit --boot-wait 60 --run-wait 30
# Current guest status 1, harness exit 3.
```

The isolated guest audit observes stored-output success for all four supported
clock IDs. Null-output probes return `-1`, errno 22 for realtime/monotonic and
succeed for process/thread CPU clocks. Unknown clock 99 with null output still
fails with `EINVAL`, as expected. This audit is excluded from `all`; the current
profile documents passing a real output buffer as the workaround. The next
bounded correction is optional-output validation while retaining unsupported
clock and unavailable-source errors. No fix to `clock_getres` is included here.
Abnormal once-initializer exit remains unconfirmed.


## Clock resolution optional output correction

`clock_getres` now validates the realtime/monotonic clock ID independently of
its output pointer and writes only when a buffer is provided. Realtime source
availability is still checked for both forms; the existing CPU branch still
checks usage availability and propagates errors. Unknown IDs remain `EINVAL`.
Existing 1 us reference and 10 ms tick-fallback values are unchanged. No public
layout, syscall, kernel or device-profile change is introduced.

The new `tools/test_time_contracts.py` target compiles the actual timing source
with prefixed symbols and modeled kernel services. Positive coverage tests all
four clock IDs with/without output in both resolution modes, invalid IDs,
unchanged output on errors, preserved errno on success, unavailable realtime,
CPU `EIO`/`ESRCH`, and monotonic fallback without realtime. It is in `test-core`.
Guest `--test clock-resolution` replaces the old audit and is included in `all`:
all four clocks accept optional output, stored resolutions are positive and
normalized, and invalid IDs fail without modifying the caller's buffer.

Three isolated host negative controls fail with exit 1: the original null
rejection, unconditional success for null output, and skipping realtime source
validation for null output. They compile source copies without mutating the
checkout. Evidence is retained in ignored `build/review/clock-resolution/`.

Final ARM64 macOS/x86_64 QEMU TCG validation passes: matched build, host timing
and synchronization targets, final timing ASan/UBSan, targeted guest resolution
group, complete thread suite and full libuv suite. Docs check 641 local links
with zero errors; whitespace checks pass. Manifests and source/log hashes are
retained with the evidence above. No full Linux/core, general SMP or native
hardware acceptance is claimed.

## Historical P2 finding: times elapsed return freezes without reference clock

The following reproduction describes `cb8985e6`, before the correction below.

`times()` in `posix_time.c` divides `srv_monotonic_us()` by 10000 without checking
for source unavailability. The real kernel `SYS_MONOTONIC_US` returns `-1` when
`timer_reference_ready()` is false, while `SYS_TICKS` continues to provide the
100 Hz tick count. Signed division turns `-1 / 10000` into zero, so successive
successful `times()` calls report no elapsed time. The CPU accounting fields
are a separate output and are not implicated by this reproduction.

The elapsed-return contract is described in the [GNU libc manual, Processor
Time Inquiry](https://www.gnu.org/s/libc/manual/pdf/libc.pdf): clock ticks since
a fixed past origin. Callers measuring durations with return-value differences
will observe a frozen interval in this fallback configuration.

```sh
python3 tools/test_time_contracts.py --audit-times-fallback \
  --output build/review/clock-resolution/times-fallback-audit
# time-contracts: times fallback start=0 end=0 delta=0 expected=50
# Exit 1; excluded from the default positive target.
```

The host probe advances modeled ticks from 123 to 173 (500 ms). The real libc
`clock_gettime(CLOCK_MONOTONIC)` advances by 500 ms using those ticks, and a
reference-enabled `times()` control advances by 50 ticks, but the fallback
`times()` delta is zero. This confirms the source-level path with modeled
services; the default reference-enabled VM and native hardware have not been
used to reproduce this configuration. Use monotonic `clock_gettime` for elapsed
intervals until corrected. The next bounded work is `times()` source selection
and fallback/error coverage. No `times()` implementation change is included.
Abnormal once-initializer exit remains unconfirmed.


## times elapsed fallback correction

`times()` samples the reference microsecond service and divides valid readings
by 10000. When that service is unavailable, it returns kernel ticks directly.
Avoiding conversion through microseconds preserves large representable tick
values. `_SC_CLK_TCK` remains 100; the self/child CPU fields, optional-output
extension, usage-error propagation and unchanged output on failure are retained.
There is no kernel, layout, syscall or device-profile change.

The default timing host target replaces the explicit failed audit. Both sources
are tested with zero, sub-tick/boundary values, fractional reference readings and
`LONG_MAX`, with and without output; successive samples advance by 50 ticks.
Distinct self/child user/system values verify 100 Hz accounting independently of
the elapsed source. Injected `EIO`/`ESRCH` from either usage call verify short
circuiting, unchanged output and absence of elapsed-clock queries on failure.
Successful calls preserve errno. Three source-copy negative controls fail with
exit 1: restored frozen return, erroneously dividing fallback ticks by 10000,
and wrong CPU accounting units. The checkout remains fixed throughout.

Guest `--test times`, included in `all`, checks `_SC_CLK_TCK`, brackets both
stored/null-output elapsed returns with monotonic samples, verifies at least
ten ticks over a 100 ms sleep and checks nondecreasing CPU totals. It reports
which source ran. The default VM uses the reference clock; missing-reference
coverage is host-modeled and does not claim a native or VM fallback boot.
Evidence is retained in ignored `build/review/times-fallback/`.

Final validation passes on ARM64 macOS with x86_64 QEMU TCG: matched build,
host timing/synchronization targets, timing ASan/UBSan, targeted `times` guest
group, complete thread suite and full libuv suite. Targeted/full guest elapsed
samples advance 10 and 11 ticks respectively and report the reference source.
Docs check 641 local links with zero errors; whitespace checks pass. Evidence
includes source/log hashes and the matched image manifest. No full Linux/core,
general SMP, native or fallback-VM acceptance is claimed.

## Continued audit checkpoint after times correction

The bounded review followed elapsed and CPU source selection through libc
`clock`, `clock_gettime`, `clock_getres`, `times`, `getrusage`, `_SC_CLK_TCK`,
`sysinfo`/`AT_CLKTCK` and the kernel usage/reference/tick services. It also
inspected `utime`/`utimes` conversion into `utimensat`/`futimens`, and the existing
relative/absolute sleep validation and remainder paths. CPU microseconds and
100 Hz accounting remain distinct; file timestamps retain documented one-second
storage and refusal of negative epochs. No additional defect was confirmed in
this pass. This is a bounded source review plus the stated host/guest coverage,
not exhaustive POSIX validation or acceptance of every source-failure scenario.

The next audit scope is deterministic sleep interruption/remainder coverage
with unavailable clocks and boundary values. Abnormal once-initializer exit
remains unconfirmed. Broader semaphore and other profile exclusions remain
open; no newly confirmed finding is being presented for another fix yet.


## Historical sleep interruption/remainder audit (published at bd300e28)

The following findings describe the runtime at `820c1c3e` and probes committed
in `bd300e28`, before the correction below.

This block adds positive controls and isolated failing regressions against the
existing implementation. The libc/kernel runtime remains unchanged. Evidence
is retained in ignored `build/review/sleep-audit/`.

### P2 — Signal-handler execution is deducted from unslept time

In `posix_time.c`, `nanosleep` samples its start before dispatching pending
signals, then samples its end only after post-wait handlers return. The computed
elapsed time therefore includes handler execution. The [Issue 8 nanosleep
contract](https://pubs.opengroup.org/onlinepubs/9799919799/functions/nanosleep.html)
defines the remainder from the requested interval and time actually slept;
handler execution occurs after suspension is interrupted. Relative
`clock_nanosleep` delegates to this implementation and inherits the defect.
Restarting from the reported remainder can omit time that was never slept.

```sh
python3 tools/test_time_contracts.py --audit-sleep-handler \
  --output build/review/sleep-audit/handler-host-final
# Exit 1: 24/24 cases report zero instead of the expected remainder.
python3 tools/thread_smoke.py --test sleep-handler-audit --boot-wait 60 --run-wait 120
# Guest status 1; harness exit 3: 6/6 cases fail.
```

Host cases cover `nanosleep` and relative realtime/monotonic `clock_nanosleep`,
reference/tick fallback, signals dispatched before or after the kernel wait,
and separate/aliased request-output buffers. The request is 1 s + 123 ns;
modeled suspension is 250 ms, and the handler runs 1.5 s. Post-wait remainder
should be 750000123 ns; pre-wait dispatch should retain the entire request.
All variants return the correct interruption error but incorrectly report zero.

The guest uses `SA_RESTART` handlers and thread-targeted signals. For a 500 ms
request, handler entry occurred 25.553–32.760 ms after the start; each handler
then ran about 600 ms. All three APIs, with separate and aliased output, returned
zero remaining time. The probe checks interruption before 250 ms, delivery count,
normalized output, return errors and errno, so a late signal cannot masquerade
as this failure. Fast-handler controls run in the positive `sleep-remainder`
group, included in `all`. Absolute `clock_nanosleep` remainder preservation has
separate positive host coverage; this finding concerns relative sleeps.

The next correction should capture the suspension end before invoking handlers
and preserve the entire request if no wait occurred. Keep aliasing, direct
`clock_nanosleep` error returns, errno and absolute-output rules. For deadline
semantics, callers can keep a monotonic absolute deadline and retry it after
`EINTR`; that intentionally counts handler wall time and is not a substitute for
relative suspension semantics. Do not rely on the current relative remainder
when handlers can run appreciably long.

### P3 — Extreme fallback elapsed ticks wrap when scaled to microseconds

The same remainder path multiplies an unsigned elapsed tick difference by
10000 before splitting it into seconds/nanoseconds. That multiplication wraps
once the tick difference exceeds `UINT64_MAX / 10000`. These values are accepted
by the request-to-ticks overflow guard and fit both the kernel tick interval and
`time_t`, so the returned remainder can be wrong even without handler execution.

```sh
python3 tools/test_time_contracts.py --audit-sleep-overflow \
  --output build/review/sleep-audit/overflow-host
# Exit 1: 6/6 cases return 18446744073709.991616000 rather than 0.440000000.
```

The host injects a tick difference of 1844674407370956 (the first overflowing
value), a request of 18446744073710 seconds, and an interruption with no handler.
All three relative APIs, with separate and aliased buffers, reproduce it. Split
ticks into whole seconds and a bounded remainder before scaling to avoid this
intermediate overflow. This is an extreme-range arithmetic issue: roughly
584,000 years of elapsed time, host-modeled only, with no VM/native occurrence
claimed. It has lower practical priority than handler-time subtraction.

### Positive controls and continued review

The default timing target now covers short-handler interruptions before/after
waiting, aliased/separate/null remainder, nanosecond borrowing, expiration clamp,
reference failure/backwards readings with tick fallback, zero and sub-tick waits,
invalid/oversized requests, non-interruption errors and absolute remainder
preservation. Both explicit audits are excluded from the positive default gate.
The guest `sleep-remainder` group covers six fast-handler cases; the existing
`sleep` group retains no-early-completion and absolute deadline controls.

Continued review followed the kernel wait loop and its existing host fixtures:
spurious wakes recheck deadlines, pending signals interrupt, and overflow is
refused. The extreme fallback finding above came from extending this review to
the libc conversion boundary. No third issue is confirmed. The next bounded
implementation work is suspension-end capture, then overflow-free remainder
conversion. Once-initializer abnormal exit remains unconfirmed; broader profile
exclusions and general asynchronous signal delivery remain open.


Final audit validation on ARM64 macOS/x86_64 QEMU TCG: matched build, final host
timing/synchronization tests, final timing ASan/UBSan and the complete positive
thread suite pass. Explicit handler and overflow host modes each exit 1; the
handler guest audit returns status 1/harness exit 3. Docs check 641 local links
with zero errors; whitespace checks pass. Receipts, source/log hashes and build
manifest are retained with the evidence above. Previous libuv validation remains
historical; this test/docs-only audit claims no new full Linux/core, general SMP,
physical-hardware or absent-reference VM acceptance.


## Sleep remainder corrections

`nanosleep` now captures the remaining interval immediately after an interrupted
kernel wait, before invoking post-wait handlers. If pre-wait dispatch handled a
signal, no suspension occurred and the full request is retained. A local
`timespec` holds the captured value until the interruption return, so nested
handler sleeps and caller-buffer writes cannot replace that snapshot. Relative
realtime/monotonic `clock_nanosleep` inherits the fix; direct errors, saved errno
and absolute-output behavior remain intact. Successful and non-interruption
error paths preserve their previous output behavior. No layout, syscall or
kernel change is introduced.

The P3 correction is in the same calculation: divide elapsed ticks by 100 for
whole seconds and scale only the remainder (0–99) into nanoseconds. This avoids
the overflowing intermediate multiplication without changing the 100 Hz source
or nanosecond borrowing/clamp logic. The maximum quotient fits `time_t` for the
kernel's 64-bit counter.

The old explicit audit modes are replaced by positive default timing tests.
The 24 long-handler combinations now preserve the expected remainder across
three relative APIs, both sources, pre/post-wait dispatch and separate/aliased
buffers. Added controls model a handler changing the caller's output and verify
`sleep` rounding and `usleep` interruption. Absolute remainder preservation also
runs with a long handler. The former overflow probe expands to 24 combinations
at either side of the old scaling limit and near the signed counter maximum.
Existing invalid/error, source failure/backwards-reading, zero/sub-tick and
clamp cases remain included.

Guest `sleep-remainder` now includes 18 cases: six fast handlers, six 600 ms busy
handlers and six handlers that call a nested 600 ms `nanosleep`. Each interrupts
a 500 ms relative request early and verifies retained unslept time, aliases,
return errors and errno. It is included in `all`; the old audit entry point is
removed. The default VM uses the reference source. Fallback and extreme ranges
are host-modeled, not a claim of absent-reference VM or native acceptance.
Evidence is retained in ignored `build/review/sleep-fixes/`.

Continued bounded review traced `sleep`/`usleep` wrapper rounding and errors,
cooperative nested handler dispatch, source failures and output ownership.
The new wrapper and nested-handler controls substantiate these paths; no further
confirmed finding was added. Scheduler time between wake and userspace sampling
and tick resolution still limit precision; the correction excludes handler
execution without claiming a kernel-provided interruption timestamp. The next
audit area is deterministic futex wait interruption and synchronization wake
ownership coverage. Initial source review traced the absorbing-signal wrapper
(all current callers use an infinite timeout), condition deadline recomputation,
expected-value checks and scheduler queue membership; it added no confirmed
defect and is not a race-test acceptance claim.
Once-initializer abnormal exit remains unconfirmed, and broad profile exclusions
remain open.


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
