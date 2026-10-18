# Desktop development session, 2026-10-18

The user authorized autonomous development until **10:00 EDT (14:00 UTC)**:
advance the widget library, shared desktop UI and useful GUI applications,
adding supporting OS work as needed. Use host checks and VMs; **do not reboot
the laptop or replace the accepted hardware boot**. The visual direction stays
quiet and refined; Tiger is a reference for scope/coherence, not a pixel copy.

## Working sequence

1. Structured keyboard delivery, reusable editor/list controls and focus routing.
2. Shared client runtime; a desktop launcher, coherent window navigation and
   application lifecycle/unsaved-close behavior.
3. Files and Text: browse, open, edit, save, reopen with honest errors and durable
   data. Keep file work out of the input/render path.
4. Settings/basic system tools and Terminal as supported by available OS APIs.
5. Regression, visual review, performance and remaining supporting OS work.

Commit verified slices, preserve evidence, and carry forward explicit limitations.
The accepted hardware checkpoint remains `f05dfcc01c28dc85` (source `a870060`).
The initial separate toolkit checkpoint is `9149128`.

## Structured keyboard foundation

Two opt-in GUI-owner-only syscalls expose keyboard events. USB HID and translated
PS/2 supply usage IDs, printable codepoints, modifiers, press/release and repeat.
The stream is independent of console Escape sequences and control bytes. In event
mode Ctrl-C is a key binding, not a foreground-process interrupt. Mode changes
discard queued input; old held-key repeat cannot replay into the console. Queue
overflow and HID rollover expose cancellation. The GUI owner's exit restores
console input even when it never explicitly disables event mode.

The existing console and hardware-test desktop remain on their byte input path.
The event layout currently supports US ASCII translation; full keyboard layouts,
IME/composition and device-specific identifiers are future work. Clients will
receive events through the compositor, not access the kernel queue themselves.

Validation: full build and core host suite passed. Keyboard ASan/UBSan passed.
The QEMU compositor regression additionally exercised real structured input,
Ctrl-C isolation, arrow keys and abrupt input-owner exit, then ran the existing
study/window/scrollbar/lifecycle checks successfully. Evidence:
`build/review/keyevents-20261018/`.

## First native application desktop

`run /fat/bin/deskd` now opens the application desktop. The earlier hardware
studies remain available through their explicit options. The shell supports
eight independent client windows, a five-application dock, a global menu,
Alt-Tab, focus/stacking, drag/resize, minimize/dock restore and zoom/restore.
Normal close requests let Text present Save/Discard/Cancel. Ctrl-Alt-Escape is
an emergency exit; it does not save documents.

The shared toolkit now has a UTF-8 text model, single/multiline editor,
selectable list, keyboard focus traversal and capture routing. The common client
runtime handles sealed-frame release, resize generations, input acknowledgement,
server loss and bounded queue retry. Apps use these components:

- **Text**: edit, select, one-level undo/redo, open, save, guarded replacement,
  unsaved-change prompts and title dirty marker. Edits during an in-flight save
  remain dirty; only the saved snapshot is marked clean.
- **Files**: asynchronous directory browsing, selection/open, parent navigation,
  refresh, and folder creation. Text opens documents. Lists are capped at 256
  entries and report truncation; paths are capped at 159 bytes.
- **Calculator**: arithmetic expressions, parentheses, decimal/exponent parsing,
  syntax/range/division-by-zero errors.
- **System**: live memory and process counts plus target information.
- **Settings**: standard/larger document text broadcast to open and future
  windows for this desktop session. Persistence is not yet implemented.

File work uses a separate persistent worker. The GUI sends/receives bounded
nonblocking pipe chunks and keeps its event loop running. Explicit nonblocking
pipe writes now defer during another process's owned storage I/O, matching the
existing try-read contract. The memory-stat snapshot is also admitted as a
bounded read-only syscall. Save uses `srv_fs_replace` (the whole-file syscall),
not the general `srv_fs_write` helper that first opens with O_TRUNC. This avoids
publishing an empty file before the replacement. The worker syncs and reads back
its snapshot. It checks an existing file's CRC before replacing, but this is not
an atomic compare-and-swap against other concurrent writers. No power-loss
atomicity claim is made for exFAT directory-sector writes.

Host coverage includes UTF-8/selection/undo/clipped rendering/focus; client
release-before-resize and queue retry; worker fragmentation and snapshot
ownership; file replacement guards, empty files and failure cases; syscall
copy-before-publication and allocation failures. The full core suite passed.
The original QEMU compositor study passed with the expanded desktop. The first
application VM run passed launch, typing, save/readback, unsaved-close cancellation
and global exit; screenshots were inspected under `build/review/desktop-apps/`.
Further window/settings/save checks are in progress. Clipboard is still per-app
at this checkpoint; the next slice makes it shared.

The expanded QEMU run also passed shared text-size changes, minimize/dock
restore, zoom/restore, and empty-file save through the whole-file syscall.
Visuals were checked at 1280×800. ASan/UBSan passed the text/editor/UI controls,
file worker behavior and fragmented worker client. These are VM results, not a
new hardware acceptance claim.
