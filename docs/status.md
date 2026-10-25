# Current status

Updated 2026-10-25. SrvrOS is an experimental x86_64 desktop OS with a shell and
server capabilities. This page describes the current source baseline.
Check the source commit of an image before assuming it contains a change;
there is no supported stable binary release yet.

## Hardware

| Target | Evidence | Remaining work |
| --- | --- | --- |
| QEMU q35 | Repeatable boot, CLI, filesystem, network and desktop regressions | Emulator coverage cannot validate physical controllers or firmware |
| MacBookAir7,2 / A1466 | Active reference machine; boot, built-in input, desktop, exFAT workspace, brightness, speaker volume and sensors exercised | Suspend/resume, measured power improvements, USB hot-unplug and broader filesystem/window scale |
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
cases were subsequently exercised in the October 23 follow-up; tap refinement is tester-accepted in PR88, although its final full diagnostic receipt is incomplete. The Terminal backspace correction passed its physical retest on
`5282cfde546ea2b5`; Text copy/paste/select-all shortcuts were explicitly
validated in the same round.
Earlier unexpected app closures are not conclusively explained by these fixes.

Air7,2 accepts sixteen-window opening, saved document integrity and clean exit
at `bf68e4e9b88b8ae7` (2026-10-24); allocation-pressure preservation and cache
retirement have host/VM evidence, without native cache totals in that round.
Current source uses dynamically allocated window/GUI records with explicit byte
budgets. Normal Text has multiple documents per process and shares a bounded lazy
file-worker pool; Files uses the same service. A combined GUI/nonblocking-pipe
wait preserves a single dispatcher and keeps accepted I/O owned until drained.
Text and Preview also share an asynchronous file chooser controller; canceled
folder reads drain before its buffers can be reused.
Local coverage includes 128 small windows, 32 real Text documents/33 exact files,
clipboard capacity, allocation refusal, worker retirement and input during
throttled QEMU writes. These do not extend native acceptance beyond its recorded
release or establish native latency/energy improvements. Covered-window repainting
now uses conservative opaque-piece tests with an exact unculled pixel oracle and
identical-workload host benchmarks. See the [runtime validation guide](desktop-runtime-validation.md)
for repeatable local gates and the remaining interactive acceptance round.

Remaining limits include software rendering, 1440×900 logical client surfaces,
process/descriptor budgets and pixel-memory admission. Explicit 2× scaling passes
VM checks at 2880×1800, but is pixel enlargement, not a high-density font renderer
or Retina Mac acceptance. See the [desktop guide](native-desktop.md),
[API evolution plan](desktop-api-evolution.md) and [device parity ledger](device-parity.md).
Parented dialogs, broader shared app services, rendering/latency profiling and
native acceptance of the new runtime remain further work.

## Files, services and devices

| Area | Current boundary |
| --- | --- |
| exFAT / USB workspace | Repeated physical read/write/flush/remount and shell workflows passed; no journal or power-loss durability guarantee |
| Internal storage | AHCI available in VM; laptop test profiles use a RAM boot image and USB workspace, not an internal-disk installation |
| Networking | e1000 and small IPv4 stack exercised in QEMU; CDC ECM USB driver has VM coverage, captured Pro adapter DHCP/DNS/HTTP and cable-status UI accepted; Air matched-adapter DHCP/DNS/HTTP and cable down/recovery accepted |
| Audio | HDA speaker volume/tone behavior confirmed on the Air; PCM file playback completed on hardware with zero reported underruns; prompted headphone/speaker controls reported successful in the v58 trial |
| Brightness / sensors | Panel adjustment and live CPU temperature/fan readings confirmed; no automatic fan-control service |
| Battery / AC / suspend | Air7,2 native ordinary Home battery service, AC transitions and cached indicator accepted; default startup remains opt-in. No accepted suspend/resume |
| Wi-Fi / Bluetooth / NVMe | Pro USB Wi-Fi firmware/MCU/baseband diagnostics ongoing in PR19; no accepted Wi-Fi connectivity, Bluetooth or NVMe support |
| POSIX / ports | [Portable C profile](posix-supported-profile.md): thread-local errno/TLS, shared files/descriptors, positional/vectored I/O, ordered spawn, bounded fork, readiness/time and stream contracts implemented with host/VM regressions; not full POSIX, Linux or Node compatibility |
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
The combined v55 profile is physically accepted on Air7,2 release
`9aa8dbf6fbda8252`: playback/input, brightness, read-only sensors and battery
coexist with document save/exit/reopen. Runtime/PCM cleanup, file preservation
and offline allocation pass. The subsequent v58/follow-up rounds accept matched-adapter Ethernet with cable
recovery, prompted headphone controls and named secondary-click/drag/scroll checks.
Tap settling is tester-accepted in PR88; its missing final receipt is not a full
regression pass. PR99 adds verified archival and capacity admission; its native
automated save/remount/reopen round preserved all prior files and passed offline
checks. [PR102 filesystem scaling](filesystem-capacity.md) replaces the old
1024-file/256-directory tables with stable demand allocation and direct VFS
enumeration. Host growth/failure tests and exact-stage VM tests with 5000 files
pass, including desktop save/reopen and capacity refusal. The Air native round
also passed beyond both old limits (1236 files / 270 directories at start),
preserving all prior files and passing offline checks before and after removing
only the disposable fixture. This closes the bounded filesystem scale milestone.

Exact source/payload identities, evidence locations and negative tests are in
the [October 19 journal](desktop-progress-2026-10-19.md) and
[Air7,2 record](machines/macbookair7-2.md). Missing reports and halted tests are
incomplete or failed evidence, never passes. Linux inventory is evidence of the
host configuration, not a SrvrOS capability claim.
