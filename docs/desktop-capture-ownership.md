# Independent screenshot progress and retirement

The compositor uses one persistent capture owner separate from its app lifecycle
worker. Preparation, pipe transport and cleanup cannot queue behind that worker's
blocking app waits, allocations or exec calls. The UI takes one immutable copy
of its painted canvas when the owner has prepared the mapping; no canvas pointer
is retained by the worker. Ordinary input/rendering never performs storage I/O.

## Publication and cancellation

A fixed job follows IDLE → PREPARING → PREPARED → SNAPSHOTTING → TRANSFERRING →
RETIRING → DONE. Release/acquire publication carries ownership. The owner is
paused at PREPARED; the UI atomically claims SNAPSHOTTING before copying pixels.
Cancellation can claim a prepared mapping only before that UI claim. While the
UI holds SNAPSHOTTING, cancellation/stop wait for publication, retaining the
mapping and thread storage. A fixed single-producer/single-consumer mailbox
publishes scalar job observations; the UI never reads a concurrently mutated
resource job. UI busy remains set until complete retirement is consumed.

The owner admits at most four4096-byte writes and one32-byte response read per
turn, using the existing64KiB pipes. Ready input/output can admit another turn
without waiting for rendering. After all input is sent it waits only for replies;
EOF, not a process-status peek, ends an incomplete response stream. Original
progress/ID/phase/length checks and immutable first-three histories remain.

Cancel before all input is sent closes through the owner and retires the child.
Once full input has been published, accepted persistence drains through its
normal final verification response. Stop joins the owner after all mappings,
FDs and PID retirement; it does not kill accepted writes or commit session exit
early. The requested poll timeout is10ms, not a hard universal cancellation
bound: preparation, storage-gated fallback, same-process exclusion and reaping
retain their existing admission rules. Persistent idle owners use event waits.

## Narrow pipe readiness

The normal SYS_POLL request array is copied before classification. Under IRQ
exclusion, only simple nonblocking one-way PIPE_READ/POLLIN and PIPE_WRITE/
POLLOUT endpoints use metadata-only observation. Generation, handle, endpoint
type/direction and `process_file_pipe_io_safe` are revalidated before every
observation and the scheduler's atomic check/enrollment. Queued read rights,
missing write readers, generation exhaustion, descriptor replacement, mode
changes or unsupported/mixed requests restore the original gated path.
The fast path neither releases descriptors nor follows a replacement family
under a foreign I/O owner. It grants no scheduler eligibility; same-process
storage exclusion remains intact. No general poll, socket or GUI wait contract
is widened.

## Measurements and regressions

The job's `finished_us` now measures response validation by the capture owner,
rather than the UI loop. History `retired_us` remains UI observation after full
resource retirement. Poll/write gaps now describe owner admissions. Keep these
caller changes explicit when comparing the old native baseline. The separate
[fresh-media proof](file-media-verification.md) also reduces verification progress
frames; a faster complete capture cannot be attributed solely to transport.

Scale>1 conversion occurs only when PREPARED can consume it. Pending preparation
no longer allocates/downsamples an image that would be discarded and recomputed.
The existing dimension/byte limits and snapshot bounds remain unchanged.

`test-capture-owner` runs actual capture-job code with real host pipes/pthreads
and a controlled protocol peer. It verifies the full Air immutable input without
UI polling, unrelated blocked work, prepared/partial/accepted cancellation,
stop/cancel while SNAPSHOTTING, buffered final before exit, join, allocation
failure and exact FD/mapping/PID retirement. Peer persistence is controlled,
not real filesystem proof; the separate complete-save/exFAT corruption fixture
covers that. The desktop scene regression checks delayed scale2 preparation.
The poll fixture covers check/enrollment, mutable requests, endpoint revocation,
HUP, signals, deadlines and fallback. Real scheduler tests and loaded guest/native
composition remain required alongside these host seams.
