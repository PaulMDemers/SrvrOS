# Broadwell eDP recovery on MacBookAir7,2

The recovery core targets the built-in 1440×900 panel on this unit. It programs
the panel power sequencer and trains the sink through DDI A AUX. A future
native provider must own the display wells, clock, DDI and scanout backing for
the entire operation. The core currently runs in a host fixture.

## Unit reference

Ubuntu i915 reports pipe A, embedded Transcoder eDP on DDI A, six bits per
color component, one 2.7 Gb/s lane, and the following adjusted mode. The
read-only SrvrOS capture from release `9b727e958b7591d3` corroborates the active
geometry, horizontal and vertical totals, embedded transcoder, SST, color
depth, and lane count. Its primary stride is 6144 bytes with X tiling.
The Ubuntu sample does not establish the complete inherited clock or backing.

| Setting | Observed value |
| --- | --- |
| Pixel clock | 88.890 MHz |
| Horizontal active, sync start, sync end, total | 1440, 1488, 1520, 1600 |
| Vertical active, sync start, sync end, total | 900, 903, 909, 926 |
| Sync polarities | Negative H and V |
| Receiver revision, maximum link-rate code, lane/framing byte | `11 0a 81` |
| Active link-rate, lane/framing, training pattern | `0a 81 00` |
| Active source PLL | LCPLL 1350, doubled on DDI A |
| Lane 0 and alignment status | `07` and `01` |
| Coding, downspread, alternate scrambler | 8b/10b, off, off |
| Panel up, down and minimum cycle | 200 ms, 74 ms, 500 ms |
| Backlight on and off delays | 210 ms, 250 ms |

The Linux reference came from `i915_display_info`,
`eDP-1/i915_panel_timings` and reads from kernel-owned `/dev/drm_dp_aux0`.
The PCH LPC controller is Wildcat Point-LP `8086:9cc3`. The core uses the
24 MHz low-power PCH divisor and the panel delays above. The surrounding
owner must enforce the 250 ms backlight-off delay before a later power-down.

## Recovery and acceptance

1. Admit the exact powered PPS, original backing and display route after a
   terminal reset. The native owner must establish CDCLK and the source
   clock, satisfy the LP PPS clock-gate restriction, prepare PWM, and
   exclude PSR, competing AUX access and independent framebuffer clients.
2. Wait for the reset power-cycle timer and program the PPS timing while the
   panel is off. Keep the backlight dark. Wait at least 200 ms from the
   power-on request and require the sequencer to be idle and on before AUX.
3. Use the DPCD 1.1 one-lane enhanced-framing profile. Retry a bounded
   native DEFER and abort other failed or ambiguous transfers. Train TPS1
   and TPS2, require clock recovery, equalization, symbol lock, and
   interlane alignment, then request source idle. The source provider
   enables the prepared scanout while idle. The core disables sink training,
   switches the source to normal, and verifies final sink config and status.
4. Verify original live scanout and advancing frames before and after a
   210 ms dark interval. Verify the panel is still on and the backlight
   permission bit is clear, then set the bit and read it back. If any
   issued operation is uncertain, hold its original lifetime. The
   enclosing boot diagnostic owns terminal save and recovery.

The joined fixture passes 24,485 cases and runs with fatal ASan/UBSan. It injects
faults across register and source accesses, tests the wire and stage order,
and checks that missing scanout keeps the backlight dark. The freestanding
x86-64 object passes a 2 KiB stack-frame limit. AUX and link training have
independent 10 ms and 150 ms ceilings.
The complete PPS and link sequence has a new 1.6 s ceiling for its physical
panel delays. Existing GT, MMIO ownership and GGTT deadlines are unaffected.

## Source restore

The joined fixture now runs the source register adapter through the actual
PPS and AUX code. The adapter checks the reset state of the route, idle
DDI A, a locked LCPLL, the live CDCLK encoding, and a clean sticky pipe-A
underrun latch before it allows panel power. After panel power it selects
LCPLL 1350 and replays nine PHY entries supplied by the board's pre-reset
owner. Each AUX and source operation rechecks panel power. The source
selects TPS1 before enabling the DDI buffer and waits at least 520 µs before
starting sink training.

While the trained link is idle the adapter sets the embedded transcoder
timings, 64-symbol transfer unit, video and link M/N, and the pipe A
watermarks for the observed 2.0 µs WM0 latency. The primary plane uses
the original X-tiled XRGB8888 backing at GGTT offset zero and the captured
6144-byte stride. A complete mapping pin includes the tiled row padding and
128-PTE tail; the inherited color LUT is bypassed for the first recovery
mode. Normal transport starts after the transcoder and plane are armed.

Each live check requires at least two advancing frames with the configured
route, plane base, stride and offset still in place, and no sticky FIFO
underrun. The restore adapter then runs the primary-plane CRC capture used
in the inherited trial, rechecks the replayed PHY and original full PTE
census, and compares the stable result to the pre-reset reference. Both
checks must match before backlight permission. The fresh-frame check keeps
its 60 ms bound. CRC sampling and teardown keep their 200 ms and 65 ms
bounds within the existing PPS deadline. The fixture exercises the actual
CRC poll and teardown and refuses to light the panel on a missing or
mismatched result, a changed backing, or uncertain retirement.

## Inherited display hardware trial

The isolated `intel-edp-baseline-v1` recipe keeps the inherited panel
running. It pins the CPU display lease, revalidates the tiled primary and
all 128 tail PTEs, samples the nine DDI A PHY translations and relevant
clock/route registers, and runs the Broadwell primary-plane CRC tap. The
adapter excludes PSR, FBC and IPS, checks the boot CPU's SMI count and
keeps the pinned resources on uncertain retirement.

Only CRC-done is locally unmasked, with interrupt delivery disabled. The
probe leaves the CRC comparison-error event masked, as i915 does for
capture, and records the observed interrupt status in each window. Four
fresh completions must cross distinct frame counters. The first two
warm-up results are ignored and the next two must agree. Then the adapter
disables CRC, waits for retirement, restores the exact local interrupt
mask, and compares the PHY and backing again. The current recipe clears
and renders the same fixed reference image before and after a distinct
alternate image. All three windows must use the same route and backing,
and the two reference CRCs must agree and differ from the alternate CRC.

The trial uses CRC control and its local interrupt state. The final reference
window also exercises the driver's display-well request as described below.
It grants no GGTT write access and leaves the PCI function untouched.
This can qualify the pre-reset observation and the CRC tap on the inherited
route. A later reset trial must still establish MMIO and the framebuffer
lifetime after reset, reproduce a known frame, and verify a matching CRC.
Qualify display and sound recovery before PCI reset-based GGTT takeover.
A separate live memory-completion witness is required before any GGTT store.

### Physical pre-reset result

Release `5d36f1891358bf06` on MacBookAir7,2 observed four consecutive CRC
completions in each window. After warm-up, frames 1034–1035 each returned
`0x5d3928e4` and frames 1040–1041 each returned `0x124ccc05` after the
console content changed. Both windows reported only CRC-done in the sampled
interrupt status. Their nine PHY entries and backing hash `0x012970c1aee06011`
matched. The probe retired both windows without retaining resources,
performed zero PTE writes, and saved a report that passed Ubuntu return
verification.

Release `43785d3ecb432ae1` then verified the fixed A–B–A sequence on the
same MacBookAir7,2. After warm-up, the reference image returned
`0xc2798333` on frames 960–961 and again on frames 972–973. The alternate
returned `0xe3219ccc` on frames 966–967. All three windows preserved the
nine PHY entries and 1,484 backing translations, restored CRC control and
the local interrupt mask, and passed offline return verification. The
restore provider must still capture and bind the reference in the same
ownership lifetime as the native reset.

### Display-well reservation trial

The final reference window adds the Broadwell driver request at
`0x45404[31]` while the BIOS request remains asserted and the global well
reports powered. The trial checks the original tiled backing before writing,
keeps the CPU/framebuffer pin and UC aliases throughout the CRC window,
and releases only the driver request it set. Four samples must agree
except for that bit in the two held samples. The vector includes the LCPLL,
CDCLK and fuse, Lynx Point-LP clock gating, panel sequencer, CPU and PCH
PWM controls. These registers are sampled read-only. The trial repeats the
full original backing census after release. Uncertain retirement keeps
the exclusion and aliases pinned.

The restore entry now rejects a missing, altered or unretired pre-reset
CRC receipt before any platform access. It checks the receipt's verified
frame sequence, PHY, lane configuration and backing against the selected
restore image. The future native reset owner must keep that receipt and
the framebuffer pinned while the PCI function and display state are
restored.
[i915 global well request and readback](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_display_power_well.c) ·
[i915 Lynx Point clock gating](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_clock_gating.c).

On MacBookAir7,2, release `d68c99c294da7d5c` passed the driver-well
cycle in 94.452 ms. The four samples differed only in the driver request
bit. They recorded LCPLL `0x4c000000`, the 675 MHz CDCLK encoding
`0x2a2`, the Lynx Point-LP clock-gating bit set at `0xc2020[12]`,
PPS status/control `0x80000008`/`0x7`, and PCH PWM override enabled
with CPU PWM disabled. The reference CRC under the driver request was
`0xc2798333`, equal to both the first window of this run and the earlier
fixed-frame trial. The original backing remained intact, the request
retired cleanly, and USB return verification passed. These observations
cover the inherited display with the firmware request still asserted.

### Display disable and restore rehearsal

The new cycle core holds the original provider, framebuffer and CRC
reference while it turns off the backlight, waits the panel's 250 ms
backlight-off delay, and disables and latches the primary plane. It
observes two new frame counters before disabling the transcoder, waits for
its active bit to clear, parks DDI A and its transport, powers off the
panel, and disconnects the port clock. The LCPLL, PCI function and GGTT
remain running. It verifies the original backing while the pipe is dark.
The existing source restore then trains the link and verifies two matching
CRC windows before enabling the backlight. Any possibly issued shutdown
write followed by failure retains the provider and backing. The host
fixture exercises the actual disable and restore code and injects write
failures, lost frame progress, backing drift and CRC mismatch.

The native preflight tests the route-independent census without turning
off the display. It brackets all 1,484 exact PTE reads with the original
PCI/framebuffer endpoint and all 32 fences. Its receipt records the
underrun interrupt mask, enable and status, AUX busy, panel fitter, display
audio outputs, other DDI buffers and low-power watermarks. A blocker can
coexist with a verified preflight receipt. It identifies preparation the
native display-cycle owner must still do.
[Intel Broadwell port disable sequence, p. 100](https://cdrdv2-public.intel.com/690828/intel-gfx-prm-osrc-bdw-vol-11-display.pdf#page=114).

On MacBookAir7,2, release `81cfc92b023e51c5` completed the native
preflight and returned to Ubuntu with offline USB integrity verified.
The independent backing census matched the route-bound digest
`0x012970c1aee06011`. The reference CRC repeated at `0xc2798333` around
alternate CRC `0xe3219ccc`; the driver-well request retired in 94.102 ms.
The sole shutdown blocker was the masked Pipe A FIFO underrun bit
(`IMR=0xffffffff`, `IER=0`, `IIR=0`). The native cycle needs to temporarily
unmask that status bit for polling while interrupt delivery stays disabled,
then restore the inherited mask. The panel stayed active during this trial.

The separate `intel-edp-cycle-v1` boot profile keeps the final reference
frame pinned and reacquires the driver-well request. The cycle opens only
Pipe A's underrun mask bit and keeps IER disabled. It preserves the live
PPS timing registers, PWM and the inherited gamma LUT selection. The raw
receipt records the dark backing digest, two restored CRCs, all three
mask values and well/lease retirement. A possible display write without
verified recovery keeps the display lease pinned and saves diagnostics
if the USB workspace is available. A safely retired outcome returns
through the normal Ubuntu boot.

#### First native display cycle

Release `f9ac1a127a36316f` produced two separately saved MacBookAir7,2
sessions, at 12:03 and 12:33 p.m. EST on November 19. Both repeated the
baseline and physically verified the mask sequence
`0xffffffff → 0x7fffffff → 0xffffffff`. Both stopped at the panel-off
stage with a clock/deadline error after eight shutdown writes. They
issued the panel-off store and never recorded the port-clock park,
dark backing check, link restore or restored CRC. Retained display and
well ownership blocked the automatic reboot path.

Mask cleanup replaced the failing register sample in the receipt, so
the last raw PPS status and the exact clock check remain unknown.
Both raw sessions passed the frozen checkpoint and receipt parsers.
Offline allocation and prior-file checks passed. The collector refused
the unexpected pair and both were reviewed and rejected as retained
failures. The evidence does not identify what selected the second boot.

#### Panel-off timing retry

The inherited `PP_OFF_DELAYS=0x02e409c4` programs 250 ms for T9 and 74 ms
for T10. The retry gives the panel-off poll 350 ms for both hardware
delays, after the existing 250 ms backlight-to-video wait. The enclosing
shutdown ceiling is 800 ms. The poll requires panel-on and sequencing
progress to clear. The power-on path separately waits for the T12
power-cycle flag to clear before setting the on target. The hardware
delay programming remains intact.
[Intel Broadwell Register Reference, pp. 1232–1236](https://cdrdv2.intel.com/v1/dl/getcontent/690789) ·
[i915 panel-off and power-cycle waits](https://codebrowser.dev/linux/linux/drivers/gpu/drm/i915/display/intel_pps.c.html#596).

The cycle emits a panel-off receipt with the first and last guarded
raw samples, elapsed polling time, sample count and outcome. The
source-register failure survives the underrun mask cleanup, whose error
is recorded separately. The host fixture verifies both completion with
T12 active and a timed-out shutdown with retained ownership.

Release `e3e5a1cc22c5e3a6` completed the isolated native cycle on
MacBookAir7,2. The panel reached off in 324.117 ms after 3,078 poll
samples. The last `PP_STATUS` was `0x08000001`, with panel power and
sequencing progress clear and the T12 flag still set. The dark
framebuffer digest remained `0x012970c1aee06011`. Both restored
primary-plane CRC windows matched the original `0xc2798333` before
the backlight was enabled. The inherited mask, driver-well request and
display lease retired cleanly. The one-shot returned to Ubuntu and
offline USB integrity passed. This result covers the display-only
cycle with the original powered PCI function and translation backing.

### PCI function reset preparation

The `intel-edp-flr-preflight-v1` recipe adds an ordinary RAM backup of
the full 5,529,600-byte aperture view. It copies under the original
display lease and compares every word while the display is dark and
again after restore. Each pass also checks the independent translation
and fence digest. Three live PCI snapshots compare identity, BARs,
command, D0, AF status and OpRegion address. Bytes remain private to the
kernel and the receipt reports only digests and timings. The preflight
keeps PCI decode live through the real display cycle.

On MacBookAir7,2, release `373cffdfd77b05c3` completed this preflight
in session `session-000660a5fa513d15-00`. The copy and both comparisons
took 198.020, 194.973 and 196.825 ms and produced the same pixel digest,
`0x7c16ae0a6bb7718d`. All 16 PCI values matched across the three samples.
The PCI command word remained `0x0007` with bus mastering enabled.
The AF Transactions Pending bit was clear in each sample.

The PPS reached panel-off in 324.073 ms and the dark translation and
fence digest remained `0x012970c1aee06011`. Both restored primary-plane
CRC windows matched `0xc2798333` before the backlight was enabled.
The display lease, driver-well request and CPU backup retired cleanly.
The diagnostic saved and unmounted its workspace, returned to Ubuntu and
passed the offline USB integrity check. The receipt records zero reset
writes.

The host fixture runs the PCI AF reset core at the dark boundary. The
configuration layer requires an independent retirement and resource
admission before writing AFCTL. It waits at least 100 ms before reading
config, then polls identity within the documented five-second bound.
It restores only the original reserved BARs and subsystem identity
when their reset values match, and enables memory decode with bus
mastering and interrupts disabled. Restore then needs a fresh backing
check before it can access the panel and prove the reference CRC twice.
Faults cover a missing retirement witness, uncertain reset store,
timeout, changed BAR and post-reset backing drift.

On the modeled post-reset path, source recovery first revalidates the clock
and backing. It requires an all-masked Pipe A with IER and IIR clear, then
unmasks only FIFO underrun before restore and restores the original mask
after the CRC checks. The fixture exercises an unexpected mask and enabled
interrupt delivery as separate refusals before any restore writes.
[i915 GEN8 display IRQ setup and reset](https://linux.googlesource.com/linux/kernel/git/netdev/net-next/+/7dda2a8bfde23c8f787aa34e5ce40725a6be65b9/drivers/gpu/drm/i915/display/intel_display_irq.c).

The native `intel-edp-dark-well-v1` diagnostic exercises driver well recovery
with PCI decode live. After shutting down the panel it releases and reclaims
only the original driver request bit. It retains the CPU display lease and
framebuffer copy throughout. The firmware request must remain asserted and
the well powered. Each of three samples checks the known clocks and panel
timings, with the panel still dark, inside a 100 ms interval. The native
cycle then compares the framebuffer and checks both restored CRCs before
lighting the panel. This qualifies reestablishing the driver request under
the firmware hold. Recovery from an actually unpowered well still needs
its own initialization and qualification.

On MacBookAir7,2, release `2d343940a84a3f33` completed the driver-well
trial in session `session-000660a69c5f8f8a-00`. The release and reclaim
took 35 µs with exact readback. The driver register changed from
`0xc0000000` to `0x40000000` and back. The firmware register stayed
`0xc0000000`, and the panel status stayed `0x08000001` across all three
samples. The framebuffer copy matched byte for byte, all three PCI
snapshots agreed, and both restored primary-plane CRCs matched
`0xc2798333` before the backlight came on. The original mask, well
request and display lease retired cleanly. The saved run returned to
Ubuntu and passed the offline USB integrity check. The receipt records
zero reset writes.

#### PCI decode hold and reset-default clock

The `intel-edp-pci-hold-v1` recipe preserves the CPU display lease,
driver-well request and framebuffer backup through an isolated PCI
Command cycle. After the existing shutdown proves the primary, pipe,
port and panel dark, it writes Command `0x0403` to inhibit bus mastering
and INTx. It requires two AF Transactions Pending clear samples at least
100 µs apart within 10 ms. It then writes `0x0400` and checks the
backup using CPU-owned RAM only. It reopens memory decode at `0x0402`
and compares the original translation/fences and every framebuffer word
before restoring the original Command `0x0007`. The sequence and
checks share a two-second bound. Successful readback permits the usual
CRC-checked panel restore. An uncertain step keeps the owner and backup
pinned. AF only describes non-posted transactions, leaving GPU
pipeline, prefetch, posted writes, cache retirement and memory reuse
outside the result. [Broadwell processor datasheet, §§3.2.3 and 3.2.30–3.2.31](https://cdrdv2-public.intel.com/330835/5th-gen-core-family-datasheet-vol-2.pdf#page=73).

Release `e897744dfc0800ec` stopped with the panel dark in Air7,2 session
`session-000660a902d8ddfb-00`. Command readback reached
`0x0007 → 0x0403 → 0x0400 → 0x0402`. AF Transactions Pending was
clear at 103 and 205 µs after bus-master inhibition. The ordinary RAM
copy digest still matched `0x7c16ae0a6bb7718d` while decode was closed.
The first live validation after memory decode reopened did not complete.
The timed hold ended at 2,873 µs, having recorded one pixel sample and no
display restore operations. The receipt does not distinguish the
platform check, translation and fence check, and first aperture
comparison. SrvrOS kept bus mastering inhibited and retained the display
lease and backup for the remainder of the run. Zero PCI reset writes
were issued. The saved result passed offline USB integrity and the run
was closed as a reviewed failure. A retry needs evidence identifying
the first failing check after Command `0x0402`.

The retry records the post-reopen check sequence in ordinary RAM and
emits it with the saved terminal receipt. It identifies the first
failing platform register by offset and observed value, then records
endpoint validation, fence and GGTT mapping and checks, and framebuffer
page and word comparison progress. Fence or PTE mismatches identify the
first entry. Pixel mismatches identify the word index. The hold keeps
its original Command sequence and deadlines. The decoder accepts older
receipts and requires the ordered trace for new runs that reach reopen.

Release `8f3ddab113407403` captured the first failure at fence-page
mapping, offset `0x100000`, in Air7,2 session
`session-000660ac8cb59311-00`. All fourteen platform reads and the
endpoint comparison at `0x0402` passed. The cache preflight independently
called the generic cache-page query, whose endpoint comparison allows
only the original Command in this image. It therefore refused before
mapping. The saved run passed offline USB integrity after the manual
return. No reset or display restore ran.

The held eDP path now revalidates the full PCI endpoint under the original
display lease before every fence or GGTT mapping. Only Command `0x0402`
is accepted during the verified hold. The cache provider then checks
that exact BAR page against the existing allowlist and a fresh matching
PAT/MTRR snapshot. The final log also records whether the eDP trial
needs to halt with ownership retained or can return after saving and
unmounting. The native return still depends on a successful full check.

Release `c05862653e0d1140` completed the decode hold in Air7,2 session
`session-000660acdda71ab0-00`. All 45 post-reopen checks passed,
including all 32 fence registers, 1,484 translations and 5,529,600
framebuffer bytes while bus mastering remained inhibited. The four
Command stores and their readbacks took 197.831 ms including verification.
Both restored CRCs matched `0xc2798333` before the backlight came on.
The final receipt selected return after save and unmount, showed zero
retained state and zero PCI reset writes, and the next Ubuntu boot
collected one session with offline USB integrity `PASS`.

#### Native resource recovery under inhibited PCI bus mastering

Version 3 of the PCI-hold receipt adds two ordered checks after platform
readback at Command `0x0402`. While the panel is dark, the adapter releases
the driver request, reads back `0x40000000`, and runs the shared resource
recovery helper. BIOS keeps the well powered. The only permitted store in the
helper reacquires the driver request. Success requires the ready phase, driver
readback `0xc0000000` and the original PLL, timer and panel clock gate.
Each helper guard checks the original display lease, framebuffer backup,
PCI identity, MMIO BAR, Command and AF status. Failure records the last phase
and retains the PCI hold. Framebuffer verification follows recovery and both
reference CRCs follow display restoration.

Release `40de8de2891163f4`, session `session-000660b7ff7b4292-00`, passed all
47 reopen checks on Air7,2. Checks 21 and 22 recorded the driver changing from
`0x40000000` to `0xc0000000` at Command `0x0402`. The original 32 fences,
1,484 translations and 5,529,600 framebuffer bytes matched. The PCI hold took
197.912 ms and both restored CRCs matched `0xc2798333`. The interrupt mask and
driver request were restored, the final log selected Ubuntu return, and offline
USB integrity passed. This physically qualifies the BIOS-powered path. Cold-well
startup, function reset and post-reset register repair remain pending.

#### GT power state during the dark PCI hold

The version 4 PCI hold receipt adds two GT samples after AF Transaction
Pending clears at Command `0x0403`. Each sample brackets read-only core
status, RC6 residency and forcewake acknowledgement reads with PCI identity,
Command and AF checks. The timestamps are microseconds from PCI hold entry.

Release `c49a10f84b3db492` (source `efcf3ff7`) saved the following samples
in Air7,2 session `session-000660b982e978ae-00`.

| Sample | Begin–end (µs) | Core status | RC6 residency (ticks) | Forcewake ACK |
| --- | --- | --- | --- | --- |
| First | 212–215 | `0x40800000` | 1,594,584 | 0 |
| Second | 2,216–2,219 | `0x40800000` | 1,594,584 | 0 |

Both status reads decode to RC0 with the core on. The counter recorded zero
additional ticks during the interval. The cause and duration of RC0 outside
those samples remain open.
[i915 core-state decoding](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_gt_pm_debugfs.c)

The PCI hold took 199.954 ms including framebuffer verification. All 47
reopen checks passed, all 5,529,600 bytes matched and both restored CRCs
were `0xc2798333`. USB integrity passed offline. A function reset still
requires evidence of request exclusion and completed GPU memory traffic.

#### Backlight PWM recovery

The `intel-edp-dark-pwm-v1` profile captures the inherited PCH PWM control
and duty before shutdown. With the panel off, CPU PWM disabled and the
original display lease held, it zeros duty and disables PCH PWM. The
recovery writes the original period with zero duty, enables the original
override, then restores the captured duty. Every store requires exact
readback. The current profile verifies but leaves the PCH granularity
setting in place. The original framebuffer and two matching CRCs still
have to pass before lighting the panel.
[Broadwell Display PRM, backlight enabling sequence, p. 154](https://cdrdv2-public.intel.com/690828/intel-gfx-prm-osrc-bdw-vol-11-display.pdf) ·
[Linux i915 Lynx Point backlight initialization](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_backlight.c).

The host fixture tests recovery from disabled, zeroed PWM registers and
the native rehearsal sequence. It injects uncertain stores and bad
readbacks at each write and checks refusal with the panel on, with the
wrong PWM route, and after losing the lease or clock. The native profile
leaves PCI decode and the display clock running. Physical function reset
and whole-device memory retirement remain separate prerequisites.

Release `a112eede6e6eb39c` passed the native rehearsal on Air7,2 in
session `session-000660ad1e06b20d-00`. All five PWM stores and readbacks
completed in 111 µs. The original control `0xc0000000` and period/duty
`0x0ad90184` were restored exactly. Both primary-plane CRCs matched
`0xc2798333` before backlight enable. All three PCI configuration
snapshots and framebuffer digests matched across the cycle. The run
released the display lease, saved its logs, unmounted the USB workspace
and returned to Ubuntu. Collection reported offline USB integrity
`PASS` and diagnosis `EDP_DARK_PWM_COMPLETE`, with zero PCI reset writes.

#### Panel power-sequencer timing recovery

The `intel-edp-dark-pps-v1` profile captures the original Air7,2 timing
words. With the original display lease and driver well held, it completes
the usual display shutdown, then waits up to 525 ms for the panel's 500 ms
T12 cycle delay to finish. The panel power, backlight and VDD override
requests must be clear and the LP clock-gate workaround must stay set.
The rehearsal clears the on- and off-delay registers to their documented
reset values and verifies both readbacks. It leaves the LP reference
divider and nonzero T12 encoding in place. It then writes back all three
captured timing words within 25 ms and verifies exact readback before
any panel power request. The usual pinned framebuffer and both reference
CRCs still gate backlight enable. The trial leaves PCI, LCPLL and PWM
running and issues no function reset. The host fixture also tests the
documented reset values with T12 active at entry and injects failures at
each store and readback.
[Broadwell Register Reference, pp. 1230–1236](https://cdrdv2.intel.com/v1/dl/getcontent/690789).

Release `b97772bb1e3c7432` passed on Air7,2 in session
`session-000660ae54e01a39-00`. Panel-off took 324.146 ms. The T12 wait
took 501.202 ms against the 525 ms limit, and the status changed from
`0x08000001` to `0`. Five verified stores took 111 µs. The on/off
timings went to zero and back to `0x007d0834` and `0x02e409c4` while
the divisor stayed `0x0004af06`. All three framebuffer digests and PCI
snapshots matched. Both restored frame CRCs matched `0xc2798333` before
the backlight returned. The display lease retired, the run saved and
unmounted the USB workspace, and the laptop rebooted into Ubuntu.
The collector closed as `EDP_DARK_PPS_COMPLETE` with offline USB integrity
`PASS` and zero PCI reset writes.

The Broadwell register reference specifies that Device 2 FLR preserves
`LCPLL_CTL`. The modeled AF reset therefore preserves the Air's locked
675 MHz selection. A distinct register, `CDCLK_FREQ`, supplies the
timer encoding. If it reads the documented default 449 under the
original locked PLL, the provider restores 674. The source requires the
original clock and uses AUX divider 338 for the 2.7 Gbit/s eDP link.
Actual values after native FLR remain unobserved.
[Broadwell Register Reference, pp. 225, 927–929](https://cdrdv2.intel.com/v1/dl/getcontent/690789).

The recovery helper runs with memory decode reopened, bus mastering
inhibited and the original display and backing excluded from other
users. It handles both an off display well with all requests clear and
a well still powered by the original BIOS request. It sets only the
driver request and waits for acknowledgement inside 1 ms. After
confirming power, it repairs the timer encoding where required and
restores the LP PPS clock-gate bit if clear. It compares readbacks and
the original PLL and keeps the driver request through panel recovery.
Unknown clock encodings, foreign requests, changed lock and uncertain
stores stop the recovery before any panel power request. The helper
implements no PLL, power-controller mailbox or firmware well writes.
[i915 driver well request and readback](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_display_power_well.c) ·
[Broadwell PPS clock-gate workaround, p. 1231](https://cdrdv2.intel.com/v1/dl/getcontent/690789).

#### Joined PCI reset and panel recovery fixture

The reset core begins with the panel dark, PCI Command `0x0403`, MSI off and
the original requester held. While GT MMIO is accessible, it reads the
power manager's memory request (`GO_GAM_REQ`) and the arbiter response
(`MSG_GO_GAM`). Both must initially allow all six documented clients.
The five engine words must show Go and a clear flush acknowledgement.
Both L3 acknowledgement bits must be clear. A registered provider then
issues one block request. The core requires the request and response masks
to reach zero, all five engines to acknowledge flush and block with Go
clear and busy indicated, and a fresh L3 flush acknowledgement. It latches
that L3 event under the exclusive request because GPM can clear it before
the engines finish. An observed L3 unblock ends the attempt.

With GAM idle, both video engines must report zero outstanding reference
and prefetch reads. The core rechecks the engines, idle status, L3 unblock
and both GAM words before closing decode. Acquisition, observation and
decode closure share a 10 ms budget. A consumed descriptor cannot issue a
second request after a partial failure. These are sufficient conditions for
this experimental protocol. The provider also owns the other traffic paths
and must keep new activity excluded throughout.
[Intel Broadwell PRM Volume 2c, pp. 587–592, 805–814, 859, 994, 1034](https://cdrdv2-public.intel.com/v1/dl/getcontent/690789)

At Command `0x0400`, the core executes the graphics remapper's global
read/write drain. It checks AF status, interrupt controls, the subsystem and
the saved BARs, verifies the drain clock and retirement of the temporary
mapping, and rechecks PCI state before AFCTL. The CPU-side ownership check
also runs at the reset store and throughout config-space recovery. The
100 ms quiet interval starts when the reset write returns.

The host fixture models the GPM request and its register responses. It
injects stale or missing acknowledgements, reopened clients, individual
busy queues, elapsed deadlines and loss of ownership at AFCTL. The Air's
GT3 media diagnostic observed all six clients allowed. Replaying its
response words with a modeled block request prevents decode closure and reset. A native
test of this path requires an exclusive request provider with a documented
trigger and a way to observe the transaction under the dark-display hold.
The message response bits belong to the hardware protocol.
[Native GT3 media observations](intel-completion-prerequisites.md#macbookair72-media-counter-result)

Only requests already processed by the graphics remapper fall within the
drain's scope. The physical provider still has to keep new requests excluded
and account for work inside the GPU and paths outside the remapper. A drain
from an earlier diagnostic cannot be replayed. The fixture exercises the
actual drain and reset code with modeled hardware and requester ownership.
[Graphics remapper coverage](intel-vtd-drain.md#what-completion-would-establish)

The host fixture runs AF reset, reopens memory decode with bus
mastering inhibited and restores the display well and clock registers.
It uses the PPS and PWM recovery routines from the native rehearsals,
starts T12 again at the reset store and models cleared delay and PWM
timing words. The source requires
the captured PWM word, the original PPS settings and the LP clock-gate
workaround before rearming Pipe A and requesting power. It uses the
restored timing words for the complete cycle and checks them and the
original PWM state again after each frame CRC. The fixture rejects a
missing pre-reset PWM sample, a changed pre-reset duty, missing or failed
resource recovery, and PWM drift during the last CRC. A failure after
reset leaves the original display owner pinned and the backlight off.
Existing native profiles exercise display-only recovery. No current
profile has a native AFCTL writer.

The native AFCTL store depends on a live requester and memory-completion
witness, a CPU-only reset hold while MMIO decode is unavailable, and a
provider that can restore the actual clock, well and interrupt state
after reset. The saved pixel copy establishes a reference independent
of the device. Replaying a changed GGTT or framebuffer still requires
the matching translation and cache ownership.
[Intel Broadwell AFCTL, Register Reference p. 2](https://cdrdv2.intel.com/v1/dl/getcontent/690789) ·
[Intel Broadwell processor datasheet, §§3.2.29–3.2.31](https://cdrdv2-public.intel.com/330835/5th-gen-core-family-datasheet-vol-2.pdf#page=86) ·
[Linux AF reset implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/pci/pci.c#L4304).

References for the AUX controller and PPS are Broadwell Register Reference
Volume 2c, pp. 343–345 and 1230–1236. The source sequencing and delay policy
were checked against the i915 implementation. [Intel Broadwell PRM index](https://www.intel.com/content/www/us/en/docs/graphics-for-linux/developer-reference/1-0/broadwell.html) ·
[i915 AUX](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_dp_aux.c) ·
[i915 DDI](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_ddi.c) ·
[i915 PPS](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_pps.c).

The source register sequence and watermark formula follow Broadwell Volume
11, pp. 102–103 and 183–184. The PLL, timing and source-clock state were
cross-checked with i915. [Intel Broadwell Display PRM](https://xorg.freedesktop.org/docs/intel/BDW/intel-gfx-prm-osrc-bdw-vol11-display_0.pdf) ·
[i915 timing and M/N](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_display.c) ·
[i915 CRC sources](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_pipe_crc.c) ·
[i915 CRC-done interrupt setup](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/i915_irq.c).

### Display owner block lifetime

In PR301 the provider receives the original reset-attempt object when it
issues its request. Its CPU-side ownership check must identify that same
request and display epoch through PCI decode closure, the remapper drain,
the reset store and decode recovery. An uncertain issuance consumes the
attempt. Before closing decode the core rejects any observed reversal in
the six request bits or six response bits, loss of an engine flush
acknowledgement, a previously blocked engine resuming, an L3 unblock or
a foreign FLR preparation. The final PCI profile check precedes the final
queue, engine, idle, L3 and arbiter reads.

The active trigger in the recovery test is modeled. On Broadwell,
`GO_GAM_REQ` carries GPM's request to the arbiter. `MSG_GO_GAM`, the
five engine messages and the L3 message report hardware state. Intel
describes the GPM as the originator and the L3 events as self-clearing.
The register descriptions do not establish a CPU protocol for acquiring
and holding ownership by overwriting the message bits.
[Intel Broadwell register reference, pp. 587–592, 805–814, 859](https://cdrdv2.intel.com/v1/dl/getcontent/690789)

Broadwell's `MI_MODE[8]` Stop Rings has a documented erratum. Stopping a
running batch can reset its memory access type, so software cannot use this
bit to pause the command streamer and then resume that batch after the PTE
exchange. Intel permits its use before an engine reset. [Intel Broadwell
register reference, p. 1322](https://cdrdv2.intel.com/v1/dl/getcontent/690789)

A second candidate is i915's software RC6 park request. Linux v6.12 issues
it after enabling RC6 on the initialized GT. On Broadwell, i915 uses the
render-forcewake domain to read offsets below `0x40000`, including the
request, five engine messages and L3 acknowledgement. Wake acquisition changes
the power-state conditions under which those observations would be made.
[i915 software park](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_rc6.c#L692-L718)
· [Register ranges](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_uncore.c#L1231-L1233)
· [Gen8 range selection](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_uncore.c#L2448-L2452)

The Air7,2 Ubuntu kernel `7.0.0-43-generic` produced the following trace
in three 1.5-second intervals. A private ftrace instance counted i915
function entries while sysfs supplied the GT0 RC6 residency counter.

| Interval | Park calls | Unpark calls | RC6 residency gained |
| --- | ---: | ---: | ---: |
| Before forcewake | 60 | 61 | 1,075 ms |
| Forcewake held | 0 | 0 | 0 ms |
| After release | 49 | 48 | 1,145 ms |

This trace tests the wake dependency on the Ubuntu driver. It supplies no
engine or L3 completion receipt for SrvrOS. Enabling the native reset still
requires a hardware block that survives the acknowledgement reads and remains
under the original owner after decode closes, or an independently readable
record of the handshakes under that same hold.

### Reset-free use of the same traffic checks

The slot 8704 writer witnesses an initially open arbiter, issues one
request for the original publication, and reads the GAM request before
the engine and L3 messages on each poll. Each engine's flush and GO
acknowledgements require its own observed request closure. The writer
checks WiDi GO against GAM bit 6. A one-shot L3 acknowledgement can
arrive after the first observed request closure while the final engine
is still draining. The writer also checks the
power-manager flush policy for every bank in both enabled slices. Volume
13 identifies the register range as replicated per bank and slice, and the
Broadwell GT3 has eight L3 banks. Each addressed read must succeed under
the same owner and restore any borrowed steering. [Intel Broadwell MMIO,
p. 3](https://cdrdv2-public.intel.com/690867/intel-gfx-prm-osrc-bdw-vol-13-mmio.pdf#page=7)
· [Mesa Broadwell GT3 configuration](https://chromium.googlesource.com/external/gitlab.freedesktop.org/mesa/mesa/+/b1c3e36fe322a7330d60d4f70c37b64aff33d723/src/intel/dev/intel_device_info.c)

Bit 4 of `SCRATCH1` at `0xb11c` limits a power-manager flush to coherent
lines when set. The slot writer requires coverage of coherent and
noncoherent lines. WiDi has its own GO acknowledgement in `MSG_IDLE_WIN`
and must report busy throughout the block. The five command engines
additionally acknowledge flush. [Intel Broadwell register reference,
pp. 815–816 and 1390](https://cdrdv2.intel.com/v1/dl/getcontent/690789)

The writer samples the enabled primary plane and its programmed and
live addresses before requesting the block. It checks them against that
snapshot before and after each store, after each posted invalidation, before
releasing the block, and again after requests reopen. Loss of a hardware
acknowledgement or movement of the plane after a store retains both the backing
and the exclusion, even when the software owner epoch still matches. The same request must cover PTE replacement,
invalidation,
restoration and retirement of readers. Returning the register alias and display
exclusion requires a confirmed release and reopened client and engine
messages. An uncertain release consumes the attempt and retains the
reservation. The host fixture exercises these transitions. The native
Air7,2 still needs a source of the request that can hold it while the
original panel scans and the driver reads the acknowledgements.

With forcewake held on the Ubuntu Air7,2, the CPD policy sampled as locked
with GAM GO messaging disabled on three reads. The register controls whether
a power transition sends the messages. It does not initiate the transition.
The version 4 context diagnostic samples the same policy under SrvrOS.
[Air7,2 measurements](intel-bdw-bcs-context.md#widi-submission-coverage)

A different integration path would give the native driver ownership of a new
GGTT interval and every submission that can reference it. Linux i915 on
Gen8 reserves the initial scanout during rebinding, writes PTEs for driver
allocations and invalidates the GGTT. That sequence assumes an initialized
driver and allocator. Slot 8704 on this Air still contains a firmware entry.
Adopting this approach would require a handoff that accounts for inherited
readers before importing any interval into the allocator.
[i915 initial plane reservation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_plane_initial.c#L196-L230)
· [Gen8 GGTT binding](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c#L405-L422)
