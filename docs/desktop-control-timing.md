# Desktop control-phase timing

The existing audio/disk stress loop records a version-1 control companion for
its retained worst loop. This is diagnostic instrumentation: the 500 ms loop,
ten-second whole-app, workload, log, ownership and integrity limits do not change.
Ordinary desktop sessions do not enable it.

Four monotonic edges partition the existing control interval into bookkeeping,
periodic reporting, audio control and I/O-trial bookkeeping. The report region
records whether the original ten-second/state gate ran, the observed state,
operation snapshot and report submission clocks, and admission result. Audio
records the child identity, original nonblocking wait flags/result and clocks,
completion transition, and optional progress-notice clocks. Explicit execution
bits distinguish an uncalled operation from a measured zero-duration operation.

Three disjoint thread-scoped scheduler traces cover report, audio and I/O
regions. A failed BEGIN invalidates the companion and never ends a trace it did
not acquire; incomplete loop exit releases only traces owned by this record.
The original loop stage boundaries and worst-loop selection remain authoritative.
Records are fixed-size current/worst copies, including the existing immutable
report-worker handoff. The UI turn performs no new logging or allocation.

`desktop-loop-control` carries sequence, schema/version, validity, base clock,
relative edges and inner clocks, execution bits and child/result fields.
`desktop-loop-control-sched` carries the same sequence/final identity, a region
index (report=0, audio=1, I/O=2), and all 21 existing scheduler fields in their
original order: version, start_us, end_us, start_tick, end_tick, blocked_us,
ready_us, blocks, signals, timeouts, dispatches, exclusions, invalid,
max_ready_us, wake_us, run_us, wake_tick, deadline_tick, reason,
ready_exclusions, late_ticks. Relative clocks and positional vectors save bytes
without dropping scheduler evidence; every atomic row retains the existing
1024-byte bound and the bootlog/exit-file cap remains 64 KiB.

`tools/control_timing.py` binds the four complete newline-terminated rows to the
same original worst loop. It rejects changed periodic/final copies, missing or
conflicting rows, overflow, invalid flags, clock/order errors, scheduler
inconsistency and changed call/child/NOHANG semantics. It conserves the original
control wall interval exactly. Inner operation durations are subsets, not extra
phases; scheduler time and wall time do not establish CPU work or USB-device time.
A record does not prove a specific cause until matching native evidence is saved.

Run `make test-control-timing` for the actual C producer, immutable handoff,
trace ownership/failure, parser corruption and full-width/late-session row-budget
regressions. The loaded stress harness accepts `--require-control-timing` to
require the companion in saved workspace logs. Native assessment must require
it alongside all original workload, saved-log and retirement predicates; a
foreign-profile exact-stage refusal remains refusal evidence only.
