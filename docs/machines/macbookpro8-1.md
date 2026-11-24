# MacBookPro8,1 bring-up record

Updated November 24, 2026. State **Linux inventory captured, exact console
and one-shot return rehearsed in QEMU, first native boot pending.**

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
MacBookPro8,1 console, input and USB workspace milestones. The first work
block records the plan and verifies the host and volume. Implementation is
expected to touch USB controller discovery and transport, hub enumeration,
and the interfaces to HID and block I/O. The current kernel USB path selects
xHCI, and the inventory contains no xHCI controller on this Mac.

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

The installed EFI payload matches the staged manifest. Ubuntu remains the
verified GRUB default and the firmware entries match the pre-install
snapshot. The one-shot boot is unarmed. The first native observation will
use the internal panel and the physical power button as a fallback if no
keyboard is available. The console-only release uses RAM storage and does
not select a USB logging volume.

## Capability ledger

| Capability | State | Next evidence |
| --- | --- | --- |
| Native boot and display | Pending | Firmware framebuffer and kernel console on the internal panel |
| Keyboard and trackpad | Linux inventory | EHCI hub route, keyboard events and verified trackpad reports |
| USB workspace | Mounted in Ubuntu | Native read, then write/sync/unmount and Ubuntu readback |
| Audio, brightness, telemetry, power and networking | Linux inventory | Individual native checks against the captured providers |

The local host checkpoint, exact VM evidence and first console run record
are in `build/review/`. The next checkpoint is the attended console boot
and return to Ubuntu. Native console, input and storage acceptance remain
open.
