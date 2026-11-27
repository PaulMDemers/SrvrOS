# MacBookPro8,1 bring-up record

Updated November 27, 2026. State **Native console output, EHCI enumeration
of the built-in topcase and workspace flash drive, and return to Ubuntu
verified. Both controllers completed their scans and released DMA in the
enumeration trial. The operator verified built-in keyboard input, command
execution and readable mixed-case text in the regular monitor. The isolated
trackpad trial verified Wellspring mode recovery, one-finger motion, two
contacts, and click press and release. Ubuntu verified the release-matched
USB report. The operator confirmed the trackpad worked in the native desktop.
The desktop report was missing after a flash save warning, so the full
desktop checklist and retirement counters remain unverified.**

## Identity and baseline

- Apple `MacBookPro8,1`, board `Mac-94245B3640C91C81`, with firmware
  `MBP81.88Z.0050.B00.1804101331`. Intel Core i5-2415M, two cores and four
  threads, with 9.6 GiB visible to Ubuntu.
- Intel Sandy Bridge integrated graphics `8086:0126` drives the internal
  1280 × 800 LVDS panel under Linux. Internal storage is a 119.2 GiB SATA disk
  on Intel AHCI `8086:1c03`.
- The 6-series USB controllers are EHCI `8086:1c2d` and `8086:1c26`, with
  UHCI functions `8086:1c2c` and `8086:1c27`. The full-speed Apple ANSI topcase
  `05ac:0245` sits on port 2 of internal high-speed hub `0424:2513` and
  advertises an 8-byte endpoint-zero packet size. Linux uses HID for the
  keyboard interfaces and `bcm5974` for the trackpad. The attached MBR/exFAT
  flash drive takes a high-speed path through the other EHCI controller.
- Intel HDA `8086:1c20` exposes a Cirrus Logic CS4206 analog codec
  `1013:4206` with subsystem `106b:1c00` and Intel HDMI audio. The wired and
  wireless controllers are Broadcom `14e4:16b4` and `14e4:4331` respectively.
- Ubuntu 26.10 boots via its UEFI entry and is the verified GRUB default.
  Secure Boot is unsupported on this host. The EFI partition is mounted and
  has space for staging. Ubuntu is the recovery OS for both installed releases.
- The privileged Linux inventory has 58 files, including 17 ACPI tables,
  with zero missing or partial reads. Raw evidence is kept locally. The host
  tool check, baseline build and QEMU xHCI UEFI USB smoke passed on `61111819`.

## Active scope

`device/macbookpro8-1/bringup` starts from `origin/main` at
`61111819eb4d99bc397b0cdb944bd34bfa9813da`. This branch owns the initial
MacBookPro8,1 console, input and USB workspace milestones. The EHCI split
trials established the topcase descriptor path and verified the workspace
flash drive with a 600 ms bound for its initial device descriptor. The active
input milestone covers the captured keyboard and trackpad through EHCI
periodic interrupts. The attended RAM-only profile verified Wellspring5 Type 2
reports after mode recovery. Its implementation scope is the
EHCI split control and periodic paths, topcase descriptor matching, boot
staging and their fixtures. It reuses the existing Apple trackpad decoder.
Pointer mapping and native workspace reads and writes follow as separate
milestones.

The scope check found separate Air7,2 Intel display work at `7f276f8c` in
PR #301, Pro11,2 USB Wi-Fi work at `b52a0974`, and Apple Silicon runtime, login and export work
in PRs #551, #550 and #510. The initial MacBookPro8,1 scope has no identified
dependency on those branches. The Air7,2 branch at `85aeb4ba` also touches the
staging tool for graphics receipts, independently of the EHCI profile.
Coordinate shared HID, storage or initialization changes if they become
necessary. [Device coordination](../agent-coordination.md)

The next trackpad block starts from `a3e1c806` on the same `61111819` base.
Ubuntu uses alternate setting 1 (multi-TT) on the captured `0424:2513`
root-connected hub and submits 478-byte interrupt requests to endpoint `0x81`
after the `08 05` to `01 05` feature-mode transition. This block matches that
hub setting and tests consecutive requests on a single periodic queue. Scope
is the explicit EHCI trackpad trial, its enumeration and DMA fixtures, and
the existing release-bound USB report. Keyboard, xHCI and ordinary startup
keep their existing paths. No overlapping EHCI work was found in PR #301,
the Pro11,2 USB Wi-Fi branch, or PRs #551, #550 and #510. The checkpoint is an
exact-stage VM rehearsal followed by an attended native one-shot and USB
report collection.

The mode timing trial starts from `69c6e085` on the same `61111819` base.
It prepares the trackpad queue and displays the checklist before switching
the device mode. It then times the first queue activation and completed
transfer and saves the first payload in the release-bound USB report. The
feature readback moves to the end of capture, before mode restoration.
The scope is the EHCI trackpad trial, its control completion timestamp,
and its fixtures. The current open PRs and Pro11,2 USB Wi-Fi branch have
no overlapping EHCI work. The native checkpoint follows the exact-stage
VM rehearsal and uses the same Ubuntu recovery and prepared USB workspace.

The raw endpoint trace starts from `936d6c77` on integration base `61111819`.
The current `origin/main` at `c523e036` and open device scopes have no
overlapping EHCI changes. Keep the HID replay and endpoint `0x81` queue from
the last trial, record a length distribution and bounded raw samples, then
poll the sibling endpoint `0x84` separately in the same feature mode.
Compare with Ubuntu USB completions from this unit. Scope is the attended
EHCI trackpad trial, its queue fixture and existing USB report. The next
checkpoint is exact-stage VM rehearsal and an attended one-shot with Ubuntu
recovery.

The Wellspring recovery trial starts from `029910c2` on base `61111819`.
`origin/main` at `c523e036` and the open device scopes have no overlapping
EHCI changes. If an eight-byte response arrives on `0x81` before a valid
frame, the attended profile will pause at a verified retired request, select normal
mode, wait at least 1 ms, and retry Wellspring mode once. It will preserve
the queue head and data toggle, record the before and after results, and
save the report to the prepared flash drive. Scope is the existing EHCI
trackpad trial and its fixtures, with an exact-stage VM rehearsal before
the native one-shot. [Linux mode recovery](https://github.com/torvalds/linux/commit/fc1e8a6f129d87c64ac8e58b50d9dfa66217cfda)

The desktop trackpad block starts at `ea08f5c9` on base `61111819`.
`origin/main` at `93e10a44` and the open device scopes have no overlapping
EHCI input work. The captured topcase will feed the existing desktop mouse
queue from a resident EHCI worker and reuse the verified Wellspring recovery.
The attended boot will open a pointer and physical click checklist in Text
in the regular compositor. After the desktop exits, it will retire the topcase
controller and save the release-bound report through the separate flash
controller. Scope is EHCI trackpad ownership, the boot profile and desktop
checklist. The checkpoint is exact-stage VM rehearsal and native acceptance
on this unit.

The desktop USB follow-up starts from `7ed554ef`. The first desktop run
confirmed working trackpad input with the operator and returned to Ubuntu,
but the flash save failed. A read-only replay using the exact initramfs and
attached drive reproduced the writable-admission refusal. The follow-up
consumes `origin/main` at `93e10a44` and the boot-inode repair in PR #554.
Scope is integrating the landed fix, replaying this unit's USB workspace, and
verifying the same desktop checklist with a native flash report.

## Bring-up sequence

1. **Qualify a console-only candidate.** Rehearse the exact EFI payload in a
   VM. Plan an attended native check for firmware framebuffer output and the
   first kernel console markers. Prove a return path to Ubuntu that works
   without the built-in keyboard or the USB log volume before attempting the
   one-shot boot. Capture the screen if the volume is unavailable.
2. **Qualify EHCI transport.** Start with controller ownership, bounded reset
   and transfer timeouts, and verified DMA retirement. Rehearse high-speed
   hub enumeration and read-only mass storage in QEMU. Exercise the
   full-speed topcase path behind the actual hub, including the required
   split transactions, before claiming keyboard support. Add UHCI companion
   support if an observed route requires it. Retest the existing xHCI path.
3. **Validate native input and workspace.** Verify the built-in keyboard,
   then the trackpad reports and pointer. Check the selected exFAT volume
   read-only first. Rehearse its MBR layout on a disposable image, then use a
   bounded test file to verify native write, sync, unmount and Ubuntu readback.
   Compare the pre-trial files and collect the exact release and boot evidence.
4. **Advance to desktop and individual providers.** Verify the 1280 × 800
   desktop, pointer and a saved-file round trip. Qualify brightness, analog
   audio routing, fan/temperature telemetry, battery/AC and networking in
   separate trials using the unit's captured resources for each match.

Before a physical trial, verify the staged manifest and exact VM result,
bind the USB UUID and recovery state in an ignored local run record, and
record the checks and one-shot Ubuntu return. The workspace identifies the
boot-time logging volume and is not a boot installer. [Ubuntu device-agent guide](../ubuntu-device-agent-setup.md)

## First console checkpoint

Release `c1a8e397a41363dc` was staged from clean source `74f803ff` using
`ram-usb-v4-console`. The exact EFI payload passed the GRUB and Limine
chainload, RAM filesystem, USB input and console rendering rehearsal.
A second Q35 rehearsal disabled USB and i8042, reached the 1280 × 800
console, and verified that GRUB cleared `next_entry` on ext4 before
chainloading. After stopping the guest from the host, the next VM boot
selected the Ubuntu default fixture without keyboard input.

In the attended native trial, the operator reached the SrvrOS terminal on
the internal panel. Keyboard input had no effect. This establishes native
console output and records an input failure for this release. The physical
framebuffer mode was not measured. The console-only release uses RAM storage
and left USB workspace persistence untested.

After Ubuntu returned, its boot ID had changed, GRUB's one-shot entry was
consumed, and the firmware entries matched the pre-install snapshot. The
installed EFI payload matched the staged manifest and the exFAT volume was
mounted at the expected location. The method used to return was not reported.
An independent local archive preserves the payload and return evidence
outside `build/`.

## EHCI controller probe

The kernel runs the probe only with `srvros.ehci=probe` and
`srvros.storage=ram`. It discovers EHCI PCI functions, validates the BAR and
capability layout, waits for firmware ownership, and resets each controller
after verifying it is halted. A successful probe leaves interrupts, bus
mastering, schedules and the configured flag clear, then releases its
temporary MMIO mapping. The probe refuses a retained BIOS semaphore. Hub
enumeration and transfer queues are later milestones. [Intel EHCI specification](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

Run the host fixture and staged VM rehearsals from the device checkout.
The EHCI rehearsal attaches two emulated controllers and adds the probe
option to a disposable copy of the plain console stage.

```sh
make -j2 test-ehci test-xhci
make -j2 hardware-stage
python3 tools/hardware_boot_smoke.py --ehci-probe
python3 tools/hardware_boot_smoke.py --console-check
```

Source `e062efae` staged as `321ddfa0f613f514`. With USB input and i8042
absent, QEMU exposed `8086:293a` and `8086:293c`. Both six-port controllers
reported OS ownership, halted reset completion and mapping retirement before
the kernel entered the console. The host fixture also covers a retained BIOS
semaphore, malformed capability data, stalled halt and stalled reset.
The xHCI host suite and staged console regression passed. The latter booted
the same kernel with USB input and the ordinary command line and produced
no EHCI probe output. This evidence is from the VM and host fixtures.

## EHCI high-speed enumeration

`srvros.ehci=enumerate` is a second RAM-only diagnostic. After the same
handoff and reset, it uses a single asynchronous endpoint-zero queue for
descriptor reads and no-data requests. It assigns addresses to high-speed
devices, reads their configurations and configures hubs to inspect connected
ports. A full-speed or low-speed child is disabled before the next address-zero
transaction. The bounded scan covers up to 16 devices, up to 16 ports per hub
and four downstream tiers. It disables the schedules and PCI bus mastering
before releasing the DMA page. An unproved halt retains the page.

The built-in QEMU hub is USB 1.1. The Q35 rehearsals use its high-speed tablet
model and a synthetic USB 2.0 hub attached over a local `usb-redir` socket.
A host fixture models a connected high-speed child, a full-speed child that
must be disabled, a malformed hub descriptor and a stalled reset. [QEMU USB emulation](https://www.qemu.org/docs/master/system/devices/usb.html) · [USB redirection protocol](https://github.com/SPICEorg/usbredir/blob/master/usb-redirection-protocol.txt)

```sh
make test-ehci hardware-stage
python3 tools/hardware_boot_smoke.py --ehci-enumerate
python3 tools/hardware_boot_smoke.py --ehci-hub
```

Source `b39cc8bc` staged as `12940cc028c792f5`. The tablet run assigned
addresses 1 and 2 on one controller and safely retired the other controller's
empty schedule. The redirected-hub run configured a three-port high-speed
hub, powered and read all three downstream ports, then addressed a second
root device. Both runs released the DMA pages after verifying controller
halt. The connected child and slower-port isolation evidence is from the
host fixture. The reset-only EHCI probe and ordinary xHCI console regressions
passed on the same staged kernel.

## EHCI full-speed split control

`srvros.ehci=split` enables full-speed endpoint-zero requests on the RAM-only
diagnostic. For a full-speed child directly behind a high-speed hub, the
asynchronous queue carries the parent hub address and the one-based port.
The driver reads the first eight descriptor bytes at address zero, adopts the
advertised endpoint-zero packet size, then keeps the same hub route through
`SET_ADDRESS` and the configuration reads. A failed request stops the scan
before another port can acquire address zero. The diagnostic defers low-speed
children and leaves any discovered full-speed hub's downstream ports alone.
[Intel EHCI specification, sections 3.6 and 4.12.1](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

```sh
make test-ehci hardware-stage
python3 tools/hardware_boot_smoke.py --ehci-split
```

The host fixtures model the routed control requests, malformed packet sizes,
and the schedule's completion and timeout behavior. The VM exercises the
split boot option with the redirected high-speed hub and a high-speed tablet.
QEMU's built-in hub is a USB 1.1 model, and the redirected high-speed hub has
empty ports, so the VM run reports zero split devices. [QEMU USB hub model](https://github.com/qemu/qemu/blob/v11.0.3/hw/usb/dev-hub.c)

Clean source `51d279b4` staged as `0f5d544618bece74`. Split-mode boot reached
the console, configured the three-port high-speed hub and enumerated the
tablet. Both controllers released their DMA pages. The manifest, host test
output and VM logs are in `build/review/macbookpro8-1-ehci-split-0f5d544618bece74/`.
The reset-only, high-speed tablet, high-speed hub and ordinary xHCI console
regressions passed on the same staged kernel.
The installed console release `c1a8e397a41363dc` matches its recovery archive.
Ubuntu is the GRUB default and `next_entry` is empty.

## Attended EHCI split trial

The native profile displays the checklist during startup and holds the final
result for a photo and manual power-button return. It stops before storage,
ordinary input initialization and userspace. Its empty initramfs fits the
candidate beside the known-good console release on the internal EFI partition.
The exact-stage QEMU test uses the high-speed hub fixture, saves a screenshot,
checks the on-disk GRUB selection was consumed and starts the same virtual disk
again to check the Ubuntu default entry. A QEMU rehearsal pass yields an
incomplete guest result because the native topcase and controllers are absent.

```sh
make -j2 build/kernel/srvros.elf
python3 tools/hardware_boot.py stage --ehci-split-trial
python3 tools/ehci_trial_smoke.py "$(cat build/hardware-boot/latest)"
python3 tools/ehci_trial_smoke.py "$(cat build/hardware-boot/latest)" --stall-hub-status
```

Clean source `8a51702a` produced release `91a277b2e434e4b3`. The exact VM
rehearsal verified the readable 1280 × 800 result screen, both emulated EHCI
scans, DMA retirement and the Ubuntu default marker on the second boot.
The ordinary xHCI console rehearsal passed on the same source. For that
trial, `c1a8e397a41363dc` was retained as the previous release. Both installed
manifests and the firmware boot order were verified. The local handoff is
`build/review/macbookpro8-1-ehci-trial-91a277b2e434e4b3/run.json`.

The native screen must show the expected `05ac:0245` topcase through
`0424:2513` port 2 with an 8-byte control packet and both Sandy Bridge EHCI
controllers completing the scan and releasing DMA. Photograph the full screen,
including the release and result, then use the power button to return to
Ubuntu. The on-disk run record binds the photo or result lines to the release
and Ubuntu boot state. A missing completion line leaves the test incomplete.

The operator transcribed the native screen from `91a277b2e434e4b3` as
`INCOMPLETE`. Both controllers reached reset-halted and released their DMA
pages. `00:1a.7` (`8086:1c2d`) completed its scan. It read the `05ac:0245`
topcase device and configuration descriptors through hub `0424:2513` port 2
with an 8-byte endpoint-zero packet, assigning address 3 through hub address
1. This verifies the split control path for the built-in topcase.

`00:1d.7` (`8086:1c26`) stopped before completing its scan. Ubuntu shows a
low-speed Apple IR receiver `05ac:8242` on port 1 of that controller's hub
and the high-speed flash drive `13fe:6700` on port 2. The first trial screen
recorded completion per controller, leaving its failing operation unknown.
The new Ubuntu boot ID, cleared `next_entry`, unchanged
firmware boot order, mounted exFAT workspace and both installed manifests
were verified. The transcript, verification and exact payload were archived
locally outside `build/` alongside the known-good console release.

### Scan failure details

The retry profile keeps the first unfinished operation for each controller
and shows the root port or hub address and port, the controller's `USBSTS`,
and the last sampled port status. Control-transfer failures also show the
USB address, request number and queue descriptor status bytes. The QEMU
stall fixture exercises this screen by returning a USB stall for hub port
2's status request. The retry preserves the same scan and DMA retirement
rules as the first run.

Clean source `00211b89` staged as `47632d5174507ea3`. In QEMU, the normal
fixture completed both scans. The forced stall reported hub 1 port 2 during
preflight, a qTD error on `GET_STATUS` and `USBSTS=0x8007`. Both rehearsals
released both DMA pages, showed the result screen and selected the Ubuntu
default fixture on the next boot. That release was installed for the retry
with `c1a8e397a41363dc` retained for recovery. The prior partial result and
its payload remain archived. The retry record and VM
evidence are in `build/review/macbookpro8-1-ehci-trial-47632d5174507ea3/`.

The operator's native result for `47632d5174507ea3` was `INCOMPLETE`. The
topcase descriptor reads and address 3 were reproduced on `00:1a.7`, and both
controllers released their DMA pages. `00:1d.7` reached hub address 1 port 2
and timed out on the initial `GET_DESCRIPTOR` at USB address 0. The Ubuntu
sysfs route ties that port to the attached `13fe:6700` workspace drive at
480 Mb/s.

The sampled hub port status `0x00100503` shows a connected, enabled, powered
high-speed port and a reset-change indication. `USBSTS=0xa005` shows that the
asynchronous schedule was active at the stop. The setup qTD completed with
status 0, while the requested data IN and status qTDs both remained active
(`0x80`) under the configured 100 ms completion limit. That locates the
timeout before address assignment. The reason the data phase stayed active
remains open. [USB hub status bits](https://github.com/torvalds/linux/blob/master/include/uapi/linux/usb/ch11.h) · [Intel EHCI specification](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

Ubuntu return was verified with a changed boot ID, consumed `next_entry`,
unchanged firmware boot order and matching manifests for the trial and the
previous console release. The workspace mounted in Ubuntu. The retry's
transcript, VM evidence and exact payload were archived outside `build/`
alongside the earlier trial.

### Initial flash descriptor timing trial

USB 2.0 allows up to 500 ms for the first data packet of a standard control
read and another 50 ms for status after the last packet. The trial allows
600 ms including host margin for the initial one-packet device descriptor
read at address zero on the captured `00:1d.7` high-speed hub route. The last
port, hub identity and exact request must match. Other requests use the
existing 100 ms limit. [USB 2.0 specification, section 9.2.6.4](https://www.usb.org/document-library/usb-20-specification)

The screen prints the measured wait and received byte count for that request.
It also reports the active EHCI queue overlay token when a transfer fails.
Trial acceptance requires both retired controllers, the topcase descriptors,
an 18-byte flash device descriptor and assigned-address enumeration of the
expected `13fe:6700` flash drive. The trial stops before storage I/O.

Clean source `88d26f10` staged as `7c634ec7f48273c4`. The host fixture passed
an 18-byte response after a simulated 120 ms on the selected route, stopped
at 100 ms on another port and bounded a stalled selected request at 600 ms.
It verified schedule disable after each case. Both exact-stage QEMU recovery
rehearsals and the ordinary EHCI hub regression passed. The trial record and VM
evidence are in
`build/review/macbookpro8-1-ehci-trial-7c634ec7f48273c4/`.

The operator's native result for `7c634ec7f48273c4` was `PASS`. The 18-byte
flash device descriptor completed after 493,456 µs of the 600,000 µs wait.
The flash drive `13fe:6700` reached USB address 2 on `00:1d.7`. The `05ac:0245`
topcase again reached address 3 through hub `0424:2513` port 2 with an 8-byte
endpoint-zero packet on `00:1a.7`. Both controllers completed their scans
and released DMA. The measured flash wait exceeded the former 100 ms limit.

The Ubuntu boot ID changed, GRUB consumed the one-shot entry, and the flash
workspace mounted on the same route. Firmware boot order and the installed
manifests matched preflight, including previous console release
`c1a8e397a41363dc`. The native transcript, verification and exact payload were
archived outside `build/`.

## Attended EHCI keyboard trial

The captured `05ac:0245` configuration identifies boot keyboard interface 0,
interrupt IN endpoint `0x83`, 10-byte maximum packets and an 8 ms interval.
The RAM-only `--ehci-keyboard-trial` profile verifies the topology and this
interface before issuing `SET_CONFIGURATION` and `SET_PROTOCOL` for boot
keyboard mode. It expects the 8-byte boot report. [USB HID specification](https://www.usb.org/document-library/device-class-definition-hid-111)

The periodic queue uses the existing hub and port route, start-split in
microframe 0 and complete-splits in microframes 2–4. Before reusing the queue
for another report, the driver verifies that the periodic schedule stopped
and preserves the endpoint's data toggle. It releases the frame list and
control page after verifying controller halt and PCI bus-master disable.
[Intel EHCI specification](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

At the guided `srv>` prompt, type `echo Ehci-42` on the built-in keyboard
and press Return. The trial accepts that line through the kernel's HID
keyboard queue and displays the result for a photo and power-button return.
The prompt accepts this one test line. The keyboard interval is bounded by
90 seconds. The trial leaves the trackpad and flash data interfaces idle.

```sh
make -j2 build/kernel/srvros.elf test-ehci test-keyboard
python3 tools/hardware_boot.py stage --ehci-keyboard-trial
python3 tools/ehci_trial_smoke.py "$(cat build/hardware-boot/latest)"
```

Clean source `520f663f` staged as `241ed4d3abb212d7`. The host fixtures
verified the captured interface match, frame list, split queue layout,
alternating data toggle, 8-byte completion, timeout and DMA retention when
schedule stop is unproved. The kernel's keyboard decoder suite passed.
Both exact-stage QEMU rehearsals verified the held screen and one-shot
return with the native topcase absent. The ordinary xHCI console regression
passed on the same source. The keyboard release was installed for the trial,
with previous console release `c1a8e397a41363dc` and Ubuntu recovery verified.
The trial record and VM evidence
are in `build/review/macbookpro8-1-ehci-trial-241ed4d3abb212d7/`.

The native screen for `241ed4d3abb212d7` reported `INCOMPLETE`, with a typing
timeout, zero completed reports and zero report bytes. In this source the
timeout follows successful keyboard configuration, selection of boot
protocol and acknowledgement of the periodic schedule enable. The raw qTD
transcription is ambiguous. Its likely reading, `0x000a0d80`, matches the
prepared active 10-byte interrupt IN descriptor. The source qTD is written
back on retirement, so the receipt leaves queue traversal and the device's
response unresolved. [Intel EHCI specification, section 4.10](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

Both scans completed and both controllers released DMA. The topcase again
reached address 3, and the flash drive returned 18 bytes in 493,359 µs and
reached address 2. Ubuntu returned with a changed boot ID and the GRUB
one-shot entry consumed. The topcase's Linux route and interface, installed
manifests, firmware boot order and exFAT mount were rechecked. The transcript
and exact payload were archived outside `build/`.

### Keyboard periodic polling evidence

Ubuntu's running `00:1a.7` schedule shows the same full-speed keyboard
endpoint 3 IN, 10-byte packet size, 8 ms interval and split mask `0x1c01`.
The trial keeps those settings and requests a keyboard report every 32 ms
using report ID zero. The result screen records whether `SET_IDLE` completed,
and the time to the first 8-byte report if one arrives. Typing still requires
the expected line from the built-in keyboard. [Linux EHCI schedule format](https://raw.githubusercontent.com/torvalds/linux/master/drivers/usb/host/ehci-dbg.c) · [USB HID specification, section 7.2.4](https://www.usb.org/document-library/device-class-definition-hid-111)

On failure the screen shows frame-index movement, whether the controller's
current pointer matched the submitted qTD, the observed complete-split
progress bits, and the last `USBSTS`, source qTD and queue-head overlay
tokens. Samples are taken before schedule disable. Frame-index movement
verifies the controller clock, while the current qTD pointer and overlay
give separate evidence of traversal. The sampled progress mask can miss
transient microframes. [Intel EHCI specification, sections 2.3.4 and 4.12.2](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/ehci-specification-for-usb.pdf)

Clean source `d384a8c2` staged as `0c509b2b181ab24d`. The host fixture
distinguished a moving frame index with an untouched queue from a loaded
qTD with observed complete-split progress. It also verified frame-index wrap,
8-byte report completion and schedule retirement. Both exact-stage QEMU
rehearsals and the ordinary xHCI console regression passed. Release
`0c509b2b181ab24d` is installed with `c1a8e397a41363dc` retained for recovery.
The run record, VM evidence and Ubuntu periodic schedule snapshot are in
`build/review/macbookpro8-1-ehci-trial-0c509b2b181ab24d/`.

The operator reported that the built-in keyboard worked properly on this
native retry. This records working input in the attended profile. The final
screen, exact test line, HID idle acknowledgement, report timing and DMA
retirement counters remain unverified for this run. The observation alone
leaves the reason for the earlier timeout unresolved. Ubuntu returned with
a changed boot ID and consumed `next_entry`. The installed manifests,
firmware boot order and exFAT mount matched preflight. The exact payload and
operator observation were archived outside `build/`.

### Resident keyboard in the regular monitor

Stage with `python3 tools/hardware_boot.py stage --ehci-keyboard-monitor`.
This RAM-only profile claims the captured topcase on `00:1a.7` and enters the
regular monitor. The periodic worker feeds the existing boot-HID decoder.
The on-screen checklist exercises mixed-case echo, history recall, cursor
movement, Delete and `reboot`. Reboot requires halted schedules, bus mastering
disabled, cleared schedule bases and verified MMIO release. The result line
shows completed reports and resource retirement for a photo before reset.
If that verification fails, use the power button to return to Ubuntu.

`tools/ehci_trial_smoke.py <release>` exercises the exact monitor payload
through serial in Q35, including echo, history editing, ACPI reset and
consumption of the one-shot entry. The host EHCI fixture checks periodic
report completion and halt-before-release behavior. The physical keyboard and
native reset are assessed from the installed laptop release.

On release `d7990a68cf4130ff` the operator confirmed the built-in keyboard,
command execution, and clear case distinctions in the regular monitor.
Ubuntu returned with a different boot ID and an empty GRUB one-shot entry.
The installed and recovery manifests, EFI boot order and exFAT mount matched.
The operator report did not specify history editing, the return method or
the EHCI release counters. The exact payload and evidence were archived
outside `build/`.

### Wellspring5 trackpad reference

Ubuntu on this unit binds `bcm5974` to interface 2, while the trackpad data
arrives on endpoint `0x81` of interface 1. The captured descriptor reports
64-byte full-speed interrupt packets and a 2 ms interval. Ubuntu's live EHCI
schedule uses a 1 ms interval and start/complete mask `0x1c01` on that endpoint.
Wellspring5 reports use a 30-byte type 2 header and 28 bytes per finger slot,
so a report with two slots spans multiple packets. Mode selection uses an
eight-byte HID feature read/modify/write with value `0x0300` and index 0.
[Linux bcm5974 reference](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/input/mouse/bcm5974.c)

Native release `b47bd60baf233327` scanned the controller and reached the
gesture prompt. The capture timed out with zero decoded frames, 656 invalid
transfers, one ignored two-byte response and an eight-byte last transfer.
The recorded restoration, DMA and MMIO release checks passed. A changed
Ubuntu boot ID and empty GRUB one-shot entry confirmed return. The photo
and verified payload are archived outside `build/`.

Release `a4e7cdb4482bf5c1` read back feature mode 1 after writing it from
mode 8. The longest completed transfer was eight bytes. All 282 eight-byte
transfers failed type 2 decoding, and none started with report ID `0x44`.
The last started with byte 2. The run restored the mode and released DMA and
MMIO. Ubuntu returned and the result and payload were archived.

The next work block adds result logging to the attached exFAT workspace
through the captured high-speed EHCI storage route on `00:1d.7`. It reuses
the shared USB Bulk-Only Transport and workspace code, selects the prepared
volume by UUID and saves one release-named file. Checks cover bulk transfer,
volume selection, flush, unmount and controller retirement. Trackpad report
decoding remains open.

Prepare the workspace marker in Ubuntu and stage with
`python3 tools/hardware_boot.py stage --ehci-trackpad-trial --ehci-trackpad-log-volume <exFAT UUID>`.
Rehearse with `python3 tools/ehci_trial_smoke.py <release>`. The attended trial
opens the checklist before polling. It requires a change in one-finger
coordinates, two simultaneous active contacts, and both click transitions
within 60 seconds. A pass also requires the feature mode to be restored and
both the controller DMA and MMIO access to be retired. The keyboard monitor
payload remains available in the recovery archive.

The trial saves `srvros-work/ehci-trackpad-<release>.txt` and requests a restart
after verified unmount and controller retirement. The result screen stays
visible on a save or retirement failure. A power cycle then returns to Ubuntu.

After the `284f78cff9438b7a` one-shot, Ubuntu returned and mounted the
expected workspace with its marker intact. The release-named report was
absent. The installed payload and recovery images matched their manifests.
The native stopping point and restart method remain unknown. The return
record is archived with the exact payload.

On the second attempt with `284f78cff9438b7a`, the photo shows that SrvrOS
registered the USB disk with 512-byte sectors and found its MBR partition.
The exFAT directory scan failed before the workspace mounted. The controller
released DMA, and a manual power cycle returned to Ubuntu. The trackpad
produced 323 eight-byte responses and zero decoded reports. The photo and
return verification are archived separately from the first attempt.

The SrvrOS parser reproduced the failure in Ubuntu. A read-only metadata
capture verified an exFAT cluster count of 473,128, a 59,142-byte allocation
bitmap with one byte beyond the 59,141 required bytes, and a matching up-case
table checksum. The FAT chains for both streams terminate after one cluster.
The parser now accepts reserved bitmap padding and validates the declared
stream chain. The existing writable limit remains one bitmap cluster.
The host regression checks file creation and remount with the reserved bytes
preserved. Native report save and automatic return still need verification.
[exFAT allocation bitmap specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#715-allocation-bitmap)

The updated Ubuntu checker read all 100 files with zero allocation errors.
The separate writable check found a primary volume dirty flag with valid,
matching boot regions. Offline exfatprogs cleared the flag and normalized
the bitmap length. The read-only writable check then passed with zero writes.
Native release `a730b00708a8820b` mounted the USB workspace and saved a
4,047-byte report that Ubuntu read back and verified by release and hash.
The saved log ends before the final unmount and reset messages. Ubuntu
booted with the one-shot entry consumed. The trackpad still timed out with
zero decoded reports, 242 eight-byte transfers and one ignored response.

Source `09b320bb`, release `1ee4e80c7cbf4111`, selects and verifies hub
alternate 1 before enumerating the topcase. It arms six interrupt transfers per
periodic schedule window. Short packets advance to the next report buffer,
and the next batch keeps the endpoint data toggle after the schedule stops.
The USB report adds the batch and completion counts and the latest eight-byte
sample. Local EHCI tests exercise the positive and refusal cases. The exact
release passed the VM's checklist, absent-controller and one-shot Ubuntu
default checks. Trackpad gestures require the attended native trial. A clean
Ubuntu `sync` and unmount cleared the mounted flash drive's dirty flag, and
the offline writable check passed with 102 files and zero writes or allocation
errors.

The native `1ee4e80c7cbf4111` run verified hub alternate 1 and completed
234 interrupt transfers across 40 schedule windows. One short response was
ignored and 233 eight-byte responses were rejected. The last was
`02 00 00 00 00 00 02 00`. Feature mode changed from 8 to 1 and was restored.
The capture decoded zero type 2 reports and timed out with motion, contacts
and clicks unverified. The topcase controller released DMA and its MMIO map.

Ubuntu read back the 4,178-byte report from the expected USB volume and
confirmed the consumed one-shot and unchanged EFI entries. The current,
console recovery and keyboard monitor payloads matched their manifests.
USB contents matched the pre-trial copy except for the new report and the
expected sidecar addition and workspace marker access time. The read-only
offline allocation check passed for all 103 files. The saved report ends
before the guest log controller's final teardown and reset messages.
The run record, report and exact payload are archived outside `build/`.

On the same topcase and controller, Ubuntu `7.3.0-11-generic` completed 988
gesture transfers on endpoint `0x81`. All began with `0x74`, with 718 reports
of 58 bytes and 270 of 86 bytes. SrvrOS's Type 2 decoder accepted all 988 in a
host replay, with one-finger motion, two active contacts and both click
transitions. The native eight-byte sample was absent from the Ubuntu capture.
The usbmon reader reported zero lost events.

In five Ubuntu starts, the mode-1 feature write completed 33–1,765 µs before
the first 478-byte interrupt submission, with a median of 156 µs. The saved
SrvrOS release performs an extra feature readback, programs the frame list
and renders the readiness prompt before enabling its periodic schedule.
Its mode-to-arm interval was unmeasured. The next native check should isolate
that sequence and log the arm timing and first endpoint payload. The Ubuntu
comparison was saved beside the original report on the USB workspace and the
offline check passed with 104 files. [Linux usbmon ABI](https://docs.kernel.org/usb/usbmon.html)

The timing candidate is source `5b446f31`, release `cacf59a022b41970`.
It prepares the first batch and displays the complete checklist before the
mode write. The timing starts when software observes the control transfer
complete. The report records the periodic enable request, the controller's
acknowledgment and the first observed interrupt completion relative to that
point. It includes the entire first payload as ordered hex chunks, then uses
the normal mode restoration and USB save path. The EHCI host tests verified
the request order and the retained multi-packet payload. The exact VM payload
passed the checklist, absent-controller and Ubuntu-default rehearsal.

The native `cacf59a022b41970` trial requested periodic enable 13 µs after
observed completion of the mode write. The controller acknowledged at 15 µs.
The first interrupt completion arrived at 474,169 µs and carried `60 02`,
which the decoder ignored as a two-byte response. The following 366
completions carried eight bytes, with the last sample matching the previous
trial. The post-capture feature read was 1, the original mode was restored,
and the topcase controller released DMA and its MMIO map. The trial timed
out with zero decoded Type 2 reports. Prompt rendering and intermediate
feature readback were eliminated from the mode-to-arm interval in this run.

The release-matched 4,363-byte USB report was verified after remount in
Ubuntu. The one-shot was consumed, firmware entries matched, and the
installed and recovery payloads verified. Prior file contents were preserved,
with the expected sidecar updates for the two reports and the workspace
marker access time. The offline allocation check passed with 105 files and
zero writes. The saved log ends before the final USB teardown and reset.
The exact payload and return evidence are archived outside `build/`.

The Ubuntu initialization comparison for `cacf59a022b41970` used live
topcase reauthorization on the same hub and EHCI controller. After
`SET_CONFIGURATION 1`, `usbhid` sent `SET_IDLE` and retrieved the 156-byte
keyboard report descriptor on interface 0, sent two keyboard output report
requests, then sent `SET_IDLE` and retrieved the 27-byte vendor report
descriptor on interface 1. The mode-1 write completed about 232 ms after
configuration in this sample. The native profile skips these HID requests.
The live Ubuntu schedule confirms 64-byte packets on endpoint `0x81` and
hub port 2, with a 1 ms period and mask `0x1c01` matching native construction.
Ubuntu resubmits its interrupt URB on the running schedule. Native keeps
six queued descriptors per window and checks schedule stop before rearming.
The usbmon evidence covers USB requests at the host driver boundary.
[Linux usbhid](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/hid/usbhid/hid-core.c) · [Linux EHCI queue code](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/usb/host/ehci-q.c) · [Linux usbmon ABI](https://docs.kernel.org/usb/usbmon.html)

Interface 2 declares a four-byte mouse report including ID `0x02`. Interface
1 declares a 512-byte HID input report including ID `0x44`. A separate Ubuntu
probe confirmed mode 8 and saw no completed interrupt payloads on `0x81`
or `0x84` in 20 seconds using their descriptor intervals. Ubuntu restored
the input drivers and mode 1, with valid Type 2 reports during the monitor
window. The source of the native eight-byte frames remains open. The
comparison report is saved on the USB workspace. The read-only allocation
check passed with 106 files, and the raw Ubuntu traces are archived locally.

The HID replay trial starts from `2ab5c95d` on the `61111819` base. A second
Ubuntu capture verified that both keyboard output requests carry `01 00`.
The native candidate replays the six observed HID requests and their
configuration-relative submission offsets before reading and writing the
trackpad feature report. It records partial progress and the timing of the
mode write. The scoped MacBookPro8,1 trial retains the existing EHCI interrupt
queue, gesture checks and USB report path. The open PRs and Pro11,2 branch
have no overlapping EHCI work. The next checkpoint is the exact VM rehearsal
and an attended one-shot with the same flash drive and Ubuntu recovery.

Source `7365fd5d`, release `c1a658684ae3c9e1`, is the HID replay candidate.
It uses the captured submission times as minimum offsets after
`SET_CONFIGURATION 1`, including the 247,973 µs offset for the feature
read. The host fixture verified all request fields and both keyboard output
payloads, and verified that a short HID descriptor response stops the trial
before the mode switch. The exact VM payload passed the checklist,
absent-controller and Ubuntu-default rehearsal with the USB report enabled.

Native release `c1a658684ae3c9e1` completed all six replay requests. It
requested the feature read at 247,975 µs and completed the mode write at
248,783 µs after configuration. The EHCI queue became active 5 µs after
the write. The first transfer completed at 224,369 µs with `60 02`, matching
the previous trial's first payload. The other 386 completions were eight
bytes each, with the same final sample as before. The gesture capture timed
out with zero decoded Type 2 reports. The post-capture feature read was 1,
the original mode was restored, and the controller released DMA and MMIO.

Ubuntu verified the 4,507-byte USB report, consumed one-shot, current and
recovery payloads, and unchanged firmware entries. The existing USB file
contents were preserved apart from the expected sidecar updates. The
read-only allocation check passed with 107 files and zero writes. The saved
report stops before the final USB controller retirement and reset. The
native evidence, exact payload and Ubuntu replay input are archived together.

The feature and queue comparison starts from `de014e05` on the `61111819`
base. It captures the complete native feature reports before and after the
mode switch and restoration, plus queue-head and qTD progress through the
first batch. It compares them with the archived Ubuntu USB trace and a live
Ubuntu EHCI queue sample. The native changes are observations within the
existing trackpad trial and its fixtures. The HID replay, interrupt queue
and USB write path retain their timing and configuration. No overlapping EHCI
work was identified in the open PRs. The next checkpoint is the exact-stage
VM rehearsal and an attended one-shot with USB collection.

Source `c1f1cbb8`, release `22f0ade122d96d67`, is the instrumented
candidate. Its exact VM payload passed the checklist, absent-controller and
Ubuntu-default rehearsal. The host fixture verified full mode byte retention
and split-state, token and link sampling for a multi-packet transfer. The
live Ubuntu reference retained 12 retired qTD snapshots from endpoint
`0x81`. Each completed 58 of 478 requested bytes, with matching qTD and
overlay tokens, alternating toggle and C-progress `0x04`. The earlier USB
trace supplies the complete mode payloads `08 05 00 00 00 00 00 00` and
`01 05 00 00 00 00 00 00`. The Ubuntu reference was saved to the flash
drive alongside the path reserved for the upcoming native report.

Native `22f0ade122d96d67` verified the full before, mode-write, post-capture
and restoration feature payloads against Ubuntu. The endpoint and capability
words agreed after masking the assigned device and hub addresses. All six
first-batch qTDs retired with matching overlay tokens, expected current and
next links, alternating toggles and sampled C-progress `0x04`. Their actual
lengths were 2, 8, 8, 8, 8 and 8 bytes. Each of 12 sampled Ubuntu qTDs
completed 58 bytes from the same 478-byte request. The native capture timed
out with zero decoded gestures and 649 rejected transfers. The source of
the short completions remains open. Linux reuses a running queue head and
resubmits through a dummy descriptor. That queue behavior remains a
separate variable from the measured feature payload and endpoint settings.

The native report, Ubuntu reference and comparison were verified on the
flash drive after remount. The offline allocation check passed for all 110
files with zero writes. The one-shot was consumed, Ubuntu returned, and the
installed and recovery payloads matched. The native report ends before the
USB controller's final teardown and reset. The evidence and exact payload
are archived outside `build/`.

The single-request trial starts from `1a3c2da6` on the `61111819` base.
The existing HID replay and mode timing stay in place. The candidate keeps
one periodic queue head linked, with one active interrupt request followed
by an inactive dummy qTD. On verified retirement it publishes work through
the old dummy while preserving the controller-owned overlay and toggle.
The changes are scoped to the trackpad trial and its fixtures. The current
open PRs have no overlapping EHCI changes. The next checkpoint is an exact
VM rehearsal and an attended one-shot with USB logging and Ubuntu recovery.
[Linux EHCI queue publication](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/usb/host/ehci-q.c)

Source `1462416f`, release `bc98fee76d0e18c3`, is the staged running-queue
candidate. It records submissions, completions and the first six queue
snapshots. The host fixture verified one schedule start and stop across
11 requests, retained the hardware toggle, and refused to republish when
the fake controller advanced into the inactive dummy. The exact VM payload
passed the checklist, absent-controller and Ubuntu-default rehearsal.

Native `bc98fee76d0e18c3` completed all six HID requests and retained the
same complete mode payloads as the previous trial and Ubuntu. The periodic
queue started once and recorded 384 resubmissions and 384 completions, with
the final request still pending at the gesture deadline. The first six
snapshots verified alternating descriptors and data toggles, matching qTD
and overlay tokens, and C-progress `0x04`. Their transfer lengths were
again 2, 8, 8, 8, 8 and 8 bytes. The full run logged one ignored response,
383 rejected eight-byte transfers, and zero decoded gestures. The original
mode and topcase DMA and MMIO retirement verified.

Ubuntu returned with the one-shot consumed and the EFI entries unchanged.
The 6,275-byte native USB report matched its archived copy. The read-only
allocation check passed for all 111 files, and prior file contents were
preserved with the expected sidecar updates. The exact payload and report
are archived outside `build/`. The saved report stops before USB teardown
and reset.

Source `10917f4d`, release `d4a82982f7c2bccb`, adds the raw endpoint trace.
The host fixture verified separate queues on `0x81` and `0x84`, their
payload lengths, bounded sample logging, and refusal to run the mouse phase
after an unsafe primary queue state. The exact stage passed its QEMU
checklist, absent-controller, and Ubuntu-default rehearsal.

Ubuntu USBFS and usbmon captured matching mode-1 completions on this unit.
Endpoint `0x81` returned 765 58-byte and 561 86-byte frames, all beginning
with `0x74`, at the descriptor interval of 2 ms. All 56 requests on endpoint
`0x84` timed out at the 8 ms descriptor interval. The full feature payload
and all three driver bindings were restored. The existing Ubuntu
`bcm5974` 1 ms baseline shows the same frame lengths and leading byte on
`0x81`. Both fresh traces and their summary were saved to the USB workspace.

Native `d4a82982f7c2bccb` recorded one `60 02` completion about 224 ms after
the mode write and 308 eight-byte completions on endpoint `0x81`. The 15
logged eight-byte samples and the final response all start with `0x02`.
The queue head identifies endpoint 1 with a 64-byte max packet, and the
first six retired qTDs and their overlays agree on the transfer lengths.
The separate 15-second `0x84` phase had no completions. This agrees with
Ubuntu on `0x84`, whose mouse descriptor specifies a four-byte ID `0x02`
report. The native trial recorded zero Type 2 gestures and verified the
feature restoration and topcase DMA and MMIO release.

Linux `bcm5974` reports the same eight-byte symptom when the mode switch
is sent before the interrupt control response. It returns to normal mode,
waits about 1 ms, and retries Wellspring mode. That is the leading explanation
for the native `0x81` traffic even though the feature readback says mode 1.
The host-side traces identify the queue that completed and leave the physical
packet source and device state unverified. A bounded native retry after the
`60 02` response can test whether the Linux recovery restores Type 2 frames.
[Linux mode recovery](https://github.com/torvalds/linux/commit/fc1e8a6f129d87c64ac8e58b50d9dfa66217cfda)
[USB trace scope](https://docs.kernel.org/usb/usbmon.html)

Ubuntu returned and consumed the one-shot. EFI entries and the installed
and recovery payloads verified. The 8,434-byte native result and comparison
matched their archive copies after USB remount. The read-only allocation
check passed for 115 files with zero writes. The saved native report ends
before the flash controller's final teardown and reset.

Source `6489a417`, release `53177189337d3d65`, is the staged mode-recovery
trial. The host fixture verified the normal and Wellspring control writes,
the minimum 1 ms wait, queue resumption with the controller's data toggle,
and restoration on timer failure. The exact-stage VM verified the operator
checklist, absent-controller handling, and Ubuntu-default rehearsal. The
flash report records the trigger, feature writes, wait and resume timing,
and the first decoded Type 2 frame after the retry if one arrives.

Native `53177189337d3d65` passed. The initial `0x81` queue again completed
`60 02` followed by an eight-byte `0x02` frame. The diagnostic paused the
queue and wrote mode 8, recorded a 1,014 µs wait, read back mode 8, and
wrote mode 1. The two writes completed 1,605 µs apart. The same queue
resumed 14 µs after the retry, and its first Type 2 frame arrived 5,502 µs
after the Wellspring write. It was 58 bytes and began with `0x74`, matching
the Ubuntu reference. The first six transfer lengths were 2, 8, 58, 58, 58
and 58 bytes with alternating qTDs and data toggles across the retry.

All 224 post-retry completions decoded as Type 2, with a maximum length of
86 bytes. The trial verified one-finger motion, two contacts, and click press
and release. Endpoint `0x84` had no completions. The full original feature
payload and topcase DMA and MMIO release verified. This accepts recovery and
gesture decoding in the isolated native trial.

Ubuntu returned with the one-shot consumed, EFI entries unchanged, and the
installed and retained payloads verified. The 7,667-byte native report and
the recovery comparison matched the archived copies after USB remount. The
offline allocation check passed for 117 files with zero writes. The saved
native report ends before flash-controller teardown and reset.

Source `738795d8`, release `c431c3461b31a9a3`, is the desktop pointer
candidate. The boot option `srvros.ehci=trackpad` selects the resident
input worker for the captured topcase during regular startup. The attended
profile connects the same worker to the regular compositor and launches
the checklist in Text. It checks cursor movement on both axes, mouse
activation of maximize and restore, and pointer selection of Return to
console. A 90-second deadline allows an incomplete run to save its report.

The host fixtures passed EHCI transfer, Type 2 pointer, ordered button-edge,
and mouse ABI checks. The exact QEMU boot opened the in-guest checklist at
1280 × 800. Virtual PS/2 input exercised both axes, both title-bar actions,
and the return menu. The absence of the captured topcase kept the VM's
overall hardware result incomplete. The reboot rehearsal returned to the
Ubuntu default. The attended image carries the compositor, Text and its
required helpers in RAM so the recovery image still fits on the EFI volume.

The operator confirmed the built-in trackpad worked in native desktop
`c431c3461b31a9a3` and reported a flash save verification warning on exit.
Ubuntu returned and consumed the one-shot, and EFI entries and retained
payloads verified. The report was absent. All 117 workspace files matched
their backups, and offline allocation and writable checks passed. The
operator observation does not supply the compositor or retirement counters.

The exact staged initramfs and current USB partition reproduced the save
blocker in a read-only host replay. The desktop bootstrap allocated inode 2
to `/fat`, while the workspace sidecar already assigned it to `/work`.
Additional boot inode numbers overlapped saved metadata. Writable admission
refused even though exFAT allocation passed with zero I/O writes or flushes.
The shared fix in PR #554 assigns generated boot IDs to a separate range.

The merge at `3849ae3b` brings in the upstream boot-inode fix. The read-only
replay found eight collisions with the original code and zero after the
merge. Writable admission passed with the workspace metadata unchanged.
Release `0a4b6e655574e009` passed the host regressions, the exact desktop VM
rehearsal, and writable admission for the current 118-file USB workspace
using its exact staged initramfs. The previous run's Ubuntu return note is
on the flash drive. The native retry will check the release-bound desktop
report after the pointer and click checklist.

## Capability ledger

| Capability | State | Next evidence |
| --- | --- | --- |
| Native boot and display | Console observed on hardware | `c1a8e397a41363dc` reached the terminal. Measure panel mode and qualify desktop separately. |
| Keyboard | Operator verified built-in input and commands in the regular monitor | Capture the native EHCI release line on a future monitor run |
| Trackpad | Isolated native recovery passed on `53177189337d3d65`. The operator confirmed desktop trackpad function on `c431c3461b31a9a3`. | Verify desktop pointer, click, and retirement counters from the saved native report |
| USB workspace | Native exFAT mount, report write and Ubuntu readback verified after offline repair | Capture the native unmount and controller retirement receipt |
| Audio, brightness, telemetry, power and networking | Linux inventory | Individual native checks against the captured providers |

The local host checkpoint, exact VM evidence and first console return are in
`build/review/`. Keyboard reports and trackpad input follow the native
enumeration checkpoint. Keep the verified console payload available for
recovery and retain the existing xHCI regression coverage.
