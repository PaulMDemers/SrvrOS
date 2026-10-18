# Native desktop widget library

The new desktop's shared controls live here. Files, Text and subsequent native
apps should use and extend this library rather than maintain private controls.
The public C API is `include/srvros/desk_widgets.h`; the build produces
`build/userspace/widgets/libdeskwidgets.a`. It is independent of the legacy
`ui.h` / `gui2.h` implementations.

## Boundaries

- **Renderer** (`desk_render.h`): pixels, glyphs, rectangles and damage.
- **Widget library**: theme, control geometry, interaction state and drawing.
  State belongs to each client instance. No allocation, syscalls, IPC, file
  access, logging, hidden event loop or application callbacks occur here.
- **Client/application**: layout, event routing, focus ownership, document model,
  application actions, file work, dirty tracking and surface commits.
- **Compositor**: window management, cross-window focus/capture, input transport
  and presentation. It uses the shared menu control for global UI, but does not
  own application documents.

Control drawing intersects the incoming canvas clip and preserves it. This
supports parent clipping and damage redraw without controls repainting outside
their assigned area. Coordinates and rectangles use the renderer's pixel units
and half-open bounds. Applications supply explicit frames; no implicit layout
tree or dynamic object graph is introduced yet.

## Initial controls

- Immutable `desk_theme_quiet` with shared warm surfaces, restrained color,
  fonts and spacing. Pass an application-owned copy for a different theme.
- Clipped labels and rectangle containment/inset helpers.
- Vertical scrollbar with independent content/viewport extents, bounded wheel
  deltas, thumb grab offset, page clicks and captured drag outside the track.
  Release applies its final coordinate. Cancellation, geometry changes and
  non-scrollable content disarm it. Empty/tiny frames are valid no-op layouts.
- Button with press feedback, release-inside activation, cancellation, disabled
  state, focus indication and an abstract keyboard activation event. A press
  beginning outside never activates it. Disable, resize and focus loss disarm it.

The existing `/fat/bin/deskclient` study now uses the shared scrollbar and a
Text size button. Ctrl-L remains its keyboard shortcut. The content, typing
strip and document state are still study code; this is not yet a text editor.
The app adapts `desk_packet` to toolkit events, so control code has no dependency
on the transport packet or protocol version.

## Ownership and event contract

Zero-initialize state once, then call `*_configure` on layout/enable changes.
Do not mutate the state fields directly. Configuring a changed frame cancels
capture; the application must mark affected areas dirty after configuration.
`*_event` returns a bitmask: REDRAW requests visual invalidation, CHANGED reports
a value change, and ACTIVATED reports an action to perform in application code.

For example, a client with a `desk_button save` routes an event as follows:

```c
unsigned result = desk_button_event(&save, event);
if (result & DESK_WIDGET_REDRAW) dirty = 1;
if (result & DESK_WIDGET_ACTIVATED) request_save(document);
```

`request_save` is application behavior, outside the toolkit. File work must not
block the application's input/render path as that workflow is developed.

Route pointer events to the relevant control, retaining its capture through
release outside the frame. Send CANCEL when the compositor cancels capture;
send FOCUS_OUT when a control loses keyboard focus. Focus and pointer capture
are separate. A keyboard router sends ACTIVATE only for the focused control,
after interpreting the app's key bindings. The button itself does not infer a
key from console bytes or steal input from another control.

The legacy study retains its original bindings. The application desktop now
uses structured key/modifier events, explicit focus notifications and Alt-Tab
for window switching. Within-window Tab/Shift-Tab traversal, editing selection
and shared clipboard actions are handled by the router and client runtime
described below; widgets remain independent of the IPC transport.

## Build and verification

```sh
tools/host-env.sh make widget-library
make test-widgets test-desktop test-desktop-io
tools/host-env.sh make -j2 all
tools/host-env.sh python3 tools/desktop_compositor_smoke.py --resolution 1280x800
```

Consumers add `-I userspace/widgets/include` and link `libdeskwidgets.a` before
the existing renderer/runtime objects or library. The `DESKCLIENT_OBJ` and
`USER_DESKCLIENT` rules in the root Makefile provide the first integration.
The public API is an evolving source API, not a promised stable binary ABI.

Host tests exercise capture, page clicks, outside release, cancellation,
resize/content changes, disabled controls, focus, abstract keyboard activation,
integer extremes, tiny frames and nested clipping. Full-frame rendering must
match tiled rendering, including stride/guard preservation. QEMU exercises the
library button and scrollbar through actual client/compositor IPC alongside
the existing focus, move, resize, close and lifecycle regression.

Initial verification on 2026-10-18: full OS build, widget/desktop/I/O host suites,
widget address/undefined-behavior sanitizers and the 1280×800 QEMU compositor
regression passed. The rendered study was visually checked. Archive symbol
inspection showed only renderer/font dependencies, with no runtime I/O or
allocation dependencies. Evidence is retained at `build/review/widgets-20261018/`.
This extraction has not been deployed to the laptop; the accepted hardware boot
and its independent backup remain available.

## Growth toward Files and Text

Add controls only as the first useful workflows require them: layout rows and
columns, focus traversal, a single-line text field, selectable file list, editor
view, dialogs and menus. Shared selection/caret/scroll behavior belongs here;
directory enumeration, document storage, save policy and unsaved-change decisions
belong to the applications. Keep the editor's document model separate from its
view so storage and large-document behavior can be tested without rendering.

Preserve the accepted desktop/I/O checkpoint while replacing the study content
incrementally. Toolkit changes need focused host and QEMU checks; request a new
physical trial when a meaningful workflow or input behavior is ready to assess.

## Editor, lists and focus (2026-10-18)

The toolkit now also exports `desk_text.h`, `desk_edit.h`, `desk_list.h` and
`desk_ui.h`. `desk_text_buffer` uses two caller-owned buffers, validates UTF-8,
tracks caret/anchor byte boundaries and offers one-level undo/redo. After undo,
use `text.data`, since the buffers swap roles. Editing is codepoint-based;
grapheme clusters, shaping, composition and multiple undo levels remain future
work. The view supports selection, keyboard navigation, word movement, clipping,
caret reveal and scrolling, independently of storage. The app chooses capacity;
Text currently allows 64 KiB.

`desk_list` borrows item strings and metadata from its caller, and provides
selection, keyboard navigation, double-click activation and a shared scrollbar.
`desk_ui` routes Tab/Shift-Tab, focus, primary-pointer capture and normalized
keyboard events across stable control IDs. Register controls once, configure
frames/enabled state during layout, then call `desk_ui_layout_changed`.
Window focus loss, cancellation and layout changes cancel pointer capture.

The common `desk_app.h` client runtime owns surface/IPC lifecycle. It is separate
from this library. Files and Text use a persistent `deskfs` worker with bounded,
nonblocking request/response polling; widgets never perform file operations.

Lists also support timed ASCII name-prefix selection (using key-event timestamps)
and repeated-letter cycling. The shared clipboard transport lives in the client
runtime (`desk_clipboard.h`), outside the toolkit; editor actions request it and
the app rechecks selection/revision before applying asynchronous results.

## Terminal screen and view

`desk_terminal.h` is a bounded screen model. Callers supply scrollback rows and
an alternate-screen buffer. It parses arbitrarily fragmented UTF-8 and a useful
ANSI/VT subset: cursor movement, erase/insert/delete, margins, scrolling,
16/256/RGB colors, basic rendition, alternate screen, cursor visibility,
application cursor mode and bracketed-paste mode. Responses to cursor/status
queries are queued for the application to send. OSC/DCS strings are consumed
without clipboard, filesystem, title or other side effects. Sequence meanings
are checked against the [xterm control-sequence reference](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html).
This is a subset, not a complete xterm or VT100 implementation.

`desk_terminal_view.h` supplies monospace drawing, clipped colored cells,
selection/copy, the shared scrollbar, output-following/scrollback anchoring,
pointer capture and keyboard-to-terminal encoding. Applications own PTYs,
clipboard transfers, paste policy, command lifetime and input/output queues.
The Terminal app demonstrates this separation. It uses bounded batches on both
sides of a nonblocking master descriptor and retains a completed session for
inspection until its window is closed.

The model accepts at most 160 columns and 64 visible rows. The app retains 576
rows including the visible screen; resizing preserves the cursor and retained
rows but does not reflow text. Cells hold one Unicode scalar; shaping, combining
marks and double-width glyphs remain future work. Selection is canceled on new
output or resize. Terminal key shortcuts use Ctrl-Shift-C/V for copy/paste and
Ctrl-C for the foreground command; Shift-PageUp/PageDown scroll history.


## Menus and application commands

`desk_menu.h` is an owned, bounded menu bar and popup control with release-based
pointer activation, press-drag selection, hover switching, F10/arrow/Home/End
navigation, Escape cancellation, disabled items, checkmarks and shortcut labels.
It validates UTF-8 labels and unique action IDs. Flat headings group at most
12 rows each; snapshots contain at most 64 entries and eight headings. The
compositor uses this same control for its global UI. Drawing preserves parent
clips and supports tiled damage; opening, changing or closing a popup damages
only its old/new bounds and the menu bar.

Applications publish up to 32 entries/four headings using `desk_app_menus` and
service bounded nonblocking sends with `desk_app_menu_tick`. The desktop keeps
the previous complete menu until a replacement validates. Commands include the
snapshot revision; the runtime rejects unknown, disabled, unfocused or stale
actions. The server adds its SrvrOS and Window menus and routes app actions back
to the focused process. Shared controls never execute commands or perform IPC.
`desk_edit_command` exposes selection, undo/redo and clipboard requests directly
so menus and keyboard shortcuts use the same editor behavior.

## Modal dialogs and wrapped text

`desk_dialog.h` owns a bounded title/message and up to three choices, using the
shared button/focus router. Apps choose the initial safe focus and cancel ID;
Escape activates cancellation, and Enter activates the focused choice. Blank
or outside clicks preserve keyboard focus and do not dismiss the dialog.
Focus loss and layout changes cancel captures. Applications own modal lifetime
and any Save/Replace/Close behavior. Text and Terminal now use this component.
The shared text-block helper wraps words and long UTF-8 words without allocation,
honors explicit newlines, and preserves the incoming drawing clip.
