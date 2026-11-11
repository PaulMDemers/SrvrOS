# Private append waits and overlapping commits

The explicit capture-write diagnostic now retains a completed commit associated
with an actual private append wait. It adds no transfers or policy changes.
The original append-wait, capture, sync/content/command and retirement contracts
remain independently required.

## Selection and meaning

The original prelude clocks bracket the wait. The observer copies the target
content identity, live size, captured/committed prefix base, effective request
bytes and whether that target already has a live backend borrow. Those fields
must describe the original full256KiB suffix window and64KiB request bound.
They do not grant permission to append or retain a content/descriptor pointer.

While a wait is active, the sync observer copies each completed content call's
actual parent sync ID, content identity and closed USB observer. Its overlap is
`[max(wait_start, content_start), content_end]`. Only calls ending before the
wait returns contribute. Among them, the first longest overlap is retained;
across waits, the first longest whole prelude is selected. A global sync/content
maximum from a different interval cannot substitute for this copy.

The final report includes the selected call's whole I/O/USB counters and exact
maximum-status command phases and sampled ring observations. These counters can
include work before wait entry. A matching identity distinguishes the target's
own content from a different file's global commit; overlapping intervals alone
do not prove the scheduler owner, an exclusive critical path, device firmware
latency or controller arrival time. Uncovered wait wall remains explicit.

An unfinished call at wait return is marked open and never retroactively
attached. No completed overlap, no USB command and an unexercised wait are
separate explicit outcomes. Failed/invalid/inflight, overflowing, conflicting,
missing or unbound evidence is refused. Diagnostic invalidity does not change
the original operation's result, ownership, quarantine or fallback.

## Bounds and evidence

CPU0 IRQ exclusion protects fixed slots using the existing eight-record bound.
There is no allocation, console output, polling or scheduling in observer hooks.
Only one selected wait is reported, at the same final timestamp as the existing
capture/append counters. Production rows fit the1024-byte atomic console bound
at full64-bit width; the entire extension is below4KiB. Periodic traffic, the
10-second logger cadence and64KiB tail are unchanged. All original final
receipts must still survive and parse; a missing receipt is not a pass.

`make test-commit-wait` executes the real sync producer through the production
formatter and strict parser, with explicit time/USB host models. It covers an
unrelated larger global maximum, clipped/completed and unfinished calls,
concurrent waits, ties, PID reuse, failures, capacity, clock and overflow
refusal, malformed companions and record-size bounds. Existing reserved-append
and scheduler tests retain the actual lease/window/notification/ownership
checks. Real USB command tests and the loaded VM remain separate requirements.

The native Air6,1 test keeps the original1800-second1% audio/disk/app workload,
three screenshots and500ms/10s limits. New collection must include this companion
alongside the original16 contracts and append-wait proof. Actual isolation
coverage and physical acceptance remain pending the matching native return.
