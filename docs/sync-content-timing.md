# Correlated sync content and capture completion evidence

This explicit timing-profile observer implements the first stage of the
[Air6,1 screenshot plan](macbookair6-1-screenshot-latency-plan.md).
It changes no persistence, admission, transport or device policy.

`sync_timing` retains per-PID worst global sync calls as before. Each call now
counts dirty shared content visits, attempted logical bytes and failures, and
retains its slowest content subcall with slot, never-reused boot identity,
optional inode, size, clocks and outcome. Content identities are internal
observation tokens, not handles or permissions. Ordinary no-profile acquisition
still assigns identities; only the explicit active sync records timing. Exhausted
identities invalidate diagnostics without refusing ordinary file operations.

The final workspace snapshot emits one `sync-content-summary` per retained PID,
then a `sync-content-selection` binding the longest file phase among those
retained calls with content visits. Ties follow record order. The selected call's
`sync-content-worst` and six `sync-content-io` rows describe its slowest content
subcall. This is not an independent global worst-content maximum. An inode or
slot alone cannot join different lifetimes. No paths, pointers or user data are
persisted. Failures stay failures; dirty/sticky-error/quarantine behavior is
unchanged. Missing or malformed joins cannot establish acceptance.

The six exclusive block-callback roles are:

| Role | Meaning |
| --- | --- |
| 0 | Root directory/flags/other metadata |
| 1 | Allocation/free, including nested sidecar allocation |
| 2 | Root payload |
| 3 | Root old-prefix payload proof |
| 4 | Nested sidecar work outside its equality proof and allocation/free |
| 5 | Nested sidecar equality proof |

Counters reuse existing exFAT cost clocks for attempted uncached reads, writes,
owner flushes and failures. Bytes are logical requests to block callbacks, not
physical USB bytes or savings. Cached reads do not count as media reads.
Pre-mutation reservation/scanning and other unobserved work remain in residual
content wall. The sum of observed callback wall must not exceed its content
span; no CPU-time or hardware-completion claim follows. The observer makes no
extra controller access or flush and logs only final bounded aggregates.

The desktop also retains its first three terminal captures in immutable operation
reports. `desktop-capture-history` and `desktop-capture-completion` include worker
identity, prepare/finish/retirement clocks and all12 worker phases. Ordinary
additional captures continue; drop/overflow diagnostics make a three-capture
comparison unavailable. The parser requires three distinct complete successful
lifetimes for native stress, or one for the short positive VM. Old current/slowest
records remain intact. These three complete histories provide a fresh baseline;
old maxima cannot reconstruct an unavailable median.

`tools/sync_content.py` joins to the complete final legacy sync snapshot;
`tools/capture_history.py` requires a complete final history. Wrong bindings,
conflicts, overflow, missing roles/completions, failed operations and invalid
clocks fail acceptance. Final rows must fit existing whole-record writes and
survive the unchanged64KiB tail alongside every previous receipt.

`make test-sync-content` exercises real buffered process writes into real exFAT
with a foreign sync between each64KiB write, unrelated dirty content and shared
writer/reader references. Observer-on/off deterministic controller callback
fingerprints and complete media bytes must agree. An injected write failure
retains the previous durable prefix, dirty contents, reader-only survivors and
quarantine. Host clocks are synthetic costs, not native timing acceptance.
Actual C capture-report output is parsed and deliberately corrupted; focused
observer/parser tests cover worst-call replacement and invalid evidence.

## Role-bound BOT command observations

A separate fixed command window nests inside the sync observer while one dirty
content subcall is active. The root begins it only after acquiring its own
auxiliary observer; a refused begin never ends a foreign window. The window is
copied with that same content and worst sync. Filesystem block callbacks bracket
their exclusive role before transfer and restore it afterwards. Role6 explicitly
covers unclassified/pre-mutation work, which the old cost-active role counters
leave in residual wall. Physical command counts need not equal logical block
callback counts: splitting and read/modify/write remain unchanged.

The passive transport observer counts attempted READ(10), WRITE(10), flush and
other commands, bytes, failed commands and CSW callbacks. It retains CSW wall,
counts above100ms and the worst existing CSW observation per role. Separate
512-byte WRITE counts, CSW wall and slow counts are subsets of all WRITE values;
larger-write values follow by subtraction. This is a size classification, not
an adjusted transfer bound, timeout, queue or device policy.

`sync-command-snapshot`, seven `sync-command-role` rows and worst-CSW rows for
nonempty roles bind snapshot, selected sync PID/start identity and content
identity. Worst rows retain opcode, command size, callback clocks and existing
service/event sample. No LBA, paths or file data are reported. The strict
`tools/sync_commands.py` parser joins these to existing final sync/content proof,
checks class/size/time subsets, service coverage and parent-CSW bounds, and
requires a matching legacy worst-CSW context when it lies inside this content.
Invalid clocks, counters, role nesting or incomplete ownership invalidate only
the diagnostic evidence; they do not authorize retries or change transport
validation/quarantine. Every original CBW/data/CSW/checkpoint remains in order.

The largest new row is conservatively687 bytes at maximum-width unsigned values,
within the existing1024-byte atomic writer. All original and new receipts must
still survive the unchanged64KiB tail. Actual loaded VM and native evidence,
not row bounds alone, establish retention. CSW timing is software-observed wall,
not exact device completion or CPU time. No speed gain or hardware cause follows
from a long CSW without the corresponding bound evidence.

## Same-command phase and ring-result companion

One fixed240-byte companion retains the first strict CSW maximum within the
selected content window. It is copied after the original command validation and
final checkpoint, with role and exact CSW start/end clocks matching an existing
role worst. It is not an independently selected worst CBW, data or checkpoint.
Three phases retain calls, requested/actual bytes, callback wall and first/last
clocks. Checkpoint count/wall and first/last clocks use the original checkpoint
samples. No new clock read or controller operation is added. Complete/ok describe
the command's final validation outcome; a successful CSW callback alone does
not establish successful command validation. Failure, overflow and invalid
clock evidence cannot establish acceptance. The successful timing envelope ends
at the final existing checkpoint sample; it does not include the unmeasured CPU
tail from that sample to function return.

`sync-bot-command`, three `sync-bot-phase` rows and `sync-bot-service` bind the
final snapshot, selected sync/content identity, role and exact CSW interval.
`tools/sync_command_phases.py` strictly joins these to the existing version1
command/content/sync reports. Historical command reports remain parseable by
their original parser; new positive trials separately require this companion.
Missing, conflicting, failed or misbound companions fail the new diagnostic
requirement. No-work snapshots emit only an empty command header.

The service row classifies each original event-ring cycle check as empty or
event-present. It reuses the existing clock before every256th check to retain
the last sample whose subsequent original cycle read found an empty head.
The matching completion event is counted before its owner is cleared. Counts
must sum to the same CSW's service-check count. The empty sample is a pre-check
software clock, not the exact empty-read, event-arrival or device-completion
time. No new event-ring read, clock, wait, scheduler operation or hardware policy
is introduced. Stale ownership, counter overflow and clock regression invalidate
diagnostics only. Event counts may include unrelated devices; they do not prove
which device delayed a command.

All original4096-byte bulk/16384-byte command bounds, BOT serialization,2-second
controller request deadlines, validation, quarantine, checkpoint eligibility,
durability barriers and owner lifetimes remain unchanged. The companion requires
the same atomic-record and64KiB saved-tail proof alongside every older receipt.

## Capture transfer cadence and allocation companions

Only the explicit audio/disk capture workload enables capture-job transfer
observations. Ordinary captures retain their existing clocks and frames. The
request remains176 bytes and the response32 bytes; `DESK_CAPTURE_TRACE_MAGIC`
opts into one `DESK_CAPTURE_READY_MAGIC` frame after both successful worker
allocations and before pixel reception. It carries the current ID, RECEIVE
status, zero reserved/bytes and a software clock. Normal request magic retains
the original stream. Malformed/unrequested/duplicate auxiliary frames fail
protocol validation; an auxiliary clock fault invalidates diagnostics without
changing the original save outcome. The auxiliary frame does not alter the
existing twelve progress-phase accounting. In particular, the old RECEIVE span
still includes allocation and reception.

Fixed scalar observations count active polls, write attempts/outcomes/bytes,
partial writes, EAGAIN/EINTR, exhausted four-write budgets and response reads.
Poll begin/end wall and intervening gaps partition the parent poll span. Write
attempt begin/end wall and gaps partition the write span, including EAGAIN,
EINTR and failed attempts. These are software syscall/poll observations, not
CPU time or exact pipe publication. Request/pixel completion clocks observe
syscall return; the worker may finish reception before the writer resumes.
Worker RECEIVE, allocation-ready, ENCODE and final-production clocks separately
bound allocation, reception and final-response observation. The terminal poll
reuses the original finished clock. Overflow/regression invalidates observations.
The allocation window includes initial progress-frame emission; the reception
window includes READY-frame emission. Neither is pure mmap or pipe CPU time.

Each immutable capture completion copies its transfer scalars before job reuse.
`desktop-capture-transfer`, `desktop-capture-transfer-write` and
`desktop-capture-transfer-read` join final snapshot/index/ID/worker/lifecycle
clocks to the unchanged version1 completion. `tools/capture_transfer.py` requires
all three rows for every expected capture, exact request/pixel/response byte
counts, original chunk/poll bounds, successful history and complete clock/gap
partitions. Transfer companions are emitted only in the final report; immutable
history retains them between checkpoints. Existing periodic history snapshots
remain provisional. Complete input, progress and
one READY/one final frame must agree; absent/disabled/invalid evidence cannot
establish positive diagnostic acceptance.

The full-size deterministic sender fixture uses the actual capture-job code and
a bounded64KiB pipe model. Air6,1 needs4196352 pixel bytes plus176 request bytes:
1026 full writes and at least257 send polls at the existing four4096-byte bound.
There are256 intervening caller intervals, even with a continuously ready peer.
Synthetic cadence is not native timing. Fragmentation, backpressure, interrupted
reads/writes, buffered progress/final output, reader loss, cancellation and
retirement retain byte/order/ownership proofs. No blocking UI poll, new transport
owner, pipe enlargement, pacing change or save/readback shortcut is introduced.
Matched host/VM and saved-tail evidence are required before the native baseline.
