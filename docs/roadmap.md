# Roadmap

Updated October 23, 2026. The goal is a pleasant native desktop on the MacBook
Air, with a solid shell and server foundation. The [current status](status.md)
and [machine records](machines/README.md) separate implementation from physical
acceptance. The [device parity ledger](device-parity.md) compares the Air with
ongoing Pro work.

## 1. Integrate the everyday laptop session

Ordinary Home battery startup, cached indicator, AC transitions and save/exit/
reopen are accepted on Air7,2. Bring existing brightness, audio, read-only sensors
and Ethernet discovery into that session without enabling the isolated AML probe
or widening hardware gates. Keep unsupported devices unavailable and apps usable.
Add cached low-charge notifications and easy access to battery details. Refresh
public documentation alongside tested behavior.

**Done when:** host/VM regressions and one attended combined Air trial establish
input, saves, playback, brightness, telemetry, indicator and clean exit/reopen.
Record absent hardware explicitly; Ethernet acceptance requires an attached adapter.
Preserve the accepted v54 recovery payload until the combined release passes.

## 2. Close remaining device and input acceptance gaps

Retain targeted secondary-click/cancellation checks, PCM channel/headphone
listening and network cable recovery in the hardware checklist. Reuse the Pro's
merged exact-adapter Ethernet support. Its draft USB Wi-Fi firmware/MCU work is
owned there; scanning and connectivity remain unfinished. Do not duplicate it or
copy its hardware acceptance to the Air. Each sibling needs captured identities.

**Done when:** the named provider has matched release/logs and explicit native
acceptance; skipped checks remain pending. No blanket fleet-wide parity claim.

## 3. Improve power from repeatable measurements

Compare matched workload, brightness, adapter state and duration using read-only
package-energy/active-frequency telemetry and scheduler occupancy. Linux policy
inventory is a comparison baseline. Evaluate polling/timer changes and deeper idle
separately. Relative charge display is not a calibrated runtime or health estimate.

**Done when:** repeatable measurements show improvement without input, storage,
audio or battery regressions. Temperature and idle percentage alone are not energy.

## 4. Make the desktop less constrained

Replace eight preallocated window-cache pairs with demand-sized buffers and align
process, queue and surface budgets. Handle allocation failure before raising the
limit. Continue reusable controls, keyboard accessibility, app save/recovery and
text coverage. Native-density typography is separate from current 2× pixel scaling.

**Done when:** workloads beyond eight windows, resizing, close/reopen and memory
pressure preserve documents, input and resource stability.

## 5. Suspend and expand hardware deliberately

Suspend/resume needs device quiescence, wake sources, firmware ownership and
reinitialization with preserved recovery; battery readout does not provide this.
Continue on-device Air6,1 and Pro11,2 development and independent bring-up of
Air7,1, Pro11,3 and Mini7,1 through the [Ubuntu workflow](ubuntu-device-agent-setup.md).
Internal installation, Bluetooth/camera, additional storage, process isolation and
POSIX/ports progress remain separate work as real applications require them.

No production-readiness or self-hosting date is promised. The
[October 18 roadmap](history/roadmap-2026-10-18.md) and
[original desktop plan](desktop-plan-2026-10-17.md) remain historical context.
