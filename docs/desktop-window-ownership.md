# Application window and model ownership

`desk_window_set` is the allocating layer above the
[shared dispatcher](desktop-application-runtime.md) and existing
[connection/dialog protocol](desktop-application-lifecycle.md). Normal Text and
Files use it through one tool-window runtime. The widget library stays pure.
There is one connection receiver/UI thread and no hidden worker or destructor.

## Stable models and admission

Initialize the set at a stable address. `desk_window_owner_new` maps a zeroed,
16-byte-aligned model together with its stable ownership record. Embed the main
`desk_app` in that model and pass it to `desk_window_owner_open`. A local open
failure retains the allocation for explicit cleanup/release. A successful local
open queues admission; only OPEN_REPLY marks it ready or refused. The existing
memory and GUI budgets remain authoritative. No new fixed window-count limit
is introduced.

A client may reserve an exact `desk_pixels_alloc(width,height)` mapping before
starting a helper, then transfer it to a freshly opened window with
`desk_app_adopt_pixels`. The window must still be opening with no configuration,
pixels, surface, frame or pending publication. Refusal retains caller ownership;
success transfers ownership to normal window disposal. The matching first
CONFIGURE reuses those pixels but still creates its stream surface. Different
geometry follows normal replacement and releases the old mapping only after a
new canvas/surface succeeds. An early close or surface failure retains the
adopted mapping until actual disposal/process exit. This reserves memory, not a
GUI identity or a published frame; GUI/model work still follows normal admission.

Text/Files reserve their first 720×480 canvas before helper creation, alongside
the initial model. Startup failure releases both unborrowed mappings. Once open,
the window owns its adopted canvas; acknowledged Close, accepted jobs and actual
retirement remain required. Later windows and resizing keep their allocation
path. Reserving a first canvas does not authorize changing memory gates or limits.

Each owner also carries one reusable `desk_dialog_window`. Route all events
through `desk_window_set_dispatch`, which identifies the owner for main and
child events and updates main admission/retirement before the app callback.
The callback retains model-specific input, drawing, notices and job completion.
Do not combine this dispatcher with another raw/legacy GUI receiver.

Allocate/open/dispose/release models only between dispatch turns. Each callback
is bounded by the shared runtime's batch; accepted I/O and rendering run after
it. Prepare file-pool/PTY interests and deadlines on the set's runtime, then use
its single combined idle wait. Repeated app shortcuts, invalid menu actions and
stale disabled controls retain the shared command guards.

## Close, Quit and accepted work

A main CLOSE_ACK retires the GUI identity, not its model. Request application Quit
through the set lifecycle, then use `desk_window_set_next_close` to select one
ready owner at a time. Applications resolve that owner's save/cancel decisions.
Cancellation clears application Quit; an acknowledged close advances to the
next owner. Closed documents remain closed when a later decision is canceled.

First close/dispose an owned dialog, then dispose the acknowledged main window.
This releases GUI storage only. Keep polling every accepted file operation,
including mutations submitted before close, until its existing owner can detach
and its transport/child lifecycle is settled. Cancel model timers and borrowed
references, free app-specific allocations, then explicitly call
`desk_window_owner_release(owner, work_drained)`. A false drain assertion refuses
release. A failed unmap restores list ownership for retry. The runtime never
kills a worker or abandons an accepted mutation to satisfy a shutdown deadline.

Fatal connection or callback failure retains all mapped owners and refuses
further allocation/disposal/release. The app drains accepted jobs without waiting
for GUI acknowledgements that cannot arrive, shuts down its shared worker pool,
and returns failure; process exit owns uncertain GUI/model resources. This avoids
both premature frees and a cleanup loop waiting on a failed receiver.

## Text and Files

Each window retains its own content/navigation, selection, clipboard transfer,
modal state and job record. Text retains unsaved-work and sequential Quit policy.
Files New window (Ctrl-Shift-N) duplicates the authoritative current location
into another window in the same process. Close affects only that window; Quit
Files (Ctrl-Q) resolves/finishes windows sequentially. Both use one lazy two-channel
file-worker pool per process, rather than a worker per window.

Files rename/duplicate/delete prompts use the existing owned-dialog presenter,
with embedded fallback after admission refusal. Information remains a view in
its own Files window; other sibling windows stay usable. Accepted mutations must
finish before their window closes. External launcher instances and folder opens
retain their existing launch behavior; this block does not widen the protocol's
Text/Preview open-document service registration.

## Validation

`make test-window-set` exercises real allocation/release around modeled GUI
transport: forty stable models, allocation/admission refusal, dialog routing,
serialized Quit, explicit drain confirmation, failed-unmap retry and fatal
transport retention. Actual Text/Files consumer VMs separately verify native
protocol behavior, saved contents, worker retirement and process baselines.
`tools/desktop_files_windows_smoke.py` grows twenty Files windows in one process,
checks independent history and owned prompts, then closes a window during an
accepted 8 MiB copy while another Text process saves. It checks exact file bytes,
bounded workers, process-baseline restoration and a standard exFAT volume before
and after the run. The general Files mutation fixture separately checks pointer
Delete confirmation. Terminal has command-guard host coverage and real-shell VM
checks for clipboard, interruption, output, independent sessions and cleanup.
Physical acceptance remains release-specific in the machine record.
