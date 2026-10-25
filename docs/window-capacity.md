# Desktop capacity and buffer ownership

The desktop supports sixteen client windows, with demand-allocated compositor
caches and current-size client canvases. Kernel GUI capacity remains 32 queues;
surface records are now demand-allocated under separate 256 KiB metadata and
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
admission budgets; dynamic queues and compositor registries remain the next
architecture stage. A surface byte budget does not guarantee that physical
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

Menus allow 96 items and up to 32 rows per group. The Window menu lists all live
clients; wheel scrolling and automatic keyboard selection reveal entries that
exceed the viewport. The launcher has 64 entries, enough for twelve app entries
and sixteen windows. Matched protocol v14 (168 bytes) supports explicit window
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
`tools/desktop_capacity_smoke.py` targets sixteen apps, seventeenth refusal,
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
compositor identities, not PID alone. Dynamic registries remain next.

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
or native latency. Queues and the sixteen-window compositor still follow their
separate current limits.
