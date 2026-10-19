# MacBookPro11,2 / A1398

Updated: 2026-10-19. State: **initial Linux inventory captured; physical SrvrOS tests pending**.

## Ownership and scope

Local on-device agent, branch `device/macbookpro11-2/bringup`, starting from
`main` at `db40ed510636847d9aad2c7d38fea9976de43252`.
The bounded objective is host provisioning, Linux inventory, baseline build/VM
validation and preparation of the first console/workspace candidate. Changes
are limited to this machine record and local setup/evidence. No shared driver
changes are underway. Open PR #4 owns the Air EC/AML audit; this work does not
duplicate that scope. Follow the [device setup guide](../ubuntu-device-agent-setup.md).

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

All native SrvrOS capabilities remain **not tested on this unit**, including
boot, keyboard/trackpad, scaled display, USB persistence, audio, brightness,
sensors, power and networking. Linux device visibility does not accept a driver.
Keep optional AML probing out of ordinary startup and preserve model gates.

Ubuntu remains the recovery OS. No reboot is authorized or armed by this setup
request, and no SrvrOS recovery payload has yet been verified on this unit.
System build/VM/diagnostic packages and all pinned submodules are installed.
Privileged inspection confirmed no existing SrvrOS installation, an empty GRUB
environment (no pending one-shot), verified Ubuntu default, and about 1 GiB free
on the ESP. Secure Boot is unsupported on this firmware.

## Next checkpoint

Finish pinned submodules and host tools, run build/core tests and sequential
baseline VM smoke, prepare the exFAT workspace and rehearse the exact console
candidate. Record results and installed state in `build/bringup/checkpoint.json`
before any physical trial. Obtain a reboot window after the candidate and recovery
checks are concrete. On return from any future trial, collect its matching USB
session before staging another candidate.
