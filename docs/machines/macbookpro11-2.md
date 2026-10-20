# MacBookPro11,2 / A1398

Updated: 2026-10-19. State: **native desktop, keyboard and trackpad exercised successfully; brightness and audio unavailable**.

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
