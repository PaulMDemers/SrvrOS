# Current status

Updated 2026-10-23. SrvrOS is an experimental x86_64 desktop OS with a shell and
server capabilities. This page describes the current source baseline.
Check the source commit of an image before assuming it contains a change;
there is no supported stable binary release yet.

## Hardware

| Target | Evidence | Remaining work |
| --- | --- | --- |
| QEMU q35 | Repeatable boot, CLI, filesystem, network and desktop regressions | Emulator coverage cannot validate physical controllers or firmware |
| MacBookAir7,2 / A1466 | Active reference machine; boot, built-in input, desktop, exFAT workspace, brightness, speaker volume and sensors exercised | Combined ordinary-session services, targeted input cases, audio routes and network adapter acceptance |
| MacBookAir6,1 | Console/input, USB workspace and bounded desktop pointer/drag/scroll trials | Preference restoration and additional devices have separate acceptance |
| MacBookAir7,1 / A1465 | Planned | Capture this model and run its own acceptance cycle |
| MacBookPro11,2 / A1398 | Native desktop/input, scaled display, brightness, speakers, CPU/two-fan telemetry and captured USB Ethernet accepted | USB Wi-Fi diagnostics in progress; battery and headphone checks deferred |
| MacBookPro11,3 / A1398 | Planned | Separate GPU/firmware inventory; no assumed graphics switching support |
| Macmini7,1 / late-2014 A1347 | Planned | Inventory, storage, display and external input bring-up |

The [machine records](machines/README.md) own exact physical evidence. Sharing
an enclosure designation or CPU family does not establish driver compatibility.

## Desktop and input

The current desktop is `/fat/bin/deskd`, with twelve applications and a common
widget library. It has global menus, a launcher, dock, clipboard, context menus,
notifications, screenshots, window movement/resizing, maximize and tiling.
File/image/diagnostic workers keep blocking operations away from event handling.

The reference Air has exercised typing, pointer movement, dragging, resizing,
two-finger scrolling and file work under load. Settings expose pointer speed,
acceleration, tap gestures and suppression while typing; suppression defaults
to off. Recent changes latch a physical two-finger press as secondary click,
cancel interrupted trackpad presses, preserve queued typing on pointer
cancellation, and correct compositor input/control delivery deadlines. Host/VM
regressions pass. The latest physical round reported generally good behavior,
with no recorded timeout or app failure; individual secondary-click/cancellation
cases are not all confirmed. The Terminal backspace correction passed its physical retest on
`5282cfde546ea2b5`; Text copy/paste/select-all shortcuts were explicitly
validated in the same round.
Earlier unexpected app closures are not conclusively explained by these fixes.

Current limits include eight client windows, software rendering and bounded
client surfaces up to 1440×900 logical pixels. Explicit 2× scaling passes VM
checks at 2880×1800, but is pixel enlargement, not a high-density font renderer
or Retina Mac acceptance. See the [desktop guide](native-desktop.md) and [device parity ledger](device-parity.md).

## Files, services and devices

| Area | Current boundary |
| --- | --- |
| exFAT / USB workspace | Repeated physical read/write/flush/remount and shell workflows passed; no journal or power-loss durability guarantee |
| Internal storage | AHCI available in VM; laptop test profiles use a RAM boot image and USB workspace, not an internal-disk installation |
| Networking | e1000 and small IPv4 stack exercised in QEMU; CDC ECM USB driver has VM coverage, captured Pro adapter DHCP/DNS/HTTP and cable-status UI accepted; Air adapter trial pending |
| Audio | HDA speaker volume/tone behavior confirmed on the Air; PCM file playback completed on hardware with zero reported underruns; audible channel separation/headphones still need confirmation |
| Brightness / sensors | Panel adjustment and live CPU temperature/fan readings confirmed; no automatic fan-control service |
| Battery / AC / suspend | Air7,2 native ordinary Home battery service, AC transitions and cached indicator accepted; default startup remains opt-in. No accepted suspend/resume |
| Wi-Fi / Bluetooth / NVMe | Pro USB Wi-Fi firmware/MCU/baseband diagnostics ongoing in PR19; no accepted Wi-Fi connectivity, Bluetooth or NVMe support |
| POSIX / ports | Useful subset, not full POSIX or Linux binary compatibility; full Node runtime recovery remains incomplete |
| Isolation / security | Separate user processes exist, but no security boundary suitable for untrusted workloads or exposed production services |

## Measured checkpoints and current acceptance

- Five physical filesystem/shell rounds passed on the reference Air, with retained
  outputs and a clean offline exFAT check.
- A 60-second physical desktop-idle capture measured **94.15% CPU0 idle samples**,
  one presentation and 76,384 framebuffer bytes during its observation window.
  This measures scheduler occupancy, not laptop power or battery life.
- An isolated native AML resource probe completed in 5.13 seconds with no reported
  errors/denials/leaks. Ordinary desktop startup still omits AML probing.
- A separate Q35 full-uACPI fixture passed a verified **30-minute, 100-suite**
  soak covering SCI/mutex, EC, namespace initialization and teardown/refusal
  cases. It is not a native EC or battery driver.
- Read-only native telemetry now measures **6.458 W average package power** and
  **2,083 MHz CPU0 active frequency** over 52.999 seconds inside a 60-second
  desktop capture. This excludes whole-laptop power and does not change CPU policy.

Ordinary Home battery acceptance on October 23 includes a real document saved
and reopened across two desktop lifetimes, clean runtime retirement, preserved
prior files, clean offline allocation and confirmed Ubuntu return. Screenshots
show the cached indicator. This does not establish battery life or calibration.
The combined v55 laptop-services profile is under validation.

Exact source/payload identities, evidence locations and negative tests are in
the [October 19 journal](desktop-progress-2026-10-19.md) and
[Air7,2 record](machines/macbookair7-2.md). Missing reports and halted tests are
incomplete or failed evidence, never passes. Linux inventory is evidence of the
host configuration, not a SrvrOS capability claim.
