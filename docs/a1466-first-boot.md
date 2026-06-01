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

Write that image to a USB stick with your preferred raw-image writer. Double
check the target disk before writing; this image is intended for removable USB
media.

## Boot Target

1. Insert the USB stick.
2. Hold Option during power-on.
3. Choose the EFI boot entry for the USB device.
4. Wait for the `srv>` monitor prompt.

## First Commands

Run these commands at the monitor:

```text
hwdiag
dmesg 8192
xhci
pci
block
```

`hwdiag` is the main capture command. It prints display mode, timer, ACPI, GPU,
xHCI, block devices, full PCI inventory, memory, scheduler/workqueue, network,
mounts, `/fat` fsck summary, process list, and a recent boot-log tail.

## What To Capture

Capture the complete text output from `hwdiag`, plus any exception screen or
serial log if present. The most important lines for A1466 input bring-up are:

- `xhci: usb addressed=... configured=... hid_keyboards=... hid_mice=...`
- `xhci: device slot=... parent=... hub_port=... route=...`
- `keyboard=1`, `mouse=1`, and `absolute=1` fields
- USB descriptor `class`, `iface`, `vendor`, and `product`
- `gpu:` and `display:` lines
- `block:` lines, especially whether the internal SSD appears

## Safety Notes

Early A1466 bring-up should treat the internal SSD as read-only unless we
explicitly decide otherwise. Prefer booting from and writing logs to the USB
image's `/fat` volume. Do not mount or write an internal disk partition during
first validation.

## Interpreting Common Results

- `hid_keyboards=1` and `hid_mice=1`: input is good enough for the next GUI and
  real-hardware testing steps.
- `hubs=1` with routed HID devices: the internal input path is likely behind a
  hub and the one-layer hub path is doing its job.
- `mouse=1 absolute=1`: the pointer is using the generic absolute HID path,
  similar to QEMU `usb-tablet`.
- `unsupported=1` with `iface=3/...`: we likely need a real HID report
  descriptor parser for that device.
- `xhci: no controller`: PCI/xHCI discovery did not find the controller; capture
  `pci` and `dmesg 8192`.
