# Desktop capacity and buffer ownership

The desktop supports sixteen client windows, with demand-allocated compositor
caches and current-size client canvases. Kernel GUI capacity is 32 queues and
64 surfaces, derived from the shared sixteen-window policy; 64 process slots
remain unchanged. File workers and terminal shells consume additional processes,
so window capacity is a ceiling, not an unconditional launch guarantee.

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
and sixteen windows. Match kernel and all desktop binaries for v12; earlier
protocol images are incompatible rather than silently negotiated.

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
