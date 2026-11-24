# MacBookPro8,1 bring-up record

Updated November 24, 2026. State **Native console output, topcase descriptor
reads through EHCI split control, and return to Ubuntu verified. The second
EHCI controller scan was incomplete. Both controllers released DMA. Keyboard
input and native USB workspace access remain pending.**

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
MacBookPro8,1 console, input and USB workspace milestones. The attended EHCI
split run verified the topcase descriptor path. The remaining enumeration
question is where the scan stopped on `00:1d.7`. The next diagnostic needs to
preserve the failing hub or port and operation on the final screen. The
inventory contains no xHCI controller on this Mac.

The scope check found separate Air7,2 Intel display work in PR #301, Pro11,2
USB Wi-Fi work at `cb0445b9`, and Apple Silicon runtime, login and export work
in PRs #551, #550 and #510. The initial MacBookPro8,1 scope has no identified
dependency on those branches. The Air7,2 branch at `fb582f40` also touches the
staging tool in its RC6 receipt path, independently of the EHCI profile.
Coordinate shared HID, storage or initialization changes if they become
necessary. [Device coordination](../agent-coordination.md)

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
```

Clean source `8a51702a` produced release `91a277b2e434e4b3`. The exact VM
rehearsal verified the readable 1280 × 800 result screen, both emulated EHCI
scans, DMA retirement and the Ubuntu default marker on the second boot.
The ordinary xHCI console rehearsal passed on the same source. The attended
release is installed as current with `c1a8e397a41363dc` retained as previous.
Both installed manifests and unchanged firmware boot order were verified. The
local handoff is `build/review/macbookpro8-1-ehci-trial-91a277b2e434e4b3/run.json`.

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
and the high-speed flash drive `13fe:6700` on port 2. The compact native
screen did not report the last operation, so the failing port or request
remains unknown. The new Ubuntu boot ID, cleared `next_entry`, unchanged
firmware boot order, mounted exFAT workspace and both installed manifests
were verified. The transcript, verification and exact payload were archived
locally outside `build/` alongside the known-good console release.

## Capability ledger

| Capability | State | Next evidence |
| --- | --- | --- |
| Native boot and display | Console observed on hardware | `c1a8e397a41363dc` reached the terminal. Measure panel mode and qualify desktop separately. |
| Keyboard and trackpad | Native topcase split control and descriptors verified | Keyboard events and verified trackpad reports |
| USB workspace | Mounted in Ubuntu. Native scan of its controller was incomplete. | Locate the failed operation on `00:1d.7`, then verify native read and write/sync/unmount with Ubuntu readback |
| Audio, brightness, telemetry, power and networking | Linux inventory | Individual native checks against the captured providers |

The local host checkpoint, exact VM evidence and first console return are in
`build/review/`. Keyboard reports and trackpad input follow the native
enumeration checkpoint. Keep the verified console payload available for
recovery and retain the existing xHCI regression coverage.
