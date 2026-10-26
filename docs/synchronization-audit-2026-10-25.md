# Synchronization and stack contract audit — 2026-10-25

## Current disposition

The three original findings below are fixed on PR175: supplied storage is never
expanded, condition variables retain their selected clock and recheck absolute
deadlines, and semaphore increments enforce `SEM_VALUE_MAX=INT_MAX` atomically.
The fourth finding, worker self-stack inspection, is also corrected with
pre-callback metadata and positive host/guest tests. The fifth finding, early
self-detach, is now corrected with creator reservations and serialized lifecycle
claims. The sixth finding, thread-name round trips, is corrected with bounded
record storage and range/lifetime checks. Initial-thread `pthread_exit` killing
live workers is the next guest-confirmed lifecycle defect; see below.
See the correction/validation record below. The original findings and receipts
are retained as history at source `a90ac149`; they do not describe the corrected
implementation. The original audit-only runner was replaced by positive
`tools/test_sync_contracts.py` regressions, integrated into `test-core`.

**Matched rebuild required:** `pthread_cond_t` now includes its clock (8 bytes,
previously 4). Static initializers retain realtime. Rebuild embedded users such
as the libuv adapter and all apps; no binary compatibility with old objects is
claimed.

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

## Continued audit — P2: initial-thread exit kills live workers

`pthread_exit` calls `srv_thread_exit`. In `process_thread_exit`, an initial
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
preserve the workers. The current profile excludes initial-thread exit with
continuing workers. Worker-side exits retain their passing regression coverage.
Once-initializer abnormal-exit recovery remains a separate unconfirmed audit
area; this observation does not establish that behavior.

Final validation for the naming correction: matched `all`, positive host suite,
ASan/UBSan, targeted name guest test, complete thread suite and complete libuv
suite pass. The name negative controls and separate main-exit audit fail as
intended. Documentation validates 641 local links with zero errors, and whitespace
checks pass. The failed audit does not count as a passing conformance test.
No Linux/core or physical hardware run is claimed.
