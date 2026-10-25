# Application lifecycle and document-owned dialogs

This is matched desktop protocol v15. Build the kernel, compositor and all GUI
clients together. The process budget is unchanged. One application may own many
document windows and their transient dialogs through one connection dispatcher.

## Modal dialog windows

`desk_window_open_dialog(connection, child, parent, title, width, height)` queues
an acknowledged window open. The parent must be admitted in the same connection,
not closing and not itself a dialog. One child per parent and one ownership level
are supported. A sibling document or unrelated application remains usable.
`OPEN.value` combines CONNECTION and MODAL; `OPEN.generation` identifies the
parent's window ID. The compositor validates that parent against the authenticated
PID. Unknown, closed, nested, duplicate and foreign parents are refused.

Activation of a blocked parent selects its child, restores it if minimized and
raises the parent below it. Pointer, scroll and context actions cannot reach a
blocked parent. Dialog close restores its parent only if the dialog held focus;
it does not steal focus back from a different app. Parent lifetime cannot finish
before its children: the connection closes a child before sending its parent's
CLOSE and refuses parent disposal until child records are disposed.

`desk_dialog_window` is a caller-owned reusable presenter. Its `sync` function
never consumes events or blocks. Dispatch its child through the application's
single connection, draw the borrowed widget into that child's canvas and keep
storage until acknowledged retirement. Admission refusal leaves an embedded
fallback available; cancellation and allocation failure preserve the document.
Titles update with the active view; dimensions are an initial admission request.
The presenter owns no filesystem jobs. Canceled chooser reads still drain through
their document's retained worker job before that owner can be freed.

Text uses this presenter for its unsaved/replacement prompts and Open/Save picker.
Preview uses the same presenter for its image picker. Close Window/Ctrl-W on a
dialog cancels that dialog, preserving the document or displayed image. Clipboard
requests use the focused dialog's identity; other document state stays separate.

## Close Window and Quit Application

Text's Ctrl-W/File → Close Window resolves only that document. Ctrl-Q/File → Quit
Text records application-wide intent through `desk_application_lifecycle` and
resolves one document at a time. A canceled prompt, canceled Save picker, or failed
save stops the remaining quit. Already closed documents stay closed. Accepted file
operations drain before window/job disposal. Closing the last document exits the
application and retires the shared file-worker pool.

Preview currently owns one image document, so Close Window and Quit Preview both
retire that application after its image/list workers drain and are reaped.
The shared `desk_file_job_retire` helper retains an idle worker PID across
nonblocking waits and refuses to retire accepted work while it is busy. The
compositor also uses it to collect its preferences worker before exit. The shared lifecycle
interface does not require applications to remain resident without windows.

## Open-document routing

An admitted non-dialog Text/Preview window advertises OPEN_DOCUMENT service.
Only one Text window advertises for each application. A file-opening LAUNCH may
then be delivered to that existing process instead of spawning another instance.
An explicit launcher/new-app action continues to start a separate process.

Paths are absolute, at most 160 bytes including NUL, assembled into an owned
request buffer. Chunks carry a monotonic server request ID and exact offsets.
Malformed, stale and overlapping requests cannot replace an accepted intent.
Text copies the path into a new independent document record; Preview accepts it
only when its existing picker/image/file jobs are idle. A refused request can
fall back to another app instance. Acknowledgements retry transport pressure.
Acceptance means the app owns the request, not that file I/O succeeded; normal
application errors preserve existing documents/images.

The compositor bounds one outstanding request per service window. Queue pressure
can select a new instance before delivery. An unacknowledged timeout reports
uncertainty and disables that service registration rather than blindly replaying
an open that might already have succeeded. App/window death removes registration. Desktop exit declines new launches;
a refused delivery cannot reopen an app behind a pending desktop shutdown.
No generic external-app portal, filesystem permission boundary, nested modal
hierarchy or stable external ABI is promised by these APIs.

## Validation

Host connection tests cover same-owner admission, duplicate/nested/foreign
refusal, child-before-parent retirement/disposal, sequential quit cancellation,
path ownership/offset checks, stale rejection and acknowledgement retry. Kernel
GUI tests reject client-forged open-document delivery. The actual Text/Preview
and lifecycle VM fixtures check consumers and saved data; the machine record owns
physical acceptance of a specific payload.

Run the focused actual-consumer gates serially:

```sh
tools/host-env.sh python3 tools/desktop_text_windows_smoke.py
tools/host-env.sh python3 tools/desktop_preview_smoke.py
tools/host-env.sh python3 tools/desktop_lifecycle_smoke.py
tools/host-env.sh python3 tools/desktop_lifecycle_smoke.py --crash
tools/host-env.sh python3 tools/desktop_lifecycle_smoke.py --refuse-dialogs
```

The crash case kills Preview with its owned picker live, then uses healthy apps
to save exact contents. The refusal case deliberately rejects modal admission
without exhausting general memory, then verifies embedded save/cancel/quit
recovery. Each fixture compares saved bytes and returns to the process baseline;
crash-only orphans are given a bounded init-reaper interval. Raw serial output is
preserved even when compositor/client writes interleave.
