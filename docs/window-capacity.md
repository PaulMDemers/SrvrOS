# Desktop capacity and buffer ownership

The desktop grows window and child-connection registries on demand instead of
refusing the seventeenth window. Stable records and reusable scene/UI scratch
storage share an 8 MiB additional-metadata budget. The initial sixteen inline
records are compatibility/bootstrap storage, not a production admission ceiling.
Compositor caches and client canvases follow current window sizes. Kernel GUI receive queues are now
demand-allocated under a 2 MiB requested-byte budget, with 64 messages per FIFO;
surface records are demand-allocated under separate 256 KiB metadata and
256 MiB pixel budgets. These include prepared and retired storage until freed.
The former 64-surface table is gone; 64 process slots remain unchanged. File workers and terminal shells consume additional processes,
so window capacity is a ceiling, not an unconditional launch guarantee.

The current admission budgets are named together in
`shared/include/srvros/desktop_protocol.h`: windows, kernel queues and queue
depth, kernel surface metadata/pixel bytes, ordered input queue depth and
compositor cache bytes.
Clipboard transfer state belongs to each admitted client; there is no separate
eight-peer table. Client retirement releases a staged write and pending replies
before reusing the record. One blocked reader keeps its own retry state and does
not hold other readers. The global clipboard remains a single 64 KiB committed
value with one staged writer; concurrent revision changes cancel partial reads.

`--apps-trace` logs separate live client, child and clipboard-owner counts,
cache bytes/budget and transport limits on admission, retirement and refusal.
Launch capacity produces a visible close-a-window notice; process-launch refusal
and pixel-cache pressure retain their distinct diagnostics. These are the current
admission budgets; each pool can independently refuse a request. A surface byte budget does not guarantee that physical
memory is available.

The compositor pixel-cache budget is 96 MiB, including published and private
buffers. It excludes the desktop canvas, client memory, kernel surfaces, page
rounding and other OS work. Empty slots allocate no pixel caches. A spare with
enough capacity can be reused; larger obsolete spares are trimmed after a new
frame is published. Unreserved spares may be reclaimed under pressure; a buffer
promised to an outstanding CONFIGURE is retained. A replacement allocation must
fit the budget before the previous spare is released. Closing a window frees
both caches. A failed allocation keeps the published frame and input owner.
If a redraw needs a reclaimed spare and allocation fails, RELEASE_RETRY releases
the kernel seal without publishing or acknowledging input. The client retains
its document/canvas and requests another frame after five ticks (50 ms), while
continuing to process input. Invalid surface copies still retire the client.

Resize reserves a private destination before changing geometry/generation.
The client allocates a new current-size canvas and kernel surface before
releasing either old allocation. Protocol v12 adds RESIZE_REFUSED with the
failed generation and retained canvas dimensions. A matching refusal restores
the last committed window geometry using a fresh generation. Returning to the
client's existing size reuses its old surface/canvas without allocation. Refusal
packets retry queue pressure; stale requests/refusals are ignored. An initial
window that cannot allocate its first frame still fails to open. Copy/protocol
errors retain their existing bounded retirement policy.

Menu and palette widgets keep their inline defaults (96 menu entries/32 rows
per group and 64 palette entries) but accept larger caller-owned storage. The
compositor grows their backing/scratch arrays with its registry, so Window lists
all live clients and the launcher searches every admitted window. Wheel scrolling
and keyboard selection reveal entries beyond the viewport. The shared list model
retains its 4096-item geometry bound; the compositor metadata budget is reached
before that bound. Application menu wire snapshots remain separately limited. Matched protocol v15 (168 bytes) supports explicit window
IDs and an opt-in connection dispatcher with acknowledged creation and close.
The ID1 single-window source wrapper remains available. Kernel stream surfaces
bind to their first committed window for their lifetime; release also matches
serial/generation. Input, capture, menus, clipboard and notifications follow
window identity. See [desktop connections](desktop-connections.md) for ownership,
refusal and disposal. Match kernel and all desktop binaries; earlier protocol
images are incompatible.

Host regressions cover transaction failure, published-pixel preservation,
reserved-spare protection, budget exhaustion/reclamation, stale refusal,
rollback without allocation, close/reopen and scrolling menus. The updated
`tools/desktop_capacity_smoke.py` now uses an explicit one-byte registry-growth
diagnostic budget to retain deterministic sixteen-app/seventeenth-refusal coverage,
slot reuse, exact saved text, screenshot work at capacity and process cleanup.
Arrange and compositor fixtures cover repeated generations, maximize/restore,
sealed surfaces and input. The sixteen-window persistence/cleanup VM, arrange fixtures at 800x600 and
1440x900 and compositor lifecycle fixture at 640x480 pass at `b96e990`,
including the deferred redraw safeguard. The
sixteen-window run recorded 49564560 peak cache bytes (about 47.3 MiB) and zero
bytes after retirement, versus the prior roughly 79 MiB eager reservation for
eight slots. This is compositor pixel memory only, not whole-system usage.
Air7,2 release `bf68e4e9b88b8ae7` opened sixteen windows successfully on
2026-10-24. Matching logs register all sixteen slots, preserve the saved document
and exit cleanly; prior files, offline allocation and Ubuntu recovery checks
pass. That non-trace session did not record cache totals or a screenshot, and
this round did not independently accept injected pressure failures or resizing.
Those retain host/VM evidence; see the reference machine record.

The additional `tools/desktop_capacity_smoke.py --clipboard` mode exercises
sixteen mixed app processes through their actual kernel/compositor clipboard path,
then refusal and slot reuse. It verifies saved cross-client contents byte-for-byte,
a screenshot and complete process/cache retirement. The host clipboard fixture
exercises 32 independently owned transfer states, full 64 KiB simultaneous reads,
a blocked reader, focus refusal, writer timeout and reuse. See the machine record
for which revisions have passed these checks; no new native acceptance is implied.

The first all-Text capacity probe exposed a separate worker-resource refusal:
the thirteenth Text process could copy/paste but its file worker was unavailable.
The document remained intact and a visible message explained the refusal. The
mixed clipboard workload uses one Text and fifteen Calculator processes; its
success alone does not establish sixteen simultaneous file workers. This reinforces
the distinction between window admission and auxiliary process/thread budgets.

D1a identity foundation runtime `d9a678b` passes full desktop host/build gates,
existing apps and persistence, sixteen clipboard clients with slot reuse, and
compositor lifecycle/resize QEMU regressions. These are VM results; that foundation
admitted only ID1 per process, before the connection dispatcher below.

Protocol v14 adds the opt-in [connection dispatcher](desktop-connections.md),
acknowledged admission/close and multiple windows per process. Runtime `e5ad6c2`
passes host/build, real two-document transport, app and capacity/lifecycle VM
regressions. Normal Text runtime `f4f4c9c` now passes sixteen independent
documents in one process, all with file workers: seventeen exact saved files
including post-refusal edits and a replacement document, correlated seventeenth
refusal, a fresh ID after close and complete worker/process retirement. This
is a different resource shape from sixteen Text processes. Large document
records use independently released anonymous mappings rather than exhausting
the legacy brk arena. The launcher selects sibling windows by non-reused
compositor identities, not PID alone. Dynamic registries follow in D2c below.

D2a replaces kernel surface slots with stable allocated records and monotonic
positive IDs. Metadata and pixel charges are reserved before preparation;
allocation failure rolls both back. Cleanup detaches records before freeing
outside the GUI metadata section. A retirement epoch rejects publication if any
process retired during preparation, conservatively including unrelated owners.
Such a failed initial allocation may be retried by opening again; existing
surfaces are unaffected. This structural change does not claim reduced total
syscall interrupt exclusion or native latency. The actual-protocol connection
consumer holds 128 tiny auxiliary surfaces while exercising its windows, then
retires them and relaunches. Those surfaces are not 128 desktop windows.

Surface budgets account for requested record/pixel payload bytes, not heap
headers or page rounding. The surface-create syscall also validates its writable
result buffer before allocation and destroys an unpublished result if copying
it back fails. The ABI probe rejects 300 read-only result buffers without
consuming an ID, then allocates the normal pool and renders windows. An earlier
probe reproduced memory exhaustion from unreachable surfaces before this fix.

Runtime `028ded1` passes the full desktop host/image build, the 128-surface
actual-ABI consumer with malformed result buffers, and sixteen normal Text
documents with exact file readback, refusal/reuse and worker cleanup. This accepts
the dynamic surface foundation in QEMU, not dynamic compositor window admission
or native latency. D2c adds dynamic compositor admission below.

D2b removes the fixed process-queue table. Queue payload charges include pending
candidates and detached records until actually freed; allocator overhead remains
outside the accounting. Preparation happens outside GUI-owned critical sections.
Before publication, send revalidates the server identity, retirement epoch,
packet direction and surface owner/window/serial state. An intervening retirement
conservatively refuses the operation; caller retry remains responsible for
pressure. Competing queue creation uses the already-published FIFO and releases
the unused reservation. Queue insertion and surface sealing/release remain one
transaction. Invalid packets and readiness queries do not allocate. Registration
cannot steal a live server or resurrect a retired owner.

Host coverage forces quota exhaustion beyond 32 connections, allocation failure,
retirement during preparation, competing publication/full FIFO, reentrant release
and server replacement. The `guifail --queues` actual-ABI workload creates 24
clients plus one server simultaneously, authenticates both directions, retires
and reaps all children, and repeats three times before a normal multiwindow
compositor trial. These are receive queues, not 24 desktop windows.

The first 40-client VM fixture hit the independent scheduler task budget at
fork index 26 (`EAGAIN`), before queue pressure. Its failed evidence is retained.
The production scheduler budget is unchanged; the positive VM workload uses 24
clients, while host fixtures independently exhaust the GUI queue byte budget.

D2c allocates stable window/child record chunks and replaces only pointer and
scratch tables during growth. Clipboard writer pointers remain valid. Child
connections grow independently from windows. Metadata reservation includes the
old and replacement tables until publication/free, but excludes inline records,
allocator/page overhead and all pixel caches. Chunks retain the session's high
water mark; closed slots are reusable and pixel caches retire immediately. All
registry mappings retire at compositor shutdown. This is bounded by available
memory, not a claim of infinite windows or removal of process/task limits.

Window menu actions, launcher selection, overlay return and drag/resize use
non-reused lifetime identity. Global menu action IDs are 32-bit; the session
refuses new identities before that namespace wraps. Scene sorting uses reusable
scratch storage rather than a window-count-sized stack allocation. Preparation
failure leaves the old registry, windows and widget snapshots intact.

`--registry-pressure-test` is a diagnostic with only one additional metadata
byte, leaving the inline records usable while forcing growth refusal. Ordinary
startup uses the 8 MiB budget. `--window-growth-smoke` runs the separate RAM-only
128-window consumer in one process; these are stress points, not new maxima.
Host tests exercise 128 stable records, 200 menu/search entries, allocation and
quota failure, independent child growth, stale identity and full reclamation.
QEMU acceptance for this candidate is tracked in the machine record; native
large-window/input acceptance is pending.

Runtime `bba1c39` passes the actual 128-window consumer, complete launcher/menu
selection, first/middle/last input, resize/replacement and 129 acknowledged
retirements. Requested registry peak is 2,020,656 bytes and pixel-cache peak
38,088,904 bytes; both retire completely. The final pressure-document,
clipboard, normal Text, existing apps and compositor regressions pass too.
These functional VM checks do not measure native input latency or total memory.

D3a separates file-job ownership from worker processes. Normal Text documents
share a lazy two-channel pool, so opening an empty document does not consume a
scheduler slot. Jobs retain their own request/result buffers; transport failure
retains the poisoned child until reaped, and last-document close drains accepted
work before pool shutdown. Files uses the same service. Existing diagnostic
captures may still use dedicated workers; independent app/process limits remain.
Candidate `fc30d06` passes 32 same-process Text documents, 33 exact files with
replacement/reuse and one shared worker reaped. A separate actual pipe/process
fixture queues forty 64 KiB writes and readbacks through two workers, then checks
exact contents, standard exFAT metadata and independent allocation traversal.
These do not accept native input latency or higher window counts on hardware.


## Scheduler storage after the mixed-app native trial

The combined Air7,2 session exposed an independent 32-thread scheduler ceiling
at fifteen app windows. Thread records now retain an inline 32-slot bootstrap
chunk and grow in stable, page-backed chunks of 32. Existing context, FPU and
stack addresses never move. CPU0 IRQ exclusion covers allocation and publication,
as it already did for kernel stacks; failed stack/record allocation publishes no
slot, identity or chunk and releases any candidate stack.

Admission charges a 32 MiB budget for 128 KiB scheduler kernel stacks plus
page-rounded dynamic record chunks. The inline bootstrap records, pointer table
and bounded diagnostic scratch are static overhead outside that charge. The
budget-derived directory can address at most 256 slots; metadata charges or
physical memory pressure can refuse earlier. This is a memory admission policy,
not unlimited threads or processes. Process-table, per-process thread, file,
client-surface and compositor budgets remain independent.

Dead stacks stay charged until a later spawn reuses their slot. Record chunks
retain their high-water storage until reboot; there is no IRQ-time compaction.
Quarantined slots/stacks remain charged and cannot be reused. Kernel identities
remain monotonic and are not recycled with slots.

Wait queues hold a waiter count; each stable thread record owns its queue
membership. Wake, timeout, user-thread cancellation and quarantine detach that
membership exactly once. Slots above 63 do not shift a bitmap or alias low slots,
and a stale queue cannot wake a reused slot belonging to a different queue.
Selection/expiry/wake scans remain bounded by resident capacity, and empty wake
queues return without scanning. Scheduling policy and storage I/O eligibility are
unchanged. Diagnostics use serialized static scratch, avoiding large arrays on
small kernel entry stacks. Worker snapshot version 2 permits slots 0..255;
collectors continue to accept version 1 with its original 32-slot bounds.

Desktop launch refusal emits `deskd: launch-failed` with app/error identity and a
rate-limited visible warning. The UI distinguishes resource pressure from other
launch errors without guessing whether a process slot, scheduler stack or memory
allocation failed. No refused child is installed in the compositor registry.

The combined-services regression is
`tools/host-env.sh python3 tools/battery_home_desktop_smoke.py --boot-session
--services --audio --mixed-apps`. It opens 24 separate app processes including
file/image workers, saves exact document contents after growth, retires the whole
lifetime, reopens and checks disposable filesystem allocation. It complements the
single-process 128-window and 32-document fixtures; it does not replace native
acceptance. The same test against old staged release `2750e5efa04573ba` reproduced
scheduler-table exhaustion after fifteen windows.

The mixed-process workload uses 2 GiB guest RAM, matching the exact-stage
reproduction. A 512 MiB run of the fixed scheduler passed the old fifteen-window
boundary but exhausted physical memory before the final Text process could
open; the desktop retained the session and displayed its resource warning.
This is an independent memory limit, not evidence of unlimited app admission.
Ordinary service fixtures retain their 512 MiB allocation.


## Unattended native mixed-app capture

The explicit `--desktop-capacity-test` staging option requires `--desktop-volume`,
`--desktop-battery` and `--desktop-services` on the captured Air7,2 profile (v73).
Ordinary desktop startup does not select it. Each of two complete battery/Home
lifetimes launches 24 independent Text, Files, Calculator, Clock, Help, Preview
and Sketch processes, waiting for each rendered admission before the next.
It holds all windows, saves a screenshot, requests normal close and requires
24 successful child reaps. The next lifetime repeats the workload to check reuse.

Each compositor round has a 180-second deadline; its kernel lifetime owner has
a separate 240-second bound before revocation/drain. Admission failure, unexpected
child exit, failed screenshot or incomplete retirement is not a pass. Existing
logger and battery shutdown contracts persist the per-round result in `exit.log`;
then the boot owner unmounts and returns to Ubuntu on handled success or failure.
Unsafe retained ownership or failed unmount still cannot reset through mounted
storage. Keep AC and the selected USB drive connected; no input is required.

Rehearse with `tools/battery_home_desktop_smoke.py --boot-session --services
--audio --capacity-auto`; add `--stage RELEASE --case profile-refusal` for the
exact native payload. Q35's native-profile refusal is expected, so it verifies
capacity/save/retirement/return, not physical battery activation. The disposable
`--capacity-low-memory` fixture uses 384 MiB to require launch refusal and a
saved failure followed by automatic return. No physical typing, pointer feel or
manual document editing is accepted by this workload.

Air7,2 native release `c9e59c1b11d5c239` / source `7fc5077` passed this workload
on October 25: both 24-app rounds rendered and saved screenshots, all children
exited normally, both service lifetimes retired and offline filesystem/allocation
checks passed. This is measured acceptance at 24 independent app windows with
combined services, not an unlimited-capacity claim. See the
[unit record](machines/macbookair7-2.md) for evidence and remaining manual checks.

The current v73 workload also uses the final Text process's ordinary connection
runtime and pooled file worker for 60 acknowledged editing/clipboard inputs.
Five checkpoints each save, stat, rename, clear RAM text and reread exact `AA`
bytes in a new `capacity-*` directory under Home. The shared chooser transport
checks submitted-list cancellation, drain, reopen and stale-response rejection
against that directory. Each round requires a normal Text retirement receipt,
all five exact files and its screenshot. Guest progress is visible above the
dock; no chat instructions or human input are required. Older v73 releases that
only opened windows do not establish these new document checks. This fixture
does not measure physical input latency or accept manual Preview-picker visuals.

Air7,2 release `f01d4049daae0b4a` / source `044d0b4` physically passed the extended
workload on October 25: two rounds, 24 app processes each, 60 acknowledged edits,
five exact files each, both chooser transports, normal pooled Text retirement,
screenshots and complete service cleanup. Offline comparison preserved all prior
non-metadata files; filesystem/allocation checks were clean. This closes the
automated document/save/reread and shared chooser-transport acceptance at this
load; synthetic events do not establish physical input latency or manual picker
visual acceptance.
