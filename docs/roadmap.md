# Roadmap

Updated October 25, 2026. The goal is a pleasant native desktop on the MacBook
Air, with a solid shell and server foundation. The [current status](status.md)
and [machine records](machines/README.md) separate implementation from physical
acceptance. The [device parity ledger](device-parity.md) compares the Air with
ongoing Pro work.

## 1. Integrate the everyday laptop session

**Completed on Air7,2 release `9aa8dbf6fbda8252`.** Ordinary Home battery,
brightness, PCM audio and read-only sensors coexist with input and saved files.
The attended save/exit/reopen trial and local host/VM regressions pass; files,
resource retirement and Ubuntu recovery were verified. Cached low-charge notices
and battery-label access to System are implemented, with synthetic warning tests.
The combined profile preserves native gates and omits the isolated AML probe.

Subsequent v58/follow-up trials accept matched Ethernet/cable recovery, prompted
headphone controls and named gesture checks; PR88 tap settling is tester-accepted
with incomplete final diagnostic receipt. Physical low-charge/calibration remains
outside this acceptance. The accepted payload/evidence and older
recovery are retained independently of build cleanup.

## 2. Close remaining device and input acceptance gaps

Retain targeted secondary-click/cancellation checks, PCM channel/headphone
listening and network cable recovery in the hardware checklist. Reuse the Pro's
merged exact-adapter Ethernet support. Its draft USB Wi-Fi firmware/MCU work is
owned there; scanning and connectivity remain unfinished. Do not duplicate it or
copy its hardware acceptance to the Air. Each sibling needs captured identities.

**Done when:** the named provider has matched release/logs and explicit native
acceptance; skipped checks remain pending. No blanket fleet-wide parity claim.

## 3. Scale persistent workspaces

PR99 prevents silent diagnostic loss at the current node-table limits and adds
verified archival. PR102 implements bounded demand allocation, stable node
identities and paths, direct enumeration, memory reservation before mutation and
deleted-slot reuse. Host tests cover thousands of files, both exhausted budgets,
allocation failures and save/rename/delete/remount. The exact-stage VM passes
with 5000 preserved files, automatic return and desktop positive/refusal checks.
The native automated round passed beyond both old limits with all prior data
preserved and clean offline allocation; only the verified fixture was removed.
See the [capacity contract](filesystem-capacity.md).

**Completed:** bounded filesystem growth and safe refusal are host/VM validated
and accepted by the exact native persistence round. Path limits, linear lookup
and full metadata-sidecar rewrites remain explicit bounds.

## 4. Improve power from repeatable measurements

Compare matched workload, brightness, adapter state and duration using read-only
package-energy/active-frequency telemetry and scheduler occupancy. Linux policy
inventory is a comparison baseline. Evaluate polling/timer changes and deeper idle
separately. Relative charge display is not a calibrated runtime or health estimate.

**Done when:** repeatable measurements show improvement without input, storage,
audio or battery regressions. Temperature and idle percentage alone are not energy.

## 5. Make the desktop less constrained

The desktop uses demand-allocated pixel caches under a 96 MiB budget, dynamic
window/child registries under an 8 MiB additional metadata budget, and separate
kernel surface/queue byte budgets. Window identity is independent of process
identity; normal Text exercises multiple real documents through one dispatcher.
Shared lazy file-worker channels and combined GUI/pipe waits remove one-worker
and one-poll-loop-per-document assumptions. Host/VM coverage includes 128 small
windows, 32 Text documents/33 exact files, explicit pressure refusal, clipboard,
worker cleanup and typing during throttled writes. See [window capacity](window-capacity.md)
and [desktop connections](desktop-connections.md).

Air7,2 physical acceptance remains sixteen-window opening, saved-file integrity
and clean exit at `bf68e4e9b88b8ae7`; that round contains no native cache totals or
pressure injection. New source does not retroactively extend that evidence.

The renderer now conservatively skips fully covered opaque windows and reuses
one stack ordering per frame; exact reference pixels and identical-workload host
benchmarks guard the change.

**Next completion gates:** validate the new runtime with focused native input/save/resize checks, and mature shared actions,
parented dialogs, chooser/document services and activation routing. Follow the
[desktop API evolution plan](desktop-api-evolution.md). Native-density typography,
accessibility and broader toolkit portability remain separate from 2× pixel scaling.
The [portable C/POSIX profile](posix-supported-profile.md) supplies the selected
thread/file/process/readiness foundation; broader Unix and Linux-adapter gaps
remain separate, with no claim of full POSIX conformance.

## 6. Suspend and expand hardware deliberately

Suspend/resume needs device quiescence, wake sources, firmware ownership and
reinitialization with preserved recovery; battery readout does not provide this.
Continue on-device Air6,1 and Pro11,2 development and independent bring-up of
Air7,1, Pro11,3 and Mini7,1 through the [Ubuntu workflow](ubuntu-device-agent-setup.md).
Internal installation, Bluetooth/camera, additional storage, process isolation and
POSIX/ports progress remain separate work as real applications require them.

No production-readiness or self-hosting date is promised. The
[October 18 roadmap](history/roadmap-2026-10-18.md) and
[original desktop plan](desktop-plan-2026-10-17.md) remain historical context.
