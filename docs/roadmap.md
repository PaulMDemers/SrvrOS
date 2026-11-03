# Roadmap

Updated October 27, 2026. The goal is a pleasant native desktop on the MacBook
Air, with a solid shell and server foundation. The [current status](status.md)
and [machine records](machines/README.md) separate implementation from physical
acceptance. The [device parity ledger](device-parity.md) compares the Air with
ongoing Pro work.

## QEMU fenced presentation — November 3, 2026

Target: an opt-in QEMU VirtIO desktop presentation path, explicitly selected by
the user. Finish the complete composed frame before publishing scanout, retain
buffer ownership until command completion, and recover the VGA console on normal
exit or desktop process death. The glass/theme milestone remains complete.

Implemented and locally verified at runtime `a8c21344`, published as draft
[PR281](https://github.com/PaulMDemers/SrvrOS/pull/281), stacked on UI PR280.
The matched build, transport/lease/console/graphics/I/O/cancellation/desktop host
regressions and sanitizers pass. Three sequential probe boots cover enabled,
absent and disabled device paths; the absent case renders/exits a standard VGA
desktop. The primary VirtIO VM verifies visible drag, deterministic scroll,
normal console restoration, abrupt desktop termination and successful restart.
The final evidence identifies a clean runtime tree and exact image hashes.

The backend stages a complete frame, alternates two resources, fences commands,
retains DMA until reset completion, and uses existing scheduler service checkpoints.
Start it with `python3 tools/macos/run.py --display-device virtio`; standard VGA
remains the default. The scoped QEMU presentation target is ready for review.
Next: review PR281 after its PR280 dependency; no further implementation is required
for this target. Host compositor/physical vblank, native drivers, live mode changes
and partial-transfer optimization remain separate work. All verification VMs are
stopped. See [the contract and screenshot](display-presentation.md).

## Glass and live theme refinement — November 2, 2026

Target: more polished glass chrome, application-wide appearance controls in
Settings, and an evidence-based plan to eliminate scanout tearing. Continue
`overhaul/modular-desktop` / draft PR280; prior repository split stays complete.
Focused research confirms the current inherited framebuffer has no synchronized
flip contract. This milestone separates composition from uploads; real vblank
and page flips remain a driver milestone, not a claim of this software change.

Required: Blue/Graphite/Teal accents and Solid/Subtle/Clear glass, persisted with
migration and broadcast to existing/new windows; clipping-safe glass and correct
translucent occlusion; unchanged content and input behavior; matched full build,
host regressions, runtime preference checks and inspected desktop/widget images.
Readiness: implemented and locally verified at runtime `db898927`, with SrvrUI
`df2e311` and SrvrApps `e4b9235` published. Matched build, desktop/widget/I/O host
suites, theme sanitizers, dynamic drag/scroll damage oracle, live theme/persistence
application VM and compositor VM pass. The separate Settings VM passed before the
final title-frost-only adjustment. [Rendered evidence](desktop-theme.md) is saved.
Next: review PR280. The scoped glass/theme/presentation investigation is complete;
real synchronized scanout is separate driver work described in the
[presentation plan](display-presentation.md). Native acceptance and Actions remain
unchanged.

## Current overhaul — November 2, 2026

Target: separate SrvrUI, SrvrApps and SrvrPorts into GitHub repositories pinned
as submodules, then apply a coherent native desktop theme inspired by Windows 7
and OS X 10.7–10.10. Desktop apps are the initial SrvrApps scope; OS services,
diagnostics and shell commands remain in SrvrOS. Application and port expansion
will be separate later work; no additional chats are created by this milestone.

Current work: `overhaul/modular-desktop`, based on main `74d38d59`. Preserve the
existing ABI, executable names, upstream pins and device acceptance boundaries.
The user authorized repository creation and integration; use focused research,
local checks and a reviewable OS branch, without enabling hosted Actions.

Required outcomes and evidence:
- Three published repositories and pinned, recursively cloneable submodules.
- Matched system build and affected desktop/widget host and VM checks.
- Shared silver/blue control and chrome treatment, with state and clipping checks
  and real rendered evidence; retain readable content and existing interactions.
- Documented repository ownership and instructions for independent app/port work.

Readiness: implemented and locally verified on `overhaul/modular-desktop`.
SrvrUI `4fdf91a`, SrvrApps `d386386` and SrvrPorts `0494ae2` are published and
pinned. Fresh recursive clone/full image build, the complete desktop/widget/I/O
host targets, native theme sanitizers, application workflow and compositor VMs
pass. Final runtime source is `e65e5938`; subsequent evidence/docs commits do
not change it. See [organization and visual evidence](repository-organization.md).
Next: review and merge the OS integration change. Native device acceptance is
unchanged. New application and port work can use the documented submodule
boundaries; no additional feature milestone is required for this review target.

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

Air7,2 release `f01d4049daae0b4a` accepts two automated 24-app rounds with
acknowledged edits, exact saved files, chooser cancellation and retirement.
Release `1d1d5edce03523db` separately accepts reported modal and unsaved-exit
corrections. These supersede the earlier sixteen-window capacity checkpoint for
their named checks; they do not accept arbitrary workloads or pressure injection.

The renderer now conservatively skips fully covered opaque windows and reuses
one stack ordering per frame; exact reference pixels and identical-workload host
benchmarks guard the change.

Air6,1 has since passed bounded 30-minute audio/disk/app stress under unchanged
deadlines; see its independent machine record. Air7,2 has bounded general
movement/dock/navigation and focused Close/window-list acceptance through
`060fffa453e26d5e`. Running dock entries, MRU switching, keyboard menu navigation
and shared Close discovery are implemented.

**Next completion gates:** expand measured input/resize/pressure coverage and
validate newer shared changes on the target before extending native acceptance.

Parented dialogs, cancellable Close/Quit and
existing-process document routing are implemented in protocol v15. Text and
Preview share the asynchronous [chooser controller](desktop-chooser.md) hosted
in native owned dialogs with an embedded fallback. Broader app services remain
future work; see the [lifecycle contract](desktop-application-lifecycle.md). Follow the
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
