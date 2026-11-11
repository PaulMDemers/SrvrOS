# Private wait, I/O owner and USB status isolation

The explicit capture-write diagnostic associates the worst private append wait
with actual scheduler I/O-owner intervals. It reuses already-timed USB status
callbacks. No transfers, controller reads, clocks, polling or policy are added
inside those observation hooks. The original append-wait, capture, sync/content/
command and retirement contracts remain independently required.

## Why the owner is recorded

An initial completed-content-only prototype passed the original loaded VM
contracts but found no completed SYS_SYNC content inside its worst private wait.
The accurate unavailable result is retained locally; it is not an isolation pass.
Kernel work such as workspace logging also owns I/O. The revised observer records
owner edges from the existing scheduler diagnostic, including kernel PID zero,
thread identity, scope sequence, syscall, sanitized name and original clocks.
It does not change acquisition, eligibility, notifications, handoff or release.

The original prelude clocks bracket the wait. The observer copies the target
content identity, live size, captured/committed prefix base, effective request
bytes and whether that target has a live borrow. They must describe the original
full256KiB suffix window and64KiB request bound. No descriptor/content pointer
survives a wait and no observation grants permission to append.

For each wait the first longest owner intersection is retained. Its overlap is
`[max(wait_start, owner_start), min(wait_end, owner_end)]`. A still-live owner at
wait return has no end timestamp: it is explicitly partial and never backfilled
with a later release. Completed owner intervals and the final partial interval
are disjoint; their sum leaves uncovered wait wall explicit. Across waits, the
first longest whole prelude is selected using the original aggregate clocks.

## USB and completed-content companions

Only existing timed status callbacks feed the new sink. It copies the same
callback/controller sample into the live owner observation without additional
transfers, controller sampling, event checks or timestamp reads. It retains
callback totals, clipped overlap, 512-byte/16KiB WRITE subsets and the status
callback with the first longest actual wait overlap, including its original
command tag/size, dispatch/resume clocks and sampled ring outcomes.

Callback clocks can start before wait entry. They are software observations,
not hardware arrival or exclusive CPU time. Bulk success is not the later full
SCSI command validation; all original transport checks remain. A lack of observed
status does not prove an owner issued no USB I/O: only already-instrumented
completed callbacks contribute. The owner identity remains useful independently.

Completed content calls are also copied at their own completion, independently
of the unrelated global maximum. The first longest intersection within the
selected wait supplies optional whole-call I/O counters and parent/content IDs.
Calls completing after wait return never attach retroactively. This separate
content selection is not automatically the selected I/O owner or its counters.

## Bounds and evidence

CPU0 IRQ exclusion protects fixed slots using the existing eight-record bound.
There is no allocation, console output, polling or scheduling in observer hooks.
One version2 companion is emitted only at the final capture/append timestamp.
Every production row fits the1024-byte atomic console bound at full64-bit width;
the entire extension is below4KiB. Periodic traffic,10-second logging and64KiB
tails are unchanged. All original final receipts must still survive and parse.

`make test-commit-wait` executes the actual producer through the production
formatter and strict parser with explicit time/USB host models. Cases include
kernel owners without SYS_SYNC, partial ownership without invented release,
multiple owners, unrelated content maxima, clipped/unfinished content, concurrent
waits, ties, reuse, failures, capacity, clocks, overflow and malformed evidence.
Actual scheduler and USB host fixtures verify forwarding while retaining their
original policy, command, clock-count, quarantine and ownership checks. Loaded
VM coverage requires an actual owner; status coverage is reported explicitly.

The native Air6,1 test retains the original1800-second1% audio/disk/app workload,
three screenshots and500ms/10s limits. Matching collection must parse this
companion alongside the original16 contracts and append-wait proof. Missing,
conflicting, failed or invalid evidence is refused. Physical isolation and
acceptance remain pending that return.
