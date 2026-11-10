# Air6,1 screenshot latency: research and implementation plan

Research date: 2026-11-09. Integration baseline `7b529b5e`; compiled native
payload `b0169186`, ports `39ad9d7`, release `69a87466479ab140`.
Three independent read-only agent audits covered saved evidence, filesystem/
USB storage, and screenshot transport/save/cleanup. No production change,
new build or physical trial was made during this research. This plan owns only
Air6,1 evidence; PR19 networking, PR301 scanout and PR379 WebCore stay separate.

## Decision

First instrument **which content and filesystem work the blocking global sync
actually performs**, joined to the existing retained worst call. Then optimize
one proven source of redundant work and repeat the unchanged automated stress.
Treat pixel-transfer pacing as a separate, second experiment. Keep first-failure
battery diagnostics in every trial; no battery transport change is supported by
the two clean runs on this payload.

## What the evidence establishes

Both separate native runs complete the original1800-second1% audio/disk/app/
three-screenshot workload, with zero recorded audio underruns, clean ownership
retirement and preserved prior files. The desktop loop remains within500ms.
The remaining problem is screenshot completion latency, not the earlier UI halt.

| Observation for the slowest screenshot | First run `f76703e09f064f11` | Repeat `a9e06a47ddb44284` |
| --- | ---: | ---: |
| Capture / worker PID | 1 / 10 | 3 / 937 |
| Total capture | 23.490265s | 27.508960s |
| Worker pixel receive | 5.853363s | 5.857470s |
| Worker WRITE phase | 14.094040s | 18.155823s |
| Raw SYS_WRITE wall,65 calls | 13.769417s | 18.147703s |
| Raw write admission gate | 13.367754s | 17.797443s |
| Gate share of raw write wall | 97.08% | 98.07% |
| Handler wall | 21.326ms | 20.850ms |
| Gate time overlapping an I/O owner | 99.919% | 99.920% |
| Disk worker PID7 worst SYS_SYNC | 2.327724s | 3.784433s |
| That sync: file phase | 2.327318s | 3.784027s |
| That sync: final device phase | 406us | 406us |
| File phase CBW / data / CSW callbacks | 170 / 538 / 170 | 305 / 1033 / 305 |
| File phase USB callback wall | 2.038611s | 3.155561s |
| File phase checkpoints / remaining wall | 215.416ms / 73.291ms | 471.781ms / 156.685ms |

The repeat's worst screenshot gate wait is3.787447s, with3.782580s of direct
PID7 SYS_SYNC ownership overlap; this matches its separately retained worst
sync. Its slowest retained CSW callback is358.362ms:358.358ms from submission
to software observation, then1us to resumption. A29.811ms sampled service gap
is also retained. These are wall/software observations, not CPU time, USB wire
speed, exact device completion time or proof of a controller defect.

Raw evidence is ignored local storage under
`build/power/air61-protocol-first-fault-20261109` and
`build/power/air61-protocol-repeat-20261109`, particularly the matching final
logs, `physical-assessment.json`, `operations.json`, `capture-write-timing.json`
and `sync-timing.json`. Both have verified independent backups outside build.
Parser semantics live in `tools/capture_write_timing.py` and
`tools/sync_timing.py`; records for the same identity must be correlated rather
than adding unrelated maxima. Phase totals include worker scheduling and need
not sum to complete launch-to-observed-finish time.

There are only three captures per run. Separate cold boots, different metadata
state and workload interleaving confound a simple comparison. USB baseline files
grew751 to760; post-run allocation grew3054 to3161 clusters. These maxima do
not establish percentile latency, statistical significance or a stable change.

## Code findings and source research

- `process_file_sync_all()` in `kernel/src/process.c` already visits each live
  dirty shared content object once. Do not propose descriptor deduplication as
  a new optimization. `content_flush()` commits its current complete buffer via
  `exfat_write_file_retryable()`.
- `userspace/playwav/io_stress.c` retains fsync, close, rename, full readback,
  unlink and global sync in each disk burst. Global sync can include another
  process's dirty screenshot prefix. Repeated prefix commits are a plausible
  amplification mechanism; current records do not identify the flushed file.
- `kernel/src/fs/exfat.c` already has bounded mutation cost counters for
  metadata/allocation/data/free reads, writes and flushes. Its four global
  slow-operation slots are not bound to the worst sync or content identity.
  Reuse the counters with explicit nesting and correlation instead of adding
  an unbounded operation trace.
- The append path writes the new suffix after proving the old prefix. Rounded
  cluster zero/copy/write occurs in the new/COW data writer, not in the allocator
  itself. Tail-zero reduction is therefore a conditional secondary candidate,
  not a demonstrated fix for append or global-sync delays.
- `userspace/lib/src/desk_capture_job.c` permits four writes of at most4096 bytes
  per poll. `userspace/deskd/deskd.c` polls once per desktop turn, with an existing
  one-tick wait while busy. The4196352-byte pixel image plus request needs at
  least257 turns. This is a concrete pacing constraint; its exact contribution
  to the5.85s receive phase needs successful-byte/EAGAIN/gap measurements.
- The response reader handles one32-byte progress/final frame per turn. The
  expected stream is well below the64KiB pipe capacity, so progress backpressure
  does not explain the observed14–18s worker WRITE phase. Worker phase clocks
  and desktop final-response observation are distinct measurements.
- `userspace/shotworker/save.c` already uses64KiB writes. It preserves immutable
  bytes, exclusive temporary creation, fsync/close/global sync, complete temporary
  readback, checked rename, second sync and complete final readback. Keep these.

The [Microsoft exFAT specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#765-validdatalength-field)
requires reads beyond ValidDataLength to return zero; its
[write-order guidance](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#81-recommended-write-ordering)
orders dirty marking, allocation and directory publication. These requirements
constrain any zero-fill or metadata optimization; they do not justify relaxing
SrvrOS's stronger existing checks.

The [USB-IF Bulk-Only Transport specification, section3.4](https://www.usb.org/sites/default/files/usbmassbulk_10.pdf)
requires the previous command's CSW before another CBW. A CSW also conveys
command success/failure. Therefore queueing simultaneous BOT commands, treating
data transfer as completion, or dropping status validation is unsuitable here.
Reducing equivalent command work must preserve transport serialization.

## Ordered work

### 1. Correlate the expensive sync to content and mutation work

Start with the existing explicit timing profile, not ordinary always-on logging.
In `process_file_sync_all()` / `content_flush()` and `sync_timing`, retain a
fixed-size accumulator for the current sync, copied **with that same call** when
its existing worst-call record wins. Include content count, attempted/succeeded
bytes, and the slowest content's slot plus generation or equivalent immutable
identity, inode where valid, logical size, dirty state, start/end and outcome.
A reusable slot or an inode alone is not a unique lifetime identity. No paths,
raw pointers or file contents belong in persisted telemetry. Current content has
a dirty boolean, so do not claim exact dirty ranges without implementing and
validating a separate observer.

Bind the existing exFAT counters to that content subcall. Distinguish root data/
prefix proof and nested sidecar equality/replacement, plus allocation and
publication work, using fixed roles with saved/restored nesting. Count actual
reads/writes/flushes and bytes, not estimated savings. Capture phase wall time,
USB command classes/counts and residual wall only as needed to separate repeated
proof reads, payload writes and metadata traffic. Do not double-count nested
roles or subtract unjoined maxima. This observation must not bypass any proof.

For comparative capture timing, also retain a fixed three-entry final capture
summary (capture ID, worker identity, start/finish and each worker phase), or an
equivalent verified per-capture receipt. Existing current/slowest records alone
can lose the middle capture and cannot supply an all-three median. Strictly
require the expected three distinct completions with consistent clocks.

Persist bounded final/worst aggregates through the existing whole-record writer;
prove atomic record size and64KiB tail retention with all existing receipts.
No per-sector console logging, dynamic tracing allocations or extra device I/O.
Missing identity, nesting conflicts, clock rollback, counter overflow or dropped
records make attribution unavailable/failed, not a performance pass. Ensure the
same owner remains alive through snapshot publication and retirement.

First run a deterministic host fixture with a growing screenshot buffer and a
foreign global sync between ordinary64KiB writes. Include an unrelated dirty
file and shared descriptors/read-only survivors. Prove which content is committed,
record exact block callbacks/order/flush barriers, and inject slow operations and
failures. This should reproduce amplification cheaply before another native run.
Then run the unchanged30-minute native stress with persistent correlated proof.

### 2. Select one equivalent-work optimization

Choose from the correlated result, not all at once:

| Measured dominant work | Candidate | Proof required before physical testing |
| --- | --- | --- |
| Repeated prefix proof or metadata serialization/reads | Coalesce reads within the existing owner-held immutable mutation scope | Every original byte is checked; no cross-mutation hint/equality cache; identical refusal and publication behavior |
| Repeated equivalent sidecar/block writes | Batch adjacent equivalent operations inside existing durability boundaries | Exact bytes and barrier ordering; keep owner/global/publication flushes and changed-metadata path |
| New/COW rounded payload tail writes | Avoid redundant physical tail work only if contract review permits | Exact visible bytes, hole/VDL zero semantics, allocation validity, no stale-data exposure or earlier publication |
| Controller-event servicing before observation | Isolate event/service latency with matched command evidence | Preserve BOT sequence, original deadlines, DMA quarantine and status/error validation |

If these do not explain the delay, review the evidence again rather than weakening
global sync or admitting arbitrary memory writes under a foreign I/O owner.
Global sync must retain its shared dirty-content semantics. The64KiB write bound,
pipe size and workload remain unchanged.

### 3. Address pixel-transfer pacing independently

After storage attribution, add bounded transfer aggregates if needed: polls,
successful bytes, partial writes/EAGAIN/EINTR, first/last send and maximum gaps,
worker final-production versus desktop observation time. These distinguish pipe
readiness, UI cadence and scheduling. Treat one-frame-per-turn final-observation
latency separately from worker save time.

If UI cadence dominates, investigate an independently owned transport worker with
readiness waits, retaining existing per-admission bounds and immutable snapshot.
It needs explicit exclusive ownership, cancellation, worker-exit and retirement
proof; it must not share a mutable capture job with the UI or block render/input.
This is a separate change and trial, not bundled with filesystem optimization.

## Validation and acceptance

For the first implementation, extend `test-sync-timing`, `test-exfat-cost`,
`test-capture-write-timing` and relevant real filesystem/process host fixtures.
Test exact joining of worst sync/content/role records, replacement by a later
worst call, clean/no-op calls, failure preservation, nesting, overflow and strict
parser rejection. Compare original versus candidate controller callback ordering
and barriers for changed and unchanged metadata, append, COW and failure cases.
For transport work, extend `test-capture` with full-size paced transfer, progress
bursts, reader loss after accepted writes, and exit during transfer/cleanup.

Build matched kernel/userspace/submodules; run core/desktop and appropriate
ordinary capture/readback/Preview/repeat/exit checks. Run the populated-Home
loaded VM sequentially, then the exact installed-stage rehearsal. No concurrent
build/staging/VM harnesses in the checkout. Inspect actual current PR overlap
before edits and integrate changes through pinned-head self-reviewed MRs.

Native acceptance retains1800s PCM plus the original60s tail,1% volume,500ms
loop bound,10s lifecycle bound, full disk readback/receipts, three exact BMPs,
post-capture app activity, clean worker/cache/battery retirement, post-PCM protocol
marker, prior-file hashes and offline fsck/allocation/headroom checks. No early
success notification, skipped verification, weaker quarantine, ownership bypass,
watchdog change or power-policy change.

For a latency claim, obtain a fresh instrumented baseline if all three old
capture histories are unavailable; never reconstruct them from raw-write clocks
or unrelated maxima. Compare all three captures and corresponding same-operation
counts/bytes against the baseline, then repeat a successful candidate on another
cold boot. Report median and maximum, disk throughput and baseline file/metadata
state; do not call fewer workload rounds an optimization. Repeated improvements
are descriptive evidence, not a percentile guarantee from six samples. A failed
performance goal must still retain integrity results and diagnostic evidence.

Before native reboot verify this unit/model/USB, Ubuntu default, empty/staged
one-shot, recovery/current payload hashes and independent backup; save the full
run record and collector state. Collect the matching return before any new run.
The intermittent battery cause stays unresolved until a real first-failure record
or reproducible causal regression supports a separate fix.

## Native checkpoint after the first implementation (2026-11-09)

PR446/release `1991ff7f10142881` completes the unchanged30-minute native test,
including every old receipt and the new correlated records/all three captures.
See the [machine evidence](machines/macbookair6-1.md). Captures19.119232s,
26.609181s and21.991400s provide a complete median21.991400s baseline.

The native selected content flush is write-dominated:2.040995s root payload and
1.071359s nested sidecar writes, versus183.299ms prefix proof, within3.431072s
content wall. This differs from the throttled VM's prefix-read-dominated sample.
No native prefix-read optimization or improvement follows from that VM result.
Optional inode0 cannot prove which process wrote the content; the observed write
admission wait is independently bound to the selected sync's ownership interval.

Next bind BOT command classes, bytes/counts and retained worst-command waits to
the existing selected content roles. A351.828ms CSW for a512-byte WRITE(10),
with seven CSW callbacks above100ms in the selected sync, warrants separating
payload/sidecar/metadata command costs before selecting a fix. Preserve transport
serialization, original command deadlines, controller/event semantics, quarantine
and all durability checks. This measurement narrows the implementation choice;
it does not establish hardware latency or justify dropping writes/barriers.

## Native command-role result and next audit (2026-11-09)

PR448/release `690fd179a79d156f` passes the unchanged1800-second1% workload:
1101 app cycles,4076 verified disk bursts, three exact BMPs, zero recorded audio
underruns,120.023ms maximum desktop loop, clean retirement and clean offline
allocation. Independent raw parsing reproduces every retained parent/role join.
See the [machine result](machines/macbookair6-1.md#native-command-role-attribution-2026-11-09).

The selected2.757894s content window is dominated by root payload callback writes
(2.365736s). Its354 physical commands include44 writes:15 of512 bytes account
for43.861ms CSW wall;29 larger writes account for2.114728s. All six CSWs above
100ms belong to larger root-payload writes. The matched worst is a16KiB WRITE,
353.884ms wall, with353.673ms before software observation and209us to resume.
Its sampled service gap is14.661ms. Small metadata and sidecar writes are cheap
in this selected window; do not generalize the previous unclassified512-byte
maximum into a metadata optimization. Optional inode0 still cannot identify the
writer or path, and software observation does not establish device latency.

Complete captures18.798846/23.601685/20.066622s provide median20.066622s, but this
passive instrumentation trial and the preceding cold boot differ in workload
interleaving and metadata state. No speed improvement is accepted. Screenshot
completion remains open despite passing desktop responsiveness and integrity.

Audit the existing serialized payload-command CBW/data/CSW path and controller
service sampling next. Determine which part of the same slow command remains
unexplained before choosing an optimization; retain minimal bounded matched
timing only where existing records are insufficient. Keep the16384-byte command
bound, BOT sequence, deadlines, event/status validation, DMA quarantine and every
existing byte proof, ownership and durability barrier. A long CSW does not
authorize a larger command, extra queue, retry or relaxed completion predicate.
Keep pixel-transfer pacing separate and the intermittent battery cause open.
