# Shared desktop application runtime

Text and Preview use the optional `userspace/desktop_runtime` archive above the
[connection and document lifecycle](desktop-application-lifecycle.md). The
[widget library](../userspace/widgets/README.md) remains pure model/controller,
layout and drawing code. The runtime allocates nothing and starts no receiver or
thread. Applications still own documents, images, accepted jobs and rendering.
This block changes no desktop protocol, kernel wait ABI or hardware gates.

## One dispatcher, bounded turns

Initialize `desk_runtime` with an existing `desk_connection`, then call
`desk_runtime_dispatch(runtime, event, context)` once per application turn.
The event callback receives the authenticated target window and a copied packet.
It returns zero to continue, one to stop the batch, or negative on app failure.
The dispatcher handles at most 32 transport polls, counting internal progress as
well as delivered events. It refuses recursive dispatch. Run job completion,
state changes and rendering after that batch, even when more GUI messages remain.
Do not combine it with another connection/app/raw GUI receiver.

Transport or callback failure marks this runtime failed; it does not close a
window, free storage, kill a worker or answer a document prompt. Drain accepted
work according to the existing owner contract, retaining uncertain GUI resources
until the established process/connection cleanup can release them safely.
Text preserves the PR170 document/allocation/cleanup timing checkpoints.

## Jobs, deadlines and one wait

After dispatch and job/render work, prepare this turn's interests with
`desk_runtime_watch` or the file-pool, file-job and image-job adapters, then call
`desk_runtime_idle(runtime, busy)`. It returns native readiness flags, or negative
after a short retry; a retry alone is not a fatal app error. Preparation is non-consuming: it does not poll
a worker, read a reply, issue a file mutation or transfer its ownership. Completion
is delivered by the application's existing job poll on the next turn. Duplicate
FDs merge read/write masks. The native wait still accepts only eight explicit
nonblocking pipe/PTY interests; invalid/full sets force a short retry rather than
silently sleeping without accepted work. Queued leases, complete buffered replies
and child retirement also retain the short retry path.

The wait deadline is the earliest supplied absolute monotonic deadline, timer
or 100 ms service heartbeat. GUI and descriptor readiness wake it earlier; the
heartbeat is not a delay imposed on input. An exhausted event batch, busy app,
pending canvas/menu publication, unavailable clock or ineligible wait uses the
existing one-tick fallback. Interests and explicit deadlines reset at the start
of each dispatch, so assemble them each turn. The connection owns cancel epochs,
FD lifetime generations and surface-publication eligibility as before.

Text supplies its shared file pool once per turn. Preview supplies both its
standalone chooser transport and image-worker transport; it can now wait when
quiescent instead of unconditionally sleeping/polling each tick. Both keep
clipboard/control retries and accepted-read retirement under their existing
owners. This is not a native latency or energy measurement, nor a fix for the
separately investigated app startup/cleanup stalls under heavy disk work.

## Caller-owned one-shot timers

A zero-initialized `desk_runtime_timer` can be armed at an absolute monotonic
microsecond deadline. Equal deadlines retain registration order. Rearming in the
same runtime replaces the old registration; attempting to move an armed timer to
another runtime is refused. Canceling or clearing timers never invokes them.
Keep each timer and its callback context at a stable address while registered.

Due callbacks run after the GUI quantum, detached before invocation. They may
cancel/free themselves or other timers, or explicitly rearm a one-shot timer.
The dispatcher rereads the list after each callback and runs at most 32 callbacks
per turn, so a timer repeatedly rearming itself in the past cannot monopolize
GUI/jobs/rendering. There is no fixed registration-count limit or implicit
periodic catch-up. Callbacks must themselves be bounded and stay on the UI thread.
Cancel a timer before freeing its owner; clear remaining timers at app teardown.

## Shared command snapshots

`desk_commands.h` describes caller-owned action IDs, enabled/checked state,
labels and shortcut bindings. The same snapshot builds a menu item and guards
keyboard/button/menu invocation. Rebuild it from current app state at invocation;
a stale enabled visual cannot authorize an action after a job/modal disables it.
Unknown, disabled or duplicate action IDs do not invoke a handler. Invalid menu
metadata leaves the destination unchanged.

Primary-modifier bindings accept Ctrl or Meta, normalize either left/right
modifier, match Shift/Alt exactly and ignore Caps Lock. Released, repeated or
canceled key events do not invoke commands. This handles app-level file/window
commands; text editing, search fields and image-local navigation remain with
their shared widget controllers.

Text's New/Open/Save/Save as/New window/Close/Quit use one command definition.
Accepted file work disables conflicting mutations while an independent new
window remains available. Quit from a document or owned picker uses the same
cancellation/application intent. Preview's toolbar, menus and app shortcuts use
one definition for Open/Cancel/Fit/Actual/Zoom/Close/Edit/Quit. Model-specific
save decisions and readonly-image cancellation stay with their owners.

## Validation and acceptance

`make test-desktop` includes runtime, window ownership and consumer-command fixtures:

```sh
tools/host-env.sh make test-desktop-runtime test-window-set \
  test-tool-commands test-preview-app test-terminal-commands
```

The existing `desktop_wait_load_smoke.py` fixture also uses this runtime: its
three-second phase change is a registered timer, while dispatch and pooled-worker
readiness share the same turn/wait path under throttled writes. It preserves the
original workload, key/frame acknowledgements, worker retirement and integrity
checks. This is an actual-ABI consumer, not native timing acceptance.

Host tests cover event/internal-message floods, reentry, timer fairness and
removal during callbacks, deadline shortening, wait capacity and worker phases,
left/right primary shortcuts, repeated/canceled input and command state changes.
Actual Text/Preview, pointer, shutdown, crash and refused-dialog VM fixtures in
the lifecycle guide remain the consumer gates, including exact saved bytes,
retained edits, canceled reads, retirement and restored process baselines.
Machine records distinguish source/VM validation from new physical acceptance.

The growth fixture tracks document identities independently of owned save-picker
IDs. It waits for the picker first frame before selection and both save completion
and picker retirement before the next document shortcut. Registry capacity is
allocated storage, not the live window count. The sixteen-document pressure run
retains its explicit quota and embedded-picker refusal fallback. Both require
exact saved contents, preserved edits, bounded workers and complete retirement.

Air7,2's PR172 local validation and consumed PR174 scheduler baseline are recorded
in its [machine record](machines/macbookair7-2.md). The persistent application
lifecycle checklist includes command parity for the new runtime. Its returned
release `986d6c68fa19ba18` accepts reported concurrent app use, observed Text save
and Preview loading/close, with clean lifetime retirement and preserved prior
files. Native playback, explicit command parity and quantitative responsiveness
remain separate acceptance work; full playback/save overlap is VM evidence.

The optional allocating [window/model layer](desktop-window-ownership.md) builds
on this dispatcher; it keeps model retirement separate from GUI acknowledgements.

Terminal also uses the shared dispatcher and one combined PTY wait. Pending
output watches read readiness; queued input also watches write readiness. Cursor
blink supplies a deadline and child-status checks retain the service heartbeat.
Clipboard and unfinished publication use short retries. Close retains the existing
PTY hangup/child cleanup policy. Terminal command snapshots guard menus, toolbar
buttons and app shortcuts, including Meta aliases for Ctrl-Shift bindings.
Disabled/repeated app shortcuts never become shell bytes; raw Ctrl-C, Ctrl-D and
Ctrl-L retain their PTY meaning. A new terminal still launches its own process;
this migration does not claim multiple terminal sessions in one process.

## Compositor lifetime work under storage load

The compositor retains a child PID until it consumes a successful kernel wait.
When a read-only process snapshot reports `exited`, it may admit one immutable
reap request to the existing lifecycle worker. The worker uses a blocking wait
with a validated prelude: an exact direct child can be claimed atomically only
after reapable publication and complete runtime cleanup. The child must have
no address space, tracked mappings, kernel stack, descriptors, used thread
records or live scheduler/private-image reference. Status, child accounting and
lifetime membership transfer once under IRQ exclusion without freeing, yielding
or filesystem work. Its empty record is cleared by the existing gated allocator
before slot reuse. Refusals, POSIX/group/orphan selectors and unfinished cleanup
use the ordinary kernel I/O boundary; the snapshot itself grants no reclamation.
The UI never waits on a PID owned by that record. Interrupted results retain the
child for retry. Sixteen in-flight records provide bounded backpressure without
changing the process/window budget; full queues keep their children registered.
Shutdown drains admitted requests before the worker/session storage disappears.
Immutable `desktop-reap-state` records count queued/running/completed requests
and identify the oldest request's PID/state. Queue age is total time since
submission; run age starts immediately before the blocking wait, and completed
time is its measured duration. Only acquire-published fields are read. Automated
lifecycle failures freeze this snapshot at the deadline and print it after the
measured UI loop; a later empty queue does not erase that failure-time state.

Whole anonymous mapping retirement has a narrow independent path: the current
main thread must be alone in its process, with matching active address space and
no alias to the foreign I/O owner or private construction image. It accepts one
complete anonymous region up to2MiB with at most4096 tracked mappings and4096
scheduler records. It validates physical translations, then retires pages and
futex bindings with interrupts masked, without yielding, allocating or releasing
foreign I/O ownership. Unmap failure retains remaining frames/region ownership;
no still-mapped frame receives free credit. Anonymous provenance follows splits
and fork's existing copy. A current active helper owned by that process may also
retire a region atomically when every live sibling is safely parked on the audited
file-poll queue used by GUI/sleep waits. A scheduler park marker survives wake to
READY but clears before that context executes, including a no-switch return.
An arbitrary READY safe syscall is insufficient: it might be suspended inside a
copy. Same-process I/O/private-image owners, unsafe siblings, wrong address spaces,
quarantined contexts and unbounded scans refuse this proof. The mutation never
yields or invokes a service window. This is a CPU0 retirement proof, not permission
for parallel address-space mutation or general threaded mapping operations.
Unsupported, partial, file-backed, unproven threaded or oversized requests take the original gate and general implementation; that fallback keeps
the normal quiescent handoff. Exit, mmap and other lifetime operations remain
gated except for the separately proven startup attempts below. These finite work
bounds are not a claim of a measured wall-time bound.

Startup has two additional validation preludes, not general allocation/thread
admission. With IRQs masked, the caller must be the sole main thread in its
current active process, with matching CR3, bounded mapping/scheduler scans and
no same-process I/O/private-image owner. The anonymous-map attempt accepts only
address zero, private anonymous RW, fd -1/offset zero and at most 2 MiB. It tests
one candidate and refuses collisions instead of searching indefinitely. A region
reservation precedes page creation; checked rollback retains still-mapped frames
and region ownership on failure. The first/reused sole-main thread attempt checks
tracked executable entry and writable stack PTEs, a contained eight-byte stack
slot and TLS bounded to two pages. Both paths use audited IRQ-safe PMM/VMM work,
without service windows or console callbacks, including allocation failures.
Refusals retain ordinary admission, fresh stack validation, cancellation before
mutation, gate timing and the final fair handoff. Other threads, heap calls,
file-backed/fixed/oversized mappings and process teardown keep their gates.

The scheduler publishes a prepared user thread's process identity, address space,
TLS/FS and FPU pointer before READY eligibility. Its fixed entry goes directly to
userspace; the general context setter is unsuitable here because refreshing
kernel mappings can allocate. This constructor is not an alternative for arbitrary
kernel callbacks or a process-image loader. Normal syscall admission still gates
its later unsafe operations, and same-process I/O ownership still excludes it.
Pre-entry cancellation can find the complete thread identity. The existing pthread
creator pin continues to protect against a child running before parent publication.

Text/Files reserve the initial unopened model before starting that helper. A
startup failure releases the unborrowed model; no GUI owner or work request has
been published. Later windows keep the existing allocating/ownership path. The
combined VM validator requires complete timing records proving initial allocation
ends before helper initialization. These CPU0 proofs do not authorize parallel
address-space mutation, change process/thread budgets or establish native latency.
Text also reserves the undo-journal header and unchanged 256 KiB journal capacity
in its document mapping. Runtime setup attaches that borrowed storage before
window-open; role initialization performs no separate journal heap allocation.
The first reservation is part of the sole-main initial mapping; later windows
use the ordinary document allocation path. Standalone Text callers retain
heap-owned history and its existing single-step fallback on allocation failure.
Explicit ownership prevents freeing a pointer into the mapped model. Journal
storage retires with that model only after GUI/job owners release it. Sparse
`text-history-reserve` and `text-history-init` timestamps and scheduler brackets
separate setup from later role initialization; incomplete brackets are not a
passing zero-duration measurement.

The first standard Text/Files canvas is now also reserved before helper startup
and adopted by the opening window. This avoids an initial CONFIGURE pixel mmap
once a helper makes the process ineligible for sole-main allocation. Geometry
mismatch and later resizing retain normal allocation/admission. The optional
client surface trace reports `canvas-pixels` and `canvas-surface`; tools also
bracket first service registration, controls, draw and present. Combined VM
validation requires complete ordered reservation/adoption/first-surface records.
The earlier native role-init-to-publish gap lacked these substeps and remains
historical unproven attribution until a new physical run.

The compositor may retain up to two completed pixel-cache buffers after window
retirement. They stay reserved inside the existing96MiB cache budget, including
their worker job slots; reuse transfers ownership without another mapping. No
queued, discarded or failed allocation enters this pool. A reused buffer remains
private staging until a full surface copy succeeds, so old or partial content
is never published as a new window. Byte-budget or worker-slot pressure evicts
retained buffers through the existing worker; budget credit follows completed
unmap. Shutdown releases both live and retained buffers. Immutable
`desktop-cache` records expose reservation bytes, retained count/bytes and reuse
count without logging on the UI turn. Automatic audio/I/O trials also require
a saved atomic `desktop-cache-retired` receipt showing zero reserved bytes, zero
retained buffers and actual reuse after final cleanup; nonzero reservations
fail retirement.

Simple read/write pipe endpoint closes can complete during unrelated filesystem
I/O through an IRQ-excluded check-and-detach operation. It requires valid pipe
references and no queued descriptor rights, preserves descriptor-generation
invalidation and reader/writer/EOF notification, and performs no allocation or
filesystem cleanup. CLOSE dispatch admits only this validated prelude; all
other types, invalid references, duplex pipes and rights-bearing pipes take the
ordinary I/O gate and a fresh descriptor lookup. Rights cleanup may recursively
flush files and must not bypass that gate. Process exit retains its gate; verified wait has only the empty retired-child
claim described above.

Worker-backed client/control/reply timeouts also publish immutable termination
requests. Sixteen slots coalesce duplicate PIDs and retain full/stopped-queue
clients for retry without a direct UI signal syscall. The worker issues the
ordinary gated signal and records its elapsed wall time/result; completion
permits UI client retirement, including all windows in that process, but is not
proof of process exit. Child ownership remains governed by the verified wait
above. Requests accepted before shutdown are drained before join releases the
worker storage. This does not extend response deadlines or make a signal safe
during filesystem ownership. A failed signal result is logged; it grants no
kernel-resource reclamation.

Screenshot preparation also runs on that worker: allocate the owned pixel map,
create pipes and spawn the child, then publish the prepared job. The UI copies its
current canvas only after preparation returns and dimensions match. A prepared
job sends no request/pixel bytes before that immutable snapshot is published.
The legacy `desk_capture_job_start` still performs prepare plus snapshot together.
A changed canvas or shutdown closes/retires the owned job; it never discards
unpublished handles or cancels an accepted file mutation. Existing verified save,
completion and retirement protections remain in force. No kernel wait ABI or
I/O eligibility gate changes are part of these boundaries.

### Sparse document-release scheduler attribution

Text/Files document release emits `app-phase-sched` beside the existing
`app-timing` wall-clock receipt. Current-thread scheduler BEGIN/END surround
only `desk_window_owner_release`; both diagnostic writes occur outside that
interval. `blocked_us` and `ready_us` distinguish scheduler states, not CPU
time or a particular I/O owner's identity. Preserve `invalid` and `exclusions`;
an unavailable BEGIN/END emits `available=no` instead of fabricated zeros.
An unsuccessful BEGIN never ends a pre-existing trace. This diagnostic changes
no syscall admission, release ownership, workload intensity or deadline.

Final Text/Files teardown uses `desk_file_pool_finish` only after all jobs are
detached and application dispatch has stopped. It publishes EOF on every idle
channel before blocking for any child, using normal kernel wait admission.
Interrupted or nonmatching waits retain the PID and live accounting; completed
children are consumed exactly once across retries. No kill or timeout releases
accepted work. Interactive dispatch continues to use nonblocking pool shutdown.
The same sparse scheduler attribution covers `pool-retire`; it includes natural
child waiting and scheduling, and does not identify an I/O owner or CPU time.

Files may close its GUI window during an accepted read-only READ/LIST/STAT.
The retired model keeps its file-job binding until the response drains; only
then can it detach and release memory. Accepted mutations and Text save work
retain their completion-before-Close rule. Visual retirement alone does not
complete process teardown or satisfy an automatic lifecycle deadline.

Files publishes its initial loading view and receives the first surface RELEASE
before advancing queued file work. Lazy file-worker creation uses ordinary
kernel admission and can wait behind I/O; it must not precede the first frame.
Retired/failed windows still drain accepted work, and this ordering does not
extend client or whole-process lifecycle deadlines.

Text/Files now create one process-owned file service before opening their GUI
windows. The shared file pool has an optional nonblocking opener: a pending
transport keeps its FIFO head queued, and a successful result transfers only
PID and descriptor values into an idle channel. The helper performs blocking
pipe/fcntl/spawn construction; neither pending nor failed startup falls back to
construction on the UI turn. Existing synchronous pool users retain their API.
Transmitted requests, response validation and channel bindings remain owned
by the dispatcher until the complete reply drains. Every queued mutation also
retains that completion obligation.

A retired model can transfer its complete mapping through
`desk_window_owner_release_with` after GUI disposal and work drain. A refused
callback leaves the model and list position unchanged. Acceptance transfers
mapping ownership and counts logical window retirement; it does not prove that
physical unmapping completed. Text/Files use sixteen bounded retirement records:
full queues retain disposed models for retry, and the helper performs the real
unmap and sparse timing trace. Failed unmaps retain a nonreusable error record
and fail final retirement. No GUI/set pointers cross into the helper.

Final shutdown stops admission and drains all startup/unmap records. Reusable
services verify pthread join before releasing caller-owned storage. Text/Files
instead have one nonreentrant, static service/runtime context for their entire
process lifetime: they acquire a helper-body quiescence acknowledgement, keep
all service/stack storage pinned, reap idle file workers, and return through
normal kernel process exit. That path kills user threads before freeing process
mappings; the parent's verified process wait proves final retirement. Quiescence
alone is never described as a pthread join or permission to reuse storage.
Version3 `file-service` receipts distinguish process-owned quiescence from a
joined reusable service and expose startup, mapping, failure and pending counts.
Automatic VM checks pair these receipts with completed process waits and actual
canvas/model/service-init/service-stop phases. Existing client, UI-loop and total
lifecycle deadlines remain unchanged. Native endurance acceptance is pending.

The Text/Files helper uses an explicitly supplied64KiB stack inside its service
storage; it remains pinned through process exit (or verified join for reusable
callers). This avoids allocating
and retiring the pthread runtime's default-sized owned stack without changing
general pthread policy. Optional canvas disposal callbacks transfer pixel-map
ownership after CLOSE_ACK. Queue refusal retains canvas/connection ownership;
a successfully destroyed kernel surface is recorded and not destroyed twice.
The same helper queues canvas and model unmaps and prints their actual separate
phases. Logical GUI disposal alone does not prove either mapping was released.
Sparse app timing marks dispatcher/helper lanes and validates clocks within each
lane, retaining cross-phase lifecycle/elapsed checks and strict whole records.

Files Close can explicitly remove a queued READ/LIST/STAT only before it has a
channel or any transmitted/received bytes. FIFO links, IDs and payload ownership
remain valid; no successful response is fabricated. Mutations, partial requests,
leased channels and legacy jobs refuse cancellation. If no queued demand remains,
the helper receives an atomic construction cancellation hint and checks it between
signal/pipe/fcntl/spawn steps. It closes acquired descriptors on early cancellation.
A late successful spawn remains an owned transport and must naturally exit/reap
through EOF; it is never killed or silently forgotten. Remaining queued demand
keeps its constructor alive. Receipts account for successful, canceled and unused
reaped starts, distinguish them from failures, and justify process-owned canceled/
unused starts with explicit unsubmitted-read cancellation. Full-process lifecycle
and accepted-copy/save integrity checks remain mandatory.


Validation of the async service at `efcdb24d` includes held real-pthread
construction/unmap, constructor cancellation and late child reap, reusable join
and process-owned storage lifetime, plus twenty Files windows retaining an
accepted 8 MiB copy through Close and concurrent Text saves. A normal combined
VM passes 21 app lifetimes with full retirement receipts. This establishes the
userspace ownership path in those tests, not native endurance. Loaded VM failures
remain open: a final disk burst can outlast PCM playback, and System admission
can exceed the compositor UI bound. Keep the existing workload and timing limits;
see the [Air record](machines/macbookair7-2.md) for measured failures and candidate
status before another physical trial.


Helper initialization now has opt-in internal pthread phase observations.
Text/Files enable them only on the creating dispatcher around service startup;
worker TLS starts with the observer disabled. Attribute setup, detached-reap
identity/records, record reservation, actual kernel creation and publication have
separate whole phase receipts and scheduler attribution. Other pthread callers
have no diagnostic sink unless they explicitly supply the internal hook.

Worker startup state is embedded in the existing bounded pthread record table,
reserved and initialized before kernel creation. It follows the same creator,
claim, detach and verified-join lifetime. Cleanup captures stack ownership before
clearing a record, allowing safe slot reuse while owned-stack unmapping yields.
Main-thread storage remains separate. This removes the startup-record heap
allocation; supplied-stack policy and the existing thread limit are unchanged.
The current-thread ID lookup alone is admitted during unrelated I/O because it
reads only scheduler-owned context. Actual thread creation, thread status/join/
detach, heap growth and mapping keep their existing admission rules. Removing a
helper startup allocation does not prove that later app allocations cannot wait;
retain end-to-end readiness, UI and full-process deadlines in validation.


Final-owner retirement can overlap idle backend EOF/reaping with queued helper
unmaps once there are no GUI/model owners or pending openings and the file pool
has no bindings or FIFO jobs. Models reach that point only after their accepted
work drains. Stop requests remain separate from completion: observing stop
requires a final acquired scan, since a producer may have queued into a recycled
slot behind the preceding scan. Then helpers finish actual mapping/unused-child
ownership and publish body quiescence. Final pool finish and verified process
waits remain mandatory. A held-last-slot/reused-first-slot regression reproduces
the missing-scan failure and verifies complete retirement after the fix.


### Compositor message fairness

The compositor retains its 32-packet receive cap and checks a 2 ms elapsed wall
budget between handled packets. Remaining packets stay queued in order; normal
input, rendering and lifecycle work run before the next batch. An in-flight
handler still completes with its original ownership and is not abandoned at the
budget. This prevents accumulated service-yielding handlers from monopolizing a
turn; it is not a maximum duration guarantee for one copy or handler. Existing
UI/lifecycle acceptance deadlines and audio/filesystem workloads are unchanged.

Combined validation first checks whether saved atomic snapshots independently
contain all required process identities, events and ordered complete phases.
When they do, the separate lossy UART channel is unnecessary. Incomplete saved
coverage retains strict whole-record UART/snapshot union validation, with no byte
repair or overwritten conflicts. The receipt identifies which evidence path was
used; an early return or partial phase set is never promoted to a passing run.


The reference Air7,2 accepts the bounded 1800-second automatic workload on
`87ecd80b` (release `7d21b1fb77479259`): 937 app open/close cycles, all three
screenshots, verified live PCM/disk overlap, no underruns at 1% volume and clean
GUI/cache/service/process retirement. UI maximum is 112151 us, readiness 2220834 us
and Close 8042159 us under unchanged limits. The [machine record](machines/macbookair7-2.md)
owns scope and integrity proof. Retained phase snapshots are tails, not full
histories of every cycle; other machines, physical input, audible routing and
unrestricted concurrency/SMP remain separate evidence.


### Fixed-table wait admission and retained lifecycle latency

Process-local futex WAIT may park during a foreign I/O scope on CPU0. The audited
body reserves a fixed-table entry under IRQ exclusion, pins active waits, checks
the mapped four-byte word before every load, and receives mapping-invalidation
wakes. It allocates nothing and performs no filesystem/device callback. Timeouts,
signals and invalidation behavior are unchanged. The IDT epilogue reacquires
ordinary admission before cancellation/exit; this does not permit resource
teardown or arbitrary wait callbacks under another owner's I/O. This proof must
be reconsidered if the futex body starts allocating or invoking service callbacks,
or if userspace starts running concurrently on multiple CPUs.

Automatic lifecycle diagnostics retain the current cycle and the worst readiness
and Close observations in fixed-size UI-owned records. The existing report worker
serializes immutable copies, including launch worker begin/end, observed visual
removal, observed process exit and actual reap queue/begin/end clocks. Zero means
unobserved, not zero cost; visual removal is sampled at the compositor loop, not
an exact client acknowledgement timestamp. Complete final worst records must
match the existing aggregate maxima. This survives rotated log tails without
new filesystem writes or per-frame logging. A full-width bounded formatter keeps
these records intact. Timing/workload limits and cleanup ownership are unchanged.

### Cold compositor buffers and admission attribution

Desktop initialization seeds the existing two retained 720×480 pixel-cache slots
before launching client and storage work. These reservations count against the
same cache budget, transfer through ordinary reuse, and remain owned until the
worker completes their actual unmap. Partial reservation is usable; unsupported
sizes and allocation failure retain the existing asynchronous fallback.

Opt-in I/O observation retains the eight longest completed admission waits plus
the worst completed process-exit wait if it is absent from those eight. Records
identify waiter PID/thread/syscall, initial owner PID/thread/scope sequence, and
start/end clocks. The initial owner may change while parked; the complete wait
must not be attributed exclusively to it. Recording uses bounded storage under
IRQ exclusion and emits no text or I/O until the existing quiescent report.

Current-process signal configuration, pending-mask reads and current-thread mask
updates use bounded metadata and may execute during foreign I/O on CPU0. They do
not execute handlers or release files. A mask update exposing fatal cancellation
still regains ordinary admission in the IDT epilogue before process exit. Signal
consumption/delivery, kill and complete resource teardown remain gated. This
policy is independent of SMP acceptance.

### Bounded process liveness lookup

`SYS_PROC_LIST` retains its existing cursor enumeration. Setting
`SRV_PROC_LIST_BY_PID` selects one exact PID from the low63 bits of the argument;
a positive return identifies the snapshot, zero means missing, and negative
means invalid user output. The process-info layout is unchanged. PID zero is
not a query target. SDK `desk_process_alive` uses one such read-only metadata
call, including for exited/stopping states, instead of enumerating every process
through separate syscalls in the compositor's service loop. This adds no handle
or lifetime reference; only actual wait/reaping proves resource completion.
New clients need the matched kernel that implements this selector.
