# Apple hardware expansion plan

Planning checkpoint: 2026-10-18. The primary development machine remains
MacBookAir7,2 (A1466). Add these four model identifiers as planned targets,
not as machines already supported. Enclosure numbers below are the user's
target descriptions; driver selection must use observed devices and firmware.

## Target matrix

| Target | Published baseline / working expectation | First additional work | Status |
| --- | --- | --- | --- |
| MacBookAir7,1 / A1465 | Early 2015 11-inch Air; HD Graphics 6000, 1366x768 panel, PCIe flash | Compare input descriptors and firmware with 7,2; fit every app/checklist to the smaller display; verify backlight, audio and battery resources | Inventory pending |
| MacBookPro11,2 / A1398 | Late 2013 or Mid 2014 15-inch Retina family; plan for integrated Iris Pro graphics, verify actual PCI inventory | 2880x1800 framebuffer performance and UI scaling; topcase report format; HDA routes; backlight and multiple sensor/fan channels | Inventory pending |
| MacBookPro11,3 / A1398 | Same Retina generations; plan for Iris Pro plus GeForce GT 750M, verify actual PCI inventory | Identify firmware-selected display GPU, GMUX resources and display/audio dependencies; preserve the working boot display before considering switching | Inventory pending |
| Macmini7,1 / A1347 | Late 2014 Mini; HD Graphics 5000 or Iris depending on configuration; HDMI, wired Gigabit Ethernet; HDD/flash/Fusion options | External-display boot, wired USB input, Ethernet controller identification, storage topology, speaker/line/headphone routes | Inventory pending |

Apple's [Pro identification list](https://support.apple.com/en-us/108052)
places both 11,2 and 11,3 in Late 2013 and Mid 2014; record year/configuration
as well as the identifier. The [2013](https://support.apple.com/en-gb/111971)
and [2014 specifications](https://support.apple.com/en-gb/111935) establish the
Retina resolution and Intel/NVIDIA graphics options. The per-identifier GPU
split above is a planning expectation pending inventory, not a driver match.
Apple identifies [Air7,1](https://support.apple.com/en-us/102869) and gives its
[display/graphics specifications](https://support.apple.com/en-gb/112441).
The [Mini identification](https://support.apple.com/en-us/102852) and
[specifications](https://support.apple.com/en-ca/111931) establish its variants.

Each machine will have its own agent developing on-device under Ubuntu, with
an exFAT USB workspace. Use the [setup/handoff guide](ubuntu-device-agent-setup.md)
and [per-machine records](machines/README.md). Agents can inventory and develop
independently; coordinate changes to common code through the integration owner.
Air7,1, Pro11,2, Mini7,1, then Pro11,3 is an estimated integration order based
on reuse/display complexity, not a requirement to wait for another agent's
machine. Capture each target before implementing its model quirks.

## Current work and shared dependencies

1. Finish physical two-finger secondary click, preserving primary held-click
   steering when a second finger joins. Independently test tap and press modes.
2. Investigate the System app's input-timeout closure from the 7,2 device trial.
3. Measure scheduler idle time, wakeups and polling under idle and controlled
   load. The 7,2 trial reported 83–85 C; this is a reason to measure, not proof
   of a particular power or thermal defect. Keep fan control separate.
4. Continue battery/AC runtime work, headphone routing and USB Ethernet support
   matched to actual adapter descriptors. Keep the unresolved AML probe out of
   ordinary startup. Live ACPI must have bounded operations, scheduled workers
   and explicit SCI/GPE/EC ownership before it becomes a shared laptop service.
5. Prepare the additional models through the inventory and capability work
   below. Do not delay the current Air fixes for speculative driver additions.

## Inventory preparation

Use a Linux boot on each physical machine, initially without altering its boot
installation. The existing read-only collector is the starting point:

```sh
sudo python3 tools/capture_linux_hardware.py --output build/hardware/<model>-<date>
```

Replace the placeholders and use a new directory. Preserve `capture.json` and
its missing/partial-read list. Record model/year, firmware revision, CPU, RAM,
display/connection, original versus replaced storage, and attached peripherals.
No machine serial number is needed. Each local agent should collect its own
inventory during setup; none has yet been supplied for the additional machines.

The collector now covers the following evidence. Inspect it on each new machine
before calling a capture complete; a collected descriptor is not driver support:

- Enumerate Apple HID interfaces across product IDs, preserving HID IDs and
  report descriptors alongside the generic USB interface/endpoint descriptors.
  Missing report reads are explicit partial captures. Never capture keystrokes.
- Record board model identifiers (without serials/UUIDs), DRM connector modes,
  EDID and driver associations, and read-only GPU boot-display association.
- Preserve all HDA codec dumps and subsystem IDs, distinguish analog audio
  from HDMI/DisplayPort audio, and inventory every exposed fan channel.
- Record storage controller class/protocol and physical disk topology. PCIe
  flash does not establish AHCI versus NVMe; replacement drives can differ.
  Treat Mini Fusion configurations as separate physical devices until an
  explicit storage plan exists. No automatic mount or write to internal disks.
- Capture Ethernet PCI IDs and Linux driver bindings on the Mini, plus actual
  USB network adapter descriptors. Do not infer an Ethernet driver from the
  presence of an RJ-45 connector or reuse QEMU's e1000 assumption.

The generalized collector passed synthetic multi-product/topology tests and a
read-only Air7,2 capture on 2026-10-19 with no unavailable reads: two `05ac:0291`
HID reports, the Intel `8086:1626` boot display, a connected 1440x900 eDP panel,
and internal/USB block topology. No sibling inventory or acceptance is implied.

Capture files stay local under `build/hardware/`. Publish concise inventories
and sanitized replay fixtures in the repo when evidence is available. Decoder
fixtures must record the source model, protocol and expected behavior.

## Driver and desktop preparation

The current audio initialization accepts Intel `9ca0` plus two VM controllers;
SMC probing requires the current display/LPC pair and firmware match. The Apple
trackpad decoder uses one fixed report layout. These are explicit scope limits
in `audio.c`, `smc.c` and `apple_trackpad.c`, not generic Apple support.

Build capability tables from verified descriptors/resources. Separate HDA
controller mechanics from codec/subsystem routing and amplifier quirks. Separate
trackpad report decoding and calibration from the shared gesture policy.
Discover backlight and EC resources per model; do not reuse 7,2 register offsets,
firmware hashes, SMC keys or battery namespace paths simply because it is a Mac.
Missing optional providers must leave a usable desktop with unavailable status.

Start each machine with the firmware/Limine framebuffer. For Retina, test
2880x1800, padded pitch, damage copying, allocation limits and readable scaled
controls; increasing font size alone is not complete HiDPI support. Test all
dialogs at 1366x768 as well. The fixed eight-window cache allocation should be
reviewed before scaling backing surfaces to Retina sizes.

For Pro11,3, first record which GPU owns the boot framebuffer and leave that
route intact. Display switching, backlight and discrete-GPU power are a separate
milestone. Linux's [hybrid graphics documentation](https://docs.kernel.org/gpu/vga-switcheroo.html)
explains MacBook Pro muxing and the dependency between GPU and display-audio
power; it is a design reference, not evidence that SrvrOS implements it.
Initial framebuffer success does not imply acceleration or external-display
hotplug. Mini display acceptance starts with one boot-attached monitor and one
connection type, then expands to HDMI/DisplayPort separately.

## Acceptance gates

For every model and relevant hardware variant, retain the release manifest,
inventory, boot command line, test results, screenshots and clean-exit report.
Keep a capability ledger with separate **planned**, **implemented/VM-tested**,
**physically accepted**, **failed** and **not tested** entries.

1. Boot from a removable, manifest-verified image into the RAM-backed system;
   preserve the existing installed OS and provide early on-screen diagnostics.
   Ordinary boot must not depend on optional AML diagnostics completing.
2. Validate keyboard/pointer, monitor geometry, rendering, USB workspace
   write/read/flush and failure reporting. Repeat cold and warm boots.
3. Validate gestures on laptops, sound routes/volume, brightness where applicable,
   live sensor freshness, and idle/load responsiveness. Test while storage is busy.
4. Accept wired networking by actual device/driver: link changes, DHCP, packets,
   disconnect/reconnect and concurrent input/storage. Wi-Fi, Bluetooth, camera,
   SD readers and Thunderbolt devices remain separately planned capabilities.
5. Add battery/AC, lid behavior and orderly power-off individually. Suspend/resume,
   GPU switching and native internal-disk writes require their own acceptance;
   none follow automatically from a successful desktop boot.

Next concrete prep deliverables: generalized inventory collector with host
fixtures; four inventory-backed capability sheets; VM small-screen/Retina
coverage; a removable trial image recipe that does not assume this host's GRUB
installation or workspace UUID. Actual driver enablement follows those records.
