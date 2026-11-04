# Roadmap

Updated November 3, 2026. The goal is a pleasant native desktop on the MacBook
Air, with a solid shell and server foundation. The [current status](status.md)
and [machine records](machines/README.md) separate implementation from physical
acceptance. The [device parity ledger](device-parity.md) compares the Air with
ongoing Pro work.

## Installed-desktop migration — November 3, 2026

Target: move a selected portable profile from a used installed QEMU desktop to
a fresh installation, preserving identity, selected document bytes and all six
portable preferences through reboot. The user authorizes coordination, full
flow verification and dependency-order merges when ready. Authentication,
permission enforcement, physical installation and browser work remain separate.

The installation chat owns offline used-image export and fresh-install restore;
the profiles chat owns SrvrApps #6 and SrvrOS #333 graphical transfer. The parent
integration chat owns independent review, combined verification and merges.
The filesystem chat's managed-workspace proof is independent; consume its merged
contracts without adding salvage requirements to healthy-profile migration.

Required evidence: installation A edits/saves/reboots; explicit selected export;
fresh B has different disk/home/local bindings; boot/reboot B verifies UUID,
text/binary files and portable preferences. Unselected documents, machine
settings, local bindings, application state, caches and implicit credentials must
not migrate. Preserve source images/receipts and refuse existing outputs,
changed/invalid previews, ambiguous imports and unsafe offline source state.
Graphical review must keep confirmation, progress and recovery understandable.

Current: final topic verification is active. A separate ordinary clone,
`SrvrOS-migration-review`, owns parent review and integration; the managed
worktree API cannot attach a nested repository under this workspace. Preserve
the primary checkout and existing review VMs. Run one build/proof per checkout,
sequence large image proofs within available host storage, and keep accepted
evidence outside build cleanup. Hosted Actions remain disabled.

Next: review exact final topic heads and evidence, merge Apps before its OS pin,
verify the combined installed export/restore workflow, and merge the remaining
installation/integration changes. No readiness claim until these gates pass.

## bc exact decimal workflow — November 3, 2026

Target: static bc 7.0.3 for scripts/files/stdin/pipelines, arbitrary-precision
decimal calculations, scale/sqrt/bases and variables/functions/loops. Isolated
`ports/bc` checkout begins at main `b78e3ed5`. First gate is safe signal recovery:
handlers must return through the cooperative libc dispatcher before nonlocal
recovery; compute-bound code needs bounded dispatch checkpoints. No blind
SA_NODEFER or sigjmp stubs. Coordinate any shared-runtime need with integration.
No dc, history/readline, NLS or extended-math acceptance initially. Required
gates: selected upstream arithmetic cases, exact guest reports, repeated
signal/error recovery, real stdout failures, clean CLI binary reproducibility
and existing ports/docs checks. Implemented and verified: bc 7.0.3 static core arithmetic, scale/sqrt/bases,
functions/loops and file/stdin/pipeline exact reports. The signal gate is
resolved in the port's generated copy: callback records state and returns,
libc restores dispatcher/mask state, and existing bc unwind paths run afterward.
Bytecode loops poll via public sched_yield every 1024 instructions. A single
long arithmetic instruction can delay signal delivery; interactive interrupt
messages differ from upstream. No shared libc/kernel changes were needed.

Guest probes pass same-process syntax/division-error recovery, repeated blocked
input and busy-loop SIGINT with successful calculations afterward, busy-loop
SIGTERM exit and actual stdout failure. Seven unchanged upstream arithmetic
fixtures plus exact report save/reopen across two boots pass. Guest fsck and
offline allocation pass; independent fsck.exfat is unavailable. Full build,
existing ports regression and docs/whitespace checks pass. Two clean CLI builds
match SHA256 `62c0737baafa6b569bd917303600e5aee341c5f09b24776fce49615e5fa6af61`.
Receipts are under `build/bc-smoke/20261103-221517-z7lm_s73/` and
`build/review/bc/accepted.json`. No full-suite, physical-device, full-image
reproducibility or general asynchronous-signal acceptance is claimed.
Status: bounded implementation complete and ready for review; no merge performed.

Integration checkout/review VM remain untouched.

## jq CLI workflow — November 3, 2026

Target: jq 1.8.2 for offline JSON files/stdin/pipelines, filtering/aggregation,
raw/compact output, filter files and CSV export consumed by SQLite. Separate
`ports/jq` checkout begins at main `5634cd9d`; integration checkout and review
VM remain untouched. Preserve upstream source, decimal-number defaults and
licenses; omit Oniguruma regex functions. Port-local entropy adaptation uses
existing getrandom. No shared libc/kernel edits without concrete gap coordination.
Required gates: static build/link, selected unchanged upstream tests, guest
JSON-to-CSV-to-SQLite workflow, Unicode/numeric/errors/pipeline checks, two clean
CLI builds with identical binaries, existing ports regressions and docs checks.
Implementation and scoped guest acceptance pass. The port builds static jq
with upstream decNumber defaults and port-local entropy, dirname and UTC
timezone-name adapters; no shared libc/kernel changes were needed. Oniguruma
regex and optional configure-gated libm/date wrappers are outside this profile.
The first run passed its 11 selected upstream cases but later aborted during
the optional three-thread regression; that test-only feature gate is omitted
for the single-thread CLI. Real pthread keys/once/atexit remain enabled. No
multithreaded jq acceptance or allocator root cause is claimed.

Two-boot JSON report/CSV-to-SQLite, exact Unicode/NUL and numeric literal output,
exit statuses, invalid JSON/filter/files, unsupported regex, actual stdout error
and recovery checks pass. The 11 unchanged upstream cases and remaining
single-state internal tests complete. Guest fsck and offline allocation pass;
independent fsck.exfat is unavailable. Two clean CLI builds match SHA256
`286f8cbe0cd27f983881208981d5f29773197aa00641487d0f6087caf7ef1de8`.
No whole-image reproducibility or physical-device acceptance is claimed.
Evidence: `build/jq-smoke/20261103-214345-kx3nfg74/` and
`build/review/jq/accepted.json`. Existing ports regression and documentation/whitespace checks pass.
Status: independent review and fresh combined guest/ports validation pass.
User authorized ports-first then OS merge. SrvrPorts PR6 is merged as
`747cca28`; SrvrOS PR337 records integration. No further implementation is
required for this bounded target; threaded jq remains separately unaccepted.


## Current HiDPI and display settings — November 3, 2026

Target: genuinely detailed 1×/2× text and widgets, authoritative Display settings,
saved scale and QEMU VirtIO resolution switching with timed Keep/Revert recovery.
User authorized implementation, renderer coordination, verification and merge.
SrvrUI retains logical layout/input and adds physical-density backing; SrvrOS
owns client surfaces, compositor, display transactions and persistence.
Unsupported native/standard VGA backends expose current mode only. Required
checks cover density/clipping, budgets, migration, runtime recovery, retained
app state, persistence and presentation cost. The matched implementation passes
these checks, including held-switcher rollback and native 2× snap/resize/theme
regressions. SrvrUI PR3 and SrvrApps PR4 are merged; SrvrOS PR303 records delivery.
See [the acceptance receipt](assets/screenshots/hidpi-display-manifest.json).
This target is complete; fractional scale and native modesetting remain separate
future work. Previous snapping/resize/theme milestones remain complete.

## Current window snapping — November 3, 2026

User authorized left/right titlebar snapping, top-edge maximize, themed placement
previews and drag-to-restore, followed by verification and merge. Base `da0db066`
includes all previously merged desktop, app, port and resizing milestones.
Reuse the existing workarea, placement and resize transaction contracts; preserve
keyboard arrangement, client content and theme updates. Required checks cover
preview-only movement, exact release placement, cancellation/modal interruption,
restore geometry, failure recovery and both output scales. All required host,
1×/2× snapping and resize, live-theme and offscreen/dock regressions pass.
The combined build with main `fa8e503f` has identical kernel/initramfs/disk
hashes to the accepted runtime. [Evidence](assets/screenshots/window-snap-manifest.json)
records sources, captures and limits; [PR297](https://github.com/PaulMDemers/SrvrOS/pull/297)
records delivery. The updated review desktop is open with Text snapped left.
No additional feature work is required for this milestone.

## Current resize interaction — November 3, 2026

The repository/theme/VirtIO milestones are complete. The user authorized the
next bounded shared desktop improvement: all edge and corner resize directions
with matching cursor feedback, verification and merge. Snapping is separate.

Branch `device/apple-silicon/edge-resize` starts at main `5b2bf3c7`. Scope stays
in the compositor: discoverable resize targets, anchored geometry, safe capture
and failure recovery, plus damage-correct cursors. App recovery remains in its own PR; merged awk integration is retained; SrvrUI portability's merged renderer boundary is retained.

Required evidence: host geometry/session/render regressions, full desktop suite,
matched OS build, real Text resize/content checks at both supported scales with
VirtIO tablet input, and clean presentation teardown. Native device acceptance
and host-window release observations remain separate. The full host suites, clean build, both resize VMs and live-theme regression
pass. [Evidence](assets/screenshots/edge-resize-manifest.json) records the tested
source and limits. [PR292](https://github.com/PaulMDemers/SrvrOS/pull/292) records delivery. The
combined image with merged awk also builds, starts and completes a live corner
resize; its kernel/compositor hashes match the accepted runtime. No additional
resize feature is required.

## SQLite CLI port — November 3, 2026

Active target: integrate SQLite 3.53.1 as a static `sqlite3` command for bounded
single-connection, exclusive-access SQL scripts and CSV import/query/export,
including database reopen. The CLI has a separate engine configuration so the
existing `sqlitedemo` remains unchanged. WAL, process commands, extension loading
and readline are excluded. Full build, host and guest VFS ownership/temp/error checks, and two-boot CSV
workflow acceptance pass. Two clean CLI builds match SHA256
`a23d90c8e8b49f7e36ff996e8cf009eee5ab7537faea6edabbe7a34cfd873b2f`.
Guest fsck and offline allocation pass; independent fsck is unavailable. Evidence
is under `build/sqlite-cli-smoke/` and `build/review/sqlite/`. No full-image
reproducibility or power-loss atomicity claim is made. Implementation is ready
for review; see
[port usage and scope](ports.md). No native device acceptance is implied.

## Pinned awk port — November 3, 2026

Target: a useful upstream interpreter for Terminal CSV/text reporting, with
pinned clean source and repeatable builds. Isolated branch `ports/awk` integrates
SrvrPorts `port/awk`; [port usage and scope](ports.md#one-true-awk) describe the
C-locale profile and explicit exclusions. No libc/kernel changes are required.
The Terminal report, three unchanged upstream programs, error/status handling,
pipe/subprocess completion, process retirement and saved-disk reopen checks pass.
Two clean full builds produce identical awk ELF and generated sources/tool
manifests; the existing ports regression and final matched guest run pass.
Kernel and exFAT hashes match; initramfs archives differ, so full-image bit
reproducibility is not claimed. The milestone is merged through SrvrPorts PR1 and SrvrOS PR291 (`c609207b`). The port is published in [SrvrPorts PR1](https://github.com/PaulMDemers/SrvrPorts/pull/1). [Portable evidence](assets/screenshots/awk-manifest.json)
records exact runtime source `6d1ae527` and published port `5e30ecd`. Shell quote parsing remains a
separate integration issue; no shared review VM or physical boot is touched.

## Current theme profiles — November 3, 2026

Target: complete Aero-inspired and Aqua-inspired widget/chrome profiles, live
Appearance previews, consistent updates across existing/new application windows,
saved style restoration, and preserved dragging/scrolling performance. Keep
Silver as the default and retain accent/glass controls and document state.

Implemented on `device/apple-silicon/theme-profiles`, based on merged PR285:
shared profile resolver and state rendering in SrvrUI; Appearance-first Settings
with compact previews in SrvrApps; v17 snapshots and v6 file migration; compositor
chrome and focused tests. Full build, desktop/widget host tests, new profile VM
and Settings/application regressions pass. Final clean runtime `50389359` passes
the matched build, focused minimum-height Settings test and profile VM. Published
SrvrUI `89fb6d9` and SrvrApps `d245fe2` are pinned in
[PR286](https://github.com/PaulMDemers/SrvrOS/pull/286), with
[UI PR1](https://github.com/PaulMDemers/SrvrUI/pull/1) and
[Apps PR1](https://github.com/PaulMDemers/SrvrApps/pull/1).
[Theme contract and evidence](desktop-theme.md#profile-acceptance--november-3-2026)
record the completed scope; no additional theme feature is required for it.
Native acceptance and device drivers are unchanged.

## Current QEMU VirtIO baseline — November 3, 2026

Follow-up: [PR285](https://github.com/PaulMDemers/SrvrOS/pull/285) is merged.
The default absolute tablet passed automated input/scaling regressions and the
user's attended automatic-release/no-stuck-drag check. The QEMU baseline is
delivered; its optional driver improvements do not block desktop work.

Target: make the smoother VirtIO presentation the QEMU default, reduce full-frame
copy cost, then deliver input, RNG, block and network drivers. User confirmed all
four drivers and accepted VirtIO dragging as smoother than VGA. Work branch:
`device/apple-silicon/virtio-baseline`, based on measurement PR283 at `d643d2b8`.

Implemented: per-resource accumulated damage; modern PCI split-queue transport;
QEMU keyboard/relative mouse including wheel; pooled entropy; durable-capable
block I/O; network provider; explicit fallback profiles. GPU host oracle and
three repeated Cocoa sessions pass, with 64–67% lower copy/transfer volume and
27–33% lower draw/present time for the measured workloads. Protocol and existing
input/storage/graphics/scheduler host regressions pass. The integrated two-boot
VM passes keyboard/Text input, real wheel scrolling, dragging, entropy use,
network link changes, DHCP, four ICMP replies, clean GPU exit and disk persistence.
QEMU compact input configuration, SeaBIOS block handoff and a QEMU receive-header
quirk are handled with bounded checks and regression coverage.

The clean runtime `986c16e8` builds and matches the tested image byte-for-byte.
Final host/desktop regressions, the explicit legacy launcher and default Cocoa
launcher pass. Source and receipts are published through
[PR284](https://github.com/PaulMDemers/SrvrOS/pull/284), following measurement PR283.
The requested baseline is implemented and verified; no additional driver feature
is required for this target. Details and evidence: [VirtIO baseline](virtio-baseline.md).
Native hardware, interrupt-driven queues, packed rings and host
vblank are separate work; they do not block the requested polling baseline.

## QEMU fenced presentation — November 3, 2026

Target: an opt-in QEMU VirtIO desktop presentation path, explicitly selected by
the user. Finish the complete composed frame before publishing scanout, retain
buffer ownership until command completion, and recover the VGA console on normal
exit or desktop process death. The glass/theme milestone remains complete.

Implemented and locally verified at runtime `a8c21344`, merged through
[PR281](https://github.com/PaulMDemers/SrvrOS/pull/281) after UI PR280.
The matched build, transport/lease/console/graphics/I/O/cancellation/desktop host
regressions and sanitizers pass. Three sequential probe boots cover enabled,
absent and disabled device paths; the absent case renders/exits a standard VGA
desktop. The primary VirtIO VM verifies visible drag, deterministic scroll,
normal console restoration, abrupt desktop termination and successful restart.
The final evidence identifies a clean runtime tree and exact image hashes.

The backend stages a complete frame, alternates two resources, fences commands,
retains DMA until reset completion, and uses existing scheduler service checkpoints.
Start it with `python3 tools/macos/run.py --display-device virtio`; standard VGA
remains the default. Both PRs are merged into `main`; post-merge runtime
`1c0896e7` has the exact tested tree and passes a matched build plus the primary
presentation/recovery VM. The scoped target is delivered and verified.
The visible comparison is complete: the user reports smoother VirtIO dragging,
while six paired-session runs quantify its higher software frame cost. See the
[measurements and limits](display-presentation.md#visible-smoothness-comparison--november-3-2026).
Reducing full-frame copy/transfer cost is an optional follow-up; the current
opt-in backend is usable and the launcher default is unchanged. Host compositor/physical vblank, native drivers, live mode changes
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
PR280 is merged. The scoped glass/theme/presentation investigation is complete;
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
