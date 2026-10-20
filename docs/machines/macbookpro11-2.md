# MacBookPro11,2 / A1398

Updated: 2026-10-19. State: **native desktop, input, brightness and sound exercised; trackpad latency reported**.

The sections below retain the dated bring-up sequence. The latest acceptance
and next scope are recorded at the end; earlier failures are historical.

## Ownership and scope

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
