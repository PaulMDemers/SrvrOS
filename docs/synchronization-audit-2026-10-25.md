# Synchronization and stack contract audit — 2026-10-25

## Current disposition

The three original findings below are fixed on PR175: supplied storage is never
expanded, condition variables retain their selected clock and recheck absolute
deadlines, and semaphore increments enforce `SEM_VALUE_MAX=INT_MAX` atomically.
The fourth finding, worker self-stack inspection, is also corrected with
pre-callback metadata and positive host/guest tests. Early self-detach remains
a separate publication-order defect; see the continued audit below.
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

## Continued audit — P2: early self-detach depends on parent publication

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
