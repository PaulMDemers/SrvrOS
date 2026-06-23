# Apple SPI/HSSPI Topcase Status

This note tracks the current Apple MacBook Air A1466 internal
keyboard/trackpad bring-up. The tested machine behaves like a MacBookAir7,2:
the built-in keyboard and trackpad are not usable through PS/2 and do not
appear as ordinary USB HID devices during srvros boot. They are exposed through
Apple's SPI/HSSPI topcase path.

## Current Hardware Evidence

Real-hardware boot captures show:

- PS/2 is unavailable:
  `keyboard: ps/2 controller unavailable`,
  `input-ps2: enabled=0`.
- xHCI initializes, but no internal USB keyboard or mouse is discovered:
  `usb_keyboards=0`, `usb_mice=0`, with external USB still expected to be the
  near-term keyboard fallback.
- The Broadwell LPSS SPI controller is present at PCI `8086:9cba`,
  bus 0, device 22, function 0.
- The paired LPSS DMA controller is present at PCI `8086:9ce0`,
  bus 0, device 21, function 0.
- ACPI DSDT contains the relevant namespace:
  `PCI0`, `SPI1`, `SDMA`, `SPIT`, and methods/resources around the topcase
  device.
- The compact ACPI summary has identified:
  `SPI1_DEVICE=SPI1@24535`,
  `SPIT_DEVICE=SPIT@24986`,
  `SIEN`, `SIST`, `UIEN`, and `UIST` method offsets.

The important interpretation is that `SPIT_DEVICE=SPIT@24986` is the topcase
child. Even if the HID marker string does not show up in every shortened
photo, the `SPIT` child and Apple SPI method cluster are present.

## Current Code State

Relevant files:

- `kernel/src/drivers/lpss_spi.c`
- `kernel/include/srvros/lpss_spi.h`
- `kernel/src/acpi.c`
- `kernel/src/main.c`
- `kernel/src/monitor.c`
- `docs/a1466-first-boot.md`
- `tools/collect_a1466_macos.sh`

Implemented pieces:

- PCI discovery for Broadwell LPSS SPI1 `8086:9cba`.
- PCI discovery for Broadwell LPSS DMA `8086:9ce0`.
- Early boot diagnostic printing for LPSS SPI/DMA command, BAR, IRQ, class, and
  MMIO decode state.
- ACPI FADT/DSDT remembering, so the DSDT is scanned even when it is not listed
  directly in XSDT/RSDT child tables.
- Focused ACPI input diagnostics for `SPI1`, `SDMA`, `SPIT`, HSSPI/topcase
  markers, and Apple topcase methods.
- A compact `acpi-input-topcase:` summary line with controller, child device,
  and method offsets.
- Bounded AML structure diagnostics for enclosing `Device`, nearby `Name`
  objects, child devices, and methods.
- Monitor commands:
  - `spi`
  - `spiregs`
  - `acpiinput`
- Read-only LPSS/PXA register diagnostics for:
  `SSCR0`, `SSCR1`, `SSSR`, `SSITR`, `SSTO`, `SSPSP`, and selected LPSS private
  registers. The diagnostic deliberately does not read `SSDR`, because reading
  the data register can consume receive FIFO state.

## Latest Hardware Signal

The latest A1466 photo shows that the controller now reaches `mapped=1`; the
USB/Limine boot path is no longer the blocker. The built-in keyboard still does
not appear as USB HID:

```text
input-usb: op=yes devices=0 keyboards=0 mice=0 hubs=0 ...
```

and the SPI path is the active lead:

```text
lpss-spi: spi1 present ... device=9cba ... command=6 memory=1 busmaster=1
lpss-spi: bar0=... bar64=1 mmio_phys=... mapped_phys=... mmio_virt=... mapped=1
```

The latest capture proved the prior `0x200` private-register assumption was
wrong for this A1466. The PXA registers at `0x000` are real, but the LPSS
capability/remap window read back as `0xffffffff`, which matches the wrong
generation layout rather than a ready Broadwell controller.

The current image treats PCI `8086:9cba` as the older LPT/Broadwell LPSS SPI
layout:

- PXA SPI device registers at `0x000`,
- LPSS private registers at `0x800`,
- profile-specific private offsets for `general=0x08`, `ssp=0x0c`, and
  chip-select control at `0x18`,
- no capability-register gate for this profile, and
- compact raw private-register output at offsets `raw00` through `raw1c`.

Expected next hardware signal:

```text
lpss-spi: ... mapped_phys=... mmio_virt=... mapped=1
lpss-spi: profile=lpt private_base=800 prepared=...
lpss-spi-regs-pre: sscr0=... sscr1=... sssr=...
lpss-spi-priv-pre: profile=lpt base=800 raw00=... raw04=... raw08=... raw0c=... raw10=... raw14=... raw18=... raw1c=...
lpss-spi-regs: sscr0=... sscr1=... sssr=...
lpss-spi-regs: enabled=... busy=... tx_not_full=... rx_not_empty=...
lpss-spi-priv: profile=lpt base=800 raw00=... raw04=... raw08=... raw0c=... raw10=... raw14=... raw18=... raw1c=...
```

If `profile=lpt private_base=800` prints non-`ffffffff` raw private values and
the status register reads cleanly, the next step is a small polling SPI
transport. If the raw private values still read as all `ffffffff`, stay on BAR
layout, power/resource state, and PCI/ACPI mapping diagnostics before attempting
topcase transactions.

## What Is Not Implemented Yet

- No active SPI transfers.
- No full LPSS SPI transfer configuration sequence beyond the early
  profile-specific setup used for diagnostics.
- No ACPI method execution for `SIEN`, `SIST`, `UIEN`, or `UIST`.
- No topcase resource parser for `_CRS` beyond structural diagnostics.
- No Apple HSSPI packet framing.
- No keyboard report injection from the topcase.
- No trackpad report parser or pointer injection from the topcase.
- No interrupt or DMA path for the SPI controller.

## Next Driver Plan

1. Confirm whether the new capture reports `profile=lpt private_base=800` and
   non-`ffffffff` raw private registers on the A1466.
2. Add a tiny LPSS SPI register model in `lpss_spi.c`: controller-id status,
   FIFO status, timeout status, and a safe idle check.
3. Add a non-invasive self-check command that verifies MMIO stability by reading
   the same status registers twice and confirming reserved/steady fields do not
   behave like unmapped memory.
4. Parse enough `_CRS` structure around `SPI1` and `SPIT` to extract:
   SPI chip select, IRQ/GPE/GPIO hints, and the method/resource names that need
   to be mirrored.
5. Implement the first polling transfer function with the controller disabled,
   configured, and re-enabled only inside a bounded operation.
6. Implement a read/status transaction that can identify the Apple topcase
   endpoint without requiring interrupts.
7. Add HSSPI packet framing and feed keyboard boot reports into the existing
   keyboard event path.
8. Once keyboard input is reliable, add trackpad pointer mode, then revisit
   interrupts/DMA for responsiveness.

## Hardware Capture Checklist

When hardware is available again, capture these lines first:

```text
acpi-input-topcase:
lpss-spi: spi1 ...
lpss-spi: bar0=... mapped_phys=... mmio_virt=... mapped=...
lpss-spi-regs:
```

The most important single question for the next boot is whether
`lpss-spi-regs` prints real register values.

## Next Hardware Session Handoff

Use the current `build/srvros-usb.img` from the GUI close-out pass. The UI work
is stable enough to stop being the active blocker: hidden-QEMU `displayd`
smokes passed, and a live QEMU run did not show exceptions. If the hardware GUI
is launched, use it mainly as a framebuffer sanity check after collecting input
diagnostics.

The default `/srvros` boot entry now includes `srvros.a1466.capture=1`, so the
kernel automatically runs the capture below after boot and persists it through
`/fat/var/log/boot.log` when `/fat` is writable. This avoids depending on the
built-in keyboard before the SPI topcase path works.
The capture ends with a compact `== final spi/input summary ==` section so a
photo of the final screen should still include the most important SPI register
and input-path state even if the longer `acpiinput` dump has scrolled.

If an external keyboard is available, the equivalent manual capture is:

```text
hwdiag
dmesg 8192
spi
spiregs
acpiinput
pci
xhci
block
```

Decision point:

- If `spiregs` reports stable register values, implement a read-only LPSS SPI
  controller self-check and then the first bounded polling transaction.
- If `spiregs` still reports unavailable MMIO, stay on BAR/page alignment,
  PCI command decode, and virtual mapping diagnostics.
- If ACPI no longer reports `SPI1`/`SPIT`/`SIEN`/`SIST`/`UIEN`/`UIST`, capture
  the complete `acpiinput` output before changing the parser.
