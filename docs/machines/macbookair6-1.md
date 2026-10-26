# MacBookAir6,1 — local bring-up

Updated: 2026-10-26. State: **Native desktop/input, bounded concurrent USB file work, duplicate-ID repair, completed-workload rerun refusal and speed/acceleration persistence physically accepted. Native brightness adjustment across 5–100%, bounded speaker/headphone audio playback and basic read-only CPU/fan telemetry physically accepted. Bounded parked battery sampling, attended AC transitions and observed alarm refresh/retirement and bounded battery/Text USB-image coexistence accepted. Short battery sampling and the bounded Home desktop session with five Text saves and three battery lifetimes, plus a three-minute continuous Home desktop capture and attended desktop AC unplug/reconnect with indicator updates, are physically accepted.**

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
| Battery/AC | Passive handoff, short parked sampling and attended AC transitions with observed alarm refresh/clean retirement and bounded exclusive-image Text coexistence accepted | Bounded mounted Home Text/Start/Stop session accepted; three-minute continuous Home capture accepted; attended desktop AC transitions accepted; longer endurance and gauge accuracy remain open; ordinary startup stays off |
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

## HPTC retry native return (2026-10-23)

Release `15f29b76df51bab0` (compiled `9316f8a`), session
`00065e895200284c-00`, has boot/session-a logs but no final report. The user's
photo shows namespace setup completed and the runtime reached `phase=sample`.
The final interpreter counters are errors=0/denied=0. This confirms the narrow
HPTC fix resolves the previous initialization denial on this unit; it does not
accept sampling or clean runtime retirement.

The first observed SCI query is decimal 16 (`0x10`), classified as OS-branch
class 5 by Air6,1 policy 3 and refused before dispatch. Terminal reason is
`query policy`. The query's Darwin branch notifies SMB0; its alternate branch
acquires SMTX, reads SMBus state and can clear SSTS. The existing refusal is
intentional; do not simply whitelist the AML method or assume a captured OSYS
value makes every reachable branch safe. The shared runtime already has a
separately owned SMBus host-event handler, but Air6,1 does not yet admit that
profile. Next scope is an Air6,1-specific status/completion/alarm audit and
bounded event-owner integration with tests, retaining exact firmware and storage
gates. No event permissions are changed by this closeout.

All 203 prior USB files remain, with only `.srvros/meta` changed and two matching
logs added. Offline exFAT/allocation pass: 205 files, 383 clusters, zero leaks,
bitmap mismatches or errors. Payload/recovery hashes and Ubuntu default pass;
Ubuntu returned on a new boot with empty one-shot. Photo, logs and return records
are backed up outside build. No reboot is armed.

## Short SMBus capture scope (2026-10-23)

Based on main `eeca0e8`, this unit owns bounded Air6,1 host-event admission in
shared battery runtime/platform, v62 staging and directed host/VM tests.
Air7,2 PR88 input timing and Pro11,2 PR19 Wi-Fi remain independent scopes.
Reuse the existing serialized SMBus event/service/retirement mechanisms with
exact Air6,1 firmware gates. Admit only the automatic short parked capture;
retain v61 query refusal and reject mounted Home, image/files and ordinary
service. No AML `_Q10` dispatch or general EC register permissions are added.

## Short SMBus candidate ready (2026-10-23)

Release `bfde0423543f20d0`, clean compiled source `63bc06f`, uses the v62
short parked host-event capture. Main `2e2eb2b` / PR88 tap diagnostics was
integrated without conflicts; Pro PR19 remains separate Wi-Fi work. Native
battery sampling and runtime retirement remain unaccepted pending this trial.

Full build/USB image, core/desktop suites, 66 staging tests and 476 local docs
links pass. Seventeen Air6,1-policy VM cases cover completion, both OS branches'
alarms, unknown/late events, controller errors/timeouts, descriptor and query
refusals, old-profile refusal, no-event and native refusal. Each has clean offline
allocation; successful cases verify saved receipts and retirement. Air7,2
completion regression passes. After integration the full suites and Air6,1
completion were repeated. The exact staged EFI/GPT VM refuses nonmatching
hardware, saves logs, unmounts and returns. Captured-table replay has zero
errors/denials/leaks. These are modeled evidence, not native acceptance.

The 205-file USB baseline is unchanged; offline exFAT/allocation pass with
383 clusters and zero leaks/mismatches/errors. Payload/recovery hashes, Ubuntu
default and empty one-shot are verified. Console recovery `c35475f62f154f1e`,
accepted desktop telemetry `eee15036f4172920` and passive preflight remain.
Superseded failed `b9a2d264ad3a0dd2` was removed from EFI only after its independent
backup was verified. New payload/run/VM evidence is backed up outside build.

Keep AC and USB connected; no input is required. The capture waits up to
30 seconds for a natural SMBus event and fresh sample generation 3 or later,
with a 128-event ceiling. Photograph any failure during the 30-second display.
Automatic Ubuntu return should follow; missing final evidence remains failure
or incomplete. Collect this release and verify retirement and all prior files
before another trial. No reboot is armed.

## Short SMBus native return accepted (2026-10-23)

Closeout scope from main `5d195a9`: record this unit's returned v62 evidence;
no shared runtime or admission changes. Release `bfde0423543f20d0`, source
`63bc06f`, session `00065e89a7794b63-00` passes the native SMBus receipt validator.
Fresh generation 3, 45 completion events, three handled/zero unclaimed SCI
deliveries, three sample attempts/zero rejected and balanced 1/1 attach/detach
prove this bounded capture. Heap/errors/denied are zero; controller/resource
retirement and foreign-mask restoration are recorded. A complete exit log and
completed marker were saved. No alarms or acknowledgement writes occurred.

Accept short parked battery sampling and clean retirement with AC connected.
Gauge accuracy, alarms, AC transitions, sustained operation and desktop battery
monitoring remain open; ordinary startup stays off. Next recommended work is a
bounded attended AC unplug/reconnect capture with Air6,1 admission/query audit.

All 205 previous files remain, with expected metadata change and three new logs.
Offline fsck/allocation pass: 208 files, 387 clusters, zero leaks/mismatches/errors.
Ubuntu returned with consumed one-shot; current/recovery hashes and default pass.
Current stays `bfde0423543f20d0`, recovery `c35475f62f154f1e`. USB is unmounted;
no reboot is armed. Matching logs, validation and return state are backed up
outside build before another trial.

## Attended parked AC transition scope (2026-10-23)

From main `249bb97`, this unit owns the next bounded Air6,1 attended AC
transition capture: shared battery capture consumer/admission, boot recipe,
collector and directed host/VM tests. Reuse the accepted serialized service and
host-event owner with USB parked; keep ordinary/Home/file ownership disabled.
Audit captured AC query effects and retain exact firmware/access gates. PR99 log
capacity guards are consumed; Pro PR19 remains independent Wi-Fi work. Prepare
and validate before an attended unplug/reconnect trial; no reboot is armed.

## Attended AC candidate ready (2026-10-23)

Release `321ba9c91e681a4c`, clean source `95c5219`, prepares v63 with the existing
parked SMBus service and exact Air6,1 firmware/access gates. It requires three
coherent fresh samples in each connected/disconnected/reconnected phase, with a
120-second deadline per phase and 4096-event ceiling. No native AC-transition
acceptance is claimed yet; ordinary battery startup remains off.

Full build/core/desktop, 67 staging tests, 24 collector checks, transition/cache
and malformed receipt tests pass. Captured `_Q21`/`_PSR` replay in all four lid/AC
states initializes/disconnects with zero errors/denials/leaks. Seven directed VM
cases pass: normal transitions, mid-read incoherence, alarm refresh, unsupported
query containment, profile refusal, native refusal and a real 120-second phase
timeout. Short Air6,1 and Air7,2 capture regressions pass. An earlier alarm fixture
reconnected before three fresh readings and correctly saved an incomplete result;
its receipt and clean filesystem evidence are retained separately. The adjusted
fixture completes after post-alarm stabilization. Exact staged EFI/GPT refuses
nonmatching native hardware, saves, unmounts and returns automatically.

The 208-file USB baseline is unchanged; fsck/allocation pass with 387 clusters
and zero leaks/mismatches/errors. Candidate and recovery hashes, Ubuntu default
and empty one-shot pass. Console recovery `c35475f62f154f1e`, accepted short
capture `bfde0423543f20d0`, desktop telemetry and passive preflight are retained.
Superseded failed HPTC trial `15f29b76df51bab0` was removed from EFI after its
independent backup was verified. Payload, run and validation evidence are saved
outside build. No reboot is armed.

Start with AC connected. Wait for UNPLUG AC, unplug only the charger, then wait
for RECONNECT AC and reconnect it. USB stays connected throughout. Prompts repeat
every five seconds. Completion or phase timeout saves and returns to Ubuntu;
photograph any fault during the 30-second pause. Collect the matching transition
receipt and verify runtime retirement and USB integrity before another run.

## Attended AC return closeout (2026-10-23)

Scope from main `d753051`: record this unit's native v63 acceptance in the
machine ledger and battery guide; no runtime or access-gate changes. Open PR102
(Air7,2 filesystem scale) and PR19 (Pro Wi-Fi) remain independent work.

Release `321ba9c91e681a4c`, source `95c5219`, session `00065e8a183ee909-00`
passes the strict transition receipt: three fresh samples per connected,
disconnected and reconnected phase, generations 1–3/5–7/10–12. Disconnected
current is -1328000 µA; connected currents are zero. Twelve attempts have zero
rejections; 182 events include two alarms/two acknowledgements, no unknowns.
Attach/detach is 1/1 and retired heap/errors/denied are zero. Post-alarm freshness,
resource restoration, complete exit log and Ubuntu return are verified.

All 208 prior files are preserved except expected metadata replacement; three
new logs produce 211 files. Offline checks pass with 391 clusters and zero
leaks/mismatches/errors. Payload/recovery hashes and Ubuntu default pass; one-shot
is consumed. USB is unmounted and no reboot is armed. Evidence is independently
backed up outside build. Accept only bounded parked AC transitions and observed
alarm refresh, not gauge accuracy, sustained service or desktop ownership.
Next recommended scope is bounded mounted-USB/file coexistence before desktop
battery integration; retain exact Air6,1 admission and serialized retirement.

## Bounded battery/file coexistence scope (2026-10-23)

From main `d7ed773`, this unit owns Air6,1 admission to the existing parked
exclusive-image/Text file capture, its distinct v64 recipe and directed policy/VM
checks. Reuse the serialized SMBus owner, 512-event ceiling and client/runtime/
image retirement order. No new AML/register permissions, mounted Home admission
or ordinary battery startup. PR102 filesystem scale and PR19 Wi-Fi remain
independent; use this main baseline without importing either active change.
Prepare a fresh disposable USB image, exact-stage checks and retained recovery
before native acceptance. No reboot is armed.

## Bounded battery/file candidate ready (2026-10-23)

Release `fc1d1f4c45d30252`, clean source `cac24ef`, uses the v64 Air6,1
exclusive-image capture. It preserves exact firmware/AML permissions and the
512-event ceiling. Ordinary mounted Home and desktop battery startup remain off;
physical battery/file coexistence is pending this trial.

Full build/USB image, native/model kernels, core/desktop suites, 68 staging tests,
24 collector tests, five strict file-receipt tests and exhaustive admission
checks pass. Eight directed Air6,1 file VMs pass: full completion, recurring
alarms, Stop during read/ack, uncertain acknowledgement containment, event limit,
profile refusal and native refusal. Completion gives 60 edits/five file cycles
and generation 25; recurring alarms complete with 15 acknowledgements and fresh
generation 30. Both images and unrelated files verify. Three regressions pass:
Air6,1 short capture/AC transitions and Air7,2 file capture. Exact staged EFI/GPT
refuses unsupported native hardware, saves, unmounts and returns; missing USB
also returns without starting the probe.

The fresh 16 MiB image is the only added USB file. All 211 prior files are
preserved; 212-file preboot baseline verifies. Offline fsck/allocation and
headroom pass: 519 clusters, zero leaks/mismatches/errors. Installed candidate,
recovery and independent payload hashes pass; Ubuntu is default and one-shot
is empty. Console recovery `c35475f62f154f1e`, accepted AC capture
`321ba9c91e681a4c`, short sampling and desktop telemetry remain retained.
Superseded brightness-only EFI payload `59086591e526632b` was removed only after
its complete independent backup was verified. Run/payload/VM evidence is saved
outside build. USB is unmounted and no reboot is armed.

Keep AC and USB connected; no input or charger changes are required. Expect
60 automatic edits and five Text save/open cycles, then saved Ubuntu return.
Photograph any fault. On return collect this release before further work and
verify its complete receipt, exact inner image, all other files and both
allocation graphs. A quick return alone is not acceptance.

## Battery/file return closeout scope (2026-10-23)

From main `7638d57` (including PR102 filesystem scale), this closeout only records
the returned Air6,1 candidate and missed image preparation check. No shared
runtime/filesystem changes; Pro PR19 Wi-Fi remains independent. The installed
physical candidate predates PR102 and stays identified by its own source/release.

## First battery/file return: image preparation refused (2026-10-23)

Release `fc1d1f4c45d30252`, source `cac24ef`, session `00065e8a98eec3bc-00`
saved `completed=NO` with `battery media parked=NO fault-reset=NO`. The exclusive
image was refused before outer unmount, battery runtime entry or Text/file work.
This is failed preparation, not coexistence acceptance or a runtime failure.

The 16 MiB image has the correct size/valid length but its 128-cluster FAT chain
jumps from cluster 392 to 394. The read-only native-driver `--battery-image`
check reproduces `image-range: REFUSED`. General allocation and fsck passed
before boot, but the dedicated contiguous-extent check was omitted. A valid
filesystem and matching image bytes do not prove exclusive-range admission.

All 212 prior files remain unchanged except expected `.srvros/meta`; the inner
image is byte-identical to its fresh baseline. Three logs bring the total to
215 files. Outer fsck/allocation pass (523 clusters, zero leaks/mismatches/errors);
inner checks pass (two files, seven clusters). Payload/recovery hashes, Ubuntu
default and consumed one-shot verify. USB is unmounted, no reboot is armed, and
matching logs/image/layout/return evidence is independently backed up.

Next prepare a fresh contiguous image, preserving this failed run's evidence,
and require `check_exfat_allocation.py --uuid 6ADA-85BB --battery-image` to pass
on the unmounted real drive before installation/boot. Do not relax exclusive
range checks or treat another general allocation pass as a substitute. Any new
payload built on the later integration baseline needs its own exact-stage
validation; this run does not accept the filesystem-scale changes from PR102.

## Contiguous-image retry scope (2026-10-23)

From main `c43d27c`, this preparation only replaces the unused disposable image's
allocation and repeats exact-payload/media checks. Reuse installed release
`fc1d1f4c45d30252`, clean compiled source `cac24ef`, without rebuilding or importing
PR102 filesystem changes. PR106 package-energy measurement and PR19 Wi-Fi remain
independent. Preserve the failed native evidence and image bytes; no kernel or
firmware permissions change. Native battery/file coexistence remains pending.

Holding the original image while writing a new copy first encountered a second
one-cluster gap. Holding both unused copies produced a contiguous 128-cluster
image at byte offset 88473600 (first cluster 653, NoFatChain set). Both temporary
files were removed only after byte comparison. Final real-drive `--battery-image`
passes, independent FAT layout inspection has no breaks, and all 215 original
file hashes still match. Offline fsck/allocation passes with 523 clusters and
zero leaks/mismatches/errors. The retry prelaunch script now requires the extent
check explicitly; a general filesystem pass cannot satisfy it.

The repeated exact-stage EFI/GPT rehearsal passes: native platform refusal,
saved diagnostics, verified disposable files, clean unmount and return. Installed
candidate/recovery hashes, Ubuntu default and empty one-shot pass. The final
prelaunch check verifies AC, unchanged host boot, USB UUID, all 215 file hashes,
fsck and the required contiguous extent. Retry evidence has its own durable
backup alongside the unchanged payload and failed run. USB is unmounted and no
reboot is armed. Keep AC/USB connected; the automatic 60-edit/five-save test
needs no input. Collect its matching session before any subsequent trial.

## Battery/file coexistence native acceptance (2026-10-23)

Closeout from main `94c9b15`: update only this unit's capability records and
current guide. PR106 power measurement and PR19 Wi-Fi remain independent.
Release `fc1d1f4c45d30252` / source `cac24ef`, retry `bcbabcafe3b8c68c`, session
`00065e8ac0a116ff-00` passes the strict receipt and exact inner-image validation:
60 edits, five file cycles/20 operations, fresh generation 24 and 360 completion
events. All 24 SCI deliveries were handled, none unclaimed; no alarms occurred.
Three descendants, runtime and image retire in order; attach/detach 1/1 and
heap/errors/denied zero. This accepts bounded exclusive-image coexistence only.

Prior files are preserved except intended image/meta changes; three new logs
produce 218 files. Outer/inner fsck and allocation pass (527/14 clusters), no
leaks/mismatches/errors; the image extent remains contiguous. Ubuntu default,
consumed one-shot and candidate/recovery hashes pass. USB is unmounted, no reboot
is armed, and evidence is backed up separately from the failed preparation.
Next scope is explicit mounted Home ownership before ordinary desktop battery
startup. No acceptance is inferred for the later PR102 filesystem implementation.

## Explicit mounted Home sampling scope (2026-10-23)

From main `7d2b903`, own only the Air6,1 short held-Home diagnostic admission,
normal-return refusal boundary, distinct boot recipe and directed tests. Reuse
the mount hold and serialized service/retirement; no desktop clients or ordinary
startup. Reset remains disabled with mounted storage; uncertain runtime halts
without filesystem cleanup. PR106 power measurement and PR19 Wi-Fi remain
independent. Consume PR102 filesystem baseline with fresh build and exact-stage
checks; no native acceptance is inherited for the new payload.

## Mounted Home candidate ready (2026-10-23)

Release `30b0d713f1a9ed55`, clean compiled source `c69cd23`, uses v65 short
held-Home sampling. Exact firmware/access gates remain unchanged. It enables
neither desktop clients nor ordinary service; uncertain faults retain the mount
and halt, with no automatic reset or live filesystem cleanup. Native acceptance
remains pending.

Full rebuild/USB image, core/desktop suites, exhaustive admission/query-receipt
checks, 69 staging tests, 24 collector tests and four receipt tests pass. Eight
Air6,1 mounted ownership VMs cover normal/alarms, effect-free refusal, unsupported
platform/profile, unmounted entry and two retained-fault cases. Four full-boot
captures cover success/alarm, saved query refusal and uncertain acknowledgement
halt. Air7,2 Home and Air6,1 image/file regressions pass. Exact staged EFI/GPT
and missing-workspace rehearsals pass. An initial harness classification wrongly
expected image revocation for v65; checker fix `d3a1f9e` separates the profiles
and the repeated exact run passes. That failed harness evidence is retained.

The existing used image and all 218 USB file contents are unchanged. Offline
fsck/allocation/headroom pass (527 clusters, zero leaks/mismatches/errors).
Installed/recovery hashes, Ubuntu default and empty one-shot verify. Console
recovery `c35475f62f154f1e`, accepted image capture `fc1d1f4c45d30252`, AC capture,
short sampling and desktop telemetry remain retained. Superseded audio-only EFI
payload `0533c0dfc63d104f` was removed only after verifying its independent backup.
Run, payload and VM evidence are backed up outside build. USB is unmounted and
no reboot is armed.

Keep AC/USB connected; no typing or charger changes are required. Expect a few
fresh samples with Home held, then retirement, saved logs and Ubuntu return.
If it halts, photograph the screen and report it; do not treat a reset or missing
completion as success. Collect matching `battery_home`, unchanged prior files,
filesystem/allocation and recovery evidence before another trial.

## Mounted Home sampling accepted (2026-10-23)

Close out the authorized v65 run on main `de449b3` (MR109). Scope is this
unit's evidence and current status; PR106 power work and PR19 Wi-Fi remain
independent. No shared runtime or device admission changes are needed.

Release `30b0d713f1a9ed55`, compiled `c69cd23`, session
`00065e8b198ee6ba-00` completes the strict native `battery_home` receipt:
three fresh generations, 45 completion events, three handled/zero unclaimed SCI
deliveries and no alarms. Runtime retires with attach/detach 1/1 and zero
heap/errors/denied; the same Home mount is held through retirement, then released.
The completed capture saves its exit log and returns to Ubuntu.

All 218 prior files survive; only expected `.srvros/meta` changes, with three
new logs. Offline fsck and allocation/headroom pass: 221 files, 531 clusters,
zero leaks/mismatches/errors. Candidate/recovery hashes and Ubuntu default pass;
the one-shot is consumed. Raw logs, strict receipt, file comparison and recovery
checks are backed up outside build. USB is unmounted and no reboot is armed.

This accepts short mounted Home sampling only, with no desktop clients or AC
changes. Ordinary desktop startup remains off; sustained coexistence and gauge
accuracy are not accepted. Next is the explicit bounded Home desktop session
with Text saves and repeated Start/Stop before ordinary desktop integration.

## Bounded Home desktop session scope (2026-10-23)

From actual main `ce1c0a6` (including PR106 Text polling changes), own only
Air6,1 admission to the existing bounded Home controls/file diagnostic, its v66
recipe and directed validation. Reuse three service lifetimes, five control
requests, 60 Text edits/five saves, 512 aggregate events and existing deadlines.
Preserve the exact mount through every epoch and client/runtime retirement.
Ordinary/continuous desktop admission stays off, and uncertain mounted failures
halt without cleanup/reset. PR19 Wi-Fi remains independent. The prior v65 run
is collected and archived; this new payload requires its own physical evidence.


## Bounded Home session candidate validated (2026-10-23)

Release `f60d9b426e2693f3`, clean compiled source `12aad26`, uses v66
`battery-home-session`. Full build/USB image, core/desktop suites, 70 staging
tests, 25 collector tests, 78 receipt tests and exhaustive admission checks pass.
Six Air6,1 boot fixtures cover completion, query/profile refusal, two retained
faults and delayed EC setup with throttled USB writes. Completion proves five
controls/three lifetimes, 60 edits/five cycles/20 operations and exact saved
bytes. Slow checkpoints took 7.99–8.29 seconds without changing deadlines.
Air7,2 Home-session and Air6,1 short-Home regressions pass. Exact EFI/GPT and
missing-workspace rehearsals pass; the latter intentionally emits no standard
result JSON, so its PASS output and serial evidence are retained separately.

All 221 existing USB files were preserved while adding a fresh `battery-study`
anchor. Offline allocation/headroom pass: 222 files, 533 clusters, no leaks or
mismatches. Superseded passive preflight EFI payload `9b528e09c13e1d4b` was
removed only after verifying its independent payload backup to make install
space. Console recovery and accepted runtime captures remain retained.

Physical acceptance is pending. Keep AC/USB connected and provide no input;
normal completion/refusal saves and returns to Ubuntu. Photograph any halt.
Require matching `battery_home_session`, five saved files containing `AA`,
unchanged prior file contents, clean allocation and consumed one-shot. Final
installation and unarmed state are recorded in the local run/launch records.

## Bounded Home session failed, clean retirement (2026-10-23)

Close out only this unit's native evidence on main `db10404` (MR111).
PR112 owns compositor idle work and PR19 Wi-Fi; no shared implementation changes
are made in this closeout. Session `00065e8b68f3ad24-00`, release
`f60d9b426e2693f3` / compiled `12aad26`, is **failed**, not accepted.

The generated workload stopped at 12 generated/11 presented edits, zero saves
and one Start request. Initial EC setup took 4.941 seconds. At failure the Start
was still waiting for fresh generation 2; Text reached its first save with
`waiting=1`, and `tool_file_capture_begin` rejects a pending control. This points
to a diagnostic sequencing race, requiring a reproducer with delayed second
sample before implementing a fix. The existing slow-setup/storage VM did not
cover this native readiness ordering. Do not simply widen hardware permissions
or classify the clean return as workload success.

The service completed generation 2/30 events, with two handled and zero unclaimed
SCI deliveries, then retired: attach/detach 1/1, zero heap/errors/denied. Three
clients retire, Home releases, the failed report is saved and Ubuntu returns.
All 222 prior files are unchanged except expected `.srvros/meta`; three new logs
bring the total to 225. The study still contains only its original anchor; no
Text output was saved. Offline fsck/allocation/headroom pass (537 clusters,
zero leaks/mismatches/errors), installed/recovery hashes pass and the one-shot
is consumed. Raw evidence, study contents and file checks are backed up outside
build. USB is unmounted; no reboot is armed and no retry is prepared.

Next: make generated saves wait for initial Start readiness, reproduce delayed
fresh generation 2, and rerun directed session tests before preparing a retry.
Keep this failed evidence and existing study directory intact during preparation.

## Start/save sequencing repair scope (2026-10-23)

From main `89b19ec`, own the generated Text capture's readiness sequencing and
VM delayed-second-sample fixture/regression. Preserve all runtime admission,
firmware permissions, owner/mount lifetime and deadlines. PR112 owns compositor
idle work; this fix avoids its files. PR19 Wi-Fi remains independent. Reproduce
the collected v66 failure before correcting the capture, then prepare a fresh
study directory while archiving the previous failed study without overwrites.

The delayed-generation-2 fixture reproduced the same 11-edit/zero-save failure
before the Text fix. PR112 merged while this work was underway; main `180b542`
has now been integrated without overlap. Rebuild and session regressions include
its compositor idle-wait changes; no prior native acceptance is inherited.

## Start/save retry ready for physical validation (2026-10-23)

Release `7fe7d3d20d91997c`, clean compiled source `5086cba`, fixes generated
checkpoint sequencing without changing runtime permissions or deadlines. The
four-second delayed-generation fixture reproduces the old 11-edit/zero-save
failure; with the fix it verifies fresh Start before write submission, all
60 edits/five exact saves and three service lifetimes. An excessive eight-second
delay still fails the existing Start deadline without submitting a write.

Full build/USB image, core/desktop, 70 staging, 25 collector and 78 receipt tests
pass. Seven Air6,1 session fixtures cover normal/delayed completion, slow setup
and storage, query/profile refusal and retained faults. Air7,2 delayed-session
and Air6,1 short-Home regressions pass. Exact EFI/GPT save/unmount/return and
missing-workspace refusal rehearsals pass. PR114 includes main `180b542`;
PR115 logger timing and PR19 Wi-Fi are independent and not included.

The failed study is preserved as `battery-study-failed-73239eaf664ab4ca`; a fresh
anchor brings the USB baseline to 226 files/539 clusters with clean allocation.
Superseded short SMBus payload `bfde0423543f20d0` was removed from EFI only after
verifying its durable backup; accepted mounted Home and console recovery remain
installed. Payload and evidence backups live outside build. Run
`9055d4b29173fe40` records final installation and unarmed state. Keep AC/USB
connected with no input; normal completion returns to Ubuntu, uncertain mounted
faults halt for a photograph. Native acceptance and ordinary battery startup
remain pending/off respectively; collect this matching run before another trial.


## Bounded Home Start/save retry accepted (2026-10-23)

Close out this unit's run on main `adf40df` (MR114), touching only this record
and the current battery guide. PR115 logger timing and PR19 Wi-Fi remain
independent. Session `00065e8bbf429e4a-00`, release `7fe7d3d20d91997c` /
compiled `5086cba`, passes the strict native Home-session receipt.

The first checkpoint deferred at serial 12 until Start observed fresh generation
2, then submitted its write. All 60 edits, five exact `AA` files, 20 worker
operations and five controls across three lifetimes completed. The service
processed 180 events with zero alarms; SCI totals are 12 handled/zero unclaimed.
Every lifetime detached cleanly with zero heap/errors/denied; all three clients
retired, the final active service retired, and Home released before saved return.

All 226 prior USB files retain their bytes except expected `.srvros/meta`;
five study files and three logs bring the total to 234. Offline fsck and
allocation/headroom pass: 548 clusters, no leaks/mismatches/errors. Installed
payload/recovery hashes and Ubuntu default pass; the one-shot is consumed and
USB is unmounted. Matching raw evidence, exact study bytes and verification
records are backed up outside build. No next trial or reboot is armed.

This accepts the bounded generated Home/Text/control workload only. Physical
input latency, continuous desktop monitoring and gauge accuracy remain separate.
Ordinary battery startup stays off. Next is a bounded continuous Home desktop
trial, reusing the existing ownership/retirement contract and requiring its own
Air6,1 fixture and native acceptance before any ordinary-startup change.

## Bounded continuous desktop scope (2026-10-23)

From main `4a991df`, including MR115 logger timing and MR116 native closeout,
own only Air6,1 admission to the explicit three-minute Home desktop capture,
its separate v67 stage and directed fixtures. Preserve the 240-second watchdog,
checked client/runtime/logger retirement and mounted-fault halt policy. Ordinary
startup, indefinite sessions and attended Home transitions stay excluded. Keep
firmware/query permissions unchanged. PR19 Wi-Fi has independent staging changes;
preserve its recipes and avoid xHCI/network files. Previous run is collected;
this trial requires its own evidence and must preserve all current USB files.

## Continuous Home candidate validated (2026-10-23)

Release `a18d72aecdf808c7`, clean compiled source `503dd05`, selects v67.
Full build/USB image and core/desktop suites pass, as do exhaustive native
admission checks, 71 staging, 26 collector and four continuous receipt tests.
Air6,1 completion ran 189.6 seconds, advanced generation 1→76 with 2440 events,
saved the desktop screenshot and periodic/final logs, and retired five clients
and runtime cleanly. Profile/query refusals and uncertain-acknowledgement halt
cases pass. Air7,2 continuous capture and Air6,1 delayed Start/save regressions
pass. Exact EFI/GPT save/unmount/return and missing-workspace rehearsals pass.
The VM screenshot was inspected; native indicator accuracy remains untested.

Run `eddc64efe16de005` preserves all 234 USB files unchanged, including the
accepted and failed study artifacts. Offline fsck/allocation/headroom pass with
548 clusters and no leaks/mismatches/errors. The failed initial v66 EFI copy
`f60d9b426e2693f3` was removed only after verifying its independent backup;
accepted v66 `7fe7d3d20d91997c`, short Home and console recovery remain retained.
Payload and evidence backups are outside build. Final installation and unarmed
state are recorded in the run/launch files; native acceptance is pending.

Keep AC/USB connected and provide no input. Allow three minutes plus startup,
screenshot saving and retirement before automatic Ubuntu return. Photograph any
halt. Collect the matching strict `battery_home_desktop` proof, all four logs,
new screenshot, prior-file hashes and offline allocation before another run.


## Continuous Home desktop accepted (2026-10-23)

Close out this unit's run on main `2702b41` (MR117), touching only acceptance
records/current guides. PR118 storage wait work and PR19 Wi-Fi are independent
and are not part of this tested payload. Session `00065e8c7ab70670-00`, release
`a18d72aecdf808c7` / compiled `503dd05`, passes the strict native continuous
Home receipt. The desktop idle window lasted 180.074 seconds; sampling covered
189.823 seconds and advanced generation 1→71 with 1064 events and zero alarms.
All 71 SCI deliveries were handled, with zero unclaimed. Five lifetime members
retired, the service detached 1/1 with zero heap/errors/denied, the logger retired
and Home released before saved Ubuntu return.

All four logs and the new 1366×768 screenshot are collected and hash-verified.
Visual inspection confirms ordinary Text and an `AC · 98%` indicator; it does
not calibrate gauge accuracy. All 234 prior files retain their bytes except
expected `.srvros/meta`; four logs and one screenshot bring the total to 239.
Offline fsck/allocation/headroom pass: 586 clusters, no leaks/mismatches/errors.
Installed/recovery hashes and Ubuntu default pass; the one-shot is consumed.
USB is unmounted. Evidence and screenshot copies are backed up outside build;
no new trial or reboot is armed.

This accepts the explicit three-minute continuous capture only. Ordinary battery
startup, longer endurance, attended input latency and gauge accuracy remain
unaccepted. Next is a separately bounded attended Home AC unplug/reconnect trial
to verify desktop indicator updates and retirement across power transitions.

## Attended Home AC transition scope (2026-10-23)

From main `d5c080d`, own only Air6,1 admission to the existing attended Home
AC transition capture, its separate v68 recipe and directed tests. Keep exact
firmware/query permissions, mount/client ownership and checked retirement;
ordinary startup and indefinite sessions remain excluded. Preserve the existing
three fresh readings per phase, two-minute phase limit and 480-second capture
watchdog, plus the Air6,1 4096-event ceiling. PR118 owns USB wait work and PR19
Wi-Fi; avoid their driver files and preserve their independent recipes. The v67
run is collected/accepted. This trial starts on AC, prompts for unplug/reconnect,
and must retain USB throughout; each phase saves its own screenshot.

## Attended Home AC candidate validated (2026-10-23)

Release `5cd210467e3a10fd`, clean compiled source `fe85b9c`, selects v68.
Full build/USB image, core/desktop, exhaustive capability checks, 72 staging,
26 collector and four receipt tests pass. Five Air6,1 transition fixtures cover
completion, profile/query refusal, retained uncertain-acknowledgement fault and
missing-transition timeout. Completion takes 69.4 seconds with ordered phases
at generations 1–3, 9–11 and 25–27; three verified screenshots show
`AC · 86%` → `86%` → `AC · 86%`, with appropriate current direction. Screenshot
indicator/prompt regions were visually inspected. Timeout saves an incomplete
result and returns only after clean retirement; uncertain ownership halts.
Air7,2 transitions and Air6,1 three-minute continuous regressions pass. Exact
EFI/GPT save/unmount/return and missing-workspace rehearsals pass.

Run `f76a5bcce314a958` retains all 239 USB files unchanged. Offline fsck and
allocation/headroom pass: 586 clusters, no leaks/mismatches/errors. Superseded
short Home EFI payload `30b0d713f1a9ed55` was removed only after verifying its
durable backup; accepted v66/v67 and console recovery remain installed. Payload
and evidence backups are outside build; final install/unarmed state is recorded
locally. PR118 storage waits and PR19 Wi-Fi are not part of this tested source.

Native acceptance is pending. Start on AC; keep USB connected throughout. Wait
for the screen to request unplugging AC, then reconnect only at its next prompt.
Wait through screenshot saving without changing power early. No keyboard input
is required. Each phase allows two minutes; normal completion or checked
incompletion saves and returns. Photograph any halt. Collect matching strict
transition proof, all logs/three screenshots, previous file hashes and offline
allocation before another run. Ordinary startup remains disabled.

## Attended Home AC first run incomplete (2026-10-23)

Close out native evidence only on main `bbdbefb` (MR120); PR118 USB wait and
PR19 Wi-Fi work remain independent. Session `00065e8ccdec4fb4-00`, release
`5cd210467e3a10fd` / compiled `fe85b9c`, is incomplete, not accepted. Connected
phase 0 passed at generations 1–3 (`AC · 98%`, current zero), and one screenshot
saved. After prompting for phase 1, the capture timed out without accepting
three fresh disconnected readings. There is no reconnect-phase evidence. Human
observations of unplugging and the indicator are pending; do not infer a driver
fault or missed user action from this receipt alone.

Sampling reached generation 51 with 765 events, zero alarms and 51 handled/zero
unclaimed SCI deliveries. All five lifetime members retired; service detach was
1/1 with zero heap/errors/denied, logger retired, Home released and the failed
report saved before Ubuntu return. All 239 prior files retain their bytes except
expected metadata; four logs and one screenshot bring the total to 244. Offline
fsck/allocation/headroom pass: 624 clusters and zero leaks/mismatches/errors.
Installed/recovery hashes and Ubuntu default pass, the one-shot is consumed,
and USB is unmounted. Raw evidence and screenshot are backed up outside build.
No retry or reboot is armed. Preserve this run while resolving the phase-1 cause;
ordinary startup and attended Home transition acceptance remain disabled/pending.


## Attended Home AC retry accepted (2026-10-23)

Close out this unit's evidence on main `54862de` (MR121); no shared runtime
changes. PR118 USB wait and PR19 Wi-Fi remain independent. The user confirmed
that AC was not unplugged in the first v68 attempt. That incomplete evidence
remains preserved; an explicitly authorized same-payload retry used a fresh run
record and the 244-file baseline without source changes.

Run `f36692208f9cb16f`, session `00065e8d108fab51-00`, release
`5cd210467e3a10fd` / compiled `fe85b9c`, passes the strict native transition
receipt. In 47.686 seconds sampling advanced generation 1→18. Connected phase
accepted generations 1–3 (`AC · 98%`, current 0); disconnected phase accepted
7–9 (`97%`, −1,329,000 µA); reconnected phase accepted 14–16 (`AC · 97%`,
current 0). All three 1366×768 screenshots were copied, hash-verified and
visually inspected. They confirm the indicator and prompt states; percentage
calibration and sustained charging behavior are not claimed.

The service processed 272 events, including two alarms/acknowledgements/cache
invalidations. All 19 SCI deliveries were handled with zero unclaimed. Five
lifetime members retired, service detach was 1/1 with zero heap/errors/denied,
and logger/mount retirement preceded saved return. All 244 prior files retain
their bytes except expected metadata; four logs and three screenshots bring the
total to 251. Offline fsck/allocation/headroom pass: 728 clusters and zero leaks,
mismatches or errors. Installed/recovery hashes and Ubuntu default pass; one-shot
is consumed, USB is unmounted and no new trial is armed. Retry evidence is backed
up separately outside build, preserving the original incomplete run.

This accepts bounded attended desktop power transitions and indicator updates.
Ordinary battery startup remains disabled. Longer attended desktop/file/input
coexistence and endurance remain separate before any startup promotion.

## Attended desktop/file/input scope (2026-10-23)

From main `7b0d1a0`, including merged MR118 USB waits and MR122 AC acceptance,
own an explicit Air6,1 attended Home capture: five-minute minimum work interval,
periodic sampling/logging, screenshot and normal user-requested desktop exit.
An eight-minute owner overrun must halt with the mount retained, not force away
unsaved work. Require physical-input counts and successful ordinary Text save/open
receipts, preserving prior files. Ordinary startup remains off; firmware/query
permissions stay unchanged. PR123 network polling and PR19 Wi-Fi are independent;
preserve their main/staging recipes and avoid driver edits. Previous AC retry is
collected and accepted; this trial needs its own run and native evidence.

## Attended Home work candidate (2026-10-23; native pending)

Source `022d468`, v69 release `8245e30a2e9019ec`, prepares a five-minute
attended Home session with continuous battery sampling, typing, Text save/reopen
and Files work. At five minutes it saves a screenshot and prompts for normal
SrvrOS > Return to console; dirty Text retains its close veto. An eight-minute
owner deadline halts with live ownership retained. Ordinary startup and native
firmware/query gates remain unchanged. See the current battery guide for steps.

Local core/desktop build tests, 74 staging tests, 26 collector tests and five
strict receipt tests pass. Directed Air6,1 Q35 cases cover completion, early
exit, unsaved-work overrun, platform refusal and query refusal. Completion ran
305.895 seconds, generation 1→123, with 3,944 events and seven lifetime members;
Text bytes, directory metadata, filesystem/allocation and full retirement pass.
The overrun retains live ownership without reset or a completion claim; periodic
sampling continued through 479 seconds. Native-style collector recipe negatives
and the existing three-phase AC capture regression pass. Exact staged GPT boot
refuses native hardware in Q35, saves/unmounts/returns; missing Home returns before
starting the probe. These are VM results, not native endurance acceptance.

Run `e787736ee4592483` preserves the accepted 251-file USB baseline (728 clusters,
zero leaks/mismatches/errors). `attended-work.txt` is available as a new file.
Ubuntu and console recovery `c35475f62f154f1e` remain the recovery plan. The
superseded Start/Stop EFI copy was removed only after independent backup hashes
matched; accepted continuous and AC payloads remain installed. The next boot
must collect its own screenshot, periodic/final logs, input/file receipts, new
file contents, prior-file hashes and clean offline allocation before acceptance.

### Integrated candidate after MR123

Main `92722b7` landed during validation and is consumed by `6f25115`. The sole
merge conflict was the boot-smoke result dictionary; both attended-work and
network-probe fields are retained. Final v69 release `78fe53e8cacdb1ba` supersedes
the never-installed `8245e30a2e9019ec` candidate. Rebuild/core/desktop and all
105 staging/collector/receipt tests pass again. Completion (305.423 seconds,
generation 1→123, 3,944 events, seven members), early exit, both refusals, the AC
transition regression and both exact staged GPT/missing-Home checks pass on the
integrated source. The eight-minute overrun receipt remains from `022d468`;
battery runtime/desktop sources and the initramfs are unchanged by integration.
The earlier stage and evidence remain preserved. Native acceptance is still
pending; the same unarmed run and 251-file baseline apply to this final payload.

## Attended Home work first return: incomplete

Close out evidence on main `c4265f8`; no shared runtime changes. Run
`e787736ee4592483`, session `00065e8e0a5dafa7-00`, release `78fe53e8cacdb1ba`
(compiled `6f25115`) exited through the normal menu after 211.554 seconds.
The five-minute minimum and screenshot phase were not reached. There were 25
key presses and 1,225 pointer events. Text saved and reopened an 11-byte new
file successfully, but there is no Files-app listing receipt. The result is
incomplete, not accepted; whether the early exit was intentional is pending
human confirmation. No driver fault is inferred from this alone.

Battery generation advanced 2→81 with 1,231 events, two alarms/acknowledgements/
cache invalidations and 86 handled/zero unclaimed SCI deliveries. Four lifetime
members drained; runtime detach was 1/1 with zero heap/errors/denied. Logger and
mount retirement completed and the failed report was saved before Ubuntu return.
All 251 prior files retain their bytes except expected metadata; four logs and
the new Text file bring the total to 256. Offline fsck/allocation pass with 734
clusters and zero leaks/mismatches/errors. Installed/recovery hashes, Ubuntu
default and consumed one-shot pass; USB is unmounted. Evidence is preserved
outside build. No retry is armed; five-minute attended work acceptance remains
pending, and ordinary battery startup remains disabled.

## Screenshot receipt correction scope (2026-10-24)

Base `bf1b9a6` includes PR127 storage timing and PR125 TCP readiness. The
bounded objective is to emit the screenshot dimensions receipt with ordinary
non-traced desktops and exercise the real image verifier in the Home capture VM.
Scope: compositor capture logging, Home VM/collector regressions, this record
and the Air6,1 battery guide. PR19 at `7e70c51` owns independent Pro USB Wi-Fi
work; no storage, firmware, query permissions or startup policy changes planned.
Next checkpoint: corrected completion VM, exact staged GPT rehearsal and an
unarmed physical retry with separate Files-app use.

Retry `ab44de460a170c07`, session `00065e8e40c90399-00`, on release
`78fe53e8cacdb1ba` completed normally after 361.122 seconds: 55 keys, 1,356
pointer events, a successful 33-byte Text save/reopen and a saved valid BMP.
Battery generation advanced 1→138; 2,062 events, no alarms and 162 handled/zero
unclaimed SCI deliveries. Seven members and runtime/logger/mount retired cleanly,
with zero heap/errors/denied. All 256 prior files retain their bytes except
metadata; 262 files/773 clusters pass offline fsck/allocation. Ubuntu recovery
and consumed one-shot verified; evidence is preserved outside build.

Full attended-work acceptance remains incomplete. The user confirms using
New window from the search menu, then Open file: this exercised Text's dialog,
not the separate Files application. The saved screenshot lacks its dimensions
receipt because it was gated on compositor tracing; the strict collector
correctly refuses it. The earlier completion VM checked BMP dimensions but
never invoked that collector helper. Keep both incomplete trials as evidence.
The next attempt must use the Files folder icon in the dock and wait for the
five-minute completion prompt before normal menu exit.

### Corrected candidate validated (PR128)

Source `aff686d`, v69 release `2cd5b8604f2406c5`, emits capture dimensions
without tracing. The unchanged strict screenshot verifier rejects both original
physical and completion-VM evidence, then accepts the corrected non-traced VM.
Full build/core, 74 staging, 27 collector and five receipt tests pass. Completion
ran 306.107 seconds, generation 1→123, with 3,944 events and seven lifetime
members; Text/Files receipts, screenshot, saved logs and offline allocation pass.
Early exit remains incomplete; the three-phase AC regression verifies all three
screenshots. Exact staged GPT rehearsal refuses unsupported Q35 hardware,
saves/unmounts/returns; missing Home returns before probing.

The next unarmed run `eab752f59fb68762` uses the preserved 262-file/773-cluster
USB baseline and a new `attended-receipt.txt`. Keep AC/USB connected; open the
Files folder icon in the dock, browse Home, and wait for the five-minute Complete
prompt before saving and exiting through the normal menu. Both earlier trials
remain incomplete and backed up separately. The obsolete parked AC payload's
EFI copy was removed only after independent backup hashes matched; console
recovery and the newer accepted Home AC payload remain installed. Ordinary
battery startup remains off; physical acceptance of this correction is pending.

Final scope review retains main `bf1b9a6`. Air7,2 storage-service-gap work and
Pro PR19 `d80fb47` remain independent; no competing driver changes are included.

## Opt-in ordinary battery session scope (2026-10-24)

Base `468ca20`; bounded objective: admit this exact Air6,1 profile to the
existing explicit ordinary desktop owner, with no diagnostic session deadline.
Scope: Home ownership admission, distinct staging recipe, directed ownership/
staging tests and ordinary-session VM harness. Reuse inherited process lifetime,
mount hold, cached sampling, periodic logging and clean retirement. Keep default
startup off and firmware/EC/query permissions unchanged. Air7,2 PR130 owns window
capacity/compositor allocation; Pro PR19 owns Wi-Fi. Avoid their implementation
files. Next checkpoint is an Air6,1 VM session beyond eight minutes, refusal/
cleanup regressions and a separately staged unarmed physical session.

Run `eab752f59fb68762` on `2cd5b8604f2406c5` demonstrated the screenshot receipt
fix: the strict verifier accepts capture/save receipts and the 1366×768 BMP.
Text saved/reopened 17 bytes. The user left it running and observed the expected
halt; periodic logs reach 477 seconds, with no final exit/retirement proof or
separate Files activity. Prior 262 files were preserved except metadata; 267
files/811 clusters pass offline checks. Evidence is retained outside build.
The user explicitly chose to move on rather than repeat diagnostic coverage;
full attended-work acceptance remains incomplete, not promoted to passed.

### Ordinary session candidate validated (PR131)

Source `75ddde8`, v70 release `87bf9866baa3f8bc`, enables only the explicit
Air6,1 ordinary Home battery owner. Full build/core, 75 staging tests, exhaustive
owner combinations and receipt compatibility tests pass. The extended GPT VM
retained 535.342 seconds of live periodic-log coverage and 6,664 events, then
exited normally, drained ten lifetime members and reopened with a fresh owner.
Text/Files/shell bytes, all four logs and offline allocation pass. Submitted-save
cancellation, runtime fault retention, opt-out/missing-Home/platform refusals and
the Air7,2 ordinary-session regression pass. Exact staged GRUB→Limine GPT boot
refuses Q35 hardware while keeping apps usable, exits/reopens and reboots cleanly.

Keep the earlier failures: the initial long harness did not answer Terminal's
expected close confirmation; this automation is corrected. A subsequent short
VM startup hit an EC query timeout and safely halted with ownership retained.
An unchanged short retry and the full final suite passed. The isolated timeout
has not been attributed to native hardware; no deadline was widened to hide it.
Native ordinary-session acceptance and extended endurance remain pending.

Unarmed run `d34f847c1520e40c` preserves the 267-file/811-cluster baseline and
uses new `ordinary-battery.txt`. Keep USB connected, use apps normally beyond
eight minutes, save and select SrvrOS > Return to console when finished, then
enter `reboot` at `srv>`. There is no completion countdown or automatic return.
Console recovery remains `c35475f62f154f1e`; the superseded `78fe53e8cacdb1ba`
EFI copy was removed only after its independent backup matched. Corrected
capture and accepted recovery payloads remain available. Main is still `468ca20`;
Air7,2 PR130 and Pro PR19 `d0a0b57` retain independent scopes.

### Ordinary monitoring physical result (2026-10-24)

Run `d34f847c1520e40c`, release `87bf9866baa3f8bc`, session
`00065e9aafda5da4-00` returned with a clean exit snapshot at 797.659 seconds
from boot, beyond the former eight-minute deadline. Battery generation reached
304 with 4,553 events and no alarms. Runtime retired with zero heap, errors or
denied operations; the six-member peak lifetime drained and released. A new
nine-byte `test.txt` persisted; prior file bytes except workspace metadata are
unchanged. Offline fsck/allocation pass: 272 files, 817 clusters, no leaks or
bitmap mismatches. Ubuntu returned with an empty one-shot entry. Evidence and
hashes are backed up outside build.

This accepts opt-in monitoring beyond eight minutes with clean native retirement
and a persisted document, not multi-hour endurance or a reopen receipt absent
from retained logs. Default battery startup remains off. The reported 16-window
result belongs to another machine.

## Combined built-in services scope (2026-10-24)

Base `bc0edb9` includes ordinary Air6,1 admission and shared 16-window support.
Objective: a distinct opt-in Air6,1 ordinary session combining battery, panel
brightness, HDA audio and read-only CPU/SMC sensors, without USB networking or
the optional AML probe. Scope: staging recipes, combined-session VM harness and
this unit's current guides. Reuse existing exact device gates and lifetimes;
no driver permission changes. Pro Wi-Fi and Air7,2 desktop API work remain
independent. Next checkpoint: combined audio/absent-provider/exit/reopen and
refusal VM coverage, exact staged GPT rehearsal, then an unarmed attended test.

### Combined candidate validated (PR132)

Source `59c1836`, v71 release `1ae70fcf03ab6152`, combines the ordinary Air6,1
battery owner with brightness, HDA and read-only CPU/SMC providers; USB networking
and optional AML probing remain off. Full build/core, 76 staging tests and docs
checks pass. Seven GPT VM cases pass: audio/normal exit/reopen, pending save,
fault retention, opt-out, missing Home, absent providers and Air7,2 regression.
The normal Air6,1 run records 712 battery events and ten lifetime members; PCM
has no remaining owner/queue or underruns. Exact staged GRUB→Limine GPT rehearsal
passes with Q35 native-profile refusal, working HDA, apps, clean exit/reopen,
offline allocation and monitor reboot.

Two earlier Air7,2 runs timed out waiting for a missing serial close-request
receipt; the first saved screen shows Terminal's expected confirmation dialog.
Their evidence remains retained. The harness now confirms Close after a short
UI settling period and retains all completion, lifetime, runtime, logger and
file-integrity assertions; the full final matrix passes. No runtime deadlines
or hardware gates changed. Main advanced to `76a55dd` with documentation-only
architecture work; no tested binary changes are needed for that integration.

Unarmed run `2319b6df4b7f2dbd` preserves 272 files/817 clusters and reserves
`combined-services.txt`. Exercise brightness, Audio playback/volume/Stop, System
CPU/fan/battery freshness, and Text/Files for at least ten minutes; exit and
reopen with `desktop battery`, then exit and `reboot`. No timed completion or
automatic return. The obsolete `2cd5b8604f2406c5` EFI copy was removed only after
its durable payload/trial hashes matched; accepted ordinary battery, console
recovery and Ubuntu remain preserved. Combined native acceptance is pending.

### Combined session returned (2026-10-24)

Run `2319b6df4b7f2dbd`, release `1ae70fcf03ab6152`, produced one session
`00065e9bf077c2fc-00` with a clean exit snapshot at 1315.719 seconds from boot.
All four providers were enabled with networking off. Battery reached generation
500 over 500 attempts (one rejected sample), 7,492 events, two alarms and two
acknowledgements; generation advanced past invalid-through 227. Runtime retired
with zero heap/errors/denied and 531 handled/zero unclaimed SCI deliveries. The
12-member peak lifetime drained and released. This does not identify the early
rejected sample's cause: retained rotating logs do not cover the whole session.

Final sensor snapshots show 1,312 fan samples with zero failures and 1,315 thermal
samples. Audio records four starts/three completions, volume 68, no final error;
PCM has zero owner/queue/underruns. The user confirms everything worked normally, including sound and brightness.
The fourth audio start has no completion count; its exact stopping action is
not separately recorded.
A new 11-byte `ten-minute-demo.txt` persisted; prior bytes except metadata remain
unchanged. Offline fsck/allocation pass at 277 files/823 clusters with no leaks
or bitmap mismatches. Ubuntu returned with empty one-shot state; evidence and
hashes are backed up outside build. No reboot is armed.

The user confirms that desktop close/reopen was not attempted. This accepts
the observed combined session with battery, brightness, audio and sensors,
file persistence and clean exit. Combined-provider desktop reopen remains
untested on this machine; multi-hour endurance is also outside this result.

## Combined close/reopen follow-up scope (2026-10-24)

Integration baseline `e79e220`; retain accepted v71 release `1ae70fcf03ab6152`
(compiled source `59c1836`) for a focused two-session native check. No runtime or
staging changes: rerun combined HDA/battery exit/reopen VM and exact staged GPT
rehearsal, then use a fresh run record and USB baseline. Publish the preceding
combined-session acceptance and this scope. PR134 POSIX work and PR19 Wi-Fi
remain independent; neither is imported into this accepted payload. Check file
persistence, provider availability, battery lifetime retirement and normal exit
on both sides of `desktop battery`. Multi-hour endurance is not this test.

### Reopen trial ready (PR135)

Fresh rehearsal source `ef184f3` differs from compiled `59c1836` only in
documentation. The unchanged combined HDA/battery GPT VM passes normal
exit/reopen with fresh lifetime, exact saved files, retired PCM and clean
allocation. Exact installed v71 release `1ae70fcf03ab6152` separately passes
GRUB→Limine GPT boot, Q35 native-profile refusal, working HDA, normal exit/reopen,
file/allocation checks and monitor reboot. Documentation checks pass.

Unarmed run `c655eca6a4f00313` uses the new 277-file/823-cluster baseline and
`reopen-services.txt`. Briefly exercise brightness, audio, sensors and battery;
save the document and return to console. Enter `desktop battery`, reopen the
document and exercise the providers again. Exit normally, then `reboot`. No
ten-minute wait or automatic return is required. The preceding combined native
run remains independently backed up; this new run has its own backup directory.
Ubuntu and console recovery stay unchanged. Physical reopen acceptance awaits
the returned evidence and user observations.

### Reopen trial failed (2026-10-24)

Run `c655eca6a4f00313` on unchanged v71 `1ae70fcf03ab6152` completed its first
session `00065e9c895ac1d7-00` with clean retirement at 87.970 seconds from boot.
The user confirms all providers worked and the document was saved. Brightness
16/44/78 and one completed audio test are logged, with live CPU/fan data and
no final audio/SMC errors. New `reopn-services.txt` persisted (four bytes).

After `desktop battery`, session `00065e9c8f025226-00` saved its startup log,
but halted before a periodic/final snapshot. The user's photograph reports
`HALTED operation deadline`, setup `complete`, elapsed 5,037,205 us, zero
errors/denied/EC error/queries and retained live state. The source gives namespace
setup a 5,000 ms operation budget. Completion timing and deadline handling need
investigation; this evidence does not yet identify a driver or scheduler defect.
Do not widen the budget or claim native reopen acceptance from VM passes.

All 277 prior file bytes except metadata are preserved. Offline fsck/allocation
pass at 283 files/831 clusters with no leaks or mismatches. Ubuntu returned with
empty one-shot state. Both sessions, the photograph and file hashes are backed
up independently outside build. No reboot is armed. First-session combined
acceptance remains valid; native combined reopen is now a recorded failure.

## Reopen deadline investigation scope (2026-10-24)

Base `36080fc`. Investigate the stale worker-drain result between a timed slice
and the outer battery operation deadline. The native photograph reports setup
complete and no in-flight work at containment; reproduce that boundary using
VM-only second-lifetime scheduling hooks before changing production behavior.
Scope: battery operation loop, disposable battery VM model and desktop regression
harness, plus this record. Preserve the five-second setup budget and live-state
containment for still-running work. PR134 owns POSIX/scheduler changes; avoid
scheduler edits. PR19 Wi-Fi is independent. Next checkpoint: failing regression,
completion-vs-busy proof, combined exit/reopen and exact staged GPT rehearsal.

### Deadline completion fix validated (PR136)

Source `faacdf2` adds a nonblocking worker completion poll at operation expiry,
retaining the five-second setup budget, callback error checks and busy-work
containment. A VM-only second-lifetime fixture reproduces the old failure:
setup complete at 5.364 seconds, no work in flight, zero EC errors/queries, then
operation-deadline HALT. With the fix it drains both lifetimes cleanly. A separate
six-second setup fixture still halts and retains the second lifetime. This
proves the stale-observation defect and is consistent with the native photo;
native confirmation is pending rather than inferred from the model.

Full build/core, 76 staging tests, documentation checks and nine combined GPT
VM cases pass: both deadline regressions, normal playback/exit/reopen, pending
save, runtime fault retention, opt-out, missing Home, absent providers and Air7,2
regression. Exact staged v71 release `06b0976a58151901` passes GRUB→Limine GPT
boot with native-profile refusal, HDA, app/file work, exit/reopen, clean allocation
and monitor reboot. No scheduler or firmware permission changes. PR134 POSIX
and PR19 Wi-Fi remain independent; main is `36080fc`.

Unarmed run `227c377cefd26812` retains 283 files/831 clusters and uses new
`deadline-reopen.txt`. Briefly check providers and save, return to console,
enter `desktop battery`, reopen the document and check providers, then exit
and reboot. No ten-minute wait or automatic return. Prior native failure and
before-fix VM evidence remain backed up separately. The old `a18d72aecdf808c7`
three-minute capture EFI copy was removed only after durable payload verification;
combined, ordinary-battery, console recovery and Ubuntu remain available.

### Deadline retry failed on native reopen (2026-10-24)

Run `227c377cefd26812`, release `06b0976a58151901` (PR136), again halted
on its second desktop lifetime. First session `00065e9cec2debcc-00` completed
cleanly at 234.640 seconds from boot: 87 battery attempts, 1,308 queries, balanced
EC attachment, zero final heap/errors/denied, working brightness/audio and live
CPU/fan samples. First setup took 4,766,350 us, including namespace load through
the contract marker at 4,326,472 us and namespace initialization beginning at
4,425,243 us.

Second session `00065e9cfa6ff1bc-00` saved only its startup log. The photograph
shows `HALTED operation deadline`, `setup=namespace-initialize`, elapsed
5,047,305 us, `completed=0 in_flight=1`, zero errors/denied/EC error/queries and
retained live state. Unlike the preceding photo, work was still active: PR136's
completion repoll does not resolve this native startup overrun. Its synthetic
completion-race regression remains valid but does not establish native repair.
Next investigation should locate namespace/AML execution and wait time before
changing the five-second bound; the current evidence does not identify a hang
or distinguish execution cost from scheduling and firmware waits.

All 283 preexisting files except metadata retain identical bytes. Five new log
files were collected; no new document appeared. Offline checks pass at 288 files/
838 clusters, with no leaks or mismatches. Ubuntu recovered with empty one-shot
state. Logs, photograph and file hashes are backed up outside build. No reboot
is armed; native combined reopen remains failed.

## Startup timing scope (2026-10-24)

Base main `85d5939`, preserving the failed retry evidence commit. Add bounded
per-stage startup accounting for AML sleep/stall/semaphore, allocation/free and
EC work, including active waits at containment. No firmware gate, scheduler or
five-second deadline changes. Add an explicit automated two-desktop-session
recipe with normal lifetime/logger teardown and Ubuntu return on completed
runs. Fatal mounted-Home containment must still halt without flushing/resetting
live storage; a photograph/manual recovery may remain necessary on failure.
PR134 (`d9876eb`) owns POSIX/scheduler and PR19 (`7873b63`) owns Wi-Fi; neither
scope is imported. Validate timing accounting, completed/busy startup regressions,
automated success/refusal/failure and exact staged GPT boot before installation.

### Automated startup diagnostic ready (PR137)

Final runtime source `582cc59`, v72 release `837ce4f6bfab6775`, records fixed-size
setup-worker sleep/stall/semaphore/EC wall times and sampled allocation/free
cost, including active waits at containment. Existing phase markers remain.
The five-second deadline and hardware permissions are unchanged. The explicit
reopen diagnostic runs two 20-second app lifetimes with fresh battery samples,
normal retirement and log saving; it rejects early manual exit. Ordinary desktop
sessions retain no timer. Fatal mounted ownership still halts without storage
writes/reset; normal diagnostic completion unmounts and returns to Ubuntu.

Full build/core, 77 staging tests and docs checks pass. On `4b41c06`, ordinary
Air6,1 save/audio/reopen, pending-save and runtime-fault cases and the Air7,2
combined regression pass. The final change only makes early diagnostic exit
incomplete; full core and all five automatic cases pass on `582cc59`: early exit,
normal, completed-at-expiry, still-busy containment and profile refusal. Exact
staged GRUB→Limine GPT boot passes two refused-but-cleanly-retired Q35 sessions,
unchanged fixture files, clean allocation and automatic recovery. Refusal is
expected VM coverage, not native acceptance. Main remains `85d5939`; PR134
`e4b7b02` and PR19 `67575f0` retain separate POSIX and Wi-Fi scopes. Shared staging/
boot files need both independent profiles preserved in later integration.

Run `d3d219186d3bc6e6` retains the 288-file/838-cluster USB baseline and requires
no human input or document edits. A machine-local one-shot Ubuntu service collects
matching sessions, requires both fresh-sample/retirement receipts, verifies prior
file hashes and offline allocation, backs up evidence and disables itself after
recovery. A halt still requires a photograph/manual restart. The user explicitly
authorizes automated reboot without another permission gate. Installed payload
hashes and Ubuntu default are verified; console recovery `c35475f62f154f1e` remains.
The obsolete `fc1d1f4c45d30252` EFI image-capture copy was removed for space only
after its durable payload backup was hash-verified. Native reopen remains failed
pending this diagnostic's returned evidence; this is not a claimed native fix.

### Automated startup timing result (2026-10-24)

Run `d3d219186d3bc6e6`, release `837ce4f6bfab6775`, passes the automated
two-session check. Sessions `00065e9d64761993-00` and `00065e9d65e8d7a2-00`
each reached fresh battery generation 6 and retired epochs 1/2 with balanced
attachment and zero final heap/errors/denied. Normal Ubuntu return and the
one-shot collector completed; the collector disabled itself. All 288 prior
files except metadata retain identical bytes; eight new logs bring the clean
filesystem to 296 files/848 clusters. One-shot boot state is empty. Evidence
and hashes are backed up outside build; no new reboot is armed.

First startup took 4,796,986 us; reopening took 4,917,553 us, only 82,447 us below
the unchanged five-second deadline. Namespace load alone took 4,455,910 us in
the second lifetime. Neither startup called AML sleep or stall. Second-start
semaphore wall time totaled 10,906 us and four EC byte transfers totaled 312 us.
Allocation made 18,847 calls, with 295 sampled calls totaling 61,586 us; free
made 7,411 calls, with 116 samples totaling 14,180 us. These sampled wall times
are not exact totals or CPU measurements. The allocator's whole-heap validation
on both entry and exit, plus its linear free-block search, is a strong next
investigation target; establish cost and preserve integrity checks in any fix.

This accepts only the short automated startup/reopen/retirement sequence. It
does not resolve the prior attended failure or establish sufficient timing
margin under interactive load. No timeout increase or allocator change was
made in this run.

## Allocator startup investigation scope (2026-10-24)

Base `4f8acd2`, retaining the collected startup result. Profile/replay the real
allocator and remove redundant allocation traversal while preserving full
before/after heap validation, free checks and the five-second battery deadline.
Scope: kernel heap allocation traversal, deterministic host integrity/performance
regressions and this machine's v72 automated startup trial. Avoid scheduler,
firmware and EC changes. PR134 `e4b7b02` has no heap implementation changes;
PR19 `807fb51` remains independent Wi-Fi scope. Publish measured old/new work
counts and reject corrupt heaps even when a suitable early free block exists.
Native timing improvement remains pending the next automated trial.

### Heap traversal trial ready (PR138)

Runtime source `e7b29e7`, v72 release `4efd82574f07eb30`, selects the first
fitting free block during the existing full entry validation walk. It finishes
that entire walk before mutation and retains the complete exit walk and both
free walks. First-fit, splitting/coalescing, metadata, firmware permissions and
the five-second startup deadline remain unchanged. Later corrupt blocks must
still halt even when an earlier suitable free block exists.

Three sequential alternating baseline/candidate replay pairs (two lifetimes
each, 18,847 allocations and 7,411 frees during each setup) reduce median host
setup time from 5.707 s to 4.856 s, about 14.9%. Cleanup/free time is similar
(1.272 s versus 1.294 s). Full validation visits and payload checksums match
exactly; about 105.45 million redundant search visits disappear per lifetime.
This is a deterministic namespace-sized workload, not a captured AML trace or
native timing prediction. Native timing margin remains to be measured.

On clean `e7b29e7`, full build/core, general smoke (including 481 CLI commands),
77 staging tests and docs checks pass. Nine battery VM cases pass: automatic
early exit, normal, completed-at-expiry, busy containment and profile refusal;
ordinary combined desktop save/reopen, pending-save cancellation, runtime fault;
and the Air7,2 combined regression. Exact staged GPT GRUB→Limine boot passes
two refused-but-cleanly-retired sessions, clean allocation and automatic return.
Q35 profile refusal is expected and is not native acceptance. Refreshed main
is `4f8acd2`; PR134 `d5d0fa1` still has no heap implementation change and PR19
`d5010dc` remains independent Wi-Fi scope.

Run `c741adb1f4bd0f75` uses this unit's USB `6ADA-85BB`, preserving all 296
files/848 clusters. The automated trial opens two 20-second desktop lifetimes,
requires fresh samples and clean retirement, then returns to Ubuntu. The one-shot
collector checks saved sessions, startup timings, prior hashes and offline
allocation, backs up evidence outside build and disables itself. No human input
is required. User authorization permits this automated reboot without asking;
fatal mounted ownership still halts and requires a photograph/manual recovery.
Ubuntu remains default and console recovery `c35475f62f154f1e` is retained.
The obsolete failed `06b0976a58151901` EFI copy is retired for space only after
its durable payload backup is hash-verified. Prior accepted payloads and the
previous automated diagnostic remain preserved. Native improvement is pending
returned evidence; this does not yet resolve the attended intermittent failure.

### Automated heap trial result (2026-10-24)

Run `c741adb1f4bd0f75`, release `4efd82574f07eb30`, runtime source `e7b29e7`
(merged through PR138, `27f0870`) passes both automated native desktop lifetimes.
Sessions `00065e9dea272465-00` and `00065e9deb7d9404-00` completed startup in
3,604,290 and 3,618,725 us, respectively, versus 4,796,986 and 4,917,553 us in
the preceding diagnostic. This run is 24.86%/26.41% faster and leaves at least
1,381,275 us below the unchanged five-second startup deadline. This is an
observed paired-run improvement, not a worst-case bound under interactive load.

Fresh battery generations 7/6 and epochs 1/2 each retired with balanced attach/
detach, zero final heap/errors/denied and released desktop lifetimes. Queries
were 105/90, with one notification in each lifetime. All 296 prior files except
mutable `.srvros/meta` retain identical contents; eight new session logs bring
the clean filesystem to 304 files/858 clusters, with no leaks or FAT/bitmap
mismatches. Automatic Ubuntu return and collection completed; all 303 durable
trial backup files were hash-verified. The collector is disabled and GRUB's
one-shot entry is empty; no reboot remains armed.

This accepts the short automated startup/reopen/retirement trial with the heap
optimization and preserved full integrity checks. It does not yet accept the
previously failing attended reopen under Text/file/audio work or extended
endurance. Next useful coverage is an attended combined desktop session followed
by close/reopen and resumed Text/file work, retaining the same startup deadline.

## Attended heap-fix reopen scope (2026-10-24)

Base main `27f0870`, preserving collected PR138 physical evidence. Prepare the
existing untimed v71 combined desktop recipe with the validated heap fix; no
runtime, firmware, deadline or shared subsystem change is planned. Rehearse
Text/Files/audio and normal close/reopen plus the exact staged GPT boot path.
The attended run must save a new document, browse it in the separate Files app,
close the desktop normally, reopen with `desktop battery`, and resume file work.
PR134 `d5d0fa1` owns POSIX and PR19 `b02be44` owns Wi-Fi; neither is imported.
The previous automated run is collected, backed up and unarmed. Preserve Ubuntu
and console recovery; report ready without starting this attended run.

### Attended reopen ready (PR139)

Source `5d858df`, v71 release `9851d427b042227b`, run `f4f57726a0d1824d` uses
the exact native kernel/initramfs bytes accepted in the prior automated heap
trial, with the existing untimed combined-session configuration. Full build/core,
77 staging tests and docs checks pass. The combined Text/Files/audio normal VM
and exact staged GPT GRUB→Limine profile-refusal VM pass, including clean
close/reopen, provider/PCM teardown, offline allocation and monitor reboot.
Q35's expected native-profile refusal is not physical acceptance. The previous
full smoke and nine-case battery matrix remain evidence for the unchanged code.

The attended checklist is in the current battery guide: ten minutes with a new
`heap-reopen.txt`, separate Files dock app and battery/brightness/audio/sensor
checks; Return to console, `desktop battery`, then reopen/append/save/reopen the
document and repeat checks for at least two minutes. Save, exit, unmount and
reboot manually. There is no timer or completion prompt. USB baseline is 304
files/858 clusters, unchanged and clean. Ubuntu default and console recovery
`c35475f62f154f1e` remain verified. Superseded startup diagnostic `837ce4f6bfab6775`
is removed from EFI only after its durable payload backup is hash-verified;
the latest accepted automatic release `4efd82574f07eb30` remains available.
Payload and trial evidence are backed up outside build. This run is ready but
unarmed; attended reopen acceptance remains pending its return.

### Attended combined reopen accepted (2026-10-25)

Run `f4f57726a0d1824d`, v71 release `9851d427b042227b`, returned with four
attended desktop lifetimes and three successful reopens. User reports running
all apps successfully. Startup took 3,600,315 / 3,791,923 / 3,732,066 / 3,753,837
us, leaving at least 1.208 seconds below the unchanged five-second deadline.
Epochs/tokens 1–4 all retired and released with balanced attachment and zero
final runtime heap/errors/denied. Session durations were about 219, 35, 201 and
179 seconds: roughly 10.6 minutes of desktop time overall, not the originally
planned ten-minute first session or an extended endurance test.

Text save/read succeeded in the first lifetime, read/append/save in the second,
and reads of the resulting 30-byte document in the third/fourth. Separate Files
activity is present in all four lifetimes. Audio finished two plays with no
pending work, and read-only thermal/fan samples continued across reopens. The
user's observation supplies the perceptual app/provider result. The generic
collector's release filter identified only the first two sessions because later
ring-buffer tails no longer contained the release banner; all four newly created
session directories were assessed directly with sequential lifetime tokens,
startup, final battery and release receipts rather than inherited exit text.

No prior files are missing. Only `.srvros/meta` and desktop preferences changed
among 304 prior files; new artifacts are 16 session logs, the Text document and
a drawing. Offline fsck/allocation are clean at 322 files/887 clusters with no
leaks or FAT/bitmap mismatches. Evidence and hashes are backed up outside build.
Ubuntu one-shot state is empty and no reboot remains armed. This accepts attended
combined desktop close/reopen with Text/Files work after the heap fix. Longer
endurance, broad input/load coverage and other unimplemented device capabilities
remain separate milestones; no integrity checks or startup budget were weakened.

## Longer combined session scope (2026-10-25)

Base main `3647dc7`, preserving the attended four-lifetime acceptance. Prepare a
30-minute paced ordinary-shell workload for USB copy/readback and repeated audio,
with a fresh-run guard, durable round/completion evidence and automatic Ubuntu
collection after manual desktop exit. Text editing and audible playback remain
attended observations. Scope is new workload/VM tests, this machine's record and
current guide; retain the existing untimed v71 profile. No kernel, firmware,
battery budget or desktop startup changes. PR153 owns capacity automation and
shared staging/battery harness changes; avoid those files. PR19 remains Wi-Fi.
Validate the newly merged scheduler/desktop baseline, keep accepted payloads and
console recovery, and leave the next physical run ready-unarmed.

The refreshed full CLI smoke exposed one stale pre-POSIX diagnostic expectation:
`cd /fat/status.txt` correctly refuses and leaves cwd unchanged, but the newer
shell prints `cannot change directory` rather than `not a directory`. Update
only that expected text and add explicit status=1/unchanged-cwd evidence; do not
alter runtime behavior. Rerun the full suite after this harness correction.

### Long combined trial ready (PR154)

Source `78859c4`, v71 release `3f0896783e4c8fe7`, run `db3ea7d68555f544`
consumes main `3647dc7` (new scheduler/desktop baseline) and retains the unchanged
native Air6,1 battery/firmware gates and five-second startup budget. The ordinary
Terminal workload performs thirty 256-KiB copy/sync/rename/readback rounds, each
with a bundled six-second PCM play and sixty-second pacing before the receipt.
It requires at least thirty minutes of pacing (roughly 33 minutes total), refuses
reruns and saves per-play and round/completion records. A nonzero audio/file
command aborts the job; the collector also rejects missing audio completion or
underruns. No forced desktop exit is added. Text/Files use and listening remain
attended, with a fresh run-specific document and checks near start/middle/end.

Full build/core passes on `44b5706`; subsequent `78859c4` changes only the CLI
harness expectation and scope documentation. Full smoke including 482 CLI
commands passes after correcting the stale diagnostic and checking failed-cd
status/unchanged cwd. The new workload host tests, 78 staging tests, new real-shell
GPT battery/audio/Text/Files workload VM, ordinary combined reopen VM and exact
staged GPT GRUB→Limine boot all pass. Workload VM checks exact outputs, zero-
underrun audio completion, rerun refusal, clean lifetime retirement and offline
allocation. Exact native-stage Q35 refusal is expected; it covers safe unavailable
providers, PCM, saves, reopen, unmount and reboot, not native acceptance.

USB preparation preserved all 322 existing files and added only two fresh workload
inputs; preflight is clean at 324 files/890 clusters. Ubuntu default and console
recovery `c35475f62f154f1e` remain verified. Superseded pre-heap combined release
`1ae70fcf03ab6152` is retired from EFI only after its durable backup is verified;
accepted attended `9851d427b042227b` and automated `4efd82574f07eb30` remain.
The one-shot Ubuntu collector requires workload completion, a completed desktop
lifetime of at least thirty minutes, clean battery retirement, the new Text
file, prior-file preservation (apart from metadata/preferences) and offline
allocation, then backs up evidence and disables itself. Human audio/input
confirmation remains distinct. Run and payload backups survive distclean.
Ready-unarmed: no reboot is scheduled; longer native acceptance remains pending.

## Audio/USB overlap correction scope (2026-10-25)

The prepared long-session script serialized copy/readback before playback; it
did not establish audio/disk overlap. Supersede that unarmed workload with a
fresh run: one-minute PCM plays in the background while bounded paced copies,
flushes and comparisons execute in the foreground, requiring the player to remain
live at both ends of each recorded burst. Retain audio completions/underruns and
per-burst evidence. The disposable VM must observe PCM output advancing during
USB writes. Keep the already validated native payload `3f0896783e4c8fe7` and its
runtime source `78859c4`; only Python workload/tests and documents change. Actual
main is `6e06bfb`; separate capacity work from PR153/155 is not introduced into
this immutable payload. PR19 remains independent Wi-Fi work. Prior run is
ready-unarmed, not physically executed; replace its collector and launch record
only after preserving its inputs/evidence. Leave the corrected test unarmed.

### Audio/USB overlap ready (PR156)

Corrected workload source `ee96e1d`, run `93a364b8128b185a`, replaces unexecuted
run `db3ea7d68555f544`. Installed native payload remains `3f0896783e4c8fe7`, source
`78859c4`, hash-identical to its already validated build and exact-stage rehearsal.
Thirty one-minute plays run in the background, each with forty foreground
256-KiB copy/flush/rename/readback bursts paced one second apart. Playing-receipt,
before/after PID liveness, absence of an early completion, explicit burst totals,
wait status and retained audio completion/zero-underrun evidence guard the trial.
Total copy/readback is 300 MiB during thirty minutes of PCM, about 31 minutes
elapsed including gaps. Text/Files and listening remain attended; automatic
Ubuntu collection verifies all 1,200 bursts and thirty complete plays.

Host verifier tests pass. On clean `ee96e1d`, the repeated short GPT VM passes
four bursts/two plays with 12 observed non-silent PCM/USB-write overlap windows.
The full native-parameter round passes all forty bursts during one complete
60-second play, with 90 observed overlap windows. Both verify readback, no
underruns, concurrent Text/Files, rerun refusal, clean battery retirement and
filesystem allocation. Development tests caught quoted numeric wait parsing,
a too-long for word list, and command substitution using read-only /fat; the
script now uses numeric wait, eight-item groups and explicit writable count
files. One short rerun hit a Q35 battery sample/EC fault; its evidence is retained.
An unchanged retry and the full-minute trial passed. This does not establish that
fixture fault's cause or native endurance acceptance.

USB inputs now comprise a fresh script, 256-KiB seed and one-minute WAV. All 324
preexisting files (including the unused prior trial inputs) remain preserved;
preflight is clean at 327 files/981 clusters. The previous run is superseded and
unarmed, and its collector disabled. The overlap collector replaces it. Ubuntu
default, console recovery and accepted payloads remain intact. Payload and run
evidence are backed up outside build. The corrected test is ready-unarmed; no
reboot is scheduled, and listening/native overlap acceptance remains pending.

## Automatic audio / filesystem stress scope (2026-10-25)

Run `93a364b8128b185a` returned with clean desktop retirement but failed the
workload: one 60-second PCM completion with zero underruns and 32 of 40
recorded disk bursts. The next burst crossed playback completion; the fixed
count/pacing underestimated native filesystem overhead. Manual Text saves do not accept sustained overlap. Preserve this failure.
The replacement is fully automatic, with listening as the only human task:
a dedicated PCM producer and separate continuous verified filesystem worker,
automatic desktop startup/progress/exit, normal battery retirement and Ubuntu
return. Record actual timestamps and counts rather than infer overlap from a
shell reading a concurrently written log. Use existing PCM/filesystem interfaces;
no firmware gates, integrity checks or startup budgets change.

Base is merged main `2c1ffc6b`. Scope: a playwav stress mode, narrow boot-only
session selection, desktop progress/worker lifecycle, staging and VM regressions.
PR157 owns shared document-dialog/app lifecycle work; preserve it and keep these
additions isolated from its client/dialog code. PR19 remains independent Wi-Fi.
The prepared native payload will be rebuilt and exact-stage tested. User has
explicitly authorized the automated reboot after validation.

### Automatic audio / filesystem stress prepared (PR158)

Native source `1c2bba2f`, v74 release `a70f9540f3aa39dd`, run
`6e0147a2ac774722` uses a continuous thirty-minute PCM producer and a separate
unpaced 256-KiB create/flush/rename/reopen/compare/delete/sync worker. A pipe starts
file activity only after PCM reports live playback. Timestamp/byte/count receipts
reject late, absent or insufficient-duration I/O, incomplete PCM and underruns.
Boot waits for fresh battery telemetry, starts the workload, shows elapsed
progress and retires the desktop/battery lifetime before unmount and automatic
Ubuntu return. No human interaction is required beyond listening. Scratch space
is bounded; all preexisting Home files remain outside the private test directory.

Full build/core and full smoke pass, including 482 CLI commands, shell editing,
UEFI USB input and displayd. The new receipt regression and 80 staging tests pass.
The automatic 60-second Q35 run on clean `1c2bba2f` completes 363 verified cycles
(95,158,272 bytes) with zero underruns and 224 independently observed non-silent
PCM/USB-write overlap windows. It retires cleanly and returns without injected
keyboard or mouse input; offline allocation is clean. Exact staged GRUB→Limine
rehearsal on `95d522c2` (harness-only follow-up) confirms native profile refusal,
no stress startup on unsupported Q35, unmount and automatic return. Its first
harness configuration lost the reference clock with synthetic ACPI under UEFI;
that failure is preserved, and the successful rehearsal uses the established
native UEFI fixture without injected AML. Native source/payload did not change.

USB baseline is 335 files / 992 clusters, no leaks or bitmap mismatches; no new
USB inputs are needed. Ubuntu and console recovery `c35475f62f154f1e` are retained.
The obsolete EFI-only copy `87bf9866baa3f8bc` is retired after exact durable backup
verification to make installation space; accepted evidence/payload backups and
the latest attended/automatic recovery payloads remain available. The new Ubuntu
collector requires the matching workload receipt, clean desktop/battery retirement,
prior-file preservation and offline allocation; listening remains separate.
User explicitly authorized reboot and automatic startup after validation. Save
armed state and durable run/payload copies before launch. Native endurance is
pending this run, not accepted by VM success.

### Automatic stress partial result and desktop latency failure (2026-10-25)

Evidence-review scope only: collect run `6e0147a2ac774722`, document audio/UI and
retirement outcomes, and identify the next bounded investigation. Integration
baseline is `afb2c3b0`; open PR157 owns shared desktop document/application
lifecycle changes and must be considered before implementing launch/close changes.
No new payload or reboot is prepared by this review.

Release `a70f9540f3aa39dd`, session `00065eafdcd77b12-00`, was intentionally
exited early. The user reports persistent, stable sound but multi-second app
open/close delays and a 3–4-second UI freeze after a dock click while audio kept
playing. Final PCM position corresponds to 1,379.66 seconds (about 23 minutes),
with zero underruns; cancellation releases the PCM owner and queue. Battery
retires with 536 attempts, 8,122 queries, 98 notifications, zero heap/errors/denied
operations and a released lifetime. This supports audio continuity for the
observed interval, not a completed thirty-minute endurance or responsiveness pass.

The automatic collector correctly reports incomplete: cancellation left no final
disk/result receipt. There is one residual private payload; no verified total
burst count/throughput can be recovered from it. Existing files are preserved
except `.srvros/meta`. Offline checks pass at 340 files / 1,000 clusters with zero
leaks/bitmap mismatches; Ubuntu's one-shot entry is empty. Full raw evidence and
the user report are retained outside build.

Close-request to process-exit intervals visible in the retained log include
3.23, 4.60 and 4.65 seconds (also 1.28 and 1.70 seconds). These include client
cleanup and are not direct measurements of the dock-to-render freeze. System
and one Files process also exit with failure status; their cause is unresolved.
The overall workload/desktop FAIL markers remain failures/incomplete evidence,
not a successful completed trial; normal battery retirement is distinct.

Code inspection identifies synchronous `srv_exec` from `deskd`'s event loop and
storage-gated process/lifetime operations as a plausible freeze path. PCM calls
remain admitted while storage is owned, consistent with stable audio. No timing
in this capture separates gate contention, executable loading, cleanup or render
stall, so root cause is not yet proven. Next: automate app launch/close during the
same unpaced workload, measure event-loop gaps plus launch/close phases and I/O
gate wait/hold time, then move blocking lifecycle work off the event loop or fix
measured contention as appropriate. Preserve integrity and ownership checks and
keep full disk pressure; do not hide the failure by adding workload sleeps.

### Active scope: responsive desktop during automatic audio/I/O (2026-10-25)

Base `3ab4a385`, branch `device/macbookair6-1/desktop-io-latency`. Reproduce and
measure event-loop stalls with automatic real app open/close during unchanged
unpaced verified disk work and PCM playback; move blocking compositor lifecycle
operations off its input/render loop where measurements justify it. Expected
scope: `userspace/deskd`, a bounded userspace lifecycle worker, regression and
VM harness, native trial records. Preserve filesystem integrity and kernel I/O
ownership gates. Air7,2 branch `dfba5295` owns process/memory lifecycle latency;
PR157 also changes document routing, so avoid competing kernel/process edits and
keep compositor integration explicit. Pro PR19 is separate Wi-Fi work.
User authorizes tests, merge and automated native reboot. Prior run is collected;
no new reboot is armed until matched builds, VM checks and recovery preflight pass.


Candidate reproduction: a fully automatic Q35 Text/Files/System lifecycle loop
under unchanged PCM/disk stress recorded a maximum compositor loop gap of
863,447 us; a second instrumented run showed cache allocation waiting 532,954 us
and free waiting hundreds of milliseconds. The initial worker candidate reduced
the maximum loop gap to 179,769 us across 21 cycles, with zero app errors,
zero PCM underruns and 218 independently observed PCM/write overlap windows.
These are development VM measurements, not native acceptance. Final clean-source
regressions and exact-stage validation precede installation and reboot.


### Responsive lifecycle stress candidate prepared (PR160)

Native source `ae5b8680`, release `4e8c2c95b707acbf`, run `97f910f8a1024bbe`
retains the thirty-minute unpaced 256 KiB write/flush/rename/readback/delete/sync
work and PCM stream, and automatically cycles Text, Files and System throughout.
The compositor uses a persistent worker for launch and pixel-cache map/unmap;
no kernel I/O eligibility or integrity checks change. Pending/discarded mappings
retain budget until release completes. Resize failure preserves the last delivered
geometry/front, and memory pressure waits for reclaimable buffers without early
byte credit. Worker metadata covers the existing cache budget; child admission
and early OPEN delivery are explicitly tracked. Ordinary screenshot initiation,
registry growth and forced termination are separate synchronous paths.

Final clean-source core/build and strict automatic VM pass on `ae5b8680`: 23 app
cycles, maximum loop gap 139,364 us, ready 1,303,625 us and close 924,975 us, zero
app errors and zero PCM underruns, with 221 independently observed PCM/write
overlap windows. Both lifecycle and I/O timestamps fall within live playback.
The full smoke suite passed on `43d9e1ef` (482 CLI cases, shell editing, UEFI USB
and displayd); later changes refine memory-budget admission and validation.
All twelve apps pass maximize/restore/close. Multiple-document close/relaunch
passes with explicit completed cache-release checks. The 128-window growth
fixture on `efd6bb1d` passes 129 retirements, restored process baseline and zero
retained registry/cache bytes. Host tests cover delayed allocation, discarded
pending allocation, copied launch inputs, child reservation, early OPEN messages,
launch failure, resize failure and full-budget delayed reclamation. Eighty staging
tests and documentation checks pass. A superseded multiwindow checker failure is
retained: it expected premature cache credit at window removal; the replacement
requires zero bytes after completed unmaps and rejects missing/nonzero receipts.

Exact native release passes GRUB→Limine rehearsal, expected Q35 native-profile
refusal, no unsupported stress startup, clean unmount and automatic return. This
is not native responsiveness acceptance. Physical validation requires the saved
PCM/disk receipt plus at least six completed app cycles spanning 1,780 seconds,
zero app errors, loop gaps no greater than 500 ms, clean battery retirement,
prior-file preservation and offline allocation. Listening remains the only human
task; keep AC/USB attached and allow automatic return after thirty minutes.

USB baseline is 340 files / 1,000 clusters, zero leaks/bitmap mismatches. Ubuntu,
console recovery `c35475f62f154f1e` and the latest accepted desktop/reopen payloads
are retained. Obsolete EFI-only `3f0896783e4c8fe7` was removed after verifying its
independent durable payload. Raw results, source/stage and run records are backed
up outside build before reboot. User explicitly authorizes this automatic launch.
Air7,2's `2eb23c11` process/graphics service-window work remains separate and
unmerged; this candidate neither consumes its v15 protocol nor changes its gates.


Integration update: PR157 merged as `c4bee5cd` before PR160 integration. The
pre-merge `4e8c2c95b707acbf` stage is superseded and was never installed or armed.
Integrate the matched v15 document/modal lifecycle and kernel service-window
baseline, retaining both the async worker and existing-process document routing,
child modal retirement, Air7,2 v75 guide and Air6,1 v74 automatic stress. Repeat
core/build, affected lifecycle VMs, automatic audio/I/O and exact-stage checks
before preparing the replacement native payload. Recovery/USB baseline unchanged.

Integrated lifecycle testing exposed a modal startup race: OPEN_REPLY marked the
dialog ready before CONFIGURE produced its canvas. Delayed compositor allocation
therefore laid out the picker at zero size, moving its initial filename focus to
Up. Extend the shared dialog helper's readiness condition to require the first
configured generation. The new host regression fails on the previous helper and
passes with this correction; retain the failing VM screenshot/log and rerun the
normal/crash/refusal lifecycle cases before native staging. This bounded library
change preserves window admission and buffer integrity checks.

Consume PR161 (`44c70a12`), which adds host routing regressions without changing
production binaries. The corrected normal lifecycle VM passes. The crash variant
initially failed its startup count because the new compositor launch timing line
split the single Text startup message. Extend the existing trace-interleaving
decoder to that line type, preserving exact startup counts, duplicate detection,
failure markers and raw evidence; rerun crash/refusal cases. No timeout or file
acceptance predicate is relaxed.

Final integrated candidate: source `7ddf2153`, native release
`8dd177cef12cec3b`. Build/core and desktop/routing host checks pass; the normal,
crash and refused-dialog VMs pass exact file contents, offline allocation and
restored process baseline. The automatic stress VM passes 19 app cycles with
maximum loop gap 438,388 us, ready time 1,212,207 us and close time 1,392,331 us,
zero app errors/PCM underruns, 393 verified bursts (103,022,592 bytes) and 224
observed PCM/write overlap windows. The full smoke suite passed on integrated
`29c75d8a`; the later dialog correction has a failing-before/passing-after host
regression and all three directed lifecycle VMs. Exact-stage GRUB/Limine boot,
expected native-profile refusal on Q35, unmount and automatic return pass.

PR160 is ready for integration and the authorized thirty-minute native launch.
The replacement payload/evidence are backed up outside build; retain Ubuntu,
console recovery and previously accepted desktop/reopen payloads. The automatic
trial opens/closes real apps throughout continuous PCM and unchanged verified
USB stress, then retires the battery runtime and returns to Ubuntu for automatic
collection. No human file work is required. Keep AC/USB connected and listen;
native responsiveness/endurance remains pending that run's complete receipts.

### Automatic UI/audio/USB stress return — 2026-10-25 (failed)

Release `8dd177cef12cec3b` / source `7ddf2153` returned automatically after
roughly half a minute, not the required thirty minutes. Matching session
`session-00065eb146d235a5-00` completed one Text open/render/close cycle:
1,624,727 us to ready and 707,052 us to close. Files then launched (exec
2,236,784 us), submitted and completed its Home directory listing, but never
produced the compositor's opened/ready receipt before the unchanged ten-second
cycle limit. The test reports FAIL and stops the workload; the temporary payload
is retained, with no final disk/PCM receipt. The logs do not yet identify the
missing window-admission/first-frame boundary, so do not assume the directory
listing alone caused the failure or increase the deadline to call this a pass.

Maximum measured desktop-loop gap was 108,087 us; no PCM underruns were recorded
during the short run. These are encouraging bounded observations, not endurance
or full responsiveness acceptance. Battery runtime retired with heap/errors/
denied all zero, 11 handled and zero unclaimed SCI events. All 340 prior files
are preserved except the expected workspace metadata update. Offline checks pass:
345 files / 1,007 clusters, zero leaks, bitmap mismatches or errors. Ubuntu is
back, next_entry is empty, automatic collection completed and the collector is
disabled. Failed evidence and payload remain backed up outside build.

Next investigation: reproduce and trace Files' OPEN publication, child
registration/admission and first-frame delivery under sustained disk pressure,
including the populated Home directory. Preserve the existing integrity checks,
ten-second cycle failure and 500 ms loop bound. No replacement run is armed.

### Files startup under load — active investigation (2026-10-25)

Base `d4715774` / merged PR160 and PR163. The local objective is to reproduce
the missing Files admission/first frame under the unchanged automatic PCM/USB
stress, fix the proven cause and repeat the fully automatic native trial.
Scope: compositor launch/admission, Files startup and narrowly required shared
runtime code, regression fixtures, this machine record and local run/collector
scripts. Preserve all integrity checks and the 10-second cycle / 500 ms loop
limits. PR162 separately owns automatic capacity-capture targeting after async
launch; preserve that work and inspect its actual integration before staging.
Pro PR19 retains independent Wi-Fi scope. User authorizes testing, self-merge,
installation and automatic reboot. Previous failed evidence is collected and
backed up; no new physical result is accepted.

### Files startup investigation checkpoint — 2026-10-25

The native missing-admission failure is not yet reproduced or fixed. Synthetic
populated-Home stress (64 existing files) passes with 400 write IOPS: source
`02b1e4c1`, ten Text/Files/System cycles, maximum loop 349,423 us, ready
3,904,186 us, close 2,809,892 us, zero app errors and 222 observed PCM/write
overlap windows. All 66 prior fixture files and offline allocation checks pass.
Build/core tests pass after consuming merged PR162 (`f4e9fbcb`).

Keep the failed experiments: 40 write IOPS delays the first Text launch past the
cycle bound; 100 write IOPS admits Text but misses its first-frame timeout.
Neither reproduces the native missing Files OPEN. An unthrottled populated run
also exceeded the loop limit (505,077 us) and remains a failure, despite passing
its audio/disk receipt. Do not convert those results into startup acceptance.

Fixed a measurement omission: an iteration that breaks on timeout was absent
from the loop maximum. The final interval now counts against the same 500 ms
limit, with a host regression. The earlier native 108,087 us maximum may therefore
understate the final stall. Added successful legacy OPEN timestamps, compositor
admission identity and deadline child/window state, plus a populated-Home VM
fixture. The ten-second app-cycle and all client deadlines, filesystem, PCM,
process/admission and native ownership checks remain unchanged.

PR164 is a diagnostic/measurement change, not a claimed Files startup fix. The
next authorized automatic native run repeats the unchanged thirty-minute
workload with these receipts, no human file work, and automatic return/collection.
Preserve the previous failed release and accepted recovery. Inspect the new
matching failure boundary before proposing a production startup change.

### Instrumented app/audio/USB stress return — 2026-10-25 (failed)

Release `d96ecb5806eba5c9` / source `6a0efaaf`, merged PR164, returned and
collected matching session `session-00065eb1d5c0e3e5-00`. Six complete cycles
(Text, Files, System twice) passed. Both Files OPEN requests reached compositor
admission in under 1 ms; ready took 3,213,396 and 4,658,569 us, and close/reap
took 1,783,328 and 1,796,376 us. The previous missing Files admission did not recur.

The seventh cycle, Text, took 7,678,976 us to its first frame. Its window then
closed, but the process had not completed cleanup/reap when the unchanged
whole-cycle deadline fired at 10,000,722 us (phase 3, no pending OPEN or app error).
It subsequently exited with status zero during test teardown. This is a failed
stress run, not acceptance: no final workload/disk receipt exists. The measured
loop maximum, now including the final interval, was 110,197 us. PCM had zero
recorded underruns during the short run; thirty-minute endurance remains pending.

Battery epoch 1 retired with heap/errors/denied zero, 315 queries and one
notification. All 345 prior USB files are preserved except expected workspace
metadata. Offline checks pass: 350 files / 1,013 clusters, no leaks, bitmap
mismatches or errors. Ubuntu recovered, next_entry is empty, collector disabled,
and matching raw evidence plus diagnosis are backed up outside build. No reboot
is armed. Current integration `f1131519` also contains PR165's independent modal
pointer correction; this returned payload predates that correction.

Next target: shared app startup and cleanup delays under sustained disk
contention, including gated allocation/unmapping and process retirement. The
receipts establish the delayed phases, not the exact kernel cause. Preserve the
10-second whole-cycle, client deadlines, 500 ms loop and all integrity checks;
do not treat successful Files admission or a zero-status late exit as a pass.

### Shared app latency under disk load — active scope (2026-10-25)

Base `296f86bb`, including PR165 modal correction and PR166 return evidence.
Investigate starvation at the shared I/O admission gate: waking blocked app
allocation/cleanup calls does not necessarily let them run before a disk writer
starts another owned operation. Scope is scheduler gate handoff at a completed
userspace syscall boundary, dispatch/cancellation integration, deterministic
scheduler regressions and automatic disk/audio/app VM/native validation. Do not
release ownership mid-operation or admit unsafe calls during owned I/O. Preserve
all existing integrity checks, the ten-second whole-cycle and 500 ms loop limits.
Pro PR19 (`200ab19b`) remains separate USB Wi-Fi work; no other open shared-runtime
MR was found. The previous run is collected and backed up. User explicitly
authorizes tests, self-merge and reboot into the automatic stress workload.

The implementation retains each blocked call's admission flag after wake-all,
clearing it only when the gate wait returns or the thread retires. A fully
completed userspace syscall that required the I/O gate gives one ready waiter
a turn before the writer can submit another operation. No handoff occurs with
active/nested ownership or an unsafe caller. GUI/audio-safe syscalls retain their
normal turns. The scheduler's ordinary round-robin/service eligibility, syscall
whitelist, VFS checks and desktop timing limits are unchanged. Cancellation is
rechecked after handoff; killed/quarantined waiters cannot retain stale admission.

The syscall-boundary regression fails on the old implementation. New host cases
exercise 64 successive writer rounds, preferred awakened admission, nested-owner
and unfinished-kernel refusal, canceled waiter retirement, and cancellation
during handoff. Initial VM experiments are retained: the overbroad prototype
(which also handed off after GUI-safe calls) completed 13 cycles at 400 IOPS but
failed a 512,391 us loop gap; at 100 IOPS Text completed but Files exceeded the
whole-cycle deadline. These are failures, not native acceptance. The narrowed
implementation and its matched physical payload require fresh validation.

Final narrowed source `74939958` passes build/core, full smoke and the storage
service VM (throttled writes, USB typing, cancellation, copy comparison, fsck and
timeout/quarantine). The populated-Home 400-write-IOPS automatic stress passes
15 complete app cycles: maximum ready 1,423,863 us, close/reap 2,526,714 us and
loop 480,204 us, with zero app errors or PCM underruns. It verifies 117 unpaced
256-KiB bursts, 223 observed PCM/write overlap windows, preservation of all
66 prior fixture files and clean offline allocation. All original limits remain.

Release `95f8a9cf59b25ba1` freezes that clean source. Its exact-stage Q35 rehearsal
passes the expected native-profile refusal, save/unmount and automatic return;
this is not physical battery or stress acceptance. PR167 carries the fix and
this documentation-only follow-up. Main remains `296f86bb`; Pro PR19 `3e979549`
retains separate Wi-Fi scope without scheduler/dispatch overlap. Next is the
authorized thirty-minute automatic native audio/disk/Text–Files–System stress,
with listening only, automatic Ubuntu return and collection. Preserve accepted
recovery and the prior failed run; require complete matching receipts before
claiming hardware acceptance.

### I/O handoff native stress return — 2026-10-25 (failed)

Release `95f8a9cf59b25ba1` / compiled source `74939958` (PR167) returned matching
session `session-00065eb25dc59f36-00`. Thirty-four automatic Text/Files/System
cycles completed; the next Files cycle became ready after 7,991,615 us and hit
the unchanged whole-cycle deadline at 10,000,506 us during close/reap. Its
window was still present at the deadline. The process subsequently exited zero
during teardown. The run ended after roughly 2.5 minutes without a final
workload receipt, so thirty-minute endurance and the latency fix remain unaccepted.

The loop maximum was 110,744 us, completed close/reap maximum 5,571,788 us, and
PCM recorded zero underruns. Files' final OPEN request reached compositor
admission in 794 us; most startup delay preceded that request. This narrows
the next investigation to app bootstrap/allocation and close/retirement timing
under sustained disk contention, without proving the remaining kernel cause.
Keep the ten-second whole-cycle, client deadlines and 500 ms loop bound intact.

Battery epoch 1 retired cleanly with 862 queries, eight notifications and zero
heap/errors/denied. All 350 prior USB files are preserved except expected workspace
metadata. Offline fsck/allocation pass: 355 files, 1,019 clusters, zero leaks,
bitmap mismatches or errors. Matching evidence and diagnosis are backed up
outside build with verified hashes. Ubuntu returned, next_entry is empty and
the collector is disabled; no new reboot is armed. Integration remains `b537c2a2`.

### App lifecycle timing — active scope (2026-10-25)

Base `46f6212f`, with the preceding native failure collected and backed up.
Add bounded timing receipts in desktools main/Text runtime for entry,
initialization, window creation/publication, close delivery, worker retirement
and memory cleanup; validate the receipts in the automatic audio/disk/app VM.
This is instrumentation, not another claimed latency fix. Do not change the
workload, scheduling, ownership checks, ten-second whole-cycle, five-second
client deadlines or 500 ms loop bound. Prepare the same automatic native trial,
self-merge and reboot under the user's explicit authorization.

PR169 (`edc320d8`) owns compositor unsaved-work shutdown interactions; keep its
deskd/session changes separate. PR19 (`c4af7dcc`) retains USB Wi-Fi scope. The
planned edits are desktools runtime/timing, stress-receipt validation and this
unit record, with no overlapping production file edits in those open PRs.

Implemented sparse `app-timing` records with PID/role/window identity and
begin/end/event timestamps. Spans include scheduling, I/O waits and diagnostic
overhead; they are not CPU-time or gate-wait attribution. A begin without an end
remains incomplete evidence. First-publication and close-delivery events connect
initialization/cleanup phases to compositor ready/reap receipts. The VM validates
required phases, paired durations and lifecycle ordering for each completed app.
Native collection preserves the available timing tail even on early failure.

Source `23cf7ca3` passes build/core, timing-validator positive/negative cases,
full smoke and populated-Home 400-write-IOPS stress (13 cycles). Its first VM
attempt halted in a synthetic battery sample with an EC error before any
instrumented app started; that failure is retained. An identical repeat passed
without configuration or predicate changes. Files worker-pool retirement reached
4,337,451 us in the repeat, demonstrating phase separation, not a native cause.

Consumed merged PR169 / main `f658f6b7` as `a54e1717`. Combined build/core and
the same automatic stress pass: 15 complete timing-validated app cycles, maximum
ready 1,457,954 us, close/reap 3,168,146 us, loop 136,962 us, zero app errors or
PCM underruns, 221 observed playback/write overlap windows, all 66 prior fixture
files preserved and clean offline allocation. The earlier full smoke remains
at `23cf7ca3`; the combined source has fresh focused runtime validation.

Release `282c3f6a06afd9eb` freezes clean runtime source `a54e1717`. Exact-stage
Q35 rehearsal passes expected native-profile refusal and automatic save/unmount/
return, not native battery or endurance acceptance. PR170's final follow-up is
documentation only. Next is the authorized automatic thirty-minute native trial
with unchanged workload/limits, listening only and automatic return/collection.

### Timing trial: three native attempts — 2026-10-25 (failed/incomplete)

Release `282c3f6a06afd9eb` / source `a54e1717` (PR170) produced three matching
sessions. The user reports interrupting the first boot, unplugging AC after a
missing battery percentage on the second, and an automatic restart after about
nine to ten minutes on the third. The single-session collector refused the three
matches; preserve its original failure and reconcile all three separately.

- `session-00065eb30ecc535a-00`: boot log only, consistent with the reported
  interruption. No completion or runtime acceptance.
- `session-00065eb30ffd922b-00`: native battery preflight refused SSDT fingerprint
  `faa7c4fc101b8bed`, absent from the exact captured set. No PCM/app cycles started;
  the failed session drained and saved its exit log at 18.28 seconds. This explains
  the missing battery indicator and is consistent with automatic failure return,
  not evidence that AC unplug caused a crash. Preserve the firmware gate; the
  differing table needs a separate read-only capture/comparison before any change.
- `session-00065eb312133238-00`: 139 app cycles completed before Files cycle 139
  exceeded the unchanged whole-cycle deadline at 10,000,118 us during close.
  Exit log saved at 545.22 seconds uptime. No final workload receipt, so the
  thirty-minute endurance test failed. The loop maximum was 346,152 us and PCM
  recorded zero underruns. Battery epoch 1 retired with 3,151 queries, 18
  notifications and zero heap/errors/denied; 220 SCI deliveries were handled.

The final Files cycle reached ready in 5,692,308 us, but main-entry to first
publication took only 210,971 us. Runtime/pool/window/role initialization took
26/48/23/25 us. Most startup delay lies outside those initializers; inspect process
launch/first scheduling before assuming widget work is responsible. After close
delivery, worker-pool retirement took 4,069,936 us and crossed the cycle deadline;
history free took 13 us and window close 985 us. Files exited zero during teardown.
Next focus is process startup and file-worker retirement under disk contention,
with all existing deadlines/integrity checks retained. PR172's shared application
dispatch work must be inspected before overlapping userspace changes.

Reconciled all three sessions and available timing tails without promoting any
attempt to pass. All 355 prior USB files remain unchanged except expected workspace
metadata. Offline fsck/allocation pass: 364 files, 1,032 clusters, zero leaks,
bitmap mismatches or errors. Ubuntu recovered, next_entry is empty and the
collector is disabled. Evidence/diagnosis are retained outside build; no new
boot is armed. Integration baseline for this return record is `eda8bfbc`.

### Process admission and worker retirement — active scope (2026-10-25)

Base `aa7b7392`, with all three preceding attempts reconciled and backed up.
Investigate READY process startup/kernel retirement threads excluded by the I/O
gate without entering its explicit wait queue, and nonblocking child reaping
that repeatedly defers while I/O is owned. Extend quiescent handoff accounting
to those contenders without changing eligibility, nonblocking behavior, deadlines,
filesystem/ownership checks or workload. Scope: scheduler, syscall wait admission,
host regressions, automatic stress/boot validation and this unit record.
Retain the targeted app timings and the separate unresolved SSDT-fingerprint
refusal; no firmware gate changes. User authorizes tests, self-merge and reboot.

PR172 (`27a97f0f`) owns shared application dispatch/waiting/actions; the proposed
kernel admission changes avoid its userspace implementation scope. PR19
(`4030debb`) retains USB Wi-Fi work. Inspect their actual updated diffs before
integration; no cross-task messages or parallel local harnesses are authorized.

The scheduler now records actual READY exclusion and deferred nonblocking reaps
as pending quiescent turns. Ordinary owner-free selection consumes the request;
owned-I/O service turns preserve it. This covers process entry and post-exit
cleanup without running either under another owner's I/O. Host regression fails
on the previous scheduler and passes with the fix, including repeated requests,
service eligibility, sleeping threads and retirement continuations. Build/core
passes; automatic workload and native acceptance remain pending.

The first positive VM completed 14 app cycles with zero app errors/PCM underruns,
but strict timing validation failed on an interleaved live UART line. Preserve
that failed harness evidence. The console already saves whole diagnostic records
before preemptible UART output; validate those saved overlapping snapshots instead.
The validator retains phase, ordering, duration and missing-record checks, rejects
duplicates within one snapshot, and deduplicates only exact overlapping records.
No production console path, workload or limit changes are needed.

Source `cca1f818` passes build/core, all 482 CLI commands and the complete smoke
suite, storage-service slow writes/input/cancellation/copy/relaunch/quarantine,
and the populated-Home 400-write-IOPS automatic workload. The positive VM
completed 19 timing-validated app cycles with zero errors and PCM underruns;
maximum ready/close/loop were 1,161,076 / 2,140,722 / 228,514 us. Files pool
retirement peaked at 742,552 us. All 66 prior fixture files survived and offline
allocation was clean. These VM timings do not establish native acceptance.

Release `788873710e559400` freezes that clean source. Exact-stage Q35 rehearsal
passes the expected native-profile refusal, saved logs, unmount and automatic
return. This follow-up changes documentation only. PR172 (`122fe555`) retains
shared app runtime ownership, PR19 (`50e4dce5`) USB Wi-Fi, and PR175 (`a90ac149`)
macOS build/docs; no overlapping kernel production edits were found.

Next is the authorized thirty-minute native audio/disk/app trial, with automatic
Text/Files/System launches and closes, continuous verified I/O, battery monitoring
and listening only. Whole-cycle ten seconds, client five seconds and loop 500 ms
remain unchanged. USB baseline is 364 files / 1,032 clusters with zero leaks,
bitmap mismatches or errors. Ubuntu remains default with console recovery and
accepted reopen payloads retained. The collector saves/checks multiple attempts
before refusing aggregate acceptance; missing completion remains failure.

### Process-admission native return — 2026-10-25 (failed)

Run `5732b14012b34571`, release `788873710e559400`, runtime `cca1f818`
(PR174) returned one matching session, `session-00065eb3b8aaf11c-00`.
The automatic trial failed after 49 complete app cycles: Files cycle 49 / PID73
crossed the unchanged ten-second whole-cycle deadline at 10,010,054 us during
close. Exit log saved at 190.998 seconds uptime. No final workload receipt was
written; this is not thirty-minute endurance acceptance.

The aggregate maximum ready time was 1,341,489 us, versus 7,313,178 us in the
preceding run; different run lengths prevent a general performance claim. Maximum
completed close was 7,869,937 us, and maximum loop gap 341,476 us. The failing
Files instance received Close at 180,124,884 us but did not leave its main loop
until 188,505,227 us, an 8,380,343 us gap. Its directory-list request 1 to worker74
was first logged submitted during post-deadline job drain. It then exited with
status143 during failed-session teardown, before a pool-retirement completion.
This failure precedes the pool-retirement phase seen in the previous trial.

Next investigate worker request/reply pipe admission under continuous disk
ownership. The current nonblocking pipe deferral returns AGAIN without requesting
the quiescent turn now used by deferred reaps. That is a concrete candidate
mechanism, not yet a demonstrated cause of this native delay. Preserve accepted
mutation ownership, CRC/reply validation and all deadlines; do not bypass cleanup.
PR172 still owns the shared application runtime and must be inspected before
any overlapping userspace work.

PCM recorded zero underruns. Battery epoch1 retired with 1,065 queries, one
notification, 71 handled SCI deliveries and zero heap/errors/denied. Overall
battery-controls reports FAIL because the desktop trial failed; clean provider
retirement does not promote the workload result. All 364 prior files survived
except expected workspace metadata. Offline fsck/allocation pass: 369 files,
1,038 clusters, zero leaks, bitmap mismatches or errors. Matching logs, original
automatic assessment and diagnosis are hash-verified outside build. Ubuntu
recovered, next_entry is empty and collection disabled; no new reboot is armed.
Integration baseline for this evidence record is `a679115f`.

### Integrated-main retest — active scope (2026-10-26)

User requests rebasing onto the bug fixes merged since the previous native trial
and retesting. Rebased `device/macbookair6-1/process-admission` onto actual main
`dfd63fef`; all local changes were already merged, so no commits required replay.
This consumes shared application runtime PR172, synchronization/build audit
PR175, AHCI completion/budget corrections PR178, and event-adapter audit PR179.
The last Air6,1 failure remains recorded and backed up; no native acceptance is
inferred from other hosts' results.

Scope is validation and this unit record: matched build/core, automatic populated
Home audio/disk/app stress, general smoke, storage cancellation/quarantine and
exact-stage rehearsal, followed by the same authorized automatic native trial.
Do not add the previously proposed pipe-admission fix before testing the new
baseline. Preserve ten-second whole-cycle, five-second client and 500 ms loop
limits, continuous verified disk work, PCM checks and firmware/ownership gates.
PR19 (`eababae2`) retains separate USB Wi-Fi ownership; no shared production edits
are planned. Prior explicit self-merge/reboot authorization remains applicable.

The fresh Ubuntu build reproduced a merged poll compile regression: the new
synthetic-readiness path uses timespec/clock_gettime without including time.h,
and its strict-C host fixture lacks the POSIX feature declaration. Extend this
validation scope only to the explicit time header and fixture feature macro;
retain the failed build log. No runtime policy, pipe admission or limits change.

The next build gate exposed an outdated process-service host fixture: mapping
retirement now references scheduler_wake_all for futex invalidation, but the
fixture did not provide it. Add a checked wake hook and exercise an active futex
on a retiring page, asserting invalidation/wake occurs before frame release.
This updates test integration only; the merged process behavior is unchanged.

The new audit drivers also mixed SrvrOS headers with GCC's hosted stdint/glibc
internals on Ubuntu. Compile their target-header translation units in freestanding
mode, retaining host linking and the separately compiled native pthread shim.
Apply this to time, synchronization and event-adapter fixtures; no test assertions
or production behavior change. Preserve the failed compile as portability evidence.

Candidate `5f0326b4` passes the full matched build/core suite, including the new
time/synchronization/futex/event-adapter host regressions, and the complete thread
guest suite. General smoke passes all 482 CLI commands, shell editing, syscall
ABI, UEFI keyboard and display. Storage-service passes throttled writes/input,
cancellation, file comparison, repeated launches and timeout/quarantine.

The populated-Home 400-write-IOPS combined VM completed 14 timing-validated app
cycles, 117 verified disk bursts and 225 observed audio/write overlap windows,
with zero app errors or PCM underruns. Maximum ready/close/loop were 1,348,680 /
6,484,660 / 188,430 us. All 66 prior fixture files remained intact and allocation
checks passed. The long close duration reinforces the need for native testing;
this is not a claimed fix or acceptance of the previous Files-close failure.

Release `83c919c9b7a24b6b` freezes clean candidate source `5f0326b4`. Exact-stage
Q35 rehearsal passes the expected native-profile refusal, saved logs, clean
unmount and automatic return. This final follow-up is documentation only. Main
was rechecked at `dfd63fef`; PR181's separate Text/Files window-lifecycle scope
and PR19's Wi-Fi work are not included. Native run `d10d796558a34499` will repeat
the same authorized thirty-minute automatic workload with unchanged limits.
USB baseline: 369 files / 1,038 clusters, zero leaks/mismatches/errors. Ubuntu
remains default; console recovery and accepted reopen backups are retained.

### Integrated-main native return — 2026-10-26 (failed)

Run `d10d796558a34499`, release `83c919c9b7a24b6b`, compiled `5f0326b4`
(PR180, merged as `afc8bb67`) returned one matching session,
`session-00065ec131ee0a4e-00`. The automated trial again failed during Files
close: cycle13 / PID25 crossed the unchanged ten-second whole-cycle limit at
10,020,194 us after 13 completed app cycles. The final snapshot was saved at
67.914 seconds uptime; no complete workload receipt exists. This is failed
native endurance evidence despite the passing preboot host/VM checks.

Maximum ready/completed-close/desktop-loop times were 1,363,239 / 7,260,207 /
109,543 us. All 13 completed cycles pass the saved-record timing validator.
The failed Files process received Close at 56,508,490 us, but left its main loop
at 65,489,095 us: an 8,980,605 us gap. Its directory-list request1 to worker26
was logged submitted between the 57.210 and 62.551 second battery records,
before the deadline. Job drain began only after the deadline; teardown exited
PID25 with status143, without a pool-retirement completion. Unlike the previous
run, submission occurred before failure, so request admission alone is not a
proven explanation. Inspect both request and reply progress under continuous
disk ownership; the existing nonblocking pipe deferral remains a candidate,
not an established cause. Keep all ownership/integrity checks and limits.

PCM recorded zero underruns. Battery epoch1 retired with 360 queries, one
notification, 24 handled/zero unclaimed SCI deliveries and zero heap/errors/denied.
The overall desktop/battery-controls result remains FAIL. All 369 prior files
were preserved except expected workspace metadata. Read-only fsck and allocation
checks pass with 374 files / 1,044 clusters and zero leaks/mismatches/errors.
Raw evidence and diagnosis are retained outside build with verified hashes.
Ubuntu recovered, the one-shot entry is empty and collection is disabled; no
new reboot is armed.

Closeout baseline is `afc8bb67`. Open PR181 (`7bd16b5e`) owns shared Text/Files
window lifecycle and accepted-job retirement; inspect its current implementation
before overlapping userspace changes. PR182 (`352f9b0d`) owns exFAT import
validation and PR19 (`3e16ff6a`) USB Wi-Fi. None was included in this physical
payload. This closeout changes only this unit's evidence record.

### Worker pipe admission — active scope (2026-10-26)

Proceeding from main `9f18d7d6` after collecting the failed integrated retest.
Scope is the kernel nonblocking pipe/PTY admission boundary, its directed syscall
and scheduler regressions, and this unit's unchanged automatic audio/disk/app
trial. Both request writes and reply reads currently return AGAIN during another
storage owner's turn without requesting the quiescent admission already used by
nonblocking reaps. Test and repair that missing request while preserving immediate
deferral, user service eligibility, descriptor cleanup safety and coalesced
scheduler ownership. This is a candidate mechanism for the observed close delay;
native acceptance remains pending.

PR181 (`dafd5bed`) owns shared Text/Files window and accepted-job lifecycle;
avoid competing userspace changes. PR182 (`352f9b0d`) owns exFAT import validation
and PR19 (`7961d09e`) Wi-Fi. Neither requires changes to this kernel admission
scope. Keep ten-second cycles, five-second clients, 500 ms loop bound, continuous
verified I/O, PCM checks, firmware gates and accepted-operation draining intact.
Next checkpoint: failing-before/passing-after host tests, matched build and VM
stress/storage checks, exact-stage rehearsal, merged candidate and authorized
automated native trial with saved evidence and Ubuntu recovery.

Implemented `49f51e9b` requests the existing quiescent turn from both nonblocking
pipe/PTY deferrals. The new directed syscall test fails before the fix and passes
after for read/write/duplex/PTY retries, with blocking, invalid and nonpipe
exclusions. Scheduler coverage retains one coalesced request across 64 owned-I/O
service turns and consumes it only at quiescence; cleanup/owner gates remain.
The current admission guide records this shared contract. PR181 edits different
sections of that guide; no userspace implementation is changed here.

Matched normal/native-battery/VM-kernel build and full core suite pass, as do
thread, general smoke (482 CLI commands, shell edits, syscall ABI, UEFI keyboard
and display), and storage-service cancellation/timeout/quarantine regressions.
The populated-Home 400-write-IOPS VM completes 19 timing-validated app cycles,
117 verified bursts / 30,670,848 bytes, and 218 observed audio/write overlap
windows with zero app errors or PCM underruns. Maximum ready/close/loop times
are 1,291,587 / 1,860,049 / 402,419 us. All 66 prior fixture files survive and
allocation is clean. Files' maximum Close-to-return is 1,753,005 us versus
6,373,924 us in the prior VM run; this is a run comparison, not native acceptance
or proof that all sources of delay are fixed.

Release `09f42ae5a75b3fef` freezes clean compiled source `49f51e9b`. Exact-stage
Q35 rehearsal passes expected native-profile refusal, saved logs, clean unmount
and automatic return. This follow-up changes documentation only. Native run
`6d1b066474954f5d` repeats the unchanged thirty-minute automated workload with
374 baseline files / 1,044 clusters, continuous audio/verified disk work and
automatic Text/Files/System cycles. Ubuntu remains default; console and accepted
reopen recovery payloads remain verified and independent of build storage.
