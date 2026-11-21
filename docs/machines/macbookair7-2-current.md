# MacBookAir7,2 current status

Updated 2026-11-21 (America/New_York). Reference A1466 with partial desktop/device acceptance.
The [dated journal](macbookair7-2.md) preserves exact-source history.

The inherited-framebuffer desktop remains usable, with independent evidence for
keyboard/trackpad, secondary click, window navigation, saved Home documents,
brightness, sensors, battery and earlier USB Ethernet/audio rounds. Native
tear-free scanout is still pending.

## Reset-free primary plane flip

Current Air7,2 work on PR301 covers exclusive access to the first candidate
GGTT slot during publication and rollback, with the inherited panel scanning
out. The VMM and Intel scanout adapter share this scope. The CPU aperture
address and original stolen RAM are distinct paths to the same slot. Native
GPU admission also depends on the firmware, engine, DMA, cache and reader
handoff. The aperture diagnostic holds the candidate CPU address across two
advancing primary-A frame counts and checks the inherited surface addresses.
It saves the aperture address and both frame counts. A live firmware/GT
range transfer is still required before publishing or restoring a PTE.
Current implementation work binds the aperture exclusion to the same one-page
publication through restoration, translation completion and reader retirement.
This depends on the VMM guard and the Broadwell page-store adapter.
On PR301 the GT range and flush trials hold the same reservation object the
one-page publisher can promote. The display exclusion stays pinned across
promotion. Both page-table writes require a single continuously held traffic
block and fresh closing checks. This path has passed host qualification.

On this Air, the GGTT table page containing slot 8704 (`0x8b811000`) and
its inherited backing page (`0x8e201000`) both fall in the EFI runtime
descriptor `[0x8b000000, 0x90000000)`, type `EfiReservedMemoryType`.
This classifies the physical memory. Authority to reassign the GPU address
must come from the graphics owner. The captured IGPU methods and OpRegion
driver-ready flags provide no identified per-slot loan. [UEFI memory map](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html#memory-allocation-services) · [i915 OpRegion driver notification](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_opregion.c#L1109-L1144)

Replaying the four native traffic packets from release `78276fdd559a38cd`
through the page writer's closing check refused each at the render engine
(`0x8000`). Media counters were empty, all six request permissions were
open, and the engine and L3 flush acknowledgements were absent. The next
physical write needs a native graphics owner that can transfer this slot
with the boot scanout pinned and keep new requests blocked through both
writes, translation invalidation and reader retirement. The automated
replacement and rollback are pending.

On November 21 the native range adapter began excluding the physical GGTT
table page along with the CPU aperture. The writer pins the only writable
MMIO alias for slot 8704. Its original traffic block and alias must survive
the final reader-retirement callback before memory can be reused.

Under Ubuntu's i915 on this unit, a 40 MiB private GEM object occupied GGTT
`[0x01200000, 0x03a00000)`. Its page at offset `0x01000000` was slot
8704. A private blitter request wrote data and a post-sync marker on that
page. After GEM wait, both the independent GEM read and the aperture view
matched. All 1,024 words also matched after a CPU write and restoration.
The i915 trace showed a single binding through verification, followed by
the matching unbind. The internal panel was enabled before and after.
This verifies the exact address and one blitter request under Linux. Native
SrvrOS still needs a device-wide drain with the original panel active.

Broadwell's render `MI_MODE` programming notes rule out using STOP_RING
as a reversible pause during an inherited batch. Resuming can use the
wrong memory access type. The next driver step is to retire inherited
submission and bring up an owned command context for explicit completion
fences. A completion on one engine still leaves the other engines, caches
and GPU readers to account for. [Intel Broadwell register reference, p. 1322](https://cdrdv2-public.intel.com/690789/intel-gfx-prm-osrc-bdw-vol-02c-commandreference-registers.pdf#page=1364)

Release `f3961550b358c858` (source `7e3af989`, session
`session-000660c0d078f7b5-00`) completed the one-page reservation trial.
The GT requester held slot 8704 across the GT-only reset, rechecked it while
PCI bus mastering was disabled, and returned it before bus mastering resumed.
All 129 sampled translations matched, including the slot's original
`0x8e201001` entry. The programmed display state was preserved, the trial
issued zero GPU page-table writes, and offline USB integrity passed.
Preparation took 221.213 ms under the 250 ms diagnostic limit. The original
preparation, GT and HDA timing limits remain unqualified. The next mapping
step requires a live handoff from the firmware owner and proof that outstanding
GPU accesses have drained before replacing and restoring the entry.

Release `88b98be2420eb2dc` (source `0eaf94b3`, session
`session-000660c05d0472e9-00`) completed the aperture diagnostic on this Air.
The CPU guard excluded BAR2 page `0xb2200000` for slot 8704 and closed
cleanly after 66.552 ms. Pipe A advanced from frame 1095 to 1096 on the
inherited surface. All 1,484 candidate translations matched and the
reservation returned. The USB integrity and Ubuntu recovery checks passed.
The first unmet condition was `firmware-transfer`. The GPU page table
received zero writes. Single-page replacement and rollback still await
a live device owner.

The range diagnostic now registers the entire inherited 34 MiB fence for the
rest of the boot. Both kernel handoff APIs reject a conflicting framebuffer
description even after the candidate lease closes. The one-page publisher
checks this independent registration before preparing or writing the mapping.
The version 11 receipt records the number of protected pages. GPU ownership
of the second range and a physical page flip remain pending.

Source `aef29e75` passed 65 transfer cases and the 336-case native adapter
fixture under fatal ASan/UBSan. Release `b05a2ddf2ec897ad` booted in QEMU,
recorded zero protected pages and zero writes on the unsupported GPU, saved
the report to exFAT and reset. The same image halted on missing workspace.

PR301 on main baseline `4016abf0` now exercises a synchronous flip to a second
surface and restores the original using the same display owner. The local
fixture runs the plane and primary CRC cores together at the Air's captured
geometry. It checks a distinct alternate CRC and exact restoration, including
rollback after an incorrect alternate image. A timed-out or uncertain return
retains both pins. Neither leg requires a PCI reset.

The physical flip trial remains pending. The native capture on release
`42cdc13b60340ddd` found both 6 MiB candidate spans firmware-reserved. The
OS-allocated alternative needs an owned GPU address interval and visible
X-tiled backing before the native adapter can arm the first flip.
[Trial contract and local coverage](../intel-bdw-primary-flip.md#reset-free-hardware-trial)

PR301 now takes OS-allocated X-tiled backing through Broadwell GGTT encoding,
publication, a synchronous flip, rollback and mapping retirement in the local
fixture. At 6144 bytes per row the padded image occupies 1,356 pages. The
128-page translation tail brings the lease to 1,484 pages. A one-pixel fault
in the back buffer fails the image check and the original still returns.

The earlier physical session `session-000660ba79e6f52e-00` records firmware
framebuffer memory at `0xb0000000` with length 5,529,600 bytes (1,350 pages).
That is exactly one 1440 × 900 image at this pitch. The same fields match
in all 128 saved exit logs containing this endpoint in the latest collection.
The native source transfer currently describes one GGTT entry and supplies
zero ownership assurances. A native test needs a live transfer covering the
whole second surface, plus the ordered publication and display adapters.
[Private scanout mapping](../intel-bdw-primary-flip.md#private-backing-in-a-transferred-interval)

PR301 now checks the second surface’s exact 1,484 original GGTT entries
under the native display and CPU mapping guards, bracketed by two reads of
all 32 hardware fences. Local coverage includes a changed last padding entry
and a changed last fence on return. The shared transfer admission still
requires five hardware assurances. The source offers none, so the
reset-free hardware flip remains pending.
[Native range observation](../intel-bdw-primary-flip.md#native-reset-free-range-observation)

The exact staged diagnostic from `748f615f`, release `a6262aa628bf2e85`,
booted in QEMU. With the workspace attached it saved its unsupported-GPU
result to exFAT and reset. The missing-workspace run halted. On the Air the
cooperative EFI lease covers the boot GT requester. It does not transfer
these mappings. The next implementation needs a native graphics owner that
can reserve them across publication, both flips and restoration, or receive
that reservation from a preceding owner. The physical flip is pending.


The native range adapter now reserves all 1,484 candidate translations across
both kernel handoff APIs before it checks them against hardware. Version 10
records the reservation generation and clean return. A conflicting reservation
cannot enter while a flip, rollback or reader retirement holds the pages. The
native source still needs control of the firmware mappings, GPU traffic, caches
and display readers before it can promote this reservation and publish a second
image.
[Reservation lifetime](../scanout-transfer.md#reserving-the-second-framebuffer-interval)

The exact staged kernel from `5134dc1e`, release `c75840e6674be3a6`, passed
both QEMU boot paths. With the USB workspace it saved a version 10
unsupported-device receipt and reset. Without the workspace it displayed
the expected halt. The QEMU receipt records zero reservations and writes.
The full 1,484-page flip, rollback, competing-owner and delayed-reader cases
run in the host fixture. A physical flip remains pending.

The Broadwell one-page publisher now exercises its aligned 64-bit store and
exact restoration in the host fixture at GGTT index 8,704. A simulated stale
device translation after a successful store keeps the range and RAM pinned.
The fixture restores both the captured Air entry `0x8e201001` and an
original with a nonzero upper word. All 63 transfer cases passed under fatal
ASan/UBSan. The write primitive
compiled with the freestanding kernel flags and is absent from the ordinary
kernel and read-only range diagnostic.

On this Air under Ubuntu kernel `7.0.0-43-generic`, i915 bound one 4 KiB
page at GPU offset `0x554000`. All 1,024 words matched an independent GEM
read after both changing and restoring them. The trace recorded the matching
unbind. This confirms the physical aperture works under i915 ownership.
SrvrOS still needs a native owner to exclude prior GPU users and maintain
completed translation invalidation and reader retirement. The next native
trial publishes and restores one entry with the original display pinned.
The 1,484-page flip follows that qualification.
[One-page implementation and Ubuntu reference](../scanout-transfer.md#broadwell-one-page-checkpoint)

## Native display recovery after PCI decode reopens

Release `40de8de2891163f4` (source `7e227182`) passed all 47 checks on this
Air. With bus mastering inhibited and BIOS keeping the well powered, the
driver released and reacquired its display power request. The complete PCI
hold took 197.912 ms. All 5,529,600 framebuffer bytes matched and the restored
primary produced the reference CRC in both windows. The USB return passed
offline verification. The run is closed.

The next prerequisite for a PCI function reset is proving completion of
outstanding GPU traffic in the same ownership lifetime. The native trial
exercised the BIOS-powered well path. Cold-well recovery, post-reset display
restoration and native tear-free scanout remain pending.
[Hardware evidence](../intel-bdw-edp-recovery.md#native-resource-recovery-under-inhibited-pci-bus-mastering)

PR301 now joins the graphics VT-d read/write drain to the PCI function reset
in the host recovery fixture on main `4016abf0`. The fixture checks loss of
requester ownership, a missing drain acknowledgement, changes in the PCI
configuration and failure to retire the remapper mapping. The native Air
still needs the same dark-display owner to control the remapper and account
for remaining GPU pipeline, cache and prefetch work. The Wi-Fi and WebCore
PRs have separate scopes.

Release `8e5b856650ecf306` (source `240f0c8d`) completed the GT3 media
counter diagnostic on this Air. All four reference and prefetch counters
read zero at each of three checkpoints. The arbiter continued to allow
requests and the engine and L3 flush acknowledgements stayed clear.
The USB evidence and Ubuntu recovery checks passed. The next reset milestone
is to exclude new GPU requests and verify outstanding writes and cache
completion while the same owner holds the display off.
[Media counter measurements](../intel-completion-prerequisites.md#macbookair72-media-counter-result)

PR301's host recovery fixture now exercises the request to block GPU
memory traffic before PCI memory decode closes. Loss of an engine or L3
acknowledgement, a busy video queue or a reopened arbiter blocks reset.
A physical trial needs a native provider that can initiate and maintain
the power manager's request under the dark-display hold.
[Reset ordering and dependencies](../intel-bdw-edp-recovery.md#joined-pci-reset-and-panel-recovery-fixture)

As an Ubuntu reference, i915 reported RC6 enabled on this unit's
`8086:1626`. Its GT0 residency counter advanced 679 ms over a 1.000 s
interval. Upstream i915 has a software park operation for an initialized
RC6 context. A candidate SrvrOS provider must account for that context,
the GT wake held to read the acknowledgements and whether the block remains
observable before PCI decode closes. Residency alone cannot establish any
of those conditions.
[i915 RC6 park](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_rc6.c#L643-L668)

A second Ubuntu comparison measured 764 ms of RC6 residency over one
second before opening i915's forcewake handle, 0 ms while the handle was
held, and 753 ms in the second after release. Each interval followed a
200 ms settling period. This motivates sampling with the guest's GT wake
released.

Release `c49a10f84b3db492` (source `efcf3ff7`) completed the physical
dark-display PCI hold. With bus mastering inhibited, both samples showed
RC0 and a clear forcewake acknowledgement. The RC6 residency counter
stayed at 1,594,584 ticks across the 2.004 ms between sample starts. All
47 reopen checks passed, the framebuffer bytes matched, and both restored
frames produced the reference CRC. Offline USB verification passed.

Release `9eb54abb15203e80` (source `4888fb6d`) confirmed that the inherited
policy already enables RC6, selects timeout mode and targets RC6 with an
800 µs promotion timer. The controls matched before engine preparation,
after preparation and after GT reset. All six memory clients retained
permission in those snapshots. After GT reset the power manager also reported
reset reasons `0x7f00`. A host replay of that sample stopped before requesting
a fresh block. The physical diagnostic closed with verified USB evidence.
[Native samples and reset-guard replay](../intel-completion-prerequisites.md#air72-inherited-rc6-controls-and-reset-reasons)

Release `78276fdd559a38cd` (source `814eb242`) traced the request through
both GT reset pulses. The seven reason bits first appeared across pulse one,
within a 724 µs interval, and were still set at the last read 171.4 ms later.
All six client permission bits were set in every one of 1,077 guarded reads.
The four engine/cache packets showed no flush acknowledgement. The 83.615 ms
maintenance interval therefore ran with sampled permissions open. The result
closed as `DIAGNOSED` after USB integrity and Ubuntu recovery verification.
[Physical trace and coverage](../intel-completion-prerequisites.md#air72-two-pulse-request-trace)

The next dependency is a native mechanism for the display owner to request
and hold a traffic block across the drain. To locate any later reason-bit
clearing, a follow-up observation needs to span wake release with a documented
register-access path and an independently held display/requester guard.
[Dark-display samples and recovery](../intel-bdw-edp-recovery.md#gt-power-state-during-the-dark-pci-hold)

PR301 now binds the modeled block to the display owner's original reset
attempt. An observed client reopening or loss of an engine acknowledgement
invalidates the transaction before decode closure. On this Air, an Ubuntu
i915 trace recorded 60 park calls before obtaining the forcewake handle,
zero during the hold and 49 after release. The engine and L3 message registers
used for verification are in the render-forcewake range. The native PCI reset
trial still needs an independently observable, maintained block.
[Owner checks and Ubuntu trace](../intel-bdw-edp-recovery.md#display-owner-block-lifetime)

## Native eDP recovery scope

PR301 owns the Broadwell port-A AUX transport and fixed-mode eDP link
training in `kernel/diagnostics`, with host tests in `tools`. The first
checkpoint is bounded AUX traffic and link-training state transitions using
this unit's Ubuntu display reference. Native use depends on verified panel
power and clock sequencing, an owned display route, and a known-good scanout
backing. GGTT writes and reset-based takeover remain downstream. The base is
main `4016abf0`. The open Pro11,2 Wi-Fi and WebCore PRs have separate scopes.

The [eDP recovery core](../intel-bdw-edp-recovery.md) now uses the single
enhanced-framing 2.7 Gb/s lane and panel power delays observed for this
unit's 1440×900 mode through Ubuntu i915. The host fixture covers dark panel power-up,
AUX link training, live-primary verification and backlight admission. The
native clock, PHY, primary restore and recovery provider is the next
checkpoint. There is no new release or physical acceptance.

PR301's next checkpoint is the source-side DDI A and pipe A restore in
`kernel/diagnostics`, using Intel's ordering and the Air7,2 reference mode.
It joins the AUX/PPS sequence to actual source register programming and live
primary readback in a host fixture. Native use requires a separate authority
for the powered display wells, restored CDCLK/LCPLL, and the original
framebuffer translation lifetime. Main is `4016abf0`. The open Wi-Fi and
WebCore work has no shared display-driver changes.

The source adapter joins LCPLL 1350 selection and PHY replay to the
1440×900 timing, M/N, watermark, and original tiled primary programming.
The same register sequence runs through PPS and AUX in the host fixture.
Backlight permission requires advancing frames, a clean underrun latch,
and independent fetch evidence. The native reset and restore adapter
still needs to supply that evidence after reinitializing the display.

PR301 now owns the inherited-route qualification for pre-reset DDI PHY,
the original framebuffer pin and its primary-plane CRC. Work is limited to
the Broadwell display diagnostic, its host/native checks and the boot recipe
that carries them. The first trial keeps the inherited display running and
leaves PCI reset and translation writes out of the sequence. It requires an
exclusive CPU display lease, verified primary mappings, and independent
CRC completion and retirement. Main remains `4016abf0`; Wi-Fi and WebCore
have separate scopes.

The first native trial, release `752c58ee0fb72b7e`, captured all nine DDI A
PHY entries and verified the 1,356 visible PTEs and 128-entry fetch tail
around the CRC window. The first window accepted zero CRC completions and
restored its control state. It saved logs, unmounted, and returned to
Ubuntu automatically. The retry unmasks only CRC-done and records the
raw poll status. The reason for the original CRC failure remains unconfirmed.

The retry, release `5d36f1891358bf06`, closed as
`EDP_PHY_CRC_BASELINE_COMPLETE`. Both windows preserved all nine PHY
entries and all 1,484 backing translations. Each produced four fresh
completions, and the stable CRC changed with the console content. The
probe restored the CRC control and interrupt mask, saved its report, and
returned to Ubuntu. The next step is to reproduce a known frame during
native display restoration and verify its CRC across reset.
[Pre-reset trial result](../intel-bdw-edp-recovery.md#physical-pre-reset-result).

Release `43785d3ecb432ae1` physically qualified the fixed A–B–A reference
sequence and passed Ubuntu return verification. The restore adapter now
requires two fresh CRC windows matching the reference before enabling
the backlight. Host fixtures cover missing and mismatched CRCs,
backing changes and uncertain retirement. On main `4016abf0`, the open
Pro11,2 Wi-Fi and WebCore PRs have separate scopes. The remaining native
checkpoint is continuous ownership of powered MMIO, clocks and the
original framebuffer across reset.

Release `d68c99c294da7d5c` physically verified the Broadwell driver
well request and release around the final reference CRC in 94.452 ms.
The firmware request remained asserted, and the pinned backing, clock,
panel sequencer and PWM state held steady. USB and Ubuntu return checks
passed. The native restore still needs to carry the backing pin across
reset and reestablish the sampled clock, panel sequencer and PWM state.
[Display-well trial result](../intel-bdw-edp-recovery.md#display-well-reservation-trial).

The PR301 host fixture now connects ordered display disable to the fixed-mode
restore adapter. It keeps PCI and LCPLL running and proves the original
backing again with the eDP pipe off. The isolated native preflight exercises
the route-independent backing census and records the live shutdown
prerequisites. Main remains `4016abf0`, with separate Wi-Fi and WebCore scopes.

Release `81cfc92b023e51c5` verified the same framebuffer translation and
fence digest through the independent native census and repeated the
reference CRC. It returned automatically and passed offline USB checks.
The preflight identified the inherited Pipe A FIFO underrun mask as the
only blocker in its shutdown prerequisite census.
[Display-cycle preflight result](../intel-bdw-edp-recovery.md#display-disable-and-restore-rehearsal).

Release `f9ac1a127a36316f` physically verified the temporary underrun
unmask and exact mask restoration in two sessions. Both stopped after
the panel-off request, before the dark backing check or any restore
writes. The inactive display remained leased and the USB reports
survived.
[Native cycle result](../intel-bdw-edp-recovery.md#first-native-display-cycle).

Release `e3e5a1cc22c5e3a6` completed the isolated native shutdown and
restore. The PPS reached panel-off in 324.117 ms with T12 still active.
The full dark backing digest held and both restored primary CRCs matched
the reference before the panel lit. The underrun mask, driver-well
request and display lease retired cleanly. The single session returned
to Ubuntu with verified USB integrity.
[Panel-off timing retry](../intel-bdw-edp-recovery.md#panel-off-timing-retry).

## PCI reset needs independent display recovery

The Air7,2 advertises PCI AF FLR, which Intel documents as resetting the
display engines and audio codec. PR301 now checks that the exact Intel
diagnostic can record a refused probe and reset in QEMU without a
framebuffer. It also verifies a serial halt without a workspace and
rejects a VM that unexpectedly has a framebuffer. The isolated native
eDP cycle now passes. Carrying the display owner and original backing
through PCI AF FLR, reestablishing the reset display resources, and an
independent memory-completion witness remain open before reset-based
GGTT takeover.
[Reset decision](../intel-completion-prerequisites.md#reset-and-display-recovery-decision).

PR301 owns the PCI AF FLR recovery work on main `4016abf0`, in the
Broadwell display diagnostic and its host and native trial tools.
Release `373cffdfd77b05c3` physically verified a 5,529,600-byte CPU-owned
framebuffer copy and identical PCI snapshots before shutdown, while the
panel was off and after restore. Both restored primary-plane CRCs
matched the reference before the backlight came on. The trial saved its
result, returned to Ubuntu and passed the offline USB integrity check
with zero reset writes. The PCI reset and configuration recovery state
machine passed its fault-injecting host fixture. Native reset requires
proof of requester and memory retirement and a provider that can
reestablish the reset display resources. Wi-Fi and WebCore are separate.
[PCI function reset preflight](../intel-bdw-edp-recovery.md#pci-function-reset-preparation).

Release `2d343940a84a3f33` verified release and reclaim of the driver
well request in 35 µs with the panel dark and firmware holding the well
powered. All framebuffer, PCI and restored CRC comparisons passed.
The one-shot returned to Ubuntu with verified USB integrity and zero reset
writes. Pipe A interrupt mask recovery after reset passed in the host
fixture. A native AF reset still depends on independent requester and
memory-retirement proof and recovery of any display resources that lose
power or state.
[Driver well recovery result](../intel-bdw-edp-recovery.md#pci-function-reset-preparation).

Release `e897744dfc0800ec` verified PCI decode close and memory reopen
with bus mastering inhibited and the panel dark. The CPU-owned copy
retained its reference digest while decode was closed. The first live
validation after reopen failed, so SrvrOS retained the display lease,
left bus mastering inhibited, and halted before panel restore. The
collected result passed offline USB integrity and was rejected as
incomplete. The host restore fixture covers the documented 450 MHz
CDCLK reset default. Native FLR and whole-device memory completion
remain open. [Decode-hold trial](../intel-bdw-edp-recovery.md#pci-decode-hold-and-reset-default-clock).

Release `8f3ddab113407403` traced the refusal to mapping the fence page
at MMIO offset `0x100000`. The display lease, all fourteen platform
registers, and the live PCI endpoint checks passed at Command `0x0402`.
The run saved its diagnostic and halted before invoking reset. USB
integrity passed after the manual return.

Release `c05862653e0d1140` passed the PCI decode hold with cache
admission bound to the original display lease and verified Command
`0x0402`. All 45 reopen checks passed. Both restored CRCs matched
before backlight. The run selected return after save and unmount, and
Ubuntu collected the session with verified USB integrity. The run
closed as `DIAGNOSED` with zero retained state and zero PCI reset
writes. [Decode-hold result](../intel-bdw-edp-recovery.md#pci-decode-hold-and-reset-default-clock).

Release `a112eede6e6eb39c` passed the native dark-panel PWM rehearsal.
It restored the captured period, duty and override with five verified
stores in 111 µs, and both frame CRCs matched before backlight enable.
PCI configuration and the framebuffer were preserved across the cycle.
The run saved and unmounted its USB workspace, returned to Ubuntu and
closed as `DIAGNOSED` with offline USB integrity `PASS`. Native PCI
function reset and whole-GPU memory retirement remain open.
[Backlight PWM result](../intel-bdw-edp-recovery.md#backlight-pwm-recovery).

### Panel power-sequencer recovery checkpoint

Release `b97772bb1e3c7432` passed the native panel timing rehearsal.
After a 501.202 ms T12 wait, five verified stores took 111 µs and
restored the original timing words. The framebuffer, PCI state and
both reference CRCs matched. The run saved, unmounted and rebooted
into Ubuntu, closing as `DIAGNOSED` with offline USB integrity `PASS`.
Native PCI function reset and whole-GPU memory retirement remain open.
[Panel timing result](../intel-bdw-edp-recovery.md#panel-power-sequencer-timing-recovery).

### Joined PCI reset recovery checkpoint

PR301 on main `4016abf0` owns joining the verified PPS and PWM
recovery to the modeled Air7,2 PCI reset and eDP source cycle. Work
covers the Broadwell recovery core and its host fixture. The same
display owner and PCI memory decode must survive into resource
verification, and captured timings and duty must agree before the
panel can light. A native AFCTL write still depends on independent
GPU memory retirement and native recovery of all reset resources.
The open Wi-Fi and WebCore PRs have distinct scopes.

The joined host fixture now models AF reset, T12, PPS and PWM
reprogramming, and both reference CRCs under the original owner.
The source refuses panel power if the restored resources are missing
or changed, and keeps the backlight dark on duty drift detected after
the final CRC. Native hardware evidence covers the isolated rehearsals.
[Joined fixture](../intel-bdw-edp-recovery.md#joined-pci-reset-and-panel-recovery-fixture).

### Display power and clock recovery after decode reopen

PR301 on main `4016abf0` owns the bounded Broadwell display-well
and clock-register recovery in `kernel/diagnostics` and its joined
host fixture. The Air's captured 675 MHz LCPLL selection should
survive Device 2 FLR. The provider will require that exact locked
state, own the original display exclusion with memory decode
reopened, reacquire only the driver well request and restore the
known timing encoding and LP clock gate before PPS/PWM recovery.
The joined fixture now exercises both a cold well with default timing
registers and a firmware-powered well with its timing intact. It requires
the original PLL and verifies the driver request and clock readbacks
before PPS/PWM writes. Native requester retirement, an adapter for the
reopened hardware resources, and a physical FLR remain open.
Wi-Fi and WebCore have distinct scopes.

## Owned blitter completion observed on Ubuntu

On the Air7,2 Broadwell GPU, the i915 probe observed 20 fresh 64-bit post-sync
markers and their earlier writes through private PPGTT mappings before calling
the userspace wait ioctl. Ten controls submitted the earlier write alone and
kept the marker at zero. SrvrOS still needs a native provider for its private
command mappings, completion and retirement.
[Ubuntu blitter check](../intel-bdw-bcs-flush.md#physical-ubuntu-reference-probe).

## Native graphics memory arbitration diagnostic complete

PR301 release `9b727e958b7591d3` returned automatically and closed as
`DIAGNOSED`. The graphics memory arbiter reported all six documented
request groups allowed and idle in all three samples. The sample after
all five engines reached Ready for Reset showed the same permission mask
as the one before preparation. Both display packets and all 129 checked
translations were preserved. USB integrity, recovery and the consumed
one-shot verified. An independent completion witness is still required
before a writable GGTT interval can be granted.
[Native arbitration result](../intel-completion-prerequisites.md#native-graphics-memory-arbitration-result).

## Firmware interference detection added to the held GT owner

On main `4016abf0`, PR301 adds a read-only SMI counter check before the
Air7,2 full-GT bootstrap stores and borrow retirement. Ubuntu read the
counter on all four logical CPUs on this unit. The host fixture injects a
count change at every checkpoint and verifies that the original GT owner
stays pinned. Release `fd9743c1fe226cd4` passed both exact-stage QEMU cases.
QEMU refused the native bootstrap before sampling the counter. It saved and
reset with a workspace, and halted visibly when the workspace was missing.
The later native arbitration run sampled the counter 19 times through
retirement and observed a stable count of 12. This covers the boot CPU
checkpoints in that run. GGTT access and GPU completion still need their own proof.
[Firmware interference check](../intel-completion-prerequisites.md#firmware-interference-check-inside-the-original-gt-owner).

## First-page physical disjointness checked

PR301 now joins the candidate's original target to the inherited primary image
and translation tail. The latest archived runs agree that the original points
into firmware-reserved stolen memory and is physically distinct from the 1,484
sampled display pages. The new kernel and collector checks refuse a candidate
that aliases a display page. Host qualification and replay cover the new
check. A live firmware and GPU ownership provider is still required before
the first write. [Physical backing check](../intel-bdw-primary-range.md#first-candidates-original-physical-backing).

## Native GT pipeline and L3 acknowledgement observation complete

PR301 release `9d2b6c6ea286892a` captured three message snapshots and returned
to Ubuntu automatically. All five Prepare for Reset requests reached Ready
for Reset. The samples showed no engine flush-and-block acknowledgement or L3
flush acknowledgement. All 18 reads completed, the three temporary aliases
retired, and USB integrity and recovery checks passed. The run closed as
`DIAGNOSED` under the wider timing policy. Outstanding GPU accesses, prefetch,
and cache completion still need an independent hardware witness before GPU
ownership can be granted.
[Native wire12 result](../intel-completion-prerequisites.md#native-wire12-result).

## Native auxiliary display-reader observation complete

PR301 release `19e263fec39500ce` captured matching control and status words
for VGA, framebuffer compression and eDP panel self-refresh while the
original requester hold was in force. Both samples showed all three paths
inactive. All eight temporary auxiliary aliases retired, all 129 checked
translations matched, and the Mac returned to Ubuntu automatically. The
USB integrity and recovery checks passed. The run closed as `DIAGNOSED` under
the wider diagnostic timing policy. The next ownership checkpoint needs
evidence that outstanding GPU accesses and prefetched reads have drained and
that cache effects have completed before a writable GGTT interval can be granted.
[Native wire11 result](../intel-bdw-primary-range.md#native-wire11-result).

## Native post-maintenance primary display footprint

PR301 release `2945aece351aa904` captured two matching live primary geometry,
reader and display-well samples after reset and memory maintenance, while the
requester was still inhibited. The 112 DWORD reads matched the original
display records and all ten temporary aliases retired. The laptop returned to
Ubuntu, and automatic collection passed. The wire11 follow-up added the
VGA, FBC and PSR register observations in the same held window.
[Native wire10 result](../intel-bdw-primary-range.md#native-wire10-result).

## Native preservation of the inherited display translation tail

PR301 release `12d3110bd495bbc2` completed the wire9 trial and returned to
Ubuntu automatically. The 128 inherited display-tail PTEs and one prospective
GGTT slot matched before and after reset and memory maintenance. Both
exact-image VM checks and all nine local validation jobs passed. The initial
collection needed a read-limit correction for wire9. The reviewed collection
verified the raw checkpoint and USB integrity and closed as `DIAGNOSED`.
Pipeline, cache and display-reader completion remain prerequisites for a PTE
write. See the [native result](../intel-bdw-gt-bootstrap.md#native-wire9-result).

## Bind GGTT publication to the transferred entries

On main `4016abf0`, PR301 binds the original PTE vector in
`scanout_transfer` to every child publication and restore. The one-page
fixture refuses drift before a write and retains exposed backing if its
restore vector no longer matches. The two-buffer fixture verifies that a
conflict in the second buffer's translation tail also blocks the first
buffer from publishing. The first physical write still requires a live
native GGTT owner with all requester, cache and display-reader prerequisites.
See the [transfer contract](../scanout-transfer.md#capture-and-maintained-ownership).

## OS-allocated scanout backing

On main `4016abf0`, PR301 adds an explicit translation tail to the existing
private scanout allocation, mapping and retirement path. The Air7,2 linear
surface has 1,350 image pages and 128 tail pages per buffer. The host fixture
uses two independently backed buffers with aligned GPU bases and verifies
exact restoration across the full interval before releasing the owner.
The native GGTT interval owner, cache/DMA completion and first live plane
transition remain dependencies. See the
[private backing contract](../scanout-memory.md#broadwell-translation-tail).

## Native scanout ownership gate diagnosed

PR301 on main `4016abf0` covers the isolated ownership gate. The native v55
trial from `c767ee23`, release `42cdc13b60340ddd`, closed as `DIAGNOSED` in
session `session-00066087a8a5365f-00`. Both PCI samples agreed, with bus
mastering enabled and transaction pending clear. The 42 fresh display-reader
words matched the earlier sample. The first unmet condition was an explicit
firmware transfer for the two 6 MiB spans. The CPU guard closed and the display
exclusion unpinned in 35.624 ms, with all 64 temporary aliases retired and
zero GPU control or pixel writes.

The matching session, prior USB files, offline allocation and installed
recovery checks passed. Current `42cdc13b60340ddd` and recovery
`9aa8dbf6fbda8252` verified after the return to Ubuntu. See the
[native gate result](../intel-bdw-stolen-scanout.md#native-v55-ownership-gate-result).
The next design step is to establish a source for live scanout ownership.
The [roadmap](../intel-tearfree-roadmap.md#existing-mapping-reservation-bridge)
records what must be proven before a first flip.

## Native CPU mapping guard lifetime complete

Source `e4d7772e`, release `7daeada549485c7e`, v54/wire7/configuration8,
completed the CPU mapping guard diagnostic in session
`session-00066087356fa4c7-00`. The original display exclusion stayed pinned
through guard acquire, check and close, then unpinned cleanly. Each whole-root
walk checked 110 table paths, 56,320 entries and 47,472 leaves. The guard took
24.119 ms and both full passes stayed within their original two-second limits.
All 59 temporary aliases retired. See the [native guard result](../intel-bdw-cpu-backing.md#native-guard-result).

The run is `DIAGNOSED`. Raw receipts, media preservation, all 1,621 prior USB
files apart from expected metadata, four new logs, offline integrity and the
installed recovery and consumed one-shot checks passed. At that checkpoint the release was
`7daeada549485c7e` with recovery `9aa8dbf6fbda8252`. Firmware, DMA, cache and
display-reader ownership remain prerequisites for writable scanout buffers.

## Native CPU backing admission diagnostic complete

On baseline4016abf0, PR301 adds the native provider's CPU backing guard:
firmware-map/PMM reservation checks for both complete candidate spans and a
bounded whole-root search for any existing CPU alias. Scope is the VMM
read-only reverse walker, isolated stolen probe, actual-core fixtures and its
versioned collector/workflow. Existing unique-alias callers keep their original
contract. No target RAM is read or written, no mapping is changed and no
firmware/DMA/cache/display-reader right is issued.

Source `bc8af8e1`, release `6771b4c6bb8713cc`, v53/wire6/config7, run
`air-stolen-cpu-backing-20261117`, session `session-000660858468bf7b-00`
[completes the CPU backing diagnostic](../intel-bdw-cpu-backing.md#native-result).
Both complete6MiB candidates have1,536 reserved firmware carriers and reserved
PMM tags, with no uncovered/other/framebuffer page. The actual whole-root walk
checks110 table paths /56,320 entries /47,470 leaves and finds no intersecting
CPU alias. The CPU phase takes22,924us; both original passes660,826/705,920us
remain below2s. All59 metadata aliases retire with no writes or retained state.

All7 frozen jobs and both exact-stage VMs passed. Matching raw/checkpoint/source,
all1,617 prior USB files apart from expected metadata, four exact new logs,
offline integrity and fresh recovery/default/consumed-one-shot checks verify.
The run is `DIAGNOSED`;19 native evidence files are independently sealed outside
build. At that checkpoint the installed pair was 6771/9aa8. The CPU observation
establishes backing eligibility for the maintained VMM guard described above.

After collection, the standalone generated USB image was losslessly archived
and verified before removing its build copy. The inactive1070 EFI copy was
removed only after full equality with its independent bank and current/recovery/
GUI/protected checks. Banks, raw evidence and the current6771 stage remain.
Root/EFI free space was 644/207 MiB at that checkpoint.

## Native reader/cache preflight complete

Source `2d7b164b`, release `10703e82a2760453`, v52/wire5/config6, run
`air-stolen-owner-final-20261117`, session `session-00066083dd93046c-00`
[completes the live reader/cache diagnostic](../intel-bdw-stolen-scanout.md#native-v52-reader-and-cache-observations-complete).
Both full census passes finish in830,295/406,653us below the unchanged2s bound;
all59 aliases retire, with zero writes/refusals/retained resources. Only primary
A is enabled; cursor/sprite readers are disabled, programmed/live addresses
agree and no sampled starting address hits a candidate. Both complete6MiB
physical spans have MTRR UC0 and effective WC1 at the existing PAT slot5.
Other PAT slots resolve UC0. This is hypothetical CPU policy, not a writable
alias, GPU cache/visibility guarantee or maintained reader retirement.

All7 frozen jobs/both VMs, matching raw/checkpoint/source, USB preservation,
offline integrity and fresh recovery/default/consumed-one-shot checks pass.
The run is `DIAGNOSED`;19 native evidence files are independently sealed outside
build. At that checkpoint current1070/recovery9aa8 and accepted GUI/banks remained.
The v53 result above supersedes the current installation. Next implement the
original live firmware/DMA/cache/all-reader ownership
provider and reservation before the first writable CPU alias. Do not repeat
this completed snapshot or turn saved packets into live authority.

## Earlier private CPU context image milestone

Source `4ce676bd`, release `08ae0d63f7b821c4`, profile `intel-bcs-context-v46`
/wire8, run `air-bcs-context-isolation-20261117`, session
`session-0006607c842689c0-00` completes one original kind2 epoch0→1
[CPU context-image lifetime](../intel-bdw-bcs-context.md#native-result-one-cpu-image-lifetime-complete).
All46 frozen validation slots and both exact-stage VM paths passed before boot.
The matching raw session and original checkpoint reparse strictly; the run is
`DIAGNOSED` and732 independently verified evidence files are banked outside build.

The complete16KiB ring/HWSP/register/completion candidate is constructed and
verified before GPU exclusion, checked at both original held-source guards,
then checked and erased only after actual GT/HDA/outer retirement. All four
CPU aliases retire,2048 zero QWords verify and the original four pages release
once, with DMA57→57. The two temporary UC aliases and15 original GPU aliases
also retire. Proposed GPU addresses remain unmapped; no command is submitted.

| Clock | Observed | Existing practical limit |
| --- | ---: | ---: |
| Preparation including guarded return | 219.532 ms | 250 ms |
| Largest original GT maintenance phase | 77.210 ms | 100 ms |
| Original GT total | 379.409 ms | 500 ms |
| HDA total | 399.125 ms | 750 ms |
| Outer total | 451.007 ms | 1000 ms |
| CPU arena setup / held / cleanup | 87 / 401 / 82 us | Existing parent and100 ms setup/cleanup |
| Image construction / erasure | 9 / 2 us | Inside the original CPU arena bounds |

The same owner completes both reset pulses/five cancellations, all32 fences and
inherited display/backing preservation. Its memory child completes remapper DR/DW
in36.987ms and System Agent GGTT invalidation in2.321ms (83.119ms child).
All1,590 prior USB files preserve apart from expected metadata; three exact logs
yield1,593files/187sessions. Boot/preflight/exit are9,204/39,096/74,581bytes,
within96KiB. Offline fsck/allocation/writable/headroom, unchanged boot region,
installed/banked recovery, Ubuntu default and consumed one-shot pass.

At the accepted CPU-image checkpoint current08ae/recovery9aa8, acceptedf0a8
and protected installed releases remained. The later census installs9fbb as
current under the unchanged9aa8 recovery; independent accepted banks remain.
Prior a382 remains independently banked; only its equal inactive EFI/staged
copies were pruned to make room for the next read-only image.
USB is unmounted, host volume is1%, this profile is silent and no reboot is armed.
Review current root/EFI capacity before staging; generated copies are retired
only after full comparison with the independent bank.
Only inactive installed/staged434e duplicates were removed after full equality
with its independently verified external bank; bank/raw evidence remain. Preserve
dirty `build/repro-7032f10` and all accepted payload/evidence banks.

This accepts CPU image construction/preservation/retirement in one original
hardware lifetime under the explicit practical policy. It does not accept the
historical stricter preparation/GT limits, actual executable LRC/mode/cache
policy, GPU mappings, DMA/cache/prefetch/firmware/display-reader ownership or
native tear-free. All eight obligations (mask255) remain, with zero managed GPU pages/PTE
writes/submissions. Enabled entry for this source and repeatability remain open.
The prior zero-arena result remains in the [dated journal](macbookair7-2.md#native-full-gt-private-cpu-arena-completes).

## Earlier native layout census

Source `aad02bb5` / release `aa2c4bed41ec15f4` / v51/wire4 / session
`session-00066082b1847677-00` completes the
[read-only existing-mapping census](../intel-bdw-stolen-scanout.md).
An isolated main defers user init for the exact terminal recipe; the unchanged
sole-root and alias checks now pass. Both complete11,776-entry vectors agree.
The passes take479.746ms and232.507ms within their original2-second bounds.
All53 CPU mappings retire, exclusion releases, and there is no retained alias,
refusal, clock failure or GPU write. Strict UC3,107 cache checks,70,770 alias
checks,23,552 translation checks and8 metadata reads verify.

| Range | GPU interval (exclusive end) | Physical interval (exclusive end) |
| --- | --- | --- |
| Complete inherited fence | 0–0x2200000 (34MiB) | 0x8c001000–0x8e201000 |
| First6MiB candidate | 0x2200000–0x2800000 | 0x8e201000–0x8e801000 |
| Second6MiB candidate | 0x2800000–0x2e00000 | 0x8e801000–0x8ee01000 |

These are observed existing translations within sampled stolen-memory bounds,
not allocated/free buffers or transferred firmware memory. Sampled FBC/PSR gates
are clear; maintained DMA/cache/prefetch/display-reader ownership remains open.
The candidate cold alias preflight retires before any metadata load.

All7 frozen steps,27 census/boot/VMM/receipt methods,197 source/return methods,
22 additional legacy boot dispatchers and both exact-stage VM paths passed.
Exact four raw files/recipe/journal/exit, prior USB contents, boot region,
offline integrity and fresh installed/banked recovery/default/consumed-one-shot
checks pass. Raw hashes/checkpoints are reparsed before the run closes DIAGNOSED;
self-contained native evidence is independently sealed outside build. At that
checkpoint currentaa2c,
recovery9aa8 and accepted GUI/protected banks remained; USB was unmounted and no
trial was armed. Earlier refusals and startup-conflict evidence remain in the
[dated journal](macbookair7-2.md#native-pre-init-read-only-census-completes).

The next bounded milestone is explicit original-lifetime borrowing of these
candidate spans, including firmware reservation and maintained engine/DMA/cache/
prefetch/display-reader exclusion before any RAM store. Then bind exact CPU
aliases/visibility, the first inherited-X-tiled-to-linear primary transition,
actual live latch and independent old-reader retirement, followed by compositor,
restoration and endurance qualification. Keep the first-write refusal until the
required live ownership witness exists. A stable layout or CPU image is not that
witness; native tear-free remains unaccepted.

The [primary-A flip core](../intel-bdw-primary-flip.md) is host-qualified for
10,000 single-leg cases plus rollback/CRC trials, bounded polling and live-latch/reader
handling, but has no native caller. All broader bootstrap ownership obligations
remain open. Ordinary desktop startup is unchanged; this exact terminal image
has its own recipe-gated pre-init dispatch.

Integration main4016abf0 and draft PR301 own this scope. PR19 Wi-Fi and PR379
WebCore do not overlap; no agents/chats were contacted or Actions requested.

## Existing-mapping borrow backend: host-qualified

The [reservation backend](../intel-bdw-stolen-borrow.md) now binds an original
live source/provider/root/epoch and the exact classified pair to the existing
scanout_memory lifecycle. It pins real CPU display exclusion, registers children
before callbacks and stops new CPU borrows while original users drain. Returning
a child never frees firmware RAM or returns the outer reservation; final return
requires fresh provider proof. Missing rights refuses before admission; later
uncertainty retains the original source, backing and exclusion.

The actual backend/memory/hold/exclusion pass7,360 cases,16 runtime-detected
compiled controls, fatal sanitizers and native2KiB frames, with the existing
memory/hold/transfer/handoff regressions passing. It is excluded from ordinary
image linkage and has no native ownership producer, CPU alias or RAM/PTE writer.
No new trial is armed. Next supply the live reservation/firmware/DMA/cache/reader
provider specified in the contract; the completed census is historical data,
not that live source. PR301 owns the software bridge; other device scopes remain
unchanged and no Actions/other agents were used.

## Qualified software prerequisites

The [mapping transaction](../intel-managed-memory-plan.md) supports the existing
practical250/500/750/1000ms aliases, retaining both legacy policies and nested
100ms transfer/publication caps. Three policies pass26,712 composed cases,
25 compiled controls, fatal sanitizers and2KiB native compiler frames.

The [primary-range classifier](../intel-bdw-primary-range.md) passes18,216 cases,
12 controls, fatal sanitizers and native frames/image isolation. It includes tile
padding,256KiB alignment,128-PTE display tail and full protected-fence separation.
Saved-Air replay is offline geometry, not a live reader/ownership token.

The [primary flip protocol](../intel-bdw-primary-flip.md) has no native caller.
Its finite9-read polling batch and100ms original operation bound cover arm and
live-latch checking without a frame-wait loop. It supplies no old-reader
retirement, GPU mapping or native tear-free grant. Related full-fence geometry,
100,000 buffer-lifetime interleavings and publication/restore regressions pass.

The [submit port](../intel-bdw-bcs-submit.md) and actual joined flush/nonce cores
pass2,519 port,2,326 flush and35 joined cases per policy,24 new plus24 existing
controls, fatal sanitizers and native frames. Port issuance is separate from
command completion and retirement; all partial/uncertain effects retain backing.
No native image calls these submission/completion cores.

The [CPU image codec](../intel-bdw-bcs-context.md) passes4,463 decode/all-word-
corruption/input cases and11 controls. The actual context adapter passes72 new
and292 original retained-prefix faults on two modeled routes, three native
controls, fatal sanitizers and2KiB frames. The complete PCI matrix passes8,237
fault cases. All42 prior frozen slots remain among the46 qualified slots.
Earlier failed candidates and staging/collector history remain in the
[GPU qualification history](macbookair7-2-gpu-history.md) and dated journal.
