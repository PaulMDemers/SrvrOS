# Working across device agents

Each Ubuntu Mac has a local agent, checkout, recovery installation and exFAT
workspace. `main` is the shared integration baseline. This guide describes how
to contribute to one OS without turning device branches into separate OS forks.
Read it with [AGENTS.md](../AGENTS.md), the [Ubuntu setup guide](ubuntu-device-agent-setup.md)
and the [machine records](machines/README.md).

## Start a work block

1. Read the actual model/unit record and its local checkpoint. Check Git status,
   branch, source commit, installed boot state and any uncollected run. Preserve
   existing work; collect an outstanding physical run before staging another.
2. Fetch `origin` and inspect `origin/main` and relevant open pull requests.
   Fetching does not update the checkout. Record the base actually used; never
   assume a remote fix is present because a chat or document mentions it.
3. Put the bounded objective, subsystem/files, dependencies and next checkpoint
   in your own machine record before shared implementation. Publish that record
   with the work branch; use a draft PR when a shared-code change is underway.
   Check other records/PRs for overlapping work first.
4. Build and test one candidate at a time in a checkout. Hardware/VM scripts use
   shared paths and `latest` pointers. Independent hardware can work concurrently;
   simultaneous builds, staging or VM harnesses in one checkout cannot.

Use `device/<model>/<topic>` branches, with a unit suffix when needed. Each
agent edits its own unit record. A record is a coordination notice, not a lock:
if two unpublished changes overlap, Git cannot prevent duplicate work. Resolve
the overlap before integrating. Do not overwrite another agent's ownership or
claim another physical machine's validation.

## Ownership and overlapping changes

The local device agent owns inventory, profile evidence, recovery, trials and
acceptance on that unit. The integration role owns review of shared contracts
and the order in which changes land; this can be the reference Air agent or a
user-designated reviewer. That role does not grant control of another machine.

For overlapping work, designate one implementation branch for the shared fix.
Other agents can produce sanitized descriptors, a reproducer, host fixtures or
results against that candidate. Keep unrelated device work moving. If no owner
has been agreed, prepare the reproduction and a small proposed change, mark the
conflict in its PR/record and defer conflicting integration. Do not silently
merge two competing implementations or force-push another agent's branch.

At this checkpoint the Air7,2 agent is taking the read-only power baseline and
native battery/AC runtime groundwork. New agents should start with their own
inventory and basic console/display/input/workspace acceptance. Shared ACPI,
EC, scheduler, input, storage and widget changes should consult that active work
before duplicating it. This is an initial allocation, not permanent ownership.
Refresh it in the machine records as work changes.

Git branches, machine records and PRs are the durable coordination surface.
Reading another task is not authorization to message it or schedule agents.
Use cross-task messaging only with direct user authorization; otherwise publish
reviewable repository work and surface an unresolved ownership conflict locally.
No background agent scheduler or automatic reboot permission is implied here.

## Share mechanisms; isolate hardware evidence

- Separate protocol/controller mechanics from model matching, report layouts,
  codec routing and quirks. Put shared controls in the widget library and shared
  window/input policy in the compositor, rather than copying them into apps.
- Add a device profile only from its captured IDs, descriptors and resources.
  A common enclosure or CPU family is not evidence of matching firmware, GPIO,
  audio routes, storage or EC registers. Unsupported hardware remains unavailable.
- Keep generic fixes and device enablement in separate commits where practical.
  A generic fix needs regression coverage for existing behavior; a new profile
  needs positive matching and negative/refusal cases. Do not make every sibling
  pass by widening a gate around one machine's constants.
- Keep blocking I/O out of input/render loops. Hardware operations need bounded
  waits, explicit worker/IRQ/queue ownership, cancellation and safe teardown.
  A timeout must not free memory or reuse DMA state still owned by hardware.
- Evolve shared structs, syscalls and wire formats deliberately: update all
  callers and tests together, document compatibility, and boot a matched kernel
  and userspace image. Do not copy a single binary from another release into an
  accepted payload and keep calling it the same release.
- Keep battery/AC readout, CPU policy, fan control, suspend/resume and GPU switching
  separate milestones. The Q35 full-uACPI fixtures do not authorize native EC/SCI
  activation. Ordinary desktop startup continues to omit optional AML probing.

## Integrate and consume changes

Publish coherent commits and a PR against `main`. Include the problem and
reproduction, affected profiles/contracts, exact tested source, host/VM results,
physical release/session if applicable, remaining unknowns and dependencies on
other PRs. Do not attach raw firmware, machine serials, credentials or private
USB files. Summarize sanitized evidence and retain the full local artifacts.

The integration owner reviews the diff and relevant CI before merging. Resolve
conflicts with the affected implementation owner; rerun tests when the merge
changes behavior. Keep `main` bootable. A passing CI job does not promote all
machines' capability ledgers to accepted. If a regression lands, revert or fix
the shared change with its evidence rather than hide it behind an undocumented
per-machine patch.

After a dependency lands, fetch and merge the actual `origin/main` into a clean
device branch, preserving local commits. Avoid rewriting published history or
reapplying a change already merged upstream. Record the new base, rebuild matched
artifacts and rerun affected host/VM checks before staging. Schedule the relevant
physical checks on that machine under its existing authorization. Acceptance of
an older release stays historical evidence, not a blanket pass for the new tree.

## October 20 battery baseline handoff

[PR #6](https://github.com/PaulMDemers/SrvrOS/pull/6) supplies the shared cached
battery service, explicit worker I/O eligibility, retired scheduler callback
handling, lifecycle/SCI diagnostics and automatic capture/collection mechanisms.
Consume this baseline before building another battery service. Its native backend
is gated to captured Air7,2 firmware/resources; shared mechanisms do not authorize
copying those gates or enabling a sibling's EC/SCI ownership. Ordinary desktop
battery startup remains disabled.

Air hardware accepts bounded cached-service lifecycle and USB **read** overlap.
The first v36 scratch write trial failed allocation preflight before native
runtime entry; ordinary offline fsck reported clean. Later independent traversal
confirmed eight orphan allocations; rehearsed recovery preserved all existing and
orphan data. Use both checks in the Ubuntu guide before future trials. The subsequent retry
accepts bounded scratch transport write/flush overlap. Ordinary filesystem
metadata updates, sustained desktop/input coexistence, gauge calibration and
the unclaimed SCI cause remain open. See the
[service contract](battery-service.md) and [Air record](machines/macbookair7-2.md).

Pro11,2 [PR #5](https://github.com/PaulMDemers/SrvrOS/pull/5) was inspected at
`7c9372b`; its battery work is deferred at the user's request. Preserve its v35
CPU-load capture and unit evidence when incorporating this baseline. Air uses
v36 for scratch writes. Reconcile `kernel/src/main.c`, `tools/hardware_boot.py`
and `tools/hardware_boot_smoke.py` by behavior, preserving both profiles and
automatic-return policy; do not resolve overlapping files wholesale with one
branch's version. Fetch the actual merged `origin/main`, rebuild matched artifacts,
and rerun affected staging/collector and VM checks before a Pro hardware trial.
Each machine retains its own UUID, recovery payload and acceptance ledger.
On the later inspection, Pro PR #5 at `3b3b66a` already uses `d72d2af` as its
base, consuming the merged PR #6 baseline; its local validation remains its own.

Air PR #8 adds the isolated v38 filesystem-image lease and sustained native
workload, based on merged PR #7 (`76508bf`). Air7,2 accepted 25 cycles over a
minute in release `c473b63c44d32cd0`; the outer workspace stays unmounted during
runtime. This does not enable ordinary desktop battery startup or concurrent
writes to a user's filesystem. Preserve Pro's separate v37 desktop network
profile when integrating the shared staging/main changes. Next Air scope is
bounding filesystem work for desktop/input coexistence and investigating the
remaining unclaimed SCI deliveries; no sibling EC gate is authorized.

## October 21 Air SCI handoff

Merged PR #24 (`b7bd60c`) checks pending EC query work before resuming its GPE,
retaining the post-resume check for late arrivals. Air7,2 release
`54dce7fb0a41b718` completed the isolated filesystem workload and two clean
lifetimes with 24 handled / zero unclaimed SCI deliveries, versus 32 / 32 in
the previous capture. This accepts the bounded trial; late-arrival races remain
possible. Consume the shared ordering helper and its host regression before
reworking query rearm behavior. IRQ claims, route polarity and native model gates
were not widened. It does not accept another model's EC ownership.

The next Air scope is an opt-in desktop/input coexistence capture with explicit
battery lifecycle and parked outer storage. Ordinary desktop battery startup
remains disabled. Coordinate overlapping main/staging edits with the existing
Pro Wi-Fi capture through actual branch/PR diffs; preserve both profiles.

## CI cadence

The user disabled repository Actions on October 20 to avoid further hosted
minute consumption. Validate PRs locally and record exact source, commands and
results before merging; do not re-enable or dispatch Actions. The workflow below
remains a reference for local coverage if the repository setting stays disabled.


The full build/VM baseline runs for pull requests targeting `main`, pushes to
`main`, and manual dispatch. Ordinary device-branch pushes do not also run a
second copy. Documentation-only diffs (`docs/**` and Markdown files) skip the
full baseline; run `make check-docs` locally. Pull-request path filtering considers
the full PR diff: a docs-only follow-up to an open code PR can still start a new
baseline. Batch acceptance notes before validation, or publish them as a separate
docs-only change after the validated code lands. A newer revision cancels obsolete
runs for the same PR or branch. Keep a PR draft while its scope is being formed,
but note that updating an open draft still triggers relevant PR validation.
Batch coherent changes rather than pushing every intermediate edit. CI evidence
must name the tested code revision; a docs-only follow-up does not claim a new
full CI run. If required status checks are added later, reconsider path filtering
so a skipped workflow cannot leave a required check waiting indefinitely.

## Evidence and checkpoints

Use distinct states: planned, inventory captured, implemented, host-tested,
VM-tested, physically accepted for named checks, failed, and not tested. Give
source/release identities and limits. A general positive user report accepts
what was actually exercised; it does not silently close unmentioned gestures,
audio routes, power readings or failure paths.

Before every physical boot, save an ignored local run record containing:

- model/unit, branch/source and dirty state; exact stage manifest and hashes;
- workspace UUID/mount, current/previous releases and independent recovery copy;
- exact-stage VM receipts, known failures, intended checks and acceptance criteria;
- prior session/screenshot names, evidence destination and reboot authorization;
- reboot state: prepared, installed, armed, returned, collected or failed.

Before ending a work block, add its current objective, commits/PR, test results,
known issues, running processes, the next concrete command and what must not be
repeated. A checkpoint belongs in ignored local storage for volatile state, plus
a concise portable summary in the unit record. Accepted recovery and required
evidence must have copies outside `build` before `make distclean`.

On return, verify the actual host/boot state and collect the matching new session
before another install. Copy screenshots separately and verify hashes. Missing
completion markers and halts are incomplete/failed evidence. Keep failed attempts;
do not overwrite them with a passing retry. Serial writers can interleave, so
inspect process/result evidence before diagnosing a timeout as a kernel hang.
Do not weaken a completion predicate merely to get a green run.

## First handoff for a new machine

Start the local agent with the [setup guide's handoff prompt](ubuntu-device-agent-setup.md#9-agent-handoff-and-integration-contract)
and a copy of the [unit record template](machines/TEMPLATE.md). It should first
publish its actual identity/inventory, source/base and bounded scope. Each host
uses its own exFAT UUID and sudo/reboot authorization. Do not start by replaying
Air7,2 commands with its UUID, accepted release or firmware gates substituted
into a new machine's record.

## Air RAM coexistence handoff (2026-10-21)

PR #26 adds the opt-in v40 laptop `--battery-desktop` capture and strict collection
proof. Pro PR #19 keeps v39 for Wi-Fi; do not renumber or combine those recipes.
The Air change includes a generic single-pass bulk unmap fix with split and
unrelated-mapping regression coverage. It does not enable ordinary desktop
battery startup or widen native hardware gates. The initial study exercises
legacy text input in RAM; rich Text-app events and filesystem load are separate
acceptance work. Use the Air unit record and exact run receipts for native state.


## Air full Text coexistence handoff (2026-10-21)

Merged PR #29 (`01e64a9`) adds the distinct v41 `--battery-text` automatic capture
and strict `battery_text` collection proof. Air7,2 accepted release
`2fd49386c4ecf0d9`: all 60 generated edits and five exact document checkpoints,
continued battery sampling, clean retirement, 23 handled / zero unclaimed SCI
interrupts and preserved USB contents. This is RAM-only generated editing, not
physical input latency or general desktop battery acceptance. v40 remains the
original study; Pro PR #19 at `e9320d9` retains its independent v39 Wi-Fi scope.

Next Air scope is combining this RAM Text workload with the existing isolated
filesystem-image workload in VM, preserving lease ownership and bounded teardown.
Other devices can consume the shared capture/collection contract; native model
and firmware gates remain evidence-specific. Ordinary desktop battery stays off.


## Air combined-load milestone closeout (2026-10-21)

Merged PR #31 (`421a69b`) adds v42 `--battery-load`: generated RAM Text editing,
exclusive disposable filesystem-image work and cached battery sampling. Air7,2
accepted release `c3746988ed5c0d5b`: 60 presented edits, 45 complete filesystem
cycles over 60.017 seconds of common activity, generation 1→25, clean retirement
and 25 handled / zero unclaimed SCI interrupts. Exact image contents, inner/outer
allocation, other-file preservation and automatic Ubuntu return verified. The
[unit record](machines/macbookair7-2.md) records the complete acceptance scope.

Shared code includes corrected compositor child-reap accounting at final cleanup
and retention of disposable VM disks on failed load assertions. Deadlines and
scheduler/hardware eligibility did not change. Other agents should consume these
commits before duplicating the lifecycle or collection work. Pro PR #19 inspected
at `39c25ce` retains v39 Wi-Fi scope. No sibling EC gate is accepted by this run.

The bounded milestone is complete; ordinary desktop battery startup remains off.
Attended input/gesture latency, regular app filesystem access while sampling,
gauge accuracy and eventual ordinary startup are separate acceptance work.
There is no new physical trial armed or shared implementation in progress for
this closeout. Actions remain disabled; validation ran locally.


## Air application-file and cached battery interface (PR #33)

Air's v43 `--battery-app-files` extends coexistence to the real asynchronous Text
file worker in the disposable exclusive image. It retains descendant identities
and the kernel-created process group until all clients retire. The shared v1
`SYS_BATTERY_STATUS` ABI is a read-only cache snapshot; desktop and System use the
shared battery presentation helper without starting hardware work. Explicit user
lifecycle controls and ordinary workspace coexistence remain pending; automatic
startup is still disabled. Native acceptance must come from this unit's matching
capture, not the VM fixture. Pro PR #19 inspected at `c263221` retains independent
Wi-Fi scope, including v39; preserve both profiles when reconciling shared files.


Air7,2 v43 release `27c39e5404ab573b` subsequently passed native collection:
five real-worker file cycles, 60 generated edits, continued cached UI/service
sampling, clean three-process retirement and exact offline contents/allocation.
The accepted source is `977a554`; native gates remain Air-specific. A host-only
collector fix treats missing expected paths in a replaced historical test image
as invalid evidence instead of aborting collection. Consume PR #33 before adding
another battery UI read interface or descendant-retirement implementation.


## October 21 scoped battery controls handoff

Air PR #34 adds a separate asynchronous command syscall and a process-group-scoped
mailbox; the existing cached read ABI stays read-only. System Sensors uses shared
Start/Stop presentation, with pending, stale-epoch and lifecycle admission checks.
The owner dispatches outside syscall/storage work, revokes admission, retires the
client tree and stops the service before returning. Automatic native v44 acceptance
is limited to captured Air7,2 with the outer workspace parked. Ordinary battery
startup stays off; sibling model/resource gates are unchanged.

The local host/VM suite and exact-stage refusal/save/return checks passed. Air
release `87524e9fe7788772` (`134f0d6`) completed two native lifetimes/four commands,
clean retirement and file-preservation/allocation/Ubuntu-return checks. Reuse this
control contract before designing another battery UI. Ordinary Home coexistence
is the next Air checkpoint. Pro PR #19 at `c6ad8e9` consumed PR #33 and retains its
separate Wi-Fi firmware capture; preserve that profile when integrating main and
staging changes. Fetch the actual merged baseline and rerun local affected tests.


## Air native recovery audit handoff

The [native AML map](battery-native-aml-audit.md), based on PR #40 `d49ef02`,
identifies shared GNVS/graphics/GPIO startup writes, non-rollback disconnect and
query 80's denied GPU/chipset path. Do not broaden native gates or infer safe
recovery from an idle EC/Global Lock snapshot alone. The Air's next bounded scope
is structured query-admission failure after completed QR_EC and before unapproved
AML, with operation/effect receipts and orderly retirement tested in VM first.
Native mounted Home and automatic startup remain off. Pro PR #19 at `e871a1e8`
is independent Wi-Fi work; this audit has no code overlap. Raw firmware stays
machine-local. Other agents should consume the contracts, not copy Air firmware
fingerprints or acceptance.


## Air returned-query study handoff (PR #42)

The explicit Q35 `battery-workspace-query` command now propagates an unsupported
query as a normal worker return, drains notifications and retires the failed
service through its existing close path. Reuse the batch receipt and resource
checks; do not infer safe native recovery from them. Partial shared writes,
retained firmware locks, poisoned protocols and failed disconnect still halt.
The study also fixes final owner bookkeeping eligibility during unrelated storage
I/O, with a forced yield after close to prevent a timing-dependent pass.

The VM admits only `_Q20`; native query behavior and model gates are unchanged.
Native mounted Home/startup remain off. Next Air scope is the exact captured
firmware/OS-state admission policy before another parked-workspace native capture.
Pro PR #19 at `faed5b7` retains independent Wi-Fi scope; only separate Makefile
test targets overlap. Local tests replace disabled Actions. No hardware acceptance
or installed payload changed in this milestone.


## Air query-policy handoff (PR #43)

The exact captured Air7,2 binding now selects a shared predispatch classifier.
It refuses unknown methods, GPU/power query80 and query10's mutable OS-dependent
SMBus branch before AML. Do not re-enable query10 based on one or two Darwin
snapshots: OSYS is shared firmware state. QueryCF is bounded in both branches;
query20/21 remain explicitly shared-write methods. See the
[full native policy](battery-native-aml-audit.md#exact-air72-dispatch-policy).

The same classifier is replayed in synthetic Q35 fixtures, including both OS
states and refusal after shared effects. Native refusal is still terminal; the
VM normal-return proof does not grant native recovery, sibling firmware access,
mounted Home or automatic startup. The installed Air release is unchanged.
Next Air work is scoped native normal-return admission followed by an automatic
parked-workspace capture. Pro PR19 retains independent Wi-Fi scope; preserve its
separate Makefile targets. Actions remain disabled and validation stays local.


## Air parked capture integration (PR #44)

The Air branch adds v45 `--battery-query`, a bounded natural query-refusal capture,
with exact parked-device admission and normal retirement before remount/save/reset.
Native firmware/register gates are unchanged. Air7,2 release `a72409383258a4a0`
(source `6f6381c`) now accepts natural query10 predispatch refusal, resource-checked
retirement and saved evidence after remount, with all prior files preserved and
clean offline allocation/Ubuntu recovery checks. It accepts no battery reading,
mounted Home, automatic startup or other machine. Next Air scope is query10
protocol/notification ownership across both OS branches, starting in VM.
Shared changes touch battery runtime, main capture dispatch, staging/collector,
VM fixtures and tests. Local validation replaces disabled Actions.

PR19 at `43a1154` also edits main/staging/tests and uses v44 for Wi-Fi
calibration, colliding with the already merged battery-controls v44. Integration
must preserve both historical exact recipes and give new Wi-Fi calibration stages
a descriptive, unique identity (as its firmware recipe already does). Do not
reinterpret an installed v44 payload or discard either profile. The Air query
profile uses v45; no Pro branch or installed payload is modified here.


## Air SMBus event study handoff (PR #45)

Air implementation `1797e8a` adds a VM-only host-controller query10 owner. Consume
the [event contract](battery-smbus-events.md) before duplicating SMBus alarm
handling: complete the EC transaction, serialize the bank, capture the alarm
before acknowledging, and invalidate cached data without recursively sampling.
The two synthetic OS branches deliberately reject AML entry. Native query10
classification and installed capture remain unchanged; the next Air scope is
an explicit parked native capture, not ordinary battery startup or mounted Home.
Pro PR19 `9ecf749` remains separate Wi-Fi work. Only the appended Makefile target
is shared; preserve its independent targets and historical capture identities.
Local validation replaces disabled Actions. No other device acceptance is implied.


## Air native SMBus capture handoff (PR #46)

Implementation `10a4cbe` adds the distinct v46 automatic `--battery-smbus`
capture. Air7,2 release `a1451825851dc5f0` accepts 45 natural completion events,
fresh generation 3, clean retirement and preserved USB/automatic Ubuntu return.
No alarms occurred: acknowledgement and invalidation paths remain VM-tested only.
Use the shared strict event/cache/retirement proof; missing final logs remain
incomplete, and native capture ownership exists only with exact parked storage
and the existing Air firmware/resource profile. Ordinary query10 dispatch,
mounted Home and automatic battery startup remain unchanged/off.

Consume the shared implementation before duplicating host-event handling. The
next Air scope is sustained event and alarm/stop-race coverage with Text/file
load in VM before another native coexistence trial. Pro PR19 `79132d6` remains
independent Wi-Fi work and overlaps main/staging/tests; preserve both profiles
and historical recipe identities. Actions remain disabled; all validation is
local. No other machine's firmware or physical acceptance is implied.


## Air SMBus workspace/Stop study (PR #47)

Air implementation `b1e80f0` adds a VM-only mounted Text/file study with repeated
SMBus completion and known/unknown alarms. The normal runs complete 60 edits and
five file cycles; read/acknowledgement Stop races explicitly report interrupted
workload with clean retirement. An uncertain acknowledgement retains containment.
Consume the masked-drain-before-resource-check correction before duplicating the
study. The native capture already checked after drain, and native invocation of
the new command refuses before platform access.

No native boot recipe or profile changed. The next Air scope is a parked
exclusive-image coexistence capture with a measured event budget, first in VM.
Native Home/startup stays off. Pro PR19 `79132d6` remains independent Wi-Fi work;
shared scope is only separate Makefile test additions. Actions remain disabled.


## Air sustained parked SMBus/Text/file handoff (PR #48)

Implementation `029e344` adds v47 `--battery-smbus-files`, reusing the real Text
worker and exclusive filesystem image while the outer workspace remains parked.
Air7,2 release `fda709928307b1b1` accepts 345 natural completion events, fresh
cache generation 23, 60 edits/five file cycles, clean retirement, exact contents
and file preservation, both allocation graphs, and automatic Ubuntu return.
The short v46 capture remains unchanged. Reuse the strict combined proof and
compact event receipts; do not accept an incomplete workload or quick reset.

Native alarms did not occur and remain VM-tested only. The native owner still
requires the exact captured Air firmware/resources and parked image lease.
Ordinary mounted Home and automatic startup are OFF. Next Air scope is auditing
lifecycle/storage ownership for explicit Home coexistence, VM first. Pro PR19
`c20917f` retains independent Wi-Fi work; preserve its profiles and tests when
integrating shared main/staging changes. No sibling acceptance is implied.
All validation ran locally with Actions disabled. Evidence/payload backups are
hash-verified outside `build`; no physical trial is armed after this closeout.


## Mounted Home control lifecycle study (PR #49)

Air implementation `d223325` reuses the System mailbox/UI in the explicit,
VM-only `battery-workspace-controls` command. Repeated sessions exercise four
lifetimes with SMBus events; manual Start/Stop/restart, active-window close and
early cancellation retire cleanly. Native/unmounted/profile refusal and uncertain
acknowledgement are covered. A live uncertain event remains halted with mounted
storage; do not replace this with forced flush/unmount/reset to make a test pass.
Clean runs preserve the complete disposable volume and permit a new compositor.

Consume the [Home lifecycle audit](battery-home-lifecycle.md) and epoch-correlated
proof before expanding native admission. Mounted-volume identity/revocation and
controls during asynchronous file work are the next Air scope. No native boot
recipe, firmware gate, accepted payload or sibling support changes here. Pro
PR19 `3d51e4f` remains independent Wi-Fi work; preserve its tests/profiles. Local
validation replaces disabled Actions, and no physical trial is armed.

## October 22 mounted battery ownership handoff

PR50 adds CPU0 workspace mount holds for the VM battery controls and SMBus/file
studies. Keep the exact device/parent/mount lifetime pinned through client and
runtime retirement; Start/Stop must not release the surrounding session's hold.
Unmount and diagnostic conversion refuse while held, while regular file I/O
remains available. A stale token cannot release a later session. This is not an
I/O lock, native Home enablement or permission to reset mounted storage. Air
retains the next controls-plus-asynchronous-file integration scope; see the
[delivery plan](battery-home-lifecycle.md). Other machines should consume the
shared primitive, not duplicate it or widen native firmware gates.


## October 22 controls during file work handoff (PR #51)

Air implementation `23837b9` adds bounded `desk_file_job_submit` to transmit an
asynchronous request without consuming its reply. Existing poll callers retain
the combined API; do not duplicate transport loops or kill a worker with an
accepted write during close. The explicit Text fixture uses the same scoped
battery syscall as System, with submitted application jobs, repeated lifetimes,
active-service exit and cancellation. Actual System buttons remain separately
covered. Six directed VM cases and three regressions pass, including exact file
contents/allocation and uncertain-runtime containment. This does not prove USB
DMA overlap, native Home operation or physical input latency.

Next Air scope is native mounted normal-return/refusal and desktop-session
ownership; ordinary startup stays OFF. Native profiles and accepted payloads
are unchanged. Pro PR19 `77ba966` remains independent Wi-Fi work. Consume the
shared submission/ownership helpers before adding another transport or lifecycle.


## Air mounted Home boundary handoff (PR #52)

Compiled `7fac77d`, validators `171b307`, release `71ab6b95892a9a8c` now accept
explicit native Home ownership with no desktop clients: generation 3, 45 natural
completion events, checked retirement, mount release and clean save/unmount/Ubuntu
return. All unrelated files are preserved and offline allocation is clean. Native
alarms were not observed. Ordinary startup remains OFF; next Air scope is native
desktop-session ownership with System controls and asynchronous file work.

Reuse the held-mount normal-return boundary; do not broaden firmware gates or
reset a live uncertain runtime. Shared console logging now appends bounded 1 KiB
records before rendering/UART to avoid interleaved saved diagnostic receipts.
Rendering remains in 64-byte batches. Arbitrarily long streams and live serial
output are not promised atomicity. Local tests include deterministic competing
writers and all 16 final VM/capture/regression checks; Actions remain disabled.
Pro PR19 `92a1bbc` retains independent Wi-Fi scope; preserve both main/staging
profiles and Makefile tests. No other machine's acceptance changes.


## Explicit Home desktop owner (PR #53, native trial pending)

Implementation `0a670c8` reuses the System mailbox and descendant-retirement
owner for an explicit held-Home session, including submitted Text saves and
active-service close. Preserve the exact mount across every Start/Stop epoch,
revoke controls before client teardown, then retire runtime before release.
The native profile stays exact and ordinary startup stays OFF. Compact event
records share the existing decoder and the explicit session has a 512-event
aggregate limit; do not infer unlimited desktop monitoring from this diagnostic.

Fifteen directed ownership cases and nine final boot/regression cases pass in
local VMs. The v49 automatic profile saves/returns on completed or handled failed
sessions; uncertain live ownership still halts. Expected Text failure is distinct
from a kernel runtime failure. A refused query can coincide with a submitted
first save, requiring valid incomplete-draft preservation, not an untouched disk
or a completed workload. Native acceptance remains pending. Pro PR19 `92a1bbc`
retains independent Wi-Fi work; preserve its main/staging profiles and tests.

The first v49 native session (`63333d27f5e5c064`) returned a **failed** workload
after the compositor's diagnostic progress timeout, with clean runtime retirement
and preserved existing files. A disposable VM reproduced the early stop with
four-second EC connections and throttled USB writes. The correction gives only
generated control/file checkpoints a 15.7-second wait and the study 90 seconds;
normal desktop input/client deadlines and the 110-second owner limit stay intact.
The slow regression completes all 60 edits and five exact file cycles, including
7.98–8.43-second checkpoints. This remains VM evidence pending the native retry.
Pro PR19 `fe5c0a8` and Air6,1 PR54 `953103b` were inspected; their Wi-Fi and
inventory/setup scopes do not overlap this compositor diagnostic correction.
