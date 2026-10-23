# MacBookAir6,1 — local bring-up

Updated: 2026-10-22. State: **Console, keyboard, bounded USB persistence and raw trackpad capture and bounded desktop pointer/drag/scroll trial accepted**.

## Identity and ownership

- Actual DMI model: MacBookAir6,1; firmware `122.0.0.0.0`.
  Marketing year and storage replacement history are not independently verified.
- Branch: `device/macbookair6-1/bringup`; integration base
  `85d5cb7ae436b6b13c6168a05af44e2d57c9e1c8`.
- Ubuntu 26.04.1 LTS, kernel `7.0.0-38-generic`, UEFI with separate writable
  FAT ESP. Ubuntu default verified; Secure Boot unsupported. No installed
  SrvrOS state or pending GRUB one-shot was found.
- Intel Core i5-4260U at 1.40 GHz; 8 GB RAM (7.7 GiB usable).
- Intel `8086:0a26` graphics, subsystem `106b:011a`, Linux i915 boot display;
  connected eDP panel exposes 1366×768.
- APPLE SSD SD0128F, approximately 113 GiB; Marvell `1b4b:9183` AHCI controller.
- Intel `8086:9c31` xHCI; Apple `05ac:0290` revision `0171` topcase.
  Two HID reports captured; interface 2 belongs to Linux bcm5974.
- Analog HDA `8086:9c20`, CS4208 codec `1013:4208`, subsystem `106b:7100`;
  HDMI HDA `8086:0a0c`, codec `8086:2807`.
- Linux applesmc exposes one Exhaust fan. Captured sensor values are Linux
  observations, not SrvrOS telemetry acceptance.
- Internal Broadcom Wi-Fi `14e4:43a0`; attached Netgear USB Wi-Fi `0846:9055`
  revision `0210`, Linux `rtw88_8822bu`; Bluetooth `05ac:828f`.
- Privileged inventory `build/hardware/macbookair6-1-2026-10-22` has zero
  missing/partial reads, including ACPI, HID descriptors, codecs and CPU policy.
  Raw firmware, EDID and device data remain local and ignored.

## Active work and shared dependencies

Bounded objective: provision the documented host toolchain, capture this unit,
validate the unchanged integration baseline and prepare console/input bring-up.
Scope is this record and model registration in current guides. No native device
or firmware gates are widened.

Fetched main at setup. Open PR #52 at `171b307` owns mounted Home battery
return work; PR #19 at `2bc3689` owns Edimax USB Wi-Fi/xHCI diagnostics.
Neither grants this Air support. Avoid competing xHCI edits until a concrete
Air6,1 transport reproducer exists; retain both branches' profile contracts.

The captured 84-byte USB configuration is byte-identical to the existing
Air7,2 `apple-05ac-0291-config.bin` fixture. Keyboard is boot interface 1,
endpoint 0x82, packet 64, interval 8. This supports reuse of configuration
parsing, not native transport or trackpad report acceptance. Current
`parse_trackpad_config` explicitly refuses PID `0290`; descriptor similarity
alone does not establish coordinate ranges, report format or mode switching.
The existing CS4208 routes admit only subsystem `106b:7e00` or `106b:5e00`,
not this unit's `106b:7100`. Backlight and SMC matching also target captured
Air7,2/Pro11,2 firmware. These require independent profiles, not a model rename.

## Capability ledger

| Capability | State | Remaining work |
| --- | --- | --- |
| Boot/display | Visible native console accepted | Desktop and exact framebuffer/layout checks pending |
| Keyboard | Typing, Up/Down, Backspace and Enter accepted | Explicit Caps Lock/repeat and broader input coverage pending |
| Trackpad/gestures | PID not admitted | Captured report/transport evidence before profile |
| USB workspace | Bounded monitor write/read and persistence accepted | Desktop/sustained I/O and hotplug not accepted |
| Sound/routes | Not tested | Exact codec routing evidence and listening checks |
| Brightness | Not tested | Exact firmware/register evidence |
| Temperature/fan | Linux inventory only | Native read-only profile evidence |
| Battery/AC | Not tested | Independent firmware audit; ordinary AML stays off |
| Networking | Not tested | Netgear is distinct from the Pro's Edimax work |

## Host validation

Against the recorded integration base, with documentation changes only:

- Documented Ubuntu dependencies installed; all seven pinned submodules initialized.
- `tools/host-env.sh make check-host`: PASS.
- `tools/host-env.sh make -j2 all usb-image test-core`: PASS; Zig 0.15.2
  verified, boot ISO and USB image produced.
- `make check-docs`: PASS.
- `tools/host-env.sh make smoke`: PASS: syscall ABI, all 481 shell commands,
  shell editing, UEFI USB keyboard and displayd. Final shell filesystem check
  reported zero errors/leaks. The initial sandbox run was blocked by Unix-socket
  permissions; the complete host-access rerun passed.
- These tests validate the integration baseline on this Ubuntu host, not native
  MacBookAir6,1 devices. Exact physical stage rehearsal remains pending.

## Local boot/test handoff

Volatile state lives in `build/bringup/checkpoint.json`; logs in `build/review`.
The 50-file inventory backup was hash-verified outside `build`, under
`~/.local/share/srvros/evidence/macbookair6-1-bootstrap-2026-10-22`.
The user authorized dependency installation using a machine-local sudo credential.
The first authorized one-shot returned to Ubuntu and was collected. No boot is
armed. Release `c35475f62f154f1e` is retained outside `build` as a known-working
console/basic-keyboard/bounded-USB payload; preserve it as recovery.

## Next actions

1. Preserve accepted release `c35475f62f154f1e` and the collected USB test file.
2. Plan bounded topcase report capture before an Air6,1 trackpad profile; native
   pointer/gesture support remains unaccepted. Coordinate any shared xHCI work.
3. Scope desktop, backlight, audio and EC/SCI enablement independently. Rebuild
   and rehearse any new candidate while retaining the accepted console payload.

## First console trial prepared (2026-10-22)

Consumed merged main `9ca1922` (PR #52) into clean device source `b31f743`.
PR #53 at `b67d179` continues Air battery desktop ownership; PR #19 at
`2de825f` remains Pro Wi-Fi work. No device gates changed locally.

Rebuilt ISO/USB/core checks and all 51 staging tests pass. Exact v7 console
release `c35475f62f154f1e` passes GRUB chainload, RAM filesystem, USB keyboard,
case/Caps/repeat and renderbench rehearsal. This console harness does not attach
a workspace disk; its default MBR field does not validate this physical GPT USB.

The attached approximately 57.6 GiB exFAT partition on GPT USB storage is now
prepared without formatting. All 102 pre-existing file hashes are unchanged;
workspace starters and a console checklist add five files. Both pre/post
read-only fsck and independent allocation checks pass, with zero leaks or errors.
The selected UUID, paths and file hashes remain in the ignored local run record.

Installed current is `c35475f62f154f1e`, with no previous SrvrOS release.
Ubuntu default and GRUB syntax are verified; no one-shot is armed and no reboot
has occurred. Payload and trial evidence have a hash-verified backup outside
`build`; this is a VM-tested candidate, not physically accepted recovery.
Run record: `build/native-desktop-trials/c35475f62f154f1e/run.json`.

Next is the attended console/keyboard trial, including a saved
`console-keyboard-first.txt`, clean sync/unmount and Ubuntu return. v7 does not
create automatic desktop session logs: retain photographs and user observations,
verify the saved file and original-file hashes, and run offline filesystem checks
before another trial. Physical keyboard, display and USB acceptance remain pending.

## First native result (2026-10-22)

Release `c35475f62f154f1e` reached a visible interactive console. The user reports
normal built-in keyboard typing, many commands, Up/Down, Backspace and Enter.
This accepts those checks only; Caps Lock, held repeat and desktop/trackpad
behavior were not separately confirmed.

The save test did not write a file: `>` and its filename were echoed literally,
and cat could not read the result. This matches monitor echo semantics; the
checklist did not require verification of shell entry. Workspace mount and shell
entry remain unconfirmed, so this is not evidence of a filesystem-driver defect.
The user then entered `reboot`. The monitor attempts to unmount an active
workspace and flush storage before reset, but no explicit unmount observation
was supplied. Ubuntu returned, its boot ID changed and the one-shot was consumed.

Post-return read-only exFAT/allocation checks pass (107 files, 109 clusters,
zero errors/leaks). All 102 original file hashes are unchanged, with no test file
or additional session log. v7 has no automatic desktop log. Evidence is saved
under the run's `physical-after` directory and copied to the durable backup.
USB persistence remains open; no new candidate was installed or reboot armed.

## USB write follow-up prepared (2026-10-22)

The next attended trial reuses installed release `c35475f62f154f1e` unchanged;
its exact VM receipt and installed/durable payload hashes were reverified.
No rebuild or reinstall is needed for this command-only correction. Main remains
`9ca1922`; other machines' battery/Wi-Fi work is independent of this trial.

Use monitor `workspace status` first and require the selected UUID with
`state=ready`. Then `write /work/srvros-work/usb02.txt Air61-USB-write-02`,
`cat /work/srvros-work/usb02.txt`, `workspace sync`, `workspace unmount`, and
`reboot` after successful unmount. Expected output file is exactly 18 bytes,
without a newline. If the workspace is absent, capture status/block/xHCI evidence
rather than treating this as a write test. The local checklist includes an
explicit mount of this unit's UUID when needed.

The new result file is absent; 108 existing file hashes (including the new
checklist) form the pre-run baseline. Offline fsck/allocation checks pass.
Run record and checklist: `build/native-desktop-trials/c35475f62f154f1e-usb-write-02/`;
a hash-verified copy survives outside `build`. Preserve the first trial evidence.
Ubuntu remains default, no one-shot is armed, and this preparation does not
claim USB persistence acceptance. Collect exact bytes, prior-file preservation,
user observations and offline checks after return before another trial.

## USB write physical acceptance (2026-10-22)

The user reports that USB-write-02 went smoothly on unchanged release
`c35475f62f154f1e` (source `b31f743`). After Ubuntu return, `usb02.txt` contains
exactly `Air61-USB-write-02`: 18 bytes without a newline. All 108 pre-test file
hashes are preserved. The only added files are the test result and SrvrOS's
`.srvros/meta` metadata file. Offline fsck reports 18 directories/110 files;
independent allocation checking reports 113 clusters, zero leaks, mismatches
or errors. Ubuntu default is verified and the one-shot is consumed.

This accepts the bounded monitor write/read, sync/unmount workflow and persisted
result on this unit. Workflow observations are user-reported; exact bytes, file
preservation and filesystem health were independently collected. v7 has no
automatic console session log. It does not accept desktop I/O, sustained loads,
hotplug, trackpad or other providers.

Evidence is under the USB-write-02 run's `physical-after` directory, with a
hash-verified copy outside `build`. Retain the accepted payload as recovery for
future trials. No new boot is armed. Main `62067a1` was fetched at collection;
this evidence applies to the recorded installed source, not the newer baseline.

## Active trackpad capture bootstrap (2026-10-22)

See the [capture workflow](../macbookair6-1-trackpad.md) for exact commands,
VM limits and the attended halt/restart contract.

Owner branch `device/macbookair6-1/trackpad-capture`, base `443dbfa` (main with
PR #54 merged). Bounded scope: captured `05ac:0290` revision `0171`, capture-only
Type 3 endpoint admission, sanitized configuration fixture and xHCI refusal/
transport tests. Pointer/gesture startup remains refused for this new PID until
native reports are collected. No EC/SMC/audio/backlight gates change.

Local configuration is byte-identical to the Air7,2 fixture. Linux v6.12
`drivers/input/mouse/bcm5974.c` identifies PID 0290 as Wellspring8 ANSI, endpoint
0x83/Type 3 and explicitly requires no mode switch. This supports a bounded
raw capture; it does not accept native reports or pointer behavior.

The trackpad VM harness now verifies the exact staged release on disposable
GPT media before installation. Shared edits are
limited to xHCI admission/tests, the fixture/Makefile argument and capture VM
harness; Pro PR #19 remains independent Wi-Fi/xHCI work. Preserve its transport
changes when later integrating. No other agent is messaged or scheduled.

Retain `c35475f62f154f1e` as accepted previous/recovery when installing the new
capture. The legacy v10 trial displays gesture/typing instructions and saves a
CRC-checked report/completion marker, then HALTS for manual restart; it does
not return automatically. Prepare only; boot when the user is ready.


Capture run `77ab86681e27132d`, release `19eb394b28cbeef5`, is prepared from
clean source `e2c660336914ce3bd412492b167bb5cfaff106bf`. Build/core/xHCI,
110 hardware-tool tests, capture-harness unit tests, exact-stage GPT rehearsal
and generic MBR regression passed. Both VM reports correctly retain incomplete
physical coverage; neither VM supplies Apple hardware acceptance.

Installed current is this capture; previous is accepted `c35475f62f154f1e`.
Physical USB UUID `6ADA-85BB` has the unconsumed request, 111 pre-test file hashes,
and passing offline fsck/allocation checks (114 clusters, zero errors/leaks).
Run evidence is in `build/trackpad-tests/77ab86681e27132d/`, with a durable copy
outside build. Ubuntu remains default and no one-shot reboot is armed. At the
next authorized reboot, record `launch.json` and arm the already installed
current entry without reinstalling/promoting an unaccepted recovery payload.
Native trackpad capture and pointer/gesture acceptance remain pending.


## Native trackpad capture accepted (2026-10-22)

Evidence-only follow-up on main `1454cb7`, branch
`device/macbookair6-1/trackpad-evidence`; no shared driver changes. Open battery
PR #55 and Wi-Fi PR #19 remain independent scopes.

The user reports run `77ab86681e27132d` appeared to pass. Collection of release
`19eb394b28cbeef5` (clean source `e2c6603`) validates report CRC/completion and
changed Ubuntu boot ID. Result: `CAPTURED`, 279 reports, 265 contact reports,
maximum two contacts, 45 button reports, eight lifts, zero malformed reports
or driver errors. Keyboard typed `AaZz09`, passed and dropped zero reports.
Native samples include touch, multi-contact and button packets.

All 109 unrelated pre-test files retain their hashes. The request contains the
expected `STARTED` marker; `.srvros/meta` was updated. Only report/completion
files were added. Offline fsck is clean (113 files); allocation reports 116
clusters, zero leaks, FAT/bitmap mismatches or errors. Persistence is verified;
on-screen unmount/halt observations remain covered by the user's overall
success report, not explicit fields in this v1 capture format.

Evidence and completion marker are in `build/trackpad-tests/77ab86681e27132d/`,
with a hash-verified durable copy under this release's existing evidence folder.
Ubuntu default and both installed payloads are verified; the one-shot is
consumed. Console/USB recovery `c35475f62f154f1e` remains previous. No new boot
is armed. This accepts bounded raw capture and keyboard coexistence. Desktop
pointer calibration, click/drag, scrolling and sustained operation remain
untested; normal pointer/gesture admission remains disabled for this PID.
The next milestone is a separate bounded desktop pointer trial.

## Active desktop pointer trial preparation (2026-10-22)

Owner `device/macbookair6-1/desktop-pointer`, integration base `dc48846`.
Use accepted capture `77ab86681e27132d` to admit the same revision/configuration
for explicit pointer/gesture modes. Scope: xHCI admission/tests, native report
fixtures, exact-stage v11/v12 VM rehearsal and this unit's test documentation.
Retain existing pointer policy and sibling behavior; no calibration claims,
firmware changes or USB workload expansion. PR #19's Wi-Fi/xHCI work and PR #55's
battery work remain independent. Prepare an attended v12 desktop/scrolling trial,
run host and exact-stage GPT checks, preserve console recovery, self-merge and
install unarmed. Physical movement/click/drag/scroll acceptance awaits this run.


Prepared run `e0d4f2a9d906c9b5`, release `e20382a7c8acbfe2`, from clean source
`ecb028c497246dec48be6bb0f0efa345afa37dc4`. Native sample decoding, xHCI tests
in all three modes, pointer/desktop/capture suites, full build/core and 110
hardware-tool tests passed. Exact v12 GPT VM rehearsal passed desktop startup/
exit, CRC report persistence/unmount, replay/read-only refusal and halt. Its
pointer/gesture coverage correctly remains incomplete without Apple hardware.
The rendered desktop trial was inspected.

Installed current is the v12 trial; `--keep-previous` retains accepted recovery
`c35475f62f154f1e`. Captured v10 payload/evidence also survives outside build.
Physical USB UUID `6ADA-85BB` has an unconsumed request and 114 pre-test hashes;
offline fsck is clean, allocation 117 clusters with zero errors/leaks.
Run/checklist evidence under `build/trackpad-tests/e0d4f2a9d906c9b5/` and exact
VM evidence have hash-verified durable backups. Ubuntu remains default and the
one-shot is empty. At readiness, save `launch.json` and arm the installed entry
without reinstalling. After manual restart collect this exact run first.
No physical pointer/click/drag/scroll acceptance is claimed yet.


## Native desktop pointer trial accepted (2026-10-22)

Evidence-only follow-up on main `22fd266`, branch
`device/macbookair6-1/pointer-evidence`; no driver or desktop changes. PR #19
remains independent Wi-Fi work. The newly integrated battery changes are not
part of the physically tested source below.

The user reports “the trackpad works well” after run `e0d4f2a9d906c9b5`, release
`e20382a7c8acbfe2`, clean implementation `ecb028c`. Collector validates report
CRC/completion and changed host boot ID: `CAPTURED`, pointer/gesture trials
`EXERCISED`, desktop exit and keyboard `PASS`. Over about 132 seconds, 5,009
reports include 3,306 pointer motion events, 34 button edges, 792 held-drag
frames, 505 scroll events and 63 lifts. There are zero malformed reports,
driver errors, pointer cancels/queue overflows or dropped keyboard reports.

This accepts the bounded desktop pointer, physical click/held drag and scrolling
trial with positive overall usability feedback. Direction/speed were not rated
separately. Typing suppression (zero frames), tap clicks (zero), and drag
handoffs (zero) remain unexercised. Five broad-contact suppression frames do
not establish general palm rejection. Sustained use and other applications
remain outside this trial's acceptance.

All 112 unrelated pre-test file hashes match; the request is correctly consumed,
workspace metadata updated and only report/completion files added. Offline
fsck is clean (116 files), allocation has 119 clusters and zero leaks/mismatches/
errors. Durable report persistence is independently verified; on-screen unmount
and halt are not separate fields in this report format. Evidence, completion,
hashes and checks in `build/trackpad-tests/e0d4f2a9d906c9b5/` have a hash-verified
backup outside build under release `e20382a7c8acbfe2`.

Ubuntu default and installed payloads are verified, one-shot consumed. Current
remains the accepted bounded v12 trial; previous console/USB recovery remains
`c35475f62f154f1e`. No new test or reboot is prepared.

## Active tap/typing trial (2026-10-22)

Base `c6a9170`, owner `device/macbookair6-1/tap-typing`. v12 study startup leaves
pointer options zero and compositor cleanup resets them: its zero tap/typing
counters do not establish a device failure. Add explicit v50 tap/typing profile,
study-only option setup and console restoration, coverage reporting/validation,
exact-stage VM and focused host checks. Preserve old recipes and ordinary app
preferences; no driver policy or firmware changes. PR #60 owns descendant
lifetime work (possible nearby desktop edits); keep this change limited to trial
startup. PR #19 Wi-Fi is independent. Prepare/merge/install unarmed and retain
accepted console recovery; native tap/typing acceptance awaits this new run.

Candidate v50 run `4eb6e6e65525268d`, release `f8e370a084725baa`, uses clean
implementation `1977c8b7e8eb333197886340d66d9db4ec3a0ced`. Build/core, focused
pointer/desktop/capture suites and 112 hardware-tool tests passed. Exact-stage
GPT rehearsal passed desktop option setup/exit, console option restoration,
CRC report/unmount, replay/read-only refusal and halt. Its tap/typing result
correctly remains `INCOMPLETE` without Apple input. Desktop and console screens
were visually inspected. No physical tap/typing acceptance yet.

Physical USB UUID `6ADA-85BB` has an unconsumed request and 117 pre-test hashes;
offline fsck is clean (117 files), allocation has 120 clusters and zero errors
or leaks. Preserve console recovery `c35475f62f154f1e` and the durable accepted
v12 payload/evidence `e20382a7c8acbfe2`. Trial instructions are in the
[current guide](../macbookair6-1-trackpad.md#tap-to-click-and-typing-suppression-trial-v50).

Legacy gesture/MBR regression also passed. Installed current is v50; previous
remains console recovery via `--keep-previous`. Installed hashes, Ubuntu default
and GRUB syntax verified; one-shot is empty. Run/checklist and exact VM evidence
have hash-verified durable backups under release `f8e370a084725baa`. At readiness
save `launch.json` and arm the already installed entry without reinstalling.
Collect run `4eb6e6e65525268d` first after manual restart.

## Tap/typing accepted; study feedback follow-up (2026-10-22)

Run `4eb6e6e65525268d`, release `f8e370a084725baa`, source `1977c8b`: the user
reports everything worked as intended, with one UI defect: tapping Text size
activates it but lacks the brief pressed-color feedback seen on physical click.
Collector verifies CRC/completion and host return; tap/typing, pointer and
gesture trials are EXERCISED, keyboard/desktop exit PASS. Counts: 18 primary
taps, 150 typing-suppressed frames, 326 scroll events, 2,484 reports, zero
malformed reports/errors/cancels/queue overflows or dropped keys. This accepts
bounded primary taps and typing suppression/recovery with positive user feedback.
Secondary taps and broad-contact rejection remain unexercised in this run.

All 115 unrelated file hashes are preserved; only expected consumed request,
workspace metadata and report/completion changes occurred. Offline fsck and
allocation pass. Evidence has a hash-verified durable backup under this release;
Ubuntu returned and one-shot is empty. No new boot is prepared.

Follow-up scope on base `14f17d2`, branch `device/macbookair6-1/tap-feedback`:
fix only the Type study's visual activation feedback with presentation-aware
expiry and regression tests. Its receive batch can consume both tap edges
before drawing. Preserve immediate activation and input order; no driver,
shared-widget or firmware policy changes. PR #19 Wi-Fi is independent. The
newer integrated battery/lifetime work is outside the physical trial acceptance.

The Type study now retains a visual-only activation pulse through the highlighted
frame's compositor release, then clears it after ten ticks (about 100 ms).
Press/release ordering, immediate activation and logical capture are unchanged.
Resize/cancel clears the pulse. Host regression covers both tap edges before
draw, highlighted/restored pixels, delayed/stale frame release and repeat/cancel.
This fix is not yet physically retested and does not alter the installed v50.

Feedback fix build/desktop host suite and compositor VM passed, including study
activation, scrollbar capture, focus, drag/resize, client/server cleanup and
console return/relaunch. Host pixel regression verifies the fast-tap highlight
and its expiry. Logs: `build/review/air61-feedback-{build,host,compositor}.log`.
No new hardware stage or installation was made; v50 remains installed unarmed.

## Highlight verification preparation (2026-10-22)

Base `52ef3ea`, branch `device/macbookair6-1/highlight-trial`: prepare a fresh
v50 request with merged Type study feedback fix. No implementation changes;
recheck build/host regression and exact-stage GPT VM, retain console recovery,
install unarmed. Primary objective: light taps on Text size visibly highlight
once, activate once, then restore; compare physical clicks and check repeated
taps/cancel/resize do not leave the highlight stuck. Tap/typing checks remain
enabled. PR #19 Wi-Fi remains independent. Prior run is collected and archived.
