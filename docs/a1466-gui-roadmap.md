# MacBook Air A1466 Bring-Up and GUI Roadmap

This milestone has two related goals:

1. Boot srvros on a real Apple MacBook Air A1466, especially the 13-inch i7
   Broadwell configuration.
2. Replace the prototype desktop/widget experiment with a resolution-aware GUI
   architecture that can grow into a real desktop.

The current QEMU path remains the main regression harness. Real hardware bring-up
should add observability and fallback paths before depending on laptop-only
devices.

## Target Hardware Profile

The A1466 model number spans several 13-inch MacBook Air revisions. The target
called out here is the i7 13-inch configuration sold around the Early 2015/Mid
2017 family, commonly identified as `MacBookAir7,2`.

Expected baseline:

- CPU: Intel Broadwell ULT, with the i7 configuration using an i7-5650U.
- Display: 13.3-inch internal panel, native 1440x900. Apple also lists common
  lower modes including 1280x800, 1024x768, and 800x600.
- Graphics: Intel HD Graphics 6000. Initial srvros support should use the UEFI
  GOP/Limine linear framebuffer, not native i915 modesetting.
- Storage: PCIe-based flash storage. The 13-inch model is documented as PCIe
  2.0 four-lane storage; in practice this family is often exposed as an Apple
  PCIe/AHCI SSD, but swapped SSDs may require NVMe.
- Input: built-in keyboard and trackpad are not safe to assume as legacy PS/2.
  Linux hardware reports for this generation show an Apple internal
  keyboard/trackpad on USB, so xHCI plus USB HID is the practical target.
- Ports: two USB 3 ports, Thunderbolt 2, SDXC slot, audio, Wi-Fi/Bluetooth.
- Networking: internal Wi-Fi is Broadcom 802.11ac and is not a near-term target.
  Early real-machine networking should use a USB Ethernet adapter after USB is
  available, or stay offline until a supported NIC path exists.

Sources used for the initial profile:

- Apple Early 2015 technical specifications:
  https://support.apple.com/en-kz/111956
- EveryMac Early 2015/Mid 2017 comparison:
  https://everymac.com/systems/apple/macbook-air/macbook-air-faq/differences-between-macbook-air-early-2015-models.html
- Linux install report showing Apple internal keyboard/trackpad under USB:
  https://atodorov.org/blog/2015/04/26/installing-red-hat-enterprise-linux-7-on-macbook-air-2015/

## Current Boot State

srvros currently boots through Limine in QEMU, initializes a higher-half kernel,
uses Limine's framebuffer, mounts an exFAT image from AHCI or initramfs-backed
block devices, and exposes shell, filesystem, networking, process, pthread, and
ports coverage through QEMU smoke tests.

The existing ISO already includes BIOS and UEFI boot files:

- `EFI/BOOT/BOOTX64.EFI`
- `boot/limine/limine-uefi-cd.bin`
- `boot/limine/limine-bios-cd.bin`

That is enough for QEMU UEFI testing, but real Apple firmware bring-up should
also produce a USB-friendly FAT32 ESP image. A hybrid ISO can work on some
machines, but the safest debug artifact for a MacBook is a GPT disk image with a
FAT32 EFI System Partition and the same kernel/initramfs payload.

## Real Hardware Bring-Up Plan

### 1. Add a Real-Hardware Boot Artifact

- `make usb-image` creates `build/srvros-usb.img`, a GPT disk image.
- Partition 1: FAT32 ESP with `EFI/BOOT/BOOTX64.EFI`, Limine config, kernel,
  and initramfs.
- Partition 2: exFAT srvros data volume, matching the QEMU AHCI image
  layout.
- Document writing the image with platform-specific commands and a clear
  warning about selecting the correct disk.
- `tools/uefi_usb_smoke.py` boots the GPT/ESP artifact through OVMF from a
  temporary copy, verifies PCIe ECAM, and confirms `/fat` mounts from the GPT
  exFAT data partition.

### 2. Strengthen Early Observability

- Keep framebuffer console as the first-class real-machine console.
- Boot logs are mirrored into an in-memory ring buffer, exposed through
  `dmesg`, and persisted to `/fat/var/log/boot.log` after the filesystem mounts.
- `bootinfo` covers framebuffer geometry, ACPI tables, PCI/PCIe config mode,
  Intel graphics discovery, xHCI diagnostics, timers, and storage. `hwdiag`
  now wraps that with full PCI, memory, scheduler/workqueue, network, mounts,
  `/fat` fsck, process list, and boot-log tail capture for first-boot reports.
- Fatal exceptions clear to a framebuffer panic screen with register details and
  recent boot-log context.

### 3. Broaden ACPI and PCI Enumeration

- Parse ACPI MCFG and add PCIe ECAM config-space access.
- Keep legacy CF8/CFC as a fallback.
- Surface model identifiers and firmware table data when available.
- Start collecting FADT/HPET data for timer calibration and shutdown/reboot.
- Improve PCI interrupt routing beyond the current legacy interrupt-line model:
  MSI/MSI-X support is likely cleaner for PCIe devices than relying on firmware
  INTx routing.

### 4. Calibrate Timers for Real CPUs

- Replace fixed LAPIC initial-count assumptions with calibration.
- Prefer HPET or ACPI PM timer when present; PIT can remain a fallback.
- Add a timer diagnostic that reports measured tick frequency.

### 5. Bring Up USB xHCI and HID

- xHCI PCI discovery, MMIO mapping, controller halt/reset, command/event rings,
  No-op and Enable Slot command completion, and connected root-port reset
  diagnostics are in place. The first USB device path now allocates xHCI device
  contexts, addresses/configures QEMU's USB keyboard and mouse, walks
  descriptors, powers/resets QEMU USB2 hub ports, routes downstream devices, and
  configures HID boot keyboard/mouse plus generic absolute pointer interrupt
  endpoints. QEMU smoke coverage now types a monitor command through the USB
  keyboard path, sends synthetic relative and absolute pointer moves, and
  verifies HID reports both directly on root ports and behind a QEMU hub. xHCI
  status also reports descriptor class/vendor/product details, parent hub port,
  route strings, and whether a pointer is absolute, which should make the first
  A1466 USB input boot log much more actionable.
- Validate the same USB keyboard report path on the A1466 internal keyboard.
- Validate the same USB mouse report path on the A1466 internal trackpad.
- Validate the generic absolute pointer path on the A1466 internal trackpad.
- Validate the hub-routed input path against the A1466 internal USB topology.
- Add broader USB hub enumeration beyond one USB2 hub layer.
- Add a real HID report-descriptor parser for non-boot trackpad features.

This is the main real-machine usability blocker. Without it, the MacBook may
boot visually but be hard or impossible to control without firmware-provided
legacy emulation.

### 6. Storage on the Internal SSD

- First try existing AHCI on the Apple SSD, after PCIe/ECAM improvements.
- Add AHCI interrupt completion after basic real-machine boot is stable.
- Add NVMe discovery/read/write as the fallback for replacement SSDs or newer
  storage variants.
- Do not write to the internal disk by default during early bring-up. Prefer USB
  or a test partition until the storage stack has stronger recovery coverage.

### 7. Networking on Real Hardware

- Defer Broadcom Wi-Fi.
- After USB exists, target a simple USB Ethernet class/device family, or use a
  known supported USB NIC and write that driver.
- Reuse the current IPv4/TCP/UDP stack once a real NIC RX/TX path exists.

## Existing GUI Review

The prototype GUI has useful pieces:

- `desktop` is already a userspace window server process.
- GUI apps such as `calcgui`, `notesgui`, `notes2`, `calc2`, `textedit`, and
  `imgedit` are separate ring-3 processes.
- The userspace UI layer has surfaces, parent/child composition, dirty marking,
  mouse hit testing, text entry, image controls, and simple events.
- The desktop compositor can redraw damaged regions and draw a software cursor.
- `displayd` is now a parallel compositor seed with a dynamically allocated root
  backbuffer, resolution-derived shell metrics, GUI server registration, and a
  smoke mode for hidden-QEMU regression coverage.
- Kernel-managed GUI surfaces now provide an app-to-compositor pixel path:
  clients can create a surface, blit pixels into it, send v2 create/damage
  messages, and let `displayd` copy the damaged surface into its root buffer.

The parts that should be replaced:

- Fixed pixel constants dominate the desktop and apps: client sizes, button
  sizes, title bars, launcher geometry, and static backing buffers.
- Apps describe server-owned widgets through a narrow fixed-size message ABI
  rather than owning a drawable window surface.
- The framebuffer API exposes info, put-pixel, fill-rect, and bulk blit
  syscalls. GUI v2 has kernel-managed surfaces, but not true mapped/shared
  user pages or a present queue yet.
- Text rendering is small bitmap glyph drawing with no font metrics, wrapping
  model, or DPI-scale concept.
- Keyboard focus, pointer capture, resize/configure events, clipboard, drag
  semantics, and multi-monitor/display-change events are absent or implicit.
- GUI queues are fixed-size process mailboxes and do not provide backpressure,
  shared memory handles, or structured copyin/copyout.

The right move is not to keep adding features to `desktop.c`. The mature version
should make `desktop` a compositor/window manager on top of a display server ABI,
while the toolkit becomes an app-side library that draws into client-owned
surfaces.

## GUI Replacement Architecture

### Display Layer

The kernel should expose display information and a safe high-throughput drawing
path:

- `display_info`: width, height, pitch, bpp, memory model, RGB mask sizes and
  shifts, framebuffer generation, and preferred scale.
- Accelerator metadata: backend kind, device-present flags, mapped-MMIO flags,
  and explicit staged capabilities for software, Intel blitter, and Intel render
  backends.
- `display_map` or controlled compositor-only framebuffer mapping.
- `surface_create`, `surface_destroy`, surface copy/blit, and later
  `surface_map` plus surface metadata for client buffers.
- `present`/`damage` calls that operate on rectangles, not pixels.

The first implementation can stay entirely software-composited and 32-bit RGB.
Native Intel modesetting, acceleration, and external monitor handling can come
later.

### Compositor and Window Manager

Replace the current fixed desktop with a compositor process, tentatively
`displayd`, responsible for:

- Owning the hardware framebuffer or its mapped equivalent.
- Tracking windows, z-order, focus, pointer capture, decorations, minimization,
  and close requests.
- Compositing client surfaces into a root backbuffer.
- Presenting only damaged rectangles.
- Drawing the software cursor as its own damage source.
- Scaling theme metrics from display size and, later, physical DPI/EDID.

The compositor should not own application widgets. It should decorate windows
and route input.

The checked-in `displayd` slice owns a root backbuffer, draws a
resolution-aware shell scene, receives legacy GUI IPC messages, accepts v2
surface-window and damage messages, composites kernel-managed client surfaces,
routes configure/focus/pointer/key events back to surface clients, and presents
dirty cursor rectangles. Its left dock now launches GUI2 clients as independent
processes, clamps new windows inside the work area, staggers duplicate app
instances, and treats unavailable launchers as recoverable UI actions.

### GUI Protocol V2

Add a new protocol beside the legacy one:

- Client to server: create window, attach surface, set title, damage rect,
  request resize/min/max constraints, set cursor, close/minimize request.
- Server to client: configure size/scale, expose, focus in/out, pointer move,
  pointer button, wheel, key down/up, text input, close request.
- Events should carry window id, sequence id, modifiers, coordinates in window
  logical units, and raw pixel coordinates when useful.
- Keep the old fixed-widget protocol as a compatibility bridge until the sample
  apps have been ported.

Current v2 status: `GUI_MSG_V2_CREATE_SURFACE_WINDOW`,
`GUI_MSG_V2_DAMAGE_SURFACE`, `GUI_MSG_V2_DESTROY_SURFACE`,
`GUI_MSG_V2_EVENT_CONFIGURE`, `GUI_MSG_V2_EVENT_FOCUS`,
`GUI_MSG_V2_EVENT_POINTER_MOVE`, `GUI_MSG_V2_EVENT_POINTER_BUTTON`, and
`GUI_MSG_V2_EVENT_KEY_DOWN` are wired through the existing GUI queue. `displayd`
also sends the existing close event to v2 apps from compositor-owned frame
buttons, and owns z-order raise, title-bar dragging, and minimize state for
surface windows. It also owns dock launchers for `/fat/bin/notes2`,
`/fat/bin/gui2demo`, `/fat/bin/surfacedemo`, and `/fat/bin/calc2`, with
hidden-QEMU coverage for duplicate instance placement and dock-launched GUI2
clients. Bottom-right resize grips now send configure events, and GUI2 clients
recreate their kernel-managed backing surface and redraw at the requested size.
Pixel storage is currently kernel-managed through
`gui_surface_create`,
`gui_surface_blit`, `gui_surface_copy`, and `gui_surface_destroy` wrappers.
`gui2` is the first app-side helper library for that path, wrapping window
open/close, resize, dirty presents, event polling, theme/layout helpers,
focus-aware widget dispatch, buttons, and textboxes.
`/fat/bin/surfacedemo` now uses it for a raw animated surface, and
`/fat/bin/gui2demo` uses it for the first v2 widget sample. `/fat/bin/notes2`
and `/fat/bin/calc2` use the same toolkit pieces for small utility apps under
`displayd`.

### Toolkit

Grow the new app-side toolkit library so it owns widgets and renders into the
client surface:

- Rect, point, size, color, and damage-list primitives.
- Rows/columns, stack, anchors, spacer, and simple grid layout.
- Theme metrics in logical pixels: padding, title-independent control height,
  border width, focus rings, colors, and font metrics.
- Controls: label, button, text entry, multiline text area, image view, checkbox,
  radio/segmented controls, list, menu, scrollbar, and canvas.
- Event dispatch with capture/bubble semantics and a returned consumer.
- Keyboard focus traversal and text input separated from raw key events.

This preserves the spirit of the previous `ui_element` tree, but moves it to
the correct side of the process boundary.

### Resolution Awareness

Use logical pixels internally and compute scale from the display:

- 1.0x: small/standard panels, including 800x600 through 1440x900.
- 1.25x or 1.5x: larger high-density panels when EDID/physical size indicates
  it, or when vertical resolution is high enough to warrant it.
- Launcher, taskbar, title bars, hit targets, and default app windows should be
  computed from theme metrics and work-area fractions, not constants.
- Minimum window sizes should be enforced in logical units.
- App windows should receive configure events when the framebuffer size changes
  or when launched on a display with a different scale.

For the A1466 internal panel, a 1440x900 mode should look like a comfortable
1.0x desktop with default windows sized as fractions of the work area.

## Implementation Slices

### Slice A: Real-Boot Scaffolding

- Add GPT/ESP USB image generation.
- Add QEMU UEFI test coverage for that image.
- Add boot logging ring buffer and `/fat/var/log/boot.log` persistence.
- Add `bootinfo`.
- Add `hwdiag` for one-command real-hardware diagnostic capture.

### Slice B: Hardware Discovery

- Add ACPI MCFG parsing and ECAM PCI access.
- Add FADT/HPET discovery and timer calibration.
- Add richer PCI dumps and interrupt-routing diagnostics.

### Slice C: USB Control Path

- Add xHCI skeleton, reset, rings, command completion, and root-port inventory.
  QEMU smoke coverage now verifies No-op, Enable Slot, and USB HID port reset
  through the xHCI path.
- Add USB descriptor walking and first-device address/configuration.
- Add USB HID boot keyboard endpoint configuration.
- Add USB HID boot mouse pointer events.
- Add class/vendor/product diagnostics for unsupported and hub-like devices.
- Add QEMU USB hub smoke coverage and one-layer USB2 hub-routed HID input.

### Slice D: Display ABI

- Extend framebuffer info to a versioned display info ABI.
- Detect Intel integrated graphics early and advertise accelerator metadata to
  userspace without making the GUI depend on acceleration being available.
- Add compositor-only framebuffer mapping or a bulk blit/present syscall.
  The first bulk `gfx_blit_rect` syscall is in place for root-backbuffer
  dirty-rect presents; shared/mapped surfaces remain future work.
- Add true mapped shared-surface pages on top of the current kernel-managed
  surface allocation/copy path.
- Add damage-rectangle helpers and tests.

### Slice D2: Intel Acceleration Backend

- Map the Intel graphics MMIO BAR and expose `gpu` diagnostics.
- Implement a GPU memory manager for pages visible through the global GTT.
- Add a kernel-owned command submission path with completion fences and hang
  detection.
- Bring up the Broadwell blitter engine first for accelerated fills, copies,
  scrolls, and window moves.
- Keep the compositor backend-pluggable so the GUI can run through software,
  Intel blitter, or later Intel render-engine paths with the same surface and
  damage protocol.

### Slice E: Compositor V2

- Add `displayd` with a root backbuffer, window records, damage tracking,
  cursor damage, decorations, and input routing.
- Add protocol v2 queues/messages.
- Keep the legacy desktop bridge while new apps are ported.

### Slice F: Toolkit V2 and Apps

- Add app-side layout/theme/render library.
- Port calculator, notes, text editor, and paint/image editor to client-owned
  surfaces. Notes and calculator have first GUI2 ports as `/fat/bin/notes2` and
  `/fat/bin/calc2`; text editor and paint remain on the legacy GUI path.
- Add resize handling and per-resolution smoke tests. The first resize path is
  implemented for GUI2 clients on the kernel-managed surface-copy backend, and
  `displayd_resolution_smoke.py` covers 800x600, 1280x800, 1440x900, and
  1920x1080.

## Testing Plan

QEMU tests before hardware:

- Boot ISO in existing BIOS-ish and UEFI modes.
- Boot GPT/ESP disk image in QEMU UEFI.
- Run GUI smoke at 800x600, 1280x800, 1440x900, and 1920x1080.
- Verify no fatal exceptions while launching, moving, resizing, minimizing, and
  closing multiple windows.
- Add framebuffer screenshot/pixel checks for nonblank root, cursor, window
  decoration, and app content.

Real hardware tests:

- First boot with read-only internal storage behavior.
- Confirm framebuffer dimensions, ACPI table list, PCI/ECAM enumeration, timer
  calibration, and boot log persistence.
- Confirm keyboard input through USB HID.
- Confirm pointer input through USB HID.
- Confirm internal storage appears read-only before enabling writes.

## Success Criteria

Real-machine milestone:

- A1466 boots to a visible srvros console from a USB-created artifact.
- A keyboard works well enough to run monitor/shell commands.
- Boot diagnostics can be saved and inspected after reboot.
- Storage is detected without writing to the internal disk by default.

GUI milestone:

- `displayd` replaces the old fixed desktop as the normal GUI entry point.
- Apps own drawable surfaces and receive input/configure events.
- Windows launch, move, resize, minimize, close, and redraw smoothly.
- The desktop scales cleanly across QEMU test resolutions and the A1466
  1440x900 panel.
