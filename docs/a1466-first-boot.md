# MacBook Air A1466 First Boot Checklist

This checklist is for the Apple MacBook Air A1466 target. It assumes the USB
input path is expected to work through xHCI, possibly behind a USB2 hub, and
that the internal trackpad may appear as a generic absolute HID pointer.

## Build the USB Image

```powershell
make usb-image
```

The resulting image is:

```text
build/srvros-usb.img
```

The USB builder mirrors the same Limine config into every standard removable
UEFI search location used by the image: `/EFI/BOOT/limine.conf`,
`/limine.conf`, `/boot/limine.conf`, `/boot/limine/limine.conf`, and
`/limine/limine.conf`. This keeps the MacBook firmware path from depending on a
single loader-relative config location.

Current pre-hardware handoff image:

```text
build/srvros-usb.img
```

As of the GUI close-out pass on June 6, 2026, that image includes the
`displayd`/GUI2 compositor milestone, the framebuffer-console mute fix, app
exit-status notices, surface-remap cleanup during resize, and launch-capacity
protection. Hidden-QEMU GUI smokes passed before the handoff, and a live QEMU
run showed no exceptions. The raw Surface Demo app is still a timed demo and
may exit on its own after a short run; that is not currently treated as a
crash.

The default `/srvros` Limine entry is currently hardware-bring-up oriented and
passes `srvros.a1466.capture=1`. That makes the kernel run the A1466 diagnostic
bundle automatically after boot, before the monitor prompt, so the capture works
even when the built-in keyboard is unavailable. Use `/srvrosnocapture` for a
normal quiet boot without the automatic dump.

Before writing removable media, rehearse the same UEFI/xHCI/HID boot shape in
hidden QEMU:

```powershell
make a1466-rehearsal QEMU=C:\msys64\ucrt64\bin\qemu-system-x86_64.exe
```

The rehearsal runs `hwdiag`, `dmesg 8192`, `xhci`, `pci`, `block`, then starts
`gui --smoke-autostart`. It writes the full serial capture to:

```text
build/a1466-rehearsal.log
```

Current local note: the ISO/exFAT displayd smoke passes, but the GPT USB
rehearsal can stop in local OVMF immediately after `BdsDxe: starting Boot0001`
without reaching kernel serial output. The harness now records serial
disconnects, QMP input failures, and QEMU exit context in
`build/a1466-rehearsal.log`. Treat that as a QEMU/OVMF USB-rehearsal issue, not
as evidence that the real A1466 USB image is unbootable; the MacBook hardware
has already reached the srvros monitor from this image family.

Write that image to a USB stick with your preferred raw-image writer. Double
check the target disk before writing; this image is intended for removable USB
media.

## Boot Target

1. Insert the USB stick.
2. Hold Option during power-on.
3. Choose the EFI boot entry for the USB device.
4. Wait for the `srv>` monitor prompt.

The normal `/srvros` entry uses quiet framebuffer boot so the screen should end
at a short boot-complete banner and the monitor prompt. If early hardware text
is needed on the panel, choose `/srvrosdebug` from Limine instead; it keeps the
framebuffer console verbose while preserving the same serial and boot-log output.

## First Commands

The default boot entry now runs the important capture commands automatically.
If an external keyboard is available, these are the equivalent manual commands
to rerun at the monitor:

```text
hwdiag
dmesg 8192
xhci
pci
block
spi
spiregs
acpiinput
```

`hwdiag` is the main capture command. It prints display mode, timer, ACPI, GPU,
xHCI, block devices, full PCI inventory, memory, scheduler/workqueue, network,
mounts, `/fat` fsck summary, process list, and a recent boot-log tail.
`spi`, `spiregs`, and `acpiinput` are the focused A1466 input bring-up commands:
they should confirm whether Broadwell LPSS SPI1 is mapped and whether the ACPI
topcase namespace still resolves to the `SPI1`/`SPIT` method cluster.

Expected automatic markers:

```text
a1466-capture: begin
== hwdiag ==
== dmesg 8192 ==
== spi ==
== spiregs ==
== acpiinput ==
== pci ==
== xhci ==
== block ==
== final spi/input summary ==
a1466-capture: end
```

Because the internal keyboard is not working yet, photograph the final screen
after `a1466-capture: end` as well as any earlier screen that contains
`lpss-spi-regs:`. If `/fat` mounted from a writable USB/exFAT block device, the
same capture is persisted in `/fat/var/log/boot.log`; if `/fat` fell back to
`initramfs-exfat`, persistence may be skipped and the screen/photo is the
capture.

## First GUI Launch

After `hwdiag` confirms a visible framebuffer and at least one working keyboard,
start a shell from the monitor and launch the supported GUI path:

```text
run /fat/bin/sh
gui
```

Equivalent direct monitor launch:

```text
run /fat/bin/gui
```

Expected console markers include `gui: starting displayd`, a `displayd:
framebuffer ...` line, and `displayd: root backbuffer ready`. On the desktop,
use the dock to open CALC, NOTES, EDIT, and PAINT, move or resize at least one
window, minimize and restore it through the taskbar, then use the on-screen Exit
button. A clean exit should return to the shell or monitor without an exception
and print `displayd: shutdown complete` followed by `displayd: exited`.

## What To Capture

Capture the complete text output from `hwdiag`, plus any exception screen or
serial log if present. The most important lines for A1466 input bring-up are:

- `xhci: usb addressed=... configured=... hid_keyboards=... hid_mice=...`
- `xhci: device slot=... parent=... hub_port=... route=...`
- `pci-input: ... vendor=8086 device=9cba ... communication ...` for the
  MacBookAir7,2 SPI controller
- `pci-input: ... vendor=8086 device=9ce0 ... dma ...` for the paired LPSS DMA
  controller
- `lpss-spi: spi1 ... id=9cba8086 ... cmd=... bar0=... mmio=... mapped=...`
- `lpss-spi: dma ... id=9ce08086 ...`
- `acpi-input: ... key=SPI1` / `key=SDMA` / `key=HSSP` lines, which point to
  the AML area that describes the Apple SPI topcase path
- `acpi-input-device: name=...` and `acpi-input-member: ...` lines near
  `SPI1`/`SPIT`/`APPLESPITOPCASE`; these identify the enclosing AML device and
  nearby methods/resources we need for topcase binding
- `acpi-input-topcase: ... topcase=spit ...` for a compact summary of the
  `SPI1` controller, `SPIT` topcase child, `APPLE-SPI-TOPCASE` HID marker, and
  `SIEN`/`SIST`/`UIEN`/`UIST` method offsets
- `lpss-spi-regs: ...` lines, which report the PXA/LPSS status/control
  registers without consuming SPI FIFO data
- `keyboard=1`, `mouse=1`, and `absolute=1` fields
- USB descriptor `class`, `iface`, `vendor`, and `product`
- `gpu:` and `display:` lines
- `block:` lines, especially whether the internal SSD appears
- `displayd:` framebuffer, root backbuffer, client launch, and shutdown lines
- A photo or screenshot of the desktop at the native 1440x900 panel resolution

For the next session, the single highest-value capture is the `spiregs` output.
If it prints real `sscr0`/`sscr1`/`sssr` register values, the next code task is
the first bounded polling LPSS SPI transaction. If it still reports unavailable
MMIO, stay on mapping/PCI BAR diagnostics before attempting controller writes.

## Safety Notes

Early A1466 bring-up should treat the internal SSD as read-only unless we
explicitly decide otherwise. Prefer booting from and writing logs to the USB
image's `/fat` volume. Do not mount or write an internal disk partition during
first validation.

## Interpreting Common Results

- `hid_keyboards=1` and `hid_mice=1`: input is good enough for the next GUI and
  real-hardware testing steps.
- `pci-input` shows `8086:9cba` but no PS/2 or USB keyboard: the built-in
  topcase is visible as Apple SPI/HSSPI, but the SPI transport driver is not
  implemented yet. Use an external USB keyboard for near-term console testing.
- `lpss-spi: spi1 ... mapped=0`: firmware did not leave PCI memory decode
  enabled, so the next driver step must enable the SPI controller's MMIO window
  before sampling registers.
- `lpss-spi: spi1 ... mapped=1`: the SPI controller MMIO is mapped and the next
  step is ACPI topcase resource binding plus HSSPI protocol bring-up.
- `lpss-spi: ... mapped=0` with `memory=1` and an MMIO address ending in a
  nonzero page offset usually means the BAR must be page-aligned before mapping.
  The register dump should work once the mapped physical base is page-aligned
  while the virtual register base keeps the BAR offset.
- `acpi-input` prints `SPI1`/`SDMA`/`HSSP` plus `acpi-input-device` and
  `acpi-input-member` lines: capture those lines closely. They should expose
  the `SPI1` controller, `SPIT`/Apple topcase child, and ACPI methods such as
  `SIEN`/`SIST` that the SPI/HSSPI input driver must honor.
- `lpss-spi-regs: enabled=... busy=... tx_not_full=... rx_not_empty=...`:
  capture these before we try active SPI transfers. A non-busy controller with
  mapped MMIO is the ideal starting point for a polling transport.
- `hubs=1` with routed HID devices: the internal input path is likely behind a
  hub and the one-layer hub path is doing its job.
- `mouse=1 absolute=1`: the pointer is using the generic absolute HID path,
  similar to QEMU `usb-tablet`.
- `unsupported=1` with `iface=3/...`: we likely need a real HID report
  descriptor parser for that device.
- `xhci: no controller`: PCI/xHCI discovery did not find the controller; capture
  `pci` and `dmesg 8192`.
