# MacBookPro11,2 / A1398

Updated: 2026-10-21. State: **native desktop, input, brightness, sound, CPU/fan telemetry and captured USB Ethernet DHCP/DNS/HTTP and live cable-status UI accepted**.

The sections below retain the dated bring-up sequence. The latest acceptance
and next scope are recorded at the end; earlier failures are historical.

## Current handoff

The user considers the current Pro bring-up generally working and closes this
phase on October 21. Network/UI acceptance is confirmed on release
`2f8abe0e131023d8`. Current installed release `5d65ddcc4ccbeaf2` is the
collected Wi-Fi identity capture, with `e47dca3a6b30c6c1` retained as recovery. Earlier desktop acceptance
stands for its named payloads; the latest run did not repeat sound, brightness
or Text saving. Closing this phase does not relabel skipped checks as passes.
Battery work, headphone testing and further trackpad-latency tuning stay deferred.
Ubuntu remains default and no reboot is armed.

Next scope: the Ethernet adapter has been replaced by Edimax `7392:a711`, USB
revision `0100`, bound by Linux to `mt76x0u`. Captured inventory and the opt-in
identity diagnostic are implemented and native identity reads accepted. Firmware,
radio, authentication and UI implementation remain pending. Preserve
shared Ethernet behavior; coordinate through the unit record and reviewable PRs.
Air PR #17 concerns SCI controller observation and does not overlap this scope.

## Original bring-up scope (historical)

Local on-device agent, branch `device/macbookpro11-2/bringup`, starting from
`main` at `db40ed510636847d9aad2c7d38fea9976de43252`.
Current objective: diagnose and enable the built-in `05ac:0262` keyboard.
Scope: captured descriptor fixtures and xHCI host regression coverage first;
collect unattended native USB/controller diagnostics before choosing any driver
change. The v19 capture path in `kernel/src/main.c` will explicitly print xHCI
controller/endpoint state into its saved snapshot; the diagnostic text will no
longer label every keyboard as the Air fixture. The first trial has no persisted USB log, so the failure layer is unknown.
PR #4 remains the Air EC/AML owner's scope; no overlapping implementation is
planned. Baseline `origin/main` remains `db40ed5` as fetched after this trial.

## Observed configuration

- Ubuntu 26.04.1 LTS, x86_64, kernel `7.0.0-38-generic`; UEFI boot with a
  separate mounted FAT ESP and Ubuntu GRUB default `0`.
- DMI model MacBookPro11,2; firmware `MBP112.88Z.0142.B00.1708080655`.
  Exact marketing year has not been independently confirmed.
- Intel Core i7-4870HQ at 2.50 GHz; 16 GB installed memory.
- Intel integrated graphics `8086:0d26`, Linux `i915`, boot VGA selected;
  connected eDP panel exposes 2880×1800. No NVIDIA GPU appears in PCI inventory.
- Internal APPLE SSD SM0256F, about 234 GiB, Samsung AHCI controller
  `144d:1600`. Original versus replacement history is unknown.
- Intel xHCI `8086:8c31`; Apple keyboard/trackpad `05ac:0262` with two captured
  HID report descriptors. Linux reports the Apple HID driver.
- Analog HDA controller `8086:8c20`, Cirrus Logic CS4208 codec `1013:4208`,
  subsystem `106b:5e00`; HDMI controller `8086:0d0c`, codec `8086:2807`.
- Linux applesmc exposes two fan channels (Left side and Right side).
- Broadcom internal Wi-Fi `14e4:43a0`; attached Edimax USB wireless adapter
  `7392:a711`; Bluetooth `05ac:8289`; FaceTime camera `14e4:1570`.
- Attached USB flash storage `13fe:6700` has an existing mounted exFAT partition.
  Its UUID and paths are retained only in the ignored local checkpoint.

The privileged Linux capture completed with **zero unavailable/partial reads**,
including ACPI tables, both Apple HID report descriptors, graphics topology,
audio codec dumps and CPU policy. The earlier unprivileged capture retained its
18 permission-denied ACPI reads for provenance. Raw captures remain local under
ignored `build/hardware/` storage.

## Acceptance and recovery

Physical release `a5b5ae578027e8c0` reached the console, confirmed by the user.
The built-in keyboard **failed**. No result file or log session was created;
USB persistence, trackpad, scaled display, audio, brightness, sensors, power and
networking remain untested. Boot visibility does not accept these providers.
The consumed one-shot has been checked after return; Ubuntu remains recovery.
No physically accepted SrvrOS recovery release exists. Optional AML stays off.

For future trials, prefer an unattended boot/check/save/unmount/return cycle.
If manual commands are necessary, display them in the boot environment or wait
for the user's confirmation before triggering reboot. This is the user's standing
instruction; do not require keyboard commands for diagnostic collection when
keyboard support itself is under investigation.

## Host and VM validation

At source `7fff5cc` (inventory-only change over `db40ed5`):

- `make check-host`: PASS, including all pinned submodules and OVMF.
- `make -j2 all usb-image test-core`: PASS; Zig 0.15.2 verified by bootstrap.
- `make smoke`: PASS (syscall ABI, 481-command shell regression, shell editing,
  UEFI USB keyboard and displayd). Final shell filesystem check reported no
  errors or leaks.
- `make check-docs`: PASS.
- Prepared the existing exFAT workspace without formatting or replacing files.
- Exact console release `a5b5ae578027e8c0`, v7 workspace profile:
  `hardware_boot_smoke.py --console-check` PASS (GRUB chainload, RAM filesystem,
  USB input, case/Caps/repeat and renderbench). This rehearsal does not attach
  the physical workspace and does not establish physical persistence.

The candidate and Linux/VM evidence have a durable local archive outside
`build`; its path is recorded in `build/bringup/checkpoint.json`. It is a tested
VM candidate, not an accepted physical recovery release.

## Next checkpoint

First console trial is collected as a keyboard failure. The captured Pro
configuration now passes the actual xHCI parser and endpoint-context tests:
interface 0, endpoint 0x83/DCI 7, packet size 10, interval 8. The Air fixture still
passes. This disproves a simple missing product-ID selection rule; physical
transport/report handling remain unresolved.

At `38ad189`, the v19 unattended profile explicitly saves controller/input
diagnostics even when PS/2 appears ready. Build, xHCI/keyboard host tests and
documentation checks pass. Exact release `bfc0b7492f2677b7` passes VM capture,
offline saved-byte verification, clean exFAT and automatic reset. Its missing
workspace rehearsal halts without reboot. The physical diagnostic is next;
it requires no commands and does not claim working keyboard support.
Preserve the previous console payload and Ubuntu default; collect the exact new
release on return before changing the driver or preparing another boot.

## Native diagnostic failure and current fix scope

Release `bfc0b7492f2677b7` halted without a saved USB session. The user's
photograph is archived locally. It shows the port-12 device stopped at the
device-descriptor stage and no keyboard registered. The captured Linux device
descriptor reports EP0 size 8; the driver initializes full speed at 64 and never
evaluates the endpoint context after learning its actual packet size.

The attached exFAT partition is on GPT. USB attachment only registers MBR
partitions, explaining why the workspace cannot be selected even when the
flash device is found. No reformat or partition-table mutation is planned.

Bounded shared changes now owned here: xHCI EP0 enumeration/packet-size update
and their host tests; validated GPT discovery for USB storage and block-layer
regressions. Existing Air fixture and MBR paths must keep passing. Air EC/AML
work remains separate. These are implementation findings, not physical fixes
accepted on this machine. Collect-before-restage is complete for the failed run.

## EP0 and GPT candidate

Source `dd5ba7e` corrects full-speed EP0 initialization to 8 bytes, reads the
8-byte descriptor prefix, and evaluates the live endpoint context when a larger
packet is advertised. Both Air and Pro descriptor fixtures pass; malformed
sizes and failed context commands are rejected. USB storage now registers
validated GPT partitions. The physical drive has a saturated protective-MBR
length; this is explicitly covered without trusting it as a writable extent.

Validation: full build/USB image and core tests PASS; focused xHCI, block/AHCI and
USB storage tests PASS; block tests under ASan/UBSan PASS; UEFI USB input/boot
regression PASS. Initial release `ad01d8a08f9b70eb` passes missing-workspace,
MBR capture and GPT capture rehearsals. Final release `fe8e4d3e22ba97eb`
adds saturated protective-MBR coverage and passes the exact GPT capture,
offline saved-log verification, clean exFAT and automatic reset rehearsal.

Next physical run uses final v19 release `fe8e4d3e22ba97eb`, with no manual
commands or optional AML. It must establish keyboard enumeration and saved GPT
workspace diagnostics before input acceptance. Actual typing and trackpad
behavior remain unaccepted. Retain the original console as previous and Ubuntu
as recovery; collect the matching session after return before another stage.

## Native EP0/GPT result and workspace repair

The next photograph of `fe8e4d3e22ba97eb` shows the built-in `05ac:0262`
configured with one keyboard registered, and `/work` mounted from `usb0p1`.
This establishes enumeration and GPT discovery on this unit, not actual typing.
The capture still halted without logs: exFAT correctly refused writable access.

Offline inspection found a clean flags field and a single FAT, but the allocation
bitmap entry declared 59,149 bytes for 473,184 clusters (59,148 required).
`fsck.exfat -n` reported the invalid size despite returning zero and printing a
clean summary. After archiving all files and the boot/FAT/root/bitmap metadata,
`fsck.exfat -p` corrected it. A byte comparison shows only root-directory offset
24 changed; the boot regions, FAT and bitmap contents are identical. A repeat
read-only check reports no errors, and all existing file hashes match.

Retry the same exact tested release with this corrected workspace. Save a new
run record rather than overwriting the failed trial. Ubuntu and the previous
console payload remain recovery; collect logs before further staging. Keyboard
reports/typing, trackpad and successful native log persistence remain pending.

## Successful native diagnostic retry

Release `fe8e4d3e22ba97eb`, session `session-00065e3b75192c41-00`, saved
matching boot and exit logs after the bitmap repair. The logs establish
`05ac:0262` keyboard enumeration (interface 0, DCI 7, packet 10), GPT workspace
mount, diagnostic writes and flush. The user confirmed an automatic return to
Ubuntu. The one-shot entry was consumed and Ubuntu remains the default; the
post-return offline exFAT check reports no errors. Evidence is archived outside
`build` along with the separate retry record.

Keyboard reports were zero at the diagnostic snapshot, so actual typing remains
untested. Trackpad, desktop interaction and other device providers are also not
accepted by this run. Next milestone is a visible typing test with instructions
in the boot environment, preserving the standing reboot policy.

## Visible keyboard acceptance scope

Next bounded work adds an opt-in, guided keyboard capture to the existing
save/unmount/return profile, with per-step deadlines and persistent results.
Touch kernel startup, boot-profile staging and exact-stage VM harness/tests.
Test letters, Shift, Caps Lock, Backspace, Enter and all arrows through the
keyboard character path (not serial injection). VM coverage must exercise both
correct and incorrect input. A timeout/failure must be recorded as such, then
return safely when persistence succeeds. Other device scopes remain unchanged.

Guided v27 release `5c3c1f8a0106166c` (kernel source `9238d40`) passes build,
keyboard host tests, 29 staging/profile tests and three exact-stage GPT VM runs:
ten prompted keys produce PASS; a wrong key produces FAIL; no input reaches the
30-second deadline and produces FAIL. All three save the matching result to the
exit log, pass offline exFAT verification and reset automatically. The instruction
screen was visually inspected. The same payload is next for physical typing;
no physical keyboard acceptance is claimed yet. Preserve the successful native
diagnostic as previous, with older payloads retained in the durable archive.

## First native guided typing result

Release `5c3c1f8a0106166c`, session `session-00065e3b9a24480c-00`, saved
`keyboard-test: result=FAIL passed=5 total=10 dropped=0`. Lowercase, Shift,
Caps Lock on/off and Backspace passed. At the Enter prompt, the final HID report
contains usage `0x38` (slash); six characters were pushed, with 17 completed
keyboard reports, zero transfer errors and zero drops. The test ended before
its first 30-second deadline, so this is wrong input rather than a timeout.
Enter and arrows remain unaccepted; clarification of the physical key pressed
is pending. Do not infer a driver mapping fault or a complete keyboard pass.

Matching logs and run metadata are archived outside `build`. The consumed
one-shot and Ubuntu default were verified; offline exFAT reports no errors.
No new reboot is armed.

The user reports running out of time and the system rebooting. Preserve that
observation alongside the saved trace: the exit snapshot is at about 24.6 seconds
after boot and the last report contains slash, so the existing 30-second step
deadline cannot explain this recorded failure. Automatic return is user-confirmed.
For a retry, improve the guided test with more time and recoverable unexpected
input rather than treating this as a proven Return-key driver defect.

## Forgiving guided retry scope

User authorized the retry. Extend the guided capture to 90 seconds per step,
keep unexpected keys within the same step, and explicitly log retries/timeouts.
Touch only startup capture, versioned staging, VM coverage and these guides.
Retain the successful diagnostic as previous. Prior native evidence is collected;
Enter and arrows remain pending. Existing EC/AML work is outside this scope.

Retry release `29af8019a1737d6e`, source `e8d89f3`, uses v28 with 90-second
per-step deadlines and recoverable unexpected input. Build, keyboard host tests,
29 profile tests and documentation checks pass. Exact-stage GPT VM tests prove
that no input saves FAIL after the full deadline and that ten deliberate wrong
keys followed by the correct keys save PASS with ten retries and zero drops.
Both runs verify saved logs, clean exFAT and automatic reset. Physical retry is
next; retain `fe8e4d3e22ba97eb` as previous and preserve all earlier run evidence.

## Native guided keyboard acceptance

Release `29af8019a1737d6e` (source `e8d89f3`), session
`session-00065e3bb98f1b54-00`, records `PASS passed=10 total=10 dropped=0`
and `retries=0`. Lowercase, Shift, Caps Lock on/off, Backspace, Enter and all
four arrows passed on the internal `05ac:0262` keyboard. The final input
snapshot records 25 completed reports, zero errors and zero dropped input.
These tested keyboard functions are physically accepted on this unit.

Matching boot/exit logs are verified and archived outside `build`; the
post-return offline exFAT check is clean. Ubuntu default and consumed one-shot
are verified. No reboot is armed. This acceptance does not cover every key,
long-duration repeat, trackpad, desktop interaction or other device providers.
Next milestone is trackpad and desktop bring-up; retain the successful diagnostic
as previous and the accepted keyboard payload in the durable archive.

## Trackpad and desktop scope

Begin exact `05ac:0262` Wellspring7 support from this unit's captured configuration:
keyboard interface0 endpoint83 packet10, vendor-HID interface1 endpoint81 packet64
interval2, boot mouse interface2 endpoint84 packet8. Type2 contact layout and
feature mode switch are documented in Linux bcm5974; Air0291 Type3 differs.
Touch bounded decoder/profile/mode transport tests and desktop trial staging.
Do not enable sibling IDs or infer physical gestures from fixtures. Preserve
accepted keyboard behavior and explicit AML exclusion. Fetch found integration
`origin/main=b6075ef` (merged EC/AML PR4); this checkout still builds from its
recorded db40ed5 ancestry plus device commits, not the new EC code. No overlapping
trackpad edits were found in that integration delta. Prior typing run is collected.

Trackpad/desktop candidate `c4b7b7bb5eb48b3a`, source `ca645d9`, is v29 at
explicit 2880x1800/2x scale. Tests cover Type2 synthetic framing/bounds, exact
Pro interface/DCI and endpoint isolation, feature read/modify/write, outbound
control setup direction, failed mode switches and actual Type2 dispatch. Existing
Air fixture, keyboard, pointer and desktop host tests pass. Build and 30 staging
tests pass. Exact GPT VM trial saves typed Text content, a 1440x900 screenshot,
matching logs, and cleanly unmounts; the final instruction screen was inspected.

Native trial next: no time limit, on-screen pointer/click/drag/scroll checklist,
then save and Return to console (F10, End, Return if pointer fails), followed by
`reboot` after `USB unmounted=yes`. Retain accepted keyboard `29af8019a1737d6e`
as previous and archive this candidate before arming. Trackpad motion, gestures
and Retina desktop behavior remain unaccepted until native evidence returns.
Type2 protocol reference: [Linux bcm5974](https://raw.githubusercontent.com/torvalds/linux/master/drivers/input/mouse/bcm5974.c).

## Native desktop and trackpad result

Release `c4b7b7bb5eb48b3a` (source `ca645d9`), session
`session-00065e3be5f7034d-00`, returned with matching boot/exit logs. The user
reports that apps, keyboard and trackpad worked, with no brightness control or
sound. The desktop exited with status 0. The Type2 trackpad recorded 12,163
reports, 11,860 contact reports, 872 button reports, zero malformed reports and
zero transfer errors. Gesture telemetry records 2,492 scroll events and two
primary taps. Keyboard telemetry records 70 reports, zero errors and zero drops.
This accepts the exercised desktop/input behavior, not every app operation or
all gestures. The profile requested 2880x1800 at 2x scale.

Native logs are copied and verified in a durable archive outside `build` with
the exact payload. Offline exFAT is clean, the one-shot is consumed and Ubuntu
remains default. No reboot is armed. Preserve this accepted desktop payload for
subsequent recovery; the installed previous payload is the guided keyboard trial.

Brightness and HDA were not enabled in v29. The read-only laptop probe reports
unsupported chipset, and audio telemetry remains disabled with zero commands.
The existing brightness path matches Air GPU/PCH/firmware and a fixed PWM period;
the CS4208 audio path matches Air subsystem `106b:7e00`, whereas this unit captured
`106b:5e00`. Neither gate may simply be widened to claim Pro support.

Next bounded objective: read-only Pro backlight/register and codec-routing
evidence, then separate evidence-matched providers and tests. Potential touch
points are laptop probe/backlight, HDA codec selection and explicit diagnostic
profiles. Optional AML, EC ownership, fan control and sibling models remain out
of scope. Refetched `origin/main` remains `b6075ef`; open PR6 changes Air ACPI lock
handling, not these providers. This result still uses the recorded device branch
ancestry, not merged PR4 code. Collect-before-restage is complete; no new stage
or physical trial is prepared yet.

## Pro backlight diagnostic implementation scope

Proceeding under the standing bring-up authorization. First add a read-only
Pro display probe, gated on this unit's captured DSDT and GPU/PCH identities.
It will read only the four documented CPU/PCH PWM registers, without binding a
brightness writer or accessing EC ports. Use the existing unattended v19 GPT
capture/save/unmount/reboot workflow. Touch laptop probe/resource matching and
host tests; preserve the accepted desktop as previous. Native boot register
state is required before choosing a brightness control profile. Linux reports
maximum 1953, distinct from the Air's 2777. Audio evidence also shows two speaker
pins (0x12 and 0x13), so the Air's single-speaker route is insufficient.
`origin/main` remains b6075ef and PR6 remains ACPI-only at this scope refresh.

Diagnostic release `a488fbd08bf01e44`, source `7f2d122`, implements the exact
Pro DSDT/GPU/HM87 gate and reads PWM offsets 0x48250/0x48254/0xc8250/0xc8254
through read-only uncached mappings. It binds no writer and bypasses all Air
EC/resource accesses. Register definitions were checked against
[Linux v6.12 Intel backlight registers](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/gpu/drm/i915/display/intel_backlight_regs.h).
Build, existing backlight/resource tests, actual Pro firmware identity/refusal
tests and documentation checks pass. The exact v19 GPT VM rehearsal verifies
saved log bytes, clean exFAT, unmount and reset. Physical capture is next, under
existing automatic-reboot authorization; no user input is required. Collect this
release on return before implementing a writable provider or staging again.

## Native read-only Pro backlight result

Release `a488fbd08bf01e44`, session `session-00065e3c79b9a84e-00`, completed
the diagnostic and saved matching boot/exit logs. The exact firmware/chipset
gate matched. Native registers: CPU control/duty both zero, PCH control
`0xc0000000`, PCH period/duty `0x07a101f9`: override mode, period 1953,
duty 505 (about 26%). This matches the independently captured Linux range and
duty. No brightness writes, provider binding, EC access or audio initialization
occurred. This accepts discovery only, not brightness adjustment.

Logs, payload and collection metadata are archived outside `build`. The offline
exFAT check is clean, Ubuntu default and consumed one-shot are verified, and no
reboot is armed. Installed previous is the accepted desktop `c4b7b7bb5eb48b3a`.
Next implement an explicit Pro brightness profile using the observed mode and
period, preserving the Air profile and rejecting changed registers/readback
failures. Then test its shared Settings path before physical adjustment. Audio
remains a separate two-speaker-route implementation and acceptance milestone.

## Pro brightness provider scope

Implement the captured 1953-period PCH override profile explicitly, retaining
Air's 2777-period default and all mode/readback refusal checks. Touch backlight
binding, Pro probe mapping, host coverage, an opt-in scaled brightness desktop
profile and its VM harness. Keep sound and EC/AML disabled in this trial. Add
on-screen Settings instructions for deliberate brightness adjustment and exit.
The prior capture is collected; origin/main is still b6075ef and PR6 owns ACPI.

Brightness candidate `c7dd411a4ea31af4`, source `1f2d677`, uses v30 scaled
desktop with brightness enabled and audio/AML/SMC off. Startup binds the exact
Pro profile without changing duty. Host tests exercise both period profiles,
wrong-profile refusal, all 5–100% requests, drift quarantine and failed readback;
Settings message/response tests and 31 boot-profile checks pass. Full build and
exact GPT VM desktop rehearsal pass (typed document, screenshot and matching
logs saved; clean unmount). The initial guide was visually inspected: brightness
steps and console/reboot instructions are visible without scrolling.

Next physical check: adjust Settings to 25/50/75%, confirm visible panel changes
and responsive input, restore a comfortable level, then Return to console and
reboot after USB unmounted=yes. No deadline or automatic reboot in this desktop
trial. Retain accepted desktop c4b7b7bb5eb48b3a as previous; archive the candidate
and run record before arming. Brightness adjustment remains physically pending.

## Native brightness acceptance

Release `c7dd411a4ea31af4` (source `1f2d677`), session
`session-00065e3c9d5e862e-00`, has matching saved boot/exit logs. The user confirms
brightness worked well across the whole range. The exact Pro provider initialized
at 26% without changing the panel. All 108 logged adjustment requests matched
their readback, spanning 5–100% and finishing at 41%. This accepts manual
Settings brightness adjustment on this unit; media keys and automatic dimming
are not covered.

Desktop exit status is zero. Trackpad telemetry records 5,374 reports with zero
malformed reports or transfer errors. No keyboard reports occurred in this saved
snapshot; prior keyboard acceptance remains separate. Offline exFAT is clean,
Ubuntu default and consumed one-shot are verified, and no reboot is armed.
Logs and the accepted payload are archived outside `build`. Current installed
release is now accepted for brightness; previous remains `c4b7b7bb5eb48b3a`.
Preserve this brightness payload when staging audio work next. Sound remains
disabled in this profile and requires Pro-specific CS4208 routing and tests.

## Pro audio implementation scope

Add exact Intel 8c20 / CS4208 1013:4208 / subsystem 106b:5e00 support.
Captured outputs are headphone pin10/DAC2, speaker pin12/DAC3 and bass speaker
pin13/DAC4; all speaker converters must receive the stereo stream and be muted
when headphones are selected. Captured GPIO0 enables the speaker amplifier.
Validate Pro revision, pin defaults, connections and capabilities before codec
mutation. Scope: audio controller selection, codec route model/tests, explicit
scaled audio trial and on-screen sound checks. Preserve Air and QEMU behavior;
no HDMI, microphone, EC/AML or sibling support. origin/main remains b6075ef;
PR6 remains ACPI scope. Accepted brightness release remains recovery.

The captured Pro codec revision is 0x100300. Main speaker pin12/DAC3 and bass
pin13/DAC4 receive the same stereo stream at channel offset zero. Headphone
pin10/DAC2 is separate. All three paths are validated before coefficient/GPIO
setup; route switches mute and disconnect both speaker converters. GPIO0 is
updated while preserving unrelated bits. Exact Pro pin defaults are checked;
Air, unknown subsystem, changed revision or invalid bass connection is refused
under the Pro profile. Linux's pinned
[CS4208 board table and GPIO fixup](https://raw.githubusercontent.com/torvalds/linux/v6.12/sound/pci/hda/patch_cirrus.c)
corroborate subsystem 5e00 and GPIO0; local codec capture supplies the routes.
The controller uses the existing Intel PCH traffic-class/coherent-DMA setup,
consistent with the [Linux PCH driver](https://raw.githubusercontent.com/torvalds/linux/v6.12/sound/pci/hda/hda_intel.c).

Candidate `e47dca3a6b30c6c1` (source `240893c`) is the explicit v31 audio,
brightness and scaled desktop trial. Build, HDA/controller/codec tests, audio
service/stream tests, Settings tests and 32 profile checks pass. Exact GPT VM
rehearsal passes saved Text/screenshot/logs and clean USB unmount; the initialized
codec also produces one captured 500 ms stereo tone with a silent tail (peak
1200). This emulated codec is not the physical Cirrus device. The initial Help
shows the sound checks; scroll for full exit and WAV playback instructions.
Physical speaker/headphone routing, quality and channel balance remain pending.

The separate stereo-48000 streaming VM regression also passes against this
build. No concurrent harness ran. Next install preserves accepted brightness
`c7dd411a4ea31af4` as previous. User listening checks use Settings Test/Stop at
low volume, optional wired headphones with speaker isolation, and the bundled
stereo WAV from the on-screen guide. Startup remains silent and this manual
trial has no deadline. Collect the exact session on return before another stage.

## Native audio result and trackpad latency report

Release `e47dca3a6b30c6c1`, session `session-00065e3cc8a4f706-00`, has
matching boot/exit logs and a clean desktop exit. The user reports sound worked
well. The exact CS4208/5e00 codec is ready with no error, two tone tests completed,
and PCM playback submitted 1,152,000 bytes with zero underruns. Final route is
speakers at 25%. Audible output is user-confirmed; headphone isolation and
individual main/bass output balance were not separately confirmed.

Trackpad telemetry records 3,020 reports, zero malformed frames and zero transfer
errors. The user reports noticeably slower cursor response than Ubuntu, estimated
at 15–45 ms. Preserve this as a subjective observation, not a measured bound or
a proven USB transport fault. Code inspection finds an input polling worker
waiting one timer tick and a compositor loop sleeping one tick, with a calibrated
100 Hz timer (10 ms ticks). Their scheduling plus rendering/scanout may contribute;
existing logs cannot apportion the end-to-end delay.

The matching evidence and sound-confirmed payload are archived outside `build`.
Offline exFAT is clean, Ubuntu default and consumed one-shot are verified, and
no reboot is armed. Accepted brightness `c7dd411a4ea31af4` remains previous.
Next bounded investigation: measure input polling, queue consumption and cursor
presentation timing under the scaled desktop, then choose a change supported by
the measurements. Inspect shared input/compositor/scheduler scope on origin
before editing; avoid a global timer-rate increase without affected regressions.

## Cursor timing scope

The refreshed Air PR6 now touches startup and boot-profile tools for battery
work. Avoid those overlapping files for this diagnostic. Add bounded timing
aggregates to the existing `--hardware-trial` compositor only, using the current
v31 stage unchanged: pointer-consumption cadence, drawing/blit duration and
first consumed pointer event to final framebuffer write. No event ABI, timer,
scheduler or xHCI behavior changes. Report measurements once at exit, not per
frame. They exclude device/USB arrival delay and scanout, and are not end-to-end
latency. Preserve the accepted sound build while measuring the scaled desktop.

The diagnostic records four software intervals in the existing hardware-trial
mode: each pointer-poll gap; gaps between consecutive polls that both consume
motion; first consumed pointer event through final successful framebuffer blit;
and drawing plus blit duration. Histogram percentiles are upper bucket bounds,
not exact percentiles. A coalesced input batch produces one presentation sample.
Idle poll gaps are separate from consecutive-motion gaps. Failed draws and
unavailable/backward clocks produce no duration sample. USB/device delay,
time already spent in the kernel input queue, and panel scanout are excluded.
No scheduler, xHCI or event ABI was changed; normal desktop mode has no added
clock reads. The standalone host test covers batching, gaps, failures and clocks.

Timing candidate `cae101ddceccbd1d`, source `74073ee`, passes build, standalone
host timing tests and exact v31 GPT desktop/audio rehearsal. A local wrapper
injects twelve VM pointer movements and verifies the saved timing records;
consume-to-final-blit has twelve samples. The sparse injected movements do not
exercise consecutive-motion sampling (zero samples, not a pass for that metric).
The host test covers its accounting; continuous physical movement is required.
The VM also saves document/screenshot/logs, cleanly unmounts and verifies a tone.
VM timing values are emulator observations and must not be used as laptop latency.

Next native run: move continuously over desktop and Text, then drag a window,
about 15 seconds each. Save and Return to console, then reboot after clean
unmount; all instructions remain in Help. Preserve sound-confirmed
`e47dca3a6b30c6c1` as previous. Collect the exact session and compare active
cadence versus draw/consume durations before selecting any latency fix.

## Native cursor timing result

Release `cae101ddceccbd1d`, session `session-00065e3d2bb52f21-00`, saved
all four timing aggregates and exited cleanly. Native measurements:

| Software interval | Samples | Mean | Maximum | p95 upper bound |
| --- | ---: | ---: | ---: | ---: |
| Pointer poll gap | 32,504 | 9,999 us | 12,650 us | 12,650 us |
| Consecutive motion poll gap | 5,713 | 9,999 us | 12,191 us | 12,191 us |
| First consumed event to final blit | 6,035 | 392 us | 2,412 us | 2,000 us |
| Draw plus blit | 6,098 | 402 us | 3,570 us | 2,000 us |

These results establish the 10 ms compositor sampling cadence and fast drawing
in this run. They support investigating input scheduling before rendering
optimization; they do not measure USB arrival, kernel queue age or panel scanout,
and do not prove the entire subjective 15–45 ms delay is software polling.
Histogram percentile bounds must not be represented as exact quantiles.
Trackpad reports total 6,667 with zero malformed reports or transfer errors.

Matching logs and collection metadata are archived outside `build`. Offline
exFAT is clean, Ubuntu default and consumed one-shot are verified, and no reboot
is armed. The USB UUID is unchanged but Linux assigned a different device node;
the local checkpoint now reflects it. Sound-confirmed `e47dca3a6b30c6c1` remains
installed previous. Next inspect event-driven pointer wake and controller polling
coordination, retaining timer/idle behavior until affected regressions are covered.

## Pointer wake implementation scope

Add a bounded pointer wait syscall and mouse-queue wake notification. The desktop
will wait for queued pointer input or one tick, replacing unconditional one-tick
sleep. Keep the 100 Hz timer, xHCI polling and storage ownership unchanged for a
controlled comparison. Dedicated queue readiness is rechecked with IRQ exclusion
by the scheduler to avoid lost wakes. Signal/cancel checks remain bounded by one
tick. Touch mouse queue, syscall number/dispatch/wrapper, compositor and existing
mouse/sleep host tests. Refreshed PR6 does not touch these files; avoid its startup,
profile and Makefile changes. Native timing baseline remains 9,999 us cadence.

Implementation `58884d6` adds syscall 133 (`srv_pointer_wait`) with a dedicated
mouse wait queue, one-tick deadline, non-consuming readiness and bounded
signal/exit checks. Scheduler readiness recheck closes the enqueue-before-block
race. Enqueue/overflow/cancel paths notify waiters; motion coalescing retains
existing readiness without redundant notifications. Desktop fallback remains
one-tick sleep if the new syscall is unavailable or interrupted. Hardware trial
logs count input-ready returns, timeout returns and errors.

Full build/USB image and `test-core` pass, including mouse order/overflow,
arrival at the wait boundary, arrival while blocked, timeout, signal/exit,
early-scheduler fallback and storage-safe syscall admission. The xHCI worker
still samples at the original rate, so this change can remove an extra desktop
wait but cannot by itself guarantee sub-10 ms USB sampling or total latency.

Candidate `9ae804d340bee2cb` (source `98fac37`, implementation `58884d6`)
passes the exact v31 GPT desktop/audio rehearsal. Twelve injected VM pointer
samples are saved, with ten pointer-ready returns, 1,264 timeouts and zero wait
errors. Document/screenshot/log persistence, clean unmount and captured tone
also pass. Emulated pointer delivery does not accept native USB responsiveness.

Matched 10-second idle checks use the archived pre-change timing release
`cae101ddceccbd1d` in an isolated image tree and the new build. Host QEMU desktop
CPU is 10.88% before versus 10.08% after; console CPU is 5.99% in both. Both
checks verify subsequent keyboard entry and saved file bytes. These short VM
observations show no busy-wait increase in the check, not a physical energy
measurement or statistically established CPU reduction. Preserve raw metrics
and baseline script with the candidate.

Next native run repeats continuous movement and dragging, compares perceived
response, and collects wait counts/timing. Keep sound-confirmed e47dca3a6b30c6c1
as previous rather than promoting the timing-only candidate. No timer-rate or
xHCI changes are in this release; remaining transport/scanout delay is separate.


## Native pointer wake result

Release `9ae804d340bee2cb`, source `98fac37`, session
`session-00065e3d67ca2723-00` returned with a clean desktop exit. The bounded
wait recorded 4,130 input-ready returns, 62,866 timeouts and zero errors.
Trackpad capture recorded 4,466 reports, zero malformed reports and zero
transfer errors. No audio tone was exercised in this run; the earlier audible
acceptance remains separate.

| Software interval | Samples | Mean | Maximum | p95 upper bound |
| --- | ---: | ---: | ---: | ---: |
| Pointer poll gap | 66,995 | 9,968 us | 12,494 us | 12,494 us |
| Consecutive motion poll gap | 3,820 | 9,854 us | 11,541 us | 11,541 us |
| First consumed event to final blit | 4,129 | 599 us | 3,962 us | 2,000 us |
| Draw plus blit | 4,162 | 601 us | 3,960 us | 2,000 us |

The input-ready path ran successfully, but active cadence remains close to
10 ms (previous mean 9,999 us). These differing manual workloads do not establish
a statistically significant improvement. Drawing remains below 1 ms on average;
USB arrival, kernel queue residence and scanout remain outside these intervals.
The user reports that response felt **worse**. This candidate fails perceived
responsiveness acceptance; clean logs do not override that regression.
Next diagnostic scope is controller delivery and queue residence, if the delay
remains, before choosing another scheduling or transport change.

Matching evidence is archived outside `build`; offline exFAT is clean, installed
payload hashes and Ubuntu default are verified, and the one-shot is consumed.
No reboot is armed. Current is the pointer-wake candidate; sound-confirmed
`e47dca3a6b30c6c1` remains previous and independently archived. Refreshed
`origin/main` is still `b6075ef`; Air PR6 at `0038ff4` owns the native battery
work and does not alter this input implementation. This return changes only
this machine's evidence record, not shared driver behavior.


Following that report, the installed current SrvrOS entry was restored to the
sound-confirmed `e47dca3a6b30c6c1`. The regressed pointer-wake release is retained
as previous for diagnosis only, with logs and payload independently archived.
Ubuntu remains the default; no reboot was requested or armed. Source retains
the published experimental implementation for review; it is not accepted as a
latency fix. Next work should measure USB delivery and queue residence and
isolate scheduling effects before another physical candidate.

## Queue and report timing scope

After the rejected wake trial, restore the compositor's unconditional one-tick
wait and add bounded aggregate diagnostics to mouse input and xHCI trackpad
processing. Record oldest/latest coalesced event queue residence, consecutive
contact report processing gaps, and processing-to-rearm time. No input event
ABI, controller polling rate or timer change. These timestamps cannot observe
hardware report arrival or display scanout. Touch mouse input/header, xHCI,
compositor and mouse host fixtures; avoid PR6's startup and harness changes.
Integration baseline remains b6075ef. Accepted installed payload stays unchanged
until exact-stage validation of a diagnostic candidate.

The diagnostic implementation restores fixed one-tick desktop sleeping and logs
that policy explicitly. Queue timestamps exclude mixed sources and synthetic
cancellation/overflow events. Fixed-size counters reject unavailable/backwards
clocks and aggregate overflow; no per-event logging is added. Full build,
USB image, test-core and documentation checks pass. Mouse fixtures cover
coalescing, mixed sources, backwards clocks, cancellation and overflow; xHCI
fixtures cover report/rearm samples and lift sequence reset. Native timing
and perceived response remain untested for this diagnostic build.

Diagnostic candidate `ad6d1b9700372be6` (source `6968afb`) passes exact v31
GPT desktop/audio rehearsal: scaled display, document/screenshot/log persistence,
clean unmount and a captured tone. The VM verifies fixed-one-tick policy and the
queue aggregate log format; its emulated mouse supplies zero Apple trackpad
queue samples, so it does not validate native residence values. On-screen Help
requests continuous circles over desktop/Text and window dragging for about
15 seconds each, with untimed manual exit and reboot instructions. Preserve
sound-confirmed `e47dca3a6b30c6c1` as previous. Next collect this exact session
before any further stage, compare queue and dispatch intervals, and ask whether
response returned to the earlier baseline.

## Final input diagnostic and remaining bring-up — 2026-10-20

Release `ad6d1b9700372be6`, session `session-00065e3db88dea73-00`, returned
with desktop exit 0, fixed-one-tick policy, 2,590 trackpad reports and zero
malformed/transfer errors. Queue residence: 2,375 samples, oldest mean 358 us
(max 10,460), newest mean 247 us (max 8,040). Consecutive contact dispatch gaps:
2,551 samples, mean 9,829 us, max 26,009. Dispatch-to-rearm: 2,590 samples,
mean 2 us, max 6. Consecutive motion polls averaged 10,000 us; consumed-event
to final blit averaged 77 us. These are software intervals, not finger-to-pixel
latency; this run does not prove a subjective improvement.

The user completed testing and requested moving on after this round. Further
trackpad latency work is deferred, not marked fixed. The failed pointer-wake
behavior remains disabled. Evidence is independently archived, offline exFAT
is clean, installed hashes/Ubuntu default are verified and no reboot is armed.
Current payload is this diagnostic; sound-confirmed `e47dca3a6b30c6c1` remains
previous and independently archived.

Current capability ledger and suggested next order:

| Area | Local status / remaining work |
| --- | --- |
| Boot/recovery, scaled internal display, keyboard, desktop apps, USB workspace | Exercised successfully; broaden cold/warm boot and storage-load regression coverage |
| Trackpad | Functional; latency investigation deferred; gesture combinations need separate exhaustive acceptance |
| Brightness | User confirmed full offered 5–100% range |
| Audio | User confirmed sound; headphone isolation, individual channel/route checks and microphone remain separate |
| CPU/thermal/fan telemetry | Native CPU temperature/package energy/active-frequency idle capture accepted below; native paired two-fan readout accepted below; fan control not accepted |
| Battery and charger | Native Pro ACPI/EC ownership and live battery/AC reporting not accepted; reuse reviewed shared groundwork, not Air firmware assumptions |
| Networking | No accepted physical network path; validate supported wired USB hardware first; internal Broadcom Wi-Fi is separate driver work |
| Power behavior | Lid events, orderly power-off and suspend/resume each need implementation/acceptance review; idle/load power baseline pending |
| Additional hardware | External displays/hotplug, Bluetooth, camera, SD and Thunderbolt peripherals remain separate later milestones |
| Storage policy | RAM root and USB workspace remain intentional; native internal-disk writes require a separate scope |
| Integration | Device changes published in PR5; review/integration and regressions on the shared baseline still required |

Recommended next bounded milestone: read-only native CPU temperature/fan and
idle/load telemetry matched to this unit's inventory. Keep fan control and
optional AML out of normal startup. Battery/AC follows its shared runtime
review and Pro-specific firmware evidence. No next boot has been scheduled.

## Read-only telemetry scope — 2026-10-20

Begin with native CPU temperature/package energy/CPU0 active frequency, then
separately implement this unit's two-fan readout. Linux CPUID is 0x40661,
GenuineIntel family 6/model 0x46; leaf 6 EAX=0x77 and ECX=9. CPU0 reads of
0x606, 0xce, 0x611, 0xe8, 0xe7, 0x1a2, 0x19c and 0x1b1 succeeded. Units are
0xa0e03, nominal ratio 25, temperature target 100 C with zero TCC offset.
Linux exposes Left side and Right side fans. Development-load samples are
fixtures, not an idle baseline or native acceptance.

Bounded first change: explicit 06_46 CPU profile alongside 06_3D in thermal
and power predicates, captured decoder fixtures and service refusal tests.
No MSR writes, fan control, EC/AML activation or frequency policy changes.
Use the existing v26 unattended desktop idle capture; SMC remains unavailable
on this Pro until an explicit two-fan profile and ABI are validated. Avoid
PR6 startup/boot-harness files. Refreshed origin/main remains b6075ef; PR6 at
6db5a12 retains battery/AC runtime ownership. Preserve accepted desktop recovery.

Preflight found that v26 launches idle capture without explicit scale, while
this Pro may supply 2880x1800 and the screenshot protocol caps BMPs at 8 MiB.
Extend scope to compositor idle-capture scale selection and capture host tests:
when no scale is supplied and the full framebuffer exceeds capture limits,
select 2x if its logical dimensions fit. Explicit scale and ordinary desktop
remain unchanged. This avoids editing PR6's startup/boot-profile paths.

The CPU profile and idle-capture sizing implementation passes full build,
USB image, and test-core. Captured Pro thermal/power values are replayed beside
the existing Air fixtures; unsupported sibling model IDs, hypervisors and
missing features remain refused. Capture tests cover Air/full-HD 1x, Retina/4K
2x and over-limit dimensions. The compositor preserves explicit scale choices.
No native Pro sensor acceptance is claimed before the unattended capture.

Candidate `de341a3cfcaa7acb`, source `f58dee1`, passes the exact v26 GPT
rehearsal at 2880x1800: automatic 2x logical desktop, 60-second idle window,
1440x900 saved screenshot, persisted logs, clean unmount and automatic reset.
VM power/thermal providers correctly report unavailable, with zero readings;
this validates the workflow and refusal, not native sensor values. Separate
power smoke verifies default-disabled and opt-in hypervisor refusal. Native
acceptance requires valid fresh thermal samples and a non-null continuous
power window within the idle measurement. SMC remains intentionally unavailable
on this Pro; two-fan support and controlled load measurements follow separately.

The same release's missing-workspace VM check also passes: it halts with
`saved=NO unmounted=NO` and does not reboot. Success and expected-failure
receipts are retained separately. The physical trial is unattended: leave input
alone for the idle interval; it saves/unmounts and returns automatically on
success. Keep sound-confirmed e47dca3a6b30c6c1 as recovery and collect the exact
release before starting fan implementation or another stage.

## Native CPU telemetry result and two-fan scope — 2026-10-20

Release de341a3cfcaa7acb, session session-00065e3debdfd870-00, completed the
59.992-second Text idle capture. Six valid power checkpoints cover 52.993 seconds:
9,161 mW package average, 3,466 MHz CPU0 active frequency, and 93.85% scheduler
idle samples. No rejected power samples. Thermal samples remained valid; initial
CPU/package was 81 C, final 66 C after screenshot work (100 C target). These
short-run readings are not equilibrium, whole-laptop power or a load baseline.
The screenshot is 1440x900, copied with matching hash. Logs and independent
archive verified; offline exFAT clean; Ubuntu default/consumed one-shot verified.

Next bounded change: Pro two-fan readout, preserving Air behavior. Captured
DSDT APP0001 at _SB.PCI0.LPCB.SMC declares I/O 0x300..0x31f, MMIO 0xfef00000
and IRQ6; use only the proven bounded I/O read protocol, no MMIO/IRQ ownership.
Linux-driver-mediated reads show FNum ui8 length1=02, F0Ac/F1Ac fpe2 length2
(raw 23a0 and 2120 at capture). Only those three keys were captured; the Linux
key selector was restored. No control keys or firmware mutations.

Touch SMC service, its host fixtures, versioned SMC status ABI and System fan
formatting. Exact Pro GPU/PCH plus existing captured DSDT identity gates access;
validate count=2 and both key metadata before publishing a complete fan pair.
ABI v2 appends a second fan value; all kernel/userspace callers build together.
Air remains exactly one fan. Quarantine either fan's transport failure and
invalidate the whole pair. Shared Air PR6 at 09b48ac does not touch these files;
origin/main remains b6075ef. CPU capture is collected before any new staging.

Two-fan implementation passes full build/USB image/test-core and focused SMC
host tests. Tests retain Air single-fan behavior, require exact Pro identities,
reject count mismatch and bad second-key metadata, verify F0/F1 read command
keys, publish only a complete pair (including valid zero RPM), and invalidate
both on a second-fan timeout. Protocol fixtures reject control-write commands.
The matched ABI v2 build updates System to display both speeds. Native readout
and the visible two-fan System presentation are not yet physically accepted.

Two-fan candidate `d2928b8e81f148cd`, source `3604d74`, passes the exact
v26 GPT/Retina VM capture: 60-second idle window, 1440x900 screenshot, saved
logs, clean unmount and automatic reset. The VM correctly refuses the native
SMC profile. A separate missing-workspace rehearsal halts without saving,
unmounting or rebooting. Receipts are retained separately. The next physical
run is unattended and must show a fresh complete two-fan pair with no failures,
as well as the completed idle capture/save/unmount/return. Visible System fan
formatting remains a separate interactive check. Keep the sound-confirmed
`e47dca3a6b30c6c1` recovery payload and collect this exact release on return.


## Native two-fan readout accepted — 2026-10-20

Release `d2928b8e81f148cd` (source `3604d74`), session
`session-00065e3e2405d9d1-00`, completed the automatic capture and returned to
Ubuntu. SMC reached active with two fans: 76 complete pairs, 156 transactions,
zero failures/errors. Final F0Ac/F1Ac readings were 2,162/1,991 RPM, published
with the same timestamp and count. Nine periodic snapshots were valid, fresh
under three seconds, and advancing. This accepts native read-only paired fan
telemetry; it does not accept fan control or the visible System presentation.

The 59.997-second Text capture had 93.88% scheduler idle samples. Six package
energy checkpoints covered 52.998 seconds at 9,246 mW average and 3,466 MHz
CPU0 active frequency. No rejected power samples; final temperature was 71 C
after screenshot work. These are short package-only measurements, not thermal
equilibrium, whole-machine power, or a controlled load baseline. The saved
1440x900 screenshot and matching logs were copied and independently archived.
Offline exFAT is clean; installed hashes, Ubuntu default and consumed one-shot
were verified. No reboot is armed. Current remains this diagnostic; accepted
sound-enabled `e47dca3a6b30c6c1` remains previous. Installation reclaimed one
obsolete unreferenced payload only after verifying its durable archive.

Next bounded milestone: controlled load/thermal baseline and visible System
readout validation. Battery/AC still depends on reviewed shared runtime work
and this Pro's firmware evidence. Trackpad latency remains deferred by request.

## Controlled-load preparation — 2026-10-20

Refreshed main remains b6075ef; Air PR6 is now e9af5d3 and owns active
battery/USB capture changes in startup, boot profiles, collectors and Makefile.
Prepare an opt-in single-worker load diagnostic inside existing threadstress,
with independent host fixtures, plus System two-fan display fixtures. No shared
boot/profile edits in this scope. Fixed idle/load/recovery phases, bounded work
chunks, fresh valid sensors and thermal stop conditions are required. This is
not a CPU policy or fan-control change. Native automatic profile wiring remains
a review dependency; do not label the existing idle profile a load capture or
reboot into a payload without the necessary save/return path.

The opt-in diagnostic is implemented as `threadstress --cpu-load-capture`;
ordinary threadstress behavior is unchanged. It has fixed 30/30/30-second
idle/load/recovery phases, one integer worker, 100 ms sensor checks, one-second
logs, and fail-closed version/state/freshness validation. It refuses load entry
at 80 C and stops at min(90 C, reference minus 10 C), on throttling/critical
flags, sensor failure or clock failure. No fan or CPU-policy writes.

Full OS/USB build, System fixtures (including both native fan values, valid zero
pair and invalid pair), and standalone load fixtures passed. Twelve negative
fixture scenarios and two successful model-shaped runs passed. Default-disabled
and opt-in hypervisor VM runs both refused load and returned to the monitor.
These establish implementation/refusal behavior, not native load performance.

Remaining dependency: a distinct reviewed capture profile and phase-aware power
collector, touching PR6-owned startup/staging/collection paths. Defer that shared
integration until ownership is resolved; do not reuse the idle acceptance label
or alter a staged release. The System display has host-formatting evidence but
still needs a native visible check. Existing installed fan diagnostic and accepted
recovery remain unchanged; no reboot is armed. Local test logs are under
`build/bringup/pro-load-*` and the standalone fixture command is documented in
`docs/power-telemetry.md`.

## Proposed load capture wiring — 2026-10-20

User requested proceeding after the dependency report. Main remains b6075ef;
Air PR6 is 556fec5, with v34 battery/USB overlap active. Prepare a distinct v35
CPU-load proposal on this device branch: a small opt-in startup branch, immutable
stage profile, exact-stage refusal rehearsal and separate phase-aware collector.
This overlaps PR6-owned startup/staging files, so defer shared integration and
resolve those hunks during review. Do not merge or replace the Air implementation.
The proposed profile runs the bounded standalone worker and saves its result,
unmounts and returns on handled sensor refusal as well as success. Missing
workspace/save/unmount remains a halt. No optional AML or EC ownership is added.

Proposed v35 implementation passes full build/USB image/test-core, existing
installer tests, bounded worker fixtures and new phase/profile fixtures.
Separate phase parsing refuses missing completion, failure markers, zero load
work and inconsistent boundaries; invalid power is null. Proceed to exact-stage
VM refusal/save/unmount/reset and missing-workspace checks before installation.

Candidate `9b7ff9eb6ebc3535`, payload source `c0f5fd5`, passes exact v35
GPT rehearsal: VM sensors/power remain unavailable, worker refuses without load,
failed result is persisted, exFAT verifies clean, and reset occurs. Separate
missing-workspace rehearsal halts with saved=NO/unmounted=NO and no reset.
The successful receipt explicitly records refused load and unavailable VM power;
it is workflow evidence, not load acceptance. Worker/collector completion checks
also require a successful process return. Rehearsal evidence is retained separately.

Next physical test is unattended: 30 seconds console idle, 30 seconds single
worker load, 30 seconds recovery, then save/unmount/return. A thermal/sensor
refusal may return earlier and must be collected as failure. No commands are
required. Preserve sound-confirmed recovery; inspect the exact new session before
any retry. System's visible two-fan presentation remains a separate check.

## Native bounded load capture — 2026-10-20

Release `9b7ff9eb6ebc3535`, session `session-00065e4a3e439b0a-00`, completed
all three console phases and returned successfully. Idle/load/recovery lasted
30.006/30.000/30.010 seconds; load completed 573,820 integer work chunks.
No thermal/sensor stop occurred. Logged core/package temperature peaked at
81 C during load; the last recovery sample was 60/62 C. Both fan readings
remained available, with 90 complete pairs and zero SMC errors at exit.

Three valid energy checkpoints cover 22.000 seconds inside load: 27,190 mW
package average and 3,500 MHz CPU0 active frequency. Recovery checkpoints cover
21.000 seconds: 9,293 mW and 3,462 MHz active frequency. Initial idle power is
unavailable: its two eligible checkpoints span 9,999,904 us, below the collector's
10-second minimum. Preserve that refusal rather than relaxing acceptance to
fit the result. Short console/single-worker data is not whole-machine energy,
thermal equilibrium, all-core stress or a desktop workload comparison.

Matching logs are copied and independently archived. Installed and accepted
recovery hashes, Ubuntu default and consumed one-shot are verified. Offline
exFAT is clean. No reboot is armed; current is the load diagnostic and previous
remains sound-confirmed `e47dca3a6b30c6c1`. The native visible System fan check,
shared integration review, and battery/AC dependencies remain open.

## Visible sensor trial scope — 2026-10-20

Prepare a distinct v36 scaled interactive desktop profile with accepted brightness,
audio and explicit CPU/SMC readout. Main remains b6075ef; Air PR6 at 41ecb6e
continues battery/USB work. Keep this small boot-profile proposal on the device
branch pending shared integration review. Update the opening Help checklist with
the two-fan check and exit commands. No timeout: user checks System/Sensors,
Pause/Resume and screenshot, then returns manually using the on-screen steps.
No AML, battery runtime, fan control or additional CPU load in this profile.

Candidate `c30ec628bf5d68aa`, source `831ba62`, passes full build/USB image,
System formatting fixtures, installer/profile checks and exact v36 GPT VM trial.
The trial verifies 2880x1800/2x, System startup, Text persistence, screenshot,
clean exit/unmount and an emulated audio tone. VM providers cannot accept native
fan visibility. The opening Help checklist carries the requested actions and
manual exit commands. Preserve the accepted sound payload as previous, archive
this candidate, then boot once for the user's untimed Sensors check. Collect
its matching session and screenshot before any further stage.

## Native System display accepted — 2026-10-20

Release `c30ec628bf5d68aa`, session `session-00065e4a63c1d349-00`, returned
with a saved 1440x900 screenshot showing Devices and Sensors side by side.
The user reported saving the devices/fans screenshot. Visual inspection confirms
readable L 2,161 / R 2,011 RPM, fan sample age 679 ms, core/package 47/51 C,
and no clipping of the paired values. Keyboard/trackpad and USB storage show
active drivers. Exit diagnostics show 6,330 paired samples, zero SMC errors,
and a clean compositor exit. Pause/Resume is unconfirmed in the retained evidence;
do not infer it from the screenshot. Battery/AC remains unavailable as expected.

Logs and screenshot were copied, hash-verified and independently archived.
Installed/recovery hashes, Ubuntu default and consumed one-shot were verified;
offline exFAT is clean. Current is the interactive sensor desktop, previous
remains sound-confirmed `e47dca3a6b30c6c1`; no reboot is armed. Shared integration
review and Pro-specific battery/AC groundwork are the next outstanding areas.

## Battery work deferred by user — 2026-10-20

The user explicitly deferred battery/charger work because another laptop is
handling it. Do not begin Pro battery/AC implementation or physical trials in
this workstream for now. Retain the unavailable status and consume reviewed
shared work later when requested. Next work should focus on shared integration
review and remaining non-battery acceptance; trackpad latency also remains
deferred under the earlier instruction. No reboot or new trial is scheduled.

## Rebase onto merged PR6 — 2026-10-20

At the user's explicit request, rebased the 75 device commits onto main
`d72d2af` (PR6), overriding the usual merge-without-rewrite preference. Local
backup `codex/pro-pre-rebase-20261020` preserves the published pre-rebase tip.
Resolved shared startup/staging/rehearsal conflicts by retaining both the merged
battery modes and Pro keyboard/load/scaled desktop paths. Battery implementation
and physical tests on this Pro remain deferred by user request.

Version numbers 31/36 exist in both probe and desktop namespaces. Scope Retina
resolution to desktop profiles so the merged battery profile recipes remain
unchanged. A compatibility matrix verifies all 42 historical main/Pro profile
configurations byte-for-byte; focused tests cover shared-number separation and
conflicting capture-mode refusal. Existing boot manifests and installed recovery
remain historical artifacts; rebasing source does not physically accept a new
build. No physical install or reboot is part of this rebase task.

Rebase validation passes: full OS/USB build and test-core; 40 installer/profile
checks, 12 desktop collector checks, load worker/parser fixtures, and docs links.
Exact GPT VM rehearsals pass for sensor desktop `8d8b85e8a68d1042` (Retina,
System startup, audio and USB save/unmount), load refusal `ea1a686c92c4fdce`
(saved failure and automatic return), and guided keyboard `896a375f17a793c4`
(wrong-key retries followed by all ten steps and saved return). These images
are VM-only. Installed accepted desktop/recovery are unchanged, and no reboot
is armed. Rebased history is published using an exact remote-head lease.

## Rebased physical regression scope — 2026-10-20

User authorized the next physical regression. Use VM-passed sensor desktop
`8d8b85e8a68d1042`, payload source `96840d9`, based on main `d72d2af`.
Fetch confirms main unchanged; Air PR7 adds host allocation checking and does
not change this payload. Its read-only checker was inspected and run separately
on this unit's unmounted USB: 65 files, 204 accounted clusters, zero leaks,
FAT/bitmap mismatches or errors. No shared implementation changes in this scope.

Check keyboard/trackpad, apps, brightness range, speaker audio, System's two
fan readings (including Pause/Resume), Text saving and a screenshot together.
Opening Help carries manual exit commands; this is untimed. Preserve Ubuntu
and sound-confirmed previous `e47dca3a6b30c6c1`, archive candidate and exact VM
receipt, and collect the matching new session before promotion or another run.
Battery/charger and trackpad latency work remain explicitly deferred.

## Rebased desktop native regression — 2026-10-20

Release `8d8b85e8a68d1042` (payload `96840d9`), session
`session-00065e4c52c62152-00`, exited cleanly with no collector attention.
The user confirmed typing, trackpad, apps, brightness, sound, both fan readings
and Pause/Resume worked; Text saving was explicitly skipped this round.
Do not claim a new native Text save/reopen pass. Screenshot persistence passed.

The saved screenshot shows L 2,156 / R 2,007 RPM, fan age 539 ms and core/package
46/51 C. Logs record pause/resume, 2,415 paired fan samples and zero SMC errors.
Matching logs and screenshot were copied, hash-verified and archived outside
`build`. Both offline exFAT checks pass, including allocation accounting with
no orphan/mismatch errors. Ubuntu default, consumed one-shot and installed/
archived recovery hashes verify. Current remains the rebased desktop; previous
remains `e47dca3a6b30c6c1`. No reboot is armed.

The named regression checks are physically accepted on this unit. Next remaining
non-battery bringup checks are headphone output/isolation and supported wired
USB networking, preceded by inventory and driver compatibility review. Battery
and trackpad latency remain deferred.

## Multiple-adapter groundwork scope — 2026-10-20

User is obtaining wired USB hardware; headphone tests are deferred for several
days. Prepare bounded provider inventory and System/Network Utility UI so every
registered adapter has a boot-local identity, driver/MAC/link state and explicit
active/inactive role. Preserve the single boot-selected IP stack and existing
provider priority; do not imply multihoming, live switching or arbitrary chipset
support. Touch net_device, the size-negotiated network status ABI, System and
Network Utility plus host/VM fixtures and current guidance. Main remains d72d2af;
Air PR7 owns filesystem/battery groundwork, with no network overlap identified.
No physical reboot is needed for this work. Hardware acceptance follows actual
adapter inventory. Battery and pointer latency remain deferred.

The first desktop network VM reproduced a hang after starting an unreachable
LAN ping: successful ping/DNS and report copies had already passed. ARP/DHCP/
DNS/ICMP timeout loops yield without blocking; the current idle-as-fallback
scheduler can select the same IRQ-masked caller and starve timer progress.
Replace these four polling yields with bounded RX wait-queue sleeps. Keep shared
scheduler/storage policy unchanged; rerun unreachable-target cancellation and
GUI responsiveness as well as adapter coexistence before accepting the fix.

Groundwork validation passes: full build/USB image/test-core, focused registry,
System, Network Utility and RX-worker fixtures, and 383 documentation links.
After the bounded wait fix, the mixed e1000/ECM desktop VM passes Ping/DNS,
adapter-attributed report copies into Text, typing during an unreachable ping,
cancellation, link down/up, compact layout, Help and clean exit. Screenshot
review confirms both adapter identities/roles are readable. The two-ECM VM passes
DHCP, successful and timed-out ping, 160 UDP echoes with packet-size/ZLP checks,
HTTP, USB keyboard coexistence, inactive traffic isolation and disconnect without
failover. Preserve the first failing desktop VM as regression evidence.

No native candidate was installed or rebooted. Current physical release stays
`8d8b85e8a68d1042`; its earlier acceptance does not cover this new networking
code. Headphone testing awaits hardware access. Next: capture the supplied USB
adapters' IDs/descriptors, assess chipset support, then stage a network-enabled
matched image with exact-stage VM validation before physical testing.

## Attached USB Ethernet trial scope — 2026-10-20

Captured adapter 0b95:1790 revision 0200 at SuperSpeed (5000 Mb/s USB).
Linux binds cdc_ncm to configuration 2 and reports an active 1000 Mb/s link.
The captured three configurations are vendor-specific, NCM, then ECM. Target
ECM configuration 3: control interface 0, data interface 1 alternate 1, MAC
string 5, 1514-byte segments, notification 0x81/16 bytes, bulk IN 0x82/1024
burst 5 and OUT 0x03/1024 burst 2. This is evidence for this adapter revision,
not blanket support for all products named AX88179.

Its SuperSpeed notification companion advertises zero bytes per interval, so
the strict generic ECM parser refuses it. Prepare a narrowly gated parser
compatibility path for the exact captured descriptor bytes and VID/PID/revision;
retain generic rejection and all transfer bounds. Add an immutable v37 scaled
network desktop with on-screen DHCP/diagnostic/return instructions. Validate
parser mutations, old recipes, full build and exact-stage USB networking VM
before an authorized native trial. Shared main remains d72d2af; Air PR7 has no
network overlap. Ubuntu and accepted desktop/recovery remain protected.

Candidate `debb2ddf3e9f4243`, source `1b2afb0`, passes exact v37 GPT VM
rehearsal: emulated USB ECM DHCP, saved successful router ping, 2880x1800/2x
Text/screenshot persistence, audio and clean return/unmount. The host ECM
fixture validates the captured configuration, all truncations, altered-byte and
sibling identity refusal; xHCI transport regressions and 41 boot-profile tests
pass. Two earlier rehearsals exposed Terminal close-confirmation/shortcut
requirements; retain their logs and use the corrected on-screen exit sequence.

Prepare one untimed native run with the adapter and cable attached. Opening Help
requests Terminal dhcp/ifconfig, exit and Command-W, then System/Network Utility
router ping/DNS and screenshot, followed by clean manual return. Preserve Ubuntu,
accepted current desktop archive and previous sound-confirmed recovery. Collect
the exact new session before any next trial; physical ECM support remains pending.

## First USB Ethernet acceptance — 2026-10-20

Release `debb2ddf3e9f4243` (payload source `1b2afb0`), session
`00065e4d6c789a85-00`, physically accepts DHCP and ping for the captured
`0b95:1790` revision `0200` adapter on this Pro. Native logs confirm ECM
configuration 3, the active usb-ecm provider, a DHCP lease and two ping processes
exiting status zero; the user confirms both DHCP and ping worked. No new
screenshot was recorded, and this run does not accept Network Utility GUI,
DNS, HTTP, throughput, cable cycling or multiple physical adapters.

Collection verified the matching logs with zero attention markers and a normal
desktop exit. Offline exFAT and allocation checks are clean (22 directories,
74 files). The one-shot entry is consumed; Ubuntu remains default. Installed
payload hashes and the previous sound-confirmed `e47dca3a6b30c6c1` recovery
match their archived manifests. Run, collection and checks are archived outside
build. No further boot is armed.

Next bounded scope: inventory another supplied adapter, or extend this adapter's
native DNS/HTTP/link coverage. Multiple-adapter inventory/UI and isolation remain
VM-tested; only one boot-selected provider owns IP configuration. Battery,
headphone testing and pointer latency remain deferred as previously agreed.

## Network maturity scope — 2026-10-20

Continue with the accepted adapter: persist DNS/HTTP results, exercise Ethernet
cable down/up with DHCP reacquisition and usable desktop diagnostics. Main
`76508bf` (PR7 filesystem fixes) is merged; Air's current published work remains
power/filesystem evidence with no network overlap. Scope: ECM link transitions
and queued traffic, HTTP client/local test fixture, USB network VM regression
and opening Help. Retain submitted DMA ownership and USB-removal quarantine;
USB hotplug registration and multihoming remain separate work. Rebuild and test
the merged filesystem baseline before another exact-stage native trial.

Saved-check review found shell `dhcp` and `dns` always returned success even
when their syscall failed. Extend this bounded scope to their exit statuses in
`userspace/sh/sh.c`, with VM failed/successful network checks, so saved reports
cannot label a failed lease as status zero.

Candidate `c7bf960edc860376` (source `193641e`) passes the exact v37 GPT
rehearsal: USB ECM DHCP/ping, scaled desktop, Text/screenshot/log persistence,
audio and clean unmount. Full build/test-core, added xHCI cable/DMA fixtures,
481 shell commands with a clean final filesystem, Help content/render fixtures
and 385 documentation links pass. The two-ECM VM passes three traffic outages
and recoveries, DHCP/DNS failure status, renewed leases, DNS, exact saved HTTP
response bytes, 160 UDP echoes/ZLP framing, inbound HTTP and keyboard/inactive
adapter isolation. Active-ECM desktop diagnostics pass saved Ping/DNS reports,
background typing/cancellation, bounded outage failure and restored ping.
Screenshots confirm readable adapter attribution, failure state and native Help.

QEMU's USB gadget keeps its CDC connection indication up during virtual cable
loss; the initial cable-notification rehearsal timed out and is preserved.
Actual down/up notifications are covered by host fixtures, not claimed as a
QEMU result. A prior serial-byte comparison failure (CR/LF translation) is also
retained; final validation compares the saved filesystem response bytes exactly.
Linux HTTP through the supplied adapter returns a response for the native target.

Next is an untimed native DNS/HTTP/cable trial. Opening Help contains the before
and after report commands, Ethernet-only unplug/reconnect checks, screenshots
and manual return instructions. Keep the USB adapter/workspace connected. This
new source has no physical acceptance yet. Preserve Ubuntu default, installed
previous `e47dca3a6b30c6c1`, and the independently archived accepted Ethernet
payload `debb2ddf3e9f4243`. Collect the matching session and saved reports before
any further trial; battery, headphones and pointer latency remain deferred.

## Native cable failure — 2026-10-20

Collected `c7bf960edc860376`, session `00065e4dd6afeb55-00`: the saved before
report confirms DHCP, DNS and HTTP 200 with successful statuses. Two screenshots
and the report verify by hash; no after report was found. The user reports a
slow disconnected indication and no reconnect indication. Logs show link state
unknown initially, no accepted CDC cable notifications, then a transmit-deadline
quarantine. This is a driver/link failure, not accepted cable recovery or merely
a repaint delay. Collection has one attention marker; normal desktop exit,
consumed one-shot, recovery hashes and offline filesystem/allocation checks pass
(23 directories, 83 files). Evidence is archived outside build; no boot armed.

Next bounded scope: capture this adapter's actual ECM notification format and
trace pending TX on cable loss before changing notification parsing or recovery.
Main remains `76508bf`; Air PR8 concerns battery/filesystem isolation. Preserve
DMA quarantine safety and exact device gates; do not hide failure with a longer
timeout or a UI-only connected indicator. Battery/headphones/latency stay deferred.

Linux ECM capture identifies the missing status: this exact adapter sends speed
and link-up notifications with wIndex 1 (data), not 0 (control). The previous
parser correctly rejected that unexpected interface. Configuration 2 and the
original Linux connection were restored after the bounded capture. Add the
notification-interface flag only in the exact captured descriptor compatibility
path. On a confirmed cable-down indication, retain the submitted TX/DMA and
suspend its deadline; on up, restore the normal two-second budget. Do not extend
unknown-link timeouts or recycle a live ring. Add capture-derived and ownership
fixtures before a native retry; raw interrupt-only capture stays local.

Cable-fix candidate `1212d33f6ccf6bd4` (source `f7bb195`) passes the full
build/test-core, capture-derived notification/ownership fixtures, two-adapter
ECM regression and exact v37 GPT desktop/audio rehearsal. Saved files/logs,
clean unmount and the readable opening Help were verified; documentation has
385 valid local links. Native cable status timing/recovery remains pending.
The next untimed retry saves fresh before/after reports and checks both UI
transitions with only the Ethernet cable unplugged. Retain Ubuntu default and
previous `e47dca3a6b30c6c1`; archive capture, payload and run state outside build.

Prompted automation scope: user requests automatic network checks/report saving,
with untimed unplug/Enter and reconnect/Enter gates. Touch desktop trial launch,
Terminal's explicit trial mode, bundled script, Help and VM fixtures. Main is
now `09207b0` (PR8 battery isolation), not yet consumed here; Air PR9 concerns
filesystem exclusion latency, not these network/terminal changes. Collected
`1212d33f6ccf6bd4`, session `00065e4e19aee836-00`, normal exit and no attention
markers: startup down/up notifications now parse; full cable recovery remains
unaccepted. No new trial starts before the automated flow is validated.

Prompted candidate `0875949083ad6ac7` (source `41375f1`) passes full build/core
checks, documentation checks and the two-adapter network regression. The actual
trial script saves exact local-server HTTP responses before/after a simulated
traffic outage, records disconnected DHCP failure, and rejects EOF at a gate.
The exact GPT/scaled desktop/audio rehearsal starts Terminal automatically,
waits at both gates until Enter, saves all phase reports and completion state,
then saves Text/screenshots/logs and unmounts cleanly. All three trial screens
were visually reviewed. Earlier EOF-fixture and argument-forwarding failures
remain in local evidence; final validation uses explicit positional targets.
Native cable timing/UI/recovery remains pending. Install with Ubuntu default
and `e47dca3a6b30c6c1` recovery retained; collect this candidate before another run.

Collected prompted `0875949083ad6ac7`, session `00065e4e72601882-00`: sequence
completed; after-reconnect DHCP, DNS and HTTP 200 all passed. Two physical
cable down/up cycles were logged without quarantine. System snapshots include
up and down states; visual update timing still awaits user confirmation. The
initial report failed because it ran before startup link negotiation completed
(link-up at about 4.44 seconds). Next bounded fix is only script startup
readiness: wait for the active provider's known link-up, with a finite budget,
before initial checks. Keep the two human cable gates untimed. Evidence and
reports are archived; offline filesystem/allocation and recovery checks pass.

The follow-up script now checks the active provider (ignoring inactive links)
for up to 30 one-second samples, saves the last startup snapshot, and reports
expiry explicitly. Host fixtures cover immediate readiness, delayed readiness
and budget exhaustion. It keeps both human cable gates untimed. This script
change is not installed or physically accepted; current installed payload
remains `0875949083ad6ac7` and Ubuntu remains the default.
The follow-up image build and two-adapter VM regression also pass, including
the actual startup gate and prompted sequence, saved HTTP response bytes,
disconnected DHCP failure, EOF refusal and the existing transport coverage.

User confirms reconnect UI remained stale in `0875949083ad6ac7`; other checks
worked. Do not accept UI recovery. Final native link-up was at 151.379 seconds,
with System close requested at about 151.98 seconds, so current logs cannot
separate late hardware detection from the one-second repaint interval. Next
bounded scope: System network transition timestamps and automatic (no Refresh)
VM down/up coverage in `system.c` / `desktop_network_smoke.py`. Main remains
`09207b0`; Air PR9/filesystem write-batching scope does not overlap. Preserve
successful native data-path evidence separately from unresolved UI timing.
The user estimates roughly a minute waiting and reports connectivity returned
quickly. Treat this as an unresolved live-update failure, not a confirmed
close-before-refresh case. Extend diagnostics to xHCI notification-completion
and processing timestamps; wake the network worker for status completions as
well as received packets. Log System state changes and a ten-second sampling
heartbeat. Test automatic up/down sampling, repaint dirtiness and no-F5 VM
recovery before another physical attempt. No native fix is claimed yet.

Full build/test-core pass with automatic System sampling/repaint assertions and
notification wakeup/timestamp coverage. The mixed e1000/ECM desktop VM now
shows automatic disconnect and reconnect without F5; the recovered System
screenshot visibly shows Connected / 1000 Mb/s. This validates the shared UI
path with e1000, not the native ECM notification timing. Native diagnostics
remain necessary; source changes are not installed and no reboot is armed.

Proceeding with the bounded UI diagnostic trial. Remote main is now `39b152d`
(PR9 filesystem exclusion and PR10 FAT batching landed); this trial retains
its already-tested `76508bf` integration base to isolate the network/UI changes.
No open overlapping device PR remains. New on-screen completion instructions
ask for a live System Network cable cycle, fifteen seconds in each state and
a screenshot if reconnect remains stale. Pending main integration is separate
from this native comparison; battery remains deferred.

Diagnostic candidate `81e3308ffcdfe637` (source `5d9ca16`) passes the exact
v37 GPT/scaled desktop/audio rehearsal: initial readiness, both prompted phases,
saved reports/sequence, Text/screenshot/log persistence and clean unmount.
The final live-UI instructions are visibly readable. Install retains Ubuntu
default and recovery `e47dca3a6b30c6c1`; native acceptance remains pending.
Collect startup/phase reports and correlate notification completion/processing
with System transition/heartbeat timestamps before changing the verdict.

Collected `81e3308ffcdfe637`, session `00065e4f10dd7857-00`: initial and
recovered DHCP/DNS/HTTP200 pass; startup readiness fix works on this unit.
User reports neither unplug nor reconnect visibly updated. System sampled the
final down at 40.629 s (driver 40.539 s) and up at 82.788 s (driver 82.518 s),
with continuing heartbeats and no quarantine. Thus sampled data is timely;
visible repaint/which displayed field remains unresolved. Next bounded scope
is a foreground idle-window VM transition test with no click/F5 after either
link change, since previous recovery screenshots focused System first.
The foreground, no-input down/up VM cycle passes; the recovered screenshot
shows Connected / 1000 Mb/s without focusing, clicking or Refresh. This is
shared-UI/e1000 evidence, not acceptance of the native report. Await the user's
identification of the unchanged field and whether Refresh/window movement helps.
No native screenshot was saved in this run. No new payload installed or reboot
armed; preserve the current diagnostic image for a focused follow-up.


Same-payload retest accepted (2026-10-20 local / October 21 UTC): release
`81e3308ffcdfe637`, source `5d9ca16`, session `00065e4fa158e3b8-00`.
User confirms everything worked and the UI updated. Fresh initial and recovered
reports pass DHCP, DNS and HTTP 200; cable-down DHCP fails as expected. Both
prompted gates and final sequence completed, with normal desktop exit and no
quarantine. System sampled the final down at 65.119 s (driver 64.599 s) and up
at 74.119 s (driver 73.299 s). This accepts visible disconnect/reconnect for
this captured adapter on this unit. Router/DNS lease values may stay unchanged
while the Link row and status message change. Earlier user-reported failures
remain historical evidence; this acceptance is from the explicit retest.

All five reports and matching session are hashed and archived in the dedicated
`retest-20261021T012530Z` directory beneath the durable release archive, without
overwriting the prior run. Offline exFAT checks pass: 27 directories, 104 files,
406 clusters, zero leaks, mismatches or errors. Ubuntu remains default, the
one-shot entry is consumed, and current/recovery payload hashes verify against
their archives. No reboot is armed. The bounded networking/UI task is complete;
next scope is separate integration review against newer main. Multiple physical
adapters remain untested; one boot-selected IP provider remains the limit.
Battery work, headphone testing and further trackpad latency work stay deferred.

Integration scope: user authorizes local validation and merging PR #5 while
Actions are disabled. Consume actual main `9d9c72e` (battery filesystem isolation,
operation scheduling and FAT batching), preserving Pro desktop/network profiles
and both machines' evidence. Resolve shared staging/kernel/tool conflicts by
behavior, run the CI baseline locally plus affected networking regressions,
then merge the reviewed PR. Installed native payload stays `81e3308ffcdfe637`;
the combined source tree requires its own later physical acceptance.


Integration validation completed locally on `8985b81` against main `b090d94`
(PRs #8–#12 included). The two staging-tool conflicts preserve both Pro
keyboard/CPU-load/scaled desktop/network profiles and Air v38 filesystem
capture, with explicit kernel selection and automatic-return policy retained.
All 66 sequential validation steps pass using Ubuntu QEMU 10.2.1: the complete
Actions baseline (host/docs/ACPI audit, BIOS/UEFI builds, 256 restart rounds,
481 shell commands, input/display and ACPI/battery fault scenarios), two-ECM
transport regression, mixed and active-USB desktop networking, CPU-load refusal,
and exact GPT/scaled/audio desktop plus v38 refusal rehearsals. Three additional
isolated-filesystem workload/automatic-deadline/active-deadline cases pass,
including verification that every outside byte is unchanged. The no-input
reconnect screenshot visibly shows the active adapter Connected / 1000 Mb/s.

Exact desktop release `d3e5117729c46e35` and filesystem diagnostic release
`49a0b0eef761b38b` save evidence and unmount cleanly in VM. These are not installed
or physically accepted. An initial run with the optional older bundled QEMU
stalled at Limine before kernel entry; its failed log is retained. System-QEMU
validation passes without changing test predicates. An intermediate test run
was intentionally stopped when PR #12 landed, then restarted on the final tree.
Actions remain disabled; this result is local evidence, not a GitHub CI pass.
The user authorizes merging PR #5 after these checks. Durable integration logs
and receipts are archived outside `build`. Installed desktop/recovery remain
`81e3308ffcdfe637` / `e47dca3a6b30c6c1`; no reboot or installation is armed.
Battery, headphone and further trackpad-latency acceptance remain deferred.

Post-merge physical regression scope: user requests pull/update and the next
native test. Main is now `cd732ec` (PR #13 bounded USB data batching). Fast-forward
this device branch, rebuild and run core/storage hardening, USB transport and
exact GPT/scaled/audio/network desktop checks before installation. Shared source
is unchanged by this device step. Use the existing untimed Terminal cable trial
and persistent Help checklist for input/apps, brightness, speakers, both fans,
Pause/Resume and Text save/reopen. Preserve current accepted network payload and
previous recovery independently; retain Ubuntu default. Battery/headphones and
further pointer-latency tuning remain deferred. Collect this exact new session
before another stage; local/VM passes do not accept the updated native tree.

Updated native candidate `2f8abe0e131023d8` (source `fd3ffd2`, main `cd732ec`)
passes local build/core and storage hardening: all 169 write/power-cut/partial
boundaries, 72 flush failures and 19 pause/revoke points. USB read/write/flush,
reload, keyboard and timeout-quarantine VMs pass, as does mixed-adapter live UI.
The exact GPT desktop rehearsal passes 2880x1800 at 2x, audio, prompted network
checks, Text/screenshot/log persistence and clean USB unmount. The physical
workspace contains `regression-checklist.txt` with all checks and return steps.
Install with previous recovery retained, save the on-disk run record, and select
one boot. Native acceptance is pending; collect this release first after return.

Collected post-merge release `2f8abe0e131023d8`, source `fd3ffd2` / main
`cd732ec`, session `00065e5123d266b4-00`. User reports everything appeared to
work as expected. Fresh before/after reports pass DHCP/DNS/HTTP 200; disconnected
DHCP fails as expected. Two cable cycles complete without quarantine, with the
final System down/up snapshots within 0.71 seconds of driver notifications.
Normal desktop exit and zero collector attention; both fans provide recent
readings with zero service failures. The logs record no audio playback and the
requested `postmerge-text-check.txt` is absent. The user subsequently confirms
only network/UI checks were performed. Brightness, sound, Text save/reopen and
the remaining desktop checklist were not retested on this payload; prior native
acceptance remains historical. Fan samples establish telemetry availability,
not a user-confirmed fan-display or Pause/Resume retest.

Matching logs and all five reports are hashed and archived with this release
outside build. Offline exFAT/allocation checks pass: 28 directories, 109 files,
412 clusters, zero leaks/mismatches/errors. Current and recovery manifests verify;
Ubuntu remains default, one-shot consumed, no reboot armed. Network/UI recovery
and the exercised persistence path are accepted on the updated tree. Battery,
headphones and further trackpad-latency work remain deferred.

USB Wi-Fi groundwork: captured Edimax `7392:a711` revision `0100`, high-speed,
one ff/02/ff interface with two bulk IN and six bulk OUT endpoints. A read-only
inventory utility and sanitized descriptor fixture validate this shape and
reject truncations/identity/topology mutations. Linux binds `mt76x0u`. No reset,
unbind, scan, firmware upload or SrvrOS reboot was performed. See
[USB Wi-Fi scope](../usb-wifi.md) for staged driver/authentication/UI work.
This phase touches inventory/tests/docs only; Air SCI work remains separate.

Next bounded implementation on main base `6c6bf9e`, draft PR #19: add a pure
MT76 identity-request/response contract and firmware layout validator, with
host fault tests. Touch only the new protocol module, tests, Make target and
Wi-Fi docs. Existing xHCI control completion lacks exact received lengths;
native wiring is deferred until short replies and timeout DMA ownership are
handled explicitly. No radio writes, firmware upload, IP provider or reboot.

Protocol checkpoint: identity request generation and exact-length response
validation plus firmware structural parsing are implemented and host-tested.
`make test-mt76-probe`, ASan/UBSan contract tests, `make -j2 all test-xhci
test-usb-ecm` and `make check-docs` pass. The module has no native caller; kernel
build success is not hardware acceptance. No firmware is packaged and no Wi-Fi
connectivity is claimed. Next: exact-byte EP0 completion and timeout ownership
fixtures before wiring the opt-in native diagnostic. Installed payload/recovery
remain unchanged, Linux still owns the adapter, and no reboot is armed.

Next bounded checkpoint: consumed main `7761200` (Air SCI evidence only).
Implement an opt-in `srvros.usb-wifi=probe` identity diagnostic for the captured
Edimax profile. Shared scope is an isolated strict EP0 IN completion path in
xHCI, new probe wiring in main, and host transport fixtures. Preserve existing
control callers. Track short data events through status completion, reject malformed residue,
and retain/quarantine the slot's EP0 ring and buffer on uncertain completion.
No firmware upload, transmitter, networking provider or physical boot is armed.

Opt-in identity implementation checkpoint: `srvros.usb-wifi=probe` now gates on
captured Edimax identity/revision/high-speed/EP0/configuration shape and performs
only ASIC/MAC vendor reads. Strict EP0 completion retains short-data length until
successful status; errors/timeouts quarantine the endpoint with DMA retained.
The diagnostic records raw identity or failure, never an active network provider.
Native silicon values and physical acceptance remain pending. The next step is
an immutable automatic-capture profile plus exact-stage VM rehearsal before any
installation. Current/recovery payloads remain unchanged; no reboot is armed.

Validation: full `make -j2 all usb-image test-core` passes; final guard/refusal
additions pass `make -j2 all test-xhci`. Host fixtures cover lengths zero through
four, malformed residue, setup/data/status errors, missing status, stale events,
duplicate short events, ring wrap, retained DMA and profile/opt-in refusals.
`python3 tools/usb_ecm_smoke.py --second-usb --wifi-probe` passes DHCP/DNS/HTTP,
three outage/recovery cycles, 160 UDP echoes, ring wraps, keyboard and disconnect
quarantine. `python3 tools/usb_storage_smoke.py` passes read/write/flush/reload and
keyboard; docs checks pass. An earlier local VM wrapper accidentally emitted two
command lines and failed to enable Ethernet; retained as harness failure evidence,
then corrected to the explicit `--wifi-probe` option with a single command line.

Authorized next run: prepare new immutable v39 Wi-Fi identity auto-capture,
using main base `7761200`. Scope: staging/profile tests, the capture branch and
exact-stage VM refusal/persistence checks. Air PR #21 is separate SCI observation;
its changes are not consumed. Capture reports failure if no matching radio or
identity read fails, saves diagnostics, and returns on handled failure. No manual
commands, radio writes or authentication. Preserve Ubuntu and accepted recovery;
verify actual USB UUID and both filesystem preflights before one-shot reboot.

Wi-Fi capture release `5d65ddcc4ccbeaf2`, source `b1aa1ca`, is installed for
the user-authorized automatic boot. New immutable v39 enables identity reads and
capture/return only. All 43 installer/profile tests and build/xHCI tests pass.
Exact-stage GPT rehearsal saved unavailable-radio/`completed=NO` evidence,
verified offline log bytes and clean exFAT, and reset automatically. The separate
missing-workspace rehearsal also returned. These are expected VM refusal paths,
not radio acceptance. Native results remain pending collection after this boot.

Preboot offline fsck and allocation checks pass. Current capture and accepted
previous recovery are verified; Ubuntu remains default. One obsolete test payload
was removed from ESP only after every file matched its durable archive. Payload,
VM receipts, prior-session list, authorization and run/checkpoint records are
archived outside build. The capture requires no keyboard input. After return,
collect the new matching session before further work, inspect raw identity and
quarantine/result markers, verify the consumed one-shot, and repeat offline
filesystem checks. Battery and radio initialization/authentication remain off.


Collected native Wi-Fi identity session `00065e52c454a4dd-00` from release
`5d65ddcc4ccbeaf2` / source `b1aa1ca`. ASIC_VERSION is `0x76100002`; MAC_CSR0 is
`0x76502000`. Both reads succeed with `quarantined=0`, and the matching saved exit
snapshot records identity read and capture completed=yes. Collector attention=0.
The two register reads are physically accepted for this adapter; no firmware,
radio, authentication or connectivity acceptance is implied. Updated host probe
fixture values reproduce the observed identity pair with distinct endian checks.

Boot/exit bytes and collection hashes are archived outside build. Ubuntu default,
consumed one-shot and current/previous payload hashes verify. Postboot fsck and
allocation checks pass: 29 directories, 111 files, 415 clusters, no leaks/mismatches
or errors. Capture is collected; no reboot is armed. Next: consume current main
before firmware work, preserve immutable v39, then establish firmware provenance,
hash and revision/calibration gates. Fetched main `8211ec1` includes Air v40/v41
capture/collection and shared process/UI changes; it is not present in this
physically tested payload. Preserve both Air recipes when resolving shared files.

Next scope: integrate main `8211ec1`, preserving Pro Wi-Fi v39 and Air v40/v41
exact recipes and refusal tests. Resolve shared staging/receipt/Make targets by
combining behaviors, then build/core and exact v39 VM checks. Independently audit
installed mt7610 firmware structure/hash/provenance and document calibration
requirements; no firmware upload or radio writes in this checkpoint.

Integration/audit checkpoint: merge `f1421f4` consumes main `8211ec1` and preserves
all laptop recipes byte-for-byte against both parents, including Pro v39 and Air
v40/v41. All 45 profile tests, full build/USB image/core suite and exact v39 GPT
VM rehearsal pass. VM-only stage `de4ec051e0ee2f06` records dirty=true because the
untracked host firmware-audit tools were being authored; it is not installed or
promoted as a clean native candidate. Saved refusal evidence and reset pass;
installed native identity release remains `5d65ddcc4ccbeaf2`.

Both local decompressed mt7610e/u images match upstream commit `431e7f03` hashes
and pass the actual kernel C layout parser and new host audit. The audit rejects
unknown hashes/names and malformed sizes; four tests plus docs checks pass.
Upstream WHENCE assigns LICENCE.mediatek. No firmware bytes are committed or
packaged. See the Wi-Fi guide for hashes, lengths, source links and explicit
calibration requirements. Next is a separately bounded EEPROM/EFUSE capture and
MCU/reset/firmware ownership implementation; accepted identities do not authorize
transmit without this adapter's validated calibration. No reboot is armed.

Next bounded implementation at base `8211ec1`: a transport-independent EFUSE
capture engine restricted to logical-read mode on the captured ASIC/MAC pair,
plus firmware segment planning. Host fixtures must cover failed operations,
busy/timeout/cancellation, erased blocks and clearing partial output. Do not
wire control-register writes into ordinary enumeration; native capture needs
separate opt-in transport, staging and exact-image rehearsal. Firmware planning
is not upload permission or radio initialization. Air's active SCI/filesystem
work is separate; main has not advanced since the previous integration.

EFUSE/planning checkpoint: added the transport-independent logical-read engine
for the accepted identity pair and a pure ILM/DLM/IVB firmware chunk planner.
Capture has a shared poll budget, read-mode-only control writes, unmapped-block
tracking and all-or-nothing private output. Host fixtures cover every operation
failure, cancellation, busy/empty/unmapped data and cross-block timeout budgets.
Firmware tests cover both pinned candidates' ILM lengths, alignment tails,
address overlap, invalid lengths and end-of-plan refusal. Build, xHCI/probe host
regressions, new engine tests and ASan/UBSan pass. Two early synthetic fixture
bugs (byte carry and stale AOUT state) were corrected; no hardware was involved.
There is no native caller, register-write transport or active upload yet.
Next is strict OUT transport/ownership and an opt-in calibration capture profile;
retain v39 unchanged. Installed release/recovery and reboot state are unchanged.

Next native calibration scope: add strict four-byte OUT completion and a guarded
EFUSE-only register transport. Run capture only after workspace logging starts,
never in ordinary enumeration. Use new v43 automatic profile; Air's active
branch already reserves v42 for battery load. Preserve v39 and all Air recipes.
Host tests must cover write rejection, short/error/timeout quarantine and no
retry. Log only MAC-redacted calibration bytes. No firmware upload or transmitter.

Calibration transport checkpoint: `4f6bf1b` adds strict OUT completion, guarded
logical EFUSE transport and opt-in v43. Shared event dispatch retains strict EP0
completion for its owner when HID/storage workers consume the event. Short/error
or uncertain completion quarantines the endpoint and retains DMA. Capture starts
only after workspace logging; successful logs redact the MAC, and failures emit
no partial calibration bytes. No firmware upload or transmitter is enabled.

Merge `ac6b9a4` consumes main `421a69b`, preserving Air v42 and Pro v39/v43;
all existing laptop recipes match both parents byte-for-byte. Full build/USB/core,
47 profile tests and docs checks pass. Clean stage `47997cb8604043ff` passes exact
GPT VM refusal/persistence/unmount/reset and missing-workspace automatic return.
Native calibration remains pending; the earlier identity acceptance is unchanged.

USB Ethernet VM regressions pass (DHCP/DNS/HTTP, three outage/recovery cycles,
160 echoes and keyboard), as do USB storage read/write/flush/reload regressions.
The clean v43 candidate is installed for the automated native trial. Ubuntu
remains default and the accepted previous recovery payload is retained. An older
network payload was removed from the EFI partition only after verifying every
file against its durable archive; accepted identity payload remains installed.
Preboot workspace fsck/allocation checks pass: 111 files, 415 clusters, zero leaks,
mismatches or errors. Local run record and archived payload/evidence are saved;
next is one-shot calibration capture and matching-session collection on return.


Native calibration collection: release `47997cb8604043ff`, source `ac6b9a4`,
session `00065e5e35f79801-00` saved all 32 logical EFUSE blocks (512 bytes),
result=0, quarantined=0, unmapped mask `cf899c42`. All unmapped blocks contain
FF bytes; MAC bytes 4–9 are zeroed. The matching exit log records both capture
and overall completed=yes; collector attention=0. Raw evidence and a reconstructed
MAC-redacted image are archived outside build. This accepts logical readout only;
board/version/regulatory calibration validation, firmware and connectivity remain
pending. Postboot fsck/allocation pass: 30 directories, 113 files, 418 clusters,
zero leaks/mismatches/errors. Ubuntu default, consumed one-shot and installed/
recovery manifests verify; no reboot is armed. Next: validate the captured layout
and calibration requirements before bounded firmware upload/readiness work.

Next scope at main `21a9fdf` (documentation-only update, merged locally): audit
captured EEPROM structure and mapped fields without claiming regulatory/radio
acceptance; add pure firmware packet encoding and a bounded, one-shot upload
engine behind an explicitly prepared/authorized transport contract. Touch only
mt76 bring-up helpers, host tests/audit, Makefile and Wi-Fi/unit docs. No native
reset/upload wiring, firmware packaging, boot recipe changes or Air subsystem
edits in this checkpoint. Preserve v43 physical read acceptance independently.

Audit/prepared-upload checkpoint: actual captured calibration passes the new
offline structural auditor (7610, version2/FAE1, one TX/RX chain, 15 consistently
erased unmapped blocks). Region bytes 10/30 remain raw metadata; no channels or
transmit permission inferred. Physical usage-map and power/gain/regulatory checks
remain open. The new packet encoder and one-shot upload engine are host-tested
only, requiring externally verified firmware and an exclusively prepared DMA/MCU
transport. No native reset/upload caller or packaged firmware was added. Next is
hardware preparation/ownership and pinned firmware packaging before a separate
native MCU-readiness diagnostic. Installed v43 and recovery remain unchanged;
no reboot is armed.

Next bounded scope at unchanged main `21a9fdf`: strict zero-data EP0 completion
for MCU reset/FCE setup, one-shot MCU/FCE preparation with fault injection, and
an explicit pinned firmware/license package for future opt-in integration. Touch
xHCI strict transport, mt76 helpers/tests, host package tool and unit/Wi-Fi docs.
Ordinary enumeration and immutable boot profiles remain unchanged until complete
native ownership and exact-stage rehearsal are available. No new physical run.

Preparation checkpoint: strict zero-data xHCI control completion and one-shot
MCU/FCE preparation are implemented with host fault tests. Inspection found that
upstream's MAC control 0x2c enables TX/RX; the proposed readiness preparation uses
0x20 with readback checks instead. Native compatibility remains unknown and no
hardware writes were issued. A hash-pinned mt7610e/license package was produced
locally; it is not in the boot image. Next: native endpoint/DMA ownership and
kernel-side verified package consumption, then an immutable readiness profile
and exact-image rehearsal. Installed calibration payload/recovery unchanged;
no reboot armed. No firmware or radio acceptance is claimed.

Integration at main `84cf41e`: Air PR #33 independently used v43 for battery app
files, colliding with Pro's published/native calibration v43. Preserve verification
of both exact historical recipes (config copies must agree); future calibration
stages use v44, while main's battery-app-files stays v43. Historical bytes from
both merge parents and installed-stage v43 verify. No host UUID is used to guess
a recipe. New scope: kernel-side pinned firmware hash verification and dedicated
native firmware EP08 DMA/control ownership; no shared battery implementation.

Native firmware implementation now connects pinned SHA-256 verification,
dedicated EP08 DMA, strict configure completion and guarded vendor callbacks to
the prepared/upload engines. It runs only in a new descriptive wifi-fw-v1 capture
with the explicitly staged firmware/license package. Runtime bulk waits share
HID/storage dispatch and quarantine uncertain completion. MAC control stays 0x20
in preparation and must read back 0x20 after MCU readiness; live-MCU preparation
is refused. Native firmware acceptance remains pending exact-stage rehearsal
and physical capture. No RF setup or network provider is enabled.

Firmware readiness candidate: clean source `fdd8d02`, release `6ec5ab2ce67afaee`,
profile `ram-usb-wifi-fw-v1`. Full build/USB/core suite and 50 boot-profile tests
pass, including both historical v43 recipes and mismatched-copy refusal. Native
transport fixtures cover EP08 context/aligned retained DMA, exact command pointer,
control write/reset gates, FCE halfwords, bulk residue/Event-Data/timeout quarantine.
Kernel SHA-256 vectors, actual pinned firmware mutations and actual staged
initramfs lookup/hash checks pass, including ASan/UBSan. Exact-stage GPT VM saves
the expected absent-adapter refusal, unmounts and resets; missing-workspace return
also passes. Native firmware startup remains untested. Package and test evidence
are archived outside build; existing physical v43 calibration acceptance remains
historical, not firmware acceptance.

Firmware candidate installation: Ethernet recovery and USB storage read/write/flush/
reload VMs pass. Release `6ec5ab2ce67afaee` is installed for an unattended native
readiness capture; Ubuntu remains default and recovery `e47dca3a6b30c6c1` is retained.
Preboot offline filesystem/allocation checks pass. Prior identity payload was
removed from EFI only after verifying its durable archive; accepted calibration
payload remains retained. Run record, exact VM receipt, payload and logs are saved
outside build. Next is the authorized one-shot capture, then matching-session
collection before any further staging. No physical firmware acceptance yet.

Native firmware trial `6ec5ab2ce67afaee`, session `00065e5f8a2199cf-00`,
returned with preparation refused (result=1), zero chunks, mac_disabled=0 and
quarantined=1; overall completed=NO. This is a failed readiness trial, not
firmware acceptance. Logs archived; Ubuntu default and consumed one-shot verified.
Offline checks pass: 115 files, 421 clusters, zero leaks/mismatches/errors.
Next bounded scope: cache and report the last prerequisite register read and
preparation/reset/upload progress in the explicit diagnostic, without additional
register operations or relaxing refusal gates. Main remains `84cf41e`; Air PR #34
owns separate desktop battery controls. No battery changes are needed here.

Cached progress diagnostic source `11da4e0`, release `56b0d84701bc99ff`, passes
build, xHCI transport, MCU preparation/upload and documentation checks. Exact GPT
VM saves diagnostics, verifies offline bytes and unmounts/resets. Candidate is
installed with Ubuntu default and previous recovery retained; preboot filesystem/
allocation checks pass. Calibration payload was removed from EFI only after
verifying its durable archive. Next authorized boot needs no input and will
record the refusal prerequisite; physical firmware startup remains unaccepted.

Native cached-progress trial `56b0d84701bc99ff`, session `00065e5fc1be293b-00`,
identifies a live MCU: first/only read 0x730 returns 1, prepared=0, reset=0,
upload=0. The explicit live-MCU refusal fires before firmware preparation writes.
Completed=NO remains a failed readiness trial; no upload accepted. Logs archived,
Ubuntu/consumed one-shot/recovery verified, offline checks pass (117 files, 424
clusters, zero leaks/mismatches/errors). Ubuntu currently uses this adapter for
its default network route, so unloading it would interrupt the agent connection.
Next: reuse the exact tested image after an attended cold adapter power-cycle
while Ubuntu is shut down. The local cold-run record/shutdown helper are prepared
but unarmed; wait for user readiness, arm one-shot and shut down, then have the
user unplug/reconnect only Wi-Fi while off and power on. No Linux driver blacklist
or firmware takeover workaround is installed.

The subsequent attended cold-start attempt returned to Ubuntu with the one-shot
entry consumed but no new USB session (latest remains `00065e5fc1be293b-00`,
the prior warm refusal). This is incomplete evidence, not upload acceptance.
Collection is archived separately; installed/recovery manifests and Ubuntu default
verify. Offline checks remain clean: 117 files, 424 clusters, zero allocation
errors. No next boot is armed. Await the user's description of the boot screen
and return path before deciding whether startup or workspace logging failed.

Attended cold retry saved session `00065e606b547868-00` for `56b0d84701bc99ff`.
The user saw SrvrOS console briefly and automatic Ubuntu return. Root port 1/slot 1
timed out on EP0 at device-descriptor-prefix, before VID/PID or Wi-Fi identity
detection; no firmware upload was attempted. Unlike the prior missing-session
attempt, this run saved a matching exit log with completed=NO. Workspace storage
initialized on SuperSpeed port 17 and remained healthy. Evidence archived;
Ubuntu/recovery/consumed one-shot verified. Offline checks pass: 119 files, 427
clusters, zero leaks/mismatches/errors. Next scope is cold-start USB enumeration
and EP0 timing/completion diagnosis; do not relax firmware ownership gates or
retry uncertain DMA blindly. No next boot is armed.

Cold USB scope: diagnose post-address descriptor timing in xHCI and add host
regressions without firmware gate changes. Fetched main `49ded19` (Air battery
controls); integration overlaps staging and reuses v44, so it is deferred while
isolating this USB change on the prior `84cf41e` baseline. The attempted merge
was aborted without changing either branch. Reconcile immutable profiles before
consuming that baseline; no Air EC or battery changes are part of this fix.

Address-recovery candidate `296927c`, release `205e2cc2fb7e616c`, adds the
10 ms controller-clock settling interval after Address Device, before descriptors.
Build/xHCI/docs pass; the new test covers clock wrap, stall quarantine, failed
address command and retained DMA. Exact GPT VM capture/save/unmount/reset passes,
as do USB Ethernet DHCP/DNS/HTTP/recovery and storage read/write/flush/reload VMs.
This is a plausible cold-start fix, not native acceptance. Main `49ded19` remains
unmerged pending resolution of the v44 staging collision; prior baseline retained.

Native address-recovery trial `205e2cc2fb7e616c`: the user observed brief console
output and automatic Ubuntu return, but collection found no new session (latest
remains `00065e606b547868-00` from the prior release). This is incomplete evidence;
the timing fix is not physically accepted and the failure location is unknown.
Installed/recovery manifests, Ubuntu default and consumed one-shot verify. Offline
filesystem/allocation checks pass: 119 files, 427 clusters, no errors. Evidence
and checkpoint archived. Next: prepare a visible failure-screen diagnostic so
missing-workspace/early failures can be photographed; no further boot is armed.

Next bounded scope: allow the existing firmware profile without auto-return via
explicit hold-on-failure, preserving all prior recipes. Add a visible prerequisite
summary and power-button recovery instructions after failed capture/save/unmount.
Extend exact-image VM checks to require a live halted guest both with and without
the workspace. No USB hardware or firmware gates change in this diagnostic.

Held diagnostic source `835f2e9`, release `d18b0b2574ed8501`, passes build,
50 profile tests and docs. Exact GPT VM saves/unmounts and remains halted on
absent-radio failure; missing-workspace VM also remains halted with logging=0
and manual recovery instructions. Installed with Ubuntu default, known-good
previous recovery and preboot filesystem/allocation checks verified. Prior
`56b0d84701bc99ff` was removed from EFI only after durable archive verification.
Run record/evidence are archived; no reboot armed. Await attended cold-start
readiness; on failure photograph the screen before manual power-button recovery.

Held native trial `d18b0b2574ed8501`, session `00065e637e1d2a93-00`,
successfully enumerated the adapter, read expected ASIC/MAC identity and configured
the firmware endpoint. Preparation made four reads, last 0x238=0x00c00000,
then failed with result=2 (I/O), reset_seen=1, prepared=0, upload=0, chunks=0,
quarantined=1. reset_seen means attempted, not completed: the reset request or
subsequent setup writes still need exact operation diagnostics. No firmware
upload accepted. The photographed held screen shows saved/unmounted success and
manual recovery instructions. Photo/logs archived. Offline checks pass (121 files,
430 clusters, zero errors); Ubuntu/recovery/consumed one-shot verified. This run
passes cold enumeration but does not establish repeatability or causation for the
10 ms delay. Next: instrument preparation control request/completion failures;
no further boot armed.

Next scope: cache the exact firmware control setup packet, actual result, matched
completion code/residue/stage and timeout, reporting after the attempt. Preserve
all transfer guards, sequencing and timeout behavior; add fault regressions before
staging another held diagnostic. Shared integration remains deferred as recorded.

Control-detail candidate `ab82b8f`, release `dfd7ab3b39432e73`, passes build
and xHCI regressions including reset setup/status errors, residue, timeout, stale
completion followed by success, no retry and retained DMA. Exact GPT VM saves
logs/unmounts and stays halted; missing-workspace also stays halted. Control
sequence, deadlines and refusal rules are unchanged. Native result pending.

Native control-detail release `dfd7ab3b39432e73`, session
`00065e639cbb404d-00`, locates the failure at control request 9: type=0x40,
request=6, value=0, index=0x800, length=4 (FCE enable write). actual=-1,
cc=0, stage=0, timeout=1: no matched completion. The prior reset completed and
its delay returned successfully, since the sequence reached this subsequent
write. Firmware upload remains unattempted (zero chunks); quarantine retained.
Photo/logs archived; Ubuntu/recovery/consumed one-shot verified. Offline checks
pass: 123 files, 433 clusters, zero errors. Next investigate reset recovery timing
and EP0 state before the first FCE write, without blind retries or weakening
completion checks. No next boot armed.

Next scope: firmware-only EP0 waits use a measured 300 ms deadline (the pinned
Linux mt76 vendor timeout), retaining a bounded stalled-clock fallback and no
retry. Existing generic control waits and reset delay remain unchanged. Cache
request elapsed time and controller-owned EP0 state/dequeue after the wait to
diagnose post-reset timeout; no endpoint recovery or new device writes.

Deadline candidate `49375c8`, release `20ffa06db0f23758`, passes build/xHCI
regressions, including delayed 250 ms completion, 300 ms timeout, frozen clock,
endpoint snapshot and no mutation/retry after uncertainty. Exact GPT saved-log
and missing-workspace held VMs pass. This measures the timeout and endpoint
state; it does not yet establish why the device stops responding after reset.
Native validation pending; recovery and the held-failure workflow are retained.

Native measured-deadline trial `20ffa06db0f23758`, session
`00065e63bd3755b9-00`, still times out on request9/write6/index0x800/length4
with no matched completion. elapsed_us=300001, endpoint state=1, cached output
dequeue=0x1eb001. Do not infer live ring progress solely from that context snapshot.
Reset completed, preparation failed and upload remained unattempted; no firmware
acceptance. Photo/logs archived, recovery/default/consumed one-shot verified.
Offline checks pass: 125 files, 436 clusters, no errors. Next investigate device
reset behavior and transport state rather than simply increasing the timeout.
No further boot armed.

Next bounded scope: add the missing MT7610 WLAN/clock enable prerequisite before
MCU reset, based on Linux v6.18 mt76x0_chip_onoff/set_wlan_state. Preserve GPIO
and antenna bits, refuse asserted reset/unreadable state, verify enable readback
and bounded crystal/PLL readiness. Keep MAC TX/RX disabled and existing quarantine.
Files: mt76 preparation engine, native xHCI allowlist/diagnostics, host fixtures.
Fetched main 065546f (Air battery audit); no open competing PR besides #19.
Integration remains deferred for the recorded immutable-profile collision.
Native evidence is required; no further boot armed.

Clock prerequisite candidate `7f06de6`, release `05573bcc8bb336a4`, passes build,
xHCI/preparation/upload/firmware host tests and docs. Fixtures cover preserved
GPIO bits, reset/unreadable/readback refusal, readiness timeout/cancellation and
every-operation failure without retry. Exact GPT VM saves/unmounts and holds;
missing-workspace VM holds with recovery instructions. Installed unarmed, Ubuntu
default and accepted recovery retained; offline workspace checks clean. Native
WLAN/clock and firmware readiness remain pending. Next: attended cold shutdown,
then collect this release before further changes.

Native clock trial `05573bcc8bb336a4`, session `00065e643293f201-00`, confirms
WLAN enable/readback (`0xff000002` → `0xff000003`) and crystal/PLL readiness
(CMB `0x00c017ff`). It still fails request13/write6/index0x800/length4, the
first FCE write after completed MCU reset: no matched completion in 300003 µs,
endpoint state=1, cached dequeue=0x1eb001. Preparation/upload remain incomplete,
zero chunks and quarantine retained. Clock initialization is observed working;
it did not resolve the post-reset failure. Photo/logs archived; Ubuntu default,
consumed one-shot and recovery verified. Offline checks: 127 files, 439 clusters,
zero leaks/mismatches/errors. Next investigate post-reset EP0/device behavior;
no next boot armed and no firmware-readiness acceptance.

Next scope: post-timeout Wi-Fi EP0 stop-and-snapshot diagnostic in xHCI, with
exact command completion, cached submitted TRB addresses and stopped transfer
event capture. Keep the endpoint quarantined, retain DMA, and never restart or
retry the request. Host fault fixtures and exact-stage held VMs precede native
test. Fetched main 961369b; only open PR remains #19. Prior integration/profile
collision remains deferred; this does not change Air/battery or firmware gates.

Stop-snapshot source `999cb37`, release `f617f5b921c5a71d`, passes build/xHCI
host tests and docs. Fault cases cover stale command completion, wrong slot,
command error/timeout, ineligible requests, one-shot execution and retained DMA.
Exact GPT VM saves/unmounts and holds; missing-workspace VM holds with recovery
instructions. Installed unarmed with Ubuntu default and accepted recovery intact.
Offline checks remain clean (127 files, 439 clusters). Native result pending;
this diagnostic does not claim a fix for the post-reset failure.

Native stop-snapshot trial `f617f5b921c5a71d`, session
`00065e64dd39e82b-00`, again times out at request13/FCE write0x800 after
300003 µs. Stop Endpoint completed successfully: state=3, dequeue=0x1eb381,
stopped transfer event cc26 at 0x1eb380 with residue4. Submitted setup/data/status
addresses were 0x1eb370/0x1eb380/0x1eb390. This locates the outstanding transfer
at the OUT data stage with its full four-byte payload remaining; it does not
establish the cause or prove a particular USB handshake on the wire. Clock
readiness passed; preparation/upload failed, zero chunks, quarantine retained.
Photo/logs archived, consumed one-shot/Ubuntu default/recovery verified. Offline
filesystem/allocation checks pass. Next compare post-reset OUT handling and
adapter initialization requirements; no next boot armed.

Next scope: separate opt-in wifi-fw-v2 MAC-startup trial. Linux v6.18 writes
MAC control 0x2c before MCU reset; v1 deliberately used 0x20. v2 tests that
startup prerequisite, restores/verifies 0x20 after FCE setup before upload, and
retains exact device/firmware gates and quarantine. No channel/RF configuration
or network frames. Files: preparation/transport, main opt-in, staging/VM recipes
and host tests. Fetched main e946f5e; only PR #19 open, Air scope battery query
receipt work. Integration profile collision remains deferred. Native proof is
required; MAC startup may remain enabled on failed I/O until physical power-off.

MAC-startup comparison source `75ea093`, v2 release `c18a4380b3f08235`,
passes build, preparation/xHCI fault fixtures, 51 staging tests and docs. The
fixtures preserve v1, require explicit one-shot startup ownership, verify final
disable before preparation succeeds, and fail closed on each operation. Exact
GPT saved-log and missing-workspace held VMs pass. Installed unarmed; Ubuntu
default/recovery verified; offline checks clean (129 files, 442 clusters).
Native result pending. Next attended cold trial compares the post-reset FCE
write with the upstream MAC startup value; no firmware readiness accepted.

Native MAC-startup comparison `c18a4380b3f08235`, session
`00065e653266c290-00`, confirms requested=1/written=1 but still times out
at request13/FCE write0x800 after 300000 µs. Stop Endpoint succeeds: state=3,
dequeue=0x1eb381, stopped event at data TRB 0x1eb380, cc26/residue4. The full
four-byte OUT payload remains outstanding, as in v1; MAC startup did not resolve
the failure. The initial MAC readback passed before reset; restoration to 0x20
was not reached. Preparation/upload remain incomplete, zero chunks, quarantine
retained; RF/connectivity not tested. This does not prove a particular USB wire
handshake or establish the cause. Matching logs/photo/run record archived in
`~/.local/share/srvros/macbookpro11-2/wifi-mac-75ea093`. Ubuntu default,
consumed one-shot and installed/current recovery payloads verified. Offline
checks pass: 131 files, 445 clusters, zero leaks/mismatches/errors. Next inspect
post-reset USB OUT transaction handling; no new boot armed or firmware readiness
accepted.

Workflow correction after the MAC-startup trial: the saved matching exit log
already includes control timeout, submitted TRBs, Stop Endpoint result, startup
flags and failure markers. Photos are redundant for this handled failure. Future
Wi-Fi trials use the existing default `-auto-return` recipe (omit
`--hold-on-failure`), save/unmount and return to Ubuntu automatically. Collect
and inspect the matching release/session on return; missing or incomplete logs
remain incomplete evidence. No routine photo request. Cold adapter power cycling
may still require the previously announced physical action. The installed held
recipe is unchanged and unarmed; apply automatic return when staging the next
candidate and verify that exact stage in the VM before installation. This is a
workflow/documentation change only. Fetched main remains e946f5e; open Air PR44
concerns battery query refusal with parked USB, outside this workflow change.

Next bounded scope (2026-10-22): xHCI EP0 transaction publication. Current code
publishes the setup cycle before building data/status; Linux queues setup with
inverse ownership and publishes only after the whole transfer is ready. Fix this
ordering with regression coverage for both cycles and ring wrap, leaving device
initialization and retry policy unchanged. This is a correctness issue, not yet
an established cause of the native stall. Files: xhci.c and host fixtures; then
exact-stage automatic-return VM checks and a cold native trial. Fetched main
24fd5f4 (Air SMBus ownership proof); only PR19 remains open. Integration profile
collision stays deferred; no shared battery changes.

EP0 publication candidate source `2389909`, release `5edf8b73bf8f8662`,
passes build/docs and xHCI host tests. Publication checks cover both cycles,
every ring start position, IN/OUT/no-data and existing strict completion/fault
fixtures. Setup remains inverse-cycle until all stages and the exact completion
owner exist; an ordered control store then releases ownership before doorbell.
Exact GPT VM saves and verifies log bytes, unmounts and resets; missing workspace
returns without starting the probe. Installed with automatic failure return,
Ubuntu default and accepted recovery intact; offline workspace checks pass.
Native result pending: no claim this fixes the stall. Next cold adapter trial
requires unplugging Wi-Fi for 10 seconds while off, reconnecting and powering
on; leave workspace connected. Logs replace photos. Installed unarmed pending
user readiness for that physical step.

Native EP0 publication trial `5edf8b73bf8f8662`, session
`00065e71f460e55a-00`, saved a complete matching exit log without a photo.
The change did not resolve the stall: request13/write6/index0x800/length4 times
out after 300003 µs. Stop Endpoint succeeds with state3, dequeue0x1eb381,
event0x1eb380, cc26/residue4; submitted setup/data/status remain
0x1eb370/0x1eb380/0x1eb390. MAC startup and clock prerequisite passed, reset
completed, preparation/upload did not; zero chunks and quarantine retained.
Firmware readiness and RF/connectivity remain unaccepted. The saved snapshot
locates the outstanding OUT data stage but does not establish a wire handshake
or its cause. Evidence archived under
`~/.local/share/srvros/macbookpro11-2/wifi-publication-2389909`. One-shot consumed,
Ubuntu default/current/recovery payloads verified; offline filesystem/allocation
checks pass (133 files, 448 clusters, no leaks/mismatches/errors). No next boot
armed. Next compare the small OUT data-stage representation and Linux's working
control sequence; avoid treating this ordering fix as a native Wi-Fi fix.

Next scope: compare immediate-data encoding for the firmware probe's four-byte
vendor OUT writes with Linux xHCI. Restrict the change to the active configured
Wi-Fi firmware owner and its descriptor buffer; keep IN, no-data, IVB and other
devices unchanged. Preserve exact completion, publication ordering and timeout
quarantine. Add a saved encoding flag and boundary/payload host tests, then
build and verify automatic-return stage. This is not a proven native fix.
Main remains 24fd5f4; Air PR46 covers SMBus/parked storage, outside this scope.

Immediate-data candidate source `88d1cd0`, release `825373ebb599fa0d`, passes
build/docs and xHCI host tests. Tests verify exact little-endian payload/high-word
zeroing, unsupported ownership/buffer/direction/length/request refusal, and an
actual guarded register transfer with IDT set. Saved diagnostics now include
`wifi-firmware-data: immediate=...`. IN, no-data and 64-byte IVB remain buffered
or unchanged. Exact GPT VM saved-log/automatic-reset and missing-workspace return
checks pass. Installed unarmed with Ubuntu default and accepted recovery intact;
offline checks pass (133 files, 448 clusters). Next cold hardware trial awaits
readiness for adapter power cycling; no firmware-readiness acceptance yet.

Native immediate-data trial `825373ebb599fa0d`, session
`00065e7298d3f135-00`, confirms `wifi-firmware-data: immediate=1` but still
fails request13/write6/index0x800/length4 after 300002 µs. Successful Stop
Endpoint reports state3, dequeue0x1eb381, stopped event0x1eb380, cc26/residue4;
setup/data/status addresses remain 0x1eb370/0x1eb380/0x1eb390. Clock and MAC
startup passed, reset completed, preparation/upload did not; zero chunks,
quarantine retained, RF/connectivity not tested. Immediate encoding did not
resolve the native stall; this does not establish a wire handshake or cause.
Matching exit log and run record archived under
`~/.local/share/srvros/macbookpro11-2/wifi-idt-88d1cd0`. Ubuntu default, consumed
one-shot and installed/current recovery verified. Offline checks pass: 135 files,
451 clusters, no leaks/mismatches/errors. No next boot armed. Next compare the
working Linux cold initialization sequence with SrvrOS before another speculative
hardware trial; preserve the live Linux network connection during investigation.

Next scope: Linux cold-initialization evidence, not another kernel candidate.
Fetched main 4ea10e1 (Air SMBus capture); only PR19 remains open. Compare the
actual adapter's control setup/payload/status/timing via bounded usbmon capture.
Linux source agrees on reset request1/value1, 5–6 ms delay and FCE write6/0x800;
its earlier WLAN disable/enable sequence and vendor retries need native evidence.
Add a host capture tool and parser fixtures; filter to initialization registers,
exclude bulk/network payloads, preserve the current network until an announced
physical replug. No driver unbind/reset or reboot as part of preparation.

Source review confirms the captured v6.18 Linux path first calls
`mt76x0_chip_onoff(false,false)`, waits for MAC, then enables WLAN during hardware
initialization. Its register transport allows up to ten attempts, with 5–10 ms
between failed 300 ms requests. SrvrOS intentionally quarantines after one
uncertain request. Neither difference is yet proven causal; do not add blind
retries or GPIO/RF writes. The new `tools/mt76_linux_capture.py` records only
selected initialization-register control URBs and reset, correlates completion
status/timing, and persists only addresses verified on this adapter's port.
Bulk/network payloads are excluded. Four parser fixtures and a 30-second passive
Ubuntu smoke run passed; no device reset or network disruption was performed.
A user-operated 10-second unplug/replug in Ubuntu is next; capture survives the
brief network loss locally. This observes host requests, not physical USB wire
handshakes. No new SrvrOS boot is staged or armed.

Linux cold-replug capture completed on kernel 7.0.0-38-generic, port1-1,
address2→8, with 53 selected requests. The captured first FCE write6/0x800/value1
has callback status -2, actual0, duration307755 µs; the identical next submission
is 10013 µs after that callback and completes status0/actual4 in95 µs. Later the
first MCU-ready read7/0x730 has status-2/actual0 after306038 µs; the next read,
10027 µs later, returns value1 successfully in71 µs. Status-2 is a cancelled URB,
consistent with Linux's 300 ms timeout/unlink path, not a wire-level error code.
This strongly motivates bounded timeout recovery/retry rather than another
payload-encoding change. A retry must first retire the old EP0 transaction,
account for late events and retain DMA until ownership is proven; current SrvrOS
quarantine must not simply be cleared. Both FCE setup and MCU-ready polling need
consideration. No SrvrOS fix or new boot is accepted/armed by this observation.

Capture limitations: some earlier setup records and a later completion are
missing; an implausible WLAN read payload exposed a parser correlation weakness
when Linux reuses an URB tag for an unselected submission. The original capture
is preserved, not silently corrected. The tool now invalidates such stale owners
and marks reuse, excludes transient address0, and passes five parser fixtures.
The retry pattern above is evidence for the next direction, not a complete
validated trace; repeat with the corrected parser before relying on exact values
or accepting a recovery implementation. Raw capture lives in
`~/.local/share/srvros/macbookpro11-2/linux-cold-1792696794998436335`.

Corrected-parser Linux cold capture confirms the retry pattern on the same
7.0.0-38-generic host, port1-1/address8→9. The first write6/0x800/value1
has callback status-2/actual0 after303066 µs; identical resubmission follows
10028 µs later and completes status0/actual4 in112 µs. The first read7/0x730
has status-2/actual0 after305986 µs; retry after10024 µs completes in1065 µs
with value1. These four records have matched completions and no tag-reuse
ambiguity. Other early missing records are now explicitly marked instead of
misattributed, so this remains a filtered, incomplete sequence rather than full
wire evidence. The cancellation/retry observation is independently repeated.
Archive: `~/.local/share/srvros/macbookpro11-2/linux-cold-1792697340613399648`,
with source/tool digest, original capture, validated comparison and checksums.
Linux default route recovered; no SrvrOS boot armed. Next bounded scope is safe
EP0 timeout retirement and retry for these observed operations, including exact
command ownership, stopped-ring dequeue advancement, late-event exclusion and
retained DMA fault tests. Never simply clear quarantine or replay the reset.
Native firmware acceptance remains pending.

Recovery implementation scope: one retry each for post-reset FCE write0x800=1
and post-IVB ready read0x730 only. Require successful Stop Endpoint with stopped
state/data-stage evidence; switch EP0 to a fresh ring and buffer using exact
Set TR Dequeue completion. Retain old/new DMA allocations on every failure;
never reuse retired transfer addresses. Delay10 ms, retry once, quarantine on
any uncertainty. Add fault/late-event fixtures and saved recovery diagnostics.
Fetched main53a24a0; Air PR48 covers sustained SMBus/Text, outside this scope.

Bounded-retry source `122fcda`, release `b7f7fb1c357d7ff5`, passes build/docs
and xHCI host fixtures. Tests cover fresh ring/buffer and copied OUT/zeroed IN,
exact Set Dequeue command failure/wrong-slot/timeout/context refusal, allocation
and delay failure, identity/phase/stop-evidence gates, one-shot limits, and full
write/read timeout→stop→dequeue→retry paths with stale old-TD events and a second
failure. All DMA stays allocated; no stale completion can retire a fresh-address
TD. Saved recovery diagnostics include attempts/successful/mask and ring/buffer.
Exact GPT saved-log automatic return and missing-workspace return VMs pass.
Installed unarmed; Ubuntu default and accepted recovery verified; offline checks
pass (135 files, 451 clusters). Native acceptance pending. Next attended cold
shutdown requires Wi-Fi unplugged10 seconds while off, reconnected before power
on; workspace stays connected. Results save and return automatically, no photos.


Native bounded-retry trial `b7f7fb1c357d7ff5`, session
`00065e7310c09789-00`: **firmware upload and MCU readiness accepted** for the
captured Edimax7392:a711 revision0100, ASIC76100002/MAC76502000. Recovery
attempts2/successful2/mask3; preparation/reset/upload complete; result0, chunks6,
mac_disabled1, quarantined0. Final MAC0x1004 read returns0x20 with exact status
completion; firmware and laptop capture both completed=yes. This validates the
bounded recovery path on this run, not Wi-Fi RF/network connectivity. No radio,
scan, association, authentication or IP acceptance is implied.

Logs, exact payload, run record and checksums retained outside build under
`~/.local/share/srvros/macbookpro11-2/wifi-retry-122fcda`. Ubuntu default, consumed
one-shot, current payload and accepted desktop recoverye47dca3a6b30c6c1 verified.
Offline checks pass:137 files,454 clusters,zero leaks/mismatches/errors. No new
boot armed. Next phase: interpret captured calibration/regulatory data and add
bounded radio/receive initialization before scan, then authentication and IP.

Next radio-phase scope: decode receive-side calibration from the accepted logical
EEPROM capture before hardware writes. Extend the offline audit with board/band
hints, PA/LNA flags, frequency correction and receive gain/RSSI/temperature data;
track missing fields explicitly and retain radio_authorized=false. Raw region
bytes do not select a country or authorize channels. Use synthetic boundary tests
and the local redacted capture; do not publish raw calibration or MAC data.
Fetched main598fe27; only PR19 open. No shared battery changes or reboot planned
for this offline prerequisite. Native RF initialization remains a later step.

Offline receive-calibration audit completed against the existing accepted
redacted512-byte capture, SHA2563969e4b2817fd02c1d5285614abc92fd9987af6c07a2444fdf68b85460a16749.
All required receive fields are mapped. Decoded frequency68, temperature−8,
5GHz LNA12/12/12, RSSI2GHz1/1 and5GHz0/0; hardware RF control and TSSI flags
are clear. Board hint both bands is not regulatory/channel authorization.
Five audit tests cover signed correction, RSSI boundaries, gain fallback,
missing/unmapped fields and band-hint separation. No radio registers changed;
firmware readiness remains accepted, RF/scanning pending. Next: per-channel power
and regulatory constraints, then a bounded native receive initialization path.
No reboot armed for this offline work.

Next bounded scope: offline 20MHz per-channel transmit calibration and signed
per-rate offsets for the captured non-TSSI layout, preserving unmapped/invalid
entries as unknown. EEPROM values are chipset calibration units, not dBm limits.
Compare against this adapter's Linux channel flags; disabled/no-IR/DFS channels
cannot become active-scan candidates merely because calibration exists. Country
confirmation is pending; Linux's current US setting is evidence, not user locale.
Main remains598fe27; only PR19 open. No hardware writes or reboot in this step.

Offline non-TSSI 20MHz power audit completed against the accepted logical image.
Values are5 for2GHz1–14 and17–20 across5GHz36–165; these are calibration units,
not dBm. Channels169/173 are invalid and177 outside the decoded map. Signed rate
offsets decoded without promoting erased/unmapped data. Eight audit tests pass,
covering channel/byte mapping, signed endpoints, invalid/missing data and TSSI
refusal. Linux PHY/regulatory snapshots saved locally; current Linux domainUS is
not treated as user country confirmation. Country question remains pending.
No regulatory policy applied, radio writes performed or reboot armed. Next work
is a country-scoped policy and bounded native receive initialization; firmware
readiness remains accepted, radio/network functionality remains pending.

User confirmed United States testing on2026-10-22. Use explicit countryUS for
the initial channel-policy work; the earlier pending-country question is resolved.
This records location, not completed regulatory-policy integration or permission
to bypass calibration/driver gates. No radio writes or reboot armed.

Next bounded scope (2026-10-22): offline US 20MHz candidate filter in
`tools/wifi_channel_policy.py` with synthetic refusal tests. Intersect a deliberately
small development channel set with the captured Linux PHY restrictions and valid
calibration. This is not a regulatory database or native transmit authorization;
unknown flags, DFS, no-IR and missing evidence fail closed. Main598fe27 inspected;
Air PR49 concerns battery UI, separate from this tool. Native RF/power validation
remains a dependency. No installed payload changes or reboot planned.

Offline US candidate filter completed against the existing paired Linux snapshots
and accepted calibration audit:20 candidates (1–11,36/40/44/48,149/153/157/161/165).
Snapshot ceilings are2dBm on2GHz,11dBm on36–48 and12dBm on149–165, distinct from
chipset calibration units. All other decoded channels refuse; transmit permission
remains false throughout. Five policy tests cover country/width, PHY flags,
regulatory band edges/flags, calibration refusal, duplicate records and lower-cap
selection. Calibration/firmware audit regressions also pass (12 tests). No native
policy installed or reboot armed. Next: bounded native RF/receive initialization
and power programming validation before any active scan.

Next scope: native MCU command wire codec, required before radio initialization.
Add freestanding bounded packet encoding and response correlation with host tests;
no hardware commands or RF enables in this checkpoint. Linuxv6.18 mt76x02 USB
command framing is the reference. Fetch shows main3c47b50 (merged battery PR49);
inspected scope does not overlap this new driver module. Device branch still
uses598fe27 integration baseline; no blind integration of capture-profile changes.
Dependencies: exclusive bounded USB command transport, native calibration/power
and RF tables remain unimplemented. No physical run or reboot armed.

MCU prerequisite implemented: bounded freestanding packet codec and serialized
non-reusing15-command diagnostic session. No xHCI command transport connection or
radio initialization is claimed. Tests cover independently specified wire bytes,
every payload length1–192, output canaries/padding, insufficient capacity,
invalid fields, response bounds, outstanding-command exclusion, sequence
exhaustion and uncertain/mismatched completion poisoning. Host and ASan/UBSan
checks pass; US policy tests are now included in test-core. Next checkpoint is
native command transport and saved response evidence, before RF initialization.
No payload installed, USB adapter disturbed or reboot armed by this work.
Full kernel build and existing preparation/upload/bring-up/xHCI regressions pass;
documentation link checks pass. Native evidence remains firmware readiness only.

Next scope: connect a one-shot MCU read diagnostic to xHCI for the captured
Edimax descriptor. Reuse the retired firmware OUT endpoint, add a separate
command-response IN endpoint/ring/buffer, and read only MAC_CSR0 through firmware.
Require accepted firmware completion and disabled MAC; retain all DMA on failure.
Record bounded transfer status and decoded response without accepting RF/network.
Main3c47b50 inspected; Air branch5fe69fa remains battery work. No concurrent
staging or physical run. Boot-profile integration follows transport tests.

One-shot MCU xHCI transport implemented behind firmwarev3 opt-in. Fixed random
read of MAC_CSR0 only; separate IN0x85/DCI11 response ring, existing OUT8 ring,
500ms waits, exact transfer ownership, short-IN bounds and retained DMA on every
failure. Saved header/first16bytes are evidence only (validated=0); final EP0 MAC
check must remain0x20. Full build/xHCI regressions and52 staging tests pass,
including new endpoint configuration, wrong slot/DCI/pointer, event errors,
residue, allocation failure and unready-firmware refusal. No native acceptance
or radio enablement claimed. Stage/VM checks precede any physical installation.

MCU transport sourcec588228, release8e87dabf5c918f1c, staged and installed unarmed.
Exact GPT saved-log/automatic-return VM and missing-workspace return VM pass;
Ubuntu default, pinned descriptor, model, workspace0E53-5900 and retained recovery
e47dca3a6b30c6c1 verified. Offline workspace fsck/allocation checks pass. Candidate,
receipts and run record archived outside build under wifi-mcu-c588228. Accepted
firmware payloadb7f7fb1c357d7ff5 remains retained; only the archive-verified older
825373ebb599fa0d payload was removed for space. Await attended cold adapter cycle:
shutdown, unplug Wi-Fi10 seconds, reconnect then power on, leave workspace drive
connected. Logs save and return automatically; no photo or commands required.
After return collect matching session first; MCU response/RF acceptance pending.

Native MCU trial8e87dabf5c918f1c session00065e73f7f56ce9-00 collected: firmware
readiness passes again (result0,chunks6,MAC0x20,recovery2/2). MCU OUT completes16
bytes; IN times out with received=-1,cc0,decoded0,failed1. Diagnostic completed=NO;
no MCU response or RF acceptance. Final mac_disabled0 denotes an unperformed
post-timeout read after quarantine, not an observed enabled MAC. Matching logs
archived under wifi-mcu-c588228/native-collection. Ubuntu default, consumed
one-shot and current/recovery manifests verified. Offline workspace clean:
139files,457clusters,zero leaks/mismatches/errors. Reboot unarmed.

Reference follow-up: Linuxv6.18 mt76x0_init_hardware sends functionQ_SELECT=1,
value1 before ordinary MCU operations; mt76x02_mcu_function_select deliberately
uses no response for Q_SELECT. Our one-shot diagnostic omitted that routing
setup. This is a candidate explanation for a successful OUT without an IN reply,
not yet proven. Next bounded change: explicit queue-selection setup with sequence0
(no expected reply), then the existing read-only command; preserve ordinary
command sequence guards and native MAC-disable checks. No new trial installed.

Next scope: add fixed Q_SELECT(1) command with sequence0/no expected reply before
one-shot MCU random read, preserving ordinary command sequence1–15 validation.
Extend host tests for wire bytes and ordered transport failures; retain firmwarev3
capture semantics and disabled-MAC checks. Main85d5cb7 fetched; battery work remains
separate. Native evidence remains prior IN timeout; this candidate is not accepted.

Fixed no-reply Q_SELECT packet added before read; generic sequence0 remains
refused. Full build, MCU codec, xHCI and52 staging regressions pass. New host
coverage verifies exact setup bytes, capacity refusal and ordered setup/read/IN
submission stopping after failure at each step, with DMA retention. Preparing
exact-stage VM checks; native response acceptance remains pending.

Queue-selection source456922b release4a47cd57ac90c504 installed unarmed. Exact
GPT saved-log/automatic-return and missing-workspace VMs pass; offline workspace
checks and Ubuntu/current/recovery verification pass. Durable archive:
~/.local/share/srvros/macbookpro11-2/wifi-queue-456922b. Known-good desktop recovery
e47dca3a6b30c6c1 retained; prior MCU trial8e87dabf5c918f1c retained. Accepted firmware
payloadb7f7fb1c357d7ff5 removed from ESP only after exact archive verification;
its durable payload/evidence remains available. Await attended cold adapter cycle,
then collect matching session before further work. No photos or commands needed.

Native queue-selection trial4a47cd57ac90c504, source456922b, session
00065e74311ea511-00: firmware result0/chunks6; Q_SELECT OUT16; random-read OUT16;
IN16,sequence1,event0,reported-length8,cc13,residue1008. Reply words are
4a010008/00411000/76502000/4a010008: requested WLAN MAC_CSR0 address and expected
chipset value, with repeated header trailer. Final MAC-disable check passes,
failed0, capture completed=yes. This accepts the observed one-shot queue/read
exchange on this adapter, not sustained MCU service or RF/network functionality.
The native logger still reports validated=0: strict command-specific response
validation is the next implementation step, using this evidence plus negative
length/event/address/value/trailer fixtures.

Matching collection archived under wifi-queue-456922b/native-collection. Ubuntu
default, consumed one-shot and installed/current recovery manifests verified.
Offline workspace clean:141files,460clusters,zero leaks/mismatches/errors. Run
record and checkpoint collected/unarmed; no new physical trial prepared.

Next bounded scope: strict validation of the native one-shot MCU identity reply
captured in session00065e74311ea511-00. Match exact16-byte envelope,sequence1,
event0,length8,register00411000,value76502000 and repeated header trailer. This
contract is specific to the captured diagnostic, not a generic MCU response
parser. Add truncation/extension and every-bit mutation tests, then require the
validator in the diagnostic. Main85d5cb7 inspected; no battery overlap, no new
radio writes or physical boot in this validation step.

Strict one-shot reply validator implemented and required by diagnostic success.
Host replay of the archived native response passes. All128 single-bit mutations,
truncated/extended lengths and a consistent stale sequence refuse; ASan/UBSan,
full build, MCU and xHCI regressions pass. This is host validation against native
evidence; the new validator has not itself been physically booted. Installed
release4a47cd57ac90c504 remains unchanged and unarmed. Next scope is bounded native
radio initialization using per-operation completion contracts; no RF acceptance.

Next scope: bounded MAC/BBP reset preparation engine, following the captured
MT7610 identity and Linuxv6.18 initialization order. Require firmware ownership,
idle WPDMA/MAC and disabled MAC before asserting CSR/BBP reset; bound the200ms
hold and verify reset release. No RF table, channel, power or TX-enable writes.
Host fault coverage before xHCI/native wiring. Main85d5cb7 remains integration
head inspected; Air battery scope separate. Installed payload unchanged.

MAC/BBP reset prerequisite engine completed with exact identity/disabled-state
checks, bounded idle polls,200ms cancellable reset hold, release readback and
post-reset idle checks. Full build, MCU and new reset tests pass; reset tests
also pass ASan/UBSan. Every operation failure stops, every later retry refuses,
and no write sets TX/RX bits. No xHCI binding or physical execution added yet;
installed release remains4a47cd57ac90c504, reboot unarmed. Next connect this engine
to a separately guarded native capture, then verify reset/MCU survival before
loading MAC/BBP/RF tables. Calibration/power enforcement remains required for TX.

Next scope: firmwarev4 opt-in native MAC/BBP reset capture, before the existing
queue-selection/strict identity-read exchange. Bind only reset-engine register
reads and ordered MAC_SYS_CTRL writes3 then0; never widen firmware write gates.
Require exclusive prepared firmware transport; final MAC must be0 after reset.
Preserve v1/v2/v3 recipes and automatic saved-log return. Host binding/profile
checks and exact-stage VMs precede installation; cold trial requires readiness.
Main85d5cb7 remains fetched baseline; Air changes remain separate.

Reset binding build and host checks pass, including exact ordered write values,
phase/identity/ownership refusals, early-release refusal and USB failure
quarantine.53 staging tests pass and documentation links check clean. New reset
engine and strict native reply validator are included in v4; hardware acceptance
remains pending. Preparing stage/VM checks before installation.

Native reset candidate source2bc3689 releasebfa7235c397175da installed unarmed.
Exact GPT saved-log/automatic-return and missing-workspace VMs pass; workspace
fsck/allocation and Ubuntu/current/recovery verification pass. Durable payload,
receipts and run record in wifi-reset-2bc3689 outside build. Accepted queue/read
payload4a47cd57ac90c504 and known-good desktop recoverye47dca3a6b30c6c1 retained;
older failed MCU payload8e87dabf5c918f1c removed from ESP after archive equality
verification. Await attended cold cycle; collect matching session first after
return. Require reset result0/prepared1/released1 and MCU validated1, disabled MAC;
no RF or connectivity acceptance follows from this trial alone.

Native reset trialbfa7235c397175da/source2bc3689/session00065e747b16d8cb-00:
**MAC/BBP reset and strict MCU identity exchange accepted** for this adapter.
Reset result0,released1,prepared1,reads9,writes2,hold200000us; final idle MAC
status1200=0. Queue OUT16/read OUT16/IN16, expected identity response, validated1,
failed0 and final disabled MAC_SYS_CTRL=0. Firmware and laptop completed=yes.
This accepts only this reset/identity sequence, not MAC/BBP tables, RF calibration,
receive/scan, association, authentication or IP connectivity.

Matching collection archived under wifi-reset-2bc3689/native-collection. Ubuntu
default, consumed one-shot and current/recovery manifests verified. Offline
workspace clean:143files,463clusters,zero leaks/mismatches/errors. Run record and
checkpoint collected/unarmed. Next: bounded MAC/baseband table initialization
with command-specific completions, then calibrated RF/receive setup.

Next scope: bounded MCU register-pair packet encoder before MAC/baseband tables.
Linuxv6.18 tables include PA and TX-power entries; do not treat them as uniformly
receive-only. Implement at most24 aligned WLAN register pairs per command, with
nonzero response sequence and atomic input validation. No table authorization or
native writes added. First native write diagnostic must establish acknowledgement
format with a disabled-MAC write before table execution. Main9ca1922 fetched and
inspected (battery/Home changes include shared staging/main); integration remains
598fe27 and profile reconciliation is deferred, not silently overwritten.


Register-pair prerequisite implemented: one bounded1–24-pair read/write packet,
complete validation before output, no implicit chunking or no-ACK writes. The
existing native identity read uses the shared builder with identical wire bytes.
Build, MCU/radio/xHCI host tests and MCU ASan/UBSan passed. No candidate installed
or reboot armed for this pure encoding change; native write acknowledgement and
MAC/baseband table execution remain pending.

Active scope: opt-in firmwarev5 MCU write-response capture after the accepted
reset and strict identity exchange. Permit exactly one sequence2 random write
of MAC_SYS_CTRL1004=0, require disabled MAC before and after, retain bounded raw
response words for inspection. No initialization tables or RF writes. Shared
main remains9ca1922; current device branch inspected clean atf2dc195. Dependencies
are accepted reset/identity transport and bounded register-pair encoder. Native
write-response format remains unaccepted until the matching saved run is read.

Write-response candidate source63d7714/released8dca7885de3c61f installed unarmed.
Build, MCU/radio/xHCI tests,54 boot-profile tests and docs passed. Exact-stage GPT
saved-log/clean-unmount/automatic-return VM and missing-workspace refusal passed.
VM does not validate native MCU writes. Durable archive wifi-write-63d7714 has
payload, receipts and run record. Workspace offline checks passed; Ubuntu remains
default, recoverye47dca3a6b30c6c1 and accepted resetbfa7235c397175da retained.
Older queue payload4a47cd57ac90c504 removed from ESP only after archive equality
verification. Native write reply pending; readiness needed for cold adapter
power cycle. Results will save automatically; no photograph or commands needed.

Native write-response run d8dca7885de3c61f/source63d7714/session00065e74d8311f56-00
collected and inspected. Reset and strict identity exchange passed. Fixed
CMD_RANDOM_WRITE sequence2, register1004=0 returned16bytes with words4a020008,
00411004,00000000,4a020008 (cc13,residue1008). EP0 readbacks before and after were0;
MAC remained disabled. Firmware/laptop completion markers present. This records
one fixed write's response format, not a general write/table acceptance. The
installed diagnostic correctly logs ack_validated=0; a strict command-specific
validator and mutation tests are next before table execution.

Collection and hashes archived in wifi-write-63d7714/native-collection. Ubuntu
default, consumed one-shot and current/recovery manifests verified. Workspace
unmounted and clean:145files,466clusters,zero leaks/mismatches/errors. Run record
and checkpoint collected/unarmed. RF, receive/scan and connectivity remain pending.

Active scope: strict validator for the fixed sequence2 MAC_SYS_CTRL=0 MCU reply
captured in session00065e74d8311f56-00, host mutation tests and diagnostic wiring.
Touches mt76 MCU codec, xHCI diagnostic, tests and these guides; no table or RF
writes. Main9ca1922 remains integration target; remote Air scopes inspected.
Native validator acceptance is pending; replay validates the saved fixture only.

Strict fixed-write ACK validator implemented and wired into v5 completion.
Saved native16-byte response replay,128 single-bit mutations, stale sequences,
length/address rejection, MCU ASan/UBSan, build and xHCI tests passed. This is
source/host validation; installedd8dca7885de3c61f is unchanged. No reboot armed.
Next table work must define bounded chunks, address/value policy and completion
contracts without assuming the single-pair echo proves multi-pair behavior.

Active scope: firmwarev6 two-pair MCU response capture, after strict single-write
ACK validation. Two identical MAC_SYS_CTRL1004=0 pairs, sequence3; no additional
registers/tables/RF. Capture bounded response words and disabled-MAC readbacks;
do not presume a multi-pair echo contract. Main9ca1922 fetched, other machine
scopes unchanged; touches MCU diagnostic, boot profile, tests and guides.

Two-pair candidate sourcefe5c0a8/release101abba9feeb8cfa installed unarmed.
Build, MCU/xHCI tests,55 boot-profile tests and docs passed. Exact-stage GPT
saved-log/clean-filesystem/automatic-return VM and missing-workspace refusal
passed. Durable archive wifi-pair-fe5c0a8 contains payload, receipts and run record.
Workspace offline clean:145files/466clusters. Ubuntu default and known-good
recoverye47dca3a6b30c6c1 preserved; prior single-write captured8dca7885de3c61f
retained. Reset payloadbfa7235c397175da removed from ESP only after archive/hash
verification. Cold hardware result pending; no multi-pair ACK or table acceptance.

Native v6 sourcefe5c0a8/release101abba9feeb8cfa/session00065e751cdb49ed-00:
strict single-write ACK accepted (ack_validated1), reset/identity prerequisites
passed. Duplicate two-pair OUT24 returned IN16, words4a030008,00411004,00000000,
4a030008; sequence3/event0/length8, cc13/residue1008. MAC before/after0. Capture
completed. This proves a bounded response to this duplicate-pair diagnostic,
not that arbitrary pairs are echoed or independently verified. Since both pairs
were identical, first-versus-last echo cannot be determined. Next establish a
distinguishing response/readback contract before initialization table execution.

Matching collection archived in wifi-pair-fe5c0a8/native-collection. Ubuntu default,
consumed one-shot and current/recovery manifests verified. Workspace unmounted,
clean147files/469clusters,zero leaks/mismatches/errors. Run/checkpoint collected
and unarmed. No RF, receive, scan or connectivity acceptance.

Active scope: v7 distinct timing-pair diagnostic, XIFS1100=33a41010 and
BKOFF1104=209 from Linuxv6.18 initialization/register definitions. Require strict
single-write ACK and disabled MAC; capture before/after EP0 values independently
of MCU reply. No RF/PA/power tables or TX/RX enable. Existing profiles retained.
Main9ca1922 fetched; other Air scopes inspected separately from this USB work.

Distinct timing candidate source607ad4f/release201acb491eb09532 installed unarmed.
Build, MCU/xHCI tests (including fixed timing packet and scoped read binding),
56 boot-profile tests and docs passed. Exact-stage GPT saved-log/clean-filesystem/
automatic-return VM and missing-workspace refusal passed. Durable archive
wifi-timing-607ad4f contains payload, receipts and run record. Workspace offline
clean:147files/469clusters. Ubuntu default/recoverye47dca3a6b30c6c1 and prior
pair capture101abba9feeb8cfa retained. Oldd8dca7885de3c61f removed from ESP only
after equality with durable archive. Native distinct response/readbacks pending.

Native timing source607ad4f/release201acb491eb09532/session00065e753e794b20-00:
accepted exact two-entry transaction with independent changed readbacks. OUT24
returned IN16, words4a030008,00411100,33a41010,4a030008: first entry echoed.
XIFS1100 changed33a4100a->33a41010; BKOFF1104 changed214->209. readback_ok1,
changed0/1 both1. Reset, identity and strict single-write ACK passed; MAC stayed0
before/after. Firmware/laptop completed=yes. No larger-batch, general table or
RF acceptance follows; installed pair ACK remains capture-only (validated0).
Next: strict timing reply validator and bounded table completion with readbacks.

Collection/hashes archived in wifi-timing-607ad4f/native-collection. Ubuntu
default, consumed one-shot and current/recovery manifests verified. Workspace
unmounted, clean149files/472clusters,zero leaks/mismatches/errors. Run/checkpoint
collected and unarmed. Receive/scan/association/connectivity remain pending.

Active scope: bounded timing initialization engine for the accepted exact pair,
strict sequence3 first-entry ACK plus independent readbacks and disabled-MAC
checks. No general table/register authorization or RF expansion. Main62067a1
fetched; shared battery/main/staging changes inspected, integration still598fe27.
Touches mt76 MCU engine, xHCI timing binding, tests and guides. Native evidence
is session00065e753e794b20-00; new engine acceptance remains pending.

Bounded timing initialization implemented: exact two-entry packet, strict
first-entry ACK, both independent readbacks and MAC0 before/after. One attempt
per session, authorization before every I/O and completion; failure quarantines
through xHCI with no retry. v7 now uses this engine; v6 remains capture-only.
Host checks passed: build, MCU/xHCI, per-operation failure/cancellation,
second-readback mismatch despite valid ACK, enabled MAC refusal,128 reply-bit
mutations,192 submitted-packet mutations, ASan/UBSan, native reply replay and docs.
No new physical acceptance: installed201acb491eb09532 is unchanged and no boot
is armed. General MAC/BBP tables, larger batches and RF remain pending.

Timing-engine source48879c2/release0c12150fc75d830c installed unarmed. Exact-stage
GPT saved-log/clean-filesystem/automatic-return VM and missing-workspace refusal
passed. Prior build/MCU/xHCI/fault/sanitizer/native-replay checks are archived with
payload and run record in wifi-timing-engine-48879c2. Workspace offline checks
passed. Ubuntu default/recoverye47dca3a6b30c6c1 and prior timing capture201acb491eb09532
retained; obsolete101abba9feeb8cfa removed only after durable archive equality.
Next hardware acceptance requires wifi-timing-init status0, ack_validated1,
complete1, reads6, both target readbacks and MAC0 before/after. Native pending.

Native timing engine accepted: source48879c2/release0c12150fc75d830c/
session00065e7570e64246-00. wifi-timing-init status0,attempted1,write_attempted1,
sent24,received16,ack_validated1,complete1,reads6. MAC before/after0; XIFS changed
33a4100a->33a41010 and BKOFF214->209. Firmware/reset/strict identity/single-write
prerequisites passed and firmware/laptop completed=yes. This accepts the exact
bounded two-register initialization only, not full MAC/BBP tables or RF.

Matching collection/hashes archived in wifi-timing-engine-48879c2/native-collection.
Ubuntu default, consumed one-shot and current/recovery manifests verified.
Workspace unmounted and clean151files/475clusters,zero leaks/mismatches/errors.
Run/checkpoint collected and unarmed. Next expand MAC/baseband initialization
with bounded commands and register-specific readback policies; RF/scan/network
connectivity remain pending.

Active scope: fixed MAC basics engine after accepted timing, two pairs per
command with sequences4/5. Targets legacy basic rate1408=13f, HT basic
rate140c=4003 (model final value), RX filter1400=17f97, maximum length1018=a1fff,
from Linuxv6.18 register/init definitions. No TX/RX enable, RF, PA/power or key
writes. Require exact first-entry ACK plus per-register readbacks and MAC0 at
each batch boundary. Pure engine/tests first; native binding remains separately
gated until validated. Main1454cb7 fetched/inspected; integration still598fe27.

Fixed four-register MAC basics engine implemented, with separate sequence4/5
packets, exact first-entry replies, per-register readbacks and MAC0 boundaries.
Host tests passed for all operation failures/authorization boundaries, reply
mutations, stale batch replies and each readback mismatch. Build, MCU/xHCI,
ASan/UBSan and docs checks passed. Not yet bound to native xHCI or accepted on
hardware; installed0c12150fc75d830c remains unchanged and no reboot is armed.
Next bind this fixed plan after accepted timing with exact packet/read allowlists,
then stage and rehearse the candidate before native acceptance.

Active scope: opt-in firmwarev8 binding of fixed MAC basics after accepted timing.
Exact sequence4/5 packets only, one submission each in order, four-register/ MAC0
read allowlist and prior timing completion required. Local transport/boot tests,
exact-stage rehearsal and preserved recovery precede native test. Main1454cb7
fetched, integration remains598fe27; no battery or sibling gates modified.

Native binding implemented; build, MAC/MCU/xHCI host suites, 57 boot-profile
tests and docs checks passed. Transport tests cover ordered submissions,
prior-batch completion, exact packet mutations, read scope, stale events and
retained-DMA quarantine. Preparing firmwarev8 exact-stage VM and hardware trial;
new register responses/readbacks are still physically unaccepted.

MAC basics native trial source facb862, release190ab5353ae2f8b6, firmwarev8:
exact-stage GPT save/unmount/return and missing-workspace return VM checks passed.
Installed unarmed with Ubuntu default and e47dca3a6b30c6c1 recovery preserved;
accepted timing engine0c12150fc75d830c retained. Prior201acb491eb09532 payload
removed from ESP only after matching its durable archive byte-for-byte.
Workspace preboot filesystem/allocation checks passed. Payload, VM receipts,
local checks and run record archived outside build in wifi-mac-native-facb862.
Next cold trial requires Wi-Fi disconnected for10 seconds while off, then
reconnected before power-on; workspace stays attached. Automatic log saving and
return enabled. Accept only both unique replies, all four exact readbacks, MAC0
and completion markers; RF/connectivity remain pending.

Native MAC basics accepted, 2026-10-22: source facb862,
release190ab5353ae2f8b6, session00065e75fad3378e-00. Firmware/reset/identity,
single-write and timing prerequisites all passed. Sequence4 and5 each sent24,
received16 with exact first-entry echoes. MAC initialization status0, submitted2,
batches_attempted2/completed2, ack_mask3, complete1, reads12; MAC stayed0.
Readbacks:1408 0->13f,140c 82008000->4003,1400 15f9f->17f97,
1018 a7fff->a1fff. Firmware and laptop completion markers present.
Matching collection archived in wifi-mac-native-facb862/native-collection.
Ubuntu default, consumed one-shot and installed/recovery manifests verified.
Workspace clean153files/478clusters with zero allocation errors; unmounted.
Run/checkpoint collected and unarmed. Next extend remaining MAC/baseband setup
with bounded commands and register-specific readbacks. RF, scan, association
and connectivity remain pending; this accepts only the fixed four-register plan.

Active scope: consume main22fd266 (PR55 shared service/workspace lifecycle and
Air6,1 input support) before extending Wi-Fi initialization. Preserve Pro MCU
commands/tests and all upstream battery profiles; no Pro battery acceptance.
Resolve overlapping boot profile numbers with a namespaced Wi-Fi calibration
recipe and exact historical verification. Build/core tests and exact fw-v8 VM
rehearsal precede any new hardware candidate. Current accepted native release
190ab5353ae2f8b6 and recovery remain installed and unarmed.

Merged baseline22fd266: full build/test-core,64 boot-profile tests and docs checks
passed. All48 upstream laptop profile variants generate identical configs.
Historical v43/v44 Wi-Fi recipes still verify exactly; new calibration uses
wifi-cal-v1 to avoid battery profile collisions. PR60 process-lifetime work is
separate and remains unmerged. Next rehearse/install firmwarev8 for a regression
of the accepted MAC plan on this baseline before extending initialization.

Merged-baseline regression installed unarmed: source285aa45,
releasecb8cc153be25dac9, firmwarev8. Exact GPT workspace/save/return and
missing-workspace VM rehearsals passed. Accepted190ab5353ae2f8b6 and desktop
recoverye47dca3a6b30c6c1 retained; prior timing-only0c12150fc75d830c removed
from ESP after full durable archive equality. Workspace preboot checks passed.
Run/payload/VM/local evidence archived outside build in wifi-main-merge-285aa45.
Next cold native run repeats the accepted sequence4/5/readback plan against
main22fd266. No new radio registers or physical battery acceptance in this run.

Merged-baseline native regression accepted, 2026-10-22: source285aa45
(main22fd266), releasecb8cc153be25dac9, session00065e765bdc87af-00.
Firmware/reset/identity/single-write/timing prerequisites passed; both MAC
commands sent24/received16 with exact sequence4/5 replies. Status0, submitted2,
completed2, ack_mask3, complete1, reads12; all four before/after readbacks match
the accepted facb862 run and MAC remained0. Completion markers present.
Collection/hashes archived in wifi-main-merge-285aa45/native-collection.
Ubuntu default, consumed one-shot and installed/recovery manifests verified;
workspace clean155files/481clusters, zero allocation errors, unmounted.
Run/checkpoint collected and unarmed. Remaining MAC/baseband initialization
can now proceed on this merged baseline; RF/scan/connectivity remain pending.

Active scope: fixed MAC frame-format plan after accepted basics. Disable TSO
0250=0 and header translation0260=0, then set HT control1410=1ff and PN padding
150c=3 using distinct sequences6/7. Values follow captured Linuxv6.18 model
initialization; no RF/power/key or TX/RX enable writes. Require accepted basics,
exact packets/replies, per-register readback and MAC0 boundaries. Native response
shape remains pending for these commands. Main340f566 inspected (process lifetime
work); this independent MCU step uses physically accepted integration22fd266.

Frame-format engine/native binding implemented with fixed seq6/7 and separate
phase ownership; prior basics must be complete. Build, basics/format/MCU/xHCI
tests,65 boot-profile tests, ASan/UBSan format tests and docs checks passed.
Firmwarev9 rehearsal is next; native replies/readbacks remain pending.

Frame-format candidate installed unarmed: source7713556,
release54d6def400fd2a6c, firmwarev9. Exact-stage GPT save/unmount/return and
missing-workspace VM checks passed. Ubuntu default, accepted merged MAC image
cb8cc153be25dac9 and desktop recoverye47dca3a6b30c6c1 retained. Original
190ab5353ae2f8b6 removed from ESP only after durable archive equality.
Workspace preboot checks clean. Payload/run/VM/local evidence archived outside
build in wifi-mac-format-7713556. Next cold trial accepts only exact seq6/7
replies, all four target readbacks, MAC0 boundaries and saved completion.
Physical frame-format acceptance remains pending; RF/connectivity untested.

Native frame-format initialization accepted, 2026-10-22: source7713556,
release54d6def400fd2a6c, session00065e76901356a1-00. All prior prerequisites
passed. Sequence6/7 each sent24/received16 with exact first-entry replies;
status0, submitted2, attempted2/completed2, ack_mask3, complete1, reads12.
MAC before/after0. Readbacks0250 0->0,0260 0->0,1410 100->1ff,150c 0->3.
Firmware and laptop completion markers present. Full matching collection and
hashes archived in wifi-mac-format-7713556/native-collection. Ubuntu default,
consumed one-shot and current/recovery manifests verified; workspace unmounted
and clean157files/484clusters with zero allocation errors. Run/checkpoint
collected and unarmed. Next extend remaining MAC/baseband initialization;
RF, scanning, association and network connectivity remain pending.

Active scope: read-only baseband prerequisite capture after accepted frame format.
Read MAC control/status, PBF clock control and BBP CORE0 (2000); bound zero-core
polling to20 reads, reject all-ones and require MAC disabled/idle before/after.
No write callback or new MCU sequences. Baseband readiness follows Linuxv6.18
mt76x0/phy.c; it does not accept baseband tables or RF. Main105dffd fetched and
inspected (Air lifecycle work); independent capture uses accepted base22fd266.

Read-only BBP engine/native binding implemented. Build, BBP/basics/format/MCU/
xHCI tests,66 boot-profile tests, BBP ASan/UBSan and docs passed. Native gate
requires completed frame format and admits only four reads. Firmwarev10
rehearsal follows; CORE0 value/readiness and clock observations remain pending.

BBP readiness candidate installed unarmed: source6e0bd27,
release6df00376834c37fd, firmwarev10. Exact GPT save/unmount/return and
missing-workspace VM checks passed. Ubuntu default, accepted frame-format
54d6def400fd2a6c and desktop recoverye47dca3a6b30c6c1 retained. Prior merged
cb8cc153be25dac9 removed from ESP only after durable archive equality.
Workspace preboot checks clean. Payload/run/VM/local evidence archived outside
build in wifi-bbp-ready-6e0bd27. Next cold trial checks responding CORE0 and
disabled/idle MAC before/after; PBF value is observation only. Baseband table
initialization and RF/connectivity remain unaccepted.

Read-only BBP readiness accepted, 2026-10-22: source6e0bd27,
release6df00376834c37fd, session00065e76bf8562b4-00. Firmware, reset,
identity, timing, basics and frame-format prerequisites passed. BBP status0,
attempted1, complete1, reads6, core_reads1; CORE0=f000f200, PBF=00080000,
MAC control/status0 before and after. Completion markers present. This accepts
core readiness only, not clock programming, baseband tables or RF.
Matching collection/hashes archived in wifi-bbp-ready-6e0bd27/native-collection.
Ubuntu default, consumed one-shot and current/recovery manifests verified;
workspace unmounted and clean159files/487clusters, zero allocation errors.
Run/checkpoint collected and unarmed. Next implement bounded baseband setup
with exact register readbacks; scanning/association/connectivity remain pending.

Active scope: first bounded BBP CORE setup after native readiness. Fixed
CORE1/4/24/32 offsets2004/2010/2060/2080 values2/0/0/4003000a from Linuxv6.18
model initialization, sequences8/9. Require captured CORE0=f000f200 before/after
each batch, MAC0 boundaries and all target readbacks. No RF, channel, power or
MAC enable writes. Main105dffd unchanged; accepted integration22fd266 retained.

BBP CORE implementation validation: full build, bounded engine and native xHCI
fixtures, prior MCU/timing/MAC/frame-format/readiness regressions, ASan/UBSan
and all 67 boot-profile tests pass. Physical acceptance remains pending.

BBP CORE candidate source23b97f2 / release5f7b66b688f803cb installed unarmed.
Exact-stage GPT save/return and missing-workspace refusal/return VM checks pass.
Workspace fsck/allocation checks pass; Ubuntu default and desktop recovery
e47dca3a6b30c6c1 retained, accepted readiness6df00376834c37fd retained. Old
format54d6def400fd2a6c removed only after full durable-archive equality checks.
Run record: build/bringup/wifi-bbp-core/run.json; durable evidence/payload in
~/.local/share/srvros/macbookpro11-2/wifi-bbp-core-23b97f2. Await cold-trial
readiness; collect the matching session first after return. RF remains untested.

BBP CORE native acceptance (2026-10-22): source23b97f2, release5f7b66b688f803cb,
session00065e76e3fff75c-00 passed exact sequence8/9 replies, both24-byte commands
and16-byte responses, status0/submitted2/completed2/ack_mask3/reads16.
CORE1 changed0->2, CORE4/24 stayed0, CORE32 changed40030001->4003000a;
CORE0 stayedf000f200 and MAC stayed0. All prior setup regressions and both
capture completion markers passed. Evidence and hashes archived outside build
in wifi-bbp-core-23b97f2/native-collection. Ubuntu default, consumed one-shot,
installed payload and desktop recovery verified. Workspace unmounted; clean
161files/490clusters with zero leaked allocations or bitmap mismatches.
Run/checkpoint collected and unarmed. Next scope is remaining bounded baseband
configuration; RF, scanning, association and connectivity remain pending.

Active scope: finish CORE42/44 and initialize IBI11/AGC0 with fixed values
0/0/0fde8081/00021400 at20a8/20b0/212c/2300, sequences10/11. Reuse the
bounded MCU engine with captured CORE identity and disabled-MAC guards,
requiring completed native CORE setup. Touch MCU/xHCI, isolated capture/profile
and tests only; no RF or transmit enable. Main105dffd inspected unchanged;
Air PR63 at182ee78 adds independent battery capture (shared profile/main files,
separate recipe). Retain integration22fd266 and preserve both scopes.

Validation for BBP front: full build, fixed-plan/per-operation failure tests,
xHCI prerequisites and transport quarantine, prior MCU/MAC/CORE/readiness
regressions, ASan/UBSan, 68 boot profiles and documentation checks pass.
Native acceptance remains pending.

Remaining CORE / initial IBI/AGC trial installed: sourcedaffe4c,
release1874caf6830abdc0. Exact-stage GPT save/return and missing-workspace
refusal/return VM checks pass. Workspace fsck/allocation clean; Ubuntu default
and desktop recoverye47dca3a6b30c6c1 retained, accepted CORE5f7b66b688f803cb
retained. Readiness6df00376834c37fd removed from ESP only after full durable
archive equality. Run record build/bringup/wifi-bbp-front/run.json; payload,
VM receipts and prepared collector archived in
~/.local/share/srvros/macbookpro11-2/wifi-bbp-front-daffe4c. Unarmed, awaiting
cold-trial readiness. Collect the matching session first after return; native
acceptance pending. No RF/scan/connectivity test is included.

Remaining CORE / initial IBI/AGC native acceptance (2026-10-22): source daffe4c,
release1874caf6830abdc0, session00065e772300653c-00 passed exact sequence10/11
replies, both24-byte commands and16-byte responses, status0/submitted2/completed2/
ack_mask3/reads16. CORE42/44 stayed0, IBI11 changed0->0fde8081, AGC0 changed
7400->21400; CORE0 stayedf000f200 and MAC stayed0. All prior setup checks and
both capture completion markers passed. Matching logs and hashes archived in
wifi-bbp-front-daffe4c/native-collection outside build. Ubuntu default, consumed
one-shot, installed payload and desktop recovery verified. Workspace unmounted;
clean163files/493clusters with zero leaked allocations or bitmap mismatches.
Run/checkpoint collected and unarmed. Next: remaining AGC/baseband initialization,
then RF/scan/association/connectivity, which remain untested.

Active scope: fixed initial AGC1/2/15/22 at2304/2308/233c/2358, values
3/003a6464/88a28cb8/1e21, sequences12/13. Require completed prior IBI/AGC0
setup, captured CORE identity and disabled MAC before/after each batch. Touch
MCU/xHCI, isolated main/staging recipe and tests. RF remains disabled. Fetched
main14f17d2 (PR63 battery capture) and PR64 Type feedback; neither adds Wi-Fi
dependencies. Retain physically accepted integration22fd266 for this trial.
The 4-bit command sequence space is not wrapped; larger remaining plans will
need explicit batching/session design before exceeding sequence15.

AGC validation: full build, all fixed MCU/MAC/baseband engines, xHCI
prerequisite/transport/read-scope/quarantine fixtures, ASan/UBSan, 69 boot
profiles and documentation checks pass. Physical acceptance remains pending.

Initial AGC trial installed: source56bf5da, releaseb245ac895a7d9396. Exact-stage
GPT save/return and missing-workspace refusal/return VM checks pass. Workspace
fsck/allocation clean; Ubuntu default and desktop recoverye47dca3a6b30c6c1
retained, accepted CORE/IBI/AGC0 release1874caf6830abdc0 retained. Earlier CORE
release5f7b66b688f803cb removed from ESP only after full durable archive equality.
Run record build/bringup/wifi-bbp-agc/run.json; payload, VM receipts and prepared
collector archived in ~/.local/share/srvros/macbookpro11-2/wifi-bbp-agc-56bf5da.
Unarmed, awaiting cold-trial readiness. Collect the matching session first on
return. Native acceptance pending; no RF/scan/connectivity test included.

Initial AGC native acceptance (2026-10-22): source56bf5da,
releaseb245ac895a7d9396, session00065e777b83c006-00 passed exact sequence12/13
replies, both24-byte commands/16-byte responses, status0/submitted2/completed2/
ack_mask3/reads16. AGC1 changed1->3, AGC2 changed6464->3a6464; AGC15/22
already matched88a28cb8/1e21. CORE0 stayedf000f200 and MAC stayed0. All prior
setup checks and both capture completion markers passed. Logs/hashes archived
in wifi-bbp-agc-56bf5da/native-collection outside build. Ubuntu default, consumed
one-shot, installed payload and desktop recovery verified. Workspace unmounted;
clean165files/496clusters, zero leaked allocations or bitmap mismatches.
Run/checkpoint collected and unarmed. Next design bounded larger command batches
and session sequencing before exhausting the4-bit sequence space, then finish
AGC/baseband initialization. RF/scan/association/connectivity remain pending.

Active scope: four-pair MCU batching comparison using accepted AGC1/2/15/22
values, one40-byte command at sequence14 replacing sequence12/13 after IBI/AGC0.
Order AGC15/22/1/2 puts changed targets last, proving the packet tail is applied.
Generalize only the private fixed-plan engine to two or four pairs; public
entry points retain fixed tables/sequences. Check every baseline/readback and
MAC/CORE identity boundaries. No new register settings or RF enable. Main9256faa
(PR64/65 Type feedback) and Air PR66 battery transitions inspected; independent
Wi-Fi scope, retain accepted integration22fd266. Touch MCU/xHCI/main/profile
and tests. Sequence reuse stays forbidden; successful batching can later
consolidate setup commands without exhausting the four-bit space.

Four-pair validation: full build, all shared fixed-plan regressions, new
13-operation/14-authorization-boundary faults, native exact40-byte packet and
quarantine fixtures, ASan/UBSan, 70 boot profiles and docs checks pass.
The comparison skips the old AGC phase; native expected tail baselines1/6464
must change to3/3a6464. Physical batching acceptance remains pending.

Four-pair comparison installed: sourced1c0811, releaseb3eaadb9ad679dbb.
Exact-stage GPT save/return and missing-workspace refusal/return VM checks pass.
Workspace fsck/allocation clean; Ubuntu default and desktop recovery
e47dca3a6b30c6c1 retained; accepted AGCb245ac895a7d9396 retained. Older
CORE/IBI1874caf6830abdc0 removed only after full durable archive equality.
Run record build/bringup/wifi-bbp-batch/run.json; payload, VM receipts and
strict tail-change collector archived in
~/.local/share/srvros/macbookpro11-2/wifi-bbp-batch-d1c0811. Unarmed, awaiting
cold-trial readiness. Collect matching session first after return; no native
batching acceptance yet. RF and connectivity remain untested.

Four-pair native acceptance (2026-10-23): sourced1c0811,
releaseb3eaadb9ad679dbb, session00065e7a17440518-00 passed one40-byte command,
exact sequence14/16-byte reply, status0/submitted1/completed1/ack_mask1/reads12.
AGC15/22 stayed88a28cb8/1e21; tail pairs AGC1/2 changed1/6464->3/3a6464.
The earlier sequence12/13 AGC phase was absent, so tail readbacks prove full
payload application. CORE0 stayedf000f200, MAC stayed0; preceding setup checks
and both completion markers passed. Matching logs/hashes archived outside build
in wifi-bbp-batch-d1c0811/native-collection. Ubuntu default, consumed one-shot,
installed payload and desktop recovery verified. Workspace unmounted; clean
167files/499clusters, zero leaked allocations/bitmap mismatches. Run/checkpoint
collected and unarmed. Next consolidate setup using accepted four-pair batches
to free command identifiers before remaining baseband work; sequence reuse,
RF/scan/association/connectivity remain unaccepted.

Active scope: consolidate five accepted four-register plans into sequences4..8
after unchanged firmware/reset/identity/write/timing steps1..3. Preserve MAC
basics -> format -> read-only BBP readiness -> CORE -> IBI/AGC0 -> AGC order.
Each phase uses one40-byte command and all four readbacks; phases2..4 retain
captured CORE identity guards. No new settings or sequence reuse. Touch fixed
MCU engine, xHCI phase ownership, isolated capture/profile and tests. Inspected
main80b567a and Air PR69: independent desktop/battery work, no Wi-Fi dependency;
retain accepted integration22fd266. Native consolidation acceptance pending.

Consolidation validation: full build, five-phase fixed engine and all older
fixed-plan regressions, native xHCI phase/prefix/readiness/read-scope/packet
fixtures, ASan/UBSan, 71 boot profiles and docs checks pass. No native
consolidation acceptance yet; prepare exact-stage VM and automatic cold trial.

Consolidated trial installed: source1b9bf8e, releasee459728ba57fea08. Exact-stage
GPT save/return and missing-workspace refusal/return VM checks pass. Workspace
fsck/allocation clean; Ubuntu default and desktop recoverye47dca3a6b30c6c1
retained; accepted batchingb3eaadb9ad679dbb retained. Older AGCb245ac895a7d9396
removed only after full durable archive equality. Run record
build/bringup/wifi-compact/run.json; payload/receipts and strict five-phase
collector archived in ~/.local/share/srvros/macbookpro11-2/wifi-compact-1b9bf8e.
Unarmed awaiting cold-trial readiness. Collect matching session first after
return; consolidated native acceptance pending. RF/connectivity remain untested.

Consolidated initialization native acceptance (2026-10-23): source1b9bf8e,
releasee459728ba57fea08, session00065e7a55ca88f1-00 passed all five40-byte
commands, exact sequence4..8 replies, status0 and one completion/ACK per phase.
Reads10/10/12/12/12 and all20 baselines/readbacks matched expectations; BBP
readiness occurred between phases1/2, CORE0 stayedf000f200 in phases2..4 and
MAC stayed0. Old phase log families absent; prior firmware/reset/identity/write/
timing checks and both completion markers passed. Logs/hashes archived outside
build in wifi-compact-1b9bf8e/native-collection. Ubuntu default, consumed one-shot,
installed payload and desktop recovery verified. Workspace unmounted; clean
169files/502clusters, zero leaked allocations/bitmap mismatches. Run/checkpoint
collected and unarmed. Next add remaining baseband settings using unused command
identifiers9..15; no sequence recycling or RF/connectivity acceptance.

Active scope: add AGC23/24/25/26 as optional compact phase5, sequence9,
addresses235c/2360/2364/2368 and values272c/2f3a/8000005a/007c2005 from
Linuxv6.18 initial MT7610 settings. All prior phases, exact replies, readbacks
and MAC/CORE guards required. Preserve five-phase v15 with explicit phase
limit; new v16 permits six. Touch fixed MCU plan, native phase ownership,
capture/profile and tests. Fetched main58b354f: Air desktop evidence only since
last inspection; Air PR69 remains independent battery scope. Retain accepted
integration22fd266. No RF or sequence reuse; native added settings pending.

AGC extension validation: full build, six-phase fixed engine and prior
regressions, native xHCI phase/limit/read-scope/transport checks, ASan/UBSan,
72 boot profiles and docs checks pass. Native phase5 acceptance pending.

AGC23..26 trial installed: sourcec9a84ec, release8bf9cd874ed0f02d. Exact-stage
GPT save/return and missing-workspace refusal/return VM checks pass. Workspace
fsck/allocation clean; Ubuntu default and desktop recoverye47dca3a6b30c6c1
retained; accepted consolidatede459728ba57fea08 retained. Older batching
b3eaadb9ad679dbb removed only after full durable archive equality. Run record
build/bringup/wifi-agc-more/run.json; payload/receipts and six-phase collector
archived in ~/.local/share/srvros/macbookpro11-2/wifi-agc-more-c9a84ec. Unarmed
awaiting cold-trial readiness. Collect matching session first after return;
new phase native acceptance pending. RF/connectivity remain untested.

AGC23..26 native acceptance (2026-10-23): sourcec9a84ec,
release8bf9cd874ed0f02d, session00065e7ad65bc92e-00 passed sequence9 sent40/
received16, exact reply, status0/submitted1/completed1/ack_mask1/reads12.
AGC23 changed272f->272c, AGC24 stayed2f3a, AGC25 changed0008005a->8000005a,
AGC26 changed005a2004->007c2005. CORE0 stayedf000f200, MAC stayed0. All24
readbacks, earlier phases, readiness ordering and both completion markers
passed. Matching logs/hashes archived outside build in
wifi-agc-more-c9a84ec/native-collection. Ubuntu default, consumed one-shot,
installed payload and desktop recovery verified. Workspace unmounted; clean
171files/505clusters, zero leaked allocations/bitmap mismatches. Run/checkpoint
collected and unarmed. Next remaining AGC/baseband settings using identifiers
10..15; RF/scan/association/connectivity remain pending.

Active scope: optional compact phase6 for AGC33/34/37/41, sequence10,
addresses2384/2388/2394/23a4 and values3238/000a0c0c/2121262c/38383e45
from Linuxv6.18 initial MT7610 settings. Require preceding six phases and
MAC/CORE boundaries; preserve v15/v16 phase limits, add v17 limit7. Touch
fixed MCU plan, xHCI limits, capture/profile and tests. Fetched mainaddbd02;
PR71 imported file-identity/workload replay fix inspected, outside isolated
Wi-Fi capture; Air PR69 remains battery scope. Retain accepted integration
22fd266 for this trial. No RF or sequence reuse; new settings native pending.

AGC33/34/37/41 candidate b3e4ee1 / releaseff59ebda5a2483ae installed
unarmed. Build, focused MCU/baseband/xHCI tests, sanitizer,73 profile tests
and documentation checks pass. Exact-stage GPT VM saved/unmounted/reset;
missing workspace returned without starting the probe. Payload, VM receipts,
run record and collector archived outside build in wifi-agc-next-b3e4ee1.
Current accepted8bf9cd874ed0f02d and desktope47dca3a6b30c6c1 retained;
obsoletee459728ba57fea08 removed from ESP only after archive equality check.
Workspace preboot filesystem/allocation checks pass, unmounted. Native
acceptance pending an attended cold adapter power cycle; Ubuntu stays default.

AGC33/34/37/41 first return is incomplete: one-shot consumed, but workspace
collection contained no session for releaseff59ebda5a2483ae and no new
session beyond00065e7ad65bc92e-00. Do not accept this phase. Installed
candidate and known-good desktop recovery match their archived bytes;
Ubuntu remains default. Workspace clean:171files/505clusters, zero leaks
or allocation mismatches, then unmounted. Collection archived as
wifi-agc-next-b3e4ee1/incomplete-return-collection; run/checkpoint now
returned/unarmed. Await boot observation to distinguish an early boot or
workspace discovery failure from a skipped test before scheduling a retry.

AGC33/34/37/41 retry accepted on this captured adapter: sourceb3e4ee1,
releaseff59ebda5a2483ae, session00065e8394ee6855-00. User observed SrvrOS
start and return automatically. Phase6 sequence10 sent40/received16 with
exact first-entry reply, status0, one submission/completion, ack_mask1 and
12 reads. AGC33 changed003c->3238; AGC34 stayed000a0c0c; AGC37 stayed
2121262c; AGC41 changed2727303a->38383e45. CORE0 stayedf000f200 and
MAC stayed0. All preceding six phases, firmware/reset/identity/timing,
readiness ordering,28 register readbacks and both completion markers passed.
Collection archived outside build in wifi-agc-next-b3e4ee1/native-collection.
Ubuntu default, consumed one-shot and candidate/desktop recovery verified.
Workspace clean:173files/508clusters, zero leaks/allocation mismatches,
unmounted. Run/checkpoint collected and unarmed. First attempt remains
incomplete with no saved session; its cause is unknown. Next remaining
AGC/baseband settings using unused identifiers11..15; no RF/connectivity
acceptance and no sequence recycling.

Active scope: phase7 AGC57/59/63 and TXC0 at sequence11, fixed offsets
23e4/23ec/23fc/2400 and values1010/baa20e96/1/00280403 from pinned
Linuxv6.18 MT7610 initialization. Require seven preceding phases and
MAC/CORE boundaries; v18 limit8, earlier limits unchanged. Touch fixed MCU
plan, xHCI limits, capture/profile/tests. Fetched main5089172; new PR72
contains only Air6,1 file identity acceptance documentation. Air PR69
remains independent battery work. Actual integrated base remains22fd266.
RF/connectivity and sequence reuse remain outside scope.

AGC57/59/63+TXC0 candidate sourcede17cb6 / releasec34215d727a60f08
installed unarmed. Build, compact/xHCI host tests, sanitizer,74 boot-profile
tests and documentation checks pass. Exact-stage GPT VM saved/unmounted
and reset; missing-workspace VM returned without probing. Payload, VM
receipts, run record and collector archived outside build in
wifi-agc-tail-de17cb6. Acceptedff59ebda5a2483ae and known-good desktop
e47dca3a6b30c6c1 retained. Obsolete8bf9cd874ed0f02d removed from ESP
only after archived-byte equality verification. Workspace clean before
install:173files/508clusters, zero allocation errors; unmounted. Native
acceptance pending cold adapter power cycle; Ubuntu remains default.

AGC57/59/63+TXC0 accepted on captured adapter: sourcede17cb6,
releasec34215d727a60f08, session00065e83ccc7f06a-00. Phase7 sequence11
sent40/received16 with exact reply, status0, one submission/completion,
ack_mask1 and12 reads. AGC57 stayed1010; AGC59 changedbaa20e23->baa20e96;
AGC63 changed0->1; TXC0 stayed00280403. CORE0 stayedf000f200 and MAC0.
All preceding phases, firmware/reset/identity/timing, readiness ordering,
32 register readbacks and both completion markers passed. Matching evidence
archived outside build in wifi-agc-tail-de17cb6/native-collection. Ubuntu
default, consumed one-shot and installed candidate/desktop recovery verified.
Workspace clean:175files/511clusters, zero leaks/allocation mismatches,
unmounted. Run/checkpoint collected and unarmed. Next remaining TXC/RXC
and other baseband initialization; identifiers12..15 unused. RF, scanning
and connectivity remain pending; no sequence recycling accepted.

Active scope: phase8 TXC1 and RXC1/2/3 at sequence12, fixed offsets
2404/2504/2508/250c and values0/12/11/5 from pinned Linuxv6.18 MT7610
initialization. Require all eight accepted phases and MAC/CORE boundaries;
v19 limit9, prior limits unchanged. Touch MCU plan, xHCI limits, capture,
profile/tests. Fetched mainbd1e834; PR73 workload refusal evidence and
ordinary desktop VM changes inspected, independent of Wi-Fi probe path.
Air PR69 remains battery scope. Actual integrated base remains22fd266.
No RF/connectivity or sequence recycling in this trial.

TXC1/RXC1/2/3 candidate sourcedf537ea / release7d6ccc2fe96d530c
installed unarmed. Build, compact/xHCI host tests, sanitizer,75 boot-profile
tests and docs checks pass. Exact-stage GPT VM saved/unmounted/reset;
missing-workspace VM returned without probing. Payload, receipts, run record
and collector archived outside build in wifi-rxc-start-df537ea. Accepted
c34215d727a60f08 and desktop recoverye47dca3a6b30c6c1 retained. Obsolete
ff59ebda5a2483ae removed from ESP only after archive equality verification.
Workspace clean before install:175files/511clusters, zero allocation errors,
unmounted. Native acceptance pending cold adapter power cycle; Ubuntu default.

TXC1/RXC1/2/3 accepted on captured adapter: sourcedf537ea,
release7d6ccc2fe96d530c, session00065e84abf0e3e4-00. Phase8 sequence12
sent40/received16 with exact reply, status0, one submission/completion,
ack_mask1 and12 reads. TXC1 stayed0; RXC1 changed14->12; RXC2 stayed11;
RXC3 stayed5. CORE0 stayedf000f200 and MAC0. All earlier phases,
firmware/reset/identity/timing, readiness ordering,36 register readbacks
and both completion markers passed. Evidence archived outside build in
wifi-rxc-start-df537ea/native-collection. Ubuntu default, consumed one-shot
and installed candidate/desktop recovery verified. Workspace clean:
177files/514clusters, zero leaks/allocation mismatches, unmounted.
Run/checkpoint collected and unarmed. Next remaining RXC/baseband settings
using identifiers13..15; no sequence reuse, RF or connectivity acceptance.

Active scope: phase9 RXC4/5/7 and TXO8 at sequence13, fixed offsets
2510/2514/251c/2620 and values0/f977c4ec/90/0 from pinned Linuxv6.18
MT7610 initialization. Require nine prior phases and MAC/CORE boundaries;
v20 limit10, earlier limits preserved. Touch MCU plan, xHCI limits,
capture/profile/tests. Refetched mainbd1e834 unchanged; Air PR69 battery
and Air6,1 desktop evidence remain separate. Actual integrated base22fd266.
No RF, connectivity or command identifier recycling. Native result pending.

RXC4/5/7+TXO8 candidate sourcefd54365 / release492306b5fccc17b9
installed unarmed. Build, compact/xHCI host tests, sanitizer,76 boot-profile
tests and docs checks pass. Exact-stage GPT VM saved/unmounted/reset;
missing-workspace VM returned without probing. Payload, receipts, run record
and collector archived outside build in wifi-rxc-tail-fd54365. Accepted
7d6ccc2fe96d530c and desktop recoverye47dca3a6b30c6c1 retained. Obsolete
c34215d727a60f08 removed from ESP only after archive equality verification.
Workspace clean before install:177files/514clusters, zero allocation errors,
unmounted. Native acceptance pending cold adapter power cycle; Ubuntu default.

RXC4/5/7+TXO8 accepted on captured adapter: sourcefd54365,
release492306b5fccc17b9, session00065e84c6495273-00. Phase9 sequence13
sent40/received16 with exact reply, status0, one submission/completion,
ack_mask1 and12 reads. RXC4 stayed0; RXC5 changed71550400->f977c4ec;
RXC7 changed0->90; TXO8 stayed0. CORE0 stayedf000f200 and MAC0.
All preceding phases, firmware/reset/identity/timing, readiness ordering,
40 register readbacks and both completion markers passed. Evidence archived
outside build in wifi-rxc-tail-fd54365/native-collection. Ubuntu default,
consumed one-shot and installed candidate/desktop recovery verified.
Workspace clean:179files/517clusters, zero leaks/allocation mismatches,
unmounted. Run/checkpoint collected and unarmed. Remaining TXBE/RXFE/RXO
and other baseband settings pending; identifiers14..15 unused. Further
expansion must respect sequence exhaustion. RF/connectivity not accepted.

Active scope: phase10 TXBE0/4/6/8 at sequence14, fixed offsets
2700/2710/2718/2720 and values0/4/0/14 from pinned Linuxv6.18 MT7610
initialization. Require ten previous phases and MAC/CORE guards; v21 limit11,
prior limits unchanged. Touch MCU plan, xHCI limits, capture/profile/tests.
Fetched mainf5e1525; PR69 opt-in Home battery startup and PR74 Air6,1
evidence inspected. Default battery remains off; those desktop paths are
independent of this isolated capture. Retain integrated base22fd266. Preserve
shared desktop recipes on future integration. No RF or identifier recycling.

TXBE0/4/6/8 candidate source56e1f4a / release6477738b1d3cf7d5
installed unarmed. Build, compact/xHCI host tests, sanitizer,77 boot-profile
tests and docs checks pass. Exact-stage GPT VM saved/unmounted/reset;
missing-workspace VM returned without probing. Payload, receipts, run record
and collector archived outside build in wifi-txbe-start-56e1f4a. Accepted
492306b5fccc17b9 and desktop recoverye47dca3a6b30c6c1 retained. Obsolete
7d6ccc2fe96d530c removed from ESP only after archive equality verification.
Workspace clean before install:179files/517clusters, zero allocation errors,
unmounted. Native acceptance pending cold adapter power cycle; Ubuntu default.

TXBE0/4/6/8 accepted on captured adapter: source56e1f4a,
release6477738b1d3cf7d5, session00065e85c7b7d806-00. Phase10 sequence14
sent40/received16 with exact reply, status0, one submission/completion,
ack_mask1 and12 reads. TXBE0 stayed0; TXBE4 changed0->4; TXBE6 changed1->0;
TXBE8 changed4014->14. CORE0 stayedf000f200 and MAC0. All prior phases,
firmware/reset/identity/timing, readiness ordering,44 register readbacks
and both completion markers passed. Evidence archived outside build in
wifi-txbe-start-56e1f4a/native-collection. Ubuntu default, consumed one-shot
and installed candidate/desktop recovery verified. Workspace clean:
181files/520clusters, zero leaks/allocation mismatches, unmounted.
Run/checkpoint collected and unarmed. Remaining TXBE/RXFE/RXO settings
pending; identifier15 remains unused. Further expansion must address
sequence exhaustion without recycling identifiers. RF/connectivity pending.

Active scope: phase11 TXBE9/10/12/13 at sequence15, fixed offsets
2724/2728/2730/2734 and values20000000/0/0/0 from pinned Linuxv6.18
MT7610 initialization. Require eleven prior phases and MAC/CORE guards;
v22 limit12, prior limits unchanged. This exhausts identifiers: compile-time
plan bound and runtime refusal forbid phase12/sequence16. Touch fixed MCU
plan, xHCI limits, capture/profile/tests. Fetched maind3670fe: Air6,1
exact-profile PWM capture and preference evidence inspected; PR76 owns
Home services/cached battery UI. No Wi-Fi overlap; retain base22fd266.
No RF or identifier recycling. Remaining setup needs validated wider batches
or a proven-drained new session before more commands can be added.

TXBE9/10/12/13 candidate source5d5488c / release9cc1888913aa7ff5
installed unarmed. Build, compact/MCU/xHCI host tests, sanitizer,78 boot-profile
tests and docs checks pass, including identifier exhaustion refusal. Exact-stage
GPT VM saved/unmounted/reset; missing-workspace VM returned without probing.
Payload, receipts, run record and collector archived outside build in
wifi-txbe-next-5d5488c. Accepted6477738b1d3cf7d5 and desktop recovery
e47dca3a6b30c6c1 retained. Obsolete492306b5fccc17b9 removed from ESP
only after archive equality verification. Workspace clean before install:
181files/520clusters, zero allocation errors, unmounted. Native acceptance
pending cold adapter power cycle; Ubuntu remains default.

TXBE9/10/12/13 native trial FAILED readback: source5d5488c,
release9cc1888913aa7ff5, session00065e85ec8dd3b8-00. Earlier eleven
phases and44 readbacks passed. Phase11 sequence15 sent40/received16 with
exact first-entry reply and ack_mask1; TXBE9 read0->20000000. TXBE10
read37dff before and38e30 after, not requested0. Engine returned status4
(MT76_TIMING_STATE), reads8, batches_completed0, complete0. TXBE12/13
and final MAC/CORE reads were not reached; their zero log fields are not
observations. Both capture completion markers are NO. No native acceptance
of phase11, no retry/recycled identifier. Failure logs archived outside build
in wifi-txbe-next-5d5488c/failed-native-collection. Ubuntu default, consumed
one-shot and installed payload/recovery verified. Workspace clean:
183files/523clusters, zero leaks/allocation mismatches; unmounted.
Run/checkpoint collected failed/unarmed. Next bounded read-only TXBE10
sampling to distinguish changing status/counter behavior from a write failure;
keep strict gates until evidence establishes semantics. No new reboot armed.

Active scope: read-only TXBE9/10/12/13 observation after the eleven accepted
compact phases. Eight fixed samples bracketed by MAC/CORE guards, no phase11
submission, retry or relaxed write verification. Touch pure MCU diagnostic,
xHCI ownership, capture/profile and tests. Main83a3f59 inspected (Air6,1
brightness documentation); PR76 Home services and PR79 Air6,1 brightness
remain separate. Integrated base22fd266. Native register semantics unknown.

Read-only TXBE observation candidate sourceffa4e17 / release70e0236472c320bc
installed unarmed. Build, compact/MCU/xHCI tests, sanitizer,79 boot-profile
checks and docs checks pass. Collector accepts a synthetic eight-sample receipt
and rejects nine incomplete/corrupt variants; this is not native evidence.
Exact-stage GPT VM saved/unmounted/reset; missing-workspace VM returned without
probing. Payload, receipts, run record and collector archived outside build in
wifi-txbe-observe-ffa4e17. Desktop recoverye47dca3a6b30c6c1 retained;
failed9cc1888913aa7ff5 remains installed for comparison. Accepted6477738b1d3cf7d5
removed from ESP only after archive equality verification. Workspace clean:
183files/523clusters, zero allocation errors, unmounted. Native observation
pending cold adapter power cycle; Ubuntu default and no reboot armed.

Read-only TXBE capture completed: sourceffa4e17, release70e0236472c320bc,
session00065e86348e7c75-00. All eleven compact phases/44 readbacks passed;
observation status0 attempted1 complete1 reads64 samples8. TXBE10 stayed
00037fff over2823351..2825238us (1.887ms); TXBE9/12/13 stayed0. MAC0 and
COREf000f200 held at all sixteen sample boundaries. Exactly eleven compact
commands; no phase11/sequence15. Both completion markers passed. No evidence
of spontaneous change in this short window; register semantics and the earlier
write mismatch remain unresolved, and phase11 is not accepted. Next audit
register mapping and write/read transport before choosing a further write trial.
Evidence archived in wifi-txbe-observe-ffa4e17/native-collection outside build.
Ubuntu default, consumed one-shot, installed payload and desktop recovery
verified. Workspace clean:185files/526clusters, zero leaks/mismatches/errors;
unmounted. Run/checkpoint collected and unarmed. RF/connectivity not tested.

TXBE mapping/transport audit completed without kernel or installed-payload changes.
Pinned Linux v6.18 definitions independently reconstruct all40 phase11 bytes;
USB request/index/value and MCU base/little-endian pairs agree. Compact/MCU tests
pass. No mapping defect found; Linux completion checking does not establish exact
readback semantics. Selected next bounded scope is TXBE9-only sequence15 with
before/after guarded samples after phases0..10, leaving TXBE10/12/13 unwritten.
Implementation and native evidence pending; do not relax failed v22 verification.
Fetched mainab99517 (merged Air6,1 brightness PR79); inspected shared staging
changes, preserve v56/basic brightness on future integration. PR76 remains Home
services/battery, PR19 owns Wi-Fi. Actual integrated base22fd266 unchanged.
Local audit receipts in build/bringup/txbe-audit, archived outside build.
Current70e0236472c320bc and recoverye47dca3a6b30c6c1 unchanged; no boot armed.

Active scope: implement TXBE9-only comparison after eleven accepted phases,
eight guarded samples on either side of one sequence15 write. No writes to
TXBE10/12/13, no identifier reuse, no v22 gate changes. Touch MCU/xHCI,
main capture, staging and tests. Mainab99517/PR76 inspected in preceding audit;
actual integrated base22fd266. Shared Air desktop profiles stay separate.

TXBE9-only comparison sourcef1785c6 / releaseb3a29375623082db installed
unarmed. Build, MCU/compact/xHCI tests, sanitizer,80 boot-profile tests and docs
checks pass. Collector validated with synthetic16 samples and12 rejection
fixtures. Exact-stage GPT VM saved/unmounted/reset; missing-workspace VM returned
without probing. Payload/receipts/run/collector archived outside build in
wifi-txbe-first-f1785c6. Accepted observation70e0236472c320bc and desktop
recoverye47dca3a6b30c6c1 retained. Failed9cc1888913aa7ff5 removed from ESP only
after archive equality verification. Workspace clean:185files/526clusters,
zero leaks/mismatches/errors; unmounted. Ubuntu default, no reboot armed.
Native result pending cold adapter power cycle; collect matching session first.

TXBE9-only comparison accepted on captured adapter: sourcef1785c6,
releaseb3a29375623082db, session00065e87131ed81c-00. Exact sequence15 reply,
sent16/received16, status0 attempted1 write_attempted1 submitted1 ACK1 complete1.
Both stages completed64 reads/eight samples. Before: TXBE9=0, TXBE10=37fff
through2836302..2839177us. After: TXBE9=20000000, TXBE10=38e30 through
2839544..2843407us. TXBE12/13 stayed0; all MAC0/COREf000f200 boundaries held.
Eleven compact phases/44 readbacks and both completion markers passed. The
TXBE9-only write reproduced the failed quartet's final TXBE10 value without
writing TXBE10; its semantics and original write result remain unresolved.
Next isolate TXBE10=0 with TXBE9 unchanged, retaining strict v22 verification.
Evidence archived outside build in wifi-txbe-first-f1785c6/native-collection.
Ubuntu default, consumed one-shot, installed payload/recovery verified. Workspace
clean:187files/529clusters, zero leaks/mismatches/errors; unmounted. Run/checkpoint
collected and unarmed. No further trial installed; RF/connectivity not tested.

Active scope: TXBE10-only zero write after phases0..10, sequence15 once,
TXBE9 required0 before/after, guarded samples and strict zero readback. No
phase11 quartet or retry. MCU/xHCI/main/staging/tests; integrated base22fd266.
Previous mainab99517/PR76 audit remains the coordination baseline. Native pending.
Fetched mainb99bb5b: merged PR76 Home services/battery UI and Air6,1 audio
changes inspected. They do not change this isolated Wi-Fi transport. PR83 owns
attended audio/Ethernet/secondary-input acceptance. Preserve desktop v55/56/57
and service dispatch on future integration; retain actual base22fd266 for this
comparison against the accepted captures.

TXBE10-only candidate sourcec96e810 / releasef8b4b41d736bd4e2 installed
unarmed. Build, compact/MCU/xHCI tests, sanitizer,81 boot-profile tests and docs
checks pass. Collector accepts synthetic16 samples and rejects14 corrupted
fixtures, including nonzero target readback and changed TXBE9. Exact-stage GPT
VM saved/unmounted/reset; missing-workspace VM returned without probing.
Payload/receipts/run/collector archived outside build in wifi-txbe-second-c96e810.
Accepted TXBE9 comparisonb3a29375623082db and desktop recoverye47dca3a6b30c6c1
retained. Observation70e0236472c320bc removed from ESP only after archive equality
verification. Workspace clean:187files/529clusters, zero allocation errors;
unmounted. Ubuntu default, no reboot armed. Await cold adapter power cycle;
collect matching session first and preserve failed readbacks as failures.

TXBE10-only comparison collected with a failed readback: sourcec96e810,
releasef8b4b41d736bd4e2, session00065e87382297d7-00. All eleven compact
phases/44 readbacks passed; sequence15 sent16/received16 with exact reply and
ACK1, but status4 complete0. Both stages completed64 reads/eight samples.
TXBE10 remained37fff before2867128..2873127us and after2873493..2875377us;
TXBE9/12/13 stayed0, all MAC0/COREf000f200 boundaries held. Both capture
completion markers NO. Unlike the TXBE9-only comparison, this isolated zero
write produced no observed TXBE10 change. Register semantics versus write/access
behavior remains unresolved; no relaxed gate or further initialization accepted.
Next scope is auditing that distinction before selecting another hardware trial.
No shared source changes in this collection. RF/connectivity not tested.
Evidence and failure-specific collector archived outside build in
wifi-txbe-second-c96e810/failed-native-collection. Ubuntu default, consumed
one-shot, installed payload and desktop recovery hashes verified. Workspace
clean:189files/532clusters, zero leaks/mismatches/errors; unmounted.
Run/checkpoint collected with failure recorded and no reboot armed.

TXBE10 access-path audit completed: Linux v6.18 MCU and vendor USB transport
agree with the compiled address/byte encoding; an older vendor-source mirror
corroborates the numeric table entry but supplies no readback semantics. Host
independent packet reconstruction and MCU/compact regressions pass. Receipts
in txbe-access-audit, archived outside build under wifi-txbe-second-c96e810.
Selected next bounded scope: one command10 TXBE10 MCU read at sequence15 after
phases0..10, bracketed by guarded EP0 observations, without a diagnostic write,
retry, identifier reuse or relaxed initialization gate. Native result pending;
no candidate installed in this audit. Currentf8b4b41d736bd4e2 and desktop
recoverye47dca3a6b30c6c1 unchanged, no reboot armed. Mainb99bb5b and PR83
attended-device scope rechecked; actual integrated base22fd266 unchanged.

Active scope: implement the selected MCU-versus-EP0 TXBE10 read comparison.
One command10 at unused sequence15, guarded before/after observations, exact
response framing and address, explicit mismatch reporting. Touch MCU/xHCI,
capture/profile and tests; no diagnostic register write or phase11 acceptance.
Dependencies remain the eleven accepted phases and pinned firmware. Current
mainb99bb5b/PR83 scope inspected; integrated base22fd266 retained for comparison.

MCU/EP0 TXBE10 comparison sourceb4417eb / releaseeb362bcf6da9fdfb installed
unarmed. Build, MCU/compact/xHCI tests, ASan/UBSan,82 boot-profile tests and docs
checks pass. Collector accepts synthetic agreement and rejects14 corrupted or
conflicting receipts. Exact-stage GPT VM saved/unmounted/reset; missing-workspace
VM returned without starting the probe. Payload, receipts, run record and collector
archived outside build in wifi-txbe-access-b4417eb. Desktop recoverye47dca3a6b30c6c1
and failed v25f8b4b41d736bd4e2 retained. Accepted TXBE9 comparisonb3a29375623082db
removed from ESP only after full archived-payload equality verification.
Workspace clean:189files/532clusters, zero allocation errors; unmounted.
Ubuntu remains default. Native comparison awaits cold adapter power cycle;
collect the matching new session first on return. No reboot armed.

Native MCU/EP0 TXBE10 comparison accepted: sourceb4417eb,
releaseeb362bcf6da9fdfb, session00065e876ba24dc3-00. Sequence15 exact16-byte
command/reply, status0 reply_validated1 complete1; MCU value37fff matched all16
EP0 samples,64 reads/eight samples per side. Before2882205..2885095us,
after2885446..2889299us; TXBE9/12/13 stayed0 and all MAC0/COREf000f200
boundaries held. Eleven prerequisite phases/44 readbacks and both completion
markers passed. This accepts only baseline read-path agreement; the v22/v25
write failures and RF/connectivity remain unaccepted. Next investigate write
behavior/register semantics before selecting another physical trial.
Evidence archived outside build in wifi-txbe-access-b4417eb/native-collection.
Ubuntu default, consumed one-shot and installed/recovery payload hashes verified.
Workspace clean:191files/535clusters, zero leaks/mismatches/errors; unmounted.
Run/checkpoint collected and unarmed. Fetched maind3875c7 (Air6,1 audio PR84);
actual integrated base22fd266 remains unchanged for these comparisons.

Active scope: isolate the TXBE10 write transport with one EP0 request6 zero
write after phases0..10 and guarded baseline samples, followed by one MCU read
at unused sequence15 and guarded EP0 samples. Exact address/value, single attempt,
no retry, TXBE9zero, MAC0/COREf000f200; nonzero readback remains failure.
Linux v6.18 usb.c confirms request6, local index2728, value0, four LE payload
bytes. Touch MCU/xHCI/main/profile/tests. Main nowd2d56a6 (Air6,1 audio work),
PR83 attended acceptance inspected; actual base22fd266 unchanged. No native
write or new candidate installed yet.

Isolated EP0 TXBE10 zero-write candidate sourcee821968 / releaseacaea56aaa88d52f
installed unarmed. Build, compact/MCU/xHCI tests, ASan/UBSan,83 boot-profile
tests and docs checks pass. Collector accepts synthetic zero readback and rejects17
corrupt/conflicting receipts. Exact-stage GPT VM saved/unmounted/reset;
missing-workspace VM returned without starting the probe. Payload, receipts,
run record and collector archived outside build in wifi-txbe-direct-e821968.
Accepted read comparisoneb362bcf6da9fdfb and desktop recoverye47dca3a6b30c6c1
retained. Failed v25f8b4b41d736bd4e2 removed from ESP only after full archived
payload equality verification. Workspace clean:191files/535clusters, zero
allocation errors; unmounted. Ubuntu default and no reboot armed. Await cold
adapter power cycle; collect the matching new session first on return.

Native direct TXBE10 zero-write readback failed: sourcee821968,
releaseacaea56aaa88d52f, session00065e87a87d976c-00. EP0 index2728 zero transfer
completed actual4; sequence15 MCU read returned37fff with exact framing/address.
Eleven prerequisite phases/44 readbacks passed. Comparison status4 complete0,
reply_validated1; both stages64 reads/eight samples. Before3106897..3108802us
and after3109221..3115210us, TXBE10 stayed37fff; MCU matched all eight post-write
samples. TXBE9/12/13 remained0, all MAC0/COREf000f200 boundaries held. Both
capture completion markers NO. A second write path reproduced the mismatch;
transport completion does not accept the register write. Next audit semantics
and initialization prerequisites; no relaxed gate, RF or phase11 acceptance.
Evidence archived outside build in wifi-txbe-direct-e821968/failed-native-collection.
Ubuntu default, consumed one-shot and installed/recovery hashes verified.
Workspace clean:193files/538clusters, zero leaks/mismatches/errors; unmounted.
Run/checkpoint collected with failure recorded; no reboot armed.

Post-v27 prerequisite audit found missing PBF/MAC clock setup: Linux v6.18
sets0400=80c00 before baseband initialization; current code only observes this
register. Five native v23..v27 readiness receipts consistently show80000,
missing bits11:10 (0c00), with MAC_SYS_CTRL0. Existing CORE readiness does not
establish clock enablement. Receipt/script/source and log hashes archived in
txbe-prerequisite-audit; reference ordering and all five receipts verified.
Causality remains unproven and other MAC-table omissions remain separate.
Selected next scope: exact80000->80c00 EP0 transition after MAC phases0/1,
before baseband readiness, with disabled/idle MAC checks, one attempt and exact
readback; then existing guarded phases and read-only TXBE10 comparison. No new
TXBE write, MCU sequence reuse, relaxed CORE gate or RF enable. Implementation
pending. No installed payload changed; currentacaea56aaa88d52f and recovery
e47dca3a6b30c6c1 retained, no reboot armed. Maind2d56a6 and PR83 attended-device
scope inspected; integrated base22fd266 unchanged. Shared implementation scope
would be MCU/xHCI/main/profile/tests; current change is audit documentation only.

Active implementation scope: opt-in PBF/MAC clock prerequisite before compact
phase2, followed by the existing read-only TXBE10 comparison. Exact baseline,
fixed one-time EP0 write and readback, MAC disabled/idle before and after;
no TXBE write or change to historical profiles. Radio/MCU/xHCI/main/staging/tests.
Fetched maind2d56a6; PR83 remains attended acceptance. Actual base22fd266.

Clock prerequisite candidate source1b3e323 / release67f92f9c2a4d9365 installed
unarmed. Build, radio/compact/MCU/xHCI tests, ASan/UBSan,84 profile tests and docs
checks pass. Collector accepts synthetic clock/read agreement and rejects17
corrupted, missing or out-of-order receipts. Exact-stage GPT VM saved/unmounted/
reset; missing-workspace VM returned without starting the probe. Payload,
receipts, run record and collector archived outside build in wifi-clock-1b3e323.
Failed direct-writeacaea56aaa88d52f and desktop recoverye47dca3a6b30c6c1 retained.
Accepted read comparisoneb362bcf6da9fdfb removed from ESP only after full archived
payload equality verification. Workspace clean:193files/538clusters, zero
allocation errors; unmounted. Ubuntu default, no reboot armed. Native clock
transition and subsequent comparison await a cold adapter power cycle; collect
matching session first after return. Existing TXBE write failures remain failures.


Native clock prerequisite accepted: source `1b3e323`, release
`67f92f9c2a4d9365`, session `00065e87eb5deca5-00`. Clock status0 complete1,
six reads, PBF80000->80c00, MAC disabled/idle before and after; readiness
retained80c00. Eleven compact phases/44 target readbacks passed. Sequence15
MCU read matched all16 EP0 samples at TXBE10=37fff; TXBE9/12/13 stayed0,
MAC0/COREf000f200 guards held. Before2866911..2872953us,
after2873320..2876195us. Both completion markers YES. This accepts the clock
prerequisite and read comparison only; no TXBE write or RF/connectivity acceptance.
Next bounded scope is an isolated TXBE10 zero-write comparison with clocks enabled,
keeping the exact zero-readback gate and historical failures intact.

Evidence archived outside build in wifi-clock-1b3e323/native-collection.
Ubuntu default, consumed one-shot and installed/recovery payload hashes verified.
Workspace clean:195files/541clusters, zero leaks/mismatches/errors; unmounted.
Run/checkpoint collected and unarmed. Fetched maindf44fa0 (Air6,1 audio endurance);
PR87 telemetry and PR83 attended acceptance remain separate scopes. Actual
integrated base22fd266 unchanged. No new candidate staged or reboot armed.

Active scope: separate v29 clock-plus-direct-write comparison, reusing the accepted
clock engine and isolated EP0 TXBE10 writer. Require clock completion before
access; one zero write, sequence15 MCU read and eight guarded post-write samples
must read zero. Touch xHCI/main/profile/tests; dependencies are the accepted
v28 prerequisite and eleven compact phases. Main df44fa0, PR83/87 inspected;
actual base22fd266 retained for this comparison. No reboot armed.

Clock-enabled isolated TXBE10 write candidate source `492fb20`, release
`19f40e64c606d997` installed unarmed. Build, radio/compact/MCU/xHCI tests,
xHCI ASan/UBSan, 85 boot-profile tests and documentation checks passed. The
collector accepts synthetic zero readback and rejects21 missing/corrupted or
conflicting receipts, including clock failures. Exact-stage GPT VM saved,
unmounted and reset; missing-workspace VM returned without starting the probe.
Payload, receipts, helpers and run record archived outside build in
wifi-clock-direct-492fb20. Accepted clock67f92f9c2a4d9365 and desktop recovery
e47dca3a6b30c6c1 retained. Failed direct-writeacaea56aaa88d52f removed from ESP
only after complete archived-payload equality verification. Workspace clean:
195files/541clusters, zero allocation errors; unmounted. Ubuntu remains default.
Await cold adapter power cycle; collect the exact new session first on return.
No reboot armed, no native write acceptance yet.


Native clock-enabled TXBE10 zero-write failed: source `492fb20`, release
`19f40e64c606d997`, session `00065e8821c93f58-00`. Clock status0 complete1,
PBF80000->80c00 retained at readiness; eleven phases/44 readbacks passed.
EP0 zero-write transport completed actual4. Sequence15 exact MCU reply returned
37fff; comparison status4 complete0 reply_validated1, matches8 expected8.
All16 EP0 samples retained37fff; TXBE9/12/13 zero, MAC0/COREf000f200 held.
Before3096597..3100465us, after3100884..3104735us. Both completion markers NO.
Clock enablement did not resolve this mismatch. Next audit remaining prerequisites
and register semantics; do not relax zero-readback or accept phase11/RF.

Failure archived outside build in wifi-clock-direct-492fb20/failed-native-collection;
separate failure collector retains the original success predicate, which rejects
this receipt. Ubuntu default, consumed one-shot and installed/recovery hashes
verified. Workspace clean:197files/544clusters, zero leaks/mismatches/errors;
unmounted. Run/checkpoint collected as failed and unarmed. Fetched main36dbc3d
(Air6,1 telemetry); PR88 two-finger tap scope is separate. Actual integrated
base22fd266 retained; no new candidate or reboot armed.


Post-v29 audit captured eight read-only Linux observations from this exact
7392:a711 adapter through mt76x0u debugfs. Kernel7.0.0-38-generic: TXBE10=0,
TXBE9=20000000, PBF80c00, COREf000f200, MAC0c; TXBE12/13zero. MAC_STATUS0
except one2. Selector restored; no register write, reset, unbind or connection
change. Fully running Linux is not equivalent to SrvrOS disabled initialization.
Numeric v6.18/v7.0 MAC/BBP tables match; TXBE9 precedes TXBE10. Additional
MAC-table/EXT_CCA/FCE/WMM setup remains absent, with causality unproven.

Next bounded implementation: accepted clocks plus TXBE9=20000000 followed by
TXBE10=0, retaining disabled MAC and strict readback. v22 lacks clocks; v29
keeps TXBE9zero, so neither tests this combination. Historical failures stand.
Audit script checks eight Linux observations and six native trials; script,
source hashes and raw receipt archived outside build in txbe-linux-audit-9d22c95.
Current19f40e64c606d997/recoverye47dca3a6b30c6c1 unchanged; no reboot armed.
Fetched main36dbc3d and inspected separate PR88 gesture scope. Integrated
base22fd266 unchanged; this work changes audit documentation only.

Active implementation scope: explicit clock-enabled TXBE9/TXBE10 ordered pair.
Reuse compact phases0..10, accepted clocks before BBP, then one command12
sequence15 containing only2724=20000000 followed by2728=0. Require both target
readbacks and MAC0/COREf000f200 before/after. No TXBE12/13 writes, sequence
reuse or RF enable. Touch MCU/xHCI/main/staging/tests; depend on accepted v28
clock setup and retain historical profiles. No reboot armed.
Fetched mainf310923 (Air6,1 telemetry acceptance); open PR90 passive battery
preflight and PR88 gestures are separate scopes. Actual integrated base22fd266.

Clock-enabled ordered-pair candidate source `0754633`, release
`29504434a7bc5aa6` installed unarmed. Build, radio/MCU/MAC/compact/xHCI tests,
compact ASan/UBSan, 86 boot-profile tests and documentation checks passed.
The collector accepts synthetic pair completion and rejects15 corrupt/conflicting
receipts. Exact-stage GPT VM saved/unmounted/reset; missing-workspace VM returned
without starting the probe. Payload, receipts, helpers and run record archived
outside build in wifi-clock-pair-0754633. Failed clock-direct19f40e64c606d997
and desktop recoverye47dca3a6b30c6c1 retained. Accepted clock67f92f9c2a4d9365
removed from ESP only after complete archived-payload equality verification.
Workspace clean:197files/544clusters, zero allocation errors; unmounted.
Ubuntu remains default. Await cold adapter power cycle; collect the exact new
session first on return. No reboot armed or native ordered-pair acceptance yet.


Native clock-enabled ordered pair failed: source `0754633`, release
`29504434a7bc5aa6`, session `00065e8878bfdf27-00`. Clock80000->80c00 and
readiness passed; phases0..10/44 target readbacks passed. Phase11 sequence15
sent24 received16 exact first-pair ACK. TXBE9 before0 after20000000; TXBE10
before37fff after38e30 instead of0. Status4 ack1 batches_completed0 complete0
reads6. Final MAC/CORE guards not reached; their zero fields are not evidence.
Both completion markers NO. Next investigate remaining initialization/state
differences from Linux; no relaxed gate or TXBE12/13/RF acceptance.

Failure archived outside build in wifi-clock-pair-0754633/failed-native-collection.
Separate failure collector verifies this receipt; unchanged success checker rejects
it. Ubuntu default, consumed one-shot and installed/recovery hashes verified.
Workspace clean:199files/547clusters, zero leaks/mismatches/errors; unmounted.
Run/checkpoint collected as failed, no reboot armed. Fetched maind5491c0
(Air6,1 battery preflight); PR88 tap diagnostics remains separate. Actual
integrated base22fd266 unchanged; no new candidate installed.


Post-v30 state audit: Linux sends the58-entry BBP table as24/24/10 pairs, only
last chunk acknowledged. TXBE10 is index37, followed by20 entries; upstream does
not perform our immediate target readback. Two read-only live snapshots of66
registers confirm TXBE10zero and nine runtime values differing from the initial
BBP table. Live Linux MAC0c/channel/calibration state differs from SrvrOS; no
causal claim or acceptance follows. Selector restored, no device writes/reset.

Selected next scope is a dedicated full-table observation diagnostic following
accepted MAC phases/clocks, fixed200/200/88-byte packets with seq0/0/6, final ACK
and post-table snapshots. Keep disabled-MAC/CORE guards, no RF/channel/MAC enable,
private fixed-packet no-response admission, explicit mismatch accounting and
historical strict tests unchanged. Do not equate snapshot completion with target
acceptance. MAC-table omissions remain uncontrolled. Implementation is pending.
Audit and proposed unsent packet fixtures archived in txbe-state-audit-e829cf1
outside build. Current29504434a7bc5aa6/recoverye47dca3a6b30c6c1 unchanged;
no reboot armed. Fetched main9bbc3b9 (Air6,1 preflight return), inspected separate
PR88 tap diagnostics; actual integrated base22fd266 retained. Documentation only.

Active scope: implement the separate full-BBP observation diagnostic with fixed
24/24/10 entry packets, seq0/0/6 and only final ACK. Snapshot all58 targets with
MAC0/COREf000f200 boundaries; mismatches are reported separately from completed
capture and never imply accepted Wi-Fi initialization. No RF/MAC enable. Touch
MCU/xHCI/main/staging/tests; reuse accepted reset/timing/MAC/clock prerequisites.

Implementation uses only MAC phases0/1 before clocks/readiness, then three full-table
commands and124 reads. Latest fetched main9bbc3b9 remains outside actual base22fd266.
PR92 Air6,1 battery sampling and PR88 tap diagnostics are separate active scopes.
Local packet/collector/sanitizer checks pass; exact-stage validation pending.

Full-table observation candidate installed: source `eb832da`, release
`4316c9963cb7ae6a`, firmware profile v31. Host build, MCU/compact/radio/MAC/xHCI
regressions, 87 staging tests, sanitizer fault coverage, independent488-byte
packet comparison and collector mismatch/rejection fixtures pass. Exact-stage
GPT absent-radio save/unmount/reset and missing-workspace return VM checks pass.
Physical table results remain pending; capture completion will not accept register
values or RF/connectivity. Candidate and evidence archived outside build in
wifi-bbp-table-eb832da. Previous known-good desktope47dca3a6b30c6c1 retained;
Ubuntu default unchanged. Workspace preflight fsck/allocation clean, unmounted.
Run record and strict observation collector ready. No one-shot armed; await cold
adapter power cycle, then collect matching session before further changes.

Native full-table observation completed: source `eb832da`, release
`4316c9963cb7ae6a`, session `00065e88f3984844-00`. Firmware/reset/timing,
MAC phases0/1, clock/readiness, all three200/200/88-byte commands (seq0/0/6)
and final ACK passed. All124 reads and four disabled-MAC/COREf000f200 boundaries
completed. Both capture markers yes; this accepts the observation workflow only.
Four mismatches remain: TXBE10/2728 before37fff after38e30 expected0;
TXBE17/2744 before/afterfb3fec3c expected0; RXO21/2954 before/after0 expected1;
RXO24/2960 before/after0 expected6. Other54 table values matched. Full-table
batching did not resolve TXBE10. Register initialization and RF/connectivity
remain unaccepted; no cause is established by this comparison.

Matching logs archived in wifi-bbp-table-eb832da/native-collection outside build.
Collector checks passed after fixing a local Python variable that shadowed the
hardware_boot module; no evidence predicates changed. Ubuntu default, consumed
one-shot and current/recovery hashes verified. Workspace clean:201files/550clusters,
zero leaks/mismatches/errors, unmounted. No reboot armed. Next audit the four
mismatching registers and remaining reference initialization prerequisites before
choosing another bounded trial; retain all earlier strict failure results.

Active audit after v31: compare the four mismatches with pinned Linux v7.0
initialization, later BBP tables and saved live-driver observations; inventory
omitted MAC prerequisites before selecting another native write trial. Scope is
source/evidence analysis and local numeric table fixtures, no device reset/write.
Fetched main90a1db8 includes PR92 Air6,1 isolated battery runtime; actual integrated
base22fd266 unchanged. PR88 tap diagnostics remains separate. Current/recovery
and unarmed boot state remain unchanged.

Post-v31 audit complete: RXO21/24 also read0 in both saved running-Linux samples;
TXBE17 readsff3fef3c in Linux versusfb3fec3c here, and TXBE10 reads0 versus38e30.
Neither later band-switch table nor DCOC table explicitly writes those four
addresses. This does not establish register semantics or eliminate indirect
calibration/state effects. Pinned Linux v7.0 MAC tables contain66 ordered writes
to60 distinct addresses; current reset/timing/MAC/clock setup matches the final
values at12 addresses, leaving48 table addresses unapplied. This is an inventory,
not a causal finding or authorization to blindly write power/PA/LDO settings.

Next bounded diagnostic selected: read-only prerequisite inventory of the60 MAC
table addresses plus EXT_CCA/FCE_L2/WMM before the full BBP observation, retaining
MAC-disabled/CORE guards. Use observed initial values to choose a justified write
set. No new candidate installed or reboot armed. Audit scripts, numeric C-extracted
MAC inventory, exact source copies and result hashes archived outside build in
bbp-mismatch-audit-47ce3d4. No live device writes/reset/unbind performed.

Implementation scope: separate v32 MAC snapshot profile before the existing full
BBP observation. Fixed63-address read allowlist,67 reads including boundary guards,
no write/exchange callback, cancellation/error/no-retry tests. BBP commands remain
blocked until snapshot completion; existing v31 behavior stays separate. Touch
MCU/xHCI/main/staging/tests and documentation; no new RF/channel/PA/LDO writes.

Fetched mainb17b80e (PR93 Air6,1 bounded battery return) and inspected PR88 tap
scope; neither is incorporated into the actual22fd266 base for this experiment.
The snapshot candidate adds no shared battery/input changes. Local staging88
cases and initial build/controller/MCU checks pass; final tests/VM pending.

MAC prerequisite snapshot installed: source `f3ba99c`, release
`b7a5985c12609971`, firmware profile v32. Build, MCU/xHCI fault/refusal/ownership
regressions,88 staging tests, ASan/UBSan, collector success/seven mutations and
docs checks pass. Exact-stage GPT absent-radio save/unmount/reset and
missing-workspace automatic return VM checks pass. Physical acceptance pending.
Candidate, VM receipts, collector/fixture and test logs archived outside build in
wifi-mac-snapshot-f3ba99c; run record saved. Ubuntu default and recovery
e47dca3a6b30c6c1 verified. Workspace201files/550clusters, zero allocation errors,
clean fsck and unmounted. No boot armed; await cold adapter cycle. On return,
collect the matching new session first, requiring complete63-value inventory and
67 reads before interpreting the unchanged full-BBP observations.

Native v32 snapshot incomplete: source `f3ba99c`, release `b7a5985c12609971`,
session `00065e892754f5ff-00`. Firmware/reset/timing, MAC phases0/1 and clock/BBP
readiness passed. Snapshot status4 complete0 reads37 writes0: initial MAC/CORE
checks plus35 target reads; target index34/address1030 returnedffffffff and the
all-ones policy stopped capture. Final guards and targets35..62 were not read;
their printed zero fields are not observations. No BBP table command ran and
both completion markers are NO. This is failed/incomplete evidence, not a pass.

The preceding34 valid targets include23 differences from final reference MAC
table settings: PBF_CFG0404=3fdc1cf5 vs77723c1f, WPDMA0208=40000050 vs30,
FCE_PSE0800=5 vs1. These support the missing-prerequisite inventory but establish
no cause of TXBE behavior. Pinned source identifies1030 as AMPDU_MAX_LEN_20M1S
and writesaaa99887; it does not document its reset read value. Do not infer a USB
transport failure or a legitimate reset value fromffffffff alone. Next distinguish
returned register data from transport validity with independent checks before
changing the snapshot admission policy or writing missing settings.

Separate failure collector archived the exact prefix and failure; success checker
unchanged. Ubuntu default, consumed one-shot and payload/recovery hashes verified.
Offline workspace clean:203files/553clusters, zero allocation errors, unmounted.
Evidence and partial comparison retained outside build in
wifi-mac-snapshot-f3ba99c/failed-native-collection. No new candidate or reboot armed.

Post-v32 AMPDU read audit: four read-only Linux samples show1030=aaa99887,
ASIC76502000, COREf000f200 and enabled MAC0c. Linux initialization writesaaa99887,
so this is not evidence of reset semantics. Debugfs selector restored; no register
writes, reset or unbind. Source audit confirms Srvr snapshot callback checks
successful control completion plus actual4 bytes before decoding; reads37/status4
therefore indicates rejection of returnedffffffff by our value policy, not its
transport-IO failure path. Individual transfer receipts were not logged, so this
does not rule out a driver defect or prove a valid reset value. Linux's USB read
API can itself return~0 on failed transfer, another reason not to infer semantics
from debugfs all-ones data alone.

Next scope selected: separate raw read-only inventory with explicit transfer
receipts, repeated ASIC/MAC/CORE guards and1030 read; retain all-ones only as
unvalidated observations and perform no subsequent BBP writes. Preserve v32's
strict stop and historical failures. Audit/capture/source hashes retained outside
build in ampdu-read-audit-6437ed0. Fetched mainbd55dd5 (PR94 Air6,1 failure display)
but actual base22fd266 unchanged. No candidate installed or reboot armed.

Active implementation: separate v33 raw read-only MAC inventory after the accepted
firmware/MAC/clock prerequisites. Log per-read USB completion, actual length and
residue, repeat ASIC/MAC/CORE guards and1030; record all-ones only as unvalidated
observations. No subsequent BBP table commands are admitted in this mode. Keep
v32 strict refusal behavior. Touch MCU/xHCI/main/staging/tests; no new RF or power
writes. Physical acceptance pending.

Fetched main6e608b0 (PR95 Air6,1 display return), inspected PR96 HPTC and PR88 tap
scopes; actual integrated base22fd266 remains unchanged for this experiment.
Initial build/MCU/xHCI regressions and89 staging cases pass. Separate collector
accepts raw all-ones observations only with70 exact successful transfer receipts,
all63 ordered values, repeated identity/guards and no BBP command; ten mutation
cases reject incorrect/incomplete evidence. No physical v33 result yet.

Raw MAC observation candidate installed: source `139f82c`, release
`2d2630e3bbbbe8d8`, profile v33. Build, MCU/xHCI fault and write-refusal tests,
89 staging cases, sanitizer checks, collector success/ten rejection cases and
docs checks pass. Exact-stage GPT save/unmount/reset and missing-workspace return
VM checks pass. Native results pending; no interpretation of all-ones as valid
settings is accepted. Archive outside build: wifi-mac-raw-139f82c, with payload,
VM receipts, collector/fixture and tests. Saved run record; Ubuntu default and
known-good e47dca3a6b30c6c1 recovery verified. Workspace203files/553clusters clean,
unmounted. No boot armed; await cold Wi-Fi cycle, then collect exact session first.

Native v33 raw capture complete: source `139f82c`, release `2d2630e3bbbbe8d8`,
session `00065e894bcf6265-00`. All70 ordered control transfers report ok1 actual4
cc1 residue0 timeout0. Initial/final ASIC76502000, MAC0 and COREf000f200 match;
all63 target values captured.1030 returnedffffffff on both reads (all_ones2).
Both completion markers yes. No BBP table command ran. This accepts the bounded
observation workflow, not register initialization, reset semantics or connectivity.
It supports repeated returned data rather than an observed USB transfer error;
no claim of independent bus-analyzer validation follows. v32 remains incomplete.

Comparison:16 of60 final reference MAC table values match,44 differ. Additional
controls: EXT_CCA141c=ffe4, FCE_L2_STUFF080c=3ff0233, WMM_CTRL0230=41040.
PBF_CFG0404=3fdc1cf5 and WPDMA0208=40000050 retain the earlier observed differences.
The inventory is now complete. Next select and audit a bounded MAC transport/
format prerequisite initialization set, using the captured starting state and
reference ordering, before returning to BBP comparison. No causal explanation of
TXBE10 follows yet; do not blindly enable omitted power/PA/LDO settings or RF.

Collector verified exact transfer/order/value/guard evidence and no table writes.
Native logs and comparison.json archived in wifi-mac-raw-139f82c outside build.
Ubuntu default, consumed one-shot and installed/recovery hashes verified; workspace
clean205files/556clusters, zero allocation errors, unmounted. No reboot armed.

Active scope: bounded FCE/WMM initialization after accepted MAC phases0/1 and
clock/readiness. Captured080c=03ff0233 ->03ff0223 clears only bit4;0230=41040
->41201 replaces low10 bits with201, preserving the rest. Reference init.c
performs these operations in this order. Captured EXT_CCA already has f000 set.
Exact initial-value gates, unused seq6 two-pair packet, strict ACK/readbacks and
MAC0/COREf000f200 boundaries; no subsequent BBP table writes. Defer PBF/WPDMA,
RF/power/PA/LDO changes. Touch MCU/xHCI/main/staging/tests; current base22fd266,
fetched main9488193 (Air6,1 HPTC) remains separate. Native behavior pending.

### 2026-10-23 — v34 MAC routing candidate installed, unarmed

Source `7cc6fe3`, release `c929ce1b090b820d`: FCE/WMM fixed-pair trial described
above. Build, compact/xHCI host tests, ASan/UBSan, 90 boot-profile tests and docs
checks passed. Exact-stage GPT VM saved diagnostics, passed offline filesystem
checks and returned automatically; missing-workspace VM returned without probing.
The collector accepts exact command/ACK, eight reads and both target readbacks;
synthetic success and five failure mutations passed. Hardware acceptance pending.

Installed with Ubuntu default and known-good previous `e47dca3a6b30c6c1` unchanged.
Raw inventory release `2d2630e3bbbbe8d8` retained; obsolete strict snapshot removed
from ESP only after comparison with its durable archive. UUID `0E53-5900` passed
preboot fsck and allocation checks (205 files, 556 clusters, no leaks), then was
unmounted. No one-shot armed. Run record: `build/bringup/wifi-mac-route/run.json`;
collector: `build/bringup/collect_wifi_mac_route_return.py`. Payload, evidence and
helpers: `~/.local/share/srvros/macbookpro11-2/wifi-mac-route-7cc6fe3/`.
Next: user-ready cold adapter power cycle, automatic capture/return, then collect
matching session before further changes. RF/connectivity remains NOT_TESTED.


### 2026-10-23 — v34 native FCE/WMM trial passed

Collected release `c929ce1b090b820d` / source `7cc6fe3`, new session
`00065e89795928b3-00`. The strict prepared collector passed: sequence6 sent24,
received16, cc13/residue1008; exact four-word ACK; status0, complete1, eight reads,
one submitted/completed batch and ack_mask1. FCE `03ff0233 -> 03ff0223` and WMM
`00041040 -> 00041201` both matched. MAC0 and COREf000f200 remained unchanged at
both boundaries. Firmware/reset/timing, compact0/1, clocks and readiness passed.
Both capture completion markers were present; no BBP table/raw/snapshot ran.

Ubuntu default, consumed one-shot and installed/recovery payload hashes verified.
Workspace fsck clean (207 files), allocation check passed (559 clusters, no leaks
or mismatches), then left unmounted. Full collection and result/checkpoint are
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-route-7cc6fe3/` with
checksums. Local collection: `build/bringup/wifi-mac-route/collection/collection-5pta14ud`.
No reboot armed. Acceptance is limited to these two MAC register operations;
RF/connectivity and the earlier four BBP mismatches remain unresolved. Next work:
review remaining reference MAC initialization prerequisites against the v33 raw
inventory, then select a bounded addition before repeating full baseband setup.

Active scope (2026-10-23): bounded common MAC policy group after accepted v34
routing. Reference common_mac_reg_table order: TX_LINK1350=1020,
TX_TIMEOUT1348=a2090, TX_RETRY134c=47d01f0f, AUTO_RSP1404=13. Captured starting
values 7f0020/10f0a90/4bb80407/7 must match before submission. One sequence7
four-pair command, exact ACK and all readbacks, MAC0/COREf000f200 boundaries.
No BBP-table or RF enable; unrelated PBF/DMA/power settings remain deferred.
Touch MCU/xHCI/main/staging/tests; retain integration base22fd266. Fetched
main2e2eb2b (PR88 taps, PR97 Air6,1); PR99 log capacity and PR98 battery SMBus
have separate active scope. No overlapping Wi-Fi implementation found.

### 2026-10-23 — v35 common MAC policy candidate installed, unarmed

Source `78f1944`, release `ee842dc8bcc56669`. Build, compact/xHCI host tests,
ASan/UBSan, 91 boot-profile tests and docs checks passed. Exact-stage GPT VM saved
diagnostics, passed offline checks and reset; missing-workspace VM returned
without probing. Collector fixture success and eight failure mutations passed.
Hardware acceptance pending for the four-register group; v34 prerequisites remain
strict and are rechecked first.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved; accepted route
`c929ce1b090b820d` retained. Raw inventory payload removed from ESP only after
byte comparison with its durable archive. UUID0E53-5900 clean207files/559clusters,
no allocation errors, unmounted. No one-shot armed. Run/collector:
`build/bringup/wifi-mac-policy/run.json`, `build/bringup/collect_wifi_mac_policy_return.py`.
Durable payload/evidence/helpers: `~/.local/share/srvros/macbookpro11-2/wifi-mac-policy-78f1944/`.
Next: readiness-confirmed cold adapter power cycle, automatic capture/return,
then collect matching session before any new run. RF/connectivity NOT_TESTED.


### 2026-10-23 — v35 failed TX_LINK strict readback

Collected release `ee842dc8bcc56669`, source `78f1944`, session
`00065e89a75cfd3a-00`. Firmware/reset/timing, compact0/1, clock/readiness and v34
FCE/WMM passed. Policy sequence7 sent40/received16, cc13/residue1008, exact ACK
including first requested value1020. Native state: status4, attempted1,
submitted1, complete0, reads7, batches_attempted1/completed0, ack_mask1.
TX_LINK1350 before007f0020, requested00001020, observed007f1020. Upper bits
remained set; no cause inferred. Other three target writes were submitted but
their readbacks and final MAC/CORE guards were not reached. Zero fields are not
observations. Both capture completion markers NO; RF/connectivity NOT_TESTED.

Prepared success collector remains unchanged. Separate failure collector
`build/bringup/collect_wifi_mac_policy_failure.py` verifies this exact failure
and recovery state. Full matching collection archived under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-policy-78f1944/failed-native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean209files/562clusters, zero leaks/mismatches, unmounted. Run and
checkpoint marked collected failure, no reboot armed. Next: inspect TX_LINK
field semantics/reference behavior before relaxing exact readback or testing
another write; remaining policy registers are unobserved.

### 2026-10-23 — TX_LINK readback audit (no device-register writes)

Read four live Linux samples from this exact7392:a711 adapter, mt76x0u, with
original debugfs regidx restored. TX_LINK1350 consistently007f1020, identical to
v35's native post-write value. Desired low control bits match1020; native changed
bit12 while retaining007f0000. Linux v7.0 rt2800 definitions identify upper bits
as remote feedback fields, but that related-chip definition is not a proven
MT7610 writable mask. Evidence suggests the whole-word equality was too strict;
it does not justify silently accepting v35 or ignoring arbitrary upper bits.

Linux also reads timeout1348=a1990, retry134c=47d01f0f, AUTO_RSP1404=3. mt76x02
runtime code updates ACK timeout and preamble selection after initialization,
so these live values are not cold-init expected values. Native remaining three
readbacks/final guards remain unknown. No driver reset/unbind or register writes.
Audit sources/results/capture and hashes:
`~/.local/share/srvros/macbookpro11-2/mac-policy-audit-44c2616/`.

Next bounded scope: separate observational policy profile with unchanged command,
ACK and initial-value gates; read all four targets plus final MAC/CORE guards,
report mismatches without aborting solely on a target value, and explicitly keep
register acceptance NOT_TESTED. Preserve v35 strict behavior. No new image
installed or reboot armed. Fetched main249bb97 (log capacity), PR101 AC transitions
separate; actual source integration base remains22fd266.

Active scope 2026-10-23: v36 policy observation, unchanged four-pair sequence7
command and initial-value gates after accepted FCE/WMM. Separate MCU entry point
collects all target readbacks despite ordinary mismatch, reports mismatch mask,
and requires final MAC/CORE guards. IO/cancellation/all-ones/ACK/ownership failures
still fail. No register acceptance or subsequent BBP writes; preserve v35 strict
behavior. Touch MCU/xHCI/main/staging/tests. Source base22fd266, fetched main249bb97;
PR101 AC transitions separate, no overlapping Wi-Fi implementation.

### 2026-10-23 — v36 policy observation candidate installed, unarmed

Source `d8e246a`, release `76e71b07371c6185`. Build, compact/xHCI tests,
ASan/UBSan, 92 boot-profile tests and docs checks passed. Observation tests cover
all16 mismatch masks and failure/cancellation/guard/ACK paths while strict v35
still fails mismatches. Collector covers all16 masks and seven invalid captures.
Exact-stage GPT VM saved diagnostics and passed offline checks; missing-workspace
VM returned without probing. Native observation result remains pending.

Ubuntu default and previous known-good `e47dca3a6b30c6c1` preserved. Failed v35
`ee842dc8bcc56669` retained; accepted route payload removed from ESP only after
full equality with its durable archive. UUID0E53-5900 clean209files/562clusters,
no allocation errors, unmounted; no one-shot armed. Run record/collector:
`build/bringup/wifi-policy-observe/run.json`,
`build/bringup/collect_wifi_policy_observe_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-policy-observe-d8e246a/`.
Next: readiness-confirmed cold adapter power cycle, automatic capture/return,
then collect matching session. Completion is observation only; register acceptance
and RF/connectivity remain NOT_TESTED.


### 2026-10-23 — v36 complete policy observations collected

Release `76e71b07371c6185`, source `d8e246a`, new session
`00065e89f951db06-00`. Firmware/reset/timing, compact0/1, clocks/readiness and
FCE/WMM passed. Policy sequence7 sent40/received16 cc13/residue1008, exact ACK;
status0 complete1 reads12 batches_attempted1/completed1 ack_mask1. All four
readbacks collected: LINK007f0020->007f1020 (request1020), TIMEOUT010f0a90->000a2090,
RETRY4bb80407->47d01f0f, AUTO_RSP7->13. Mismatch_mask1 correctly reports only
TX_LINK. Initial/final MAC0 and COREf000f200 verified; both capture markers yes.
Completion accepts observation only: register_acceptance=NOT_TESTED and
RF/connectivity NOT_TESTED. No BBP table ran; prior strict failure stays recorded.

Prepared observation collector passed unchanged. Complete collection and updated
run/checkpoint archived with checksums under
`~/.local/share/srvros/macbookpro11-2/wifi-policy-observe-d8e246a/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean211files/565clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: define an explicit bounded TX_LINK readback expectation supported
by this native capture and same-adapter Linux evidence, keeping the remaining
three exact checks and final guards; do not globally relax register comparison.

Active scope 2026-10-23: v37 exact policy validation, unchanged write bytes,
prerequisites/initial gates/ACK. Require LINK readback007f1020 (v36 native and
same-adapter Linux evidence), other three exact requested values, all12 reads and
final MAC0/COREf000f200. No ignored-bit mask; reject every other LINK value.
Preserve v35 strict and v36 observation modes. Touch MCU/xHCI/main/staging/tests.
Actual base22fd266; fetched maind753051 includes Air6,1 AC work, separate scope.

### 2026-10-23 — v37 exact policy candidate installed, unarmed

Source `c0ee877`, release `678a8d195cc57e83`. Build, compact/xHCI tests,
ASan/UBSan, 93 boot-profile tests and docs checks passed. Tests reject every
readback/final-guard bit mutation and retain IO/cancellation/no-retry behavior.
Collector rejects128 target-bit mutations and six invalid capture variants.
Exact-stage GPT VM saved diagnostics and returned with clean filesystem checks;
missing-workspace VM returned without probing. Native validation pending.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Complete v36
`76e71b07371c6185` retained; failed v35 removed from ESP only after comparison with
its durable archive. UUID0E53-5900 clean211files/565clusters, no allocation errors,
unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-policy-validate/run.json`,
`build/bringup/collect_wifi_policy_validate_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-policy-validate-c0ee877/`.
Next: readiness-confirmed cold adapter power cycle and automatic capture/return,
then collect matching session. RF/connectivity NOT_TESTED.


### 2026-10-23 — v37 exact policy validation passed natively

Collected release `678a8d195cc57e83`, source `c0ee877`, new session
`00065e8a2764f20b-00`. Firmware/reset/timing, compact0/1, clocks/readiness and
FCE/WMM passed. Policy command sequence7 sent40/received16 cc13/residue1008,
exact ACK; status0 complete1 reads12 batches_attempted1/completed1 ack_mask1.
Readbacks exactly LINK007f1020, TIMEOUT000a2090, RETRY47d01f0f, AUTO_RSP00000013.
Initial/final MAC0 and COREf000f200 verified. Validation complete1,
write_mismatch_mask1 exact_profile1; both capture completion markers yes.
No observation mode or BBP table ran. Prepared collector passed unchanged.
Acceptance is limited to the exact bounded policy operation on captured7392:a711;
RF/connectivity and remaining MAC/BBP initialization are still NOT_TESTED.

Complete matching collection, run/checkpoint and hashes archived under
`~/.local/share/srvros/macbookpro11-2/wifi-policy-validate-c0ee877/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean213files/568clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: select remaining reference MAC prerequisites against captured
inventory before revisiting full baseband initialization. Preserve all earlier
failed/observational records and the exact profile's device gates.

Active scope 2026-10-23: v38 bounded CCK/OFDM/GF20/GF40 protection group after
accepted v37 policy. Reference relative order1364/1368/1374/1378, requested
07f40003/07f42004/01742004/03f42084; captured starts00100003/00200003/01002004/
02002084. Exact starts, sequence8 four-pair packet/ACK, twelve reads and final
MAC0/COREf000f200. No PBF/DMA/RF changes or BBP-table writes. Touch MCU/xHCI/main/
staging/tests; base22fd266, fetched maind7ed773; PR102 filesystem scaling separate.

### 2026-10-23 — v38 CCK/OFDM/GF candidate installed, unarmed

Source `1947345`, release `89ee4a2fc2121ce2`. Build, compact/xHCI tests,
ASan/UBSan, 94 boot-profile tests and docs checks passed. Collector rejects128
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native protection-group acceptance pending.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v37
`678a8d195cc57e83` retained; v36 observation removed from ESP only after full
comparison with its durable archive. UUID0E53-5900 clean213files/568clusters,
no allocation errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-protect/run.json`, `build/bringup/collect_wifi_mac_protect_return.py`.
Durable payload/evidence: `~/.local/share/srvros/macbookpro11-2/wifi-mac-protect-1947345/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return, then
collect matching session. RF/connectivity NOT_TESTED.


### 2026-10-23 — v38 protection group passed natively

Collected release `89ee4a2fc2121ce2`, source `1947345`, new session
`00065e8a54a2695f-00`. Firmware/reset/timing, compact0/1, clock/readiness,
FCE/WMM and exact v37 policy passed. Protection sequence8 sent40/received16,
cc13/residue1008, exact ACK; status0 complete1 reads12, one batch completed,
ack_mask1. Exact before->after: CCK00100003->07f40003, OFDM00200003->07f42004,
GF20_01002004->01742004, GF40_02002084->03f42084. Initial/final MAC0 and
COREf000f200 verified. Both capture completion markers yes; no BBP table ran.
Acceptance limited to this bounded protection group on captured7392:a711;
remaining initialization and RF/connectivity NOT_TESTED.

Prepared collector passed unchanged. Complete collection/run/checkpoint archived
with hashes under `~/.local/share/srvros/macbookpro11-2/wifi-mac-protect-1947345/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean215files/571clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: select the remaining MAC/protection settings from the reference
inventory before revisiting full baseband initialization.

Active scope 2026-10-23: v39 mixed-mode/TXOP/RTS group after accepted v38.
Reference order136c/1370/1340/1344, writes01742004/03f42084/0000583f/00ffff20;
captured starts00402004/00802084/0400243f/00ffff07. Exact starts, sequence9 packet/
ACK, twelve reads and final MAC0/COREf000f200; no subsequent BBP or RF enable.
Touch MCU/xHCI/main/staging/tests; base22fd266, fetched mainf2e0945 battery-files;
PR102 filesystem scale separate, no Wi-Fi overlap.

### 2026-10-23 — v39 mixed-mode/TXOP/RTS candidate installed, unarmed

Source `d4c5fbc`, release `2cfc6b033353005e`. Build, compact/xHCI tests,
ASan/UBSan, 95 boot-profile tests and docs checks passed. Collector rejects128
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native acceptance pending for this group.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v38
`89ee4a2fc2121ce2` retained; v37 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean215files/571clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-contention/run.json`,
`build/bringup/collect_wifi_mac_contention_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-contention-d4c5fbc/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. RF/connectivity NOT_TESTED.


### 2026-10-23 — v39 mixed-mode/TXOP/RTS passed natively

Collected release `2cfc6b033353005e`, source `d4c5fbc`, new session
`00065e8a97cf1e48-00`. Firmware/reset/timing, compact0/1, clock/readiness,
FCE/WMM, exact policy and CCK/OFDM/GF protection all passed. Sequence9
sent40/received16 cc13/residue1008, exact ACK; status0 complete1 reads12,
one batch completed, ack_mask1. MM20_00402004->01742004,
MM40_00802084->03f42084, TXOP0400243f->0000583f, RTS00ffff07->00ffff20.
Initial/final MAC0 and COREf000f200 verified. Both capture markers yes; no BBP
table ran. Acceptance is limited to this bounded group on captured7392:a711;
remaining initialization and RF/connectivity NOT_TESTED.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-contention-d4c5fbc/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean217files/574clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: select remaining reference MAC prerequisites against the captured
inventory before revisiting full baseband initialization.

Active scope 2026-10-23: v40 TX_SW0/1/2 and header-translation-disabled group after
v39. Registers1330/1334/1338/0260 writes601/40000/0/0; captured starts0204080c/
000c0808/000c0408/0. Four live Linux samples agree with TX_SW final values;
active beacon offsets differ, deferred. Same-adapter read-only audit restored
regidx, no register writes/reset/unbind. Exact gates, sequence10 packet,12reads
and final MAC0/COREf000f200. Reference final table values, not replay of earlier
common TX_SW intermediate writes. Touch MCU/xHCI/main/staging/tests; base22fd266,
fetched mainc43d27c; PR106 power measurements separate.

### 2026-10-23 — v40 TX software-control candidate installed, unarmed

Source `e6e83ab`, release `656e4b9c37ae4baf`. Build, compact/xHCI tests,
ASan/UBSan, 96 boot-profile tests and docs checks passed. Collector rejects128
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native acceptance pending for this group.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v39
`2cfc6b033353005e` retained; v38 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean217files/574clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-txsw/run.json`,
`build/bringup/collect_wifi_mac_txsw_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-txsw-e6e83ab/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. RF/connectivity NOT_TESTED.

### 2026-10-23 — v40 TX software-control group passed natively

Collected release `656e4b9c37ae4baf`, source `e6e83ab`, new session
`00065e8acd64d0df-00`. Firmware/reset/timing, compact0/1, clock/readiness,
FCE/WMM, exact policy, protection and contention prerequisites all passed.
Sequence10 sent40/received16 cc13/residue1008 with exact ACK; status0 complete1,
reads12, one batch completed, ack_mask1. TX_SW0 0204080c->00000601,
TX_SW1 000c0808->00040000, TX_SW2 000c0408->00000000; header translation
remained00000000. Initial/final MAC0 and COREf000f200 verified. Both capture
completion markers present; no BBP table ran. Acceptance is limited to this
bounded group on captured7392:a711; RF/connectivity NOT_TESTED.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-txsw-e6e83ab/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean219files/577clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: review remaining reference MAC prerequisites, including beacon
layout, before revisiting the full baseband table and its unresolved readbacks.

Active scope 2026-10-23: v41 beacon-offset initialization after accepted v40.
Captured041c/0420 starts ece8e4e0/fcf8f4f0; reference common table writes
f8f0e8e0/6f77d0c8. Live Linux beacon offsets are later USB layout, not these
initialization values. Sequence11 two-pair command, eight guarded reads; no
beacon upload/timer enable, MAC TX/RX, RF or BBP table. Touch MCU/xHCI/main/
staging/tests. Base22fd266; fetched main7d2b903, PR106 power and PR109 Air6,1
Home policy remain separate; preserve their existing capture profiles.

### 2026-10-23 — v41 beacon-offset candidate installed, unarmed

Source `c9e2ab6`, release `015cce9347654d25`. Build, compact/xHCI tests,
ASan/UBSan, 97 boot-profile tests and docs checks passed. Collector rejects64
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Normal VM repeated to retain its shared receipt after the
missing-workspace run replaced it. Native acceptance pending for this pair.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v40
`656e4b9c37ae4baf` retained; v39 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean219files/577clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-beacon/run.json`,
`build/bringup/collect_wifi_mac_beacon_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-beacon-c9e2ab6/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. RF/connectivity NOT_TESTED.

### 2026-10-23 — v41 beacon-offset initialization passed natively

Collected release `015cce9347654d25`, source `c9e2ab6`, new session
`00065e8b0501527d-00`. All preceding firmware/reset/timing, compact0/1,
clock/readiness and bounded MAC groups passed. Sequence11 sent24/received16
cc13/residue1008 with exact ACK; status0 complete1 reads8, one batch completed,
ack_mask1. BCN_OFFSET0 ece8e4e0->f8f0e8e0 and BCN_OFFSET1 fcf8f4f0->6f77d0c8
matched exactly. Initial/final MAC0 and COREf000f200 verified. Both completion
markers saved; no BBP table ran. This accepts the initialization pair only on
captured7392:a711; operational beacon layout and RF/connectivity NOT_TESTED.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-beacon-c9e2ab6/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean221files/580clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: review remaining reference MAC prerequisites before revisiting the
full baseband table and its unresolved readbacks.

Active scope 2026-10-23: v42 VHT protection/fallback after accepted v41.
Registers13e0/13e4/13e8/1358: captured startse3f02004/e3f02084/e3f02104/edcba988,
reference targetse3f42004/e3f42084/e3f42104/edcba980. Four live Linux samples
match first/fourth; middle targets have extra10000 in active Linux, not a
cold-init expectation. Keep exact reference targets; no widened mask. Read-only
audit restored regidx and did not reset/unbind/write device registers. Sequence12,
four-pair packet,12reads and final MAC0/COREf000f200; no BBP table or RF enable.
Touch MCU/xHCI/main/staging/tests. Base22fd266; fetched mainde449b3 (Air6,1 Home),
PR106 Text polling/power separate; existing battery capture profiles preserved.

### 2026-10-23 — v42 VHT protection/fallback candidate installed, unarmed

Source `af42f25`, release `a5200b9a725a2ae5`. Build, compact/xHCI tests,
ASan/UBSan, 98 boot-profile tests and docs checks passed. Collector rejects128
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native acceptance pending for this group.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v41
`015cce9347654d25` retained; v40 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean221files/580clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-vht/run.json`,
`build/bringup/collect_wifi_mac_vht_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-vht-af42f25/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. RF/connectivity NOT_TESTED.

### 2026-10-23 — v42 VHT protection/fallback passed natively

Collected release `a5200b9a725a2ae5`, source `af42f25`, new session
`00065e8b2132c5a9-00`. All preceding firmware/reset/timing, compact0/1,
clock/readiness and bounded MAC groups passed. Sequence12 sent40/received16
cc13/residue1008 with exact ACK; status0 complete1 reads12, one batch completed,
ack_mask1. TX_PROT_CFG6/7/8 changed e3f02004/e3f02084/e3f02104 to
 e3f42004/e3f42084/e3f42104; VHT_HT_FBK_CFG1 edcba988->edcba980.
All four matched exact reference targets, including the two that differed under
active Linux. Initial/final MAC0 and COREf000f200 verified. Both completion
markers saved; no BBP table ran. Acceptance is limited to this bounded group on
captured7392:a711; RF/connectivity NOT_TESTED.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-vht-af42f25/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean223files/583clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: review remaining reference MAC prerequisites before revisiting the
full baseband table and its unresolved readbacks.

Active scope 2026-10-23: v43 packet-buffer limits/configuration after accepted v42.
Registers0408/040c/0404/0250: captured starts1f9f1f1f/fe9f/3fdc1cf5/0,
reference targets1fbf1f1f/fe9f/77723c1f/0. Four live Linux samples agree;
FCE0800 remains5 under Linux versus table1, deferred separately. Audit restores
regidx without device writes/reset/unbind. Sequence13 four-pair command,12reads,
final MAC0/COREf000f200. No PBF reset, DMA configuration, BBP table or RF enable.
Touch MCU/xHCI/main/staging/tests. Base22fd266; fetched maince1c0a6; Air6,1
Home-session PR111 and Air7,2 compositor-idle work separate.

### 2026-10-23 — v43 packet-buffer candidate installed, unarmed

Source `9d1ba79`, release `246fade6686cd2fa`. Build, compact/xHCI tests,
ASan/UBSan, 99 boot-profile tests and docs checks passed. Collector rejects128
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native acceptance pending for this group.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v42
`a5200b9a725a2ae5` retained; v41 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean223files/583clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-buffer/run.json`,
`build/bringup/collect_wifi_mac_buffer_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-buffer-9d1ba79/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. RF/connectivity NOT_TESTED.

### 2026-10-23 — v43 packet-buffer configuration passed natively

Collected release `246fade6686cd2fa`, source `9d1ba79`, new session
`00065e8b57f9a360-00`. All preceding firmware/reset/timing, compact0/1,
clock/readiness and bounded MAC groups passed. Sequence13 sent40/received16
cc13/residue1008 with exact ACK; status0 complete1 reads12, one batch completed,
ack_mask1. TX_MAX_PCNT1f9f1f1f->1fbf1f1f; RX_MAX_PCNT remainedfe9f;
PBF_CFG3fdc1cf5->77723c1f; TSO_CTRL remained0. All four matched exact targets.
Initial/final MAC0 and COREf000f200 verified. Both completion markers saved;
no BBP table ran. Acceptance is limited to this bounded group on captured7392:a711;
frame-engine control0800 and RF/connectivity remain untested.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-buffer-9d1ba79/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean225files/586clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: audit remaining frame-engine/MAC prerequisites before revisiting the
full baseband table and its unresolved readbacks.

Active scope 2026-10-23: v44 FCE control observation after accepted v43.
FCE0800 is already written1 during firmware preparation, yet raw native and live
Linux later read5. Linux/vendor MAC table also writes1; no sourced bit2 writable
mask found. Observe a sequence14 pair0800=1/080c=03ff0223, retaining accepted
L2 routing. Captured starts5/03ff0223 mandatory; eight reads and final MAC0/
COREf000f200. First target differences are reported, never accepted; second target
must remain exact. No retry, BBP table or RF enable. Touch MCU/xHCI/main/staging/
tests. Base22fd266, fetched maindb10404; Air compositor PR112 separate.

### 2026-10-23 — v44 FCE control observation installed, unarmed

Source `8294ee1`, release `6e342f3cdd9f35db`. Build, compact/xHCI tests,
ASan/UBSan, 100 boot-profile tests and docs checks passed. Collector validates
raw first-target values with matching mismatch flags and rejects32 L2 mutations
and six invalid captures. Exact-stage GPT VM saved diagnostics, passed offline
checks and returned; missing-workspace VM returned without probing. Native
observation pending; completion cannot accept the FCE control register.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v43
`246fade6686cd2fa` retained; v42 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean225files/586clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-fce/run.json`,
`build/bringup/collect_wifi_mac_fce_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-fce-8294ee1/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. FCE register acceptance and RF/connectivity
NOT_TESTED; do not promote a captured difference into a writable-bit mask.

### 2026-10-23 — v44 FCE control observation completed natively

Collected release `6e342f3cdd9f35db`, source `8294ee1`, new session
`00065e8b8555a943-00`. All preceding firmware/reset/timing, compact0/1,
clock/readiness and bounded MAC groups passed. Sequence14 sent24/received16
cc13/residue1008 with exact ACK echoing the requested0800=1. Status0 complete1
reads8, one batch completed, ack_mask1. FCE control0800 remained5 before/after;
L2 routing080c remained03ff0223. Observation mismatch_mask1 and explicit
register_acceptance=NOT_TESTED saved. Initial/final MAC0 and COREf000f200 verified.
Both completion markers saved; no BBP table ran. This is complete diagnostic
evidence, not exact target acceptance or proof that bit2 is read-only.

Prepared observation collector passed unchanged. Complete collection/run/checkpoint
and hashes archived under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-fce-8294ee1/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean227files/589clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: use the captured write1/read5 evidence when defining strict profile
validation; do not infer a general writable-bit mask. Remaining MAC/baseband
initialization and RF/connectivity remain untested.

Active scope 2026-10-23: v45 exact FCE captured-profile validation after v44.
Reuse sequence14 pair0800=1/080c=03ff0223 with the same captured starts and
MAC/CORE guards. Require complete readback5/03ff0223 and mismatch_mask1;
reject every other full word, no masked bits. Preserve v44 observation semantics.
Touch MCU/xHCI/main/staging/tests. Base22fd266; fetched main180b542 with compositor
idle changes; PR114 battery Start/save and PR115 logger timing remain separate.
No new register writes, BBP table or RF enable in this trial.

### 2026-10-23 — v45 exact FCE profile validator installed, unarmed

Source `538fee1`, release `7301441289281aaf`. Build, compact/xHCI tests,
ASan/UBSan, 101 boot-profile tests and docs checks passed. Collector rejects64
single-bit readback mutations and five invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native validation pending for this exact captured profile.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Completed
v44 observation `6e342f3cdd9f35db` retained; v43 payload removed from ESP only
after full equality with its durable archive. UUID0E53-5900 clean227files/589clusters,
zero allocation errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-fce-validate/run.json`,
`build/bringup/collect_wifi_fce_validate_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-fce-validate-538fee1/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. Full-word5/03ff0223 validation only; no generalized
bit mask, BBP table or RF/connectivity acceptance.


### 2026-10-23 — v45 exact FCE profile validation passed natively

Collected release `7301441289281aaf`, source `538fee1`, new session
`00065e8bc3faaa90-00`. All preceding firmware/reset/timing, compact0/1,
clock/readiness and bounded MAC groups passed. Sequence14 sent24/received16
cc13/residue1008 with exact ACK; status0 complete1 reads8, one batch completed,
ack_mask1. FCE0800 remained5 and L2 routing080c remained03ff0223. Exact validation
reported complete1, write_mismatch_mask1, exact_profile1; observation marker absent.
Initial/final MAC0 and COREf000f200 verified, with both completion markers saved.
No BBP table ran. Acceptance is limited to the captured write1/read5 profile on
7392:a711; no generalized bit mask or RF/connectivity acceptance.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under
`~/.local/share/srvros/macbookpro11-2/wifi-fce-validate-538fee1/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean229files/592clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: review remaining MAC prerequisites before revisiting the full baseband
table and its four unresolved readbacks. Scanning and connectivity remain untested.

Active scope 2026-10-23: integrate fetched main4a991df into the local device
branch before adding further MAC prerequisites. Preserve accepted v45 FCE sequence
and all historical Wi-Fi recipes. Shared baseline includes USB callback/logger
measurement, storage scaling and GUI waits; Air6,1 Home acceptance remains
model-specific and does not accept Pro battery ownership. PR19 is the only open
PR; no other active shared edit was requested. Resolve integration overlaps,
run local host and exact-stage VM checks, then prepare an unchanged-sequence
native regression. Installed7301441289281aaf and known-good recovery remain
untouched until the integrated candidate passes. No new RF or register scope.

### 2026-10-23 — integrated v45 regression installed, unarmed

Merged main `4a991df` as source `e1e595c`; staged/installed release
`9fadb1185a7592ea`, unchanged v45 firmware/FCE recipe. Full build and test-core
passed, alongside119 boot-profile tests and493 documentation links. All90
historical firmware and91 main boot configurations compare exactly against their
respective parents. Wi-Fi MCU sequence/native model gates are unchanged.
Exact-stage GPT VM verified saved/offline file bytes, clean allocation and automatic
return; missing-workspace VM returned without probing. Combined ECM device-checks
and Wi-Fi-probe VM passed DHCP/DNS/HTTP, three outage/recovery cycles,160 UDP echoes,
TCP serving, shared keyboard and disconnect quarantine. Human audio/gesture
acceptance is not implied by scripted VM answers.

Prepared collector retains all v45 predicates, passes the prior complete native
fixture with new release identity and rejects64 readback mutations plus three
incomplete captures. Run/collector:
`build/bringup/wifi-integrated/run.json`,
`build/bringup/collect_wifi_integrated_return.py`. Payload and test evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-integrated-e1e595c/`.
Ubuntu remains default, known-good previous `e47dca3a6b30c6c1` and accepted
v45 `7301441289281aaf` retained. Older v44 was removed from ESP only after full
archive equality. Workspace0E53-5900 clean229files/592clusters, zero allocation
errors and unmounted. No one-shot armed; native integrated acceptance pending.
Next: readiness-confirmed cold adapter cycle, automatic save/return, collect the
matching session before further implementation. RF/connectivity remains untested.


### 2026-10-23 — integrated v45 regression passed natively

Collected release `9fadb1185a7592ea`, source `e1e595c` (main4a991df integrated),
new session `00065e8c5588a9b9-00`. Prepared collector passed unchanged: all prior
firmware/reset/timing, compact0/1, clock/readiness and bounded MAC groups passed.
FCE sequence14 sent24/received16 cc13/residue1008 with exact ACK, complete1
reads8, one batch completed and ack_mask1. Full-word readbacks5/03ff0223,
write_mismatch_mask1 and exact_profile1 verified. Initial/final MAC0 and
COREf000f200 plus both completion markers saved. No full BBP table or RF ran.

Complete collection/run/checkpoint and hashes archived under
`~/.local/share/srvros/macbookpro11-2/wifi-integrated-e1e595c/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace0E53-5900 clean231files/595clusters, zero leaks/mismatches, unmounted.
No reboot armed. This accepts the prior Wi-Fi initialization sequence on the
integrated baseline, not general desktop or battery behavior. Next: remaining
MAC prerequisites before revisiting the four unresolved baseband readbacks.
Scanning, association and connectivity remain untested.


Active scope 2026-10-23: bounded v46 AMPDU1030 initialization following accepted
integrated v45. Sequence15 pair1030=aaa99887/080c=03ff0223; second target preserves
accepted routing. Initial1030=ffffffff is admitted only in this dedicated engine,
with successful exact USB4-byte transfer; it is not declared a valid reset value.
Require exact post-write targets, final MAC0/COREf000f200 and eight reads. Preserve
all existing all-ones refusals and v45 behavior. No retry, BBP table or RF enable.
Touch MCU/xHCI/main/staging/tests. Integrated base4a991df; fetched2702b41 adds
Air6,1 continuous desktop scope. PR118 draft edits storage waiting, not Wi-Fi
control/MCU dispatch; retain its existing shared storage path unchanged.

### 2026-10-23 — v46 AMPDU length initialization installed, unarmed

Source `9fa1ff6`, release `e14f396f3d59c1b6`. Build, compact/xHCI host tests,
ASan/UBSan,120 boot-profile tests and493 documentation links passed. All90 earlier
Wi-Fi boot recipes remain byte-identical. The collector rejects64 readback
mutations,40 transfer failures and two incomplete captures. Exact-stage GPT VM
saved diagnostics, verified offline bytes/allocation and returned; missing-workspace
VM returned without probing. Native acceptance remains pending.

Run/collector: `build/bringup/wifi-mac-ampdu/run.json`,
`build/bringup/collect_wifi_mac_ampdu_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-ampdu-9fa1ff6/`.
Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved, accepted
integrated `9fadb1185a7592ea` retained. Earlier7301441289281aaf removed from ESP
only after full equality with its durable archive. Workspace0E53-5900 clean231files/
595clusters, zero allocation errors and unmounted. No one-shot armed. Next:
readiness-confirmed cold adapter cycle, automatic save/return and collection of
matching session. Sequence15, eight exact transfer receipts and strict post-write
readbacks required; no BBP table, RF or connectivity acceptance.


### 2026-10-23 — v46 aggregation-length initialization passed natively

Collected release `e14f396f3d59c1b6`, source `9fa1ff6`, new session
`00065e8ccad60890-00`. Prepared collector passed unchanged: all preceding
firmware/reset/timing, compact0/1, clock/readiness and bounded MAC groups passed.
AMPDU sequence15 sent24/received16 cc13/residue1008 with exact ACK; complete1,
reads8, one batch completed, ack_mask1. Register1030 changedffffffff->aaa99887;
080c remained03ff0223. All eight ordered read receipts reportedok1, actual4,
cc1, residue0 and timeout0. Initial/final MAC0 and COREf000f200 verified; both
completion markers saved. No full BBP table or RF ran.

Complete collection/run/checkpoint and hashes archived under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-ampdu-9fa1ff6/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace0E53-5900 clean233files/598clusters, zero leaks/mismatches, unmounted.
No reboot armed. Acceptance is bounded to this register write/readback, not
reset semantics, aggregation performance or connectivity. Next: remaining MAC
prerequisites before revisiting the four unresolved baseband readbacks. Further
MCU commands require deliberate sequence identifier reuse after15; do not use16.


Active scope 2026-10-23: v47 full BBP observation after accepted MAC/AMPDU chain.
Reuse the existing58-entry/three-chunk observation engine after every bounded MAC
step completes. Exact legacy table ACK uses sequence6, deliberately reused only
after all prior responses have been consumed; route's sequence6 ACK has a different
address/value and must be rejected. No new table values or RF enable. Require
all prerequisite modes/completions/submissions/inactive owners throughout table
reads/exchanges. Preserve v31 and v46 behavior. Touch xHCI/main/staging/tests;
collector reports mismatches without accepting them. Base4a991df, fetchedbbdbefb
adds Air6,1 AC work; PR118 remains draft storage-wait scope, kept separate.

Prepared v47 source `88809f0`, release `6d6d0b1a49c3de4b`, installed unarmed.
Native build, compact/xHCI fixtures, ASan/UBSan,121 boot-profile tests, collector
fixture plus121 rejection cases, and493 documentation links passed. Exact-stage
GPT VM saved/verified logs and returned automatically; missing-workspace VM
returned without starting the probe. Workspace remains clean233files/598clusters.
Ubuntu remains default, known-good `e47dca3a6b30c6c1` and accepted AMPDU
`e14f396f3d59c1b6` retained. Integrated `9fadb1185a7592ea` was removed from the
ESP only after byte-for-byte archive verification. Payload, checks, collector
and run record archived outside build under `wifi-bbp-after-mac-88809f0`.
Physical result pending; next is an attended cold adapter cycle. After return,
collect the new matching session first and compare all58 observations with v31.

### 2026-10-23 — v47 post-MAC baseband observation returned

Source `88809f0`, release `6d6d0b1a49c3de4b`, native session
`session-00065e8d0bce2741-00` completed. The prepared collector passed unchanged:
all prior MAC/AMPDU prerequisites, three200/200/88-byte chunks, deliberate
sequence6 reuse with exact ACK,124 reads, four MAC0/COREf000f200 guard pairs,
58 observations and both completion markers.54 targets matched; the same four
targets as v31 remained mismatched:

- TXBE10/2728: expected0, before37fff, after38e30.
- TXBE17/2744: expected0, before/afterfb3fee3c (v31fb3fec3c).
- RXO21/2954: expected1, before/after0.
- RXO24/2960: expected6, before/after0.

All other before/after values exactly match v31. This single comparison does
not establish the cause of the TXBE17 difference. MAC initialization through
AMPDU did not resolve the four mismatches; table completion remains observation
only, with register acceptance and RF/connectivity NOT_TESTED. Do not widen
acceptance predicates based on this run. Next: audit these four register
definitions and initialization ordering against the reference driver before
selecting a bounded diagnostic; avoid repeating unchanged table trials.

Ubuntu default, consumed one-shot, installed/known-good payload hashes verified.
Workspace0E53-5900 clean235files/601clusters, zero leaks/mismatches, unmounted.
Collection, comparison, run/checkpoint and hashes archived outside build under
`~/.local/share/srvros/macbookpro11-2/wifi-bbp-after-mac-88809f0/`.
No reboot armed.

Active scope 2026-10-23: audit the four persistent BBP mismatches after v47.
Compare pinned Linux register tables, initialization ordering and vendor-authored
register uses; capture fresh read-only Linux values if useful. Publish findings
and a bounded next diagnostic. No new native writes or acceptance relaxation.
Touch this unit record/current Wi-Fi guide only until evidence justifies code.
Integration base remains4a991df; fetched main54862de adds Air6,1 documentation.
PR118 storage-wait remains separate; PR19 owns this Wi-Fi work.

### 2026-10-23 — post-v47 read-only audit

Four fresh Linux7.0.0-38-generic samples on the same7392:a711 adapter read
TXBE10=38e30, TXBE17=fb3fee3c, RXO21=0 and RXO24=0: all four exactly match
v47. MAC=0c and CORE=f000f200. This differs from older Linux samples where
TXBE10 was0. The Ubuntu run followed SrvrOS without confirmed adapter power
loss, so warm state and later Linux initialization remain confounders.
Only debugfs read-address selection and register reads were used; selector
restored, no register writes, driver unbind or reset.

Pinned [Linux v7.0 initialization](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/init.c)
writes the BBP table, then the G-band20MHz switch table and DCOC table; it does
not test equality of these four readbacks. The vendor-authored source mirrored
at [commit4f1f50e](https://github.com/xtknight/mt7610u-linksys-ae6000-wifi-fixes/tree/4f1f50e3b5e5230f36a13f33112873765389dab3)
likewise initializes them, without documenting their bit-level readback semantics
in the inspected definitions/uses. These facts do not establish that a mismatch
is harmless, nor accept native RF/connectivity. No predicates were relaxed.

Next diagnostic: power-cycle only the Wi-Fi adapter while Ubuntu remains up,
then repeat the same read-only samples with dynamic PHY discovery and exact
descriptor/driver/model gates. This controls adapter power history without a
new SrvrOS write trial. Collector prepared locally as
`build/bringup/bbp-post-mac-audit/capture-cold.py`. Cold results pending.
Raw audit and source hashes archived outside build under
`~/.local/share/srvros/macbookpro11-2/bbp-post-mac-audit-c223f58/`.
No reboot armed; installed payload remains6d6d0b1a49c3de4b.

### 2026-10-23 — cold Linux BBP comparison

User confirmed Wi-Fi unplugged for10seconds and reconnected under Ubuntu.
Exact model/descriptor/driver gates passed; PHY changed fromphy1 tophy2.
Four read-only samples consistently returned:

| Register | Warm Linux / v47 | Cold Linux | Table target |
| --- | --- | --- | --- |
| TXBE10/2728 | 38e30 | 38e30 | 0 |
| TXBE17/2744 | fb3fee3c | ff3fef3c | 0 |
| RXO21/2954 | 0 | 0 | 1 |
| RXO24/2960 | 0 | 0 | 6 |

ASIC76502000, MAC0c, COREf000f200 and TXBE9=20000000 were stable.
Debugfs selector restored; no register writes or driver reset performed by
the capture. Normal Linux enumeration followed the physical reconnection.
All four equality mismatches also occur after cold Linux initialization; warm
SrvrOS carryover alone does not explain them. TXBE17 varies between captures.
This does not document register semantics or establish native acceptance.
No initialization-time Linux trace or connection/traffic test was captured.

Next: audit the reference G-band20MHz BBP switch and DCOC stages that follow
the full table, including RF/ownership boundaries, before implementing a
bounded continuation. Do not spend another boot trying to force equality
without evidence that equality is required. Existing strict diagnostics remain
unchanged. Cold samples/comparison archived alongside the previous audit in
`bbp-post-mac-audit-c223f58`; installed release unchanged, no reboot armed.
