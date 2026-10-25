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
own timer deadlines. Combined GUI/descriptor waits, shared worker scheduling,
parented dialogs and dynamic window registries remain later work.

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
clipboard transfer and asynchronous file worker. A closed document remains owned
until accepted I/O completes, the window is acknowledged and its worker exits
and is reaped. Closing one document preserves its siblings. Last-window close
exits the application after those retirements finish.

Run `tools/host-env.sh python3 tools/desktop_text_windows_smoke.py` for the normal
Text path: independent documents, clipboard, undo/redo, exact persisted files,
unsaved cancel, close/reopen with a new ID, save-on-close and process cleanup.
The surface reuse regression also checks that a recycled kernel table slot can
bind to a new window after its prior surface is destroyed. The machine record
identifies tested sources; VM evidence does not accept native input latency.
File workers remain per document; shared/lazy worker scheduling is later work.
