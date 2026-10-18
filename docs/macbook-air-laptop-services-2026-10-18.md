# MacBook Air laptop-service bring-up

Read-only inventory taken from the target Ubuntu host on 2026-10-18, after the
native desktop VM work. No power, display, EC or firmware settings were changed.
The raw local capture is `build/hardware/desktop-power-20261018/`; repeat with:

```sh
python3 tools/capture_linux_hardware.py
```

The collector now includes `laptop-state.json`, an explicit allowlist of Linux
power/backlight properties and firmware/driver associations. It does not read
battery serial numbers or input events. Values retain Linux units and are not
an atomic snapshot. The unprivileged capture records unavailable ACPI-table
reads in `capture.json`; an unread table is not evidence that it is absent.

## Observed target

| Component | Observation |
| --- | --- |
| Machine | MacBookAir7,2; Intel Broadwell-U |
| Display | Intel HD Graphics 6000, PCI `8086:1626`; eDP panel |
| Backlight | `intel_backlight`, type `raw`; observed actual/requested 852, maximum 2777 |
| AC supply | `ADP1`, `ACPI0003`, Linux platform driver `ac`; online at capture |
| Battery | `BAT0`, `ACPI0002`, Linux platform driver `acpi-sbs` |
| Battery namespace | `\_SB_.PCI0.LPCB.EC__.SMB0.SBS0` |
| SMBus namespace | `\_SB_.PCI0.LPCB.EC__.SMB0`, `ACPI0001` |
| Embedded controller | `\_SB_.PCI0.LPCB.EC__`, `PNP0C09` |
| Built-in Wi-Fi | Broadcom BCM4360, PCI `14e4:43a0` |
| USB networking | No USB Ethernet adapter present in this capture |

Battery status and reported capacity are separate observations: Linux reported
`Full`, capacity 86, and zero instantaneous current. The desktop must preserve
provider semantics and distinguish unknown/stale readings rather than invent a
percentage from unrelated fields or infer charging from an AC connection alone.

## Battery/AC implementation sequence

1. Capture and inspect this machine's relevant ACPI namespace/resources. Resolve
   the EC I/O resources and SMBus controller configuration from firmware evidence;
   do not assume that finding `_BIF`/`_BST` is the target battery path.
2. Implement the required bounded EC transaction transport, serialization,
   timeout/error handling and query/event ownership. Keep waiting outside GUI and
   interrupt-disabled paths. Decide the AML evaluation strategy explicitly;
   SrvrOS's current ACPI scanner is not a general AML interpreter.
3. Add the EC-backed SMBus host path and read-only SBS sampling. Preserve raw
   state, signed current, unit/scaling metadata, validity and sample age. Start
   with adapter presence, battery presence, capacity and charging state.
4. Publish a versioned cached power snapshot to userspace. System's existing
   properties/table controls can display it; the global status area can consume
   the same snapshot. Unsupported hardware must remain explicitly unavailable.
5. Validate AC connect/disconnect and repeated sampling on the laptop while
   typing and doing file I/O. A stopped or timed-out provider must not freeze the
   desktop or leave a convincing stale “charging” indicator.

This sequence follows the observed ACPI0001/ACPI0002 chain. As protocol references,
Linux's pinned [SBS driver](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/acpi/sbs.c)
matches ACPI0002 and separates reported state of charge from charging direction;
its [SMBus host-controller driver](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/acpi/sbshc.c)
uses EC transactions and the firmware `_EC` configuration. These files are GPL
references; no implementation was copied into SrvrOS.

## Brightness implementation sequence

The existing `intel_gfx.c` recognizes this Broadwell device but currently maps
only the initial 16 KiB of BAR0 and reports planned acceleration. It does not
provide a panel/backlight backend. Map and validate the required display
register range only after resolving the active panel's control path and firmware
state. Preserve a working panel mode and the existing software framebuffer.

Start with read-only discovery of the current duty/range/control mode. Then add
a bounded brightness provider with clamping, an explicit unavailable state, and
readback; connect it to the shared slider in Settings. Test keyboard and USB
storage responsiveness through repeated brightness changes. Automatic blanking
and lid policy follow only after manual brightness is reliable. Linux's pinned
[Intel backlight implementation](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/gpu/drm/i915/display/intel_backlight.c)
is a reference for the multiple possible panel/backlight paths, not proof that
one register sequence is correct for this machine.

## Acceptance order after this VM-only session

1. Run the expanded native desktop on the laptop: open/edit/save/reopen a USB
   document, use Terminal and Preview, tile/resize, and exercise keyboard and
   trackpad during I/O. Retain the accepted recovery image.
2. Bring up read-only battery/AC reporting, then manual brightness.
3. When a USB network adapter arrives, capture VID/PID, interfaces and chipset,
   then choose the driver target. Preserve the e1000/QEMU regression meanwhile.
4. Treat safe shutdown, lid behavior and suspend/resume as separate acceptance
   gates covering storage flushing and device reinitialization. They are not
   implied by battery reporting or brightness control.
