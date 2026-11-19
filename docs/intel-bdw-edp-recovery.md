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
