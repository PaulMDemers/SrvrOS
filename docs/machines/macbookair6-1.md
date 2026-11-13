# MacBookAir6,1 — local bring-up

Updated: 2026-11-12. State: **Native desktop/input, bounded concurrent USB file work, duplicate-ID repair, completed-workload rerun refusal and speed/acceleration persistence physically accepted. Native brightness adjustment across 5–100%, bounded speaker/headphone audio playback and basic read-only CPU/fan telemetry physically accepted. Bounded parked battery sampling, attended AC transitions and observed alarm refresh/retirement and bounded battery/Text USB-image coexistence accepted. Short battery sampling and the bounded Home desktop session with five Text saves and three battery lifetimes, plus a three-minute continuous Home desktop capture and attended desktop AC unplug/reconnect with indicator updates, are physically accepted.**

Current diagnostic milestone: the exact-source pre-user two-pass read-only
sampler is physically accepted on release `2ddbd3b285f6bf41`. All24 DWORD reads
completed with identical raw samples and borrowed mapping graphs, verified
retirement/claim release, and88.955/83.461ms lifetimes under the unchanged100ms
limit. This accepts the isolated diagnostic history, not current register
access, register writes, root-table walking or device ownership. See the dated
native result below.

Latest accepted desktop checkpoint: the [validated command-tail return](#native-validated-command-tail-return-2026-11-11)
completes the unchanged 30-minute 1% audio/disk/app/three-screenshot profile on
release `0d3f2c1ce3a4aa4c`, with 1,078 app cycles, 3,993 verified disk bursts,
zero recorded underruns and clean retirement/allocation. The exact selected
payload WRITE sends 16 KiB in 1.028 ms, then spends 370.267 ms in status;
status callbacks cover 91.87% of the worst 3.125-second private wait.
Screenshot totals remain 8.99/16.11/12.29 seconds. This accepts bounded diagnostic
and functional coverage; long completion latency and its device/controller cause
remain unresolved, with no isolated performance improvement claimed.

Latest isolated storage checkpoint: release `998fdfb9b4f9c5a8` completes the
[native sidecar remount trial](#native-sidecar-remount-return-2026-11-12).
All original integrity/retirement checks and per-sample first-field evidence pass.
The 37/56-command pattern remains: the three rewrites first differ in atime,
with no material timing improvement established. Cold-boot inode persistence
remains open; the comparison observes 56 prior IDs reassigned by the legacy
restore path, with unique resulting IDs and unchanged permission/ownership fields.


## Initial inventory (2026-10-22)

- Actual DMI model: MacBookAir6,1; firmware `122.0.0.0.0`.
  Marketing year and storage replacement history are not independently verified.
- Branch: `device/macbookair6-1/bringup`; integration base
  `85d5cb7ae436b6b13c6168a05af44e2d57c9e1c8`.
- Ubuntu 26.04.1 LTS, kernel `7.0.0-38-generic`, UEFI with separate writable
  FAT ESP. Ubuntu default verified; Secure Boot unsupported. No installed
  SrvrOS state or pending GRUB one-shot was found.
- Intel Core i5-4260U at 1.40 GHz; 8 GB RAM (7.7 GiB usable).
- Intel `8086:0a26` graphics, subsystem `106b:011a`, Linux i915 boot display;
  connected eDP panel exposes 1366×768.
- APPLE SSD SD0128F, approximately 113 GiB; Marvell `1b4b:9183` AHCI controller.
- Intel `8086:9c31` xHCI; Apple `05ac:0290` revision `0171` topcase.
  Two HID reports captured; interface 2 belongs to Linux bcm5974.
- Analog HDA `8086:9c20`, CS4208 codec `1013:4208`, subsystem `106b:7100`;
  HDMI HDA `8086:0a0c`, codec `8086:2807`.
- Linux applesmc exposes one Exhaust fan. Captured sensor values are Linux
  observations, not SrvrOS telemetry acceptance.
- Internal Broadcom Wi-Fi `14e4:43a0`; attached Netgear USB Wi-Fi `0846:9055`
  revision `0210`, Linux `rtw88_8822bu`; Bluetooth `05ac:828f`.
- Privileged inventory `build/hardware/macbookair6-1-2026-10-22` has zero
  missing/partial reads, including ACPI, HID descriptors, codecs and CPU policy.
  Raw firmware, EDID and device data remain local and ignored.

## Initial setup scope (historical)

Bounded objective: provision the documented host toolchain, capture this unit,
validate the unchanged integration baseline and prepare console/input bring-up.
Scope is this record and model registration in current guides. No native device
or firmware gates are widened.

Fetched main at setup. Open PR #52 at `171b307` owns mounted Home battery
return work; PR #19 at `2bc3689` owns Edimax USB Wi-Fi/xHCI diagnostics.
Neither grants this Air support. Avoid competing xHCI edits until a concrete
Air6,1 transport reproducer exists; retain both branches' profile contracts.

The captured 84-byte USB configuration is byte-identical to the existing
Air7,2 `apple-05ac-0291-config.bin` fixture. Keyboard is boot interface 1,
endpoint 0x82, packet 64, interval 8. This supports reuse of configuration
parsing, not native transport or trackpad report acceptance. Initial `parse_trackpad_config` refused PID `0290`; subsequent exact-gated admission
and native capture/gesture acceptance are recorded below. Descriptor similarity
alone did not establish coordinate ranges, report format or mode switching.
At initial inventory, the CS4208 routes admitted only subsystem `106b:7e00` or `106b:5e00`,
not this unit's `106b:7100`. Backlight and SMC matching also target captured
Air7,2/Pro11,2 firmware. These require independent profiles, not a model rename.

## Capability ledger

| Capability | State | Remaining work |
| --- | --- | --- |
| Boot/display | Console and full native desktop exercised at 1366×768 | Broader layout and extended-session coverage |
| Keyboard | Typing, Up/Down, Backspace and Enter accepted | Explicit Caps Lock/repeat and broader input coverage pending |
| Trackpad/gestures | Exact PID0290 profile; bounded pointer/drag/scroll, primary tap, typing suppression and study feedback accepted | Bounded app coexistence exercised; secondary tap, broad-contact rejection and long sessions pending |
| USB workspace | File saves/navigation, 60-round concurrent workload and rerun refusal accepted; latest 30-minute automated audio/app/screenshot overlap passes 4,247 verified disk bursts, 1,101 app cycles and three captures with preserved prior files and clean allocation | Latest screenshot-inclusive run stays within the unchanged 500 ms loop limit (120.184 ms maximum); fresh-media verification and independent capture progress pass native stress, with corruption fault cases covered on host; screenshot completion remains slow; broader interactive/endurance coverage, failure handling and hotplug remain open |
| Sound/routes | User-confirmed headphone/speaker operation, manual route changes and Stop/recovery; 30-minute playback during automated disk/app work completed with zero underruns and user-confirmed steady sound | Route changes during the long run, jack disconnect and detailed channel/isolation observations not separately recorded |
| Brightness | Exact-gated read-only capture and visible 5–100% adjustment accepted | Broader endurance; no reboot persistence claim |
| Temperature/fan | Basic native read-only CPU/one-fan display and freshness accepted | Longer desktop/file/audio coexistence; Pause/Refresh/reopen observations |
| Battery/AC | Passive handoff, short parked sampling and attended AC transitions with observed alarm refresh/clean retirement and bounded exclusive-image Text coexistence accepted | Bounded mounted Home Text/Start/Stop session accepted; three-minute continuous Home capture accepted; attended desktop AC transitions accepted; longer endurance and gauge accuracy remain open; ordinary startup stays off |
| VT-d diagnostic window | Single-window and two-pass zero-load qualification accepted; isolated read-only sampler now accepts24 DWORD reads with identical raw samples/graphs and verified retirement/claim release in88.955/83.461ms | No register writes, root-table walk or device ownership grant; ordinary desktop regression for changed registry pending |
| Networking | Not tested | Netgear is distinct from the Pro's Edimax work |

## Host validation

Against the recorded integration base, with documentation changes only:

- Documented Ubuntu dependencies installed; all seven pinned submodules initialized.
- `tools/host-env.sh make check-host`: PASS.
- `tools/host-env.sh make -j2 all usb-image test-core`: PASS; Zig 0.15.2
  verified, boot ISO and USB image produced.
- `make check-docs`: PASS.
- `tools/host-env.sh make smoke`: PASS: syscall ABI, all 481 shell commands,
  shell editing, UEFI USB keyboard and displayd. Final shell filesystem check
  reported zero errors/leaks. The initial sandbox run was blocked by Unix-socket
  permissions; the complete host-access rerun passed.
- These tests validate the integration baseline on this Ubuntu host, not native
  MacBookAir6,1 devices. Exact physical stage rehearsal remains pending.

## Local boot/test handoff

Volatile state lives in `build/bringup/checkpoint.json`; logs in `build/review`.
The 50-file inventory backup was hash-verified outside `build`, under
`~/.local/share/srvros/evidence/macbookair6-1-bootstrap-2026-10-22`.
The user authorized dependency installation using a machine-local sudo credential.
The first authorized one-shot returned to Ubuntu and was collected. No boot is
armed. Release `c35475f62f154f1e` is retained outside `build` as a known-working
console/basic-keyboard/bounded-USB payload; preserve it as recovery.

## Current objective and next actions

Final-receipt retention is physically accepted on source `eda7541e`, release
`98bced62ca7be424` (PR266). All required proofs now parse directly from the
unchanged 64 KiB exit file. The overall platform trial remains failed on the
original RC6 timing predicate; package C3 remains zero. No reboot is armed.

The read-only source audit and first native display-control inventory are
complete. Source `78fbea3c`, release `6ac139e77c5d3070` (PR269), directly retains
all 26 controls at both endpoints and all original proofs in the 64 KiB exit
log. Raw controls are identical: primary A, eDP transcoder with pipe-A input and
DDI A enabled; the other sampled planes/cursors/fitters/DDI buffers and VGA
are disabled. The BIOS well request remains set. Audio/interrupt ownership and
other unmeasured dependencies remain open; the well is not proven unused.

The compact format is now physically accepted on source `8f0c5aa6`, release
`0bc5f7e0298ca886` (PR273): all 15 power, 15 idle-residency and 15 idle-entry
records decode directly from the final file alongside every original proof.
A fresh source-matched replay also retains all critical bytes under unchanged
stress. No legacy model or joined files participates in native acceptance.
The overall platform test still fails the original RC6 bound (about 0.0966%
nominal excess), and package C3 remains zero. No power transition is authorized.

The read-only audio/IRQ endpoints are now physically observed on integrated
source `4e2ff068`, release `ee85beb85b75fdca` (PR275 plus PR272). Both eight-control
records and all prior compact proofs survive directly in the matching final log.
Sampled graphics-audio outputs/ELD bits are clear; CPU/PCH interrupt enables are
zero with all mask bits set. This accepts bounded read-only capture, not HDA/DMA
retirement, ownership or an unused display well. The BIOS well request stays set.

The overall platform trial remains failed: nominal RC6 exceeds its PM-clock
bracket by about 80.425 ms after one tick (0.0893%). Package C3 remains zero.
Original limits, integrity checks and policy are unchanged. USB preservation,
offline filesystem/allocation, payload backups and Ubuntu recovery pass; no
reboot is armed.

The remaining dependency audit is complete: captured GPU `0x0a26` is ULT,
so generic CRT/FDI initialization and port-D probing are unsuitable next steps.
The existing eDP input is A_ON rather than the fitter forced-through A_ONOFF
route. Display HDA at 00:03.0 remains D0 with decode/bus mastering enabled;
its controller/link/DMA context is not covered by the graphics audio controls.

Read-only display-HDA capture is now physically observed on source `f921afc2`,
release `2bcb21ae64ee1920` (PR287). Both exact-gated snapshots and every original
proof survive directly in the matching final log. GCAP reports two output
streams. CORB/RIRB DMA, position-buffer enable and both stream RUN bits are
clear; GCTL indicates the controller is out of reset. These bounded control
observations do not establish link/codec readiness, DMA retirement or ownership.

All 17 power/idle/entry samples and 32 coordinated core-C3 rounds pass their
existing checks. The overall platform test remains failed on the original RC6
bound (about 0.0730% nominal excess), and package C3 remains zero. USB integrity,
Ubuntu recovery and durable payload/evidence verification pass. No boot is armed.

The diagnostic-only adoption/eligibility model is now implemented and locally
validated on integrated source `fa8e503f`, release `9316558efb4b7007` (PR296
plus concurrent desktop integration), with matched submodules. It copies observations, enforces epoch/reference/in-flight
lifetimes and refuses unknown or changed prerequisites. The native adapter sets
no assurance bits and has no writer API. Quiet hardware remains insufficient.

The matching native dry run completed with version-8 raw-data-bound refusal
mask 127, epoch 1, Observed → Retired, software guard 1 → 0 and zero references,
in-flight work, assurances, activity flags or hardware authority. All required
proofs survive directly in the matching final log. This accepts the diagnostic
software lifetime/refusal only; no real framebuffer adoption, HDA shutdown or
well clear is implemented. Next is milestone B's readout/backing-lifetime
completeness, retaining unsupported-domain and firmware unknowns.

The overall platform trial remains failed on the unchanged RC6 timing predicate
(about 0.0872% nominal excess); package C3 remains zero. USB integrity, recovery
and durable evidence verification pass. No reboot is armed. PR288/278 apps,
PR252 lifetimes and PR19 Wi-Fi remain separate.

The first milestone-B readout is physically observed on source `728bcbeb`,
release `9b485ca12b4c2482` (PR302). Both nine-read endpoints are identical, and
all 1,056 inherited framebuffer pages translate contiguously at CPU aperture
address `0xa0000000`, with matching geometry and firmware FRAMEBUFFER coverage.
Primary-A control reports tiled mode, so the conservative linear-aperture
predicate correctly remains unbound. GGTT/stolen backing, fence interpretation,
real allocator pin/lifetime and firmware/domain ownership are still unknown.

The version-9 packed resource/dependency fields, both new readouts and all
original proofs survive directly in the matching 64 KiB final file. All 15
power/idle/entry samples and 32 coordinated rounds pass their existing checks.
The whole platform trial still fails the original RC6 predicate (about 0.1073%
nominal excess); package C3 remains zero. USB integrity, recovery and durable
hash verification pass; no reboot is armed. Next is a read-only tiled-aperture /
GGTT backing audit, retaining every unknown assurance and original limit.

### Active exact Haswell vector pilot (2026-11-03)

Base `0cba30a1`, actual recursive pins consumed; installed native readout remains
`728bcbeb` / `9b485ca12b4c2482`. PR301 remains open and now owns Broadwell
scanout-lifetime work as well as its capture. This bounded Air6,1 pilot uses the
same endpoint/map/read/unmap/clock contract inside the existing isolated Air6,1
AP diagnostic, without modifying that driver's API, Broadwell gates or owner.
It is a model-specific adapter, not a competing generic display backend; common
factoring remains dependent on PR301's integrated contract.

Scope: exact Haswell-only readout core/adapter, dedicated opt-in stage/decoder,
positive/refusal/retention tests and automated capture. Capture the complete
1536-entry-bounded DWORD GGTT extent twice, raw GGC/config/route/framebuffer,
and documented FENCE0–15 pairs. Upper sixteen fences and unobserved swizzle
controls stay unknown; no unique-fence, CPU-view equivalence, allocator pin or
hardware authority is granted. Only the existing safe display-arbitration
swizzle control is observed. Independent 100ms readout bounds do not replace
20ms coordinated-C3 or any other original limit. No power/control/data writes,
backing RAM mappings or sibling admission. No ordinary startup hook.

PR252 is integrated in this baseline; PR301, PR313/314 filesystem/profile work
and PR19 Wi-Fi retain their own scope. Next: actual C adapter fixtures and strict
full-vector wire/retention checks, matched builds/exact-stage VM, local USB and
recovery preflight, merge and an automated read-only one-shot under existing
authorization. Native acceptance remains pending; no reboot is armed yet.

### Active tiled-backing source audit (2026-11-03)

Updated to actual main `8f47f196`, with matched pinned app/widget submodules.
The installed/accepted readout remains source `728bcbeb` / release
`9b485ca12b4c2482`; newer integration has no inherited native acceptance.
Scope: Haswell-specific fence/GGTT/cache/swizzle audit and host-only executable
fixtures, documentation and bounded capture contract. No kernel/MMIO/staging,
policy, allocator or sibling-gate edits. PR301 already owns the shared backing
capture and its Air7,2 adapter; record the Haswell dependency there through this
reviewable unit/PR scope, without a competing driver or automatic cross-task
message. PR252 lifetimes, PR313/314 namespaces/profiles and PR19 Wi-Fi stay separate.
Next checkpoint: validated Haswell address/size/cache/fence fixtures and a
read-only adapter contract against the actual PR301 interface. No boot is armed.

### Active read-only backing readout scope (2026-11-03)

Base `d5822348`, local device branch `device/macbookair6-1/process-admission`.
Milestone B first readout binds Haswell primary-A control/stride/linear and tile
offsets/surface/pipe size, aperture PCI BAR and inherited Limine framebuffer
geometry/physical mappings and memory-map classification at both endpoints.
Scope: isolated platform diagnostic/readout fixture, host decoder/retention,
matched validation and automated native capture. No ordinary driver, PMM policy,
well/HDA write or assurance-bit changes. GGTT PTE/stolen-memory ownership and
firmware exclusion remain unknown; this readout cannot complete backing adoption.
PR301 owns separate Air7,2 capture; PR252 lifetimes, PR299 Terminal and PR19 Wi-Fi
remain separate. The first maximum-retention model exposed a 619-byte proof displacement.
Lossless version-9 resource/dependency packing preserves every field within the
original 64 KiB tail. Positive/refusal/schema and native-comparison fixtures and
source-matched retention are required before exact-stage rehearsal/native capture.

### Historical battery-runtime objective (2026-10-23)

Base `9bbc3b9`, branch `device/macbookair6-1/battery-runtime`. Audit connected
Air6,1 AML initialization and query effects using private captured regions and
synthetic EC state. Scope: separate diagnostic-native profile/policy, shared
runtime only where a model-specific contract is required, staging/VM tests and
Air6,1 docs. PR88 owns trackpad timing; PR19 owns Pro USB Wi-Fi. Preserve both.
Next checkpoint is five bounded AC-connected samples with parked USB and full
retirement. Ordinary desktop admission and charging controls remain disabled.

## First console trial prepared (2026-10-22)

Consumed merged main `9ca1922` (PR #52) into clean device source `b31f743`.
PR #53 at `b67d179` continues Air battery desktop ownership; PR #19 at
`2de825f` remains Pro Wi-Fi work. No device gates changed locally.

Rebuilt ISO/USB/core checks and all 51 staging tests pass. Exact v7 console
release `c35475f62f154f1e` passes GRUB chainload, RAM filesystem, USB keyboard,
case/Caps/repeat and renderbench rehearsal. This console harness does not attach
a workspace disk; its default MBR field does not validate this physical GPT USB.

The attached approximately 57.6 GiB exFAT partition on GPT USB storage is now
prepared without formatting. All 102 pre-existing file hashes are unchanged;
workspace starters and a console checklist add five files. Both pre/post
read-only fsck and independent allocation checks pass, with zero leaks or errors.
The selected UUID, paths and file hashes remain in the ignored local run record.

Installed current is `c35475f62f154f1e`, with no previous SrvrOS release.
Ubuntu default and GRUB syntax are verified; no one-shot is armed and no reboot
has occurred. Payload and trial evidence have a hash-verified backup outside
`build`; this is a VM-tested candidate, not physically accepted recovery.
Run record: `build/native-desktop-trials/c35475f62f154f1e/run.json`.

Next is the attended console/keyboard trial, including a saved
`console-keyboard-first.txt`, clean sync/unmount and Ubuntu return. v7 does not
create automatic desktop session logs: retain photographs and user observations,
verify the saved file and original-file hashes, and run offline filesystem checks
before another trial. Physical keyboard, display and USB acceptance remain pending.

## First native result (2026-10-22)

Release `c35475f62f154f1e` reached a visible interactive console. The user reports
normal built-in keyboard typing, many commands, Up/Down, Backspace and Enter.
This accepts those checks only; Caps Lock, held repeat and desktop/trackpad
behavior were not separately confirmed.

The save test did not write a file: `>` and its filename were echoed literally,
and cat could not read the result. This matches monitor echo semantics; the
checklist did not require verification of shell entry. Workspace mount and shell
entry remain unconfirmed, so this is not evidence of a filesystem-driver defect.
The user then entered `reboot`. The monitor attempts to unmount an active
workspace and flush storage before reset, but no explicit unmount observation
was supplied. Ubuntu returned, its boot ID changed and the one-shot was consumed.

Post-return read-only exFAT/allocation checks pass (107 files, 109 clusters,
zero errors/leaks). All 102 original file hashes are unchanged, with no test file
or additional session log. v7 has no automatic desktop log. Evidence is saved
under the run's `physical-after` directory and copied to the durable backup.
USB persistence remains open; no new candidate was installed or reboot armed.

## USB write follow-up prepared (2026-10-22)

The next attended trial reuses installed release `c35475f62f154f1e` unchanged;
its exact VM receipt and installed/durable payload hashes were reverified.
No rebuild or reinstall is needed for this command-only correction. Main remains
`9ca1922`; other machines' battery/Wi-Fi work is independent of this trial.

Use monitor `workspace status` first and require the selected UUID with
`state=ready`. Then `write /work/srvros-work/usb02.txt Air61-USB-write-02`,
`cat /work/srvros-work/usb02.txt`, `workspace sync`, `workspace unmount`, and
`reboot` after successful unmount. Expected output file is exactly 18 bytes,
without a newline. If the workspace is absent, capture status/block/xHCI evidence
rather than treating this as a write test. The local checklist includes an
explicit mount of this unit's UUID when needed.

The new result file is absent; 108 existing file hashes (including the new
checklist) form the pre-run baseline. Offline fsck/allocation checks pass.
Run record and checklist: `build/native-desktop-trials/c35475f62f154f1e-usb-write-02/`;
a hash-verified copy survives outside `build`. Preserve the first trial evidence.
Ubuntu remains default, no one-shot is armed, and this preparation does not
claim USB persistence acceptance. Collect exact bytes, prior-file preservation,
user observations and offline checks after return before another trial.

## USB write physical acceptance (2026-10-22)

The user reports that USB-write-02 went smoothly on unchanged release
`c35475f62f154f1e` (source `b31f743`). After Ubuntu return, `usb02.txt` contains
exactly `Air61-USB-write-02`: 18 bytes without a newline. All 108 pre-test file
hashes are preserved. The only added files are the test result and SrvrOS's
`.srvros/meta` metadata file. Offline fsck reports 18 directories/110 files;
independent allocation checking reports 113 clusters, zero leaks, mismatches
or errors. Ubuntu default is verified and the one-shot is consumed.

This accepts the bounded monitor write/read, sync/unmount workflow and persisted
result on this unit. Workflow observations are user-reported; exact bytes, file
preservation and filesystem health were independently collected. v7 has no
automatic console session log. It does not accept desktop I/O, sustained loads,
hotplug, trackpad or other providers.

Evidence is under the USB-write-02 run's `physical-after` directory, with a
hash-verified copy outside `build`. Retain the accepted payload as recovery for
future trials. No new boot is armed. Main `62067a1` was fetched at collection;
this evidence applies to the recorded installed source, not the newer baseline.

## Active trackpad capture bootstrap (2026-10-22)

See the [capture workflow](../macbookair6-1-trackpad.md) for exact commands,
VM limits and the attended halt/restart contract.

Owner branch `device/macbookair6-1/trackpad-capture`, base `443dbfa` (main with
PR #54 merged). Bounded scope: captured `05ac:0290` revision `0171`, capture-only
Type 3 endpoint admission, sanitized configuration fixture and xHCI refusal/
transport tests. Pointer/gesture startup remains refused for this new PID until
native reports are collected. No EC/SMC/audio/backlight gates change.

Local configuration is byte-identical to the Air7,2 fixture. Linux v6.12
`drivers/input/mouse/bcm5974.c` identifies PID 0290 as Wellspring8 ANSI, endpoint
0x83/Type 3 and explicitly requires no mode switch. This supports a bounded
raw capture; it does not accept native reports or pointer behavior.

The trackpad VM harness now verifies the exact staged release on disposable
GPT media before installation. Shared edits are
limited to xHCI admission/tests, the fixture/Makefile argument and capture VM
harness; Pro PR #19 remains independent Wi-Fi/xHCI work. Preserve its transport
changes when later integrating. No other agent is messaged or scheduled.

Retain `c35475f62f154f1e` as accepted previous/recovery when installing the new
capture. The legacy v10 trial displays gesture/typing instructions and saves a
CRC-checked report/completion marker, then HALTS for manual restart; it does
not return automatically. Prepare only; boot when the user is ready.


Capture run `77ab86681e27132d`, release `19eb394b28cbeef5`, is prepared from
clean source `e2c660336914ce3bd412492b167bb5cfaff106bf`. Build/core/xHCI,
110 hardware-tool tests, capture-harness unit tests, exact-stage GPT rehearsal
and generic MBR regression passed. Both VM reports correctly retain incomplete
physical coverage; neither VM supplies Apple hardware acceptance.

Installed current is this capture; previous is accepted `c35475f62f154f1e`.
Physical USB UUID `6ADA-85BB` has the unconsumed request, 111 pre-test file hashes,
and passing offline fsck/allocation checks (114 clusters, zero errors/leaks).
Run evidence is in `build/trackpad-tests/77ab86681e27132d/`, with a durable copy
outside build. Ubuntu remains default and no one-shot reboot is armed. At the
next authorized reboot, record `launch.json` and arm the already installed
current entry without reinstalling/promoting an unaccepted recovery payload.
Native trackpad capture and pointer/gesture acceptance remain pending.


## Native trackpad capture accepted (2026-10-22)

Evidence-only follow-up on main `1454cb7`, branch
`device/macbookair6-1/trackpad-evidence`; no shared driver changes. Open battery
PR #55 and Wi-Fi PR #19 remain independent scopes.

The user reports run `77ab86681e27132d` appeared to pass. Collection of release
`19eb394b28cbeef5` (clean source `e2c6603`) validates report CRC/completion and
changed Ubuntu boot ID. Result: `CAPTURED`, 279 reports, 265 contact reports,
maximum two contacts, 45 button reports, eight lifts, zero malformed reports
or driver errors. Keyboard typed `AaZz09`, passed and dropped zero reports.
Native samples include touch, multi-contact and button packets.

All 109 unrelated pre-test files retain their hashes. The request contains the
expected `STARTED` marker; `.srvros/meta` was updated. Only report/completion
files were added. Offline fsck is clean (113 files); allocation reports 116
clusters, zero leaks, FAT/bitmap mismatches or errors. Persistence is verified;
on-screen unmount/halt observations remain covered by the user's overall
success report, not explicit fields in this v1 capture format.

Evidence and completion marker are in `build/trackpad-tests/77ab86681e27132d/`,
with a hash-verified durable copy under this release's existing evidence folder.
Ubuntu default and both installed payloads are verified; the one-shot is
consumed. Console/USB recovery `c35475f62f154f1e` remains previous. No new boot
is armed. This accepts bounded raw capture and keyboard coexistence. Desktop
pointer calibration, click/drag, scrolling and sustained operation remain
untested; normal pointer/gesture admission remains disabled for this PID.
The next milestone is a separate bounded desktop pointer trial.

## Active desktop pointer trial preparation (2026-10-22)

Owner `device/macbookair6-1/desktop-pointer`, integration base `dc48846`.
Use accepted capture `77ab86681e27132d` to admit the same revision/configuration
for explicit pointer/gesture modes. Scope: xHCI admission/tests, native report
fixtures, exact-stage v11/v12 VM rehearsal and this unit's test documentation.
Retain existing pointer policy and sibling behavior; no calibration claims,
firmware changes or USB workload expansion. PR #19's Wi-Fi/xHCI work and PR #55's
battery work remain independent. Prepare an attended v12 desktop/scrolling trial,
run host and exact-stage GPT checks, preserve console recovery, self-merge and
install unarmed. Physical movement/click/drag/scroll acceptance awaits this run.


Prepared run `e0d4f2a9d906c9b5`, release `e20382a7c8acbfe2`, from clean source
`ecb028c497246dec48be6bb0f0efa345afa37dc4`. Native sample decoding, xHCI tests
in all three modes, pointer/desktop/capture suites, full build/core and 110
hardware-tool tests passed. Exact v12 GPT VM rehearsal passed desktop startup/
exit, CRC report persistence/unmount, replay/read-only refusal and halt. Its
pointer/gesture coverage correctly remains incomplete without Apple hardware.
The rendered desktop trial was inspected.

Installed current is the v12 trial; `--keep-previous` retains accepted recovery
`c35475f62f154f1e`. Captured v10 payload/evidence also survives outside build.
Physical USB UUID `6ADA-85BB` has an unconsumed request and 114 pre-test hashes;
offline fsck is clean, allocation 117 clusters with zero errors/leaks.
Run/checklist evidence under `build/trackpad-tests/e0d4f2a9d906c9b5/` and exact
VM evidence have hash-verified durable backups. Ubuntu remains default and the
one-shot is empty. At readiness, save `launch.json` and arm the installed entry
without reinstalling. After manual restart collect this exact run first.
No physical pointer/click/drag/scroll acceptance is claimed yet.


## Native desktop pointer trial accepted (2026-10-22)

Evidence-only follow-up on main `22fd266`, branch
`device/macbookair6-1/pointer-evidence`; no driver or desktop changes. PR #19
remains independent Wi-Fi work. The newly integrated battery changes are not
part of the physically tested source below.

The user reports “the trackpad works well” after run `e0d4f2a9d906c9b5`, release
`e20382a7c8acbfe2`, clean implementation `ecb028c`. Collector validates report
CRC/completion and changed host boot ID: `CAPTURED`, pointer/gesture trials
`EXERCISED`, desktop exit and keyboard `PASS`. Over about 132 seconds, 5,009
reports include 3,306 pointer motion events, 34 button edges, 792 held-drag
frames, 505 scroll events and 63 lifts. There are zero malformed reports,
driver errors, pointer cancels/queue overflows or dropped keyboard reports.

This accepts the bounded desktop pointer, physical click/held drag and scrolling
trial with positive overall usability feedback. Direction/speed were not rated
separately. Typing suppression (zero frames), tap clicks (zero), and drag
handoffs (zero) remain unexercised. Five broad-contact suppression frames do
not establish general palm rejection. Sustained use and other applications
remain outside this trial's acceptance.

All 112 unrelated pre-test file hashes match; the request is correctly consumed,
workspace metadata updated and only report/completion files added. Offline
fsck is clean (116 files), allocation has 119 clusters and zero leaks/mismatches/
errors. Durable report persistence is independently verified; on-screen unmount
and halt are not separate fields in this report format. Evidence, completion,
hashes and checks in `build/trackpad-tests/e0d4f2a9d906c9b5/` have a hash-verified
backup outside build under release `e20382a7c8acbfe2`.

Ubuntu default and installed payloads are verified, one-shot consumed. Current
remains the accepted bounded v12 trial; previous console/USB recovery remains
`c35475f62f154f1e`. No new test or reboot is prepared.

## Active tap/typing trial (2026-10-22)

Base `c6a9170`, owner `device/macbookair6-1/tap-typing`. v12 study startup leaves
pointer options zero and compositor cleanup resets them: its zero tap/typing
counters do not establish a device failure. Add explicit v50 tap/typing profile,
study-only option setup and console restoration, coverage reporting/validation,
exact-stage VM and focused host checks. Preserve old recipes and ordinary app
preferences; no driver policy or firmware changes. PR #60 owns descendant
lifetime work (possible nearby desktop edits); keep this change limited to trial
startup. PR #19 Wi-Fi is independent. Prepare/merge/install unarmed and retain
accepted console recovery; native tap/typing acceptance awaits this new run.

Candidate v50 run `4eb6e6e65525268d`, release `f8e370a084725baa`, uses clean
implementation `1977c8b7e8eb333197886340d66d9db4ec3a0ced`. Build/core, focused
pointer/desktop/capture suites and 112 hardware-tool tests passed. Exact-stage
GPT rehearsal passed desktop option setup/exit, console option restoration,
CRC report/unmount, replay/read-only refusal and halt. Its tap/typing result
correctly remains `INCOMPLETE` without Apple input. Desktop and console screens
were visually inspected. No physical tap/typing acceptance yet.

Physical USB UUID `6ADA-85BB` has an unconsumed request and 117 pre-test hashes;
offline fsck is clean (117 files), allocation has 120 clusters and zero errors
or leaks. Preserve console recovery `c35475f62f154f1e` and the durable accepted
v12 payload/evidence `e20382a7c8acbfe2`. Trial instructions are in the
[current guide](../macbookair6-1-trackpad.md#tap-to-click-and-typing-suppression-trial-v50).

Legacy gesture/MBR regression also passed. Installed current is v50; previous
remains console recovery via `--keep-previous`. Installed hashes, Ubuntu default
and GRUB syntax verified; one-shot is empty. Run/checklist and exact VM evidence
have hash-verified durable backups under release `f8e370a084725baa`. At readiness
save `launch.json` and arm the already installed entry without reinstalling.
Collect run `4eb6e6e65525268d` first after manual restart.

## Tap/typing accepted; study feedback follow-up (2026-10-22)

Run `4eb6e6e65525268d`, release `f8e370a084725baa`, source `1977c8b`: the user
reports everything worked as intended, with one UI defect: tapping Text size
activates it but lacks the brief pressed-color feedback seen on physical click.
Collector verifies CRC/completion and host return; tap/typing, pointer and
gesture trials are EXERCISED, keyboard/desktop exit PASS. Counts: 18 primary
taps, 150 typing-suppressed frames, 326 scroll events, 2,484 reports, zero
malformed reports/errors/cancels/queue overflows or dropped keys. This accepts
bounded primary taps and typing suppression/recovery with positive user feedback.
Secondary taps and broad-contact rejection remain unexercised in this run.

All 115 unrelated file hashes are preserved; only expected consumed request,
workspace metadata and report/completion changes occurred. Offline fsck and
allocation pass. Evidence has a hash-verified durable backup under this release;
Ubuntu returned and one-shot is empty. No new boot is prepared.

Follow-up scope on base `14f17d2`, branch `device/macbookair6-1/tap-feedback`:
fix only the Type study's visual activation feedback with presentation-aware
expiry and regression tests. Its receive batch can consume both tap edges
before drawing. Preserve immediate activation and input order; no driver,
shared-widget or firmware policy changes. PR #19 Wi-Fi is independent. The
newer integrated battery/lifetime work is outside the physical trial acceptance.

The Type study now retains a visual-only activation pulse through the highlighted
frame's compositor release, then clears it after ten ticks (about 100 ms).
Press/release ordering, immediate activation and logical capture are unchanged.
Resize/cancel clears the pulse. Host regression covers both tap edges before
draw, highlighted/restored pixels, delayed/stale frame release and repeat/cancel.
This fix is not yet physically retested and does not alter the installed v50.

Feedback fix build/desktop host suite and compositor VM passed, including study
activation, scrollbar capture, focus, drag/resize, client/server cleanup and
console return/relaunch. Host pixel regression verifies the fast-tap highlight
and its expiry. Logs: `build/review/air61-feedback-{build,host,compositor}.log`.
No new hardware stage or installation was made; v50 remains installed unarmed.

## Highlight verification preparation (2026-10-22)

Base `52ef3ea`, branch `device/macbookair6-1/highlight-trial`: prepare a fresh
v50 request with merged Type study feedback fix. No implementation changes;
recheck build/host regression and exact-stage GPT VM, retain console recovery,
install unarmed. Primary objective: light taps on Text size visibly highlight
once, activate once, then restore; compare physical clicks and check repeated
taps/cancel/resize do not leave the highlight stuck. Tap/typing checks remain
enabled. PR #19 Wi-Fi remains independent. Prior run is collected and archived.

Fresh run `cd54ca6d8f2cd4fb`, release `ab0bea06acccc443`, clean source `d15b5a7`
is installed unarmed. Build/desktop/capture tests pass, including the batched-tap
pixel/expiry regression; prior compositor integration covers the unchanged fix.
Exact-stage v50 GPT rehearsal passes startup/options/exit, report/unmount,
replay/read-only refusal and halt. Hardware coverage remains pending.

USB UUID `6ADA-85BB`: 120 pre-test file hashes, clean fsck, allocation 123
clusters with zero leaks/errors. `--keep-previous` retains console recovery
`c35475f62f154f1e`; accepted tap/typing payload `f8e370a084725baa` is durably
backed up. Installed hashes, Ubuntu default and GRUB syntax verified; next_entry
empty. Run/checklist and VM evidence have hash-verified copies outside build.
At readiness save launch.json and arm the installed entry without reinstalling.
After manual restart collect this run and the user's visible-highlight feedback.


## Native tap highlight verification accepted (2026-10-22)

Evidence-only follow-up on base `9256faa`, branch
`device/macbookair6-1/highlight-evidence`; no implementation changes. Open
PR #66 AC/battery transitions and PR #19 Wi-Fi remain independent work.

For run `cd54ca6d8f2cd4fb`, release `ab0bea06acccc443`, source `d15b5a7`, the
user reports “The tapping and clicking both working great.” This is positive
feedback for the requested highlight verification on the merged MR #64 fix.
Visible feedback acceptance is user-reported; saved counters do not measure
rendered button colors or pulse timing. Collector validates report CRC/completion
and changed host boot ID: tap/typing, pointer and gesture trials EXERCISED,
keyboard/desktop exit PASS. Counts include 11 primary taps, 20 button edges,
32 typing-suppressed frames, 26 scroll events and 765 reports. No malformed
reports, driver errors, pointer cancels/queue overflows or dropped keys.
Secondary taps and broad-contact suppression remain unexercised in this run.

All 118 unrelated pre-test files retain their hashes. Only the expected consumed
request, metadata and report/completion changes occurred. Offline fsck is clean
(122 files); allocation 125 clusters, zero leaks/mismatches/errors. Evidence and
completion under `build/trackpad-tests/cd54ca6d8f2cd4fb/` have a hash-verified
durable backup under release `ab0bea06acccc443`. Installed payloads and Ubuntu
default are verified; one-shot consumed. Current remains this accepted bounded
trial, previous remains console recovery `c35475f62f154f1e`. No new boot prepared.

## Full desktop/USB session preparation (2026-10-22)

Base `208423f`, branch `device/macbookair6-1/desktop-session`. Bounded objective:
full workspace desktop with an attended, finite Terminal copy/sync/readback job
while using Text, Files, window management and settings. Scope: additive v53
input-only desktop staging, shared fixture/verification helper, exact-stage VM
coverage and this unit's checklist. No kernel, driver or app behavior changes.
PR #66 owns v52 AC/battery capture; PR #19 owns Wi-Fi. Their separate recipes
and native gates remain unchanged. Prior highlight run is collected. Preserve
console recovery `c35475f62f154f1e`; install unarmed after exact-stage validation.

The initial [full desktop session checklist](../macbookair6-1-desktop-session.md)
uses v53 release `306174ddfec763ef`, clean source `f8a498a`; VM tooling finalized
at `819acd0`. Build, desktop host suite, staging/collector tests (56 installer
checks, 22 collector checks) and docs pass. Exact-stage GPT VM passes 60 one-MiB
copy/sync/readback rounds with concurrent Text save, Terminal writes and screenshot
capture, plus exact retained bytes, session collection, two clean unmounts and
preferences across a fresh VM boot. Two earlier harness failures (semicolon key
mapping and outdated Settings coordinates) were corrected; neither was a native
OS failure. Failed logs remain local. No native full desktop acceptance yet.

Native run `23dadab462e9ed83` has fresh inputs and 124 pre-boot file hashes on
UUID `6ADA-85BB`. Offline fsck is clean; allocation has 134 clusters, zero leaks,
mismatches or errors. Install retains console recovery `c35475f62f154f1e` via
`--keep-previous`. Installed hashes, Ubuntu default and empty one-shot are checked.
Evidence/checklist live in `build/desktop-sessions/23dadab462e9ed83`, with durable
payload and trial copies outside build under release `306174ddfec763ef`.

At readiness recheck installed state, USB identity/input hashes and record the
launch before arming. In the desktop Terminal run `sh session-23dadab462e9ed83.sh`,
then use other apps while it advances. Save `notes-23dadab462e9ed83.txt`, wait for
USB SESSION COMPLETE, save a screenshot, return to console and require clean
log save/unmount before `reboot`. On return collect the matching new desktop
session and verify workload/notes/preferences/prior files and both offline checks
before staging another run. The preparation leaves reboot unarmed.

MR #66 landed during preparation; consumed merged main `ddf4ebf` and retained
both the v52 battery recipe and v53 basic desktop recipe. Combined source
`f689b7c` rebuilds successfully; 57 staging/installer tests, 23 collector tests,
the desktop suite and battery control/transition regressions pass. Initial
release `306174ddfec763ef` is superseded by `9ddea14869c9ed6f`; the same unstarted
USB run and inputs remain valid. Final exact-stage GPT rehearsal passes all 60 rounds, concurrent app saves,
retained bytes, two clean exits/unmounts and fresh-boot preference persistence.
The rebuilt release is installed unarmed with console recovery retained; installed
hashes, Ubuntu default and empty one-shot are verified. Final payload and trial
backups are hash-verified outside build under `9ddea14869c9ed6f`. The run command,
checklist and physical acceptance scope above are unchanged.

## Full desktop return: persistence confirmed, picker defect (2026-10-23)

Evidence-only follow-up on merged main `80b567a`, branch
`device/macbookair6-1/desktop-evidence`. PR #69 ordinary battery sessions and
PR #19 Wi-Fi remain separate. No new payload or reboot is prepared.

Run `23dadab462e9ed83`, release `9ddea14869c9ed6f`, returned to Ubuntu with a
changed boot ID. Two matching sessions (`00065e7a277b701f-00` and
`00065e7a391c3f26-00`) have exit snapshots, clean desktop exit and no collector
attention flags. The user confirms restarting/rerunning after an interruption.
The workload retains rounds 1–19 followed by 1–60, a completion marker and an
exact final 1 MiB payload. Strict single-run verification correctly rejects the
79-line round log; do not call the original uninterrupted trial a pass.

The user reported Text claiming saves while files did not appear, and inability
to reenter Home after navigating Up. Four newly saved text files exist on USB:
`demo_test.txt`, `test-save-test.txt`, `test2.txt`, `t3.txt`. Logs report successful
file writes and directory reads, but Home listings never reach picker-ready.
Persisted `.srvros/meta` contains two duplicate-inode pairs within Home:
639 for the first trackpad completion and a later request, 647 for another
completion and the new session script. `desk_listing_decode` refuses duplicate
inodes, rejecting the entire directory. Parent browsing can succeed, but entering
Home again hits the same rejection. This is a metadata identity defect, not
sufficient evidence to blame double-click timing or lost document writes.

All 124 pre-boot file hashes are preserved except expected `.srvros/meta`.
Offline fsck is clean (139 files); allocation traversal finds 160 clusters with
zero leaks/mismatches/errors. Installed current/recovery hashes and Ubuntu default
verify; one-shot is empty. Raw sessions, new files, metadata and return records
are hash-verified outside build under the installed release's trial directory.
No screenshot or saved desktop preferences was found; those checks remain open.
Full desktop acceptance is withheld pending the picker fix and a fresh trial.

Two follow-ups are needed: reproduce persisted/imported inode collisions and fix
identity restoration while retaining strict listing validation; make the workload
explicitly refuse an existing run directory. Its current `mkdir ... || exit 1`
guard is insufficient because SrvrOS mkdir succeeds for existing directories.
Preserve the interrupted and restarted evidence instead of rewriting round logs.

## Identity and workload retry fix scope (2026-10-23)

Base `58b354f`, branch `device/macbookair6-1/file-identities`. Fix duplicate VFS
identities during metadata import/registration, preserving permissions/times and
strict picker validation. Add collision/allocator regressions and a disposable
VM fixture with duplicate persisted IDs, saved-file reopen and Home reentry.
Fix the shell workload's existing-directory guard and exercise refusal without
changing retained round evidence. Scope: VFS metadata, workload/rehearsal tools,
tests and this unit's guide. PR #69 owns opt-in battery session startup; PR #19
owns Wi-Fi. Neither owns VFS identity restoration. Prior native run is collected;
retain its evidence and console recovery. Prepare a fresh v53 trial only after
host tests and exact-stage VM pass, install unarmed, then merge under existing
authorization. No firmware/profile gates are widened.

The VFS now remaps imported zero/colliding IDs to unused live IDs across the
whole node table; unique requested IDs and all other metadata fields are retained.
Registration and metadata restoration share the rule; allocation skips used IDs
and handles wraparound. The picker retains strict duplicate checks. The host
regression fails against the old implementation at the collision assertion and
passes on the fix, including file/directory import, repeated updates, zero and
maximum IDs, wraparound and retained mode/time. Core and desktop suites pass.

Exact-stage v53 GPT VM passes on release `db7ce82008f4b9a6`, clean source
`78e285a`: seeded duplicate persisted IDs become unique, saved Text files reopen,
Home Up/reentry works, all 60 copy/sync/readback rounds verify, and retries during
the active workload and after a fresh boot return nonzero without changing the
round log or final payload. Preferences, session logs and two clean unmounts pass.
The guard explicitly tests directory existence before mkdir; it is a sequential
rerun guard, not an atomic concurrent-job lock. Run one instance at a time.

Fresh native run `f48940182e6911a0` is installed unarmed. Command:
`sh session-f48940182e6911a0.sh`. Follow the updated full desktop checklist,
including visible file listings, Up/double-click Home reentry, saved-file reopen
and a second invocation that must print REFUSED without another round. Prior
metadata/evidence remains untouched on USB so mount-time repair is exercised.
There are 141 pre-boot hashes; fsck is clean and allocation has 169 clusters with
zero leaks/mismatches/errors. Console recovery `c35475f62f154f1e`, installed hashes,
Ubuntu default and empty one-shot are verified. Payload/trial evidence has a
hash-verified durable copy outside build under the new release. Physical fix
acceptance is pending; collect this fresh run before another trial.

## File identity fix physical result (2026-10-23)

Run `f48940182e6911a0`, release `db7ce82008f4b9a6` (clean source `78e285a`,
merged PR #71), returned to Ubuntu. Matching session
`session-00065e82d1e50247-00` has its exit snapshot and desktop exit with no
collector attention. The user reports the file issues resolved during this run.
All 172 persisted metadata IDs are nonzero and unique, including repair of the
previous collisions. Two new text files persist. All 141 preboot file hashes
are unchanged except the expected `.srvros/meta` repair/update.

The workload verifies exactly rounds 1–60 once, 62,914,560 bytes copied, exact
final readback and completion marker. Offline fsck is clean (150 files), and
allocation traversal passes with 187 clusters and zero leaks, mismatches or
errors. This accepts the reported file-listing/save/navigation fix and the
bounded concurrent USB workload on this unit. Physical rerun refusal and
preference persistence are not independently captured; their VM evidence remains
valid but this report does not promote them to physical acceptance. No screenshot
was saved. Broader gesture/session and optional device coverage remains open.

Raw collection, changed/new files, hash report and offline checks are preserved
under the run directory and hash-verified durable evidence outside build at
`~/.local/share/srvros/evidence/db7ce82008f4b9a6/trial`. Run/launch records are
returned-collected; GRUB one-shot is empty and USB is unmounted. No new trial
is staged or armed. Next work should choose a bounded remaining acceptance scope.

## Rerun refusal and preference persistence scope (2026-10-23)

Base `5089172`, branch `device/macbookair6-1/rerun-preferences`. Prepare a focused
attended follow-up using accepted release `db7ce82008f4b9a6` and the completed
`f48940182e6911a0` workload. Preserve that workload; capture refusal stdout and
exit status and compare its retained files with pre-test reference bytes. Save
pointer speed 5/acceleration off through Settings, check reopening, then collect
before a second boot verifies restoration. Scope is workload evidence helper,
VM rehearsal and this unit's guide/record; no OS or firmware gate changes.
PR #69 battery startup and PR #19 Wi-Fi remain separate. Re-run exact-stage GPT
VM before readiness; retain Ubuntu default and console recovery. No reboot yet.

Focused phase-1 run `359e52fa68fe19bf` is ready, installed-unarmed, reusing
unchanged accepted release `db7ce82008f4b9a6` (OS source `78e285a`). Preparation
helper/VM code `8441c88`, regression `b925cc3`: 57 staging tests, refusal preflight
and collector regression, Settings/preferences host tests and docs checks pass.
The exact-stage GPT VM passes two boots, saved preferences, the new refusal
stdout/status/comparison receipt, 60 exact rounds, and two clean unmounts.
Two initial fixture-construction attempts failed before boot because nested
names are unsupported by the image builder; the final fixture uses flat names.

Native command: `sh refusal-359e52fa68fe19bf.sh`. It checks completed workload
`f48940182e6911a0`; require REFUSAL VERIFIED, then set Settings speed 5 and
acceleration off, wait for ready and reopen Settings. Return to Ubuntu for
collection before the separate restoration boot. All 150 prior files are
preserved; seven fresh inputs bring the preboot count to 157. Offline checks
pass (201 clusters, zero leaks/mismatches/errors). Installed payload and console
recovery hashes, Ubuntu default and empty one-shot are verified. Local run and
VM receipts are backed up outside build under the release's
`refusal-preferences/359e52fa68fe19bf` directory. Physical acceptance is pending.

### Refusal captured; preference restoration ready

Phase 1 `359e52fa68fe19bf` returned with matching new session
`session-00065e84b6a90e72-00`: exit snapshot/desktop exit present, no collector
attention. The actual refusal stdout matches, exit status is 1, verification
receipt passes, and the completed workload remains byte-for-byte unchanged.
`desktop.conf` persists speed 5 and acceleration off. All 157 preboot files
are unchanged except expected metadata updates. No screenshot was saved; visual
reopen behavior was not separately reported. Offline fsck is clean (165 files),
allocation passes with 210 clusters and zero leaks/mismatches/errors.

Phase 2 `3559ab3a5aab1fb8` is ready on the same verified installed payload,
with all 165 current file hashes recorded and no new USB inputs. Open Settings
and confirm speed 5/acceleration off before any change; capture a screenshot if
possible, return to console and reboot to Ubuntu. Do not rerun the workload or
refusal wrapper. The existing exact-stage two-boot VM receipt remains applicable;
no OS changes or new installation are involved. Both run records/evidence have
verified durable backups. Ubuntu default/recovery are intact; one-shot is empty.
Refusal and saved preference bytes are physically verified; restoration remains
pending this second boot. Newly merged PR #69 is not part of this installed
release; do not widen the trial to its battery startup behavior.

### Preference restoration physically accepted (2026-10-23)

Phase 2 `3559ab3a5aab1fb8` returned on unchanged release `db7ce82008f4b9a6`.
New matching session `session-00065e84cfa28c9c-00` has its exit snapshot and
desktop exit, with no collector attention. The user confirms settings persisted;
the saved screenshot shows Trackpad speed 5 of 5 and acceleration unchecked.
`desktop.conf` is byte-identical to phase 1, and the earlier refusal receipts and
completed workload still verify. All 165 preboot files are preserved except the
expected metadata update. New files are the four session logs and screenshot.
Offline fsck is clean (170 files); allocation passes with 249 clusters and zero
leaks/mismatches/errors. Evidence and screenshot have a hash-verified durable
backup outside build. One-shot is empty, USB unmounted, and recovery verified.

This closes the focused completed-workload rerun refusal and speed/acceleration
persistence checks on this unit. It does not expand acceptance to every preference,
concurrent script starts, optional hardware services or other Mac models. No
new boot is staged or armed; both focused runs are collected.

## Read-only brightness capture ready (2026-10-23)

Implementation/source `a0f148a`, release `142dd83ded3d2da4`, run
`b1bf2459dd0cd1f9`: exact GPU `0a26`, PCH `9c43`, 31,777-byte DSDT fingerprint
`af9a47bc352dc6bf` admits only the read-only PWM capture. Current Ubuntu DSDT
matches the inventory bytes. No EC/AML access, writable PWM mapping or provider
binding is admitted on this profile. Native mode/period evidence is still pending.

Full build/USB image and core suite pass, as do backlight tests, 58 staging tests
and captured-firmware positive/negative tests (wrong chipset, length, checksum
and checksum-correct modification refused). Exact-stage GPT v19 VM saves logs,
checks retained file bytes and clean exFAT, then resets; the missing-workspace
case returns without starting the probe. This is recovery-path validation, not
native PWM acceptance. Docs and diff checks pass.

Installed unarmed with console recovery `c35475f62f154f1e` retained; accepted
desktop `db7ce82008f4b9a6` remains installed and independently backed up. One
superseded EFI payload (`9ddea14869c9ed6f`) was removed only after verifying its
complete independent backup. Ubuntu default and empty one-shot verified.
All 170 USB files are recorded; offline fsck/allocation pass (249 clusters,
zero leaks/mismatches/errors). Payload and run evidence are preserved outside
build. This next boot is unattended: leave USB attached, allow automatic Ubuntu
return, then collect before any write-enabled brightness implementation.


## Native PWM capture result (2026-10-23)

Run `b1bf2459dd0cd1f9`, release `142dd83ded3d2da4`, session
`session-00065e85df9c8068-00` returned to Ubuntu with matching boot/exit logs,
`completed=yes` and no collector attention. Exact Air6,1 admission succeeded:
CPU control/duty `0/0`, PCH control `c0000000`, PCH duty `0ad904cc`;
decoded mode 2 (PCH override), period 2777, duty 1228 (about 44% duty).
The capture explicitly records no PWM writes, provider binding or EC access.
This accepts read-only access, not panel adjustment or calibrated luminance.

All 170 prior file hashes match except expected metadata update; only two
session logs are new. Offline fsck is clean (172 files), allocation passes with
252 clusters and zero leaks/mismatches/errors. Final saved/unmounted console
messages are emitted after logging stops and are not present in the saved exit
snapshot; retained logs, returned host state and offline integrity are recorded
separately rather than claiming those console messages were captured.
Evidence has a hash-verified backup outside build. One-shot is empty and USB
unmounted; recovery remains verified. Next is an exact Air6,1 brightness provider
preserving observed mode/period, with an attended visible adjustment trial.
No new boot is staged or armed.

## Brightness control scope (2026-10-23)

Base `83a3f59`, branch `device/macbookair6-1/brightness-control`. Bind an explicit
Air6,1 profile only behind the existing captured GPU/PCH/DSDT gate and observed
CPU control 0, PCH control c0000000, period2777. Reuse bounded 5–100% duty writes,
readback verification and drift quarantine. Discovery must leave the panel alone.
Prepare a brightness-only unscaled desktop v56 via basic+brightness staging;
no audio, EC/AML, battery or other optional service. Scope: backlight/profile,
probe binding, staging/VM tests and this unit's docs. PR #76 reserves v55 for
combined services; preserve that separate work when integrating shared staging.
PR #19 remains Pro Wi-Fi work. Native visible adjustment remains unaccepted.

### Attended brightness control ready

Clean source `1cc5f82`, release `59086591e526632b`, run `19da744d205e2b2c`
implements explicit Air6,1 control using the native captured period2777. The
existing exact GPU/PCH/DSDT gate is retained. Discovery leaves duty unchanged;
requests are bounded 5–100%, preserve mode/period and verify readback. Any mode,
period or readback mismatch disables the provider. Read-only capture stays read-only.

Full build/USB image, core and desktop suites, all-profile backlight tests,
captured-firmware tests and 59 staging tests pass. Exact-stage GPT v56 VM passes
two boots, Settings/preferences, Text saves, screenshots, diagnostic collection
and clean unmounts. Unsupported Q35 brightness remains unavailable. These tests
do not establish visible native adjustment. Docs/diff checks pass.

Installed unarmed, console recovery retained, accepted desktop and prior capture
backups verified. Superseded EFI payload `19eb394b28cbeef5` was removed only after
verifying its independent backup. All 172 existing USB files are recorded;
offline fsck/allocation pass (252 clusters, zero issues). Payload/run evidence
is backed up outside build. Follow the attended brightness guide: inspect the
initial level without a startup change; try 50/25/75/100/5 then50, confirm visible
changes and responsive input, reopen Settings, screenshot and return manually.
No battery/EC/AML/audio/network/sensor service is enabled. Native acceptance pending.


## Native brightness adjustment accepted (2026-10-23)

Run `19da744d205e2b2c`, release `59086591e526632b` (source `1cc5f82`), session
`session-00065e863524e78f-00`: the user reports proper brightness operation
throughout the range. Exact Air6,1 provider starts ready at 44%, with no discovery
write. Logs contain 193 successful brightness readings spanning 5–100%, ending
at 58%, with no unavailable result. The desktop exit snapshot is present and
collector attention is empty. This accepts visible adjustment on this unit;
no screenshot was saved, and final50/reopen or absence of a visible startup
change is not separately claimed. Reboot persistence/calibrated luminance remain
outside scope.

Offline fsck is clean (176 files); allocation passes with 257 clusters and zero
leaks/mismatches/errors. Prior files match except metadata and desktop.conf;
the latter changed pointer speed5 to3 and acceleration0 to1. The user confirmed
these changes were intentional; this is not a preference-persistence regression. Four new session logs are retained. Run/launch are collected,
one-shot empty and USB unmounted. Evidence is backed up outside build; no new
boot is staged or armed.

## Audio listening candidate (2026-10-23)

Implementation `8f3656a`, PR #82, v57 release `0533c0dfc63d104f` adds exact
`9c20` / CS4208 `106b:7100` revision `100300` admission. Both observed pin
pairs are validated before codec writes; speaker DAC3/pin12 and headphone
DAC2/pin10 reuse muted startup, GPIO0 and the existing serialized worker.
See the [route evidence and listening checklist](../macbookair6-1-audio.md).

Full build/USB image, core and desktop host tests, 60 staging tests and docs
checks passed. The exact staged GPT VM passed two desktop sessions, GUI file
save/reopen, preference restoration, recorded 500 ms tone and six-second stereo
WAV, zero underruns and silent tails. Native audio remains **not tested**;
QEMU routing cannot establish this Air's sound quality or channel balance.

The candidate retains accepted brightness and input, with battery/AML, SMC,
sensors and networking off. Preserve accepted brightness `59086591e526632b`
and previous console recovery `c35475f62f154f1e`. Local run
`build/audio/42421979e85c44eb` holds 176 preboot file hashes and clean offline
exFAT/allocation evidence. Ubuntu stays default; reboot is unarmed. Next action
is the attended speaker/volume/channel/Stop test and optional manual headphones,
followed by collection and file/allocation checks before acceptance.

## Native audio return (2026-10-23)

The user reports that sound worked properly on release `0533c0dfc63d104f`
(compiled `8f3656a`). Two new sessions were collected: `00065e8718b76081-00`
initialized audio and exited without test tones; `00065e874fca8c4d-00` completed
three tones, a 1,152,000-byte WAV and cancellation of a second stream at byte
609,188. Volume requests include 10% and 25%. Both sessions exited cleanly;
audio ended ready, no error, no owner and zero underruns.

This accepts basic audible speaker playback on this unit. Detailed left/right
balance, absence of startup pops, headphone isolation and sustained playback are
not separately confirmed. No headphone selection or connection was logged.
The diagnostic literal `physical sound NOT_TESTED` is intentionally not an
automatic acceptance flag; listening confirmation comes from the user.

All 176 prior files remain, with only filesystem metadata changed and eight new
session logs. Offline exFAT and allocation checks pass: 184 files, 267 clusters,
zero leaks/mismatches/errors. Ubuntu return, empty one-shot, installed/recovery
hashes and the independent payload backup are verified. Full return evidence is
retained outside `build`; no new boot test is armed.

## Headphone/extended playback ready (2026-10-23)

The next attended trial reuses accepted release `0533c0dfc63d104f`, compiled
`8f3656a`. No payload or hardware gate changed. Run `5a302d248eee397a` in local
`build/audio/extended-20261023` has the headphone/extended checklist and a
verified 60-second stereo WAV in USB Home's `audio-extended` directory. All 184
existing files are preserved; the new baseline has 185 files. Offline exFAT and
allocation checks pass: 356 clusters, no leaks/mismatches/errors.

Targeted HDA, audio service/stream, WAV conversion and Settings host tests pass.
The existing PCM VM harness completed five one-minute plays with zero guest
underruns, exact recorded samples (maximum error zero), distinct stereo and USB
input retained. Actual guest kernel/initramfs hashes match the accepted release;
the newer harness checkout is recorded separately, not claimed as compiled code.
The exact v57 GPT VM additionally passed two desktop sessions, GUI save/reopen,
preferences, tone/WAV capture, durable logs and clean unmounts. These results do
not accept physical headphones or extended native playback.

Use the [extended checklist](../macbookair6-1-audio.md) to check manual headphone
routing, jack indication, left/right isolation, five full one-minute plays,
Stop/recovery and return to speakers. Console recovery `c35475f62f154f1e`
remains previous; Ubuntu stays default. No reboot is armed. On return identify
new sessions against this run's preboot hashes: the release ID is deliberately
the same as the earlier accepted speaker trial. Preserve the prior trial evidence.

## Headphone trial return (2026-10-23)

The user reports all headphone and speaker audio worked as intended. New
sessions `00065e879b6e0aac-00` and `00065e87be98c776-00` both exited cleanly.
The first initialized without playback. The second records connected headphones,
manual headphone selection at 10%, two headphone tones, a complete 1,152,000-byte
channel WAV, subsequent Stop requests, selection back to speakers and a third
completed tone. Audio ended ready with no owner, error or underruns.

Three longer streams were canceled: handles 2/3 on headphones and 4 on speakers.
No complete 11,520,000-byte one-minute stream is evidenced, so the five-minute
native endurance target is **incomplete**, not passed. Headphones still reported
connected at exit; disconnected-jack indication is not established. The user's
listening report accepts bounded headphone/speaker operation; individual
left/right and speaker-isolation observations were not separately enumerated.

All 185 preboot files remain unchanged except `.srvros/meta`; eight new session
logs were added. Offline exFAT/allocation checks pass: 193 files, 366 clusters,
no leaks/mismatches/errors. Ubuntu returned with empty one-shot and verified
payload/recovery hashes. Return evidence is retained in this run's separate
backup outside `build`; the earlier speaker acceptance remains intact.

The user explicitly confirmed the early stops were intentional, with no playback
problem. They are not failures, but do not complete the full-duration target.

## Read-only telemetry trial ready (2026-10-23)

Release `eee15036f4172920`, compiled clean source `60fc97a`, is installed for
attended run `3f703367ab650765`. MR87 adds captured CPU signature `40651` and
the exact Air6,1 DSDT/GPU/PCH single-fan profile. The v59 unscaled desktop keeps
brightness/audio and enables only CPU temperature and SMC fan reads. No fan
control, EC/AML, battery, SMC MMIO or IRQ6 ownership is enabled. See the
[telemetry checklist](../macbookair6-1-telemetry.md). Native readings remain pending.

Full build, USB image, core/desktop host suites, 63 boot-profile tests and docs
checks pass. The actual DSDT matcher passes. The exact staged GPT VM passes
two desktop sessions, save/reopen, preferences, tone/WAV playback with zero
underruns, saved device status and clean unmounts. Generic QEMU correctly
reports thermal/SMC unavailable; this does not accept native readings.

USB `6ADA-85BB` remains unmounted with a 193-file baseline; offline exFAT and
allocation checks pass (366 clusters, zero leaks/mismatches/errors). Installed
payload/recovery and independent backups are hash-verified. Console recovery
`c35475f62f154f1e` and accepted audio `0533c0dfc63d104f` are retained. Ubuntu
remains default and no reboot is armed. Run records, captures and VM evidence
are stored locally and copied outside build before boot.

## Read-only telemetry return (2026-10-23)

The user reports all sensors displayed fine. Matching session
`00065e8823afc352-00` on release `eee15036f4172920` ran about 82 seconds and
exited cleanly with no collector attention. Saved snapshots show boot-core
57–63 C, package 58–64 C and one fan at 1175–1231 RPM. Both providers reached
81 samples; every saved device sample was less than one second old. SMC ended
with 84 transactions, zero failures and error zero. Audio tone activity occurred
with final audio ready/error zero; PCM stayed idle in the saved snapshots.

This accepts basic native read-only temperature/fan display and fresh sampling.
The full two-minute file/audio coexistence checklist, Pause/Refresh/reopen and
screenshots are not independently established. No new document or screenshot
was saved. This is not thermal calibration, fan-control or cooling acceptance.

All 193 preboot files remain; only `.srvros/meta` changed and four matching
session logs were added. Offline checks pass: 197 files, 371 clusters, no leaks,
bitmap mismatches or errors. Ubuntu returned with an empty one-shot; installed
candidate/recovery and backup hashes are verified. Run records and return logs
are retained outside build under the candidate's evidence/trial directory.

## Battery/AC passive preflight ready (2026-10-23)

Run `10567bbea85fe3e2`, release `9b528e09c13e1d4b`, clean source `f6bab3e`,
is installed unarmed. Fresh DSDT/ECDT/FADT match the original unit capture.
Offline loading of nine AML tables with private GNVS/SMBus PCI snapshots matches
the EC namespace/resources with zero errors, denials, writes or leaked allocations.
Full initialization and runtime permissions remain unaudited; this result is not
live battery telemetry. See the [battery bring-up note](../macbookair6-1-battery.md).

The distinct v60 automatic test records only exact-gated PM1, GPE and EC-status
reads, saves to USB, unmounts and returns to Ubuntu. Leave AC connected. It does
not issue EC commands, read EC data, initialize AML or own SCI. Native handoff
state is needed before a later bounded battery-sampling profile can be admitted.

Build/USB image, core/desktop suites, 64 staging tests, 24 collector tests,
passive-port host checks, actual resource-gate/mutation tests and docs checks
pass. Exact-stage GPT VM confirms refusal without native access, saved logs,
clean filesystem/unmount and automatic reset. Native preflight remains pending.
USB has 197 baseline files; offline allocation passes with 371 clusters and
zero leaks/mismatches/errors. Installed/independent payload hashes are verified;
console recovery `c35475f62f154f1e` and accepted telemetry `eee15036f4172920`
remain available. Ubuntu stays default with empty one-shot. Evidence is backed
up outside build before boot; no reboot has been requested.

## Battery/AC passive preflight return (2026-10-23)

Matching session `00065e8886bf8b00-00`, release `9b528e09c13e1d4b`, records
all 36 passive reads, zero writes and `completed=yes`, with no collector attention.
Ubuntu returned on a new boot with empty one-shot. PM1 control/enable are zero,
status is `8001`, and EC status is zero (idle at the snapshot). GPE enable
byte 12 is `40`, byte 14 is `04`, and every other enable byte is zero: enabled
sources `66` and `72` hexadecimal. Their pending status bytes were zero; other
masked GPE status bits were set. Nothing was acknowledged or changed.

This accepts the passive firmware handoff capture only. It does not prove
battery/AC readings, event ownership, AML initialization or EC/SCI teardown.
The complete snapshot is retained for the independent runtime audit.

All 197 prior files remain; only `.srvros/meta` changed and two matching session
logs were added. Offline exFAT/allocation checks pass: 199 files, 374 clusters,
zero leaks/mismatches/errors. Installed and independent recovery/payload hashes
are verified. Return records and logs are backed up outside build. No boot is armed.

## Bounded battery/AC runtime ready (2026-10-23)

The independent connected audit is complete for the first bounded profile;
native operation remains pending. Release `674c597b082f2494`, clean source
`308e4bd`, is installed unarmed for run `5b60eacb23867c83`. See the
[runtime audit](../macbookair6-1-battery.md#connected-runtime-audit-and-bounded-sampling-candidate)
for exact GNVS/PPMT/OpRegion, PCI, EC and query permissions. Offline initialization,
allowed queries and EC detach pass in all four modeled lid/AC states. GPIO stays
denied. `_Q10`, `_Q80`, absent Air7,2 methods and unknown queries are refused.

v61 uses its own native binary and refuses service, transitions and mounted-USB
entry. It parks USB, attempts five AC-connected samples, retires and saves logs
before automatic Ubuntu return. The first native trial may expose another
ownership/refusal condition; missing final logs remain failure/incomplete evidence.
No desktop battery provider or charge-control access is admitted.

Full build/USB image, native binaries, core/desktop suites, 65 staging tests,
captured firmware/mutation tests and docs checks pass. QEMU passes five-sample
success, denied access, transport fault, failed foreign-event restoration and
EC-disconnect deadline containment; separate Global Lock handoff/release pass.
The exact staged GPT VM refuses unsupported hardware, saves logs, unmounts and
returns. These are runtime regression results, not native sampling acceptance.

USB has 199 baseline files; offline exFAT/allocation pass (374 clusters, zero
leaks/mismatches/errors). Installed/recovery and independent backup hashes pass;
Ubuntu remains default with empty one-shot. Console recovery `c35475f62f154f1e`,
accepted desktop telemetry `eee15036f4172920` and passive preflight
`9b528e09c13e1d4b` are retained. Raw audit and run evidence are copied outside
build before boot. Leave AC and USB connected for the automatic trial.

## Bounded battery/AC runtime return (2026-10-23)

Release `674c597b082f2494` (compiled `308e4bd`), matching session
`00065e88fbc08db1-00`, returned without an exit log or five-sample completion
receipt. The collector flags the missing exit. Saved `boot.log` and
`session-a.log` end immediately before the unattended battery probe; they do
not establish entry, sampling, retirement or the cause of the interruption.
This trial is incomplete/failed acceptance, not a native battery pass.
The user confirms automatic recovery; the screen reset too quickly to capture.

All 199 prior USB files remain; only `.srvros/meta` changed and the two matching
logs were added. Offline exFAT and allocation checks pass: 201 files, 377 clusters,
zero leaks, bitmap mismatches or errors. Ubuntu returned on a new boot, with
empty one-shot and installed payload/recovery hashes verified. Evidence and
updated run records are retained outside build. No reboot is armed.

Next work is to locate the stopping point using the screen observation and
bounded diagnostics while keeping USB parked during uncertain EC/SCI ownership.
Do not widen firmware permissions or enable desktop battery startup based on
this result. The existing host/VM passes remain regression evidence only.

## Failure-display follow-up scope (2026-10-23)

Based on main `b17b80e`, this unit owns a bounded terminal failure display in
shared `battery_runtime.c`, its small delay helper and host/VM tests. Other open
work (Air7,2 PR88 input timing, Pro11,2 PR19 Wi-Fi) was inspected; no overlapping
battery-runtime scope was found. Automatic contained faults retain the existing
storage interlock and interrupt/source masking, enter the emergency console,
show reason/setup/worker/EC details for 30 seconds, then reset. Clock failure
shortens the pause through the existing per-delay iteration bound. Mounted/I/O
faults still halt. No firmware permissions or successful sampling path change.
Next checkpoint: host/VM validation and a new exact staged Air6,1 trial.

## Failure-display candidate ready (2026-10-23)

Release `b9a2d264ad3a0dd2`, clean source `453e063`, is installed unarmed.
Full build/USB image, both native battery binaries, VM binary, core/desktop
suites, 65 staging tests and docs checks pass. Host tests cover all 301 delay
completion/failure boundaries. Q35 confirms readable framebuffer diagnostics,
interrupts disabled and 30.009-second hold before automatic reset; a parked
filesystem fault also resets after the hold with outside bytes preserved.
An active scratch-I/O fault blocks reset and skips the delay, retaining storage;
offline contents/exFAT pass. Normal sampling passes. The exact staged GPT VM
refuses nonmatching native hardware, saves logs, unmounts and returns.

The USB baseline is 201 files; offline allocation passes with 377 clusters and
zero leaks/mismatches/errors. Current/recovery payload hashes and Ubuntu default
are verified, with no one-shot armed. Console recovery `c35475f62f154f1e` and
accepted desktop telemetry `eee15036f4172920` remain installed. The superseded
`db7ce82008f4b9a6` payload was removed from EFI only after its independent backup
was verified. New payload and evidence are backed up outside build.

Leave AC and USB connected. If a failure screen appears, photograph its reason
and progress during the 30-second pause before automatic Ubuntu return. This is
another diagnostic attempt, not battery acceptance; a failure outside the
instrumented path can still reset without a report. Collect the matching session
before any further trial, and retain missing completion as incomplete/failure.

## Failure-display native return (2026-10-23)

Release `b9a2d264ad3a0dd2` (compiled `453e063`), session
`00065e8929e2f258-00`, has boot/session-a logs but no final report. The user's
photo now identifies terminal containment during `namespace-initialize`:
`denied map address=0xfed1c000 bytes=16384`, operation region `\RCRB`, while
executing `\_SB.PCI0.LPCB.HPET._STA`. The screen shows errors=4, denied=1,
EC phase=6/error=0, queries=0, no SCI deliveries, worker completed=2/in-flight=0,
and the photograph prompt before automatic reset. It confirms the diagnostic
screen is visible on this unit; exact physical pause duration is not measured.
Battery sampling and clean retirement remain unaccepted.

The captured DSDT's HPET `_STA` reads HPAE in the DWORD at RCRB+`3404`
(`fed1f404`). The original connected replay already explicitly allowed and
logged this read with value `80`; the earlier claim that HPTC AML access was
unneeded was an audit-summary/native-policy mistake. The Air6,1 gate currently
excludes both the opaque RCRB mapping and that exact DWORD read. Next scope is
to admit only the captured read, test width/address/write refusals and replay
initialization; do not permit arbitrary RCRB accesses or writes. No code gate
has been widened in this closeout.

All 201 prior USB files remain; only `.srvros/meta` changed and two matching
logs were added. Offline exFAT/allocation pass: 203 files, 380 clusters, zero
leaks/mismatches/errors. Installed and independent payload/recovery hashes pass;
Ubuntu returned on a new boot with empty one-shot. Photo, logs and return records
are retained outside build. No reboot is armed.

## HPTC correction scope (2026-10-23)

Based on main `6e608b0`, this unit owns the native HPTC read correction in
`battery_native_platform.c` and the shared pure predicate/profile tests.
Open PR88 input timing and PR19 Wi-Fi were inspected; no overlapping battery
scope was found. Admit the opaque captured RCRB region and only a 32-bit read
at HPTC `fed1f404`, already sanity-checked by native admission. All other MMIO,
widths and writes remain denied. Air7,2 retains its existing equivalent policy;
GPIO stays denied for Air6,1. Re-run connected captures, host/VM checks and an
exact staged five-sample trial with the existing 30-second failure display.

## HPTC correction candidate ready (2026-10-23)

Release `15f29b76df51bab0`, clean compiled source `9316f8a`, uses the captured
HPTC DWORD read with the existing exact firmware/resource admission and bounded
five-sample owner. Native sampling remains pending. GPIO, other chipset accesses
and writes remain denied; the 30-second terminal failure display is retained.

Full build/USB image, native/VM binaries, core/desktop suites, 65 staging tests
and docs checks pass. Both compiled policy profiles exercise every RCRB byte,
width and write refusal; the Air6,1 platform test checks mapping boundaries,
actual memory-access refusal and captured table mutations. Connected replay in
all four modeled lid/AC states initializes with zero errors/denials/leaks and
reads only HPTC `fed1f404` for chipset MMIO. Removing that snapshot permission
reproduces the HPET initialization failure. Q35 normal sampling and denied-access
containment pass; exact staged GPT VM refuses nonmatching native hardware,
saves logs, cleanly unmounts and returns. These are not physical acceptance.

The 203-file USB baseline and offline allocation pass (380 clusters, zero
leaks/mismatches/errors). Candidate/recovery hashes and Ubuntu default are
verified; no one-shot is armed. Console recovery `c35475f62f154f1e`, accepted
desktop telemetry `eee15036f4172920` and passive preflight remain installed.
Failed superseded `674c597b082f2494` was removed from EFI after verifying its
independent backup. New payload/run evidence is retained outside build.
Leave AC and USB connected; photograph any failure during the 30-second pause.
Collect the matching session and verify completion/retirement and USB integrity
before another trial. Missing final logs remain failure/incomplete evidence.

## HPTC retry native return (2026-10-23)

Release `15f29b76df51bab0` (compiled `9316f8a`), session
`00065e895200284c-00`, has boot/session-a logs but no final report. The user's
photo shows namespace setup completed and the runtime reached `phase=sample`.
The final interpreter counters are errors=0/denied=0. This confirms the narrow
HPTC fix resolves the previous initialization denial on this unit; it does not
accept sampling or clean runtime retirement.

The first observed SCI query is decimal 16 (`0x10`), classified as OS-branch
class 5 by Air6,1 policy 3 and refused before dispatch. Terminal reason is
`query policy`. The query's Darwin branch notifies SMB0; its alternate branch
acquires SMTX, reads SMBus state and can clear SSTS. The existing refusal is
intentional; do not simply whitelist the AML method or assume a captured OSYS
value makes every reachable branch safe. The shared runtime already has a
separately owned SMBus host-event handler, but Air6,1 does not yet admit that
profile. Next scope is an Air6,1-specific status/completion/alarm audit and
bounded event-owner integration with tests, retaining exact firmware and storage
gates. No event permissions are changed by this closeout.

All 203 prior USB files remain, with only `.srvros/meta` changed and two matching
logs added. Offline exFAT/allocation pass: 205 files, 383 clusters, zero leaks,
bitmap mismatches or errors. Payload/recovery hashes and Ubuntu default pass;
Ubuntu returned on a new boot with empty one-shot. Photo, logs and return records
are backed up outside build. No reboot is armed.

## Short SMBus capture scope (2026-10-23)

Based on main `eeca0e8`, this unit owns bounded Air6,1 host-event admission in
shared battery runtime/platform, v62 staging and directed host/VM tests.
Air7,2 PR88 input timing and Pro11,2 PR19 Wi-Fi remain independent scopes.
Reuse the existing serialized SMBus event/service/retirement mechanisms with
exact Air6,1 firmware gates. Admit only the automatic short parked capture;
retain v61 query refusal and reject mounted Home, image/files and ordinary
service. No AML `_Q10` dispatch or general EC register permissions are added.

## Short SMBus candidate ready (2026-10-23)

Release `bfde0423543f20d0`, clean compiled source `63bc06f`, uses the v62
short parked host-event capture. Main `2e2eb2b` / PR88 tap diagnostics was
integrated without conflicts; Pro PR19 remains separate Wi-Fi work. Native
battery sampling and runtime retirement remain unaccepted pending this trial.

Full build/USB image, core/desktop suites, 66 staging tests and 476 local docs
links pass. Seventeen Air6,1-policy VM cases cover completion, both OS branches'
alarms, unknown/late events, controller errors/timeouts, descriptor and query
refusals, old-profile refusal, no-event and native refusal. Each has clean offline
allocation; successful cases verify saved receipts and retirement. Air7,2
completion regression passes. After integration the full suites and Air6,1
completion were repeated. The exact staged EFI/GPT VM refuses nonmatching
hardware, saves logs, unmounts and returns. Captured-table replay has zero
errors/denials/leaks. These are modeled evidence, not native acceptance.

The 205-file USB baseline is unchanged; offline exFAT/allocation pass with
383 clusters and zero leaks/mismatches/errors. Payload/recovery hashes, Ubuntu
default and empty one-shot are verified. Console recovery `c35475f62f154f1e`,
accepted desktop telemetry `eee15036f4172920` and passive preflight remain.
Superseded failed `b9a2d264ad3a0dd2` was removed from EFI only after its independent
backup was verified. New payload/run/VM evidence is backed up outside build.

Keep AC and USB connected; no input is required. The capture waits up to
30 seconds for a natural SMBus event and fresh sample generation 3 or later,
with a 128-event ceiling. Photograph any failure during the 30-second display.
Automatic Ubuntu return should follow; missing final evidence remains failure
or incomplete. Collect this release and verify retirement and all prior files
before another trial. No reboot is armed.

## Short SMBus native return accepted (2026-10-23)

Closeout scope from main `5d195a9`: record this unit's returned v62 evidence;
no shared runtime or admission changes. Release `bfde0423543f20d0`, source
`63bc06f`, session `00065e89a7794b63-00` passes the native SMBus receipt validator.
Fresh generation 3, 45 completion events, three handled/zero unclaimed SCI
deliveries, three sample attempts/zero rejected and balanced 1/1 attach/detach
prove this bounded capture. Heap/errors/denied are zero; controller/resource
retirement and foreign-mask restoration are recorded. A complete exit log and
completed marker were saved. No alarms or acknowledgement writes occurred.

Accept short parked battery sampling and clean retirement with AC connected.
Gauge accuracy, alarms, AC transitions, sustained operation and desktop battery
monitoring remain open; ordinary startup stays off. Next recommended work is a
bounded attended AC unplug/reconnect capture with Air6,1 admission/query audit.

All 205 previous files remain, with expected metadata change and three new logs.
Offline fsck/allocation pass: 208 files, 387 clusters, zero leaks/mismatches/errors.
Ubuntu returned with consumed one-shot; current/recovery hashes and default pass.
Current stays `bfde0423543f20d0`, recovery `c35475f62f154f1e`. USB is unmounted;
no reboot is armed. Matching logs, validation and return state are backed up
outside build before another trial.

## Attended parked AC transition scope (2026-10-23)

From main `249bb97`, this unit owns the next bounded Air6,1 attended AC
transition capture: shared battery capture consumer/admission, boot recipe,
collector and directed host/VM tests. Reuse the accepted serialized service and
host-event owner with USB parked; keep ordinary/Home/file ownership disabled.
Audit captured AC query effects and retain exact firmware/access gates. PR99 log
capacity guards are consumed; Pro PR19 remains independent Wi-Fi work. Prepare
and validate before an attended unplug/reconnect trial; no reboot is armed.

## Attended AC candidate ready (2026-10-23)

Release `321ba9c91e681a4c`, clean source `95c5219`, prepares v63 with the existing
parked SMBus service and exact Air6,1 firmware/access gates. It requires three
coherent fresh samples in each connected/disconnected/reconnected phase, with a
120-second deadline per phase and 4096-event ceiling. No native AC-transition
acceptance is claimed yet; ordinary battery startup remains off.

Full build/core/desktop, 67 staging tests, 24 collector checks, transition/cache
and malformed receipt tests pass. Captured `_Q21`/`_PSR` replay in all four lid/AC
states initializes/disconnects with zero errors/denials/leaks. Seven directed VM
cases pass: normal transitions, mid-read incoherence, alarm refresh, unsupported
query containment, profile refusal, native refusal and a real 120-second phase
timeout. Short Air6,1 and Air7,2 capture regressions pass. An earlier alarm fixture
reconnected before three fresh readings and correctly saved an incomplete result;
its receipt and clean filesystem evidence are retained separately. The adjusted
fixture completes after post-alarm stabilization. Exact staged EFI/GPT refuses
nonmatching native hardware, saves, unmounts and returns automatically.

The 208-file USB baseline is unchanged; fsck/allocation pass with 387 clusters
and zero leaks/mismatches/errors. Candidate and recovery hashes, Ubuntu default
and empty one-shot pass. Console recovery `c35475f62f154f1e`, accepted short
capture `bfde0423543f20d0`, desktop telemetry and passive preflight are retained.
Superseded failed HPTC trial `15f29b76df51bab0` was removed from EFI after its
independent backup was verified. Payload, run and validation evidence are saved
outside build. No reboot is armed.

Start with AC connected. Wait for UNPLUG AC, unplug only the charger, then wait
for RECONNECT AC and reconnect it. USB stays connected throughout. Prompts repeat
every five seconds. Completion or phase timeout saves and returns to Ubuntu;
photograph any fault during the 30-second pause. Collect the matching transition
receipt and verify runtime retirement and USB integrity before another run.

## Attended AC return closeout (2026-10-23)

Scope from main `d753051`: record this unit's native v63 acceptance in the
machine ledger and battery guide; no runtime or access-gate changes. Open PR102
(Air7,2 filesystem scale) and PR19 (Pro Wi-Fi) remain independent work.

Release `321ba9c91e681a4c`, source `95c5219`, session `00065e8a183ee909-00`
passes the strict transition receipt: three fresh samples per connected,
disconnected and reconnected phase, generations 1–3/5–7/10–12. Disconnected
current is -1328000 µA; connected currents are zero. Twelve attempts have zero
rejections; 182 events include two alarms/two acknowledgements, no unknowns.
Attach/detach is 1/1 and retired heap/errors/denied are zero. Post-alarm freshness,
resource restoration, complete exit log and Ubuntu return are verified.

All 208 prior files are preserved except expected metadata replacement; three
new logs produce 211 files. Offline checks pass with 391 clusters and zero
leaks/mismatches/errors. Payload/recovery hashes and Ubuntu default pass; one-shot
is consumed. USB is unmounted and no reboot is armed. Evidence is independently
backed up outside build. Accept only bounded parked AC transitions and observed
alarm refresh, not gauge accuracy, sustained service or desktop ownership.
Next recommended scope is bounded mounted-USB/file coexistence before desktop
battery integration; retain exact Air6,1 admission and serialized retirement.

## Bounded battery/file coexistence scope (2026-10-23)

From main `d7ed773`, this unit owns Air6,1 admission to the existing parked
exclusive-image/Text file capture, its distinct v64 recipe and directed policy/VM
checks. Reuse the serialized SMBus owner, 512-event ceiling and client/runtime/
image retirement order. No new AML/register permissions, mounted Home admission
or ordinary battery startup. PR102 filesystem scale and PR19 Wi-Fi remain
independent; use this main baseline without importing either active change.
Prepare a fresh disposable USB image, exact-stage checks and retained recovery
before native acceptance. No reboot is armed.

## Bounded battery/file candidate ready (2026-10-23)

Release `fc1d1f4c45d30252`, clean source `cac24ef`, uses the v64 Air6,1
exclusive-image capture. It preserves exact firmware/AML permissions and the
512-event ceiling. Ordinary mounted Home and desktop battery startup remain off;
physical battery/file coexistence is pending this trial.

Full build/USB image, native/model kernels, core/desktop suites, 68 staging tests,
24 collector tests, five strict file-receipt tests and exhaustive admission
checks pass. Eight directed Air6,1 file VMs pass: full completion, recurring
alarms, Stop during read/ack, uncertain acknowledgement containment, event limit,
profile refusal and native refusal. Completion gives 60 edits/five file cycles
and generation 25; recurring alarms complete with 15 acknowledgements and fresh
generation 30. Both images and unrelated files verify. Three regressions pass:
Air6,1 short capture/AC transitions and Air7,2 file capture. Exact staged EFI/GPT
refuses unsupported native hardware, saves, unmounts and returns; missing USB
also returns without starting the probe.

The fresh 16 MiB image is the only added USB file. All 211 prior files are
preserved; 212-file preboot baseline verifies. Offline fsck/allocation and
headroom pass: 519 clusters, zero leaks/mismatches/errors. Installed candidate,
recovery and independent payload hashes pass; Ubuntu is default and one-shot
is empty. Console recovery `c35475f62f154f1e`, accepted AC capture
`321ba9c91e681a4c`, short sampling and desktop telemetry remain retained.
Superseded brightness-only EFI payload `59086591e526632b` was removed only after
its complete independent backup was verified. Run/payload/VM evidence is saved
outside build. USB is unmounted and no reboot is armed.

Keep AC and USB connected; no input or charger changes are required. Expect
60 automatic edits and five Text save/open cycles, then saved Ubuntu return.
Photograph any fault. On return collect this release before further work and
verify its complete receipt, exact inner image, all other files and both
allocation graphs. A quick return alone is not acceptance.

## Battery/file return closeout scope (2026-10-23)

From main `7638d57` (including PR102 filesystem scale), this closeout only records
the returned Air6,1 candidate and missed image preparation check. No shared
runtime/filesystem changes; Pro PR19 Wi-Fi remains independent. The installed
physical candidate predates PR102 and stays identified by its own source/release.

## First battery/file return: image preparation refused (2026-10-23)

Release `fc1d1f4c45d30252`, source `cac24ef`, session `00065e8a98eec3bc-00`
saved `completed=NO` with `battery media parked=NO fault-reset=NO`. The exclusive
image was refused before outer unmount, battery runtime entry or Text/file work.
This is failed preparation, not coexistence acceptance or a runtime failure.

The 16 MiB image has the correct size/valid length but its 128-cluster FAT chain
jumps from cluster 392 to 394. The read-only native-driver `--battery-image`
check reproduces `image-range: REFUSED`. General allocation and fsck passed
before boot, but the dedicated contiguous-extent check was omitted. A valid
filesystem and matching image bytes do not prove exclusive-range admission.

All 212 prior files remain unchanged except expected `.srvros/meta`; the inner
image is byte-identical to its fresh baseline. Three logs bring the total to
215 files. Outer fsck/allocation pass (523 clusters, zero leaks/mismatches/errors);
inner checks pass (two files, seven clusters). Payload/recovery hashes, Ubuntu
default and consumed one-shot verify. USB is unmounted, no reboot is armed, and
matching logs/image/layout/return evidence is independently backed up.

Next prepare a fresh contiguous image, preserving this failed run's evidence,
and require `check_exfat_allocation.py --uuid 6ADA-85BB --battery-image` to pass
on the unmounted real drive before installation/boot. Do not relax exclusive
range checks or treat another general allocation pass as a substitute. Any new
payload built on the later integration baseline needs its own exact-stage
validation; this run does not accept the filesystem-scale changes from PR102.

## Contiguous-image retry scope (2026-10-23)

From main `c43d27c`, this preparation only replaces the unused disposable image's
allocation and repeats exact-payload/media checks. Reuse installed release
`fc1d1f4c45d30252`, clean compiled source `cac24ef`, without rebuilding or importing
PR102 filesystem changes. PR106 package-energy measurement and PR19 Wi-Fi remain
independent. Preserve the failed native evidence and image bytes; no kernel or
firmware permissions change. Native battery/file coexistence remains pending.

Holding the original image while writing a new copy first encountered a second
one-cluster gap. Holding both unused copies produced a contiguous 128-cluster
image at byte offset 88473600 (first cluster 653, NoFatChain set). Both temporary
files were removed only after byte comparison. Final real-drive `--battery-image`
passes, independent FAT layout inspection has no breaks, and all 215 original
file hashes still match. Offline fsck/allocation passes with 523 clusters and
zero leaks/mismatches/errors. The retry prelaunch script now requires the extent
check explicitly; a general filesystem pass cannot satisfy it.

The repeated exact-stage EFI/GPT rehearsal passes: native platform refusal,
saved diagnostics, verified disposable files, clean unmount and return. Installed
candidate/recovery hashes, Ubuntu default and empty one-shot pass. The final
prelaunch check verifies AC, unchanged host boot, USB UUID, all 215 file hashes,
fsck and the required contiguous extent. Retry evidence has its own durable
backup alongside the unchanged payload and failed run. USB is unmounted and no
reboot is armed. Keep AC/USB connected; the automatic 60-edit/five-save test
needs no input. Collect its matching session before any subsequent trial.

## Battery/file coexistence native acceptance (2026-10-23)

Closeout from main `94c9b15`: update only this unit's capability records and
current guide. PR106 power measurement and PR19 Wi-Fi remain independent.
Release `fc1d1f4c45d30252` / source `cac24ef`, retry `bcbabcafe3b8c68c`, session
`00065e8ac0a116ff-00` passes the strict receipt and exact inner-image validation:
60 edits, five file cycles/20 operations, fresh generation 24 and 360 completion
events. All 24 SCI deliveries were handled, none unclaimed; no alarms occurred.
Three descendants, runtime and image retire in order; attach/detach 1/1 and
heap/errors/denied zero. This accepts bounded exclusive-image coexistence only.

Prior files are preserved except intended image/meta changes; three new logs
produce 218 files. Outer/inner fsck and allocation pass (527/14 clusters), no
leaks/mismatches/errors; the image extent remains contiguous. Ubuntu default,
consumed one-shot and candidate/recovery hashes pass. USB is unmounted, no reboot
is armed, and evidence is backed up separately from the failed preparation.
Next scope is explicit mounted Home ownership before ordinary desktop battery
startup. No acceptance is inferred for the later PR102 filesystem implementation.

## Explicit mounted Home sampling scope (2026-10-23)

From main `7d2b903`, own only the Air6,1 short held-Home diagnostic admission,
normal-return refusal boundary, distinct boot recipe and directed tests. Reuse
the mount hold and serialized service/retirement; no desktop clients or ordinary
startup. Reset remains disabled with mounted storage; uncertain runtime halts
without filesystem cleanup. PR106 power measurement and PR19 Wi-Fi remain
independent. Consume PR102 filesystem baseline with fresh build and exact-stage
checks; no native acceptance is inherited for the new payload.

## Mounted Home candidate ready (2026-10-23)

Release `30b0d713f1a9ed55`, clean compiled source `c69cd23`, uses v65 short
held-Home sampling. Exact firmware/access gates remain unchanged. It enables
neither desktop clients nor ordinary service; uncertain faults retain the mount
and halt, with no automatic reset or live filesystem cleanup. Native acceptance
remains pending.

Full rebuild/USB image, core/desktop suites, exhaustive admission/query-receipt
checks, 69 staging tests, 24 collector tests and four receipt tests pass. Eight
Air6,1 mounted ownership VMs cover normal/alarms, effect-free refusal, unsupported
platform/profile, unmounted entry and two retained-fault cases. Four full-boot
captures cover success/alarm, saved query refusal and uncertain acknowledgement
halt. Air7,2 Home and Air6,1 image/file regressions pass. Exact staged EFI/GPT
and missing-workspace rehearsals pass. An initial harness classification wrongly
expected image revocation for v65; checker fix `d3a1f9e` separates the profiles
and the repeated exact run passes. That failed harness evidence is retained.

The existing used image and all 218 USB file contents are unchanged. Offline
fsck/allocation/headroom pass (527 clusters, zero leaks/mismatches/errors).
Installed/recovery hashes, Ubuntu default and empty one-shot verify. Console
recovery `c35475f62f154f1e`, accepted image capture `fc1d1f4c45d30252`, AC capture,
short sampling and desktop telemetry remain retained. Superseded audio-only EFI
payload `0533c0dfc63d104f` was removed only after verifying its independent backup.
Run, payload and VM evidence are backed up outside build. USB is unmounted and
no reboot is armed.

Keep AC/USB connected; no typing or charger changes are required. Expect a few
fresh samples with Home held, then retirement, saved logs and Ubuntu return.
If it halts, photograph the screen and report it; do not treat a reset or missing
completion as success. Collect matching `battery_home`, unchanged prior files,
filesystem/allocation and recovery evidence before another trial.

## Mounted Home sampling accepted (2026-10-23)

Close out the authorized v65 run on main `de449b3` (MR109). Scope is this
unit's evidence and current status; PR106 power work and PR19 Wi-Fi remain
independent. No shared runtime or device admission changes are needed.

Release `30b0d713f1a9ed55`, compiled `c69cd23`, session
`00065e8b198ee6ba-00` completes the strict native `battery_home` receipt:
three fresh generations, 45 completion events, three handled/zero unclaimed SCI
deliveries and no alarms. Runtime retires with attach/detach 1/1 and zero
heap/errors/denied; the same Home mount is held through retirement, then released.
The completed capture saves its exit log and returns to Ubuntu.

All 218 prior files survive; only expected `.srvros/meta` changes, with three
new logs. Offline fsck and allocation/headroom pass: 221 files, 531 clusters,
zero leaks/mismatches/errors. Candidate/recovery hashes and Ubuntu default pass;
the one-shot is consumed. Raw logs, strict receipt, file comparison and recovery
checks are backed up outside build. USB is unmounted and no reboot is armed.

This accepts short mounted Home sampling only, with no desktop clients or AC
changes. Ordinary desktop startup remains off; sustained coexistence and gauge
accuracy are not accepted. Next is the explicit bounded Home desktop session
with Text saves and repeated Start/Stop before ordinary desktop integration.

## Bounded Home desktop session scope (2026-10-23)

From actual main `ce1c0a6` (including PR106 Text polling changes), own only
Air6,1 admission to the existing bounded Home controls/file diagnostic, its v66
recipe and directed validation. Reuse three service lifetimes, five control
requests, 60 Text edits/five saves, 512 aggregate events and existing deadlines.
Preserve the exact mount through every epoch and client/runtime retirement.
Ordinary/continuous desktop admission stays off, and uncertain mounted failures
halt without cleanup/reset. PR19 Wi-Fi remains independent. The prior v65 run
is collected and archived; this new payload requires its own physical evidence.


## Bounded Home session candidate validated (2026-10-23)

Release `f60d9b426e2693f3`, clean compiled source `12aad26`, uses v66
`battery-home-session`. Full build/USB image, core/desktop suites, 70 staging
tests, 25 collector tests, 78 receipt tests and exhaustive admission checks pass.
Six Air6,1 boot fixtures cover completion, query/profile refusal, two retained
faults and delayed EC setup with throttled USB writes. Completion proves five
controls/three lifetimes, 60 edits/five cycles/20 operations and exact saved
bytes. Slow checkpoints took 7.99–8.29 seconds without changing deadlines.
Air7,2 Home-session and Air6,1 short-Home regressions pass. Exact EFI/GPT and
missing-workspace rehearsals pass; the latter intentionally emits no standard
result JSON, so its PASS output and serial evidence are retained separately.

All 221 existing USB files were preserved while adding a fresh `battery-study`
anchor. Offline allocation/headroom pass: 222 files, 533 clusters, no leaks or
mismatches. Superseded passive preflight EFI payload `9b528e09c13e1d4b` was
removed only after verifying its independent payload backup to make install
space. Console recovery and accepted runtime captures remain retained.

Physical acceptance is pending. Keep AC/USB connected and provide no input;
normal completion/refusal saves and returns to Ubuntu. Photograph any halt.
Require matching `battery_home_session`, five saved files containing `AA`,
unchanged prior file contents, clean allocation and consumed one-shot. Final
installation and unarmed state are recorded in the local run/launch records.

## Bounded Home session failed, clean retirement (2026-10-23)

Close out only this unit's native evidence on main `db10404` (MR111).
PR112 owns compositor idle work and PR19 Wi-Fi; no shared implementation changes
are made in this closeout. Session `00065e8b68f3ad24-00`, release
`f60d9b426e2693f3` / compiled `12aad26`, is **failed**, not accepted.

The generated workload stopped at 12 generated/11 presented edits, zero saves
and one Start request. Initial EC setup took 4.941 seconds. At failure the Start
was still waiting for fresh generation 2; Text reached its first save with
`waiting=1`, and `tool_file_capture_begin` rejects a pending control. This points
to a diagnostic sequencing race, requiring a reproducer with delayed second
sample before implementing a fix. The existing slow-setup/storage VM did not
cover this native readiness ordering. Do not simply widen hardware permissions
or classify the clean return as workload success.

The service completed generation 2/30 events, with two handled and zero unclaimed
SCI deliveries, then retired: attach/detach 1/1, zero heap/errors/denied. Three
clients retire, Home releases, the failed report is saved and Ubuntu returns.
All 222 prior files are unchanged except expected `.srvros/meta`; three new logs
bring the total to 225. The study still contains only its original anchor; no
Text output was saved. Offline fsck/allocation/headroom pass (537 clusters,
zero leaks/mismatches/errors), installed/recovery hashes pass and the one-shot
is consumed. Raw evidence, study contents and file checks are backed up outside
build. USB is unmounted; no reboot is armed and no retry is prepared.

Next: make generated saves wait for initial Start readiness, reproduce delayed
fresh generation 2, and rerun directed session tests before preparing a retry.
Keep this failed evidence and existing study directory intact during preparation.

## Start/save sequencing repair scope (2026-10-23)

From main `89b19ec`, own the generated Text capture's readiness sequencing and
VM delayed-second-sample fixture/regression. Preserve all runtime admission,
firmware permissions, owner/mount lifetime and deadlines. PR112 owns compositor
idle work; this fix avoids its files. PR19 Wi-Fi remains independent. Reproduce
the collected v66 failure before correcting the capture, then prepare a fresh
study directory while archiving the previous failed study without overwrites.

The delayed-generation-2 fixture reproduced the same 11-edit/zero-save failure
before the Text fix. PR112 merged while this work was underway; main `180b542`
has now been integrated without overlap. Rebuild and session regressions include
its compositor idle-wait changes; no prior native acceptance is inherited.

## Start/save retry ready for physical validation (2026-10-23)

Release `7fe7d3d20d91997c`, clean compiled source `5086cba`, fixes generated
checkpoint sequencing without changing runtime permissions or deadlines. The
four-second delayed-generation fixture reproduces the old 11-edit/zero-save
failure; with the fix it verifies fresh Start before write submission, all
60 edits/five exact saves and three service lifetimes. An excessive eight-second
delay still fails the existing Start deadline without submitting a write.

Full build/USB image, core/desktop, 70 staging, 25 collector and 78 receipt tests
pass. Seven Air6,1 session fixtures cover normal/delayed completion, slow setup
and storage, query/profile refusal and retained faults. Air7,2 delayed-session
and Air6,1 short-Home regressions pass. Exact EFI/GPT save/unmount/return and
missing-workspace refusal rehearsals pass. PR114 includes main `180b542`;
PR115 logger timing and PR19 Wi-Fi are independent and not included.

The failed study is preserved as `battery-study-failed-73239eaf664ab4ca`; a fresh
anchor brings the USB baseline to 226 files/539 clusters with clean allocation.
Superseded short SMBus payload `bfde0423543f20d0` was removed from EFI only after
verifying its durable backup; accepted mounted Home and console recovery remain
installed. Payload and evidence backups live outside build. Run
`9055d4b29173fe40` records final installation and unarmed state. Keep AC/USB
connected with no input; normal completion returns to Ubuntu, uncertain mounted
faults halt for a photograph. Native acceptance and ordinary battery startup
remain pending/off respectively; collect this matching run before another trial.


## Bounded Home Start/save retry accepted (2026-10-23)

Close out this unit's run on main `adf40df` (MR114), touching only this record
and the current battery guide. PR115 logger timing and PR19 Wi-Fi remain
independent. Session `00065e8bbf429e4a-00`, release `7fe7d3d20d91997c` /
compiled `5086cba`, passes the strict native Home-session receipt.

The first checkpoint deferred at serial 12 until Start observed fresh generation
2, then submitted its write. All 60 edits, five exact `AA` files, 20 worker
operations and five controls across three lifetimes completed. The service
processed 180 events with zero alarms; SCI totals are 12 handled/zero unclaimed.
Every lifetime detached cleanly with zero heap/errors/denied; all three clients
retired, the final active service retired, and Home released before saved return.

All 226 prior USB files retain their bytes except expected `.srvros/meta`;
five study files and three logs bring the total to 234. Offline fsck and
allocation/headroom pass: 548 clusters, no leaks/mismatches/errors. Installed
payload/recovery hashes and Ubuntu default pass; the one-shot is consumed and
USB is unmounted. Matching raw evidence, exact study bytes and verification
records are backed up outside build. No next trial or reboot is armed.

This accepts the bounded generated Home/Text/control workload only. Physical
input latency, continuous desktop monitoring and gauge accuracy remain separate.
Ordinary battery startup stays off. Next is a bounded continuous Home desktop
trial, reusing the existing ownership/retirement contract and requiring its own
Air6,1 fixture and native acceptance before any ordinary-startup change.

## Bounded continuous desktop scope (2026-10-23)

From main `4a991df`, including MR115 logger timing and MR116 native closeout,
own only Air6,1 admission to the explicit three-minute Home desktop capture,
its separate v67 stage and directed fixtures. Preserve the 240-second watchdog,
checked client/runtime/logger retirement and mounted-fault halt policy. Ordinary
startup, indefinite sessions and attended Home transitions stay excluded. Keep
firmware/query permissions unchanged. PR19 Wi-Fi has independent staging changes;
preserve its recipes and avoid xHCI/network files. Previous run is collected;
this trial requires its own evidence and must preserve all current USB files.

## Continuous Home candidate validated (2026-10-23)

Release `a18d72aecdf808c7`, clean compiled source `503dd05`, selects v67.
Full build/USB image and core/desktop suites pass, as do exhaustive native
admission checks, 71 staging, 26 collector and four continuous receipt tests.
Air6,1 completion ran 189.6 seconds, advanced generation 1→76 with 2440 events,
saved the desktop screenshot and periodic/final logs, and retired five clients
and runtime cleanly. Profile/query refusals and uncertain-acknowledgement halt
cases pass. Air7,2 continuous capture and Air6,1 delayed Start/save regressions
pass. Exact EFI/GPT save/unmount/return and missing-workspace rehearsals pass.
The VM screenshot was inspected; native indicator accuracy remains untested.

Run `eddc64efe16de005` preserves all 234 USB files unchanged, including the
accepted and failed study artifacts. Offline fsck/allocation/headroom pass with
548 clusters and no leaks/mismatches/errors. The failed initial v66 EFI copy
`f60d9b426e2693f3` was removed only after verifying its independent backup;
accepted v66 `7fe7d3d20d91997c`, short Home and console recovery remain retained.
Payload and evidence backups are outside build. Final installation and unarmed
state are recorded in the run/launch files; native acceptance is pending.

Keep AC/USB connected and provide no input. Allow three minutes plus startup,
screenshot saving and retirement before automatic Ubuntu return. Photograph any
halt. Collect the matching strict `battery_home_desktop` proof, all four logs,
new screenshot, prior-file hashes and offline allocation before another run.


## Continuous Home desktop accepted (2026-10-23)

Close out this unit's run on main `2702b41` (MR117), touching only acceptance
records/current guides. PR118 storage wait work and PR19 Wi-Fi are independent
and are not part of this tested payload. Session `00065e8c7ab70670-00`, release
`a18d72aecdf808c7` / compiled `503dd05`, passes the strict native continuous
Home receipt. The desktop idle window lasted 180.074 seconds; sampling covered
189.823 seconds and advanced generation 1→71 with 1064 events and zero alarms.
All 71 SCI deliveries were handled, with zero unclaimed. Five lifetime members
retired, the service detached 1/1 with zero heap/errors/denied, the logger retired
and Home released before saved Ubuntu return.

All four logs and the new 1366×768 screenshot are collected and hash-verified.
Visual inspection confirms ordinary Text and an `AC · 98%` indicator; it does
not calibrate gauge accuracy. All 234 prior files retain their bytes except
expected `.srvros/meta`; four logs and one screenshot bring the total to 239.
Offline fsck/allocation/headroom pass: 586 clusters, no leaks/mismatches/errors.
Installed/recovery hashes and Ubuntu default pass; the one-shot is consumed.
USB is unmounted. Evidence and screenshot copies are backed up outside build;
no new trial or reboot is armed.

This accepts the explicit three-minute continuous capture only. Ordinary battery
startup, longer endurance, attended input latency and gauge accuracy remain
unaccepted. Next is a separately bounded attended Home AC unplug/reconnect trial
to verify desktop indicator updates and retirement across power transitions.

## Attended Home AC transition scope (2026-10-23)

From main `d5c080d`, own only Air6,1 admission to the existing attended Home
AC transition capture, its separate v68 recipe and directed tests. Keep exact
firmware/query permissions, mount/client ownership and checked retirement;
ordinary startup and indefinite sessions remain excluded. Preserve the existing
three fresh readings per phase, two-minute phase limit and 480-second capture
watchdog, plus the Air6,1 4096-event ceiling. PR118 owns USB wait work and PR19
Wi-Fi; avoid their driver files and preserve their independent recipes. The v67
run is collected/accepted. This trial starts on AC, prompts for unplug/reconnect,
and must retain USB throughout; each phase saves its own screenshot.

## Attended Home AC candidate validated (2026-10-23)

Release `5cd210467e3a10fd`, clean compiled source `fe85b9c`, selects v68.
Full build/USB image, core/desktop, exhaustive capability checks, 72 staging,
26 collector and four receipt tests pass. Five Air6,1 transition fixtures cover
completion, profile/query refusal, retained uncertain-acknowledgement fault and
missing-transition timeout. Completion takes 69.4 seconds with ordered phases
at generations 1–3, 9–11 and 25–27; three verified screenshots show
`AC · 86%` → `86%` → `AC · 86%`, with appropriate current direction. Screenshot
indicator/prompt regions were visually inspected. Timeout saves an incomplete
result and returns only after clean retirement; uncertain ownership halts.
Air7,2 transitions and Air6,1 three-minute continuous regressions pass. Exact
EFI/GPT save/unmount/return and missing-workspace rehearsals pass.

Run `f76a5bcce314a958` retains all 239 USB files unchanged. Offline fsck and
allocation/headroom pass: 586 clusters, no leaks/mismatches/errors. Superseded
short Home EFI payload `30b0d713f1a9ed55` was removed only after verifying its
durable backup; accepted v66/v67 and console recovery remain installed. Payload
and evidence backups are outside build; final install/unarmed state is recorded
locally. PR118 storage waits and PR19 Wi-Fi are not part of this tested source.

Native acceptance is pending. Start on AC; keep USB connected throughout. Wait
for the screen to request unplugging AC, then reconnect only at its next prompt.
Wait through screenshot saving without changing power early. No keyboard input
is required. Each phase allows two minutes; normal completion or checked
incompletion saves and returns. Photograph any halt. Collect matching strict
transition proof, all logs/three screenshots, previous file hashes and offline
allocation before another run. Ordinary startup remains disabled.

## Attended Home AC first run incomplete (2026-10-23)

Close out native evidence only on main `bbdbefb` (MR120); PR118 USB wait and
PR19 Wi-Fi work remain independent. Session `00065e8ccdec4fb4-00`, release
`5cd210467e3a10fd` / compiled `fe85b9c`, is incomplete, not accepted. Connected
phase 0 passed at generations 1–3 (`AC · 98%`, current zero), and one screenshot
saved. After prompting for phase 1, the capture timed out without accepting
three fresh disconnected readings. There is no reconnect-phase evidence. Human
observations of unplugging and the indicator are pending; do not infer a driver
fault or missed user action from this receipt alone.

Sampling reached generation 51 with 765 events, zero alarms and 51 handled/zero
unclaimed SCI deliveries. All five lifetime members retired; service detach was
1/1 with zero heap/errors/denied, logger retired, Home released and the failed
report saved before Ubuntu return. All 239 prior files retain their bytes except
expected metadata; four logs and one screenshot bring the total to 244. Offline
fsck/allocation/headroom pass: 624 clusters and zero leaks/mismatches/errors.
Installed/recovery hashes and Ubuntu default pass, the one-shot is consumed,
and USB is unmounted. Raw evidence and screenshot are backed up outside build.
No retry or reboot is armed. Preserve this run while resolving the phase-1 cause;
ordinary startup and attended Home transition acceptance remain disabled/pending.


## Attended Home AC retry accepted (2026-10-23)

Close out this unit's evidence on main `54862de` (MR121); no shared runtime
changes. PR118 USB wait and PR19 Wi-Fi remain independent. The user confirmed
that AC was not unplugged in the first v68 attempt. That incomplete evidence
remains preserved; an explicitly authorized same-payload retry used a fresh run
record and the 244-file baseline without source changes.

Run `f36692208f9cb16f`, session `00065e8d108fab51-00`, release
`5cd210467e3a10fd` / compiled `fe85b9c`, passes the strict native transition
receipt. In 47.686 seconds sampling advanced generation 1→18. Connected phase
accepted generations 1–3 (`AC · 98%`, current 0); disconnected phase accepted
7–9 (`97%`, −1,329,000 µA); reconnected phase accepted 14–16 (`AC · 97%`,
current 0). All three 1366×768 screenshots were copied, hash-verified and
visually inspected. They confirm the indicator and prompt states; percentage
calibration and sustained charging behavior are not claimed.

The service processed 272 events, including two alarms/acknowledgements/cache
invalidations. All 19 SCI deliveries were handled with zero unclaimed. Five
lifetime members retired, service detach was 1/1 with zero heap/errors/denied,
and logger/mount retirement preceded saved return. All 244 prior files retain
their bytes except expected metadata; four logs and three screenshots bring the
total to 251. Offline fsck/allocation/headroom pass: 728 clusters and zero leaks,
mismatches or errors. Installed/recovery hashes and Ubuntu default pass; one-shot
is consumed, USB is unmounted and no new trial is armed. Retry evidence is backed
up separately outside build, preserving the original incomplete run.

This accepts bounded attended desktop power transitions and indicator updates.
Ordinary battery startup remains disabled. Longer attended desktop/file/input
coexistence and endurance remain separate before any startup promotion.

## Attended desktop/file/input scope (2026-10-23)

From main `7b0d1a0`, including merged MR118 USB waits and MR122 AC acceptance,
own an explicit Air6,1 attended Home capture: five-minute minimum work interval,
periodic sampling/logging, screenshot and normal user-requested desktop exit.
An eight-minute owner overrun must halt with the mount retained, not force away
unsaved work. Require physical-input counts and successful ordinary Text save/open
receipts, preserving prior files. Ordinary startup remains off; firmware/query
permissions stay unchanged. PR123 network polling and PR19 Wi-Fi are independent;
preserve their main/staging recipes and avoid driver edits. Previous AC retry is
collected and accepted; this trial needs its own run and native evidence.

## Attended Home work candidate (2026-10-23; native pending)

Source `022d468`, v69 release `8245e30a2e9019ec`, prepares a five-minute
attended Home session with continuous battery sampling, typing, Text save/reopen
and Files work. At five minutes it saves a screenshot and prompts for normal
SrvrOS > Return to console; dirty Text retains its close veto. An eight-minute
owner deadline halts with live ownership retained. Ordinary startup and native
firmware/query gates remain unchanged. See the current battery guide for steps.

Local core/desktop build tests, 74 staging tests, 26 collector tests and five
strict receipt tests pass. Directed Air6,1 Q35 cases cover completion, early
exit, unsaved-work overrun, platform refusal and query refusal. Completion ran
305.895 seconds, generation 1→123, with 3,944 events and seven lifetime members;
Text bytes, directory metadata, filesystem/allocation and full retirement pass.
The overrun retains live ownership without reset or a completion claim; periodic
sampling continued through 479 seconds. Native-style collector recipe negatives
and the existing three-phase AC capture regression pass. Exact staged GPT boot
refuses native hardware in Q35, saves/unmounts/returns; missing Home returns before
starting the probe. These are VM results, not native endurance acceptance.

Run `e787736ee4592483` preserves the accepted 251-file USB baseline (728 clusters,
zero leaks/mismatches/errors). `attended-work.txt` is available as a new file.
Ubuntu and console recovery `c35475f62f154f1e` remain the recovery plan. The
superseded Start/Stop EFI copy was removed only after independent backup hashes
matched; accepted continuous and AC payloads remain installed. The next boot
must collect its own screenshot, periodic/final logs, input/file receipts, new
file contents, prior-file hashes and clean offline allocation before acceptance.

### Integrated candidate after MR123

Main `92722b7` landed during validation and is consumed by `6f25115`. The sole
merge conflict was the boot-smoke result dictionary; both attended-work and
network-probe fields are retained. Final v69 release `78fe53e8cacdb1ba` supersedes
the never-installed `8245e30a2e9019ec` candidate. Rebuild/core/desktop and all
105 staging/collector/receipt tests pass again. Completion (305.423 seconds,
generation 1→123, 3,944 events, seven members), early exit, both refusals, the AC
transition regression and both exact staged GPT/missing-Home checks pass on the
integrated source. The eight-minute overrun receipt remains from `022d468`;
battery runtime/desktop sources and the initramfs are unchanged by integration.
The earlier stage and evidence remain preserved. Native acceptance is still
pending; the same unarmed run and 251-file baseline apply to this final payload.

## Attended Home work first return: incomplete

Close out evidence on main `c4265f8`; no shared runtime changes. Run
`e787736ee4592483`, session `00065e8e0a5dafa7-00`, release `78fe53e8cacdb1ba`
(compiled `6f25115`) exited through the normal menu after 211.554 seconds.
The five-minute minimum and screenshot phase were not reached. There were 25
key presses and 1,225 pointer events. Text saved and reopened an 11-byte new
file successfully, but there is no Files-app listing receipt. The result is
incomplete, not accepted; whether the early exit was intentional is pending
human confirmation. No driver fault is inferred from this alone.

Battery generation advanced 2→81 with 1,231 events, two alarms/acknowledgements/
cache invalidations and 86 handled/zero unclaimed SCI deliveries. Four lifetime
members drained; runtime detach was 1/1 with zero heap/errors/denied. Logger and
mount retirement completed and the failed report was saved before Ubuntu return.
All 251 prior files retain their bytes except expected metadata; four logs and
the new Text file bring the total to 256. Offline fsck/allocation pass with 734
clusters and zero leaks/mismatches/errors. Installed/recovery hashes, Ubuntu
default and consumed one-shot pass; USB is unmounted. Evidence is preserved
outside build. No retry is armed; five-minute attended work acceptance remains
pending, and ordinary battery startup remains disabled.

## Screenshot receipt correction scope (2026-10-24)

Base `bf1b9a6` includes PR127 storage timing and PR125 TCP readiness. The
bounded objective is to emit the screenshot dimensions receipt with ordinary
non-traced desktops and exercise the real image verifier in the Home capture VM.
Scope: compositor capture logging, Home VM/collector regressions, this record
and the Air6,1 battery guide. PR19 at `7e70c51` owns independent Pro USB Wi-Fi
work; no storage, firmware, query permissions or startup policy changes planned.
Next checkpoint: corrected completion VM, exact staged GPT rehearsal and an
unarmed physical retry with separate Files-app use.

Retry `ab44de460a170c07`, session `00065e8e40c90399-00`, on release
`78fe53e8cacdb1ba` completed normally after 361.122 seconds: 55 keys, 1,356
pointer events, a successful 33-byte Text save/reopen and a saved valid BMP.
Battery generation advanced 1→138; 2,062 events, no alarms and 162 handled/zero
unclaimed SCI deliveries. Seven members and runtime/logger/mount retired cleanly,
with zero heap/errors/denied. All 256 prior files retain their bytes except
metadata; 262 files/773 clusters pass offline fsck/allocation. Ubuntu recovery
and consumed one-shot verified; evidence is preserved outside build.

Full attended-work acceptance remains incomplete. The user confirms using
New window from the search menu, then Open file: this exercised Text's dialog,
not the separate Files application. The saved screenshot lacks its dimensions
receipt because it was gated on compositor tracing; the strict collector
correctly refuses it. The earlier completion VM checked BMP dimensions but
never invoked that collector helper. Keep both incomplete trials as evidence.
The next attempt must use the Files folder icon in the dock and wait for the
five-minute completion prompt before normal menu exit.

### Corrected candidate validated (PR128)

Source `aff686d`, v69 release `2cd5b8604f2406c5`, emits capture dimensions
without tracing. The unchanged strict screenshot verifier rejects both original
physical and completion-VM evidence, then accepts the corrected non-traced VM.
Full build/core, 74 staging, 27 collector and five receipt tests pass. Completion
ran 306.107 seconds, generation 1→123, with 3,944 events and seven lifetime
members; Text/Files receipts, screenshot, saved logs and offline allocation pass.
Early exit remains incomplete; the three-phase AC regression verifies all three
screenshots. Exact staged GPT rehearsal refuses unsupported Q35 hardware,
saves/unmounts/returns; missing Home returns before probing.

The next unarmed run `eab752f59fb68762` uses the preserved 262-file/773-cluster
USB baseline and a new `attended-receipt.txt`. Keep AC/USB connected; open the
Files folder icon in the dock, browse Home, and wait for the five-minute Complete
prompt before saving and exiting through the normal menu. Both earlier trials
remain incomplete and backed up separately. The obsolete parked AC payload's
EFI copy was removed only after independent backup hashes matched; console
recovery and the newer accepted Home AC payload remain installed. Ordinary
battery startup remains off; physical acceptance of this correction is pending.

Final scope review retains main `bf1b9a6`. Air7,2 storage-service-gap work and
Pro PR19 `d80fb47` remain independent; no competing driver changes are included.

## Opt-in ordinary battery session scope (2026-10-24)

Base `468ca20`; bounded objective: admit this exact Air6,1 profile to the
existing explicit ordinary desktop owner, with no diagnostic session deadline.
Scope: Home ownership admission, distinct staging recipe, directed ownership/
staging tests and ordinary-session VM harness. Reuse inherited process lifetime,
mount hold, cached sampling, periodic logging and clean retirement. Keep default
startup off and firmware/EC/query permissions unchanged. Air7,2 PR130 owns window
capacity/compositor allocation; Pro PR19 owns Wi-Fi. Avoid their implementation
files. Next checkpoint is an Air6,1 VM session beyond eight minutes, refusal/
cleanup regressions and a separately staged unarmed physical session.

Run `eab752f59fb68762` on `2cd5b8604f2406c5` demonstrated the screenshot receipt
fix: the strict verifier accepts capture/save receipts and the 1366×768 BMP.
Text saved/reopened 17 bytes. The user left it running and observed the expected
halt; periodic logs reach 477 seconds, with no final exit/retirement proof or
separate Files activity. Prior 262 files were preserved except metadata; 267
files/811 clusters pass offline checks. Evidence is retained outside build.
The user explicitly chose to move on rather than repeat diagnostic coverage;
full attended-work acceptance remains incomplete, not promoted to passed.

### Ordinary session candidate validated (PR131)

Source `75ddde8`, v70 release `87bf9866baa3f8bc`, enables only the explicit
Air6,1 ordinary Home battery owner. Full build/core, 75 staging tests, exhaustive
owner combinations and receipt compatibility tests pass. The extended GPT VM
retained 535.342 seconds of live periodic-log coverage and 6,664 events, then
exited normally, drained ten lifetime members and reopened with a fresh owner.
Text/Files/shell bytes, all four logs and offline allocation pass. Submitted-save
cancellation, runtime fault retention, opt-out/missing-Home/platform refusals and
the Air7,2 ordinary-session regression pass. Exact staged GRUB→Limine GPT boot
refuses Q35 hardware while keeping apps usable, exits/reopens and reboots cleanly.

Keep the earlier failures: the initial long harness did not answer Terminal's
expected close confirmation; this automation is corrected. A subsequent short
VM startup hit an EC query timeout and safely halted with ownership retained.
An unchanged short retry and the full final suite passed. The isolated timeout
has not been attributed to native hardware; no deadline was widened to hide it.
Native ordinary-session acceptance and extended endurance remain pending.

Unarmed run `d34f847c1520e40c` preserves the 267-file/811-cluster baseline and
uses new `ordinary-battery.txt`. Keep USB connected, use apps normally beyond
eight minutes, save and select SrvrOS > Return to console when finished, then
enter `reboot` at `srv>`. There is no completion countdown or automatic return.
Console recovery remains `c35475f62f154f1e`; the superseded `78fe53e8cacdb1ba`
EFI copy was removed only after its independent backup matched. Corrected
capture and accepted recovery payloads remain available. Main is still `468ca20`;
Air7,2 PR130 and Pro PR19 `d0a0b57` retain independent scopes.

### Ordinary monitoring physical result (2026-10-24)

Run `d34f847c1520e40c`, release `87bf9866baa3f8bc`, session
`00065e9aafda5da4-00` returned with a clean exit snapshot at 797.659 seconds
from boot, beyond the former eight-minute deadline. Battery generation reached
304 with 4,553 events and no alarms. Runtime retired with zero heap, errors or
denied operations; the six-member peak lifetime drained and released. A new
nine-byte `test.txt` persisted; prior file bytes except workspace metadata are
unchanged. Offline fsck/allocation pass: 272 files, 817 clusters, no leaks or
bitmap mismatches. Ubuntu returned with an empty one-shot entry. Evidence and
hashes are backed up outside build.

This accepts opt-in monitoring beyond eight minutes with clean native retirement
and a persisted document, not multi-hour endurance or a reopen receipt absent
from retained logs. Default battery startup remains off. The reported 16-window
result belongs to another machine.

## Combined built-in services scope (2026-10-24)

Base `bc0edb9` includes ordinary Air6,1 admission and shared 16-window support.
Objective: a distinct opt-in Air6,1 ordinary session combining battery, panel
brightness, HDA audio and read-only CPU/SMC sensors, without USB networking or
the optional AML probe. Scope: staging recipes, combined-session VM harness and
this unit's current guides. Reuse existing exact device gates and lifetimes;
no driver permission changes. Pro Wi-Fi and Air7,2 desktop API work remain
independent. Next checkpoint: combined audio/absent-provider/exit/reopen and
refusal VM coverage, exact staged GPT rehearsal, then an unarmed attended test.

### Combined candidate validated (PR132)

Source `59c1836`, v71 release `1ae70fcf03ab6152`, combines the ordinary Air6,1
battery owner with brightness, HDA and read-only CPU/SMC providers; USB networking
and optional AML probing remain off. Full build/core, 76 staging tests and docs
checks pass. Seven GPT VM cases pass: audio/normal exit/reopen, pending save,
fault retention, opt-out, missing Home, absent providers and Air7,2 regression.
The normal Air6,1 run records 712 battery events and ten lifetime members; PCM
has no remaining owner/queue or underruns. Exact staged GRUB→Limine GPT rehearsal
passes with Q35 native-profile refusal, working HDA, apps, clean exit/reopen,
offline allocation and monitor reboot.

Two earlier Air7,2 runs timed out waiting for a missing serial close-request
receipt; the first saved screen shows Terminal's expected confirmation dialog.
Their evidence remains retained. The harness now confirms Close after a short
UI settling period and retains all completion, lifetime, runtime, logger and
file-integrity assertions; the full final matrix passes. No runtime deadlines
or hardware gates changed. Main advanced to `76a55dd` with documentation-only
architecture work; no tested binary changes are needed for that integration.

Unarmed run `2319b6df4b7f2dbd` preserves 272 files/817 clusters and reserves
`combined-services.txt`. Exercise brightness, Audio playback/volume/Stop, System
CPU/fan/battery freshness, and Text/Files for at least ten minutes; exit and
reopen with `desktop battery`, then exit and `reboot`. No timed completion or
automatic return. The obsolete `2cd5b8604f2406c5` EFI copy was removed only after
its durable payload/trial hashes matched; accepted ordinary battery, console
recovery and Ubuntu remain preserved. Combined native acceptance is pending.

### Combined session returned (2026-10-24)

Run `2319b6df4b7f2dbd`, release `1ae70fcf03ab6152`, produced one session
`00065e9bf077c2fc-00` with a clean exit snapshot at 1315.719 seconds from boot.
All four providers were enabled with networking off. Battery reached generation
500 over 500 attempts (one rejected sample), 7,492 events, two alarms and two
acknowledgements; generation advanced past invalid-through 227. Runtime retired
with zero heap/errors/denied and 531 handled/zero unclaimed SCI deliveries. The
12-member peak lifetime drained and released. This does not identify the early
rejected sample's cause: retained rotating logs do not cover the whole session.

Final sensor snapshots show 1,312 fan samples with zero failures and 1,315 thermal
samples. Audio records four starts/three completions, volume 68, no final error;
PCM has zero owner/queue/underruns. The user confirms everything worked normally, including sound and brightness.
The fourth audio start has no completion count; its exact stopping action is
not separately recorded.
A new 11-byte `ten-minute-demo.txt` persisted; prior bytes except metadata remain
unchanged. Offline fsck/allocation pass at 277 files/823 clusters with no leaks
or bitmap mismatches. Ubuntu returned with empty one-shot state; evidence and
hashes are backed up outside build. No reboot is armed.

The user confirms that desktop close/reopen was not attempted. This accepts
the observed combined session with battery, brightness, audio and sensors,
file persistence and clean exit. Combined-provider desktop reopen remains
untested on this machine; multi-hour endurance is also outside this result.

## Combined close/reopen follow-up scope (2026-10-24)

Integration baseline `e79e220`; retain accepted v71 release `1ae70fcf03ab6152`
(compiled source `59c1836`) for a focused two-session native check. No runtime or
staging changes: rerun combined HDA/battery exit/reopen VM and exact staged GPT
rehearsal, then use a fresh run record and USB baseline. Publish the preceding
combined-session acceptance and this scope. PR134 POSIX work and PR19 Wi-Fi
remain independent; neither is imported into this accepted payload. Check file
persistence, provider availability, battery lifetime retirement and normal exit
on both sides of `desktop battery`. Multi-hour endurance is not this test.

### Reopen trial ready (PR135)

Fresh rehearsal source `ef184f3` differs from compiled `59c1836` only in
documentation. The unchanged combined HDA/battery GPT VM passes normal
exit/reopen with fresh lifetime, exact saved files, retired PCM and clean
allocation. Exact installed v71 release `1ae70fcf03ab6152` separately passes
GRUB→Limine GPT boot, Q35 native-profile refusal, working HDA, normal exit/reopen,
file/allocation checks and monitor reboot. Documentation checks pass.

Unarmed run `c655eca6a4f00313` uses the new 277-file/823-cluster baseline and
`reopen-services.txt`. Briefly exercise brightness, audio, sensors and battery;
save the document and return to console. Enter `desktop battery`, reopen the
document and exercise the providers again. Exit normally, then `reboot`. No
ten-minute wait or automatic return is required. The preceding combined native
run remains independently backed up; this new run has its own backup directory.
Ubuntu and console recovery stay unchanged. Physical reopen acceptance awaits
the returned evidence and user observations.

### Reopen trial failed (2026-10-24)

Run `c655eca6a4f00313` on unchanged v71 `1ae70fcf03ab6152` completed its first
session `00065e9c895ac1d7-00` with clean retirement at 87.970 seconds from boot.
The user confirms all providers worked and the document was saved. Brightness
16/44/78 and one completed audio test are logged, with live CPU/fan data and
no final audio/SMC errors. New `reopn-services.txt` persisted (four bytes).

After `desktop battery`, session `00065e9c8f025226-00` saved its startup log,
but halted before a periodic/final snapshot. The user's photograph reports
`HALTED operation deadline`, setup `complete`, elapsed 5,037,205 us, zero
errors/denied/EC error/queries and retained live state. The source gives namespace
setup a 5,000 ms operation budget. Completion timing and deadline handling need
investigation; this evidence does not yet identify a driver or scheduler defect.
Do not widen the budget or claim native reopen acceptance from VM passes.

All 277 prior file bytes except metadata are preserved. Offline fsck/allocation
pass at 283 files/831 clusters with no leaks or mismatches. Ubuntu returned with
empty one-shot state. Both sessions, the photograph and file hashes are backed
up independently outside build. No reboot is armed. First-session combined
acceptance remains valid; native combined reopen is now a recorded failure.

## Reopen deadline investigation scope (2026-10-24)

Base `36080fc`. Investigate the stale worker-drain result between a timed slice
and the outer battery operation deadline. The native photograph reports setup
complete and no in-flight work at containment; reproduce that boundary using
VM-only second-lifetime scheduling hooks before changing production behavior.
Scope: battery operation loop, disposable battery VM model and desktop regression
harness, plus this record. Preserve the five-second setup budget and live-state
containment for still-running work. PR134 owns POSIX/scheduler changes; avoid
scheduler edits. PR19 Wi-Fi is independent. Next checkpoint: failing regression,
completion-vs-busy proof, combined exit/reopen and exact staged GPT rehearsal.

### Deadline completion fix validated (PR136)

Source `faacdf2` adds a nonblocking worker completion poll at operation expiry,
retaining the five-second setup budget, callback error checks and busy-work
containment. A VM-only second-lifetime fixture reproduces the old failure:
setup complete at 5.364 seconds, no work in flight, zero EC errors/queries, then
operation-deadline HALT. With the fix it drains both lifetimes cleanly. A separate
six-second setup fixture still halts and retains the second lifetime. This
proves the stale-observation defect and is consistent with the native photo;
native confirmation is pending rather than inferred from the model.

Full build/core, 76 staging tests, documentation checks and nine combined GPT
VM cases pass: both deadline regressions, normal playback/exit/reopen, pending
save, runtime fault retention, opt-out, missing Home, absent providers and Air7,2
regression. Exact staged v71 release `06b0976a58151901` passes GRUB→Limine GPT
boot with native-profile refusal, HDA, app/file work, exit/reopen, clean allocation
and monitor reboot. No scheduler or firmware permission changes. PR134 POSIX
and PR19 Wi-Fi remain independent; main is `36080fc`.

Unarmed run `227c377cefd26812` retains 283 files/831 clusters and uses new
`deadline-reopen.txt`. Briefly check providers and save, return to console,
enter `desktop battery`, reopen the document and check providers, then exit
and reboot. No ten-minute wait or automatic return. Prior native failure and
before-fix VM evidence remain backed up separately. The old `a18d72aecdf808c7`
three-minute capture EFI copy was removed only after durable payload verification;
combined, ordinary-battery, console recovery and Ubuntu remain available.

### Deadline retry failed on native reopen (2026-10-24)

Run `227c377cefd26812`, release `06b0976a58151901` (PR136), again halted
on its second desktop lifetime. First session `00065e9cec2debcc-00` completed
cleanly at 234.640 seconds from boot: 87 battery attempts, 1,308 queries, balanced
EC attachment, zero final heap/errors/denied, working brightness/audio and live
CPU/fan samples. First setup took 4,766,350 us, including namespace load through
the contract marker at 4,326,472 us and namespace initialization beginning at
4,425,243 us.

Second session `00065e9cfa6ff1bc-00` saved only its startup log. The photograph
shows `HALTED operation deadline`, `setup=namespace-initialize`, elapsed
5,047,305 us, `completed=0 in_flight=1`, zero errors/denied/EC error/queries and
retained live state. Unlike the preceding photo, work was still active: PR136's
completion repoll does not resolve this native startup overrun. Its synthetic
completion-race regression remains valid but does not establish native repair.
Next investigation should locate namespace/AML execution and wait time before
changing the five-second bound; the current evidence does not identify a hang
or distinguish execution cost from scheduling and firmware waits.

All 283 preexisting files except metadata retain identical bytes. Five new log
files were collected; no new document appeared. Offline checks pass at 288 files/
838 clusters, with no leaks or mismatches. Ubuntu recovered with empty one-shot
state. Logs, photograph and file hashes are backed up outside build. No reboot
is armed; native combined reopen remains failed.

## Startup timing scope (2026-10-24)

Base main `85d5939`, preserving the failed retry evidence commit. Add bounded
per-stage startup accounting for AML sleep/stall/semaphore, allocation/free and
EC work, including active waits at containment. No firmware gate, scheduler or
five-second deadline changes. Add an explicit automated two-desktop-session
recipe with normal lifetime/logger teardown and Ubuntu return on completed
runs. Fatal mounted-Home containment must still halt without flushing/resetting
live storage; a photograph/manual recovery may remain necessary on failure.
PR134 (`d9876eb`) owns POSIX/scheduler and PR19 (`7873b63`) owns Wi-Fi; neither
scope is imported. Validate timing accounting, completed/busy startup regressions,
automated success/refusal/failure and exact staged GPT boot before installation.

### Automated startup diagnostic ready (PR137)

Final runtime source `582cc59`, v72 release `837ce4f6bfab6775`, records fixed-size
setup-worker sleep/stall/semaphore/EC wall times and sampled allocation/free
cost, including active waits at containment. Existing phase markers remain.
The five-second deadline and hardware permissions are unchanged. The explicit
reopen diagnostic runs two 20-second app lifetimes with fresh battery samples,
normal retirement and log saving; it rejects early manual exit. Ordinary desktop
sessions retain no timer. Fatal mounted ownership still halts without storage
writes/reset; normal diagnostic completion unmounts and returns to Ubuntu.

Full build/core, 77 staging tests and docs checks pass. On `4b41c06`, ordinary
Air6,1 save/audio/reopen, pending-save and runtime-fault cases and the Air7,2
combined regression pass. The final change only makes early diagnostic exit
incomplete; full core and all five automatic cases pass on `582cc59`: early exit,
normal, completed-at-expiry, still-busy containment and profile refusal. Exact
staged GRUB→Limine GPT boot passes two refused-but-cleanly-retired Q35 sessions,
unchanged fixture files, clean allocation and automatic recovery. Refusal is
expected VM coverage, not native acceptance. Main remains `85d5939`; PR134
`e4b7b02` and PR19 `67575f0` retain separate POSIX and Wi-Fi scopes. Shared staging/
boot files need both independent profiles preserved in later integration.

Run `d3d219186d3bc6e6` retains the 288-file/838-cluster USB baseline and requires
no human input or document edits. A machine-local one-shot Ubuntu service collects
matching sessions, requires both fresh-sample/retirement receipts, verifies prior
file hashes and offline allocation, backs up evidence and disables itself after
recovery. A halt still requires a photograph/manual restart. The user explicitly
authorizes automated reboot without another permission gate. Installed payload
hashes and Ubuntu default are verified; console recovery `c35475f62f154f1e` remains.
The obsolete `fc1d1f4c45d30252` EFI image-capture copy was removed for space only
after its durable payload backup was hash-verified. Native reopen remains failed
pending this diagnostic's returned evidence; this is not a claimed native fix.

### Automated startup timing result (2026-10-24)

Run `d3d219186d3bc6e6`, release `837ce4f6bfab6775`, passes the automated
two-session check. Sessions `00065e9d64761993-00` and `00065e9d65e8d7a2-00`
each reached fresh battery generation 6 and retired epochs 1/2 with balanced
attachment and zero final heap/errors/denied. Normal Ubuntu return and the
one-shot collector completed; the collector disabled itself. All 288 prior
files except metadata retain identical bytes; eight new logs bring the clean
filesystem to 296 files/848 clusters. One-shot boot state is empty. Evidence
and hashes are backed up outside build; no new reboot is armed.

First startup took 4,796,986 us; reopening took 4,917,553 us, only 82,447 us below
the unchanged five-second deadline. Namespace load alone took 4,455,910 us in
the second lifetime. Neither startup called AML sleep or stall. Second-start
semaphore wall time totaled 10,906 us and four EC byte transfers totaled 312 us.
Allocation made 18,847 calls, with 295 sampled calls totaling 61,586 us; free
made 7,411 calls, with 116 samples totaling 14,180 us. These sampled wall times
are not exact totals or CPU measurements. The allocator's whole-heap validation
on both entry and exit, plus its linear free-block search, is a strong next
investigation target; establish cost and preserve integrity checks in any fix.

This accepts only the short automated startup/reopen/retirement sequence. It
does not resolve the prior attended failure or establish sufficient timing
margin under interactive load. No timeout increase or allocator change was
made in this run.

## Allocator startup investigation scope (2026-10-24)

Base `4f8acd2`, retaining the collected startup result. Profile/replay the real
allocator and remove redundant allocation traversal while preserving full
before/after heap validation, free checks and the five-second battery deadline.
Scope: kernel heap allocation traversal, deterministic host integrity/performance
regressions and this machine's v72 automated startup trial. Avoid scheduler,
firmware and EC changes. PR134 `e4b7b02` has no heap implementation changes;
PR19 `807fb51` remains independent Wi-Fi scope. Publish measured old/new work
counts and reject corrupt heaps even when a suitable early free block exists.
Native timing improvement remains pending the next automated trial.

### Heap traversal trial ready (PR138)

Runtime source `e7b29e7`, v72 release `4efd82574f07eb30`, selects the first
fitting free block during the existing full entry validation walk. It finishes
that entire walk before mutation and retains the complete exit walk and both
free walks. First-fit, splitting/coalescing, metadata, firmware permissions and
the five-second startup deadline remain unchanged. Later corrupt blocks must
still halt even when an earlier suitable free block exists.

Three sequential alternating baseline/candidate replay pairs (two lifetimes
each, 18,847 allocations and 7,411 frees during each setup) reduce median host
setup time from 5.707 s to 4.856 s, about 14.9%. Cleanup/free time is similar
(1.272 s versus 1.294 s). Full validation visits and payload checksums match
exactly; about 105.45 million redundant search visits disappear per lifetime.
This is a deterministic namespace-sized workload, not a captured AML trace or
native timing prediction. Native timing margin remains to be measured.

On clean `e7b29e7`, full build/core, general smoke (including 481 CLI commands),
77 staging tests and docs checks pass. Nine battery VM cases pass: automatic
early exit, normal, completed-at-expiry, busy containment and profile refusal;
ordinary combined desktop save/reopen, pending-save cancellation, runtime fault;
and the Air7,2 combined regression. Exact staged GPT GRUB→Limine boot passes
two refused-but-cleanly-retired sessions, clean allocation and automatic return.
Q35 profile refusal is expected and is not native acceptance. Refreshed main
is `4f8acd2`; PR134 `d5d0fa1` still has no heap implementation change and PR19
`d5010dc` remains independent Wi-Fi scope.

Run `c741adb1f4bd0f75` uses this unit's USB `6ADA-85BB`, preserving all 296
files/848 clusters. The automated trial opens two 20-second desktop lifetimes,
requires fresh samples and clean retirement, then returns to Ubuntu. The one-shot
collector checks saved sessions, startup timings, prior hashes and offline
allocation, backs up evidence outside build and disables itself. No human input
is required. User authorization permits this automated reboot without asking;
fatal mounted ownership still halts and requires a photograph/manual recovery.
Ubuntu remains default and console recovery `c35475f62f154f1e` is retained.
The obsolete failed `06b0976a58151901` EFI copy is retired for space only after
its durable payload backup is hash-verified. Prior accepted payloads and the
previous automated diagnostic remain preserved. Native improvement is pending
returned evidence; this does not yet resolve the attended intermittent failure.

### Automated heap trial result (2026-10-24)

Run `c741adb1f4bd0f75`, release `4efd82574f07eb30`, runtime source `e7b29e7`
(merged through PR138, `27f0870`) passes both automated native desktop lifetimes.
Sessions `00065e9dea272465-00` and `00065e9deb7d9404-00` completed startup in
3,604,290 and 3,618,725 us, respectively, versus 4,796,986 and 4,917,553 us in
the preceding diagnostic. This run is 24.86%/26.41% faster and leaves at least
1,381,275 us below the unchanged five-second startup deadline. This is an
observed paired-run improvement, not a worst-case bound under interactive load.

Fresh battery generations 7/6 and epochs 1/2 each retired with balanced attach/
detach, zero final heap/errors/denied and released desktop lifetimes. Queries
were 105/90, with one notification in each lifetime. All 296 prior files except
mutable `.srvros/meta` retain identical contents; eight new session logs bring
the clean filesystem to 304 files/858 clusters, with no leaks or FAT/bitmap
mismatches. Automatic Ubuntu return and collection completed; all 303 durable
trial backup files were hash-verified. The collector is disabled and GRUB's
one-shot entry is empty; no reboot remains armed.

This accepts the short automated startup/reopen/retirement trial with the heap
optimization and preserved full integrity checks. It does not yet accept the
previously failing attended reopen under Text/file/audio work or extended
endurance. Next useful coverage is an attended combined desktop session followed
by close/reopen and resumed Text/file work, retaining the same startup deadline.

## Attended heap-fix reopen scope (2026-10-24)

Base main `27f0870`, preserving collected PR138 physical evidence. Prepare the
existing untimed v71 combined desktop recipe with the validated heap fix; no
runtime, firmware, deadline or shared subsystem change is planned. Rehearse
Text/Files/audio and normal close/reopen plus the exact staged GPT boot path.
The attended run must save a new document, browse it in the separate Files app,
close the desktop normally, reopen with `desktop battery`, and resume file work.
PR134 `d5d0fa1` owns POSIX and PR19 `b02be44` owns Wi-Fi; neither is imported.
The previous automated run is collected, backed up and unarmed. Preserve Ubuntu
and console recovery; report ready without starting this attended run.

### Attended reopen ready (PR139)

Source `5d858df`, v71 release `9851d427b042227b`, run `f4f57726a0d1824d` uses
the exact native kernel/initramfs bytes accepted in the prior automated heap
trial, with the existing untimed combined-session configuration. Full build/core,
77 staging tests and docs checks pass. The combined Text/Files/audio normal VM
and exact staged GPT GRUB→Limine profile-refusal VM pass, including clean
close/reopen, provider/PCM teardown, offline allocation and monitor reboot.
Q35's expected native-profile refusal is not physical acceptance. The previous
full smoke and nine-case battery matrix remain evidence for the unchanged code.

The attended checklist is in the current battery guide: ten minutes with a new
`heap-reopen.txt`, separate Files dock app and battery/brightness/audio/sensor
checks; Return to console, `desktop battery`, then reopen/append/save/reopen the
document and repeat checks for at least two minutes. Save, exit, unmount and
reboot manually. There is no timer or completion prompt. USB baseline is 304
files/858 clusters, unchanged and clean. Ubuntu default and console recovery
`c35475f62f154f1e` remain verified. Superseded startup diagnostic `837ce4f6bfab6775`
is removed from EFI only after its durable payload backup is hash-verified;
the latest accepted automatic release `4efd82574f07eb30` remains available.
Payload and trial evidence are backed up outside build. This run is ready but
unarmed; attended reopen acceptance remains pending its return.

### Attended combined reopen accepted (2026-10-25)

Run `f4f57726a0d1824d`, v71 release `9851d427b042227b`, returned with four
attended desktop lifetimes and three successful reopens. User reports running
all apps successfully. Startup took 3,600,315 / 3,791,923 / 3,732,066 / 3,753,837
us, leaving at least 1.208 seconds below the unchanged five-second deadline.
Epochs/tokens 1–4 all retired and released with balanced attachment and zero
final runtime heap/errors/denied. Session durations were about 219, 35, 201 and
179 seconds: roughly 10.6 minutes of desktop time overall, not the originally
planned ten-minute first session or an extended endurance test.

Text save/read succeeded in the first lifetime, read/append/save in the second,
and reads of the resulting 30-byte document in the third/fourth. Separate Files
activity is present in all four lifetimes. Audio finished two plays with no
pending work, and read-only thermal/fan samples continued across reopens. The
user's observation supplies the perceptual app/provider result. The generic
collector's release filter identified only the first two sessions because later
ring-buffer tails no longer contained the release banner; all four newly created
session directories were assessed directly with sequential lifetime tokens,
startup, final battery and release receipts rather than inherited exit text.

No prior files are missing. Only `.srvros/meta` and desktop preferences changed
among 304 prior files; new artifacts are 16 session logs, the Text document and
a drawing. Offline fsck/allocation are clean at 322 files/887 clusters with no
leaks or FAT/bitmap mismatches. Evidence and hashes are backed up outside build.
Ubuntu one-shot state is empty and no reboot remains armed. This accepts attended
combined desktop close/reopen with Text/Files work after the heap fix. Longer
endurance, broad input/load coverage and other unimplemented device capabilities
remain separate milestones; no integrity checks or startup budget were weakened.

## Longer combined session scope (2026-10-25)

Base main `3647dc7`, preserving the attended four-lifetime acceptance. Prepare a
30-minute paced ordinary-shell workload for USB copy/readback and repeated audio,
with a fresh-run guard, durable round/completion evidence and automatic Ubuntu
collection after manual desktop exit. Text editing and audible playback remain
attended observations. Scope is new workload/VM tests, this machine's record and
current guide; retain the existing untimed v71 profile. No kernel, firmware,
battery budget or desktop startup changes. PR153 owns capacity automation and
shared staging/battery harness changes; avoid those files. PR19 remains Wi-Fi.
Validate the newly merged scheduler/desktop baseline, keep accepted payloads and
console recovery, and leave the next physical run ready-unarmed.

The refreshed full CLI smoke exposed one stale pre-POSIX diagnostic expectation:
`cd /fat/status.txt` correctly refuses and leaves cwd unchanged, but the newer
shell prints `cannot change directory` rather than `not a directory`. Update
only that expected text and add explicit status=1/unchanged-cwd evidence; do not
alter runtime behavior. Rerun the full suite after this harness correction.

### Long combined trial ready (PR154)

Source `78859c4`, v71 release `3f0896783e4c8fe7`, run `db3ea7d68555f544`
consumes main `3647dc7` (new scheduler/desktop baseline) and retains the unchanged
native Air6,1 battery/firmware gates and five-second startup budget. The ordinary
Terminal workload performs thirty 256-KiB copy/sync/rename/readback rounds, each
with a bundled six-second PCM play and sixty-second pacing before the receipt.
It requires at least thirty minutes of pacing (roughly 33 minutes total), refuses
reruns and saves per-play and round/completion records. A nonzero audio/file
command aborts the job; the collector also rejects missing audio completion or
underruns. No forced desktop exit is added. Text/Files use and listening remain
attended, with a fresh run-specific document and checks near start/middle/end.

Full build/core passes on `44b5706`; subsequent `78859c4` changes only the CLI
harness expectation and scope documentation. Full smoke including 482 CLI
commands passes after correcting the stale diagnostic and checking failed-cd
status/unchanged cwd. The new workload host tests, 78 staging tests, new real-shell
GPT battery/audio/Text/Files workload VM, ordinary combined reopen VM and exact
staged GPT GRUB→Limine boot all pass. Workload VM checks exact outputs, zero-
underrun audio completion, rerun refusal, clean lifetime retirement and offline
allocation. Exact native-stage Q35 refusal is expected; it covers safe unavailable
providers, PCM, saves, reopen, unmount and reboot, not native acceptance.

USB preparation preserved all 322 existing files and added only two fresh workload
inputs; preflight is clean at 324 files/890 clusters. Ubuntu default and console
recovery `c35475f62f154f1e` remain verified. Superseded pre-heap combined release
`1ae70fcf03ab6152` is retired from EFI only after its durable backup is verified;
accepted attended `9851d427b042227b` and automated `4efd82574f07eb30` remain.
The one-shot Ubuntu collector requires workload completion, a completed desktop
lifetime of at least thirty minutes, clean battery retirement, the new Text
file, prior-file preservation (apart from metadata/preferences) and offline
allocation, then backs up evidence and disables itself. Human audio/input
confirmation remains distinct. Run and payload backups survive distclean.
Ready-unarmed: no reboot is scheduled; longer native acceptance remains pending.

## Audio/USB overlap correction scope (2026-10-25)

The prepared long-session script serialized copy/readback before playback; it
did not establish audio/disk overlap. Supersede that unarmed workload with a
fresh run: one-minute PCM plays in the background while bounded paced copies,
flushes and comparisons execute in the foreground, requiring the player to remain
live at both ends of each recorded burst. Retain audio completions/underruns and
per-burst evidence. The disposable VM must observe PCM output advancing during
USB writes. Keep the already validated native payload `3f0896783e4c8fe7` and its
runtime source `78859c4`; only Python workload/tests and documents change. Actual
main is `6e06bfb`; separate capacity work from PR153/155 is not introduced into
this immutable payload. PR19 remains independent Wi-Fi work. Prior run is
ready-unarmed, not physically executed; replace its collector and launch record
only after preserving its inputs/evidence. Leave the corrected test unarmed.

### Audio/USB overlap ready (PR156)

Corrected workload source `ee96e1d`, run `93a364b8128b185a`, replaces unexecuted
run `db3ea7d68555f544`. Installed native payload remains `3f0896783e4c8fe7`, source
`78859c4`, hash-identical to its already validated build and exact-stage rehearsal.
Thirty one-minute plays run in the background, each with forty foreground
256-KiB copy/flush/rename/readback bursts paced one second apart. Playing-receipt,
before/after PID liveness, absence of an early completion, explicit burst totals,
wait status and retained audio completion/zero-underrun evidence guard the trial.
Total copy/readback is 300 MiB during thirty minutes of PCM, about 31 minutes
elapsed including gaps. Text/Files and listening remain attended; automatic
Ubuntu collection verifies all 1,200 bursts and thirty complete plays.

Host verifier tests pass. On clean `ee96e1d`, the repeated short GPT VM passes
four bursts/two plays with 12 observed non-silent PCM/USB-write overlap windows.
The full native-parameter round passes all forty bursts during one complete
60-second play, with 90 observed overlap windows. Both verify readback, no
underruns, concurrent Text/Files, rerun refusal, clean battery retirement and
filesystem allocation. Development tests caught quoted numeric wait parsing,
a too-long for word list, and command substitution using read-only /fat; the
script now uses numeric wait, eight-item groups and explicit writable count
files. One short rerun hit a Q35 battery sample/EC fault; its evidence is retained.
An unchanged retry and the full-minute trial passed. This does not establish that
fixture fault's cause or native endurance acceptance.

USB inputs now comprise a fresh script, 256-KiB seed and one-minute WAV. All 324
preexisting files (including the unused prior trial inputs) remain preserved;
preflight is clean at 327 files/981 clusters. The previous run is superseded and
unarmed, and its collector disabled. The overlap collector replaces it. Ubuntu
default, console recovery and accepted payloads remain intact. Payload and run
evidence are backed up outside build. The corrected test is ready-unarmed; no
reboot is scheduled, and listening/native overlap acceptance remains pending.

## Automatic audio / filesystem stress scope (2026-10-25)

Run `93a364b8128b185a` returned with clean desktop retirement but failed the
workload: one 60-second PCM completion with zero underruns and 32 of 40
recorded disk bursts. The next burst crossed playback completion; the fixed
count/pacing underestimated native filesystem overhead. Manual Text saves do not accept sustained overlap. Preserve this failure.
The replacement is fully automatic, with listening as the only human task:
a dedicated PCM producer and separate continuous verified filesystem worker,
automatic desktop startup/progress/exit, normal battery retirement and Ubuntu
return. Record actual timestamps and counts rather than infer overlap from a
shell reading a concurrently written log. Use existing PCM/filesystem interfaces;
no firmware gates, integrity checks or startup budgets change.

Base is merged main `2c1ffc6b`. Scope: a playwav stress mode, narrow boot-only
session selection, desktop progress/worker lifecycle, staging and VM regressions.
PR157 owns shared document-dialog/app lifecycle work; preserve it and keep these
additions isolated from its client/dialog code. PR19 remains independent Wi-Fi.
The prepared native payload will be rebuilt and exact-stage tested. User has
explicitly authorized the automated reboot after validation.

### Automatic audio / filesystem stress prepared (PR158)

Native source `1c2bba2f`, v74 release `a70f9540f3aa39dd`, run
`6e0147a2ac774722` uses a continuous thirty-minute PCM producer and a separate
unpaced 256-KiB create/flush/rename/reopen/compare/delete/sync worker. A pipe starts
file activity only after PCM reports live playback. Timestamp/byte/count receipts
reject late, absent or insufficient-duration I/O, incomplete PCM and underruns.
Boot waits for fresh battery telemetry, starts the workload, shows elapsed
progress and retires the desktop/battery lifetime before unmount and automatic
Ubuntu return. No human interaction is required beyond listening. Scratch space
is bounded; all preexisting Home files remain outside the private test directory.

Full build/core and full smoke pass, including 482 CLI commands, shell editing,
UEFI USB input and displayd. The new receipt regression and 80 staging tests pass.
The automatic 60-second Q35 run on clean `1c2bba2f` completes 363 verified cycles
(95,158,272 bytes) with zero underruns and 224 independently observed non-silent
PCM/USB-write overlap windows. It retires cleanly and returns without injected
keyboard or mouse input; offline allocation is clean. Exact staged GRUB→Limine
rehearsal on `95d522c2` (harness-only follow-up) confirms native profile refusal,
no stress startup on unsupported Q35, unmount and automatic return. Its first
harness configuration lost the reference clock with synthetic ACPI under UEFI;
that failure is preserved, and the successful rehearsal uses the established
native UEFI fixture without injected AML. Native source/payload did not change.

USB baseline is 335 files / 992 clusters, no leaks or bitmap mismatches; no new
USB inputs are needed. Ubuntu and console recovery `c35475f62f154f1e` are retained.
The obsolete EFI-only copy `87bf9866baa3f8bc` is retired after exact durable backup
verification to make installation space; accepted evidence/payload backups and
the latest attended/automatic recovery payloads remain available. The new Ubuntu
collector requires the matching workload receipt, clean desktop/battery retirement,
prior-file preservation and offline allocation; listening remains separate.
User explicitly authorized reboot and automatic startup after validation. Save
armed state and durable run/payload copies before launch. Native endurance is
pending this run, not accepted by VM success.

### Automatic stress partial result and desktop latency failure (2026-10-25)

Evidence-review scope only: collect run `6e0147a2ac774722`, document audio/UI and
retirement outcomes, and identify the next bounded investigation. Integration
baseline is `afb2c3b0`; open PR157 owns shared desktop document/application
lifecycle changes and must be considered before implementing launch/close changes.
No new payload or reboot is prepared by this review.

Release `a70f9540f3aa39dd`, session `00065eafdcd77b12-00`, was intentionally
exited early. The user reports persistent, stable sound but multi-second app
open/close delays and a 3–4-second UI freeze after a dock click while audio kept
playing. Final PCM position corresponds to 1,379.66 seconds (about 23 minutes),
with zero underruns; cancellation releases the PCM owner and queue. Battery
retires with 536 attempts, 8,122 queries, 98 notifications, zero heap/errors/denied
operations and a released lifetime. This supports audio continuity for the
observed interval, not a completed thirty-minute endurance or responsiveness pass.

The automatic collector correctly reports incomplete: cancellation left no final
disk/result receipt. There is one residual private payload; no verified total
burst count/throughput can be recovered from it. Existing files are preserved
except `.srvros/meta`. Offline checks pass at 340 files / 1,000 clusters with zero
leaks/bitmap mismatches; Ubuntu's one-shot entry is empty. Full raw evidence and
the user report are retained outside build.

Close-request to process-exit intervals visible in the retained log include
3.23, 4.60 and 4.65 seconds (also 1.28 and 1.70 seconds). These include client
cleanup and are not direct measurements of the dock-to-render freeze. System
and one Files process also exit with failure status; their cause is unresolved.
The overall workload/desktop FAIL markers remain failures/incomplete evidence,
not a successful completed trial; normal battery retirement is distinct.

Code inspection identifies synchronous `srv_exec` from `deskd`'s event loop and
storage-gated process/lifetime operations as a plausible freeze path. PCM calls
remain admitted while storage is owned, consistent with stable audio. No timing
in this capture separates gate contention, executable loading, cleanup or render
stall, so root cause is not yet proven. Next: automate app launch/close during the
same unpaced workload, measure event-loop gaps plus launch/close phases and I/O
gate wait/hold time, then move blocking lifecycle work off the event loop or fix
measured contention as appropriate. Preserve integrity and ownership checks and
keep full disk pressure; do not hide the failure by adding workload sleeps.

### Active scope: responsive desktop during automatic audio/I/O (2026-10-25)

Base `3ab4a385`, branch `device/macbookair6-1/desktop-io-latency`. Reproduce and
measure event-loop stalls with automatic real app open/close during unchanged
unpaced verified disk work and PCM playback; move blocking compositor lifecycle
operations off its input/render loop where measurements justify it. Expected
scope: `userspace/deskd`, a bounded userspace lifecycle worker, regression and
VM harness, native trial records. Preserve filesystem integrity and kernel I/O
ownership gates. Air7,2 branch `dfba5295` owns process/memory lifecycle latency;
PR157 also changes document routing, so avoid competing kernel/process edits and
keep compositor integration explicit. Pro PR19 is separate Wi-Fi work.
User authorizes tests, merge and automated native reboot. Prior run is collected;
no new reboot is armed until matched builds, VM checks and recovery preflight pass.


Candidate reproduction: a fully automatic Q35 Text/Files/System lifecycle loop
under unchanged PCM/disk stress recorded a maximum compositor loop gap of
863,447 us; a second instrumented run showed cache allocation waiting 532,954 us
and free waiting hundreds of milliseconds. The initial worker candidate reduced
the maximum loop gap to 179,769 us across 21 cycles, with zero app errors,
zero PCM underruns and 218 independently observed PCM/write overlap windows.
These are development VM measurements, not native acceptance. Final clean-source
regressions and exact-stage validation precede installation and reboot.


### Responsive lifecycle stress candidate prepared (PR160)

Native source `ae5b8680`, release `4e8c2c95b707acbf`, run `97f910f8a1024bbe`
retains the thirty-minute unpaced 256 KiB write/flush/rename/readback/delete/sync
work and PCM stream, and automatically cycles Text, Files and System throughout.
The compositor uses a persistent worker for launch and pixel-cache map/unmap;
no kernel I/O eligibility or integrity checks change. Pending/discarded mappings
retain budget until release completes. Resize failure preserves the last delivered
geometry/front, and memory pressure waits for reclaimable buffers without early
byte credit. Worker metadata covers the existing cache budget; child admission
and early OPEN delivery are explicitly tracked. Ordinary screenshot initiation,
registry growth and forced termination are separate synchronous paths.

Final clean-source core/build and strict automatic VM pass on `ae5b8680`: 23 app
cycles, maximum loop gap 139,364 us, ready 1,303,625 us and close 924,975 us, zero
app errors and zero PCM underruns, with 221 independently observed PCM/write
overlap windows. Both lifecycle and I/O timestamps fall within live playback.
The full smoke suite passed on `43d9e1ef` (482 CLI cases, shell editing, UEFI USB
and displayd); later changes refine memory-budget admission and validation.
All twelve apps pass maximize/restore/close. Multiple-document close/relaunch
passes with explicit completed cache-release checks. The 128-window growth
fixture on `efd6bb1d` passes 129 retirements, restored process baseline and zero
retained registry/cache bytes. Host tests cover delayed allocation, discarded
pending allocation, copied launch inputs, child reservation, early OPEN messages,
launch failure, resize failure and full-budget delayed reclamation. Eighty staging
tests and documentation checks pass. A superseded multiwindow checker failure is
retained: it expected premature cache credit at window removal; the replacement
requires zero bytes after completed unmaps and rejects missing/nonzero receipts.

Exact native release passes GRUB→Limine rehearsal, expected Q35 native-profile
refusal, no unsupported stress startup, clean unmount and automatic return. This
is not native responsiveness acceptance. Physical validation requires the saved
PCM/disk receipt plus at least six completed app cycles spanning 1,780 seconds,
zero app errors, loop gaps no greater than 500 ms, clean battery retirement,
prior-file preservation and offline allocation. Listening remains the only human
task; keep AC/USB attached and allow automatic return after thirty minutes.

USB baseline is 340 files / 1,000 clusters, zero leaks/bitmap mismatches. Ubuntu,
console recovery `c35475f62f154f1e` and the latest accepted desktop/reopen payloads
are retained. Obsolete EFI-only `3f0896783e4c8fe7` was removed after verifying its
independent durable payload. Raw results, source/stage and run records are backed
up outside build before reboot. User explicitly authorizes this automatic launch.
Air7,2's `2eb23c11` process/graphics service-window work remains separate and
unmerged; this candidate neither consumes its v15 protocol nor changes its gates.


Integration update: PR157 merged as `c4bee5cd` before PR160 integration. The
pre-merge `4e8c2c95b707acbf` stage is superseded and was never installed or armed.
Integrate the matched v15 document/modal lifecycle and kernel service-window
baseline, retaining both the async worker and existing-process document routing,
child modal retirement, Air7,2 v75 guide and Air6,1 v74 automatic stress. Repeat
core/build, affected lifecycle VMs, automatic audio/I/O and exact-stage checks
before preparing the replacement native payload. Recovery/USB baseline unchanged.

Integrated lifecycle testing exposed a modal startup race: OPEN_REPLY marked the
dialog ready before CONFIGURE produced its canvas. Delayed compositor allocation
therefore laid out the picker at zero size, moving its initial filename focus to
Up. Extend the shared dialog helper's readiness condition to require the first
configured generation. The new host regression fails on the previous helper and
passes with this correction; retain the failing VM screenshot/log and rerun the
normal/crash/refusal lifecycle cases before native staging. This bounded library
change preserves window admission and buffer integrity checks.

Consume PR161 (`44c70a12`), which adds host routing regressions without changing
production binaries. The corrected normal lifecycle VM passes. The crash variant
initially failed its startup count because the new compositor launch timing line
split the single Text startup message. Extend the existing trace-interleaving
decoder to that line type, preserving exact startup counts, duplicate detection,
failure markers and raw evidence; rerun crash/refusal cases. No timeout or file
acceptance predicate is relaxed.

Final integrated candidate: source `7ddf2153`, native release
`8dd177cef12cec3b`. Build/core and desktop/routing host checks pass; the normal,
crash and refused-dialog VMs pass exact file contents, offline allocation and
restored process baseline. The automatic stress VM passes 19 app cycles with
maximum loop gap 438,388 us, ready time 1,212,207 us and close time 1,392,331 us,
zero app errors/PCM underruns, 393 verified bursts (103,022,592 bytes) and 224
observed PCM/write overlap windows. The full smoke suite passed on integrated
`29c75d8a`; the later dialog correction has a failing-before/passing-after host
regression and all three directed lifecycle VMs. Exact-stage GRUB/Limine boot,
expected native-profile refusal on Q35, unmount and automatic return pass.

PR160 is ready for integration and the authorized thirty-minute native launch.
The replacement payload/evidence are backed up outside build; retain Ubuntu,
console recovery and previously accepted desktop/reopen payloads. The automatic
trial opens/closes real apps throughout continuous PCM and unchanged verified
USB stress, then retires the battery runtime and returns to Ubuntu for automatic
collection. No human file work is required. Keep AC/USB connected and listen;
native responsiveness/endurance remains pending that run's complete receipts.

### Automatic UI/audio/USB stress return — 2026-10-25 (failed)

Release `8dd177cef12cec3b` / source `7ddf2153` returned automatically after
roughly half a minute, not the required thirty minutes. Matching session
`session-00065eb146d235a5-00` completed one Text open/render/close cycle:
1,624,727 us to ready and 707,052 us to close. Files then launched (exec
2,236,784 us), submitted and completed its Home directory listing, but never
produced the compositor's opened/ready receipt before the unchanged ten-second
cycle limit. The test reports FAIL and stops the workload; the temporary payload
is retained, with no final disk/PCM receipt. The logs do not yet identify the
missing window-admission/first-frame boundary, so do not assume the directory
listing alone caused the failure or increase the deadline to call this a pass.

Maximum measured desktop-loop gap was 108,087 us; no PCM underruns were recorded
during the short run. These are encouraging bounded observations, not endurance
or full responsiveness acceptance. Battery runtime retired with heap/errors/
denied all zero, 11 handled and zero unclaimed SCI events. All 340 prior files
are preserved except the expected workspace metadata update. Offline checks pass:
345 files / 1,007 clusters, zero leaks, bitmap mismatches or errors. Ubuntu is
back, next_entry is empty, automatic collection completed and the collector is
disabled. Failed evidence and payload remain backed up outside build.

Next investigation: reproduce and trace Files' OPEN publication, child
registration/admission and first-frame delivery under sustained disk pressure,
including the populated Home directory. Preserve the existing integrity checks,
ten-second cycle failure and 500 ms loop bound. No replacement run is armed.

### Files startup under load — active investigation (2026-10-25)

Base `d4715774` / merged PR160 and PR163. The local objective is to reproduce
the missing Files admission/first frame under the unchanged automatic PCM/USB
stress, fix the proven cause and repeat the fully automatic native trial.
Scope: compositor launch/admission, Files startup and narrowly required shared
runtime code, regression fixtures, this machine record and local run/collector
scripts. Preserve all integrity checks and the 10-second cycle / 500 ms loop
limits. PR162 separately owns automatic capacity-capture targeting after async
launch; preserve that work and inspect its actual integration before staging.
Pro PR19 retains independent Wi-Fi scope. User authorizes testing, self-merge,
installation and automatic reboot. Previous failed evidence is collected and
backed up; no new physical result is accepted.

### Files startup investigation checkpoint — 2026-10-25

The native missing-admission failure is not yet reproduced or fixed. Synthetic
populated-Home stress (64 existing files) passes with 400 write IOPS: source
`02b1e4c1`, ten Text/Files/System cycles, maximum loop 349,423 us, ready
3,904,186 us, close 2,809,892 us, zero app errors and 222 observed PCM/write
overlap windows. All 66 prior fixture files and offline allocation checks pass.
Build/core tests pass after consuming merged PR162 (`f4e9fbcb`).

Keep the failed experiments: 40 write IOPS delays the first Text launch past the
cycle bound; 100 write IOPS admits Text but misses its first-frame timeout.
Neither reproduces the native missing Files OPEN. An unthrottled populated run
also exceeded the loop limit (505,077 us) and remains a failure, despite passing
its audio/disk receipt. Do not convert those results into startup acceptance.

Fixed a measurement omission: an iteration that breaks on timeout was absent
from the loop maximum. The final interval now counts against the same 500 ms
limit, with a host regression. The earlier native 108,087 us maximum may therefore
understate the final stall. Added successful legacy OPEN timestamps, compositor
admission identity and deadline child/window state, plus a populated-Home VM
fixture. The ten-second app-cycle and all client deadlines, filesystem, PCM,
process/admission and native ownership checks remain unchanged.

PR164 is a diagnostic/measurement change, not a claimed Files startup fix. The
next authorized automatic native run repeats the unchanged thirty-minute
workload with these receipts, no human file work, and automatic return/collection.
Preserve the previous failed release and accepted recovery. Inspect the new
matching failure boundary before proposing a production startup change.

### Instrumented app/audio/USB stress return — 2026-10-25 (failed)

Release `d96ecb5806eba5c9` / source `6a0efaaf`, merged PR164, returned and
collected matching session `session-00065eb1d5c0e3e5-00`. Six complete cycles
(Text, Files, System twice) passed. Both Files OPEN requests reached compositor
admission in under 1 ms; ready took 3,213,396 and 4,658,569 us, and close/reap
took 1,783,328 and 1,796,376 us. The previous missing Files admission did not recur.

The seventh cycle, Text, took 7,678,976 us to its first frame. Its window then
closed, but the process had not completed cleanup/reap when the unchanged
whole-cycle deadline fired at 10,000,722 us (phase 3, no pending OPEN or app error).
It subsequently exited with status zero during test teardown. This is a failed
stress run, not acceptance: no final workload/disk receipt exists. The measured
loop maximum, now including the final interval, was 110,197 us. PCM had zero
recorded underruns during the short run; thirty-minute endurance remains pending.

Battery epoch 1 retired with heap/errors/denied zero, 315 queries and one
notification. All 345 prior USB files are preserved except expected workspace
metadata. Offline checks pass: 350 files / 1,013 clusters, no leaks, bitmap
mismatches or errors. Ubuntu recovered, next_entry is empty, collector disabled,
and matching raw evidence plus diagnosis are backed up outside build. No reboot
is armed. Current integration `f1131519` also contains PR165's independent modal
pointer correction; this returned payload predates that correction.

Next target: shared app startup and cleanup delays under sustained disk
contention, including gated allocation/unmapping and process retirement. The
receipts establish the delayed phases, not the exact kernel cause. Preserve the
10-second whole-cycle, client deadlines, 500 ms loop and all integrity checks;
do not treat successful Files admission or a zero-status late exit as a pass.

### Shared app latency under disk load — active scope (2026-10-25)

Base `296f86bb`, including PR165 modal correction and PR166 return evidence.
Investigate starvation at the shared I/O admission gate: waking blocked app
allocation/cleanup calls does not necessarily let them run before a disk writer
starts another owned operation. Scope is scheduler gate handoff at a completed
userspace syscall boundary, dispatch/cancellation integration, deterministic
scheduler regressions and automatic disk/audio/app VM/native validation. Do not
release ownership mid-operation or admit unsafe calls during owned I/O. Preserve
all existing integrity checks, the ten-second whole-cycle and 500 ms loop limits.
Pro PR19 (`200ab19b`) remains separate USB Wi-Fi work; no other open shared-runtime
MR was found. The previous run is collected and backed up. User explicitly
authorizes tests, self-merge and reboot into the automatic stress workload.

The implementation retains each blocked call's admission flag after wake-all,
clearing it only when the gate wait returns or the thread retires. A fully
completed userspace syscall that required the I/O gate gives one ready waiter
a turn before the writer can submit another operation. No handoff occurs with
active/nested ownership or an unsafe caller. GUI/audio-safe syscalls retain their
normal turns. The scheduler's ordinary round-robin/service eligibility, syscall
whitelist, VFS checks and desktop timing limits are unchanged. Cancellation is
rechecked after handoff; killed/quarantined waiters cannot retain stale admission.

The syscall-boundary regression fails on the old implementation. New host cases
exercise 64 successive writer rounds, preferred awakened admission, nested-owner
and unfinished-kernel refusal, canceled waiter retirement, and cancellation
during handoff. Initial VM experiments are retained: the overbroad prototype
(which also handed off after GUI-safe calls) completed 13 cycles at 400 IOPS but
failed a 512,391 us loop gap; at 100 IOPS Text completed but Files exceeded the
whole-cycle deadline. These are failures, not native acceptance. The narrowed
implementation and its matched physical payload require fresh validation.

Final narrowed source `74939958` passes build/core, full smoke and the storage
service VM (throttled writes, USB typing, cancellation, copy comparison, fsck and
timeout/quarantine). The populated-Home 400-write-IOPS automatic stress passes
15 complete app cycles: maximum ready 1,423,863 us, close/reap 2,526,714 us and
loop 480,204 us, with zero app errors or PCM underruns. It verifies 117 unpaced
256-KiB bursts, 223 observed PCM/write overlap windows, preservation of all
66 prior fixture files and clean offline allocation. All original limits remain.

Release `95f8a9cf59b25ba1` freezes that clean source. Its exact-stage Q35 rehearsal
passes the expected native-profile refusal, save/unmount and automatic return;
this is not physical battery or stress acceptance. PR167 carries the fix and
this documentation-only follow-up. Main remains `296f86bb`; Pro PR19 `3e979549`
retains separate Wi-Fi scope without scheduler/dispatch overlap. Next is the
authorized thirty-minute automatic native audio/disk/Text–Files–System stress,
with listening only, automatic Ubuntu return and collection. Preserve accepted
recovery and the prior failed run; require complete matching receipts before
claiming hardware acceptance.

### I/O handoff native stress return — 2026-10-25 (failed)

Release `95f8a9cf59b25ba1` / compiled source `74939958` (PR167) returned matching
session `session-00065eb25dc59f36-00`. Thirty-four automatic Text/Files/System
cycles completed; the next Files cycle became ready after 7,991,615 us and hit
the unchanged whole-cycle deadline at 10,000,506 us during close/reap. Its
window was still present at the deadline. The process subsequently exited zero
during teardown. The run ended after roughly 2.5 minutes without a final
workload receipt, so thirty-minute endurance and the latency fix remain unaccepted.

The loop maximum was 110,744 us, completed close/reap maximum 5,571,788 us, and
PCM recorded zero underruns. Files' final OPEN request reached compositor
admission in 794 us; most startup delay preceded that request. This narrows
the next investigation to app bootstrap/allocation and close/retirement timing
under sustained disk contention, without proving the remaining kernel cause.
Keep the ten-second whole-cycle, client deadlines and 500 ms loop bound intact.

Battery epoch 1 retired cleanly with 862 queries, eight notifications and zero
heap/errors/denied. All 350 prior USB files are preserved except expected workspace
metadata. Offline fsck/allocation pass: 355 files, 1,019 clusters, zero leaks,
bitmap mismatches or errors. Matching evidence and diagnosis are backed up
outside build with verified hashes. Ubuntu returned, next_entry is empty and
the collector is disabled; no new reboot is armed. Integration remains `b537c2a2`.

### App lifecycle timing — active scope (2026-10-25)

Base `46f6212f`, with the preceding native failure collected and backed up.
Add bounded timing receipts in desktools main/Text runtime for entry,
initialization, window creation/publication, close delivery, worker retirement
and memory cleanup; validate the receipts in the automatic audio/disk/app VM.
This is instrumentation, not another claimed latency fix. Do not change the
workload, scheduling, ownership checks, ten-second whole-cycle, five-second
client deadlines or 500 ms loop bound. Prepare the same automatic native trial,
self-merge and reboot under the user's explicit authorization.

PR169 (`edc320d8`) owns compositor unsaved-work shutdown interactions; keep its
deskd/session changes separate. PR19 (`c4af7dcc`) retains USB Wi-Fi scope. The
planned edits are desktools runtime/timing, stress-receipt validation and this
unit record, with no overlapping production file edits in those open PRs.

Implemented sparse `app-timing` records with PID/role/window identity and
begin/end/event timestamps. Spans include scheduling, I/O waits and diagnostic
overhead; they are not CPU-time or gate-wait attribution. A begin without an end
remains incomplete evidence. First-publication and close-delivery events connect
initialization/cleanup phases to compositor ready/reap receipts. The VM validates
required phases, paired durations and lifecycle ordering for each completed app.
Native collection preserves the available timing tail even on early failure.

Source `23cf7ca3` passes build/core, timing-validator positive/negative cases,
full smoke and populated-Home 400-write-IOPS stress (13 cycles). Its first VM
attempt halted in a synthetic battery sample with an EC error before any
instrumented app started; that failure is retained. An identical repeat passed
without configuration or predicate changes. Files worker-pool retirement reached
4,337,451 us in the repeat, demonstrating phase separation, not a native cause.

Consumed merged PR169 / main `f658f6b7` as `a54e1717`. Combined build/core and
the same automatic stress pass: 15 complete timing-validated app cycles, maximum
ready 1,457,954 us, close/reap 3,168,146 us, loop 136,962 us, zero app errors or
PCM underruns, 221 observed playback/write overlap windows, all 66 prior fixture
files preserved and clean offline allocation. The earlier full smoke remains
at `23cf7ca3`; the combined source has fresh focused runtime validation.

Release `282c3f6a06afd9eb` freezes clean runtime source `a54e1717`. Exact-stage
Q35 rehearsal passes expected native-profile refusal and automatic save/unmount/
return, not native battery or endurance acceptance. PR170's final follow-up is
documentation only. Next is the authorized automatic thirty-minute native trial
with unchanged workload/limits, listening only and automatic return/collection.

### Timing trial: three native attempts — 2026-10-25 (failed/incomplete)

Release `282c3f6a06afd9eb` / source `a54e1717` (PR170) produced three matching
sessions. The user reports interrupting the first boot, unplugging AC after a
missing battery percentage on the second, and an automatic restart after about
nine to ten minutes on the third. The single-session collector refused the three
matches; preserve its original failure and reconcile all three separately.

- `session-00065eb30ecc535a-00`: boot log only, consistent with the reported
  interruption. No completion or runtime acceptance.
- `session-00065eb30ffd922b-00`: native battery preflight refused SSDT fingerprint
  `faa7c4fc101b8bed`, absent from the exact captured set. No PCM/app cycles started;
  the failed session drained and saved its exit log at 18.28 seconds. This explains
  the missing battery indicator and is consistent with automatic failure return,
  not evidence that AC unplug caused a crash. Preserve the firmware gate; the
  differing table needs a separate read-only capture/comparison before any change.
- `session-00065eb312133238-00`: 139 app cycles completed before Files cycle 139
  exceeded the unchanged whole-cycle deadline at 10,000,118 us during close.
  Exit log saved at 545.22 seconds uptime. No final workload receipt, so the
  thirty-minute endurance test failed. The loop maximum was 346,152 us and PCM
  recorded zero underruns. Battery epoch 1 retired with 3,151 queries, 18
  notifications and zero heap/errors/denied; 220 SCI deliveries were handled.

The final Files cycle reached ready in 5,692,308 us, but main-entry to first
publication took only 210,971 us. Runtime/pool/window/role initialization took
26/48/23/25 us. Most startup delay lies outside those initializers; inspect process
launch/first scheduling before assuming widget work is responsible. After close
delivery, worker-pool retirement took 4,069,936 us and crossed the cycle deadline;
history free took 13 us and window close 985 us. Files exited zero during teardown.
Next focus is process startup and file-worker retirement under disk contention,
with all existing deadlines/integrity checks retained. PR172's shared application
dispatch work must be inspected before overlapping userspace changes.

Reconciled all three sessions and available timing tails without promoting any
attempt to pass. All 355 prior USB files remain unchanged except expected workspace
metadata. Offline fsck/allocation pass: 364 files, 1,032 clusters, zero leaks,
bitmap mismatches or errors. Ubuntu recovered, next_entry is empty and the
collector is disabled. Evidence/diagnosis are retained outside build; no new
boot is armed. Integration baseline for this return record is `eda8bfbc`.

### Process admission and worker retirement — active scope (2026-10-25)

Base `aa7b7392`, with all three preceding attempts reconciled and backed up.
Investigate READY process startup/kernel retirement threads excluded by the I/O
gate without entering its explicit wait queue, and nonblocking child reaping
that repeatedly defers while I/O is owned. Extend quiescent handoff accounting
to those contenders without changing eligibility, nonblocking behavior, deadlines,
filesystem/ownership checks or workload. Scope: scheduler, syscall wait admission,
host regressions, automatic stress/boot validation and this unit record.
Retain the targeted app timings and the separate unresolved SSDT-fingerprint
refusal; no firmware gate changes. User authorizes tests, self-merge and reboot.

PR172 (`27a97f0f`) owns shared application dispatch/waiting/actions; the proposed
kernel admission changes avoid its userspace implementation scope. PR19
(`4030debb`) retains USB Wi-Fi work. Inspect their actual updated diffs before
integration; no cross-task messages or parallel local harnesses are authorized.

The scheduler now records actual READY exclusion and deferred nonblocking reaps
as pending quiescent turns. Ordinary owner-free selection consumes the request;
owned-I/O service turns preserve it. This covers process entry and post-exit
cleanup without running either under another owner's I/O. Host regression fails
on the previous scheduler and passes with the fix, including repeated requests,
service eligibility, sleeping threads and retirement continuations. Build/core
passes; automatic workload and native acceptance remain pending.

The first positive VM completed 14 app cycles with zero app errors/PCM underruns,
but strict timing validation failed on an interleaved live UART line. Preserve
that failed harness evidence. The console already saves whole diagnostic records
before preemptible UART output; validate those saved overlapping snapshots instead.
The validator retains phase, ordering, duration and missing-record checks, rejects
duplicates within one snapshot, and deduplicates only exact overlapping records.
No production console path, workload or limit changes are needed.

Source `cca1f818` passes build/core, all 482 CLI commands and the complete smoke
suite, storage-service slow writes/input/cancellation/copy/relaunch/quarantine,
and the populated-Home 400-write-IOPS automatic workload. The positive VM
completed 19 timing-validated app cycles with zero errors and PCM underruns;
maximum ready/close/loop were 1,161,076 / 2,140,722 / 228,514 us. Files pool
retirement peaked at 742,552 us. All 66 prior fixture files survived and offline
allocation was clean. These VM timings do not establish native acceptance.

Release `788873710e559400` freezes that clean source. Exact-stage Q35 rehearsal
passes the expected native-profile refusal, saved logs, unmount and automatic
return. This follow-up changes documentation only. PR172 (`122fe555`) retains
shared app runtime ownership, PR19 (`50e4dce5`) USB Wi-Fi, and PR175 (`a90ac149`)
macOS build/docs; no overlapping kernel production edits were found.

Next is the authorized thirty-minute native audio/disk/app trial, with automatic
Text/Files/System launches and closes, continuous verified I/O, battery monitoring
and listening only. Whole-cycle ten seconds, client five seconds and loop 500 ms
remain unchanged. USB baseline is 364 files / 1,032 clusters with zero leaks,
bitmap mismatches or errors. Ubuntu remains default with console recovery and
accepted reopen payloads retained. The collector saves/checks multiple attempts
before refusing aggregate acceptance; missing completion remains failure.

### Process-admission native return — 2026-10-25 (failed)

Run `5732b14012b34571`, release `788873710e559400`, runtime `cca1f818`
(PR174) returned one matching session, `session-00065eb3b8aaf11c-00`.
The automatic trial failed after 49 complete app cycles: Files cycle 49 / PID73
crossed the unchanged ten-second whole-cycle deadline at 10,010,054 us during
close. Exit log saved at 190.998 seconds uptime. No final workload receipt was
written; this is not thirty-minute endurance acceptance.

The aggregate maximum ready time was 1,341,489 us, versus 7,313,178 us in the
preceding run; different run lengths prevent a general performance claim. Maximum
completed close was 7,869,937 us, and maximum loop gap 341,476 us. The failing
Files instance received Close at 180,124,884 us but did not leave its main loop
until 188,505,227 us, an 8,380,343 us gap. Its directory-list request 1 to worker74
was first logged submitted during post-deadline job drain. It then exited with
status143 during failed-session teardown, before a pool-retirement completion.
This failure precedes the pool-retirement phase seen in the previous trial.

Next investigate worker request/reply pipe admission under continuous disk
ownership. The current nonblocking pipe deferral returns AGAIN without requesting
the quiescent turn now used by deferred reaps. That is a concrete candidate
mechanism, not yet a demonstrated cause of this native delay. Preserve accepted
mutation ownership, CRC/reply validation and all deadlines; do not bypass cleanup.
PR172 still owns the shared application runtime and must be inspected before
any overlapping userspace work.

PCM recorded zero underruns. Battery epoch1 retired with 1,065 queries, one
notification, 71 handled SCI deliveries and zero heap/errors/denied. Overall
battery-controls reports FAIL because the desktop trial failed; clean provider
retirement does not promote the workload result. All 364 prior files survived
except expected workspace metadata. Offline fsck/allocation pass: 369 files,
1,038 clusters, zero leaks, bitmap mismatches or errors. Matching logs, original
automatic assessment and diagnosis are hash-verified outside build. Ubuntu
recovered, next_entry is empty and collection disabled; no new reboot is armed.
Integration baseline for this evidence record is `a679115f`.

### Integrated-main retest — active scope (2026-10-26)

User requests rebasing onto the bug fixes merged since the previous native trial
and retesting. Rebased `device/macbookair6-1/process-admission` onto actual main
`dfd63fef`; all local changes were already merged, so no commits required replay.
This consumes shared application runtime PR172, synchronization/build audit
PR175, AHCI completion/budget corrections PR178, and event-adapter audit PR179.
The last Air6,1 failure remains recorded and backed up; no native acceptance is
inferred from other hosts' results.

Scope is validation and this unit record: matched build/core, automatic populated
Home audio/disk/app stress, general smoke, storage cancellation/quarantine and
exact-stage rehearsal, followed by the same authorized automatic native trial.
Do not add the previously proposed pipe-admission fix before testing the new
baseline. Preserve ten-second whole-cycle, five-second client and 500 ms loop
limits, continuous verified disk work, PCM checks and firmware/ownership gates.
PR19 (`eababae2`) retains separate USB Wi-Fi ownership; no shared production edits
are planned. Prior explicit self-merge/reboot authorization remains applicable.

The fresh Ubuntu build reproduced a merged poll compile regression: the new
synthetic-readiness path uses timespec/clock_gettime without including time.h,
and its strict-C host fixture lacks the POSIX feature declaration. Extend this
validation scope only to the explicit time header and fixture feature macro;
retain the failed build log. No runtime policy, pipe admission or limits change.

The next build gate exposed an outdated process-service host fixture: mapping
retirement now references scheduler_wake_all for futex invalidation, but the
fixture did not provide it. Add a checked wake hook and exercise an active futex
on a retiring page, asserting invalidation/wake occurs before frame release.
This updates test integration only; the merged process behavior is unchanged.

The new audit drivers also mixed SrvrOS headers with GCC's hosted stdint/glibc
internals on Ubuntu. Compile their target-header translation units in freestanding
mode, retaining host linking and the separately compiled native pthread shim.
Apply this to time, synchronization and event-adapter fixtures; no test assertions
or production behavior change. Preserve the failed compile as portability evidence.

Candidate `5f0326b4` passes the full matched build/core suite, including the new
time/synchronization/futex/event-adapter host regressions, and the complete thread
guest suite. General smoke passes all 482 CLI commands, shell editing, syscall
ABI, UEFI keyboard and display. Storage-service passes throttled writes/input,
cancellation, file comparison, repeated launches and timeout/quarantine.

The populated-Home 400-write-IOPS combined VM completed 14 timing-validated app
cycles, 117 verified disk bursts and 225 observed audio/write overlap windows,
with zero app errors or PCM underruns. Maximum ready/close/loop were 1,348,680 /
6,484,660 / 188,430 us. All 66 prior fixture files remained intact and allocation
checks passed. The long close duration reinforces the need for native testing;
this is not a claimed fix or acceptance of the previous Files-close failure.

Release `83c919c9b7a24b6b` freezes clean candidate source `5f0326b4`. Exact-stage
Q35 rehearsal passes the expected native-profile refusal, saved logs, clean
unmount and automatic return. This final follow-up is documentation only. Main
was rechecked at `dfd63fef`; PR181's separate Text/Files window-lifecycle scope
and PR19's Wi-Fi work are not included. Native run `d10d796558a34499` will repeat
the same authorized thirty-minute automatic workload with unchanged limits.
USB baseline: 369 files / 1,038 clusters, zero leaks/mismatches/errors. Ubuntu
remains default; console recovery and accepted reopen backups are retained.

### Integrated-main native return — 2026-10-26 (failed)

Run `d10d796558a34499`, release `83c919c9b7a24b6b`, compiled `5f0326b4`
(PR180, merged as `afc8bb67`) returned one matching session,
`session-00065ec131ee0a4e-00`. The automated trial again failed during Files
close: cycle13 / PID25 crossed the unchanged ten-second whole-cycle limit at
10,020,194 us after 13 completed app cycles. The final snapshot was saved at
67.914 seconds uptime; no complete workload receipt exists. This is failed
native endurance evidence despite the passing preboot host/VM checks.

Maximum ready/completed-close/desktop-loop times were 1,363,239 / 7,260,207 /
109,543 us. All 13 completed cycles pass the saved-record timing validator.
The failed Files process received Close at 56,508,490 us, but left its main loop
at 65,489,095 us: an 8,980,605 us gap. Its directory-list request1 to worker26
was logged submitted between the 57.210 and 62.551 second battery records,
before the deadline. Job drain began only after the deadline; teardown exited
PID25 with status143, without a pool-retirement completion. Unlike the previous
run, submission occurred before failure, so request admission alone is not a
proven explanation. Inspect both request and reply progress under continuous
disk ownership; the existing nonblocking pipe deferral remains a candidate,
not an established cause. Keep all ownership/integrity checks and limits.

PCM recorded zero underruns. Battery epoch1 retired with 360 queries, one
notification, 24 handled/zero unclaimed SCI deliveries and zero heap/errors/denied.
The overall desktop/battery-controls result remains FAIL. All 369 prior files
were preserved except expected workspace metadata. Read-only fsck and allocation
checks pass with 374 files / 1,044 clusters and zero leaks/mismatches/errors.
Raw evidence and diagnosis are retained outside build with verified hashes.
Ubuntu recovered, the one-shot entry is empty and collection is disabled; no
new reboot is armed.

Closeout baseline is `afc8bb67`. Open PR181 (`7bd16b5e`) owns shared Text/Files
window lifecycle and accepted-job retirement; inspect its current implementation
before overlapping userspace changes. PR182 (`352f9b0d`) owns exFAT import
validation and PR19 (`3e16ff6a`) USB Wi-Fi. None was included in this physical
payload. This closeout changes only this unit's evidence record.

### Worker pipe admission — active scope (2026-10-26)

Proceeding from main `9f18d7d6` after collecting the failed integrated retest.
Scope is the kernel nonblocking pipe/PTY admission boundary, its directed syscall
and scheduler regressions, and this unit's unchanged automatic audio/disk/app
trial. Both request writes and reply reads currently return AGAIN during another
storage owner's turn without requesting the quiescent admission already used by
nonblocking reaps. Test and repair that missing request while preserving immediate
deferral, user service eligibility, descriptor cleanup safety and coalesced
scheduler ownership. This is a candidate mechanism for the observed close delay;
native acceptance remains pending.

PR181 (`dafd5bed`) owns shared Text/Files window and accepted-job lifecycle;
avoid competing userspace changes. PR182 (`352f9b0d`) owns exFAT import validation
and PR19 (`7961d09e`) Wi-Fi. Neither requires changes to this kernel admission
scope. Keep ten-second cycles, five-second clients, 500 ms loop bound, continuous
verified I/O, PCM checks, firmware gates and accepted-operation draining intact.
Next checkpoint: failing-before/passing-after host tests, matched build and VM
stress/storage checks, exact-stage rehearsal, merged candidate and authorized
automated native trial with saved evidence and Ubuntu recovery.

Implemented `49f51e9b` requests the existing quiescent turn from both nonblocking
pipe/PTY deferrals. The new directed syscall test fails before the fix and passes
after for read/write/duplex/PTY retries, with blocking, invalid and nonpipe
exclusions. Scheduler coverage retains one coalesced request across 64 owned-I/O
service turns and consumes it only at quiescence; cleanup/owner gates remain.
The current admission guide records this shared contract. PR181 edits different
sections of that guide; no userspace implementation is changed here.

Matched normal/native-battery/VM-kernel build and full core suite pass, as do
thread, general smoke (482 CLI commands, shell edits, syscall ABI, UEFI keyboard
and display), and storage-service cancellation/timeout/quarantine regressions.
The populated-Home 400-write-IOPS VM completes 19 timing-validated app cycles,
117 verified bursts / 30,670,848 bytes, and 218 observed audio/write overlap
windows with zero app errors or PCM underruns. Maximum ready/close/loop times
are 1,291,587 / 1,860,049 / 402,419 us. All 66 prior fixture files survive and
allocation is clean. Files' maximum Close-to-return is 1,753,005 us versus
6,373,924 us in the prior VM run; this is a run comparison, not native acceptance
or proof that all sources of delay are fixed.

Release `09f42ae5a75b3fef` freezes clean compiled source `49f51e9b`. Exact-stage
Q35 rehearsal passes expected native-profile refusal, saved logs, clean unmount
and automatic return. This follow-up changes documentation only. Native run
`6d1b066474954f5d` repeats the unchanged thirty-minute automated workload with
374 baseline files / 1,044 clusters, continuous audio/verified disk work and
automatic Text/Files/System cycles. Ubuntu remains default; console and accepted
reopen recovery payloads remain verified and independent of build storage.

Before installation, main advanced to `0858e8e6` with PR181's shared Text/Files
window lifecycle and PR185's bounded UART progress matcher. Merged that actual
baseline at `85fdf6c8`; the earlier candidate remains separately preserved and
was not installed. Extend integration scope only to the stress timing validator:
explicitly select the new Files mapped-model/disposal lifecycle, require its
close acknowledgement and complete phases, and retain the old validator mode
for historical captures. Do not infer a layout from absent phases or relax the
timing limits. Rebuild and repeat the combined stress and affected window/close
VMs before staging the integrated native payload.

Integrated clean candidate `c275c098` passes matched build/core and the fresh
populated-Home 400-write-IOPS stress VM: 19 validated cycles, 116 verified bursts /
30,408,704 bytes, 223 observed audio/write overlap windows, zero app errors and
PCM underruns, preserved prior files and clean allocation. Maximum ready/close/
loop times are 1,100,329 / 2,289,616 / 149,988 us. New timing-layout tests require
complete mapped-model disposal/acknowledgement and reject missing phases or a
wrong layout while retaining historical validation.

Fresh actual-consumer VMs also pass twenty same-process Files windows with
independent history, owned dialog cancel/collision, an accepted 8 MiB copy retained
through close, concurrent Text save, exact contents and complete cleanup;
ordinary shutdown cancellation, failed-save retention and save/discard decisions;
and real Terminal shell/PTY input, paste, backspace, independent sessions, Ctrl-C,
close cancellation and persisted output. Earlier thread/general/storage checks
above apply to `49f51e9b`; the kernel admission fix is identical in the integrated
candidate. No new production change was needed beyond that fix and merged main.

Final release `52eb28003f5be609` freezes `c275c098`, superseding uninstalled
`09f42ae5a75b3fef`. Its exact EFI rehearsal passes native-profile refusal on Q35,
saved logs, unmount and automatic return. Run `6d1b066474954f5d` now targets this
integrated release; baseline files, workload and all acceptance limits remain
unchanged. Final documentation is separate from the compiled source. Physical
acceptance requires the complete thirty-minute receipt after Ubuntu returns.

### Pipe-admission native return — 2026-10-26 (responsiveness failure)

Run `6d1b066474954f5d`, release `52eb28003f5be609`, compiled `c275c098`
(PR184 merged as `5bcd061c`) returned one matching session,
`session-00065ec1ef2cd6aa-00`. Audio/disk work completed all 1,800 seconds:
345,600,000 PCM bytes, zero underruns, worker status0, and 3,263 verified disk
bursts / 855,375,872 bytes. The binary disk receipt exactly matches the text
receipt, with 1,797.212 seconds between first and last I/O. The user confirms
audio remained steady throughout. This accepts that bounded playback/file-work
overlap on this unit, not the full desktop responsiveness result.

All 703 generated app cycles completed with zero child errors and no aggregate
app deadline failure. Maximum ready/close times were 1,773,756 / 3,109,315 us;
the earlier ten-second Files-close failure did not recur in this run. However,
the maximum desktop-loop gap was 558,863 us, above the unchanged 500,000 us limit.
The desktop and overall battery-controls result correctly remain FAIL despite
the successful audio/disk receipt. App coverage spans 1,782.666 seconds inside
the PCM interval, and the final exit snapshot was saved at 1,808.899 seconds
uptime. The retained complete timing tails validate cycles684–702 against the
new Files lifecycle; the log ring does not retain per-phase proof for all703.

The individual over-limit loop record has rotated out of every retained log.
Only its aggregate maximum survives, so neither its timestamp, event count nor
its cause can be established from this evidence. Next retain bounded worst-loop
timing with stage attribution across log rotation, then diagnose the remaining
compositor/scheduling gap. Do not raise the 500 ms limit or assume the gap is
rendering, storage, logging or firmware without attribution.

Battery epoch1 retired with 10,588 queries, 151 notifications, 813 handled/zero
unclaimed SCI deliveries and zero heap/errors/denied. All 374 prior files survived
except expected workspace metadata. Offline fsck and allocation pass: 380 files,
1,052 clusters, zero leaks/mismatches/errors. Original automatic failure, complete
workload receipts and detailed diagnosis are preserved outside build with verified
hashes. Ubuntu recovered, next_entry is empty and collection is disabled; no new
reboot is armed. Closeout baseline is `5bcd061c`; open PR182 (`a35e2c33`) retains
exFAT/storage-ownership work and PR19 (`44deba4e`) Wi-Fi. This closeout edits only
the unit record, not their implementation scope.

### Persistent worst-loop diagnostics — active scope (2026-10-26)

Starting from main `19a6fe05` after the complete failed responsiveness return.
Add fixed-size worst-loop retention to the opt-in compositor stress diagnostic:
timestamp, app cycle/phase identity, over-limit count and wall-time stage totals.
Re-emit the retained record periodically and at completion so log rotation cannot
erase the worst observation. These are elapsed intervals including preemption,
not CPU-time or kernel/firmware attribution. Preserve the exact loop measurement,
500 ms bound, app deadlines, workload and ownership/integrity checks.

Scope is compositor diagnostic bookkeeping, directed host/receipt regression,
combined stress/exact-stage validation and this unit record. PR182 (`a35e2c33`)
owns exFAT/storage ownership; PR19 (`88dff107`) owns Wi-Fi. No competing storage,
driver, firmware or scheduler change is planned. Next checkpoint is a merged,
locally validated candidate and the authorized fully automatic native trial.

Candidate `a8cdd087` adds fixed-size current/worst records and nine elapsed-time
stages, with periodic/final whole-record logging through the existing console.
The final partial loop remains included in the original maximum; diagnostic
formatting/emission is measured rather than excluded. No new writes/flushes,
workload pacing, app deadlines or hardware/ownership policy are introduced.
The host regression retains an old maximum through 1,000 shorter loops, checks
exact-bound behavior, conserved stages, partial exit, invalid clocks and complete
bounded formatting. Receipt tests reject truncated/missing/conflicting records,
lost maxima and disagreement with the existing lifecycle aggregate.

Matched build/core passes. The populated-Home 400-write-IOPS VM completes twenty
timing-validated app cycles, 117 verified bursts / 30,670,848 bytes and 220
observed audio/write overlap windows, with zero app errors or PCM underruns,
all 66 prior fixture files preserved and clean allocation. Maximum ready/close/
loop are 1,202,132 / 1,960,929 / 330,784 us. The final diagnostic covers 1,989
samples with zero over-limit/invalid measurements and exactly matches the loop
maximum. Its stages sum to the interval; render accounts for 323,275 us. An
older worst interval is preserved unchanged in several later periodic reports.
This proves diagnostic retention, not the cause of the earlier native gap.

The ordinary desktop shutdown VM passes cancellation, failed-save retention,
save/discard decisions and exact file/process cleanup, with no opt-in loop
diagnostic emitted. Release `080b645e1b3a5533` freezes clean `a8cdd087`;
exact EFI rehearsal passes the expected Q35 native-profile refusal, saved logs,
unmount and automatic return. Native run `d66265a8b5ea4b59` repeats the unchanged
thirty-minute workload on 380 baseline files / 1,052 clusters. Its collector
preserves the retained worst-loop record even when later acceptance fails.
Ubuntu remains default with console and accepted reopen recovery retained.

### Retained worst-loop native return — 2026-10-26 (wait/resume gap)

Run `d66265a8b5ea4b59`, release `080b645e1b3a5533`, compiled `a8cdd087`
(PR187 merged as `4d195e25`) returned one matching session,
`session-00065ec2b80b0dc6-00`. The complete 1,800-second audio/disk receipt
passes: 345,600,000 PCM bytes, zero underruns, worker status0, and 3,012 verified
bursts / 789,577,728 bytes. The binary disk receipt exactly matches the text;
first-to-last I/O spans 1,797.448 seconds inside playback. The user confirms
audio remained steady throughout this run. This accepts the bounded audio/disk
overlap; the separate desktop responsiveness failure below remains unchanged.

All 674 generated app cycles completed with zero child errors. Maximum ready/
close times were 1,899,733 / 3,820,172 us, and application coverage spans
1,784.523 seconds inside playback. The complete retained timing tails validate
cycles655–673 with the shared Files lifecycle. The overall desktop result remains
FAIL: two of 95,597 loop samples exceeded the unchanged 500,000 us limit, with
a maximum of 540,803 us. No invalid timing samples were recorded.

The new diagnostic retains sequence30358 across log rotation and in the final
receipt, exactly matching the lifecycle maximum. It spans 575,911,056 through
576,451,859 us uptime, cycle212, from phase0 to phase1 with pid0 at both ends.
WAIT accounts for 540,740 us; all other stages together account for 63 us
(including 4 us render). Stage totals exactly conserve the interval. This
accepts bounded diagnostic retention on native hardware, not responsiveness.

WAIT covers wait selection, the GUI wait or one-tick sleep, and resumption to
the next loop timestamp. The receipt does not distinguish the selected branch,
requested deadline, wake-to-run delay, I/O admission or preemption. Next isolate
those boundaries while keeping the loop/client/cycle limits and all integrity
and ownership checks unchanged. The evidence does not establish a storage,
scheduler or firmware root cause and cannot attribute the other over-limit
sample independently from this retained maximum.

Battery epoch1 retired with 10,563 queries, 111 notifications, 788 handled/zero
unclaimed SCI deliveries and zero heap/errors/denied. All 380 prior files survive
except expected workspace metadata. Read-only fsck and independent allocation
pass: 386 files / 1,060 clusters, zero leaks/mismatches/errors. Original automatic
failure, matching logs, workload receipts and the detailed return diagnosis are
preserved outside build with verified hashes. Ubuntu recovered, next_entry is
empty and collection is disabled; no reboot is armed.

Closeout base is `4d195e25`. PR182 (`16b81077`) retains exFAT/storage-ownership
work and PR19 (`f70ee542`) Wi-Fi. This closeout only records this unit's evidence;
no shared implementation is changed or new native trial prepared.

### Wake/resume attribution — active scope (2026-10-26)

Base `ee5468f6`, same local device branch. Isolate the retained WAIT gap with
opt-in, fixed-size per-thread scheduler diagnostics and compositor wait selection/
call boundaries. Retain blocked-state versus ready-to-dispatch totals, timeout/
signal wakes, deadline lateness in ticks and observed I/O eligibility exclusions.
These are diagnostic observations, not permission to widen scheduler admission.
Keep 500 ms loops, five-second clients, ten-second app cycles, continuous verified
I/O, PCM checks and all integrity/ownership gates unchanged.

Scope: scheduler bookkeeping, a bounded current-thread diagnostic ABI, compositor
worst-loop receipts, host/receipt/VM regressions and this unit record. PR182
(`16b81077`) owns exFAT/storage ownership; PR19 (`f70ee542`) owns Wi-Fi. Do not
change those implementations. Prior run is fully collected and listening confirmed.
Next checkpoint: reviewed/merged diagnostics, matched host/core and stress/boot
validation, then the authorized automatic thirty-minute native run and recovery.

Candidate `f1587a2f` adds an explicit bounded current-thread trace ABI and
version2 retained loop companions. Scheduler observations separate blocked state
from READY-to-selection, retain the longest ready interval with its wake/deadline
and exclusion counts, and flag invalid clocks. GUI waits and one-tick sleeps keep
their original branches, durations and fallback behavior. No scheduler selection,
I/O admission, lifetime, firmware, integrity or acceptance limit changes.

Matched normal/native-battery/VM kernels and full core pass. Directed host tests
separate 10 ms blocked from 520 ms ready while preserving unsafe-thread exclusion;
cover signal wakes, late expiry, runnable preemption, reset/refusal and invalid
clocks; and validate exact-size mapped syscall copies. Receipt tests preserve
historical version1 support and require complete immutable version2 companions,
ordered calls, conserved state times and explicit no-WAIT early exits. Formatting
and thousand-loop retention tests cover the whole companion set. Thread stress
and storage-service VMs pass, including cancellation, timeout/quarantine and file
comparison. Ordinary shutdown passes with no opt-in loop diagnostics emitted.

The populated-Home 400-write-IOPS VM passes twenty app cycles, 117 verified bursts /
30,670,848 bytes and 221 audible-write overlap windows, with zero child errors
or PCM underruns, 66 prior files preserved and clean allocation. Maximum ready/
close/loop times are 1,061,653 / 1,559,799 / 139,632 us. Its final diagnostic covers
2,068 samples, zero over-limit/invalid observations, and retains the original
sequence135 through completion. The worst interval uses the ten-tick GUI wait:
96,605 us blocked, 41,563 us READY-to-selection, twelve observed I/O exclusions,
and expiry observed exactly at its deadline tick. This validates attribution in
the VM, not the cause of the prior native over-limit interval.

Release `0ede0fc436410e26` freezes clean `f1587a2f`. Exact EFI rehearsal passes
the expected Q35 native-profile refusal, saved logs, unmount and automatic return.
Run `f6e01ca76b5a4da2` repeats the unchanged thirty-minute automatic workload on
386 baseline files / 1,060 clusters. Its collector requires version2 attribution
and retains it independently of an overall failure. Payload/evidence backups
remain outside build; Ubuntu, console and accepted reopen recoveries are retained.
Final main remains `ee5468f6`; PR182 (`91a4f594`) and PR19 (`9a50f4dc`) keep
separate storage and Wi-Fi scopes. Final documentation changes no compiled code.

### Wake attribution native return — 2026-10-26 (narrow automated pass)

Run `f6e01ca76b5a4da2`, release `0ede0fc436410e26`, compiled `f1587a2f`
(PR190 merged as `4ee18f69`) returned one matching session,
`session-00065ec3dd1e846a-00`. The automatic collector passes the full thirty-minute
workload: 345,600,000 PCM bytes, zero underruns, worker status0, 3,154 verified
bursts / 826,802,176 bytes, and an exactly matching binary disk receipt.
First-to-last I/O spans 1,797.058 seconds inside playback. The user confirms
audio remained steady throughout this run.

All 689 app cycles completed with zero child errors. Maximum ready/close times
were 1,912,039 / 3,460,137 us; application coverage spans 1,785.112 seconds
inside playback. Retained complete timing tails validate cycles671–688. All
94,747 loop samples are valid and below the unchanged 500,000 us limit, but the
498,244 us maximum passes by only 1,756 us. This is acceptance of this bounded
automated run, not evidence that the prior intermittent failures are fixed.

The final version2 receipt retains sequence65648, cycle481, phase0 to phase1,
from 1,251,783,148 to 1,252,281,392 us uptime. The selected ten-tick GUI wait
returned normally, without fallback sleep. It spent 96,987 us blocked and
401,159 us READY-to-selection. Timeout wake was observed at tick125057, exactly
its deadline tick; no late ticks were observed in this bracket. Twenty-three
I/O eligibility exclusions were observed during the same ready interval.
The complete loop has 498,180 us in WAIT and 64 us across all other stages.
Companion clocks, stage/state totals and the lifecycle maximum validate exactly.

This isolates the dominant interval after wake-up, rather than late tick expiry
for this sample. Exclusion counts do not prove that all READY time was spent
continuously excluded or identify the owner. Source inspection shows GUI waits
are already admitted as I/O-safe, while same-process siblings remain excluded;
the compositor's persistent lifecycle pthread performs app launches and mapping
work. That is a specific candidate for the next ownership/admission audit during
launch, not a confirmed owner identity or permission to weaken sibling protection.
Keep all limits and integrity/ownership checks unchanged.

Battery epoch1 retired cleanly with 10,537 queries, 67 notifications, 751 handled/
zero unclaimed SCI deliveries and zero heap/errors/denied. All 386 prior files
survive except expected workspace metadata. Read-only fsck and independent
allocation pass: 392 files / 1,068 clusters, zero leaks/mismatches/errors. Matching
receipts, logs and detailed diagnosis are retained outside build with verified
hashes. Ubuntu recovered, next_entry is empty and collection is disabled; no
new boot is armed. Closeout base is `4ee18f69`; PR182 (`2a091597`) and PR19
(`db3d8633`) retain separate storage and Wi-Fi scopes. This closeout changes
only this unit record.

### Private app-image ownership audit — active scope (2026-10-26)

Base `d24dbc11`, same local device branch. The lifecycle pthread runs exec;
its image loader operates on a newly reserved child using a kernel snapshot of
the executable path, but the service gate currently excludes the caller's UI
siblings throughout that private child work. Parent mapping refresh, descriptor
inheritance and record mutation require the existing caller exclusion and must
retain it. Audit and narrowly distinguish private child-image ownership from
parent mutation, without releasing the global I/O/cancellation gate or admitting
unsafe syscalls. No acceptance, workload or resource limits change.

Scope: process image construction, scheduler exclusion identity for that explicit
scope, ownership/refusal regressions and this unit record/current contract.
PR182 (`2a091597`) owns exFAT/storage changes and PR19 (`db3d8633`) Wi-Fi;
inspect their current shared-file scope before integration. Prior return is fully
collected and listening confirmed. Next checkpoint is failing-before/passing-after
ownership tests, matched build/core and VM stress/lifecycle validation, then the
merged candidate and authorized automatic native trial with preserved recovery.

### Private app-image ownership — candidate validated (2026-10-26)

PR192 builds at `daa94dd8` from `d24dbc11`. Only fresh child-image construction
uses the child as the scheduler exclusion identity, after a kernel path snapshot
and refusal of live targets or inherited/foreign ownership. Audited parent UI
siblings may run; the global storage/cancellation gate and unsafe-call exclusion
remain. Ordinary parent exclusion resumes before descriptor inheritance, parent
refresh or child publication. Existing loader, mapping and rollback integrity
checks, resource limits, 500 ms loop, five-second client and ten-second cycle
limits remain unchanged. PR182's current `process.c` edits are confined to
separate file-flush paths; PR19 (`97fb3750`) has no overlapping implementation.

Directed scheduler/process-service/loader regressions and the matched build/full
core suite pass. The old exclusion-predicate control fails the new sibling-service
assertion; refusal, nesting, path snapshot and failed-load restoration tests pass.
Terminal real-shell/PTY/clipboard/exit/persistence, threading, throttled storage
service and desktop shutdown VM gates pass. The populated-Home automatic VM
(400 write IOPS) completed 60 seconds, 115 verified bursts, 20 app cycles,
zero underruns/errors, 399.027 ms worst loop, 1.251122 s worst readiness and
1.410113 s worst close, preserving all 66 fixture files.

Release `204302d98c2906fd` is staged clean from the tested code. Exact-stage VM
verifies native-profile refusal, saved evidence and automatic return on Q35;
it does not accept native EC ownership. Payload hashes and durable backup pass.
The authorized next native run is the unchanged 30-minute continuous PCM,
unpaced verified USB work and automatic Text/Files/System launch/close trial,
retaining worst-loop wait/scheduler diagnostics. No guest input is needed beyond
listening; keep AC and USB attached. Ubuntu remains the recovery OS with console
`c35475f62f154f1e` and the accepted automated/attended reopen payloads retained.
The previous bounded stress payload `0ede0fc436410e26` remains available. Native
responsiveness improvement is pending the matching returned receipt, file
preservation/allocation checks and listening confirmation; collect before another
trial. Local run `b907f47823d34af2` is under
`build/battery/air61-launch-audit-20261026`, with a durable copy outside `build`.

### Private app-image ownership — native return (2026-10-26)

Run `b907f47823d34af2`, release `204302d98c2906fd`, compiled `daa94dd8`
(PR192 merged as `33edbafc`), returned automatically to Ubuntu. Exactly one
matching session, `session-00065ec63701f318-00`, contains the final workload,
loop, application and battery retirement receipts. This is a **PASS for the
bounded automated workload**, including the user’s subsequent confirmation that
sound remained steady throughout all 30 minutes.

The full 1,800 seconds submitted 345,600,000 PCM bytes with zero underruns and
worker status zero. Continuous verified filesystem work completed 3,091 bursts
(810,287,104 bytes), spanning 1,797.237424 seconds within live playback. Automatic
Text/Files/System cycles completed 692 times with zero errors; worst readiness
was 1.835340 s and worst close 2.871404 s. All limits and workload pacing remain
unchanged. Retained complete app phases for cycles 674–691 independently validate;
they are a tail, not a replacement for the full-run aggregate receipt.

Worst desktop loop is now **111.984 ms**, down from 498.244 ms in the previous
run, across 93,834 samples with zero over-limit or invalid measurements. Of that
worst loop, 111.880 ms is wait and 104 microseconds is other measured work. The
scheduler records 99.833 ms blocked, 12.014 ms READY and zero I/O exclusions;
the timeout wakes on its deadline tick. The previous worst recorded 401.159 ms
READY and 23 exclusions. This supports the private-image ownership fix on this
unit without claiming that every workload or longer endurance is accepted.

Battery epoch 1 retires after 10,530 queries and 54 notifications with one
attach/detach and zero heap, errors or denials. All 392 pre-existing files are
preserved except expected `.srvros/meta` bookkeeping; the 398-file result has
1,076 allocated clusters, zero leaks/mismatches/errors and clean offline fsck.
GRUB one-shot is consumed and the matching collector disables itself. The
333-file returned evidence backup was hash-verified outside `build`; diagnostic
closeout files are subsequently added to that backup. No new reboot is armed.

Closeout base is `ab7a7c93` (PR193 adds only the Air7,2 record). PR194 owns
window/dock work, PR182 storage hardening and PR19 Wi-Fi; this closeout changes
only the Air6,1 machine record. The next useful acceptance is attended interactive
use under the same automated background workload, if desired, rather than a
higher latency limit. Retain separate limits for this bounded result and broader
interactive behavior.

### Attended interaction under automatic load — active scope (2026-10-26)

Base `43aff7d5` includes merged PR194 window movement/running dock work and the
accepted PR192 private-image scheduling fix. Previous run is fully collected
and listening confirmed. Prepare a separate attended Air6,1 profile: launch Help
with the complete window/dock/Text checklist, automatically run the same bounded
continuous PCM and verified filesystem workload, and leave normal app interaction
to the user. Keep desktop open at workload completion so unsaved edits can be
resolved normally. Do not automatically close user-owned windows or claim the
automatic app-cycle requirement was exercised in this separate attended profile.

Scope: explicit diagnostic profile routing, compositor workload lifecycle and
Help content, staging/refusal and VM regression coverage, and this unit record.
The original unattended workload and all existing limits/integrity/firmware gates
remain unchanged. PR182 owns storage hardening and PR19 Wi-Fi; avoid their
implementation scope. Next checkpoint: matched build/core, window/dock and loaded
VM tests, exact-stage native refusal and verified recovery, then attended native
trial with guest instructions and durable return collection. User’s existing
self-merge/install/reboot authorization applies; no new reboot is armed yet.

The loaded attended VM exposed a 1.535 s loop at periodic diagnostic emission;
targeted timing reproduced 1.749 s in report output, not the nonblocking player
wait. Extend this trial's scope to one immutable diagnostic snapshot on the
existing lifecycle worker. The UI retries a busy fixed slot without overwriting
it; shutdown joins/drains before the final synchronous receipt. Keep every loop
sample, limit violation and saved-record integrity check. This affects diagnostic
report delivery in both explicit stress modes, not ordinary desktop policy.
PR19's shared boot/staging files contain separate Wi-Fi branches and a distinct
profile namespace; no competing desktop implementation found.

Integration refresh found PR198's keyboard-navigation scope and its new Help
ID21 and profile v76. Integrate merged PR198 baseline `0c63e679`; reserve
ID22 and profile v77 for this separate attended-load checklist. Keep the Air7,2
navigation profile distinct from this Air6,1 trial. Include held MRU switching,
cancellation, same-app cycling and keyboard menus in this guest checklist. Exact-stage rehearsal sends the console unmount command and exits the
VM after refusal; it does not claim automatic native return or native acceptance.

### Attended-load candidate validated (2026-10-26)

Candidate `7b650985` integrates PR198 (`0c63e679`). Matched regular/native/fixture kernels and userspace, `test-core`, `test-desktop`, and 83 staging/profile host checks pass. The loaded attended Q35 fixture at 400 write IOPS completes 60 seconds of PCM with zero underruns and 112 verified bursts (29,360,128 bytes). Its worst loop is 209.724 ms across 1,010 samples, with no over-limit/invalid measurements. It verifies exact Text bytes, completion without forced exit, unsaved Cancel, normal retirement, all 66 prior files and clean allocation. The early-exit fixture correctly refuses completion. Window/dock and navigation VMs restore their process baseline and pass; these synthetic results do not accept physical keyboard/trackpad behavior.

The unchanged automatic populated-Home stress fixture also passes: 20 app cycles, 116 verified bursts, zero underruns/errors, worst loop 339.537 ms, readiness 1.291386 s and close 1.500975 s. Exact v77 payload `ab8cbaa369d74d28` passes the UEFI native-profile refusal rehearsal with console unmount; native evidence remains pending. The complete guest checklist includes fresh Text files/notes, two screenshots, the full 30-minute load, normal exit and one ordinary `desktop battery` reopen. The local run record retains USB baseline, recovery, collector and one-shot authorization; accepted payload/evidence backups remain outside `build`.

### Attended-load return: screenshot/interaction failure (2026-10-26)

Release `ab8cbaa369d74d28`, compiled source `7b650985`, session
`session-00065ec77837abe9-00` is **failed/incomplete**, not accepted. The user
reports generally good interaction until a screenshot took about 2–3 minutes to
appear/save, followed by inability to launch applications or exit the desktop.
Pointer motion and menus remained responsive. There is no exit snapshot or
confirmed battery retirement/reopen. Do not infer a whole-desktop scheduler stall
from these symptoms or promote the trial from its audio result.

The independent workload receipt completes 1,800 seconds of PCM, 345,600,000
bytes, zero recorded underruns and worker status zero. Its 2,947 verified disk
bursts total 772,538,368 bytes; the binary receipt matches. A 1366×768 screenshot
(4,196,406 bytes) decodes successfully, and `load-notes.txt` survives. All 398
prior files match except expected `.srvros/meta` bookkeeping. Offline fsck and
allocation checks are clean: 405 files, 1,117 clusters, zero leaks/mismatches/errors.
The collector preserves the failed run, disables itself and confirms the one-shot
entry was consumed. Payload/evidence backups remain outside `build`.

The retained periodic loop records report a 111.218 ms maximum and no over-limit
samples, but no final loop receipt exists. The rolling logs no longer contain
the screenshot and failed launch/exit events, so they cannot establish which
request or worker remained pending. This is a gap in functional diagnostics,
not evidence that the reported failure did not happen.

A local disposable Q35 extension takes a screenshot during the existing
60-second populated-Home workload at 400 write IOPS, then opens Files and requests
normal exit with unsaved Text/Cancel/save. Screenshot completion takes 45.974 s;
Files launch and cooperative exit work. The run nevertheless fails the unchanged
500 ms limit with one 539.680 ms loop, including 533.053 ms rendering; clients
exit with status zero and battery ownership retires. It does not reproduce the
native launch/exit failure or establish its cause. An initial exploratory helper
was interrupted after it waited for an already-consumed completion marker; that
harness error is retained separately, not counted as an OS failure or pass.

Next bounded scope: screenshot transport/save/verification timing and persistent
pending launch/close/capture state, with automatic screenshot → app launch/close
→ normal exit/reopen under audio/disk load. Preserve immutable capture data,
readback/identity/flush checks, accepted-writer lifetime and all existing limits.
Do not weaken screenshot verification or add a timeout that kills an active file
writer. No new payload is staged or reboot armed after this return.

### Automatic screenshot stress — active scope (2026-10-26)

Base `a9d69554`; prior failed attended run is collected and backed up. Add
automatic screenshots to the explicit audio/disk/app stress profile and retain
screenshot transport/save phases, pending app launches and cooperative-exit state
in periodic and final diagnostics. Scope: shotworker/capture protocol and host
fixtures, compositor diagnostic snapshots/trial sequencing, VM collector and this
unit record. Keep 500 ms loop, 5 s client, 10 s app-cycle, playback/owner deadlines
and all file-integrity/writer-lifetime rules unchanged. PR182 owns exFAT/storage
hardening; PR19 owns Wi-Fi. No competing screenshot implementation is in the
current integration baseline/open PR list. Next checkpoint: matched build, host
fixtures, loaded screenshot/app/exit VM and exact-stage refusal, self-merge and
automatic native trial under existing explicit reboot authorization.

### Automatic screenshot stress candidate (2026-10-26)

Candidate `a496fd64` integrates PR201 baseline `5db81dd5` (cooperative
window close and root task discovery). The explicit v74 automatic stress now
requests three screenshots during native playback, retains current and slowest
capture phase timings plus pending app-launch/close/exit state, and checks fresh
Files launch/close and cooperative exit after playback. The short VM takes one
screenshot. Diagnostic output has a separate joined worker and immutable fixed
snapshots; ordinary desktop policy, hardware gates, accepted-writer lifetime,
file verification and all existing performance/deadline limits are unchanged.

Host capture protocol/save and operation snapshot regressions pass, including
fragmented progress, malformed frames, immutable reporting and incomplete-log
refusal. The earlier full core suite and ordinary capture/attended VMs pass;
the integrated matched build and desktop suite pass. VM app evidence combines
complete console lines and atomic saved snapshots for both cycle and phase
receipts under strict
phase/order checks; interleaved fragments are discarded. No log limit was raised.

Native screenshot delay and the failed launch/exit remain unaccepted. These
changes collect evidence and automate reproduction, not a claimed root-cause
fix. The 30-minute trial requires no input beyond listening; keep AC and USB
connected. It must retain three decoded screenshots, complete original workload
and lifecycle proof, normal exit/battery retirement, preserved prior files and
clean allocation before acceptance. The local run record retains exact source,
payload, recovery, collector and one-shot authorization, with durable backups
outside `build`.

The integrated loaded VM at 400 write IOPS passes 60 seconds with 115 verified
bursts (30,146,560 bytes), 19 app cycles, zero underruns/errors, worst loop
295.123 ms, ready 1.415554 s and close 1.356357 s. The screenshot takes
53.219710 s, including 49.307799 s receiving pixels; subsequent Files launch/close
takes 1.164776 s and normal exit completes with empty pending queues. All 66
prior files survive and allocation is clean. An interleaved console cycle record
caused one harness failure; whole-record snapshot recovery and regressions fix
collection without hiding missing/conflicting receipts. That attempt is retained.
The integrated window-command VM also passes cooperative close, modal behavior,
exact files and process-baseline checks. Exact native payload `7eeee7846ae4eedc`
passes Q35 refusal, saved logs, unmount and automatic return. Native acceptance
remains pending run `bfaa7ba0c0774d01`.

### Screenshot stress return and transfer fix — active scope (2026-10-26)

Release `7eeee7846ae4eedc`, session `session-00065ec8ae946d1a-00`, completes
30 minutes with three verified screenshots, 674 app cycles, 2,897 verified disk
bursts (759,431,168 bytes), zero recorded underruns, normal exit and battery
retirement. All prior files survive except allowed bookkeeping/preferences;
offline allocation is clean. Audible quality remains unconfirmed. The prior
launch/exit failure does not reproduce, but screenshot latency does: slowest
capture 202.486889 s, including 117.302066 s receiving pixels. This is not
acceptance of capture responsiveness.

Base `eacb4ec4` includes PR200 diagnostics. Trace bounded screenshot pipe
transfers and scheduling under storage load; retain immutable snapshots, rights
and signal cleanup exclusions, same-process I/O exclusion, exact writes/readback,
identity/flush checks and all existing limits. Scope: bounded pipe syscall/process
admission, screenshot worker transport, host regressions, loaded VM and this unit
record. PR182 touches process filesystem operations, not the pipe transfer path;
PR19 remains Wi-Fi. Publish the narrow change and validate matched builds,
core/desktop and exact native-stage VM before self-merge and an automatic native
rerun under the user's existing explicit authorization. No new reboot is armed.

### Bounded screenshot transfer candidate validated (2026-10-26)

Source `69226122` retains the existing 4096-byte atomic pipe bound and four-chunk
UI poll budget. The kernel admits only nonblocking byte transfers with no rights
release or broken-pipe signal; cleanup, PTY, large/blocking request, user-memory,
same-process and retirement exclusions remain. The screenshot worker uses bounded
nonblocking requests and readiness waits. No integrity checks or limits changed.

Matched builds, the full core/desktop host suite, targeted admission/transport
regressions, ordinary screenshot VM (exact immutable pixels, durable readback,
notification/Preview and clean exit) and pipe VM (100,000-line persisted checksum
and early-reader exit) pass. The loaded 400-write-IOPS VM passes 112 verified bursts,
14 app cycles and zero audio underruns/errors. Worst loop is 172.611 ms, ready
1.443386 s, close 5.907184 s and post-capture Files cycle 0.806321 s; all existing
limits pass. Capture total improves from 53.219710 to 38.075712 s; pixel receipt
improves from 49.307799 to 7.681562 s. Both verification passes remain enabled and
now occur during active disk load. All 66 prior files survive; allocation is clean.

Exact native payload `39d73c7da8c8ab69` passes Q35 hardware refusal, log save,
unmount and automatic return. The automatic native rerun is `d0a0deb29df04c74`,
with three captures over 30 minutes and the unchanged audio/disk/app workload.
Its record retains the 414-file USB baseline, original recovery and durable
payload/evidence backup. Native latency improvement remains pending that return;
the older inability to launch/exit is not claimed fixed by this VM result.

### Screenshot transfer native return (2026-10-27)

Release `39d73c7da8c8ab69`, source `69226122`, session
`session-00065ec9d904ac27-00` substantially improves capture latency but **fails
performance acceptance**. All three 1366×768 BMPs decode. Slowest capture falls
from 202.486889 to 39.944696 s; retained captures 2/3 show pixel receipt of
8.627094/10.959870 s, versus 117.302066 s in the prior slowest capture. Current
capture 3 completes in 36.269593 s. Verification remains enabled.

The full 1,800-second workload completes 2,831 verified bursts (742,129,664 bytes),
345,600,000 PCM bytes and zero recorded underruns; the user confirms steady sound.
There are 602 app cycles, no child errors and a successful post-capture Files
cycle (0.721493 s). Cooperative exit is requested, final operation queues/windows
are empty and battery runtime retires with heap zero and no errors/denials.
The desktop and outer controls correctly propagate a failed test result.

One of 104,634 measured loops takes 660.272 ms, exceeding the unchanged 500 ms
limit. It occurs about 33.6 s after boot; 655.438 ms is in post-render handling,
which includes capture polling/completion. Rendering is 4 us and final wait is
4.773 ms, so the evidence does not attribute it to drawing or final wake delay.
Exact capture suboperation is not retained: completion/descriptor cleanup and
snapshot release require targeted investigation, not a claimed diagnosis.
Readiness/close maxima are 2.161654/6.160328 s under the original cycle policy.

All 414 prior files survive except expected `.srvros/meta` bookkeeping. Offline
fsck/allocation are clean: 423 files, 1,331 clusters, zero leaks/mismatches/errors.
Evidence and decoded-image checks are backed up outside `build`; the collector
is disabled and the one-shot entry consumed. No new boot is armed. Preserve
all limits and accepted-writer/file-integrity rules in the next investigation.

### Screenshot cleanup — active scope (2026-10-27)

Base `6b6d2f4d`; prior run is collected, backed up and failed on one 660 ms
post-render loop. Capture polling currently closes descriptors and unmaps its
snapshot synchronously on completion. Move completion resource ownership to the
existing joined lifecycle worker, preserving immutable data, final acknowledgement,
accepted-writer lifetime, publication order and all limits. Add retained cleanup
timing and delayed-cleanup regressions; validate ordinary capture and loaded
stress before the next automatic native trial. Scope: capture job library,
compositor worker/publication, diagnostics/host and VM collectors. PR182 filesystem
hardening and PR19 Wi-Fi remain separate.

The user also requests 1% test audio for the next few hours. Add an expiring,
explicit stress-only volume override in this unit's test input, retaining ordinary
audio defaults and the full PCM workload. Record its expiration with the local
run, verify it in the guest and keep AC/USB connected during automatic testing.
Self-merge and reboot are explicitly authorized; no new boot is armed yet.

### Screenshot cleanup — candidate validated (2026-10-27)

PR204 source `306a8ff4` moves completion pipe close, snapshot unmap and child
retirement to the joined lifecycle worker. Capture remains busy until that owner
publishes full retirement. Final acknowledgement, immutable pixels, accepted
writes, bounded protocol chunks and all performance/owner limits remain intact.
Retained cleanup records separate close/unmap/reap execution time. Host regression
covers delayed retirement, single ownership and failure publication.

Full host/core, matched kernels and documentation checks pass. Populated Home
at 400 write IOPS passes 60 seconds of full PCM at verified 1% volume, 100 verified
disk bursts, 15 app cycles, automatic screenshot/follow-up Files/normal exit,
zero underruns and all 67 prior fixture files. Worst loop is 462.262 ms against
the unchanged 500 ms bound. Ordinary capture VM passes immutable pixels during
typing, durable readback, Preview, repeated capture and clean exit. Exact v74
stage `d7fe87e92eff81b3` passes chainload and expected Q35 native-profile refusal;
it does not establish native battery or cleanup acceptance.

Next native run is 30 minutes of PCM, unpaced verified disk work, repeated app
cycles and three automatic screenshots, then normal desktop exit and battery
retirement. The explicit volume input requests 1% until 2026-10-27 08:30:45 UTC;
playback receipt records actual volume. All 423 prior USB files were preserved
when adding this one input; baseline is 424 files / 1,332 clusters with no leaks,
allocation mismatches or errors. Ubuntu and console recovery remain unchanged.
Native results must be collected before another trial; acceptance is pending.

### Screenshot cleanup — native return failed early (2026-10-27)

Release `d7fe87e92eff81b3`, compiled source `306a8ff4`, merged PR204
`6cb4eaeb`, returned one matching session `session-00065ecbd4ad7e4a-00`.
The full stress trial is **failed**, not accepted: Files cycle 7 exceeded the
unchanged ten-second total cycle deadline at 10.009730 s. Its window had become
ready in 1.677275 s; close was pending on an active directory listing. That
listing reported success only during failure shutdown. This occurs before normal
screenshot completion/resource cleanup, so it does not identify the new cleanup
worker as the cause. Next investigation is directory-list completion/storage
contention during screenshot writes, coordinating any filesystem changes with
PR182; no limits or integrity checks should be relaxed.

The 2,426 measured loops have a maximum of 109.796 ms, zero invalid samples and
zero excursions above 500 ms. This short run cannot accept full endurance. Audio
was verified at 1% with zero recorded underruns, but playback stopped early and
neither workload completion receipt exists. One screenshot was requested and
zero were acknowledged before abort. Its accepted writer drained and retired;
one complete 1366x768 BMP (4,196,406 bytes) decodes successfully afterward. Abort
cleanup execution totals 111.687 ms (close 101.488, unmap 1.033, reap 9.154 ms),
excluding waiting between reap attempts. Normal completion remains unverified.

Battery retirement reports heap/errors/denied all zero, though the enclosing
controls trial correctly reports FAIL. All 424 preboot files survive except
expected `.srvros/meta` bookkeeping. Offline fsck/allocation pass: 430 files,
1,373 clusters, zero leaks/mismatches/errors. Returned evidence is hash-verified
outside `build`. The collector is disabled, one-shot consumed and no new reboot
armed. The 1% stress input retains its recorded three-hour expiration.

### Files listing under screenshot writes — active scope (2026-10-27)

Base `d4d9c17c`; the failed PR204 return is collected and backed up. Objective:
remove repeated storage-admission waits from Files enumeration by returning
bounded directory entries with their metadata in one quiescent syscall batch.
Keep storage exclusion, shared directory cookies, live open-file sizes, path and
output validation, file-count limits and all stress deadlines unchanged. Scope:
directory syscall/ABI, process enumeration, deskfs listing and regression/VM tests.
PR182 owns exFAT/block/VFS/writeback auditing; this change does not edit those
backend contracts or its process writeback functions. PR19 retains Wi-Fi scope.
Validate full host tests and loaded/exact-stage VMs, merge and run the unchanged
30-minute native stress with explicit 1% audio under the user's authorization.

### Files metadata batches — candidate validated (2026-10-27)

PR206 runtime source `7c1ef57a` adds sixteen-entry name/metadata batches behind
the existing storage gate. Files no longer reacquires that gate for every name,
path resolution and stat. Shared directory cookies, live open-file sizes and
user-buffer checks remain intact; no exFAT/backend mutation, scheduler admission,
file-count bound or stress deadline changes. Legacy directory operations retain
their interface. Original Files source fails the bounded-batch regression.

Full host/core and matched standard/Air6,1 builds pass. Directed host tests cover
metadata, invalid capacities, failed/malformed replies, shared positions and the
unchanged 256-entry truncation. The real directory VM passes pointer and handle
refusals plus metadata/cookie checks. Populated Home at 400 write IOPS passes
60 seconds of PCM at 1%, 108 verified disk bursts, 18 app cycles, automatic
screenshot/retirement, follow-up Files and normal exit. Worst loop is 117.417 ms,
readiness 1.436334 s and close 1.840253 s, all within existing limits; zero audio
underruns/app errors and all 67 prior fixture files preserved. Ordinary capture
VM passes immutable pixels, durable readback, folder/Preview, repeat and exit.

Exact v74 stage `012aaffda0be860b` passes chainload, expected Q35 native-profile
refusal and automatic return. This does not accept the native workload. Next is
the unchanged 30-minute / three-screenshot stress with automatic app cycles and
unpaced verified disk work, explicit 1% volume and automatic collection. USB
baseline remains 430 files / 1,373 clusters, all prior hashes matched and no
leaks/mismatches/errors. The quiet input remains valid until 08:30:45 UTC on
2026-10-27. Ubuntu and verified console recovery remain the fallback.

### Files metadata batches — bounded native acceptance (2026-10-27)

Release `012aaffda0be860b`, runtime source `7c1ef57a`, merged PR206 `e7953352`,
completed session `session-00065ecc2a71b33f-00` with an automated **PASS**.
All 1,800 seconds of PCM completed at verified 1% volume: 345,600,000 bytes,
zero recorded underruns and worker status zero. The user confirms steady sound
throughout this quiet run.
Unpaced disk work completed 3,266 verified bursts / 856,162,304 bytes, with
sustained live-playback overlap and matching binary/text receipts.

The session completed 703 application cycles with zero errors. Maximum readiness
is 1.722836 s and close 4.799946 s; the original ten-second cycle bound passes.
All 95,841 measured desktop loops are valid, with zero excursions over the
unchanged 500 ms limit; maximum is 311.305 ms. This accepts the bounded Files
listing and screenshot cleanup coexistence trial, not unlimited endurance or
instantaneous UI response under all loads.

All three automatic screenshots have successful acknowledgements, full worker
retirement and decodable 1366x768 BMPs (4,196,406 bytes each). Slowest completion
is 22.715026 s, compared with 39.944696 s in the previous completed-capture
reference release `39d73c7da8c8ab69`; that reference still failed its loop bound.
Retained cleanup execution is 73.704 ms for the slowest capture and 71.626 ms
for the final capture. Follow-up Files completes in 711.504 ms, then normal exit
leaves no queued launches, live windows or capture owner. Battery retirement
reports heap/errors/denied zero and enclosing controls PASS.

All 430 preboot files survive except expected `.srvros/meta` bookkeeping.
Offline fsck/allocation pass with 439 files, 1,480 clusters and zero leaks,
mismatches or errors. The matching evidence, images and accepted payload are
hash-verified outside `build`. Ubuntu remains the default, console recovery is
preserved, the collector is disabled and no reboot is armed. The next step can
return to broader attended desktop use; this workload needs no relaxed limits.

### Read-only CPU power telemetry — active scope (2026-10-27)

Base `4dbd566f` includes the collected Files/screenshot acceptance above. The
next bounded objective is explicit CPU package energy and CPU0 active-frequency
telemetry for this unit, followed by the existing automatic v35 30-second
idle/load/recovery phases. Linux CPU0 capture confirms signature `0x40651`,
CPUID.1 ECX/EDX `0x7ffafbbf`/`0xbfebfbff`, CPUID.6 ECX `9`, and readable
RAPL units/platform/energy/APERF/MPERF. Raw evidence stays in ignored local storage.

Touched scope: power profile gate, its arithmetic/service regressions, power
trial documentation and local staging/collection. Preserve the five-register
read-only allowlist, feature/hypervisor refusals, thermal stops and all timing
and integrity bounds. No CPU policy, fan control or ordinary desktop opt-in
changes. Reuse the existing v35 diagnostic; no battery/AML ownership is needed.
PR182 owns filesystem/backend writeback auditing and PR19 USB Wi-Fi; neither
has overlapping power scope. Native power acceptance remains pending. Keep
Ubuntu, console recovery and the accepted combined desktop payload available.

### Read-only CPU power telemetry — candidate ready (2026-10-27)

PR208 runtime source `932f7b50` admits only the independently captured 06_45
profile alongside existing 06_3D/06_46 support. Units are `0xa0e03`; platform
`0x8083df3011400` supplies the 2,000 MHz reference. The Linux development-load
fixture decodes to 6,610 mW package power / 834 MHz active frequency; it is not
native idle evidence. Five read-only MSRs, CPU feature/hypervisor checks and
sampling/thermal bounds remain unchanged. The new regression fails against the
original model gate and passes with the profile.

Full host/core, matched standard build, documentation, load safety and collector
tests pass. Power and load VMs verify default-disabled and unsupported-hardware
refusal. Exact v35 release `22632f396dab67a4` passes GPT workspace save/unmount
and automatic return; missing workspace halts without reboot. VMs cannot accept
native measurements. The prepared physical run has three automatic 30-second
idle/load/recovery phases, visible progress and automatic Ubuntu collection.
Acceptance requires valid power coverage in all three phases as well as workload
completion, original-file preservation and clean offline allocation.

USB preflight matches the prior 439 files / 1,480 clusters without leaks or
mismatches. Verified payload and evidence backups are outside `build`; Ubuntu,
console recovery and accepted combined desktop remain available. No input or
listening is required; leave AC and USB connected. Native acceptance is pending.

### Read-only CPU power telemetry — first native result (2026-10-27)

Release `22632f396dab67a4`, runtime `932f7b50`, merged PR208 `c07dc402`,
saved session `session-00065ed46d470f31-00` and returned automatically to Ubuntu.
All three 30-second phases completed; load executed 250,652 integer-work chunks.
The power provider reports the captured units/reference, both validity flags
and zero rejected samples throughout the measured checkpoints. Thermal/fan
checks completed without a stop; peak logged core/package temperatures are
61/63 C and fan observations span 1,171–1,234 RPM.

Covered load power is 5.134 W over 10.999826 seconds with CPU0 active frequency
1,300 MHz. Recovery is 4.127 W over 20.999667 seconds and 1,288 MHz. These are
short console-workload package measurements, not whole-machine power or savings.
The complete three-phase telemetry trial remains **incomplete**: the two eligible
idle checkpoints span 9.999841 seconds, 159 microseconds below the unchanged
ten-second coverage requirement. The next checkpoint was logged after idle
ended and is correctly excluded. No missing data or hardware error is inferred
from this coverage refusal; do not round it into a pass or weaken the threshold.
Next work should make phase checkpoints reliably cover the existing window.

All 439 prior files are preserved except expected `.srvros/meta` bookkeeping.
Four new logs bring the volume to 443 files / 1,485 clusters; offline fsck and
allocation checks pass with zero leaks, mismatches or errors. Collected evidence
and the payload are verified outside `build`. The collector is disabled, the
one-shot boot entry is consumed and no further reboot is armed. Existing accepted
desktop/recovery payloads remain the recovery reference.

### CPU power phase logging — active scope (2026-10-27)

Base `99241a70` contains the collected incomplete v35 run. The bounded objective
is to retain extra cached power checkpoints inside each unchanged 30-second
phase so idle has more than the unchanged ten seconds of required coverage.
Touch the explicit power diagnostic checkpoint path, v35 main wiring, service
and collector regressions, and this unit's trial documentation. Keep ordinary
power sampling and USB logging cadence unchanged; retain a bounded in-memory
capture and emit it before the final persisted report. No new MSR reads/writes,
CPU policies, sensor gates or acceptance-limit changes. PR182 filesystem/backend
work and PR19 Wi-Fi have no overlapping power scope. After host/VM validation,
merge and repeat the automatic native test under the user's reboot authorization.

### CPU power phase logging — repeat candidate ready (2026-10-27)

PR210 runtime `e7a41714` retains up to 24 already-sampled power snapshots in
static memory at intervals of at least five seconds, only during v35's explicit
load diagnostic. It prints their original sample timestamps after stopping
retention and before the final workspace save. No extra MSR reads, allocations,
disk writes or wakeups; sampling/USB cadence and all duration, thermal and
collector limits remain unchanged. The collector refuses malformed, truncated
or overflowed retained captures and keeps the ten-second per-phase requirement.

Full host/core, standard matched build and docs pass. Driver regressions cover
sampling drift across all phases, no extra reads, capacity/stop/restart and
refused CPUs. Collector regressions reproduce the exact native shortfall and
show added checkpoints supply valid coverage; the original parser fails both
new coverage cases. CPU-load safety fixtures retain all twelve refusal/stop
paths. Power/load disabled and unsupported-hardware VMs pass.

Exact v35 release `a5f8fdf5d377c116` passes GPT workspace save/unmount/automatic
return and missing-workspace halt rehearsals. The repeat native trial keeps
30 seconds each idle/load/recovery with visible progress and automatic Ubuntu
collection. USB baseline is 443 files / 1,485 clusters with clean allocation.
The older stress payload's EFI copy was retired only after durable hash
verification; latest accepted stress `012aaffda0be860b`, console recovery and
current installed payload remain protected. Native repeat acceptance is pending.

### CPU power phase logging — bounded native acceptance (2026-10-27)

Release `a5f8fdf5d377c116`, runtime `e7a41714`, merged PR210 `e283eecb`,
completed session `session-00065ed4d4558b8e-00` and returned automatically to
Ubuntu. All three 30-second phases completed, including 250,682 load chunks.
Seventeen retained checkpoints have both validity flags, zero rejected samples
and no overflow. The full final log fits within the unchanged 64 KiB tail.

| Phase | Covered interval | Checkpoints | Package average | CPU0 active frequency |
| --- | --- | --- | --- | --- |
| Idle | 20.000003 s | 7 | 4.128 W | 1,288 MHz |
| Single-worker load | 25.000003 s | 7 | 5.161 W | 1,299 MHz |
| Recovery | 25.000004 s | 9 | 4.107 W | 1,290 MHz |

Coverage combines retained and ordinary workspace checkpoints inside each phase.
Every phase satisfies the unchanged ten-second minimum. Peak logged core/package
temperatures are 64/66 C; the fan spans 1,938–2,589 RPM. No diagnostic thermal
stop occurred. This accepts bounded read-only package energy and CPU0 active
frequency, plus the phase logging fix, on this unit. It does not accept CPU/fan
policy changes, equilibrium power, battery-life predictions, multi-core stress
or savings against earlier differently instrumented trials. Ordinary desktop
power telemetry remains opt-in and is not enabled by ordinary staging.

All 443 baseline files are preserved except expected `.srvros/meta` bookkeeping.
Four new logs bring the USB to 447 files / 1,490 clusters. Offline fsck/allocation
pass with zero leaks, mismatches or errors. Matching evidence and the payload
are hash-verified outside `build`. The collector is disabled and the one-shot
entry consumed; no reboot is armed. Ubuntu and console recovery remain intact.

### Desktop power phases — active scope (2026-10-27)

Base `e62c1b2a`; bounded objective is one automatic battery-owned desktop lifetime
with 30 seconds settling, 60 seconds idle, 60 seconds Text/Files/System app
cycles, then 60 seconds recovery and clean retirement/Ubuntu return. Measure
package power, temperatures and CPU0 worker wakeups separately by phase. Keep
existing app deadlines, telemetry validity, firmware ownership and integrity
checks. No CPU/fan control writes or ordinary startup changes.

Scope: an explicit Air6,1 desktop staging profile, compositor diagnostic phases,
battery session selection, bounded preservation of periodic log checkpoints,
strict phase collection and host/VM regression harnesses. Normal logger cadence
and rotating snapshots stay unchanged; only this diagnostic reserves extra file
slots for at most 32 checkpoints. PR182 owns filesystem/backend auditing and
PR19 Wi-Fi; neither overlaps these diagnostic callers. Validate positive battery
VM and exact-stage hardware refusal, merge and run automatically under the
user's existing authorization. Native combined power acceptance is pending.

### Automatic desktop power candidate — prepared (2026-10-27)

PR212 prepares release `1d1cbd1cb03d3032`, runtime source `7a9cfeaf`, for the
30-second settle / 60-second idle / 60-second app workload / 60-second recovery
trial above. Full host core, matched standard/Air6,1/fixture builds, USB image,
profile/collector regressions and documentation checks pass. The complete Q35
TCG fixture passes 12 Text/Files/System cycles, with maximum desktop loop
161,585 us, readiness 533,205 us and close 430,994 us. Phase worker coverage is
5–6 checkpoints over 40.26/50.36/50.79 seconds. Fixture files are preserved and
offline allocation is clean. The exact staged EFI payload explicitly refuses
Q35 at the native PCI/ownership gate, then unmounts and returns normally.
No VM energy or temperature values are treated as native observations.

An initial verbose-UI TCG rehearsal failed at a 513,336 us loop; exploratory KVM
also faulted on a battery query deadline. Both remain failed evidence. The power
profile now uses ordinary desktop UI instrumentation, retaining phase receipts
and periodic telemetry without extra verbose resource tracing. The subsequent
complete TCG run passes the same 500 ms loop and 10 s app limits; KVM is not
acceptance evidence. The final harness additionally checks explicit refusal
messages, without changing the staged runtime.

Run `feba3c750d2a4a8d` retains the 447-file / 1,490-cluster USB baseline, UUID
`6ADA-85BB`, console recovery `c35475f62f154f1e`, accepted CPU-power payload
`a5f8fdf5d377c116` and accepted stress payload `012aaffda0be860b`. Only the
superseded incomplete CPU capture's EFI copy `22632f396dab67a4` was retired after
verifying its durable backup. The new payload and run evidence are backed up
outside `build`; automatic Ubuntu collection is prepared. Keep AC/USB attached,
leave input alone, and collect matching phase measurements before acceptance.
Native combined desktop power, temperature and worker-wakeup results are pending.

### Automatic desktop power phases accepted (2026-10-27)

Documentation-only follow-up on integration `1434c7dc`: collect and record the
completed PR212 trial in this unit record, the machine index and power guide.
No shared runtime changes or dependencies on open PR182/PR19 are introduced.

Run `feba3c750d2a4a8d`, release `1d1cbd1cb03d3032`, runtime `7a9cfeaf`, saved
session `session-00065ed629188a57-00` passes all three native phases. The desktop
completed 13 automated Text/Files/System open/close cycles and exited normally.
Maximum UI loop was 115,440 us (500,000 us limit); maximum app readiness was
704,385 us and close was 51,434 us (unchanged 10 s lifecycle limit). Battery
monitoring remained fresh, advancing generations 12→35→59→82. Retirement reports
82 attempts, zero rejected samples, heap zero, errors zero and denied zero;
client ownership was revoked and the runtime retired cleanly.

| Phase | Power coverage | Points | Mean CPU package power | CPU0 active frequency | Core / package temperatures |
| --- | --- | --- | --- | --- | --- |
| Idle | 50.998362 s | 6 | 4.131 W | 1,292 MHz | 59 / 59 C |
| App workload | 51.998342 s | 6 | 4.150 W | 1,294 MHz | 57–59 / 58–59 C |
| Recovery | 40.998694 s | 5 | 4.137 W | 1,292 MHz | 57–59 / 58–59 C |

Each phase lasted approximately 60.06 s. Worker snapshots separately cover
51.624 / 51.528 / 40.449 s. Idle timeout-wakeup rates include `xhci-hid` 90.133/s,
`uthread` identity 17 at 89.436/s, `bootstrap` 48.350/s and `battery-owner`
18.964/s. Recovery rates remain similar. App-phase partial worker lifetimes are
reported separately, not attributed as complete counters. These are software
wakeups, not hardware residency or energy attribution. The 19 mW app/idle
package difference does not establish a meaningful power change from one short
light-workload trial. A useful next step is auditing periodic desktop/lifetime
worker waits and repeating this baseline under unchanged correctness limits;
no CPU/fan policy or sleep/resume acceptance follows from these measurements.

All 447 prior files are preserved except expected `.srvros/meta` bookkeeping;
23 new log files bring the USB to 470 files / 1,514 clusters. Offline fsck and
allocation pass with zero leaks, bitmap mismatches or errors. The independent
saved-record parser agrees with automatic collection. All 353 collected durable
evidence hashes and the installed/backup payloads were verified. Ubuntu returned,
the one-shot entry is consumed and the collector is disabled. No reboot is armed.
The accepted payload/evidence is retained outside `build`; native bounded desktop
power/temperature/worker telemetry is accepted on this unit, with ordinary
power telemetry still opt-in and broader endurance/power policy still pending.

### Idle worker polling reduction — active scope (2026-10-27)

Base `fbb6eaa5`, accepted desktop-power run `feba3c750d2a4a8d`. Audit identifies
one-tick unconditional sleeps in the desktop lifecycle and diagnostic-report
workers. Replace only their empty-queue polling with latched futex notifications;
cover launch, allocation, discard, capture cleanup, report publication and stop.
Keep the existing one-tick retry while a screenshot child is still retiring,
resource ownership/credit rules, joins, queue bounds and every deadline unchanged.
The native bootstrap loop also checks workspace identity, control requests and
watchdogs; its polling is deferred until all those wake sources can be covered.
USB HID polling belongs to separate device work and is unchanged.

Scope: compositor worker notification/admission sites, host race/retirement
regressions, relevant worker documentation, and a repeat of the identical v78
power trial with the 470-file / 1,514-cluster USB baseline. Open PR182 filesystem
and PR19 Wi-Fi work do not own these compositor queues. Run full host tests,
positive power and screenshot/app/disk VMs, exact staged native refusal, merge,
then reboot automatically. Native wakeup/energy improvement remains unproven
until the matching physical return is collected.

### Event-driven desktop workers — validated candidate (2026-10-27)

PR214 candidate runtime `e0008d92`, release `cb881058052a1012`, replaces empty
lifecycle/report polling with separate latched futex waits. Every UI-side work
publication and stop notifies its worker. Capture retirement and interrupted-wait
fallback retain the old one-tick retry; resource credit, child ownership, joins,
firmware guards, queue bounds and all deadlines remain unchanged. Host threaded
race/rollback/retirement tests, the full core suite, matched builds and docs pass.

The complete power VM passes 12 app cycles with a 189,071 us worst UI loop.
Lifecycle worker timeout wakeups are zero in all phases; idle and recovery also
have zero switches/signals, while app work produces 43 signal wakeups over its
covered interval. The concurrent audio/disk/app/screenshot VM passes 60 s, 366
verified disk bursts (95,944,704 bytes), 26 app cycles, one saved screenshot,
follow-up Files launch and clean exit. It records 222 audio/write overlap windows,
zero underruns and a 439,121 us worst UI loop under the unchanged 500,000 us
limit. Both VMs preserve prior files and pass offline allocation. The exact EFI
payload refuses Q35 at the native PCI/resource gate and returns after unmount.

Repeat run `db449f93b4b14340` uses the identical v78 settle/idle/apps/recovery
recipe and native limits, against the accepted `1d1cbd1cb03d3032` baseline.
Its USB baseline remains 470 files / 1,514 clusters, UUID `6ADA-85BB`; Ubuntu and
console recovery `c35475f62f154f1e` remain unchanged. The older automated-reopen
EFI copy `4efd82574f07eb30` was retired only after verifying its durable payload;
later attended-reopen `9851d427b042227b`, accepted stress and both accepted power
payloads remain installed/backed up. Automatic return collection compares power
and worker counters without treating this short comparison as proven energy
savings. Native validation remains pending; no battery-supervisor or USB polling
change is included in this candidate.


### Idle desktop worker waits physically accepted (2026-10-27)

Documentation-only acceptance on integration `8715d684` (PR214); no runtime
changes or dependencies on open PR182/PR19. Run `db449f93b4b14340`, release
`cb881058052a1012`, runtime `e0008d92`, saved matching native session
`session-00065ed6efff4c7c-00`. The unchanged v78 settle/idle/apps/recovery test
passes with 13 completed Text/Files/System cycles and automatic Ubuntu return.
Worst UI loop is 115,492 us against 500,000 us; readiness is 565,144 us and close
83,141 us against the unchanged 10 s lifecycle limits.

Lifecycle worker `uthread` identity 17 has zero timeout wakeups in every phase.
Idle and recovery also have zero switches or signal wakes across their covered
52.208329 s and 51.122871 s intervals. During 40.828704 s of app coverage it has
54 switches and 36 signal wakes (0.882/s), serving work without periodic polling.
The accepted baseline had 89.436 / 87.604 / 89.026 timeout wakes/s respectively.
This accepts the targeted lifecycle wait change on this unit, not removal of all
worker polling. Bootstrap supervision (~48/s), battery ownership (~19/s) and
USB HID (~90/s) remain unchanged and need their own wake-source audits.

| Phase | Power coverage / points | Mean package power, baseline → repeat | CPU0 active MHz | Core / package C |
| --- | --- | --- | --- | --- |
| Idle | 51.998343 s / 6 | 4.131 → 4.121 W | 1,292 | 61–63 / 62–65 |
| Apps | 40.997963 s / 5 | 4.150 → 4.167 W | 1,294 | 60–61 / 61–62 |
| Recovery | 40.998693 s / 5 | 4.137 → 4.130 W | 1,293 | 60–61 / 60–62 |

Power is essentially unchanged in this short comparison; different temperatures
and small differences do not establish energy savings. Battery phase generations
advance 12→35→58→81; retirement reports 82 attempts, zero rejected samples,
heap/errors/denied zero, and revoked/retired ownership. All integrity checks,
queue bounds, screenshot-child retry, firmware gates and deadlines are unchanged.

All 470 prior USB files are preserved except expected `.srvros/meta` bookkeeping.
The 23 new logs bring the filesystem to 493 files / 1,538 clusters; fsck and the
allocation walk pass with zero leaks, bitmap mismatches or errors. Independent
reparsing agrees with the collector and all 381 collected durable evidence hashes
verify. Payload/evidence survive `make distclean`. The one-shot boot is consumed,
the collector is disabled, and no reboot is armed. Console recovery
`c35475f62f154f1e` and the earlier accepted payloads remain retained.


### Battery desktop supervisor wait audit — active scope (2026-10-27)

Base `a3066f87`, accepted idle-worker run `db449f93b4b14340`. Replace only the
ordinary lifetime-owned desktop supervisor's ten-millisecond polling with a
process-exit/control/ownership event wait and the existing absolute diagnostic
deadline. Preserve mount identity validation, mailbox epoch/admission checks,
process reaping and lifetime drain, service ownership/watchdogs and every limit.
Capture/study loops that inspect sample progress retain their existing polling.
Battery-owner sampling and USB HID workers are outside this change.

Scope includes a non-reaping process event wait, control-request notification,
mount-hold release notification, supervisor deadline selection and race/refusal
regressions. Open PR182 touches process/storage files for filesystem auditing;
this change is confined to process wait APIs and mount-hold notification, not
its file I/O, allocation or storage-admission implementation. PR19 owns Wi-Fi.
Validate host/core, complete power/control VMs and exact-stage refusal, merge,
then repeat the unchanged automated v78 power trial. Native improvement remains
pending until matching returned evidence is collected.


### Event-driven battery supervisor — validated candidate (2026-10-27)

Runtime `d1eaeec8` replaces only ordinary lifetime-owned session polling with
process/control/ownership notifications and the original absolute deadline.
The read-only wait shares process ownership predicates without reaping. Accepted
control publication and legal mount-hold invalidation notify after their state
changes. Existing mount immutability, native ownership checks, firmware/IRQ
watchdogs, diagnostic capture loops and retirement limits remain unchanged.

Full core/build tests and the new host ownership/race/deadline/mount tests pass.
At source `13725b56`, the ordinary Home VM passes two actual System Stop/Start
cycles (four dispatched requests, three clean epochs), file persistence and
reopen. The automatic reopen VM passes both 20-second deadlines. The identical
power recipe passes 12 app cycles, 445,159 us worst UI loop, 532,512 us readiness
and 582,337 us close. Bootstrap has zero switches/timeouts/signals during covered
idle and recovery, with 22 event wakes and zero timeouts during app work.

The 60-second audio/disk/app/screenshot VM passes at 1% volume with 378 verified
bursts / 99,090,432 bytes, 26 app cycles, one capture, follow-up Files, normal
exit and zero underruns. Worst loop is 329,981 us, readiness 1,011,293 us and
close 550,023 us. Prior files and offline allocation pass. Two earlier guest
completions lacked complete timing evidence after UART interleaving and disk
rotation; they remain incomplete evidence. Host-only commit `153c75fb` retains
six already-flushed disk checkpoints by identical double reads and matching
generation, without pausing or writing to the guest. All validators and limits
are unchanged, and the passing run supplies complete timing records.

Stage `aa576838270d623a` at `153c75fb` has unchanged guest payload hashes from
those control/deadline tests. Exact GRUB/staged-Limine Q35 rehearsal passes native
profile refusal, unmount and automatic return; it cannot accept native telemetry.
Run `03633a2df1e14db2` repeats v78 against accepted `cb881058052a1012`, with the
493-file / 1,538-cluster baseline and local UUID `6ADA-85BB`. Keep AC/USB connected;
no input is needed. Ubuntu and console recovery `c35475f62f154f1e` are retained.
The superseded original desktop-power EFI copy `1d1cbd1cb03d3032` may be removed
only after verifying its durable backup; the latest accepted power, console,
CPU-power, stress and attended-reopen payloads remain protected. Save/install
verification and automatic collection records before arming. Native supervisor
wakeup/power comparison is pending; no energy savings are claimed.


### Battery supervisor event waits physically accepted (2026-10-27)

Documentation-only acceptance on integration `664c22a3` (PR216); no runtime
changes or dependencies on open PR182/PR19. Run `03633a2df1e14db2`, release
`aa576838270d623a`, staged source `153c75fb` (runtime change `d1eaeec8`), saved
matching session `session-00065ed778e804ca-00`. All unchanged v78 phases pass,
with 13 Text/Files/System cycles, normal retirement and automatic Ubuntu return.
Worst UI loop is 110,061 us (500,000 us limit), app readiness 641,296 us and
close 51,645 us (unchanged 10 s limits).

Bootstrap supervisor identity 1 has zero timer wakeups in every covered phase.
Idle and recovery also have zero switches/signals; app work produces 25 switches
and 25 signal wakes. Worker coverage is 51.843215 / 51.577041 / 40.448709 s for
idle/apps/recovery. The preceding accepted release had approximately 48 timer
wakes/s. Lifecycle worker identity 17 retains zero timer wakes, with no covered
idle/recovery switches. Battery-owner sampling (~19 timer wakes/s), USB HID,
firmware watchdogs, ownership checks and every limit remain unchanged.

| Phase | Power coverage / points | Mean package power, prior → repeat | CPU0 active MHz | Core / package C |
| --- | --- | --- | --- | --- |
| Idle | 50.998372 s / 6 | 4.121 → 4.130 W | 1,292 | 59–61 / 59–61 |
| Apps | 50.998344 s / 6 | 4.167 → 4.140 W | 1,294 | 57–59 / 58–60 |
| Recovery | 40.998694 s / 5 | 4.130 → 4.132 W | 1,292 | 57–59 / 58–59 |

Package power remains about 4.1 W; small differences and different temperatures
in these short runs do not establish energy savings. Battery generations advance
12→35→58→81. Retirement reports 82 attempts, zero rejected samples, heap/errors/
denied zero, balanced attachment, revoked clients and retired runtime ownership.

All 493 prior files are preserved except expected `.srvros/meta` bookkeeping;
23 new logs bring the USB to 516 files / 1,562 clusters. Offline fsck/allocation
pass with zero leaks, bitmap mismatches or errors. Independent saved-log parsing
agrees with automatic collection and all 466 collected durable hashes verify.
Evidence and payload remain outside `build`. Ubuntu returned, the one-shot entry
is consumed, the collector is disabled and no reboot is armed. The superseded
`1d1cbd1cb03d3032` EFI copy was removed only after verifying its durable backup;
console `c35475f62f154f1e`, prior accepted `cb881058052a1012`, CPU-power, stress
and attended-reopen payloads remain retained. This accepts the ordinary session
supervisor wait change on this unit; broader endurance and power policy remain
separate work.

### Read-only idle residency — active scope (2026-10-27)

Base `108969f7` includes the collected supervisor-wait acceptance. Add explicitly
opted-in CPU0 core C3/C6/C7 and package C2/C3/C6/C7/C8/C9/C10 residency reads for
this captured 06_45 CPU, guarded by the existing native power gate and invariant
TSC support. Intel SDM Volume 4 tables 2-20 and 2-31 define the counters; capture
this unit's Linux read-only baseline before admitting them. Other CPU profiles
retain their existing five-register power telemetry, with residency unavailable.

Touch power sampling/arithmetic, bounded capture records, collector regressions
and documentation. Reuse v35's unchanged 30-second idle/load/recovery phases,
24-record capacity, one-second sampling, thermal/fan stops, ten-second phase
coverage and USB integrity checks. No C-state, frequency, fan or battery policy
writes, scheduler changes or new polling. Distinguish unavailable/reset/stale
from valid zero residency; core and package percentages have separate scopes.
PR182 filesystem audit and PR19 Wi-Fi remain independent. Next checkpoint is
host/VM validation, self-merge, exact-stage installation and the authorized
automatic native run; hardware acceptance is pending collection after return.

### Read-only idle residency — candidate ready (2026-10-27)

PR218 runtime `d7cda917` adds the ten captured residency MSRs plus TSC brackets
only on native 06_45 with invariant TSC. Linux CPU0 inventory confirms readable
core/package counters, including valid zeros for package C6–C10. The regression
fixture preserves the measured raw values; it does not assert native acceptance.
Core C3/C6/C7 and package C2/C3/C6/C7/C8/C9/C10 ratios remain separate, with no
inferred C0/C1. Invalid/reset/stale readings are explicitly unavailable.

Full matched standard build, USB image, host/core and documentation checks pass.
Power arithmetic/service regressions cover exact CPU refusal, cached-only reads,
unchanged cadence/capacity, valid zero, counter resets/reserved bits, independent
state totals, overflow and stale status. Eight collector tests pass, including
strict receipt/timestamp pairing and invalid/missing/short coverage refusal;
load fixtures retain all twelve safety refusal/stop paths. Power/load VMs pass
default-disabled and unsupported-hardware refusal.

Exact v35 release `ed06103a5275d7ca` passes GPT save/unmount/automatic return and
missing-workspace halt rehearsals. Run `a8c3c17ccdba458a` will automatically run
30 seconds each idle/load/recovery, save the bounded samples and return to Ubuntu
for collection. Acceptance requires both power and residency coverage of at least
ten seconds per phase, complete workload/return evidence, unchanged prior files
and clean offline allocation. All existing policies, cadence, capacities,
watchdogs, thermal/fan stops and time limits remain unchanged. No input is needed;
leave AC and USB connected. Native residency acceptance is pending return.

USB preflight preserves all 516 baseline files with 1,562 allocated clusters and
zero leaks/mismatches/errors. The superseded `cb881058052a1012` EFI copy was retired
after durable hash verification; accepted current desktop `aa576838270d623a`,
console recovery, accepted CPU power, stress and attended-reopen payloads remain
protected. Source, exact-stage receipts, payload and run/collection records are
retained outside `build` before reboot; Ubuntu remains the default OS.

### Read-only idle residency — bounded native acceptance (2026-10-27)

Return closeout from base `f516c8ea` is documentation/evidence only; no runtime
policy or new trial. PR182 filesystem and PR19 Wi-Fi scopes remain independent.
Release `ed06103a5275d7ca`, runtime `d7cda917`, merged PR218 `f516c8ea`, completed
run `a8c3c17ccdba458a`, session `session-00065ed874cb714b-00`, and automatically
returned to Ubuntu. The three ordered 30-second phases completed, with 250,478
single-worker load chunks and peak logged temperature 67 C. Fifteen retained
records have full support/valid masks, zero rejected samples and no overflow.
The 63,866-byte exit log retains the complete required evidence within the
unchanged 64 KiB tail. Independent raw-record review agrees with the collector.

| Phase | Residency coverage | Residency checkpoints | Package average | CPU0 active frequency |
| --- | --- | --- | --- | --- |
| Idle | 23.999236 s | 5 | 4.130 W | 1,287 MHz |
| Load | 23.999235 s | 5 | 5.167 W | 1,299 MHz |
| Recovery | 23.999234 s | 5 | 4.137 W | 1,290 MHz |

Power coverage is respectively 23.999236/28.999076/26.999139 seconds, combining
retained and ordinary checkpoints. Every phase exceeds both unchanged ten-second
requirements. All ten raw residency counters remain exactly zero, not merely
rounded percentages: no CPU0 core C3/C6/C7 or package C2/C3/C6/C7/C8/C9/C10
residency was recorded. TSC brackets advance normally. This accepts the bounded
read-only capture/validation path on this unit, not deep-state entry or CPU
policy. It does not measure C0/C1, all-core residency, equilibrium power or
battery life. The next useful investigation is read-only auditing of idle entry,
other-core state and firmware/package restrictions before changing CPU policy.

All 516 baseline files are preserved except expected `.srvros/meta` bookkeeping.
Four new logs bring the volume to 520 files / 1,567 clusters; offline fsck and
allocation checks report zero leaks, mismatches or errors. Collected evidence
and current/recovery payloads are hash-verified in durable storage outside
`build`. The automatic collector is disabled, GRUB's one-shot entry is consumed
and no further reboot is armed. Ubuntu remains the default recovery OS.

### Idle entry and firmware audit — active scope (2026-10-27)

Base `51f8b39b` includes the collected residency run. Audit the unchanged CPU0
`sti; hlt; cli` readiness/idle path and add capture-only entry/return/readiness
counters with generation-safe start/stop. Read only the captured 06_45 E2 package
C-state configuration register at capture boundaries, and retain raw CPUID
MONITOR/MWAIT capability information. No control writes, MWAIT activation,
secondary-CPU startup, polling/cadence, watchdog or limit changes. No MP request
exists in the current Limine boot path; other cores' live state remains unknown.

Touch scheduler diagnostic accounting, power capture/service regressions, strict
saved-record validation and this unit's guides. Preserve the existing v35
30-second idle/load/recovery workload, 24-record capacity, 64 KiB log tail,
ten-second coverage minimum, thermal/fan stops and USB integrity checks. PR182
filesystem work touches other scheduler-adjacent process paths; PR19 owns Wi-Fi.
Neither currently changes this idle loop or power collector. Next checkpoint is
host/VM validation, merge and the authorized automatic native run. Prior run is
fully collected; this audit has no new hardware acceptance yet.

### Idle-entry/firmware audit — candidate ready (2026-10-27)

PR220 runtime `b6ffff14` retains the original readiness check and contiguous
`sti; hlt; cli` in the compiled idle worker. Capture-only counters track entry
attempts, returns and readiness bypasses; generation checks prevent a preempted
return from an earlier capture polluting a new one. There are no clocks, I/O,
allocations or printing in that idle path. Read-only E2 snapshots occur only at
capture start/stop behind the accepted 06_45/invariant-TSC gate. No firmware
methods, control writes, MWAIT activation or AP startup are added.

Linux CPU0 inventory reads E2 `0x408`: package C10 limit, unlocked, I/O-MWAIT
redirection enabled and zero demotion/undemotion bits. This is Linux inventory,
not SrvrOS firmware-policy evidence. Native values and raw MONITOR/MWAIT CPUID
capabilities are saved by the next run. No other-core live state is inferred.

Full matched build/USB image, host/core and docs pass on `b6ffff14`; source
`c888ab53` adds only the test header dependency, with final matched rebuild,
power regression and docs passing. Scheduler regressions cover inactive/nested
capture, ready bypass, HLT path, delayed return across stop/restart and epoch
exhaustion. Driver tests cover exact model-gated E2 reads, failed/nested capture,
restart and cached records. Ten collector tests pass, including generation/
timestamp pairing, monotonic counts, one outstanding return, missing/malformed
records, changed configuration and unknown limits. All twelve existing load
safety refusal/stop cases remain passing. Power/load VMs verify default-disabled
and unsupported-hypervisor refusal.

Exact v35 release `6b1f017332e0931b` passes GPT save/unmount/automatic return and
missing-workspace halt. Run `49fe4e29d5ed450a` uses the unchanged three 30-second
idle/load/recovery phases, with automatic Ubuntu return and strict collection.
Required evidence includes >=10 seconds of power, residency and paired idle-entry
coverage per phase, complete firmware audit, workload completion, file preservation
and clean allocation. Zero HLT counts or changed firmware values remain explicit
observations; they are not concealed or reinterpreted as deeper-state acceptance.
All power policies, cadence, time/capacity limits, thermal stops, watchdogs and
integrity checks remain unchanged. No human input is needed; keep AC/USB connected.

USB preflight preserves 520 files / 1,567 allocated clusters with zero leaks,
mismatches or errors. The old `a5f8fdf5d377c116` EFI copy was retired only after
verifying its durable backup; accepted residency `ed06103a5275d7ca`, desktop,
stress, attended reopen and console recovery payloads remain protected. Exact
payload/evidence/run records are backed up outside `build` before reboot. Ubuntu
stays the default. Native idle-entry/firmware audit acceptance remains pending.


### Idle-entry/firmware audit — bounded native acceptance (2026-10-27)

Return closeout from base `e4f8bf84` is documentation/evidence only. PR182 remains
filesystem work and PR19 at `6b12e4f2` remains Pro Wi-Fi; neither changes this
acceptance scope. No new runtime policy or trial is being prepared.

Release `6b1f017332e0931b` (runtime `b6ffff14`, exact staged source `c888ab53`,
merged PR220) completed run `49fe4e29d5ed450a`, session
`session-00065ed949956fde-00`, and returned automatically to Ubuntu. All three
ordered 30-second phases completed, with 250,484 single-worker load chunks and
peak logged temperature 71 C. Fifteen retained records have full residency
support/valid masks, zero rejections and no capture overflow. Independent raw
entry-counter and energy arithmetic agrees with the collector. The 65,290-byte
exit log retains all required evidence inside the unchanged 65,536-byte tail;
future diagnostics must account for its small remaining margin.

| Phase | Paired residency/entry coverage | HLT entries / returns | Ready bypasses | Package average | CPU0 active frequency |
| --- | --- | --- | --- | --- | --- |
| Idle | 23.999235 s | 2,378 / 2,378 | 1,189 | 4.147 W | 1,287 MHz |
| Load | 22.999267 s | 0 / 0 | 0 | 5.174 W | 1,300 MHz |
| Recovery | 23.999234 s | 2,371 / 2,371 | 1,186 | 4.139 W | 1,290 MHz |

Power coverage is 23.999235/27.999108/24.999203 seconds respectively; all three
power, residency and entry coverage requirements pass unchanged. The complete
capture records 5,937 HLT entries, 5,936 returns and 2,969 readiness bypasses.
One outstanding return at stop is allowed by the generation-safe accounting;
it is not an unbalanced lifecycle failure. Covered idle/recovery intervals have
about 99 HLT entries per second. Counts are attempts/returns, not measured sleep
time or interrupt attribution. All ten raw deeper-residency counters remain
exactly zero. C0/C1 are unmeasured; HLT is reached, but deeper entry is unproven.

Native E2 starts and ends at `0x408`: package limit C10, configuration unlocked,
I/O-MWAIT redirection enabled and no recorded demotion/undemotion flags. Thus the
observed package-limit field is not restricting this capture to shallow states;
this does not rule out other firmware/device/core restrictions. MONITOR is
advertised, with CPUID.5 `64/64/3/286531872`; no MONITOR/MWAIT instruction is used.
The scheduler still manages CPU0 only and other cores' live state remains unknown.
CPUID logical capacity is not evidence of online CPUs. No policy/control writes,
AP startup, timer cadence, watchdog, thermal or integrity limit changes occurred.

All 520 baseline files are preserved except expected `.srvros/meta` bookkeeping.
Four new logs bring the USB to 524 files / 1,572 clusters. Offline fsck/allocation
pass with zero leaks, mismatches or errors. Installed/current and protected
recovery payload hashes verify; evidence is retained outside `build`. Ubuntu is
default, the collector is disabled and the one-shot is consumed. No reboot is
armed. This accepts the bounded read-only audit, not deeper idle or energy savings.
Next work should examine boot-time secondary-core state and the prerequisites
for an explicit model-gated deeper-idle experiment; further polling reductions
alone do not address the missing hardware residency demonstrated here.

### Secondary-core/deeper-idle prerequisites — active scope (2026-10-27)

Base `43c79c73`; prior idle audit is collected and accepted. Investigate the pinned
bootloader's secondary-CPU handoff, captured MADT inventory and native CPU0
CPUID topology. Add bounded capture-only topology and read-only BSP/MONITOR
configuration evidence if the captured CPU documentation permits it. Distinguish
firmware inventory and CPUID capacity from live AP state; do not start APs, send
IPIs, change idle instructions, evaluate AML or alter power policy or limits.

Scope is power diagnostic capture, strict host/collector regressions and this
unit's records. Reuse unchanged v35 idle/load/recovery phases and all coverage,
thermal, watchdog, filesystem and 64 KiB log limits. Account for the last run's
small log margin before adding output. PR182 filesystem audit and PR19 Wi-Fi
(`482f2eb5`) do not currently change these power paths. Next checkpoint: local
and exact-stage validation, self-merge, durable run record and authorized native
automatic test. Secondary-core live state and deeper idle remain unaccepted.

### Secondary-core/prerequisite audit — candidate ready (2026-10-27)

PR222 runtime `8a0b9166` adds a single compact capture receipt: CPU0 CPUID.0B
SMT/core/terminator tuples and read-only APIC-base/misc snapshots at start/stop.
The exact native 06_45/invariant-TSC gate is unchanged. No extra reads occur in
ordinary sampling/status, and no AP startup, IPI, MWAIT, control writes, polling,
scheduler or power-policy changes are introduced. All timing, capacity, thermal,
watchdog and integrity limits remain unchanged.

Pinned Limine binary `5be26a73` identifies v11.4.1; matching source tag commit
`1276c440` skips `init_smp` without an MP request, which this kernel omits.
Captured MADT has four enabled APIC IDs (0,2,1,3), plus disabled placeholders.
Linux CPU0 leaf-B reports two addressable threads/core and four/package. These
are inventory/capability observations: remote cores' live state cannot be
established from CPU0-local reads. No claim of halted, running or wait-for-SIPI
APs is made. The power guide records prerequisites for a separately designed
race-safe deeper-idle experiment; none is enabled by this trial.

Matched build, USB image, full host/core and docs checks pass on `8a0b9166`.
Service tests verify exact boundary read counts, gate/nesting refusal, restart,
unsupported-leaf avoidance and cached status. Twelve collector tests pass with
strict epoch/field/bounds validation, malformed/missing-record refusal, changed
configuration and unknown topology handling. The twelve existing load safety
refusal/stop cases pass. Power/load VMs retain disabled/unsupported refusal.
Replay of the prior native log with the 121-byte prerequisite record retains
all required proof in 65,411 bytes, below the unchanged 65,536-byte tail. Actual
new native capture completeness still requires collection after return.

Exact v35 release `8022333a9240598e` passes GPT save/unmount/automatic return and
missing-workspace safe-halt rehearsal. Run `7b7df1e1897c495d` will execute the
unchanged 30-second idle/load/recovery phases, then save and return automatically.
It requires complete power/residency/entry coverage, the new prerequisite receipt,
prior-file preservation and clean offline allocation. No human input is needed;
keep AC/USB connected. Native prerequisite acceptance is pending return.

USB preflight preserves 524 files / 1,572 allocated clusters with zero leaks,
mismatches or errors. Preserve accepted audit `6b1f017332e0931b`, console recovery,
desktop power, stress and attended reopen payloads. The older residency-only
`ed06103a5275d7ca` EFI copy may be retired after durable hash verification to make
room; its independent durable payload/evidence remains retained. Exact payload,
run and validation records must be durable before the authorized one-shot boot.


### Secondary-core/prerequisite audit — bounded native acceptance (2026-10-27)

Return closeout from base `6d9847e6` is documentation/evidence only. PR182
filesystem audit and PR19 Wi-Fi (`c5e457cd`) remain independent; no new runtime
policy, AP experiment or boot is being prepared.

Release `8022333a9240598e`, source `8a0b9166`, merged PR222, completed run
`7b7df1e1897c495d`, session `session-00065edabaa7e840-00`, and automatically
returned to Ubuntu. All three ordered 30-second phases completed, including
250,562 load chunks, peak logged temperature 70 C, fifteen retained records,
full residency support/valid masks and zero rejections/overflow. Independent
raw-entry, topology/configuration and energy arithmetic agrees with collection.
The 65,484-byte exit log preserves all required proof inside the unchanged
65,536-byte tail; there is almost no spare margin for additional output.

| Phase | Paired residency/entry coverage | HLT entries / returns | Ready bypasses | Package average | CPU0 active frequency |
| --- | --- | --- | --- | --- | --- |
| Idle | 22.999635 s | 2,267 / 2,267 | 1,133 | 4.150 W | 1,289 MHz |
| Load | 22.999461 s | 0 / 0 | 0 | 5.144 W | 1,299 MHz |
| Recovery | 23.999619 s | 2,359 / 2,359 | 1,181 | 4.141 W | 1,290 MHz |

Power coverage is 22.999635/24.999602/23.999619 seconds. All unchanged ten-second
coverage requirements pass. The whole capture records 5,928 entries, 5,927
returns and 2,965 readiness bypasses; one outstanding return at stop is allowed.
All ten raw deeper-residency counters remain exactly zero. C0/C1 and time asleep
are unmeasured; this does not imply that HLT failed or that CPU0 never idled.

Native maximum basic CPUID leaf is 13. Leaf-B tuples are `1,2,256,0`,
`4,4,513,0`, `0,0,2,0`, reporting CPU0 APIC ID 0, two addressable threads/core
and four/package. These describe topology, not live CPU count. APIC-base starts
and ends at `4276095232`: BSP=1, APIC enabled, x2APIC disabled. Misc starts and
ends at `8716425`, with MONITOR FSM enabled and consistent with CPUID MONITOR
advertisement. CPUID.5 advertises extensions and masked-interrupt break, with
substate nibbles `0,2,1,2,4,1,1,1`; this does not select a hint or prove wakeup
correctness. E2 remains `0x408` (unlocked C10 limit). None of these configuration
snapshots changed. Remote CPU state remains unknown; there was no AP startup,
IPI, MONITOR/MWAIT execution, control write or policy/limit change.

All 524 baseline files are preserved except expected `.srvros/meta` bookkeeping.
Four new logs bring the USB to 528 files / 1,577 clusters. Offline fsck/allocation
pass with zero leaks, mismatches or errors. Installed/current and protected
recovery payload hashes verify, and collected evidence is hash-verified outside
`build`. The older residency-only EFI copy `ed06103a5275d7ca` was retired after
verifying its durable payload; accepted prior audit `6b1f017332e0931b`, console,
desktop power, stress and attended reopen payloads remain protected. Ubuntu is
default, the one-shot is consumed, the collector is disabled and no reboot is
armed.

This accepts the bounded read-only prerequisite capture only. The next useful
step is a separately opted-in MONITOR/MWAIT wakeup experiment with an exact-model
hint, race-safe readiness recheck, bounded latency checks and HLT fallback,
before claiming deeper core/package entry. Other-core observation would require
its own AP ownership/rendezvous design and remains separate work. No performance,
battery-life or energy-saving claim follows from these short measurements.

### Opt-in MONITOR/MWAIT wakeup — active scope (2026-10-27)

Base `71379774`; previous prerequisite run is collected. Implement an explicit
CPU-load capture option for a bounded CPU0 MONITOR/MWAIT wakeup experiment on
the captured 06_45 CPU. Use shallow hint 0 only, require native MONITOR FSM,
extensions/masked-interrupt break and 64-byte monitor-line capability, recheck
scheduler readiness after MONITOR, and cap MWAIT attempts at 32 before HLT
fallback. Measure the IRQ-masked return interval against one existing two-tick
scheduler quantum (20 ms at the unchanged 100 Hz tick). A timing fault latches
HLT fallback; it is not a pass. No deeper hint, AP startup, IPI or MSR write.

Touch scheduler diagnostic idle selection, power capture admission/receipt,
explicit staging/main dispatch, collector and host/VM tests. Default idle remains
HLT. Existing 30-second phases, ten-second coverage, watchdogs, thermal/fan stops,
64 KiB logs and integrity limits remain unchanged. PR182 filesystem audit and
PR19 Wi-Fi do not currently modify this idle path; preserve their independent
profiles. Next checkpoint is validation, self-merge, exact-stage install and the
authorized automatic trial. Native MWAIT wakeup acceptance remains pending.

### Opt-in MONITOR/MWAIT wakeup — candidate ready (2026-10-27)

PR224 runtime `94c2b9e7` adds the explicit `--cpu-load-capture
--mwait-wakeup-test` profile. The normal HLT policy remains unchanged. The gated
idle worker arms a private aligned monitor line, rechecks readiness with IRQs
masked, and uses hint 0/extension 1 for at most 32 shallow C1 waits. The PM-clock
bracket is measured before IRQ dispatch; this is wait duration, not ISR latency.
Clock regression or a wait over the existing 20 ms scheduler quantum latches
HLT fallback. Successful trials also return to HLT after 32 attempts, and must
record 32 subsequent HLT entries and timer progress. Stable prerequisite
snapshots and complete phase/retirement proofs are required. No deeper hint,
AP startup, IPI, MSR write, tick change or relaxed limit is introduced.

Final matched build/USB image, full host/core and docs pass on `94c2b9e7`.
Scheduler tests cover readiness recheck, exact attempt cap, mandatory HLT,
20 ms boundary/excess duration, regressing clock and retired epochs. Service
tests cover opt-in and capability refusal, failed admission, nested capture,
failed trial completion and configuration drift. All 97 collector/staging tests
and twelve existing load safety refusal/stop cases pass. Disassembly retains
MONITOR, shallow MWAIT and the unchanged contiguous `sti; hlt; cli` fallback.
Prior-log replay with the 156-byte trial receipt retains all required evidence
within the unchanged 64 KiB tail, although early startup text is trimmed.

Exact release `8030ae20c2611a09` passes unsupported-MWAIT refusal, GPT save/unmount/
automatic return and missing-workspace safe halt. A VM receipt-label error for
the new profile was caught before install; harness-only `ba20b181` correctly
labels its power refusal and the exact positive save/return rehearsal was rerun.
The immutable staged runtime/manifest remains `94c2b9e7`; no payload changed.
Native wakeup/latency acceptance is pending run `7dc08174f8034042`, which uses
the unchanged three 30-second phases and automatic Ubuntu return/collection.
No human input is required; keep AC/USB connected.

Preflight preserves all 528 files / 1,577 allocated clusters with zero leaks,
mismatches or errors. Protect accepted prerequisite payload `8022333a9240598e`,
console recovery, desktop power, stress and attended reopen. The older audit-only
EFI payload `6b1f017332e0931b` may be retired after durable hash verification;
its independent backup remains retained. Payload, exact-stage checks and run
records must be backed up outside `build` before arming the authorized reboot.


### Shallow MONITOR/MWAIT wakeup — bounded native acceptance (2026-10-27)

Return closeout from merged PR224/base `052ebc18` is documentation/evidence only.
PR182 filesystem audit (`2a091597`) and PR19 Wi-Fi (`6e0e25ec`) remain independent.
No new runtime policy, deeper hint, AP experiment or reboot is being prepared.

Release `8030ae20c2611a09`, compiled source `94c2b9e7`, completed run
`7dc08174f8034042`, session `session-00065edb20755615-00`, and automatically
returned to Ubuntu. The explicit shallow hint 0/extension 1 trial records all
32 attempts and returns, a maximum masked wait of **9,971 us** against the
unchanged 20,000 us limit, zero faults, timer ticks 62 to 93, and 32 subsequent
HLT entries. No post-MONITOR readiness bypass occurred in this native run; that
race remains covered by the host regression. Independent raw-receipt review
agrees with the collector. The saved 65,536-byte exit tail retains every required
receipt; early startup text is trimmed, with identity preserved in boot.log.
This accepts bounded interrupt wakeup and mandatory HLT fallback, not IRQ dispatch/runnable latency or a store-signalling protocol.

All three ordered 30-second phases complete, with 250,554 load chunks and a
63 C peak logged temperature. Fifteen retained records have full support/valid
masks, zero rejections and no overflow. Paired residency/entry coverage is about
24 seconds in every phase; power coverage is 23.999618/23.999620/26.999572 seconds.
The unchanged ten-second coverage requirement passes. Package averages are
4.122/5.167/4.127 W for idle/load/recovery. Covered HLT entries/returns are
2,378/0/2,373; whole-capture HLT totals are 5,908 entries and 5,907 returns.
All ten deeper-residency raw counters remain zero. The short MWAIT trial ends
before the retained phase intervals, which mostly measure the HLT fallback;
these phase averages do not establish MWAIT energy savings. C0/C1 and time
asleep remain unmeasured.

Native APIC/misc and E2 snapshots remain unchanged, including the permissive
C10 package limit. Other logical processors' live state remains unknown.
Ordinary boot/desktop idle policy, tick frequency, thermal/fan stops, watchdogs,
integrity checks and all limits remain unchanged. No AP startup, IPI, MSR write
or deeper hint was introduced.

All 528 baseline files are preserved except expected `.srvros/meta` bookkeeping.
Four new logs bring the USB to 532 files / 1,582 clusters; offline fsck and
allocation pass with zero leaks, mismatches or errors. Installed/current and
protected recovery payload hashes verify. Evidence is retained and hash-verified
outside `build`. Ubuntu remains default, the one-shot is consumed, the collector
is disabled and no reboot is armed. The older audit-only EFI payload
`6b1f017332e0931b` was retired after verifying its durable backup; the accepted
prerequisite, console, desktop power, stress and attended reopen payloads remain.

The next proposed step is a separately gated deeper-hint experiment with the
same readiness, latency, fallback and integrity protections, plus an explicit
review of timer behavior and sibling-thread/package prerequisites. This result
does not accept deeper entry, longer endurance or a permanent power policy.

### Bounded deeper-idle trial — active scope (2026-10-27)

Base `3bbf80b7`; shallow run collected and accepted. Add a separate explicit
Haswell 06_45 C3 hint `0x10` trial, at most 32 waits with the unchanged 20 ms
masked interval limit, readiness recheck, epoch retirement and 32-entry HLT
fallback. Require CPUID ARAT and the live calibrated 100 Hz LAPIC timer; capture
TSC-bracketed core/package C3 deltas around each attempt, including valid zero
observations. Other-core state is unknown and may prevent deeper residency.
No AP startup, IPI, control-register write, general idle-policy change or relaxed
limit. Scope: scheduler diagnostic, power admission/receipt, read-only LAPIC
readiness helper, explicit staging, collector and host/VM regressions. PR182
filesystem (`2a091597`) and PR19 Wi-Fi (`6e0e25ec`) remain independent. Next:
validate, self-merge, exact-stage install and authorized automated native run.


### Bounded C3 trial — validated candidate (2026-10-27)

PR226 runtime `5ad0cb6f`, exact staged release `1bac141e5965a9b0`, adds only the
explicit C3 hint recipe. Full matched build/USB/core/docs pass. Directed host
checks cover shallow compatibility, timer/capability/unsupported-hint refusal,
32-attempt cap, readiness race, retired epochs, latency boundary and failure,
C3 counter regression/overflow and valid zero residency, and mandatory HLT.
All 98 collector/staging tests and twelve existing load refusal/stop cases pass.
Disassembly preserves MONITOR, selected-hint MWAIT, pre-dispatch measurement and
the original contiguous `sti; hlt; cli` fallback.

The exact GPT workspace VM passes explicit C3 refusal, saved logs, unmount and
automatic return; missing workspace halts safely. Disabled/unsupported power and
load regressions also pass. These VMs do not accept native C3 entry or latency.
Prior native log replay with both deeper-test receipts retains all phase proof
under the unchanged 64 KiB limit. No counter/thermal/watchdog/integrity threshold
has been widened. Valid zero C3 remains an unobserved result, distinct from a
successful bounded wakeup test.

Native run `cee8062fef7e4084` will execute the same three 30-second phases with
visible progress and automatic Ubuntu return/collection; no human input is
needed. Local model, connected AC, USB identity and all 532 baseline file hashes
verify. Allocation/headroom are clean at 1,582 clusters. Preserve shallow accepted
payload `8030ae20c2611a09`, console recovery, desktop power, stress and attended
reopen. The superseded prerequisite-only EFI copy `8022333a9240598e` may be
retired after verifying its durable backup. Save/hash the candidate and run
records outside `build` before arming. Native acceptance remains pending.


Before final integration, main `671a89ed` / PR227 landed an Air7,2 Help/checklist
update. Consumed it at `2a8fabd0`, rebuilt matched artifacts and reran the full
core/docs suite plus exact-stage GPT save/return, missing-workspace and disabled/
unsupported power/load VMs; all pass. Final candidate is **`980f4936c6282637`**,
compiled source `2a8fabd0`, with the same run `cee8062fef7e4084`. Earlier candidate
`1bac141e5965a9b0` was rehearsed but never installed or physically booted. The
integration changes no idle/runtime path. The prerequisite-only EFI payload
`8022333a9240598e` was retired after durable verification; accepted shallow
`8030ae20c2611a09` and the protected recovery/endurance payloads remain installed.
Physical C3 wakeup/residency acceptance is still pending the automatic run.


### Bounded C3 hint — native result, residency unobserved (2026-10-27)

Closeout from merged PR226/base `f5917467` is documentation/evidence only.
PR182 filesystem audit (`2a091597`) and PR19 Wi-Fi (`6e0e25ec`) retain independent
scope. No new boot, deeper selector or power-policy change is being prepared.

Final release `980f4936c6282637`, compiled source `2a8fabd0`, completed run
`cee8062fef7e4084`, session `session-00065edb80a92930-00`, and automatically
returned to Ubuntu. All 32 hint-16/extension-1 attempts returned; maximum masked
wait was **9,968 us** under the unchanged 20,000 us limit, with zero faults,
timer ticks 65 to 96 and 32 subsequent HLT entries. ARAT and live calibrated
timer readiness passed. No post-MONITOR readiness bypass occurred; that race
remains host-tested. Independent raw-receipt review agrees with collection.

The per-attempt residency brackets total **620,687,304 TSC ticks**, with exactly
**zero core C3 and zero package C3 ticks**. These are valid zero observations,
not a counter fault or proof that the requested state was entered. This accepts
bounded execution of the C3 hint, interrupt wakeup and mandatory HLT fallback;
**physical C3 residency is not accepted**. C0/C1, remote logical processors' live
state, exact sleep time and IRQ/runnable latency remain unmeasured.

All three ordered 30-second phases complete, with 250,530 load chunks, peak
logged temperature 62 C, fifteen fully valid retained records and zero
rejections/overflow. Paired residency/entry coverage is 23.999619 seconds in
each phase. Power coverage is 23.999619/23.999619/26.999571 seconds, above the
unchanged ten-second minimum. Package averages are 4.100/5.120/4.101 W for
idle/load/recovery. Covered HLT entries/returns are 2,378/0/2,373; whole-capture
counts are 5,907 entries and 5,906 returns. All ten deeper raw counters remain
zero. The 32-attempt trial precedes retained phase intervals; the phase power
averages mostly describe HLT fallback and establish no MWAIT energy savings.
Native APIC/misc and E2 snapshots are unchanged, including E2 `0x408`.

All 532 previous files are preserved except expected `.srvros/meta` bookkeeping.
Four new logs bring the USB to 536 files / 1,587 clusters. Offline fsck and
allocation pass with zero leaks, mismatches or errors. The 65,536-byte exit tail
retains all required proof; boot.log retains release/command-line identity.
Installed/current and protected recovery payload hashes verify. Collected
artifacts are hash-verified outside `build`. Ubuntu is default, the one-shot is
consumed, the collector is disabled and no reboot is armed.

Next investigate the missing residency, especially the still-unknown state of
the sibling logical processor and firmware constraints. Any AP observation or
rendezvous requires separate ownership and recovery design. Do not infer that
the firmware limit, sibling state, or any particular constraint is the cause
from these CPU0 receipts alone. Ordinary idle policy and all limits remain intact.


### Missing C3 residency investigation — active scope (2026-10-28)

Base `b9526c88`, after collecting and closing the bounded C3 trial. Investigate
sibling-thread prerequisites and firmware constraints using the pinned Limine
source, Intel's Haswell U/Y documentation and read-only Ubuntu residency/topology
observations. Scope is this unit record and local evidence; no AP startup,
CPU offlining, MSR/control-register writes, new idle selector or policy/limit
change. PR182 filesystem and PR19 Wi-Fi retain their independent scope. Next
checkpoint: distinguish observed hardware capability from unproven native AP
state and specify the smallest justified follow-up. No new reboot is armed.


### Missing C3 residency — investigation result (2026-10-28)

A read-only Ubuntu reference on this same unit confirms that the residency
registers advance and that deeper core/package idle is physically possible.
Three ambient ten-second windows under `intel_idle` report CPU0 core C3
0.799/0.222/0.401%, core C7 70.341/95.710/94.004%, and package C3
44.933/72.618/69.147% of elapsed TSC. All four logical CPUs show positive
core C3/C6/C7 and package C2/C3 deltas in every window. Package C6–C10 stay
zero in this reference. These are ambient Ubuntu observations, not a controlled
power comparison or SrvrOS acceptance; sequential MSR reads perturb execution
and do not form simultaneous per-core snapshots.

Linux topology pairs CPUs 0/2 (APIC IDs 0/1) and 1/3 (APIC IDs 2/3). All four
CPUs remain online. Per-CPU E2 stays `0x408`, matching SrvrOS, and misc stays
`0x850089`. Linux uses x2APIC while the native trial uses xAPIC; the environments
are not equivalent. No CPU hotplug, idle-state configuration, power-policy or
MSR write was performed. Raw delta arithmetic, monotonicity, bounds and unchanged
control/configuration snapshots were independently checked. Evidence lives in
`build/power/air61-c3-investigation-20261028`, with a hash-verified durable copy
outside `build`.

Intel's [Haswell U/Y datasheet, sections 4.2.4–4.2.5](https://cdrdv2-public.intel.com/329001/4th-gen-core-family-mobile-u-y-processor-lines-vol-1-datasheet.pdf)
explains that the shallower thread request determines the core's idle state;
package entry additionally depends on the other cores and platform components.
This makes the unmanaged sibling a concrete prerequisite to investigate.
The Ubuntu measurements rule out a blanket hardware inability to enter C3 or
a permanently inactive residency counter. They do not prove the sibling is
the native blocker, nor rule out other handoff/configuration differences.

Pinned Limine v11.4.1 source (`1276c440`) only invokes `init_smp` when a
`LIMINE_MP_REQUEST` is present. SrvrOS's `kernel/src/limine_requests.c` contains
no such request. Therefore the bootloader's AP parking loop is not evidence
of the AP state in our current native boot. When invoked, that loop uses
MWAIT hint zero (C1), or PAUSE without MONITOR support. Merely adding the MP
request would not establish the C3 sibling prerequisite. See the pinned
[MP-request path](https://github.com/limine-bootloader/limine/blob/v11.4.1/common/protos/limine.c)
and [AP trampoline](https://github.com/limine-bootloader/limine/blob/v11.4.1/common/sys/smp_trampoline.asm_x86).

**Next milestone: diagnostic secondary-CPU ownership and rendezvous.** Start
with an explicitly separate diagnostic payload, because a static Limine MP
request takes effect before SrvrOS can parse an opt-in command line. Keep the
ordinary kernel's boot requests intact. First prove per-CPU identity, private
stack/descriptor/fault handling, coherent mailbox wake/acknowledgement and bounded
return to a known park state. APs must not enter the current single-CPU scheduler,
allocator, console or shared BSP TSS/interrupt handlers. Define storage lifetime
and failed-AP containment before release; a timeout cannot reclaim memory that
a remote CPU may still access. Any bootloader-reclaimable memory still used by
parked APs must remain reserved. Rehearse missing/duplicate/late acknowledgements,
unsupported topology and fault containment in multi-CPU VMs before physical use.
This rendezvous changes AP state and cannot retrospectively observe the original
firmware park state. Only after ownership is accepted should a separate coordinated
C3 experiment reuse the existing 32-attempt, 20 ms and HLT-fallback protections.
No general SMP scheduling or permanent idle-policy change is implied.

This investigation changes documentation only. Installed C3 diagnostic
`980f4936c6282637` and console recovery `c35475f62f154f1e` still hash-verify;
Ubuntu remains default, the one-shot is empty and no new reboot is armed.


### Secondary-CPU ownership diagnostic — active scope (2026-10-28)

Base `5a94d755`; PR182 filesystem (`29df1192`) and PR19 Wi-Fi (`b8d63682`)
remain independent. Implement a separate MP-request kernel/staging profile,
private AP stacks/GDT/TSS/fault IDTs and static coherent mailboxes. No AP joins
scheduler/allocator/console or changes power MSRs. Prove 32 ordered wake/ack
exchanges per AP under the unchanged 20 ms bound, then terminal retirement;
retain all AP and bootloader storage until reset, including failure paths.
Ordinary kernel has no MP request. Scope: diagnostic kernel objects, bounded
BSP capture hook, staging/parser and directed host/multi-CPU VM tests. Next:
validate normal/fault/refusal paths, self-merge and exact-stage automated native
capture with USB saved receipts and Ubuntu recovery. C3 is not requested. See the [ownership diagnostic contract](../ap-ownership-diagnostic.md).


### AP ownership diagnostic — validated candidate (2026-10-28)

PR232 source `3cc9a389`, release `53b2c4c90f65e647`, passes the matched ordinary
and separate diagnostic build, USB/core/docs suite, 84 staging tests and directed
host receipt/deadline tests. ELF inspection confirms zero MP requests in the
ordinary kernel and one in the diagnostic. AP disassembly contains no C calls,
WRMSR or exception return. It retains private descriptor/IST and C1-only wait paths.

Exact-stage GPT Q35 rehearsal completes all 96 exchanges on four CPUs, saves
logs, unmounts and automatically resets. Two CPUs also pass. One/six CPU cases
refuse safely. Missing startup acknowledgement exceeds the unchanged deadline
and reports failure; a real injected UD2 remains on that AP's private fault
handler while the other APs retire. Both failure receipts are saved and return
automatically. Missing workspace follows the established failure-return path
without claiming a saved result. VM APs use PAUSE; native MONITOR/store wakeup
remains unaccepted until physical evidence. All injected configurations remain
private temporary VM copies. A precommit staging manifest was archived and the
identical payload restaged from clean committed source; the final exact-stage
rehearsal passes with its final manifest hash.

Prepared native run `f08a07b619d4456d` needs no input: verify all three AP
identities, 32 exchanges each and terminal retirement, save logs and return to
Ubuntu for automatic collection. All 536 baseline file hashes match the previous
collected run. Read-only fsck, allocation and headroom pass at 1,587 clusters,
with zero leaks/mismatches/errors. Preserve installed C3 probe
`980f4936c6282637`, console recovery, desktop power, stress and attended reopen.
The superseded shallow-only EFI copy `8030ae20c2611a09` may be retired after its
durable payload hashes verify, to make staging space. Its evidence and payload
remain outside `build`. Native ownership acceptance is pending; C3 is not tested.


### AP ownership — native acceptance (2026-10-28)

Documentation/evidence closeout on integration base `951135b0` (PR231).
PR182 filesystem (`903030ef`) and PR19 Wi-Fi (`5a2e0089`) retain independent
scope. The newer integrated parity/runtime changes have not been physically
accepted by this run: its compiled source remains `3cc9a389`, merged through
PR232 at `80b0a8aa`. No new payload or reboot is prepared by this closeout.

Release `53b2c4c90f65e647` completed run `f08a07b619d4456d`, session
`session-00065ee89de49ed6-00`, and automatically returned to Ubuntu. All three
APs report the expected signature/APIC identity and complete 32 ordered mailbox
exchanges each, **96 total**, using native MONITOR/C1 MWAIT and BSP-store wakeup.
Independent review of the raw saved receipts agrees with automatic collection.

| APIC ID | Startup wait (us) | Maximum exchange (us) | Retirement wait (us) | Wait attempts | Faults |
| --- | --- | --- | --- | --- | --- |
| 2 | 5 | 2 | 2 | 33 | 0 |
| 1 | 5 | 2 | 1 | 33 | 0 |
| 3 | 4 | 1 | 2 | 34 | 0 |

Every measured interval is below the unchanged 20,000 us bound. All APs report
terminal retirement (state 2); storage remains retained until reset. These are
PM-clock-bracketed BSP acknowledgement measurements, not ISR/runnable latency
or a general latency guarantee. Wait-attempt counters do not measure physical
C1 residency. Fault and timeout containment remain VM/host-tested, not native
fault-injected. This accepts bounded explicit AP ownership, mailbox wakeup and
terminal retirement, not general SMP scheduling, reclaimability, permanent idle
policy, deeper-state entry or energy savings. The original firmware AP state
before the diagnostic's MP handoff remains unknown.

All 536 baseline files are preserved except expected `.srvros/meta` bookkeeping.
Two new logs bring the volume to 538 files / 1,590 clusters; offline fsck and
allocation report zero leaks, mismatches or errors. Installed/current and all
protected recovery payload hashes verify. The superseded shallow-only EFI copy
`8030ae20c2611a09` was retired after durable hash verification; its payload and
evidence remain outside `build`. The accepted C3 diagnostic, console recovery,
desktop power, stress and attended reopen payloads remain installed. Evidence
is hash-verified outside `build`. Ubuntu is default, the one-shot is consumed,
the collector is disabled and no reboot is armed.

Next: design a separately gated coordinated C3 experiment using the now-proven
AP ownership path, existing 32-attempt/20 ms bounds and BSP HLT fallback. Require
explicit peer readiness, wake/retirement ownership and per-attempt residency
proof. Ordinary boot retains no MP request; no C3 support is accepted yet.


### Coordinated C3 — active scope (2026-10-28)

Base `7b0cc468`, accepted AP ownership collected. PR182 filesystem (`903030ef`)
and PR19 Wi-Fi (`52e47a10`) retain independent scope. Add a separate opt-in
coordinated C3 profile to the diagnostic ELF, retaining ordinary no-MP boot.
After the accepted C1 ownership handshake, require all APs to arm monitored
mailboxes for each of at most 32 C3 attempts. BSP retains its calibrated ARAT
timer, scheduler readiness checks and HLT fallback, then wakes peers by store.
Bound the whole masked coordination round by the existing 20 ms ceiling; reject
early wake, missing/future receipts, faults, clock/counter regression or firmware
drift. Retain memory until reset on every path. Capture per-attempt BSP and peer
residency and distinguish valid zero from entry. Scope: diagnostic AP protocol,
diagnostic-only scheduler hooks, staging/parser and directed regressions. Next:
validate real protocol with VM simulation clearly separated from native C3,
rebuild matched baseline, self-merge and run the automated native capture.

Coordinated candidate validation uses source `827b01c2`, matched release
`57274dfaccfdc0b9`, with PR234 documentation baseline `45b8647a` consumed.
`all usb-image ap-probe-kernel test-core check-docs` passes; ordinary MP-request
absence and AP disassembly isolation verify. The final stage passes four-CPU
protocol simulation, targeted missing/early/faulted-peer cases, 1/2/6-CPU refusal,
exact native-profile VM refusal/save/return and missing-workspace containment.
The harness now rejects an ownership timeout before the requested injected
phase; the earlier such VM attempt is retained as incomplete directed coverage.
The original 65,536-byte log tail retains all phase and coordinated proof in a
synthetic replay of prior native receipts. VM simulation is not native C3 evidence.

PR235 supplies the implementation and strict proof parser. The next unattended
run retains AC/USB, shows the original 30-second idle/load/recovery progress,
attempts 32 coordinated C3 rounds, verifies HLT fallback and AP retirement,
saves and returns automatically. All power-policy registers, integrity checks
and limits remain unchanged. Native coordinated entry and latency are pending;
valid zero residency is not a C3-entry pass. Keep the accepted AP ownership and
console recovery installed, retain older evidence outside `build`, and collect
the matching session before another trial.

### Coordinated C3 — native acceptance (2026-10-28)

Release `57274dfaccfdc0b9`, runtime/harness source `827b01c2` (PR235 merge
`7bad3681`), completed session `session-00065ee9b6091a84-00` and returned to Ubuntu
automatically. Independent raw-receipt review confirms all 32 coordinated rounds,
maximum whole masked round 9,983 us and maximum BSP wait 9,951 us, below the
unchanged 20,000 us bounds. All 32 HLT fallback receipts are present. Every AP
reports 32 armed/completed attempts, no early return or fault, unchanged E2
`0x408`, and terminal retirement. Maximum startup/ownership/retirement intervals
are 6/2/2 us; all memory remains retained until reset.

Every round records positive core C3 on both physical cores. BSP totals are
635,343,942 TSC ticks and 633,350,860 core-C3 ticks (99.686% of these short measured
wait brackets). APIC 1, the BSP's SMT sibling, reports the same core-C3 total;
APIC 2/3 report 634,388,760 ticks for their shared physical core. These counters
must not be added as four independent cores. Package C3 is valid zero in all
32 rounds. This accepts bounded coordinated **core** C3 entry, wakeup, fallback
and retirement on this unit, not package C3, general SMP scheduling, sustained
idle policy, energy savings or reclaimability. The original firmware AP state
before the MP handoff remains unknown.

The original 30-second idle/load/recovery phases completed with 250,520 load
chunks, valid power/residency/entry coverage and a 58 C peak logged temperature.
Covered package averages were 4.050/5.130/4.092 W. APs return to C1 between/after
the short C3 trial; the later phase deltas do not represent sustained C3 policy.
No power-policy MSR, timing limit or integrity check changed.

All 538 baseline files are preserved except expected `.srvros/meta` bookkeeping;
four new logs bring the volume to 542 files / 1,595 clusters. Offline fsck and
allocation checks pass with zero leaks, mismatches or errors. Installed/current
and protected recovery payload hashes verify. The prior BSP-only C3 EFI copy
`980f4936c6282637` was removed for space after durable verification; its payload
and evidence remain outside `build`. Accepted AP ownership, console recovery,
desktop power, stress and attended reopen payloads remain installed. Return
collection backed up 463 files with verified hashes before this closeout.
Ubuntu is default, the one-shot is consumed, collection is disabled and no reboot
is armed.

Closeout consumes PR236 baseline `732ad5a5` and checks its integrated scheduler
regressions; that newer runtime was not in this physical release. PR182 filesystem
(`811c4685`) and PR19 Wi-Fi (`52e47a10`) retain separate scope. Next: a read-only
audit of package C3 blockers and platform prerequisites, then a separately bounded
experiment. Keep ordinary startup and all power-policy settings unchanged until
there is a reviewed next test; core C3 success does not accept deeper package states.

### Package C3 audit — active scope (2026-10-28)

Base `6cbfedab`; coordinated core-C3 acceptance and its return evidence are
complete. Inspect package-level permission/latency constraints without changing
power policy, entry hints, 32-round/20 ms bounds or fallback. Add optional
read-only package C2 brackets, C3 interrupt-response-limit snapshots and bounded
standard PCI power/link snapshots to the existing isolated coordinated diagnostic.
Report graphics/platform permission as unknown where no safe documented read is
available; device state alone must not become a causal blocker verdict. Ordinary
boot, AML and GPU/MMIO control remain unchanged. Scope: diagnostic-only capture,
strict staging/parser and host/VM regressions. PR238 (`99e4412d`) desktop reap/
screenshot work, PR182 (`950a4ee0`) filesystem work and PR19 (`7ac6b0e6`) Wi-Fi
retain independent scope. Next: audit register semantics, validate read-only
capture and unchanged recovery, self-merge and run the automatic native trial.

The audit implementation is in PR239 (`48355ccd`). Its first matched build and
host suite pass, including bounded conventional PCI capability reads, malformed/
overlapping chain refusal, C2 counter bounds, strict parsing and existing scheduler
fallback. The initial staged image is `34f300408d696362`. Native audit evidence
is still pending. PR238 has now merged as `2c34a460`; consume its desktop-worker
changes and rebuild a matched image before the final native launch. This does
not change the diagnostic kernel's policy or timing requirements.

The additional PCI rows can move the original CPU-load configuration banner
out of the 65,536-byte exit tail. Collection requires that unchanged banner in
this exact session's periodic/final logs; all phase completions, counter/timing
proofs, retirement and audit records must still validate from the final log.
A maximum-width 25-device replay (the previously captured enumeration count)
retains those proofs without increasing any log or test limit. Capability state,
graphics permission and unsupported encodings remain distinct from zero.

Final candidate `bae0678ed7e9b38b` uses source `c5818be5`, incorporating PR238
main `2c34a460`. The matched ordinary/diagnostic build, full core suite and docs
checks pass. All nine package-profile VM cases pass: four-CPU simulation,
missing/early/faulted peer, 1/2/6-CPU refusal, exact native-profile refusal with
save/return, and missing workspace. Directed failure-state review, ordinary
kernel isolation and unchanged-tail coverage pass. Separate legacy AP-ownership
and coordinated-C3 simulation regressions also pass with no package reads.

The next automated native run captures 32 package-C2 brackets, paired C3 IRTL
and standard PCI snapshots, then completes the existing 30-second idle/load/
recovery phases and returns to Ubuntu. Collection requires complete matching
receipts and offline filesystem preservation. No human input is required; keep
AC and USB connected. Native audit acceptance remains pending. Preserve console
recovery and accepted coordinated-C3 payload `57274dfaccfdc0b9`; retire only the
older standalone AP-ownership EFI copy after verifying its durable backup to
make space. Its evidence and payload remain available outside `build`.

### Package C3 audit — native result (2026-10-28)

Release `bae0678ed7e9b38b`, source `c5818be5` (PR239 merge `ec7bad42`), completed
`session-00065eea9a060051-00` and returned automatically. Independent raw-receipt
review confirms 32 successful coordinated rounds, maximum whole round 9,982 us,
maximum BSP wait 9,951 us and 32 HLT fallbacks under unchanged 20,000 us bounds.
Both physical cores report positive C3 in every round. All three APs retire to
terminal state 2 without faults; startup/ownership/retirement maxima are 6/2/1 us.

Package C2 is positive in every round: 632,453,980 counter ticks over 635,361,332
TSC ticks, about 99.542% of the added short brackets. Package C3 remains valid
zero. The C2 bracket encloses the narrower BSP core/C3 bracket; its ratio is not
whole-session residency or an energy-saving claim. This accepts the read-only
audit and bounded package-C2 observation, while package C3 remains unresolved.

Paired C3 interrupt-response-limit snapshots remain `0x8842`, valid 67,584 ns.
All 25 conventional PCI snapshots are complete and unchanged between 2,072,215
and 92,121,507 us. Graphics `8086:0a26` is D0, but PCI D0 does not reveal graphics
RC6. Root port `00:1c.4` and the `8086:1547` bridge/device path report ASPM disabled
although their captured Link Capabilities advertise support. Other links have
L1 or L0s/L1 enabled. These are candidates for further read-only investigation,
not proof of a causal blocker or authorization to enable ASPM. Actual device
LTR values, graphics idle requests and platform permission remain unknown.
The firmware package limit remains permissive and unchanged (`0x408`).

The original 30-second idle/load/recovery phases complete with 250,523 load
chunks; covered package averages are 3.977/5.096/4.026 W and the logged temperature
peaks at 56 C. APs return to C1 outside the short coordinated test, so the later
phase measurements do not represent sustained C3 policy. No policy register,
integrity check, watchdog, timing bound or ordinary-startup behavior changed.

All 542 baseline files are preserved except expected `.srvros/meta` bookkeeping;
four new logs bring the volume to 546 files / 1,600 clusters. Offline fsck and
allocation checks pass with no leaks, mismatches or errors. The unchanged
65,536-byte exit tail retains all final proofs, with the original configuration
banner in same-session periodic logs. The 468-file automatic return backup was
hash-verified outside `build`. Current/console/accepted coordinated-C3 and other
protected payloads verify; the older standalone AP EFI copy was retired only
after verifying its durable payload. Ubuntu remains default, the one-shot is
consumed, collection is disabled and no reboot is armed.

Closeout consumes PR240 main `2e3ba64b` and checks its document-timing regression;
that newer runtime was not in this physical image. PR182 filesystem and PR19
Wi-Fi retain separate scope. Next: audit documented graphics/platform idle
permission and device-latency reporting with read-only diagnostics where safe.
Do not change power policy or declare a specific package-C3 blocker from these
PCI snapshots alone.

### Graphics/platform latency audit — active scope (2026-10-28)

Base `ac259fcb`, same local device branch. Extend the isolated package diagnostic
with documented read-only graphics/platform and device-latency observations.
First audit access semantics: do not force-wake graphics, enable decoding, alter
ASPM/LTR, acknowledge status or read undocumented power-gated MMIO merely to
produce a value. Explicitly report unavailable evidence where safe reads cannot
be established. Keep ordinary startup, 32 attempts, 20 ms bounds, HLT fallback,
firmware policy and integrity checks unchanged. Scope: diagnostic-only access,
bounded capability decoding, strict receipt/parser and host/VM regressions.
PR182 filesystem and PR19 Wi-Fi remain independent; no overlapping edits planned.
Next checkpoint: reviewed register/access contract and matched automated native
candidate, with the accepted package audit and console recovery preserved.

The access audit selects only Haswell uncore graphics status/RC6 residency and
ECAM-backed standard LTR capability values. RC_CONTROL/forcewake and PCODE remain
untouched; no validated direct platform grant register has been established, so
that signal is explicitly unavailable. E2 policy evidence stays distinct from a
grant. Shared PCI changes add a read-only extended API with no legacy fallback;
no existing write path or device power configuration changes. Host tests cover
ECAM refusal and full offsets, bounded malformed/overlapping LTR chains, graphics
BAR gates, counter-wrap/interval decoding and strict native receipt matching.

Candidate `a3ac64f7cd15293c` uses source `8e0f5bf6` (PR242). Matched ordinary and
diagnostic build, full core suite and focused host/parser tests pass. All nine
platform-profile VM cases pass, including requested peer failures, CPU-count
refusals, exact native refusal/save/return and missing workspace. Legacy AP-owner,
coordinated-C3 and package-only simulations pass without platform reads. Ordinary
kernel isolation, diagnostic disassembly and a maximum-width 25-device receipt
replay pass; no log or test limit changed. The physical USB preflight preserves
546 files / 1,600 clusters with zero allocation errors or leaks.

The next unattended run captures graphics/latency snapshots around the existing
32 coordinated attempts and 30-second idle/load/recovery phases, saves receipts
and returns to Ubuntu for automatic collection. No human input is required;
keep AC and USB connected. Graphics/platform diagnostic acceptance is pending.
Preserve accepted package-audit `bae0678ed7e9b38b` and console recovery. To make
EFI space, retire only the older coordinated-only `57274dfaccfdc0b9` installed
copy after verifying its durable payload; its native evidence remains retained.

### Graphics/platform audit — failed diagnostic return (2026-10-28)

Release `a3ac64f7cd15293c`, source `8e0f5bf6` (PR242 merge `b101e8eb`), saved
`session-00065eeb02087de2-00` and returned to Ubuntu. The platform audit explicitly
reports `fault=1 completed=NO`; the enclosing capture also reports failure.
Do not promote this image or treat automatic return as platform acceptance.

All 32 coordinated attempts finish within the unchanged 20 ms limit (maximum
9,982 us), with 32 HLT fallbacks and clean AP retirement. The first short round
lasts 23 us with zero deeper residency; the other 31 observe core C3 on both
physical cores and package C2. Package C3 remains zero. The three workload phase
completion receipts are present, but overall process/capture success is false
because of the platform audit failure.

Graphics status is `0x11800303` in both snapshots (RC6 state encoding). RC6 raw
count advances from 13,525,192 to 83,959,518. At the documented nominal 1.28 us
unit, this is 90.155937280 s versus a 90.058543 s snapshot timestamp interval.
That 0.108% mismatch independently violates the current parser's cross-clock
bound; do not clamp it to 100%, declare a calibrated residency ratio, or raise
the bound without investigating the counter/timebase and sampling semantics.
The raw counter/state observations do not identify a package-C3 blocker.

Four Intel root ports (`00:1c.1`, `.2`, `.4`, `.5`) are rejected after the first
extended header in both snapshots. Read-only Ubuntu inspection shows header
`0x20000000` at offset `0x100`: zero ID/version, next offset `0x200`. The walker
incorrectly rejects every zero ID/version. PCI-SIG defines ID zero as a valid
header-only Null Extended Capability, allows repeated instances, and assigns no
meaning to its version field. This is consistent with the native rejection;
the original native receipt did not record the rejected raw header. Correct
Null traversal with retained range/alignment/cycle/overlap bounds and a captured
fixture, rather than treating all malformed headers as acceptable.

The two Broadcom endpoints report stable LTR capability raw `0x10031003`, which
decodes to 3.145728 ms maximum snoop and no-snoop values. These are configured
capability values, not proof of a currently asserted latency request. Direct
platform permission remains unavailable; firmware E2/IRTL stay unchanged.

All 546 baseline files are preserved except expected `.srvros/meta` bookkeeping;
four new logs bring the volume to 550 files / 1,605 clusters. Offline fsck and
allocation checks pass with no leaks, mismatches or errors. The 446-file return
backup is verified outside `build`, and installed/protected payloads verify.
Ubuntu remains default, collection is disabled, one-shot consumed and no reboot
armed. The accepted package audit remains `bae0678ed7e9b38b`; current platform
candidate is failed. Closeout consumes PR243 main `4ae360ef`; that newer runtime
was not tested in this physical image. Next: fix/test Null capability handling,
investigate the RC6 timebase discrepancy, and repeat this same bounded audit
without changing power policy or existing limits.

### Null capability and RC6 timing retry — active scope (2026-10-28)

Base `02434766` (including PR245 clock-only workspace-save timing), same device
branch. Fix standard Null Extended Capability traversal without relaxing
alignment, range, cycle, overlap or version checks for non-Null capabilities.
Investigate the RC6/PM-time discrepancy with tightly bracketed MMIO reads and
ordered TSC observations; retain the existing nominal-unit mismatch predicate.
Scope is diagnostic-only PCI traversal, platform receipts/parser, regressions
and this unit's automated retry. PR182 filesystem and PR19 Wi-Fi are separate.
No power-policy, forcewake, watchdog, workload, 32-attempt, 20 ms, log-tail or
integrity limit changes. Previous failed platform evidence remains immutable.
Next checkpoint: reviewed regressions, exact-stage VM checks and self-merged
candidate, followed by one automated native run with Ubuntu recovery retained.

The existing failed-run power checkpoints span 83.998667 s and give an observed
TSC/PM ratio of 1,999.999738 ticks/us, consistent across successive intervals.
This does not identify RC6's timebase error, but does not support a corresponding
0.108% PM/TSC drift. The new two-sided PM/ordered-TSC brackets measure actual
MMIO placement. The strict original RC6 mismatch remains a failure, and its raw
timing diagnostic is saved independently of acceptance. A 90-second host clock
regression preserves fractional accumulation across repeated 24-bit PM wraps.

Retry candidate `8c9a5a28464ffd7c`, compiled source `ac343119` (PR246), passes the
matched build, full core suite/docs, all nine platform VM cases and three legacy
AP-owner/coordinated/package profiles. Directed failures retain expected fault
codes and terminal peer states. Disassembly confirms diagnostic isolation and
no added control writes. Version-2 timing plus 25 maximum-width device receipts
retain required proofs within the unchanged 64 KiB tail. USB preflight preserves
550 files / 1,605 clusters, with clean allocation and required headroom.

Read-only Ubuntu follow-up confirms each of the four root-port chains is Null
`0x20000000` at `0x100`, followed by terminal `0x0001001e` at `0x200`. Native
traversal and RC6 timing acceptance remain pending. The unattended retry uses
the same 30-second idle/load/recovery phases and 32 bounded C3 attempts, saves
results, and returns automatically. Keep AC/USB connected; no input is required.
Before installation, repoint current through the normal installer to accepted
package audit `bae0678ed7e9b38b`, then retire only the failed `a3ac64f7cd15293c`
EFI copy after durable verification to make space. Console recovery, accepted
payloads and the failed candidate's durable payload/evidence remain retained.

### Null traversal accepted; RC6 timing unresolved (2026-10-28)

Release `8c9a5a28464ffd7c`, source `ac343119` / PR246 merge `46b60b24`, saved
`session-00065eebb0420243-00` and returned automatically. All three workload
phases and the enclosing process/capture complete successfully. The runtime
platform receipt is fault-free with 25 devices; the independent strict collector
still rejects RC6 timing. This is partial acceptance, not an overall platform
audit pass or a reason to promote this payload over accepted package recovery.

The four affected root ports (`00:1c.1`, `.2`, `.4`, `.5`) now traverse two
extended headers in both snapshots and correctly report LTR capability absent.
No device reports a malformed/unavailable chain. This accepts the Null traversal
fix on this unit. The two Broadcom LTR capabilities remain `0x10031003` at
`0x1b0` with six headers. Platform grant remains unavailable; these configured
maximum latency values do not identify a currently asserted latency request.

Both graphics status observations remain `0x11800303` (RC6 encoding). The RC6
count advances 13,546,258→83,976,940: 90.151272960 s at the nominal 1.28 us unit.
The original timestamp span is 90.057986 s. Tight PM brackets around the actual
reads bound elapsed time to 90.057877–90.057879 s; each read bracket is 1 us wide.
The corresponding ordered TSC interval is 180,115,732,486–180,115,735,386 ticks,
or 1,999.999717–1,999.999793 ticks per PM microsecond. The nominal RC6 interval
still exceeds the upper PM bracket by about 93.394 ms (0.104%). Read placement
and PM/TSC disagreement do not explain this discrepancy. Its hardware clock or
counter semantics remain unresolved; do not clamp residency, substitute nominal
MHz as a TSC calibration, or relax the existing acceptance predicate.

All 32 coordinated rounds observe core C3 on both physical cores and package C2,
with maximum whole-round latency 9,982 us, 32 HLT fallbacks, no faults and clean
retirement of all three APs. Package C2 covers 99.5384% of its short brackets;
package C3 remains unobserved. E2 `0x408` and IRTL `0x8842` stay unchanged.

The 550 baseline files are preserved except expected `.srvros/meta` bookkeeping;
four new logs bring the volume to 554 files / 1,610 clusters. Offline fsck and
allocation checks pass with zero leaks, mismatches or errors. The automatic
447-file return backup verifies outside `build`; current and protected recovery
payloads verify. Ubuntu is default, the one-shot is consumed and the collector
is disabled. No new reboot is armed.

Closeout scope is this record/current diagnostic guide and evidence review only;
PR247 logger barriers, PR182 filesystem and PR19 Wi-Fi remain separate. Next:
audit RC6 counter scaling, clock tolerance and update semantics against primary
hardware documentation, then select a bounded read-only comparison if needed.
Do not change power policy, integrity checks or timing limits to force a pass.

### RC6 scaling/update audit — active scope (2026-10-28)

Base `00338707`, same device branch. Audit Haswell RC6 register scaling and
update/clock semantics against primary documentation; retain the existing
strict mismatch predicate. Add bounded read-only counter observations during
the existing capture if needed to distinguish sustained rate from endpoint
update behavior. Scope: isolated platform diagnostic, diagnostic-only sampling
hook/build, receipt parsing/tests and this record. No new worker or timer, no
forcewake/control access, and no policy, watchdog, workload, log or latency-limit
changes. PR247 logger barriers, PR182 filesystem and PR19 Wi-Fi do not overlap
this bounded scope. Next checkpoint: source audit, host/VM checks, self-merged
candidate and automated native repeat; preserve all prior failed evidence.

The audit confirms Haswell's documented 32-bit/1.28 us accumulator; public
register documentation supplies no clock tolerance or update cadence, and IGT's
software test allowance is not adopted. The new series retains every other
existing power checkpoint (up to 12 pairs), with no added worker/timer/control
access. It preserves the original mismatch test and saves observations on
failure. Bounded sampling, host/parser regressions and full-core validation
passed before integration, as did all nine VM cases on `02911e4b80d22cc3`.

PR247 merged during validation. Consume actual main `bb8561f7`, including the
logger-only committed-data flush and durable receipt, before staging the native
retry. This does not change normal sync/unmount integrity; rerun the matched
build, core suite and exact-stage VM coverage on the integrated candidate.
Pre-integration receipts remain separate and are not attributed to that image.

Integrated source `05668557`, release `0804f2944e9364d3`, passes the matched
build, full core/docs suite, isolation/disassembly checks, nine directed/exact
VM cases and three legacy profiles. The first legacy ownership VM exceeded the
unchanged 20 ms deadline (21,979 us) and retired safely; its failed receipt is
retained separately. The identical rerun passed. Maximum-width tail replay,
including the integrated logger's durability receipts, preserves all proofs
inside 64 KiB. No timing or integrity requirement was relaxed.

PR249 prepares the fully automated native retry with sparse paired RC6/PM/TSC
observations, normal idle/load/recovery phases, 32 coordinated rounds, HLT
fallback and automatic Ubuntu collection. The collector saves timing/series
diagnostics even if the original RC6 predicate still fails. Native acceptance
and the counter's hardware clock/update explanation remain pending. The local
run record binds this source/release to USB `6ADA-85BB`, its 554-file baseline,
1,610 allocated clusters, protected recovery and durable evidence backup.


### RC6 series return reviewed and repository synchronized (2026-10-31)

PR249 merged as `b1ba4e68`. Its release `0804f2944e9364d3`, compiled source
`05668557`, returned automatically with `session-00065eed045ce1e7-00`.
The matching exit SHA-256 is
`a08d7c9e8f5cc84c6c792d165b0d9c3cc1e7dd521affaec555f900265440529e`.
All three 30-second idle/load/recovery phases and final capture completed;
no halt, exception or incomplete runtime marker appears. The platform receipt
is fault-free with 25 devices, but strict platform acceptance remains failed.

Nine series points match the retained power timestamps, have continuous counter
arithmetic and unchanged immediate paired values. Six of the eight intervals
between series points exceed their upper PM bracket at nominal RC6 scaling;
two are below it. Including both endpoint intervals, eight of ten intervals
exceed the bracket. Thus this run's excess is distributed across several
intervals, rather than appearing only in the final endpoint. This does not
establish a precise update cadence or identify the counter's oscillator.
Nominal RC6 elapsed time is 90.143014400 s versus a PM bracket of
90.059109–90.059113 s, approximately 83.901 ms (0.0932%) excess before the
existing one-tick allowance. The original predicate still fails; no scaling,
tolerance or power-policy change is justified by this observation alone.

All 32 coordinated attempts completed under the unchanged 20 ms limit;
maximum whole-round latency was 9,984 us. The short initial attempt recorded
zero residency; the following 31 observed BSP core C3 and package C2.
Package C2 occupied about 99.5355% of the aggregate short brackets, while
package C3 stayed zero. All three APs retired without faults.

The 554 baseline files are preserved except expected `.srvros/meta` bookkeeping;
four logs bring the volume to 558 files / 1,615 clusters. Offline fsck and
allocation checks passed with zero leaks, mismatches or errors. Installed
current `0804f2944e9364d3`, console recovery `c35475f62f154f1e` and accepted
package payload `bae0678ed7e9b38b` verify against their durable backups.
The matching return evidence also verifies outside `build`. The one-shot is
consumed and automatic collection is disabled. Current remains a diagnostic
with unresolved RC6 acceptance, not a newly accepted replacement for recovery.

Repository catch-up adds this closeout and current-guide/index updates only.
Documentation and RC6 collector regressions are the applicable local checks;
no rebuilt image, new hardware test or native acceptance of the 94 newer main
commits is claimed. The October 31 workspace cleanup reclaimed empty allocation
from VM images with identical hashes; all native evidence and recovery remain.


### Ubuntu RC6 comparison — active scope (2026-11-01)

Updated the device checkout and local main to fetched `9340c9fb` (PR260),
consuming 66 commits since the October 31 closeout. PR261 owns ongoing desktop
lifecycle latency, PR252 resolver/socket and PR19 Wi-Fi; this scope touches none
of their runtime files. Capture three passive 90-second Ubuntu i915 RC6 windows,
with sparse sysfs reads bracketed by raw/adjusted monotonic clocks.
Scope: a read-only host capture tool, its evidence/parser checks and Makefile target, this record
and the diagnostic guide. No SrvrOS power policy, counter conversion, timing
predicate, boot payload or reboot state changes. The millisecond sysfs interface
and Linux-managed graphics workload differ from raw SrvrOS MMIO/PM brackets;
record those limits before drawing cross-OS conclusions. Publish the baseline
and a bounded next diagnostic after review, without accepting newer source on
this hardware merely because it was fetched. No new physical boot is armed.


### Ubuntu RC6 baseline captured (2026-11-01)

Host-only helper source `9aecef5e`, Ubuntu kernel `7.0.0-38-generic`, i915
`8086:0a26`, TSC clocksource. Three passive 90-second windows retained all
28 bracketed samples, with RC6 enable state 1 before/after and runtime state
active. Raw-clock spans were approximately 89.9964/89.9961/89.9961 seconds;
RC6 advances were 39.356/31.473/36.602 seconds (43.73/34.97/40.67%). All
27 ten-second intervals stayed below elapsed time, including the rounded
endpoint annotation. Total RC6 time was 107.431 seconds over about 269.9886
raw seconds. Maximum sysfs read bracket was 606,052 ns. Adjusted versus raw
monotonic elapsed time differed by about 42.27 ppm; these are Linux clock
observations, not an independent calibration of SrvrOS's PM timer.

This capture does not reproduce SrvrOS's small overcount, but its much lower
RC6 fraction can hide such a discrepancy. It cannot accept the native timing
predicate or distinguish hardware scaling from publication/OS behavior. Linux
continues managing graphics/runtime power; the helper itself writes no policy
or MMIO controls. All original SrvrOS limits and failed evidence remain intact.

A separate read-only turbostat context capture, three ten-second intervals,
reported package C3 at 14.04/13.23/14.41%, core C7 at roughly 49–51%, and
package C6/C7 at zero. CPU busy was 25.18/27.15/27.50%; this was the normal
Ubuntu desktop, not controlled idle. Graphics RC6 was 42.71/43.79/44.57%.
These are separate observations, not simultaneous evidence for the earlier
windows. Positive package C3 establishes that this physical Mac can reach C3
under Linux's own policies/device state; it does not identify the missing
SrvrOS prerequisite or authorize copying Linux control writes.

`make test-linux-rc6-baseline`, ten existing RC6 parser regressions, docs and
whitespace checks pass. Raw observations, upstream source references, tool hash
and review are backed up with hashes outside `build`. No boot payload changed
and no reboot is armed. Next power work should compare package-C3 prerequisites
and display/device state with this Linux reference while preserving the
unresolved RC6 mismatch. A matched current-main build and native desktop
regression remain separate work, without inherited physical acceptance.


### Cross-OS package-C3 prerequisites — active scope (2026-11-01)

Base `e339b9a8`, same clean device checkout. Compare Linux CPU idle configuration,
read-only known-supported package MSRs, kernel-owned graphics/display reports and
PCI PM/link/LTR snapshots with the saved SrvrOS release `0804f2944e9364d3`.
Reuse the existing bounded PCI decoder for matching fields. Scope: host-only
capture/comparison helpers, fixtures, Makefile checks and this unit/current
power guide. PR261 desktop admission, PR252 sockets and PR19 Wi-Fi remain
independent. No shared runtime or native register allowlist changes, forced
wakeups, control writes, power-policy changes or new boot in this audit.
Preserve unknowns and choose a bounded follow-up from captured differences;
Linux package C3 alone does not identify a SrvrOS blocker.


### Package-C3 prerequisite comparison captured (2026-11-01)

Based on `e339b9a8`, two host-only comparisons against native source `05668557`,
release `0804f2944e9364d3`, preserve the exact exit hash recorded in the October
31 closeout. All 25 BDF/identity/class matches and comparison results repeat.
No missing/unreadable devices or MSR drift occurred. Linux's four CPUs all have
package configuration `0x408`, C3 IRTL `0x8842` and misc-enable `0x850089`,
matching the native capture. Linux C3 descriptors use the same MWAIT `0x10`.
This removes those sampled configuration differences from the current shortlist,
without proving identical scheduling or every platform prerequisite.

ASPM, LTR enable/support and clock-PM enable/support match across all 25
functions. Independent extended-capability review also matches all paired
native LTR states, offsets, values and header counts, including the two
Broadcom `0x10031003` values. These are configured maxima, not observed latency
requests. Common Clock Configuration differs at SSD root port `00:1c.4` and
Thunderbolt functions `05:00.0`, `06:00.0`, `07:00.0` (Linux set, native clear).
Other raw interrupt/link/status fields differ and are retained; no link change
is proposed from these snapshots.

Seven Linux functions are D3hot/runtime suspended where native PMCSR was D0:
both audio controllers `00:03.0` and `00:1b.0`, root port `00:1c.0`, and
Thunderbolt downstream ports `06:03.0`–`06:06.0`. Their surrounding Linux
sysfs states were retained because configuration reads can invoke kernel
runtime PM. Driver ownership, active DMA/interrupts and teardown requirements
must be audited before any proposed transition; this is candidate evidence,
not a causal result or permission to copy Linux control values.

Cached Linux display state has only the 1366×768 eDP connector enabled,
pipe A/eDP domain references, and zero software references to the `display`
power well/audio domains. There is no matching native display-power-well
snapshot; reference counts alone do not establish hardware-off state. An early
exploratory `drpc` read is excluded from passive evidence after source review
showed its kernel forcewake/PCODE read path. The retained display comparison
uses cached reports instead. No native register allowlist is expanded here.

The host helper reuses the existing bounded PCI decoder and performs only
kernel-mediated read access. Five new comparison/adapter regressions, existing
package/latency/collector tests and the Linux RC6 helper tests are the applicable
validation. Raw snapshots, source references and reviewed comparison remain in
ignored local storage with a verified durable copy. See the current AP audit
guide for the field table and caveats.

Next: validate and add a narrowly gated read-only native display/power-well
snapshot to the existing bounded C3 trial. Keep device ownership analysis
separate from any future D-state/link policy experiment. Package C3 remains
unobserved in SrvrOS, and its RC6 timing failure remains open. No boot image,
power policy, integrity check or limit changed, and no reboot is armed.


### Read-only native display-well snapshot — active scope (2026-11-01)

Base `71686249`. Add four read-only Haswell power-well observations at
`0x45400`–`0x4540c` to the existing isolated platform capture endpoints. Retain
exact Air6,1 CPU/GPU/PCH/DSDT admission and map only one additional uncacheable,
read-only/non-executable page. No forcewake, PCODE, control writes, new workers,
waits, polling or per-round work. Keep the existing 32 attempts, 20 ms limit,
HLT fallback, RC6 scale/predicate, USB integrity and log limits unchanged.
Scope: isolated platform diagnostic, independent observation parser/tests,
current guide and this unit's automated trial/evidence. PR261 desktop admission,
PR252 sockets and PR19 Wi-Fi do not overlap these runtime files. Validate
source semantics, mapping/refusal and receipts, full matched build/core and
exact-stage VM before install. Prepare the unattended native trial with visible
progress and automatic save/Ubuntu return, preserving known-good recovery.


### Display-well candidate validated (2026-11-01)

Final compiled source `dff1f18f`, release `c6fd6d4c55a57566`, adds the four
Haswell display-well endpoint reads with exact model admission and D0 rechecks
before both MMIO endpoints. Version-4 receipts retain BIOS/driver/KVM/debug
request/status values and separate PM brackets. The independent observation
parser saves useful display evidence even if the original RC6 timing predicate
still fails. There is no power-control, forcewake/PCODE or measured-round change.

Matched ordinary/AP build, USB image, full core/docs suite and isolation/
disassembly checks pass. All nine directed/exact-stage VM cases and three
legacy profiles pass on this final candidate. Focused tests cover occupied
addresses, partial-map failures, read-only/NX/cache flags, D0/D3/missing-PM/
malformed/absent-device refusal, bounded ordered endpoint reads, all-ones data,
missing/duplicate/misordered receipts and observations retained on RC6 failure.
Sparse checkpoints never read display wells. Maximum-width replay with 25 PCI
rows, 12 series pairs, both well endpoints and durability padding preserves all
original proofs inside the unchanged 64 KiB tail (7,037 added bytes).
Earlier pre-D0-recheck VM receipts remain separate and are not substituted for
final-candidate validation. These checks do not physically accept the new reads.

The unattended trial retains 30-second idle/load/recovery phases, 32 coordinated
attempts, 20 ms limits and HLT fallback. It shows progress, saves observations
and returns to Ubuntu for automatic collection. No human input is required;
keep AC and this unit's USB connected. The 558-file / 1,615-cluster baseline,
exact-stage receipt, manifest and durable payload/evidence are bound in the
local run record. Console recovery `c35475f62f154f1e` and accepted package
payload `bae0678ed7e9b38b` remain protected. Retire the failed RC6 candidate's
EFI copy only after durable verification and normal installer fallback.

PR264 owns this isolated diagnostic. Native results, hardware well state and
the package-C3 blocker remain pending the matching physical return. An enabled
well or a firmware request is not permission to clear it. Keep the unresolved
RC6 predicate, device ownership/teardown requirements and all limits intact.


### Native display-well observation; exit proof incomplete (2026-11-01)

Closeout scope is evidence review and this record/current guide/index only,
based on `bbbdead1` / merged PR264. Release `c6fd6d4c55a57566`, source
`dff1f18f`, returned with `session-00065f4155a6df61-00`. Exact exit SHA-256:
`9395307a5dca53fc403d3343752b3e4fb98b33a5a07874afa3eed31c3a876fcb`.
The overall trial remains failed: the original RC6 timing bound still fails,
and the required exit-only workload/AP-owner evidence is incomplete.

Both valid display endpoints report `0xc0000000` for BIOS and `0x40000000`
for driver/KVM/debug. The BIOS request bit is set; the other request bits are
clear, and every view reports the well enabled. The PM read brackets are
2,204,793–2,204,795 us and 92,251,505–92,251,507 us. Source fault is zero and
the runtime capture completes. This records bounded native request/status
observations; it does not establish uninterrupted state between endpoints,
identify the package-C3 blocker or authorize releasing firmware ownership.

`exit.log` is exactly 65,536 bytes and begins during the load phase. It lost the
idle-phase proof and the initial AP-owner header. Consequently `load_result`
and `ap_c3_result` fail on that file; the trial must not be promoted to a pass.
The saved periodic logs retain the beginning. For diagnostic review only,
`session-b.log` and `exit.log` have a unique, byte-identical 30,688-byte overlap.
Joining that proven overlap yields a 93,633-byte continuous stream. The original
raw logs and their hashes remain unchanged; this reconstruction does not replace
the failed exit-only requirement or relax its parser.

The full stream confirms all three workload phases and 32 C3 attempts completed,
maximum whole-round time 9,983 us, no faults and terminal AP retirement.
The first short attempt has zero residency; the next 31 observe BSP core C3
and package C2. Aggregate C2 is about 99.5349%; package C3 remains zero. Eight
RC6 series pairs have continuous arithmetic. Nominal RC6 is 90.117999360 s
versus a PM bracket of 90.046711–90.046714 s, approximately 71.285 ms (0.0792%)
excess before the existing one-tick allowance. The strict bound remains failed.

The earlier tail replay used the older accepted package trace. It omitted
newer main's `io-owner` checkpoint output (`72454534`), which adds 7,701 bytes
in this native stream. Existing worker reports contribute another 9,533 bytes;
they were already present in the older trace. Thus the old replay's passing
7,037-byte addition was not representative of the current complete output.
The next diagnostic fix must preserve required receipts inside the unchanged
64 KiB tail using current-source logging, without dropping checks or accepting
reconstructed evidence as an exit-log pass. PR261 owns active desktop/I/O work;
this closeout does not alter its shared logger/scheduler implementation.

All 558 baseline files are preserved except expected `.srvros/meta` bookkeeping;
four new logs bring the volume to 562 files / 1,620 clusters. Offline fsck and
allocation pass with zero leaks/mismatches/errors. Current, console recovery and
accepted package payloads verify against durable backups; 484 automatic return
backup files verify before this review is added. Ubuntu is default, one-shot is
consumed and collection is disabled. No new boot is armed.

Next: fix source-matched receipt coverage first. Then audit active display
resources and firmware handoff before any experiment releasing the BIOS request;
keep controller D-state/common-clock candidates separate. Power policy, integrity
checks, original limits and the failed RC6 evidence remain unchanged.


### Final receipt retention — active scope (2026-11-01)

Base `1a0c9085`. Fix the failed exit-tail coverage by deferring the original CPU
phase acceptance records and frozen AP admission receipt to their final reports.
Keep all live progress, sensor checks, I/O-owner/worker diagnostics, phase timing,
workload and strict parser formats; preserve failure markers and partial progress.
No tail-size, watchdog, power-policy, ownership or integrity change. Scope:
`userspace/threadstress/load_capture.c`, isolated `kernel/diagnostics/ap_probe.c`,
focused fixtures and source-matched replay, unit/current diagnostic docs. PR261
owns shared logger/scheduler/desktop changes; this fix does not edit those
subsystems. PR252 sockets and PR19 Wi-Fi remain independent. Publish/test the
bounded fix, validate a matched image/VM and prepare the automated repeat with
normal recovery and collection. The earlier RC6 failure remains independent.


### Final receipt candidate validated (2026-11-01)

Compiled source `eda7541e`, release `98bced62ca7be424`, retains the original
three completed CPU phase records and validated AP admission fields for their
final reports. Live progress, partial failure progress, all sensor/clock checks,
worker/I/O diagnostics and strict receipt grammars remain. Successful CPU phase
receipts appear only after all three phases succeed; AP admission is frozen
before startup and reported before actual retirement results. No scheduler/
logger edit, hardware access, policy, ownership or limit change belongs here.

Matched ordinary/AP build, USB image, full core/docs and isolation/disassembly
checks pass. Twelve main/legacy VM cases pass, with actual receipt ordering,
startup/final admission equality and no successful CPU receipts on sensor refusal
verified. Both positive Air/Pro CPU fixtures and twelve refusal/stop paths pass
final-only emission assertions. The strict AP parser rejects progress-only
admission. The source-matched failed-stream regression preserves every critical
record byte-for-byte in the original 64 KiB tail with 128 KiB extra pre-final
chatter and 29,813 bytes of conservative receipt width/capacity padding. Old
layout coverage fails. Replay/model output is explicitly synthetic and cannot
accept this or the failed native run. Original strict parsers are unchanged.

The automated retry uses the same 30/30/30-second phases, 32 coordinated
attempts, 20 ms limit and HLT fallback. The return collector now writes a direct
`exit-receipt-coverage.json` before the separate unchanged RC6/platform assertion;
no reconstruction or modeled record can satisfy native exit checks. Independent
display observations and original RC6 failure evidence remain available.

The run record binds this unit's 562-file / 1,620-cluster USB baseline, exact-stage
receipt, manifest and protected recovery. No input is needed; keep AC/USB
connected for progress/save/automatic Ubuntu return. PR266 owns this bounded
fix; PR261's shared logger/desktop work remains separate. After return, verify
the actual exit file first. Native retention acceptance and the package-C3/RC6
issues remain pending; no power transition is authorized by this receipt fix.


### Native final-receipt retention accepted; RC6 still fails (2026-11-01)

Merged PR266 (`25a0f342`), compiled source `eda7541e`, release
`98bced62ca7be424`, returned automatically with
`session-00065f427a21a2ac-00`. The 65,536-byte exit file has SHA-256
`cb238c7227d91d85b2a032fa1b284c50a5079b0ed3108e2505aa14ff3c15d50d`.
All required phase/admission/power/idle/C3/package/display proofs now validate
**directly from this file**, using the unchanged strict parsers. No joining,
modeling or synthetic padding participates in native acceptance. This accepts
the receipt-retention fix only; the overall platform assessment remains failed.

The three 30-second idle/load/recovery phases complete, with covered package
power of 4.102 / 5.134 / 4.107 W. All 32 coordinated rounds complete within the
unchanged 20 ms bound; maximum whole round is 9,983 us. After the initial zero
round, 31 observe BSP core C3; package C2 is about 99.5359% over the short
round brackets. Package C3 remains zero. All three APs retire terminally with
zero faults. These observations do not accept ordinary SMP or a new power policy.

The original RC6 predicate still fails: nominal RC6 is 90.142353920 s versus
PM elapsed 90.048071–90.048074 s, about 94.279 ms (0.1047%) excess before the
existing one-tick allowance. Eight sparse series samples remain continuous;
no tolerance or scaling change is made. Both display endpoints again report
BIOS `0xc0000000`, driver/KVM/debug `0x40000000`, source fault zero and completed
capture. Firmware ownership and causality remain unproven.

All 562 prior files are preserved except expected `.srvros/meta` bookkeeping;
four new session logs bring the USB to 566 files / 1,625 allocated clusters.
Read-only fsck and allocation pass with zero leaks/mismatches/errors. Installed
candidate, console recovery and accepted package payloads match durable copies;
462 automatic return evidence files verify before review additions. Ubuntu is
default, the one-shot is consumed and automatic collection is disabled. No new
boot is armed. Raw logs, direct parser results and failed overall assessment are
retained locally and backed up outside `build`.

Next: audit active display resources and firmware handoff read-only, preserving
all policies, limits and ownership checks. Receipt coverage is resolved; RC6
clock semantics and the package-C3 blocker remain separate open questions.


### Display-resource and firmware-handoff audit — active scope (2026-11-01)

Base `a6b9e1b7` / PR267. Audit the inherited framebuffer path and validated
Haswell display-domain dependencies against pinned Linux v7.0 sources and this
unit's cached evidence. Scope is source/evidence review and current diagnostic/
unit documentation; no MMIO probing, firmware call, register write or power-policy
change. Define a bounded native read-only resource capture only after identifying
its register access prerequisites. PR261 desktop/I/O, PR252 sockets and PR19
Wi-Fi remain independent. Keep the accepted receipt fix, failed RC6 bound and
zero package C3 separate. No reboot or new payload is armed by this audit.


### Display-handoff source audit complete (2026-11-01)

Source/evidence review on base `a6b9e1b7` confirms the inherited framebuffer
continues scanout use, while SrvrOS has no complete Haswell display-resource
adoption/release model. Limine v11.4.1 source `1276c440` matches the pinned binary
release; ExitBootServices is not a display-resource retirement receipt.
Pinned Linux v7.0 holds initialization references during state readout, transfers
a BIOS request to a driver request without a request gap, and sanitizes unused
wells after pipe/encoder ownership is established. No part of that control
sequence was executed on this machine during the audit.

The Haswell switchable global well excludes pipe A, transcoder eDP and DDI A,
but includes all three panel fitters, B/C pipes, A/B/C transcoders, other listed
ports and display audio/VGA. An internal panel is therefore insufficient to
explain the BIOS request. Cached Ubuntu routing cannot stand in for inherited
native routing; native eDP input selection and enabled dependent resources are
still unknown. Lack of GT forcewake does not establish off-domain MMIO safety.

The current guide records the next explicit endpoint capture contract: fixed
read-only allowlist, validated access prerequisites, independent observations,
no extra power-hook/round reads, strict malformed/refusal fixtures, current-source
tail coverage and exact-stage rehearsal. This audit changes documentation only.
No new image, register access, power policy or reboot is introduced. Keep the
physically accepted receipt fix and failed RC6/package-C3 evidence unchanged.
Local source hashes/review live under the ignored display-handoff evidence root
and are backed up outside `build`.


### Read-only native display controls — implementation scope (2026-11-01)

Base `9c23bf4d`. Add an exact-Air6,1 26-control endpoint inventory to the existing
explicit platform diagnostic: A/B/C/eDP transcoder controls, primary/sprite/cursor
A/B/C, panel fitters A/B/C, DDI A–E and VGA. Preserve firmware requests; audio
and interrupt registers remain outside this initial allowlist. Scope is isolated
platform producer, strict independent parser, host/refusal/tail fixtures and
unit/current guide. No shared scheduler/logger or PR261/252/19 scope changes.
Validate exact read-only maps, D0/BAR/decode and powered-well gates, non-atomic
pre/post observations, final receipts and all unchanged limits before staging.


### Display-control candidate validated (2026-11-01)

Compiled source `78fbea3c`, release `6ac139e77c5d3070`, adds the 26-control
endpoint inventory specified by the current guide. Twelve exact private pages
are read-only/NX/cache-disabled. GPU identity, BAR, decode, D0 and all four well
views are revalidated; off domains and invalid/changed data are explicit failure
or unknown evidence. The raw fixed-order records preserve every value while
avoiding the per-control verbosity that failed the maximum tail check. No
firmware request, power policy, timing limit, ownership or integrity check changes.
Audio/interrupt inventory remains outside this first allowlist.

Full matched ordinary/AP build, USB image, core suite and docs pass. Producer
fixtures exercise every occupied destination and map failure, off-domain skips,
wrong identity/BAR/decode/D0, all-ones and changing well state. Fourteen platform
parser tests and the three retention tests pass. Source-matched latest native
output with explicitly synthetic new resource records preserves every critical
record byte-for-byte under 128 KiB additional pre-final chatter and 30,405 bytes
of width/capacity padding in the unchanged 64 KiB tail. Synthetic records are
never native acceptance. The strict original RC6 failure remains a failure.

All nine main VM cases and the three legacy owner/C3/package profiles pass.
Actual final-admission ordering and equality plus failure receipt behavior are
checked across all twelve. Ordinary-kernel symbol isolation, diagnostic AP and
platform disassembly, no direct control-write operations and sparse power-hook
isolation also pass. The exact native-stage GPT rehearsal saves/unmounts/returns
on its expected Q35 native refusal; missing Home halts without unsafe reset.
These VM results do not accept the new native MMIO observations.

The automated trial retains 30/30/30-second phases, 32 coordinated rounds,
20 ms whole-round bounds and HLT fallback. The return collector saves independent
resource and RC6 observations, requires complete direct exit-file evidence and
preserves the unchanged overall platform predicate. Keep AC and this unit's USB
connected; no input is required. The USB baseline is 566 files / 1,625 clusters,
with all prior hashes, headroom, fsck and allocation verified. Console recovery
and accepted package/stress/reopen payloads remain protected. Native resource
routing and dependency observations remain pending the matching return.


### Native display-control inventory accepted; platform still fails (2026-11-01)

Closeout scope is direct native evidence and unit/current guide/index updates,
on tested integration `7ee6a048` / PR269. Compiled source `78fbea3c`, release
`6ac139e77c5d3070`, returned automatically with
`session-00065f432ef9068c-00`. Its 65,536-byte exit log SHA-256 is
`2a15db620223b35c077fa4e84d7c773f0fda1d780d21f2f6cc8bf61cf98548ac`.
All original phase/admission/power/idle/C3/package/display proofs and the new
resource records validate directly from this file, without reconstruction or
modeling. This accepts the bounded read-only resource capture, not the overall
platform test or any display-power policy.

Both endpoints have 26 valid identical raw values, source fault zero and complete
capture. Resource PM brackets are 2,234,663–2,234,678 us and
92,285,753–92,285,764 us. Both pre/post well sets retain BIOS `0xc0000000` and
driver/KVM/debug `0x40000000`. eDP config `0xc0000018`, eDP DDI function
`0x82200000` and DDI A `0x80000011` show the eDP transcoder enabled with pipe-A
input and DP-SST mode through DDI A. Primary A (`0x98004400`) is enabled.
Transcoders A/B/C, primary B/C, all sampled sprites/cursors/fitters, DDI B–E
and VGA are disabled according to their enable/mode fields. Nonzero reserved
or unrelated fields remain raw evidence, not inferred resource activity.

The sampled routing does not expose an enabled resource from the listed
switchable-well dependencies. This narrows that inventory; it does not prove
the well unused, establish DMA/interrupt/firmware ownership or identify a causal
package-C3 blocker. Audio and interrupt control inventory remains outside this
first allowlist. No request bit, power policy or timing limit was changed.

All three 30-second phases complete, with covered package power
4.020 / 5.133 / 4.045 W. All 32 coordinated rounds observe BSP core C3 and remain
within the unchanged 20 ms bound; maximum is 9,983 us. Package C2 is about
99.5376% over the short brackets; package C3 is zero. All three APs retire
terminally without faults. The original strict RC6 bound still fails: nominal
90.149831680 s versus PM elapsed 90.051091–90.051093 s, about 98.739 ms
(0.1096%) excess before the existing one-tick allowance. Nine series samples
remain arithmetically continuous. Failed overall assessment is preserved.

All 566 prior files are preserved except expected `.srvros/meta`; four new logs
bring USB to 570 files / 1,630 clusters. Offline fsck/allocation pass with zero
leaks/mismatches/errors. Installed candidate, console recovery and accepted
package match durable payloads; 479 automatic return evidence files verify
before these review additions. Ubuntu is default, the one-shot is consumed and
collection is disabled. No new boot is armed; raw logs and review are backed up
outside `build`.

Fetched main `466f4743` now includes PR261's shared scheduler/logger/storage/
lifecycle changes. This physical trial predates those changes and does not
accept them. Consume and locally validate that integration before another stage;
repeat source-matched tail coverage using the changed logging source. Next
hardware scope remains the missing display-audio/interrupt dependencies, with
firmware ownership and the RC6 discrepancy kept separate. PR252 and PR19 retain
their independent scopes.


### PR261 integration and remaining dependency audit — active scope (2026-11-02)

Base `95ddb8d6` includes merged PR261. Rebuild and validate this unit's matched
ordinary/AP tree, core/VM refusal/lifecycle paths and retention budget before any
new native stage. New I/O-gate and filesystem-cost checkpoint reports require
explicit current-source sizing; the older native trace is not source-matched.
Scope: bounded regression tooling and evidence/docs, plus source-only audit of
remaining display audio/interrupt controls and driver ownership. No shared
scheduler/storage/logger edits, register accesses, power changes or new native
capture are implied. PR252 sockets and Pro PR19 Wi-Fi remain independent. Keep
installed release `6ac139e77c5d3070`, its failed RC6 assessment and recovery intact.


### Integration validated; maximum checkpoint budget fails (2026-11-02)

Runtime integration base `95ddb8d6` / PR261 passes matched ordinary/AP build,
USB image and full core tests locally. Additional source-guard/budget tests and
the original receipt tests pass. Baseline smoke passes syscall ABI, all 482 shell
commands, shell editing, UEFI USB input and displayd; guest filesystem checks
report no errors/leaks. Ordinary/AP disassembly/symbol and sparse-hook isolation
pass. Source `5ea8b3aa` (host tooling/docs changes only after that runtime base)
stages VM-only release `4853b3e956523a10`: simulation, exact native refusal with
save/unmount/return and missing-workspace halt all pass on GPT. Missing Home is
an explicit expected halt, not a normal-completion JSON or a native acceptance.

The old native trace is still rejected against changed logger/scheduler source.
The strict replay guard now additionally fingerprints `exfat.c` and `exfat.h`,
covering report formats and capacities changed outside the logger. A separate
explicitly synthetic source-derived addition model adds nine gate rows, one gate
summary, four cost rows and sixteen phase rows: 9,056 bytes at conservative
maximum widths. Against the preserved native exit proofs and unchanged stress
padding/tail size, required critical records are evicted. This failure is retained;
it does not mean this source was booted on the Air or that the old native run
failed resource capture. No model substitutes for a matching new native trace.

The source-only dependency audit identifies display HDA as distinct from PCH
CS4208 playback, rejects ordinary audio initialization as a read-only probe,
and records graphics-audio and DE/SDE mask/enable candidates for later access
review. Display audio, interrupt, DMA and firmware teardown ownership remain
unmeasured. No register read, control write, policy or limit change is added.

PR271 publishes the audit/regression tooling. Resolve final-proof retention while
preserving every sample, failure marker and checkpoint before extending the
inventory or installing another native candidate. VM-only stage is not approved
for native use; its manifest remains intact and `latest` is restored to installed
`6ac139e77c5d3070`. Installed candidate, console recovery and accepted package
verify, with 483 prior native evidence files rechecked. Ubuntu remains default,
no one-shot/reboot is armed and no install occurred. New audit/test evidence is
backed up outside `build`; prior RC6/package-C3 failures remain unchanged.


### Compact final telemetry — active scope (2026-11-02)

Base `c9235491`. Preserve all retained CPU power, idle-residency and idle-entry
fields in a versioned fixed-order encoding, replacing repeated field names only.
Legacy logs remain readable through the same strict semantic checks; malformed,
mixed, duplicate, missing and overflowed records refuse coverage. Scope is the
capture-only power reporter, host decoding/retention tools and fixtures/docs.
No logger/scheduler/storage edits, sampling/phase/attempt changes, discarded
checkpoint output or power/control writes. PR272 lifecycle, PR252 lifetimes and
PR19 Wi-Fi remain independent. Require the current-source added-checkpoint
maximum budget to pass before staging a native retry.


### Compact final telemetry candidate validated (2026-11-02)

Compiled source `8f0c5aa6`, release `0bc5f7e0298ca886`, emits indexed format-2
power/idle/entry records only at final reporting. Every original numeric field,
including duplicate timestamps, remains. Live progress, ordinary status output,
24-sample capacity, cadence, phase timing and 32-round / 20 ms bounds do not
change. No new logger/scheduler/storage code, ownership change, hardware access
or power/control write is introduced.

The strict host decoder rejects missing/duplicate/unknown headers and rows,
wrong indices/field counts, overflow, unsigned-width errors and mixed encodings,
then applies the original semantic power/idle/entry/prerequisite/MWAIT/RC6-series
checks. Producer tests validate every actual formatter vararg against retained
C snapshots. Four codec tests cover exact canonical roundtrips, identical parsed
results, power-only unavailable audits, malformed records and preserved failures.
Existing legacy parser/receipt/budget tests pass. No native collector invokes
the regression-only legacy conversion model.

The preserved prior native exit round-trips byte-for-byte and produces identical
strict parsed results after modeled conversion. Including all 9,056 bytes of
current maximum added checkpoint output, legacy layout fails while compact
layout preserves every critical record byte-for-byte in 64 KiB with 128 KiB
pre-final chatter and 25,995 bytes of width/capacity padding. Schema-aware padding
uses the original field bounds and the new 0–23 index bound. Models remain
synthetic, never physical acceptance; original RC6 timing still fails.

Matched ordinary/AP build, USB image, full core/docs and final focused tests
pass. Isolation/disassembly and sparse power-hook checks pass. All nine main
VM cases, three legacy owner/C3/package profiles and twelve-case final receipt
ordering/failure review pass. Exact staged GPT native refusal saves/unmounts/
returns; missing workspace halts without unsafe reboot. These VM/host results
validate the implementation and recipe, not the new native format's acceptance.

The retry retains the same automated 30/30/30-second phases and read-only
26-control platform inventory. Its collector additionally requires one actual
format-2 audited/idle header and complete direct decoding, while every original
platform/RC6 predicate remains. USB baseline is 570 files / 1,630 clusters.
Preserve all prior hashes and console/accepted payloads; no input is required,
and automatic progress/save/Ubuntu collection remain. The run record binds exact
stage, source, install/recovery, checks and ready/armed state. Native acceptance
is pending the matching return; do not accept synthetic model records as evidence.


### Native compact final-proof retention accepted (2026-11-02)

Merged PR273 (`e1c05956`), compiled source `8f0c5aa6`, release
`0bc5f7e0298ca886`, returned automatically with
`session-00065f52588bba63-00`. The 65,536-byte exit file SHA-256 is
`6873738dcf5a5dad0faa44a860264112f714611ea17308b5bd5432e620648ee8`.
One actual `v=2 idle=1 entry=1` header and 15 rows of each p2/i2/e2 kind retain
all samples. Direct schema decoding and original strict semantic checks accept
every required workload, power/idle/entry, prerequisite, MWAIT, AP owner/C3,
package, display-well and 26-control proof. No legacy conversion, reconstructed
value or joined log is used. This accepts bounded compact retention and its
coexistence with the PR261 checkpoint output, not full desktop/endurance or
overall platform acceptance on the new integration.

The fresh source-matched replay uses this actual native exit and all nine
logger/checkpoint/capture/ordering source fingerprints. Every critical record
survives unchanged in 64 KiB with 128 KiB extra pre-final chatter and 26,738 bytes
of width/capacity padding. That stress replay remains synthetic regression
coverage; the actual direct exit checks above provide the physical evidence.

All three phases complete, with covered package power 4.025 / 5.066 / 4.059 W.
All 32 coordinated rounds observe BSP core C3, with maximum whole-round time
9,983 us under the unchanged 20 ms bound. All three APs retire without faults.
Package C2 is about 99.5413% over the short brackets; package C3 remains zero.
All 26 display-control values are identical at both endpoints. The BIOS well
request remains set; the new encoding neither changes nor resolves ownership.

The original RC6 bound still fails: nominal 90.139143680 s versus PM elapsed
90.052151–90.052154 s, approximately 86.990 ms (0.0966%) excess before the
existing one-tick allowance. The failed overall assessment is preserved; no
scale, tolerance, policy or limit changes. The next dependency capture remains
separate from RC6 clock semantics and any later power-well control experiment.

All 570 prior files are preserved except expected `.srvros/meta`; four new logs
bring USB to 574 files / 1,635 clusters. Offline fsck/allocation pass with zero
leaks/mismatches/errors. Installed candidate, console recovery and accepted
package payloads match durable copies; 471 automatic return evidence files
verify before review additions. Ubuntu remains default, one-shot is consumed,
collection is disabled and no reboot is armed. Review/raw evidence is backed
up outside `build`. Next: the audited read-only display-audio/interrupt inventory,
with exact access gates and current-source compact retention checks retained.


### Display-audio/IRQ endpoints — active scope (2026-11-02)

Base `88c56afa`. Add eight fixed readbacks to the explicit exact-Air6,1 platform
diagnostic: HSW audio pin/ELD plus A/B/C config and DE/SDE mask/enable controls.
Require captured LPC identity/class/BDF before/after alongside the existing GPU
identity/BAR/D0/decode/well gates. No HDA initializer, verbs, data-window or IIR
access, acknowledgement, power request or IRQ-mask write. Scope: isolated
platform producer, independent strict parser, mapping/failure/retention fixtures
and current docs. PR272 lifecycle, PR252 lifetimes and PR19 Wi-Fi remain separate.
Keep all policies, limits, samples and compact proof requirements unchanged.


### Read-only audio/IRQ candidate validated (2026-11-02)

Compiled source `eceb07fc`, release `1848dd7c663eb738`, adds two eight-control
endpoint observations to the existing exact profile: graphics pin/ELD and A/B/C
audio config, CPU DE mask/enable and PCH SDE mask/enable. Three fixed private
read-only/NX/cache-disabled pages are used. Captured LPC `8086:9c43`, 00:1f.0,
class `060100` is checked before/after alongside GPU BAR/decode/D0 and unchanged
well values. IRQ all-ones masks remain valid raw values; unavailable or changed
access never decodes as disabled hardware. No HDA/codec transaction, IIR read,
status acknowledgement, mask write, ownership transfer or power request occurs.

Matched ordinary/AP build, USB image, full core and docs pass. Focused producer
fixtures exercise occupied pages, each partial map failure, wrong PCH identity/
class/location, off-domain skips, GPU decode refusal, all-ones audio failure,
PCH/well change and valid all-masked IRQs. Actual formatter fields are compared
against complete distinct retained endpoint data. Sixteen platform-module parser
tests and four receipt-tail tests pass, preserving old versions and rejecting
malformed, missing, duplicate, wrong-version and invalid new endpoints.

The explicit synthetic retention model preserves the previous native compact
proofs, adds the new endpoint records and replaces final checkpoint report
shapes with source-derived maximum-width/capacity reports (9,056 bytes replacing
3,443 native checkpoint bytes). All critical records survive byte-for-byte in
64 KiB with 128 KiB pre-final chatter and 26,936 bytes of additional receipt
width/capacity padding. No raw native file is changed. This is regression
coverage only, not a source-matched native trace or acceptance of new registers.
Original RC6 acceptance remains unchanged and failed on the input trace.

All nine main VM cases and three legacy owner/C3/package profiles pass, with
failure/final-admission review across twelve. Ordinary/AP symbol/disassembly,
no direct control-write operations and sparse-hook isolation pass. Exact-stage
GPT native refusal saves/unmounts/returns, while missing Home halts without reset.
Host/VM success does not establish native dependency observations.

The automated trial keeps the existing 30/30/30-second phases, 32 rounds, 20 ms
bounds, HLT fallback, 26-control resource inventory and format-2 telemetry. The
collector additionally requires both actual valid dependency records; independent
JSON observations are saved before the separate strict RC6 assertion. Keep AC
and this unit's USB connected; no input is required. The baseline is 574 files /
1,635 clusters, with prior hashes, allocation/fsck and headroom verified. Previous
payload/evidence and protected console/accepted recoveries remain durable; exact
ready/armed/returned state belongs to the local run record. Native acceptance
and every ownership/control-policy inference remain pending the actual return.


### Audio/IRQ trial rebuilt after PR272 integration (2026-11-02)

PR275 merged as `4e2ff068` while main also gained PR272's Files EOF and audio-
stress signal changes. The final source guard correctly refused to arm the
previous `eceb07fc` / `1848dd7c663eb738` image against that changed checkout.
No native boot occurred. Its twelve-case evidence and payload remain durably
preserved as a superseded unarmed candidate; its collector was disabled.

Rebuilt the integrated `4e2ff068` tree: matched ordinary/AP build, USB image,
full core/docs, hardware producer/parser/retention and isolation/disassembly
checks pass. The exact eight-control implementation and strict observation
semantics are unchanged. The explicit maximum-checkpoint retention model passes
again with current fingerprints and unchanged limits. Fresh stage
`ee85beb85b75fdca` passes all nine main VM cases and three legacy owner/C3/package
profiles plus twelve-case failure/final-receipt review. Exact GPT refusal/save/
unmount/return and missing-workspace halt remain verified on this image.

The actual USB remains the expected 574 files / 1,635 clusters, with all prior
hashes preserved and fsck/allocation/headroom clean. The local integration run
record binds the matched stage, payload/recovery verification and ready/armed/
returned state. The earlier image is not a native result. Next is the same
unattended read-only endpoint trial on this rebuilt image, requiring real
version-6 dependency records and all original compact proofs; no modeled rows
or older VM receipt substitutes for its matching evidence. Native display-audio/
IRQ ownership and overall platform acceptance remain pending. RC6's original
bound, package-C3 uncertainty and all policies/limits stay unchanged.


### Native display-audio/IRQ capture collected (2026-11-02)

Integrated source `4e2ff068`, release `ee85beb85b75fdca`, returned automatically
with matching session `session-00065f53a4c841b9-00`. Its actual 65,536-byte exit
SHA-256 is `d8f7320405ba731e9db34522949bbfaacfaac625c5a7e661a64bc562454ea3c7`.
One version-2 header, all 15 power/15 idle/15 entry records, both version-6
dependency endpoints and every original proof parse directly. No synthetic
retention rows or joined log supplies missing acceptance data.

Both dependency endpoints pass live exact PCH and unchanged well checks, reading
eight controls in 15 and 13 us. Raw values are identical: pin/ELD `0`, audio
configs A/B/C `0x0070fa60`, DEIMR/SDEIMR `0xffffffff`, DEIER/SDEIER `0`. Decoded
ELD-valid, CP-ready and output-enable bits are clear for A/B/C; mathematical
enable-and-unmasked intersections are zero. BIOS well request remains set and
all 26 resource controls remain unchanged. These non-atomic observations do not
prove HDA/stream/DMA retirement, IRQ ownership, platform permission, well disuse
or the cause of zero package C3.

All 32 coordinated core-C3 rounds pass (9,983 us maximum), with positive core
residency and terminal AP retirement. Original idle/load/recovery phases complete
with mean package power 4.024/5.113/4.033 W over covered intervals. Package C3
remains zero. RC6's nominal 90.131319040 seconds exceeds the PM bracket
90.050888–90.050891 seconds by about 80.425 ms after one tick (0.0893%); the
original strict predicate remains failed, so overall platform acceptance is
**failed**, independently of successful read-only capture and retention.

All 574 prior files survive except expected workspace metadata; four new session
logs yield 578 files / 1,640 clusters. Read-only fsck and allocation checks are
clean, with no leaks/mismatches/errors. The one-shot was consumed; Ubuntu default,
current/console recovery and protected durable payload hashes are verified.
All 461 collected evidence files were independently rehashed outside `build`.
The local run is returned/collected, its collector disabled and no reboot armed.
Next is a source audit of remaining display-well dependencies and safe ownership
requirements; this result grants no power transition or policy change.


### Remaining display-well ownership audit — active scope (2026-11-02)

Base `cfbf3f97`; docs/source audit only. Cross-check the actual two native
endpoint inventories against the Haswell domain map, firmware takeover/readout,
CRT/FDI and display-HDA lifecycles. Scope: this unit record and the current AP
ownership guide, with pinned upstream sources/hash receipts kept locally. No
MMIO additions, power writes, policy/limit changes or physical boot are planned
in this audit. PR252 lifetime and PR19 Wi-Fi scopes remain independent; fetched
main has no production changes beyond the tested `4e2ff068` image.


### Remaining display-well ownership audit completed (2026-11-02)

Audited Linux v7.0 commit `028ef9c96e96197026887c0f092424679298aae8` against
native source `4e2ff068` / release `ee85beb85b75fdca`, with pinned sources and
hash receipts retained locally. The [current dependency audit](../ap-ownership-diagnostic.md#remaining-haswell-ult-well-dependencies-2026-11-02)
records domain coverage, model exclusions and ownership prerequisites.
No production code, MMIO allowlist, policy, limit or installed payload changes.

`0x0a26` is ULT GT3: upstream excludes CRT and port D. Raw zero slots in the
existing inventory do not imply implemented ports. Existing eDP input encoding
0 also excludes the specific fitter-A forced-through condition (encoding 4).
Those observations narrow the audit; they do not grant a well transition.

The actionable gap is separate display HDA `8086:0a0c` at 00:03.0: actual PCI
endpoints are D0 with command `0x0006`. Analog `8086:9c20` is a separate owner.
Linux binds display-HDA power to graphics audio references, stops/resets the
controller before releasing them, and adopts the display under an INIT reference
before unused-well cleanup. SrvrOS has no equivalent ownership graph; clear
ELD/output bits and all-masked display IRQs do not establish DMA retirement.

Next implementation: exact-gated read-only display-HDA control context after
width/side-effect/domain review, explicit unavailable/changed evidence, bounded
capability-derived stream coverage and unchanged compact retention checks. It
must not initialize HDA, issue verbs, acknowledge status, change PCI/D-state or
clear the BIOS request. A future well transition needs its own serialized
ownership/adoption/restore design. RC6 timing failure and package-C3 zero remain
unresolved. Audit-only closeout requires docs/link and diff checks; no hardware
acceptance is added and no new boot is armed.


### Read-only display-HDA context — active scope (2026-11-03)

Base `e376669c` with pinned SrvrUI/SrvrApps/SrvrPorts. Add two bounded display-HDA
control snapshots to the isolated Air6,1 platform diagnostic, exact 00:03.0
`8086:0a0c` / subsystem `106b:011a`, validated BAR/D0/decode and display-well
checks. Fixed-width global controls and capability-bounded stream controls only;
no initialization, reset, verbs, status acknowledgement, power/PCI writes or
new polling. Scope: isolated producer, host decoder, access/formatter/parser and
retention regressions, machine/current guide. Preserve every existing phase,
latency limit and integrity check. PR278 app registry, PR252 lifetimes and PR19
Wi-Fi retain separate ownership. Next checkpoint: host/build/VM validation and
an exact-source automated read-only native candidate; no new boot armed yet.


### Display-HDA candidate validation and automated handoff (2026-11-03)

Compiled source `f921afc2`, release `2bcb21ae64ee1920`, adds two exact-gated
read-only display-HDA snapshots to the existing platform profile. The actual
`8086:0a0c` / `106b:011a` identity, 00:03.0/class, live D0/decode/16 KiB BAR,
GPU/PCH and stable well gates are mandatory. GCAP/GCTL, interrupt and ring DMA
controls, position-buffer context and 1–30 capability-bounded stream controls
use exact widths excluding status bytes. Reset/off remain unknown; active DMA
is observed without being mistaken for an access error. No initializer, verbs,
reset, power/PCI/IRQ write or new polling is added.

The first decimal report failed the conservative final-log budget; that failed
synthetic result remains local. Fixed 179-byte little-endian payloads encoded
canonically in 240 base64 characters preserve every raw field, partial/failure
row and unused slot. The actual C emitter is checked field-by-field after an
independent decode. The host parser rejects malformed/noncanonical encoding,
unknown states, counts, gates, missing/duplicate endpoints and invalid raw data.
Old report versions remain readable. Version 7 adds mandatory actual HDA proof;
no prior predicate or original RC6 bound is relaxed.

Matched ordinary/AP build and USB image, full core/desktop/widget suites, docs,
focused mapping/access/width/reset/change/formatter tests, twenty platform tests
and five tail tests pass. Symbol/disassembly isolation confirms no diagnostic
MP request in the ordinary ELF, no forbidden platform control writes and no new
sparse-sampler accesses. All nine main and three legacy VM profiles pass,
including directed failure-state and twelve-case final-receipt review. Exact
GPT refusal saves, unmounts and returns; missing Home halts as expected.

The explicit synthetic maximum-30-stream model preserves all prior native
proofs and new HDA rows byte-for-byte under the unchanged 64 KiB tail, source-
derived maximum checkpoint shapes, 128 KiB pre-final chatter and 26,898 bytes
of width/capacity padding. Its source fingerprints include the new producer
include. This is regression evidence only, not new native acceptance.

Local preflight verifies 578 files / 1,640 clusters, all prior hashes, clean
read-only fsck/allocation and headroom, exact model/USB and connected AC. The
previous diagnostic's EFI copy is retired only after independent durable
payload/evidence hashes pass and normal installation repoints its entry; console
and accepted recoveries remain intact. The new matched candidate retains console
recovery and Ubuntu default. Exact install/backup/boot state is in the local run
record. Native acceptance is pending, and no model/synthetic/other-session rows
may substitute for its returned HDA controls. Original phases, 32 rounds, latency
limits, fallback, ownership checks and policies remain unchanged. No input is
required; progress is shown and results save before automatic Ubuntu return.


### Native display-HDA capture collected (2026-11-03)

Compiled source `f921afc2`, release `2bcb21ae64ee1920`, returned automatically
with matching session `session-00065f6457730365-00`. The actual 65,536-byte exit
SHA-256 is `40dbbe337a1c97fe2dfe858ca6c41ee296e6cd699251ac35970748607f83cd36`.
Both version-7 HDA records decode directly alongside all 17 power, 17 idle and
17 entry samples and every original required proof. A fresh ten-source-matched
stress replay preserves every critical byte with 26,155 bytes of width/capacity
padding and 128 KiB pre-final chatter. No modeled or joined data supplies native
acceptance; the replay remains synthetic regression evidence only.

Both HDA endpoints pass all access checks with unchanged BAR/command/PM/wells.
They take 19 and 18 us, each with twelve MMIO reads. GCAP `0x2001` describes two
output streams; GCTL `1` is unchanged and means controller reset is deasserted.
INTCTL, CORBCTL, RIRBCTL and DPLBASE are zero. Both stream controls retain
`0x040000`: RUN and reset are clear, with zero stream tags. This accepts bounded
read-only capture/retention, not link or codec readiness, DMA retirement, IRQ
ownership or power-off permission. Existing resource/audio/IRQ controls and
firmware well request remain unchanged.

The initial local collector imported the decoder but omitted standalone HDA JSON
extraction because its script-edit pattern did not match. The existing mandatory
coverage check already validated both HDA records, so no missing data was
accepted. Closeout saved the HDA result directly from the unchanged matching
exit and preserved the original assessment. The local collector now explicitly
saves/asserts that result; executing its actual extraction/assertion section
passes the matching trace and refuses missing HDA records. The original overall
failure classification is unchanged.

All 32 coordinated rounds pass (9,982 us maximum), with core-C3 residency and
clean terminal AP retirement; package C3 remains zero. Original ordered phases
complete with mean covered package power 4.038/5.047/4.027 W. Nominal RC6 is
90.119211520 seconds versus a PM bracket of 90.053459–90.053461 seconds, about
65.749 ms excess after one tick (0.0730%). The strict RC6 predicate remains
failed; the **overall platform trial is failed**, independently of complete HDA
control observations and final-log retention.

All 578 baseline files survive except expected workspace metadata; four session
logs yield 582 files / 1,645 clusters. Read-only fsck and independent allocation
are clean, with no leaks/mismatches/errors. The one-shot is consumed and collector
disabled. Installed candidate, console recovery, protected payload hashes and
Ubuntu default are verified. All 474 originally collected durable evidence files
were independently rehashed before correction/closeout; updated receipts are
backed up separately with originals retained. No reboot is armed.

Next is a bounded ownership/adoption/shutdown/restore design using the observed
active firmware eDP route and display-HDA controller out of reset. No well clear,
controller reset or power-policy change follows from this read-only run. RC6's
clock mismatch and the package-C3 blocker remain unresolved.


### Display/HDA ownership and restoration design — active scope (2026-11-03)

Base `5b2bf3c7` (PR289's SrvrUI host extraction consumed with exact pins).
Design only: define firmware display adoption, exclusive HDA/domain ownership,
quiesce/release/restore state transitions, refusal/failure behavior and staged
verification using native `f921afc2` / `2bcb21ae64ee1920` evidence. Scope: current
ownership guide and this unit record; no new MMIO, reset, well request, policy,
limit, executable or installed-payload change. PR291 ports, PR288/278 apps,
PR252 lifetimes and PR19 Wi-Fi remain independent. Next checkpoint: reviewable
state machine and explicit prerequisites for a future diagnostic implementation.
No physical test or reboot is armed by this design.


### Display/HDA ownership and recovery design completed (2026-11-03)

Consumed integration `5b2bf3c7` and exact submodule pins, preserving native source
`f921afc2` / release `2bcb21ae64ee1920` as separate evidence. The
[design](../macbookair6-1-display-ownership.md) maps observations to explicit
owner/epoch/reference, adoption, guard, HDA quiescence, eligibility, well-off and
restore states. Unknown ownership, foreign activity, pending work, stale epochs,
changed gates or missing recovery refuse; uncertainty after a write retains
ownership and memory. No existing limits or integrity checks are relaxed.

The important recovery gap is codec state: HDA reset/discovery does not guarantee
preservation of firmware codec settings. A future write profile needs a defined
owned quiet target and verified recovery, not bulk replay of controller or DMA
addresses. It also needs the live eDP/framebuffer backing lifetime pinned and an
evidence/reset path independent of a lost display. Both remain prerequisites.

Next is milestone A: implement a no-write adoption/eligibility owner with exact
profile checks, immutable epoch, counted users/in-flight operations and retained
refusal/fault receipts, then validate its positive/negative host and VM paths.
It must not mark a dry-run software token as hardware authority. Later guard/HDA
and well-off milestones have separate write-allowlist and recovery gates. This
work changes documentation only, leaves the installed payload and boot state
unchanged, and adds no physical acceptance. Docs/source review checks pass; no
new boot is armed.


### No-write adoption owner — active scope (2026-11-03)

Base `c609207b`, with exact UI/apps/ports pins. Implement milestone A's pure
software epoch/reference/in-flight owner, immutable observed profile and
eligibility/refusal evaluator. Connect it only to the isolated read-only
platform diagnostic, retaining all raw evidence and unknown adoption/ownership
prerequisites. No writer interface, new MMIO, policy/limit changes or general
kernel ownership claims. Scope: diagnostic helper/adapter, strict compact report,
producer/parser/lifetime/refusal/retention tests and unit/current design docs.
PR288/278 apps, PR252 lifetimes and PR19 Wi-Fi remain independent. Next checkpoint:
matched build/host/VM validation and source-bound no-write native evidence.


### No-write owner candidate validated and native handoff (2026-11-03)

Compiled source `fcd52df8`, release `814f7e939bc4adf9`, implements milestone A
only in the isolated platform diagnostic. The allocation-free software model has
no I/O/writer callback. One boot pool supplies epochs; tokens bind object,
epoch, slot and monotonic serial. Eight route/audio reference slots retain busy
operations. Closing freezes new work and drains before retirement; a fault keeps
the guard and prohibits reuse. Wrong identity/state/owner, unknown/active/changed
facts, stale/duplicate tokens, malformed counters and exhaustion refuse.

The adapter consumes existing retained observations without new hardware reads.
All seven backing/users/HDA/IRQ/restore/recovery/firmware assurances stay unknown.
Its expected result is refusal mask 127, Observed → Retired, an empty software
ledger, guard 1 → 0 and authority zero. These software counts are not evidence
that firmware or hardware users are absent. No hardware owner state or writer
exists, even if a synthetic host fixture supplies all assurance bits.

The fixed 28-byte version-8 record includes the profile digest, refusal and
lifetime fields. The decoder recomputes raw-data identity and active/change
context from required resource/audio/IRQ/HDA rows. It refuses malformed,
missing/duplicate and authority-claiming data; all earlier proofs and original
RC6 semantics remain mandatory. The conservative current-source maximum-checkpoint
model preserves every critical byte with unchanged 64 KiB retention, 128 KiB
pre-final chatter and 26,155 bytes of width/capacity padding. This model remains
synthetic, not new native acceptance.

Matched ordinary/AP build, USB image, full core/desktop/widget/host/docs suites,
focused state/lifetime/adapter/emitter tests, final-source ASan/UBSan, twenty-two
platform tests and six tail tests pass. An instrumented host formatter emits the
actual C adapter record, independently checked against Python's wire schema and
expected observed-word layout/digest. Symbol/disassembly and read-count checks
verify diagnostic isolation, no forbidden control writes and no added hardware
reads. All nine main and three legacy VM cases, directed failures and final-
receipt review pass. Exact GPT refusal saves/unmounts/returns; missing Home halts.
The integrated AWK build required installing Bison on this Ubuntu host.

The local baseline is 582 files / 1,645 clusters, with all prior hashes, read-only
fsck, independent allocation and headroom verified. Prior HDA diagnostic payload
and evidence are verified durably before retiring only its EFI copy through the
normal repoint/install sequence. Console and accepted recoveries are retained;
Ubuntu remains default. The new automated 30/30/30-second trial preserves every
existing phase, 32-round/20 ms bound, fallback and integrity check. No human input
is needed; progress/results and automatic collection use the existing profile.
The local collector explicitly saves/asserts the dry-run result; its actual code
section passes a synthetic valid fixture and refuses missing records. Native
acceptance remains pending; no modeled, older or joined proof can supply it.


### No-write owner rebuilt after concurrent integration (2026-11-03)

PR296 merged as `fa8e503f` while main gained shared app/desktop changes, including
PR295. The app-pin/source checks prevented arming the earlier `fcd52df8` /
`814f7e939bc4adf9` image. No native boot occurred. Its collector is disabled;
its payload and host/VM evidence remain independently backed up as superseded,
unarmed evidence. That state is not a physical failure or acceptance.

Synchronized the exact app pin and rebuilt the frozen `fa8e503f` integration.
The owner, adapter and focused fixture sources are byte-identical to the tested
implementation. Full matched build/USB/AP ELF, core/desktop/widget/host/docs,
isolation and maximum-retention checks pass. New release `9316558efb4b7007`
passes all nine main and three legacy VM cases with directed-failure and final-
receipt review. The native stage is restored and matched to its exact GPT
refusal/save/unmount/return rehearsal. Fourteen supplemental actual-adapter
activity cases and ULT port-D exclusion pass on synthetic retained words only.

The actual USB is still 582 files / 1,645 clusters, with unchanged hashes and
clean fsck/allocation/headroom. The unarmed EFI candidate is retired only after
verifying its independent durable payload/evidence and repointing through the
normal installer. The rebuilt candidate retains console recovery and Ubuntu
default. Its local record binds exact source, payload, rehearsal, collector and
ready/armed/returned state. The integration source stays fixed through this
native run; later mainline changes require their own validation rather than
being silently inherited by this candidate. No control writes or policies change.
Next is collection of actual version-8 expected-refusal/lifetime evidence and all
prior proofs. Native acceptance remains pending; no old/model/joined receipt
can replace this matching run.


### Native no-write owner result (2026-11-03)

Integrated source `fa8e503f`, release `9316558efb4b7007` (PR296), completed
session `session-00065f65ae97e590-00` and returned automatically to Ubuntu.
The unchanged 65,536-byte exit file has SHA-256
`8e7c8db5e2167b35259f2374f0ead628341ec9673fde3125dd8868fcd7091e81`.
Version 8 directly retains the owner record, both HDA/resource/dependency
endpoints and all original compact power/idle/entry and AP/package proofs.

The raw-data digest matches both stable endpoints. The expected seven-unknown
refusal is 127, epoch 1, Observed → Retired, software guard 1 → 0, with zero
route/audio references, in-flight work, assurance/activity flags and hardware
authority. This physically verifies the diagnostic-only software refusal and
lifetime. It does not prove actual backing, domain, firmware or HDA ownership.

All 15 retained power/idle/entry samples and ordered 30/30/30-second phases
complete; average package power is 4.102/5.168/4.112 W. All 32 coordinated C3
rounds pass, maximum 9,983 us, with positive core residency, mandatory HLT
fallback and clean AP retirement. Package C3 remains valid zero. The original
RC6 check still fails: nominal 90.130926080 s versus the PM bracket, exceeding
its original bound by 78.569800 ms (about 0.0872%). The whole platform trial
therefore remains failed; no threshold, integrity check or power policy changed.

All 582 prior files are preserved except expected workspace metadata; four new
session logs produce 586 files / 1,650 allocated clusters. Read-only fsck and
independent allocation checks pass with zero leaks, bitmap mismatches or errors.
Installed payload hashes, console/accepted recoveries and durable backups verify;
Ubuntu is default, the one-shot is consumed and the collector is disabled.
The tested runtime remains frozen to `fa8e503f`; later mainline app/snapping
changes have no inherited physical acceptance. No reboot is armed.


### Primary-A and inherited backing readout staged (2026-11-03)

Source `728bcbeb`, release `9b485ca12b4c2482` (PR302), implements the first
read-only milestone-B comparison on integration base `d5822348`. Nine fixed
reads use existing private read-only/NX mappings: primary-A control, stride,
legacy offset, surface, Haswell offset, pipe-A size, live surface and two
control/surface rechecks. BAR2/PCI command and well endpoints are preserved.
Only a single bounded RGB32 inherited descriptor drives kernel page-table
observations, capped at 4,096 pages and 4,096 firmware entries. Every page must
translate contiguously; ambiguous/absent firmware coverage remains unknown.
A last-canonical-page regression and exact-cap case verify a page-count loop
cannot wrap into an unbounded walk. No framebuffer pixels or GGTT PTEs are read.

Two fixed 203-byte records retain every readout field. Version 9 additionally
packs every existing resource/dependency field losslessly into 154/90-byte
records, preserving the original strict validators and version-1–8 decoding.
Source-matched maximum-checkpoint modeling preserves all critical bytes under
64 KiB retention, 128 KiB pre-final chatter and 26,282 bytes of width/capacity
padding. This remains explicitly synthetic, not native acceptance. The earlier
unpacked model failed retention and was corrected; no proof, sample capacity,
latency bound or integrity check was removed.

Full matched build/USB/AP, core/desktop/widget/host/docs suites pass on `6a9f85c9`.
The final diagnostic-only wrap fix on `728bcbeb` passes rebuilt matched artifacts,
actual C fixtures (26 backing cases), ASan/UBSan, 36 platform/parser/tail tests,
isolation/disassembly and actual-emitter C-to-Python wire checks. Actual collector
save/decoder/assertion code passes a complete synthetic fixture and refuses
missing backing or malformed packed-resource data. All nine fresh main VM cases
and three legacy profiles pass on the final source, with directed-failure and
final-receipt review. Exact GPT refusal saves/unmounts/returns; missing Home halts.
The native stage is restored and independently hash-verified outside build.

The baseline is 586 files / 1,650 allocated clusters from the collected prior
native run. Console and accepted recovery payloads remain protected. The local
run requires exact unit/USB identity, all prior hashes, clean offline fsck and
allocation/headroom, verified payload/recovery backups and Ubuntu default before
one-shot arming. No human input is required. All seven assurance bits, GGTT/stolen
backing, true pin/lifetime, domain ownership and firmware exclusion remain unknown.
Compatible aperture/geometry observations cannot turn those unknowns into grants.
Native comparison and the original RC6/package-C3 results must be collected
separately before the next implementation; no broader desktop acceptance is
inferred for this tree.


### Native primary-A and inherited backing readout (2026-11-03)

Source `728bcbeb`, release `9b485ca12b4c2482` (PR302), completed matching session
`session-00065f667400f67f-00` and returned automatically to Ubuntu. The unchanged
65,536-byte final exit file has SHA-256
`791ef184dccd9e1a56251d6c14bb527d17f07832eea401f3f3b3e22e02056eb3`.
Both new readouts, losslessly packed resource/dependency rows, HDA/dry-run
records and every original CPU/AP/package proof parse from that file directly.
No model, joined session or missing-field reconstruction supplies acceptance.

Both backing endpoints have state 1, exactly nine reads and stable raw controls;
PM brackets are 35/34 us. Primary-A control is `0x98004400` (tiled bit set),
stride 5,632, legacy/Haswell offsets zero, programmed/live surface zero, and pipe
size 1366×768. BAR2 is `0xa000000c`, high word zero, with stable PCI command 7.
All well words and earlier display/HDA control observations remain stable.

The single RGB32 inherited descriptor has pitch 5,632 and span 4,325,376 bytes.
All 1,056 pages translate contiguously from CPU aperture address `0xa0000000`;
the sole enclosing firmware entry is FRAMEBUFFER with that exact span, among
44 entries. This accepts bounded readout and endpoint mapping continuity, not
underlying GGTT/stolen RAM identity, fence interpretation or an actual pin.
Tiled mode correctly produces `unbound-or-unsupported` rather than a false
linear-backing grant. The software owner still refuses all seven unknowns
(mask 127), retiring its empty software guard without hardware authority.

All 15 retained power/idle/entry samples and ordered idle/load/recovery phases
complete, averaging 4.007/5.173/4.013 W package power. All 32 coordinated core-C3
rounds pass, maximum 9,984 us, with positive core residency, mandatory HLT
fallback and terminal AP retirement. Package C3 remains valid zero. The whole
platform trial remains failed: nominal RC6 is 90.147270400 s and exceeds its
original snapshot-clock bound by 96.605120 ms after one tick (about 0.1073%).
No power policy, ownership/integrity check, sample capacity or limit changed.

All 586 prior files remain unchanged except expected workspace metadata; four
new session logs yield 590 files / 1,655 allocated clusters. Read-only fsck and
independent allocation checks pass with zero leaks, bitmap mismatches or errors.
Installed/recovery payload hashes and durable evidence verify; Ubuntu remains
default, the one-shot is consumed and the collector is disabled. No reboot is
armed. The next scope is read-only tracing of the tiled aperture/GGTT backing;
no HDA/well transition or real adoption is authorized by this result.


### Tiled/GGTT source audit and shared-capture dependency (2026-11-03)

Main is updated to `8f47f196` with its actual app/widget pins; the installed
native readout remains `728bcbeb` / `9b485ca12b4c2482`. No newer tree inherits
that acceptance. The [Haswell audit](../macbookair6-1-tiled-backing-audit.md)
finds model-critical differences from PR301's Broadwell capture: +2 MiB GGTT
alias in a 4 MiB BAR, 32-bit PTEs, different GGC size fields and cache bit 11
rather than physical-address bit 39. Haswell swizzle also remains unobserved.

The Intel register volume documents sixteen fences/ten pitch bits while Linux
selects thirty-two Gen7 fences and a wider pitch. Both references are retained;
no new native allowlist is inferred from a sibling. Partial coverage cannot
prove uniqueness. Linux fence-debug metadata was unavailable, resolving nothing.

The host-only executable contract reproduces the actual 1,056-entry / 4,224-byte
GGTT extent and tile padding, and tests generation/cache/address/fence refusal
cases. All eight test groups pass; no raw PTE/fence/GGC has been captured natively.
PR301 owns shared capture; a separately exact Haswell adapter and versioned
full-vector receipt remain next after integration or explicit shared-interface
coordination. This audit does not introduce a competing driver, runtime gate,
control writer or staged payload. No policy, limit or integrity check changes.
Docs/host fixtures are validated independently; a new matched build, exact-stage
VM, native preflight and complete collection are still required before another
physical test. Existing RC6 failure/package-C3 zero and unknown ownership remain.
No reboot is armed.


### Exact Haswell vector pilot implemented (2026-11-03)

Base `0cba30a1`, with actual recursive pins. PR301 remains open and independently
owns the Broadwell capture/scanout lifetime. The first Haswell adapter is scoped
to the existing Air6,1 isolated diagnostic, preserving its exact device gates and
using the bounded read-only I/O contract without modifying the other driver.
See the [pilot contract](../macbookair6-1-tiled-backing-audit.md#isolated-haswell-pilot-implementation).

The dedicated profile skips the ordinary Intel stub, requires existing decode/D0,
excludes RAM from the full 4 MiB MMIO range and uses a non-global RO/NX window.
It retains complete DWORD GGTT vectors twice, raw config/route/framebuffer and
FENCE0–15, with 1,536-entry and 100 ms readout bounds, full comparisons and verified
window retirement. The documented lower range stays distinct from unobserved
upper fences; only display-arbitration swizzle is read. No writer, allocator pin,
full swizzle/uniqueness proof or hardware authority is added. Original RC6 failure,
package-C3 zero and all existing limits remain separate and unchanged.

Actual core/adapter fixtures, max-capacity/clock/map/retirement/change failures,
source-independent producer decoding and maximum-tail coverage are required
before installation. Matched build, full host suites, exact-stage identity
refusal/persistence, missing-workspace halt and old-profile regressions remain
pending. The current installed native image is still `9b485ca12b4c2482`; no new
physical acceptance or reboot is claimed by implementation alone.

### Haswell vector pilot validation and prepared native run (2026-11-03)

[PR324](https://github.com/PaulMDemers/SrvrOS/pull/324) integrates actual main
`59928ec9`; tested code is `baaa94b5`, with ports `ca749517` and the other
recursive pins preserved. Recent mainline installation/salvage/profile changes
are included. Strict GCC host compatibility fixes retain namespace/range checks,
POSIX no-follow/exclusive/close-on-exec flags, flush and ownership behavior; no
warning flag is disabled. The ports-only best-effort-result compatibility change
is merged in [SrvrPorts PR5](https://github.com/PaulMDemers/SrvrPorts/pull/5).

Matched normal/AP kernels, RAM/USB images, complete core/desktop/widget suites,
profile-chooser regressions, host prerequisites and docs checks pass locally.
Actual capture core/adapter tests and ASan/UBSan pass, including full-vector
producer decoding, mapping/clock/change failures, retained uncertainty and normal
kernel isolation. Source-matched maximum retention preserves all raw records
with 128 KiB earlier chatter and 32 KiB subsequent allowance in the unchanged
64 KiB exit tail; its source-derived checkpoint bound is 16,514 bytes. This is
synthetic retention coverage, not physical acceptance. Sanitizer stdout/stderr
are retained separately; the initial interleaved host transcript is kept, not
repaired into evidence.

Four legacy AP owner/C3/package/platform VM profiles pass. Exact staged release
`e98aed0c5f8d06cf` safely refuses Q35 identity, saves its refusal, unmounts and
resets. Missing workspace returns without starting the capture or falsely saving.
Both are negative-path checks, not native readout acceptance. The exact staged
payload is durably backed up; at this validation checkpoint it is uninstalled
and unarmed. The approved next step is its automated native install/one-shot,
requiring complete two-pass observations, unchanged endpoints, verified window
retirement, preserved USB files and clean offline allocation. Runtime arming and
collection state stay in the local run record. No human input is required.

The previous native payload remains `9b485ca12b4c2482` until that installation.
Its complete payload/evidence must verify before reclaiming only its EFI copy;
console recovery and accepted fallbacks remain protected. GGTT observation does
not grant real ownership/pinning, complete fence/swizzle knowledge, package C3 or
RC6 timing acceptance. Those limitations and all existing limits remain unchanged.

### Native Haswell full-vector observation accepted (2026-11-03)

Release `e98aed0c5f8d06cf`, compiled source `baaa94b5` from merged PR324,
automatically returned and collected session `session-00065f696b8bd4f1-00`.
Both complete 1,056-entry DWORD vectors agree byte-for-byte, with stable config,
framebuffer and documented lower-fence endpoints. The two readout brackets are
371 and 368 us under the original 100 ms bounds. Each performs 1,152 window reads;
eight temporary maps retire with no retained uncertainty or hardware authority.

Every PTE is present. Address candidates are consecutive from `0x8da00000` through
`0x8de1f000` (exclusive backing end `0x8de20000`), entirely inside the captured
firmware-reserved stolen interval `0x8da00000..0x8fa00000`. Host/GPU GGC agree at
`0x209`, giving a 2 MiB table candidate. FENCE0 is `0x00fff02b00000001` and is the
sole candidate among the sixteen documented rows; the unobserved upper range
still prevents a uniqueness grant. Display-arbitration swizzle bit 13 is clear;
full swizzle/configuration and CPU-detiled equivalence remain unproven. No backing
pixels were mapped or read, and no pin, ownership or power-control permission is
accepted by this observation.

All 590 prior files are preserved apart from expected workspace metadata. Two
new logs yield 592 files / 1,658 allocated clusters. Read-only fsck and independent
allocation checks pass with zero leaks, bitmap mismatches or errors. The exact
24,705-byte exit receipt independently decodes again; 495 durable evidence files
verify. Installed payload/recovery hashes verify, Ubuntu remains default, the
one-shot is consumed and the collector is disabled. No reboot is armed.

Closeout consumes main `3b3fb5b3` for documentation only; that newer tree does not
inherit this release's physical acceptance. Next is resolving remaining fence
coverage and swizzle/lifetime prerequisites through source audit and bounded
read-only evidence. Prior RC6 timing failure and valid-zero package C3 remain
unchanged; no graphics writer or ordinary adoption is enabled.

### Fence/swizzle prerequisite audit in progress (2026-11-03)

Consume main `5634cd9d` and its actual recursive pins; native acceptance stays
with `baaa94b5` / `e98aed0c5f8d06cf`. PR301 at `5287fa13` remains open and owns
Broadwell capture/scanout lifetime; this task does not modify its driver or gates.
The bounded objective is to resolve Haswell fence-count/pitch and inherited
swizzle interpretation from pinned primary sources and the accepted raw receipt,
then specify the smallest justified next read-only experiment. Intended changes
are this unit record, the Haswell audit guide and host-only arithmetic/fixtures.
No register allowlist, power policy, ownership or capture limit is widened by
this audit. A new native run requires separately justified controls and the full
matched build/stage/VM/recovery sequence; no reboot is currently armed.

### Fence/swizzle host audit complete (2026-11-03)

The [prerequisite audit](../macbookair6-1-tiled-backing-audit.md#inherited-fenceswizzle-prerequisite-audit-2026-11-03)
re-decodes the accepted native receipt. Full display field 14:13 is `00`;
CPU/aperture and GT controls remain unobserved. Linux preserves inherited tiled
swizzle policy from the display field, but this is not cross-path consistency
or ownership proof. Exact `0a26` device-info tracing confirms Linux's Gen7
32-fence model; the Intel 16-row discrepancy remains explicit and no upper-row
allowlist is introduced. Native fence pitch agrees under both references, but
its 16 MiB coverage is not proof of a 16 MiB allocated/live object.

The GT control at `0x4030` requires render forcewake in the pinned Haswell access
model and stays deferred. The smaller proposed experiment captures raw CPU
`TILECTL=0x101000` twice, outside that forcewake table, retaining all current
checks and limits. Its native two-PTE-page layout fits the existing ten-map
ceiling; a three-page layout must refuse rather than relax the ceiling. Raw
TILECTL interpretation remains separate from the generic PRM field and Linux's
prefetch-bit label. No backing pixels, power controls or scanout ownership are
added. Host tests exercise reserved display values, unchanged authority refusal,
fence-count/extent ambiguity and next-capture map capacity. This audit does not
prepare or arm another physical boot; implementing/validating that narrowly
scoped readout remains the next step. Native RC6/package-C3 limitations persist.

### Bounded CPU TILECTL implementation scope (2026-11-03)

Base `819956be` with actual recursive pins. Extend only the isolated exact-Air6,1
pilot with an explicit TILECTL modifier and version-2 receipt. Touch the existing
Haswell core/adapter/header, main dispatch, staging/VM decoder, retention proof
and regressions. Preserve version 1 and all normal/other diagnostic profiles.
One CPU/aperture word at `0x101000` per pass remains an observation, not a
full-path consistency/ownership grant. Keep ten maps, 100 ms pass/gap, 1,536 PTEs
and 64 KiB tail; three-PTE-page layouts must refuse the extension. No GT access,
forcewake, extra fences, backing-pixel mappings or policy writes. PR301's
Broadwell driver remains independent. After matched host/build/exact-stage VM
checks, use local recovery/USB preflight and a saved run record for an authorized
automated one-shot/collection. No new native acceptance is claimed beforehand.

The opt-in version-2 path is implemented: one exact CPU DWORD after each final
fence check, matching observations, existing endpoint recheck/verified retirement,
and early three-PTE-page refusal under the unchanged ten-map ceiling. Version 1
remains available. Focused actual core/adapter/decoder and max-tail regressions
pass, including changed/all-ones/read-failure CPU words and mapping retirement.
Matched full build/suites, sanitizer and exact-stage VM/native checks remain
pending; no physical acceptance or reboot is claimed by this implementation.

### Raw CPU TILECTL trial validated and prepared (2026-11-03)

[PR343](https://github.com/PaulMDemers/SrvrOS/pull/343), code `1b0d37dd`, passes
matched ordinary/AP and RAM/USB builds, complete core/desktop/widget/profile
chooser/host/docs checks, plus actual core/adapter ASan/UBSan for both versions
and the maximum-capacity version-2 producer. The default installation suite
passes 56 tests with its existing opt-in exhaustive I/O matrix skipped. No
warning/check, read deadline, mapping or data capacity is relaxed.

Actual version-2 producer receipts at 1,056 and 1,536 entries independently decode;
source-matched maximum tail retention preserves all raw records under the
unchanged allowance. Ordinary kernel isolation, changed/all-ones/missing CPU
words, invalid offsets/CR3, three-PTE-page refusal and uncertain retirement are
covered. Four old AP VM profiles and the original Haswell version-1 profile
pass. Exact version-2 release `8dc0947f9c7fbd56` safely refuses Q35 identity,
saves/unmounts/resets, and returns without starting/saving when workspace is
missing. Negative-path VM success does not accept native CPU control readout.

The exact payload is durably backed up. At this validation checkpoint the new
release is uninstalled and unarmed; the next authorized step installs it with
console recovery/accepted fallbacks preserved, then runs the automated capture
and Ubuntu collection. USB baseline is 592 files / 1,658 clusters, all prior
hashes preserved and offline integrity/headroom clean. Native source remains
`baaa94b5` / `e98aed0c5f8d06cf` until installation. No input is required. The local
run record owns subsequent arming/collection state. Raw CPU observations do not
accept full swizzle consistency, upper fences, a pin or graphics ownership;
original RC6/package-C3 limitations remain unchanged.

### Native raw CPU TILECTL observation accepted (2026-11-03)

Release `8dc0947f9c7fbd56`, compiled source `1b0d37dd` from merged PR343,
automatically returned and collected session `session-00065f6b460f31de-00`.
The complete version-2 receipt independently decodes again: both raw CPU/aperture
TILECTL values are `0x00000000`; display control remains `0x16661056` with bits
14:13=`00`. These are matching sampled observations, not a continuous guarantee
or proof of the unobserved GT path.

Both complete 1,056-entry GGTT vectors and endpoint/lower-fence observations
agree. All entries are present and point into the firmware-reserved stolen
interval. Each pass performs 1,153 window reads; ten temporary mappings retire
with no retained uncertainty or hardware authority. Brackets are 383 and 376 us,
under the unchanged 100 ms bounds. No GT/forcewake/upper-fence/backing-pixel access,
policy write, pin or actual scanout ownership is accepted by this readout.

All 592 prior files remain unchanged apart from expected workspace metadata.
Two new logs yield 594 files / 1,661 allocated clusters; read-only fsck and
independent allocation checks pass with zero leaks, bitmap mismatches or errors.
The exact 24,816-byte exit receipt and 475 durable evidence files verify.
Installed/recovery payload hashes, Ubuntu default and the consumed one-shot
verify; collector disabled and no reboot armed.

Closeout consumes main `5ed7f015` and its recursive pins for documentation only;
newer desktop/port changes do not inherit this physical acceptance. CPU and
display observations narrow the swizzle unknowns, while GT access prerequisites,
the Intel16/Linux32 fence discrepancy, full-path consistency and real allocation/
scanout lifetime remain open. Prior RC6 timing failure and package-C3 zero are
unchanged. The next work is resolving those remaining prerequisites before any
ownership or power-control change.

### Exact-Haswell Linux-model fence coverage scope (2026-11-04)

Base `845f703c`, native version-2 result collected before this work. PR301 remains
an independent Broadwell scope; no shared driver or hardware gate is copied.
Pinned Linux PCI IDs/device-info map actual `0a26` to Haswell ULT GT3/Gen7; its
native-aperture fence initializer selects 32 and its Gen6+ register macro uses
`0x100000+8*i`. The proposed remaining rows end at `0x1000ff`, in the already
admitted fence page and outside its render-forcewake range. This independently
justifies an explicit exact-model read experiment, not resolution of the Intel
16-row documentation discrepancy or a hardware-uniqueness/ownership grant.

Implement a separately opted-in version-3 32-fence receipt alongside full GGTT
vectors and raw CPU TILECTL. Preserve versions 1/2 with sixteen rows, ten maps,
100 ms pass/gap, 1,536 PTEs, 64 KiB tail, identity/D0/firmware/RAM gates and
uncertain-retirement behavior. Touch only the isolated Haswell core/adapter,
main/staging/decoder, retention/tests and this audit guide. No GT/forcewake,
backing-pixel mapping, power policy, pin or assurance-bit change. After full
matched validation and local recovery/USB checks, prepare/run the authorized
automated test. No new native acceptance is claimed in advance.

Version3 is implemented as a required-modifier extension:32 rows on the same
fence page, raw labels preserve the Linux-model assumption, and global uniqueness
and every owner assurance stay false. Focused actual core/adapter and independent
v1/v2/v3/max producers/decoders/tail tests pass, including changed/all-ones upper
rows, candidate overlap and uncertain mapping retirement. Matched full suites,
sanitisers and exact-stage negative/legacy VMs remain pending before installing
or arming a native test. No source change is physically accepted yet.

### Linux-model32 trial validated and prepared (2026-11-04)

[PR351](https://github.com/PaulMDemers/SrvrOS/pull/351), code `169f3808`, passes
matched normal/AP and RAM/USB builds, full core/desktop/widget/profile-chooser/
host/docs checks, actual core/adapter ASan/UBSan and independent v1/v2/v3/max raw
producer decoding. The default installation suite reports71 tests, with its
existing opt-in exhaustive I/O matrix skipped. No warning or integrity check,
map/read/time/data limit or power policy is relaxed.

Source-matched maximum retention preserves the full version3 records in the
unchanged64KiB tail. Exact-model source predicates and pinned reference hashes
verify the0a26/Haswell/Gen7/native-aperture32-row and register-offset chain; its
Intel16 discrepancy stays unresolved. All four older AP VM profiles and both
older Haswell modes pass. Exact release `86257762f155128f` passes Q35 identity
refusal/save/unmount/reset and missing-workspace no-start/no-save return. These
negative-path checks do not accept native32-row values or table size.

The exact payload is durably backed up. At this validation checkpoint it is
uninstalled and unarmed. The authorized next step preserves console recovery and
accepted fallback/evidence, installs this image, then runs/collects automatically.
USB baseline594 files/1661 clusters, all hashes, fsck/allocation and headroom pass.
No human input is required. Local run records own subsequent arming/collection.
Native acceptance remains `1b0d37dd` / `8dc0947f9c7fbd56` until the new run; no GT,
forcewake, global uniqueness, pin, lifetime, firmware-exclusion or ownership grant
is inferred. RC6 timing and package-C3 limitations remain unchanged.


### Native Linux-model32 fence observation accepted (2026-11-04)

Release `86257762f155128f`, compiled source `169f3808` from merged
[PR351](https://github.com/PaulMDemers/SrvrOS/pull/351), automatically returned
and collected session `session-00065f6cbb8aa860-00`. Independent decoding of the
complete version-3 raw receipt matches the saved assessment. Both 32-row fence
observations agree: fence0 is `0x00fff02b00000001`, and rows1–31 are zero.
Fence0 is the sole candidate **within the exact Haswell Linux model**. Upper-row
zero observations do not prove register existence, absolute table size or resolve
the Intel16/Linux32 reference discrepancy; global hardware uniqueness stays false.

Both complete 1,056-entry GGTT vectors agree; all entries are present and point
into firmware-reserved stolen RAM. CPU TILECTL is zero twice, with the sampled
display swizzle field `00`. Each pass performs 1,249 window reads; brackets are
396/391 us under unchanged 100 ms bounds. All ten temporary mappings retire,
with no retained uncertainty, GT/forcewake access, backing-pixel mapping, policy
write, pin or ownership assurance. Full-path swizzle and lifetime remain unknown.

All 594 prior files retain their hashes apart from expected workspace metadata.
Two new logs give 596 files / 1,664 allocated clusters. Read-only fsck and the
independent allocation check pass with zero leaks, bitmap mismatches or errors.
The 25,280-byte exit receipt and 496 durable evidence files independently verify.
Installed/recovery payload hashes, Ubuntu default and consumed one-shot verify;
collector disabled, no reboot armed. Evidence has a durable copy outside build.

Closeout consumes integration `eab863ae` and recursive pins for documentation
only. Newer Files/profile changes do not inherit native acceptance of this tested
source. Remaining work is a source audit of GT access prerequisites and full-path
swizzle, reference-count uncertainty and actual backing allocation/scanout lifetime
before any ownership or power-control change. Prior RC6 timing failure and zero
package-C3 residency remain unchanged. No further native test is armed here.


### Remaining GT/lifetime prerequisite audit scope (2026-11-04)

Base `7766ed5a` with recursive integration pins; version3 native receipt collected
before this block. PR301 at `d320c5d4` owns independent Broadwell DMA/scanout work;
no driver, register gate or native acceptance is imported. Audit pinned Haswell
forcewake and initial-plane allocation/pinning paths against the accepted raw
receipt. Update only the host path-audit report, its regressions and Air6,1 docs
to distinguish completed observations from missing authority/lifetime proofs.
No new MMIO, firmware transaction, power writer, boot profile or limit change.
Save source hashes and native host-audit output locally, validate host/docs tests,
and integrate the bounded audit before proposing another native experiment.


Audit complete: pinned primary uncore and initial-plane files were independently
refetched/hash-matched. Haswell's GT read requires MT request/HSW acknowledgement
and thread-status acquisition, so no raw GT read is admitted. The native16MiB
fence exceeds the observed4,325,376-byte footprint by12,451,840 bytes; that excess
is not an owned allocation. The host report now tracks version1/2/3 completion,
explicitly refuses hardware/firmware exclusion and lifetime assurances, and no
longer proposes repeating completed CPU capture. Regression coverage includes
large/overlapping upper fences and unchanged no-authority results across versions.

`make test-haswell-backing check-docs`, independent native version3 host re-audit
and diff checks pass. Actual core/adapter producers and all three maximum-tail
checks remain passing. No kernel/staging/policy/limit change or new physical
acceptance; no payload installed or reboot armed. Next is a host-only exact-layout
reservation proposal and collision/alias/stale/retirement tests, coordinated with
PR301 before any shared allocator work. Native GT/DMA/scanout ownership remains
separate and the RC6/package-C3 limitations persist. Durable audit evidence is
saved outside build before closeout.


### Host-only inherited layout proposal scope (2026-11-04)

Base `f4ea0ccb`, recursive pins unchanged. PR301 inspected at `a803af45`; its
scanout-space API stays independent and unmerged. Add a separate host-only
exact-Air6,1 layout proposal derived from the strict version3 raw receipt and a
finite simulated reservation/lifetime model. Cover GGTT/physical conflicts,
partial overlaps, PTE aliases, foreign/stale tokens, retained references and
uncertain retirement without importing native managed ranges or setting owner
assurances. Touch only tools/tests, the Haswell test target and Air6,1 docs.
No kernel/shared allocator changes, MMIO, boot stage, power policy or new limits.
Validate actual receipt replay and host refusals, then merge and durably preserve
the host evidence. This simulation cannot accept a native pin or allocation.


Implemented the separate host proposal/model. Replay of the accepted version3
receipt yields the exact4,325,376-byte/1056-page surface, physical candidates
`[0x8da00000,0x8de20000)`, and full protected16MiB inherited fence. No native
managed range, physical ownership, pin or assurance bit is created. Exact GGTT
and physical collisions, aliases, stale/foreign tokens, retained/busy readers,
slot/generation exhaustion and uncertain quarantine are covered by nine new
regressions, including a valid matching aliased receipt that the proposal refuses.

`make test-haswell-backing check-docs`, all actual core/adapter producers and
three raw maximum-tail checks, independent native-receipt host replay and diff
checks pass. The shared PR301 allocator remains untouched: software protection
of a full inherited fence is not a managed native grant or DMA mapping. Durable
host evidence is kept outside build. No new native acceptance, installation or
reboot is armed. Next prerequisite is this unit's read-only DMA/resource inventory
and an exact-model ownership/publication contract before any hardware writer;
GT forcewake, firmware exclusion and actual scanout retirement remain unbound.


### Read-only DMA/resource inventory scope (2026-11-04)

Base `1fbec70d`, PR301 inspected at `b89e4e43`; native Broadwell DMA gates remain
independent. Collect this actual MacBookAir6,1's exported DMAR bytes, Ubuntu GPU
PCI/resource/IOMMU topology and kernel memory-resource catalog into ignored local
evidence. Decode offline with installed ACPICA and verify checksum/length/scopes;
compare only against the accepted Haswell footprint. No `/dev/mem`, PCI config
writes, IOMMU controls, driver unbind, firmware method, root-table walk or reboot.
Touch only Air6,1 documentation; no native parser/driver admission is added.
Ubuntu driver state is inventory evidence, not SrvrOS translation acceptance.


Inventory completed without MMIO/reboot: valid136-byte DMAR (39-bit host width,
flags01) declares GPU-specific segment0 `00:02.0` DRHD at `0xfed90000`, a separate
include-all unit at `0xfed91000`, and GPU RMRR `[0x8d800000,0x8fa00000)` covering
both accepted table/stolen intervals and the observed surface. ACPICA decoding,
independent byte/checksum/scope checks and native-footprint containment agree.
See the [sanitized DMA inventory](../macbookair6-1-dma-inventory.md).

Ubuntu has i915/active DMAR and a GPU-only IOMMU group; that initialized state is
not native SrvrOS translation evidence. Its exported firmware catalog has no
entry enclosing either DRHD page; `/proc/iomem` runtime labels do not substitute
for native Limine admission. This unit's flags/RMRR and reserved gap edges differ
from the Broadwell profile; no sibling gate is copied. No DMA/MMIO/ownership
assurance or new physical runtime acceptance is granted.

Next is an isolated firmware-only native capture of complete DMAR/current Limine
catalog and exact identity before any VT-d read protocol. It requires matched
validation and a new recorded run; none is staged here. Raw inventory and
verification have durable copies outside build; docs/diff checks pass. Current
installed payload/recovery and reboot state remain unchanged, with no reboot
armed. GT forcewake and real backing/publication/scanout lifetime remain open.


### Firmware-only native capture implementation scope (2026-11-04)

Base `5f670602`, read-only Ubuntu inventory complete and PR301 remains independent.
Implement a separate opt-in AP diagnostic profile that copies the complete pinned
Air6,1 DMAR and complete bounded current Limine catalog twice, with exact PCI
identity, ACPI-table-RAM/translation checks, stable copies and bounded receipt retention.
No VT-d/GT/GGTT MMIO, root-table walk, graphics bind, PTE publication or ownership
assurance. Touch isolated firmware core/adapter/tests, main dispatch, staging,
strict decoder/VM checks and exact Air6,1 docs. Ordinary startup stays unchanged.
Validate host/refusal/codec/tail and matched builds/VMs, preserve recovery and USB
integrity, save the exact run and prepare the authorized automated native test.
No native acceptance is claimed before return/collection.


### Firmware-only capture validated and prepared (2026-11-04)

[PR362](https://github.com/PaulMDemers/SrvrOS/pull/362), tested capture source
`f3cfcb90`, prepares exact release `2ee43cb527989448`. Final admission requires
ACPI reclaimable/NVS table RAM; generic reserved/device ranges cannot supply
bytes. Full unit-table SHA256 pin, identity, translations and complete two-copy
catalog/table agreement are mandatory. No native VT-d/GT/GGTT register read,
new mapping, firmware method, writer or assurance bit is added.

Matched normal/AP/RAM/USB builds, core/desktop/widgets/profile chooser/host/docs
checks pass across the broad invocation and affected reruns. The first invocation
found three pre-existing GCC fixture warnings in preferences-finish, process
service's longjmp selector and profile catalog. Separate test-only commits fix
formatting/volatile semantics without warning suppression; all affected groups
then pass. The71-case installation suite passes with its existing opt-in
exhaustive matrix skipped; it was not repeated for those test-only changes.
Only the new diagnostic's narrowed ACPI-RAM guard changes runtime code after
the broad build; its matched rebuild, seven decoder/config/hash/retention tests
and core/actual-adapter ASan/UBSan checks pass on the final source.

The exact local pinned-table adapter produces a full128-entry synthetic catalog
that independently decodes; every raw record survives the unchanged64KiB tail
with32KiB postamble allowance (source bound16,514 bytes). Ordinary ELF lacks the
capture symbols. All seven older AP/Haswell profiles pass again on the final
candidate. Exact Q35 identity refusal saves/unmounts/returns; missing workspace
returns without capture/save. No VM fixture accepts native firmware resources.

The exact staged payload is independently backed up outside build. USB baseline
596 files/1664 clusters, all prior hashes, clean fsck/allocation and headroom
verify. At this documentation checkpoint the candidate is uninstalled/unarmed;
local run records own subsequent installation, arming and collection. The next
authorized step preserves recovery/accepted backups and runs the no-input
firmware-only capture automatically. Native acceptance remains the earlier
`86257762f155128f` observation until collection; no translation/ownership or
RC6/package-C3 acceptance follows from preparing this trial.


### Capture revalidation after current-main integration (2026-11-04)

Before merge/install, fresh fetch found main `5f4f6fc6` with PR317 isolation,
PMM/VMM, USB retirement and current Apps/UI pins. The prepared `f3cfcb90` /
`2ee43cb527989448` candidate remains independently backed up, uninstalled and
unarmed; it is superseded for this run. Integrate through merge `1de62896`,
preserving both new firmware targets and upstream GPT-inspector targets in the
Makefile EOF conflict. The host longjmp fixture correction remains intact.

Rebuild and repeat full local checks, capture sanitizers/codec/tail and the
sequential exact-stage/legacy VM matrix against the consumed integration.
Firmware copying still requires ACPI reclaimable/NVS RAM and exact existing
translations, consistent with new canonical-address VMM checks. No native
acceptance transfers to PMM/USB/desktop changes merely through this merge.
Local run state remains revalidating/unarmed until the replacement candidate
passes every required check and fresh storage/recovery verification.


### Integrated firmware-only trial ready for installation (2026-11-04)

Current-main capture validation completes at `aec97036` after consuming PR317:
all requested normal/AP/RAM/USB, core/desktop/widgets/profile/host/docs groups
pass in one fresh invocation. The71 installation cases pass with the unchanged
single opt-in exhaustive-matrix skip. Capture sanitizers, independent128-entry
producer/decoder, complete raw-tail retention and all seven legacy plus both
exact/missing-workspace VM cases pass against that integrated kernel.

A subsequent main `a90bf57e` xz packaging merge is incorporated as `e53b7a63`.
The only port delta is xz/README; kernel, capture and core userspace sources are
unchanged. The rebuilt diagnostic ELF is byte-for-byte identical to the fully
qualified `aec97036` kernel. Matched images, capture tests/sanitizers/retention
and exact-image Q35 refusal/save/unmount plus missing-workspace return pass
again. This retains relevant earlier results without repeating unchanged
subsystem suites or transferring native acceptance to the new port.

Final staged release is `6b7dbee3a9441281`, compiled source `e53b7a63`, from
[PR362](https://github.com/PaulMDemers/SrvrOS/pull/362). Its exact payload and
superseded uninstalled candidates are backed up outside build. The baseline
remains596 USB files/1664 clusters with prior hashes/integrity/headroom verified.
At this checkpoint it is uninstalled/unarmed; the authorized next step preserves
console recovery and accepted backups, installs and automatically runs this
no-input firmware-only capture. Local records own subsequent arm/collection
state. No native translation, register-access permission, pin or ownership is
granted by these tests; original RC6/package-C3 limitations remain unchanged.


### Native firmware/full-catalog observation accepted (2026-11-04)

Release `6b7dbee3a9441281`, compiled source `e53b7a63` from merged
[PR362](https://github.com/PaulMDemers/SrvrOS/pull/362), automatically returned
and collected session `session-00065f79059839bf-00`. Independent re-decoding of
the complete15,124-byte exit receipt matches the saved assessment. Both136-byte
DMAR copies match the unit SHA256 pin and all42 current Limine entries agree.
Copy brackets are21/16 us, separated by4 us, under unchanged100ms bounds.
The table is at `0x8cd64000`, inside the ACPI-reclaimable entry
`[0x8cd64000,0x8cd8f000)`, with exact existing translations verified before reads.

Both declared DRHD pages (`0xfed90000` and `0xfed91000`) have zero catalog
intersections. Their nearest reserved predecessor is
`[0xfed1c000,0xfed20000)` and successor `[0xffe10000,0xffe40000)`. These native
observations corroborate this unit's declarations/gap edges; no resource/MMIO
admission or DMA translation state follows. No VT-d/GT/GGTT register, new mapping,
root-table walk, firmware method, writer, pin or hardware ownership is accepted.
The Intel16/Linux32 discrepancy and full-path swizzle/lifetime remain unresolved.

All596 prior files retain their hashes apart from expected workspace metadata.
Two new logs yield598 files/1667 allocated clusters; read-only fsck and independent
allocation checks pass with zero leaks, bitmap mismatches or errors. The raw exit
SHA256 is `9a21ea46259d9f158fe0025710b92efb4d01b398217c5f4aa95acfb5b3d045f2`.
All623 automatically backed-up evidence files independently verify. Fresh
installed/recovery payload checks, Ubuntu default and consumed one-shot pass;
collector disabled, no reboot armed. Evidence remains durable outside build.

Closeout consumes main `3fcf2630` and recursive pins for documentation only;
later stream/port changes do not inherit acceptance from this tested release.
Next is an exact-unit source audit of bounded read-only VT-d register/resource
admission and retirement before implementing a separately opted-in test. The
native gap is not a generic mapping permission or a copied Broadwell gate.
Firmware exclusion, real allocation/GGTT ownership, GT wake ownership and matched
scanout/restore retirement remain separate. Prior RC6 timing failure and zero
package-C3 residency are unchanged.


### Exact-unit VT-d read/resource audit scope (2026-11-04)

Base `db95fc9b`, native firmware/catalog run already collected and closed.
PR301 at `69367853` remains independent Broadwell scanout/cache work. Audit
primary VT-d register width/read semantics and UEFI resource discovery against
this unit's pinned DMAR and complete native catalog. Add only a host-side
candidate-contract audit/refusal tests and Air6,1 documentation. No native
mapping/register/writer, shared DMA driver, boot profile, policy or limit change.
Keep the GPU-specific page distinct from include-all/IRQ/HPET scope and preserve
all prior ownership unknowns. Save primary-source hashes and actual-receipt
host audit, validate locally and merge before proposing an implementation/test.


Audit complete: primary VT-d4.1 sections8.3/11.1–11.4 permit the proposed aligned
DWORD read set, distinguish TES status from programmed RTADDR and do not justify
root traversal or writes. The host-only contract replays the accepted raw receipt
and admits only a resource candidate for the explicit GPU page with this unit's
exact native gap edges. Generic reserved coverage, partial/all-type overlap,
missing/changed/sibling edges, malformed catalogs and forbidden offsets refuse.

A separate prerequisite was identified: PCD alone is not effective strong-UC
proof under inherited PAT/MTRR/CR0 state. Current VMM adopts inherited CR3 and
its physical-translation query supplies no full leaf/cache/alias proof. The next
bounded work is feature-gated read-only Air6,1 CPU cache/alias context without
mapping VT-d, before any register-window implementation. PR301's architectural
cache mechanism and Broadwell address gates remain independent; its accepted
PAT values or classifier results do not transfer to this unit.

`make test-haswell-firmware check-docs` passes, including eight new gap/offset/
failed-receipt/foreign-cache regressions and actual core/adapter/hash checks.
Actual native-receipt host replay passes the candidate audit with all cache,
mapping/read permission, DMA-state and ownership fields false. Primary PDFs,
source hashes and host output have a durable copy outside build. No kernel,
policy, limit, installed payload or physical acceptance changes; no reboot armed.


### Air6,1 read-only cache/alias context scope (2026-11-04)

Base `d10a4e10`, prior firmware run collected/closed. PR301 common architectural
cache core/header/tests are inspected at `e48abf79` and reused unchanged at the
same paths to avoid a competing fork; its Broadwell adapter/gates/addresses and
acceptance are not imported. Add an exact Air6,1 opt-in firmware/cache-context
profile: bounded CPU0 PAT/MTRR/CR0 snapshots plus RAM-only x86 page-table walks
of the known HHDM candidate alias and an unused proposed private window. Never
dereference the VT-d virtual/physical target, map it or change cache policy.
Require fresh pinned firmware/catalog/PCI guards, raw preservation, finite bounds,
refusal tests and matched host/VM checks before preparing the automated trial.
No native ownership, mapping grant or full-alias proof is claimed in advance.

PR376 candidate validation: compiled source `57c6b416`, exact release
`058606de6d8652b2`; harness-only follow-up `a03790ba` corrects the new profile's
progress assertion while requiring its independently decoded strict refusal.
Full matched normal/AP/image builds, core/desktop/widgets/profile/host/docs
checks pass, including89 installation tests with one existing opt-in skip.
Three baseline GCC misleading-indentation diagnostics were fixed by whitespace
only; no warning or integrity check was disabled. Shared architectural cache
core/header/tests remain byte-identical to PR301 `e48abf79` (also unchanged in
its subsequently fetched `535e8435`).

ASan/UBSan common core, CPU RAM walker and production adapter maximum/nine
failure cases pass. Independently decoded49-MSR/16-PTE/128-catalog-row raw output
survives the unchanged64KiB tail with32KiB postamble allowance. Ordinary kernel
excludes the diagnostic. Sequential exact-stage Q35 checks pass legacy firmware,
legacy32-fence capture, new cache/firmware refusal/save/unmount/return and missing
workspace. The initial new-profile VM attempt failed its legacy-progress harness
assertion after correct guest refusal; that failure is preserved locally.

Native cache-context capture is prepared for an unattended one-shot with visible
progress and automatic save/unmount/Ubuntu return. Fresh local checks verify AC,
USB identity, all598 prior file hashes,1667 allocated clusters, zero leaks/errors
and file/directory headroom. Independent payload/evidence copies survive build
cleanup; Ubuntu remains recovery/default and the known-good console is retained.
Require both raw CPU/cache contexts and eight matching CPU-page-table RAM paths,
bracketing pinned firmware/catalogs, completion and offline integrity before
acceptance. Physical result remains pending; no VT-d target read, new mapping,
write, policy change, exhaustive alias proof or ownership follows from this trial.

### Native cache-context closeout (2026-11-05)

PR376 merged as `f11ce344`; release `058606de6d8652b2`, compiled source
`57c6b416` and harness `a03790ba` completed unattended session
`session-00065f7f1ab4e407-00`. Independent replay matches the automatic assessment.
Two complete raw CPU/cache contexts agree: PAT=`0x0000010500070406`,
MTRRCAP=`0xd0a`, DEF_TYPE=`0xc06`, CR0=`0x80010033`, CR3=`0x8cf06000`,
CR4=`0x620`, HHDM=`0xffff800000000000`. Each uses37 MSR reads/eight PTE loads;
whole contexts are63/58us with a7us gap, within unchanged49/16/100ms bounds.
All eight raw walks agree; both the candidate HHDM address and private window
are absent at level2. The covering variable MTRR is UC, and hypothetical PAT
indices0/2/3 classify UC. No actual device leaf, all-alias proof, mapping grant
or VT-d translation state is observed. No VT-d data read or policy/control write.

Bracketing pinned firmware/catalog observations agree across all42 rows; the
complete21,839-byte exit log survives with SHA256
`5be895e235db6e0f929e381b23fd6306c7bbf71489e81ebd2841bde0727bffaf`.
All598 prior files retain hashes apart from expected workspace metadata; two
new logs yield600 files/1670 clusters. Automatic and fresh offline checks pass
with zero leaks, bitmap mismatches or errors and sufficient headroom. All511
automatically archived evidence files and installed/recovery payload hashes
verify independently. Ubuntu default and consumed one-shot verify; collector
disabled and no reboot armed. Payload/evidence backups remain outside build.

Closeout consumes main `88bdc382` and its recursive pins for documentation only;
new runtime/browser changes do not inherit this older payload's native acceptance.
Next is a bounded source audit of resource reservation and CPU alias/leaf admission
before implementing any separately opted-in VT-d read window. Actual ownership,
reference-count discrepancy, GT/full-path swizzle and scanout/restore retirement
remain separate. RC6 timing failure and zero package-C3 residency are unchanged.

### VT-d resource/CPU-alias admission audit scope (2026-11-05)

Base `379d6f8d`; native cache run collected and durably closed. Inspect current
PMM/VMM and PR301 `dc569037` boot-client/cache work without editing its shared
mechanisms. Add a host-only exact-cache-receipt admission audit and refusal
regressions, plus this record and the current DMA guide. Distinguish a catalog
gap and two absent VAs from an exclusive resource lease or complete CPU alias
census. Audit prospective4KiB leaf cache/permission encoding and retirement
requirements; keep EFER/NX enablement and live-root exclusion explicit. No kernel,
new mapping/read, firmware method, policy, bound or physical acceptance change.

Audit finding: PMM starts gaps reserved and only frees USABLE catalog entries;
reserved-frame frees refuse, but no exclusive MMIO lease follows. Inherited and
private in-space mappings are absent from the public kernel mapping list, and
upper-half root copying can share lower tables. A two-VA lookup cannot supply a
complete alias census. Current map/unmap helpers also need expected raw-leaf,
root/CPU exclusion and explicit poisoned retirement checks before device use.
The prospective PAT3 supervisor RO/XD/non-global4KiB leaf classifies UC under the
accepted context, but NX capability/current EFER.NXE were not captured. The
host-only audit models this encoding while retaining every live admission gate
as unproved; full raw receipt, malformed/changed/refused inputs, two absent VAs,
external claims, leaf corruption, WP and UC-minus/WC/overlap refusal tests cover it.
Next is bounded read-only relevant-root/alias and NX-state diagnostics with
explicit completeness/refusal. Existing limits and hardware grants stay unchanged.

Source `996fc363` passes `make test-haswell-firmware test-haswell-cache-context
test-x86-cache check-docs`, including six new admission regressions and the
existing actual firmware/cache adapters, raw decoders, RAM walker and architectural
cache matrix. The accepted21,839-byte native receipt replays as
`HOST_ADMISSION_AUDIT_BLOCKED`: its two absent candidate VAs and UC encoding
observation leave every live resource/alias/NX/leaf/retirement grant false.
Audited source/reference hashes, replay output and local test logs are retained
with a durable copy outside build. No payload installation or physical result
changes; previous capture remains closed and no reboot is armed.

### Bounded read-only alias/NX diagnostic scope (2026-11-05)

Base `3f158022`; prior native run collected/closed. PR301 `7e33d1a5` remains
independent GPU ownership work; no competing VMM edit is present in that branch.
Add a separate v3 firmware/cache profile, retaining v1/v2 and their49-MSR,
16-PTE context caps. New census has explicit64-table/32768-entry per-pass and
4096-byte lossless encoding caps, two exact passes within100ms total, and refuses
any nonkernel registered root. Read-only NX/EFER endpoints and CPU0 IRQ exclusion
bracket the census. Keep complete raw RAM table data or refuse; no target data,
new mapping, page-table/policy write or hardware ownership. Touched scope: isolated
Haswell diagnostic/core/decoder, a read-only VMM sole-root guard, profile/harness,
tests and own docs. Require max-tail, guard/overflow/change/fault tests and exact
VM save/return/refusal checks before native installation under existing authority.

PR381 candidate `dbb1756c`, release `715cee81b804a3ab`, now has complete local
validation coverage. Broad build/core/desktop/widgets/profile/host/docs on
`2db07928` completed89 installation tests (one existing opt-in skip) and the
installed-root exFAT cases; its sole remaining failure was a legacy FD fixture
link. The published fixture support and dependency were reused in `dbb1756c`.
All other targets then passed again, explicitly retaining `test-installed-root`;
its source inputs, driver binaries and both native kernels/initramfs are proven
unchanged. The original failed run and successful retry remain separate evidence.

The new runtime baseline needed GCC/legacy-signal fixture compatibility already
published in PR301 `941f37eb`/`7e33d1a5`; reused fixture bodies and targeted recipes
are identical to those revisions. The signal-state production change is verified
whitespace-only. No warning, assertion, integrity gate or limit was disabled.
An earlier fixture/kernel hex-format mismatch was corrected before this validation;
the strict decoder now requires the actual fixed-width kernel spelling. Failure
step/cursor records retain the exact last source/index and expected/read word.

ASan/UBSan alias core, real VMM guard and actual adapter complete/refusal cases
pass, including64 tables/32768 reads per pass and the4095-byte maximum lossless
payload. Both raw passes survive directly in the unchanged64KiB tail with the
audited16,514-byte source postamble plus8192-byte margin. Independent strict
decoding and common-cache byte identity, ordinary-kernel exclusion and version1/2
compatibility verify. Five sequential exact-stage VM trials pass: legacy firmware,
legacy cache, legacy32 fences, new identity-refusal/save/unmount/return and missing
workspace. Q35 refusal is not native alias/NX acceptance.

Fresh local model, AC, USB identity/all600 prior file hashes,1670 clusters,
headroom and zero allocation leaks/errors verify. The unattended native candidate
retains console recovery and independent payload/evidence backups. It must save
both complete raw census passes, cache/NX/firmware guards and clean integrity
results before acceptance. No device target read, new mapping, firmware ownership,
power-policy change or hardware-alias completeness follows; physical result pending.

Final host-only decoder revision `7f08b972` also rejects out-of-width CPUID words.
Its affected suites, actual producer/sanitizer fixtures and maximum-tail checks
pass; the saved exact VM refusal receipt redecodes correctly. Native payload,
profile and VM harness remain byte-identical to the tested `dbb1756c` candidate.
The run record distinguishes these tool and native source revisions explicitly.

### Pre-init scheduling correction before native alias trial (2026-11-05)

PR381 candidate715cee81b804a3ab was installed but never armed or booted. Source
review confirms main starts persistent `/init --system` before the late capture;
the sole-root guard would therefore refuse as intended. Preserve that guard,
v3 history and all budgets. Add a distinct v4/pre-init modifier: capture before
creating the first user root, retain immutable raw results, and emit once after
normal init readiness and USB logger startup. Require an explicit pre-user-init
receipt epoch, actual prepare/report ordering tests and matched VM save/return
checks. No root is ignored, killed or temporarily hidden, and no policy/ownership
or native acceptance is granted. Previous candidate evidence is retained locally.

PR382 source `81d1013f`, v4 release `6d59e033859033b9`, passes current matched
normal/AP/USB builds and affected firmware/cache/alias/VMM/docs checks. Broader
PR381 desktop/core/installer receipts remain named historical evidence; no
unrelated runtime/filesystem source changed in this scheduling correction.
Actual adapter prepare/report tests, ASan/UBSan and independent epoch/raw decoders
pass, including new-root rejection after capture without any reread on report,
single emission,64-table/32768-entry passes and4095-byte lossless output within
the unchanged64KiB tail. No guard or budget was weakened.

Six sequential exact-stage VM trials pass v1 firmware, v2 cache, v3 alias,
legacy32 fences, v4 refusal/save/unmount/return and missing workspace. The v4
serial order proves capture progress before init starts and raw reporting after
init/logger startup. Fresh AC/model/USB600-file hash and1670-cluster headroom/
allocation checks pass with zero leaks/errors. The v3 payload and110 development
evidence files are independently archived as superseded-unbooted; no physical
alias/NX result is claimed. V4 native pre-user-init acceptance remains pending.


### Native pre-init alias census refusal (2026-11-05)

Collected run `0aad811f8a2f4140`, source `81d1013f`, release
`6d59e033859033b9`, session `session-00065f90c3adc353-00`. The v4 preparation
runs before user init and the immutable receipt survives deferred logging, but
the diagnostic does **not** pass: `status=2 step=3 pages=64 reads=5166,0`,
`bytes=882,0`, and `completed=NO`. Preparation spans4698us, well below100ms.
The cursor is table `0x02004000`, entry45, raw `0x02032023`. At that first-pass
nonfinal entry, the source attempts to add a new table with all64 slots occupied;
this is table-cap exhaustion, not a deadline, encoding-cap or second-pass change.
The full graph has at least65 distinct tables; its exact size is not established.
No complete two-pass raw graph or bracketed NX/cache proof is accepted.

Independent strict v4 replay matches the collector's failure. The16006-byte
exit receipt is retained with SHA256
`c79d1bac23910235e7659f4f6314a1e88c78e9ee9766c13d9c96948643dec908`.
All600 prior file contents are preserved apart from expected workspace metadata;
only two new session logs appear. Offline fsck, independent allocation and fresh
headroom checks pass:602 files,1673 clusters, zero leaks/mismatches/errors.
Installed and durable payload hashes, console recovery, Ubuntu default and the
consumed empty one-shot verify. Automatic collector is disabled; no boot is armed.
The523-file automatic evidence archive was independently rehashed outside build.

Next is a source/layout audit of the inherited table graph and feasibility of
complete alias coverage at the existing bounds. Do not increase caps, omit roots
or silently count a partial graph as complete. The current32768-entry pass cap
also cannot enumerate65 full512-entry pages. Preserve failure evidence and
separate any future coverage design from resource/leaf/retirement admission.
No VT-d target access, mapping, policy write or ownership is authorized by this
receipt; prior accepted CPU/cache observations remain historical and unchanged.


### Alias census layout/feasibility audit scope (2026-11-05)

Base `7b887bbe`; v4 failed native run collected and closed. Audit the pre-init
mapping producers and saved firmware/receipt evidence, distinguish inherited
from kernel-added tables, and reproduce the capacity lower bound with host-only
fixtures. Touch scope: source-only/host-only audit, regression fixtures and own
current docs; no native producer, mapping, boot order, power policy or limit change.
Inspect current PR301 GPU/VMM admission and PR19 Wi-Fi work without duplicating
them. Preserve the failed64-table receipt; do not stage a rerun that cannot
establish full coverage under its existing32768-entry-per-pass ceiling.
Next checkpoint: reviewable feasibility result, local tests and durable evidence.


### ECAM census feasibility audit complete (2026-11-05)

Source audit identifies kernel-created PCI ECAM mappings before the v4 capture,
not merely inherited bootloader tables. The archived MCFG and byte-identical
current Linux table declare156 buses at `0xe0000000`. Current4KiB mapping requires
78 PT pages plus at least3 upper tables:81 pages /41472 loads per full pass,
already beyond the unchanged64-table/32768-entry bounds. Native v4 retains no
full graph/raw MCFG, so this is a source-derived lower bound, not direct attribution
of every native table or the exact failed cursor. The native65th-table refusal
remains the direct observation.

Actual PCI/VMM-source fixtures verify every ECAM physical translation and exact
P/RW/PCD/NX leaf, saturation of the incomplete128-entry public mapping registry,
unchanged RAM tables during census,64-page full two-pass success and65/81/131-page
mandatory refusal. The separate host-only strict MCFG model has six tests; core,
adapter/decoder, PCI and VMM checks and ECAM ASan/UBSan pass. No source limits,
policy, mapping or boot order change; no new native test is staged or armed.

See the [current layout audit](../macbookair6-1-dma-inventory.md#ecam-layout-and-fixed-bound-feasibility-audit-2026-11-05).
Next is a separately reviewed exact2MiB ECAM mapping-footprint design with
permission/cache equivalence, conflict refusal and rollback tests. This could
remove78 leaf-table pages without shrinking mapped coverage; whether the entire
remaining native graph fits is still unknown. An earlier snapshot, source intent
or partial census cannot establish current full-root admission. Raw evidence and
source-matched validation have durable copies outside build; prior physical
acceptance and recovery remain unchanged.


### Checked2MiB ECAM mapping design scope (2026-11-05)

Base `4cc0408b`; native v4 failure is collected and the layout audit is merged.
Develop a host-only exact-unit mapping proposal and fault-tested publication/
retirement model before a kernel mapper or native activation. Scope: tools,
affected cache-decoder host helper/tests, host-test recipes and this unit's
current design docs. Preserve mapped coverage, P/RW/PCD/NX attributes, all existing
census limits and ordinary PCI/VMM behavior. Require homogeneous MTRR coverage
inside each large leaf, explicit PAT-bit placement, zero-slot admission and
precise failure/rollback/quarantine rules. Historical evidence cannot authorize
live publication. Current PR301 GPU and PR19 Wi-Fi remain separate; inspect their
actual heads and do not duplicate production VMM/main/PCI changes. No new boot is
armed. Next checkpoint is a reviewable design with host evidence and open native
backend requirements, not a hardware-acceptance claim.


### Checked ECAM mapping design complete (2026-11-05)

The [host-only proposal and backend contract](../macbookair6-1-ecam-mapping-design.md)
preserve all39936 ECAM4KiB physical mappings and P/RW/PCD/NX attributes using78
prospective2MiB leaves. The accepted older CPU/cache receipt classifies each
large leaf's complete4KiB granules uniformly UC; historical data grants no live
permission. Large-leaf PAT bit12 remains clear, PS bit7 is set, and no cache/control
writer is introduced. The original strict cache receipt still replays identically.

Twelve host proposal/lifetime test groups cover cache/geometry/input failures,
zero-slot and exclusion guards, detached failures, uncertain publication and
retirement, exact A-bit changes, terminal retention and permanent handoff.
Actual-source VMM synthetic layout tests verify all mappings/end bytes, exact
attributes, public-registry behavior and checked user-root preparation/refresh;
the unchanged census accepts its three reachable tables/1536 loads per pass.
Original4KiB boundary refusals, cache/alias/layout, PCI/VMM and sanitizer checks
remain passing. These are host design proofs, not a native mapper or rollback.

Next implement the narrow checked boot backend and fresh pre-PCI identity/cache/
NX admission with real fault-injected publication/retirement tests, explicit
bounded read accounting and unchanged ordinary PCI behavior. The design uses
one detached PD and one checked parent link; every original4KiB VA is invalidated
before handoff/free, and uncertainty retains the owned frame without fallback.
Registry updates occur only at final handoff. No kernel mapping or policy changes,
new payload or reboot is armed. The failed native v4 receipt remains failed;
all accepted device/recovery evidence remains unchanged and durably retained.


### Checked ECAM backend implementation scope (2026-11-05)

Base `27f805bc`; all earlier native runs collected, no boot armed. Implement the
real boot-only checked mapper and fresh pre-PCI admission for the exact captured
Air6,1 geometry. Keep activation opt-in and the ordinary PCI path, profile budgets
and census limits unchanged. Scope: isolated AP-diagnostic backend/adapter,
minimal guarded VMM/PMM/PCI integration as needed, actual-source fault fixtures,
build recipes and current unit/design docs. Require exact source/RAM/zero-slot
admission, detached staging, ordered single-link publication, full4KiB invalidation,
verified pre-handoff cleanup and retained poison after uncertainty. Fresh CPU/cache/
NX, full raw MCFG/catalog and pre-enumeration legacy PCI identity checks must
replace historical assumptions. Inspect PR301/19 overlap before production edits;
no GPU, Wi-Fi or unrelated ownership changes. Next checkpoint: source-matched
backend/gate tests and matched builds before any opt-in native profile/test.


### Actual checked ECAM backend/gate candidate (2026-11-05)

The real AP-only backend stages a private PD, checks the complete borrowed path,
zero parent slot and registry, atomically publishes one link, invalidates all39936
original4KiB VAs and either retires with verified unlink/invalidation/free or
hands off permanently with the original registry semantics. Detached failure
cleanup also rechecks the borrowed path; unknown state and broken fresh admission
retain the frame. Actual-source fault injection covers all512 private stores
before/after acknowledgement and publication/unlink, source, invalidation,
metadata, clock, ownership and cleanup errors. No A/D masking or relaxed check.

The fresh gate verifies captured CPU/control/NX state, BSP0 and parked four-CPU
entrypoints, restored legacy PCI identity selection, full pinned MCFG/DMAR/catalog
copies and two stable bounded cache/EFER observations. Its maximum49+49 cache
MSRs/two EFER reads and20 identity reads/40 CF8 selection/restoration writes are
explicitly separate from existing capture accounting. The new mapper lifetime is
bounded at65536 RAM words and100ms across prepare/retire-or-handoff; existing
49-MSR/16-PTE and64-table/32768-entry/4096-byte bounds are unchanged. PMM adds only
a read-only bitmap/tag observation with IRQ-preservation tests.

Matched normal/AP/USB builds, affected host suites and actual backend/gate
ASan/UBSan pass. Entry points are retained only in the AP ELF and have no startup
caller; ordinary PCI/main behavior remains unchanged. Native CPU exceptions remain
terminal, not resumable rollback. Existing-profile VM regressions are pending
at this candidate checkpoint. See the [current implementation contract](../macbookair6-1-ecam-mapping-design.md).
Next add the separate versioned opt-in dispatcher, poison handling and complete
raw receipt/tail/VM checks before a physical trial. No new payload is installed or
reboot armed; the native v4 census is still failed and earlier acceptance unchanged.


### ECAM backend validation closeout (2026-11-05)

Implementation `db002799` passes matched normal/AP/USB builds, actual backend/gate
and affected PMM/VMM/firmware/cache/alias/layout/PCI suites, and both ASan/UBSan
fixtures. Eight sequential VMs pass normal syscall ABI, normal UEFI USB keyboard,
legacy firmware/cache/v3-alias/32-fence profiles, exact v4 refusal/save/return and
missing-workspace handling. These exercise the existing paths; the new backend
has no startup caller and gains no native acceptance from them.

Follow-up `54718be4` adds direct2MiB/1GiB RAM-walk cases, rejected device/reserved
sources, lost tags/backward clocks and missing architectural features. The topology
fixture is host-neutral, with identical native early checks, and assertions stay
on even under a compiler's release defaults; unrelated privileged user-frame
routes abort via existing test hooks. New fixture ARM64 cross-compilation passes;
the initial compile failure is retained and is not macOS/ARM runtime acceptance.
All affected checks and ASan/UBSan pass again, followed by six sequential exact-stage
AP profile rehearsals on this source. Ordinary inputs remain unchanged from the
two passing normal VMs. The new helper remains uncalled in all those profiles.

Native pins match the captured complete MCFG and existing DMAR pin; ordinary ELF
exclusion and AP symbol retention verify. Raw/source/VM/validation evidence has a
hash-verified copy outside build. Next is the distinct dispatcher/checked PCI-window
adoption and full raw receipt/decoder/tail profile. No new payload is installed,
no reboot is armed and the failed native v4 census remains the current hardware
result. Existing recovery/Ubuntu default and all prior device evidence are retained.


### Opt-in ECAM adoption and dual-receipt scope (2026-11-06)

Base `ca43ce23`; prior native failure collected, backend/gate merged and dormant.
Add a distinct v5 profile, pre-PCI dispatch and checked one-use adoption of the
active exact ECAM window. Preserve ordinary PCI and v1–v4. Poison is terminal
before PCI/USB access; clean foreign refusal follows the old path but cannot pass.
Retain complete fresh backend/gate raw proof in a separate64KiB `ecam.log`, then
report the unchanged pre-init alias/cache proof into `exit.log`, with a strict
same-session/epoch reference and cross-proof checks. No hash-only or partial
proof substitutes, no context/census/tail limit changes. Scope: guarded main/PCI,
backend snapshots/formatter, logger checkpoint, profile/decoder/collector/fixtures
and own docs. PR301 GPU and PR19 Wi-Fi remain separate; publish bounded changes
and rehearse matched stages before physical install/one-shot under existing auth.


### v5 opt-in adoption candidate (2026-11-06)

The isolated AP profile now dispatches the checked mapper before PCI, consumes
its exact window once with fresh RAM/registry/control verification, and stops
visibly before PCI/USB on uncertain publication or adoption. Normal kernel/PCI
and v1–v4 remain separate. Adoption shares the100ms/65536-word lifetime; the
old64-table/32768-entry/4096-byte census and49-MSR/16-PTE checks are unchanged.
Full early raw proof goes to a separately verified64KiB `ecam.log`, followed by
the old cache/alias proof in `exit.log` and a same-session/epoch reference. The
collector requires both and matches their controls, firmware and ECAM branch.

Matched normal/AP/USB builds and affected host tests pass at this checkpoint.
Actual-source PCI/adoption and formatter fixtures, maximum catalog/MSR output,
4KiB/2MiB/1GiB RAM walks, complete dual-proof decoder and separate-file collector
checks pass. Maximum early output plus postamble fits without tail enlargement.
Sanitizer/ARM compile and sequential exact-stage VMs remain pending here. No
new install or reboot is armed; the old native v4 failure remains failed. Next
freeze the source, complete those checks, self-review/merge and prepare the
exact-unit one-shot with known-good recovery and all prior USB files preserved.


### v5 source/VM validation closeout (2026-11-06)

Implementation `2f53e475`, PR387, passes matched normal/AP/USB/ISO builds, the
actual backend/gate/adoption/formatter/collector tests and affected PMM/VMM/PCI/
firmware/cache/alias/logger suites. Four actual-source ASan/UBSan fixtures pass;
new formatter and PCI fixtures also cross-compile to ARM64. Two ordinary VMs
pass syscall ABI and UEFI USB keyboard checks. Seven sequential staged AP VMs
pass legacy firmware/cache/v3-alias/32-fence/v4 paths plus v5 exact refusal/save/
return and missing-workspace handling. These are workflow/refusal checks, not
native positive acceptance. Complete dual-file host fixture output also passes
for maximum128-row firmware catalogs and49-MSR snapshots within unchanged tails.

A decoder-only review follow-up explicitly requires protected paging and complete
admitted firmware source pages, matching the actual gate's checks. Its host
regressions pass; boot binaries are unchanged. Final exact-stage rehearsal with
that collector is next. The local USB preflight verifies all602 prior files,
1673 allocated clusters, no leaks/mismatches and session-creation headroom.
Current native result remains the failed v4 census. Preserve its full payload
and failure evidence outside build; only its obsolete EFI copy may be retired
for space, retaining Ubuntu and all accepted fallbacks. No reboot is armed at
this checkpoint. Native v5 success still requires both complete receipts and
matching post-return file/allocation checks.


### Native v5 adoption halt — failed (2026-11-06)

Release `56308db224d9e6ab`, compiled implementation `2f53e475` with collector
`f57b5f0f` (merged PR387), stopped visibly at **PCI window adoption failed**.
The supplied photograph records retained-poison state5,35949 checked words and
one parent publication attempt. The dispatch path establishes that prepare and
handoff reached ACTIVE before adoption failed; it does not accept adoption,
continued PCI access or the later complete-root census. No matching USB session
was created, consistent with the pre-PCI/USB terminal screen.

The word cap65536 was not reached. Elapsed lifetime, first failed guard and
expected/observed readback values are absent, so this evidence cannot distinguish
a deadline from a changed table, source/ownership/metadata or other guard. Do not
mask A/D bits, restart the lifetime or relax a bound based on this photograph.
Next add bounded stage/elapsed/first-mismatch diagnostics for adoption and its
fresh stable gate, with failure-path regression coverage before another trial.

On Ubuntu return, all602 prior USB files are byte-identical with no new files;
fsck is clean and independent allocation confirms1673 clusters, zero leaks,
zero FAT/bitmap mismatches and zero errors. The supplied photo, failed run record
and complete collection/integrity evidence are hash-verified outside build.
Ubuntu default and installed recovery/accepted fallback hashes verify. The
collector is disabled, reboot timer inactive and one-shot cleared; no new boot
is armed. Native v5 remains failed; earlier independent device acceptance stays
unchanged. The installed current is this failed diagnostic, with known-good
recovery retained; it must not be called an accepted payload.


### Adoption failure diagnostics scope (2026-11-06)

Base `26e3b161`, failed native v5 fully collected; no reboot armed. Add bounded,
first-failure stage/reason, existing lifetime-clock observations and exact
expected/observed values to the checked mapper/adoption and fresh stable gate.
Show these on the existing pre-PCI terminal screen, without target rereads,
new control writes, retries, A/D masking, restarted deadlines or changed limits.
Keep the v5 success raw proof and normal/legacy paths compatible. Scope: AP-only
ECAM backend/adapter/terminal formatter, real fault/formatter fixtures and own
current docs. PR301 GPU and PR19 Wi-Fi scopes remain separate. Next freeze and
validate source, sequential exact-stage refusal/return rehearsals, self-merge,
then prepare a new unit-specific trial with all602 prior files/recovery retained.


### First-failure instrumentation candidate (2026-11-06)

The AP helper now retains the first stage/reason, existing lifetime sample and
exact available expected/observed values for adoption, checked RAM/HHDM reads
and the fresh stable gate. The existing terminal screen reports those records
without target rereads or extra successful-path timer samples. Invalid/backward
clock samples cannot claim valid elapsed time; the later display clock remains
separate. Strict A/D/word comparisons, IRQ/root/ownership/source checks,100ms/
65536-word lifetime and all context/census/log limits are unchanged.

Actual backend/gate/adoption and terminal-format regressions pass, including
first-failure immutability, changed raw words/metadata/firmware, lost ownership,
cap/deadline/backward-clock failures, no diagnostic table rereads/writes/frees
and one post-failure display-clock call. Existing default and maximum-catalog
success receipts are byte-identical to the prior producer. Matched images and
affected host suites pass; final frozen builds, sanitizers/ARM compile and exact
staged VMs are required before the next install. The previous native v5 halt is
still failed with cause unknown; no new native result or reboot is implied.


### Instrumented v5 validation checkpoint (2026-11-06)

Implementation `d97c36f3` (PR389) passes frozen normal/AP/USB/ISO builds, affected
PMM/VMM/PCI/firmware/cache/alias suites and actual-source first-failure/terminal
regressions. Four actual-source ASan/UBSan fixtures, including mismatch,
backward-clock and deadline terminal variants, pass; profile/PCI fixtures also
cross-compile to ARM64. Seven sequential staged VMs pass legacy firmware/cache/
v3-alias/32-fence/v4 paths and the instrumented v5 refusal/save/return and
missing-workspace paths. The exact stage is `a85cf3e1549a1928`; its byte-verified
payload is retained outside build. These checks do not accept native adoption
or identify the cause of the earlier halt. Normal ELF exclusion/AP-only retention
also verify. The prior native failure and all accepted recovery remain retained.

Metadata component0/1/2 identifies VA/PA/flags; catalog component0/1/2 identifies
base/length/type. Stable CPU-state component0–3 identifies signature/features/
physical width/CR0. Table/index and expected/observed fields are authoritative
only when their detail flags are set. No new reboot is armed at this checkpoint.
Next complete ordinary VM checks, self-merge and verify the unit's installation,
run record, USB baseline and recovery before its authorized one-shot trial.


### Instrumented v5 return — early adoption observed, full census failed (2026-11-06)

Release `a85cf3e1549a1928`, implementation `d97c36f3`/PR389, returned with the
matching saved boot, ECAM and exit files. The complete early raw blob validates
prepare/handoff and one-use adoption:36268 checked words,98684us across the
unchanged100000us lifetime (1316us remaining),37+37 cache MSRs and the pinned
42-row firmware catalog. Session/epoch/root/frame/adoption/save references agree.
The preceding adoption halt did not reproduce; this near-deadline success does
not identify its cause or establish repeatable timing headroom.

The **overall trial remains failed**: the later pre-user-init alias census hit
64 tables after5678 first-pass reads, with zero second-pass reads and945 partial
encoded bytes. The source writes `completed=NO`; the strict dual-file collector
correctly returns no completed proof. No full-root, exhaustive-alias, VT-d resource
or ownership acceptance follows from the early component observation. The ECAM
representation change alone is insufficient to establish that this native graph
fits the unchanged census caps.

A read-only check of the exact staged ELF finds115.160MiB BSS, including the
104.375MiB static `processes` array and its embedded per-process mapping arrays.
The saved root has HHDM, I/O and kernel branches, but the failed census does not
retain a complete native graph. Large static kernel storage is therefore a
source-backed candidate for remaining table pressure, not proven native branch
attribution. Next audit that storage and mapping lifetimes for deferred backing
without reducing process/mapping capacities, and attribute startup cost before
changing it. Review concurrent shared process work before implementation; do
not mask A/D changes, increase caps or restart deadlines.

All602 prior paths remain present:601 files are byte-identical and only expected
internal `.srvros/meta` changed. Three new session logs were saved. Both fsck and
independent allocation pass (605 files,1677 clusters,zero leaks/mismatches/errors).
All raw files, ELF inspection and assessment are hash-verified outside build.
Ubuntu/recovery/fallback hashes verify; collector disabled, timer inactive and
one-shot cleared. No boot is armed. The installed diagnostic remains unaccepted
as a complete profile; prior independent device acceptance is unchanged.


### Mapping-storage and handoff scope (2026-11-06)

Base `e1a30e1e`; prior native v5 collected failed overall, no boot armed. Replace
unused embedded mapping arrays with image-owned deferred full-capacity backing,
retaining65536 mappings per process, all64 slots and existing publication,
ownership and teardown checks. Cover allocation failure before page publication,
fork/exec transfer/rollback, cleanup and slot reuse. Scope: private process
mapping field/helpers, map/cleanup/image-transfer code and real host fixtures;
leave slot/lifetime publication and PR301 owner-revocation work unchanged. PR19
has no process.c delta against this baseline; PR301's process edits concern
scope-owner revocation/publication, not mapping backing. Publish bounded commits
and preserve those independent changes.

Also remove redundant registry duplicate searches for the freshly proved unique
ECAM span, keeping39936 attempts/events, capacity128, complete pre/post registry
verification and the original root/source/clock checks and100ms lifetime. No
policy, ownership gate, A/D mask or limit change. Measure the exact ELF reduction
and source-matched host/VM results before a new physical stage; native graph fit
and timing improvement remain unaccepted until a complete matching receipt.


### Deferred backing and handoff candidate (2026-11-06)

Unused process slots no longer embed65536 mapping records each. A private image
reserves its full original capacity through the checked heap and zeros it in
64KiB service batches before the first user-page publication. Failed allocation
publishes no page. Exec transfers unique backing under IRQ exclusion after old
image retirement; cleanup frees it after the tracked prefix is empty. Atomic
growth requires existing backing; fresh-image, frame-copy and reaping checks
reject inconsistent or still-owned storage.64 slots,65536 mappings and all
other capacities remain unchanged. Slot/lifetime ownership and PR301's separate
owner-revocation changes are untouched.

The AP-only ECAM span now appends the available registry prefix without repeated
duplicate searches after proving the old VAs are excluded and new VAs unique.
All39936 attempts/events,128-row capacity, original root/source/clock checks,
invalidation and complete pre/post verification remain. Every starting count
0–128 matches the old registry result. This does not restart the100ms deadline
or change the65536-word cap, old census/context limits or any policy.

All17 process suites, full-capacity storage/ownership failures and actual ECAM
fault/oracle tests pass. Matched images and the full local core suite pass
(one optional exhaustive I/O test skipped by default). The measured AP ELF BSS
drops exactly96MiB to20090472 bytes (about19.160MiB); its process array is8781824
bytes. Sanitizers, frozen-source builds and staged VM checks remain next at
this checkpoint. Native graph fit/timing improvement remain unaccepted; the
prior failed run/evidence/recovery are retained and no new boot is armed.

The final native sanitizer checks pass for full-capacity mapping storage, image
lifecycle, loader and checked ECAM backend/gate. The new storage fixture and
ECAM gate also cross-compile to ARM64. Initial cross-compilation exposed old
unrelated STI/HLT instructions emitted by process host fixtures; those unexpected
host paths now trap, while native disassembly and relocations compare identical.
All process suites were rerun after that host-only adaptation. Native runtime
acceptance and exact-stage VM checks remain pending at this source checkpoint.


### Frozen mapping-storage validation (2026-11-06)

[PR391](https://github.com/PaulMDemers/SrvrOS/pull/391), compiled/tested runtime
`ad1d6199`, exact v5 release `83ef1c984bb62a79`: full local core/build checks
passed, including the89-test installed-root matrix (one optional exhaustive
I/O case skipped by default). After the host-only privileged-instruction
adaptation, all17 process suites and the actual ECAM backend passed again;
native disassembly/relocations stayed identical. Five actual-source ASan/UBSan
fixtures pass, and mapping-storage/ECAM-gate fixtures compile for ARM64.

Seven sequential staged Q35 checks passed: legacy firmware, cache context,
alias-v3, fences32, v4, exact-v5 refusal and v5 missing workspace. The v5 success
formatter fixtures remain byte-identical to the original. Normal images exclude
AP diagnostic symbols; the AP image retains them. These VM results validate
refusal/collection and shared runtime behavior, not Haswell native admission.

Next native run checks the unchanged early100ms/65536-word handoff and later
64-table/two-pass full managed-root census with the smaller image. Require both
matching raw receipts and complete final save/unmount/return. An early halt or
missing completion remains failed/incomplete. Ubuntu stays default; protected
accepted recovery and full prior failed payload/evidence remain outside build.
No power policy, mapping permission, integrity check or limit was widened.

Ordinary syscall, UEFI USB keyboard, fork, exec-vector and scheduler-yield VM
checks also passed against the matched normal image. Native graph/timing
acceptance is pending the automated run; no result is inferred from Q35.


### Smaller-image native closeout (2026-11-06)

PR391 merged as `bcd75f13`; runtime `ad1d6199`, release `83ef1c984bb62a79`,
session `session-00065fa40ca8a88a-00` returned automatically and was collected.
The full profile **failed**, with its explicit `completed=NO` retained. The
strictly decoded early raw proof observes successful checked ECAM adoption in
93775us (6225us margin to the unchanged100ms limit),36268 checked words, one
parent publication and the original39936 invalidations/metadata attempts. This
is one bounded component observation, not full-profile acceptance or a native
process-lifecycle stress pass.

The smaller image removes the prior64-table exhaustion: pass one traversed all
30 tables /15360 entries and encoded3870 bytes, within unchanged limits. Pass
two refused at read6847, table `0x8cebf000` index190: expected
`0x8cebe003`, observed `0x8cebe023`, a raw accessed-bit transition. Preserve
that exact comparison; do not mask A/D bits or retry a failed measured pass.
The source reads/writes its own static result backing while walking, so
measurement-induced first access is a hypothesis to audit, not an established
attribution. Next scope should identify the affected mapping and first-touch
path and establish stable measurement prerequisites before the original bounded
two-pass capture, retaining all RAM/root/translation guards and limits.

All605 prior files are present and hash-identical except expected `.srvros/meta`;
only three matching logs were added. Offline checks pass at608 files /1681
clusters, zero leaks/FAT-bitmap mismatches/errors. Raw failed evidence and exact
payload are hash-verified outside build. One-shot state is clear and collection
disabled; Ubuntu and protected accepted recovery remain. No new boot is armed.


### Raw-census first-touch scope (2026-11-06)

Base `6896aa9d`; prior failed native run collected, no boot armed. Audit the
walker/result-storage first-touch path and add a guarded, bounded RAM-only
preparation traversal before the original two exact snapshots. Preparation and
both snapshots share the original100ms clock; each traversal retains64 tables
and32768 maximum reads, and measured output remains4096 bytes/pass. Record
preparation separately; it is additional read-only prerequisite work, not a
third accepted snapshot, a failed-pass retry or a restarted deadline. Prime
private result backing before observations; never write page tables or mask
A/D changes. Fail closed on preparation errors or graph expansion afterward.

Touched scope: diagnostic alias core/adapter/result contract, host/core/collector
regressions and own machine/design records. Normal startup remains unchanged.
PR301 owns separate Air7,2 scanout/process revocation work and PR19 owns Wi-Fi;
actual open PRs and current integration baseline inspected, no conflicting
alias-core changes published. Next checkpoint: reproduced first-touch race,
exact-comparison negative tests, matched build/staged VM validation and bounded
automated native capture under existing reboot authorization.


### First-touch preparation candidate (2026-11-06)

The original core fails the targeted HHDM first-touch reproducer at the first
comparison (status5,2049 reads); the new core completes guarded preparation
and both exact snapshots (6144 reads for four tables). Private result backing
is primed inside the shared lifetime before source observations. Preparation
retains the same RAM/root/translation accessor and discovers the graph once;
expansion afterward is refused. Measured A-bit, D-bit and address changes still
fail, as do preparation/source failures, shared-clock expiry, malformed graphs
and all existing table/output bounds. The maximum64-table fixture still passes.

Affected alias/cache/firmware/ECAM core, adapter, strict collector, layout and
proposal suites pass, including legacy receipt compatibility and malformed
preparation refusal. Matched normal/AP/USB/ISO images build; actual core/fixture
ASan/UBSan checks pass and both compile for ARM64 with assertions enabled.
Exact-stage VMs and native acceptance remain next. No measured raw-bit masking,
PTE write, changed cap, restarted deadline or failed-snapshot retry is introduced.


### Frozen first-touch stage (2026-11-06)

Runtime/collector `0b9b303e`, exact release `ab360d3ce2d281e4`: all seven
legacy/new staged VM rehearsals pass, including v5 refusal and missing
workspace. Normal image symbols exclude the diagnostics and the AP image
retains them. Original v5 producer receipts remain byte-identical; joint
collector checks retain the new preparation proof, reject a bad receipt and
fit the unchanged65536-byte log-tail budget, including maximum fixtures.
Native complete root/timing acceptance remains pending. Additional preparation
work is counted separately while sharing the original census lifetime.

Ordinary syscall, UEFI USB keyboard, fork, exec-vector and scheduler-yield VM
checks also pass against the matched normal image. The automated native trial
will require complete same-session raw proofs, preparation accounting, final
completion and clean USB save/unmount/return; prior failures remain retained.


### Exact boot mapping/root context accepted (2026-11-06)

PR393 merged as `30bcac81`; runtime/collector `0b9b303e`, release
`ab360d3ce2d281e4`, session `session-00065fa46d93e81f-00` passed the full
automated profile and strict matching dual-file decoder. Checked ECAM adoption
took93608us with36268 checked words, within the unchanged100ms/65536-word
lifetime. One parent publication,512 private writes,39936 invalidations and
metadata attempts, zero frees and one adoption remain verified by the raw proof.

Guarded preparation read15360 entries across30 tables in13658us. Both measured
passes then read15360 entries and encoded3816 bytes each, matching exactly
including A/D bits. Preparation plus both passes took40181us inside the original
100ms clock, with64-table/32768-entry/4096-byte caps unchanged. NX/EFER/control
endpoints, pinned firmware/cache context, same root/epoch and early adoption
reference all passed. Target leaves are zero. This accepts the boot-only checked
ECAM CPU mapping and pre-user-init sole-managed-root context on this exact unit;
it does not grant live post-init admission, exhaustive hardware aliases, a
VT-d resource lease/read, graphics ownership or another model's acceptance.
The earlier raw-change failures remain failed; the targeted host reproducer
and this native pass support first-touch preparation without proving every
detail of the earlier native transition's cause.

Completion was YES, final logs saved, USB unmounted and Ubuntu returned
automatically. All608 prior files remain hash-identical except expected
`.srvros/meta`; only the three matching logs were added. Offline checks pass
at611 files /1685 clusters with zero leaks/FAT-bitmap mismatches/errors. Exact
payload and full evidence are hash-verified outside build. Collection is
disabled and one-shot state empty; Ubuntu and protected recovery are retained.

Next is an audit of remaining live resource/alias/leaf admission and checked
retirement before any separately opted-in VT-d read window. Keep ordinary
startup and power policy unchanged. Actual ownership, reference-count discrepancy,
GT/full-path swizzle, RC6 timing and package-C3 blockers remain separate.


### Post-census admission audit scope (2026-11-06)

Base `4bba4b93`; accepted release `ab360d3ce2d281e4` is collected and closed,
no boot armed. Consume its strict same-session dual-file boot/root proof in the
host-only VT-d admission audit. Distinguish observed boot-only30-table coverage
and NX state from fresh live exclusion, a device-resource lease, actual RO/NX/UC
leaf admission and poisoned retirement. Reinspect current PMM/VMM APIs and
PR301's actual merge-base delta without editing its separate ownership work.
Current PR301 has no new memory/alias-core delta from its integration ancestor;
PR19 Wi-Fi and PR379 WebCore remain independent.

Touched scope: host admission tool/refusal tests and own current machine/DMA
guides. No kernel, profile, policy, limit, mapping or device-read change. Validate
raw session/epoch/source linkage and reject missing/cross-session/changed proofs;
old context-only mode must remain blocked. Replay the exact accepted receipt,
publish source findings and the next bounded implementation contract. No physical
rerun is required for a host-only audit; retain accepted payload/recovery.


### Post-census admission audit complete (2026-11-06)

The host tool now requires both raw logs and the exact session for boot-proof
mode and uses the existing strict joint decoder. The accepted `ab360d3ce2d281e4`
receipt replays as `HOST_BOOT_ADMISSION_AUDIT_BLOCKED`: historical boot mapping,
complete30-table managed-root coverage and enabled NX are observed, with zero
target leaves; every live resource/mapping/read/ownership grant remains false.
Legacy context-only replay remains blocked and unchanged.

Source inspection confirms PMM reservation is not an exclusive device lease;
the128-row public VMM list is partial; the sole-root predicate assumes its CPU0
caller and does not validate AP state; generic existing-map acceptance checks
PA rather than exact flags; generic unmap lacks an expected-leaf token, clear
readback and retained poison. The ECAM boot backend owns a separate permanent
window and is not a resource grant. The [read-window design](../macbookair6-1-vtd-read-window-design.md)
specifies fresh whole-lifetime exclusion, exact-page token, absent-slot/owned
publication, RO/NX/non-global/UC verification, per-load translation checks and
expected-leaf/TLB retirement with retained claims/backing on uncertainty.

Affected firmware/read/admission/cache/alias/ECAM/layout/collector suites pass,
including four new boot-admission tests using the real producer, maximum
fixture, missing/changed/cross-session/raw/completion refusal, external-claim
non-authority and CLI argument pairing. Exact accepted native replay and docs
checks pass. No kernel/profile/policy/limit change or new physical acceptance;
installed payload/recovery unchanged and no boot armed. Next implementation is
a dormant checked resource/mapping backend with actual-source failure fixtures,
then window-only qualification before any separately opted-in target reads.


### Dormant checked window backend scope (2026-11-06)

Base `b9615fc5`; accepted native run closed, no boot armed. Implement an AP-only
VMM backend for the exact absent PDPT7 / PD409 / PT80 window, staging two private
frames and one parent publication. Single-attempt owner/epoch token, fresh
guard callbacks plus actual sole-root/translation/RAM checks, fresh bounded
raw census, exact leaf/path verification, local invalidation and detached-frame
retirement; latch poison and retain claims/backing on uncertain publication or
cleanup. Keep the census bounds and a separate65536-word/100ms whole mapping
lifetime; no target-load API. Native gate/receipt/profile integration remains
separate, with no current hardware caller or grant.

Scope: private VMM include/header, AP-only build retention, real VMM host fault
fixture, dormant-backend docs and own record. No generic map/unmap semantics,
CPU policy or installed payload change. Actual PR301 merge-base memory delta
is empty; its scanout and PR19 Wi-Fi scopes stay separate. A local single-owner
token is valid only within a caller-proved exclusive epoch; it is not a global
MMIO lease against bypassing clients. Validate clean refusal, borrowed frames,
raw/source/clock/root/IRQ failures, reentry/stale tokens, exact A/D changes and
retained poison before considering a future window-only native qualifier.


### Dormant window backend candidate (2026-11-06)

The AP-only actual VMM backend now performs a fresh bounded census, stages two
private tagged/nonborrowed pages, validates the exact RO/XD/PAT3 leaf/path and
publishes a single expected-zero parent. Owner/epoch/root tokens,100ms whole
lifetime, separate65536-word mapper accounting, immediate readback, fenced
invalidation and checked detached-frame cleanup are enforced. Uncertainty
retains claim/backing with first-failure poison and no retry. No target-load
API, native gate, boot caller or new hardware grant is added. Effective UC and
all-client exclusion remain the future trusted bridge's obligations; this
local reservation alone is not a global MMIO lease.

The actual VMM/census fault fixture passes clean/refused/poisoned lifetimes,
including existing aliases/slots, borrowed/duplicate/MMIO allocations, changed
raw data/A/D/permissions, source/root/IRQ/clock/cap failures, before/after parent
publication, bad readback, reentry/stale tokens and partial cleanup. A successful
fixture lifetime uses60451 mapper words, with untouched borrowed pages and zero
target loads; synthetic clock values are not native latency acceptance.
Matched images and affected ECAM/alias/cache/firmware/process mapping/image/
service suites pass. Sanitizers/cross-compilation and frozen VM checks follow
before integration. Installed accepted `ab360d3ce2d281e4` and recovery unchanged;
no reboot armed and native window qualification remains pending.


### Frozen dormant-backend checks (2026-11-06)

Runtime `a1802859` passes the actual VMM/census fault suite under ASan/UBSan
and compiles its full fixture for ARM64 with assertions enabled. Ordinary VMM
disassembly and relocations are identical to the pre-change object; the normal
image excludes the API while AP retains prepare/retire with no boot caller.
Seven sequential legacy/new-profile staged VMs pass, including exact v5
`5b83b22ff8bdfc8c` refusal and missing-workspace recovery. Those boots validate
existing-profile regression, not execution or native acceptance of the new
backend. Matched normal/AP/USB/ISO and affected process/image/service, ECAM,
firmware/cache/alias/collector tests pass. Native timing and whole-client
resource admission remain unmeasured; do not weaken bounds on their behalf.

Ordinary syscall, UEFI USB keyboard, fork, exec-vector and scheduler-yield VMs
also pass against the matched normal image. The validated code remains
`a1802859`; this follow-up changes documentation only. No hardware run, install
or reboot is performed for the dormant backend. The accepted native payload
and its earlier evidence remain independent and retained.


### Window-only native integration scope (2026-11-06)

Base `4cb7b5a7`; installed accepted `ab360d3ce2d281e4` closed/collected, no
boot armed. Add fresh exact-unit CPU0/parked-AP/cache/NX/WP/firmware gate to the
dormant backend, retaining whole-lifetime CPU0 IRQ/mutation exclusion before
first user root. Reuse the proven gate implementation through a distinct
compile-time adapter without overwriting early ECAM evidence. Retain raw
before/linked/retired/private-page observations before freeing frames; bounded
separate window.log and strict same-session collector, new explicit v6 profile.
No target loads, register writes, generic API/policy changes or widened limits.
Poison halts before user init/USB reuse; clean refusal saves and returns.

Scope: private backend evidence, gate/template/report, pre-user dispatcher,
fixed-name quiesced log snapshot, staging/VM/collector and failure tests, own
guides. PR301 scanout and PR19 Wi-Fi remain independent; inspect main/staging
deltas before integration and preserve their profiles. Validate real gate and
formatter, malformed/refused/raw/maximum receipt cases, existing profiles and
exact-stage VM before installing one-shot under standing reboot authorization.


### Window-only profile candidate (2026-11-06)

The explicit v6 recipe now holds CPU0 interrupt/root/client exclusion before
first user init and calls a separate instance of the exact-unit firmware/cache/
NX/WP/parked-AP gate. It verifies the observed catalog gap and PAT3 strong-UC
classification, publishes/retires the checked window, retains raw observations
before PMM free and saves separate window.log plus matching exit reference.
Neither target page is a permitted RAM source; there is no target-load API.
Poison halts before init/USB reuse with latched progress; clean refusal saves
and returns. Existing profile recipes and all limits remain unchanged.

The combined actual ECAM + window gate/backend/formatter fixture passes strict
three-file decoding, including a128-row catalog and log-tail bounds. Eight new
Python tests cover raw fields, missing/duplicate/cross-session proof, separate
file collection, profile dependencies, fresh CPU/NX/WP/AP/cache/gap/MCFG refusal
and terminal retained failure. Existing ECAM, alias/cache/firmware and logger
regressions pass. The updated shared decoder preserves its old strict rules;
new cross-epoch checks never mask a measured census or owned window entry;
monotonic hardware A/D transitions and unexpected EFER changes have dedicated
positive/refusal regressions. Stopped clocks during verification retain poison.
Native qualification remains pending; matched builds, sanitizers and frozen VM
rehearsals precede the next automatic one-shot test.

Actual backend, standalone gate/formatter and combined early-ECAM/window
fixtures pass ASan/UBSan and compile for ARM64 with assertions enabled. The
complete/refused/poisoned and maximum-catalog paths are included. Raw complete
receipts explicitly record claim release, and failed saved receipts preserve
census/gate/retirement progress without rereading hardware. Frozen full core
and matched images/VM validation remain next at this checkpoint.

Full-core validation exposed a stale futex extraction harness left by the
earlier PROCESS_ARCH_STI portability change. The harness now verifies the
exact native macro and host-trap definitions, models only the macro boundary
and preserves the extracted production function bodies verbatim. The prior
failed full run is retained; rerun the real interleaving fixture and complete
full-core validation before staging. No futex runtime or limit is changed.


The first staged v6 VM exposed a harness-only version range that selected the
old firmware-progress assertion instead of cache/ECAM/window checks. The guest
saved refusal records and returned cleanly. Extend the version selector through
v6 so all existing strict three-file refusal assertions execute; preserve that
failed rehearsal and rerun the sequential matrix. No kernel or gate change.


### Frozen window-only profile validation (2026-11-06)

Runtime `9a09ec3d` and fixture correction `c94297a8` pass matched normal/AP,
USB/ISO builds and full `test-core`, including the installed-root matrix
(one explicit opt-in test skipped). The earlier stale futex extractor failure
is retained separately; the corrected real interleaving fixture passes without
changing production futex code. Actual backend, standalone gate/report and
combined early-ECAM/window fixtures pass ASan/UBSan and ARM64 compilation.
The normal image excludes the diagnostic backend/gate/report; AP retains the
real path and excludes synthetic test pins.

Nine sequential staged VMs pass legacy firmware/cache/alias/fence/v4/v5
regression, v5/v6 missing-workspace recovery and exact v6 safe refusal. Five
ordinary syscall, UEFI USB keyboard, fork, exec-vector and scheduler-yield VMs
also pass. The selected exact v6 payload is `fad46d637616b072`. Q35 refusal
checks staging, saving and return only; native window qualification is pending.
The successful VM harness revision is `d19ac35f`, with unchanged payload
hashes from `c94297a8`. This docs-only follow-up does not change the tested code. Other machines'
scanout and Wi-Fi scopes remain independent. Installed accepted ECAM/root and
recovery payloads remain retained for the next automatic one-shot trial.


### Native window-only safe refusal (2026-11-06)

PR397 release `fad46d637616b072`, compiled `c94297a8`, returned automatically
with matching session `session-00065fa7691d63c7-00` and all four logs saved.
The new window backend refused `HHDM-entry` in its first fresh gate: one mapper
word, no census, allocation/publication/unlink/invalidation/free attempts and
zero target loads/writes. This profile is failed, not physically accepted.
The earlier ECAM raw receipt records adoption in95.552ms; preinit preparation
and two exact30-table snapshots completed, but overall `completed=NO` correctly
prevents joint ECAM/root acceptance for this run.

The saved raw root HHDM entry has low permission bits7. Retained complete paths
to root RAM and the DMAR source contain upper User bits but supervisor leaves:
their effective permissions remain supervisor/writable. The backend currently
requires low bits3 at every level, so rejects the first upper entry. Next scope
is a full-path effective-permission audit/fix with supervisor leaf and cumulative
write/User enforcement, reserved-bit/translation/source validation and exact raw
comparisons retained; add positive mixed-upper/supervisor-leaf and negative
effective-user/nonwritable regressions. No policy or deadline/cap change follows.

All611 prior files survive byte-for-byte except expected `.srvros/meta`; four
new logs make615 files/1690 clusters. Offline fsck/allocation/headroom are clean,
with zero leaks or FAT/bitmap mismatch. Raw failure/evidence remains backed up
outside build. Ubuntu default and accepted ECAM/root plus recovery payloads are
retained. Collector disabled after return; no next boot armed.


### HHDM effective-permission correction scope (2026-11-06)

Base `e8639e92`; collected PR397 failure retained. Audit only the diagnostic
window backend's HHDM path validator, its real backend/combined fixtures and
contract. Permit upper User bits only after the complete path establishes
a supervisor leaf and cumulative writable access. Retain every presence,
reserved-bit, canonicality, RAM provenance, physical translation, alignment,
raw comparison, ownership and timeout/cap check. Own window publication still
requires its original exact supervisor entries. Add mixed-upper positives for
4KiB/2MiB/1GiB source leaves, effective-user/nonwritable and changed-raw refusal.
PR301 scanout and PR19 Wi-Fi remain independent; no foreign branch is imported.
Run actual fixtures/sanitizers, matched build/core and staged/ordinary VMs before
self-merge and automatic v6 retry under standing authorization. Native status
remains failed/pending until matching three-file completion and USB checks.


The candidate now validates the whole HHDM path before approving a pointer:
presence/reserved bits at every level, cumulative R/W, effective supervisor
access plus an explicitly supervisor leaf, then alignment/physical identity.
All24 mixed-ancestor/leaf-size lifetimes pass with borrowed arrays unchanged.
User-leaf (including supervisor ancestors) and read-only ancestor/leaf negatives
refuse before allocation/stores. A post-publication upper U/S mutation still
fails the exact raw comparison and retains poisoned ownership/backing. The
combined real early-ECAM/window/gate/report fixture now also runs the captured
upper7/leaf3 pattern through strict same-session decoding; all9 Python profile
tests pass. Successful core mapper accounting remains60451 words. No limits,
owned-window attributes, firmware/model gates or integrity comparisons change.


### Frozen HHDM correction checks (2026-11-06)

Code `182e7998` passes matched normal/AP/USB/ISO builds and complete test-core
(one explicit opt-in test skipped). Actual backend, standalone gate/report and
combined early-ECAM/window fixtures pass ASan/UBSan and ARM64 compilation,
including the retained native upper-User/supervisor-leaf shape. All nine strict
profile/collector tests pass. The source proof excludes the diagnostic path
from ordinary images and test pins from native AP code. The mapper and census
limits and exact borrowed/owned raw comparisons remain unchanged.

Nine sequential staged VMs pass earlier firmware/cache/alias/fence/v4/v5
profiles plus v5/v6 missing-workspace and exact v6 refusal; five ordinary
syscall/UEFI USB keyboard/fork/exec-vector/yield VMs pass. The selected native
payload is `951a5f67ad85ec0b`. These Q35 results exercise refusal/save/return;
native completion remains pending. Preserve the prior safe refusal and all raw
evidence. A redundant unreferenced early-desktop EFI copy may be retired only
after its exact payload and complete acceptance evidence outside build verify;
retain accepted ECAM/root, stress/reopen and Ubuntu/recovery payloads.


### Native HHDM fix advances to bounded census refusal (2026-11-07)

PR399 release `951a5f67ad85ec0b`, source `182e7998`, returned automatically
with matching session `session-00065fa8c779fe3e-00` and four saved logs. The
HHDM permission fix passed the initial fresh gate. Window preparation touched
all30 tables/15360 entries, then the first measured pass reached2588 entries
before the unchanged100ms whole-lifetime deadline refused; second pass never
started. Last retained good time is99999us after begin, census status3, gate
phases1, firmware bytes332 and168 mapper words outside census. No parent, unlink,
invalidation or free attempts occurred; no mapping was published and target
reads/writes remain zero. Overall `completed=NO` correctly rejects this profile.
The earlier ECAM and separate preinit census emitted complete raw subreports,
but do not establish whole-profile acceptance for this failed run.

Next audit: repeated per-word HHDM translation, RAM provenance, exclusion and
clock-check cost in the window census. Attribute and reduce redundant work only
while retaining raw A/D/permission/address comparisons, source/ownership proof,
100ms lifetime and all existing word/table/entry/data caps. No deadline increase,
retry or mapping authority is accepted by this observation.

All615 prior files survive except expected `.srvros/meta`; four new logs make
619 files/1695 clusters. Offline fsck, allocation and headroom pass with zero
leaks/FAT-bitmap mismatches. Matching raw logs and analysis are retained outside
build. Ubuntu remains default, one-shot entry empty, collector disabled after
return; no next boot armed. Accepted ECAM/root and recovery remain retained.


### Bounded census clock-cost audit scope (2026-11-07)

Base `e8e56ee3`, prior deadline refusal collected. Audit the opt-in window
backend's repeated clock reads inside each guarded HHDM word operation. Keep
every HHDM raw load, provenance/root/exclusion check, exact comparison and all
limits. Coalesce only redundant reference-clock sampling into bounded logical
access scopes: census reads consume their immediately preceding HAC clock
check; ordinary reads/stores retain fresh entry/completion checks and stores
check immediately before mutation. No cached translation or RAM grant. Add
clock/load/provenance/guard counts and phase timing from existing timestamps,
plus delayed/stopped-clock, changed-root/source/raw and publication regressions.
Scope backend/header/report/decoder/tests/own guides; no generic clock, HAC,
VMM, policy or sibling gate change. PR301 scanout/PR19 Wi-Fi remain independent.
Validate old/new actual-fixture clock counts, matched build/core, sanitizers and
sequential staged/ordinary VMs before merge and automatic native retry. Native
qualification remains pending under the unchanged100ms lifetime.


The actual backend fixture measures106618 reference-clock calls before and
33386 afterward, with exactly98851 raw source loads and60451 mapper words in
both successful lifetimes. These are host operation counts, not native latency
acceptance. Clock sampling is coalesced only within bounded logical accesses;
all individual HHDM/raw/provenance/root checks remain. No generic clock, HAC,
translation cache or policy changes. Ten strict profile tests pass, including
a delayed actual gate/backend/report refusal with retained cost diagnostics.
New regressions cover one-use clock tickets, delayed walks, immediate pre-store
expiry, root/provenance changes during walks, post-publication expiry and retained
poison; existing raw A/D, permission, ownership, cap and clock failures still pass.


### Frozen bounded clock-scope checks (2026-11-07)

Code `d5b45505` passes matched normal/AP/USB/ISO builds and complete test-core
(one explicit opt-in test skipped). Actual backend, standalone gate/report and
combined early-ECAM/window fixtures pass ASan/UBSan and ARM64 compilation,
including delayed census refusal and retained terminal failure. Ten strict
profile/collector tests pass. Actual three-file proof covers normal, maximum
catalog and native HHDM shapes with cost records and unchanged raw comparisons.
The ordinary image excludes diagnostic code/test pins. Host operation counts
retain98851 raw loads/60451 mapper words while clock calls fall106618→33386;
this is not native latency acceptance. Cost clock counters measure backend
reference callbacks; independent firmware/cache clock calls remain outside
that count. Phase times retain the shared reference clock.

Nine sequential staged VMs pass legacy firmware/cache/alias/fence/v4/v5 plus
v5/v6 missing-workspace and exact v6 refusal. Five ordinary syscall/UEFI USB
keyboard/fork/exec-vector/yield VMs pass. Selected payload `a80942d96c7c830c`
is hash-verified and backed up for an automatic native retry. Q35 refusal does
not establish native completion. Preserve the previous deadline failure and
unchanged100ms/word/census caps. Only an unreferenced failed-trial EFI copy may
be retired after full payload/evidence hash verification outside build; accepted
ECAM/root and Ubuntu/recovery payloads remain retained.


### Native bounded clock scopes advance into second pass (2026-11-07)

PR401 release `a80942d96c7c830c`, source `d5b45505`, returned automatically
with session `session-00065fb3e56b976e-00` and four saved logs. Initial gate
completed in244us. Window census prepared30 tables/15360 entries, completed the
first15360-entry pass (3924 encoded bytes), and read6857 entries of the second
pass before the unchanged100ms whole lifetime refused. Last good elapsed time
is99998us, with99754us in the census bracket. No publication/unlink/invalidation/
free attempts or stores occurred; no mapping was published and no target
loads/writes occurred. Overall `completed=NO`: this remains failed/pending.

This run completed37577 census reads versus17948 previously (about2.09 times
the progress at the same limit). Retained backend counts:37736 clock callbacks,
176277 raw loads,465489 guard checks,213896 RAM classifications,37619 logical
reads, zero stores; mapper words outside census168. These counts identify the
remaining repeated root/exclusion and provenance work to audit, not per-category
native latency attribution. Keep every live/raw/provenance/ownership check and
all limits while investigating faster equivalent scans/lookups. No translated
pointer or stale-source grant should be substituted for those checks. The
earlier ECAM and preinit census subreports do not accept this failed profile.

All619 prior files survive except expected `.srvros/meta`; four new logs make
623 files/1700 clusters. Offline fsck/allocation/headroom pass, with no leaks or
FAT-bitmap mismatch. Raw/cost receipts and analysis remain outside build. Ubuntu
default, accepted ECAM/root and recovery retained; one-shot entry empty and
collector disabled after return. No next boot armed.


### Live occupancy and catalog lookup scope (2026-11-07)

Base `5e7116b4`; PR401 failure collected. Replace the64 per-record occupancy
booleans with one authoritative64-slot bitmap, used by every registration,
lookup, release and sole-root check. Keep live IRQ/CR3/root/count checks; do not
add a cached empty-registry summary. Preserve64 slots,256 tables/space,128 kernel
map limits, publication ordering and all ownership checks. Add full-slot capacity,
stray-bit/count contradiction and teardown/snapshot regressions. Scope includes
private VMM registry representation and affected host fixtures.

The window-only gate may index its existing validated immutable catalog snapshot
by sorted source-row indices. Reclassify actual row bounds/type on every call;
retain fresh full-catalog continuation comparisons and all source/raw checks.
Prove equivalence against the old full overlap scanner for unsorted catalogs,
all types, gaps, boundaries and maximum/malformed inputs. Earlier ECAM lookup
remains unchanged. Retain native lookup-step diagnostics, prior failures and all
limits. PR301 currently has no overlapping VMM/window edits; PR19 Wi-Fi remains
independent. Publish/test matched core, sanitizers and staged/ordinary VMs before
merge and automatic native retry. Native100ms qualification remains pending.


Directed VMM prepare/range/user-frame and actual window tests pass. Occupancy
coverage exercises every bit, the full64-slot capacity, refusal beyond capacity,
permuted teardown, live count contradictions and unchanged-registry snapshots.
57792 source-row queries match the old scanner across shuffled1–128-row catalogs,
all types, gaps, boundaries and partial pages; malformed/stale index cases refuse.
Fresh catalog mutation still produces retained terminal failure before mapping
publication. All11 profile/collector tests pass with backward-readable cost data.

One-million-call host probes observe root checks about60.17→4.29ms and128-row
RAM queries127.16→24.24ms, with identical accepted RAM results. These isolated
host measurements are diagnostic, not native timing acceptance. Shared VMM
representation changed, so normal-image identity is not claimed. Matched full
core/build, sanitizers/cross-compilation and ordinary/staged VMs precede retry.


### Frozen live-lookup validation (2026-11-07)

Code `cca4d31f` passes matched normal/AP/USB/ISO builds and complete test-core
(one explicit opt-in test skipped). Six actual fixture families pass ASan/UBSan
and ARM64 compilation with assertions enabled: backend, gate/report, combined
ECAM/window, registry preparation, user ranges and guarded user-frame copying.
The registry/range portable models bind unrelated x86-only frame helpers to
traps at their existing test boundary; production bodies remain unchanged.
The initial cross-harness failure is retained. All11 profile tests pass,
including57792 indexed/full-scanner comparisons and retained fresh-catalog
failure. Root/copy/range fixtures preserve bitmap state in snapshot assertions.

The AP object proves a single live occupancy-word reference in its sole-root
predicate. Actual producer/decoder proof covers normal/maximum/native HHDM
shapes with ram_steps diagnostics and unchanged raw comparisons. Nine sequential
staged VMs pass legacy firmware/cache/alias/fence/v4/v5 plus v5/v6 missing-volume
and exact v6 refusal. Five ordinary syscall/UEFI USB keyboard/fork/exec-vector/
yield VMs pass against the changed ordinary kernel. Selected payload
`92d9f134639f509c` is verified/backed up. Native100ms completion and ordinary
native desktop acceptance for the new shared representation remain pending.
Preserve the prior failure, every source/root/ownership check and all limits;
retire only an unreferenced failed-trial EFI copy after complete backup hashes
verify. Accepted ECAM/root and Ubuntu/recovery remain retained.


### Native window-only qualification accepted (2026-11-07)

PR403 payload `92d9f134639f509c`, compiled `cca4d31f`, session
`session-00065fb5328c9afc-00` completed the strict three-file proof and returned
to Ubuntu automatically. The complete claimed window lifetime is77.402ms,
including an188us initial gate and54.524ms census. All30 tables were prepared
and compared in two exact15360-entry passes;3888 encoded bytes/pass and52165
mapper words stay inside the unchanged64-table/32768-entry/4096-byte and65536-word
limits. The RO/NX/supervisor/non-global/PAT3 UC mapping was published and retired
with1026 private writes, one parent publication, one unlink, two invalidations,
two proven-owned frame frees and verified claim release. Retained raw records
are22265 bytes. No target loads/writes occurred and every hardware/read/ownership
grant remains false. Early ECAM adoption also completed in85.518ms.

Retained cost:72254 backend clock callbacks,268741 raw loads,714449 root guards,
327352 RAM classifications,57581 logical reads,1028 store attempts,46080 census
reads and2172423 indexed-row comparisons. These are this exact native lifetime's
observations, not a general latency guarantee. The result physically accepts
only the bounded pre-user mapping/retirement profile; ordinary native desktop
coverage for the changed shared registry and fresh register-read caller/A/D
admission remain separate work. Prior refusals/halts remain historical failures.

All623 prior files survive except expected `.srvros/meta`; four new logs make
627 files/1705 clusters. Offline fsck/allocation/headroom pass with zero leaks or
FAT-bitmap mismatch. Exact payload and full matching raw evidence are retained
outside build with verified hashes. Ubuntu default and independent recovery
remain retained, one-shot empty, collector disabled, no next boot armed.


### Guarded read-caller contract audit scope (2026-11-07)

Base `eefc5bfd`, accepted window payload92d9f134639f509c collected. Audit the
next read caller as host-only protocol work: replay the strict accepted three-file
window receipt, bind unchanged12-DWORD/two-sample limits, and test proposed exact
A-preseeded owned entries with an already-accessed borrowed root. No A/D masks,
source-proof reuse as a live grant, current backend reset or second lifetime
inside the100ms no-read claim. Define two independently bounded100ms attempts
and the20–100ms inter-sample interval including teardown/fresh preparation.
Add pure host path/sample/timeline refusal models and regressions plus current
guides. No native kernel/boot/staging/codec change, target I/O or new reboot for
this audit. A separate dormant read-qualified backend and no-read A-preseed/two-
window qualification precede any actual register-read adapter. PR301/PR19 retain
independent hardware scopes. Existing windows and read/ownership grants remain
unchanged. Publish the audit with exact replay evidence and targeted host checks.


### Guarded read-caller audit complete (2026-11-07)

The [caller audit](../macbookair6-1-vtd-read-caller-audit.md) and host-only models
strictly replay accepted session `session-00065fb5328c9afc-00`, without creating
live grants. The proposed owned parent flags0x23/leaf0x80000000fed90039 preseed A;
borrowed root A must already be set in each fresh pass. No A/D masking or old
no-load-state reset is allowed. Two independent100ms attempts preserve the
12-DWORD/two-sample/two-map contract. The20–100ms inter-sample interval includes
teardown and fresh preparation; blind extra20ms sleeps could violate it.

A naive full-source pre-read wrapper would charge4192 firmware bytes, exceeding
4096. A proposed adjacent-guard coalescing yields3996 bytes; actual-source proof
is still required. Worst unaligned guard/path accounting from61621 fixture words
estimates63586, without increasing65536. Repeated whole-private-page scans per
load are unsuitable. Retain full lifecycle scans and exact pre/post read paths.
A dormant bounded backend with raw retention and qualified guard scheduling,
then a distinct two-window/A-preseed no-target-load native trial, precede any
read-enabled adapter. Historical mapping is not current admission or DMA state.

Nine new host tests cover fixed limits/encoding,256 individual raw-path bit
changes, frame geometry, sequence/width/unavailable/torn/changed data, lifetimes,
stale owner/root/epochs, overlap, gap/frozen clocks/iteration and receipt bounds,
guard budgets and strict/nonpromoting receipt replay. Existing firmware/read/
admission/window-profile tests and docs checks pass. The normal/AP binary hashes
remain identical to the accepted runtime; no kernel, boot recipe or installed
payload changes and no reboot is armed. Ordinary native desktop regression for
the shared occupancy representation remains independent pending work.


### Dormant two-pass qualification backend scope (2026-11-07)

Base `d0ded465`; accepted92d9f134639f509c retained, no pending run. Implement
an isolated compile-flag-only VMM two-context engine with A-preseeded owned
paths, fresh admission per pass, one attempt/pass/boot series, exact raw
pre/post paths for12 qualification slots, zero target loads and independent
100ms lifetimes. Gap includes teardown/repreparation, minimum wait before
publication,20–100ms sample-marker interval and million-iteration cap. Hold
CPU0 IRQ/AP/root/client exclusion throughout. Retain both complete census,
borrowed/private/path/counter observations in bounded RAM; no hash substitutes.

Qualify a one-use phase1 source-check ticket for only the immediately adjacent
verification, preserving3996-byte source and65536-word limits through actual
existing gate instances in host fixtures. Wrong roots/tokens/profiles, changed
raw/source/cache state, delayed/stopped clocks, partial publication/cleanup and
reentry retain existing refusal/poison ownership rules. Accepted no-read code
has no behavior change in existing normal/AP builds; mutually exclusive profile
checks exist only when the new flag is compiled. No native gate/boot caller,
new target-read API, staging/install or reboot in this dormant step. Add actual
VMM/gate fault tests, native compile-only object, sanitizer/ARM64 coverage and
matched normal/AP/core/VM regression proof. Full raw serialization and a fresh
native bridge precede the distinct no-target-load qualification. PR301/PR19
retain independent scope; no competing VMM/window edit is listed there.

### Dormant two-pass backend implemented (2026-11-07)

The compile-only `SRVROS_AIR61_READ_WINDOW` engine now retains two complete raw
contexts with independent100ms attempts, exact A-preseeded owned paths,12
zero-load markers/pass and measured20–100ms inter-pass gap. Actual existing gate
instances charge3996 firmware bytes and63476 mapper words/pass, including the
128-row catalog. Neither budget changed. One adjacent phase1 source token is
consumed exactly once; lifecycle page scans and per-slot exact paths remain.

Host regressions cover both passes, raw path bits, fresh source/cache/root/token
changes, IRQ exclusion, deadlines, reentry, allocation, invalidation, uncertain
cleanup, retained first-pass evidence and profile mutual exclusion. No native
bridge, boot caller, target-load API, installed payload change or reboot is
introduced. Full raw serialization plus a fresh native bridge precede a distinct
no-target-load qualification; existing read/ownership acceptance remains false.

Validation of frozen runtime `fae9b02d`: normal/AP/USB/ISO builds and full
`test-core` pass, including89 installed-root tests and one explicit opt-in skip.
ASan/UBSan execution and the full ARM64 fixture link pass with assertions active;
512 raw-path bit mutations fail closed. Existing producer proof, nine staged
VM profiles and five ordinary syscall/UEFI USB keyboard/fork/exec/yield VMs pass.
The normal ELF remains byte-identical; AP differences are debug-only, with
byte-identical stripped files. Existing builds contain no new backend symbols.
Dedicated raw retention is0x99188 bytes in the compile-only object. Accepted
installed92d9f134639f509c/recoveryc35475f62f154f1e remain untouched. No reboot is
armed. Native latency, raw serialization, fresh bridge admission and the later
no-target-load two-pass qualification remain unaccepted separate work.

### Fresh bridge and raw two-pass receipt scope (2026-11-07)

Base `a35a1952`, accepted92d9f134639f509c/recoveryc35475f62f154f1e retained,
no pending native run. Add two independent production gate instances and a
compile-only native admission bridge for the dormant qualifier. Require current
single-adopted ECAM state, fresh exact-unit admission and live CPU0/IRQ/AP/root/
client exclusion throughout both lifetimes and gap. Compare fresh pass1 source
and control observations against pass0; never use historical receipts as grants.
Terminal poison retains ownership and cannot restore interrupts or resume clients.

Add a bounded lossless two-pass receipt retaining complete raw page/census/gate/
path/cost observations, with session/owner/root/epoch and gap binding, plus a
strict nonpromoting offline decoder and real-producer mutation regressions.
Keep all caps and exact raw A/D checks unchanged. Existing profile/decoder
acceptance stays distinct. No native boot caller, default build selection,
staging/install, reboot, register load or device ownership grant in this step.
Native compilation, host actual-gate/failure tests, sanitizer/portable checks and
matched existing-build/core/VM regressions precede merge. Native boot recipe and
no-target-load qualification remain the next checkpoint. PR301/PR19 have no
competing VMM/window/ECAM edits in their inspected file lists.

### Fresh bridge and raw receipt implemented (2026-11-07)

The compile-only native bridge now uses two production gate instances, fresh
per-pass exact-unit checks and cross-pass raw source/cache/control comparison.
It requires current ECAM ACTIVE/single adoption and holds IRQ exclusion across
both lifetimes and gap. Poison prints retained progress and halts; it does not
restore interrupts or resume clients. Native instances cannot reset.

The one-shot RAM-only R1/Z1 encoder retains both complete raw pass proofs and
all path/cost observations. Lossless canonical zero runs fit the combined
45000-byte wire cap within the unchanged65536-byte tail allowance; per-pass
33000-byte raw bodies and all mapper/firmware/time limits remain unchanged.
The independent strict decoder rejects old-profile confusion, tampering,
malformed extents/compression, stale identities and altered path bits. It never
promotes a retained observation to native/read/ownership acceptance. A native
boot caller, joint saved-session binding and exact staged qualification remain
separate next work; no installed payload or boot state is changed here.

Frozen native/runtime source `1a8bd9f4` passes normal/AP/USB/ISO builds and full
`test-core`, including89 installed-root tests and one explicit opt-in skip.
The five native objects link together without host hooks;18 ASan/UBSan bridge/
encoder cases and the complete ARM64 fixture link pass. Seven final consumer
regressions include512 path-bit mutations plus29 coherently repeated root-flag
changes forbidden by the existing native guard. Eleven existing window-profile
tests, actual producer proof, nine staged diagnostic VMs and five ordinary
syscall/UEFI USB keyboard/fork/exec/yield VMs pass. Both existing ELFs remain
byte-identical to the preceding merged backend build.

Normal/max-catalog receipts retain24701/48317 raw bytes in4864/22456 wire bytes.
The full45000-byte wire cap with framing and a1024-byte envelope is64122 bytes,
below65536. All limits stay unchanged. No native result is inferred. Native
integration still needs a boot caller, strict joint saved-session binding and
an audit of first-touch RAM/A-D effects under the existing bounded preparation;
no raw masks, timing exclusions or limit increases are permitted. Installed
92d9f134639f509c/recoveryc35475f62f154f1e remain untouched; no reboot is armed.

### Native two-pass zero-load qualification scope (2026-11-07)

Base `709c6ef4`; accepted `92d9f134639f509c` and recovery
`c35475f62f154f1e` remain retained, with no pending run. Add a separate explicit
v7 boot recipe/ELF for the two-pass bridge, early pre-user-init dispatch,
bounded raw receipt framing, isolated saved read-window snapshot and strict
joint ECAM/alias/exit/session persistence validation. Existing profiles remain
separate; there is no target-load adapter or new hardware ownership.

Audit first-touch effects and touch next-pass private RAM inside pass0's
existing clock/root/IRQ guards before its census. Do not reset or move work
outside either lifetime, mask A/D changes, or change mapper, source, census,
gap, iteration or saved-tail limits. Keep all complete raw observations.
Retain terminal ownership/IRQ exclusion on uncertain publication/cleanup.
Add real producer/joint mutation/refusal tests, native/portable/sanitizer and
matched build/core/exact staged VM checks. Merge, then preserve recovery and
prior USB files, install/verify one-shot payload and collection records, and
run the unattended qualification with visible progress and automatic return.
Native acceptance requires the matching collected session; missing completion
or a halt is not a pass. No policy or register-read permission is introduced.

The v7 boot implementation now selects a separate ELF without the old window
backend, enforces the explicit two-pass token and runs before user init. It
charges next-pass private RAM first-touch to pass0 before its census with live
per-page deadline/root guards; pass1 retains its full preparation. A bounded
R1/Z1 envelope uses the existing quiesced isolated `window.log` save. Joint
validation binds it to early ECAM and independent source/alias/exit proofs,
requires matching saved references and whole-trial completion, and retains
raw source-to-claim hardware A/D promotions under the existing W1 relation.
Both claimed lifetimes and both claimed graph snapshots remain exact.

Main/staging files are also present in PR301/PR19; this branch owns only the
Haswell v7 dispatcher/recipe and dedicated ELF, separate from their device
features. Their current hunks and unit scopes were inspected; no VMM/two-pass
feature implementation is duplicated. No cross-task messages were sent.

Frozen runtime `b5d9c92e` passes matched normal/AP/two-pass/USB/ISO builds and
full `test-core`, including 89 installed-root tests with one explicit opt-in
skip. Eighteen bridge/encoder and four real early-ECAM/two-pass ASan/UBSan modes
pass, as do both complete ARM64 fixture links. Five joint persistence/profile
tests, seven decoder/producer tests, backend bit/ownership failures and eleven
legacy window-profile tests pass. All eleven sequential diagnostic VM cases
and five ordinary syscall/UEFI USB keyboard/fork/exec/yield VMs pass.

Exact v7 candidate `390c4221b5e50422` passes both unsupported-Q35 refusal and
missing-workspace rehearsal. The native ELF contains both fresh gate instances
and the new backend/envelope, without legacy window or host-hook symbols.
Host fixtures retain 63476 mapper words/pass and the original firmware/time/
census/gap bounds. They do not accept native timing or hardware behavior.

The local immutable-artifact helper's same-inode rename corner case was caught
by payload verification and corrected without changing payload bytes; failed
harness evidence is retained and the remaining VM checks passed. Twenty-four
verified duplicate host initramfs copies were hardlinked, preserving all bytes.
Unreferenced EFI payload `a80942d96c7c830c` was retired only after verifying its
independent complete backup and absence from current/recovery/GRUB references.
Accepted current `92d9f134639f509c` and recovery `c35475f62f154f1e` remain intact.
The automated native trial is being prepared; acceptance remains pending its
matching collected raw session and all prior-file/offline-integrity checks.

### Native two-pass return: evidence first-touch refusal (2026-11-07)

Merged PR408 runtime `b5d9c92e`, release `390c4221b5e50422`, returned
automatically with session `session-00065fb87209eea3-00`. Both independent
zero-load passes retired and released their claims in82575/82244us, using
53673 mapper words each; the measured slot gap was81558us. Early ECAM proof
and whole-session save/completion succeeded. Zero target reads/writes occurred.

**The two-pass qualification is not accepted.** The strict joint decoder
rejects nine changed borrowed-graph leaf entries. Each gains exactly hardware
Accessed/Dirty bits (`0x60`), with unchanged addresses and all other raw bits.
The saved native graph and exact ELF symbols bind these pages to pass0's
evidence output, including linked/retired/private snapshots. Next-pass private
RAM priming alone omitted these current-pass output pages before its census.
This explains the observed graph difference; it grants no masking exception.

All627 prior files remain present and unchanged except expected filesystem
metadata; four new diagnostic logs were saved. Offline fsck/allocation passes
with zero leaks or bitmap mismatches. Ubuntu recovery returned, the one-shot
is clear and no reboot is armed. Raw proof and symbol-bound analysis are
retained locally and independently backed up outside `build`.

Next bounded scope: first-touch complete current/next evidence storage before
the first census, charged to the original guarded lifetime, with regression
coverage for current-pass output first-touch. Preserve exact raw comparison,
all integrity checks and every existing time/word/source/gap limit. No target
register reads, power policy changes or hardware ownership are authorized by
this result. Earlier accepted window-only evidence remains unchanged.

### Both-pass evidence first-touch fix scope (2026-11-07)

Base `ac8c2d5b`; PR409 records the failed native two-pass qualification.
Bounded scope is `air61_read_window.inc` and its host regression: charge
first-touch of the complete two-pass private context, including pass0 evidence
output, to pass0 before its census. Preserve same-value writes, per-page live
clock/root guards, phase ordering and exact raw graph comparisons. No source
or policy limits change. PR301/PR19 retain their independent device scopes;
no overlapping implementation is claimed. Rebuild matched artifacts, run
core/sanitizer/portable and staged VM checks, then self-merge and run one
unattended native qualification with verified recovery and persistent records.

Frozen runtime `f8e16123` passes matched normal/AP/two-pass/USB/ISO builds and
full core tests, including89 installed-root tests and one expected opt-in
skip. The backend regression observes guarded stores across both complete
private contexts and refuses deadline/root changes in either half or at the
final byte before census/allocation. ASan/UBSan passes the complete backend
regression; restoring next-pass-only priming in an isolated fixture is rejected
by the new coverage assertion. Native images contain no observer callback.

Eighteen bridge and four combined-producer sanitizer modes, both ARM64 fixture
links, real joint saved-session proofs, all eleven sequential diagnostic VM
cases and five ordinary boot/syscall/keyboard/fork/exec/yield VMs pass. Exact
v7 payload `517661c4c5a9627f` passes unsupported-hardware refusal and
missing-workspace rehearsal. All source, word, clock and exact-bit checks
remain unchanged; native acceptance is pending its matching collected return.

The unreferenced older ECAM EFI copy `ab360d3ce2d281e4` was retired only after
verifying its independent complete backup and absence from active/recovery/GRUB
references. Current/recovery and accepted window `92d9f134639f509c` remain
verified and intact. The USB baseline is631 files with clean offline allocation.

### Native two-pass zero-load qualification accepted (2026-11-07)

PR410 runtime `f8e16123`, merge `ff189e75`, payload `517661c4c5a9627f`,
session `session-00065fb921c0ecf0-00` completes strict joint ECAM/R1/independent
source-and-exit validation and returns to Ubuntu automatically. Independent
replay of the collected raw files reproduces the pass. Both complete claimed
lifetimes are82610/82379us, each below100000us, using53673 mapper words and
30 census tables per pass. The measured slot gap is81700us, within the
unchanged20000–100000us bound. Each pass retains all12 zero-load path markers,
exact before/after words and complete raw census/gate evidence.

The two borrowed graphs are now exactly identical, including raw A/D bits.
Both mappings retire with verified frame ownership, claim release and no
target reads/writes. The earlier independent source-to-first-claim snapshots
retain171 one-way hardware A/D promotions under the existing relation; these
are distinct from the exact within-claim and cross-pass comparisons. No masks,
clock exclusions, census/word limits or policy changes were introduced. The
prior PR408 trial remains a historical failed qualification.

All631 prior USB files remain present and unchanged except expected filesystem
metadata; four new logs make635 files/1715 clusters. Offline fsck/allocation
passes with zero leaks or FAT/bitmap mismatches. Matching raw evidence and the
complete payload are independently hash-verified outside `build`, alongside
the retained accepted single-window and recovery payloads. Ubuntu default is
unchanged, the one-shot is empty, collection is disabled and no boot is armed.

Acceptance covers only this exact pre-user CPU mapping/retirement qualification
with zero target loads. It supplies no register-read or hardware ownership
grant and no sibling acceptance. Next scope is a separately guarded read-only
register caller preserving the audited12-DWORD/two-sample contract and all
existing limits. Ordinary native desktop regression for the earlier shared
registry change remains separate pending work.

### Guarded target-register sampler scope (2026-11-07)

Base `f01a1ad4`, after accepted zero-target-load qualification
`517661c4c5a9627f`. Bounded objective: a separate explicit v8 read-only
register-sampling ELF/profile and distinct raw receipt/strict consumer, retaining
the v7 qualification unchanged. Scope: existing two-pass backend, bridge,
serializer/envelope, boot selection, host fixtures and isolated collector.
Exactly12 fixed aligned DWORDs per pass, at most two passes, no pointer API,
register writes or table dereference. Every load retains fresh source guard,
exact four-entry pre/post path, raw value and timing; all lifetime, word,
census, source, gap and receipt limits remain unchanged. Uncertain reads or
path/control/retirement changes retain ownership and halt without USB reuse.

Exercise partial/unavailable/torn/changed samples, every path-bit mutation,
wrong/stale context, deadline/root/IRQ/source/cache failure, bounds, no retry,
raw persistence and cross-profile refusal. Preserve accepted qualification and
independent recovery. Run matched builds/core, sanitizers/portable checks and
exact-stage refusal/save/return/missing-workspace VMs before any native trial.
PR301/PR19 still own independent device scopes; no competing device change or
cross-task messaging is introduced. Native acceptance requires collected raw
evidence from this unit; retained historical receipts grant no live access.

### Guarded register sampler validation and native candidate (2026-11-07)

Frozen runtime `f6bddede` implements a separate v8 read-only sampler with its
own ELF, exact opt-in, owner and R2/Z2 receipt. The accepted v7 qualification
remains separate. Each of two claims permits only12 fixed aligned DWORD
loads, retains exact four-entry pre/post paths, raw values, timestamps and
counts, and requires proven retirement. Unavailable/partial reads, torn
brackets, changed samples or uncertain path/control state poison the series
and retain backing/IRQ exclusion without retry or USB reuse. No target write
or RTADDR dereference exists. No power policy or existing limit is changed.

The new per-load cache fault tests exposed the distinction between existing
boundary cache snapshots and continuation-only source guards. The read-enabled
profile now additionally retains13 bounded cache/NX admissions per pass, before
each DWORD and after the batch. Each has the original49-MSR cache-snapshot
cap and one separately counted EFER read, with no reset of original gate
counters. Every snapshot and all its work count inside the claimed lifetime.
All raw cache values/counts/times are preserved; no extra MSR reads occur in v7.

Matched normal/AP/v7/v8/USB/ISO builds and full core tests pass, including89
installed-root tests with one expected opt-in skip. The complete sampler
backend passes ASan/UBSan, including512 pre-load and512 post-load raw path-bit
faults, all read positions, cache/NX/WP/catalog/root/IRQ/deadline faults,
unavailable/torn/changed/partial samples and no retry after poison. Thirteen
combined-producer sanitizer modes and both full ARM64 sampler fixture links
pass; existing bridge/producer sanitizer and portable regressions also pass.

Host tools `4efc7fc5` pass21 affected consumer/profile tests, including
1024 serialized pre/post path-bit mutations, stale first-cache timing, framing,
count, profile and sample corruption. Normal/max-catalog producer records retain
37203/60819 raw bytes in8328/25920 wire bytes. Both fixture passes charge63476
mapper words and3996 firmware bytes. Original pass-body/raw/wire/tail and
clock/census/gap/iteration caps remain unchanged. Model timing is not native
latency acceptance. Only the separate sample image contains the sample entry
points; no native image includes host hooks. Disassembly confirms one32-bit
MMIO load bracketed by LFENCE instructions at the fixed window.

All thirteen final staged diagnostic VM cases and five ordinary syscall/UEFI
USB keyboard/fork/exec/yield VMs pass. Exact v8 payload `0325ad569a8b0a83`
passes unsupported-hardware refusal, saved strict R2 references, automatic
return and missing-workspace rehearsal. Two prior harness failures identified
version ranges that omitted v8; those failed attempts remain retained. Both
classifiers now share a regression-tested table. A stricter host decoder also
requires the first per-load cache snapshot to follow fresh phase1 admission.
The frozen runtime bytes are unchanged by these host-only fixes.

The failed, unreferenced EFI copy `390c4221b5e50422` was retired after its
independent backup and absence from active/recovery/GRUB references were
verified. Accepted `517661c4c5a9627f`, earlier `92d9f134639f509c` and recovery
`c35475f62f154f1e` remain intact and independently backed up. The USB baseline
is635 files with clean allocation. The next unattended native run is being
prepared with persistent records, visible progress and automatic return/strict
collection on a clean outcome. A poisoned claim halts for a photograph and
Ubuntu restart. Native register-read acceptance remains pending matching
physical evidence; no hardware ownership or sibling acceptance is inferred.

### Native register sampler: complete reads, strict graph refusal (2026-11-07)

PR412 runtime `f6bddede`, host tools `4efc7fc5`, payload `0325ad569a8b0a83`,
session `session-00065fbb1ff88738-00` returns automatically with all four logs
saved. The kernel reports24 completed read-only DWORDs, zero target writes,
both mappings retired and both claims released. Raw offsets/values match
exactly between the two samples; cache/NX admission and per-load path/timing
checks complete. Whole claimed lifetimes are83450/87115us with53673 mapper
words per pass; the measured slot gap is85777us. All remain within existing
limits. Early ECAM/source validation also completes.

**Native register-read qualification is not accepted.** The strict R2 decoder
refuses exact cross-pass borrowed-graph equality. Two leaf entries differ: a
kernel text page spanning verification/gap/path functions gains Accessed
(`0x20`), and a page inside the first gate's phase1 firmware evidence gains
Accessed/Dirty (`0x60`). All other raw bits and the graph page set are unchanged.
Exact saved graph/ELF symbol binding identifies these cold regions; it does
not justify masking them. Both-pass output priming does not cover the first
gate's later evidence or these first-executed text translations. The current
sampler receipt remains a failed qualification despite completed reads.

All635 prior USB files remain unchanged except expected filesystem metadata.
Four new logs make639 files/1720 clusters; offline fsck/allocation passes with
zero leaks or FAT/bitmap mismatches. Full raw evidence, the failed payload,
accepted v7/single-window payloads and independent recovery remain hash-verified
outside `build`. Ubuntu returned, the one-shot is empty, collection is disabled
and no new boot is armed. No ownership, policy or sibling acceptance is added.

Next bounded scope: pre-touch both gate-private evidence sets and the needed
trusted kernel text before the first census under live clock/root/IRQ guards,
inside the original lifetime. Audit actual source/word/time budgets and add
first-touch regressions. No hidden target priming load, A/D mask, time exclusion
or cap increase is permitted. Repeat the exact read-only qualification after
host/VM validation; keep historical failures recorded.

### Sampler text/gate first-touch fix scope (2026-11-07)

Base `09ad7c23`, after the failed v8 trial documented in PR413. Scope is
the existing sampler/gate sources, linker-bounded sampler text, host fixtures
and isolated trial tools. Group sampler-owned code into a trusted section
bounded to64KiB/16 pages, then perform guarded RAM-only first-touch before the
first census. Validate exact supervisor/RO/executable RAM paths before/after
each byte read, charging all page-table/code reads to the original word/time
limits. No target load may prime code. Warm both gates' complete private
evidence with same-value writes under pass0's original live guards.

Keep R2 raw comparisons, all firmware/cache/census/gap/tail/mapper caps and
terminal ownership handling unchanged. Add real first-touch/guard/fault
regressions and audit maximum accounting. Run matched core/build, sanitizer/
portable and exact-stage VM checks, self-merge, preserve accepted/recovery
payloads and USB files, then run one automated native read-only qualification.
PR301/PR19 retain independent scopes; no competing device implementation or
cross-task message is introduced. No native acceptance is inferred before
collecting the matching complete raw proof.

### Sampler first-touch fix validated; native trial prepared (2026-11-07)

Frozen runtime/host tools `d3aa3eac` adds same-value priming of both gate
evidence sets under pass0's original callback-only guard. Gate0 must already
have its first fresh admission; gate1 remains unstarted. The read-enabled
image also groups sampler-owned VMM/bridge/gate code into a trusted linker
section. Its actual32620 bytes span8 pages, below the64KiB/16-page refusal
bound. Before the first census, each page receives one RAM-only volatile byte
read, with full fresh path checks before/after and live clock/root guards.
Supervisor RO/executable RAM leaves are required. The exact preparatory
leaf-A effect is checked; no PTE is written and no target load primes code.
All retained census and target path comparisons remain bit-exact.

The maximum16-page fixture charges656 additional mapper words, for64132 in
pass0 and63476 in pass1; both stay below65536. Firmware remains3996 bytes/pass,
with13 independently bounded cache/NX admissions and24 total target reads.
Raw normal/max-catalog receipts are37563/61179 bytes, packed into8588/26180
wire bytes. Original clock/source/census/gap/body/raw/wire/tail limits are
unchanged, and every preparation step counts inside pass0's original lifetime.

Matched normal/AP/v7/v8/USB/ISO builds and full core tests pass, including89
installed-root tests and one expected opt-in skip. Actual guard/store hooks
verify both gate regions and all16 code pages precede the census/first target
read. Regressions include all256 text-path bit mutations, read/root/IRQ/time/D
faults at every code page, bounds/permission refusal, no retry, and the existing
pre/post target-path and cache/sample faults. Full sampler backend and thirteen
producer modes pass ASan/UBSan; both full ARM64 sampler fixture links pass.
Existing bridge/profile sanitizer and portable checks also pass.

All thirteen sequential diagnostic VM cases and five ordinary boot/syscall/
USB-keyboard/fork/exec/yield VMs pass. The21 consumer/profile tests pass in
the frozen core suite. Native ELF symbols confirm the bounded membership and
absence of host hooks; all four runtime images retain their frozen hashes
after VM checks. Exact v8 payload `2ddbd3b285f6bf41` passes unsupported-board
refusal/save/return and missing-workspace rehearsal. No new physical result
is inferred; the prior v8 refusal remains historical failure.

Unreferenced old attended-desktop EFI copy `9851d427b042227b` was retired
only after its independent full backup and absence from active/recovery/GRUB
references were verified. Accepted `517661c4c5a9627f`, earlier single-window
`92d9f134639f509c`, current failed sampler `0325ad569a8b0a83` and recovery
`c35475f62f154f1e` remain intact and backed up. The USB baseline is639 files
with clean allocation. The next unattended trial will save matching R2/ECAM/
source/exit proofs and return on a clean outcome. Uncertain ownership halts
without USB reuse. Native acceptance awaits the matching collected return.

### Native read-only sampler accepted; collector token corrected (2026-11-07)

Bounded return-review scope: collect run `8c366ac267ff4410`, independently
validate the unchanged strict R2 decoder, preserve original collection reports,
and update this unit's acceptance record. Integration baseline is merged PR414
`6415b898`; frozen runtime and tested host tools are `d3aa3eac`. Open PR301,
PR19 and PR379 retain separate scopes. No runtime or shared decoder change is
part of this return review.

Release `2ddbd3b285f6bf41`, session `session-00065fbc382c2c47-00`, completes
all24 attempts/reads with zero target writes. Both12-DWORD samples and both
retained raw borrowed graphs are identical. The strict decoder directly returns
`OBSERVED_RETIRED_READ_ONLY_SAMPLE` from the matching unchanged window, ECAM
and exit files. The complete source/retirement proofs and saved references pass;
both private mappings retire and release their claims. All13 cache/NX admissions
per pass retain their original independent bounds. Lifetimes are88955/83461us,
mapper words54001/53673 and inter-sample gap82075us. All original time, word,
firmware, census, wire and tail limits remain unchanged.

The automatic collector initially reported `FAILED_OR_INCOMPLETE`: its local
wrapper still required the previous zero-load boot token. Replaying every
original predicate identifies that assertion; replacing only the wrapper's
expected token with the exact staged `air61-two-pass-read-only` token makes all
predicates pass. The strict R2 decoder already requires this correct token and
was unchanged. Original collector/assessment artifacts remain preserved beside
the separately reviewed PASS report. The generic attention list also includes
the literal successful `hsw-sample-failure: reason=none` line; it is not a native
failure. No native failure or graph difference was suppressed. PR413's prior
first-touch failure remains historical evidence.

All639 prior files remain intact except expected workspace metadata. Four new
logs produce643 files and1725 allocated clusters; offline filesystem and full
allocation traversal pass with zero leaks, mismatches or errors. Installed,
exact-stage and independent payload hashes match. Accepted zero-load and
single-window payloads and recovery `c35475f62f154f1e` remain verified. Full raw
run evidence and reviewed reports are hash-verified outside `build`. Ubuntu is
default, its one-shot is consumed, collection is disabled and no reboot is armed.

This accepts only this exact isolated read-only qualification. It does not prove
full-interval register stability, that RTADDR points to a loaded table, device
ownership, or permission for register writes/root-table walking. Next: audit the
sampled capability/control values and define a separately gated next operation;
ordinary desktop regression for the shared mapping registry remains pending.

### Read-only capability interpretation audit scope (2026-11-07)

Base `f1fd3f33`, after PR415 acceptance. Scope: add a host-only audit of the
same-session strict R2/ECAM/exit receipt, decode a bounded subset of legacy
VT-d capability/status fields against Intel's register reference, retain input
hashes, and document the next prerequisite. No new kernel mapping, MMIO read,
register write, table dereference, device ownership or power-policy change.
Synthetic decoder checks must remain distinct from native evidence. PR301,
PR19 and PR379 retain their existing independent implementation scopes.
Native replay must use the accepted release/session directly, without test
firmware pins. Preserve Ubuntu/recovery and the accepted payload; no reboot is
needed for interpreting already captured bytes. A future physical operation
requires its own explicit scope, exact-stage tests and fresh admission.

### Captured VT-d capability audit result (2026-11-07)

Host tool source `18292bc5` directly replays accepted session
`session-00065fbc382c2c47-00`, release `2ddbd3b285f6bf41`, through the unchanged
strict R2 consumer with this unit's actual firmware pin. All three input hashes
match the independent accepted trial backup. The host-only interpretation is
`HISTORICAL_CAPABILITY_AUDIT`; it performs no hardware access or installation.
See the [caller audit](../macbookair6-1-vtd-read-caller-audit.md#captured-capability-interpretation-2026-11-07)
for the raw values, register reference and bounded field interpretation.

Graphics-specific DRHD0 reports legacy version1.0,256 domains,48-bit adjusted
page-table geometry and39-bit maximum guest addressability. Translation,
queued-invalidation and interrupt-remapping enable status are clear at both
sampled endpoints; the observed root register is zero. Supported mechanisms
are not active mechanisms, and disabled translation does not prove stopped GPU
DMA, retired firmware or owned backing. DRHD1 was not sampled. Existing RC6
timing failure and package-C3 blocker remain open; no policy or limit changes.

Next concrete checkpoint: prepare the outstanding automated ordinary desktop
regression for the shared mapping registry, preserving the sampler/recovery
payloads and user files. Only after that coverage should a separately scoped
read-only fault/control inventory be considered. No register write, table walk,
invalidation, translation command or power transition follows from this audit.
No new physical test is installed or armed by this host-only work.

Validation at host-tool source `18292bc5`: all26 focused Python tests pass
(5 capability-audit,9 sampler,5 profile,7 qualification), as does the actual
sampler backend with its existing guarded-path/poison/no-retry fault coverage.
Native receipt replay and independent input hashes pass without test pins.
Documentation validation checks1108 local links with zero errors; diff checks
pass. The matched kernel/payload bytes remain those accepted in PR415; no new
kernel or VM acceptance is claimed by this host-only change.

### Ordinary desktop mapping-registry regression scope (2026-11-07)

Base `d3472e06`, after the captured-capability audit. Objective: exercise the
normal desktop/battery build after the shared mapping-registry changes, with
the existing fully automatic30-minute PCM, verified USB disk work, Text/Files/
System open/close cycles, three screenshots and normal retirement/return.
Keep the existing500ms loop,10s app-cycle, screenshot, storage and integrity
limits unchanged; use1% audio for this run. No VT-d diagnostic token, target
MMIO access, translation command or power-policy change belongs in this profile.

Scope is this unit's test preparation/evidence record; use the existing v74
Air6,1 automation without changing runtime workload limits. Build the matched
normal/native battery images and run core checks, populated-Home loaded VM,
ordinary capture VM and exact-stage refusal/return rehearsal sequentially.
Verify this unit, USB `6ADA-85BB`, Ubuntu and console recovery; retain accepted
sampler `2ddbd3b285f6bf41` and known-good stress payload `012aaffda0be860b`.
Publish validated preparation, self-merge, save durable run/collector records and
run one automatic boot under the user's standing authorization. Native pass
requires matching complete receipts, unchanged prior files and clean offline
filesystem/allocation checks. PR301/PR19/PR379 retain independent scopes.


### Desktop regression harness maintenance (2026-11-08)

Full matched builds/core checks pass at `7dbfadd1` (89 installed-root tests,
one expected skip). Pre-install VM failures revealed two harness issues, not
accepted native regressions: the synthetic battery VM kernel was left at desktop
protocol15 while current userspace requires18; the ordinary screenshot harness
selected Return to console using an obsolete relative menu position after
profile actions were added. Preserve both failed VM artifacts. Scope expands
only to screenshot-harness menu navigation and explicit synthetic VM build
preparation, plus guide clarification. Keep every workload/timeout/pixel/readback
and retirement assertion unchanged; rerun both actual VM trials before staging.
The runtime stays frozen at `7dbfadd1`; host-tool source is recorded separately.


### Ordinary desktop regression prepared; VM limitations retained (2026-11-08)

Frozen runtime `7dbfadd1` passes matched normal/native battery/ISO/USB builds
and the full core suite, including89 installed-root tests with one expected skip.
Host tools `f6276f06` fix the screenshot harness's menu selection; no guest
runtime or limits change. Frozen normal/native ELF and initramfs hashes remain
identical through VM preparation. Isolated sampler entrypoints are absent from
both ordinary/native images.

Rebuilding the explicit synthetic battery VM kernel resolves the protocol15/18
fixture mismatch. The old menu harness's failure is also preserved; its corrected
TCG run now passes exact pixels, immutable capture during typing, durable BMP
readback, notification folder/Preview, repeated menu capture and clean exit.

Two current-source loaded TCG runs **fail** the original500ms loop bound:
540486us (3 excursions) and669527us. The first retained interval attributes
486267us to rendering. These remain unresolved software-emulation performance
failures; no renderer fix or TCG timing acceptance is claimed. The identical
populated-Home/400-write-IOPS workload under KVM passes22 app cycles,193713us
maximum loop,1559335us readiness and1639075us close, zero app errors/underruns,
one complete screenshot, clean worker/cache/battery retirement and all67 prior
fixture files. Both accelerators retain the same500ms/10s limits and disk work.
KVM is the passing loaded preparation receipt, not a substitute TCG pass or a
native observation. Native latency acceptance requires the physical result.

Exact native v74 payload `c831655c4120352e` passes TCG GRUB→Limine chainload,
explicit unsupported-Q35 refusal, file preservation, unmount and automatic
return. Manifest source is the tested host-tool commit `f6276f06`; the unchanged
runtime bytes are independently pinned to `7dbfadd1`. The physical trial keeps
all existing30-minute PCM, verified disk overlap, automatic app/screenshot,
loop/lifecycle and retirement requirements. The existing bounded60-second PCM
tail remains valid; playback may extend only as the original producer permits.
No VT-d/ECAM/Haswell capture token is present.

Run `1fbeb56fc50f45f6` uses this unit's USB `6ADA-85BB`, with643 baseline files,
145 directories and1725 clusters; filesystem, full allocation and headroom checks
pass. Only the owned quiet-volume input is intentionally refreshed for1% audio;
its previous bytes are preserved. The unreferenced failed-sampler EFI duplicate
`0325ad569a8b0a83` was removed only after full independent backup/hash and GRUB
reference checks; accepted sampler `2ddbd3b285f6bf41`, zero-load/single-window,
known-good stress and console recovery remain retained. Ubuntu is the default,
with recovery `c35475f62f154f1e` preserved. Install verification, final preflight,
durable run/collector records and merged preparation must precede arming this
one automatic boot. No physical pass is inferred from this preparation.


### Native ordinary desktop regression fails render-loop bound (2026-11-08)

Return-review scope at integration base `ec27e55b`: collect/review run
`1fbeb56fc50f45f6` and preserve its failure without changing runtime, timing or
integrity gates. PR301/PR19/PR379 retain independent scopes; this change only
records this unit's matching evidence.

Release `c831655c4120352e`, runtime `7dbfadd1`, tools `f6276f06`, session
`session-00065fc7c3a95b0d-00`, completes the full1800-second workload at1% volume.
The separately durable text/binary receipts agree on3793 verified disk bursts /
994312192 bytes, within live PCM playback, with345845760 PCM bytes, zero
recorded underruns and worker status0. The PCM length obeys the original bounded
completion tail. This is software playback evidence; audible quality is not
confirmed for this run.

All1079 automatic app cycles complete with zero app errors. Maximum readiness
is1896398us and close1650028us, below the unchanged10s bound. Three screenshots
are acknowledged, their1366×768 BMPs decode, workers/cache retire, follow-up
Files passes and normal exit is requested. Battery retirement reports attached/
detached1, heap/errors/denied0. The enclosing desktop and battery-controls result
correctly remains **FAIL**, despite `desktop-audio-io: completed=yes`.

Responsiveness fails the unchanged500000us loop bound:52840 valid measured loops
include12 excursions; maximum967444us. Its retained attribution is964736us in
rendering,2491us in the wait stage and only small remaining stages. Retained
wait/scheduler records validate directly; no timestamp reconstruction or limit
change is used. The worst interval,607594939–608562383us, overlaps the second
capture's606971966–648590122us interval. This correlation does not identify the
specific render operation or prove capture is its cause. The slowest screenshot
completes in41618156us. Both loaded TCG failures from preparation remain failed
evidence; the short passing KVM run did not establish this native endurance gate.

**Ordinary desktop regression on this source is not accepted.** Earlier accepted
stress release `012aaffda0be860b` remains historical evidence for its source;
the separate isolated sampler acceptance remains unchanged. Next scope: add
bounded persistent subphase and scheduling attribution inside rendering to
separate scene drawing, presentation and capture work, then fix the measured
cause without weakening the500ms/10s limits or integrity checks.

All643 prior files survive except expected workspace metadata; nine new logs,
receipts and screenshots produce652 files /1832 allocated clusters. Offline
filesystem and full allocation checks pass with zero leaks/mismatches/errors.
Installed/exact-stage/independent payload and retained recovery hashes verify;
raw run evidence and the independent failed-result review are hash-verified
outside `build`. Ubuntu returned automatically, the one-shot is consumed,
collection is disabled and no reboot is armed. Missing or failed responsiveness
is not promoted by successful audio, screenshots or clean storage retirement.

### Persistent render attribution scope (2026-11-08)

Base `f94d97d1`; the failed native regression is collected and backed up.
Objective: retain bounded render subphase and scheduler attribution with the
same worst-loop interval, distinguishing stack preparation, scene composition,
framebuffer upload, fence/presentation and bookkeeping. Capture transfer already
runs in LOOP_POST; retain its separate timing/scheduler companion too. Scope:
`userspace/deskd` diagnostic structs/reporting and presentation callbacks,
strict host receipt validation, retention/fault regressions and this unit's
existing automated stress preparation. No renderer algorithm, limits, policy,
source/ownership gates or integrity checks change. Keep current plus retained
worst records only; no per-frame logging or unbounded history/allocation.
PR301 owns independent scanout/handoff work; do not change its kernel/GPU/display
contracts. PR19/PR379 retain unrelated scopes. Validate host/core and actual
loaded/capture/exact-stage VMs, self-merge and run the same automatic native
30-minute/three-capture workload at1% volume. Native timing acceptance remains
pending; the967444us failure and both TCG failures stay historical evidence.


### Render attribution validated; native diagnostic prepared (2026-11-08)

Runtime and tested tools `a9e72ecc` add fixed-size render/capture companions to
the existing worst-loop record. The compiled current-plus-worst loop state is
1928 bytes; render/capture records are312/240 bytes. Composition/upload callbacks
measure their own wall time/counts/area inside the presentation interval; stack,
fence and bookkeeping remain separately bracketed. Independent scheduler traces
cover render and capture bodies. Capture transfer stays in LOOP_POST. No
rendering algorithm or500ms/10s requirement changes, and all diagnostic overhead
remains in outer-loop accounting. Ordinary non-stress paths omit the diagnostic
syscalls. No per-frame logging, heap allocation or unbounded history is added.

Focused and full core checks pass, including the actual C writer→strict reader,
clock/count/overflow refusals, unchanged attribution after1000 later shorter
loops, maximum record-size checks, and89 installed-root tests with one expected
skip. Matched normal/native/VM/ISO/USB builds pass. Both ordinary/native images
omit isolated sampler entrypoints. All frozen payload hashes remain identical
through sequential VM testing; no optional hardware gate or power policy changes.

The current loaded TCG run completes the workload but **fails** the original
500ms loop bound at598881us. Its saved final attribution validates directly:
render555420us, presentation555332us, compose6695us, upload548593us; render
scheduler ready time527499us, blocked0,29 dispatches. Capture transfer takes29us
in that retained interval, with no active capture worker. These are VM-only
clues, not proof of the previous native stall's cause or a latency fix.

The first loaded KVM run halts on a synthetic battery-query deadline and lacks
final completion; it remains FAILED_OR_INCOMPLETE, with partial checkpoints
preserved. An unchanged repeat passes22 cycles,357028us maximum loop,
1122840us readiness and1458959us close, zero app errors/underruns, screenshot,
worker/cache/battery retirement and all67 prior fixture files. Its new render/
capture companions pass the strict saved-record reader. No KVM deadline fix or
general reliability acceptance is claimed from the repeat. The original967444us
native failure and earlier loaded TCG failures remain failed evidence.

The ordinary TCG screenshot trial passes exact pixels, immutable capture during
typing, durable BMP readback, folder/Preview, repeated menu capture and exit.
Exact v74 payload `30f9ea98b926043a` passes TCG GRUB→Limine chainload, explicit
native-board refusal, file preservation, unmount and return. PR301's active diff
contains no overlapping desktop/timing files at this inspection; its scanout and
hardware ownership contracts remain unchanged.

Prepared run `77c67dc520ea441b` uses USB `6ADA-85BB`,652 baseline files,
147 directories and1832 allocated clusters, with clean filesystem/allocation/
headroom checks. Only the owned1% quiet-volume input is intentionally refreshed;
previous bytes are saved. EFI headroom requires archiving only the unreferenced
old desktop-power duplicate `aa576838270d623a` after independently verifying its
complete accepted backup and absence from current/previous/GRUB references.
Current failed-regression `c831655c4120352e`, accepted sampler/zero-load/single-
window/stress and console recovery `c35475f62f154f1e` remain retained and verified.

After verified install, merged preparation and durable collector/run records,
run the same fully automatic30-minute PCM/verified USB/app/three-screenshot
native workload at1%. The collector now saves matching render/capture attribution
even if the original latency gate fails. No native pass or renderer fix is
inferred before collecting the matching return. Ubuntu remains the recovery OS.


### Native render attribution identifies READY/exclusion delay (2026-11-08)

Return review at integration base `ceeabd0d` collects run `77c67dc520ea441b`,
release `30f9ea98b926043a`, runtime/tools `a9e72ecc`, session
`session-00065fc8e0a17311-00`. This scope records strict native attribution and
the failed responsiveness gate; no source policy, runtime or limit changes.

The same1800-second workload completes at1% volume:3794 verified disk bursts /
994574336 bytes,345780224 PCM bytes within the original completion-tail bound,
zero recorded underruns and worker status0. Text/binary disk receipts agree
and all disk work lies inside live playback. All1081 app cycles finish with
zero app errors; readiness2078039us and close1860876us remain below10s. Three
1366×768 BMPs decode, capture/cache workers retire, follow-up Files passes and
normal exit is requested. Battery retirement reports attached/detached1 and
heap/errors/denied0. Listening quality is not confirmed for this run.

Responsiveness remains **FAILED**:51815 valid loops include21 excursions over
the unchanged500000us bound. The worst sequence13946 lasts752295us, with
744855us in LOOP_RENDER. Its four new final companions validate directly from
matching saved snapshots, including clocks, stage/callback accounting, duplicate
agreement and scheduler conservation; no UART repair or inference is used.

Within that retained render interval, stack preparation takes7us, presentation
744830us, fence3us and bookkeeping8us. Two composition callbacks cover448128
logical pixels and742469us; two uploads cover the same area and2344us. These are
wall times, including scheduling. The enclosed scheduler trace records737469us
READY,0 blocked,2 dispatches and80 I/O-exclusion observations. Its longest
READY interval is737022us (`wake=468212658`, `run=468949680`). Thus the dominant
measured delay is waiting to run inside rendering, not established drawing CPU
cost. In this loop capture ID1 is idle, with pending/busy0 before/after and an
11us capture poll; an active screenshot transfer does not explain this interval.

The [scheduler admission code](../../kernel/src/scheduler.c) increments exclusion
observations when an active I/O owner prevents a READY thread from running.
These80 observations are checks, not80 independent waits or measured ownership
intervals. The retained record does not identify the I/O owner/private image or
which eligibility condition rejected the renderer. Do not infer a specific
same-process owner or waive admission from the count alone. Next scope: audit
GUI-render thread READY/user-safe transitions and I/O ownership/exclusion
lifetimes, retaining all ownership checks, I/O safety and500ms/10s limits.

The new native attribution collection is observed and its strict proof passes;
ordinary desktop responsiveness on this source is **not accepted**. Desktop and
enclosing battery-controls correctly report FAIL despite completed audio/work
and clean resource retirement. Previous native/TCG failures and the initial
KVM query-deadline halt remain historical failures; no latency fix is claimed.

All652 prior files survive except expected workspace metadata. Nine new logs,
receipts and screenshots produce661 files /1939 clusters; offline filesystem
and allocation checks pass with zero leaks/mismatches/errors. Installed,
exact-stage, independent payload and retained recovery copies verify. Full raw
and independently reviewed evidence is hash-verified outside `build`. Ubuntu
returned automatically, the one-shot is consumed, collection is disabled and
no reboot is armed. The review is not a new boot preparation.

### User timer-return admission fix scope (2026-11-08)

Base `4bda5009`, after the native737469us READY/80-exclusion observation.
Source audit finds that CPL3 interrupt entry correctly marks its kernel
continuation unsafe, but IRQ0 currently preempts inside `irq_dispatch`, before
returning through the common epilogue. That leaves an ordinary userspace timer
return suspended as an unsafe kernel continuation for the remainder of foreign
I/O. Existing unsafe-continuation tests deliberately protect this state; do not
make IRQ handlers or arbitrary kernel continuations I/O-eligible.

Scope: move timer preemption ownership to the ISR/common-return boundary;
retain kernel-origin preemption safety, timer accounting/EOI/quantum semantics,
cancellation ordering and all optional public/private signal/frame proof gates.
Only a fully unwound user return may temporarily publish user-safe eligibility
around the scheduler yield, with IRQs masked and without teardown in that suffix.
Keep `io_thread_allowed` and same-process/private-image checks unchanged.
Add host regressions for real IRQ/common-return order and actual scheduler
foreign-owner selection, including unsafe/kernel/same-image refusal, then run
full matched/core and IRQ/thread/signal plus loaded/exact-stage VM checks.

PR19's actual remote diff has no `idt.c`/`irq.c`/`scheduler.c` changes at this
inspection; its three-line scheduler-host fixture edit concerns a separate
binding-test volatile declaration. Avoid that hunk. PR301's diff has no competing
timer/scheduler changes. Publish this bounded implementation independently;
no cross-task messaging, user-safe blanket admission or limit change. The
previous native owner identity is not reconstructed from the existing counter;
this fix targets a reproducible source mechanism and needs new native evidence.

### User timer-return candidate validation and native preparation (2026-11-08)

Runtime/tools source `fac79995` implements the scoped return-boundary change.
The original dispatcher on base `4bda5009` fails the new actual IRQ-ordering
regression; the fixed IRQ/ISR/scheduler tests pass. Foreign-owner user-return
progress, unsafe nested/kernel continuations, same-process/private-image refusal,
IF/root/binding/state guards and cancellation before publication are covered.
The I/O admission predicate, signal/frame gates, quantum, integrity checks and
500ms/10s limits are unchanged. The original native interval's owner remains
unidentified; this is a reproduced source mechanism, not reconstructed ownership.

Full matched ordinary/native/VM kernels, USB image and core suite pass, including
89 installed-root tests in725.104s with one expected skip. Public-signal,
private-signal and frame-copy IDT variants compile; this is compile coverage,
not experimental feature activation or guest/native acceptance. Ordinary syscall
ABI, UEFI USB keyboard, fork, exec-vectors and yield VMs pass. The populated-Home
TCG load at400 write IOPS passes the unchanged60-second workload:24 app cycles,
169 verified disk bursts/44302336 bytes, zero PCM underruns/worker errors,
three-way workload overlap and one completed screenshot with follow-up Files
and clean retirement. Worst loop is481500us with zero excursions; maximum
whole launch/close are1202605/1000264us. The worst render companion records
422659us cumulative READY across27 dispatches, maximum single READY20412us
and zero I/O exclusions. These are VM observations, not a native latency pass.

Ordinary screenshot/pixel/immutable-snapshot/durable-readback/Preview/exit checks
pass. Exact v74 native stage `ca0e7e313136e985` passes the Q35 native-profile
refusal/save/unmount/automatic-return rehearsal on that same frozen source.
Normal/native kernels exclude the optional isolated sampler symbols. No VT-d,
ECAM, AP, idle-policy or additional hardware diagnostic is enabled.

Prepared run `4ae9cc9dd6264da3` repeats the fully automatic1800-second PCM,
verified USB create/flush/rename/readback/delete/sync, Text/Files/System cycles
and three-screenshot stress at1% volume, with the same persistent loop/render/
capture/scheduler proofs and original acceptance bounds. Fresh local preflight
confirms this MacBookAir6,1 and its own USB, all661 baseline file hashes,
1939 allocated clusters, clean offline filesystem/allocation and headroom.
Only the owned quiet-volume expiry is intentionally refreshed. The independently
verified payload and run evidence are retained outside `build`; the unreferenced
old failed-regression `c831655c4120352e` EFI duplicate is archived to make room,
with its complete independent backup retained. Existing current/accepted/recovery
payloads remain protected. Recovery stays `c35475f62f154f1e` and Ubuntu remains
the default. Installation/one-shot state is recorded locally; no native result
is claimed here. Collect the matching session after return before another run.

### Native user timer-return stress acceptance (2026-11-08)

PR421's runtime `fac79995`, release `ca0e7e313136e985`, completed run
`4ae9cc9dd6264da3` in session `session-00065fca0d377865-00` and returned to
Ubuntu automatically. The exact-source1800-second stress passes the original
500ms desktop-loop and10s launch/close limits, with no integrity/ownership
relaxation. All56109 retained loop samples are valid, with zero excursions and
maximum130032us, versus752295us/21 excursions on the preceding source.
The1080 app cycles reach maximum launch1485617us and close2789154us, with zero
lifecycle errors. The worst loop is now129884us of ordinary GUI waiting;
its scheduler companion records97389us blocked,32462us READY and zero I/O
exclusions. Its render/capture suffixes take25/10us with neither worker busy.
This is acceptance of this native workload, not proof of every scheduling path
or attribution of the preceding run's unknown I/O owner.

Playback at1% supplies345763840 PCM bytes with zero recorded underruns and
worker status0. Disk work completes3857 verified bursts/1011089408 bytes across
the live playback interval; text and binary receipts agree. Three1366x768 BMPs
decode, follow-up Files activity succeeds, and screenshot/app/cache/battery
workers retire with clean desktop exit and unmount. Audible quality remains
pending human confirmation. Screenshot completion itself remains slow under
load: maximum43.499249s, pixel receive7.318272s and cleanup90.761ms. The original
loop/lifetime requirements pass; this does not claim fast screenshot saves.

All661 prior files survive with only expected `.srvros/meta` changes; nine new
logs/receipts/images produce670 files/2046 clusters. Offline fsck and independent
allocation checks pass with zero leaks, mismatches or errors. Matching session
hashes, receipt predicates, timing companions, decoded images and the complete
619-file return backup were independently verified; reviewed evidence is retained
outside `build`. Installed/staged/independent payload and retained recovery
verify. The collector is disabled and the one-shot consumed; no reboot is armed.
Next bounded performance follow-up is screenshot encode/write completion under
load, retaining the accepted responsive desktop and all existing limits.

### Bounded audible check scope (2026-11-08)

On base `825a79a9`, the user requests a louder approximately30-second check
because the preceding1% run was not audible/not listened to. Prepare an explicit
listening-only capture: five bundled six-second left/right/stereo WAV plays at
25%, automatic Stop/retirement and saved diagnostics before Ubuntu return.
Keep ordinary boot, the accepted1800-second stress and all existing timing and
hardware gates unchanged; no battery/AML or disk/app stress is requested here.
Touch only explicit capture orchestration/staging and its regression/rehearsal.
The current native stress remains accepted for recorded checks; subjective
quality is unassessed. No new physical result is claimed by this scope.

The explicit `--audio-listen` v1 profile is implemented on runtime `c615c282`.
Matched ordinary kernel/RAM/USB builds and audio-service, streaming, WAV and
conversion regressions pass. All90 staging/CLI tests pass. Exact release
`5bc0a824e9f71ab6` rehearses five full streams/5760000 bytes with zero underruns
in30.971897s, Stop, quiet restore, saved diagnostics, clean unmount/allocation
and automatic return. Missing workspace returns without starting playback.
Stage provenance is `f9be0423`; final harness `5ff0662b` differs only in its
profile-specific assertion. The initial successful guest run hit a generic
chipset-probe harness assertion; that failure and raw results are retained
separately, and the corrected exact rehearsal passes. No native audible result
is claimed. PR19's adjacent Wi-Fi capture orchestration remains independent;
future integration must preserve both explicit capture conditions. PR301's
scanout/handoff and PR379's WebCore audit are untouched.

### Bounded audible check accepted (2026-11-08)

Release `5bc0a824e9f71ab6`, runtime `c615c282`, run `7f96f67cb4754329`,
session `session-00065fcaec9ba89f-00` completed five six-second speaker channel
fixtures at25% in30.249052s. All5760000 PCM bytes complete with zero underruns;
Stop and1% quiet restore pass before saved diagnostics, unmount and automatic
Ubuntu return. The user confirms: “the audio was audible and worked fine.”
This accepts the bounded audible speaker check; it does not retroactively
supply subjective listening evidence for the separate1% endurance session.

All670 prior files survive except expected `.srvros/meta`; four new logs
produce674 files/2051 clusters. Offline filesystem/allocation/headroom checks
pass with zero leaks/mismatches/errors, and the complete513-file return backup
is independently hash-verified outside `build`. The one-shot is consumed,
collection is disabled and no new boot is armed. Screenshot completion latency
under load remains the next performance follow-up; no scheduling/ownership or
power-policy scope is added by this acceptance.

### Screenshot write admission audit scope (2026-11-08)

Base `d7ab6db5`, after accepted stress runtime `fac79995` and bounded audible
confirmation. The retained slowest capture spends31.864228s in WRITE, versus
4.766ms encode,7.318272s receive and subsecond fsync/sync/readback phases.
These POSIX writes populate kernel-buffered file contents; device
persistence occurs in later flush/sync phases. Each16KiB write re-enters the
ordinary default-deny namespace I/O admission path. Its exact wait/owner time
has not been independently measured; encoding is not the primary observed cost.

Bounded scope: audit caller batching and test coalescing four adjacent16KiB
writes into64KiB ordinary writes, within existing supported request/file sizes.
Keep the same snapshot/BMP bytes, new-file/O_EXCL handling, checked descriptor
operations, full temporary/final byte verification, fsync/global sync, guarded
rename, cancellation semantics and failure cleanup. Do not change kernel I/O
eligibility, syscall whitelist, hardware/power gates, existing capacities,
500ms/10s responsiveness bounds, or1800-second stress requirements. Add a
reproducer for excessive write-call admission count plus existing short-I/O and
failure coverage; verify exact pixels/readback and loaded responsiveness in VMs
before a new native repeat at1%. No speedup is physically accepted yet.

PR19's current Wi-Fi diff has no shotworker/save implementation change. PR301's
process additions concern its independent scanout/handoff scope; this bounded
caller change avoids those kernel files. PR379 remains an independent audit.

### Screenshot write batching validation and candidate (2026-11-08)

Frozen runtime/tools `989846b3` changes only the shotworker's ordinary buffered
write batching, its host regression and documentation. The original16KiB writer
fails the new200000-byte admission-count regression; the64KiB candidate passes
with four bounded calls and both complete400000-byte combined readbacks. Existing
short-I/O, exclusive temporary/destination, write/sync/corruption/rename failure
coverage passes, including a new uncertain post-rename sync case retaining the
published file while reporting failure. Capture transport, immutable job and BMP
round-trip/refusal tests pass. Matched ordinary/native/VM kernels, RAM/USB images,
the desktop suite and documentation checks pass. Full core is not rerun for this
caller-only change; no kernel/source admission predicate or policy is modified.

The unchanged populated-Home TCG workload at400 write IOPS passes60-second PCM,
26 app cycles, one screenshot and follow-up Files/normal exit with200 observed
overlap windows. Zero underruns/worker/lifecycle errors; maximum loop222555us,
launch699025us and close1188785us stay inside the original bounds. The retained
capture takes13.123122s, with WRITE4.826539s and receive5.166458s. The preceding
same-config VM receipt took24.772760s/WRITE15.770099s. These separate VM observations
support the caller-batching hypothesis; host scheduling is uncontrolled and no
native improvement is inferred. Original full flush/readback/cache/retirement,
receipt and offline filesystem checks pass.

Ordinary capture verifies exact pixels, immutability while typing, durable BMP
readback, Preview, menu/shortcut repetition and exit. Exact v74 stage
`1c74bc8281678786` passes native-profile refusal/save/unmount/return in Q35 on that
same clean source. Ordinary/native images exclude the optional isolated sampler.
Next native run repeats the unchanged automatic1800-second audio/verified USB/
Text/Files/System/three-screenshot workload at1%, retaining the existing phase,
loop/scheduler, lifecycle, file-preservation and allocation proofs. Performance
acceptance remains pending the matching physical result.

ESP preparation may archive only the unreferenced older accepted stress duplicate
`012aaffda0be860b`, after verifying its complete independent payload backup and
absence from GRUB/current/recovery references. Accepted newer stress
`ca0e7e313136e985`, audible check `5bc0a824e9f71ab6`, isolated sampler/zero-load/
single-window payloads and console recovery `c35475f62f154f1e` stay installed and
verified. Historical acceptance/evidence and the archived restorable payload
remain outside `build`; this is space reclamation, not removal of recovery.

### Native screenshot write batching improvement accepted (2026-11-08)

PR425 runtime `989846b3`, release `1c74bc8281678786`, run `f4be328f2bc244f0`,
session `session-00065fcb69c953b0-00` completes the unchanged 1800-second
PCM/verified disk/app/three-screenshot workload and returns automatically.
The slowest retained screenshot takes 23.196470s, down from 43.499249s on the
preceding accepted stress source. Its WRITE phase falls from 31.864228s to
12.669813s, with receive 6.689667s and encode 4.766ms. Final screenshot WRITE
is 11.046327s. These are observed separate native runs, not a controlled
throughput guarantee; batching improves completion, but saves remain slow.

All 53483 loop samples are valid, with zero excursions and maximum 120142us
under the original 500ms bound. The 1078 app cycles have zero errors and maximum
launch/close 1887035/3860868us, below the unchanged 10s limits. The worst loop
contains 100098us of GUI waiting; its companion records 100042us blocked,
26us READY and zero I/O exclusions. No native owner identity is inferred from
these final companions. All three BMPs decode at 1366x768; follow-up Files,
capture cleanup, app/cache/battery retirement, normal exit and unmount pass.
Slowest retained screenshot cleanup is 75.501ms, with no pending retirement.

At 1%, playback supplies 345780224 PCM bytes with zero recorded underruns or
worker errors. Disk work completes 3889 verified bursts/1019478016 bytes during
live playback; binary and text receipts agree. Subjective sound is unassessed
for this unattended run; the separate short 25% audible acceptance remains its
own evidence. No duration, responsiveness, capacity, hardware/ownership or
persistence requirement changed.

All 674 prior files survive except expected `.srvros/meta`; nine new logs,
receipts and screenshots produce 683 files/2158 allocated clusters. Offline fsck
and independent allocation checks pass with zero leaks/mismatches/errors.
Raw session hashes, receipts, final timing/retirement proofs, decoded BMPs and
604-file automatic return backup are independently verified. Installed/staged/
independent payload and protected accepted/recovery images verify. Reviewed
results are backed up outside `build`. The older `012aaffda0be860b` EFI duplicate
was archived only after verifying its independent restorable payload; current,
newer accepted and recovery payloads remain installed. Collector disabled,
one-shot consumed, Ubuntu default intact; no reboot is armed.

Next useful scope is targeted per-write timing to separate admission waits
from buffer growth/copy cost within the remaining 12.7s WRITE phase, keeping
all accepted save checks and bounds. The approximately 6.7s receive phase also
remains a performance follow-up; this result accepts improvement, not instant
screenshot completion or a kernel I/O eligibility expansion.

### Targeted screenshot write timing scope (2026-11-08)

Base `2718c8e3`, after accepted write batching source `989846b3`. Next checkpoint
is measurement, not a new admission or allocation optimization: separate each
screenshot worker's raw buffered-write call wall time, initial I/O-admission
wait, handler work and common-return handoff, plus buffer reservation/growth
and byte-copy spans. Keep all 64KiB batching, capacities, snapshot/rename/full
readback/persistence checks, policies and original endurance/latency bounds.

Use an explicit stress-only diagnostic flag, immutable kernel executable identity,
fixed bounded PID records, and copied periodic/final workspace-log snapshots.
Measured hooks must allocate/log nothing and grant no I/O eligibility or hardware
authority. Retain records across worker exit, expose incomplete/overflow/backward
clock evidence, and require all three worker records after native return. No new
userspace syscall or capture-frame ABI is planned. Touch process timing internals,
IDT observation hooks, stress boot configuration and safe logger reporting, with
host state-machine/ordering tests and strict saved-record validation in loaded/
exact-stage VMs before the unchanged automatic1800-second1% native repeat.

PR301's independent scanout process additions and PR19's Wi-Fi orchestration
must remain intact; neither actual remote diff contains this write timing
mechanism. No cross-task messages or delegation. Current native run is collected,
recovery retained and no new boot armed at scope publication.

### Targeted write timing validation and native preparation (2026-11-08)

Frozen kernel/runtime `cf852581` passes matched ordinary/native/VM and RAM/USB
builds, full core/desktop/docs, including 89 installed-root tests in718.051s
with one expected skip. Focused timing state, exact active executable identity,
enabled IDT gate/handler/handoff ordering, cancellation, process write/teardown,
logger and strict saved-row tests pass. Public/private signal and frame-copy
IDT variants compile; this is compile coverage, not feature activation. Ordinary
syscall ABI, UEFI USB keyboard, fork, exec-vectors and yield VMs pass, as does
immutable screenshot pixel/readback/Preview/repeat/exit coverage.

The unchanged populated-Home TCG workload at400 write IOPS passes with26 app
cycles,201 overlap observations, zero audio underruns/worker/lifecycle errors,
maximum loop499377us and launch/close745469/1307225us. It retains one complete,
retired diagnostic record:63 attempts/completions,4096054 BMP bytes, seven
capacity changes, no failures/invalid/overflow. Summed raw call wall time is
4.719289s: gate4.266253s, handler56.922ms and handoff393.554ms. Reservation
49.563ms and copy4.765ms nest within the handler. This validates the measurement;
it does not establish the native distribution or identify the foreign I/O owner.
Original responsiveness, receipt, retirement and offline allocation checks stay.

Host-tools follow-up `7a27bfc4` gives the diagnostic its own explicit
`--capture-write-timing` / `ram-usb-v1-capture-write-air61-desktop` recipe.
Legacy v74/v80 configurations remain byte-identical to their original recipes,
with fixed-hash regressions and all seven installed current/accepted/recovery
payloads plus independent backups verified. All92 staging/CLI tests pass.
That follow-up changes only host profile/rehearsal/tests, documentation and a
private-header host-test dependency; all frozen kernel/RAM image hashes match
`cf852581`. No kernel/userspace/shared/submodule source changed after the full
core run. Exact stage `f5e9c4c9603cf2eb` on that tool source passes native-board
refusal/save/unmount/return in Q35, with no native diagnostic result claimed.

The new native run repeats the original automatic1800-second1% PCM/verified
USB/app/three-screenshot workload. It additionally requires three complete,
retired raw write records, exact BMP byte totals and consistent nested spans;
missing or malformed observations cannot pass. Gate, handler, handoff, reserve
and copy evidence is retained independently even if original workload bounds
fail. Native cost attribution remains pending. Ordinary/listening profile
behavior, 64KiB batching, all flush/full-readback/rename/ownership checks and
500ms/10s limits remain unchanged.

If ESP space is needed, archive only the unreferenced older stress duplicate
`ca0e7e313136e985` after exact backup and no-reference verification. Keep latest
accepted stress `1c74bc8281678786`, audible check `5bc0a824e9f71ab6`, sampler,
zero-load, single-window and console recovery `c35475f62f154f1e` installed;
historical/restorable payloads and accepted evidence remain outside `build`.


### Native screenshot write timing result (2026-11-08)

PR427 merge `e5d13469`, frozen kernel/runtime `cf852581` and profile tools
`7a27bfc4`, release `f5e9c4c9603cf2eb`, run `78f642b6588442aa`, session
`session-00065fccdfafc692-00`: the automatic1800-second1% audio/USB/app/three-
screenshot repeat passes all original requirements. This accepts the diagnostic
measurement on this unit; no optimization or policy change was made.

All three retired workers retain65 completed raw writes and4196406 BMP bytes
apiece, eight capacity changes each, and zero failures, invalid clocks or dropped
records. Their summed wall spans are:

| Worker PID | Raw calls | Admission gate | Handler | Return handoff | Reservation (inside handler) | Copy (inside handler) |
| --- | --- | --- | --- | --- | --- | --- |
| 10 | 11.268167s | 10.979613s | 21.295ms | 267.029ms | 19.997ms | 0.997ms |
| 480 | 8.540802s | 8.262777s | 20.811ms | 256.976ms | 19.486ms | 1.005ms |
| 957 | 7.726258s | 7.436346s | 20.798ms | 268.870ms | 19.493ms | 0.995ms |

The admission gate accounts for96.25–97.44% of measured raw write-call wall
time. Buffer reservation and copying are small in this run. These are wall
intervals, not CPU time or proof of a particular foreign I/O owner. POSIX wrapper,
progress reporting and user scheduling outside the raw call remain unmeasured.
Slowest full screenshot is23.738913s, including5.897257s receive and12.015772s
WRITE; prior accepted batching run was23.196470s/12.669813s. The measurement-only
repeat does not establish a further performance improvement. Slowest retained
cleanup is29.393ms, with no pending worker retirement.

The run completes1084 app cycles and3859 verified disk bursts/1011613696 bytes
with345862144 PCM bytes, zero recorded underruns and zero worker/lifecycle
errors. All53337 valid desktop-loop observations stay within500ms; maximum
121877us. Maximum launch/close is2276652/1599627us, below unchanged10s limits.
Worst-loop wait companion records102314us blocked,44us READY and zero I/O
exclusions; this does not identify owners for the separate screenshot gate waits.
All three1366x768 BMPs decode; follow-up Files, app/cache/capture/battery
retirement, normal exit and unmount pass. Subjective listening is unassessed at1%;
the earlier bounded25% listening acceptance remains separate.

All683 prior files survive except expected `.srvros/meta`; nine new files yield
692 files/2265 allocated clusters. Offline fsck and independent allocation checks
pass with zero leaks/mismatches/errors. Independent review verifies session
hashes, timing parsers, receipt agreement, BMP decoding and the646-file automatic
backup. Reviewed evidence is retained outside `build`. Installed, staged and
independent payload hashes, accepted payloads and recovery `c35475f62f154f1e`
verify. Ubuntu remains default, one-shot is consumed, collector is disabled and
no reboot is armed. Older stress duplicate `ca0e7e313136e985` was archived only
after independent payload and no-reference verification; its backup remains.

Next bounded scope: identify what holds or delays admission at the screenshot
write gate, including owner duration and scheduling between admissions, before
choosing a fix. Preserve same-process/private-image exclusion, watchdogs, all
flush/full-readback/rename checks,64KiB batching and500ms/10s/endurance limits.
The receive phase remains a separate performance follow-up. No new implementation
or physical trial is started by this result closeout.


### Screenshot admission holder scope (2026-11-08)

Base `1bd3aea9` after PR428 accepted write-timing evidence. Bounded objective:
identify complete owner overlap and blocked/READY scheduling inside the65 raw
write admission waits per screenshot worker. Existing longest-wait rows retain
only the initial owner, while final owner history is a rotating tail; these
cannot identify the complete source of the observed96–97% gate wall time.
Add opt-in fixed-storage per-worker aggregate and worst-wait companions with
stable scheduler thread identities, syscall numbers, exact scope/overlap clocks,
and visible incomplete/invalid evidence. Measurement hooks allocate/log nothing;
ordinary boot remains disabled. Touch scheduler observation hooks, capture
metadata/logger and strict host/VM/native evidence parsing. Do not speculate a
new admission rule or change scheduling selection before attribution is known.

Preserve full filesystem/descriptor serialization, same-process/private-image
exclusion, flush/full-readback/rename checks,64KiB batching,500ms desktop loop,
10s lifecycle and1800s endurance. Validate nested ownership, multiple owners,
wake-to-READY delay, unobserved calls, copied final records and rejection of
incomplete clocks/counts before a matched exact-stage native repeat. PR301's
Air7,2 scanout process scope and PR19's distinct USB Wi-Fi remain independent;
actual fetched main and open PRs contain no competing gate-observation change.
The last run is collected, independently backed up and no new boot is armed.


### Screenshot admission holder validation (2026-11-08)

Frozen source `e4f7f77c` passes matched normal/native/VM/RAM/USB builds, full
core/desktop/docs, including89 installed-root tests in727.921s with one expected
skip. Real scheduler regressions cover ordinary zero-contention admission,
block/wake/reacquisition/dispatch, nested and successive owners, exact state
partitions, unchanged unsafe-waiter exclusion, fixed-capacity refusal, copied
retention, incomplete ownership and clock/overflow rejection. Dispatcher
cancellation/identity ordering and strict missing/conflicting/invalid companion
parsers pass. Public/private signal and frame-copy variants compile; no optional
feature activation or native acceptance is implied.

Ordinary syscall ABI, UEFI USB keyboard, fork, exec-vectors and yield VMs pass.
Ordinary immutable-pixel screenshot/readback/Preview/repeat/exit behavior passes.
Populated-Home TCG stress at400 write IOPS passes26 app cycles,201 overlap
observations, zero audio/worker/lifecycle errors,261521us maximum loop and
724166/848654us maximum launch/close. The retired screenshot worker retains63
raw writes/4096054 BMP bytes and complete new admission companions. Its63 gates
span4.108986s, with4.060601s overlapping125 outermost owner scopes; the state
partition is3.305904s blocked,799.844ms READY and3.238ms running. Nested scopes
do not double-count. Its worst726.036ms gate retains one owner: thread17/PID7,
syscall53 (`FSYNC`), covering725.920ms of that wait. This identifies the VM's
worst-wait owner and validates observation; native ownership/delay distribution
still requires the matching physical record. Independent saved-log reparsing
passes. No complete foreign operation trace or CPU-time attribution is claimed.

The predicates for independent/helper memory, retired processes, private-image
ownership, ready-thread selection, handoff, owner yield/wait/checkpoint and the
owned admission wait loop remain byte-identical to base `1bd3aea9`. Userspace,
shared ABI and pinned ports have no diff; the RAM image hash equals the accepted
`f5e9c4c9603cf2eb` run. No scheduling/eligibility, filesystem, capacity, flush,
readback, rename or500ms/10s/1800s limit changed. Ordinary timing remains off.

Exact stage `93d4474e781f4b6a`, source `e4f7f77c`, passes native-board refusal,
save/unmount and automatic Ubuntu return in Q35. Prepared native run
`f36742f3d2664f44` repeats the unchanged automatic1800-second1% PCM/verified
USB/app/three-screenshot profile and additionally requires all three complete,
retired raw-write records and consistent final gate state/owner companions.
Physical cost attribution and any optimization remain pending. Preflight
verifies this unit/USB, all692 existing files,2265 allocated clusters and clean
headroom/allocation; only the intended quiet-volume expiry is renewed.

Workspace cleanup removes16 old build-stage duplicates only after exact
independent payload verification, retaining their full restorable backups and
all evidence. For ESP headroom, the older unreferenced batching payload
`1c74bc8281678786` is archived after exact backup/no-GRUB-reference checks;
retain latest accepted stress `f5e9c4c9603cf2eb`, audible check, sampler, zero-load,
single-window and console recovery `c35475f62f154f1e` installed. No new physical
pass is claimed by this preparation record.


### Native screenshot admission holder result (2026-11-08)

PR429 merge `66277083`, frozen source `e4f7f77c`, release `93d4474e781f4b6a`,
run `f36742f3d2664f44`, session `session-00065fce486a0aee-00`: automatic
1800-second1% audio/USB/app/three-screenshot stress passes all original checks.
This accepts the new native observations, not a latency optimization.

All three retired screenshot workers complete65 raw writes/4196406 BMP bytes
each, eight capacity changes, and zero invalid, dropped, active or failed records.
Every final gate companion is present and its state partition agrees exactly:

| Worker PID | Gate wall | Owner overlap | Blocked | READY | Running |
| --- | --- | --- | --- | --- | --- |
| 11 | 8.869162s | 8.855521s | 8.370989s | 497.920ms | 0.253ms |
| 475 | 11.416367s | 11.398317s | 9.951976s | 1.464127s | 0.264ms |
| 935 | 10.125499s | 10.110449s | 8.054251s | 2.070984s | 0.264ms |

Owner overlap covers99.842–99.851% of gate wall time. Ownership and scheduler
state are independent overlapping classifications; READY time must not be added
to owner overlap. The worst gate per worker is1.695968/1.442932/1.670416s.
Each retains thread19/PID7, syscall87 (`SYS_SYNC`), as its longest overlapping
owner, accounting for99.819–99.948% of that particular worst wait. This directly
identifies the owner/syscall for the retained worst waits; the aggregate does
not identify every owner of all400 overlapping scopes. Complete foreign
operation internals and POSIX/progress scheduling outside raw calls remain
unmeasured. These are wall intervals, not CPU time.

Raw call sums are9.190594/11.725786/10.440288s. Handler work remains about21ms
per screenshot, reservation about20ms and copying about1ms. Slowest complete
screenshot is22.379800s, including7.030952s receive and11.841051s WRITE. Prior
measurement-only run was23.738913s overall; this variation is not a claimed
performance fix. Slowest retained cleanup is307.105ms, with no pending retirement.

The workload completes1080 app cycles and3884 verified bursts/1018167296 bytes,
with345862144 PCM bytes, zero recorded underruns and zero worker/lifecycle
errors. All53601 valid loop observations satisfy500ms, maximum119993us; maximum
launch/close is1324913/2522480us within unchanged10s. Worst-loop companion has
99614us blocked,20192us READY and zero exclusions. All three1366x768 BMPs
decode, follow-up Files and normal exit pass, and app/cache/capture/battery
owners retire before unmount. Listening is unassessed at1%; prior bounded25%
listening acceptance stays separate.

All692 prior files survive except expected `.srvros/meta`; nine additions yield
701 files/2372 allocated clusters. Offline fsck and independent allocation checks
pass with zero leaks/mismatches/errors. Independent review verifies session
hashes, raw-write/gate/state/loop/lifetime parsers, binary/text receipts, decoded
BMPs and605 automatic-backup file hashes. Reviewed evidence is backed up outside
`build`. Installed/staged/independent payloads, protected accepted images and
console recovery `c35475f62f154f1e` verify. Ubuntu stays default, one-shot is
consumed, collector disabled, no reboot armed and no source changes pending.

Next bounded scope is auditing the global synchronization path underlying these
long owned intervals, separating file synchronization from device flush and
service-window scheduling before choosing a serialization-preserving fix.
Retain required global barriers, full readbacks, flush/rename/failure protection,
same-process/private-image exclusion and all existing limits. Do not infer that
all gate waits are `SYS_SYNC`, skip a durability barrier, or widen admission
based on this result. Receive timing remains a separate follow-up. No new
implementation or physical test is started by this closeout.


### Global sync subphase audit scope (2026-11-08)

Base `d20312ac`, after collected PR430. Existing native evidence narrows the
retained third worst gate: owner `SYS_SYNC` scope1217551410–1219222267us
contains an exFAT append1217551592–1219222266us. That append records524288
payload bytes taking1480169us versus5519us in ten flush calls; prefix/metadata
reads account for additional time. This is one correlated operation, not an
attribution of every sync. Source audit confirms global sync flushes all live
dirty content objects, including another writer's partial file, before flushing
all writable parent devices. Existing append admission verifies the old prefix
from media; skipping this or excluding an open writer would weaken guarantees.

Bounded next checkpoint: observe file-sync and parent-device-flush phases of
`SYS_SYNC` separately, reusing existing BOT callback/event-dispatch/resume/service
sampling in each phase. Retain bounded per-PID aggregates and the slowest complete
call outside worker lifetime; explicitly expose conflicts, capacity, invalid
clocks and incomplete traces. Enable only with the existing explicit capture-
write diagnostic profile. Touch sync observation, syscall/main/logger hooks,
strict parsers and host/VM evidence; do not change process-file synchronization,
block/device flush, exFAT, USB/xHCI transfer logic or scheduler policy. PR19's
USB ring-fencing scope and PR301's scanout scope remain independent. No contact
or delegation is authorized. No new native trial is armed at publication.

Keep global barriers, full prefix/readback/rename/failure protection, buffer and
transport bounds,500ms loop,10s lifecycle and1800s endurance unchanged. A
serialized USB completion or service delay must be measured before choosing its
fix; merely omitting persistence work is not an optimization candidate.


The phase observer needs a separate fixed USB observation window: the existing
logger may retain its diagnostic scope across a quiescent scheduling gap while
a user's `SYS_SYNC` starts. Reusing that scope correctly refused overlaps but
made the new observation unavailable. Add one auxiliary observation slot that
never resets/ends the legacy slot. Capture each existing callback/controller
sample once and accumulate it independently into active slots; neither slot
creates a transfer or grants I/O ownership. Nested begins within a slot still
refuse. Overlap, independent end, foreign preservation, isolated overflow and
single-callback/single-sample regressions are required. BOT16KiB commands,
4KiB DMA segments, quarantine, flush and controller/scheduler behavior stay.

Keep periodic sync output to counters; full retained worst-call/USB detail is
final-only so the unchanged64KiB tail can preserve app receipts. Three earlier
VM runs with verbose dumps retained incomplete app proof; the compact TCG
repeat retains complete app records but fails its500ms loop bound at539323us
(516079us render) and also exposes the diagnostic-scope conflict. These attempts
remain failures. No native trial is armed, and no criterion or limit is relaxed.


### Global sync phase validation and native preparation (2026-11-08)

Final compiled source `fffeafa5` adds observations only. Full core/desktop at
baseline `2eb4c38d` passes, including89 installed-root tests in731.226s with one
expected skip. Subsequent changes are diagnostic emission, independent USB
observation and their fixtures/docs. Final matched normal/native/VM/RAM/USB
images, USB BOT/failure/retirement/observer tests, scheduler model, syscall barrier
matrix, sync/raw-write/gate parsers, logger/bootlog, unchanged HSW retention,
desktop and docs checks pass. Optional IDT variants compile. No full-core run on
the final tree is claimed; its unaffected baseline and final affected checks are
recorded separately. SYNC executes both original operations in order even when
file synchronization fails, preserving its exact result behavior.

The legacy USB emitter and USB command/transfer/read/write/flush/retirement/
attachment bodies remain byte-identical to `d20312ac`. Filesystem, file-content
synchronization, block layer, xHCI, scheduler, shared ABI and pinned ports have
no diff; the RAM image matches accepted release `93d4474e781f4b6a`. The new
auxiliary observation slot has independent start/end/validity/overflow state;
regressions prove one raw callback and one metadata sample with both slots active,
peer preservation and unchanged16KiB BOT/4KiB DMA bounds. No gate, retry,
quarantine, watchdog, power policy, global barrier or persistence check changed.

Ordinary sysabi/UEFI USB keyboard/fork/exec-vectors/yield VMs pass, as do ordinary
immutable screenshot/readback/Preview/repeat/exit and real USB RO/write/flush/
reload/keyboard integration. Populated-Home KVM stress at400 write IOPS passes
23 app cycles,181 overlap observations, zero audio/worker/lifecycle errors,
159253us maximum loop and900323/1199801us maximum launch/close, with complete
saved atomic app receipts. Independent saved-log parsing agrees on all raw
write/gate and new sync/USB records. All final sync observations have zero
conflicts, errors, invalidity or drops.

In that VM, PID7's131 global sync calls retain a slowest1.019904s call: file
phase1.019402s, device phase502us. File-phase USB callbacks total184606us,
including134102us to software event observation and11207us observation-to-resume;
device callbacks total421us. These are VM software wall spans, not physical
transfer latency, CPU time or attribution of the unmeasured residual. Native
subphase distribution remains pending. Earlier incomplete app-proof runs and
compact346's539323us TCG loop/conflicting USB trace remain failed and locally
retained; no UART fragments, limits or completion predicates were repaired.

Exact stage `13b77acfd1826907` passes native-board refusal/save/unmount/return in
Q35. Prepared native run `9015901705fc42cc` repeats original automatic1800-second
1% PCM/verified USB/app/three-screenshot stress, with all original500ms loop,
10s lifecycle, full prefix/readback/rename/flush/failure/retirement checks plus
complete final SYS_SYNC file/device and USB event/resume/service evidence.
Preflight verifies this MacBookAir6,1/own USB, all701 prior files,2372 allocated
clusters, clean fsck/allocation and headroom; only quiet-volume expiry is renewed.
Current accepted `93d4474e781f4b6a` and console recovery `c35475f62f154f1e` stay
installed. Older unreferenced `f5e9c4c9603cf2eb` is archived after exact
independent backup and no-GRUB-reference checks. Native acceptance and any
synchronization performance fix remain pending.


### Native global sync phase results (2026-11-09)

PR431 merge `b7d3927b`, compiled source `fffeafa5`, release
`13b77acfd1826907`, run `9015901705fc42cc`, session
`session-00065fd039c2251a-00` completes the original automated1800-second
1% audio/disk/app/three-screenshot workload. Independent saved-log parsing
agrees with automatic collection:1082 app cycles,3768 verified262144-byte
bursts (987758592 bytes),345780224 PCM bytes, zero recorded underruns and
worker/lifecycle errors.54378 valid loop observations have zero excursions
and128759us maximum; maximum app launch/close2056579/4030297us stay within
unchanged500ms loop/10s lifecycle limits. Three exact1366x768 BMPs, follow-up
Files, normal exit, battery/cache/app/capture retirement and automatic Ubuntu
return pass. All701 prior files retain their contents except expected filesystem
metadata;710 files/2479 allocated clusters have clean fsck/allocation, zero
leaks and zero FAT/bitmap mismatches. The independent710-file evidence manifest
and installed/recovery payload hashes verify. Subjective listening is unassessed
at1%; the earlier user-confirmed30-second25% check remains separate.

Final sync evidence has five retained PIDs, zero conflicts/errors/invalidity/
drops, no active trace, and complete independent USB observations. Workload
PID7 performs3768 global sync calls totaling16.363732s. Its slowest call takes
1.752273s: file synchronization1.751870s and parent-device flush403us. That
file interval contains the retained exFAT append31405705–33157326us:524288
payload bytes,1.460292s data writes,3,735,788 prefix/metadata read bytes in the
phase-zero bracket, and5563us across ten flush calls. This is a correlated
worst call, not a characterization of every sync or every gate wait.

For that slowest file phase, USB callback wall totals1.438713s: CBW2727us,
data291391us and CSW1144595us. Three CSW callbacks exceed100ms; the maximum
is370174us. Software event observation accounts for1.432967s across all
callbacks; observation-to-resume totals1639us. The maximum sampled service gap
is20053us. The remaining313157us outside those callback brackets is not yet
attributed. These are software wall intervals, not controller completion times,
pure device latency or CPU time. The dominant retained wait is before software
observation of command status; a scheduling or device cause is not established.
PID948 separately retains a674379us file phase with243206us USB callbacks,
showing that callback coverage is not a universal explanation of file wall time.

Screenshot write admission still dominates the raw calls: PID11/470/948 record
11.606/8.759/10.354s in the gate versus about21ms each in handlers. Their worst
waits still overlap thread19/PID7 syscall87. The first gate's longest owner
correlates with the slowest PID7 file phase above. Slowest screenshot total
22.589527s, receive5.953988s and WRITE11.950868s leave completion latency open;
this observation-only release is not an accepted performance optimization.

Next bounded investigation: correlate slow CSW observations with command type,
transfer size and software service progress, and attribute the file-phase wall
outside USB callbacks before choosing a serialization-preserving fix. Retain
full prefix validation, required global barriers, all live dirty writers,
readbacks/flush/rename/failure handling, same-process/private-image exclusions,
transport bounds, watchdogs and every existing limit. Receive latency remains
separate. No new implementation or boot is started by this result closeout.
Ubuntu remains default; the one-shot entry is empty, the collector disabled,
current release `13b77acfd1826907` and console recovery `c35475f62f154f1e`
verify. Raw logs/images and complete payload/evidence backups remain local,
with accepted backups outside build. Earlier failed VM runs remain failures;
final-source host/VM provenance is unchanged from the validation section above.


### Command-status and service-window audit scope (2026-11-09)

Base `928555f3`, after collected PR432. Bounded objective: retain the slowest
CSW callback's command opcode/size and exact software submission/observation/
resume/service sample in each explicit sync phase, plus wall time in existing
BOT scheduler checkpoints. This separates request context and yielded service
from the currently unassigned file wall interval without guessing hardware
completion. Touch USB diagnostic metadata, sync final emission/parser and focused
fixtures; leave xHCI, scheduler policy, file synchronization, filesystem and
block flush behavior intact. No command addresses or user payloads are logged.
Existing PR19 transport retirement and PR301 scanout work remain independent;
no contact/delegation. Preserve all transport bounds, integrity checks, watchdogs,
ownership gates and stress limits. Next checkpoint: strict host/VM validation,
reviewable merge, exact-stage preflight and automatic native repeat at1% volume.


### Command-status/checkpoint diagnostics validated and staged (2026-11-09)

Frozen source `dc7b09f7` adds auxiliary-only fixed metadata for the slowest CSW
callback in each retained sync phase: command opcode/size, exact request/actual
length and existing submission/observation/resume/service sample. Existing BOT
checkpoints are called exactly once at their original locations, with separate
wall/call aggregates. No addresses/payloads are logged. Final-only
`sync-usb-command` rows must match the CSW maximum, phase bounds and service
coverage; callback plus checkpoint wall must fit the enclosing phase. Missing,
conflicting, overflowing or incomplete evidence fails strict collection.

Full matched normal/native/VM/RAM/USB images, core, desktop and docs pass on
that source, including89 installed-root tests in720.355s with one expected skip.
Focused fixtures cover read/write/flush context, exact checkpoint call counts,
independent observer preservation, overflow and malformed/missing command rows.
Source comparison proves unchanged command control flow after removing only
diagnostic arguments/checkpoint wrappers; begin/end/sector/transfer/read/write/
flush/retirement/attachment bodies and the legacy USB formatter are identical.
xHCI, scheduler, process/file synchronization, filesystem, block flush and syscall
behavior have no diff. No transport bound, error/quarantine handling, watchdog,
ownership gate, integrity check or stress limit changes.

Ordinary sysabi, USB keyboard, fork/exec-vectors/yield VMs and real USB
read-only/write/flush/reload/input integration pass. Populated-Home KVM stress at
400 write IOPS passes22 app cycles,181 overlap observations, complete saved
atomic app receipts, zero audio/worker/lifecycle errors,140159us maximum loop
and1138647/1870975us maximum launch/close. Independent saved-checkpoint parsing
agrees on complete sync/CSW/checkpoint/raw-write/gate evidence. Ordinary immutable
screenshot/readback/Preview/repeat/exit passes. Three experimental IDT variants
compile only; no feature activation or physical acceptance follows.

VM PID7's130 global sync calls retain a765024us file phase with438522us in
existing BOT checkpoints; its slowest CSW is a512-byte WRITE(10),932us callback.
This demonstrates useful residual attribution in that VM, not the distribution
or cause of native latency. Exact stage `53bbc1245abcc03d` passes Q35's native
board refusal/save/unmount/automatic-return path. It does not accept native
hardware operation. Next trial repeats the original automated1800-second1%
PCM/disk/app/three-screenshot stress and every500ms loop/10s lifecycle/full
prefix/readback/barrier/rename/failure/retirement requirement, adding complete
command/checkpoint evidence. Native timing and any performance fix remain pending.
Retain accepted `13b77acfd1826907` and console recovery `c35475f62f154f1e`;
archive only a superseded unreferenced EFI duplicate after independent payload
verification to make installation room. No sibling or power-policy acceptance.


### Native command-status/checkpoint results (2026-11-09)

PR433 merge `e85a6846`, compiled source `dc7b09f7`, release
`53bbc1245abcc03d`, run `34893cf2c173469c`, session
`session-00065fd1484d6702-00` completes the original automated1800-second1%
PCM/disk/app/three-screenshot workload. Automatic collection and independent
saved-log parsers agree:1074 app cycles,3880 verified262144-byte bursts
(1017118720 bytes),345763840 PCM bytes, zero recorded underruns and worker/
lifecycle errors.53609 valid loop observations have zero invalidity/excursions,
129963us maximum loop and2144889/1999509us maximum launch/close, inside the
unchanged500ms/10s limits. Three exact1366x768 BMPs, follow-up Files, normal
exit, battery/cache/app/capture retirement and automatic Ubuntu return pass.
All710 prior files preserve contents except expected filesystem metadata;
719 files/2586 allocated clusters have clean fsck/allocation, zero leaks and
zero FAT/bitmap mismatches. Independent617-file evidence manifest and installed/
recovery payload hashes verify. Subjective audio remains unassessed at1%; the
prior user-confirmed30-second25% check is separate.

All five retained sync PIDs have complete command/checkpoint/USB records with
zero errors/conflicts/invalidity/drops and no active trace. Workload PID7 makes
3880 global sync calls totaling17.908305s. Its slowest call takes2.332299s:
file2.331895s, parent-device flush404us. File-phase wall partitions into
2.004412s USB callbacks,236574us across671 existing BOT checkpoints, and90909us
outside both brackets. CSW callbacks account for1.809615s; five exceed100ms.
The slowest retained CSW is a16384-byte WRITE(10),350728us callback with exact
13-byte status return. Submission-to-software-observation350529us versus198us
observation-to-resume,43045 service checks/169 samples and10063us maximum
sampled gap show continued service during this particular long wait. This does
not establish hardware completion time, a media fault or the causes of all slow
CSWs. The correlated append622810698–625142332us writes524288 payload bytes
in1.797184s; its ten flush calls total5566us. Prefix/metadata and other work
remain required. The command/checkpoint result accepts observations, not a fix.

Raw screenshot worker PID11/473/942 gate totals are9.368/16.285/8.092s versus
about21ms handler totals. The first two retained worst waits overlap PID7
SYS_SYNC; the third overlaps PID7 SYS_FSYNC (53), so global sync alone is not
the full gate-delay explanation. Slowest screenshot total27.058020s, receive
6.955717s and WRITE16.709473s leave latency open; variation against the prior
22.589527s observation is not evidence of an improvement. Cleanup completes.

Next useful bounded scope: audit write amplification and batching opportunities
in the file-sync/append path, correlating command type/size with media work before
choosing a fix. Preserve full old-prefix validation, all live dirty writers,
required global barriers, full readbacks/flush/rename/failure protection,
same-process/private-image exclusions and every transport/policy/watchdog limit.
Do not attribute a350ms wait to delayed software resume or claim that all gates
are global sync. Receive latency stays separate. No new implementation or boot
is started by this closeout. Current `53bbc1245abcc03d`, prior accepted
`13b77acfd1826907` and console recovery `c35475f62f154f1e` verify; superseded
`93d4474e781f4b6a` has a verified independent full payload backup after removing
only its unreferenced EFI duplicate for installation space. Ubuntu stays default,
one-shot entry empty, collector disabled; accepted payload/evidence backups
remain outside build. Earlier failed attempts retain their original status.


### Unpublished contiguous FAT-link batching scope (2026-11-09)

Base `f30f67e1`, after collected PR434. Audit finds append already writes only
the new payload and proves the full old prefix, but builds consecutive FAT links
using separate four-byte writes. On sector-backed storage these are separate
read/modify/write commands to the same sector. The existing constant-value FAT
fill already batches this class of work; sequential next-cluster links do not.
Bounded objective: batch only consecutive unpublished link entries in the append
path, within512 bytes and sector boundaries, with exact next-cluster values.
New allocations are unreachable and an old NoFatChain stream ignores these links
until final publication. Leave the final old-tail attachment separate and retain
all original data/link/publication flush barriers. Fragmented chains retain the
existing traversal. No buffer/transport/policy/watchdog or stress limit changes.

Touch exFAT append link preparation and focused/independent comparative/fault
fixtures, plus this unit's evidence. Preserve old-prefix validation, full payload
writes/readbacks, all live dirty content/global sync, copy-on-write fallback,
rename/failure/quarantine/ownership handling and unrelated metadata bytes.
Inspect actual open PRs for conflict; no messaging/delegation. Next checkpoint:
prove fewer block-write calls with identical final bytes and barrier ordering,
run fault cuts and full host/VM validation, merge, then exact-stage/native stress.
Native latency benefit remains unproved; reject rather than broaden this scope.


### Unpublished FAT-link batching validated and staged (2026-11-09)

Frozen source `c831012f` batches only the two append-preparation loops for
consecutive unpublished FAT links. Scratch writes stay within512-byte boundaries;
next-cluster values, final tail attachment and all data/link/publication flushes
remain unchanged. Full old-prefix proof, payload writes, fragmented traversal,
alias/alignment/COW fallback and failure/quarantine handling are untouched.
Source comparison proves every other byte of exFAT unchanged; process/global
sync, block, USB/xHCI, scheduler, syscall, logger and streaming paths have no diff.
PR19's metadata-restore changes are outside these append/link sections; PR301's
scanout scope remains independent. No contact/delegation or sibling acceptance.

Independent actual-driver comparisons on4KiB and128KiB clusters produce identical
complete media images and ordered logical FAT writes/flushes:22 block writes
become20, with ten flushes unchanged. Actual-writer fixtures cover exact values,
512-byte boundaries,4KiB-sector neighbours, range/address overflow refusal and
partial-write quarantine/no retry. Existing append tests pass full old-prefix,
changed/alignment/alias fallbacks and all reachable write/flush/tear cuts for
contiguous and fragmented old extents. Existing torn-publication limitations
remain documented; neither new atomicity nor native latency gains are claimed.

Full matched normal/native/VM/RAM/USB images, core, desktop and docs pass on
`c831012f`, including89 installed-root tests in730.011s with one expected skip.
Ordinary sysabi/USB keyboard/fork/exec-vectors/yield VMs and real USB read-only/
write/flush/reload/input integration pass. Populated-Home KVM400-write-IOPS stress
passes22 app cycles,175 overlap observations, zero audio/worker/lifecycle errors,
239126us maximum loop and1518919/1397290us maximum launch/close. Complete
unmodified console records with retained atomic checkpoints supply app timing;
independent reparse agrees, without reconstructing UART fragments. Saved sync/
CSW/checkpoint/raw-write/gate/loop parsing also agrees. Ordinary immutable
screenshot/readback/Preview/repeat/exit passes. Optional IDT variants compile
only; no experimental feature is activated.

Exact stage `8caa2866d8888e8c` passes native-board refusal/save/unmount/automatic
return in Q35. It does not accept native filesystem or hardware behavior. Next
is the unchanged automatic1800-second1% PCM/verified disk/app/three-screenshot
trial, retaining500ms loop/10s lifecycle, every full-prefix/readback/barrier/
rename/failure/ownership/retirement requirement and complete diagnostic records.
Native integrity and latency acceptance remain pending. Keep accepted
`53bbc1245abcc03d` and console recovery `c35475f62f154f1e`; after full independent
backup/no-reference verification, only superseded EFI duplicate `13b77acfd1826907`
may be archived to make installation room. Own USB, offline allocation/headroom,
all719 prior file hashes and durable run/payload/evidence records must verify
before one-shot reboot. All prior failed attempts retain their original status.


### Native FAT-link batching regression accepted (2026-11-09)

PR435 merge `bdc14461`, compiled source `c831012f`, release
`8caa2866d8888e8c`, run `831a258cecf34f5a`, session
`session-00065fd860fdb30a-00` completes the unchanged automated1800-second1%
PCM/disk/app/three-screenshot trial. Automatic collection and independent raw
saved-log parsers agree:1093 app cycles,3780 verified262144-byte bursts
(990904320 bytes),345780224 PCM bytes, zero recorded underruns and worker/
lifecycle errors.53447 valid loop observations have zero invalidity/excursions,
129431us maximum loop and1955399/2307934us maximum launch/close, within the
unchanged500ms/10s limits. Three exact1366x768 BMPs, follow-up Files, normal
exit, battery/cache/app/capture retirement and automatic Ubuntu return pass.
All719 prior files retain contents except expected filesystem metadata;
728 files/2693 clusters have clean fsck/allocation with zero leaks/mismatches.
Independent635-file durable evidence hashes and installed/recovery payloads
verify. Subjective listening is unassessed at1%; prior25%30-second speaker
acceptance remains separate.

Complete retained sync/CSW/checkpoint/raw-write/gate observations pass with no
active/invalid/conflicting/dropped/error records. Workload PID7 makes3780 global
sync calls totaling14.536845s. Its slowest call takes1.831720s: file1.831313s,
parent-device flush407us. File wall contains1.601202s USB callbacks,175414us
existing checkpoints and54697us unassigned residual. The slowest retained CSW
is a16384-byte WRITE(10),347970us callback; submission-to-software-observation
347760us, observation-to-resume207us,41252 service checks/162 samples and14670us
maximum sampled gap. This is software wall observation, not hardware completion
or a media-fault diagnosis. Its correlated append1216305924–1218136981us writes
524288 payload bytes with eight metadata write calls/432 bytes and ten flushes;
these are observations of one operation, not a controlled cross-run comparison.
The controlled host22→20 call reduction with identical media/barriers remains
the command-reduction proof. Required data/prefix/flush work was not skipped.

Raw screenshot worker PID11/484/964 gate totals14.942/7.842/8.577s remain much
larger than roughly21ms handler totals. Their longest retained owners are PID7
FSYNC, SYNC and SYNC respectively. Slowest screenshot25.810888s, receive
6.450034s and WRITE15.320174s compare with prior27.058020/6.955717/16.709473s.
One variable endurance run does not establish a stable latency improvement;
screenshot completion remains open. This accepts native functional/integrity
regression for the bounded FAT-link batching, not the end of latency work.

Next bounded audit: remaining data/metadata write amplification in file-sync and
append/sidecar persistence, retaining every prefix proof, required global/live-
writer synchronization, zero-initialization/publication guarantee, full readback,
flush/rename/failure/quarantine/ownership rule and existing transport/policy/
watchdog/stress limit. No new implementation or boot starts in this closeout.
Ubuntu stays default, one-shot entry is empty and collector disabled. Current
`8caa2866d8888e8c`, accepted `53bbc1245abcc03d` and console recovery
`c35475f62f154f1e` verify; superseded `13b77acfd1826907` remains independently
backed up after removing only its unreferenced EFI duplicate for space. Required
payload/evidence backups are outside build. Historical failed attempts retain
failure status and no other model's acceptance changes.


### Exact-media sidecar equality scope (2026-11-09)

Base `238c212a`, after collected PR436. Audit finds every metadata sync builds
and replaces the entire sidecar, even if its complete serialized bytes already
match media. Zero-filled allocation tails remain required and untouched.
Bounded objective: after full serialization, compare an eligible existing
sidecar with bounded direct-media reads, under the existing mutation owner.
Only a complete length/valid-length/byte match, with no pending temporary file
or live sidecar client, may retain the current extent. Flush before returning;
retain outer data/clean-marker and unconditional global device barriers. A
mismatch retains original replacement; failed/incomplete evidence refuses
without treating a failed read as a mismatch. No persistent equality cache.

Touch sidecar comparison and a shared direct-read helper preserving existing
read accounting, focused/comparative/fault fixtures and current docs. Preserve
all caller/global live-writer sync, prefix proof, initialized padding, changed-
metadata temp/replace publication, pending-temp handling, fallback, quarantine,
ownership, buffer/transport/policy/watchdog and stress limits. PR19's metadata
restore/parser work must be inspected for overlap; no messaging/delegation.
Next checkpoint: prove no-op writes are avoided only for exact media equality,
changed metadata still persists/remounts, read/fault/pending-temp refusal and
full host/VM regression before merge and unchanged automated native trial.
Native gain is pending; reject this path if equality cannot be proved safely.


### Exact-media sidecar equality validated and staged (2026-11-09)

Frozen source `62dd5ea1` retains an existing sidecar extent only after current
serialization fully matches uncached media bytes, with exact size/ValidDataLength,
bounded16KiB scratch and FAT termination for linked extents. Pending temp names
and live sidecar clients retain original replacement checks. A mismatch retains
replacement; failed/unavailable proof refuses. Verified equality flushes owner
writes before unchanged outer clean-marker and unconditional global barriers.
Only the unnecessary new-temp/data/replace transaction is omitted; changed
metadata retains the exact original writes/flushes. Allocation zero-padding,
full user-prefix/readbacks, all dirty writers, quarantine/ownership, transport,
power/watchdog and stress limits remain unchanged. No persistent equality cache.
PR19's metadata-restore/parser sections are separate; no messaging/delegation.

Actual-driver fixtures for4KiB/128KiB clusters show identical final media for
no-op metadata:10 writes/7 flushes become two clean-marker writes/four flushes.
For128KiB clusters, requested writes drop131374→4 bytes in that fixture. Changed
metadata has identical complete media and ordered writes/flushes to the baseline,
and permissions persist on remount. Pending-temp/live-client fallback, complete
uncached comparison (including stale-cache and last-byte cases), read/allocation
refusal, fragmented/invalid FAT traversal and all14 equal-path write/flush/cut/
tear boundaries per cluster size pass. Existing append interruption tests pass.
This is controlled host evidence, not a native timing improvement claim.

Initial source `d2063ae1` failed the strict cost-formatter checkpoint-budget model;
that failed build/log remains preserved. Final source restores the legacy cost
formatter byte-for-byte and prints checked sidecar counters only through the
existing explicit sync diagnostic profile. Disabled-profile/periodic/final logger
fixtures and original retention models pass without changing capacity or limits.
Full matched normal/native/VM/RAM/USB builds, core, desktop and docs pass on
`62dd5ea1`, including89 installed-root tests in734.278s with one expected skip.

Ordinary sysabi/USB keyboard/fork/exec-vectors/yield and real USB read-only/write/
flush/reload/input integration pass. Populated-Home KVM400-write-IOPS stress
passes23 app cycles,183 overlap observations, zero audio/worker/lifecycle errors,
142449us maximum loop and998662/1636125us maximum launch/close. Complete saved
atomic app records and independent reparsing agree; saved sync/CSW/checkpoint/
raw-write/gate/loop and sidecar counters agree. Final VM counters:611 checks,
132 equal,479 changed,615113 retained serialized bytes and zero refused proofs,
flush failures, active traces or invalidity. Retained bytes are not physical
write savings. Equality grows by87 between complete samples inside live PCM.
Ordinary immutable screenshot/readback/Preview/repeat/exit passes. Optional IDT
variants compile only. Exact stage `c1578543a5d0e7e4` passes Q35 native-board
refusal/save/unmount/automatic return; native behavior is not yet accepted.

Next trial repeats original1800-second1% PCM/verified USB/app/three-screenshot
stress, every500ms loop/10s lifecycle/full-prefix/readback/ownership/retirement
check, all prior timing evidence and complete equality counters. It additionally
requires equality growth between two complete samples during live playback,
plus a complete post-playback snapshot, with no proof/flush/counter failures.
Native latency benefit remains pending. Keep accepted `8caa2866d8888e8c` and
console recovery `c35475f62f154f1e`; archive only superseded unreferenced EFI
copy `53bbc1245abcc03d` after independent payload verification. Own USB/all728
prior file hashes/offline allocation/headroom and durable run/payload/evidence
records must verify before one-shot reboot. Four old stage duplicates were
removed only after complete independent payload/file hash verification, freeing
about580MiB; accepted backups, raw evidence and active payloads are preserved.
No sibling acceptance or historical failure status changes.

### Sidecar equality native trial halted (2026-11-09)

Release `c1578543a5d0e7e4`, compiled source `62dd5ea1`, run
`525a5325e1434c1a`, session `00065fd9dc146258-00` is **FAILED**, not
physically accepted. Saved logs end at196.537365s with active PCM and no
recorded underruns, but no workload completion, exit or clean retirement.
The supplied terminal photograph reports `HALTED query transport`,1599 queries,
zero runtime errors/denials, worker15/completed825/in-flight1, `ec_phase=6`
(`EC_DONE`) and `ec_error=0` (`EC_OK`). This is not evidence of a transport
timeout: transfer can refuse before transport polling. Admission, worker context,
lease ownership and any interleaving remain to be diagnosed. No causal link to
the sidecar change is established, and no deadline or ownership gate is relaxed.

Partial sidecar counters at186.447545s show1661 checks,337 equal,1324 changed,
zero refused/flush failures/active/invalid, and32410215 retained serialized bytes.
These establish exercise only; missing full stress/retirement proof prevents
native acceptance or a latency claim. One screenshot survives; the required
three-screenshot/1800-second test did not complete.

All728 prior files remain present with contents unchanged except expected
`.srvros/meta`;732 files remain after the interrupted run. `fsck.exfat -n`
reports clean, but the independent allocation checker fails with one orphaned
cluster and zero FAT/bitmap mismatches. Keep this discrepancy as failed evidence;
no repair or new boot was attempted. Media stays unmounted. The photo, raw logs,
file hashes, failed checks and independent review are retained outside build in
a hash-verified643-file trial backup. Ubuntu remains default, one-shot entry is
empty, collector disabled and no reboot armed. Installed candidate, accepted
`8caa2866d8888e8c` and console recovery `c35475f62f154f1e` payloads verify.
Next bounded work: diagnose transfer refusal/ownership and preserve orphan
allocation evidence before any repair or new trial, retaining existing integrity,
watchdog, transport and stress limits. Historical host/VM passes remain separate.

### Query transfer refusal audit scope (2026-11-09)

Base `d057f7d8`, after collecting the failed sidecar trial. Bounded objective:
preserve read-only allocation/allocated-media evidence, identify the orphan,
and audit native query transfer admission, worker context, EC lease and completion
state. If current evidence cannot distinguish refusal from poll failure, add
fixed-storage failure-only diagnostics with real admission/transfer regression
cases before another native trial. Touch the battery runtime adaptation and its
focused tests only as needed; no speculative ownership bypass, retry, deadline
extension, interrupt enabling or transport reset. Preserve watchdogs, isolation,
query/EC budgets, all integrity and stress limits. Inspect current PR19/301
scope before shared edits; no other machine acceptance or delegation follows.
Next checkpoint: a reproduced cause/fix or explicit diagnostic candidate, matched
host/VM checks, preserved/reconciled offline allocation and exact-stage recovery
proof before any automatic native test. Current failed USB stays unmounted and
unmodified until its allocation evidence is independently backed up.

### Query transfer diagnostic validated and staged (2026-11-09)

Frozen source `10cb60fb`, ports `d40e118` (SrvrPorts PR13), retains the original
IF/worker/lease/poison admission and EC100ms transfer budget. It adds one fixed
RAM failure observation, cleared per lifetime and printed only during existing
terminal containment. Original entry flags/identities and lease/poison distinguish
admission refusal from begin/poll failure; EC phase/error/polls/status and clocks
are retained. No retry, IRQ enabling, controller reset, allocation, filesystem
save, permission grant or policy/limit change follows. This is a diagnostic
candidate: the native cause is unresolved and no fix is claimed. A successful
repeat alone must not close an intermittent failure. See the
[failure evidence contract](../battery-service.md#failure-only-transfer-admission-evidence).

The real wrapper and EC implementation pass host cases for every admission
refusal, prior DONE/OK, invalid/poisoned begin, unexpected output and the unchanged
100ms timeout, including no EC I/O on refused admission. Production images omit
host IRQ substitution; only the explicit battery image contains the report.
Transport, scheduler, filesystem, worker queue, hardware profiles, ownership,
watchdogs and stress/staging policies are unchanged. Actual current PR19/301
scopes do not touch this battery adaptation; no other machine acceptance follows.

The interrupted USB was preserved before recovery: full prefix/FAT and all2732
allocated clusters, including orphan2700, are independently archived and every
range hash verified outside build. There were2731 referenced clusters and zero
unallocated references. On a disposable reconstruction, `fsck.exfat -y -s`
retained the full128KiB orphan in LOST+FOUND and preserved every one of732
existing file hashes. Physical rescue matched that exact733-file result. The
strict allocation checker now passes2733 clusters with zero leaks/mismatches/
errors, and original32-file/eight-directory plus metadata headroom passes.
Historical native failure remains failed. Only the disposable reconstruction
was removed; the verified allocated-media archive and original evidence remain.

Full matched normal/native/VM/RAM/USB builds, core, desktop and docs pass,
including89 install/recovery tests in771.679s with one expected skip. Ordinary
syscall, USB keyboard, fork/exec-vectors/yield and actual USB read-only/write/
flush/reload/input integration pass. Populated-Home KVM400-write-IOPS stress
passes21 app cycles,179 overlap observations,146245us worst loop and
1282089/1796066us maximum launch/close, with zero audio/worker/lifecycle errors
and clean retirement. Independent saved app/sync/write/gate/loop proof agrees;
atomic saved app coverage is complete. Sidecar counters are complete with611
checks/132 equal/479 changed, zero refused/flush/active/invalid, and equality
increases86 during live PCM. These are synthetic regression observations.
Ordinary immutable screenshot/readback/Preview/repeat/exit passes. Exact stage
`318c5032d1519383` passes Q35 native-board refusal/save/unmount/return.

Next native trial repeats the original1800-second1% PCM/verified USB/app/three-
screenshot workload with every500ms loop/10s lifecycle, full readback, timing,
equality-growth, ownership and retirement requirement unchanged. No input is
required; keep AC and USB attached. Mounted-Home containment still halts safely
with live state retained: photograph the full terminal and restart Ubuntu if
encountered. Collect the matching run first after return. Preserve accepted
`8caa2866d8888e8c` and console recovery `c35475f62f154f1e`; obsolete zero-load
qualification payload `517661c4c5a9627f` may be removed from EFI only after its
full independent backup and absence of boot references verify. Its evidence
and accepted history remain outside build. Ready/armed state belongs to the
local run record, not this staged-source entry. Native cause/fix and sidecar
acceptance remain pending.

### Query diagnostic / sidecar native regression completed (2026-11-09)

Release `318c5032d1519383`, frozen SrvrOS `10cb60fb` / ports `d40e118`,
run `a8e0df6a178545d6`, session `00065fda7a63e0c5-00` completed the full
1800-second1% workload and returned automatically. It passes1085 app open/close
cycles,4037 verified262144-byte disk bursts (1058275328 bytes),345747456 PCM
bytes, zero recorded underruns/worker/lifecycle errors and all three exact
1366×768 BMPs. The52052 valid loop observations have zero excursions/invalidity
and140092us worst loop under the unchanged500ms limit; maximum launch/close
are1871323/2081206us under the unchanged10s limits. Follow-up Files, screenshot
cleanup, normal exit and full cache/worker/battery retirement pass. Battery
retirement records705 attempts, zero rejections,10666 queries,110 notifications,
matched attach/detach and zero heap/errors/denials. Independent parsing of saved
loop/app/sync/write/gate/sidecar/window/receipt/BMP and retirement proof agrees.

The previous query-transport halt did not recur. No transfer-failure diagnostic
was emitted; those rows are terminal failure-only output, not a continuous
transfer-failure counter. **The intermittent cause remains unresolved; this
successful repeat is not a battery fix.** The failed `c1578543a5d0e7e4` trial
and its preserved media/photo remain failed history. Subjective audio is
unassessed at1%; prior short25% listening acceptance remains separate.

This accepts native bounded sidecar-equality exercise and functional/integrity
regression on this source/release:16371 checks,3235 equal,13136 changed and zero
refused/flush/active/invalid counters. Twenty new equalities are witnessed
between two complete snapshots inside live PCM, with a complete final snapshot
after playback. The313969245 retained serialized bytes are logical proof bytes,
not measured physical write savings. All733 prior files, including the full
recovered orphan, retain contents except expected sidecar metadata changes.
Final742 files /2840 clusters pass offline fsck and the strict zero-leak/FAT-
bitmap/error checker. No integrity, ownership, watchdog or stress limit changed.

Slowest screenshot is23.539827s (receive6.561760s / WRITE13.073907s), with
57.379ms worst cleanup. Worst retained PID7 global sync is2.347472s, of which
file-sync is2.347069s and parent-device flush403us; the slowest file-phase CSW
is a16KiB WRITE10 callback taking354896us. Software observation covers354894us
before dispatch and1us after it, not hardware completion time. Screenshot/
write/sync latency remains open; one variable run establishes no stable gain.

Ubuntu/default recovery, installed candidate and retained accepted/recovery
payloads verify; one-shot entry is empty and collector disabled. The obsolete
zero-load qualification EFI duplicate was removed only after full independent
backup and no-reference checks; its evidence remains outside build. Current
trial/payload backups and original failed-run allocated-media evidence survive
`make distclean`. No reboot is armed. Next bounded step: repeat the same
instrumented native workload to seek recurrence before treating the intermittent
battery issue as resolved; preserve existing failure containment and limits.

### Identical-payload query diagnostic repeat completed (2026-11-09)

Run `178eae2fce944cc1`, session `00065fdbaa4182e0-00`, repeated the exact
installed `318c5032d1519383` payload (compiled `10cb60fb`, ports `d40e118`).
A fresh exact-stage VM refusal/save/unmount/return passed before the repeat;
full host/VM and first native validation were reused only after staged,
installed and independent payload hashes verified. Checkout `f30ad7a5` differs
from compiled source only in documentation. No source, workload or policy changed.

The complete1800-second1% run passes1081 app cycles,4309 verified disk bursts
(1129578496 bytes),345731072 PCM bytes, all three exact1366×768 BMPs and zero
recorded underruns/worker/lifecycle errors. All52415 loop observations are valid,
with zero excursions and121495us maximum under500ms. Launch/close maxima
1675310/2398606us remain under10s. Follow-up Files, screenshot/cache/worker
cleanup, normal exit, battery retirement and automatic Ubuntu return pass.
Battery retirement has705 attempts, zero rejections,10599 queries,28 notifications,
matched attach/detach and zero heap/errors/denials. Independent saved evidence,
receipt/BMP checks and offline filesystem/allocation proof agree.

All742 prior files remain present with contents unchanged except expected
sidecar metadata; final751 files/2947 clusters have zero leaks, FAT/bitmap
mismatches or errors. Sidecar counters complete17459 checks/3601 equal/13858
changed, zero refused/flush/active/invalid, with23 new equalities witnessed inside
live PCM and a complete final snapshot. The353801693 retained serialized bytes
are logical proof bytes, not physical write savings. Slowest screenshot is
26.788837s (receive6.351189s / WRITE14.890256s), with54.199ms worst cleanup;
latency remains open and no stable improvement is inferred.

This and the previous run are two separate cold-boot30-minute passes, totaling
2166 app cycles/8346 verified disk bursts/six screenshots, not one continuous
hour of acceptance. The earlier query-transport halt did not recur; no failure-
only diagnostic was emitted. **The intermittent cause remains unresolved.**
No fix, continuous transfer-failure count or subjective1% sound claim follows;
the original failed trial/media/photo remain protected. Keep failure containment,
watchdogs and all limits unchanged. A useful next diagnostic audit is to retain
the first upper-level SBS/SMBus failure context as well as the later transfer
refusal; EC DONE/OK can coexist with a poisoned upper-level transaction.

Current/accepted/recovery payloads verify, Ubuntu remains default, one-shot is
empty and collector disabled. USB is unmounted, no reboot is armed, and the
separate repeat evidence backup survives `make distclean`. This closeout makes
no further implementation or boot change.

### First upper-protocol failure diagnostic scope (2026-11-09)

Base `e22adf1f`, after collecting both complete diagnostic runs. Bounded
objective: preserve the first SBS/SMBus refusal/failure before a later EC query
refusal can hide its origin. Add fixed RAM reason/time/phase/status/poll-gap
observations at existing driver failure boundaries, then latch the first runtime
operation with worker/IF/I/O context before local state or later attempts replace
it. Emit a bounded terminal/retirement record for persistent collection. Preserve
exact EC100ms, read-word2s, event1s, sample20s/poll budgets, byte ordering, failure/
poison/cancel behavior, last-good sample, ownership, watchdogs and stress limits.
Poll gaps are wall observations, never proof of scheduler or hardware latency.

Touch internal EC-SMBus/SBS observation fields and failure-only helpers, battery
runtime reporting and real-driver/first-latch/strict-record regressions as needed.
No retry, IRQ enabling, extra EC/SMBus transaction, reset, ownership bypass or
speculative fix. Existing transfer diagnostics remain separate. Current PR19/301
must be checked for overlap before shared edits; no cross-task messaging or
other-machine acceptance. Next checkpoint: prove original outcomes/transactions/
limits unchanged, first evidence survives a later refusal/clean teardown, full
matched host/VM/exact-stage checks, then the original automated native workload.
The battery cause remains unresolved and historical failures remain failed.

### First upper-protocol observation validated and staged (2026-11-09)

Frozen SrvrOS `b0169186` / ports `39ad9d7` (SrvrPorts PR14) adds internal
EC-SMBus/SBS failure snapshots and the first runtime operation latch. Existing
failure actions remain unchanged: original EC100ms/read-word2s/event1s/sample20s
and poll caps, byte order, last-good sample, poison/cancel, bank ownership and
watchdogs are preserved. Poll gaps use the existing supplied clocks and are
wall observations, not scheduling or hardware completion attribution. There is
no retry, extra EC/SMBus transaction, IRQ enabling, reset or permission grant.
The newly initialized runtime clears the latch; later operations and local
state teardown cannot replace its copy. Source ownership remains held through
retirement reporting. See the [evidence contract](../battery-service.md#first-battery-protocol-failure-evidence).

The real-driver/wrapper regression reproduces an upper2s deadline poisoning an
EC still at DONE/OK, then a later query transfer refusal. First evidence survives
that refusal and destruction of local runtime state. Added cases cover each bus
reason, exact boundary clocks/gaps, pre-cancel EC state, acknowledgement failure,
unknown cancellation time, no-I/O refusal and unchanged last-good data. Strict
record tests consume actual C output and reject missing/malformed/conflicting/
unbound proof. An empty-latch epoch-bound retirement marker after PCM is newly
required by the explicit stress collector; any observed operation failure fails
diagnostic acceptance without changing the normal runtime's original outcome.
The first failure is not a complete causal trace and may be unrelated to a later
fault. Status zero alone is not proof of a successful read.

Full matched normal/native/VM/RAM/USB builds, core, desktop and docs pass on
`b0169186`, including89 install/recovery tests in744.925s with one expected skip.
An independent comparison runs the original legacy fixtures against old/new
state machines using matched extended internal headers. Original assertions and
ordered controller-callback/legacy poll/begin/cancel state fingerprints match
(1218854 bus and519025 sample trace words). This is logical behavior regression,
not binary-layout or latency equivalence. Existing256-status/address event,
SBS and real-wrapper cases pass. Six report rows fit the unchanged1024-byte
atomic bootlog record bound at a conservative385 bytes maximum. Legacy log
capacity, budgets and UART/whole-record collection rules are unchanged.

Ordinary syscall, USB keyboard, fork/exec-vectors/yield and real USB read-only/
write/flush/reload/input integration pass. Populated-Home KVM400-write-IOPS
stress passes22 app cycles,182 overlap observations,139986us maximum loop and
1238680/1607735us maximum launch/close, zero audio/worker/lifecycle errors and
clean retirement. Independent saved app/sync/write/gate/loop/equality proof
agrees, with complete atomic app coverage. The new clean epoch1 protocol marker
at65.222141s follows PCM end63.285093s. Ordinary immutable screenshot/readback/
Preview/repeat/exit passes. Exact stage `69a87466479ab140` passes Q35 native-board
refusal/save/unmount/automatic return; native observation remains pending.

Next trial repeats the original1800-second1% audio/disk/app/three-screenshot
workload with every500ms loop/10s lifecycle/readback/equality/timing/retirement/
allocation requirement unchanged, plus the new bound post-playback protocol
proof. No native cause or fix is claimed. Previous318c two-run acceptance and
c157 failure/photo/allocated-media evidence remain intact. Keep known-working
`318c5032d1519383`, accepted `8caa2866d8888e8c` and console recovery
`c35475f62f154f1e`; remove only the unreferenced failed c157 EFI duplicate after
its full independent backup and no-boot-reference checks. Four old stage-only
duplicates were removed after every file hash matched independent payload
backups, freeing607374682 bytes; installed/current/latest/recovery/raw evidence
were preserved. A fresh751-file baseline, offline allocation/headroom, recovery,
full payload/evidence backup and saved run state must verify before reboot.
Keep AC/USB attached; no input is needed. Existing mounted-Home containment
still halts with live state retained: photograph the complete terminal and
restart Ubuntu if it occurs. Collect the matching run first after return.

### First upper-protocol diagnostic native return (2026-11-09)

Run `f76703e09f064f11`, release `69a87466479ab140`, compiled SrvrOS
`b0169186` / ports `39ad9d7` (root PR442 / SrvrPorts PR14), returned to Ubuntu
normally. Matching session `session-00065fdce144f9b8-00` completes the original
1800-second1% automated PCM/disk/app/three-screenshot workload. Independent
saved-record parsing agrees with collection:1085 app cycles,4214 verified
262144-byte disk bursts (1104674816 bytes),345763840 PCM bytes and zero recorded
audio underruns, worker errors or lifecycle failures. All52464 loop samples are
valid, with zero excursions and121357us maximum under the unchanged500ms bound.
Maximum launch2066842us and close5633912us remain below the original10s limit;
this does not establish interactive latency improvement.

Three exact1366x768 BMPs decode, follow-up Files and normal desktop exit pass,
and worker/cache/battery ownership retires cleanly. The battery completes705
attempts, zero rejections,10575 queries and one notification, with one attach/
detach and zero remaining heap/errors/denied operations. The new version1 epoch1
protocol marker is directly present in the matching final log at1811607697us,
after PCM end1808450516us: retired, captured0, kind0. This physically accepts
clean-marker persistence/binding in this run; no real failure was captured, so
native first-failure content and the intermittent halt's cause remain unproved.
Host injected-failure regressions remain separate. No retry or fix is inferred.

All751 prior files retain their hashes except expected `.srvros/meta` changes;
760 files/3054 allocated clusters pass offline fsck and strict allocation with
zero leaks, FAT/bitmap mismatches or errors. Equality diagnostics record17080
checks,3506 equal/13574 changed, zero errors and348611537 logical serialized
bytes retained;23 equal-count growth during PCM and the post-playback final
record are independently bound. These are logical observations, not physical
I/O savings or stable performance acceptance. Slowest screenshot23.490265s,
pixel receive5.853363s and WRITE14.094040s leave completion latency open.

Original limits, integrity checks and power/ownership policy remain unchanged.
Sound at1% is unassessed; the separate user-confirmed short25% audible test
remains accepted. The original c157 failed trial, photograph and interrupted
media archive remain protected. This is a third separate30-minute clean run
in the diagnostic sequence, with a new instrumented payload, not a continuous
90-minute test or a battery fix. Next useful work is a bounded repeat with the
first-failure latch retained, rather than speculative transport changes.

Current/recovery payloads and independent backups verify. Ubuntu remains default,
the one-shot entry is empty, collector disabled and USB unmounted; no reboot is
armed. Raw matching evidence and an independent trial backup survive distclean.

### Identical first-protocol diagnostic repeat (2026-11-09)

Run `a9e06a47ddb44284` repeats installed release `69a87466479ab140` compiled
`b0169186` / ports `39ad9d7`, with no code or workload change. Fresh exact-stage
Q35 refusal/save/unmount/automatic-return rehearsal passes; installed/staged/
independent payload hashes match. Full matched host/VM validation is reused only
for that identical payload. Only the local1% quiet-volume expiry was renewed.

Matching native session `session-00065fddf41c5fe5-00` completes1800 seconds,
1078 app cycles,4211 verified262144-byte disk bursts (1103888384 bytes),
345763840 PCM bytes and three exact decoded1366x768 BMPs. Zero audio underruns,
worker errors or lifecycle failures are recorded. Independent final-record,
binary receipt, image and prior-file checks agree with collection. All retained
loop samples validate with no excursions; maximum121868us remains below500ms.
Maximum launch3642026us and close2190458us remain below10s. Follow-up Files,
normal desktop exit and worker/cache/battery retirement pass.

Battery records705 attempts, zero rejections,10578 queries, five notifications,
one attach/detach and zero retained heap/errors/denied operations. The final
version1 epoch1 protocol marker at1811311668us follows PCM end1808072660us:
retired, captured0, kind0. This second separate30-minute pass on the instrumented
payload accepts repeated clean-marker persistence, not a continuous hour or a
battery fix. No real first-failure contents were captured; the intermittent
cause remains unresolved and the original c157 failure/photo/media stay intact.

All760 prior file hashes are preserved except expected `.srvros/meta` changes.
Offline fsck and strict allocation pass with769 files/3161 clusters, zero leaks,
FAT/bitmap mismatches or errors. Equality proof has17068 checks,3444 equal and
13624 changed, zero errors and346545005 logical serialized bytes retained;
23 equal-count growth during PCM and post-playback final proof are bound. These
are not physical write savings or stable latency improvement. Slowest screenshot
27.508960s, receive5.857470s and WRITE18.155823s leave completion latency open.
All original limits, integrity checks and policies remain unchanged. Unattended
1% sound is unassessed; separate short audible acceptance remains distinct.

Current/recovery payloads and durable backups verify, Ubuntu stays default,
one-shot is empty, collector disabled and USB unmounted. No reboot is armed.
Matching raw evidence has its own verified backup outside build. Further battery
changes need captured failure evidence; repeated clean runs do not justify a
speculative transport change. Screenshot write/sync latency remains a separate
measured follow-up.

### Screenshot latency research scope (2026-11-09)

Base `7b529b5e`, after the matching diagnostic repeat was independently
collected and merged. Bounded objective: use independent read-only subagent
audits of evidence, screenshot worker/transfer and filesystem/USB code, plus
primary specifications, to select a concrete latency investigation plan. Scope
is documentation and local evidence analysis; no production code, device policy,
limits, installed payload or new physical run changes in this research block.
Current PR19 networking, PR301 scanout and PR379 WebCore retain separate scopes.
Next checkpoint: publish measured attribution, missing causal bindings, ordered
implementation/validation steps and explicit integrity/acceptance conditions.
Battery first-failure diagnostics remain retained; no speculative battery fix.

Research outcome: the [screenshot latency plan](../macbookair6-1-screenshot-latency-plan.md)
records three independent audits and primary exFAT/USB sources. Latest raw WRITE
wall is97–98% admission waiting, overwhelmingly overlapping storage ownership.
The repeat's worst wait binds to PID7 global sync with3.784027s file work and
406us final device flush; content identity remains missing. First implementation
is correlated sync/content/exFAT operation evidence with deterministic interleaved
flush regression, followed by one proven equivalent-work optimization. Pixel
transfer pacing is separate. No production change or reboot is armed by this plan.

### Correlated sync/content diagnostic implementation scope (2026-11-09)

Base `76608d63`, implementing the first stage of the researched screenshot
latency plan. Bound existing worst SYS_SYNC to real shared content lifetime,
size/outcome and exclusive filesystem I/O roles; retain all three completed
captures for valid comparisons. Touch internal sync timing, process/exFAT
observation hooks, final workspace reporting, desktop capture history and strict
host/parser/VM/native collection. Parallel subagents own capture history and
real-process integration fixtures; root owns shared kernel/collection changes.
No transport, gate, power, ownership, watchdog, allocation or durability policy
changes. PR19 networking, PR301 scanout and PR379 WebCore remain separate.
Next checkpoint: original controller operations/outcomes unchanged under the
observer, complete bound records under existing atomic/tail limits, full matched
validation, then the original unattended30-minute1% stress and Ubuntu recovery.

### Correlated content observer validated and staged (2026-11-09)

Frozen source `ae775d2c` / unchanged ports `39ad9d7` adds internal never-reused
content identity and exact worst-sync/content observation, six exclusive exFAT
block-callback roles, and all three terminal capture histories. The new strict
parsers require bound final records, complete roles/clocks/outcomes and complete
capture lifetimes. See the [contract](../sync-content-timing.md). Ordinary file
operations, global-sync semantics, all owner/global/publication barriers,
controller commands, save readbacks, ownership and watchdog/latency/workload
limits remain unchanged. This is a correlated diagnostic baseline, not a fix.

Real process/exFAT tests interleave65 ordinary64KiB writes into a4196406-byte
BMP with global sync and unrelated dirty content. Shared writers/readers visit
one content once per sync; slot reuse changes identity, while retained reader
failures preserve dirty state and quarantine. Observer-on/off ordered callback/
payload fingerprints and complete final media agree:13764 success callbacks
(hash117aef28e220d2ce),893 writes/881 flushes,13189372 logical write bytes;
injected failure has3850 callbacks with the same on/off fingerprint and preserves
the previous32-chunk durable prefix. Synthetic10/20/30us callback costs exercise
role sums only; they are not native performance. Actual C capture-report output
passes strict parsing and malformed-join rejection.

Full matched normal/native/VM/RAM/USB images, core, desktop and docs pass,
including89 installation/recovery tests in777.587s with one expected skip.
Early host adapter/import/shared-header compilation failures were corrected
before the frozen successful validation; no native trial ran on those candidates.
Ordinary syscall, USB keyboard, fork/exec-vectors/yield and real USB read-only/
write/flush/reload/input checks pass. Populated-Home KVM400-write-IOPS stress
passes22 app cycles/171 overlap observations,140004us maximum loop,
2419479/1070494us maximum launch/close and all original audio/worker/retirement
requirements. Complete saved atomic app records and every old/new final parser
agree independently. Ordinary immutable screenshot/readback/Preview/repeat/exit
passes. Exact stage `1991ff7f10142881` passes Q35 native-board refusal/save/
unmount/automatic return; this is not native positive-device acceptance.

The new VM content record binds PID7 sync24959838us to its selected content
subcall, identity114/slot1,3342336 logical bytes. That subcall takes575126us:
176 old-prefix reads cover2883584 bytes in413697us, while four root payload
writes cover458752 bytes in54547us. Total observed role callbacks568044us leave
7082us residual. Inode is unavailable (zero); identity/time/size bind the content
subcall, not a proven creator PID or path. This supports repeated-prefix cost in
the controlled VM, not a native diagnosis. The single VM capture's complete
history is retained (19.847938s); the native trial must retain all three. A fresh
native baseline is needed before choosing an optimization or claiming gain.

Open PR19/301 share some files, but inspected changes cover metadata import,
logger lifecycle/options and process user-scope/lifetime admission, distinct from
these mutation/content observation hooks. No unmerged behavior or sibling
acceptance is consumed. PR379 WebCore remains separate.

Next is the original1800-second1% automatic audio/disk/app/three-screenshot run,
with500ms loop/10s lifecycle limits, all readbacks/receipts/equality-growth/
retirement/allocation proofs, clean post-PCM battery protocol marker and the new
correlated content/complete capture histories required. Baseline769 USB file
hashes and recovery must verify first. Preserve known-working69a8/318c/8caa and
consolec354 payloads plus full backups; only superseded unreferenced single-window
92d9 EFI duplication may be archived after every file matches its independent
backup and no boot reference exists. The later2ddb sampler remains retained.
Ubuntu remains recovery/default. Save full run/armed state and independent
payload/trial backups before reboot, then collect the matching return first.
The intermittent battery halt remains unresolved; the original c157 failure,
photograph and media archive remain protected. No human input is needed; keep
AC/USB attached. If mounted-Home containment halts, photograph the terminal and
restart Ubuntu. No runtime threshold or policy has been relaxed.

### Correlated content and capture history native baseline (2026-11-09)

Run `2b8237341e9d43e1`, release `1991ff7f10142881`, compiled `ae775d2c` /
ports `39ad9d7` (PR446), returned normally. Matching native session
`session-00065fdfad020f6d-00` completes1800 seconds at1%,1105 app cycles,
4170 verified262144-byte bursts (1093140480 bytes),345731072 PCM bytes and
three exact decoded BMPs. Zero audio underruns, worker or lifecycle errors are
recorded. All53395 loop samples validate without excursions, maximum123210us;
maximum launch3294354us and close2160375us stay below10s. Follow-up Files,
normal desktop exit and worker/cache/battery retirement pass. All769 prior file
hashes remain unchanged except expected `.srvros/meta`;778 files/3268 clusters
pass fsck and strict allocation with zero leaks, mismatches or errors.

All three final capture histories survive and parse independently:19.119232s,
26.609181s and21.991400s, median21.991400s. This is the first complete native
three-capture baseline, not a latency improvement or a reconstructed comparison
with historical current/slowest-only telemetry. New correlated records and every
old acceptance receipt survive the unchanged64KiB saved-log tail.

The selected PID7 SYS_SYNC is3.431484s, with3.431080s in its file phase and
404us in the final device phase. Its single dirty-content visit, identity7585/
slot0, has4128768 logical bytes and3.431072s wall. Native callback roles differ
materially from the throttled VM: root payload writes458752 bytes in2.040995s
(59.49% of content wall); nested sidecar writes131360 bytes in1.071359s (31.23%);
old-prefix proof reads3670016 bytes in183.299ms (5.34%). Total role callback
wall3.419615s leaves11.457ms residual. Prefix plus suffix equals the selected
logical size. Optional inode0 is unavailable: this binds content lifetime,
size and operation, not a creator PID or path.

PID958's worst raw-write gate lasts3.434526s and directly overlaps this PID7 sync
ownership for3.429651s during capture3. This establishes blocking correlation,
not proof that PID958 owns the selected content. The same sync has seven CSW
callbacks above100ms. Its slowest351.828ms CSW belongs to a512-byte WRITE(10):
351.627ms submission-to-software-observation,197us observation-to-resume and
14.695ms sampled service gap. These remain software wall observations; device
latency, exact hardware completion and the role of that command are unproved.
Do not add overlapping role/USB/checkpoint intervals as if disjoint CPU costs.

Battery705 attempts, zero rejections,10578 queries and six notifications retire
with one attach/detach and zero heap/errors/denied operations. The final epoch1
protocol marker after PCM is clean/captured0. The intermittent battery cause
remains unresolved; original c157 failure/media/photo remain retained.

This accepts native correlated-content and complete capture-history persistence
under all original limits and integrity checks. It does not accept a speed gain.
The next bounded investigation is to bind BOT command count/size and worst-command
waits to these existing content roles, separating root payload, sidecar payload
and metadata writes. Native evidence prioritizes write-command completion over
prefix-read optimization. Preserve every byte proof, flush/publication barrier,
owner, transport deadline and quarantine rule; do not speculate about hardware
or remove work based on the VM result.

Installed/current/recovery payloads and independent backups verify, Ubuntu is
default, one-shot empty, collector disabled and USB unmounted. No reboot is armed.
Matching raw evidence has a verified independent trial backup outside build.

### Role-bound USB command timing implementation scope (2026-11-09)

Base `c367f169`, after independently collecting and accepting the correlated
native baseline. Bounded objective: bind BOT command counts/bytes, slow-CSW
counts and worst completion observations to the same selected content and six
filesystem roles, with a seventh explicit unclassified/pre-mutation bucket.
Touch passive USB storage observation, sync/content and exFAT role bracketing,
final workspace reporting, strict parsers and host/VM/native collection. User
authorized subagents; separate agents own transport, parsers and host adapters.
Root owns integration and physical testing. Preserve every command/validation/
checkpoint, byte proof, flush/publication barrier, admission/ownership, quarantine,
watchdog and runtime limit. No latency fix or hardware cause is claimed yet.
PR19 metadata import/logger options and PR301 process lifetime/scanout edits
remain separate; inspect current overlapping hunks before integrating. Next
checkpoint: prove observer-on/off transfer equivalence, exact retained joins and
whole-record/tail retention, full matched validation, then original unattended
30-minute1% native stress. Preserve the accepted1991 baseline and old recovery.

### Command-role observer validated and staged (2026-11-09)

Frozen source `ea27948b` / unchanged ports `39ad9d7` adds a passive content-owned
USB command window inside the existing owned sync observer. Seven roles retain
command classes/bytes, failures, CSW wall/slow counts/worst samples and512-byte
WRITE subsets. Brackets precede existing block callbacks; role6 captures otherwise
unclassified pre-mutation work. The window is copied with the same selected
content/sync. Refused admission preserves foreign observers. Original commands,
validation, checkpoints, byte proofs, barriers, quarantine, ownership and limits
remain unchanged. See the [command evidence contract](../sync-content-timing.md#role-bound-bot-command-observations).

Transport host tests exercise actual command/validation code with simulated
controller callbacks:512-byte versus16KiB writes and slow samples, read/flush/
other separation, nested/foreign admission, immutable copies, overflow, clock
regression, premature end and all original fault/quarantine cases pass. Observer
on/off controller callbacks, checkpoints, commands, complete media and synthetic
clock results agree. Real process/exFAT on/off integration, parser and actual C
capture-wire tests also pass. No native timing equivalence follows from synthetic
host clocks. Row maxima remain687 bytes within the existing1024-byte writer.

Full matched normal/native/VM/RAM/USB build, core, desktop and docs pass, including
89 installation/recovery tests in730.450s with one expected skip. Ordinary
syscall, USB keyboard, fork/exec-vectors/yield and real USB read-only/write/flush/
reload/input checks pass. Populated-Home KVM400-write-IOPS stress passes22 app
cycles/175 overlap observations,158861us maximum loop and879605/1681478us
maximum launch/close under all original requirements. Old and new acceptance
receipts survive in saved logs with complete atomic app coverage. Ordinary
immutable screenshot/readback/Preview/repeat/exit passes. Exact stage
`690fd179a79d156f` passes Q35 native-board refusal/save/unmount/automatic return;
native positive-device acceptance remains pending.

Independent saved-log parsing exactly reproduces sync, content, command and
capture JSON. The selected VM content identity120/slot2 (optional inode0),
3997696 bytes and1121120us span, has330 commands:278 reads/42 writes/10 flushes,
3955712 read bytes/465920 write bytes and53600us CSW wall. Writes split into
14x512-byte commands (7168 bytes,2219us CSW) and28x16KiB commands (458752 bytes,
5728us CSW). Role2 root payload has20x16KiB writes; role4 sidecar has three512-byte
and eight16KiB writes. All seven roles, size/time subsets, same legacy maximum
and complete single-capture history validate. There are no CSW callbacks above
100ms in this VM. This validates observation/retention, not a native cause, a
writer/path identity, exact hardware latency or a speed gain.

Next native trial repeats the original1800-second1% PCM/disk/app/three-screenshot
workload, unchanged500ms loop/10s lifecycle/original60s tail, every readback,
receipt, equality-growth, worker/cache/battery retirement, file/allocation check,
clean post-PCM protocol marker and all three histories, plus the new seven-role
command/size/worst joins. Baseline778 prior files and recovery must verify first.
Keep accepted1991/69a8/318c/8caa, consolec354 and isolated2ddb payloads installed
with full independent backups. Only the unreferenced historical short-audible
5bc EFI duplicate may be archived after every file matches its independent
backup and no boot reference remains; prior listening evidence stays accepted.
Three old stage-only duplicates were already hash-verified against independent
payloads and removed, freeing455662890 bytes; installed/current/latest/recovery
and raw evidence were preserved. Save the full run/armed state and independent
payload/trial backup before reboot. Keep AC/USB attached; no input is required.
Ubuntu remains default recovery. Collect the matching return first; if native
containment halts, photograph the terminal and restart Ubuntu. Battery fault
cause remains unresolved, with original failed media/photo retained.

### Native command-role attribution (2026-11-09)

PR448, frozen source `ea27948b` / unchanged ports `39ad9d7`, release
`690fd179a79d156f`, completes the original1800-second1% automated audio/disk/app/
three-screenshot workload. One matching session completes1101 app cycles and
4076 verified disk bursts/1068498944 bytes with345763840 PCM bytes, zero recorded
underruns, worker errors or lifecycle errors. All52905 loop observations are
valid and below500ms; maximum120023us. Maximum launch2073260us and close3639435us
remain below10s. Three exact4196406-byte1366x768 BMPs decode, post-capture Files
activity and normal exit pass, and worker/cache/battery retirement is clean.
All778 prior files retain their hashes except expected metadata;787 files and
3375 allocated clusters remain, with zero leaks, allocation mismatches or errors.

Independent raw-log parsing exactly reproduces the legacy sync/write/gate and
new content/command/history/protocol assessments. Every original receipt and
all seven command roles survive the unchanged64KiB saved-log tail. The selected
PID7 sync lasts2.758309s: file phase2.757906s and device phase403us. Its single
content visit, identity3719/slot0, has4196406 logical bytes and2.757894s wall.
Optional inode0 remains unavailable; this identifies the content lifetime and
sync caller, not a writer PID or path. Root payload callback writes329782 bytes
in2.365736s (85.78% of content wall); prefix proof reads3866624 bytes in202.895ms.
All role callback wall is2.746199s, leaving11.695ms residual.

The same content window has354 physical commands:300 reads,44 writes and10
flushes;4252160 read bytes,468480 write bytes and2.167689s CSW callback wall.
The15 writes of512 bytes account for43.861ms CSW wall; the29 larger writes
account for2.114728s. All six CSW callbacks above100ms belong to larger root
payload writes. Root payload has22 writes/330240 physical bytes, including one
512-byte write; its WRITE CSWs total2.108302s, of which only2.140ms belongs to
that512-byte write. Sidecar WRITE CSWs total13.607ms with none above100ms.
Role6 is explicitly present and empty. Physical and logical byte/count totals
differ because transport splitting and sector read/modify/write are retained.

The exactly matched worst CSW is a16KiB root-payload WRITE(10),353.884ms wall:
353.673ms submission-to-software-observation,209us observation-to-resumption,
14.661ms sampled service gap and9.659ms completion-observation gap. These are
software wall observations, not exact device completion, CPU time or proof of a
controller defect. The preceding run's unclassified slow512-byte example does
not predict this run's dominant role/size. Do not combine unrelated maxima or
remove small metadata writes based on it.

Complete capture lifetimes are18.798846s,23.601685s and20.066622s, median20.066622s.
This is a diagnostic run with different interleaving and metadata state, not an
accepted speed gain over the previous complete baseline. The slowest complete
capture has9.783633s WRITE/6.718911s receive; capture3 has11.207176s WRITE despite
a shorter total lifetime. Keep each capture's phases paired with its identity.

Battery705 attempts, zero rejections,10590 queries and19 notifications retire
with one attach/detach and zero heap/errors/denied operations. Epoch1's final
protocol marker is clean/captured0 after PCM. No intermittent battery failure
was reproduced; its cause remains unresolved and original failed media/photo
remain protected. Unattended1% sound is unassessed; prior separate25% listening
acceptance remains intact.

Next bounded objective: audit the existing serialized payload-command CBW/data/
CSW and controller-service path against this native evidence, adding only the
minimal matched-command timing needed if current observations cannot distinguish
the remaining wait. Preserve the16384-byte command bound, all original runtime
limits, deadlines, checkpoints, status validation, DMA quarantine, ownership and
byte/flush/publication proofs. Prefix work and small metadata writes are not the
dominant measured cost in this selected call. No speculative transport or power
policy change is justified. Check current PR19/301 overlap before shared edits.

Matching raw evidence, independent review and full payload/trial backups are
retained outside build. Current/recovery and retained payload hashes verify;
Ubuntu is default, one-shot consumed, collector disabled and USB unmounted. No
reboot is armed. The next turn starts from this collected result.

### Same-command completion audit scope (2026-11-09)

Integration base `25594a0f`, after collecting the accepted command-role run.
The source audit has not established a transport bug: the existing sampled
ring-service gap and short post-observation resume cannot alone explain the
selected353.884ms CSW. Existing role maxima omit earlier phases of that exact
command and do not distinguish empty ring checks from unrelated events.

Bounded objective: retain one fixed CBW/data/CSW/checkpoint companion with the
same winning command, including final validation outcome, and classify existing
ring checks using only the existing sampling clock. Touch passive USB/xHCI
observation, final reporting, strict parser/host fixtures and native collection.
Separate user-authorized subagents own transport, xHCI audit and strict parsing;
root owns integration, sequential validation and physical testing. No new ring
read, USB request, clock read, retry, wait, checkpoint, validation or policy is
introduced by the observers. Preserve4096-byte bulk/16384-byte command bounds,
deadlines, quarantine, ownership, durability proofs and500ms/10s runtime limits.
PR19 has independent xHCI networking additions; keep these narrow observer hunks
separate. PR301 scanout/process work remains independent. Next checkpoint is
observer equivalence and strict same-command/tail coverage, followed by the
unchanged automated30-minute1% native workload. Do not claim a performance fix
without causal evidence. Retain the accepted690/1991 baselines and recovery.

### Same-command observer validated for native trial (2026-11-09)

Frozen source `725b8e6c` / ports `39ad9d7`, release `1820d7a0297b1b65`, retains
one240-byte companion with the exact selected content's first maximum CSW.
CBW/data/CSW callback summaries and original checkpoint clocks are copied after
full command validation; failed command outcomes remain distinct from successful
bulk callbacks. The timing envelope ends at the final checkpoint sample, not
the unmeasured return tail. Existing ring checks gain empty/event counters and
the last existing sample whose subsequent cycle read found an empty head. That
pre-check clock is not exact event arrival or device completion. No extra clock,
ring read, controller operation, wait, checkpoint or policy change is introduced.

Actual transport/controller host tests pass: same-role and cross-role first
ties, four4096-byte data segments per16KiB command, exact checkpoints, zero-data
flush, immutable selection, later failed command retention, every original
failure/quarantine case, overflow and invalid clocks/ownership. Observer on/off
clock/sample reads, controller callbacks, commands, checkpoints and complete
media agree. The ring fixture proves513 original checks with exactly three
existing clock samples,512 empty results and one event. Strict parser regressions
reject missing/misbound/conflicting companions, later tied maxima, understated
single-call/checkpoint wall, invalid byte/phase bounds and ring-count mismatches.

Full matched normal/native/VM/RAM/USB builds, core, desktop and docs pass;
89 installation/recovery tests take759.385s with one expected skip. Ordinary
syscall, keyboard, fork/exec-vectors/yield and USB read-only/write/flush/reload/
persistence/input pass. Loaded KVM400-write-IOPS populated Home passes22 app
cycles/173 overlap windows,139984us maximum loop,1081973us launch and1019149us
close, with zero recorded audio/worker/lifecycle errors and clean retirement.
All old and new receipts survive the unchanged64KiB tail with five checkpoints
and complete atomic app evidence. Ordinary immutable screenshot/readback/Preview/
repeat/exit passes. Exact stage passes native-board refusal/save/unmount/return;
positive native acceptance remains pending.

The saved VM companion joins PID7 sync26716386/content119/role2 and its16KiB
WRITE CSW27692304–27693121. Callback/checkpoint envelope2062us contains CBW174us,
four data callbacks851us, CSW817us, four checkpoints96us and124us residual.
The same CSW has33 ring checks:32 empty and one event; last sampled-empty
pre-check clock27692433 precedes software observation27693096. This proves
retention and joining, not a native latency fix or a hardware cause. The sync
snapshot measures30496 bytes within the128KiB kernel stack. The five added rows
have maximum widths533/434/350 bytes within the unchanged1024-byte atomic writer.

The next native trial keeps1800-second1% PCM plus the original60-second tail,
500ms loop/10s lifecycle limits, disk readbacks, three exact BMPs and histories,
equality growth, clean cache/worker/battery retirement, post-PCM protocol marker,
all prior file hashes and offline allocation/headroom. It additionally requires
the exact command companion and classified ring checks in a post-PCM snapshot.
No input is required; keep AC/USB attached. Preserve accepted690/1991/69/8caa,
consolec354 and isolated2ddb installed payloads and full independent backups.
Only the unreferenced historical query-only318 EFI duplicate may be archived
after full manifest/every-file equality and GRUB/firmware reference checks; its
full payload and accepted history remain backed up. Four older stage-only
duplicates were separately verified against full independent payloads and removed,
freeing594453273 bytes without deleting installed payloads or raw evidence.
Save run/armed state and verified independent payload/trial backups before
reboot, then collect the matching return first. Original battery failure evidence
remains protected and its cause unresolved. No speed gain is claimed.

### Native command phases and ring results (2026-11-09)

PR450/release `1820d7a0297b1b65`, frozen source `725b8e6c` / ports `39ad9d7`,
completes the unchanged1800-second1% audio/disk/app/three-screenshot workload.
The matching session completes1099 app cycles and4106 verified disk bursts/
1076363264 bytes with345763840 PCM bytes, zero recorded audio underruns or
worker/lifecycle errors, and clean desktop/cache/worker/battery retirement.
All52811 loop observations are valid with zero excursions; maximum127764us.
Maximum launch1786318us and close2381131us remain within10s. Three exact1366x768/
4196406-byte BMPs decode, binary disk receipt and post-capture app/normal-exit
proofs pass. All787 prior file hashes persist except expected metadata;796 files
and3482 allocated clusters remain, with zero leaks, mismatches or errors.

Independent raw-log parsing reproduces every old acceptance assessment and all
new companion rows from the same four saved logs. The unchanged64KiB final tail
retains complete command/content/phase/service, app, capture and retirement
evidence. PID7's selected sync lasts2.046927s (files2.046523s/device404us). Its
single dirty content113/slot1 has1835008 logical bytes and2.046509s wall. Inode0
cannot establish writer/path. Root payload writes393216 logical bytes in1.467938s
(71.73%); metadata writes144 logical bytes in411.589ms (20.11%); old-prefix proof
reads1441792 bytes in53.801ms. All role callback wall2.037861s leaves8.648ms
residual. Physical transport byte/count totals differ from logical block work.

The same window has188 commands:132 reads,46 writes and10 flushes,1818624 read
bytes/531456 write bytes and1.832325s CSW callback wall. Its five CSWs above100ms
include four larger root-payload writes and one512-byte metadata write. Fourteen
512-byte writes total408.138ms CSW wall;32 larger writes total1.417532s. Root
payload still dominates overall write work, but the preceding run's exclusively
larger-write slow classification is not universal. Do not infer a size-specific
transport fix or remove required metadata writes from either selected maximum.

The exact first global maximum is that512-byte role0 metadata WRITE. Its380.439ms
callback/checkpoint envelope partitions into CBW10us, data10us, CSW379.916ms,
final checkpoint500us and3us residual. Full command validation succeeds. The
same CSW records379.913ms submission-to-software-observation,1us observation-to-
resume,6678 original ring checks (6677 empty and one matching event),27 existing
samples and20.078ms maximum sampled service gap. Its last sampled-empty pre-check
clock is360.057ms after submission and19.856ms before software observation.
That clock precedes the original cycle read that found an empty head; it is not
an exact empty-read, hardware completion or event-arrival timestamp.

These observations do not support unrelated-event backlog or a379ms post-event
scheduling stall for this command. They do not distinguish device from controller
delay or prove exact hardware latency. Original BOT serialization,4096/16384-byte
bounds,2s controller deadline, status validation, ownership, quarantine and every
durability/integrity check remain unchanged. No causal USB policy fix is justified.

All capture histories survive:28.407627s,17.788599s and19.289795s, median19.289795s.
No speed gain is accepted from separate instrumented cold boots and differing
workload/metadata interleaving. Their receive phases remain5.851004/6.230978/
6.251989s. The next bounded investigation moves to the independent pixel-transfer
pacing stage of the [current plan](../macbookair6-1-screenshot-latency-plan.md):
measure successful sends, EAGAIN/readiness and gaps before changing transport
ownership or scheduling. The unchanged source permits four4096-byte sends per
desktop poll. Preserve immutable pixels, current per-admission bounds, pipe
capacity, cancellation/exit ownership, complete readbacks and all runtime limits.

Battery705 attempts, zero rejections,10575 queries and one notification retire
with one attach/detach and zero heap/errors/denied operations. Epoch1 protocol is
clean/captured0 after PCM. Intermittent battery cause remains unresolved; original
failed media/photo remain preserved. Unattended1% sound is unassessed; the prior
separate25% audible check remains accepted.

Matching raw evidence and full payload/trial backups verify outside build.
Current1820 and retained690/1991/69/8caa/2dd/recoveryc354 payloads verify. The
historical query-only318 EFI duplicate was archived only after full independent
payload/every-file equality and no GRUB/firmware references; its payload and
accepted history remain retained. Ubuntu is default, one-shot consumed, collector
disabled and USB unmounted. No reboot is armed; collect-first closeout is complete.

### Pixel-transfer pacing measurement scope (2026-11-09)

Base `0d83d045`, after independently collecting the accepted same-command run.
Bounded objective: separate desktop send cadence/backpressure, worker allocation
and pixel reception before choosing a transport owner or scheduling change. The
1366x768 input requires1026 successful writes and at least257 polls at the
existing four4096-byte admission bound. The receive phase also includes two
worker allocations; its six seconds cannot all be attributed to UI cadence.

Touch opt-in capture-job scalar diagnostics, one explicitly opted-in allocation
ready frame, immutable three-capture companion reports/strict parser and full-size
deterministic fixtures. Ordinary request/response sizes and save phases remain
unchanged; a separate request magic selects the diagnostic frame extension.
Root coordinates user-authorized transport/fixture/parser subagents and owns
sequential build/VM/native validation. Preserve immutable snapshots, pipe size,
per-poll/chunk bounds, current prepare/cleanup owners, every save/readback/barrier
and all runtime limits. No blocking poll is added to the UI; the current shared
lifecycle worker can block on unrelated app work and is not assigned transport.
PR301 process/scanout and PR19 networking remain separate. Next checkpoint is
actual full-size mock-pipe proof, bounded complete saved companions, matched VM
validation and the unchanged30-minute1% native test. No speed fix is claimed.

### Capture-transfer baseline validated for native trial (2026-11-09)

Frozen source `3d5f3cf7` / ports `39ad9d7`, release `eb3a60e0fbc0b436`, adds
opt-in software transfer observations without changing capture cadence or owners.
Only the explicit audio/disk test selects TRACE request magic and one allocation-
ready frame. Request176/response32 bytes, four4096-byte write attempts per poll,
one response read, pipe capacity, immutable pixels, existing phase accounting,
resource retirement and all save/readback/durability checks remain unchanged.
Ordinary capture keeps its original clocks and frames. A272-byte scalar record
retains poll/write attempt wall and gaps, byte/outcome counters and worker
RECEIVE/allocation-ready/ENCODE/final clocks. Clock/overflow faults invalidate
diagnostics only; malformed auxiliary protocol still fails validation.

The actual full-size sender fixture passes4196352 pixel bytes plus176 request
bytes through a bounded64KiB pipe model:1026 writes,257 send polls and256 synthetic
caller intervals. It covers immutable bytes/path, partial headers/body, EAGAIN/
EINTR, blocked peers, progress bursts, EOF/EPIPE, buffered final output after exit,
cancellation and retirement after accepted work. Ordinary clock/callback and
trace-mode normalized input/control equivalence, malformed READY, counter reset,
overflow and clock-fault tests pass. Nine strict parser tests and actual C
three-capture wire checks pass, including final-only emission, immutable middle
completion and malformed bindings. Row maxima608/769/648 bytes fit the unchanged
1024-byte atomic writer. Request and pixel completion clocks observe syscall
return, not exact pipe publication; worker reception may precede writer resume.

Matched normal/native/VM/RAM/USB builds, desktop/capture, real process/exFAT sync
integration and docs pass. Ordinary syscall/keyboard/fork/exec-vectors/yield,
real USB read-only/write/flush/reload/persistence/input and immutable screenshot/
readback/Preview/repeat/exit pass. Kernel/process/scheduler/syscall/exFAT/USB/xHCI/
EC/SBS/lifecycle/save implementation and ports are unchanged. This scoped run
does not claim another execution of the previous full89 installation tests.

Final-source populated-Home KVM400-write-IOPS stress passes22 app cycles and180
overlap windows:239893us maximum loop,1000005us launch and1708854us close, with
zero recorded audio/worker/lifecycle errors and clean allocation/retirement.
Independent parsing reproduces all16 assessments and all22 app lifetimes from
saved logs alone, alongside every legacy final receipt in the unchanged64KiB
tail. Five periodic checkpoints omit the new transfer companions; immutable
history emits them only in the final report. An earlier source78f1271d validation
passed the existing original-console/atomic-checkpoint fallback with one missing
saved app event; that complete evidence is preserved locally. Final-only output
reduces repeated diagnostic traffic. Neither the differing cycle counts nor
these two runs establish a causal performance or retention improvement.

The final VM's4096176-byte input has1001 successful writes and166 EAGAIN outcomes,
1167 attempts, zero partial/interrupted/failed writes and249 exhausted budgets.
Its5.954783s write-attempt envelope is73.609ms callback wall plus5.881174s gaps.
Worker allocation window39.657ms plus reception window5.895186s equals the old
RECEIVE phase5.934843s. Those windows include initial progress/READY emission;
they are not pure mmap/pipe CPU time. The reply has201 progress frames, one READY
and one final response:6496 bytes. Final production-to-UI observation is618.284ms;
complete capture21.278283s. Gaps combine desktop cadence, rendering, scheduling
and backpressure; VM observations do not establish native cause or a speed gain.

Exact stage passes Q35 native-board refusal/save/unmount/automatic return; native
positive acceptance remains pending. Next test repeats the original1800-second
1% audio/disk/app/three-screenshot workload plus60s tail and every original
500ms/10s/readback/receipt/equality/retirement/protocol/file/allocation requirement.
All three immutable transfer companions must bind the final history and exact
Air input size, valid clock partitions and worker allocation/reception split.
Keep AC/USB attached; no input is required. No blocking UI poll, new transfer
owner, USB/power policy change or save shortcut is part of this baseline.

Retain accepted1820/690/1991/8caa, isolated2dd and consolec354 installed payloads
and full independent backups. Only the unreferenced historical first-protocol69
EFI duplicate may be archived after full manifest/every-file equality and no
GRUB/firmware references; its full payload/history remain backed up. Four old
stage-only duplicates were separately checked against complete independent
payloads and removed, freeing591379208 bytes without deleting installed payloads
or raw evidence. Baseline796 files, recovery, run/armed state and independent
payload/trial backups must verify before reboot. Collect the matching return
first. Original battery failure media/photo remain protected; cause unresolved.

### Native capture-transfer baseline (2026-11-09)

PR452/release `eb3a60e0fbc0b436`, frozen source `3d5f3cf7` / ports `39ad9d7`,
completes the unchanged1800-second1% audio/disk/app/three-screenshot workload.
The matching session completes1070 app cycles and4019 verified disk bursts/
1053556736 bytes, with345731072 PCM bytes, zero recorded audio underruns or
worker/lifecycle errors, and clean desktop/cache/worker/battery retirement.
All53099 loop observations are valid, with zero excursions and129977us maximum.
Maximum launch2131630us and close3729053us remain within10s. All three exact
1366x768/4196406-byte BMPs decode; binary disk receipt, post-capture app work,
normal exit, equality and every prior acceptance check pass. All796 prior file
hashes persist except expected metadata;805 files/3589 allocated clusters remain,
with zero leaks, FAT/bitmap mismatches or errors.

Independent parsing of the four saved matching logs reproduces every companion
and parent assessment. All three immutable transfer records join the exact
capture histories and final post-PCM snapshot in the unchanged64KiB tail.
Capture durations are18.841303/25.986970/17.580316s, median18.841303s. This is an
instrumented baseline, not an accepted speed improvement over other cold boots.

The worker allocation windows are84.210/902.179/161.925ms; reception after
allocation takes5.419596/5.828279/5.642890s. These windows include progress/READY
emission and scheduling, so allocation is not pure mmap CPU time. Each capture
sends4196528 bytes in1026 successful writes and exhausts256 four-write budgets.
EAGAIN counts are225/322/252, with no partial/interrupted/failed writes.
Write-attempt envelopes5.502525/6.746859/5.770351s partition into only8.552/
9.123/8.707ms syscall wall and5.493973/6.737736/5.761644s gaps. Maximum write gaps
are32.184/39.389/45.577ms. Each reply retains207 progress frames, one READY and
one final response. Final production-to-observation adds510.815/536.822/839.938ms;
write phases remain9.265289/15.892517/8.249228s and remain a separate cost.

The transfer gap dominates sender syscall wall, while allocation alone does not
explain reception. Gaps combine UI turns, rendering, scheduling and backpressure;
they do not prove pure sleep time, pipe publication time or device latency.
Next audit an exclusively owned readiness-driven capture transport, preserving
four4096-byte writes per admission, one response read, pipe capacity and immutable
pixels. Prove bounded readiness, cancellation, accepted-work exit and retirement
before changing ownership. The shared lifecycle worker can block on unrelated
app work and must not acquire this transport without such proof. Keep the
500ms/10s limits, all save/readback/durability checks and USB/power policy intact.
Do not repeat the unchanged diagnostic workload without a new question.

Battery705 attempts, zero rejections,10579 queries and six notifications retire
with one attach/detach and zero heap/errors/denied operations. The post-PCM epoch1
protocol marker is clean/captured0. The intermittent battery cause stays unresolved;
original failed media/photo remain preserved. Unattended1% sound is unassessed;
the earlier separate25% listening acceptance remains accepted.

Matching raw evidence and complete payload/trial backups verify outside build.
Currenteb3 and retained1820/690/1991/8caa/isolated2dd/recoveryc354 installed payloads
verify. Historical69/318 EFI duplicates remain backup-only with their full
payloads and accepted histories retained. Ubuntu is default, one-shot consumed,
collector disabled and USB unmounted. No reboot is armed. Shared main is the
merged PR452 baseline; open PR301 scanout, PR19 networking and PR379 WebCore
remain separate. This closeout changes documentation only.

### Screenshot code-audit scope (2026-11-10)

Base `486b6d2a`, after collecting and backing up the PR452 native transfer run.
User-requested audit before another reboot: root and three read-only subagents
review transport/readiness, storage/durability, and completion/lifecycle/test
coverage. Publish exact code references and distinguish demonstrated behavior,
confirmed code risks and unproven native causes. This checkpoint makes no production change, physical
USB write, stage or reboot. One isolated
host-only reproduction may inspect the shared-content versus media verification
edge case; no concurrent build/VM harness runs. Other machines' PR301 scanout,
PR19 networking and PR379 WebCore remain separate. Preserve all runtime limits,
barriers, byte checks, ownership/quarantine and original battery-failure evidence.
Next checkpoint is a ranked code audit and regression requirements, not another
unchanged diagnostic reboot.

### Screenshot code-audit findings (2026-11-10)

Three subagent reviews and root cross-checks are consolidated in the
[code audit](../macbookair6-1-screenshot-code-audit.md). Confirmed hotspots are
UI-turn pixel sends and response observation, storage-gated ordinary pipe poll,
shared lifecycle preparation/cleanup, and repeated global commits of growing
content. Native raw screenshot writes spend96–98% of wall time in admission;
handler work is about21ms per capture. No new causal hardware claim follows.

A disposable real process/exFAT/VFS host reproduction confirms a correctness
edge case: a retained reader can keep correct shared RAM alive so a fresh reopen
discards corrupted media bytes and reads RAM instead, including after rename.
Controls without the retained reader and after last-reference release detect
corruption. This tests the exact read-only-open core, not complete capture_save
or native scheduling; it establishes no corruption in accepted native runs.
Prioritize explicit fresh-media verification and the complete-save regression
before transport optimization. Ordinary shared-content semantics and all
readbacks/barriers must remain intact. Other confirmed issues are discarded
screenshot child exit status and redundant scale>1 UI resampling; scale1 native
results do not exercise the latter. No new double-free/deadlock was demonstrated.

No production code changed and no physical USB was touched. Reproduction sources,
compiler command, three case logs and media hashes are preserved locally and in
a verified backup outside build; disposable images were removed. Next steps and
required readiness/cancel/exit/retirement regressions are in the audit. Ubuntu,
installed payloads, prior acceptance and unarmed reboot state remain as collected.

### Fresh-media verification implementation scope (2026-11-10)

Base `a2a6d692`, following the three-agent code audit. Bounded objective: make
both screenshot byte proofs explicitly consume fresh VFS media snapshots even
when ordinary readers retain shared content. Add a synchronous bounded read-only
verification ABI under existing namespace/I/O admission, its userspace wrapper,
and complete-save real process/exFAT corruption regressions. Keep normal shared
file/descriptor semantics, checked-rename open-prefix refusal, all write/sync/
readback/cleanup barriers, limits and power/device gates. Root owns API/core/
shotworker, subagents own syscall tests and integration fixtures; one sequential
build/VM owner. Follow with the independent readiness/transport change only
after this correctness contract is validated. PR301 lifetime-owner work remains
separate; PR19 uses syscall169 in its actual Wi-Fi diff; verification selects170 to avoid
a numeric collision. PR379 remains separate. Publish the candidate and host/VM
results before physical acceptance; no prior native result accepts this change.

The dependent transport work prepares a separate exclusive screenshot owner,
with narrow simple-pipe readiness under unchanged scheduler eligibility. Root
and subagents additionally touch the poll implementation/test and desktop
capture/scaled-snapshot ownership, with no shared filesystem/power optimization.
Validate the new [media proof](../file-media-verification.md) and transport as
separate regressions before the combined candidate. Correctness validation now
reproduces actual historical full-save false success after final-path corruption
and rejects it in the candidate; checked rename still refuses retained temporary
readers. Snapshot proof progress now emits start/completion rather than per64KiB
read frames, so later completion timings must account for that protocol traffic
change separately from transport pacing.

### Verification and capture-owner host candidate (2026-11-10)

The complete real process/exFAT/VFS save fixture passes14 candidate/historical
cases. Actual historical save.c falsely returns success when final media
corruption is hidden by a reader acquired after checked rename; the candidate
returns I/O failure and preserves the destination. Temporary corruption now
fails before rename. Positive retained readers, ordinary shared RAM semantics,
checked-rename refusal, anchor contents, fsck and allocation remain correct.
The new verification syscall170 (169 is already used by the actual PR19 diff)
uses fresh VFS bytes, exact type/identity/length/mtime and bounded checked copies;
normal opens, shared buffers and all barriers remain intact.

The dedicated [capture owner](../desktop-capture-ownership.md) and strict pipe
metadata wait pass focused host tests. Actual job/pipes/pthreads transfer the
full Air input with no UI polling while unrelated work is held pending. Tests
cover prepared/partial/accepted cancellation, deterministic cancel/stop while
SNAPSHOTTING, buffered final before child exit, joining, allocation failure and
exact FD/mapping/PID retirement. They exposed and fixed a stop-after-DONE wake
race. The scale2 scene checks no discarded pre-prepare conversion. Host seams
are portable POSIX; the protocol peer models persistence, while the separate
complete-save fixture exercises real filesystem work.

Independent API, pipe and owner reviews find no current production blocker.
They prompted a final-comparison metadata fault test and snapshot-copy race
coverage. Same-process scheduler exclusion remains unchanged; the requested10ms
poll timeout is not a universal cancellation deadline. Existing screenshot
child-exit status and failed-temp-cleanup receipts remain separate robustness
gaps. No native performance/acceptance claim is made. Next checkpoint is frozen
matched images, full core/desktop checks, ordinary and loaded guest composition,
strict saved receipts, exact-stage rehearsal and verified recovery/install/run
records before the original30-minute1% automated trial.

The first frozen full-core attempt found a pre-existing PR452 wire-check runner
that parsed required CLI arguments on unittest import. Its main guard now matches
the history-wire runner; discovery and the direct actual-C wire test both remain
required. The interrupted validation is retained locally and is not a pass; rerun
the full frozen suite after this test-only fix.

The first owner VM candidate5a3e982c/releasea48fa93078681a00 passes the original
functional checks, but independent inspection finds11047 EAGAIN retries. Generic
POLLOUT admits any free byte while the next atomic4096-byte write needs its full
length: a176-byte header plus fifteen writes leaves3920 bytes free. This is a
new owner retry-storm defect, not evidence to change generic readiness or pipe
bounds. No native boot uses that candidate. Its complete successful functional
logs and blocking audit are retained locally as historical evidence.

The corrected owner probes immediate readiness only after an admission makes no
input/response/frame/READY progress, then backs off one existing tick for a
positive hint. Otherwise it uses the normal bounded readiness wait; productive
turns gain no added delay. A portable deterministic64KiB staging model reaches
that exact61616-byte prefix, observes five EAGAINs/five backoffs, then forwards
the exact bytes to the real peer and verifies all4196528 input bytes. Focused
owner/desktop tests pass again; rerun frozen validation before native deployment.
The VM harness now retains the raw BMP and binary disk receipt for independent
post-image audit. All byte/receipt validators remain unchanged. The first VM's
app/file-service evidence uses the existing original-console fallback for a
PID18 gap; the other fourteen assessments parse from saved logs alone. Do not
claim complete saved-only app coverage or isolated speed gain from that trial.

The corrected82416edb loaded VM reduces EAGAINs to6/1001 successful writes, with
330021us sender span and267198us post-allocation reception. It completes22 app
cycles and clean return within original runtime limits, but strict validation
rejects one missing ready cycle7/PID18 receipt. The original row is interleaved
inside UART text and has rolled out of the saved tail; closed7 survives. No
missing text is reconstructed and no native boot uses that incomplete candidate.
All matching raw logs and the rejection are retained as historical evidence.

A short complete-cycle producer row now preserves the exact ready elapsed scalar
at readiness and emits it with original close elapsed/cycle/PID/errors at close.
Original rows remain. The strict decoder checks all cycles/PIDs, same-format
duplicates, agreement between copies, zero errors, versions and original10s
bounds. Seventeen parser tests and actual C wire checks pass. No log cap, runtime
limit, UART IRQ lock or stored worst summary is used to bypass missing evidence.
The harness clears stale companion JSON and retains binary evidence before later
checks. Revalidate the frozen candidate; app-stage/file-service fallback remains
an explicit existing evidence limit.


## Fresh-media and capture-owner candidate validation (2026-11-10)

PR455 compiles source `b4982ee9a3b41eab875c406fbd32b835c05a37cf`, with ports
`39ad9d702d018723457e59415e10e22d9778acb8`, into matched release
`d551c6584bed346e`. Full normal/USB/native-battery/VM-battery builds, core,
desktop and docs pass locally; hardware-install coverage reports89 tests with
one skip. The complete historical/candidate save replay passes14 cases, including
old false success versus new rejection for backing corruption hidden by a
retained reader. All replay dependencies remain byte-identical at this source.

Ordinary syscall ABI, USB keyboard, fork/exec-vectors/yield, real USB storage
write/flush/reload and capture/readback/Preview/repeat/exit checks pass. The
populated Home KVM at400 write IOPS completes21 app cycles and176 observed overlap
windows with zero recorded audio underruns, all67 prior files preserved, exact
BMP and binary disk receipt, clean allocation and complete saved app records.
Maximum loop/ready/close durations are140.163ms/1.121010s/1.545872s under the
unchanged500ms/10s bounds. Exact-stage Q35 rehearsal verifies the native profile's
resource refusal, saved report, unmount and automatic return; it accepts no
native EC ownership. Independent review reproduces all retained assessments
from saved/checkpoint logs without console fallback, including exact lifecycle
receipts, and checks raw BMP pixels against the PNG and the40-byte disk receipt.
The disposable whole-volume image is not retained for a second traversal; its
prior-file/allocation checks remain the source-bound harness evidence.

The loaded capture completes in14.349282s. Its sender spans393.242ms, allocation
66.816ms, subsequent reception297.934ms and owner final-response observation
7.637ms. There are1001 successful writes and14 EAGAINs, rather than the first
candidate's11047 retries. These are source-bound VM observations, not native
performance acceptance: the proof progress traffic and response observer change,
and storage/barrier cost remains substantial.

Next is the original automated1800s1% audio/disk/app/three-screenshot hardware
trial, with60s tail, unchanged500ms loop/10s lifecycle, all fresh byte proofs,
prior-file hashes, sync/command/transfer/history records and clean retirement.
Recheck this unit's USB6ADA-85BB, AC, Ubuntu default and recoveryc354 payload,
full independent backups, offline allocation and exact release before arming.
No native candidate acceptance is claimed here. The intermittent battery cause,
screenshot child-status/cleanup receipts and storage amplification remain open.


## Native fresh-media and independent capture (2026-11-10)

The automatic trial of release `d551c6584bed346e`, compiled from
`b4982ee9a3b41eab875c406fbd32b835c05a37cf` with ports
`39ad9d702d018723457e59415e10e22d9778acb8`, completes in
`session-00065fe705c67e8a-00`. PR455 is merged at `40767a50`; its documentation-only
follow-up does not change the tested binaries. This accepts bounded native
composition of fresh VFS screenshot proofs, independent capture ownership and
the original workload. Corruption/refusal fault injection remains host coverage,
not a native media-fault claim.

The original1800s1% workload completes4247 verified disk bursts,1113325568 disk
bytes and345763840 PCM bytes with zero recorded audio underruns or worker errors.
The lifecycle controller completes1101 app cycles with no errors,51,600 valid
loop samples, zero limit excursions and120.184ms maximum loop. Maximum app ready
and close durations are2.656183s and2.188021s under the unchanged10s limit. Three
automatic screenshots complete during live PCM, a follow-up app opens after them,
and ordinary desktop exit, cache retirement and worker cleanup complete normally.
No subjective listening report is required at1%; the earlier30-second25%
user-confirmed speaker check remains separate.

All three1366×768 BMPs have exact4196406-byte lengths and decode completely.
The raw40-byte disk receipt agrees with the workload report. Independent saved
log parsing reproduces capture transfer/history, write/admission, sync/content/
command phases, loop/render/operation, equality, clean post-PCM protocol and
retirement evidence. Native lifecycle evidence retains the original controller
summary and worst-lifetime records; the four bounded logs do not preserve every
one of1101 individual app-stage receipts. Do not transfer the shorter VM's full
saved-only per-cycle coverage claim to this native trial.

Capture totals are13.803511/19.805365/15.025137s, median15.025137s. Sender write
spans are336.282/647.823/288.122ms, with18/18/12 EAGAINs and1026 successful writes
each. Allocation spans147.351/117.353/79.467ms precede post-allocation reception
116.554/448.089/138.002ms. Final-response observation takes62/1703/1036us;
slowest resource cleanup is2.755ms. The owner response observer and reduced
verification progress traffic differ from the previous baseline, so these
absolute measurements do not isolate a transport speed gain.

Residual write phases take10.036229/16.348322/8.630967s. Each has65 full raw writes;
95.93–98.15% of raw write-call wall time is admission, with actual handler wall
around21ms per screenshot. The second capture's worst2.8968s gate contains an
observed2.82064s overlap with PID7's existing SYS_SYNC scope. Its selected
1441792-byte content commit takes2.820715s, including2.663533s in data-role block
callbacks. These observations identify shared storage admission and repeated
content commit work as the next audit target; they do not identify physical
media timing or justify removing barriers, prefix proofs or ownership gates.

All805 prior files are present with unchanged bytes except expected filesystem
metadata. There are814 files and3696 allocated clusters after nine new outputs;
independent offline fsck and allocation traversal report zero leaks, bitmap/FAT
mismatches or errors. Actual USB hashes match collection. Battery epoch1 retires
cleanly after705 attempts,10579 queries and five notifications, with zero heap,
errors or denied operations; its post-PCM first-protocol snapshot is clean.
The earlier intermittent battery cause remains unresolved and its original
failed-media archive is retained.

Ubuntu default, consumed one-shot, current/recovery payloads and independent
payload/trial backups are verified; the collector is disabled, USB unmounted and
no reboot armed. All500ms/10s/workload/tail/pipe/admission/ownership/durability
requirements remain unchanged. The next bounded work is a deterministic audit
of filesystem write admission and global content-commit amplification, preserving
full integrity checks, existing barriers and all limits.


## Active storage-admission audit scope (2026-11-10)

Bounded objective: reproduce the accepted d551 screenshot's remaining write
admission and repeated content-commit cost, then remove only demonstrated
avoidable work with equivalent integrity/durability semantics. Integration base
is `f5043c4a`; the prior native run is fully collected and no reboot is armed.
Scope is process shared-content flushing, exFAT append/replacement/sidecar work,
existing scheduler-admission contracts, deterministic host process/VFS/exFAT
fixtures and this unit's result guides. No hardware/profile changes are intended.

Actual open PR301 at `f620b735` edits process lifetime-owner loss; PR19 at
`5b91f61d` edits network/process lifetime pipe handoff. Neither changes the
content-flush functions under audit. Keep those scopes intact and inspect actual
integration again before merging shared code. PR379 remains separate WebCore
planning. No other-machine task is contacted.

Keep every dirty live content in global sync, both fresh screenshot proofs,
complete append-prefix checks, checked rename, all fsync/global/device barriers,
sticky failure/retry rules, ownership/quarantine and scheduler eligibility.
Do not defer dirty data merely to avoid another writer's sync, widen the stream
cluster gate, enlarge64KiB writes/16KiB bulk or change500ms/10s/1800s+60s limits.
Next checkpoint is a real-process/VFS/exFAT replay quantifying callback/byte/
barrier work and any equivalent reduction, independently reviewed before a
frozen build/core/loaded/exact-stage/native candidate. Unproved shortcuts do not
justify another hardware run. Existing accepted payload/recovery and raw failed
media remain protected; workspace pruning may remove only full-hash-verified
uninstalled stage duplicates with independent payload backups.


The [storage-admission plan](../macbookair6-1-storage-admission-plan.md) now has a
real65-write process/VFS/exFAT reproduction. A controlled adjacent append can
retain NoFatChain and omit three unnecessary FAT writes without changing its
eight flushes. However, the ordinary screenshot replay never takes that path:
all13787 callbacks/904 writes/889 flushes are unchanged. This is not a proven
screenshot-delay fix and no native reboot is planned solely for it. Fault
comparison additionally preserves baseline cases where fsck reports clean but
complete allocation still finds orphans; do not count those as clean recovery.
A reserved-buffer/writeback ownership contract is the next design prerequisite,
not an automatic grant to bypass the existing I/O gate.


The frozen `f44b7967` storage candidate passes full matched/core/desktop/docs,
ordinary USB/capture and loaded400-IOPS VM validation. Independent review checks
all original contracts plus the new local-phase counters, actual BMP and binary
receipt. The VM has22 app cycles, zero recorded underruns and140.143ms maximum
loop; app stages require the existing original-console fallback. Exact-source
143 cuts retain81 baseline-equivalent failed repairs explicitly. No native
stage/install/reboot is performed and accepted hardware remainsd551/c354.
The bounded NoFatChain optimization does not fix the ordinary replay's admission
delay. Main work continues through a separately reviewed pre-reserved immutable-
prefix append contract, with no existing ownership or runtime requirement waived.


## Active reserved-append ownership scope (2026-11-10)

Bounded objective: reduce demonstrated screenshot write admission by admitting
only explicitly reserved, private, bounded append-only RAM insertion during
foreign I/O, with immutable captured writeback prefixes and dirty-generation
tracking. Base is merged PR457 `c3550c82`; its unrelated adjacent-allocation
optimization has no claimed ordinary-workload gain. The accepted physical
payload remainsd551/recoveryc354 and no new reboot is armed.

Scope: shared process content/write-description leases and snapshot lifecycle,
readonly scheduler entry proof, exact raw SYS_WRITE classification/copy/fallback,
a gated capacity-only descriptor opt-in, shotworker's exclusive temporary file,
versioned ordinary/fast write diagnostics and meaningful host/VM regressions.
PR301's lifetime-owner changes and PR19's pipe/network handoff remain separate;
actual heads `b8e9ca45`/`29e639b7` are inspected, with no competing reserve command.
Publish/review all shared changes through this unit's branch; no other-machine
messages or hardware gate changes are authorized by this scope.

The first user is only a newly created empty write-only exclusive temporary
file. Reservation preserves visible size, offset, dirty state and metadata and
uses the existing16MiB buffer bound. Fast copies remain at most64KiB, require
both content and write-description privacy, stable process/image/FD/capacity,
matching main/CR3 context, full user-range validation and unchanged FSIZE rules.
No allocation, namespace/backend work, signal delivery or yield occurs in the
atomic attempt. Every unsupported/ambiguous case retains ordinary admission.
Snapshot data/length are immutable through the backend; concurrent appended
bytes remain dirty until a later flush. No pre-entry dirty prefix may disappear
from global sync and no drain-until-clean loop may extend it indefinitely.

An optimization-only dirty-tail window is to be conservatively bounded before
implementation so batching does not create an unchecked giant final flush.
This does not raise any existing copy, write, command, pipe or runtime limit.
All full fresh screenshot checks, prefix proofs, fsync/global/device barriers,
checked rename, ownership/quarantine, same-process exclusion and500ms/10s/
1800s+60s requirements remain. New diagnostics must explicitly partition true
ordinary gates and proved RAM copies; never manufacture zero gate observations
under foreign ownership. Experimental/typed signal paths gain no implicit grant.

Next checkpoints: independently reviewed process/syscall ownership contracts,
actual mid-writeback append/fault/refusal/dirty-survivor tests, a source-bound
65-write replay showing relevant reduction, strict old/new diagnostic fixtures,
frozen full matched/core/VM/exact-stage checks, then verified recovery/install/
run records and the original1% native stress only if evidence justifies it.

The [reserved-append contract](../reserved-append.md) is implemented in the
working candidate, including the conservative256KiB suffix window, immutable
borrow and dirty-generation retirement, strict ordinary/fast diagnostics and
ordinary fallback. Review identified an interrupt-return lifetime gap; borrow
retirement now precedes restoring the original interrupt state, with a queued
last-close regression. The controlled host replay must include a genuinely
unreserved baseline and retain all65 foreign sync calls/markers, both full fresh
verifications and complete allocation checks. Full integration and native
acceptance remain pending; no new payload or reboot is armed. Actual refreshed
PR301/PR19 heads are0927d66e/4ab16524; their latest changes retain separate
graphics qualification and authenticated-network evidence scope.

Final cross-review also found a generic interrupt-enabled last-release slot
reuse gap. The candidate retains its last content reference through flush/free/
reset and restores the original interrupt state only after retirement, without
changing backend service windows. A forced queued allocation regression checks
that the new slot survives old cleanup. The initial b742ffe9 full build was
stopped for this review and is incomplete, not accepted; a new frozen source
must pass all integration checks before any physical stage or reboot.

Frozen source `51670ae6` now passes matched images, full core/desktop/docs and
89 install tests with one existing skip. Ordinary ABI, USB input, fork/exec/yield,
real USB persistence, ordinary capture and populated Home400-write-IOPS KVM
stress pass. The loaded trial completes21 app cycles and187 overlap windows,
zero recorded audio underruns,238.866ms maximum loop,1.238376s ready and
1.681271s close maxima. Its valid1280×800 screenshot takes9.679639s;63 checked
raw writes contain24 proved copies and39 ordinary calls, with no growth, error,
invalid or dropped evidence. Complete saved lifecycle/app/file-service records,
raw BMP and40-byte disk receipt, captured-prefix companions and all original
byte/retirement checks pass. This is VM evidence, not native timing or a causal
speed comparison.

Release `a1397d0b2e7ba0d1` is staged from that exact source with ports `39ad9d70`.
Its native-board-refusal/save/unmount/automatic-return VM rehearsal passes with
the exact manifest. Current hardware remainsd551/recoveryc354; no native reboot
is armed at this checkpoint. The next physical run retains the original1800s1%
audio/disk/app/three-screenshot workload,60s tail,500ms loop and10s lifecycle
requirements, all proofs/barriers and automatic collection. Native acceptance
and the earlier intermittent battery cause remain open. Refreshed PR301/PR19
heads0778cd1a/3afff099 retain separate graphics/network work; integration main
remainsc3550c82 before this PR's merge. Actions stay disabled.

## Native reserved-append result (2026-11-10)

Release `a1397d0b2e7ba0d1`, compiled from `51670ae6` with ports `39ad9d70`,
completes the original automatic 1800-second workload in
`session-00065fefd5a209b5-00`. Implementation PR458 is merged at `9580bb4b`;
its final follow-up changes documentation only. This accepts bounded native
composition of private reserved append, captured-prefix writeback and the
existing audio/disk/app/screenshot workload, with all original limits and
integrity checks retained.

The run completes 1,079 app cycles, 4,062 verified disk bursts, 1,064,828,928
disk bytes and 345,763,840 PCM bytes at 1% volume, with zero recorded audio
underruns or worker errors. Maximum UI loop, app ready and app close times are
121.468 ms, 1.893635 s and 5.371300 s, within the unchanged 500 ms/10 s limits.
All three screenshots save during PCM, a follow-up app opens, and ordinary exit,
cache retirement and worker cleanup finish. The bounded native logs retain the
original lifecycle summary and worst-lifetime proofs; they do not retain all
individual cycle/app-stage receipts. Subjective sound at 1% remains unassessed;
the earlier short 25% listening acceptance stays separate.

Each 1366×768 BMP has the exact 4,196,406-byte length and decodes fully. Capture
durations are 11.053331/15.913968/6.072230 s, median 11.053331 s, versus the prior
d551 run's 15.025137 s median. Different cold boots and interleavings, including
different completed disk/app counts, prevent a causal speed claim. Each capture
has 65 raw writes: 26/27/27 proved copies and 39/38/38 ordinary calls. Strict v2
partition, real ordinary gate counts and captured-prefix companions validate;
no growth, copy failure, invalid or dropped evidence is recorded. Active-append
dirty-survivor fault/ownership proofs remain host/VM evidence; retained native
selected-prefix records do not establish continuous coverage of every commit.

Write phases still take 5.581984/11.843483/3.781357 s, and ordinary storage
admission accounts for 94.24–97.89% of raw write wall time. The worst retained
gate is 2.301091 s. Its observed global-sync owner and the selected maximum
content commit are different intervals in this run; do not attribute that gate
to the selected content's callbacks. Remaining shared I/O admission is the next
performance target, without changing barriers, ownership or runtime limits.

All 814 prior files survive with unchanged bytes except expected filesystem
metadata; nine outputs bring the total to 823 files and 3,803 allocated clusters.
Independent fsck, full allocation traversal and actual USB hashes pass with zero
leaks, bitmap/FAT mismatches or errors. The raw 40-byte disk receipt and all 16
original saved-log contracts reproduce. Battery epoch1 retires after 705
attempts, 10,684 queries and 131 notifications, with zero heap, errors or denied
operations and a clean post-PCM first-protocol observation. This run does not
resolve the earlier intermittent battery failure; its original media survives.

Ubuntu default, consumed one-shot, installed a139/recoveryc354 and independent
payload/trial backups are verified. The collector is disabled, USB unmounted
and no reboot armed. No hardware or power-policy gate changes follow from this
result. Original failure evidence and other machines' acceptance stay separate.

## Active exact gate-correlation scope (2026-11-10)

Bounded objective: identify the actual sync/content operation behind each
retained screenshot gate owner, then reproduce and reduce demonstrated avoidable
admission work. Base is main `72d49457`; a139 is fully collected and accepted,
recovery remainsc354, and no new boot is armed. Scope is fixed diagnostic
snapshots in sync timing and scheduler gate observation, persistent workspace
formatting, strict versioned parsing and directed host/VM/native validation.
Open PR301/PR19 heads455ad5e1/2ed325a4 retain separate graphics/network scope;
no other-machine task is contacted.

Copy the live parent identity and contained active content counters at the
selected owner's exact release edge. Label this an in-flight observation, not
completed writeback or media timing. Do not substitute the unrelated global
content maximum, treat a missing match as zero work, or change admission,
ownership, copy/pipe/command caps, proofs, barriers, power policy or500ms/10s/
1800s+60s requirements. First checkpoints are exact binding/refusal and actual
producer/formatter/parser regressions, then a frozen matched build and loaded
VM. Native evidence is required before claiming the remaining cause or a gain.

The [exact gate observation](../capture-gate-correlation.md) now has a fixed
live reader, copied owner retention and mandatory versioned wire companions.
Focused actual scheduler/sync-producer composition and production formatter/
parser checks pass. The previous short VM's maximum owner is file-open, while
native a139 retains sync owners; preserve those distinct operations rather
than force matching to the global maximum. Full frozen integration and new
native observation are pending. No latency fix or new hardware acceptance is
claimed by this diagnostic-only candidate.

The initial b480 loaded VM completes its workload but fails required app-receipt
validation: PID19 has incomplete saved phases and interleaved UART fallback
records. Its failed artifacts are retained. Repeated new correlation rows add
1,336 bytes per populated periodic observation to the same bounded log. The
candidate now emits those retained contexts only at final and keeps periodic
owner output byte-equivalent to the legacy projection, with a directed
regression. No receipt repair, missing-data waiver, larger log or changed
workload/timing limit is accepted; the corrected frozen candidate needs a new
complete loaded run before merge or native preparation.

Corrected frozen source `62670020` passes matched images, full core/desktop/docs,
89 install tests with one expected skip, ordinary ABI/input/fork/exec/yield,
USB persistence, ordinary capture and loaded400-write-IOPS KVM validation. The
loaded run completes23 app cycles and175 overlap windows with zero recorded
audio underruns,181.267ms maximum loop,797.554ms ready and1.017712s close maxima.
All required app/file-service receipts are complete in saved logs; no UART
fallback is needed. The exact BMP and40-byte disk receipt and all16 original
contracts validate. Full-build image hashes match the already passed corrected
loaded run, so that exact run is reused rather than repeated.

The new live observation actually matches a selected SYS_SYNC owner to content
identity88, captured size2949120, whose start precedes the complete847.842ms
owner interval. Its cumulative callbacks through that edge total842.528ms,
including676.029ms reading2359296 prefix bytes. These are VM software callback
spans, not native hardware timing or completed writeback; the separate global
maximum is not substituted. Screenshot total is11.638868s. No latency gain is
claimed by this diagnostic change.

Stage `3403de90a056fca3` from that exact source passes the native-board-refusal,
save/unmount/automatic-return rehearsal. Native hardware remainsaccepteda139/
recoveryc354 and no new boot is armed at this checkpoint. Next is a source-bound
native observation at the original1800s1%,60s tail,500ms/10s limits and all
existing byte, barrier, ownership and retirement requirements. Report actual
parent/content coverage, including legitimate parent-only device-flush cases,
before choosing a change to the commit path.

## Native exact gate-correlation result (2026-11-10)

Release `3403de90a056fca3`, source `62670020`, completes the original1800s1%
PCM/disk/app stress with1055 app cycles,3836 verified262144-byte disk bursts,
1005584384 disk bytes,345780224 PCM bytes and zero recorded underruns. Maximum
desktop loop is120.567ms, app ready1.147130s and close3.789923s, within the
existing500ms/10s requirements. All three exact1366x768 BMPs save and decode.
Their total durations are9.990791/14.004349/8.644861s. This diagnostic candidate
does not establish a causal speed change from previous cold runs.

Separate local raw-log parsing and interval review pass all16 original
contracts and the new owner/context companions. Every retained longest
screenshot wait selects the background SYS_SYNC owner and has a valid enclosing
live content, state3. Admission waits account for95.55/97.23/96.54% of raw write
call wall time. The ordinary handlers total about1.4ms per screenshot. The
[detailed attribution](../capture-gate-correlation.md#native-air61-observation-2026-11-10)
records each capture's actual cumulative roles rather than substituting the
independent global content maximum.

Capture2 now exactly joins that completed maximum and its command observations
by parent/content identity and whole interval containment. Its1.595471s owner
scope observes1.470821s of payload-write callbacks and36.690ms of prefix reads.
The selected16KiB write spends359.848ms in USB status wait after1.038ms data
transfer; sampled ring service gaps reach20.058ms, with1us observed-completion
to caller resumption. This localizes delayed software-visible USB completion,
while the controller/device cause remains unresolved. Capture1 also spends
739.293ms in cumulative prefix reads, alongside403.267ms payload writes.
Capture3 observes1.171760s payload writes and10.535ms prefix reads. Each live
content begins73–80us before its owner scope. These cumulative callback wall
spans cover that content through owner release and are not exclusive CPU/media
measurements or a complete attribution of every wait.

The same completed capture2 content retains four qualified appends/262144 bytes
beyond its3145728-byte committed prefix, live size3407872, with dirty state
preserved. This supplies the native active-append dirty-survivor sample absent
from a139. Each screenshot makes25–27 qualified copies and38–40 ordinary
writes. The remaining delay is dominated by shared filesystem ownership across
long commits, with root payload completions and prefix verification contributing.
Worker receive/allocation spans stay271.578–333.039ms and final status
observation0.737–1.824ms. Capture retirement finishes within95.016ms.

All prior823 USB files survive, except the expected metadata update. The832
resulting files occupy3910 clusters with zero leaks, FAT/bitmap mismatches or
allocation errors. Direct read-only USB hashes agree with collection, all three
BMPs and the40-byte disk receipt validate, and battery/cache/capture/desktop
retirement and post-PCM first-protocol checks pass. Native lifecycle records
retain bounded tails and worst observations. Auditing was performed locally by
the root agent; complete individual-cycle trace coverage is not claimed. Audio
has zero recorded underruns at1%; subjective listening was unassessed for this
run. The earlier intermittent battery failure remains unresolved and its original
failed-media archive is verified and preserved.

Ubuntu default, consumed one-shot, disabled collector, unmounted USB and exact
installed/staged/independent payloads are verified. Recoveryc354, prior accepteda139
and historical acceptance backups remain protected. Cleanup removed19 full-hash
verified uninstalled stage duplicates and regenerable build outputs, leaving
about9GiB free without deleting raw evidence. No new reboot is armed. Next
bounded work is a deterministic replay of the matched sync/write-completion
overlap and an audit of exFAT commit ownership and repeated prefix work, with
all proofs, barriers, ownership, byte caps and runtime/latency limits retained.

## Matched-commit replay audit scope (2026-11-10)

Base is main `83c955fd`, following the reviewed3403 native observation. Bounded
objective is a process/VFS/exFAT replay of the matched2555904-byte committed
prefix,3145728-byte captured writeback and262144-byte concurrent suffix, then
an audit of transaction work and quiescent boundaries. A bounded Ubuntu
new-file reference on this unit's USB may help assess long write completion.
The local run record retains workspace identity, recovery and original hashes.
Scope is host replay tools and evidence guides until a relevant equivalent
reduction is demonstrated. Required data/metadata publication, prefix checks,
fresh readback, barriers, ownership and all existing byte/runtime limits remain
in force. Installed3403/recoveryc354 and original failed media stay protected.

Open PR301 at18f0ab6f and PR19 atbd1f2ee6 retain separate graphics/network work.
No other-machine task is contacted. The next checkpoint is measured source work
and failure/dirty-survivor equivalence for this captured boundary. An unchanged
work result alone supplies no performance justification for another native boot.

## Matched-commit audit result (2026-11-10)

The new [commit-boundary replay](../capture-gate-correlation.md#commit-boundary-replay-and-ubuntu-reference-2026-11-10)
matches the observed old/captured/live lengths and four concurrent64KiB appends
through real process/VFS/exFAT code. The fifth copy refuses at256KiB, committed
length stays3145728 and the extra262144 bytes remain dirty. A modeled359.848ms
data-callback delay preserves ordered source I/O, all eight selected-commit
flushes and the complete media hash. All65 writes, fresh temporary/final proofs,
rename guards, independent fsck and full allocation pass. The host turn/clock
are models, without USB controller or actual scheduler-wait timing.

The ownership audit found that current transaction scope includes required
proofs, data/allocation, namespace/sidecar publication and the final clean-marker
barrier. The existing single dirty-content visit and immutable snapshot already
avoid descriptor duplication and moving-target writeback. No equivalent
source-work reduction was demonstrated by this bounded replay. Native screenshot
latency remains unresolved; no runtime optimization is claimed by this result.

Bounded Ubuntu direct-file references add evidence about the same actual USB.
Sequential25616KiB writes peak at4.183ms. Two mixed references include64 cycles
of256KiB create/fsync/rename/fresh-read/unlink/volume-sync work and complete main
prefix readback. Their other-file syscall maxima are264.650/274.095ms, with3/2
calls above100ms. The second reference's USB trace has11430 fully matched
commands and zero dropped, pending or unmatched records. A512-byte metadata
WRITE(10) spends274.030ms awaiting status completion. Its16KiB payload commands
have at most19.523ms status waits. This proves long software-visible USB status
waits under Linux too, while differing command sizes and schedules leave the
native slow16KiB payload command cause open. Linux includes one larger metadata
command. Native caps and policies are unchanged; physical-packet/media/controller
timing is not established by either software trace.

All three Ubuntu references exclusively create bounded scratch files, verify
their exact direct readback and remove them. Before/after hashes match all832
prior files. Fsck and complete allocation retain3910 clusters with zero leaks,
FAT/bitmap mismatches or errors. USB is unmounted, Ubuntu remains default and
no reboot is armed. Current3403/recoveryc354 and independent accepted/failed
evidence remain protected. The temporary USB monitoring reader has exited.

The new regression, original reserved-append proofs and actual backend failure
cases, USB transport and xHCI host tests pass locally. Existing byte, ownership,
proof, barrier and runtime limits remain. This tools/evidence change adds no
kernel/userspace runtime behavior or new physical acceptance. Next performance
work needs a demonstrated equivalent reduction in commit work before another
native stress boot is justified.

## Active populated-save reduction scope (2026-11-10)

Base is main `7e8f70ae`. Extend the actual process/VFS/exFAT replay to the full
65-write save and foreign-barrier schedule with 832 seeded Home files and
changing timestamps. Account separately for payload, prefix, sidecar and flush
work. The first candidate under examination is unnecessary allocation-tail
padding in newly written internal metadata payloads on 128KiB clusters. Every
logical metadata byte, final sector padding, required comparison, publication
and barrier must remain correct, including failed/torn operations and recovery.
Ordinary file behavior and existing ownership, byte and runtime limits remain
required. No performance improvement or new hardware acceptance is claimed yet.

Touched scope is exFAT metadata payload emission, disposable populated replay
and fault fixtures, and this unit's current guides. Open PR301 at74920c2f and
PR19 at4e0ee3e4 retain separate graphics/network scope. No other-machine task is
contacted. Current3403/recoveryc354 and original failed media stay protected.
Next checkpoint is a measured equivalent reduction in the complete populated
save, with baseline/candidate logical media and failure comparisons.

The first [metadata-padding candidate](../exfat-metadata-tail.md) supplies an
explicit internal-metadata flag to the existing data writer. It writes every
serialized byte plus zeroed final-sector padding, preserving the allocation,
exact logical/valid lengths, publication and all barriers. Ordinary payloads
and directory initialization retain complete-cluster behavior. Existing byte,
ownership, proof and runtime limits remain required.

The full populated development replay now has832 initial files, persisted
timestamps, poisoned free space and time advances across all65 foreign barriers.
All65 screenshot-size writes, prior files, both full fresh proofs and complete
allocation pass. The candidate saves2770944 metadata-write bytes in unreserved/
reserved-sequential controls and1700352 bytes with controlled reserved overlap.
Logical files and all prefix/read/write-count/flush work match baseline. This
demonstrates a source-byte reduction. Native performance remains pending.

Frozen implementation `cc4f9278`, based on main `7e8f70ae`, passes matched
normal/native/VM builds, full core/desktop/docs checks, ordinary ABI/input/USB
and capture VMs, and the populated Home400writeIOPS KVM audio/disk/apps/
screenshot trial. Independent saved-receipt checks retain every original
contract. The source/image hashes remain fixed throughout validation. Stage
`c95f851e735b98ff` passes the exact native board-refusal/save/unmount/return VM
rehearsal; it has no native hardware acceptance yet.

All132 metadata-publication fault points match baseline logical contents,
protected files, exits and pre/post-repair allocation results. The72 inherited
strict-allocation failures after repair remain incomplete evidence, not a clean
recovery claim. Raw and repaired images are retained locally. Source changes do
not alter ownership, integrity checks, flushes, byte caps or timing requirements.

At integration review, PR301 `6d127542` remains separate graphics work with no
exFAT writer edit; PR19 `6ca94b59` changes metadata import/restore in the same
file, with distinct hunks and no competing payload-padding implementation or
format change. Neither is a dependency, and no other-machine task is contacted.
Next checkpoint is the automated1800-second1% native workload plus60-second
tail, three exact screenshot receipts, continuous app/disk work, clean battery
retirement and full USB preservation/allocation checks. Preserve current3403,
recoveryc354, priora139 and the original failed-media evidence. Different cold
boot timing is observational; it does not isolate the controller/media cause.

## Metadata-tail native return (2026-11-10)

Run `cb6d24eadf834d66`, release `c95f851e735b98ff`, compiled `cc4f9278`
(implementation PR463, merge `6b185086`) completed session
`session-00065ff856a28876-00` and returned to Ubuntu automatically. This unit's
bounded functional regression passes independent raw-log/binary parsing of all16
original contracts. The1800-second1% workload records3901 verified256KiB disk
bursts (1022623744 bytes),345763840 PCM bytes, zero recorded audio underruns,
1076 app cycles,121478us worst loop,2215821us launch and3018940us close. All
original caps and runtime/latency requirements remain unchanged. Subjective
listening is unassessed at1%; earlier audible playback acceptance is separate.

All three exact BMPs decode, their hashes and40-byte disk receipt match the
actual USB, and capture/cache/battery/desktop retirement plus normal exit pass.
Native logs retain bounded app-stage tails and complete summaries/worst records;
this does not claim all1076 individual lifecycle or service receipts survived.

Screenshot totals are8.824182/14.902387/15.328993 seconds. Prior3403 totals
were9.990791/14.004349/8.644861. The metadata byte reduction is real in the
controlled replay, but this native run demonstrates no consistent speed gain.
Shared I/O admission remains95.71/97.02/98.49% of raw write-call wall. Capture1
selects `SYS_OPEN_MODE` ownership with no live sync context; captures2/3 have
exact live sync/content observations. Capture3 also joins the selected completed
content and command by full parent/content identity and interval containment.

That owner scope lasts2.918074 seconds; cumulative payload-write callbacks take
2.816760 seconds, versus19.927ms for110880 metadata-payload bytes and23.885ms
for1638400 prefix-read bytes. Its16KiB WRITE(10) status wait is354.680ms,
with20.053ms maximum sampled service gap. These are software wall observations,
not physical media timing or exclusive CPU costs. Only retained longest owners
are correlated. This selected content has equal live/committed2162688-byte
lengths with zero concurrent appends, unlike the earlier dirty-suffix observation;
do not transfer the earlier append distribution to this run. Capture2's checked
rename also takes2.655208 seconds. Cleanup peaks at62.800ms.

Direct read-only rehash matches all841 current files; all832 prior files remain
unchanged except the expected metadata sidecar. Independent fsck and complete
allocation find4017 clusters with zero leaks, FAT/bitmap mismatches or errors.
Currentc95f, recoveryc354, prior3403/a139 and independent accepted/failed evidence
are verified. Only the fully backed, unreferenced historicaleb3 EFI duplicate
was retired for install space; its independent payload remains intact. Ubuntu
remains default, the one-shot is consumed, collection is disabled and USB is
unmounted. The earlier intermittent battery fault remains unresolved; its
original failed-media archive is verified, with no recurrence in this run.

The next bounded target is payload USB completion and shared commit ownership,
including the open/rename waits. A source-equivalent reduction needs evidence
without weakening integrity, durability, ownership, byte caps or timing limits.
No new reboot is armed during this return review. Main `6b185086` is the actual
review base; open PR301 `e0d68e53` and PR19 `e107870f` retain their independent
graphics/network scopes. This return change touches only this unit record and
the metadata-tail guide; no shared runtime code or other-machine acceptance.

## Active queued-append admission scope (2026-11-11)

Base is actual main `3703eaea`, with reviewed c95f native results collected.
Investigate the full-window raw-entry fallback: a queued private writer can
miss a later captured-prefix advance and remain excluded through the complete
foreign USB transaction despite a newly valid bounded RAM suffix. Scope is
process/syscall/scheduler notification and revalidation, fixed diagnostic
companions, host regression and this unit's guides. No xHCI/BOT transfer policy,
hardware timeout, command size, proof, flush, ownership or window limit changes
are authorized by this mechanism. Long device-status cause remains unresolved.

Open PR301 `0be2519c` changes separate process allocation/lifetime hunks; PR19
`80518bb9` changes network/xHCI control and process spawn/pipe/metadata-import
hunks. Neither supplies a competing reserved-append retry. Preserve those scopes;
no other-machine task is contacted. Next checkpoint is a reproducible missed
window and safe notified retry with all original negative/ownership tests,
matched build/VM validation and an automated1% native stress observation.
Currentc95f/recoveryc354, all841 USB files and original failed evidence remain
protected. This active scope is not a new physical acceptance or speed claim.

The missed-window regression now passes with the unchanged64KiB copy and
256KiB suffix limits, four copies under a live immutable prefix and a refused
fifth. Spurious notifications, owner completion, cancellation, FD-generation or
lease changes, failed content and owner/image/sibling/CR3 exclusions retain
safe behavior. A real-scheduler fixture blocks/wakes the private prelude while
preserving foreign scope depth, ordinary exclusion and quiescent fallback turns.
The real borrow publication/retirement notifications and fresh user-memory
validation after a wait are also exercised. No USB/BOT/xHCI policy changes.

Final freeze `b11db747` contains runtime implementation `8462addd` plus only a
host formatter-test correction. The initial full core run passes233 of239
targets before finding the test's periodic projection lacked the new final-only
companion. The correction requires strict companion parsing and unchanged
periodic bytes; the corrected target, last six core targets and desktop/docs
pass against identical production sources. Matched builds and source/image
hashes are checked; initial failure and source-equivalence records are retained.

Ordinary ABI/input/thread/USB/capture VMs and the populated400writeIOPS KVM
audio/disk/apps/screenshot trial pass. Independent reparse retains all16 original
contracts and complete saved lifecycle/app/service evidence. Four queued retries
become ready while a foreign owner and live writeback borrow remain active.
The loop maximum is139940us; launch/close maxima are1289420/1152100us. These
are VM observations, not a native speed comparison. The new final-only wait
companion exposes all private-prelude wait wall, including eventual fallback;
no synthetic owner-release or zero-gate observation is fabricated.

Stage `bcd7246fbcc828f6` passes exact native board refusal, save, unmount and
return. The next native workload remains automatic1800-second1% PCM/disk/apps,
three exact screenshots and original60-second tail, followed by full receipt,
retirement and841-prior-file/allocation checks. Native overlap coverage and
latency remain pending; long device/controller status cause remains unresolved.
At final scope review, main remains `3703eaea`; PR301 `4e76b63d` and PR19
`835ffb01` edit distinct lifetime/spawn/pipe/network syscall hunks, with no
competing reserved-append recheck. Currentc95f/recoveryc354 and prior3403/a139
remain protected. No other-machine task is contacted.

## Queued-append native return (2026-11-11)

Run `428a58c610984577`, release `bcd7246fbcc828f6`, compiled `b11db747`
(runtime `8462addd`, implementation PR465 merge `d4580da3`) completed session
`session-00065ffa3ebd03ab-00` and returned to Ubuntu automatically. Independent
raw-log/binary parsing passes all16 original contracts and the required queued
wait companion. The1800-second1% workload records3707 verified256KiB bursts
(971767808 bytes),345731072 PCM bytes, zero recorded underruns and1066 app
cycles. Worst loop is121522us, launch1093289us and close1889203us. All original
limits, fresh proofs, barriers and ownership checks remain unchanged. Subjective
listening is unassessed at1%; prior audible playback acceptance is separate.

All three exact BMPs decode and their hashes plus the40-byte disk receipt match
actual USB bytes. Capture/cache/battery/desktop retirement and normal exit pass.
Native lifecycle logs retain bounded tails, complete summaries and worst records;
this does not claim every individual cycle/service receipt survived.

The notified retry path is exercised4/6/6 times while a foreign owner and live
immutable borrow remain active. Screenshot totals are7.479590/7.281154/
10.530490 seconds, versus8.824182/14.902387/15.328993 in the prior cold run.
This accepts bounded native retry functionality and records lower observed
times; different boots/interleavings do not establish isolated causality.

Ordinary admission totals are187/167/185us and their worst samples2/2/3us,
with no retained owner or live sync context. Private-prelude wait totals are
4.277356/3.636174/6.719198 seconds,96.44/96.72/97.83% of raw write-call wall;
the slowest private wait is1.797150 seconds. Remaining waiting is recorded in
the explicit private prelude; tiny ordinary gates do not show its elimination.
Successful private wait wall overlaps
handler time; fallback prelude wall remains in whole-call residual. Do not sum
these as exclusive phases or invent ordinary owner records for them. Cleanup
peaks at71.016ms. Capture3's checked rename still takes1.374035 seconds.

The separately bound selected global content call lasts1.796672 seconds, with
1.667766 seconds payload-write callback wall,41.861ms prefix reads and24.163ms
metadata-payload writes. Its exact16KiB WRITE(10) status callback takes354.113ms
with19.352ms maximum sampled service gap. This completed global command is not
an exact join to any individual private wait: those companions retain aggregate
and worst wall without per-wait timestamps/owner identity. Source/controller/
media cause remains open. The selected captured3932160-byte prefix commits,
while four appends/262144 bytes produce live4194304 and dirty-after state;
the original generation/immutable-prefix/window proof is preserved.

Read-only rehash matches all850 current files. All841 prior files remain
unchanged except the expected metadata sidecar. Independent fsck and complete
allocation find4124 clusters with zero leaks, FAT/bitmap mismatches or errors.
Installed/staged/independent payloads, recoveryc354 and priorc95f/3403/a139 are
verified. Only the fully backed, unreferenced historicald551 EFI duplicate was
retired for install space; its independent full payload remains intact. The
original failed battery media archive is verified; its intermittent cause stays
unresolved, with no recurrence captured in this run. Ubuntu remains default,
the one-shot is consumed, collection is disabled and USB is unmounted.

Next target remains long payload completion and shared commit work, using
matched command-size/order evidence before further policy changes. Do not infer
that tiny ordinary gates mean no contention or use unrelated global maxima as
private-wait attribution. No new reboot is armed during this result review.
Actual review base is main `d4580da3`; open PR301 `72efc460` and PR19
`3d8a9c2a` retain separate graphics/network scopes. This result change touches
only this unit record and the reserved-append guide; no runtime code or other
machine acceptance changes.

## Active payload placement and commit scope (2026-11-11)

Base is actual main `ae869c9b`, after the reviewed bcd native return. Investigate
remaining USB status latency with matched16KiB payload commands and bounded
scratch-file address-order controls, then audit repeated allocation/commit work
for a validated reduction. Current allocation searches from bitmap index zero
for each request; repeated reuse is a hypothesis, not an established media
cause. Keep every allocation proof, prefix/fresh readback, publication, flush,
ownership check and byte/runtime limit. No transport timeout, queued BOT command,
retry, DMA or power-policy relaxation is in scope.

Touched scope is disposable reference/replay tools, a narrowly justified shared
filesystem/transport fix if supported, and this unit's guides. Open PR301
`c1220fe9` has no storage hunk; PR19 `acc17976` owns separate xHCI control/network
and metadata-import hunks, with no allocator change. Preserve those scopes and
avoid competing implementation. No other-machine task is contacted. Currentbcd,
recoveryc354, priorc95f/3403/a139, all850 USB files and original failed evidence
remain protected. Next checkpoint is measured command/address-order behavior
and a regression-backed equivalent reduction before the next1% native stress.

The bounded Ubuntu address-order reference preserves all850 files and clean
4124-cluster allocation. Its1024 matched16KiB writes include25 status waits
above100ms in one rotating phase, peaking349.147ms; both hot phases stay below
1.2ms status and the other rotating phase below17.1ms. Complete CBW/data/CSW
pairing has zero pending/unmatched/lost events. This reproduces the long wait
outside SrvrOS with the same command size; it does not establish media versus
controller cause, or support a rotating allocation policy. No allocation order
change is planned from this evidence.

Implementation scope narrows to unnecessary FAT work for contiguous NoFatChain
streams. The format declares those FAT entries invalid; bitmap ownership and
exact contiguous extent remain authoritative. Defer ignored FAT initialization
and clearing, and materialize every required link plus the final terminator
before any transition to a real FAT chain. Direct directory/system allocation
must retain required chain initialization. Existing validators, prefix/full
proofs, allocation counts, publication ordering and every barrier stay in place.
Next checkpoint is poisoned/stale-FAT and conversion/fault regression plus
measured complete-save command/source-work reduction. No device-latency fix
or native acceptance is claimed by the reference alone.


The implementation now defers only invalid NoFatChain FAT initialization/clear.
Required FAT-backed conversion explicitly writes all links and terminal before
publication, including a single-cluster suffix; directory/system and fragmented
allocation retain initialization. Poisoned FAT, exact bitmap/neighbour checks,
512/4096-byte geometry, OOM and required-write quarantine pass. The original
append replay passes131 remaining cuts with unchanged prefix/barrier/negative
paths and the81 inherited allocation-repair gaps still reported as failures.

Complete populated process/VFS/exFAT screenshot controls preserve all65 writes,
65 foreign syncs, fresh proofs, logical files, source reads and flushes. Eager
versus lazy FAT saves393 callbacks/1572 bytes in sequential controls and304/
1240 under overlap. Required terminal work moves into append preparation; this
is a complete-save reduction, not a claim every phase decreases. These are
source callbacks rather than physical command counts or native speed evidence.
Next checkpoint is frozen matched builds/core/VM validation, self-merge and
an unchanged automated1800-second1% native stress with all prior receipts.


Final freeze `72567e32` passes matched ordinary/native/VM kernels and userspace,
all240 default core targets, desktop and docs checks. The89-case installation
set has its existing single default-off exhaustive host-I/O matrix skip; no new
skip is introduced. Metadata-tail comparison now has114 actual fault cuts and63
matched inherited incomplete repairs; omitted invalid FAT writes account for
removed cut points, not a general recovery improvement. Both append controls
retain131 cuts and81 known incomplete repairs. All validators remain strict.

Ordinary ABI/input/thread/USB/capture VMs pass. The populated400writeIOPS KVM
trial passes all16 original contracts and the required append-wait companion,
with complete saved app timing,22 app cycles,141368us maximum loop,1260211us
launch and1357945us close. Six queued waits become ready under a live borrow.
The ordinary gate's exact completed-content match count is zero in this VM;
unavailable contexts remain explicit, not fabricated positive attribution.
The deterministic whole-save controls establish reduced source work; VM wall
time is not native performance acceptance.

Stage `c7d2293b66b7999b` passes exact board refusal, save, unmount and automatic
return. The next native test retains1800-second1% PCM/disk/app stress, three exact
screenshots,60-second tail, all timing/proof/retirement contracts and850-file
preservation plus fsck/full allocation. Native performance remains pending.
Actual integration base remains main `ae869c9b`; PR301 `25e6219c` has no storage
hunk and PR19 `3da01075` retains distinct metadata-import/scheduler-test hunks.
Actions remain disabled. Currentbcd, recoveryc354, priorc95f/3403/a139 and all
independent historical payload/evidence backups remain protected.


## Lazy-FAT native return (2026-11-11)

Run `7f38790cee114722`, release `c7d2293b66b7999b`, compiled `72567e32`
(implementation PR467 merge `52907f8b`), completed session
`session-00065ffc821878bb-00` and returned to Ubuntu automatically. Independent
raw-log/binary parsing passes all16 original contracts and the required queued
wait companion. The1800-second1% trial records3954 verified256KiB bursts
(1036517376 bytes),345862144 PCM bytes, zero recorded underruns and1089 app
cycles. Maximum loop is122325us, launch3391486us and close2460649us, within the
unchanged500ms/10s limits. All three exact BMPs decode and match actual USB
hashes; the40-byte workload receipt, proofs and capture/cache/battery/desktop
retirement pass. Native logs retain bounded lifecycle tails and worst records,
not every individual cycle. Subjective sound remains unassessed at1%; previous
audible acceptance is separate. Earlier intermittent battery fault cause remains
unresolved, with no recurrence captured in this session.

This accepts the bounded lazy-FAT functional path, not a screenshot speed fix.
Screenshot totals are8.049986/9.633137/17.522316 seconds, versus7.479590/
7.281154/10.530490 in the preceding bcd run. Different cold boots, workload
density and interleavings do not isolate causality, but this observation is
slower overall. The deterministic source callback reduction remains valid;
removing invalid FAT work did not eliminate native completion tails.

Queued readiness is exercised6/6/7 times under a live borrow. Ordinary admission
sums165/181/169us, with worst waits2/2/3us and no attributed owner/context.
Private-prelude waits total4.393120/4.918529/14.548418 seconds,96.36/96.25/
98.90% of raw write-call wall. The largest private wait is3.588768 seconds.
These waits overlap handler time for successful copies; fallback prelude wall
lies in whole-call residual. Do not add these as exclusive phases or assign an
unrelated global maximum as the exact owner of an individual private wait.

A separately bound global content call during capture3 lasts3.584277 seconds.
Payload write callbacks take3.117253 seconds; allocation read/write callbacks
366737us; prefix reads36857us; metadata-payload read/write29691us. Eight of its
forty16KiB payload status callbacks exceed100ms, with2.823784 seconds total
status wall and a352.037ms maximum. Its selected slowest status callback instead
belongs to a512-byte allocation-phase WRITE(10), lasting355.361ms with14.669ms
maximum sampled service gap. Long status tails therefore occur in both sampled
command sizes. Software clocks still do not distinguish media/firmware from
controller delivery. No command-to-private-wait identity is retained. The exact
3473408-byte captured prefix commits while four appends totalling262144 bytes leave
live3735552 and dirty-after state; the original immutable-prefix proof remains.
Cleanup peaks at66.079ms; capture2's checked rename takes1.891898 seconds.

Read-only rehash verifies all859 actual files. All850 prior files remain
unchanged except the expected metadata sidecar. Independent fsck and full
allocation report4231 clusters, zero leaks, FAT/bitmap mismatches or errors.
Installed/staged/independent c7 payload, recoveryc354 and priorbcd/c95f/3403/a139/
2dd payloads are verified. Only the fully backed, unreferenced historical8caa
EFI duplicate was retired for install space; its complete independent payload
remains verified. The original failed-media archive and returned trial backup
are intact. Ubuntu remains default, the one-shot is consumed, collection is
disabled and USB is unmounted. No new reboot is armed.

The next performance target remains long USB completion tails and their exact
shared-commit ownership, rather than screenshot transfer/cleanup. Preserve all
proofs, barriers, ownership, command caps and runtime limits. A bounded512B versus
16KiB reference and exact per-wait owner/command attribution would separate the
remaining hypotheses; neither is implemented or accepted by this return review.
Actual review base is main `52907f8b`; PR301 `25e6219c`, PR19 `3da01075` and
PR379 retain independent scopes. This review touches only this unit record and
the contiguous-allocation guide; no runtime or other-machine acceptance changes.


## Active filesystem performance passes (2026-11-11)

Actual base is main `5e72e2dd`, with the c7 native return independently reviewed
and backed. The user requests several filesystem read/write performance passes.
Scope is shared exFAT allocation/bitmap reads, FAT-chain traversal and bounded
stream append/read, with block-layer work only if a measured defect requires it.
First reproduce the bounded-stream writer's dependency on eager FAT terminal
initialization, then audit unnecessary bitmap slack reads and transaction-cache
churn from prefix payload reads. Preserve all allocation/alias/readback proofs,
publication ordering, barriers, quarantine, lease ownership and existing cache,
transfer, byte/runtime limits. No USB timeout/command/power-policy change.

PR301 `25e6219c` has no storage hunk; PR19 `882f64fc` changes separate metadata
import and scheduler-test hunks, with no allocator/read-path implementation.
PR379 remains independent design work. No other-machine task is contacted.
Currentc7/recoveryc354, all859 USB files and outside-build accepted backups stay
protected. Native source optimization/performance claims require matching new
evidence; no new reboot is armed by this scope notice. Next checkpoint is a
regression-backed stream correction and measured, independently attributed
read/write reductions before broader validation.


Three filesystem passes are implemented against `5e72e2dd`. Pass1 (`9327360c`)
reproduces/corrects the separate bounded-stream FAT-terminal dependency and
retains eligible contiguous stream extents with all publication flushes. Full
stream tests, independent fsck and poisoned-FAT/forced-chain controls pass.
Pass2 (`e386f11a`) clips bitmap reads/scratch to declared bits rounded to the
existing device block; first-fit order and every allocation check/write remain.
Pass3 keeps512-byte FAT windows only inside the existing transaction cache for
large clusters, with fresh full prefix reads bypassing that metadata cache.
Small clusters retain the original entry path to avoid a measured cache-touch
regression. Cache capacities, command/stream limits and timeouts are unchanged.

The actual filesystem-plus-block-cache comparison has nine passing cases across
4/128KiB clusters and512/4096-byte devices. On populated128KiB/512-byte growth,
device reads fall12868→11755→10825 and bytes179819520→162649088→162172928.
Replacements fall514→363 reads; metadata rounds642→434. Every complete image,
logical file map, device write count/bytes and flush count matches. This is host
work reduction, not native timing; synthetic bitmap sizes differ from this USB.
The new [performance contract](../exfat-io-performance.md) records the controls.
Next checkpoint is frozen matched build/core/desktop/VM validation before merge.
Current installedc7/recoveryc354 and all859 USB files remain untouched; no new
native speed acceptance or reboot is implied by these host measurements.

Frozen runtime `e6d06f2a` now passes matched builds, all 244 default core targets,
desktop/docs, stream-manager/syscall/process cleanup, ordinary ABI/input/thread
VMs, real USB transport and ordinary capture. Populated Home KVM at400 write
IOPS passes all original timing/ownership/content/command/capture contracts,
including complete saved app records and the post-playback battery observation:
60 seconds,231 bursts/60,555,264 bytes,24 app cycles and zero audio underruns at1%.
Exact-stage release `32afb62a63264174` passes foreign-board refusal/save/unmount/
return in UEFI; it has not been installed or physically accepted.

The first loaded VM is retained as an evidence failure: two adjacent64KiB tails
left a607-byte gap containing a required begin receipt, and UART interleaved.
The unchanged retry passed; no inferred records, parser waiver, logger cadence,
workload or bound changed. Complete coverage on every future run remains unproven.
Metadata-tail114-cut and append131-cut controls retain their63/81 inherited
incomplete-repair cases; these optimizations do not claim interruption recovery.

Final review corrects only the host full-bitmap-write control to retain full
initialized scratch. Its five original exact-media/write/flush comparisons pass;
preprocessed production/default-host/other measured control tokens remain
identical to the frozen runtime. Latest main remains `5e72e2dd`; PR19 `58a48e04`
retains separate metadata-import hunks, and PR301 `037510a8` has no exFAT hunk.
Shared performance changes are in PR469. Installed c7, recovery c354, the859-file
USB baseline and independent accepted backups remain unchanged. Native timing
is the next acceptance step; host callback savings are not a native speed claim.

## Native filesystem comparison preparation (2026-11-11)

The user authorizes installing and automatically starting the merged PR469
filesystem candidate. Integration base is `883f79bf`; only this unit record and
local trial tooling change in this preparation. Rebuild matched images, verify
the focused filesystem controls, run populated Home stress and the exact native
stage in VMs, then install with recovery c354 retained and Ubuntu default.
The native profile keeps the original 1800-second workload, three screenshots,
app open/close cycles, 1% volume and all limits, allocation/byte proofs, durability
barriers and ownership requirements. No input is required; progress and results
are displayed/saved in the guest. Automatic collection must bind this release
and new session before another trial.

Compare screenshot wall/phase/cleanup, app lifecycle and storage observations
with the accepted c7 session `session-00065ffc821878bb-00` (runtime `72567e32`):
capture totals 8.049986/9.633137/17.522316 seconds, 1089 app cycles, 122325us
maximum loop and zero recorded underruns. Different cold boots/interleavings
make this an observational native comparison; the deterministic callback
controls remain separate evidence. Current c7 and recovery c354 stay protected,
with outside-build evidence and payload backups. EFI space may require retiring
one verified, unreferenced historical duplicate; its complete independent copy
must remain. No runtime change or sibling acceptance is part of this trial.

Candidate `80b4e32ef314ae05`, built from clean `820eb85c`, passes the fresh
stream-allocation, bitmap, FAT-window, actual filesystem/block-cache, stream
service and documentation checks. Populated Home KVM at400 write IOPS passes
all original contracts with complete saved app records, screenshot readback,
224 bursts and zero recorded audio underruns at1%. Independent saved-record
reviews and the exact-stage UEFI refusal/save/unmount/return also pass. The
earlier244-target core run is retained with explicit production/default-host
token equivalence through the host-only bitmap-control correction; no runtime
code changes were added in this preparation.

The candidate is installed with c354 retained, c7 still verified in EFI and
independent candidate/current/recovery backups. All859 prior USB files verify;
only the1% volume override expiry was renewed. Offline fsck, full allocation
and unchanged headroom checks pass, as do Ubuntu default and unchanged firmware
BootOrder/no BootNext. Only historical2dd's unreferenced EFI duplicate was
retired after exact full-tree backup verification; its independent payload
remains. Automatic matching-session collection is enabled. The local run record
will record the one-shot arm immediately before the authorized reboot. Native
results and screenshot performance acceptance remain pending that return.

## Native filesystem comparison return (2026-11-11)

Collected run `347434d70c5c44a5`, release `80b4e32ef314ae05`, compiled `820eb85c`
(merged PR469 runtime with host-only comparator correction and own-unit docs),
session `session-0006600400f302c0-00`. The original1800-second1% automatic workload
passes all16 independently reparsed contracts plus the append-wait companion:
3895 verified256KiB bursts/1,021,050,880 bytes,345,763,840 PCM bytes, zero recorded
underruns,1071 app cycles,120687us maximum loop and no excursions at unchanged
500ms/10s limits. All three exact1366x768 BMPs, final proofs, ownership/cache,
battery and normal exit/return checks pass. Native bounded tails/worst receipts
do not retain every individual cycle; no subjective1% sound claim is made.

Screenshot totals are7.307485/10.890831/12.560273 seconds versus c7's
8.049986/9.633137/17.522316. The worst improves28.3%, while the middle worsens13.1%.
WRITE phases are4.378074/6.668082/9.959347 seconds. Different cold boots, media
state and interleaving make this observational; aggregate burst count is lower
than c7's3954, and an isolated sustained speedup is unproved. One native allocation
sample now reads59392 bitmap bytes versus131072 previously. Generic bounded
stream admission remains64KiB; that path's host coverage is separate on this
128KiB-cluster USB.

Private prelude waits still dominate95.69/97.50/98.45% of raw write call wall,
with3.615494-second maximum and17 actual ready-under-live-borrow rechecks. Ordinary
gates total180/165/183us with no retained owner. The separately selected1MiB
global content call during capture3 takes3.614946 seconds, including3.532703
seconds of payload callbacks. Forty16KiB payload status waits total3.188556
seconds; nine exceed100ms and the maximum is353518us. Service gaps peak10069us.
Its512-byte allocation status maximum is1906us. There is no exact per-private-
wait owner/command join, and these software clocks do not establish a device,
firmware or controller cause. Long completion tails remain open.

Fresh read-only actual USB hashes preserve all859 prior files except the expected
metadata sidecar. Nine new files bring totals to868 files/4338 allocated clusters;
fsck and full allocation report zero leaks, mismatches or errors. Installed/staged/
independent80b4, recoveryc354 and retained c7 verify, as do retired2dd's independent
full tree, the original failed-media archive and the matching trial backup.
Ubuntu remains default, one-shot consumed, collector disabled and USB unmounted.
No new reboot is armed. This accepts the bounded native combined filesystem
profile; performance remains mixed and the earlier battery-fault cause remains
unresolved, with no recurrence captured here. Review base is main `91514b3f`;
this result update changes only this unit record and the filesystem performance
guide, with no runtime or other-machine acceptance change.

## Active commit-wait isolation (2026-11-11)

From main `a611fd9e`, isolate the missing association between private screenshot
waits and global commits. Scope is observation hooks in the reserved-append wait,
fixed sync observer state, a compact final-only logger companion, strict parser
and regressions. Retain the completed content call with the longest actual
overlap inside the worst screenshot wait, including its parent sync identity
and command timing. Report unavailable/incomplete overlap explicitly; whole-call
USB counters are not exclusive work inside the wait or proof of hardware cause.
Only one selected wait is emitted to preserve the existing64KiB tail budget.

PR19's process lifetime/pipe work is separate from the reserved-append wait;
there is no competing sync observer implementation. No other task is contacted.
Keep scheduler eligibility, notification/polling policy, ownership, durability,
all proofs and command/cache/byte/runtime limits unchanged. Validate disabled,
overlapping/partial/unavailable, repeated, failed, overflow and retirement cases,
plus actual C producer-to-parser wiring and loaded/exact-stage VM evidence.
Then retain accepted80b4/c354 and run the original1800-second1% automatic stress
with three screenshots. Native acceptance and causal diagnosis remain pending
matching collection; no new reboot is armed by this scope notice.


The completed-content-only prototype returned a valid no-overlap result for the
loaded VM's worst wait, despite passing every original workload contract. Its
source `c2bc6f92` and matching evidence are retained. The revised scope also
copies existing scheduler owner diagnostic edges and already-timed USB status
samples, including kernel owners without SYS_SYNC. Scheduler admission/release
and USB timing/polling/transfer logic remain unchanged; these are metadata sinks
with no extra device operations. Version2 reports the longest actual
owner overlap, partial live ownership and observed status callbacks separately
from optional completed content. The original fixed tail/cadence and all16
contracts remain required. Actual real-scheduler/USB forwarding fixtures and
loaded execution precede native installation.


The first version2 owner run identifies PID7/SYS_UNLINK holding the selected I/O
scope for879761us, with no status sample because unlink was outside the old
sync/logger timing windows. That run's missing app-stage receipts remain a
strict evidence failure, separately preserved. The final candidate extends the
existing read-only timestamp/cached-result path to otherwise untimed status
callbacks inside observed owners. Already-timed callbacks still share one
sample; polling/event reads, request count, commands, deadlines and every
integrity/ownership check remain unchanged. Host controls verify the disabled
clock count, status-only window and shared-sample behavior before a fresh freeze.

## Commit-wait isolation validation and native preparation (2026-11-11)

Runtime `4f4feeeb` is based on main `a611fd9e` and published through PR472.
Refreshed PR19 at `ef9b60df` and PR301 at `eb938961` have separate process/logger
hunks; neither owns these reserved-wait, sync-observer, scheduler-diagnostic or
USB status hooks. No other task was contacted. Policy, deadlines, scheduler
eligibility/handoff, filesystem proofs, flushes, quarantine and all existing
workload/integrity limits are unchanged. Otherwise untimed status callbacks gain
opt-in timestamp observations and one cached sample, without new controller/ring
reads, transfers or polling decisions. See [the diagnostic contract](../commit-wait-isolation.md).

The full 245-target core suite, desktop/docs checks, ordinary ABI/input/thread
VMs, USB read/write/reload, ordinary screenshot VM and exact-stage rehearsal
passed at `9ade1c0f`. A subsequent diagnostic-only clock-edge correction at
`4f4feeeb` rejects a regressed callback ending before wait entry instead of
mistaking it for unavailable overlap. Its actual C regression fails before the
fix and passes afterward with both IRQ states and unchanged clocks/ownership.
The affected observer, USB, scheduler, sync, reserved-append, capture, logger and
media-integration tests passed again, followed by rebuilt matched ordinary/native/
fixture images, a loaded VM run and exact-stage rehearsal. Full-core evidence
remains explicitly attributed to `9ade1c0f`; it is not relabeled as a final-source
full-suite run.

The final 60-second 1% KVM workload completes 214 verified disk bursts, 23 app
cycles and an exact screenshot, with zero recorded underruns and a 140.084 ms
maximum loop. Maximum app ready/close times are 1.120230/1.516421 seconds within
the original 10-second limits. Every original timing/integrity contract and the
append-wait companion passes. Where used, the original strict union of whole
console records and retained atomic checkpoints is labeled explicitly; no trace
repair or saved-only acceptance is invented. Earlier prototype unavailable
coverage and the owner-only attempt's missing app receipts remain preserved
as incomplete/failed evidence.

The exact worst wait is 1.505666 seconds. Its selected PID7/thread18
`SYS_OPEN_MODE` owner spans 1.525696 seconds, covers 1.505621 seconds of that wait
and leaves 45 us uncovered. Fifteen status callbacks total 2.970 ms, with no
100 ms callbacks. These are virtual-clock observations under the existing
400-IOPS load, not the hardware's long status tails or exclusive CPU time.
The native trial will retain actual owner/status intervals rather than infer a
join from a separately selected global content maximum. Optional completed
content and partial/unavailable ownership remain explicit.

Release `4598c7e5aac15a6b`, run `e42faf938d164a49`, is installed with matching
staged/EFI/independent payload hashes and the exact capture-write desktop profile.
The existing 868-file USB baseline is verified; only its expiring 1% volume
preference is renewed. Offline fsck/full allocation/headroom checks pass.
Recovery `c35475f62f154f1e` and accepted `80b4`/`c7` remain verified; only the fully
backed-up, unreferenced `a139` EFI duplicate was retired, preserving its complete
independent payload. Ubuntu remains the default, firmware BootOrder is unchanged
and no BootNext or GRUB one-shot was set at the preparation checkpoint.

The automated 1,800-second audio/disk/app test retains three screenshots,
500 ms loop and 10-second app limits, every byte/CRC/fresh-media/allocation proof,
and clean battery/worker retirement. The enabled return collector additionally
requires the version2 wait/owner/status companion alongside all original receipts.
Post-merge arming and reboot state are saved in the independent local run record.
Native acceptance and the cause of long USB completions remain pending matching
return; the earlier intermittent battery transport failure remains unresolved.

## Native commit-wait isolation return (2026-11-11)

Release `4598c7e5aac15a6b`, runtime `4f4feeeb`, completes run
`e42faf938d164a49` in session `session-00066006bde6accb-00` and returns to Ubuntu
automatically. Independent reparse of the matching raw logs and binary receipts
passes all 16 original contracts, the original append-wait companion and the
new version2 owner/status companion. The 1,800-second 1% workload completes
4,062 verified disk bursts (1,064,828,928 bytes), 345,747,456 PCM bytes, zero
recorded underruns and 1,088 app cycles. Maximum loop is 127.664 ms, ready/close
3.505205/7.308982 seconds, within the unchanged 500 ms/10-second limits.
Every screenshot, cache/file worker, desktop and battery lifetime retires cleanly.
Native tails retain summaries and worst records, not every individual app event.
Subjective audio at 1% is unassessed; earlier listening acceptance is separate.

The selected screenshot PID952 waits 2.894034 seconds with target identity7265,
a 3,407,872-byte captured prefix, 3,670,016-byte live file and a 65,536-byte
request. Four actual I/O-owner intersections cover 2.893501 seconds, leaving
533 us uncovered. The longest is PID7/thread20/SYS_SYNC87, spanning
2.892890 seconds and overlapping 2.891353 seconds of the wait. Independently
completed content has the same target identity, parent PID and enclosing
interval in this run; that match is checked rather than inferred from a global
maximum. The private window, request bounds and live-borrow rechecks are unchanged.

That owner records 261 completed status callbacks totalling 2.522982 seconds;
their exact wait overlap is 2.521687 seconds (87.13% of the bracket). Forty-three
16 KiB WRITE statuses total 2.494952 seconds, versus 19.214 ms for ten 512-byte
WRITE statuses. The completed content's payload subset has 36 commands/589,824
bytes with 2.487495 seconds of status wall and seven callbacks over 100 ms.
The retained worst status is a 16 KiB WRITE at tag542510 taking 352.020 ms:
351.745 ms from submission to observed completion, then 273 us to resume.
It records 39,716 empty checks and one event check, maximum sampled service gap
10.079 ms and final sampled gap 9.598 ms. These observations isolate long status
completion from a multi-second post-observation wakeup delay in this bracket.
They do not identify hardware arrival time or separate device firmware/media
from controller delivery; only one worst private wait is retained.

| Capture | Prior 80b4 total, seconds | Candidate total, seconds | Candidate WRITE phase, seconds |
| --- | ---: | ---: | ---: |
| 1 | 7.307485 | 6.405844 | 4.308990 |
| 2 | 10.890831 | 10.675969 | 5.351022 |
| 3 | 12.560273 | 26.604312 | 13.554029 |

The third capture additionally spends 3.606016 seconds in fsync, 5.065401 seconds
in temporary-file proof and 2.334707 seconds in final proof. Sender pixel transfer
is 139.921 ms, allocation 12.148 ms, and final worker-result observation 61 us;
worker cleanup completes in 77.912 ms. Long total completion therefore remains
storage work/waits rather than pixel transfer or final notification in this
capture. The private prelude totals 13.268134 seconds across its 40 waits.
Different cold boots, media state and interleaving prevent causal speed claims;
the slower third screenshot is retained, not classified as an improvement.
No barrier, fresh-media proof, owner check, command size or runtime limit changed.

Fresh read-only USB rehash preserves every one of the 868 prior files except
the expected `.srvros/meta` sidecar update. Nine new files yield 877 files and
4,445 allocated clusters, with zero leaks, FAT/bitmap mismatches or errors in
both collector and independent allocation checks. Installed/staged/independent
candidate, recoveryc354, accepted80b4/c7, retireda139's complete independent tree,
the original failed-media archive and matching trial backup all verify.
Ubuntu is default, the one-shot is consumed, firmware BootNext is absent,
collection is disabled and USB is unmounted. No reboot is armed.

This accepts bounded native owner/status diagnostics and the original combined
functional/integrity profile. Screenshot performance remains open, as does the
earlier intermittent battery transport-fault cause; no recurrence is captured.
Next investigate the 16 KiB payload status-completion tails and how long shared
commit ownership prevents private progress, preserving all request/time/fairness
bounds, flushes and integrity checks. No additional implementation or trial has
started at this checkpoint.

## Active payload command-tail and commit-ownership audit (2026-11-11)

From main `189f6e43`, investigate the long 16 KiB WRITE status callbacks inside
the exactly observed shared commit owner. Scope: existing USB command diagnostic
completion hook, fixed owner/wait metadata, final-only command companion, strict
parser and producer/transport regressions. Reuse already collected command-phase
and checkpoint clocks; associate only the exact tag/status interval, retain
complete validation separately from bulk success, and expose the logical block
range where available without retaining pointers. Partial or unavailable command
completion remains explicit. No additional device/ring reads or transfers.

Audit whether ownership can be shortened while respecting the filesystem's
mutable transaction and pending BOT/DMA lifetime. Do not release ownership or
weaken ordering on assumption. Preserve scheduler policy/fairness, command and
request sizes, all deadlines, flushes, fresh-media/CRC/allocation proofs, log
cadence/tail bounds and the original 1,800-second 1% workload with three captures.
PR19 at `f367eb0e` changes separate logger startup/Wi-Fi capture paths; PR301 at
`84e25638` has no competing command/commit implementation. No other task is
contacted. Matching 4598 native return is collected/reviewed and independently
sealed; accepted4598/80b4/c7 and recoveryc354 are retained. Validate locally,
publish/self-merge, install a matched candidate and run the native test under
the existing reboot authorization. New native acceptance remains pending.

## Validated command-tail preparation (2026-11-11)

Runtime `fbea6abe` is based on main `189f6e43` and published through PR474.
Refreshed PR19 at `b9f78149` changes separate logger startup/Wi-Fi capture hunks;
PR301 at `890d0fd3` has no competing command or commit hooks. No other task was
contacted. The [command-tail contract](../commit-command-tail.md) records the
ownership audit and exact receipt. Normalizing out the new metadata reproduces
the previous USB transport source byte-for-byte; filesystem, process, scheduler,
xHCI, syscall and original timing-parser hashes are unchanged.

The audit found transaction mutation/proof/publication and clean-marker flush
state still live under the shared owner, with pending BOT/DMA lifetime retained.
The [USB-IF BOT specification](https://www.usb.org/sites/default/files/usbmassbulk_10.pdf),
section3.4, also requires serialization of outstanding commands on the success
path. No arbitrary early ownership release is justified. This candidate adds
command evidence; it does not claim a latency improvement or a device/media/
controller cause. Command sizes, DMA segments, service/polling policy, timeouts,
fairness, flushes and all integrity checks remain unchanged.

The existing CSW clocks are copied into a command-local anchor. At the original
terminal validation path, exact tag/opcode/block context and the full original
validation verdict attach only to the matching live owner/status interval.
Already-collected command phases/checkpoints are optional; outside content tracing
the record explicitly carries command-only context, without invented clocks.
Larger status peaks clear old context, and late completion cannot backfill a
returned wait or released owner. Full-validation failures remain failures even
when the status bulk transfer succeeded. The single final row is at most843bytes;
all five rows total3803bytes at full field width, within the original4KiB budget.
Periodic bytes,10-second cadence and64KiB tails are unchanged.

Matched ordinary/native/fixture builds and all245 default core targets pass on
`fbea6abe`, including the new actual C/production-formatter/parser regressions.
Desktop/docs, scheduler/USB ownership and malformed-command quarantine, reserved
append, sync and full media-integration checks pass. The default optional
exhaustive-I/O setting is retained; no test requirements are weakened. Ordinary
ABI/input/fork/exec/yield, USB read/write/reload and screenshot VMs pass.
Exact-stage UEFI rehearsal passes the expected foreign-board refusal and automatic
return; this does not accept native firmware or battery ownership.

The populated400-IOPS KVM workload completes60seconds at1% with217 verified disk
bursts (56885248bytes),23 app cycles, an exact screenshot and zero recorded
underruns. Maximum loop is139.610ms; app ready/close maxima are1.236530/0.686488s,
within unchanged limits. Independent parsing passes every original16 contract,
append-wait, actual owner/status and new validated-command companion. Its exact
selected997us status belongs to a validated16KiB WRITE, tag7843, LBA29824,
32logical512-byte blocks and zero CDB flags. The selected owner is PID7/thread18/
SYS_FSYNC53, outside content tracing: phase availability is correctly false with
role7 and zero uncollected counters. Complete-phase forwarding is covered by the
real driver fixture and actual C/production formatter tests; VM command-only
coverage is not mislabeled as complete phase coverage. These are virtual
observations, not the Air's long hardware completion cause.

Release `0d3f2c1ce3a4aa4c`, run `ef8f650e89c64c4b`, is installed and verified
against its stage and independent payload copy. The actual877-file USB baseline
and offline fsck/allocation/headroom pass; only its expiring1% volume preference
is renewed. Accepted4598/80b4/c7 and recoveryc354 remain verified. Only the fully
verified, unreferencedbcd EFI duplicate was retired, retaining its entire
independent payload; previously retireda139/2dd/8caa copies also remain verified.
Ubuntu stays default and firmware BootOrder is unchanged, with no BootNext or
GRUB one-shot at the preparation checkpoint. The enabled return collector requires
the new command context alongside every original receipt. Post-merge arming and
independent run-record backups precede the existing authorized automatic reboot.

The next native run retains1800seconds at1%, three screenshots,500ms loop and
10-second app limits, all byte/CRC/fresh-media/allocation proofs and clean
battery/desktop/worker retirement. No human input is required. Native acceptance
remains pending its matching return. Screenshot completion and the earlier
intermittent battery transport-fault cause remain unresolved.

## Native validated command-tail return (2026-11-11)

Release `0d3f2c1ce3a4aa4c`, runtime `fbea6abe`, completes run
`ef8f650e89c64c4b` in session `session-000660086ecf2077-00`, then returns to Ubuntu
automatically. Independent matching raw-log/binary review passes all 16 original
contracts, append-wait, actual owner/status and the new exact validated-command
companion with complete phases. The original 1,800-second 1% workload completes
3,993 verified bursts (1,046,740,992 disk bytes), 345,812,992 PCM bytes, zero
recorded underruns and 1,078 app cycles. Maximum loop is 129.790 ms; app ready/close
maxima are 2.835216/2.750272 seconds within the unchanged 500 ms/10-second limits.
All capture, file/cache, desktop and battery lifetimes retire cleanly. Native
logs retain bounded summaries/worst records, not all individual app-stage events.
Subjective audio at 1% is unassessed; earlier listening acceptance stays separate.

The worst private wait is PID473's 3.124572-second full-window prelude. Its target
identity3628 has a 1,507,328-byte captured prefix, 1,769,472 live bytes and a
65,536-byte request. Seven actual owner intersections cover 3.124439 seconds,
leaving 133 us uncovered. The largest is PID7/thread20/SYS_SYNC87, with a
3.120900-second scope and 3.119375-second clipped overlap. Independently completed
content matches that target, PID and enclosing interval; no unrelated global
maximum is substituted. Sixteen live-borrow ready retries exercise the original
bounded recheck, with no change to append permission or progress guarantees.

The owner records 146 status callbacks totalling 2.871844 seconds; exact wait
overlap is 2.870563 seconds (91.87%). Forty-three 16 KiB WRITE statuses total
2.846330 seconds, versus 16.981 ms for nine 512-byte WRITE statuses. The matching
content's payload subset has 36 commands/589,824 bytes, 2.838810 seconds of status
wall and twelve statuses over 100 ms. These are one selected wait/owner and
content, not whole-run device counters.

The new exact command receipt supplies full original validation and complete
phases for tag274626, opcode42, LBA1567936, 32 logical512-byte blocks and zero
CDB flags, with payload role2. It has one CBW, four existing4KiB data requests,
one CSW and four original checkpoints. Every requested/actual data byte matches;
original tag/signature/residue/status checks pass before the positive verdict.

| Part of selected command | Wall time |
| --- | ---: |
| Whole command | 371.324 ms |
| CBW | 0.008 ms |
| Four data transfers, 16 KiB total | 1.028 ms |
| Status callback | 370.267 ms |
| Existing checkpoints | 0.012 ms |
| Remaining command interval | 0.009 ms |

Status is 99.715% of this complete command's wall interval. Submission to observed
completion takes 370.067 ms; observed completion to resume is 198 us. Cached
service observations retain 42,790 empty checks, one event check, a 10.078 ms
maximum sampled gap and a 9.619 ms final sampled gap. This localizes the selected
command delay to status-completion observation after its data phase, without
claiming hardware arrival time or separating device/media/firmware from controller
delivery. Full command validation is not durability before the original flushes.

One additional read-only512-byte boot-sector inspection maps the logical range
to a 16 KiB-aligned offset, 96 KiB into one128KiB filesystem cluster. It does not
cross that logical cluster boundary. This is post-return filesystem geometry,
not flash geometry or retrospective file ownership. The inspection reads only
the boot sector and changes no native command behavior, retry/reset handling,
cache policy or request size.

| Capture | Prior4598 total, seconds | Candidate total, seconds | Candidate WRITE phase, seconds |
| --- | ---: | ---: | ---: |
| 1 | 6.405844 | 8.991598 | 6.534990 |
| 2 | 10.675969 | 16.109374 | 11.409647 |
| 3 | 26.604312 | 12.285139 | 8.906136 |

Capture2's private preludes total11.176127seconds across39 waits; allthree capture
workers retire, with87.287ms as the retained cleanup maximum. Cold-boot/media/interleaving
variation remains substantial and disk burst count is3993 versus4062 previously.
No causal or sustained speed improvement is accepted from this diagnostic run.
All byte/CRC/fresh-media/allocation proofs, barriers, deadlines, fairness and
original workload limits remain unchanged.

Fresh actual USB rehash preserves all877 prior files except the expected
`.srvros/meta` sidecar update. Nine new files yield886 files/4552 allocated
clusters, with zero leaks, FAT/bitmap mismatches or errors in both collection and
independent offline checks. Current/staged/independent candidate, recoveryc354,
accepted4598/80b4/c7, retiredbcd's complete tree, prior retireda139/2dd/8caa payloads,
the original failed-media archive and matching trial backup all verify. Ubuntu
remains default, one-shot is consumed, BootNext is absent, collector is disabled
and USB is unmounted. No reboot is armed.

This accepts bounded complete native command-context diagnostics and the original
combined functional/integrity profile. Long completion latency and the earlier
intermittent battery transport-fault cause remain unresolved, with no recurrence
of that fault captured here. A useful next control is bounded storage-only
write/flush work to separate filesystem/interleaving effects from completion
latency under the same sizes, polling, deadlines and integrity checks. No further
implementation or physical test has started at this checkpoint.

## Active bounded storage-only comparison (2026-11-11)

From main `f16fa5ab`, prepare a separate fixed32KiB filesystem/direct-extent
write/flush/readback control, six balanced pairs within the existing180-second
comparison bound. Keep16KiB transport commands,4KiB DMA segments, current polling,
all deadlines, filesystem barriers/proofs and allocation/claim/quarantine checks.
The old4096-byte battery scratch API remains unchanged; use the existing general
exclusive-range proof for a fresh32KiB fixture, park the outer filesystem, revoke
all public write aliases and retain the whole-device raw claim during direct I/O.
No caller-supplied raw offset/length or reuse of user documents. Any uncertain
write/readback poisons the capability and refuses filesystem reentry for that boot.

Scope: explicit opt-in workspace comparison, automatic boot profile/controller,
compact bounded receipts, strict collection, actual producer and lease/fault
fixtures. Pause periodic logging while measuring; verify all sample bytes/CRC,
raw containment, fresh readback, clean lease retirement and offline file/allocation
preservation. Use1% volume with brief boundary cues outside measured storage;
there is no continuous desktop/app/battery/audio workload in this isolated
control. Historical30-minute stress requirements and acceptance stay separate.
PR19 `b9f78149` owns separate logger startup/Wi-Fi capture hunks; PR301 `890d0fd3`
has no competing workspace/direct-extent control. No other task is contacted.
Current0d3/accepted4598/80b4/c7 and recoveryc354 plus their evidence are retained.
Validate, self-merge and run automatically under existing sudo/reboot authorization;
new native acceptance remains pending matching return.

Implementation in PR476 adds the explicit `--storage-only-capture` ordinary-kernel
profile and fixed twelve-sample producer. Disposable-media tests use the actual
workspace/exFAT/VFS/BOT/block/claim code, including512/4096-byte sectors,
4/128KiB clusters, exact authorized-range containment and terminal quarantine
for partial writes, bad flushes, readback mismatch and invalid clocks/deadlines.
The strict receipt checks all samples, CRCs, command/service clocks and retired
leases; offline native collection additionally requires all prior files and
allocation preservation. Source/VM validation and native installation remain
pending; no physical performance or new acceptance result is claimed.

Fresh pre-validation fetch keeps main at`f16fa5ab`; PR19 is now`5248f3cb`
and PR301`42facdb0`. Their actual main/staging/smoke diffs add separate
network/scanout profiles; this isolated workspace control remains independent.
No other branch is merged or rewritten.

The full core/desktop/docs baseline and ordinary sysabi, USB input, thread exec
and USB write/reload VM regressions passed at`7c89275d`. Final review strengthens
only the new optional fixture/collector: distinct512-byte sectors and16KiB
chunks catch a transport that aliases both reads and writes, and ring-counter
validation respects unsampled empty observations while requiring the exact
empty/event partition and original service coverage. Final affected-path,
matched-image and exact-stage requalification is pending. Controller limits,
policies, integrity proofs, existing payloads and ordinary startup are unchanged.


### Ready isolated native comparison

Final optional fixture/collector source`527bbb81` passed36 real-stack cases,
including distinct-sector/chunk alias corruption and legitimate unsampled-empty
observations, original block/USB/workspace/image leases and logger checks,
93 boot/installer unit tests, documentation, exact-stage GPT saved receipt/full
fixture/offline fsck and ordinary sysabi/USB input/thread exec/USB write-reload
VM regressions. The separately retained full core/desktop/docs baseline is
`7c89275d`; only the new payload and collector/fault fixture changed afterward.
No controller, filesystem, scheduler, polling, deadline or integrity limit changed.

Release`78cfda46ee1b63a0` is installed from the matched ordinary kernel/initramfs.
The exact-stage run completes all12 samples and saves the stronger final payload.
Actual Air6,1 UUID`6ADA-85BB`, AC, all886 prior file hashes, headroom/full allocation,
independent accepted payloads and original failed-media archive verify. Ubuntu
remains default with recovery`c35475f62f154f1e`; accepted0d3/4598/80b4/c7 remain on
EFI and independently backed up. Only the unreferenced, fully verifiedc95f EFI
duplicate was retired; its complete independent payload is retained. Five old
inactive synthetic stream fixtures were losslessly archived with per-file SHA
verification to keep build headroom; native evidence and active fixtures remain.

The saved local run is`2923b641532e4681`; its new automatic collector requires
one matching session, all12 CRC/byte/command/service samples, six retired leases,
exact final sequence12 bytes, prior-file preservation and offline fsck/allocation.
The guest runs six balanced32KiB pairs within180seconds with brief1% boundary
cues outside measurements, shows progress and returns automatically. No human
input or listening report is required. Implementation/host/VM validation is
complete; native performance/acceptance awaits this run's matching return.


## Native storage-only comparison return (2026-11-12)

Merged PR #476 (`087fff1b`) release `78cfda46ee1b63a0`, compiled from
`527bbb81`, completed run `2923b641532e4681` in matching session
`session-0006600cc8c0dd00-00`. All twelve balanced 32 KiB samples completed in
5.602029 seconds, within the unchanged 180-second bound. The production parser
and independent review validate each sequence CRC, complete fresh readback,
six exclusive leases/alias revocations/retirements, command/service clocks and
two completed 1% boundary cues. The final sequence-12 fixture independently
matches all 32768 bytes, with 64 distinct sectors. No sustained PCM, desktop,
battery, screenshot or human listening acceptance follows from this control.

| Measured phase, per 32 KiB | Filesystem, milliseconds | Direct extent, milliseconds |
| --- | ---: | ---: |
| Write, including existing filesystem work/barriers | 46.004–77.938 | 5.371–5.390 |
| Explicit flush | 0.404–0.405 | 0.577–0.579 |
| Fresh readback | 4.347–4.352 | 1.852–1.858 |
| Total measured I/O leg | 50.755–82.691 | 7.807–7.822 |

The filesystem write stage issues 37 or 56 BOT commands versus two direct
writes. Its slower samples follow a remount and have 19 additional commands;
this is an observed association, not attribution to a particular metadata
operation. The slowest selected write-stage CSW callback is 4.177 ms for a
512-byte filesystem WRITE; direct 16 KiB WRITE maxima are 1.072–1.080 ms.
The earlier seconds-long status waits do not reproduce in these short quiet
measurements. Filesystem command amplification remains visible, and the prior
loaded completion delay remains unresolved; this accepts no performance fix.

The measured I/O legs total 0.449364 seconds. The remaining 5.152665 seconds
include preparation, exclusive proofs, parking, retirement, remount/readback and
final checks. Six gaps after direct I/O and before the next sample are
817.078–833.309 ms; no individual operation inside those gaps is timed here.
The direct data phase excludes those mandatory costs, so its smaller number
must not be presented as end-to-end filesystem throughput or a safe bypass.
Different order/cache/media state also prevents a causal speed claim.

The automatic collector initially stopped on an incorrect Home-relative metadata
exception. The sidecar is at the **volume root**, `.srvros/meta` (SrvrOS
`/work/.srvros/meta`), as defined by the existing filesystem implementation.
The original failed assessment/script/checks are retained with hashes. The local
collector now uses that exact path; an explicit review of the same returned
media passes without rearming, another boot or any device/acceptance-limit change.
All 886 prior files remain, with only this expected private metadata sidecar
changed and all other 885 hashes identical. Four new files yield 890 files and
4557 allocated clusters; exfatprogs and independent traversal report zero leaks,
FAT/bitmap mismatches or errors. New logs and the fixture match collected hashes.

Current/staged/independent candidate, recovery `c35475f62f154f1e`, accepted
0d3/4598/80b4/c7 payloads, retired c95f's independent full payload, the original
failed-media archive and the matching trial backup verify. Ubuntu remains the
default, firmware order is preserved, one-shot is consumed, BootNext is absent,
collector is disabled and USB is unmounted. No reboot is armed. The bounded
quiet control is accepted for this unit; sustained status latency and the older
intermittent battery transport-fault cause remain unresolved.

A useful next step is per-role filesystem/BOT accounting during bounded repeated
writes, retaining all proofs, barriers, sizes and deadlines. Separate the
37/56-command metadata/proof work from load-dependent status-completion tails;
this control alone cannot explain or clear the screenshot delays. No new
implementation or physical trial has started at this checkpoint.

## Active bounded filesystem command attribution (2026-11-12)

From main `7c1a08a0`, add per-role BOT command/byte/status accounting to the
existing bounded repeated 32 KiB storage control. Retain six balanced pairs,
twelve samples, 180 seconds, 16 KiB commands, 4 KiB DMA, current polling and all
original transport deadlines, filesystem proofs/barriers, exclusive leases,
fresh-byte/CRC readback and quarantine/retirement checks. Use a separate explicit
v2 trace profile; preserve the v1 recipe and receipt reader. Brief 1% boundary
cues remain outside measurement, with no desktop, battery or sustained PCM load.

Scope: existing exFAT callback role hooks, generic USB content observer fast
inactive path, compact per-role receipts/strict collection, stage/controller
selection and actual producer/observer equivalence and failure fixtures. Roles
retain the established metadata, allocation/free, data, prefix proof, sidecar
update/equality and unclassified mapping. Observe existing work without new media
operations, scheduler checkpoints or clocks in the role hooks. Reconcile totals
with the original callback counts and selected command; missing/overflowing or
conflicting evidence remains invalid. Keep the unchanged 64 KiB log tail and
32 KiB receipt bound. Preserve all accepted evidence and original collector-path
failure; the volume-root sidecar exception stays exactly `.srvros/meta`.

PR19 `f0f77ed7` and PR301 `6d2482b2` have separate network/graphics scopes;
inspect actual overlapping files before implementing the shared observer hooks.
No other task or subagent is contacted. Current accepted `78cfda46ee1b63a0`,
0d3/4598/80b4/c7 and recovery c354 remain protected. Publish the bounded change,
validate matched host/VM artifacts, self-merge and run automatically under the
existing sudo/reboot authorization. Native trace acceptance remains pending its
own matching return; long loaded status waits and screenshot delays are unresolved.

Actual overlap inspection finds PR19's exFAT edits confined to metadata decoding
and restore admission around the sidecar loader. The callback labels at read,
write, flush and dirty-flag I/O are separate; this branch leaves that loader and
all storage behavior intact. PR301 does not touch these filesystem/USB observer
files. The published objective remains one local branch with no cross-task contact.

The v2 producer preserves the exact workload and adds sparse role masks/counts,
status totals/maxima and 512-byte WRITE subsets. A final dry wire count refuses
oversize evidence within the unchanged 32 KiB receipt and 64 KiB tail. Prototypes
pass 72 profile/refusal/fault cases plus 36 legacy-adapter controls, with identical
ordered BOT bytes, callback/clock counts and complete final media. Formal matched
build, host/VM validation and native installation remain pending.


### Ready native command-attribution trial

Final source `68644c19` passes all 72 v1/v2 profile/refusal/fault executions
and 36 legacy-role controls with identical ordered BOT bytes, callback/clock
counts and final media, maximum-width actual row sizing/overflow refusal,
94 boot/installer tests, original storage leases/SYS_SYNC observation checks,
the full core/desktop/docs suite and ordinary sysabi, USB input, thread exec
and USB write/reload VM regressions. An initial standalone-host linkage failure
was corrected by selecting its existing disabled-runtime role adapter; that
failed attempt is retained. Existing offline-repair fixture limitations remain
unchanged, including 81 baseline-known strict allocation failures after repair.
No native fault-recovery or loaded-latency fix is claimed.

Candidate `084167c2be826f2a` uses the explicit v2 trace recipe. Its content-addressed
stage retains the original `41ced5f5` manifest rather than rewriting provenance:
the final-source complete kernel/initramfs and all five loaded kernel sections
are byte-identical and separately verified. The exact staged GPT trial passes
all twelve original samples and role/status/512-byte subsets, exact command/time
sums and wire byte counts, full fixture readback, saved logs, clean filesystem,
unmount and automatic return. Its receipt is about 20 KiB inside the unchanged
32 KiB bound and 64 KiB tail. Transferred data-byte totals include required
padding/initialization; they are not a claim about logical user-file length.

Fresh Air6,1/UUID `6ADA-85BB`, AC, all 890 prior file hashes, offline fsck/full
allocation/headroom, accepted payloads and the original failed-media archive
verify. The candidate is installed and independently backed up. Ubuntu remains
default with recovery `c35475f62f154f1e`; accepted 78cf/0d3/4598/80b4/c7 remain
on EFI and independently retained. Only the unreferenced, completely verified
3403 EFI duplicate was retired, with its full independent payload preserved.
PR19 is now `b0aa87df` and PR301 `cef27b1c`; final observer-file changes do not
overlap this bounded work, and neither branch is merged or rewritten.

Saved local run `d060e1967e7c45b0` requires the v2 receipt, all original byte/CRC,
command/service and six-lease retirement proofs, exact final sequence-12 bytes,
all prior files except the exact volume-root metadata update, and offline
allocation integrity. It shows progress and returns automatically, with brief
1% boundary cues outside the same six balanced pairs/twelve 32 KiB samples and
180-second bound. No input or listening report is required. Native attribution
acceptance awaits this run's matching return; screenshot and long loaded status
latency remain unresolved. No policy, hardware gate, watchdog, ownership,
integrity or workload limit changed.

## Native command-attribution return (2026-11-12)

Merged PR #478 (`a419f045`) release `084167c2be826f2a` completes run
`d060e1967e7c45b0` in matching session `session-0006600f9a16bacf-00`.
The immutable manifest records `41ced5f5`; qualified source `68644c19` has the
same complete kernel/initramfs bytes, as already verified before installation.
Automatic first collection and independent review pass all twelve samples,
six exclusive lease retirements, command/service/role sums and subsets, CRCs,
geometry and exact final sequence-12 bytes. The control takes 5.714710 seconds
within the unchanged 180-second bound, with two completed 1% boundary cues.
Role/data rows occupy 19171 bytes and the full receipt 20133 bytes, within the
original line/32 KiB receipt/64 KiB tail limits. Only this bounded quiet native
attribution is accepted; no loaded latency or screenshot fix is claimed.

The trace accounts for the previously observed 37/56-command write stages:

| Callback role | Steady 37-command sample | 56-command sample | Difference |
| --- | ---: | ---: | ---: |
| Other filesystem metadata | 11 | 11 | 0 |
| Allocation/free | 8 | 12 | +4 |
| File data | 8 | 8 | 0 |
| Sidecar update | 1 | 24 | +23 |
| Sidecar equality proof | 9 | 1 | −8 |
| Total | 37 | 56 | +19 |

Samples 5/9 share the first composition; 4/8/12 share the second. The first
sample also totals 37, with one read assigned differently between metadata and
allocation. No prefix-proof or unclassified filesystem commands are present.
The sidecar-update role's 24 commands comprise nine reads, eleven writes and
four flushes, transferring 131584 read bytes and 119296 write bytes. The cheaper
path reads 117760 bytes under the equality-proof role and retains one flush;
the update path's equality proof reads only 16384 bytes. With the unchanged
source's early mismatch behavior, this is consistent with an equality miss in
the first chunk followed by normal replacement. The changed serialized field
and exact dirtying cause are not recorded; timestamps, identities or other
metadata changes must not be guessed from these counts.

Every filesystem sample transfers **128 KiB of file data for the 32 KiB logical
write**, as eight 16 KiB commands. This matches the existing full-cluster
initialization on this unit's 128 KiB clusters; it is transferred-byte
amplification, not eight logical file writes. Each direct sample remains two
16 KiB writes/32 KiB. Metadata traffic is additional. No initialization, equality
proof, barrier, allocation, byte-readback or ownership check was skipped.

Filesystem write/flush/readback legs are 50.782–83.294 ms; direct legs are
7.803–8.118 ms, excluding mandatory raw proof/retirement/remount costs. The six
post-raw gaps are 825.558–866.948 ms. The largest measured write-stage CSW
callback is 4.185 ms, for a 512-byte metadata WRITE; direct maxima are
1.065–1.072 ms. Every recorded over-100-ms status subset is zero. These software
observations do not reproduce the earlier seconds-long loaded status waits and
do not identify hardware completion time or clear the sustained screenshot issue.
Different boots, file counts and media/cache state prevent a causal speed claim.

All 890 prior paths remain, with only the expected volume-root `.srvros/meta`
changed and the other 889 hashes identical. Four new files give 894 files and
4562 allocated clusters. Fresh collector reads, exfatprogs and independent
allocation checks report zero leaks, FAT/bitmap mismatches or errors; copied
new files match their media hashes. Current/staged/independent candidate,
recovery c354, accepted 78cf/0d3/4598/80b4/c7, retired 3403/c95f independent
payloads, original failed-media archive and trial backup verify. Ubuntu remains
default, firmware order is stable, one-shot is consumed, BootNext is absent,
collector is disabled and USB is unmounted. No reboot is armed.

The next concrete target is sidecar write amplification: audit why equality
misses after remount and which metadata fields changed, then look for a safe
reduction while retaining all persistence semantics and fresh proofs. PR19
already owns separate metadata decode/restore work; do not duplicate that
implementation or infer its native acceptance here. No new implementation or
physical trial has started. Existing offline-repair limitations and the older
intermittent battery transport-fault cause remain unresolved.

## Active sidecar remount/equality audit (2026-11-12)

From fetched main `ca454d7c`, reproduce the sidecar equality miss after a raw
lease/remount and distinguish record order from real path/identity/permission/
timestamp changes. Scope is exFAT serialization/equality diagnostics and bounded
host regressions, the existing storage-only command trace/strict collection and
this unit's trial records. If proved redundant, reduce serialization-order
traffic while retaining every fresh-media byte comparison, required metadata
field, publication/flush barrier, dirty marking, owner/client exclusion and
quarantine. No cache of equality, deferred persistence or limits change is allowed.
Keep twelve 32 KiB samples/six pairs, 180 seconds, 16 KiB commands/4 KiB DMA,
unchanged transport policy and brief 1% boundary cues outside measurement.

PR19 at `41c49fde` owns metadata decoder/atomic restore changes; its actual
exFAT diff remains confined to parse/apply/load admission. Do not duplicate that
implementation. PR301 at `537eed80` has no exFAT/VFS overlap. Work stays in the
existing local device checkout with no subagents or cross-task contact. The local
MacBookAir6,1 and designated USB UUID are confirmed in the ignored run record; current accepted `084167c2`,
recovery `c35475f6`, prior accepted payloads and outside-build evidence remain
protected. The previous run is collected, independently reviewed and sealed.
Publish the bounded candidate, run matched host/core/desktop and exact-stage VM
checks, self-merge and run automatically under existing authorization. Native
acceptance and any sustained screenshot benefit await matching evidence.

The real importer/slot-reuse reproducer now confirms an order-only rewrite:
every path and all eight fields match before/after remount, but VFS enumeration
order changes. Stable bytewise-path serialization keeps the sidecar identical
through four remounts. On both 4 KiB and 128 KiB host geometries the no-op path
retains only two volume-flag writes (four bytes) and four flushes; legacy order
requires eight writes and seven flushes. Actual atime, mode/ctime and rename
changes still persist, with complete file and allocation verification. A slow
BOT fixture also observes genuine timestamp-driven differences, so those writes
must remain. First-difference diagnostics classify shape or expected field, not
an inferred semantic cause; no actual values or paths are logged.

Scratch is reserved before dirty marking, bounded by existing node limits and
freed with the mount. Fresh media equality, all eight serialized fields, pending
name/client checks, dirty/clean markers, flush/publication order and quarantine
remain. Maximum-width diagnostics fit the original receipt reserve. PR19's
metadata decoder/restore code is unchanged. Focused regression/failure validation
is in progress; the initial diagnostic-unit adapter failures are retained locally.
No native benefit, cold-boot identity restore or screenshot fix is accepted yet.

Before native testing, the candidate also retains a strict per-sample equality/
first-field delta for each of the original twelve writes. This separates remount
association from actual timestamp/namespace changes directly. Fixed RAM-only
snapshots are charged inside the write interval; no clock, I/O, allocation or
scheduler operation is added. One filesystem comparison and zero raw-path
comparisons are required, with all sums bounded by final counters. The first
build/focused/exact-stage VM pass is retained; its incomplete broad suite was
stopped deliberately for this refinement and is not represented as a pass.

### Ready native sidecar remount trial

Source `9c1bd01a` passes the matched ordinary build, canonical/legacy-order real
importer regression, original sidecar publication/refusal/fault controls, 72
v1/v2 producer cases and 36 legacy observer controls with identical ordered BOT
bytes/clock calls/media, maximum-width wire/scratch bounds, storage leases and
SYS_SYNC checks, boot tests, full core/desktop/docs and ordinary syscall ABI,
USB input, thread exec and USB write/reload VM regressions. The 89-case installer
matrix retains its one platform skip; the existing 81 strict-allocation failures
after offline repair remain baseline limitations, not accepted recovery. Initial
diagnostic-unit setup failures and the superseded partial broad run are retained.

Candidate `998fdfb9b4f9c5a8` has exact final-source kernel/initramfs provenance.
Its exact GPT stage passes all twelve samples, full bytes/CRC/lease/role/service
checks and required paired final/per-sample first-field evidence. Five filesystem
samples compare equal; sample four first differs in atime. That identifies the
first changed field in this VM, not every changed field or a native cause. The
host interleaved-node fixture separately proves and removes order-only traffic.
No timestamp suppression, persistence deferral, cache of equality, changed limit
or native/sustained screenshot improvement is claimed.

Fresh local model/USB/AC and all 894 prior file hashes verify, along with clean
exfatprogs, full allocation, writable admission and headroom. The candidate is
installed and independently backed up. Ubuntu remains default, firmware order
is stable and recovery `c35475f62f154f1e` is retained. Current accepted 0841 and
prior 0d3/4598/80b4/c7 payloads remain installed and independently protected. Only
the unreferenced 78cf EFI duplicate was retired after complete independent-payload
verification; its payload and accepted evidence remain outside build storage.
PR19 at `4d976eda` still changes only the separate decoder/restore loader hunks;
PR301 at `8353e4e3` has no overlap with this writer/trace scope. Neither is consumed
or contacted. The payload's runtime source remains exactly the tested revision.

The saved local run requires twelve original 32 KiB samples/six balanced pairs,
180 seconds, unchanged transport/ownership/proof/retirement checks and every prior
file hash except the exact volume-root sidecar update. It adds one valid equality
comparison per filesystem write, none per raw write, reconciled first-field/shape
counters and unchanged receipt bounds. Brief 1% cues remain outside measurement.
Progress and results are automatic; no human input is needed. The return collector
is enabled; this recorded ready checkpoint is unarmed. Self-merge, save the full
independent run backup and arm the authorized one-shot next. On Ubuntu return,
collect and review the matching evidence before staging another trial. Long loaded
USB waits and screenshot latency remain unresolved.


## Native sidecar remount return (2026-11-12)

Merged PR #480 (`7f9f3058`), runtime source `9c1bd01a`, release
`998fdfb9b4f9c5a8`, completes the matching automatic native run in 5.703998 seconds.
First collection and independent review pass all twelve original 32 KiB samples,
six exclusive lease retirements, fresh full-byte/CRC checks, geometry, command/
service/role sums and the exact final sequence-12 file. Each filesystem write
owns one valid sidecar comparison; all six raw writes own none. The final counters
have ten checks: three equal and seven changed (four shape differences outside
the sampled writes plus three atime-first differences during measured writes), with
zero refused proofs, flush failures, active scopes, invalid counters or format
mismatches. Row bytes are 20371 and the full storage/sidecar receipt is 21623
bytes, within the unchanged 32 KiB receipt, 2048-byte reserve, line and 64 KiB tail
bounds. Both 1% boundary cues complete; this is not an audible/endurance trial.

Filesystem samples 1/5/9 compare equal and issue 37 commands. Samples 4/8/12
rewrite and issue 56; their first differing **expected field** is atime (mask
64), not path order or shape. The 19-command difference still comprises +23
sidecar-update, +4 allocation/free and −8 equality-proof commands. Sidecar updates
have nine reads, eleven writes and four flushes (119808 write bytes). Equal proofs
read 118272 bytes; changed proofs stop at the first 16384-byte chunk. The eight
ordinary file-data commands still initialize 128 KiB for each 32 KiB logical
replacement. Canonical serialization is physically observed and the three exact
fresh equality proofs pass; the host order-only traffic reduction is not a
measured reduction in these native samples.

Filesystem write times are 47.070–78.938 ms (previous 46.034–78.522 ms), with
full write/flush/readback legs of 51.816–83.699 ms (previous 50.782–83.294 ms).
Direct legs take 7.803–7.827 ms; their proof/retirement/remount work remains
outside those per-leg numbers but inside the original 180-second trial bound.
Six post-raw gaps are 833.742–849.209 ms. The maximum measured CSW callback is
4.178 ms, with zero over-100-ms status subsets. The overall elapsed difference
from 5.714710 seconds is not evidence of a causal speedup; loaded USB status
waits and screenshot latency are not resolved or exercised here.

The three mismatch offsets fall inside the `.workspace` marker's atime field in
the directly saved final sidecar. Source confirms `workspace_mount` invokes
`marker_valid`, which calls `vfs_read_all`; successful reads update atime. This
implicates internal mount verification in the first mismatch. Intermediate
expected buffers were not saved, and a first-field trace cannot exclude other
changes: diagnostic file readbacks also update atime and normal writes update
mtime/ctime. Do not suppress all metadata writes from this observation. The next
bounded audit should separate kernel verification side effects from application
accesses and establish the complete changed-field set before reducing persistence
traffic; preserve normal application timestamps, every byte proof and barrier.
A marker-only change may leave the rewrite count intact because actual file
writes also advance mtime/ctime. Optimizing the cost of required sidecar updates
is a separate target from avoiding verification-only timestamp side effects.

The saved final sidecar has 1100 canonical path-ordered records, up from 1095,
with five new records for the session directory, three logs and fixture. All
prior record paths remain. The complete prior-record comparison finds 56 inode
changes and one atime change (the marker); mode, nlink, uid, gid, mtime and ctime
are identical for prior records. Resulting sidecar inode IDs are unique/nonzero.
This does **not** accept stable cold-boot IDs: the unchanged sequential restore
calls `vfs_set_metadata`/`import_metadata`, which remaps colliding saved identities.
PR19 owns separate atomic decoder/restore work and remains unmerged; do not
claim this writer change implements or physically accepts that work.

All 894 prior file paths remain; only the exact volume-root `.srvros/meta` bytes
change, with the other 893 hashes identical. Four new files give 898 files and
4567 allocated clusters. The actual copied new files match media hashes;
exfatprogs and production allocation checking report zero leaks, FAT/bitmap
mismatches or errors. Current/staged/independent candidate, recovery c354, prior
0841/0d3/4598/80b4/c7, retired 78cf/3403/c95 independent payloads, original failed-
media archive and trial backup verify. Ubuntu remains default, firmware order
is stable, the one-shot is consumed, BootNext is absent, the collector is disabled
and USB is unmounted. No reboot is armed and no next trial has started.

## Active complete-metadata audit and sidecar update cost (2026-11-12)

From fetched main `9a8d2377`, audit every serialized metadata field for the
existing bounded storage comparison and reduce required sidecar-update work.
Preserve normal atime/mtime/ctime, all eight persisted fields, canonical format,
fresh complete byte proofs, temporary-name publication, dirty/clean markers,
barriers, ownership/client exclusions, quarantine and every existing limit.
The completed previous native trial and its full independent backup are sealed;
no run is outstanding. This remains one local device checkout with no subagents
or cross-task contact. Local model, designated USB identity, installed current
998f and retained recovery c354 are recorded and rechecked before physical work.

Writer scope: prove a reusable inactive entry run in the first sector of the
private metadata directory, before any END marker, to avoid the redundant whole-
cluster directory read. Only this narrow case may return early. END consumption,
ambiguous/long runs, later sectors/clusters and growth retain the original full
scan and prefix/tail preparation. All media writes and flush order must match
legacy behavior. No equality/prefix byte proof may use a cached substitute.
Diagnostics scope: a separate explicit full-field storage trace reads the complete
old sidecar and merge-compares canonical path records/all eight numeric fields
with the newly serialized table. Count all changed fields and namespace changes,
not merely the first mismatch. Retain the existing v1/v2 readers/recipes and the
same twelve 32 KiB samples, six pairs, 180 seconds, 16 KiB commands, 4 KiB DMA,
transport deadlines, 32 KiB receipt/64 KiB tail and 1% boundary cues. Diagnostic
read overhead must remain visible and distinct from update work.

Actual PR19 at `3225dca2` still owns the separate metadata decoder/atomic restore
loader hunks; do not alter or duplicate its authoritative restoration work.
PR301 at `2efa8028` has no overlap with these storage/boot files. Retain the
unresolved cold-ID persistence and loaded USB/screenshot issues. Publish the
bounded candidate, validate actual full-field observation and byte/write/barrier
compatibility/fault paths, run the matched host/VM checks, self-merge and launch
the authorized automatic native comparison. No performance acceptance before its
matching physical return; no limits, recovery protocol or hardware gate changes.

The complete audit prototype confirms that a rewrite can include the diagnostic
file's atime/mtime/ctime and the marker's atime together; marker suppression alone
would miss required persistence. The v3 observation reads the full old table and
counts every field/namespace change without applying data or changing ordinary
v1/v2 equality behavior. Its additional reads/CPU time remain inside the existing
write interval and proof role, not hidden from the comparison.

The first large-cluster BOT host A/B reduces six updates' directory input from
789504 to 134144 bytes, with identical complete media, ordered writes/flushes and
full metadata-change evidence. The first END-consuming allocation still falls
back; subsequent pre-END deleted-slot reuses avoid the full cluster read. Small
clusters retain their original path. Paired publication/failure/repair controls
pass, including stale private tail bytes behind END; existing recovery gaps are
unchanged. Full build, strict-v3 VM, regression qualification and fresh physical
preflight remain pending. These host results do not establish native savings.

### Complete audit candidate qualified and installed (2026-11-12)

Runtime `a45aaa1d` / release `8a89638dfb0281b5` now passes the matched ordinary
build, full core/desktop/docs, 95 boot-profile tests, exact-stage GPT v3 capture,
ordinary syscall/USB keyboard/thread-exec and USB read/write/reload regressions.
The complete staged kernel and initramfs hashes match the frozen tested source.
All 108 v1/v2/v3 producer/refusal cases and 72 legacy-role controls pass. The
complete-field observer covers all eight numeric fields, namespace changes,
late read/format failure, owner/token refusal and the original wire budget.

The frozen-source directory A/B reproduces 54 to 15 reads and 789504 to 134144
read bytes over six required updates on 512-byte-sector/128 KiB-cluster media;
both paths retain 42 writes, 344064 write bytes and 24 flushes. The 4 KiB-sector
large-cluster control also reduces reads; small clusters retain identical I/O.
Complete media, ordered write/flush payloads and full metadata-change counts are
identical. Every paired write/flush/failure/tear control passes, including stale
bytes after the private directory's END marker. Source audit accounts for every
driver edit and proves the serializer, authoritative loader, publication writes,
allocation/free, markers and barrier paths unchanged. Transport, commit ownership,
VFS, scheduler, synchronization and limits are unchanged.

The original installer matrix retains its one platform skip; contiguous append
retains the 81 known strict allocation failures after offline repair. Neither is
new recovery acceptance. Physical preflight separately reports a clean volume,
898 unchanged prior-file hashes, 4567 allocated clusters, zero leaks/errors,
admitted writes and available headroom. Installed/staged/independent candidate
and recovery payloads verify. One unreferenced historical EFI duplicate was
retired only after independent complete-payload verification; its backup remains.
Ubuntu default/order and recovery c354 are retained; automatic matching collection
is enabled. Local run/reboot state and verified backups remain outside Git.

Next checkpoint: self-merge this exact runtime plus documentation, then launch
the authorized unattended v3 comparison with the original twelve samples/six
pairs, 180-second bound and 1% boundary cues. Collect and independently review the
matching session, all complete-field/role/byte proofs, prior hashes and offline
allocation before accepting native savings. Extra full-table audit reads stay
charged to proof role 5; directory update work is role 4. Total v2/v3 timing is
not a causal speed comparison. Cold-ID restoration remains PR19's separate work;
loaded screenshot/commit latency remains open. No native result is claimed yet.

### Native complete-field audit and directory reuse accepted (2026-11-12)

Merged PR482 runtime `a45aaa1d` / release `8a89638dfb0281b5` completed native v3
session `000660193a9f9efb-00`: all twelve 32 KiB samples/six pairs in 5.874794 s,
both 1% cues, exact final sequence, CRC/readback/extent/claim retirement and clean
automatic Ubuntu return. Independent review reparses the complete receipt,
recomputes payload/CRC and wire bounds, rereads every file on a read-only mount,
and repeats offline fsck/allocation. The bounded trial passes unchanged limits.

Each required rewrite (samples 4/8/12) now uses one 512-byte directory read,
versus nine reads/131584 bytes in the prior trial. Update-role commands fall from
24 to 16, retaining eleven writes and four flushes. That avoids exactly 128 KiB
of directory input per update. Native write bytes grow from 119808 to 120320 as
the sidecar gains records; the host A/B remains the controlled proof of identical
media and ordered writes/flushes. Equal samples 1/5/9 retain zero sidecar payload
writes and their original clean flush.

All six complete comparisons cover 1104 matched records with no namespace or
representation changes. Each rewrite changes exactly two records: the diagnostic
file's atime/mtime/ctime and the workspace marker's atime. Inode, permissions,
link count and ownership are unchanged inside these comparisons. Thus marker-only
timestamp suppression would still miss required file metadata persistence.
Full audit reads nine commands/118784 bytes per filesystem sample in proof role
5, charged inside the write interval. The 22989 row bytes/24241 receipt bytes fit
the original 32 KiB receipt and 64 KiB tail. Maximum observed CSW callback is
4.184 ms, with no callbacks over 100 ms in this quiet run.

Filesystem writes take 53.203–84.312 ms versus 47.070–78.938 ms previously; no
overall timing improvement follows from this v2/v3 comparison because full-table
diagnostics and table growth differ. Direct writes remain 5.366–5.532 ms. Ordinary
32 KiB filesystem replacement still transfers 128 KiB in eight data commands.
The directory-read reduction is accepted; loaded screenshot/commit latency and
end-to-end performance remain unaccepted.

All 898 prior files remain; 897 non-sidecar hashes are identical and only the
exact volume-root sidecar changes. The volume now has 902 files/4572 clusters,
zero leaks, bitmap/FAT mismatches or errors. Final metadata has 1105 canonical
records and unique nonzero IDs. Across the cold boot, 72 prior IDs are reassigned
through the untouched legacy sequential restore path; other prior fields retain
their values except the marker's atime. Stable cold identity remains unresolved,
owned by PR19's separate restoration work, not accepted by this trial. Existing
offline repair gaps likewise remain open.

Installed/staged/independent payloads, retained recovery c354, prior accepted
payloads, retired independent payloads, original failed-media archive and trial
backup verify. Ubuntu remains default, the one-shot is consumed, BootNext is
absent, the collector is disabled and USB is unmounted. The full return evidence
is sealed outside build storage. No reboot or new trial is armed.

Next useful checkpoint is the original automated audio/disk/app/three-screenshot
stress at 1%, with full-field tracing off so its extra proof work does not alter
that workload. Retain every original lifecycle, screenshot, byte, timing and
integrity requirement; use that trial to assess the optimized ordinary path
under contention. This result alone does not establish loaded latency savings.

## Active post-sidecar combined stress qualification (2026-11-12)

From fetched main `c44a9e51`, prepare the existing Air6,1 automatic capture-write
desktop profile against the merged filesystem improvements. Retain the original
1800-second PCM/disk/app workload at 1%, three exact screenshots, 500 ms loop and
10-second lifecycle limits, every fresh byte/CRC/allocation proof, flush, ownership
and retirement requirement. The heavy storage-only full-field audit is off;
ordinary screenshot/write/commit diagnostics and all collection contracts remain.
No runtime, scheduler, device gate, power policy or workload-limit edit is planned.

The matching v3 return is reviewed and sealed; no physical run is outstanding.
Local model, USB identity, installed current and recovery stay in the ignored run
record. Keep current 8a89, the accepted command-tail stress payload and recovery
c354 available. Publish only this unit's preparation/evidence; no subagents or
cross-task contact. PR19 at `37ae8304` retains its separate metadata restoration
scope; PR301 at `5bfaa366` changes separate display profiles and shared staging
hunks, without a competing storage or audio/disk-stress implementation.

Runtime source is unchanged from fully qualified `a45aaa1d`; retain its complete
core/desktop/docs and ordinary boot evidence through an exact source-equivalence
check. Build matched ordinary, native and synthetic battery kernels; rerun focused
filesystem/lease/capture/sync/profile checks, populated/throttled positive stress
with a large sidecar, independently parse its receipts and rehearse the exact
native stage. Fresh physical preflight must preserve every prior file except the
explicitly renewed 1% preference, retain verified recovery and save the on-disk
run/collection record before self-merge and authorized one-shot reboot. Native
loaded performance remains pending the matching thirty-minute return.

### Post-sidecar stress candidate qualified and installed (2026-11-12)

Source `dca44437` / release `9463151939d54175` passes fresh matched ordinary,
native Air6,1 and synthetic battery builds, sampler-isolation checks, focused
filesystem/lease/reserved-append/audio/capture/sync/commit/battery/profile/desktop
regressions, 95 boot-profile tests, ordinary screenshot VM and exact-stage UEFI
rehearsal. The latter correctly refuses the foreign board and returns; positive
workload execution is separately verified by the synthetic battery fixture.
All runtime/build/dependency/test sources remain byte-identical to fully tested
`a45aaa1d`, and complete ordinary kernel/initramfs hashes are identical. Its sealed
full core/desktop/docs and ordinary ABI/input/thread/USB qualification is retained
explicitly rather than reported as a new suite execution.

The populated 400-write-IOPS KVM run adds 1100 preserved archive records and
existing Home files. It completes sixty seconds at 1%, 179 verified disk bursts
(46923776 bytes), 21 app cycles, an exact screenshot and zero recorded underruns.
Maximum loop is 141.350 ms; ready/close maxima are 1.059933/1.366323 s, within the
unchanged bounds. Independent parsing passes the original saved contracts, raw
BMP/binary receipt, append waits, exact owner/status correlation and validated
command context with complete phases. No guest input is injected. These are
virtual qualification results, not native speed acceptance.

The existing capture-write Air6,1 profile is unchanged. Its configuration and
positive saved logs omit heavy full-field metadata activation and rows. Original
ordinary fresh-byte sidecar equality, screenshot/write/commit diagnostics and
all timing/integrity checks remain. The candidate retains thirty-minute PCM,
the existing audio tail allowance, continuous verified disk work, app cycles,
three screenshots, 500 ms loop and 10-second lifecycle limits, and clean desktop,
battery, cache and worker retirement.

Physical preflight retains all 902 prior files, renewing only the explicit 1%
volume preference expiry. Before/after fsck, allocation, headroom and write
admission pass with 4572 clusters and zero leaks/errors. Installed/staged/
independent candidate and prior payloads verify. Current 8a89, accepted stress
0d3, prior 998f/80b4/c7 and recovery c354 stay installed. Only the independently
verified, unreferenced 4598 EFI duplicate is retired; its full payload remains
outside build storage. Ubuntu default/firmware order remain unchanged, the USB
is unmounted and the matching automatic return collector is enabled.

Next checkpoint is the authorized self-merge and one-shot launch, with the full
run/recovery/volume/check record and independent backup saved before reboot.
No native loaded result is claimed until the matching thirty-minute session,
all three screenshot byte proofs, every original receipt, prior-file hashes and
offline allocation are collected and independently reviewed. The earlier battery
transport-fault cause, cold-ID restoration and offline repair gaps remain open.

### Native post-sidecar stress failed at the lifecycle deadline (2026-11-12)

Release `9463151939d54175`, source `dca44437`, returned from session
`0006601c3d51eefb-00` after about ten minutes, before the original thirty-minute
workload completed. This is a failed native run. The automatic collector first
reports incomplete capture history; raw review establishes an actual lifecycle
failure, with two completed images rather than the required three. No parser,
deadline, capture count, workload or integrity requirement is relaxed.

At 620.300462 s monotonic, cycle 360 hits phase 3 (close/reap), PID479 Text,
elapsed 10.009502 s against the unchanged whole-cycle ten-second limit. It has
360 completed cycles, zero child errors and no measured loop excursion: retained
maximum loop is 120.025 ms. Launch/ready maximum is 4.721952 s; completed-close
maximum is 2.461250 s. The failing cycle's close remains pending at the deadline,
so that completed-close maximum does not describe its unresolved wait.

The same PID's actual exec takes 4.682070 s. A matching desktop-worker `SYS_EXEC`
gate waits 4.676039 s behind PID7/thread20's storage scope, which lasts 5.226930 s.
Text then becomes ready, receives Close, releases its surface/mappings and stops
its unused file service. Main return is at 615.630785 s; its `SYS_EXIT` gate then
waits 6.145917 s behind another PID7/thread20 scope. These separate storage waits
consume the cycle budget. Text eventually exits status zero during failure
cleanup, after the deadline has already failed the trial.

The second selected owner is a `SYS_SYNC` scope lasting 6.470787 s. The strict
append/owner/command companions bind its actual screenshot overlap and completed
content; no unrelated global maximum is substituted. It records 155 status
callbacks totaling 6.009694 s, with 6.008411 s overlapping the wait, including 47
16 KiB WRITE statuses totaling 5.634261 s. Selected tag245440 is a fully validated
16 KiB WRITE: 1.389 ms data transfer, 378.360 ms status callback and 19.994 ms of
existing checkpoints, within its 399.759 ms total command. This is software
completion-observation timing, not a hardware-arrival or flash-geometry proof.

Both exact 1366x768 BMPs are saved and their workers retire, the second during
failed-session cleanup. Their lifetimes are 19.767119/17.820727 s, with WRITE
phases 12.498196/14.249399 s. Capture history has zero dropped/invalid entries;
the third capture is never requested. Partial analysis of these two completed
records is explicitly separate from original expected-three acceptance, which
remains failed. There is no completed audio/disk receipt. The last active PCM
snapshot records zero underruns at 1%, but sustained thirty-minute audio/disk
coverage and audible quality are not accepted. Normal desktop/battery controls
report failure and the workload is terminated by failed-session teardown.
The battery service itself retires with zero errors/denied operations and no
retained heap; its first-protocol observation is clean. The enclosing controls
verdict follows the desktop failure, not a new battery transport fault.

Fresh independent read-only rehash preserves all 902 prior paths and all 901
non-sidecar hashes; only the exact volume-root metadata sidecar changes. Seven
new files include the four logs, two BMPs and interrupted payload.tmp. Offline
fsck and independent allocation both report 909 files/4645 clusters and zero
leaks, FAT/bitmap mismatches or errors. Matching raw logs, BMP headers/pixels,
partial timing/ownership/command records, installed/staged/independent payloads,
retained recovery and original failure archive verify. Ubuntu is default, the
one-shot is consumed, collector disabled, USB unmounted and no reboot armed.
The failed trial and review are sealed outside build storage; accepted prior
stress/recovery payloads remain available.

The quiet directory-read saving remains accepted. Loaded screenshot and app
latency are still unresolved, and this run supplies no overall speedup or full
endurance acceptance. Next audit storage-independent process launch and exit
admission alongside long shared-sync payload/status waits. `SYS_EXIT` currently
regains ordinary admission and `process_exit_gate` waits before teardown; skipping
that gate without proving descriptor, worker, rights and memory lifetimes would
be unsafe. Preserve the original cycle budget and live-owner protection rather
than resetting the deadline, pacing away contention or releasing an active
transaction. No runtime change or new trial has started at this checkpoint.

## Active independent launch/exit admission audit (2026-11-12)

From fetched main `f61b40eb`, fix the observed storage-gated desktop app launch
and exit without changing the whole-cycle deadline, workload, owner protection
or integrity rules. The failed 9463 run and all 760 backup files are reviewed and
sealed; no run is outstanding. One local device checkout is used, without
subagents or cross-task contact. Current/recovery/USB identity and reboot state
remain in the ignored local record and are rechecked before physical work.

Scope: process/syscall/scheduler admission and narrowly bounded RAM-backed image,
descriptor/worker/rights/memory lifetime proofs, their regressions and current
contract documentation. Permit independent progress only after the complete
request/resources qualify; preserve ordinary gated fallback for storage,
callbacks, shared/uncertain resources and unsupported cases. Never release or
borrow a foreign live transaction, omit file flushes/cleanup, waive handle or
mapping checks, pace away contention or reset the ten-second lifecycle budget.

PR19 at `d634433c` owns separate pipe-handoff/network and metadata-restore work;
its process hunks affect background kernel spawn/pipe handoff, not the targeted
user exec/exit gates. PR301, refreshed through `82dd3da8`, changes user-scope owner-loss handling;
preserve its authority model and existing detach behavior, without implementing
that separate change here. Inspect and retain those boundaries when adding
eligibility checks. Keep cold-ID restoration and device/power policy separate.

Publish a bounded candidate and real negative/refusal/ownership tests, then run
matched ordinary/native/fixture builds, full regression checks, populated loaded
VM and exact-stage rehearsal. Preserve failed/accepted payloads and recovery;
verify all prior files and renew only the explicit 1% volume preference. After
self-merge and saved independent run/collection records, launch the original
fully automated 1800-second PCM/disk/app/three-screenshot workload at 1%, with
heavy full-field metadata audit off and every original limit/proof intact.

### Independent lifecycle candidate implementation (PR486)

The audit also found two scheduler barriers after the original exec/exit wait:
fresh kernel startup had no eligible user tuple, and context binding/clearing
explicitly refused foreign ownership. The candidate publishes and consumes a
fresh prepared child tuple, and provides a separately checked atomic retirement
transition; ordinary bind/clear guards stay intact. The complete same-loader
RAM copy, entry, exit, real context return and reap-publication host fixture holds
a foreign owner throughout, forbids storage callbacks/waits, and checks rollback
on allocation/scheduler refusal. Separate fixtures cover immutable-source
identity/ranges/refusals, real prepared-child resolution, safe sibling/owner/CR3
qualification, TLS/PTE ownership and fail-stop retention on unexpected unmap.

Scope includes the read-only exFAT view/ready-publication marker and fixed
GUI/network/stream/display resource-presence snapshots needed by those proofs.
It does not admit VFS descriptors, pipes/queued rights, live device resources,
user-scope-owner teardown or mutable/storage-backed executable reads. The
existing redirected-file teardown/flush and ordinary image/thread regressions
remain required. Qualification and native boot evidence are still pending;
9463 remains failed evidence, and the original workload/limits remain unchanged.

A final admission pass distinguishes a helper blocked inside the ordinary I/O
entry wait from an unsafe continuation doing mutable work. The exact I/O wait
park marker may qualify an otherwise unsafe sibling; it is cleared on dispatch.
An arbitrary wait queue, an unparked unsafe sibling, a mismatched CR3, or any
same-process owner/private-image alias still refuses. This prevents a desktop
report writer awaiting admission from defeating a proven private child/clean
exit, without admitting its I/O or changing the existing memory/helper gates.
The first build/focused validation attempt was stopped before any VM/native
launch to qualify this additional exact-park case in the final candidate.

Final retirement qualification also requires the quiet detached background
continuation used by desktop app launches. Foreground commands must restore a
waiting parent's context and retain ordinary admission. Positive background
return/reap and negative foreground/nonquiet retirement are covered separately.

The first loaded VM on runtime `e012fe23` reached a 60-second PCM PASS (179
verified bursts, zero recorded underruns), 24 clean app cycles, clean desktop
retirement and USB unmount, but the host sampled QMP as QEMU exited before it
drained the complete terminal line. That attempt remains failed validation with
its six checkpoints/serial/audio preserved locally. The harness now distinguishes
an exited QEMU from a broken live connection, stops overlap sampling after the
PCM result and drains serial after exit; it still requires the complete terminal
receipt, every original saved workload/retirement proof and clean QEMU exit.
Mocked live-error/missing-device/exit races supplement the required fresh VM run.

A later unchanged-runtime VM completed 24 app cycles and 180 verified PCM/disk
bursts with zero errors/underruns, but strict phase validation correctly failed:
two saved snapshot starts were 11.723334 s apart and 73,161 log bytes apart,
leaving 7,625 bytes outside the original 65,536-byte tail. Missing startup phases
were not accepted from interleaved UART. The logger had armed the next ten-second
period after completing the preceding write, adding storage latency each time.
The candidate now anchors that same 1,000-tick period to capture start, including
the boot capture; an overrun skips missed periods and retains a full idle period,
without queued catch-up writes. Tail/file/checkpoint caps, workload, all record
requirements, deadlines, write/readback/flush and ownership checks are unchanged.
Delayed-write/overrun/failure regressions and fresh full qualification are required.

The stream absence check is IRQ-protected through its admission transition, with
both enabled/disabled caller-state and nonempty-stream refusal coverage. The
full-capacity mapping fixture also covers the checked private teardown seam.
A development sweep now links all 46 lifecycle/scheduler/syscall host fixtures;
their runtime assertions and the fresh full suite remain required.

Integration refresh remains `f61b40eb`. PR19 at `9d1d92b4` has independent extended
Wi-Fi capture/phase controls in the logger, but leaves the periodic worker's
cadence unchanged; those larger capture limits and profile APIs are not consumed
here. Preserve both start/phase-control semantics when integrating that branch.
PR301 at `ad1b1110` has no additional common runtime overlap after the previously
reviewed owner-loss changes. No other device's acceptance is inherited.


### Independent lifecycle qualification and next native run (2026-11-12)

PR486 runtime `4629fced` passed the fresh complete core/desktop/docs
suite (including the 89-case installation matrix with its default-skipped
opt-in exhaustive I/O case), boot-tool regressions, matched ordinary/native/Q35
builds, ordinary
syscall/UEFI-USB/fork/exec-vector/yield guests, USB storage and ordinary screenshot
smokes. The full core rerun followed verified archival of inactive staging
artifacts after a fixture link exhausted disk; the failed attempt is preserved.
No completed core prefix was skipped. The affected 46 host fixture binaries also
compile and link, with focused ownership/lifecycle/rollback regressions passing.

The populated KVM workload at 400 write IOPS and 1,100 metadata files passed 23
app cycles, 176 verified disk bursts and 178 observed PCM/I/O overlaps, one exact
screenshot, 139.971 ms maximum loop and zero recorded underruns. All 1,167 prior
files remained intact. Independent saved-log validation passed the original
contracts and append/commit/command companions, including complete startup
phases without UART reconstruction. The unchanged 64 KiB tail and declared
1,000-tick capture period retain their original caps; observed snapshot deltas
were at most 61,266 bytes. The exact staged native payload also passed its separate
foreign-board refusal/automatic-return VM rehearsal; that is not native ownership
acceptance.

The next installed candidate, release `7acb756c0ac5f4bb`, runs the original fully
automatic 1,800-second
PCM/disk/app/three-screenshot stress test at 1%, with heavy full-field metadata
audit off. Every deadline, byte/CRC/readback/flush/allocation, ownership and
retirement requirement remains unchanged. Fresh local model/USB/recovery,
prior-file hashes, independent payloads, offline writable/allocation checks and
Ubuntu-default/one-shot state were verified; only the explicit quiet-volume
expiry was renewed. A saved local run record and automatic return collector bind
its release/source to the next session. Known-good recovery and prior failed
9463 evidence remain retained. Self-merge and one-shot launch are the next local
steps; native acceptance remains pending until the matching return is collected
and independently reviewed. PR19 refreshed to `86a239a4` changes separate Wi-Fi
reconnect/fence helpers, with no new common process/scheduler/logger overlap;
PR301 remains at the previously inspected `ad1b1110`. No sibling hardware
acceptance is implied.
