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

The fixture passes 1,158 cases, including fatal ASan/UBSan runs. It injects
faults across register and source accesses, tests the wire and stage order,
and checks that missing scanout keeps the backlight dark. The freestanding
x86-64 object passes a 2 KiB stack-frame limit. AUX and link training have
independent 10 ms and 150 ms ceilings.
The complete PPS and link sequence has a new 1.6 s ceiling for its physical
panel delays. Existing GT, MMIO ownership and GGTT deadlines are unaffected.

Remaining for the first hardware trial are the native source-clock and PHY
provider, full timing/M:N and original-plane restore, independent frame and
underrun readback, and an exact staged recovery with durable off-screen
evidence. Qualify the original mapping and sound recovery before attempting
a PCI reset-based GGTT takeover. A separate live memory-completion witness is
required before even one GGTT store.

References for the AUX controller and PPS are Broadwell Register Reference
Volume 2c, pp. 343–345 and 1230–1236. The source sequencing and delay policy
were checked against the i915 implementation. [Intel Broadwell PRM index](https://www.intel.com/content/www/us/en/docs/graphics-for-linux/developer-reference/1-0/broadwell.html) ·
[i915 AUX](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_dp_aux.c) ·
[i915 DDI](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_ddi.c) ·
[i915 PPS](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_pps.c).
