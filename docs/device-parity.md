# Air and Pro device parity

Reviewed October 23, 2026 against main `b01ae95` and Pro PR19 head `f1785c6`.
This compares available shared code with each machine's evidence. A passing Pro
trial does not accept a different Air controller, firmware or connected adapter.
The [Air record](machines/macbookair7-2.md) and
[Pro record](machines/macbookpro11-2.md) retain exact releases and limitations.

| Capability | Shared implementation / Pro evidence | Air7,2 position and next check |
| --- | --- | --- |
| Keyboard and trackpad | Captured Pro `05ac:0262` and Air `05ac:0291` reports; full-speed EP0 correction merged | Typing, pointer, scrolling and dragging exercised. Retain targeted secondary-click/cancellation acceptance gaps; Pro latency tuning is deferred. |
| Display and windows | Pro 2880×1800 at explicit 2×; native apps, maximize, tiling | Air 1440×900 at 1× accepted. No need to copy Retina scaling; both still have eight client windows and bounded software surfaces. |
| Panel brightness | Exact Air and Pro PWM profiles merged and physically exercised | Combined ordinary battery session v55 physically accepted. No automatic brightness policy. |
| HDA and PCM | Separate CS4208 controller/subsystem routing for Pro and Air; Pro speakers exercised | Air speaker volume/tone and combined-session PCM playback accepted. Audible channel separation and headphones need explicit checks. Do not substitute the Pro codec profile. |
| CPU and fan telemetry | Haswell/Broadwell CPU matching; Pro two-fan SMC and Air one-fan profiles merged | Both have read-only telemetry evidence; Air v55 accepts coexistence with battery and app I/O. No fan-control writes. |
| Battery/AC | Shared cache, service lifetime and UI; native profile currently accepted only on Air7,2 | Ordinary Home startup, save/exit/reopen, AC transitions and indicator accepted. Pro battery work remains deferred; no copied EC/firmware gate. |
| USB exFAT | MBR and validated GPT discovery; imported-file identity repair and replay refusal merged | Air workspace accepted on its recorded payloads. The v55 trial includes the imported-file identity baseline and preserves all prior data; targeted collision cases remain host/VM evidence. No journaling/power-loss guarantee. |
| USB Ethernet | Captured `0b95:1790`, revision `0200`, ECM compatibility and cable-status work merged; Pro DHCP/DNS/HTTP/cable UI accepted | Matching revision 0200 adapter now attached at 5 Gb/s; all 98 ECM configuration bytes match the existing fixture. Native DHCP/DNS/HTTP and cable/UI recovery are next; no arbitrary ASIX VID/PID enablement. |
| USB Wi-Fi | Pro PR19: Edimax `7392:a711`/`mt76x0u`, firmware/MCU/baseband diagnostics in progress | Draft work is not merged or connectivity-ready. Preserve Pro ownership; consume after review and test matching hardware independently. RF, scan and association remain unaccepted. |
| Internal Wi-Fi/Bluetooth/camera | No accepted complete device path on either target | Separate projects; USB Wi-Fi progress does not enable Broadcom internal Wi-Fi. |
| Internal storage | AHCI implementation; hardware desktop boots use RAM plus USB Home | Internal installation is not accepted on either machine. Preserve Ubuntu and known-good boot payloads. |
| Suspend, battery life, calibration | Separate from battery readout and CPU telemetry | No accepted suspend/resume or calibrated runtime estimate; requires device reinitialization/recovery and measurements. |

## Integrated desktop scope

The explicit v55 profile combines ordinary battery-owned Home with the existing
brightness, HDA, CPU/SMC and CDC ECM boot options. It adds no controller matching
exceptions and does not include the optional isolated AML probe. v54, Air6,1 v53
and Pro v37 recipes remain immutable. Unknown or absent devices stay unavailable;
apps and battery controls must remain usable when individual providers refuse.

Kernel device workers are boot-scoped: they are initialized once, remain available
at the monitor, and are not recreated on desktop reopen. App-owned PCM and jobs
retire with their processes; battery, desktop lifetime and logging retire with
the Home session. The mounted Home hold releases after these users drain, while
Home remains mounted until explicit unmount or reboot. This does not promise
full hardware power-down on desktop exit.

## Validation and next acceptance

1. Passed host tests: immutable staging recipes, cached warning policy and notification
   behavior, no stale charge display, existing codec/sensor/network gates.
2. Passed disposable VMs: combined providers absent, native battery unavailable, normal
   battery model, save/exit/reopen and pending-save cancellation. Verify logs,
   files, allocation and zero retained app-owned PCM; exact staged EFI rehearsal.
3. Air v55 accepted: `9aa8dbf6fbda8252`, typing/pointer use, PCM playback,
   brightness, sensors, battery/AC, save/exit/reopen and Ubuntu return. Matching
   logs show two clean lifetimes, no retained PCM owner/queue or underruns,
   preserved files and clean offline allocation. One saved screenshot shows the
   document and indicator; reopening is tester-confirmed. Low-charge thresholds
   remain synthetic tests; no physical drain or calibration claim.
4. Ethernet is a separate conditional check once an adapter is attached. Capture
   identity first, then DHCP/DNS/HTTP and cable removal/reconnect with saved evidence.

Keep untested providers and unsupported models marked as such. Integration on
one machine is not fleet-wide hardware acceptance.

The [attended device checklist](device-acceptance.md) records headphone, Ethernet
and specific gesture observations without treating script completion as a pass.
