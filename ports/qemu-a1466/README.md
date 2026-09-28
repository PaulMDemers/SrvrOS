# QEMU A1466 Input Rehearsal Model

This package adds a focused Apple MacBook Air A1466 input personality to QEMU
10.2.0. It is not a complete MacBook machine model. It emulates the hardware
surface needed to develop and regression-test the SrvrOS Broadwell LPSS
SPI/topcase driver without reflashing a USB stick for every register change.

The current model provides:

- Intel LPSS DMA PCI identity `8086:9ce0` at `00:15.0`.
- Intel LPSS SPI1 PCI identity `8086:9cba` at `00:16.0`.
- A 64-bit SPI MMIO BAR with PXA-compatible controller registers at `0x000`.
- Broadwell/LPT LPSS private registers at `0x800`.
- A bounded 64-packet topcase FIFO exposed through PXA `SSSR`/`SSDR`.
- QEMU keyboard-event translation into CRC-valid 256-byte Apple SPI keyboard
  packets with USB HID usages and modifier state.
- An injected SSDT with `PCI0.SPI1.SPIT`, `APP000D`,
  `APPLE-SPI-TOPCASE`, SPI resources, and `SIEN`/`SIST`/`UIEN`/`UIST`.

Build and run the full smoke from MSYS2 UCRT64 or PowerShell:

```text
make a1466-emulator-smoke
```

The first run clones pinned QEMU `v10.2.0` into
`build/tooling/qemu-a1466-src` and performs a native UCRT64 build. Later runs
are incremental. The smoke boots `build/srvros-usb.img` with both i8042 and USB
disabled, runs the automatic A1466 capture, types `echo spi-input-ok` through
QMP, and requires SrvrOS to execute it through the emulated SPI topcase path.
It then verifies nonzero keyboard-report counters and writes
`build/a1466-emulator-smoke.log`.

`tools/build_a1466_ssdt.py` uses a host `iasl` when available and can fall back
to `iasl` installed in WSL. The tested WSL package is `acpica-tools`.

Current boundary:

- Keyboard input currently uses bounded polling rather than GPE/IRQ delivery.
- The packet model covers unfragmented keyboard reports, not topcase commands
  or fragmented payloads.
- No trackpad/pointer event backend yet.
- No GPE/IRQ delivery or functional DMA yet.

Real A1466 hardware remains the authority for unknown register defaults, ACPI
power sequencing, GPIO/GPE routing, timing, and packet details. The next phase
is to compare the proven polling transaction against the physical controller,
then add ACPI enable sequencing and interrupt-driven reads.
