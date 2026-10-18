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
  and presentation. It does not execute controls or own application documents.

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

The study currently has one text destination and a Ctrl-L action. General
within-window focus traversal is **not implemented**. The current compositor
reserves Tab for window switching and TEXT transports decoded console bytes.
Structured key/modifier events and explicit window-focus notifications need a
tested transport extension before adding general keyboard navigation, text
selection, shortcuts and clipboard behavior. The toolkit event API leaves
those routing decisions outside individual widgets.

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
