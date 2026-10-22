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
