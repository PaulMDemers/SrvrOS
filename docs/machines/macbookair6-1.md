# MacBookAir6,1 — local bring-up

Updated: 2026-10-23. State: **Native desktop/input, bounded concurrent USB file work, duplicate-ID repair, completed-workload rerun refusal and speed/acceleration persistence physically accepted. Native brightness adjustment across 5–100%, bounded speaker/headphone audio playback and basic read-only CPU/fan telemetry physically accepted.**

## Initial inventory (2026-10-22)

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

## Initial setup scope (historical)

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
parsing, not native transport or trackpad report acceptance. Initial `parse_trackpad_config` refused PID `0290`; subsequent exact-gated admission
and native capture/gesture acceptance are recorded below. Descriptor similarity
alone did not establish coordinate ranges, report format or mode switching.
At initial inventory, the CS4208 routes admitted only subsystem `106b:7e00` or `106b:5e00`,
not this unit's `106b:7100`. Backlight and SMC matching also target captured
Air7,2/Pro11,2 firmware. These require independent profiles, not a model rename.

## Capability ledger

| Capability | State | Remaining work |
| --- | --- | --- |
| Boot/display | Console and full native desktop exercised at 1366×768 | Broader layout and extended-session coverage |
| Keyboard | Typing, Up/Down, Backspace and Enter accepted | Explicit Caps Lock/repeat and broader input coverage pending |
| Trackpad/gestures | Exact PID0290 profile; bounded pointer/drag/scroll, primary tap, typing suppression and study feedback accepted | Bounded app coexistence exercised; secondary tap, broad-contact rejection and long sessions pending |
| USB workspace | File saves/navigation, 60-round concurrent workload and rerun refusal accepted | Longer/larger workloads, failure handling and hotplug |
| Sound/routes | User-confirmed headphone/speaker operation; manual route changes, short WAV and Stop/recovery with zero underruns | Five-minute native endurance incomplete; jack disconnect and detailed channel/isolation observations not separately recorded |
| Brightness | Exact-gated read-only capture and visible 5–100% adjustment accepted | Broader endurance; no reboot persistence claim |
| Temperature/fan | Basic native read-only CPU/one-fan display and freshness accepted | Longer desktop/file/audio coexistence; Pause/Refresh/reopen observations |
| Battery/AC | Passive native handoff accepted; connected offline runtime audited and isolated five-sample candidate implemented | Native bounded samples and retirement; ordinary desktop remains off |
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

## Current objective and next actions

Base `9bbc3b9`, branch `device/macbookair6-1/battery-runtime`. Audit connected
Air6,1 AML initialization and query effects using private captured regions and
synthetic EC state. Scope: separate diagnostic-native profile/policy, shared
runtime only where a model-specific contract is required, staging/VM tests and
Air6,1 docs. PR88 owns trackpad timing; PR19 owns Pro USB Wi-Fi. Preserve both.
Next checkpoint is five bounded AC-connected samples with parked USB and full
retirement. Ordinary desktop admission and charging controls remain disabled.

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

Fresh run `cd54ca6d8f2cd4fb`, release `ab0bea06acccc443`, clean source `d15b5a7`
is installed unarmed. Build/desktop/capture tests pass, including the batched-tap
pixel/expiry regression; prior compositor integration covers the unchanged fix.
Exact-stage v50 GPT rehearsal passes startup/options/exit, report/unmount,
replay/read-only refusal and halt. Hardware coverage remains pending.

USB UUID `6ADA-85BB`: 120 pre-test file hashes, clean fsck, allocation 123
clusters with zero leaks/errors. `--keep-previous` retains console recovery
`c35475f62f154f1e`; accepted tap/typing payload `f8e370a084725baa` is durably
backed up. Installed hashes, Ubuntu default and GRUB syntax verified; next_entry
empty. Run/checklist and VM evidence have hash-verified copies outside build.
At readiness save launch.json and arm the installed entry without reinstalling.
After manual restart collect this run and the user's visible-highlight feedback.


## Native tap highlight verification accepted (2026-10-22)

Evidence-only follow-up on base `9256faa`, branch
`device/macbookair6-1/highlight-evidence`; no implementation changes. Open
PR #66 AC/battery transitions and PR #19 Wi-Fi remain independent work.

For run `cd54ca6d8f2cd4fb`, release `ab0bea06acccc443`, source `d15b5a7`, the
user reports “The tapping and clicking both working great.” This is positive
feedback for the requested highlight verification on the merged MR #64 fix.
Visible feedback acceptance is user-reported; saved counters do not measure
rendered button colors or pulse timing. Collector validates report CRC/completion
and changed host boot ID: tap/typing, pointer and gesture trials EXERCISED,
keyboard/desktop exit PASS. Counts include 11 primary taps, 20 button edges,
32 typing-suppressed frames, 26 scroll events and 765 reports. No malformed
reports, driver errors, pointer cancels/queue overflows or dropped keys.
Secondary taps and broad-contact suppression remain unexercised in this run.

All 118 unrelated pre-test files retain their hashes. Only the expected consumed
request, metadata and report/completion changes occurred. Offline fsck is clean
(122 files); allocation 125 clusters, zero leaks/mismatches/errors. Evidence and
completion under `build/trackpad-tests/cd54ca6d8f2cd4fb/` have a hash-verified
durable backup under release `ab0bea06acccc443`. Installed payloads and Ubuntu
default are verified; one-shot consumed. Current remains this accepted bounded
trial, previous remains console recovery `c35475f62f154f1e`. No new boot prepared.

## Full desktop/USB session preparation (2026-10-22)

Base `208423f`, branch `device/macbookair6-1/desktop-session`. Bounded objective:
full workspace desktop with an attended, finite Terminal copy/sync/readback job
while using Text, Files, window management and settings. Scope: additive v53
input-only desktop staging, shared fixture/verification helper, exact-stage VM
coverage and this unit's checklist. No kernel, driver or app behavior changes.
PR #66 owns v52 AC/battery capture; PR #19 owns Wi-Fi. Their separate recipes
and native gates remain unchanged. Prior highlight run is collected. Preserve
console recovery `c35475f62f154f1e`; install unarmed after exact-stage validation.

The initial [full desktop session checklist](../macbookair6-1-desktop-session.md)
uses v53 release `306174ddfec763ef`, clean source `f8a498a`; VM tooling finalized
at `819acd0`. Build, desktop host suite, staging/collector tests (56 installer
checks, 22 collector checks) and docs pass. Exact-stage GPT VM passes 60 one-MiB
copy/sync/readback rounds with concurrent Text save, Terminal writes and screenshot
capture, plus exact retained bytes, session collection, two clean unmounts and
preferences across a fresh VM boot. Two earlier harness failures (semicolon key
mapping and outdated Settings coordinates) were corrected; neither was a native
OS failure. Failed logs remain local. No native full desktop acceptance yet.

Native run `23dadab462e9ed83` has fresh inputs and 124 pre-boot file hashes on
UUID `6ADA-85BB`. Offline fsck is clean; allocation has 134 clusters, zero leaks,
mismatches or errors. Install retains console recovery `c35475f62f154f1e` via
`--keep-previous`. Installed hashes, Ubuntu default and empty one-shot are checked.
Evidence/checklist live in `build/desktop-sessions/23dadab462e9ed83`, with durable
payload and trial copies outside build under release `306174ddfec763ef`.

At readiness recheck installed state, USB identity/input hashes and record the
launch before arming. In the desktop Terminal run `sh session-23dadab462e9ed83.sh`,
then use other apps while it advances. Save `notes-23dadab462e9ed83.txt`, wait for
USB SESSION COMPLETE, save a screenshot, return to console and require clean
log save/unmount before `reboot`. On return collect the matching new desktop
session and verify workload/notes/preferences/prior files and both offline checks
before staging another run. The preparation leaves reboot unarmed.

MR #66 landed during preparation; consumed merged main `ddf4ebf` and retained
both the v52 battery recipe and v53 basic desktop recipe. Combined source
`f689b7c` rebuilds successfully; 57 staging/installer tests, 23 collector tests,
the desktop suite and battery control/transition regressions pass. Initial
release `306174ddfec763ef` is superseded by `9ddea14869c9ed6f`; the same unstarted
USB run and inputs remain valid. Final exact-stage GPT rehearsal passes all 60 rounds, concurrent app saves,
retained bytes, two clean exits/unmounts and fresh-boot preference persistence.
The rebuilt release is installed unarmed with console recovery retained; installed
hashes, Ubuntu default and empty one-shot are verified. Final payload and trial
backups are hash-verified outside build under `9ddea14869c9ed6f`. The run command,
checklist and physical acceptance scope above are unchanged.

## Full desktop return: persistence confirmed, picker defect (2026-10-23)

Evidence-only follow-up on merged main `80b567a`, branch
`device/macbookair6-1/desktop-evidence`. PR #69 ordinary battery sessions and
PR #19 Wi-Fi remain separate. No new payload or reboot is prepared.

Run `23dadab462e9ed83`, release `9ddea14869c9ed6f`, returned to Ubuntu with a
changed boot ID. Two matching sessions (`00065e7a277b701f-00` and
`00065e7a391c3f26-00`) have exit snapshots, clean desktop exit and no collector
attention flags. The user confirms restarting/rerunning after an interruption.
The workload retains rounds 1–19 followed by 1–60, a completion marker and an
exact final 1 MiB payload. Strict single-run verification correctly rejects the
79-line round log; do not call the original uninterrupted trial a pass.

The user reported Text claiming saves while files did not appear, and inability
to reenter Home after navigating Up. Four newly saved text files exist on USB:
`demo_test.txt`, `test-save-test.txt`, `test2.txt`, `t3.txt`. Logs report successful
file writes and directory reads, but Home listings never reach picker-ready.
Persisted `.srvros/meta` contains two duplicate-inode pairs within Home:
639 for the first trackpad completion and a later request, 647 for another
completion and the new session script. `desk_listing_decode` refuses duplicate
inodes, rejecting the entire directory. Parent browsing can succeed, but entering
Home again hits the same rejection. This is a metadata identity defect, not
sufficient evidence to blame double-click timing or lost document writes.

All 124 pre-boot file hashes are preserved except expected `.srvros/meta`.
Offline fsck is clean (139 files); allocation traversal finds 160 clusters with
zero leaks/mismatches/errors. Installed current/recovery hashes and Ubuntu default
verify; one-shot is empty. Raw sessions, new files, metadata and return records
are hash-verified outside build under the installed release's trial directory.
No screenshot or saved desktop preferences was found; those checks remain open.
Full desktop acceptance is withheld pending the picker fix and a fresh trial.

Two follow-ups are needed: reproduce persisted/imported inode collisions and fix
identity restoration while retaining strict listing validation; make the workload
explicitly refuse an existing run directory. Its current `mkdir ... || exit 1`
guard is insufficient because SrvrOS mkdir succeeds for existing directories.
Preserve the interrupted and restarted evidence instead of rewriting round logs.

## Identity and workload retry fix scope (2026-10-23)

Base `58b354f`, branch `device/macbookair6-1/file-identities`. Fix duplicate VFS
identities during metadata import/registration, preserving permissions/times and
strict picker validation. Add collision/allocator regressions and a disposable
VM fixture with duplicate persisted IDs, saved-file reopen and Home reentry.
Fix the shell workload's existing-directory guard and exercise refusal without
changing retained round evidence. Scope: VFS metadata, workload/rehearsal tools,
tests and this unit's guide. PR #69 owns opt-in battery session startup; PR #19
owns Wi-Fi. Neither owns VFS identity restoration. Prior native run is collected;
retain its evidence and console recovery. Prepare a fresh v53 trial only after
host tests and exact-stage VM pass, install unarmed, then merge under existing
authorization. No firmware/profile gates are widened.

The VFS now remaps imported zero/colliding IDs to unused live IDs across the
whole node table; unique requested IDs and all other metadata fields are retained.
Registration and metadata restoration share the rule; allocation skips used IDs
and handles wraparound. The picker retains strict duplicate checks. The host
regression fails against the old implementation at the collision assertion and
passes on the fix, including file/directory import, repeated updates, zero and
maximum IDs, wraparound and retained mode/time. Core and desktop suites pass.

Exact-stage v53 GPT VM passes on release `db7ce82008f4b9a6`, clean source
`78e285a`: seeded duplicate persisted IDs become unique, saved Text files reopen,
Home Up/reentry works, all 60 copy/sync/readback rounds verify, and retries during
the active workload and after a fresh boot return nonzero without changing the
round log or final payload. Preferences, session logs and two clean unmounts pass.
The guard explicitly tests directory existence before mkdir; it is a sequential
rerun guard, not an atomic concurrent-job lock. Run one instance at a time.

Fresh native run `f48940182e6911a0` is installed unarmed. Command:
`sh session-f48940182e6911a0.sh`. Follow the updated full desktop checklist,
including visible file listings, Up/double-click Home reentry, saved-file reopen
and a second invocation that must print REFUSED without another round. Prior
metadata/evidence remains untouched on USB so mount-time repair is exercised.
There are 141 pre-boot hashes; fsck is clean and allocation has 169 clusters with
zero leaks/mismatches/errors. Console recovery `c35475f62f154f1e`, installed hashes,
Ubuntu default and empty one-shot are verified. Payload/trial evidence has a
hash-verified durable copy outside build under the new release. Physical fix
acceptance is pending; collect this fresh run before another trial.

## File identity fix physical result (2026-10-23)

Run `f48940182e6911a0`, release `db7ce82008f4b9a6` (clean source `78e285a`,
merged PR #71), returned to Ubuntu. Matching session
`session-00065e82d1e50247-00` has its exit snapshot and desktop exit with no
collector attention. The user reports the file issues resolved during this run.
All 172 persisted metadata IDs are nonzero and unique, including repair of the
previous collisions. Two new text files persist. All 141 preboot file hashes
are unchanged except the expected `.srvros/meta` repair/update.

The workload verifies exactly rounds 1–60 once, 62,914,560 bytes copied, exact
final readback and completion marker. Offline fsck is clean (150 files), and
allocation traversal passes with 187 clusters and zero leaks, mismatches or
errors. This accepts the reported file-listing/save/navigation fix and the
bounded concurrent USB workload on this unit. Physical rerun refusal and
preference persistence are not independently captured; their VM evidence remains
valid but this report does not promote them to physical acceptance. No screenshot
was saved. Broader gesture/session and optional device coverage remains open.

Raw collection, changed/new files, hash report and offline checks are preserved
under the run directory and hash-verified durable evidence outside build at
`~/.local/share/srvros/evidence/db7ce82008f4b9a6/trial`. Run/launch records are
returned-collected; GRUB one-shot is empty and USB is unmounted. No new trial
is staged or armed. Next work should choose a bounded remaining acceptance scope.

## Rerun refusal and preference persistence scope (2026-10-23)

Base `5089172`, branch `device/macbookair6-1/rerun-preferences`. Prepare a focused
attended follow-up using accepted release `db7ce82008f4b9a6` and the completed
`f48940182e6911a0` workload. Preserve that workload; capture refusal stdout and
exit status and compare its retained files with pre-test reference bytes. Save
pointer speed 5/acceleration off through Settings, check reopening, then collect
before a second boot verifies restoration. Scope is workload evidence helper,
VM rehearsal and this unit's guide/record; no OS or firmware gate changes.
PR #69 battery startup and PR #19 Wi-Fi remain separate. Re-run exact-stage GPT
VM before readiness; retain Ubuntu default and console recovery. No reboot yet.

Focused phase-1 run `359e52fa68fe19bf` is ready, installed-unarmed, reusing
unchanged accepted release `db7ce82008f4b9a6` (OS source `78e285a`). Preparation
helper/VM code `8441c88`, regression `b925cc3`: 57 staging tests, refusal preflight
and collector regression, Settings/preferences host tests and docs checks pass.
The exact-stage GPT VM passes two boots, saved preferences, the new refusal
stdout/status/comparison receipt, 60 exact rounds, and two clean unmounts.
Two initial fixture-construction attempts failed before boot because nested
names are unsupported by the image builder; the final fixture uses flat names.

Native command: `sh refusal-359e52fa68fe19bf.sh`. It checks completed workload
`f48940182e6911a0`; require REFUSAL VERIFIED, then set Settings speed 5 and
acceleration off, wait for ready and reopen Settings. Return to Ubuntu for
collection before the separate restoration boot. All 150 prior files are
preserved; seven fresh inputs bring the preboot count to 157. Offline checks
pass (201 clusters, zero leaks/mismatches/errors). Installed payload and console
recovery hashes, Ubuntu default and empty one-shot are verified. Local run and
VM receipts are backed up outside build under the release's
`refusal-preferences/359e52fa68fe19bf` directory. Physical acceptance is pending.

### Refusal captured; preference restoration ready

Phase 1 `359e52fa68fe19bf` returned with matching new session
`session-00065e84b6a90e72-00`: exit snapshot/desktop exit present, no collector
attention. The actual refusal stdout matches, exit status is 1, verification
receipt passes, and the completed workload remains byte-for-byte unchanged.
`desktop.conf` persists speed 5 and acceleration off. All 157 preboot files
are unchanged except expected metadata updates. No screenshot was saved; visual
reopen behavior was not separately reported. Offline fsck is clean (165 files),
allocation passes with 210 clusters and zero leaks/mismatches/errors.

Phase 2 `3559ab3a5aab1fb8` is ready on the same verified installed payload,
with all 165 current file hashes recorded and no new USB inputs. Open Settings
and confirm speed 5/acceleration off before any change; capture a screenshot if
possible, return to console and reboot to Ubuntu. Do not rerun the workload or
refusal wrapper. The existing exact-stage two-boot VM receipt remains applicable;
no OS changes or new installation are involved. Both run records/evidence have
verified durable backups. Ubuntu default/recovery are intact; one-shot is empty.
Refusal and saved preference bytes are physically verified; restoration remains
pending this second boot. Newly merged PR #69 is not part of this installed
release; do not widen the trial to its battery startup behavior.

### Preference restoration physically accepted (2026-10-23)

Phase 2 `3559ab3a5aab1fb8` returned on unchanged release `db7ce82008f4b9a6`.
New matching session `session-00065e84cfa28c9c-00` has its exit snapshot and
desktop exit, with no collector attention. The user confirms settings persisted;
the saved screenshot shows Trackpad speed 5 of 5 and acceleration unchecked.
`desktop.conf` is byte-identical to phase 1, and the earlier refusal receipts and
completed workload still verify. All 165 preboot files are preserved except the
expected metadata update. New files are the four session logs and screenshot.
Offline fsck is clean (170 files); allocation passes with 249 clusters and zero
leaks/mismatches/errors. Evidence and screenshot have a hash-verified durable
backup outside build. One-shot is empty, USB unmounted, and recovery verified.

This closes the focused completed-workload rerun refusal and speed/acceleration
persistence checks on this unit. It does not expand acceptance to every preference,
concurrent script starts, optional hardware services or other Mac models. No
new boot is staged or armed; both focused runs are collected.

## Read-only brightness capture ready (2026-10-23)

Implementation/source `a0f148a`, release `142dd83ded3d2da4`, run
`b1bf2459dd0cd1f9`: exact GPU `0a26`, PCH `9c43`, 31,777-byte DSDT fingerprint
`af9a47bc352dc6bf` admits only the read-only PWM capture. Current Ubuntu DSDT
matches the inventory bytes. No EC/AML access, writable PWM mapping or provider
binding is admitted on this profile. Native mode/period evidence is still pending.

Full build/USB image and core suite pass, as do backlight tests, 58 staging tests
and captured-firmware positive/negative tests (wrong chipset, length, checksum
and checksum-correct modification refused). Exact-stage GPT v19 VM saves logs,
checks retained file bytes and clean exFAT, then resets; the missing-workspace
case returns without starting the probe. This is recovery-path validation, not
native PWM acceptance. Docs and diff checks pass.

Installed unarmed with console recovery `c35475f62f154f1e` retained; accepted
desktop `db7ce82008f4b9a6` remains installed and independently backed up. One
superseded EFI payload (`9ddea14869c9ed6f`) was removed only after verifying its
complete independent backup. Ubuntu default and empty one-shot verified.
All 170 USB files are recorded; offline fsck/allocation pass (249 clusters,
zero leaks/mismatches/errors). Payload and run evidence are preserved outside
build. This next boot is unattended: leave USB attached, allow automatic Ubuntu
return, then collect before any write-enabled brightness implementation.


## Native PWM capture result (2026-10-23)

Run `b1bf2459dd0cd1f9`, release `142dd83ded3d2da4`, session
`session-00065e85df9c8068-00` returned to Ubuntu with matching boot/exit logs,
`completed=yes` and no collector attention. Exact Air6,1 admission succeeded:
CPU control/duty `0/0`, PCH control `c0000000`, PCH duty `0ad904cc`;
decoded mode 2 (PCH override), period 2777, duty 1228 (about 44% duty).
The capture explicitly records no PWM writes, provider binding or EC access.
This accepts read-only access, not panel adjustment or calibrated luminance.

All 170 prior file hashes match except expected metadata update; only two
session logs are new. Offline fsck is clean (172 files), allocation passes with
252 clusters and zero leaks/mismatches/errors. Final saved/unmounted console
messages are emitted after logging stops and are not present in the saved exit
snapshot; retained logs, returned host state and offline integrity are recorded
separately rather than claiming those console messages were captured.
Evidence has a hash-verified backup outside build. One-shot is empty and USB
unmounted; recovery remains verified. Next is an exact Air6,1 brightness provider
preserving observed mode/period, with an attended visible adjustment trial.
No new boot is staged or armed.

## Brightness control scope (2026-10-23)

Base `83a3f59`, branch `device/macbookair6-1/brightness-control`. Bind an explicit
Air6,1 profile only behind the existing captured GPU/PCH/DSDT gate and observed
CPU control 0, PCH control c0000000, period2777. Reuse bounded 5–100% duty writes,
readback verification and drift quarantine. Discovery must leave the panel alone.
Prepare a brightness-only unscaled desktop v56 via basic+brightness staging;
no audio, EC/AML, battery or other optional service. Scope: backlight/profile,
probe binding, staging/VM tests and this unit's docs. PR #76 reserves v55 for
combined services; preserve that separate work when integrating shared staging.
PR #19 remains Pro Wi-Fi work. Native visible adjustment remains unaccepted.

### Attended brightness control ready

Clean source `1cc5f82`, release `59086591e526632b`, run `19da744d205e2b2c`
implements explicit Air6,1 control using the native captured period2777. The
existing exact GPU/PCH/DSDT gate is retained. Discovery leaves duty unchanged;
requests are bounded 5–100%, preserve mode/period and verify readback. Any mode,
period or readback mismatch disables the provider. Read-only capture stays read-only.

Full build/USB image, core and desktop suites, all-profile backlight tests,
captured-firmware tests and 59 staging tests pass. Exact-stage GPT v56 VM passes
two boots, Settings/preferences, Text saves, screenshots, diagnostic collection
and clean unmounts. Unsupported Q35 brightness remains unavailable. These tests
do not establish visible native adjustment. Docs/diff checks pass.

Installed unarmed, console recovery retained, accepted desktop and prior capture
backups verified. Superseded EFI payload `19eb394b28cbeef5` was removed only after
verifying its independent backup. All 172 existing USB files are recorded;
offline fsck/allocation pass (252 clusters, zero issues). Payload/run evidence
is backed up outside build. Follow the attended brightness guide: inspect the
initial level without a startup change; try 50/25/75/100/5 then50, confirm visible
changes and responsive input, reopen Settings, screenshot and return manually.
No battery/EC/AML/audio/network/sensor service is enabled. Native acceptance pending.


## Native brightness adjustment accepted (2026-10-23)

Run `19da744d205e2b2c`, release `59086591e526632b` (source `1cc5f82`), session
`session-00065e863524e78f-00`: the user reports proper brightness operation
throughout the range. Exact Air6,1 provider starts ready at 44%, with no discovery
write. Logs contain 193 successful brightness readings spanning 5–100%, ending
at 58%, with no unavailable result. The desktop exit snapshot is present and
collector attention is empty. This accepts visible adjustment on this unit;
no screenshot was saved, and final50/reopen or absence of a visible startup
change is not separately claimed. Reboot persistence/calibrated luminance remain
outside scope.

Offline fsck is clean (176 files); allocation passes with 257 clusters and zero
leaks/mismatches/errors. Prior files match except metadata and desktop.conf;
the latter changed pointer speed5 to3 and acceleration0 to1. The user confirmed
these changes were intentional; this is not a preference-persistence regression. Four new session logs are retained. Run/launch are collected,
one-shot empty and USB unmounted. Evidence is backed up outside build; no new
boot is staged or armed.

## Audio listening candidate (2026-10-23)

Implementation `8f3656a`, PR #82, v57 release `0533c0dfc63d104f` adds exact
`9c20` / CS4208 `106b:7100` revision `100300` admission. Both observed pin
pairs are validated before codec writes; speaker DAC3/pin12 and headphone
DAC2/pin10 reuse muted startup, GPIO0 and the existing serialized worker.
See the [route evidence and listening checklist](../macbookair6-1-audio.md).

Full build/USB image, core and desktop host tests, 60 staging tests and docs
checks passed. The exact staged GPT VM passed two desktop sessions, GUI file
save/reopen, preference restoration, recorded 500 ms tone and six-second stereo
WAV, zero underruns and silent tails. Native audio remains **not tested**;
QEMU routing cannot establish this Air's sound quality or channel balance.

The candidate retains accepted brightness and input, with battery/AML, SMC,
sensors and networking off. Preserve accepted brightness `59086591e526632b`
and previous console recovery `c35475f62f154f1e`. Local run
`build/audio/42421979e85c44eb` holds 176 preboot file hashes and clean offline
exFAT/allocation evidence. Ubuntu stays default; reboot is unarmed. Next action
is the attended speaker/volume/channel/Stop test and optional manual headphones,
followed by collection and file/allocation checks before acceptance.

## Native audio return (2026-10-23)

The user reports that sound worked properly on release `0533c0dfc63d104f`
(compiled `8f3656a`). Two new sessions were collected: `00065e8718b76081-00`
initialized audio and exited without test tones; `00065e874fca8c4d-00` completed
three tones, a 1,152,000-byte WAV and cancellation of a second stream at byte
609,188. Volume requests include 10% and 25%. Both sessions exited cleanly;
audio ended ready, no error, no owner and zero underruns.

This accepts basic audible speaker playback on this unit. Detailed left/right
balance, absence of startup pops, headphone isolation and sustained playback are
not separately confirmed. No headphone selection or connection was logged.
The diagnostic literal `physical sound NOT_TESTED` is intentionally not an
automatic acceptance flag; listening confirmation comes from the user.

All 176 prior files remain, with only filesystem metadata changed and eight new
session logs. Offline exFAT and allocation checks pass: 184 files, 267 clusters,
zero leaks/mismatches/errors. Ubuntu return, empty one-shot, installed/recovery
hashes and the independent payload backup are verified. Full return evidence is
retained outside `build`; no new boot test is armed.

## Headphone/extended playback ready (2026-10-23)

The next attended trial reuses accepted release `0533c0dfc63d104f`, compiled
`8f3656a`. No payload or hardware gate changed. Run `5a302d248eee397a` in local
`build/audio/extended-20261023` has the headphone/extended checklist and a
verified 60-second stereo WAV in USB Home's `audio-extended` directory. All 184
existing files are preserved; the new baseline has 185 files. Offline exFAT and
allocation checks pass: 356 clusters, no leaks/mismatches/errors.

Targeted HDA, audio service/stream, WAV conversion and Settings host tests pass.
The existing PCM VM harness completed five one-minute plays with zero guest
underruns, exact recorded samples (maximum error zero), distinct stereo and USB
input retained. Actual guest kernel/initramfs hashes match the accepted release;
the newer harness checkout is recorded separately, not claimed as compiled code.
The exact v57 GPT VM additionally passed two desktop sessions, GUI save/reopen,
preferences, tone/WAV capture, durable logs and clean unmounts. These results do
not accept physical headphones or extended native playback.

Use the [extended checklist](../macbookair6-1-audio.md) to check manual headphone
routing, jack indication, left/right isolation, five full one-minute plays,
Stop/recovery and return to speakers. Console recovery `c35475f62f154f1e`
remains previous; Ubuntu stays default. No reboot is armed. On return identify
new sessions against this run's preboot hashes: the release ID is deliberately
the same as the earlier accepted speaker trial. Preserve the prior trial evidence.

## Headphone trial return (2026-10-23)

The user reports all headphone and speaker audio worked as intended. New
sessions `00065e879b6e0aac-00` and `00065e87be98c776-00` both exited cleanly.
The first initialized without playback. The second records connected headphones,
manual headphone selection at 10%, two headphone tones, a complete 1,152,000-byte
channel WAV, subsequent Stop requests, selection back to speakers and a third
completed tone. Audio ended ready with no owner, error or underruns.

Three longer streams were canceled: handles 2/3 on headphones and 4 on speakers.
No complete 11,520,000-byte one-minute stream is evidenced, so the five-minute
native endurance target is **incomplete**, not passed. Headphones still reported
connected at exit; disconnected-jack indication is not established. The user's
listening report accepts bounded headphone/speaker operation; individual
left/right and speaker-isolation observations were not separately enumerated.

All 185 preboot files remain unchanged except `.srvros/meta`; eight new session
logs were added. Offline exFAT/allocation checks pass: 193 files, 366 clusters,
no leaks/mismatches/errors. Ubuntu returned with empty one-shot and verified
payload/recovery hashes. Return evidence is retained in this run's separate
backup outside `build`; the earlier speaker acceptance remains intact.

The user explicitly confirmed the early stops were intentional, with no playback
problem. They are not failures, but do not complete the full-duration target.

## Read-only telemetry trial ready (2026-10-23)

Release `eee15036f4172920`, compiled clean source `60fc97a`, is installed for
attended run `3f703367ab650765`. MR87 adds captured CPU signature `40651` and
the exact Air6,1 DSDT/GPU/PCH single-fan profile. The v59 unscaled desktop keeps
brightness/audio and enables only CPU temperature and SMC fan reads. No fan
control, EC/AML, battery, SMC MMIO or IRQ6 ownership is enabled. See the
[telemetry checklist](../macbookair6-1-telemetry.md). Native readings remain pending.

Full build, USB image, core/desktop host suites, 63 boot-profile tests and docs
checks pass. The actual DSDT matcher passes. The exact staged GPT VM passes
two desktop sessions, save/reopen, preferences, tone/WAV playback with zero
underruns, saved device status and clean unmounts. Generic QEMU correctly
reports thermal/SMC unavailable; this does not accept native readings.

USB `6ADA-85BB` remains unmounted with a 193-file baseline; offline exFAT and
allocation checks pass (366 clusters, zero leaks/mismatches/errors). Installed
payload/recovery and independent backups are hash-verified. Console recovery
`c35475f62f154f1e` and accepted audio `0533c0dfc63d104f` are retained. Ubuntu
remains default and no reboot is armed. Run records, captures and VM evidence
are stored locally and copied outside build before boot.

## Read-only telemetry return (2026-10-23)

The user reports all sensors displayed fine. Matching session
`00065e8823afc352-00` on release `eee15036f4172920` ran about 82 seconds and
exited cleanly with no collector attention. Saved snapshots show boot-core
57–63 C, package 58–64 C and one fan at 1175–1231 RPM. Both providers reached
81 samples; every saved device sample was less than one second old. SMC ended
with 84 transactions, zero failures and error zero. Audio tone activity occurred
with final audio ready/error zero; PCM stayed idle in the saved snapshots.

This accepts basic native read-only temperature/fan display and fresh sampling.
The full two-minute file/audio coexistence checklist, Pause/Refresh/reopen and
screenshots are not independently established. No new document or screenshot
was saved. This is not thermal calibration, fan-control or cooling acceptance.

All 193 preboot files remain; only `.srvros/meta` changed and four matching
session logs were added. Offline checks pass: 197 files, 371 clusters, no leaks,
bitmap mismatches or errors. Ubuntu returned with an empty one-shot; installed
candidate/recovery and backup hashes are verified. Run records and return logs
are retained outside build under the candidate's evidence/trial directory.

## Battery/AC passive preflight ready (2026-10-23)

Run `10567bbea85fe3e2`, release `9b528e09c13e1d4b`, clean source `f6bab3e`,
is installed unarmed. Fresh DSDT/ECDT/FADT match the original unit capture.
Offline loading of nine AML tables with private GNVS/SMBus PCI snapshots matches
the EC namespace/resources with zero errors, denials, writes or leaked allocations.
Full initialization and runtime permissions remain unaudited; this result is not
live battery telemetry. See the [battery bring-up note](../macbookair6-1-battery.md).

The distinct v60 automatic test records only exact-gated PM1, GPE and EC-status
reads, saves to USB, unmounts and returns to Ubuntu. Leave AC connected. It does
not issue EC commands, read EC data, initialize AML or own SCI. Native handoff
state is needed before a later bounded battery-sampling profile can be admitted.

Build/USB image, core/desktop suites, 64 staging tests, 24 collector tests,
passive-port host checks, actual resource-gate/mutation tests and docs checks
pass. Exact-stage GPT VM confirms refusal without native access, saved logs,
clean filesystem/unmount and automatic reset. Native preflight remains pending.
USB has 197 baseline files; offline allocation passes with 371 clusters and
zero leaks/mismatches/errors. Installed/independent payload hashes are verified;
console recovery `c35475f62f154f1e` and accepted telemetry `eee15036f4172920`
remain available. Ubuntu stays default with empty one-shot. Evidence is backed
up outside build before boot; no reboot has been requested.

## Battery/AC passive preflight return (2026-10-23)

Matching session `00065e8886bf8b00-00`, release `9b528e09c13e1d4b`, records
all 36 passive reads, zero writes and `completed=yes`, with no collector attention.
Ubuntu returned on a new boot with empty one-shot. PM1 control/enable are zero,
status is `8001`, and EC status is zero (idle at the snapshot). GPE enable
byte 12 is `40`, byte 14 is `04`, and every other enable byte is zero: enabled
sources `66` and `72` hexadecimal. Their pending status bytes were zero; other
masked GPE status bits were set. Nothing was acknowledged or changed.

This accepts the passive firmware handoff capture only. It does not prove
battery/AC readings, event ownership, AML initialization or EC/SCI teardown.
The complete snapshot is retained for the independent runtime audit.

All 197 prior files remain; only `.srvros/meta` changed and two matching session
logs were added. Offline exFAT/allocation checks pass: 199 files, 374 clusters,
zero leaks/mismatches/errors. Installed and independent recovery/payload hashes
are verified. Return records and logs are backed up outside build. No boot is armed.

## Bounded battery/AC runtime ready (2026-10-23)

The independent connected audit is complete for the first bounded profile;
native operation remains pending. Release `674c597b082f2494`, clean source
`308e4bd`, is installed unarmed for run `5b60eacb23867c83`. See the
[runtime audit](../macbookair6-1-battery.md#connected-runtime-audit-and-bounded-sampling-candidate)
for exact GNVS/PPMT/OpRegion, PCI, EC and query permissions. Offline initialization,
allowed queries and EC detach pass in all four modeled lid/AC states. GPIO stays
denied. `_Q10`, `_Q80`, absent Air7,2 methods and unknown queries are refused.

v61 uses its own native binary and refuses service, transitions and mounted-USB
entry. It parks USB, attempts five AC-connected samples, retires and saves logs
before automatic Ubuntu return. The first native trial may expose another
ownership/refusal condition; missing final logs remain failure/incomplete evidence.
No desktop battery provider or charge-control access is admitted.

Full build/USB image, native binaries, core/desktop suites, 65 staging tests,
captured firmware/mutation tests and docs checks pass. QEMU passes five-sample
success, denied access, transport fault, failed foreign-event restoration and
EC-disconnect deadline containment; separate Global Lock handoff/release pass.
The exact staged GPT VM refuses unsupported hardware, saves logs, unmounts and
returns. These are runtime regression results, not native sampling acceptance.

USB has 199 baseline files; offline exFAT/allocation pass (374 clusters, zero
leaks/mismatches/errors). Installed/recovery and independent backup hashes pass;
Ubuntu remains default with empty one-shot. Console recovery `c35475f62f154f1e`,
accepted desktop telemetry `eee15036f4172920` and passive preflight
`9b528e09c13e1d4b` are retained. Raw audit and run evidence are copied outside
build before boot. Leave AC and USB connected for the automatic trial.

## Bounded battery/AC runtime return (2026-10-23)

Release `674c597b082f2494` (compiled `308e4bd`), matching session
`00065e88fbc08db1-00`, returned without an exit log or five-sample completion
receipt. The collector flags the missing exit. Saved `boot.log` and
`session-a.log` end immediately before the unattended battery probe; they do
not establish entry, sampling, retirement or the cause of the interruption.
This trial is incomplete/failed acceptance, not a native battery pass.
The user confirms automatic recovery; the screen reset too quickly to capture.

All 199 prior USB files remain; only `.srvros/meta` changed and the two matching
logs were added. Offline exFAT and allocation checks pass: 201 files, 377 clusters,
zero leaks, bitmap mismatches or errors. Ubuntu returned on a new boot, with
empty one-shot and installed payload/recovery hashes verified. Evidence and
updated run records are retained outside build. No reboot is armed.

Next work is to locate the stopping point using the screen observation and
bounded diagnostics while keeping USB parked during uncertain EC/SCI ownership.
Do not widen firmware permissions or enable desktop battery startup based on
this result. The existing host/VM passes remain regression evidence only.

## Failure-display follow-up scope (2026-10-23)

Based on main `b17b80e`, this unit owns a bounded terminal failure display in
shared `battery_runtime.c`, its small delay helper and host/VM tests. Other open
work (Air7,2 PR88 input timing, Pro11,2 PR19 Wi-Fi) was inspected; no overlapping
battery-runtime scope was found. Automatic contained faults retain the existing
storage interlock and interrupt/source masking, enter the emergency console,
show reason/setup/worker/EC details for 30 seconds, then reset. Clock failure
shortens the pause through the existing per-delay iteration bound. Mounted/I/O
faults still halt. No firmware permissions or successful sampling path change.
Next checkpoint: host/VM validation and a new exact staged Air6,1 trial.

## Failure-display candidate ready (2026-10-23)

Release `b9a2d264ad3a0dd2`, clean source `453e063`, is installed unarmed.
Full build/USB image, both native battery binaries, VM binary, core/desktop
suites, 65 staging tests and docs checks pass. Host tests cover all 301 delay
completion/failure boundaries. Q35 confirms readable framebuffer diagnostics,
interrupts disabled and 30.009-second hold before automatic reset; a parked
filesystem fault also resets after the hold with outside bytes preserved.
An active scratch-I/O fault blocks reset and skips the delay, retaining storage;
offline contents/exFAT pass. Normal sampling passes. The exact staged GPT VM
refuses nonmatching native hardware, saves logs, unmounts and returns.

The USB baseline is 201 files; offline allocation passes with 377 clusters and
zero leaks/mismatches/errors. Current/recovery payload hashes and Ubuntu default
are verified, with no one-shot armed. Console recovery `c35475f62f154f1e` and
accepted desktop telemetry `eee15036f4172920` remain installed. The superseded
`db7ce82008f4b9a6` payload was removed from EFI only after its independent backup
was verified. New payload and evidence are backed up outside build.

Leave AC and USB connected. If a failure screen appears, photograph its reason
and progress during the 30-second pause before automatic Ubuntu return. This is
another diagnostic attempt, not battery acceptance; a failure outside the
instrumented path can still reset without a report. Collect the matching session
before any further trial, and retain missing completion as incomplete/failure.

## Failure-display native return (2026-10-23)

Release `b9a2d264ad3a0dd2` (compiled `453e063`), session
`00065e8929e2f258-00`, has boot/session-a logs but no final report. The user's
photo now identifies terminal containment during `namespace-initialize`:
`denied map address=0xfed1c000 bytes=16384`, operation region `\RCRB`, while
executing `\_SB.PCI0.LPCB.HPET._STA`. The screen shows errors=4, denied=1,
EC phase=6/error=0, queries=0, no SCI deliveries, worker completed=2/in-flight=0,
and the photograph prompt before automatic reset. It confirms the diagnostic
screen is visible on this unit; exact physical pause duration is not measured.
Battery sampling and clean retirement remain unaccepted.

The captured DSDT's HPET `_STA` reads HPAE in the DWORD at RCRB+`3404`
(`fed1f404`). The original connected replay already explicitly allowed and
logged this read with value `80`; the earlier claim that HPTC AML access was
unneeded was an audit-summary/native-policy mistake. The Air6,1 gate currently
excludes both the opaque RCRB mapping and that exact DWORD read. Next scope is
to admit only the captured read, test width/address/write refusals and replay
initialization; do not permit arbitrary RCRB accesses or writes. No code gate
has been widened in this closeout.

All 201 prior USB files remain; only `.srvros/meta` changed and two matching
logs were added. Offline exFAT/allocation pass: 203 files, 380 clusters, zero
leaks/mismatches/errors. Installed and independent payload/recovery hashes pass;
Ubuntu returned on a new boot with empty one-shot. Photo, logs and return records
are retained outside build. No reboot is armed.

## HPTC correction scope (2026-10-23)

Based on main `6e608b0`, this unit owns the native HPTC read correction in
`battery_native_platform.c` and the shared pure predicate/profile tests.
Open PR88 input timing and PR19 Wi-Fi were inspected; no overlapping battery
scope was found. Admit the opaque captured RCRB region and only a 32-bit read
at HPTC `fed1f404`, already sanity-checked by native admission. All other MMIO,
widths and writes remain denied. Air7,2 retains its existing equivalent policy;
GPIO stays denied for Air6,1. Re-run connected captures, host/VM checks and an
exact staged five-sample trial with the existing 30-second failure display.

## HPTC correction candidate ready (2026-10-23)

Release `15f29b76df51bab0`, clean compiled source `9316f8a`, uses the captured
HPTC DWORD read with the existing exact firmware/resource admission and bounded
five-sample owner. Native sampling remains pending. GPIO, other chipset accesses
and writes remain denied; the 30-second terminal failure display is retained.

Full build/USB image, native/VM binaries, core/desktop suites, 65 staging tests
and docs checks pass. Both compiled policy profiles exercise every RCRB byte,
width and write refusal; the Air6,1 platform test checks mapping boundaries,
actual memory-access refusal and captured table mutations. Connected replay in
all four modeled lid/AC states initializes with zero errors/denials/leaks and
reads only HPTC `fed1f404` for chipset MMIO. Removing that snapshot permission
reproduces the HPET initialization failure. Q35 normal sampling and denied-access
containment pass; exact staged GPT VM refuses nonmatching native hardware,
saves logs, cleanly unmounts and returns. These are not physical acceptance.

The 203-file USB baseline and offline allocation pass (380 clusters, zero
leaks/mismatches/errors). Candidate/recovery hashes and Ubuntu default are
verified; no one-shot is armed. Console recovery `c35475f62f154f1e`, accepted
desktop telemetry `eee15036f4172920` and passive preflight remain installed.
Failed superseded `674c597b082f2494` was removed from EFI after verifying its
independent backup. New payload/run evidence is retained outside build.
Leave AC and USB connected; photograph any failure during the 30-second pause.
Collect the matching session and verify completion/retirement and USB integrity
before another trial. Missing final logs remain failure/incomplete evidence.
