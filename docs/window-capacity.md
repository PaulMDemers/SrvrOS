# Desktop capacity and buffer ownership

The candidate supports sixteen client windows, with demand-allocated compositor
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
sealed surfaces and input. VM and physical acceptance are pending for this
candidate; see the reference machine record before claiming native support.
