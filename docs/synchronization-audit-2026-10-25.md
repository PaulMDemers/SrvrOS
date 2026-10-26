# Synchronization and stack contract audit — 2026-10-25

## Scope and evidence

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

## Findings

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

## Reproduce

From the repo root with the macOS command-line C compiler:

```sh
python3 tools/audit_sync_contracts.py --output build/review/sync-audit/default
# Expected exit 1: known defects, not a failed invocation.
python3 tools/audit_sync_contracts.py --expect-known-defects --output build/review/sync-audit/known
# Expected exit 0: all four known observations reproduced, NOT conformance.
```

The current receipt is:

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

## Documentation reconciliation

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
