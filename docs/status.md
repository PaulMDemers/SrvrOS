# Current status

Updated 2026-10-26. SrvrOS is an experimental x86_64 desktop OS with a shell and
server capabilities. This page describes the current source baseline.
Check the source commit of an image before assuming it contains a change;
there is no supported stable binary release yet.

## Hardware

| Target | Evidence | Remaining work |
| --- | --- | --- |
| QEMU q35 | Repeatable boot, CLI, filesystem, network and desktop regressions | Emulator coverage cannot validate physical controllers or firmware |
| MacBookAir7,2 / A1466 | Active reference; combined laptop services, bounded filesystem growth, two automated 24-app rounds with saved-file checks, modal/unsaved-exit retest | Suspend/resume, measured power improvements, USB hot-unplug, broader input/pressure/endurance coverage |
| MacBookAir6,1 | Desktop/input/preferences, brightness/audio, opt-in battery and combined session/reopen evidence | Sustained disk/audio/app stress still fails its lifecycle deadline; timing investigation continues |
| MacBookAir7,1 / A1465 | Planned | Capture this model and run its own acceptance cycle |
| MacBookPro11,2 / A1398 | Native desktop/input, scaled display, brightness, speakers, CPU/two-fan telemetry and captured USB Ethernet accepted | USB Wi-Fi diagnostics in progress; battery and headphone checks deferred |
| MacBookPro11,3 / A1398 | Planned | Separate GPU/firmware inventory; no assumed graphics switching support |
| Macmini7,1 / late-2014 A1347 | Planned | Inventory, storage, display and external input bring-up |

An ARM64 macOS host can build and run the x86_64 guest with QEMU TCG; see the
[macOS guide](macos-development.md). That is emulator coverage, not native ARM64 support.

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
regressions pass. The earlier input-focused physical round reported generally good behavior,
with no recorded timeout or app failure; individual secondary-click/cancellation
cases were subsequently exercised in the October 23 follow-up; tap refinement is tester-accepted in PR88, although its final full diagnostic receipt is incomplete. The Terminal backspace correction passed its physical retest on
`5282cfde546ea2b5`; Text copy/paste/select-all shortcuts were explicitly
validated in the same round.
Earlier unexpected app closures are not conclusively explained by these fixes.

Air7,2 first accepted sixteen-window opening and saved-file integrity at
`bf68e4e9b88b8ae7`. Later release `f01d4049daae0b4a` passed two automated rounds
of 24 mixed app processes, edit/clipboard acknowledgements, ten exact USB files,
chooser cancellation and clean retirement. Broader physical input and pressure
injection are not implied by this automated result.
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
for repeatable local gates and the separate automated/native acceptance limits.

Remaining limits include software rendering, 1440×900 logical client surfaces,
process/descriptor budgets and pixel-memory admission. Explicit 2× scaling passes
VM checks at 2880×1800, but is pixel enlargement, not a high-density font renderer. Pro11,2 has
its own scaled-display acceptance; VM results do not cover other Retina Macs. See the [desktop guide](native-desktop.md),
[API evolution plan](desktop-api-evolution.md) and [device parity ledger](device-parity.md).
Protocol v15 implements same-process parented dialogs, sequential cancellable
Quit and existing-process Text/Preview document routing. Air7,2 release
`1d1d5edce03523db` accepts the reported modal-button and unsaved desktop-exit
corrections with saved documents, clean retirement and prior-file preservation.
Its playback ended canceled with zero recorded underruns; full 120-second
playback/save overlap is VM evidence. Broader services and measured latency/energy
improvements remain further work. The subsequent Air7,2 shared-runtime release
`986d6c68fa19ba18` accepts reported multi-window Text/Preview/System use, Text
save, Preview load, clean retirement and file preservation. It does not accept
native PCM overlap, latency/energy measurements or every menu/quit edge. See the
[lifecycle contract](desktop-application-lifecycle.md) and machine record.

Air6,1 release `95f8a9cf59b25ba1` still failed the ten-second app lifecycle bound
after 34 completed cycles under disk/audio load, despite the merged I/O handoff.
PR170 adds phase timing, not a proven fix. Its three subsequent native attempts
at `282c3f6a06afd9eb` are now reconciled in PR173: one interrupted boot, one
battery fingerprint refusal and one lifecycle timeout after 139 completed cycles.
That run exceeded ten seconds during Files worker-pool retirement. PR174's
merged process-admission correction passed its VM workload, but the next native
release `788873710e559400` failed after 49 completed cycles: Files close crossed
the unchanged ten-second bound before pool retirement. PR176 preserves that
failure and identifies worker pipe admission as an unproven next candidate.
Thirty-minute endurance is not accepted; late exits do not turn failed runs into
passes. See the [Air6,1 record](machines/macbookair6-1.md) for exact evidence.

## Files, services and devices

The [October 26 exFAT import audit](filesystem-audit-2026-10-26.md) adds checked
single-FAT boot geometry and zero-filled reads beyond imported `ValidDataLength`,
including preservation across rename/replacement. Entry-set/name validation and
bounded directory-chain traversal now reject malformed imports; the image packer
emits matching checksums/flags. Older generated images need rebuilding.
Host/sanitizer and disposable VM coverage establish these bounded contracts;
exact-alias retention, exclusive mount namespaces and allocation-map quarantine
are now covered. Overlapping device views, full media integrity, hardware-removal
lifetime and crash recovery remain under audit.

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

The [synchronization audit](synchronization-audit-2026-10-25.md) led to PR175 fixes
for supplied-stack bounds, selected condition clocks, semaphore count overflow
and worker self-stack inspection/early self-detach.
Focused host and guest regressions cover those contracts; matched consumers must
be rebuilt for the changed condition-variable layout. Record publication and
exclusive cleanup ownership now cover early detach and concurrent reapers.
Detached cleanup remains lazy; bounded libc thread names now retain independent
values with range and lifecycle checks. Initial-thread `pthread_exit` now lets
workers finish before normal process cleanup, with matched kernel/libc builds.
Initial-thread join/detach now retains return values and exclusive collection
ownership without releasing process teardown resources. Negative absolute
condition deadlines now follow normal timeout and mutex reacquisition semantics;
broader semaphore semantics remain outside the supported profile. Null-output
`clock_getres` probes now validate supported clocks and source availability
without requiring storage. `times()` now uses advancing kernel ticks when the
reference clock is unavailable, preserving accounting units and usage errors.
Both sources have host regression coverage; default VM checks exercise the
reference path. Relative sleep remainders now exclude handler execution and
preserve a full request when no suspension occurred. Extreme fallback conversion
uses quotient/remainder arithmetic to avoid overflow. Both sleep findings are
corrected. Futex entries now retain active-wait ownership across enqueue/wake
gaps and cancellation teardown; deterministic host and guest coverage checks
interruption and queue reuse. Futex timeout overflow is rejected, and removal
or access revocation of a waited-on mapping safely faults the old wait, including
immediate address reuse. Broader profile exclusions remain open.

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


The bounded synchronization audit on PR175 is complete: once-initializer exit now
resets/wakes abandoned controls before destructors, recursive mutex depth refuses
overflow, and `sem_trywait` retries contention while tokens remain. Futex capacity
and scheduler-slot wake selection are verified; FIFO/starvation freedom remains
unsupported. See the [audit closeout](synchronization-audit-2026-10-25.md#final-synchronization-pass--2026-10-26)
for the review boundary and separate unresolved hardware/subsystem work.


The [AHCI offline investigation](ahci-offline-investigation-2026-10-26.md) corrects
confirmed false-offline decisions when completion/readiness arrives at a wait
bound, separates phase budgets and adds failure diagnostics. Historical spontaneous
offline events remain unattributed. A follow-up campaign passes six full-thread
VMs, two storage stress VMs and a ten-cycle relaunch VM with no spontaneous
recurrence; active investigation is shelved pending new evidence. Genuine
pending-command failures still quarantine the disk and retain DMA memory.


The subsequent [eventfd/epoll audit](event-adapter-audit-2026-10-26.md) corrects
libc-managed epoll ownership across close, native duplication and descriptor
reuse, with positive host/guest tests and stale-result validation. Waits now
preserve immediate results, recheck synthetic state through bounded poll intervals,
retain a monotonic deadline and handle interruption/empty infinite waits. The
adapters remain experimental. Nonblocking eventfd writes now enforce capacity
and reserved values without changing state on failure; writable readiness agrees
with that capacity. Semaphore reads now consume one token; creation validates
flags before allocating state. Epoll now rotates across stable ready sets and
reports native HUP/ERR with an empty interest mask. One-shot delivery now disables
its registration until MOD rearms it; priority bits survive conversion. Continued
audit corrected silent EPOLLET acceptance: ADD/MOD now returns EOPNOTSUPP and
preserves existing state. Closeout adds epoll_pwait masks, interruptible blocking
eventfd I/O, eventfd poll readiness, normal-data event aliases and explicit mode/
ownership refusals. Kernel notification, edge delivery, native priority-source
support and full Linux compatibility remain outside this bounded profile. See the [current coverage and backlog](audit-progress-2026-10-26.md).

The bounded event-adapter audit is complete with passing host/sanitizer, matched
thread and libuv VM gates. PR179 carries the implementation and delivery record.
