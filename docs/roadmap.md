# Roadmap

Updated November 4, 2026. The goal is a pleasant native desktop on the MacBook
Air, with a solid shell and server foundation. The [current status](status.md)
and [machine records](machines/README.md) separate implementation from physical
acceptance. The [device parity ledger](device-parity.md) compares the Air with
ongoing Pro work.

## Current work closeout — November 4, 2026

The user requested finishing the current non-browser chat scopes, merging only
verified changes in dependency order, then updating documentation and status.
Browser work is explicitly exempt and remains ongoing. No deadline or reduced
acceptance bar is implied. Later CLI queue rows and new feature milestones are
not part of this closeout.

Current delivery gates:

- Larger-document profiles: finish the corrected source-change fixture and
  complete GUI, recovery, storage and bounded-memory verification before
  Apps #11 and OS #367. The first full proof remains failed, not relabeled.
- GNU Patch: complete strict sidecar validation and the current qualification
  rerun, independently review, merge Ports #12 then OS #370, and stop the queue.
- Filesystem GUID selection: close the bounded review/tests and merge OS #366;
  preserve original media and completed streaming-review evidence.
- Runtime audit: documentation checkpoint OS #365 is merged as `db95fc9b`;
  browser target review and runtime acceptance remain separately owned.
- Installation OS #364 (`70c3ea0f`), application quality OS #368 (`88a95f9a`)
  and UI portability closeout OS #371 (`518175df`) are merged and verified.
  Their owners are idle; optional follow-on work is deferred.

The integration coordinator owns aggregate status consistency. Topic owners
retain their evidence and unit records. Heavy compiler/VM/local-model windows
remain serialized with explicit handoffs and resource reserves. Actions remain
disabled. Original installations, review-disk evidence and accepted backups are
retained. The three historical review VM PIDs are now absent; no cause is
established by this closeout, and no restart is requested. Physical-device PRs
#301/#19 have separate current-main integration gates and their owners are not
reachable through the available local chat tools; no new physical trial is authorized
by this record. PR #19's proposed syscall 164 conflicts with browser's reserved
164–167 and cannot merge unchanged; profile streaming reserves 168.

This is an in-progress closeout record, not a claim that pending PRs are merged
or verified. Final merged hashes and remaining explicit exceptions follow after
the delivery checks.

## Automatic CLI queue — November 4, 2026

The closeout instruction above supersedes automatic continuation: finish GNU
Patch, then stop. GNU grep and sha256sum remain deferred backlog entries.

User authorized the bounded GNU diff milestone, then automatic research,
implementation, independent review and dependency-order merge through a finite
queue: GNU diff, GNU patch, GNU grep and GNU sha256sum. Detailed profiles,
verification and resource ownership are in [the queue](cli-port-queue.md).

GNU diff 3.12 is implemented and qualified in the isolated checkout. The useful
profile is two regular C-locale text files, normal/quiet/three-line unified
reports, labels, long and unterminated lines, actual-read caps and 0/1/2 statuses.
The changed simultaneous 256 KiB/8192-line/64 KiB-line fixture passes both boots.
Exact native GNU references and patch roundtrips, allocation recovery, repeated
SIGINT/SIGTERM after comparison progress and observed held-output stall, output
errors, preserved inputs, reboot bytes and fsck pass on clean `5796a9cd`.

Actual Terminal/less/error/readback expert walkthrough and full CLI/ports
regressions pass against the same payload hashes. Two clean GNU core/gnulib and
diagnostic builds match against the same SDK objects; native upstream scripts
have three PASS and one expected XFAIL. No independent SDK/whole-image rebuild,
full guest upstream suite, RSS/true peak, in-call interruption latency, physical
or user-study claim. Local compact patch remains a narrower tested subset.

Independent review finds no blocker within the bounded profile. Delivery is
port first, then OS: [SrvrPorts #11](https://github.com/PaulMDemers/SrvrPorts/pull/11)
and [SrvrOS #369](https://github.com/PaulMDemers/SrvrOS/pull/369); linked PR states
record the merge outcome. [Portable evidence](assets/screenshots/diff-manifest.json)
and a verified ignored archive outside build cleanup retain source, failed and
passing proofs. Integration checkout and review VMs remain untouched; no shared
libc/kernel change. Source-only GNU patch preparation is isolated while delivery
finishes; further builds require a fresh resource window.

## xz bounded stream interchange — November 4, 2026

Authorized target: upstream XZ Utils 5.8.4 static single-threaded `xz`, preset-0
compression, `.xz` decompression and integrity testing, regular files and finite
stdin/stdout pipelines. Preserve inputs and require stdout for transformed data;
refuse automatic file replacement/deletion and unaccepted modes. Proposed codec
budgets are 8 MiB compression and 32 MiB decoding with no silent adjustment.

Work starts from `f4ea0ccb` in isolated `SrvrOS-ports-xz` on `ports/xz`. Port-local
allocator and cooperative signal recovery are explicit gates, not assumed SDK
capabilities. Required checks: authenticated pristine source, real target link,
host/guest byte interoperability and malformed/memory/write failures, repeated
blocked-I/O and CPU-active interruption, exact second-boot saved bytes, actual
Terminal workflow, supported tar interchange through an intermediate file, selected
upstream regressions, two clean CLI builds and existing ports/fsck checks.

Outside scope: threaded codecs, legacy/raw formats, helper scripts, custom filter
CLI, automatic replacement/metadata/sparse-file behavior and job control. Keep
integration checkout and review VM untouched; coordinate only a reproduced shared
blocker. Status: implemented and verified for this bounded profile. No shared libc or
kernel change was needed. Final guest receipt
`build/review/xz/20261104-135556-m_qxupp7` passes both-boot byte interoperability,
host preset-6 decode, zero-filled/multiblock/concatenated data, format/corruption/
memory/output refusals and preserved inputs. Allocator failure at every tested
site and 64 reuse cycles show zero net mapping growth. Repeated blocked I/O
SIGINT/SIGTERM restore flags; cancellation after observed codec progress passes.
This is cooperative recovery, not an in-call interruption-latency guarantee.

The actual Terminal workflow and two Ctrl-C pipelines pass at
`build/review/xz-terminal/proof-svpdq5tn`, with expert visual review. Six selected
native-host upstream programs contain 22 passing subcases and two skipped threaded
subcases; no guest full-upstream-suite claim. Two clean CLI builds match ELF hash
`e0efaa0d2b08361a0c56d58c247cea01704a4e4b1ad30ba9c68a8d5cc4128e1f`.
Existing ports smoke and guest fsck pass. All qualification is QEMU only; full
image reproducibility and physical acceptance are not claimed. Milestone complete;
draft PRs are open: [SrvrPorts #10](https://github.com/PaulMDemers/SrvrPorts/pull/10)
and dependent [SrvrOS #363](https://github.com/PaulMDemers/SrvrOS/pull/363).
Current-main merge review passes on clean `df34da93`, including the matched
build, both-boot memory/signal probes, actual Terminal walkthrough and ports/fsck.
Authorized delivery order is port first, OS second; linked PR state records delivery.

## End session and switch profile — delivered and verified November 4, 2026

The requested no-reboot session workflow is implemented and independently
qualified: **SrvrOS → Switch Profile…** resolves unsaved work, drains
accepted operations and retires the old session before opening a fresh chooser.
**Return to console** uses the same cooperative shutdown. Cancel keeps the
current profile; previously accepted closes may finish and closed apps stay closed.

Acceptance covers Text/Sketch Save/Discard/Cancel and failed saves, Terminal
confirmation, in-flight Files/Profiles work, direct app failure and retry,
a naturally retiring orphan, repeated A → B → A, separate preferences and
HOME/XDG/cwd, fresh clipboard/notifications, unchanged machine settings, exact
saved data and 640 × 480 keyboard operation. Full matched builds, focused actual
source host/sanitizer tests, independent source/storage/rendered review and three
disposable QEMU lifetimes establish the bounded result. The original graphical
receipt retains its environment-capture FAIL; a separate source-equivalent
context proof closes that gap without relabeling it. See the
[workflow and acceptance](portable-profiles.md#ending-sessions-and-switching-profiles)
and [composite review](assets/screenshots/profile-switch-review.json).

Runtime [OS #354](https://github.com/PaulMDemers/SrvrOS/pull/354) is merged as
`f58e0725`. [Apps #9](https://github.com/PaulMDemers/SrvrApps/pull/9) merged first
as `c3ad11b1`, then [OS #355](https://github.com/PaulMDemers/SrvrOS/pull/355) as
`82face72`. Both merged trees equal their reviewed candidates; OS pins accepted
Apps `2a8bc2d9`. Retained payload and host-source hashes match. No further work
is required for this target. Existing installation/creation evidence
and review VMs remain preserved; browser work has the heavy build/VM window.
Authentication, permissions, concurrent logged-in desktops and physical-media or
power-loss qualification remain separate. Actions remain disabled.

## Desktop profile creation — delivered and verified November 4, 2026

Users can create a portable profile from the startup chooser or Profiles app,
review its identity and six default preferences, recover an interrupted creation,
select it after reboot and migrate it to another QEMU installation. Merged
[SrvrApps #8](https://github.com/PaulMDemers/SrvrApps/pull/8) first (`f4017137`),
then [SrvrOS #348](https://github.com/PaulMDemers/SrvrOS/pull/348) (`0fb4ed6c`).
The OS pins reviewed Apps `0bf8e137`; the Apps merge has the same tree.

Acceptance passed on clean runtime source `1ef0ccdd`: matched build, 25 CLI test
groups, production catalog/desktop/fsync checks and CLI/catalog ASan/UBSan.
Twenty disposable VM lifetimes verify both creation entry points, empty initial
Documents, distinct UUIDs, invalid/occupied/capacity refusals, all five interrupted
creation/recovery boundaries, remembered reboot selection, and graphical export
and import into independently installed B with chooser/reboot/Text readback.
Identity, all six preferences and selected text/binary bytes survive; destination
HOME/alias/UID/GID differ while machine settings, unrelated profiles, exclusions
and legacy files remain local. The 640 × 480 chooser proof includes Ctrl-A,
Undo/Redo and Tab navigation. Rendered form/review/recovery/refusal/readback states
were independently inspected.

The coordinator independently verified 37 compressed image streams, initial and
final A/B filesystem contents, stopped/recovered profiles, and all 380 members of
the preserved evidence backup. See the [creation acceptance](portable-profiles.md#desktop-profile-creation--qualified-for-review),
[screenshots and source manifest](assets/screenshots/profile-create-manifest.json),
and [independent merge review](assets/screenshots/profile-create-review.json).
Later main `ba35d16c` adds host-only inspection: guest exFAT preprocessing is
identical, other guest inputs/proof tools are unchanged, and all 148 retained
artifacts/metadata match. This preserves the original runtime proof attribution;
no repeat VM run is claimed.

No further work is required for this target. Original A/B/transport/baseline
images and existing review VMs remain preserved. The installation chat's separately
authorized fresh-v2-B four-boot proof resumes after this resource handoff; it is
not an unfulfilled creation requirement. Physical-media and arbitrary power-loss
acceptance, authentication and permission enforcement remain separate work.

## less Terminal report browsing — November 3, 2026

Target: upstream less 710 static pager for plain ASCII text files and finite
pipelines, backward/forward navigation, g/G/line movement, literal search and
resize redraw. Work is isolated on `ports/less` from main `7ef140ef`; integration
checkout/review VM remain untouched. Required gates: truthful vt100 capability
adapter matching Terminal, returning signal callbacks with EINTR recovery,
pipeline key input from the real terminal, repeated quit/interruption and
termios/alternate-screen restoration, exact non-TTY output, selected upstream
regressions, two clean CLI builds and existing ports checks. Compile secure,
without regex; shell/editor/preprocessors/log writes/follow/job control and full
Unicode-width correctness are outside initial acceptance.

Implemented: the static pager, truthful local capability table, fd 2 keyboard
validation and deferred signal processing in the generated port copy. No shared
libc/kernel changes or concrete shared blockers were found. Guest PTY evidence
`build/review/less/20261103-225541-00_67wtr` passes file/finite-pipe navigation,
literal search, resize redraw, unknown TERM rejection, exact passthrough and two
blocked-read interrupts with the pipe writer held open until successful quit
and exact mode/screen restoration. The launcher inherits the PTY controlling
terminal as the actual shell does. The initial direct pipe fixture did not;
its failed signal receipt is retained and is not product acceptance.

Actual graphical Terminal evidence `build/review/less-terminal` passes report
browsing, pipeline keyboard input, resize redraw and shell continuation after
quit/interruption. Expert visual inspection confirms additional report lines
after resize and a restored usable shell; no user study is claimed. The two
unchanged upstream `seq200.lt`/`wordwrap.lt` fixtures pass 106 steps with the
native host LESSTEST runner in secure/no-regex mode. These are host regressions,
separate from target PTY evidence, not a claim that the upstream runner works
in the guest. Two clean target CLI builds are byte-identical (ELF SHA-256
`65b16e3ff879ca892f9d9cd45c8f22b8881cc22b31accdae7691f18f619a7f4c`);
whole-image reproducibility is not claimed. Existing `tools/ports_smoke.py` passes, including guest fsck with zero leaks,
FAT errors or filesystem errors. The bounded milestone and current-main merge review are complete: [SrvrPorts #8](https://github.com/PaulMDemers/SrvrPorts/pull/8)
and [SrvrOS #346](https://github.com/PaulMDemers/SrvrOS/pull/346). Merge dependency
order is port first, OS second. Current-main `83e5d1ad` requalification on clean
commit `1fa90b8c` passes matched build, guest PTY/held-open pipe signal gates,
actual graphical Terminal and ports/fsck regressions. PR state records delivery.
Only concrete shared blockers are coordinated with integration.

## Installed-desktop migration — delivered November 3, 2026

The coordinated QEMU milestone is complete. A used installed desktop can export
selected portable data through the Profiles application or the offline installer
CLI, then restore into a fresh installation with its own disk/home/local binding.
SrvrApps [PR6](https://github.com/PaulMDemers/SrvrApps/pull/6) merged first
(`2ebaca5e`), followed by SrvrOS [PR333](https://github.com/PaulMDemers/SrvrOS/pull/333)
(`60618a4c`). The installation owner separately merged the offline exporter in
[PR342](https://github.com/PaulMDemers/SrvrOS/pull/342) (`a64661e9`). Merge results
retain the reviewed code and the pinned Apps revision is an ancestor of Apps main.

Required outcomes and evidence:

- Graphical transfer: 19 disposable VM lifetimes passed, including all five
  interrupted-import recoveries, incomplete-export preservation, invalid and
  duplicate refusals, remembered selection/reboot, Text readback and a real
  640×480 review. Full identity/display name, six preferences and selected
  text/binary bytes match; synthetic unselected documents, credential placeholders,
  application data and caches stay on A and are absent from the bundle and B.
- Final combined runtime: a fresh four-boot proof on merged `60618a4c` edits,
  saves and reopens A, directly exports its stopped image, then provisions and
  reboots B with different GPT/home/alias/UID/GID and machine settings. All
  portable values and selected bytes survive; B retains destination-local state.
  Sixteen reason-matched export refusals and 33 inspection/export steps preserve
  the source, output and historical records as required.
- Independent review: source and rendered-flow reviews found no remaining
  blocker. All 40 graphical image archives were decompressed and hash-checked.
  Direct final A/B filesystem reads independently verified identity, preferences,
  documents, exclusions and local/machine separation. The combined images also
  contain the matched new `profile` and `deskprofiles` binaries; A's final whole
  image hash matches its export-source hash.
- Matched build, 70 installation tests (one optional matrix entry skipped),
  21 guest-profile groups, profile/session/fsync regressions and documentation
  checks pass. The first combined attempt stopped for storage before any guest
  boot and remains INCOMPLETE; it is separate from the subsequent passing run.

[Qualification record](assets/screenshots/installed-migration-review.json) and
[restored desktop](assets/screenshots/installed-migration-restored.png) identify
exact sources and receipts. See [profile operations](portable-profiles.md) and
[offline installation/export](disk-installation.md) for contracts and reproduction.
Full proofs, matched artifacts and restoration mappings are retained outside
build cleanup. Large inactive synthetic proof images were losslessly archived;
open images, existing interactive review VMs and source changes were preserved.

Acceptance is for the named QEMU runtime, not physical installation, arbitrary
power-loss durability, authentication or permission enforcement. Later opt-in
hardware diagnostics and salvage proofs retain their own acceptance. Hosted
Actions remain disabled. No further work is required for this migration target;
Unix filesystem semantics, account security and browser runtime remain separate
future milestones.

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
