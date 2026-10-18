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

## Shared clipboard and useful Files workflows

The desktop now owns a 64 KiB session clipboard. Client transfers are bounded
and nonblocking. Copy publishes only a complete staged value; partial copies
and client death retain the previous value. Paste cancels if the clipboard
changes mid-transfer. Cut waits for acknowledgement, and Cut/Paste recheck the
editor revision and selection before mutating text. Malformed UTF-8, oversized
pastes and newlines in single-line fields are rejected without losing content.
The clipboard survives the source window closing, until the desktop exits.

Files now supports Ctrl-L for its location, Ctrl-R to refresh, Alt-Up for the
parent, and timed name-prefix selection in the reusable list control. A VM run
created a folder, navigated into it/back out, and selected/opened the saved
text document from Files. Cross-app copy from Calculator to Text, acknowledged
cut/undo, save and console readback passed in the same run.

Visual review exposed duplicate directory rows after 64 distinct children.
The libc `readdir` shim had a fixed 64-name deduplication array and silently
stopped recording names after that point. It now grows the set, reports memory
or listing failures, rejects overlong names and correctly identifies explicit
directory entries. `scandir` propagates listing failures instead of returning
partial success. Host/ASan/UBSan checks cover >100 children, repeated descendant
paths, allocation retry and error propagation. QEMU confirms the duplicate row
is gone. Full core checks pass; shared clipboard model tests and sanitizers pass.

The VM harness hit one diagnostic-output race: startup logging interleaved with
compositor output. Startup is now logged before window registration and app-mode
trace logs only the first frame per window. Assertions still verify persisted
file content; diagnostics are not treated as an atomic logging channel.

Next: a graphical terminal with a reusable screen/view model and isolated tty
state, followed by richer shared menus/dialogs and broader application workflows.

## Isolated terminal sessions

Added a bounded PTY line discipline and refcounted kernel endpoint manager,
exposed through `srv_pty_create` (master/slave descriptors). Each pair owns its
input, output, termios, window size and foreground group. Canonical records,
EOF, UTF-8 erase, raw bytes, echo/output newline processing, VMIN, poll,
nonblocking backpressure, descriptor duplication and hangup are covered.
Foreground Ctrl-C/resize signals are restricted to processes attached to that
PTY, even if another terminal uses the same numeric process group. Closing the
last master sends HUP and wakes blocked readers/writers. Redirected `isatty` and
`fstat` now describe their actual endpoints; terminal group ioctls are fd-aware.
The shell line editor disables ISIG while editing its prompt, then restores it
for foreground commands. Masked signals do not interrupt blocked reads, and
unblocking a default-ignored SIGWINCH does not kill the process.

This is an initial native PTY API: no `/dev/pts` namespace, no VTIME timers,
serial-device controls, background-job SIGTTIN/SIGTTOU enforcement, or complete
POSIX terminal emulation claim. Unsupported termios modes are rejected.
Explicit nonblocking PTY operations follow the existing desktop try-I/O gate;
they retry while another process owns storage rather than entering unsafe work.

Host tests exercise bounded state/fuzz invariants, waiting/wakeup, interruption,
capacity/generation reuse, descriptor lifecycle and terminal-scoped signals.
ASan/UBSan passes the model and manager. The full core suite passed, and the
`ptyprobe` guest passed real syscall tests for isolated attributes, binary raw
input, canonical EOF, inherited stdio, window size, character-device stat,
foreground Ctrl-C and close hangup. Evidence: `build/review/pty/`.

## Native Terminal and reusable terminal controls

The dock now launches a real Terminal app, and New opens independent windows.
It runs the native shell over its own PTY, with bounded nonblocking read/write
batches, a 64 KiB input queue, scrollback selection, shared clipboard, window-size
updates and a close confirmation while the shell is running. Completed sessions
remain visible. Ctrl-Shift-C/V copy/paste, Ctrl-C interrupts a foreground command,
and Shift-PageUp/PageDown scroll history. Pasted text is validated and rejected
as a whole if it contains control bytes other than tab/newline/carriage return.
The shell keeps per-window command history in memory (`HISTFILE=` now explicitly
disables disk history) rather than competing over a shared history file.

The screen parser, view, selection, scroll following and key encoding live in
the widget library. The app owns PTY and clipboard transport. Bundled DejaVu Sans
Mono supplies a fixed 10-pixel cell font; ANSI color/cursor/erase/scroll/margin
commands, alternate screen, fragmented UTF-8 and bounded query responses are
supported. Limitations are documented in `userspace/widgets/README.md`.

Two old console assumptions were corrected: `clrscr`/`gotoxy` now send control
sequences through stdout, and linenoise reads escape continuations by polling
its own stdin instead of consuming the physical keyboard queue. `stty size`
reports the terminal's actual rows/columns; raw mode uses the shared `cfmakeraw`.

Host tests and ASan/UBSan pass parser fragmentation/fuzz bounds, colors/margins,
UTF-8, alternate screens, resize, selection/copy, scroll anchoring, keyboard
encoding, tiled/clipped rendering and pixel guards. Host checks also cover
console-control routing and terminal-local escape reading. QEMU passed two
independent shells, clipboard commands, edited-command persistence, scrollback,
Ctrl-C status 130, continued operation of the other shell, Clear, resizing
(20×90 reported by the child), close cancellation and shell/window exit. The
Files/Text/application regression also passed with the six-item dock. Screens
were visually inspected under `build/review/desktop-terminal/`.

The first Terminal harness expected a console exit log for a killed command.
The screen correctly showed status 130; the harness now checks the shell's
persisted `$?` value. This was an assertion issue, not a Ctrl-C failure. These
remain VM/host results; the accepted hardware checkpoint is unchanged.

## Parent-owned waits and orphan cleanup

Reviewing Terminal lifecycle exposed a pre-existing race: the system init's
periodic `wait(0, NOHANG)` could reap another live application's child. A new
host regression reproduced the incorrect behavior before the fix. Wait targets
are now restricted to the calling parent. The designated system reaper can
adopt children only after their parent exits; kernel-owned hardware autotest
processes remain reserved for their kernel waiter. Legacy monitor background
launches are explicitly assigned to init.

The host regression and full core suite pass. The guest PTY probe now delays
its wait through multiple init polling cycles and still receives status 37;
it also creates an orphan and confirms init removes it. This passed in QEMU.

## Shared menus and global desktop commands

The widget library now provides a menu bar/popup control with pointer release
and press-drag selection, hover switching, F10/arrow navigation, disabled items,
checkmarks and shortcut labels. It owns a bounded validated UTF-8 snapshot and
returns action IDs; it performs no application work or transport. Editor menu
commands and keyboard shortcuts share `desk_edit_command`.

Protocol v4 adds staged application-menu publication and revision-tagged
commands. The server retains the previous complete menu until a replacement
validates; clients reject stale, disabled, unknown or unfocused commands. The
kernel enforces message direction. Apps supply File/Edit/View/Go/Calculate
menus as appropriate; the desktop adds SrvrOS launch/exit and Window menus,
including minimize, zoom, next-window and a live window list. Menu damage is
limited to the bar and old/new popup bounds.

The full host core suite passes, including menu model, transport, client-runtime
and kernel direction tests. Menu ASan/UBSan and tiled/clipped compositor drawing
pass. QEMU passed File-menu Save, Edit-menu Copy, cross-app clipboard, normal
launch/minimize/zoom/close workflows and keyboard global exit. The Terminal
regression also passes. Visual evidence is in `build/review/desktop-apps/`.
The first VM run exposed a missing menu-event branch in the tools app; that was
fixed and the complete workflow rerun successfully. Hardware remains unchanged.

## Shared confirmation dialogs

Text's unsaved/replace panels and Terminal's close confirmation now use the same
owned dialog component. It keeps safe initial focus, handles Escape/Tab/Enter,
preserves focus on blank clicks, cancels capture across focus/resize changes,
and wraps explanatory text at Unicode boundaries. The application still owns
modal state and actions. The host tests and ASan/UBSan pass; both native-app and
Terminal QEMU regressions pass. The unsaved panel was visually inspected.

## Shared Open / Save picker and document workflows

Text now has Open, first-save and Save As panels built from a reusable file
picker. The control owns lexical path/filename state and borrowed list items;
the app continues using its nonblocking file worker. It supports folder
navigation, parent/refresh/location shortcuts, typed selection, filename entry
and shared clipboard requests. Cancel remains responsive while a listing runs;
the controller drains/discards that response without touching the document.
The folder decoder validates a complete payload before replacing borrowed data.

Unsaved-document continuations survive a chosen save path, and canceling clears
the pending close/open safely. Overwriting a different existing file still
requires explicit Replace and uses the existing CRC guard, sync and readback.
The original 64 KiB document, 159-byte path and 256-entry listing limits remain.
The picker accepts UTF-8 lexically; the current exFAT creation path is still
restricted to ASCII names.

Host and sanitizer tests pass picker state, paths, Unicode boundaries, busy
cancellation, clipboard isolation, controller lifetime, drawing/clipping and
listing validation. QEMU passed initial Save, Open, Save As, replacement cancel,
confirmed replacement/readback, folder navigation/cancel, and the native app and
Terminal workflows. The picker was visually inspected. A test expectation was
adjusted after reopening a document: its caret correctly starts at the beginning,
so the append test now explicitly moves to the end.

The VM also exposed a real global-menu race after asynchronous save completion:
republishing app menus could dismiss an open SrvrOS menu and send the following
Enter into the document. Commit d5734ef preserves an open heading, keeps only
unchanged enabled selections and disarms changed pointer actions. Host regressions
cover this behavior, and the previously failing Terminal workflow now passes.

## UTC wall-clock foundation

The old `time()`, `gettimeofday()` and `CLOCK_REALTIME` exposed uptime as if it
were a Unix date. A bounded, read-only CMOS RTC sample now seeds UTC at boot;
runtime calls advance that seed with the monotonic reference and perform no CMOS
I/O. ACPI's no-RTC flag and optional century register are honored. Two complete
stable samples, update-in-progress checks, power-valid and mode checks, strict
BCD/binary decoding, 12/24-hour conversion and Gregorian validation reject bad
readings. Missing/stuck hardware cannot block boot indefinitely. Without a
century register the explicitly documented range is 1970–2069.

`SYS_REALTIME_US` exposes the cached clock. Libc and `date [-u]` use it and return
an error if no valid wall clock exists. Monotonic time remains independent;
its libc interface now retains microsecond precision when the reference exists.
The RTC is assumed UTC (Ubuntu's LocalRTC is false on this laptop). There is no
clock-setting, NTP, timezone database or suspend/resume resynchronization yet;
initial accuracy is limited by the RTC's one-second calendar sample.

Validation: RTC host cases cover invalid dates, leap-century rules, BCD/binary,
12 AM/PM, rollovers during sampling, ACPI bounds/no-RTC, stopped references and
cached reads. A disposable QEMU with RTC fixed to 2026-10-18 12:34 UTC passed the
new `clockprobe` ABI/libc checks and `date` output. Evidence:
`build/review/clock/serial.log`. Specification references: ACPI 6.6 FADT
(https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html), Linux
RTC register description (https://kernel.org/doc/html/v5.19/virt/kvm/x86/timekeeping.html).
The full core host suite also passed after the clock changes.

## Shared choices, menu-bar clock and persistent Settings

Checkboxes and radio choices now belong to the toolkit and focus router. Settings
uses them in a scrollable panel with wheel, thumb drag and Page Up/Down support.
The menu bar displays the validated UTC clock, with optional date, 12/24-hour
format and an explicitly fixed UTC offset in 15-minute increments. Offsets do not
pretend to provide a timezone database or automatic daylight-saving changes.
Text size and sage/sand/slate desktop colors apply to open windows immediately.

Protocol v5 broadcasts the complete preference snapshot, revision and save state;
focused Settings requests update individual fields. The desktop is the single
preference owner. It reads `/fat/home/desktop.conf` asynchronously and merges
changes made during that initial read. Saves are debounced, snapshot-based,
CRC-guarded, synced and read back by the shared file worker. Edits during a save
trigger another snapshot. Invalid or unreadable configuration and conflicting
writes retain session choices without blindly replacing the file. Settings
reports saving, saved, session-only and conflict states. Normal desktop exit
waits for pending preferences, with a bounded escape for a stuck worker.

The worker transport moved from Text/Files into the shared client library.
File READ now distinguishes a missing directory entry from a data-read failure;
the latter must not be treated as permission to create a new settings file.
Host preference tests and ASan/UBSan pass, including malformed/truncated config,
field masks, calendar offset boundaries, delayed loading, in-flight changes,
debounce, conflicts and failure retention. The final Settings VM workflow passed
live updates, resizing/scrolling, disk readback and fresh-process restoration.

### Runtime image footprint discovered by regression

The expanded desktop exposed a real 512 MiB VM limit: two Terminals plus Text and
the preference worker left no physical pages for a new command's stack. The
application suite likewise could not start Settings. The boot initramfs was
265 MiB, mostly debug sections duplicated across its command binaries plus the
embedded exFAT image. Individual tiny commands carried nearly 1 MiB on disk even
when their executable segments occupied only tens of KiB.

Image packaging now removes section/debug trailers from **staged runtime copies**
only. A checked ELF64 packer preserves the program-header table and every segment
payload at its original offset, including TLS, notes and unwind information; it
clears the unused section-table fields and trims the tail. Full host `.elf` files
remain available for symbols/debugging. The exFAT builder uses the same helper.
All built binaries passed payload-equivalence checks. The staged initramfs app
payload fell from 142,674,248 to 6,186,587 bytes; the complete boot archive fell
from 265 MiB to 135 MiB. VM regressions are being repeated at the original 512 MiB.
The two-Terminal regression now passes at 512 MiB. Boot free pages rose from
35,428 to 68,744 (about 130 MiB recovered). One application-suite attempt stopped
at Limine's countdown before entering the kernel; its screenshot and serial log
identify a bootloader-stage failure, and a fresh VM run is being checked.

The full core host suite passed. The native application suite and two-Terminal
suite pass at 512 MiB with packed images; libc/node compatibility probes also
pass. The application-suite rerun reached and passed all guest checks. The VM
harness now explicitly selects Limine's visible entry if its countdown stalls,
with at most two Enter events and no retries after kernel loading starts.

A subsequent Settings test exposed a separate interaction race: a save-status
acknowledgement relaid out controls and canceled a held pointer press. Status-only
updates now preserve capture and typed offset drafts; a focused host regression
holds a checkbox press across the acknowledgement and verifies release activation.

The final Settings VM run passes after the acknowledgement fix and visual layout
polish. The default window now shows the complete settings panel; resizing smaller
retains scrolling and all choices. The startup test accepts either ordering of
initial preference loading and first window presentation, since both are async.
The checked screenshot is `docs/assets/screenshots/native-desktop-settings.png`.

### Accurate System memory and file dates

The memory syscall now reports firmware-described RAM rather than the allocator's
highest physical address. MMIO framebuffer ranges and address-space holes no
longer inflate a 512 MiB VM to roughly 4 GiB. The allocator's address-span API is
unchanged. VFS-generated timestamps now use UTC seconds from the RTC-seeded wall
clock, with zero for an unavailable clock; imported explicit timestamps remain
unchanged. Existing old uptime-based metadata is not guessed or rewritten.

Validation: PMM range/overflow/alignment tests, focused VFS creation/access/write/
utime/import tests, full core host suite, and exFAT failure/power-cut hardening
passed. A fixed-RTC 512 MiB QEMU guest passed libc wall/monotonic checks, actual
exFAT create/write/stat/utime/unlink checks, and a physical-RAM accounting bound.

### Shared table and live System app

Added `desk_table` to the widget archive and focus router: borrowed columns/rows,
stable-ID selection across refresh/sort, fixed and remaining-space columns,
right-aligned numbers, sorting requests/indicators, row navigation/typeahead,
double-click, vertical scrolling, clipping, and keyboard heading navigation.
F6 switches headings/rows; Left/Right and Enter/Space choose/apply a heading.
Application code owns data access and ordering.

System now shows a live process table and accurate physical-memory summary, with
pause/resume, manual refresh, column sorting and corresponding global View menu
commands. The process ABI appends parent PID and private mapped-user-page bytes;
size-negotiated copying retains compatibility with the previous shorter record.
The UI labels process mode and mapped pages explicitly rather than inventing CPU
usage or presenting shared surface memory as per-process private memory.

Validation: table/controller tests, table ASan/UBSan, full core and desktop host
suites passed. A 512 MiB QEMU workflow passed heading clicks/keyboard sorting,
pause/manual refresh/resume, resize/row navigation, global View commands, and a
libc probe covering new process fields, old record bounds and invalid ABI input.
Screenshots were inspected; the Parent column was widened after that review.

### Files details and guarded file actions

Files now uses the table for Name, Kind, Size and Modified UTC, with folder-first
sorting and stable selection across refreshes. The worker listing includes inode
and modification metadata. A reusable `desk_prompt` supplies editable Rename and
Duplicate panels, including focus/clipboard/cancel behavior. Files also offers
permanent Delete with Cancel initially focused and an explicit no-Trash warning.
F2 renames, Delete confirms deletion, and the File menu exposes the same commands.
Secondary toolbar actions move to the menu when the window is narrow.

A new guarded namespace syscall validates the selected inode, size, modification
time and node type in the same serialized continuation as rename/delete/copy.
Destinations are never overwritten. Open items are refused, and the existing
exFAT mutation/flush boundaries remain responsible for disk changes. The worker
syncs and verifies results; duplicate also checks the copied bytes. Duplicate is
currently limited to individual files up to 64 KiB, while directory rename and
delete require an empty directory. Metadata guards use second-resolution dates;
they are not a general content-version or multi-writer transaction API.

Validation includes shared prompt/controller tests, deferred clipboard isolation,
worker fragmentation/snapshot/error tests, kernel ABI/identity/collision/open-file
checks, and prompt ASan/UBSan. The QEMU workflow passed actual rename, verified
copy, default-cancel and confirmed delete, collision preservation, folder creation
and empty-directory rename. It reads the surviving document back through the
console after the desktop closes.

Visual review caught a pre-existing libc stub: `strftime` returned question marks
for every calendar field. A standalone C-locale formatter now handles common
calendar names, numeric fields, compound formats, ISO week years, numeric padding,
and UTC labels; unsupported formats fail instead of fabricating text. It passed
367,070 comparisons across 1900–2100 against the host C library, including ISO
week boundaries. Guest clock tests now assert formatted output, and `date` accepts
`[-u] [+FORMAT]`. Format meanings were checked against the
[POSIX strftime reference](https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/functions/strftime.html).
Locale/timezone databases and extended field-width rules for compound formats
remain unsupported.

A second review found mount-time timestamps masquerading as modification dates.
Imported exFAT entries now start with unknown dates before persisted SrvrOS
metadata is applied. Native exFAT timestamp/UTC-offset fields are not yet decoded;
unknown dates display a dash. New mutations still receive UTC timestamps, and a
remount test advances the mock clock to prove persisted dates are retained.

Final checks for this slice: the complete exFAT fault/power-cut suite passes with
unknown imported dates and persisted UTC remount checks. The final Files workflow
passes with that change, as does saved Settings across a fresh desktop session
using the revised worker protocol. The fixed-RTC clock guest verifies both libc
calendar formatting and `date +FORMAT`. File-date rendering bounds malformed
metadata to the supported year range before calendar conversion.
Screenshots: `native-desktop-files.png`, `native-desktop-rename.png`, and
`native-desktop-system.png` under `docs/assets/screenshots/`.

### Global application/window launcher

Command-Space and the SrvrOS menu now open a shared `desk_palette`. It owns bounded
search/query state and stable 64-bit result IDs; token filtering, keyboard/list
navigation, cancellation and clipped drawing remain independent of launching or
IPC. The desktop lists recent open/minimized windows and explicit new-app entries.
Window actions resolve process identities at activation, avoiding reused slots.

The overlay suspends client focus, restores it on cancel, and consumes outside
clicks through release. Its single-line query shares the desktop clipboard;
local copies cancel older staged writers and update the clipboard revision.

Validation: palette and clipboard host tests, full desktop host regression,
ASan/UBSan, tiled/full scene equality with the overlay, and the 512 MiB QEMU
workflow passed. The guest verifies search via keyboard and global menu, focus
restoration by saved-document readback, no close-box click-through, both clipboard
directions and explicit new windows. Visual review shortened detail labels to fit.
Screenshot: `native-desktop-launcher.png`.

### Files navigation and a real system root

Files now has a Places area (Home, Startup disk, System root), Back/Forward,
Alt-Left/Right, Alt-Home and F5. Its 24-entry history commits only successful
listings and retains per-location selection/scroll state. Failed requests leave
the displayed folder and history intact; new navigation after Back discards the
old forward branch. Lexical absolute paths collapse repeated slashes and dot
components. Narrow layouts keep navigation and move secondary commands to menus.

The guest test exposed an existing VFS gap: boot-archive files had no root or
parent directory nodes. Boot registration now creates `/` and bounded persistent
parent-directory names, rejecting conflicting or noncanonical archive paths.
This makes root and archive subfolders visible to stat/readdir and the GUI.
It does not make the boot archive writable.

Validation: full core and desktop host suites, Files ASan/UBSan, root/parent VFS
and root enumeration tests passed. The QEMU Files workflow passes Places,
Back/Forward/branching, failed-path recovery, normalization, refresh and all prior
rename/duplicate/delete checks. The final layout screenshot was inspected and
updated at `native-desktop-files.png`.

### Shared search bar and Text search

Text now supports incremental search, Previous/Next, one wrap around the document,
ASCII case-insensitive or exact matching, and a modeless shared search bar.
Ctrl-F opens/focuses it; Ctrl-G/Shift-Ctrl-G and F3/Shift-F3 repeat a search even
after closing the bar. Clicking the document restores editing while the bar stays
visible. Opening a file picker or modal dialog closes search and retains the query.
Deferred clipboard mutations recheck the bar's lifetime/focus and cannot spill
into the document.

The toolkit separates `desk_searchbar` interaction from the read-only
`desk_text_find` model. Focused host tests include 20,000 reference comparisons,
UTF-8 boundaries, directions/wrapping, case rules, query validation, controller
focus ownership and delayed clipboard isolation. Search ASan/UBSan passed.
The QEMU workflow passed incremental, next/previous, case and wrapped searches,
query clipboard transfer, hidden-bar repeat, document editing, picker isolation,
and exact saved-document readback.
