# Native desktop guide

This is the current `deskd` desktop, implemented with the shared native widget
library. The older `gui`/`displayd` applications and the browser design study are
historical prototypes. Start the current desktop from the SrvrOS monitor:

```text
run /fat/bin/deskd
```

For an explicitly enabled Home session with battery monitoring, use the full
battery runtime and `desktop battery` after mounting Home. Use **SrvrOS →
Return to console** for cooperative shutdown with Save/Discard/Cancel decisions.
**Ctrl+Alt+Esc** is an emergency exit and can discard unsaved edits; it still
runs the owned cleanup path. Home stays mounted for reopening. Default `deskd` startup does not start firmware work. See
[ordinary Home startup](battery-home-lifecycle.md#opt-in-ordinary-home-startup)
for the v54 boot profile and validation limits. Click the battery label to open
System. Fresh discharging readings at 20% and 5% add low-charge notifications
without stealing focus; stale/unavailable readings do not trigger alerts.
Fresh AC power or recovery above 25% rearms the low warning; recovery above 10%
rearms the critical warning. Notifications do not automatically suspend or power
off the machine. The combined v55 provider profile is described in the
[device parity ledger](device-parity.md).

Automated evidence is from disposable QEMU VMs, with repeated physical trials
on Air7,2. The October 23 v55 trial accepted combined Home/battery, playback,
brightness/volume, live sensors and save/exit/reopen. See the machine record for
the exact payload and remaining listening/gesture checks. Physical two-finger pressing now selects secondary click at press;
named secondary-click/drag/scroll checks were accepted in the later follow-up.
Tap settling is tester-accepted in PR88 with an incomplete final diagnostic
receipt; neither establishes every gesture combination. Secondary tapping is separate.
The October 19 interactive round reported generally good behavior and recorded
no recurrence of the earlier System input-timeout closure. That earlier cause
is not conclusively established. The Terminal backspace correction passed its subsequent hardware
retest, alongside explicit Text Ctrl+C/V/A validation. Host regressions now cover input and control
packet delivery deadlines; clients get a delivery attempt before queued time is
blamed on them, and genuinely unresponsive clients still have bounded cleanup.
See the [machine record](machines/macbookair7-2.md) for precise
acceptance and the [development journal](desktop-progress-2026-10-19.md) for history.
New machine agents should follow the [Ubuntu setup guide](ubuntu-device-agent-setup.md).

![Native Text and Files at 1440×900, arranged side by side](assets/screenshots/current/desktop-files-text.png)

[Current gallery and capture provenance](screenshots.md). Later unattended native
captures measured idle behavior and diagnostics; they do not replace interactive
acceptance of new input changes.

## Display scale

The default remains 1× on every machine. An explicit integer scale can be selected
when starting the desktop:

```text
run /fat/bin/deskd --scale=2
```

At a 2880×1800 boot framebuffer this gives a 1440×900 logical desktop. Windows,
controls, pointer movement, hit testing and damage use logical pixels; the final
copy expands each pixel to a 2×2 block. Client surfaces and window-cache budgets use logical dimensions. This is integer enlargement of the existing
raster toolkit, not new higher-resolution font rendering or GPU acceleration.
The mode requires at least 640×480 logical pixels; other scales are rejected.
`--scale=1` selects the existing behavior. It can accompany `--apps-trace` or a
trial mode. There is no automatic DPI inference or persistent scale setting yet.

Saved screenshots contain the logical canvas (1440×900 at the example mode),
including its cursor. Expanding that BMP by 2× reproduces the displayed pixels.
This keeps captures within Preview's current 8 MiB file limit. Serial diagnostics
record physical dimensions, logical dimensions and scale. VM evidence does not
accept the firmware-selected display or GPU on an additional physical Mac.

The unattended `--idle-capture` mode is reserved for the v25/v26 hardware diagnostic
profiles. It uses the prepared USB workspace, samples 60 seconds after startup,
saves a screenshot and exits. Normal launches and the interactive hardware trial
retain their existing manual exit behavior. See the [device-agent guide](ubuntu-device-agent-setup.md).

## Repainting and covered windows

The compositor sorts the window stack once per immutable frame and reuses that
order for each damage rectangle. It can skip a lower window when both its
clipped frame and shadow are fully covered by higher opaque window pieces.
The two pieces stay separate: their bounding union contains unpainted corners.
Partially covered windows retain normal back-to-front painting. This optimization
allocates no visibility regions and changes neither damage generation nor frame
acknowledgements, cache ownership, output scaling or framebuffer copying.

The internal unculled path remains a pixel oracle for deterministic/randomized
host fixtures. Those compare identical scenes, clips and caches, including 128
records, shadow holes, overlays and malformed small diagnostic frames. Host
benchmarks report pixel writes and timing distributions for the same workload;
they do not measure physical input latency, display scanout or power use.
Opaque replacement pixels/shadows are part of the proof: translucent windows or
decoration outside the frame require revisiting it before enabling occlusion.

## Desktop controls

The top bar contains the SrvrOS menu, the focused application's menus, Arrange,
Window, and the clock. The dock opens or focuses common apps. Command means the
Mac keyboard's Command key, or the Meta/Windows key in a VM.

| Action | Shortcut or control |
| --- | --- |
| Find an app or existing window | Command-Space; type a name and press Enter |
| Search file names | Command-Shift-F |
| Cycle windows | Alt-Tab / Alt-Shift-Tab |
| Navigate menus | F10, arrows, Enter, Escape |
| Tile left / right | Command-Alt-Left / Right |
| Center at the original normal size | Command-Alt-Up |
| Maximize / restore original geometry | Command-Alt-Down |
| Save a screenshot | Command-Shift-3 |
| Open Alerts | Command-Shift-N |
| Open contextual Help | F1 |
| Contextual actions | Right-click, Control-click, or Shift-F10 in supported controls |
| Close the active app | Command-W; apps guard unsaved work |
| Return to the monitor | SrvrOS → Return to console |

The launcher lists open windows as well as app launch entries. Select an existing
window to resume it; choose the app entry to open another instance. Window and
child registries grow under a metadata budget rather than a fixed window count.
Memory, processes and worker resources can still refuse an opening request.

The compositor now allocates buffers for live window sizes instead of reserving
eight full-screen cache pairs at startup. Published and private buffers share a
96 MiB pixel-cache budget. Failed growth retains the old frame; clients can
refuse a resize and return to their last working size without discarding document
state. Closing a window releases its caches. Oversized Window menus scroll with
the wheel, and keyboard selection remains visible.

Kernel GUI records are demand-allocated: 2 MiB queue storage, 256 KiB surface
metadata and 256 MiB surface pixels, with the existing 64 process
slots. Workers and terminal shells consume extra resources independently of
window count; multiple Text documents can share one application process.
See [window capacity and ownership](window-capacity.md) for the matched desktop protocol,
allocation guarantees and exact validation. Sixteen-window persistence and
resize/lifecycle VMs pass. Air7,2 physically accepts sixteen-window opening,
saved-file integrity and clean exit at `bf68e4e9b88b8ae7`; native pressure
injection/cache totals were not measured in that round.

Tile left/right retain the original frame across side switches. Center returns
to the original normal dimensions at the middle of the workspace. Restore size
returns to the original position as well. Tiled windows must be restored or
centered before free dragging/resizing. Narrow screens disable tiling when two
minimum-size clients would not fit.

The clock opens a month calendar with Today and keyboard navigation. Alerts
retain the current session's notifications. Timer and screenshot banners do not
steal keyboard focus. Screenshots include the pointer and are saved as verified
BMP files under the session Home’s `Screenshots` folder; the completion banner opens it.
Home is `/fat/home` normally and `/work/srvros-work` for the prepared USB laptop trial.

## Applications

| Application | Current useful workflows |
| --- | --- |
| Files | Places and location browsing; Back/Forward; sortable details; create folders; open, rename, duplicate, inspect, or permanently delete selected items |
| Text | Open/edit/save UTF-8 text; selection and clipboard; Find/Replace and Replace All; multi-step undo/redo; draggable horizontal and vertical scrollbars |
| Terminal | A real PTY-backed shell; scrollback, selection/copy, foreground signals, resize, and independent sessions |
| Preview | Open PNG/JPEG/BMP; fit, zoom and pan; open the image in Sketch |
| Sketch | Pen, Line and Box tools; brush size and RGB color panel; undo/redo; import images and save verified BMP copies |
| Find Files | Background name search under a folder; kind filters; sortable results; open, reveal in Files, and copy paths |
| Calculator | Expressions, parentheses, keypad, explicit errors, copied results and calculation history |
| System | Live process and memory information; supported adapter inventory, active interface, address, link state, speed and duplex; open diagnostics |
| Network Utility | Background Ping and DNS lookup; Stop; copied reports |
| Settings | Persisted desktop color, document text size, clock display/UTC offset and trackpad speed/acceleration |
| Clock | Countdown and stopwatch; pause/resume, laps and copied times; completion notification |
| Help | Offline topics, topic search, Back/Forward, keyboard navigation and copied pages |

App menus display the relevant shortcuts. Text uses Ctrl-O/S/F/H for Open, Save,
Find and Replace; Files uses Ctrl-L for location, Ctrl-F to search the current
folder and Ctrl-I for item information. Terminal reserves Control keys for the
foreground program: Ctrl-C interrupts it, Ctrl-Shift-C/V copy/paste, and
Shift-PageUp/PageDown scroll. A shell that exits leaves its terminal output
visible until the window closes.

## Switching windows and using the keyboard

Hold **Alt** and press **Tab** to open the window switcher. Keep Alt held and
press Tab again to advance, or Shift-Tab to reverse; release Alt to activate the
selected window. Escape cancels without switching. Quick Alt-Tab returns to the
most recently used window. The list remains in the same order while held; new
windows do not steal the selection, and retired identities cannot activate a
replacement window. Minimized windows can be restored from the list. Owned
modal dialogs appear through their parent document rather than as extra tasks.

**Command-grave** cycles the current application's windows in creation order;
Command-Shift-grave reverses. **Command-M** minimizes a window and its owned
dialog together. Minimizing or closing the active window selects the most recent
visible window, leaving other minimized windows hidden. Closing a modal returns
to its own document. With no visible windows, desktop keyboard commands remain
available to restore one.

**F10** or **Control-F2** opens the global menu bar (Fn may be needed). Left/Right
changes menus; Up/Down, Home/End choose commands; Return activates and Escape
cancels. Tab/Shift-Tab still navigate app controls, and ordinary app shortcuts
retain their existing behavior outside the switcher.

The untimed **Window navigation trial** in Help combines these checks with
partial offscreen movement, dock restoration, file saves and normal return. The
explicit `--desktop-navigation-trial` staging profile opens it automatically in
the combined Air7,2 battery/Home session. It does not run an automated acceptance
or reboot timer. Hardware acceptance remains pending until the tester reports
against that staged release.

## Moving windows and using the dock

Ordinary windows can extend past the left, right and bottom edges. Movement
keeps at least 160 horizontal pixels of title bar reachable below the menu and
32 pixels above the dock's reserved area. Content is clipped to the display;
moving does not resize the client surface. Arrange → Center / Command-Alt-Up
brings a window fully into the work area. Maximize, restore and tiling retain
their existing size/admission contracts. New windows and owned dialogs still
start within the work area.

The dock keeps six pinned apps, followed by other applications with admitted
main windows. Their entries remain while minimized and disappear after the last
main window retires. A green running bar, active background and multiple-window
count show state; an outline bar means every main window is minimized. Dialogs
activate their owner's entry without inflating its document count. The dock
fits entries to the screen and clips long labels; the count displays `99+` for
larger groups, without imposing a window limit.

Click an inactive app to restore/activate its most recently used main window.
Further clicks on an active app cycle its windows, including minimized ones.
Right-click or Control-click an entry for its searchable window list and New app
command. The list uses the existing dynamically sized launcher and stable
window identities. Choosing a parent with an owned dialog returns focus to that
dialog. A closing window is not activated, and a fully closing group does not
launch a duplicate process. The dock owns clicks/scrolling over its panel, so
windows beneath it do not receive those events. Pin customization and persisted
dock ordering are outside this block.

## Documents and background work

Text supports multiple documents in one application process. Choose File → New
Window or press Ctrl-Shift-N; each window has its own undo history, clipboard
selection, file path and unsaved-close dialog. Ctrl-N creates a fresh document
in the current window after resolving edits. Closing one window preserves the
others; the last close waits for owned file work and worker retirement.

Files also supports File → New window / Ctrl-Shift-N in the same process, starting
at the current folder. Each window has independent navigation, history and
selection. Ctrl-W closes that window; Ctrl-Q closes the Files application
sequentially. Rename, Duplicate and Delete use owned dialogs, with embedded
fallback if dialog admission is refused. Other Files windows remain usable.

Files and Text share the [window/model ownership layer](desktop-window-ownership.md)
and each process uses a lazy file-worker pool with at most two transport
channels and bounded nonblocking communication.
Save operates on a snapshot, verifies stored bytes, and leaves later edits dirty.
External file changes and name collisions produce explicit errors rather than
silently overwriting a conflicting file. Normal window close allows
Save/Discard/Cancel where appropriate. Normal desktop exit waits for accepted
background saves; Ctrl-Alt-Escape is an emergency exit, not a save operation.

Open/Save pickers and folder fields normalize absolute paths consistently:
repeated slashes and `.` collapse, and `..` stops at root. This is lexical path
normalization, not symbolic-link resolution. Operations target the mounted VFS;
the desktop does not yet offer a removable-volume manager or automatic mounting.

## Trackpad and saved preferences

Settings offers five trackpad speed levels and an acceleration toggle. Slow
movement retains precision; fast swipes gain more travel. Two-finger scrolling
is independent. This policy currently applies to the Apple trackpad path, not
generic mice. The default desktop starts at speed 3 with acceleration on;
console/legacy trial input retains fixed scaling. The final feel awaits the
next MacBook Air trial.

A malformed trackpad report, endpoint failure or palm suppression during a held
press now cancels that interaction instead of sending an ordinary release that
could activate a widget. Cancellation discards uncertain queued pointer actions,
preserves an external mouse's held-button state, and requires a fresh trackpad
release before another physical press. The compositor preserves queued text and
keys while delivering cancellation ahead of subsequent pointer input. These
fault paths have host coverage; physical acceptance remains pending.

The prepared laptop trial saves preferences in
`/work/srvros-work/desktop.conf` using the existing verified background writer.
Normal desktop sessions use `/fat/home/desktop.conf`; if `/fat` is RAM-backed,
those settings last only until reboot. Settings reports save errors and external
file conflicts. Existing v1 and v2 preference files load with typing suppression
off and migrate to v3 on the next successful change. v1 also receives default
speed and acceleration values. Save verification does not
provide power-loss atomicity.

## Deliberate current limits

- Text documents and clipboard payloads are bounded to 64 KiB. Text keeps at
  most 256 undo records within its history arena. Editing is Unicode-codepoint
  based; shaping, grapheme clusters, IME and complete keyboard layouts are open.
- Files duplicates individual files up to 16 MiB. Directory deletion requires
  an empty directory; there is no recursive copy, Trash or undo for file deletion.
- File lists and search results retain at most 256 entries and report limits.
  Actionable paths are at most 159 bytes. Find Files searches names, with ASCII
  case folding; it is not content indexing or a symlink-aware search service.
- Preview accepts files up to 8 MiB, dimensions up to 4096 and four million
  pixels. EXIF orientation, color management and animated images are not yet
  implemented. Sketch is a small bitmap editor with four undo steps, not a
  layered graphics application.
- Terminal implements a bounded ANSI/VT subset. Resizing does not reflow saved
  lines; combining and double-width glyph semantics remain open.
- Notification history and calculator/stopwatch history belong to the session.
  Windows and unsaved documents are not restored after reboot.
- Network Utility uses the existing network stack with boot-selected e1000 or
  opt-in [USB CDC ECM](usb-network-2026-10-18.md). Other USB Ethernet chipsets,
  Wi-Fi, live failover and re-enumeration remain open.
- Settings has target-specific brightness and initial HDA sound controls; System
  has Devices and Sensors tabs. [Audio](macbook-air-audio-2026-10-18.md) and
  [temperature reporting](macbook-air-sensors-2026-10-18.md) need physical
  acceptance. Battery/AC, fan control, safe suspend and removable-device recovery
  remain separate hardware work.
- Save verification is not power-loss atomicity. The generated test-image format
  also has existing external exFAT-conformance limits, recorded in the journal;
  the accepted hardware flash filesystem was not changed by this session.

## Developing and testing

Start with the [widget library guide](../userspace/widgets/README.md), which maps
shared controls, pure models, event routing and client-runtime services. Apps
should extend that library rather than copy a control implementation. The
[desktop architecture plan](desktop-plan-2026-10-17.md) describes the ownership
boundaries; the journal records their implementation in order.

```sh
tools/host-env.sh make -j2 all
make -j2 test-core test-desktop
tools/host-env.sh python3 tools/desktop_arrange_smoke.py --resolution 1440x900
tools/host-env.sh python3 tools/desktop_arrange_smoke.py --small
tools/host-env.sh python3 tools/desktop_lifecycle_soak.py --rounds 6
tools/host-env.sh python3 tools/desktop_capacity_smoke.py
tools/host-env.sh python3 tools/desktop_recovery_smoke.py
```

Each VM script copies the filesystem image before mutating it. Evidence lives
under `build/review/`; these commands neither reboot the laptop nor install a
hardware boot image. App-specific `tools/desktop_*_smoke.py` workflows exercise
saved file bytes, clipboard, controls and worker behavior beyond the lifecycle
checks. Read a script's options before using it as a targeted regression.

The next laptop-service work is grounded in a
[read-only battery/backlight inventory](macbook-air-laptop-services-2026-10-18.md),
including the observed ACPI SBS path and hardware acceptance order.

## Opt-in laptop acceptance trial

The UUID examples below belong to the reference Air. On another machine use
its own prepared USB UUID. Current staging produces v22 and omits the optional
AML probe after its native stall. Audio and sensors remain device-gated; a
desktop boot does not imply all providers support the machine.

`python3 tools/hardware_boot.py stage --desktop-volume 37d8c5d0` stages the
interactive trial for the prepared USB workspace on this laptop. It enables
Apple trackpad gestures, keeps AHCI disabled and `/fat` in RAM, and opens Help
at **Laptop trial**. No automatic pass is inferred and there is no timed exit
or automatic reboot. The normal boot profiles remain unchanged.

The checklist covers window controls, typing/clipboard, persistent USB document
save/reopen, Terminal output, the new apps and screenshots. The prepared volume
must mount successfully before the desktop starts. Return to console closes the
session and attempts to flush/unmount `/work`; the console reports the result and
waits. Type `reboot` only after `USB unmounted=yes`. Errors remain visible.

Before installing a trial, preserve the accepted hardware release. Use
`install --release RELEASE --keep-previous` when the existing previous entry is
the accepted recovery to retain; the installer verifies it before making changes.
Rehearse the trial’s
exact staged loader/kernel/initramfs with:

```sh
tools/host-env.sh python3 tools/hardware_boot_smoke.py --desktop-check
```

This creates a disposable USB workspace with the stage's UUID, boots through
UEFI GRUB and Limine, checks the initial Help page, saves mixed-case text through
the GUI using its default USB path, saves a screenshot and Terminal working-directory
evidence, exits and unmounts, and independently checks file contents and exFAT
consistency. A second fresh VM checks saved preferences and documents, separate
log sessions and another clean unmount. It does not validate the physical Apple input path.


### Persistent trial workspace and diagnostics

In the laptop trial, Files Home, the document pickers, Text's new-document path,
Terminal's initial directory, preferences and screenshots use `/work/srvros-work`.
Normal sessions retain `/fat/home`. Applications receive the session location
explicitly; choosing a default directory does not perform filesystem I/O from
the compositor event loop.

Every laptop trial creates a fresh `Logs/session-<timestamp>-<suffix>` directory
on the prepared USB volume. `boot.log` saves the initial console tail. Changed
logs are checkpointed about every ten seconds into alternating `session-a.log`
and `session-b.log`; `exit.log` is saved before the final flush/unmount. Each file
contains at most the latest 64 KiB, with a timestamp and byte counter in its
snapshot marker. This is bounded diagnostic history, not a complete transcript.
The explicit Wi-Fi firmware capture alone opts into a 128KiB startup snapshot.
It refuses activation if the original 64KiB ring already discarded bytes, and
refuses a save if the extended ring overflowed. Ordinary desktop and battery
snapshot limits remain 64KiB. Host collection accepts files up to 128KiB, with
the existing aggregate 64MiB collection ceiling; collection size is not proof
of successful hardware completion.
Writes are read back and flushed through the existing storage service gate.
A failed save is visible and stops periodic retries; the previous checkpoint
remains available. A sudden reset can lose recent messages and these writes do
not provide power-loss atomicity. Photograph a frozen screen when possible.

The compositor now records close requests, child exit statuses and forced-close
reasons. Release identity and read-only laptop-service preflight readings are
included in the boot snapshot. The preflight checks the captured firmware and
chipset identity before reading status/backlight registers; it does not write
EC commands, change ACPI mode or alter brightness.

After returning to Ubuntu, collect without modifying the USB source:

```sh
python3 tools/hardware_desktop.py --uuid 37d8c5d0
```

The collector verifies the prepared volume marker, copies bounded log files into
`build/desktop-collections/`, checks the copies and records hashes and attention
markers in `collection.json`. Missing exit logs and recorded failures remain
visible. A collection is evidence for review, never an automatic hardware pass.


### Unattended laptop-service capture

For diagnostics that do not need physical input, stage a separate profile:

```sh
python3 tools/hardware_boot.py stage --laptop-probe-volume 37d8c5d0
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace
```

The v19 profile runs the same read-only laptop preflight, mounts the prepared USB
workspace, saves boot/exit snapshots with release identity, flushes and unmounts,
then requests a normal reboot. It opens no desktop and does not test physical
keyboard/trackpad behavior. Missing workspace, failed logging, failed unmount or
failed reboot halts on a visible error, without a countdown. Install the exact
verified release with `--keep-previous`, then select it once using `grub-reboot`;
Ubuntu remains the default. Collect its session logs with `hardware_desktop.py`
after Ubuntu returns. A saved capture is not desktop hardware acceptance.


### Brightness bring-up

The v20 interactive laptop profile enables the brightness provider after the
same exact firmware/resource and PCI checks used by the read-only preflight.
Startup only reads registers. Settings adds a shared slider below Trackpad with
a 5–100% range; opening Settings also only queries the provider. Other hardware
shows unavailable. Changes apply to this session and are not loaded from the
preferences file or changed automatically at startup.

The initial backend supports the observed PCH override mode, period 2777 and
disabled CPU PWM only. It changes the duty field, preserves the upper period
bits, writes no control/enable/polarity register, and checks readback. A changed
mode/period, invalid duty or failed readback makes the provider unavailable until
reinitialization. There are no waits, allocations or filesystem accesses in the
brightness syscall. Only the registered compositor can call it; the compositor
accepts changes only from focused Settings. Query responses and failed requests
never overwrite unrelated desktop preferences. Physical brightness acceptance
still requires the laptop: compare 20%, 50% and a comfortable final setting while
typing and moving the pointer, then confirm clean USB flush/unmount on exit.


### Window size limits

Maximize fills the usable 1440x900 laptop desktop, leaving the menu bar, dock and
eight-pixel side margins visible. Clicking again restores the original window
position and size. Preview's Fit/1:1 controls affect the image inside that window
independently. Shared client surfaces are capped at 1440x900; the kernel pixel
budget derives from the same constants. Larger screens still respect that cap.
Compositor caches remain preallocated, so resizing adds no cache allocations.

All standard desktop windows draw the same three outlined titlebar buttons:
minimize, maximize/restore and close, from left to right. Their visibility depends
on desktop mode, not an application catalog ID. Drawing and hit testing share
the control rectangles. Window > Maximize (or Restore size) exposes the same
action for the active app.

### Typing and trackpad movement

Settings → Trackpad → Disable while typing is off by default, including when
loading older preference files. Switching it on opts into the existing built-in
keyboard suppression policy: ordinary typing starts a 300 ms quiet interval,
held typing keys extend it, and incidental contacts stay suppressed until lift.
Physical clicks and ongoing drags override typing suppression; palm rejection
remains independent. External mice and keyboards do not participate in this
paired internal-device policy. Turning the setting off clears a suppression latch
on the next trackpad frame and rebases motion so suppressed travel cannot jump
the cursor. The normal verified preferences writer saves the choice.

### Bounded pointer wake

The experimental `srv_pointer_wait()` (syscall 133) remains available. It
returns 1 if the mouse queue is ready, 0 after at most one timer tick, or a
negative error/interruption. It does not consume an event. The dedicated queue
is notified when input is enqueued or cancellation replaces queued gestures;
readiness is rechecked under scheduler IRQ exclusion before blocking. The
one-tick deadline retains service of keyboard, client messages and clocks and
bounds signal/exit handling. A failed/unavailable call falls back to ordinary
one-tick sleep. Timer frequency and xHCI polling are unchanged. This is not a
promise of interrupt-driven USB or sub-tick device sampling.

The Pro11,2 physical comparison reported worse responsiveness. The desktop now
uses its previous unconditional one-tick sleep; hardware-trial logs identify
`policy=fixed-one-tick`. The syscall remains for controlled investigation and
is not an accepted latency fix.

Input summaries include software queue residence for trackpad-only batches:
`queue-oldest` preserves the first constituent timestamp across coalescing;
`queue-latest` uses the newest constituent. Mixed mouse/trackpad batches and
synthetic cancellations are excluded. xHCI reports consecutive contact dispatch
gaps (lift/malformed frames reset the sequence) and dispatch-to-rearm duration.
These fixed-size aggregates add monotonic clock reads during trackpad handling;
they do not measure hardware arrival, USB transaction duration or display
scanout. Values accumulate since boot, with counts, means and maxima; compare
matching workloads and inspect sample counts. The event ABI and polling rate
are unchanged.

For unattended `--idle-capture` only, an omitted scale is selected to keep the
saved BMP within the existing 8 MiB capture limit: 1x when it fits, otherwise
2x when that fits. A 2880x1800 framebuffer therefore captures 1440x900. Explicit
`--scale=1`/`--scale=2` remains authoritative, and ordinary desktop launch does
not auto-scale. Logs record both framebuffer and logical dimensions.

The SMC status ABI is version 2 (72 bytes), appending `fan1_millirpm` and a zero
reserved field. `fan_millirpm` retains its F0Ac meaning. Matched kernel/userspace
images are required; the syscall rejects old-size buffers. The exact captured
Pro11,2 GPU/PCH/DSDT profile requires FNum=2 and readable fpe2 metadata for both
F0Ac/F1Ac. A snapshot is published only after both reads complete. Failure of
either transaction invalidates the pair and quarantines the worker. The original
Air profile still requires one fan and returns zero for the second value.
System displays left/right RPM for the Pro; stale/faulted snapshots are not shown
as current. Diagnostic `smc-fans` lines retain both values and their shared sample
age; older workspace summary fields retain F0Ac for compatibility. Fan control,
SMC MMIO, IRQ6 and other keys remain unused. Resource/key references are the
captured DSDT/Linux sysfs evidence and Linux's
[Apple SMC driver](https://github.com/torvalds/linux/blob/v6.12/drivers/hwmon/applesmc.c).

The Pro v36 device-branch proposal adds `--desktop-sensors` to the scaled
`--desktop-brightness --desktop-audio` trial. It enables exact-profile read-only
CPU/SMC providers, retaining 2880x1800/2x, brightness and audio. It does not enable
AML, battery runtime or CPU/fan controls. The opening Help page includes the
System/Sensors two-fan, Pause/Resume, screenshot and manual return checks.
This is untimed; exit/unmount before reboot. Host formatting and VM boot checks
are separate from native visible acceptance. Shared profile integration remains
subject to review against the active Air branch.

## Quiescent client message waits

`desk_app_wait(app, ticks)` is a reusable client transport helper. It admits only
a configured, clean window with no sealed frame, pending resize, failed send or
unfinished menu publication. The application must separately exclude jobs,
clipboard transactions, timers and diagnostic work that require shorter service.
The legacy GUI-only helper keeps a ten-tick (100 ms) maintenance bound.
Normal Text/Files use the combined worker wait described below. Terminal uses
the same shared dispatcher with PTY read/write readiness and cursor deadlines;
other app roles and the compositor retain their own cadence.

The additive `SYS_GUI_WAIT` (136) takes the expected compositor PID and a maximum
of 100 ticks. It returns 1 for pending messages, 0 on timeout, -1 for invalid
arguments/lost server/cancellation, or `SRV_ERR_INTR` for a pending signal. It
never consumes a packet. Server identity is checked on each wake; the server
itself cannot use this client-only wait. Queued input, releases and close messages
wake the client immediately; the maintenance bound is not input latency.

The implementation reuses the signal/file-readiness wait queue. Message insertion
and GUI process cleanup wake that queue; readiness is rechecked with interrupts
masked before blocking. Spurious file/signal wakes only recheck the original
deadline. This avoids a new private signal-delivery route, at the cost of waking
unrelated readiness waiters. The syscall accesses only bounded GUI state and
retains the existing audited storage-service scheduling rules. No pointers to
userspace buffers or reusable GUI queue slots survive a wait.

An ineligible/failed wait falls back to the prior one-tick client loop. Clipboard,
title/menu/frame publication and explicit diagnostic workloads keep that path.
Normal Text/Files can wait on active file-worker pipes with `SYS_GUI_WAIT_IO`
(152); `SYS_GUI_CANCEL_WAIT` (153) interrupts combined process-local waits without canceling
accepted jobs. These helpers capture up to eight explicit nonblocking pipe/PTY
interests, an absolute monotonic deadline and a cancellation epoch. Close/dup2
invalidation, peer loss and signals are checked on each wake. During owned
storage I/O the kernel defers ordinary pipe readiness but still returns GUI
input; it does not enter the filesystem gate. See [desktop connections](desktop-connections.md)
for ownership, ready flags and short-retry policy. This changes no firmware,
device polling or CPU policy. Native energy improvement requires a matched repeated comparison against
the worker-attributed baseline; reduced timer wakes alone are not an energy result.

### Quiescent compositor waits

`SYS_GUI_SERVER_WAIT` (137), exposed as `gui_server_wait(ticks)`, is server-only.
It observes the current server's incoming GUI queue, structured keyboard events
and pointer events without consuming them or allocating queues. It returns 1
when any source is ready, 0 at the deadline, and a negative result for invalid
ownership, signals/exit or invalid bounds. The maximum is ten 100 Hz ticks;
zero is a nonblocking readiness check. Registration/access policy is unchanged.

The same IRQ-protected predicate is rechecked immediately before blocking.
GUI publication, structured keyboard publication, pointer enqueue/overflow/cancel
and process teardown wake the shared file-poll queue. Unrelated notifications
recheck readiness without extending the original deadline. Coalescing an already
queued pointer event needs no second wake because readiness never became false.
This reuses signal/exit handling; it can also wake unrelated file-poll sleepers.
No hardware polling interval or interrupt route is changed.

Ordinary `deskd` uses the wait only when no frame was just painted, no button or
drag/resize is active, and no input/control/frame reply, client startup/close,
clipboard transfer, preference save or screenshot job needs progress. Those paths
keep the one-tick cadence. Generated coexistence, legacy studies, battery-guided
and hardware-trial timing recipes keep their prior cadence too; the explicit
idle capture uses the new path. Clock, cached battery, notification and child
housekeeping are bounded by the 100 ms idle maintenance cap. New timed features
must update idle admission or supply an earlier deadline rather than assuming a
10 ms loop. VM functional input checks are not physical latency acceptance.


Explicit USB Wi-Fi firmware prerequisite profile v81 uses512KiB complete-prefix
capture; ordinary desktop logging stays64KiB and older firmware profiles keep
their128/256KiB selection. The larger capture still refuses an already-lost
prefix and checks byte-preserving save/collection. This opt-in diagnostic does
not enable Wi-Fi traffic or ordinary desktop startup probing; see
[USB Wi-Fi bring-up](usb-wifi.md). The explicit v86/v87 channel/VCO
diagnostics select1MiB to preserve the full earlier proof; v81-v85 remain512KiB,
and ordinary desktop capacity remains unchanged. Captures refuse lost prefixes.


### Compositor lifecycle worker

Application launch and compositor pixel-cache mapping/release run on one
persistent session worker. The input/render thread copies bounded launch requests,
reserves child slots and polls release/acquire completions; it never waits for
these operations. Window creation messages that race launch publication are
held until the child is registered. Unregistered senders are never admitted.
The kernel's storage gate, process ownership and memory integrity checks remain
unchanged; app startup can take time while the desktop continues to render.

Pixel reservations count against the existing cache budget until the worker
actually unmaps them, including a window closed before allocation completes.
Only complete private buffers become display fronts. Pending buffers request a
frame retry without a false low-memory notification; allocation failure during
resize restores the last delivered geometry and preserves its published front.
Shutdown stops and joins the worker before releasing its session records.
The worker queues are bounded and reject excess admission visibly. Registry
growth, screenshot initiation and exceptional forced process termination are
separate synchronous paths; the automatic lifecycle receipt measures the tested
normal launch/render/close path, not every possible desktop operation. Shared-address-space exclusion still
applies if the worker itself owns filesystem I/O; the RAM executable test does
not promise bounded launch latency for disk-backed executables.

### Attended desktop under automatic audio and USB load

The Air6,1 v77 `--desktop-audio-io-attended` profile requires the explicit
battery/services profile. Help opens the complete checklist automatically.
Continuous PCM and unpaced verified filesystem work run for 30 minutes using
the existing worker. The user exercises windows, dock, Text and Files; automatic
app cycling is deliberately absent so it cannot close user-owned documents.
The separate v74 unattended app-cycle acceptance requirement remains unchanged.

Completion stops the load, displays a notice and leaves the desktop open.
Save notes/screenshots and return normally, optionally reopen with `desktop
battery`, then unmount Home and reboot manually. Early exit is incomplete, never
a pass. The attended receipt requires completed load, keyboard/pointer activity,
normal exit, no child errors and the unchanged 500 ms desktop loop bound; visual
and listening acceptance still require the user's observations. Normal client
limits, filesystem validation, firmware gates and battery retirement are retained.
The native profile has no whole-desktop time limit; the worker and its playback
watchdog remain bounded. A separate 60-second VM mode checks completion and early
exit without altering the native duration.
