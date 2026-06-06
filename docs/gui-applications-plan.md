# GUI Applications and Polish Plan

This plan focuses on turning the current GUI from a successful prototype into a
small, coherent desktop with useful applications. It intentionally ignores the
old fixed-widget desktop path except as a regression target.

## Current Baseline

The supported GUI launch path is:

```text
/fat/bin/gui
```

`gui` starts `/fat/bin/displayd`, which owns the framebuffer/root backbuffer,
window frames, dock launchers, taskbar, cursor damage, z-order, resize,
minimize, close, and GUI2 event routing.

Current GUI2 applications:

- `calc`
- `notes`
- `textedit`
- `fileman`
- `paint`
- `gui2demo`
- `surfacedemo`

Useful pieces already exist:

- Client-owned GUI2 surfaces through kernel-managed pixel buffers.
- Configure/focus/pointer/key events routed from `displayd` to clients.
- App-side `gui2` helper library for windows, damage, event polling, theme
  helpers, buttons, textboxes, and canvas controls.
- Resolution smoke coverage for 800x600, 1280x800, 1440x900, and 1920x1080.
- Dock launch, taskbar restore, frame controls, resize, and paint smoke tests.

Implemented in the first interface polish slice:

- `gui2` has a reusable multiline text area with focus, cursor movement,
  newline insertion, backspace/delete, left/right movement, simple scrolling,
  clipped line rendering, and pointer-based cursor placement.
- `textedit` edits a real multiline buffer, loads `argv[1]` or
  `/fat/text.txt`, and supports Save, Reload, and Clear.
- `notes` persists `/fat/notes.txt`, keeps a quick-entry Add flow, and renders
  the note list through the shared multiline control.
- `fileman` is available as `/fat/bin/fileman`; it browses `/` and `/fat`,
  selects entries with Prev/Next, opens directories, and launches Text Edit for
  regular files.
- `displayd --launcher-smoke` deterministically launches dock apps for
  hidden-QEMU regression coverage instead of depending on synthetic relative
  mouse clicks.

Implemented in the second interface polish slice:

- `gui2` has a reusable list control with row selection, focus state, hover
  state, selected-row scrolling, pointer activation, and keyboard navigation
  through `j`/`k` plus Enter/Space activation.
- `fileman` now uses the shared list control instead of a read-only text area.
- `fileman` gained a name field plus New, Rename, Delete, Refresh, Up, and Open
  actions using the existing VFS `mkdir`, `rename`, and `unlink` syscalls.

Implemented in the third interface polish slice:

- `gui2` now has shared app header, status bar, button-row layout, and explicit
  toolbar/status/list-row theme metrics.
- Notes, Text Edit, File Manager, Calculator, and Paint use the shared app
  chrome helpers so the basic app set has consistent top and bottom bars.
- File Manager now supports Copy and Move for selected files/items using the
  name field as the destination, preserves selection after refresh when
  possible, and opens `.bmp` files with Paint while other regular files open in
  Text Edit.
- `tools/fileman_selftest_smoke.py` boots srvros in hidden QEMU and validates
  File Manager's mkdir/write/copy/move/delete cleanup path on `/fat`.

Implemented in the fourth interface polish slice:

- `gui2` list controls can render a compact table header with multiple columns,
  sort indicators, and pointer events for header clicks.
- `gui2` has a reusable confirmation dialog primitive with primary/secondary
  actions and Enter/Escape handling for modal app flows.
- File Manager now presents Name and Size columns, sorts directories before
  files, toggles ascending/descending sort by clicking column headers, and keeps
  the selected item visible after refresh where possible.
- File Manager delete is now a confirm-first operation instead of an immediate
  unlink, and action status survives directory refreshes.
- The File Manager self-test now checks the new sort ordering before validating
  the filesystem mutation path.

Implemented in the fifth interface polish slice:

- `gui2` list/table controls draw a compact scrollbar when content exceeds the
  visible rows.
- `gui2` list/table keyboard navigation now includes page up/down and top/bottom
  shortcuts: `p`, `n`, `g`, and `G`, alongside the existing `k`/`j` row
  movement.
- File Manager copy/delete now supports directories recursively with a bounded
  collection pass before filesystem mutation.
- File Manager move now attempts a direct rename first and falls back to
  recursive copy/delete when needed.
- File Manager action statuses include file/directory counts for recursive
  operations, and its self-test now validates recursive copy/delete cleanup.

Implemented in the sixth interface polish slice:

- File Manager recursive copy, fallback move, and delete now run through a
  cooperative operation runner instead of blocking the app event loop for the
  whole tree.
- File Manager shows in-progress file/directory counts while the operation
  advances and redraws between filesystem steps.
- File Manager app-driven copies now advance in bounded read/write chunks, so
  very large files yield back to the UI between chunks instead of copying in one
  long synchronous call.
- File Manager rejects new navigation/mutation button actions while an
  operation is active, keeping the operation model simple and predictable until
  a richer progress/cancel dialog exists.
- The File Manager self-test now validates the cooperative recursive copy and
  delete runner in addition to the synchronous helper path.

Implemented in the seventh interface polish slice:

- `gui2` dialogs now support progress mode with a progress bar, progress text,
  and secondary-action cancellation.
- File Manager opens a cancellable progress dialog for recursive copy, fallback
  move, and delete operations.
- Canceling a File Manager operation closes any open copy descriptors, refreshes
  the directory view, and reports the canceled state in the status bar.

Implemented in the eighth interface polish slice:

- `notes` is now a two-pane GUI2 app with a note list, multiline editor, title
  field, and New/Rename/Delete/Save/Reload controls.
- Notes are stored as individual text files under `/fat/home/notes`, with title
  sanitization, stable sorted listing, duplicate-title avoidance, and size
  details in the list.
- Notes uses shared GUI2 chrome, confirmation dialogs, multiline editing,
  resize/configure handling, dirty-document status, selection refresh, and
  discard-change prompts before switching, reloading, deleting, or closing a
  dirty note.
- `tools/notes_selftest_smoke.py` boots srvros in hidden QEMU, runs
  `/fat/bin/notes --selftest`, verifies create/list/rename/delete behavior on
  the exFAT image, and checks the resulting filesystem image.

Implemented in the ninth interface polish slice:

- `textedit` now tracks document dirty state, cursor line/column, byte count,
  and path status in its header/status bar.
- Text Edit guards dirty documents with discard confirmations before reload,
  clear, and close operations.
- Text Edit supports practical control-key shortcuts through the existing GUI2
  key path: Ctrl-S saves, Ctrl-R reloads, Ctrl-L clears, and Ctrl-Q exits.
- `tools/textedit_selftest_smoke.py` boots srvros in hidden QEMU, runs
  `/fat/bin/textedit --selftest`, verifies save/load/rewrite cleanup behavior
  on `/fat/home`, and checks the resulting exFAT image.

Implemented in the tenth interface polish slice:

- `gui2` now includes a shared file open/save dialog with current-directory
  browsing, parent navigation, directory-first sorting, filename entry,
  Enter/Escape keyboard handling, and reusable Open/Save/Cancel buttons.
- Text Edit uses the shared dialog for Open and Save As, including dirty-file
  discard prompts before replacing an unsaved document.
- Text Edit now has explicit Open and Save As toolbar actions in addition to
  the existing Save, Reload, Clear, and close flows.
- Paint uses the same shared dialog for BMP Open and Save As. The first pass
  accepts exact 160x120 BMP files, matching the current fixed paint canvas,
  while later canvas-resize work can relax that constraint.
- Paint now exposes zoom, pan, fit, and visible image/view dimensions. The
  shared GUI2 canvas maps drawing input through its current viewport, so future
  image and document viewers can reuse the same source-to-screen transform.
- `tools/paint_selftest_smoke.py` boots srvros in hidden QEMU, runs
  `/fat/bin/paint --selftest`, verifies BMP encode/save/reload/decode pixel
  round-tripping on `/fat/home`, and checks the resulting exFAT image.

Implemented in the eleventh interface polish slice:

- `displayd` now has a compositor app registry with stable app ids, launcher
  labels, expected window titles, categories, executable paths, accent colors,
  and default window-size hints for the current GUI app set.
- Dock launches record the spawned PID's app identity, and surface windows bind
  back to that registry either by launch PID or by matching their announced
  window title.
- Window title bars and focused taskbar entries now use the app registry accent
  color instead of one hardcoded compositor accent, while launch logs and dock
  labels continue to preserve the existing smoke-test-visible strings.
- `displayd` now loads launcher metadata from `/fat/etc/displayd/apps.conf`
  when present, with compiled defaults as the recovery path. The generated
  exFAT image ships this config in a compact pipe-delimited format:
  `id|label|title|category|path|color|width|height`.
- Surface-window creation uses registry default width/height hints when a
  client does not provide a usable initial size, while existing app-provided
  surface sizes continue to take precedence.

Implemented in the twelfth interface polish slice:

- `/fat/etc/displayd/apps.conf` now supports the richer format
  `id|label|title|category|path|icon|color|width|height|flags|order`.
- `displayd` keeps compatibility with the earlier eight-field app registry
  format, but the generated image now ships icon, flag, and ordering metadata.
- App entries are sorted by `order`, can be hidden from the dock, and can be
  marked disabled so installed-but-unavailable tools have a visible disabled
  state instead of failing silently.
- Dock launchers now render compact symbolic icon tiles derived from app
  metadata, with disabled entries drawn in a muted style.
- New windows keep app-requested initial positions when they are reasonable,
  fall back to a work-area-aware default position when needed, and cascade away
  from already-open windows to reduce immediate overlap.

Main limitations:

- Visual language is still programmer-art level.
- Apps are useful demos, but not yet everyday utilities.
- Text rendering is bitmap-font based with limited metrics.
- No menus, clipboard, notification primitives, or broad shared app-command
  model yet.
- The shared file picker is implemented and wired into Text Edit and Paint, but
  File Manager still needs richer open-with/save-location flows.
- No persistent per-app settings model yet. App launcher metadata is loaded
  from disk, but bitmap icons, per-user ordering, and richer desktop-file
  metadata are still future work.
- Operation cancellation is best-effort. Canceling after filesystem mutation has
  begun may leave the already-copied or already-deleted portion in place.

## UI Direction

The near-term look should be quiet, crisp, and utility-first:

- Dark desktop surface with subtle contrast, not a decorative landing screen.
- Flat rectangular controls with small radii and clear focus states.
- Consistent title bars, dock items, taskbar entries, and app content spacing.
- One neutral background, one panel color, one field color, one accent color,
  and specific semantic colors for warning/error/success.
- Window content should feel dense but legible, closer to a compact workstation
  UI than a marketing product page.
- Controls should align to a small spacing scale, such as 4, 8, 12, 16, and 24
  logical pixels.

The A1466's 1440x900 panel should be treated as the design reference at 1.0x.
All default windows should be sized as work-area fractions and then clamped to
minimum logical sizes.

## Design System Work

Before adding many apps, tighten the shared `gui2` vocabulary:

1. Theme tokens:
   colors, spacing, border widths, focus ring, disabled state, title font,
   body font, monospace font, and control heights.
2. Layout primitives:
   row, column, grid, stack, spacer, align, inset, split pane, and scroll area.
3. Input controls:
   single-line text field, multiline text area, button, icon button placeholder,
   checkbox, segmented control, list row, menu row, slider, and color swatch.
4. Content controls:
   label, code/monospace label, status bar, toolbar, image view, canvas, table,
   and tree/list view.
5. Dialog primitives:
   alert, confirm, file open/save, text prompt, color picker, and about dialog.
6. App shell helpers:
   standard toolbar, status bar, dirty-document flag, command dispatch, and
   keyboard shortcuts.

The goal is that applications assemble shared widgets instead of each app
hand-drawing its own tiny UI dialect.

## Application Milestones

### Milestone 1: Polish Existing Apps

- Calculator:
  add memory buttons, clear-entry behavior, keyboard input, percent/sqrt/sign,
  better expression display, and consistent grid spacing.
- Notes:
  add search/filtering, richer keyboard shortcuts, and optional save-before
  rename/create flows.
- Text editor:
  add selection basics, search, and tab-width settings.
- Paint:
  add richer tools such as pencil/line/rectangle/fill, undo depth of at least
  one, and optional canvas resize. Zoom, pan, fit, color swatches, and visible
  image/view dimensions are now in place.

Exit criteria:

- Each app survives resize, minimize/restore, duplicate instances, and close.
- Each app uses shared theme/layout primitives.
- Each app has hidden-QEMU smoke coverage for launch and one meaningful action.

### Milestone 2: Add Table-Stakes Desktop Utilities

Add small but real utilities:

- File Manager:
  browse `/`, `/fat`, and `/fat/home`; open files with associated apps; copy,
  move, rename, delete, mkdir; show size/date/type.
- Terminal:
  GUI terminal connected to the existing shell, with scrollback and copy/paste
  once clipboard exists.
- System Monitor:
  process list, memory, scheduler/workqueue stats, network state, mounted
  filesystems, and kill/refresh controls.
- Network Panel:
  show IP/router/DNS, link state, route table, ARP table, and quick ping/host
  probes.
- Log Viewer:
  open `/fat/var/log/boot.log` and dmesg captures with search/filter.
- Settings:
  theme, pointer speed, keyboard repeat, date/time display, and GUI scale.

Exit criteria:

- The desktop can be used to inspect the running system without returning to
  the monitor for routine information.
- Each app has a simple app command contract and predictable persistence path.

### Milestone 3: Make Apps Feel Integrated

- Add file associations:
  `.txt` -> text editor, `.bmp` -> paint/image viewer, `.log` -> log viewer,
  unknown files -> file info dialog.
- Add a shared recent-files store.
- Add clipboard primitives for text first, then images.
- Add app menus or toolbar command menus.
- Add modal dialogs and non-modal progress/status messages.
- Add launch errors that render in the GUI instead of only on the console.
- Add app icons when the asset pipeline is ready.

Exit criteria:

- Users can launch, inspect, edit, save, and reopen common files entirely inside
  the GUI.
- Apps share visible interaction patterns.

### Milestone 4: Desktop Polish

- Improve dock:
  hover/focus states, running indicators, tooltips, and disabled/error launch
  state.
- Improve taskbar:
  active window marker, minimized state, title truncation, and overflow.
- Improve windows:
  resize cursors, better hit targets, title truncation, minimum/maximum size,
  snap-to-edge, and optional centered launch.
- Improve cursor:
  separate cursor shapes for arrow, text, resize, and busy.
- Add keyboard navigation:
  Alt-Tab or Ctrl-Tab window switching, Tab focus traversal, Enter/Escape
  dialog behavior, and app shortcuts.
- Toolkit-level Tab and Shift-Tab focus traversal is now shared by GUI2 lists,
  text fields, text areas, and buttons. The keyboard layer emits the standard
  backtab escape sequence for Shift-Tab, and `displayd` folds it into a single
  GUI key event for apps.

Exit criteria:

- Moving, resizing, focusing, minimizing, and launching windows feels consistent
  across resolutions.

### Milestone 5: Rendering and Performance

- Replace copy-heavy surface updates with shared mapped surfaces.
- Add a present queue with rectangle coalescing and backpressure.
- Cache glyph rendering and add string measurement.
- Add clipping and scroll invalidation helpers.
- Prepare compositor backends for software, Intel blitter, and later render
  acceleration without changing app-facing APIs.

Exit criteria:

- GUI stays responsive with multiple windows, text editing, and paint canvas
  updates.
- The same apps run unchanged on QEMU and A1466.

## Suggested Next Batch

The best next development batch is:

1. Add selection/search shortcuts to Text Edit and Notes.
2. Extend hidden-QEMU smoke coverage for file dialog flows, file manager navigation,
   app resize, duplicate instances, and clean shutdown.
3. Start the GUI terminal/system-monitor utilities once the shared dialog and
   editor-document flows are settled.

That batch gives the desktop a more complete feel and creates the foundation
for every future app.

Progress on this batch:

- Text Edit and Notes now have visible Find fields plus Find buttons. Ctrl-G
  repeats find-next using the current query and jumps the editor cursor to the
  next case-insensitive match.
- The shared GUI2 textarea exposes a cursor setter so apps can jump to search
  results while keeping the match scrolled into view.
- Paint now has zoom out, zoom in, fit, and WASD/arrow-like toolbar panning over
  a shared canvas viewport. The status line reports image size, zoom, viewport
  size, and viewport origin.
- True text selection ranges remain pending a richer textarea selection model.
