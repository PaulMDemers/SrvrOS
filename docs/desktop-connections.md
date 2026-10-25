# Desktop connections and window lifetime

The desktop connection implementation uses matched protocol v14. It adds multiple
window identities per authenticated process. Compositor registries grow under
explicit byte budgets; refusal remains part of the creation contract. The shared header, kernel, compositor and applications must
come from the same build. This is not a stable external binary ABI.

`desk_app_open/poll/close` remain the single-window source compatibility path.
New clients include `srvros/desk_connection.h`, initialize one
`desk_connection`, and use `desk_window_open` for caller-owned `desk_app` records.
They call **only `desk_connection_poll`** to consume the process GUI queue. Do not
mix queue readers, use a polling loop per window, or move/free a registered record.
All calls belong to the application's UI thread; there are no hidden callbacks
or worker threads in this runtime.

## Admission and identity

`desk_window_open` queues a local request; its zero return is not server
admission. One creation request may be outstanding per connection. The dispatcher
retries queue pressure, receives `DESK_OPEN_REPLY` with a typed result, and then
receives/configures the new canvas. A successful `DESK_CONFIGURE` event means the
canvas and surface exist. A refused open produces a closed record that can be
disposed; it does not close sibling windows.

Window IDs increase for the process lifetime, including refused requests. A new
window never reuses an old ID, even if its application record or compositor slot
is reused. Kernel-authenticated PID and window ID form the routing identity.
Surface commits bind the surface to its window for that surface's lifetime;
release must also match its serial and generation. Old queued events cannot
become events for a new window in the same storage slot.

The server retains each creation reply independently of the live-window table.
Capacity and cache-allocation refusals therefore have a reply slot even when no
window slot is free. A conforming client waits for that reply before its next
creation request. The server rejects replayed IDs and mode changes within the
same process connection.

## Dispatch and drawing

`desk_connection_poll` returns the target `desk_app` and a packet for application
events. It handles transport releases and rotates per-window maintenance even
when the incoming queue never empties. Each window keeps its own configuration,
canvas, frame serial, input serial, menus, focus and close state. Existing
`desk_app_canvas`, `present`, `title` and menu functions act on that record.
Outbound helpers defer service traffic until admission; a second reader through
`desk_app_poll` is refused for a connection-managed window.

Bind a clipboard or notification helper's `window` field to its admitted window
ID before starting a request. Helpers filter replies by that identity. Clipboard
publication still requires the actual focused window. Notification history
retains the originating window; two sibling windows may use the same request
serial without merging their notices. The history quota remains per process.

The compositor routes input and pointer capture to the PID/window pair. Changing
focus between sibling windows also changes the global menu owner. There is no
new focus-stealing policy or transient-parent API in this block.

## Close and failure

After the application has resolved unsaved edits and drained any accepted file
job, call `desk_window_close`. It stops new frames and begins asynchronous
retirement. Keep polling. The server releases an accepted frame before sending
`DESK_CLOSE_ACK`, then retires that window. Only then may
`desk_window_dispose` destroy the surface, free canvas memory and unlink the
record. Application documents and job data are separately owned by the caller.
Closing one window does not exit its process or close its siblings.

A local window runtime failure emits the local-only `DESK_WINDOW_FAILED` event
and begins the same retirement path. Keep the document available for recovery;
continue dispatching healthy siblings and wait for the failed window's close
acknowledgement. Failed resize retains the existing canvas through the existing
resize-refusal contract.

A fatal connection error requires process shutdown: do not infer that a live
server relinquished surfaces just because a deadline elapsed. Confirmed server
death uses kernel cleanup to unseal buffers and allows disposal. Unresponsive or
protocol-faulting clients remain subject to the compositor's bounded process
termination policy. This can affect all windows in that process.

`desk_connection_wait` checks every registered window before waiting on the one
GUI queue. The caller must also exclude active jobs, service transfers and its
own timer deadlines. For worker readiness use the combined wait below;
parented dialogs remain later work. Window registries grow under explicit
metadata budgets.

## Validation

Host fixtures exercise separate frame/focus state, one reader, creation credits,
queue pressure, typed refusal, stale IDs, release-before-close, local canvas
failure, server/process death and notification/capture routing. The RAM-only
`deskd --multiwindow-smoke` consumer exercises the real protocol with independent
editable documents, menus and clipboard helpers in one process. Run it using:

```sh
tools/host-env.sh make -j2 test-desktop all
tools/host-env.sh python3 tools/desktop_multiwindow_smoke.py
```

Normal Text now uses this connection runtime. File → New Window or
Ctrl-Shift-N creates an independent document; Ctrl-N still resets the current
document after resolving unsaved edits. Each document owns its history, picker,
clipboard transfer and asynchronous file-job state. Normal Text shares a lazy
two-channel file-worker pool across its documents. A closed document remains owned
until accepted I/O completes and the window is acknowledged, then detaches its
job. Idle transports remain available to siblings. Last-window close retires the
pool and reaps its children before the application exits. Files uses the same
service with its single job; diagnostic captures retain dedicated workers.

Run `tools/host-env.sh python3 tools/desktop_text_windows_smoke.py` for the normal
Text path: independent documents, clipboard, undo/redo, exact persisted files,
unsaved cancel, close/reopen with a new ID, save-on-close and process cleanup.
The surface reuse regression also checks that a recycled kernel table slot can
bind to a new window after its prior surface is destroyed. The machine record
identifies tested sources; VM evidence does not accept native input latency.

## Shared file-worker service

`srvros/desk_file_job.h` supplies `desk_file_pool` with caller-owned channels
and jobs. Initialize a pool with an explicit concurrency budget (1–32 channels),
then bind zero-initialized, stable job records. Binding creates no process. The
normal Text/Files policy uses two channels; that bounds workers, not documents.
All pool/job calls belong to one dispatcher thread. A job owns its request and
response buffers, so another document cannot overwrite its retained result.

`desk_file_job_start` snapshots the path/payload and queues one request for that
job. Successful return means local acceptance, not a completed filesystem write.
The FIFO head acquires an idle transport when submit/poll next runs; polling a
later job cannot jump the queue. Progress remains bounded and nonblocking. A
pool-wide request ID never repeats; exhaustion refuses a new request. Response
magic, ID, size, operation-specific shape and checksum are validated before
completion. An idle channel is returned only after the full response drains.

Transport/spawn failures return through the requesting job. A poisoned channel
closes its pipes but retains its child PID and concurrency charge until reaped;
no uncertain mutation is retried automatically. A later explicit operation may
retry using a healthy channel. Other queued jobs retain their snapshots. One
unavailable channel cannot consume more than its original concurrency slot.

`desk_file_job_detach` refuses a busy job, including one still queued. Keep its
owner and poll until completion/failure even if its GUI connection has gone.
There is no cancellation of accepted mutations in this contract. Detach idle
jobs before releasing their records. `desk_file_pool_shutdown` refuses bound
jobs; once unbound, it closes idle transports, polls child exit and returns one
only when every worker has been reaped. It never kills a worker. The legacy
`desk_file_job_init` still creates a dedicated transport for existing consumers.

The host fixture covers forty queued jobs, two leases, immutable snapshots,
response isolation, FIFO/reuse, busy detach/shutdown refusal, transport factory
failures, poisoned-child retention, explicit retry and request-ID exhaustion.
`tools/desktop_file_pool_smoke.py` exercises forty queued 64 KiB writes/readbacks
through actual worker processes and checks the disposable image offline.
`tools/desktop_capacity_smoke.py --document-growth` exercises 32 real Text
documents with exact persistence and shared worker retirement. See the machine
record for validated revisions; these fixtures do not establish native latency.

## Waiting for GUI and worker readiness

Fill a `srv_gui_wait` request with an absolute `srv_monotonic_us()` deadline and
explicit nonblocking pipe/PTY interests, then call `desk_connection_wait_io`
(or `desk_app_wait_io` for a legacy single window). The helper supplies the
version/server/cancellation token and returns ready flags or a negative error.
It rejects unfinished window publication just like the legacy GUI-only wait.
Up to eight interests are accepted; general files, sockets, blocking endpoints
and unsupported event masks are rejected. Only `POLLIN`/`POLLOUT` are requested;
`revents` can additionally report hangup/error/invalidated binding.

Deadline zero or a past deadline is a non-consuming probe; future deadlines are
limited to one second. The result can combine `SRV_GUI_READY`,
`SRV_GUI_FD_READY` and `SRV_GUI_CANCELED`. `SRV_GUI_DISCONNECTED` reports peer
loss; `SRV_GUI_DEADLINE` means no other requested condition was ready. Readiness
is advisory: dispatch a bounded batch and tolerate another thread changing an
endpoint afterward. Close/dup2 during the wait reports `POLLNVAL`, even if that
FD number is reused before the waiter runs. Invalid input before capture is an
error instead. Signal interruption returns `SRV_ERR_INTR`.

`gui_cancel_wait()` requires an established client receive queue, increments a
process-local epoch and wakes combined waiters. Legacy `gui_wait` does not
observe this epoch. The
connection helper adopts the observed epoch on a successful result. It must not
be called concurrently by multiple GUI dispatchers. Another thread may request
cancellation, but that only interrupts the wait: keep accepted jobs alive and
drain them before detaching. At epoch exhaustion cancellation remains sticky.

`desk_file_pool_interests` sets the request's count/interests without I/O. One
means the pool can wait; zero means queued dispatch, buffered completion or
child retirement needs a short retry; negative means invalid/capacity. Always
poll jobs and GUI before rebuilding the interests. Keep clipboard transfer,
unsaved-close, title/menu publication and other timer policy in the owning loop.
Normal Text/Files currently use a 100 ms maximum wait and a one-tick fallback.
During owned storage I/O the kernel still returns GUI/cancel/invalidation but
defers ordinary pipe readiness, rechecking at most one tick later.

The file-job start API accepts a previous request path or retained input/output
slice: it snapshots the path and uses overlap-safe payload copying before
resetting headers. Accepted requests stay immutable while busy.

`tools/desktop_multiwindow_smoke.py` starts with `guifail --wait`, exercising
actual threads, pipe readiness, cancellation, FD replacement and server exit.
The multiwindow completion collector requires successful child exits and exact saved
console records; live UART writes can interleave and are retained separately.
`tools/desktop_wait_load_smoke.py` injects 16 idle and 32 active-write keystrokes
while forty 64 KiB file jobs run through two channels on a disposable, throttled
QEMU disk. It checks acknowledged input, worker/process retirement, exact offline
files, fsck and allocation consistency. Reported kernel-key-queue-to-dispatch
latency percentiles are histogram bounds in that VM, not physical arrival,
scanout latency or an energy measurement.

## Shared file selection (D3c)

Text and Preview use a caller-owned [chooser controller](desktop-chooser.md)
above the pure file-picker widget. It correlates asynchronous LIST completions,
retains canceled work until drained and returns a copied path/mode without
starting document/image I/O. It supports pooled and legacy jobs, keeps one UI
dispatcher and does not introduce parented windows or a nested modal loop.
