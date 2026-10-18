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
  `desk_button_centered` optionally centers short labels, as used by Calculator.

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
tracks caret/anchor byte boundaries and offers one-level undo/redo by default.
Attach a caller-owned `desk_text_history` and payload arena for up to 256 edit
records, bounded by both record count and byte storage. Each record stores only
deleted/inserted bytes and the original selection; old records are evicted when
needed. A new edit after undo discards the redo branch. Loads clear history;
saving is outside the model and does not clear it. The arena must fit twice the
maximum document size so every valid edit remains undoable. Use
`desk_text_buffer_can_undo` for menu availability and `text.data` for content,
since the default buffers swap roles. Text allocates a 256 KiB history arena;
allocation failure retains single-step undo. Editing is codepoint-based;
grapheme clusters, shaping and composition remain future work. The view supports selection, keyboard navigation, word movement, clipping,
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
so menus and keyboard shortcuts use the same editor behavior. Content updates
use `desk_menu_replace`: an open heading remains open, an unchanged selected
item stays selected, and changed popup contents disarm pointer release. Removed
headings fall back to the first menu without selecting a command. This keeps a
background update from sending the user's next Enter into the document.

## Modal dialogs and wrapped text

`desk_dialog.h` owns a bounded title/message and up to three choices, using the
shared button/focus router. Apps choose the initial safe focus and cancel ID;
Escape activates cancellation, and Enter activates the focused choice. Blank
or outside clicks preserve keyboard focus and do not dismiss the dialog.
Focus loss and layout changes cancel captures. Applications own modal lifetime
and any Save/Replace/Close behavior. Text and Terminal now use this component.
The shared text-block helper wraps words and long UTF-8 words without allocation,
honors explicit newlines, and preserves the incoming drawing clip.

## File picker

`desk_file_picker.h` composes the shared editors, list, scroll bar and buttons
into a reusable Open/Save panel. It owns the location/filename text and borrows
validated listing items. It returns Browse, Accept and Cancel requests with a
bounded absolute path; it never performs I/O or assumes a particular filesystem.
The app marks loading and supplies a completed listing or error. Cancel stays
available while loading. Ctrl-L edits the folder, Alt-Up navigates to its parent,
F5 refreshes, Enter opens/accepts, and Escape cancels. Clipboard requests identify
the relevant editor field. Lexical validation preserves UTF-8 but filesystem
rules may further restrict names (the current exFAT writer creates ASCII names).

Text adapts this control to its asynchronous file worker. Ctrl-O opens the
picker, Ctrl-S saves (choosing a path for a new document), and Ctrl-Shift-S uses
Save As. Ctrl-L still focuses the explicit document path field. Canceling a
picker drains/discards pending listing work and keeps the document; deferred
clipboard edits cannot mutate a closed picker or its underlying document.
Save/replace confirmation and verified writes remain application behavior.

## Checkboxes and radio choices

`desk_check` shares button capture, release activation, focus, disable and
cancellation semantics. Checkboxes toggle; radio controls select themselves and
the application configures their peers as a group. Both integrate with `desk_ui`
for Tab traversal, Space/Enter activation, repeat suppression and clipped drawing.
Settings uses them for text size, clock options and desktop color.

The asynchronous `desk_file_job` transport is in the shared userspace library
(`srvros/desk_file_job.h`), separate from controls. Text, Files and the desktop's
preference owner use it to keep filesystem work out of their UI event loops.
`desk_preferences` provides validated field merging, a bounded text format and
fixed-offset clock formatting. These models perform no file operations.

## Tables

`desk_table.h` provides up to eight columns and 256 borrowed rows. Unique stable
row IDs retain selection across data refreshes or application sorting. The first
cell supplies row typeahead; arrows, Home/End and Page Up/Down navigate rows.
Headings request sorting with `DESK_TABLE_SORT`; the app applies the order and
resubmits rows. The control shows the selected sort column/direction. F6 switches
between headings and rows; Left/Right chooses a heading and Enter/Space requests
its sort. Tab continues normal control traversal. Fixed-width columns and
remaining-space columns share one clipped viewport; narrow layouts clip trailing
columns. The shared list navigation and scrollbar provide scrolling, double-click
activation and capture cancellation without embedding a data source in the UI.

## Text prompts

`desk_prompt.h` composes the shared dialog, editor and focus router into an owned,
bounded single-line prompt. Opening selects the initial text; Enter submits the
field, Escape cancels, and Tab reaches Cancel/Accept. Outside clicks preserve
focus, while focus loss and resize cancel captures. Clipboard requests identify
the field so the application can apply the existing revision/selection guards.
Name validation, overwrite policy and the resulting operation stay in the app.
Files uses this for Rename and Duplicate, and the shared safe-default dialog for
permanent deletion.

## Search palettes

`desk_palette.h` combines a single-line editor and selectable result list. It owns
up to 64 validated item snapshots with 64-bit identities, label/detail strings,
and a bounded query. Space-separated tokens match labels or details; ASCII case
is ignored and other UTF-8 bytes match exactly. Up/Down moves the result selection
while typing, Enter activates, Tab reaches the list, and Escape or an outside
press requests cancellation. Replacing items preserves the selected identity.
The owner handles actions and clipboard requests, then calls `desk_palette_filter`
after external edits. No launching, focus policy or clipboard transport lives in
the control.

The compositor uses this for Command-Space application/window search. Open windows
use process identities; application entries explicitly launch a new instance.
Opening suspends client focus, cancel restores it, and outside clicks are consumed
through release. The query shares the desktop clipboard with applications.

## Search bars and text search

`desk_searchbar.h` is a modeless single-line query with Previous/Next, a case
checkbox, and Close. It returns commands and editing/clipboard flags; the owner
controls focus between bar and document and supplies result feedback. Enter and
Shift-Enter request forward/backward search; Escape closes. The compact 72-pixel
layout fits the editor's minimum width and preserves parent clips.

`desk_text_find` searches UTF-8 byte boundaries in either direction, wrapping once
and reporting whether it wrapped. Case-insensitive matching folds ASCII only;
other Unicode bytes compare exactly. It changes neither the document nor its undo
history. Text uses the bar with Ctrl-F, Ctrl-G/Shift-Ctrl-G and F3/Shift-F3; repeat
search also works after the bar closes. File pickers and modal dialogs close the
bar while retaining its query. Clipboard results recheck its lifetime and focus.

## Calendar and tabs

`desk_calendar.h` is a date browser for Gregorian years 1–9999. It owns a selected
date and an optional Today highlight supplied by its owner, accepts any first
weekday, and lays out adjacent-month days in six rows. Arrows move days/weeks,
Page Up/Down moves months (Shift moves years), Home/End chooses month bounds,
and Ctrl-Home chooses Today. Pointer release inside selects a date; the owner
handles any action. No clock, timezone, appointment or storage logic lives here.
The desktop's menu-bar clock uses it in a focus-isolated popup.

`desk_tabs.h` owns up to 16 validated tab snapshots with stable IDs. Selection
survives replacement/reordering by identity, and disabled tabs are skipped.
Left/Right/Home/End selects a page; Enter/Space activates it. The strip is one
Tab focus stop and integrates with `desk_ui`. Pointer capture follows the shared
release-inside/cancel rules. Frames have equal-width items, with clipped labels;
use at least 24 pixels per item and 24 pixels in height to enable interaction.
The app owns page contents and their retained state. System uses separate tables
under Processes and Network, retaining process selection when switching pages.

## Image view and folder snapshots

`desk_image.h` presents a borrowed immutable `0xAARRGGBB` pixel buffer. Fit keeps
an image at or below 100%; explicit zoom steps run from 6.25% to 400%. Zoom keeps
the image point at the viewport center. Dragging, wheel/trackpad deltas and arrow
keys pan; Page Up/Down pans by a viewport, Home/End chooses its bounds. `+`/`−`
zooms, `0` fits, and `1` selects actual pixels. The view clips to its frame,
composites alpha over a checkerboard, and participates in shared focus/capture.
The widget does not allocate, decode, open files or own its source pixels.

Preview uses a separate read-only `imageworker` for PNG/JPEG/BMP. Its transport
in `desk_image_job.h` snapshots a path, drains at most 64 KiB per poll, checks the
complete response before exposing pixels, and supports cancellation with a
nonblocking worker reap. The caller adopts completed pixels with `take` and
releases them using `desk_image_pixels_release` with the response dimensions.
Source, large decode and received pixel buffers use mapped memory rather than
the small process heap. Limits are 8 MiB input, 4096 pixels per dimension,
4 million total pixels and 64 MiB peak decoder payload allocation.

`desk_listing_decode` validates a complete worker folder snapshot before changing
caller-owned entry/item/detail arrays. It rejects malformed names, duplicate
identities/names and bad lengths, sorts folders first, and builds borrowed list
items. Files and Preview's picker share it; filesystem calls remain in their
workers. `desktop_apps.h` owns the launch catalog, pinned dock positions and image
extension associations. Preview is available through the launcher and Files.

## Structured documents

`desk_document.h` is a read-only, scrollable document made of title, heading,
paragraph, code and note blocks. The view copies up to 64 descriptors and borrows
immutable UTF-8 strings; each block is limited to 4095 bytes and total source text
to 32768 bytes. IDs are unique and nonzero. Invalid replacements leave the current
page intact. Word wrapping, explicit newlines and tabs are shared with dialogs
through `desk_flow.h`; code blocks use the mono font, while paragraphs reflow at
the supplied body size. Notes have a restrained inset rule.

The control owns its scrollbar and integrates with shared focus/capture, wheel,
arrows, Page Up/Down, Space/Shift-Space and Home/End. `jump` reveals a block subject
to scroll bounds; `text` measures or exports a plain-text page with blank lines
between blocks. It does not parse markup, select text, navigate links or access
files. The owner supplies navigation, history and clipboard behavior.

Help uses this view for offline application guides, with the shared search palette
for topics, 16-entry history and scroll restoration. The global SrvrOS menu and
F1 open Help; F1 chooses the active app's topic. Help and Preview remain launcher
entries, keeping the six dock pins stable. About SrvrOS opens the guide's About
page. Preferences reflow Help's body text without changing its content.

`desk_label_ellipsis` draws a single-line label, replacing an overflowing tail
with U+2026 at a UTF-8 boundary. It measures against the complete assigned frame,
not the current damage clip, so tiled redraws do not move the marker. It draws
nothing if the marker itself cannot fit. Titles, lists, tables and tabs use this;
ordinary clipped labels remain available for content that should not be elided.
Image sampling uses exact rational stepping, preserving the direct-division
reference output while avoiding a divide per displayed pixel.
