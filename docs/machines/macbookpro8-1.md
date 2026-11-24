# MacBookPro8,1 bring-up record

Updated November 24, 2026. State **Native console output and return to Ubuntu
verified. Keyboard input failed during the first native trial. The EHCI
controller probe passed in QEMU.**

## Identity and baseline

- Apple `MacBookPro8,1`, board `Mac-94245B3640C91C81`, with firmware
  `MBP81.88Z.0050.B00.1804101331`. Intel Core i5-2415M, two cores and four
  threads, with 9.6 GiB visible to Ubuntu.
- Intel Sandy Bridge integrated graphics `8086:0126` drives the internal
  1280 × 800 LVDS panel under Linux. Internal storage is a 119.2 GiB SATA disk
  on Intel AHCI `8086:1c03`.
- The 6-series USB controllers are EHCI `8086:1c2d` and `8086:1c26`, with
  UHCI functions `8086:1c2c` and `8086:1c27`. The full-speed Apple ANSI topcase
  `05ac:0245` sits behind an internal high-speed hub. Linux uses HID for the
  keyboard interfaces and `bcm5974` for the trackpad. The attached MBR/exFAT
  flash drive takes a high-speed path through the other EHCI controller.
- Intel HDA `8086:1c20` exposes a Cirrus Logic CS4206 analog codec
  `1013:4206` with subsystem `106b:1c00` and Intel HDMI audio. The wired and
  wireless controllers are Broadcom `14e4:16b4` and `14e4:4331` respectively.
- Ubuntu 26.10 boots via its UEFI entry and is the verified GRUB default.
  Secure Boot is unsupported on this host. The EFI partition is mounted and
  has space for staging. The first SrvrOS install will have Ubuntu as its
  recovery OS.
- The privileged Linux inventory has 58 files, including 17 ACPI tables,
  with zero missing or partial reads. Raw evidence is kept locally. The host
  tool check, baseline build and QEMU xHCI UEFI USB smoke passed on `61111819`.

## Active scope

`device/macbookpro8-1/bringup` starts from `origin/main` at
`61111819eb4d99bc397b0cdb944bd34bfa9813da`. This branch owns the initial
MacBookPro8,1 console, input and USB workspace milestones. The active work
block adds an opt-in EHCI endpoint-zero control schedule, high-speed device
and hub enumeration, and DMA retirement. It builds on the verified EHCI
handoff/reset path and shares USB setup packet definitions with xHCI. The
inventory contains no xHCI controller on this Mac. Full-speed split transfers,
HID reporting and block I/O belong to later milestones.

The scope check found separate Air7,2 Intel display work in PR #301, Pro11,2
USB Wi-Fi work at `ad95b6a0`, and Apple Silicon runtime, login and export work
in PRs #551, #550 and #510. The initial MacBookPro8,1 scope has no identified
dependency on those branches. Coordinate shared HID, storage or initialization
changes if they become necessary. [Device coordination](../agent-coordination.md)

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

The emulated QEMU hub is USB 1.1. The Q35 rehearsal uses its high-speed tablet
model for real control transactions. A host fixture models a high-speed hub,
its high-speed child and the full-speed deferral path. [QEMU USB emulation](https://www.qemu.org/docs/master/system/devices/usb.html)

```sh
make test-ehci hardware-stage
python3 tools/hardware_boot_smoke.py --ehci-enumerate
```

## Capability ledger

| Capability | State | Next evidence |
| --- | --- | --- |
| Native boot and display | Console observed on hardware | `c1a8e397a41363dc` reached the terminal. Measure panel mode and qualify desktop separately. |
| Keyboard and trackpad | Keyboard failed on first native release | EHCI hub route, keyboard events and verified trackpad reports |
| USB workspace | Mounted in Ubuntu | Native read, then write/sync/unmount and Ubuntu readback |
| Audio, brightness, telemetry, power and networking | Linux inventory | Individual native checks against the captured providers |

The local host checkpoint, exact VM evidence and first console return are in
`build/review/`. The next checkpoint is an EHCI control-transfer path and
high-speed hub enumeration in a disposable VM. Full-speed split transactions
and the captured topcase reports follow before native keyboard acceptance.
Keep the verified console payload available for recovery and retain the
existing xHCI regression coverage.
