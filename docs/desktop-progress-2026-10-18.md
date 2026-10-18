# Desktop development session, 2026-10-18

The user authorized autonomous development until **10:00 EDT (14:00 UTC)**:
advance the widget library, shared desktop UI and useful GUI applications,
adding supporting OS work as needed. Use host checks and VMs; **do not reboot
the laptop or replace the accepted hardware boot**. The visual direction stays
quiet and refined; Tiger is a reference for scope/coherence, not a pixel copy.

For the current app set, shortcuts and limits, use the
[native desktop guide](native-desktop.md). Entries below are chronological:
statements such as “not yet implemented” describe that entry's checkpoint and
may be superseded by later work in this journal.

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

Final search validation also passed entry through the global Search menu, and the
checkbox focus frame was shortened after visual review. The complete core and
desktop host suites pass. Screenshot: `native-desktop-search.png`.

### Bounded UTC calendar conversion

Replaced libc's per-year loop and pre-1970 clamp with bounded Gregorian conversion.
`gmtime[_r]` and UTC `localtime[_r]` support negative epochs, validate representable
`tm_year`, and preserve output on overflow. The reentrant forms no longer use a
shared temporary. `mktime` normalizes out-of-range calendar fields, updates the
caller structure and checks overflow; positive DST hints normalize into the fixed
UTC zone. There is still no timezone/DST database. Added the missing `EOVERFLOW`
error constant and message.

The shared day/date formulas are adapted from Howard Hinnant's
[public-domain calendar algorithms](https://howardhinnant.github.io/date_algorithms.html).
Normalization behavior was checked against the
[POSIX mktime reference](https://pubs.opengroup.org/onlinepubs/009695399/functions/mktime.html).
Host tests passed 669,508 comparisons with libc: every day of 1600–2400, negative
and extreme epochs, randomized field normalization, leap days and output
preservation on overflow. ASan/UBSan and the complete core/desktop host suites
passed. The fixed-RTC guest verifies negative epochs, normalized leap-day input,
overflow handling, current clock formatting and actual file timestamps.

### Shared calendar and global clock panel

Clicking the menu-bar clock or choosing Calendar from SrvrOS opens a date browser.
The shared calendar widget handles Gregorian dates from year 1 through 9999,
month/year paging, day/week navigation, Today, keyboard focus and release-inside
pointer activation. The desktop supplies its current fixed-offset date; changing
the offset updates Today across midnight. The panel restores client focus and
consumes dismissal clicks through release, and can switch directly to Launcher.
It is a date browser, without appointment storage.

Validation: 2,825,928 reference grid dates across every month of 1600–2400 and all
seven week starts, boundary/leap tests, capture cancellation, focus routing,
clipped/tiled rendering, ASan/UBSan and the full desktop host suite passed. QEMU
passed clock/menu entry, navigation, Today, pointer selection, offset rollover,
overlay switching and exact Text readback after focus restoration. The screenshot
was inspected and saved as `native-desktop-calendar.png`.

Host tests now reuse a separate native toolkit archive with generated header
dependencies and atomic archive replacement. The freestanding production archive
is unchanged. This avoids recompiling the entire toolkit separately for each
host test; the complete desktop host suite passed using the shared objects.

### Multi-step editing history

The shared text model now supports an optional caller-owned delta journal, with
up to 256 edits and a fixed byte budget. It restores UTF-8 selections, discards
redo only when a new edit creates a branch, evicts the oldest records as needed,
and rejects failed/aliased insertions without changing history. The original
two-buffer single-step mode remains available. Text allocates 256 KiB for edit
payloads; if allocation fails it retains single-step undo. Saving retains history;
loading/new documents clear it. Menus test independent undo/redo availability.

Validation: 300,000 randomized reference-model operations, maximum-size edits,
byte/record eviction, UTF-8 and failure atomicity passed with ASan/UBSan and the
full desktop host suite. QEMU passed multi-step undo/redo, redo branching, cut
restoration, independent path-field history, global Edit commands, history after
save, reset after open and exact final-file readback. Edits are individual actions;
continuous typing is not yet coalesced into word-sized undo groups.

### Shared tabs and System network information

The new tab strip owns validated labels and stable identities, preserves selection
on item replacement, skips disabled pages, and shares keyboard focus and pointer
capture rules with other controls. System uses it for Processes and Network. The
network page reads the versioned kernel status ABI for IPv4/router/DNS, hardware
address, address source, packet totals and socket counts. Pause/Refresh applies
to both views, with process selection retained between pages. A configured IP is
reported as such; this is not a physical-link or Internet-connectivity claim.

Validation: host focus/identity/capture/disabled/resize tests, full-versus-tiled
rendering, ASan/UBSan, System status ABI/error/cadence checks and the full desktop
suite passed. QEMU passed both no-adapter and e1000 DHCP variants, including tab
navigation, pause/refresh, resize and real address/traffic counters. Screenshots
were reviewed: `native-desktop-network.png` and updated `native-desktop-system.png`.

### Bound number conversion before expanding Calculator

The existing decimal converter accumulated exponents in a signed int and scaled
once per exponent unit. Large pasted exponents could overflow that int or hold
the app in a very long loop. Decimal exponent accumulation now saturates while
consuming the full input, and scaling stops at floating-point overflow/zero.
Zero with any exponent remains exact (including signed zero), consistent with
[WG14 defect report 23](https://open-std.org/JTC1/SC22/WG14/issues/c90/issue0023.html).
Range errors and float narrowing are reported through ERANGE; leading ASCII
whitespace is handled. Conversion is extracted for independent host testing.
It remains the existing decimal-only, approximate converter: hexadecimal floats,
full long-double precision and correctly rounded conversion are future libc work.

Validation: huge and 65,536-digit exponents, signed zero, range/end-pointer cases,
30,000 ordinary decimal reference comparisons and ASan/UBSan passed. The full
core/desktop host suite and guest libc/node probes passed, including the new
large-exponent checks. The libc smoke runner is now headless and handles a stalled
Limine menu through QMP, matching the desktop test harnesses.

### Calculator keypad and calculation history

Calculator now has a keypad built from shared centered buttons, a separate result
display, Use result/Copy actions, and a 16-entry in-memory history table. Successful
evaluation selects the expression for fresh typing; an operator immediately after
a result starts from that result. Ans inserts the last value, ± negates the
expression, and history activation recalls an expression without adding a duplicate
entry. The expression is bounded to 255 bytes. Small windows keep entry, Calculate,
Clear and Copy, with remaining actions available from the global Calculate menu.

Visual review added the missing U+2212 minus glyph to all generated atlases and
shortened compact-window help/status text. Number-range diagnostics now preserve
the first parser error. The calculator still uses double arithmetic and a 12-digit
result display; history is session-local.

Validation: host controller tests cover keypad/result chaining, history eviction
and recall, clipboard snapshots, errors, capacity/focus and full-versus-tiled compact
rendering. ASan/UBSan and the full desktop host suite passed. QEMU passed real
keypad clicks, answer insertion/negation, history recall, huge-exponent and division
errors, compact resize, global Copy result and exact cross-app saved readback.
The final screenshot was reviewed and saved as `native-desktop-calculator.png`.

### Bulk pipe throughput for desktop workers

The new image worker exposed a transport bottleneck: a 4 KiB pipe emptied once
per 100 Hz UI turn carries only about 400 KiB/s, so a maximum-size 16 MiB pixel
response could hit its 30-second timeout. Pipes now have bounded 64 KiB rings
(4 MiB of fixed storage across the 64-pipe pool). Reads/writes copy at most two
contiguous spans instead of updating a ring index for every byte. Nonblocking
partial writes, wakeups, EOF, close behavior and rights offsets remain intact.

Validation: 20,000 randomized reference transfers plus wraparound, full/empty,
blocking wakeup, EOF, reader closure and rights-consumption tests passed with
ASan/UBSan. The full core/desktop host suite passed. QEMU passed a 100,000-line
three-process pipeline with exact persisted `cksum` readback (2052179976, 588895
bytes), and an early-closing `yes | head | wc` pipeline. The large-image VM test
completed a 16 MiB response in 399 ticks (about four seconds), including decode
and transfer. These are VM measurements, not a laptop hardware latency claim.

### Preview and shared image presentation

Preview is a native read-only PNG/JPEG/BMP viewer with Open/Cancel, fit/actual-size,
zoom, captured drag and two-finger/wheel panning, global File/View menus and the
shared file picker/clipboard. Its image view is a reusable allocation/I/O-free
widget over borrowed ARGB pixels, including transparent checkerboard compositing.
Failed or canceled loads retain the previous image and title. Small windows keep
all six toolbar actions; the Cancel label shortens to Stop. Preview is available
in the launcher and through image extensions in Files. One catalog now owns app
names, executable paths, dock pins and extension associations. Files and the
picker also share validated, folders-first listing snapshots.

A separate read-only worker uses a pinned stb_image implementation with only the
three supported decoders enabled. Files are limited to 8 MiB, dimensions to 4096
and total pixels to 4 million. Decode payload allocations are capped at 64 MiB
including temporary reallocation copies. Source/large decode/received buffers use
mapped memory; the initial heap-backed attempt exhausted the small process heap.
The transport consumes at most 64 KiB per turn, checks lengths/identity/checksum,
exposes only complete pixels, and cancels/reaps workers without blocking the UI.
A damaged-JPEG mutation exposed signed overflow in the upstream scalar IDCT;
a documented local patch widens both passes and clamps before narrowing. Exact
reference JPEG output is unchanged.

Validation: image geometry/alpha/clipping/tiled rendering, controller retention
and compact layout, decoder malformed/truncated/mutated inputs, mapped large-image
lifetimes, fragmented transport/checksums/worker death/timeouts/cancellation and
interrupted reaping passed host tests. Decoder, widget/controller and pipe checks
passed ASan/UBSan; the full core/desktop suite and production build passed.
The disposable QEMU workflow passed PNG/JPEG/BMP, malformed/oversized retention,
picker clipboard, global menus, resize, Files association and clean exit. Its
stress variant canceled/retried a 4096×1024 image, completed its 16 MiB pixel
response, and verified exact saved text typed into Text during that load.
`tools/desktop_preview_smoke.py --stress` and `tools/pipe_smoke.py` retain the
regressions. The screenshot is `native-desktop-preview.png`.

Current limits: nearest-neighbor zoom, no color-management or EXIF orientation,
no editing/export, no animated image support. The image worker is a separate
process with bounded resources, not a security sandbox. No hardware boot changed.

### Offline Help and shared structured documents

Help is a native application with twelve offline topics, a searchable shared
palette, Back/Forward history (16 entries with scroll restoration), page copying
and related-app launching. F1 opens the active application's topic. The global
SrvrOS menu includes Help, and About now opens a real About page. Help is in the
launcher without adding another dock pin. Larger-text preferences reflow its
body; the minimum window retains all toolbar actions and readable chooser hints.

The shared document widget supports validated borrowed title/heading/paragraph/
code/note blocks, word wrapping, explicit newlines/tabs, a captured scrollbar,
keyboard navigation, block jumps and plain-text export. It has fixed limits and
no allocation, I/O or navigation policy. Shared flow helpers were extracted from
dialogs. The clipboard client now safely accepts its own buffer or overlapping
slices as copy input.

Validation: content/UTF-8/limits and atomic replacement, history eviction and
branching, scroll restoration, clipboard snapshot/stale-reply guards, app routing,
minimum/large layouts, both body fonts, parent clipping and tiled rendering passed
host tests. ASan/UBSan, the full core/desktop suite and production build passed.
QEMU passed F1 context, search, history, scrolling, page copy into Text with exact
persisted-byte comparison, related Preview launch, search-field clipboard, minimum
resize, About/global menus, launcher and orderly exit. The test explicitly
qualifies app versus window searches because filenames may also match. Evidence:
`tools/desktop_help_smoke.py`, `build/review/desktop-help/`, screenshot
`native-desktop-help.png`. No hardware deployment or reboot occurred.

### Image sampling throughput and readable truncation

Image views now advance exact rational source coordinates using a quotient and
remainder instead of dividing for every destination pixel. The clipped left edge
initializes the sampler, so full and tiled draws remain identical. Pixel-write
accounting moves outside the inner loop. Four hundred randomized geometry/zoom/
clip cases match a separate direct-division reference, including exact write
counts and alpha output; sanitizers and the full desktop suite passed.

A host benchmark over 100 960×640 frames per mode measured the previous image
painter at 716/873/797 ms for opaque fit/actual/zoom and 859 ms for alpha-heavy fit.
The new painter measured 256/251/307 ms and 540 ms respectively in the same setup.
These are approximate renderer timings on the host, not a whole-desktop or
physical SrvrOS latency claim. `make bench-image-view` retains a direct-reference
comparison, without turning timing variability into a pass/fail threshold.

The shared single-line ellipsis helper preserves UTF-8 boundaries, parent clips
and damage-independent layout. Window titles, list rows, table cells/headings and
tabs now use it. Widths too small for the marker draw nothing. Both font sizes,
malformed/unsupported text, newline truncation, exact-fit boundaries, tiled drawing
and stride guards passed host/sanitizer checks. Production build and native
Preview/Help VM workflows passed; compact title screenshots were reviewed.

The Help smoke now uses the committed surface's PID and server retirement when
closing a window. UART mirroring deliberately permits interleaved process output
to preserve input latency; one run mixed an app farewell and server retirement.
That was test-observation ambiguity, not a failed close, and the console's bounded
critical sections remain unchanged.


### Clock and notification center

Added an independent Clock application with countdown and stopwatch tabs, a compact
monospace time face, pause/resume, exact-once completion, a 64-lap session history
and copyable time/TSV lap data. Duration input accepts seconds, m:ss or h:mm:ss up
to 24 hours; the stopwatch saturates at 100 hours. Timers and the stopwatch keep
running while minimized. Closing an active Clock asks for confirmation; these are
session timers, not persisted alarms, and there is no audio or suspend wakeup.

Reusable pieces are `desk_notice` (owned, validated informational cards), a clipped
determinate progress painter, and caller-clocked countdown/stopwatch models. The
notification client snapshots and retries a bounded request without blocking app
input. Protocol v6 adds app-to-server notices and server-to-app acknowledgements;
the kernel enforces those directions. The desktop validates identity/text, bounds
history to sixteen records/four per source, deduplicates retries even after Clear,
and rate-limits new notices. Eight-second banners never focus their source and
defer expiry/replacement during pointer capture. Alerts / Command-Shift-N opens
history; Open app restores the source window or launches its app if it has closed.

Host coverage includes exhaustive duration roundtrips, exact expiry and rollback,
copy/cut/paste isolation, lap eviction, close cancellation, queue pressure,
acknowledgement retries, source quotas, invalid UTF-8, duplicate serials, modifier
variants, capture/expiry and focus restoration. Cards, progress, Clock and history
views pass full/tiled rendering and stride guards at normal and compact sizes.
Address/undefined sanitizers passed for the notice, time-model and Clock suites.
Full `test-core` passed; subsequent targeted kernel transport/session checks passed
after VM-discovered integration fixes (notification packet routing and modifier
mask matching). The production image builds successfully.

`desktop_clock_smoke.py` passes in native QEMU: background timer completion while
Text keeps typing, banner dismissal without focus loss, history/open-source,
close cancellation, exact saved text and copied time, independently formatted lap
TSV comparison, compact timer/stopwatch, invalid input, history clear and orderly
session exit. Screenshots were visually reviewed and saved as
`native-desktop-clock.png` and `native-desktop-notifications.png`. Hardware remains
at its accepted checkpoint; no physical reboot/deployment occurred.


### Reusable contextual menus

Added an owned, bounded `desk_popup` widget with separators, disabled/checked
rows, keyboard/first-letter navigation, overflow scrolling, scrollbar capture and
outside-press/release isolation. Popup sizing uses independent label and shortcut
columns, clamps to the content area and preserves UTF-8 boundaries when clipped.
List row hit-testing/geometry is now public, so Files anchors keyboard menus near
the selected row without duplicating list layout.

Protocol v7 adds generation-checked server-originated context requests and distinct
coalescible hover packets. Right-click, Control-click and Shift-F10/Menu-key reach
app context handlers; the kernel rejects forged client context messages. Text and
editable tool fields retain their selection while offering Undo/Redo/Cut/Copy/
Paste/Select all. Files selects the clicked row without opening it and offers its
existing guarded file actions. The desktop background provides application
shortcuts through the same widget. Existing file confirmations are preserved.

Host tests cover atomic validation, owned snapshots, anchor bounds, disabled rows,
keyboard selection/reveal, press/release and cancellation, overflow wheel/drag,
clipping/stride/tiled rendering, clicked file selection, stale resize requests,
hover coalescing, kernel transport direction and all Control/Shift key variants
used by the new bindings. The desktop host suite passed, and popup address/UB
sanitizers passed. Production builds and `desktop_context_smoke.py` passed after
layout refinement: native mouse and keyboard routes, exact selected-text copy/
paste, outside click on New without activation, compact layout, Files rename,
duplicate/delete cancellation, desktop launch and orderly exit. Screenshot
`native-desktop-context-menu.png` was reviewed. Physical Control-click remains to
be tested at a later laptop checkpoint; no hardware deployment occurred.


### Shared property views and fresh Files information

Added `desk_properties`: sixteen owned UTF-8 label/value pairs, responsive two-column
or stacked layout, wrapped values, shared scrollbar/keyboard/focus routing and exact
plain-text export. Validation and snapshot publication are atomic, including inputs
borrowed from the existing view. Labels/values are bounded; all data remains local
to the widget with no filesystem or clipboard side effects.

Files now has Information (Ctrl-I, File menu or contextual menu), Copy path and Copy
details. A read-only worker request checks the selected inode and obtains current
kind, exact byte size and modification time without reading file contents. A changed
identity or missing item is shown explicitly. Folder contents are not recursively
totaled. Closing a pending information view does not reopen it on late completion.
The compact layout keeps Path, Details and Close available; metadata remains fully
scrollable. Help covers the information view and the new contextual bindings.

Host tests passed for worker metadata including files larger than the editor limit,
identity changes, missing files, fragmented responses and invalid response lengths.
Controller/widget tests passed for clipboard snapshots, late completion, changed/
missing/malformed results, both body fonts, compact layout, clipping and tiled/stride
rendering. Property/controller address/UB sanitizers and the desktop suite passed.
Production builds succeeded. `desktop_file_info_smoke.py` passed with exact file
content, path and metadata exports read back from the VM disk; folder metadata,
context access, compact scrolling and orderly exit were exercised. The reviewed
screenshot is `native-desktop-file-info.png`. No laptop reboot or deployment.

### Global screenshot capture and background image saving

Command-Shift-3 and SrvrOS > Save screenshot now snapshot the fully painted RAM
canvas (including the pointer), with no framebuffer reads. An immutable bounded
copy streams through nonblocking pipes to a dedicated worker; each desktop turn
sends at most 16 KiB. The worker writes a BMP through a temporary file, flushes and
verifies the content, performs a guarded non-replacing rename, then verifies the
final destination. Screenshots use unique names under `/fat/home/Screenshots`.
The 8-MiB BMP limit matches Preview and covers the Air panel and full HD. Normal
session exit waits for a pending save; the emergency exit never kills an accepted
file mutation. Completion appears in a nonintrusive notification with an Open
folder action. That destination is owned by the server, not client packets.

Host tests cover fragmented transfers, immutable pixel/path ownership, retry,
protocol/worker failure, limits, existing files, partial I/O, failed writes, sync,
readback and rename, cleanup and nonblocking worker reaping. BMP validation also
now rejects overflowing dimensions and malformed header offsets and handles the
signed-height minimum without undefined behavior. Address/UB sanitizers passed;
the full desktop suite and production build passed. The VM capture test compared
the saved BMP pixel-for-pixel with a QMP display reference, typed during saving,
verified exact editor content on disk, opened the notification's folder and loaded
the image in Preview, repeated capture through the menu, and exited cleanly.
Evidence: `desktop_capture_smoke.py`, `native-desktop-capture.png`. No hardware
reboot or deployment occurred.

### Shared color/drawing controls and Sketch

Added an inclusive signed-range slider with pointer snapping, keyboard steps,
focus/capture behavior and no accidental wheel changes. The reusable RGB panel
combines three sliders, twelve swatches, a color preview, exact hex export and a
draft that is only committed by Use. Both join the existing toolkit rather than
being private application controls. The compact panel fits a 280×240 window.

The bitmap model uses a caller-owned arena with bounded pointer-rotated history.
Strokes, lines, boxes and clear are transactions; canceled gestures restore the
previous image, previews leave no ghosts, no-op edits consume no undo step and
new edits discard redo. Saved revision IDs keep background snapshots separate
from newer edits. The editable image view shares Preview's zoom/pan rendering and
coordinate mapping. Transparent imports are flattened onto white for opaque BMP
output. Focus loss, resize, tool changes and Escape cancel unfinished gestures.

Sketch is a native launcher application with Pen/Line/Box/Pan, 1–32 pixel brushes,
color selection, four undo steps, image import, global menus, file pickers and
verified background BMP copies. Existing files are preserved. New/Open/Close ask
about unsaved work; saving remembers its exact revision while editing can continue.
Preview's Edit in Sketch hands off the current image. Help documents controls and
limits. Layers, text objects, flood fill and selections remain future work.

Host/model/controller tests and address/UB sanitizers passed; the full desktop
suite and production build passed. The VM test independently generated expected
pixels for Pen/Line/Box and compared both saved BMPs exactly, with undo/redo,
editing during save, collision preservation, a safe close prompt, hex clipboard
export into Text, Preview-to-Sketch import, compact drawing/color views and orderly
exit. Normal/compact screenshots were reviewed. This test also exposed and fixed
a one-shot worker race: a process can exit after queuing its reply; the capture
client now drains the pipe and uses EOF rather than a racy liveness probe. The
specific ordering has a host regression test. No hardware reboot or deployment.

### Files copies beyond the editor limit

Files can now duplicate individual files up to 16 MiB, including screenshots and
Sketch drawings. The existing guarded filesystem ABI gains a kernel-owned source
snapshot operation: identity/size/date/type, absent destination and open-file
conflicts are checked before the read/write scope. The background file worker
independently streams source and destination CRC32 checks, syncs, and only reports
success after verification. Text's 64-KiB editing limit remains independent.
An incremental nibble-table CRC preserves the existing protocol checksum.

Kernel/worker/UI host tests cover multi-megabyte and empty copies, stale identity,
collisions, open-file conflicts, bounded sizes, read/write failure, snapshot
release and corrupted readback. Kernel and worker address/UB sanitizers passed.
The checksum matched a separate bitwise implementation on 1,000 randomized inputs.
Production build passed. The throttled-disk VM test copied 4 MiB + 17 bytes while
Text visibly accepted input, verified exact document and binary contents from the
VM disk, preserved an existing destination, renamed the copy and exited cleanly.
Evidence: `desktop_large_copy_smoke.py`. No hardware reboot or deployment.

### Shared Find/Replace and reversible document transformations

The UTF-8 model now supports explicit-range replacement and literal replacement
of the selected match or all nonoverlapping occurrences in the original document.
It validates before publishing, rejects aliasing/invalid text/capacity overflow,
does not re-search inserted text, and records the entire transformation as one
undo step with the prior selection. Identical replacement is a no-op. Case
folding follows Find's existing ASCII-only rule.

The shared modeless search control has an optional Find/With mode. Text exposes
it through Ctrl-H and Search > Find and replace; Enter in With replaces the
selected match, Ctrl-Enter replaces all. Empty With removes matches. Query and
replacement fields have independent clipboard/context routing and lifetime guards.
The panel uses the toolbar space so a document viewport remains visible at the
minimum window size. File operations and saved-revision behavior remain intact.

Host tests cover literal/Unicode/case/overlap/empty/no-op cases, rejected edits,
10,000 independent replacement/undo/redo comparisons, field focus, menu/context
routing, delayed clipboard guards and tiled/stride drawing. Model and control
address/UB sanitizers passed, as did the desktop suite and production build.
The VM exercised single/all/empty replacements, clipboard transfer into With,
undo and redo across saves, no-match behavior, global menus and compact layout.
Both saved documents matched independently expected bytes. Normal/compact
screenshots were reviewed. No hardware reboot or deployment.

### Shared editor scrollbars and offscreen-line rendering

Multiline editors can opt into automatic horizontal and vertical bars. Text uses
both, with shared track paging, captured thumb dragging, release outside, focus/
resize cancellation and an inert intersection. Viewport movement does not edit
or move the selection; typing/navigation reveals the caret. Bars disappear when
content fits, and single-line fields retain their layout. The horizontal scrollbar
configuration is independently reusable by other controls.

Content extents are cached by revision/font. Painting skips complete physical
lines above the viewport, and caret positioning only measures glyphs on its own
line. On the same host, 300 bottom-of-document redraws of a 62,400-byte document
fell from 777,321 to 246,884 microseconds (about 3.1×); old/new pixel hashes were
identical. This is a host rendering microbenchmark, not physical input latency.
The reproducible driver is `tools/bench_edit.c`.

Host tests include 5,000 rotated-axis scrollbar comparisons, extreme coordinates,
selection/revision preservation, paging/drag/cancel/resize, metric invalidation,
small views, clipping and padded/tiled drawing. Widget/editor address/UB sanitizers,
full core/desktop suites and production build passed. The VM dragged both axes on
a 200-line document, typed at the preserved caret, copied the whole document,
resized to minimum size and saved two exact expected files. Screenshots were
reviewed. No hardware reboot or deployment.

## Network Utility and live link reporting

Network Utility adds native Ping and DNS lookup tabs, a validated target editor,
Stop, a wrapped report, clipboard export, shared context/global menus, Help, and
compact layouts. It directly invokes the existing read-only tools without shell
parsing. A reusable client-side command collector drains at most 16 KiB per turn,
retains bounded output, waits for EOF plus the child's exit status, and handles
cancel/timeout/reaping. It is explicitly unsuitable for mutating workers because
cancellation kills its child. Network operations use a 15-second deadline; output
is capped at 4 KiB. Run targets are snapshots, and asynchronous clipboard edits
recheck focus, revision and selection. This app configures no network settings.

The size-aware network status ABI now appends link flags and speed. e1000 reads
its live STATUS register; System distinguishes a disconnected cable from a
retained IPv4 address and shows speed/duplex. Unknown adapters remain explicitly
unknown. System's Diagnostics button opens Network Utility.

Validation: production build and full core/desktop host suites pass. Dedicated
checks cover all 256 low STATUS combinations; command fragmentation, bounded
output, truncation/draining, EOF/exit ordering, timeout/cancel and cleanup;
application prerequisites, target snapshots, malformed output, clipboard races
and compact/tiled rendering. Command collector and complete app/widget checks
pass ASan/UBSan. QEMU passed Ping to its local gateway, localhost DNS, saved report
exports, typing into Text during a pending diagnostic, cancellation, live link
down/up, compact/context/Help, close during work and clean desktop exit. No public
network service is required by the regression. Evidence: `build/review/desktop-network/`.
The laptop has not been rebooted or deployed to; USB network drivers are still
unimplemented pending an adapter target.

## Find Files and shared file-search result model

Find Files searches literal names under a selected folder through a separate
read-only worker. It supports All/Files/Folders, ASCII-insensitive matching,
sortable results, Open, Show in Files, Copy path, context/global menus and Help.
Files' Ctrl-F passes its current folder; Command-Shift-F and the SrvrOS menu
launch a home-folder search. Query/location edits do not relabel an in-flight
snapshot. Stop and window close cancel this read-only worker. Small windows
retain an accessible result row and use wider name/kind columns; resizing keeps
keyboard focus on the results. The first 256 matches are shown with an explicit
limit notice. File contents are not searched, and this is a view of the mounted
VFS rather than an index or atomic filesystem snapshot.

The reusable `desk_file_search` model owns no I/O: canonical UTF-8 path checks,
root boundaries, literal matching and strict binary snapshot validation live in
the toolkit. Worker output is bounded below 64 KiB. Long/noncanonical paths are
counted as skipped, never turned into actionable truncated prefixes. A failed or
malformed scan publishes no results. The desktop global menu now derives its
base count from its actual entries, with a regression requiring every global
command (including Return to console) exactly once.

Host checks cover 10,000 independent literal matches, invalid/duplicate/outside
results, atomic rejection, 256-match limits, disappearing files, fragmented
writes, immutable request snapshots, stable selection on sorting, open/reveal,
clipboard races and compact/tiled rendering. The desktop suite passes; model,
worker and app sanitizer checks pass. Disposable image fixtures now support
multi-cluster directories; 300 fixture files round-trip byte-for-byte. An
additional fsck check exposed an existing limitation in the generated base image
(zero boot-region checksum); the fixture helper is not claimed to make that
synthetic image externally exFAT-conformant. The hardware test flash filesystem
and accepted boot checkpoint remain untouched.

The completed Search VM run passed Files/global launch, case-insensitive names,
folder-boundary isolation, file opening and folder reveal, exact clipboard path
and document exports, 256-of-300 result reporting, kind filters, missing-folder
and empty results, compact keyboard navigation, context menu, contextual Help
and clean desktop exit. Every source fixture was independently checked after
shutdown. Screens were reviewed at normal and 280×240 client sizes. Evidence:
`build/review/desktop-find/`.

## Consistent shared path handling

Files, Find Files and the reusable Open/Save picker now use `desk_path` for
lexical UTF-8 normalization, child joins and parent navigation. Repeated slashes,
`.` and `..` behave consistently; parent traversal stops at root. Invalid or
oversized results leave destination buffers untouched, including overlapping
input/output. Search sends and displays the canonical location used by its
worker. This does not claim symbolic-link resolution.

The 10,000 independent component-stack oracle, UTF-8/bounds/aliasing checks and
ASan/UBSan pass. Files, Search, picker model and picker-controller checks pass.
The production build and full Search VM workflow pass with noncanonical paths
entered in both Search and every Save picker; exact saved paths and document
contents are unchanged. Evidence: `build/review/desktop-find/`.

## Side-by-side workspace arrangement

The desktop's Arrange menu provides Tile left, Tile right and Center. The matching
shortcuts are Command-Alt-Left/Right/Up; Command-Alt-Down zooms or restores. Tiling
uses the available workspace between menu and dock and respects each client's
surface limits. Switching sides retains one original restore frame. Center uses
the original normal size, and restored windows can be dragged/resized normally.
Repeated same-size placement avoids unnecessary surface generations. Moving a
captured window cancels the old pointer capture even when its dimensions match.

Pure geometry checks cover thousands of screen sizes, narrow-display rejection,
non-overlap, borders/dock clearance and bounds. Session tests cover original-frame
preservation, repeated placement, centering, shortcut modifiers/repeats, capture
cancellation and the maximum 62-entry menu with all eight windows and four app
headings. Arrange is reserved against application menu spoofing. The full core
and desktop suites pass; scene geometry/rendering passes ASan/UBSan. QEMU passed
side-by-side typing and exact saved contents, menu and shortcut routing, repeated
resize generations, centering and restore at both 1280×800 and 800×600. Screens
were inspected at both resolutions. Evidence: `build/review/desktop-arrange/`
and `build/review/desktop-arrange-small/`.

## Desktop lifecycle, capacity and recovery acceptance

The six-round disposable-VM soak passed 72 application lifetimes. Each round
opened all twelve apps, tiled/restored each window, decoded a 1024×768 image,
ran Find Files and exited a real Terminal shell. After init reaped exited
workers, only the baseline init/supervisor processes remained. Live kernel heap
was **4096 bytes** at baseline and after every round. After the first warmup
round, all six samples had **972716 in the PMM reported used-frame counter**,
with unchanged
per-tag counts (including user frames and page tables). This counter includes
reserved address ranges; its trend is a cleanup check, not a usable-RAM size.
The initial increase is
retained heap capacity and cached kernel stacks; it does not grow each round.
The test's bounds remain 64 KiB live-heap spread and 128 physical frames, rather
than assuming identical allocator behavior on every build. Actual samples are
in `build/review/desktop-lifecycle/samples.json`.

A separate 512 MiB VM passed eight simultaneous windows, refusal of a ninth,
reuse of the released slot, the full Window menu, exact saved text, and verified
screenshot output while all eight slots were occupied. Normal exit left no GUI
processes. Visual inspection caught a missing checkmark glyph; menus and
checkboxes now share a clipped native 11×8 checkmark. A successful app launch
also clears the obsolete capacity/failure notice. Targeted launch tests cover
full capacity without spawning, failed exec, and successful slot reuse. The
final capacity VM rerun passed after both fixes. Evidence:
`build/review/desktop-capacity/`.

Emergency recovery passed three cycles with unsaved Text, a live shell running
`cat` in a PTY, and a pending Ping diagnostic. Every cycle reclaimed the GUI,
PTY children and diagnostic; USB typing worked in the restored console. The
same init, supervisor and web-server PIDs survived, and exact HTTP response bytes
were fetched both during the desktop workload and after emergency exit. A fresh
desktop then saved a document whose bytes were checked outside the VM. This
covers cancellation of read-only work, not power loss during accepted writes.
Evidence: `build/review/desktop-recovery/`.

The final renderer/arrangement workflow also passed at **1440×900** (the Air's
panel size) and **1920×1080**, alongside the earlier 1280×800/800×600 checks.
Host widget/menu/popup checks and the full desktop suite pass. The OS image
build passes. Physical gesture feel, USB-media removal and laptop power controls
still require their own hardware acceptance; none is inferred from these VMs.

## Toolkit boundaries and current documentation

The toolkit guide now maps controls/models to their public headers and consuming
apps, and gives the ownership/event/worker/render lifecycle for a new native app.
`test-widget-boundary`, included in `test-widgets`, inspects archive imports and
rejects dependencies outside renderer primitives and pure libc helpers. Both
host and freestanding archives pass (271 definitions, respectively 24 and 21
external helpers). Injected allocation and OS-call imports are correctly rejected.
This is a link dependency guard, not a proof about arbitrary C behavior.

The native desktop guide provides a current app inventory, shortcuts, workflows,
explicit limits and VM commands. README and the original roadmap point to it;
the chronological journal remains available without presenting earlier
limitations as the current feature set. No hardware boot entry or accepted
checkpoint was replaced.

A final read-only Ubuntu inventory identified the battery as ACPI0002 /
`acpi-sbs` beneath the EC-backed ACPI0001 SMBus controller, and the display as
Intel Broadwell HD 6000 with `intel_backlight`. No USB Ethernet adapter is
present. The Linux capture tool now records an explicit power/backlight
property allowlist without serial numbers or setting changes. The
[laptop-service follow-on](macbook-air-laptop-services-2026-10-18.md) records
evidence, source references and acceptance order; no unverified power driver
was enabled.

The final runtime build passed a second six-session / 72-application soak, now
requiring the entire post-session process set (PID, executable and state) to
match the initial baseline. All six rounds again returned to 4096 bytes of live
heap, with identical post-warmup PMM counters. The updated capacity check also
requires exact process-set restoration and passes. Final soak evidence is in
`build/review/desktop-lifecycle-final/`; the earlier six-round samples are retained
separately. Across both completed soaks, 144 application lifetimes were exercised.

## Frozen VM candidate

Candidate **c126e61fa08f382d**, source **06f60c8**, is preserved independently of
`build/` at `$HOME/.local/share/srvros/vm-candidates/desktop-20261018-c126e61fa08f382d/`.
It contains the ISO, exFAT image, kernel, initramfs, manifest, logs, memory/process
samples and selected screenshots. Artifact copies were checked against SHA-256
hashes. The exact packaged ISO and a disposable copy of its exFAT image then
passed the eight-window workflow, launch refusal/reuse, saved document bytes,
screenshot readback and exact process-set cleanup. The manifest includes that
additional packaged-boot evidence.

Every file listed in the accepted hardware checkpoint `f05dfcc01c28dc85` still
matches its original manifest hash. The candidate is VM-tested and has not been
installed to GRUB or accepted on the physical laptop. All test VMs have stopped;
Ubuntu remained running throughout this development session.


## Physical acceptance trial prepared after the autonomous window

The user authorized a laptop reboot for the expanded native desktop. Added an
opt-in `ram-usb-v17-desktop-37d8c5d0` boot profile, with RAM root, AHCI disabled,
Apple gestures and the existing UUID-selected USB workspace. It starts `deskd
--hardware-trial` at the new Help checklist; the profile refuses to launch if the
workspace is unavailable. Closing the desktop flushes/unmounts USB and leaves the
monitor visible indefinitely. There is no automatic reboot or inferred pass.

Staged release: `e9dcca07595df573`. The frozen VM candidate is untouched; this
trial adds only its boot wrapper and checklist. The accepted hardware checkpoint
`f05dfcc01c28dc85` was reverified and is the installed current release before this
deployment, so the installer will retain it as previous.

Validation: production build and Help content/layout suite passed; all 17 boot
installer tests passed. The exact staged GRUB/UEFI loader, kernel and initramfs
passed the expanded hardware-boot smoke with a disposable UUID-matching USB
volume. It opened Laptop trial, typed `Laptop trial AaZz09 saved on USB.` in Text,
saved through the GUI, returned to the monitor and cleanly unmounted. Independent
file-byte comparison and offline exFAT check passed; USB console input worked
afterward. Two initial harness waits were corrected to avoid trace-only and
interleaved serial markers; these were harness issues, not guest acceptance.
The physical session and user's feedback remain pending.


## Laptop trial feedback and scroll-pressure correction

The user returned from `e9dcca07595df573` and reported that generally everything
worked great. The initial Help instructions closed unexpectedly, possibly around
a second trackpad contact, and one other application closed once. The events were
not reliably reproducible. Pointer movement was functional but required too many
swipes. This is positive provisional hardware feedback, not acceptance of the
intermittent closures. The accepted recovery checkpoint remains unchanged.

Inspection found a reproducible compositor failure mechanism: every wheel-only
event also enqueued a stationary hover. Alternating SCROLL/HOVER prevented the
adjacent-scroll coalescer from reducing traffic; a full 64-packet client queue
forced the app to close. The actual physical events have no retained trace, so
this is a plausible cause rather than proof of their origin.

Wheel-only events now omit redundant hover. Adjacent scrolling coalesces only at
the same content coordinates and configuration generation. At queue capacity,
replaceable motion/scroll packets may be dropped; ordered key/button/focus packets
can evict a queued motion packet while preserving the order of remaining input.
Press/release/cancel packets are never classified as disposable. Queue pressure
logs once per client. A queue containing only ordered events retains the existing
forced-close limit, now with an explicit reason in diagnostics; stalled clients
and protocol failures retain their existing recovery behavior.

The session regression floods a transport-blocked client with alternating scroll
and hover, verifies it survives at bounded capacity, enqueues a release and drains
it successfully. It also checks 1,024 same-target wheel increments coalesce and
that a changed target stays separate. The Help VM regression covers its existing clipboard, navigation, save and exit
checks. An attempted USB mouse wheel burst could not exercise scrolling: the
current generic USB mouse handler ignores wheel data, so no physical-trackpad
coverage is claimed. The pressure regression injects at the compositor boundary.

### Pointer acceleration follow-up

The current trackpad converter uses fixed scaling with fractional remainders,
without velocity-dependent gain. Add a bounded gain curve based on monotonic
sample intervals, keeping slow movement precise and increasing travel for fast
swipes. Keep scrolling independent, rebase on contact-count changes/lifts, and
expose sensitivity in Settings. Validate diagonal consistency, sample-rate
variation, reversals, device cancellation and no jumps after pauses; final curve
selection requires the laptop. This is queued after closure reliability, not
implemented or hardware-accepted in this correction.

Validation outcome: production rebuild, the complete desktop host suite, the
focused pressure regression, and the Help QEMU workflow passed. The latter also
verified the saved clipboard document byte-for-byte. A final logging-only cleanup
preserved the established retirement marker used by other VM harnesses; the
production build and pressure regression passed again afterward. No hardware
installation or reboot was performed for this correction.


## Trackpad controls, persistent laptop preferences and power groundwork

The scroll-pressure correction remains queued for the next physical trial.
This round adds velocity-dependent pointer gain at Apple trackpad sample receipt,
before input coalescing. Slow motion keeps fractional precision; fast swipes get
up to 3x extra travel. Five base speeds and an acceleration toggle are exposed
through the shared Settings slider/check controls. Two-finger scrolling is
independent. Contact/mode changes, cancellation, setting changes and implausible
coordinate jumps rebase motion history; invalid or paused clocks cannot amplify
motion. The native desktop defaults to speed 3 with acceleration on. Console and
legacy input retain fixed scaling. The actual gain curve still needs laptop
feedback; VM pointer tests do not simulate the Apple device.

Only the registered compositor can apply the bounded kernel input policy; owner
cleanup restores fixed mapping. Preferences migrate valid v1 files to v2 when
next saved and retain the existing debounce, background verification and conflict
handling. The hardware-trial desktop uses `/work/srvros-work/desktop.conf`, so its
prepared USB workspace can carry settings across boots. Other launches keep
`/fat/home/desktop.conf`. Save status remains explicit, and verified saves do not
claim power-loss atomicity.

Host regressions cover the gain curve (including its middle range), diagonal
consistency, different sample rates, reversals, speed levels, pauses/cancellation,
scroll independence, compositor access/cleanup, preferences migration and widget
capture. The complete `make all test-core` run passed, including the Help content
and layout suite; evidence is `build/review/desktop-next-blocks-build-tests.txt`.
The Settings VM workflow passed saved clock/color/text/pointer changes and a
fresh desktop-session reload, with screenshots under
`build/review/desktop-settings/`. Its pointer remains QEMU's ordinary mouse, so
this validates the settings path rather than physical acceleration.

The on-screen Laptop trial guide now explicitly asks for repeated fast scrolling,
adding/lifting the second finger, keeping Help and another app open, comparing
pointer speeds and checking saved settings after the following boot. These checks
remain **pending hardware**. No physical boot installation or reboot was performed.

Separately, read-only firmware capture resolved the laptop's EC ports, register
layout and conditional battery namespace. A bounded callback-driven EC byte
transport and fault-path tests are committed as groundwork. It has no hardware
binding or caller; battery, brightness and suspend remain unavailable. See the
[laptop-service evidence and next steps](macbook-air-laptop-services-2026-10-18.md).


Staged candidate **3ebe5b991358262b** passed the exact GRUB/UEFI hardware-profile
rehearsal on a disposable USB image. The first VM saved a Text document plus
speed 5/acceleration off, then returned to the monitor and unmounted cleanly.
A completely new VM booted the same RAM-root release and USB image; changing
only the acceleration toggle preserved the loaded speed 5 and wrote acceleration
on. Independent file-byte checks and offline exFAT checks passed after both
sessions. Evidence: `build/review/desktop-workspace-reboot-vm.txt`,
`hardware-grub-first-serial.log`, `hardware-grub-serial.log`, and the
`workspace-settings-{saved,restored}.png` screenshots in that review directory.
An initial harness timeout mixed byte offsets with decoded GRUB Unicode text;
the harness was corrected, and the complete two-boot test passed on rerun.

The candidate includes the previous scroll-pressure correction. It is staged
only: the installed physical trial remains **e9dcca07595df573**, with accepted
recovery **f05dfcc01c28dc85**. The frozen earlier VM candidate remains untouched.
Next physical gate: confirm no spontaneous app closures during gesture bursts,
choose a comfortable pointer speed, type/drag under I/O, and confirm USB settings
on a subsequent boot. Next power block: resolve EC/AML initialization and event
ownership before enabling a scheduled SMBus/SBS provider.


## Persistent desktop workspace, session logs and laptop preflight

The USB laptop trial now makes its prepared workspace the default Home across
Files, document pickers, Text, Terminal, Find, Preview and Sketch. Screenshots
and preferences use that same persistent volume. Normal desktop sessions retain
`/fat/home`. No new filesystem operations were added to UI input handlers.

The kernel retains a 64 KiB console tail and writes verified, flushed snapshots
under a fresh `srvros-work/Logs/session-…` directory for each trial. Boot,
alternating ten-second checkpoints and pre-unmount exit snapshots preserve
release identity, application close/exit reasons and laptop preflight readings.
The writer is an ordinary scheduled worker using the existing storage service
gate, so waiting occurs through the established responsive I/O path. Failed
writes remain visible and disable periodic retries. This is bounded tail history,
not a complete transcript or power-loss-atomic crash journal.

`tools/hardware_desktop.py` collects and verifies these logs after Ubuntu returns,
records SHA-256 hashes and flags attention markers. It never labels a collection
as a hardware pass. The USB source is read-only during collection; local outputs
are separate per invocation.

The v18 boot profile records its release and enables a read-only power/backlight
preflight. Exact captured firmware and PCI/resource checks gate all register
reads. This records inherited SCI/EC/GPE state and the active PWM mode without
changing ACPI ownership or brightness. The EC-backed SMBus read-word protocol
engine has delayed-response, overlap, timeout and fault tests, but no live
hardware binding. Battery/AC reporting and brightness controls remain pending
firmware initialization/event ownership and the physical preflight.

Validation so far: `make all test-core`, desktop host tests, bootlog/logger
fault-path tests, EC-SMBus tests, synthetic and actual captured ACPI-table tests,
and Python boot-profile/collector tests passed. Build evidence is
`build/review/persistent-laptop-build-tests.txt`; focused evidence is
`persistent-desktop-host.txt`, `laptop-resources-host.txt` and
`persistent-boot-tool-tests.txt` in the same review directory.


The expanded VM check found an existing shell issue: builtin `pwd` ignored its
stdout redirection even though Terminal's inherited working directory was correct.
`pwd` now honors overwrite/append and returns failure for an unwritable destination.
The VM regression checks those results using files on the disposable USB volume.
An earlier harness attempt also used Ctrl-W rather than Terminal's Command-W
close shortcut; that harness error was corrected. Failed-attempt logs remain in
`build/review/` instead of being counted as passes.


Final candidate **a91913d3db4219e8**, built from source commit **8a5f6c8**, passed
the expanded exact GRUB/UEFI two-boot rehearsal. The first session saved default
USB Text and screenshot files, verified Terminal's workspace directory through
`pwd` overwrite/append and error-status files, and saved periodic/final logs.
The second fresh VM retained the document/screenshot bytes and pointer speed,
changed the acceleration toggle, and wrote a distinct log session. Both unmounts
and offline exFAT checks passed. The Ubuntu collector copied and verified both
sessions with exit snapshots and no attention markers. Evidence is
`build/review/persistent-desktop-two-boot-vm.txt`, the two hardware GRUB serial
logs, and `build/review/desktop-collections/collection-bw9a5_9i/`.

Boot installation now supports `--keep-previous`, which verifies and retains the
existing recovery entry instead of rotating it out. The installer regression
covers retention, corrupt recovery rejection and unchanged state on rejection.
The accepted **f05dfcc01c28dc85** recovery and its independent backup both remain
available. Physical gesture/acceleration acceptance and actual laptop preflight
values are still pending; VM success does not settle those hardware checks.


Candidate **a91913d3db4219e8** is now installed as `srvros-current`;
**f05dfcc01c28dc85** remains `srvros-previous` through `--keep-previous`.
Both installed payload manifests were reverified after GRUB regeneration. The
prepared USB workspace is present. Ubuntu remains the default and no reboot was
requested. Candidate evidence is retained under
`build/native-desktop-trials/a91913d3db4219e8/`. Awaiting the user's availability
for the physical trial; battery/brightness providers remain unimplemented.


## Unattended preflight follow-up

The user authorized the next reboot on the expectation that it returns by itself.
The interactive v18 desktop trial intentionally waits for manual exit, so a
separate v19 laptop-service capture now handles that request. It collects the
same read-only diagnostics, writes verified boot/exit snapshots, flushes and
unmounts the USB workspace, then reboots. Any save/unmount/reboot failure halts
with the error visible. Physical gesture/acceleration acceptance stays pending.
The exact boot-profile VM regression covers successful file persistence plus
reset, and a missing-workspace run must remain halted without resetting.


Unattended release **8d348e2935c28972** passed the exact GRUB/UEFI VM capture:
boot/exit log bytes and release identity survived, offline exFAT was clean, and
the guest reset automatically. A separate missing-workspace VM remained halted
without rebooting. Production build and 20 boot-tool tests passed. The release
is installed as current, with **f05dfcc01c28dc85** retained as recovery. The user
has authorized the one-time automatic capture boot; physical results are pending.
Evidence: `build/native-desktop-trials/8d348e2935c28972/`.


Physical capture **8d348e2935c28972** is now recovered. The single new session
`session-00065e22393276ae-00` has verified boot/exit files, the expected release,
the diagnostics-collected marker and zero collector attention markers. Its exit
snapshot precedes final flush/unmount/reset, so those later actions are not
independently present in the saved transcript. No desktop ran; gesture and
acceleration acceptance remains pending.

The exact firmware/resource checks succeeded on hardware. SCI_EN and the EC GPE
enable were both zero. Backlight control was PCH override (`0xc0000000`) with CPU
PWM disabled and duty 388 of period 2777. The next brightness implementation can
target this observed mode while preserving period/control bits and validating
readback. Battery/AC reporting needs ACPI initialization and SCI/EC event ownership
before binding the tested transport. Full readings and implications are recorded
in `docs/macbook-air-laptop-services-2026-10-18.md`.

Physical evidence is archived under
`build/native-desktop-trials/8d348e2935c28972/physical-collection/`; the host pending
record now says `capture-recovered`. No new installation or reboot was performed
during collection.


## Brightness control implementation

The observed PCH override mode is now connected to a bounded brightness backend
and a Settings slider below Trackpad. The backend is enabled only by the new v20
interactive laptop profile after exact firmware/resource/chipset checks; normal
and automatic read-only capture profiles retain their earlier behavior. Startup
and queries perform no writes. Explicit 5–100% requests change only duty, preserve
the period and control state, and require matching readback. Unexpected state
makes the provider unavailable without retrying writes or switching modes.

The registered compositor exclusively owns the syscall. Protocol v8 adds serial-
tagged brightness messages; only focused Settings may change brightness, replies
are retried, stale replies are ignored, and Settings refreshes on focus and shows
unavailable after a timeout/failure. Changes apply to this session, independently
of the persistent preferences file. The existing shared slider, keyboard controls,
capture handling and scrollable layout are reused. The hardware guide includes
20%/50%/comfortable-level checks while typing and moving the pointer.

The full production build and `test-core` passed. Focused tests cover all allowed
percentages, unchanged-duty suppression, invalid requests, mode/period drift,
readback failure and disabled-provider behavior; GUI ownership/focus checks;
reply retry; and Settings stale responses, timeout, capture and send failure.
Evidence: `build/review/brightness-build-tests.txt` and
`brightness-final-host-tests.txt`. Two host-test stub declarations needed fixes
while adding the new logging path; production code compiled successfully.
The log collector now retains provider/brightness messages and highlights failed
change requests. Both VM checks passed: the Settings workflow (including disabled
brightness on unsupported QEMU hardware), and the exact v20 GRUB/UEFI profile
across two fresh boots, saved preferences, documents, screenshots, durable logs
and clean USB unmounts. The revised Settings layout was visually inspected.
Candidate **0f64e8a09d6c965e** and its evidence are archived under
`build/native-desktop-trials/0f64e8a09d6c965e/`. Physical brightness changes remain
unverified; the next trial also retains the pending gesture/acceleration checks.

Candidate **0f64e8a09d6c965e** is installed as current; accepted recovery
**f05dfcc01c28dc85** is retained. Both installed payloads passed hash/config
verification. Ubuntu remains the default. No reboot is scheduled: this trial
needs a person to observe brightness changes and exit the interactive desktop.


## Returned brightness/desktop trial and maximize correction

Physical release **0f64e8a09d6c965e** returned with the user's report that things
generally worked well and files were saved. Session `session-00065e228bee957b-00`
has boot, periodic and exit snapshots, `deskd: exited`, matching release identity
and zero collector attention markers. Both screenshots were copied locally.
Evidence: `build/native-desktop-trials/0f64e8a09d6c965e/physical-collection/`.
The provider was ready and Settings queried 14%, but no brightness change requests
appear in the recovered logs. Physical slider adjustment remains unverified.

The second screenshot confirms Preview was marked maximized but its client area
was capped at 960x640, leaving the frame centered at 239,52 with size 962x685.
This was a shared desktop limit, not an image-fit problem. The shared surface
bounds now cover 1440x900 and the kernel derives its pixel limit from the same
constants. Maximize accounts for frame borders and retains an eight-pixel side
margin, menu bar and dock: frame 8,52,1424,768 on the target. Restore preserves
the original frame. Tiling uses the same enlarged limits.

The existing preallocated double caches remain bounded, now about 79 MiB total
for eight windows (previously 37.5 MiB); each client canvas is at most 4.94 MiB.
No per-frame allocations were added. Larger displays still respect this explicit
client-size cap. Trace logging now records the first committed frame after each
configuration, enabling tests to verify the client resized with its frame.
The production build and full `test-desktop` passed. QEMU at 1440x900 passed
Preview titlebar maximize/restore with matching client commits, image formats,
fit/zoom/pan, failure retention and Files association; the arrangement suite
passed repeated tile/restore/center/maximize cycles and exact saved documents.
The maximized Preview screenshot was visually inspected. Candidate
**158b693c01837e7b** is staged with evidence in
`build/native-desktop-trials/158b693c01837e7b/`. No replacement hardware image
has been installed and no reboot is scheduled.

The 512 MiB QEMU capacity regression also passed with all eight client slots:
ninth launch refusal, slot reuse, exact saved text, screenshot creation and
process cleanup back to baseline. Evidence is archived with the staged release.


## Standard titlebar controls across apps

The user clarified that maximize did not appear available outside Preview.
The old titlebar drew very small glyphs and gated minimize/maximize drawing on
the application ID, while input handling used desktop mode. This did not prove
why a particular hardware window looked incomplete, but the two conditions were
inconsistent. Standard desktop mode now determines control availability for
every window, independently of app identity. Minimize, maximize/restore and close
have distinct outlined buttons and stronger icons. Drawing and hit testing use
one shared geometry helper. The global Window menu now says Maximize instead
of Zoom, and Help explains the button order and restore behavior.

The production build, desktop host suite and Help tests passed. The scene test
now checks titlebar hit regions and uses the shared surface bounds for placement
assertions. Its missing protocol-header build dependency was added so changes
to those limits cannot leave this test using an old binary. A new VM regression
clicks maximize/restore/close in each of the 12 application types, checks exact
frame geometry and matching client surface commits, and saves normal/maximized
screenshots. All 12 apps passed in the 1440x900 VM: Text, Files, Calculator,
System, Settings, Terminal, Preview, Help, Clock, Sketch, Network Utility and
Find Files. Normal Text and Settings screenshots were visually inspected.
Candidate **c8715a60f16075dd** is staged; evidence is archived in
`build/native-desktop-trials/c8715a60f16075dd/`. It supersedes the staged
maximize-only candidate. No new installation or reboot was performed.
