# Apple Silicon macOS build host

## Graphical profile transfer — qualified for review

User authorized the Profiles app, reviewed export/import and disposable two-VM
qualification; authentication and permission enforcement remain separate. Work
uses `SrvrOS-profile-326-review`, branch `device/apple-silicon/profile-transfer`.
Final GUI proof source `f2360267` includes main `7ed98371`, Apps `b2220909`, UI
`9eaaa74e` and Ports `747cca28`. Receipt `proof-yuylqeu1/receipt.json` records
PASS across all 19 VM lifetimes, including all five recovery checkpoints,
interrupted export, full identity/six preferences/exact selected bytes, four
exclusion fixtures, chooser/reboot/Text and real 640 × 480 scrolling review.
Host and ASan/UBSan checks pass; full evidence, matched artifacts and sources are
byte-verified outside `build` in `.local/profile-transfer/final-proof-yuylqeu1/`.
The portable-profile guide and committed screenshot manifest record exact hashes,
checks, preservation and qualification limits.

Latest main `7ef140ef` is integrated after proof. Clean source `c813f078` passes
a fresh matched build and focused checks. Of 144 payloads, 143 are identical;
only sysabi diagnostics change and bc is added. Profile/GUI/proof-tool sources
are unchanged. The committed integration receipt preserves both source
attributions and all hashes. Apps PR6 precedes OS PR333.
The coordinator owns independent combined review and merging; hold these merges.
The installation owner separately proves offline used-image export and fresh
restoration. Preserve the primary checkout and review VMs 6233/61610/85869. No
physical media/reboot or Actions; one build/harness at a time. Completed proof
snapshots are losslessly compressed only after byte verification and last use,
with a bounded archive budget and retained restore mapping.

## PR338 managed filesystem merge review

Review consumes main `a37eb7b2`, then `f0c88a6d` and `b78e3ed5`, preserving all
host notices. Revalidated all 154 original evidence hashes. After jq changed the
runtime image, matched `1e83ee2c` passes fresh three-boot managed USB qualification
and all 11 canonical/CLI cases. Complete fresh proof backups are independently
cloned/hashed (154 files) at `.local/filesystem-evidence/pr338-review/`. Focused
host checks and 19 installation-inspection tests pass using the documented Darwin
feature macro. PR317 owner reports no current blocker; explicit slot/terminal
work stays separate. Review VMs 61610, 6233 and 85869 remain untouched; all review
guests stopped. See the [filesystem contract](../filesystem-layout.md) for limits
and the next proposed late-flush proof. User authorized merge if ready.

## Managed USB workspace filesystem proof

Isolated `SrvrOS-filesystem`, branch
`device/apple-silicon/filesystem-managed-salvage`, consumes main `5634cd9d` and
subsequently `62b0708e`. PR317 lifetime review precedes the fixture-only BOT fault;
no terminal stop/reclaim or production process/block/driver changes. Matched
`3c480516` passes clean control, retained-reader salvage/failed retirement and
restart readback through actual workspace commands; canonical compatibility
passes all 11 cases. Combined `351eb986` produces identical tested payloads.
The [filesystem contract](../filesystem-layout.md) records exact limits and
154-file SHA-256-verified backups outside build cleanup. All task VMs stopped;
review VMs 61610, 6233 and 85869 and primary checkout remain untouched. Delivery
is a scoped draft PR; no physical trial, hosted Actions or merge requested.

## PR336 integrated-main merge review — ready

User authorizes review and merge if ready. Reuse the isolated
`SrvrOS-installation-io` checkout and existing inspection branch at `b3c6a694`;
fetched main remains `62b0708e`. Own review/tests, proof-equivalence receipt and
installation continuity documentation only. Inspect filesystem/profile contracts
and pending PR333 without consuming unmerged changes. Preserve review VMs
61610/6233/85869, primary checkout and prior evidence; one build/harness here,
no physical media, host reboot or Actions.

Review finds no material blocker or unresolved thread. Fresh installation checks
pass (55 passing and one default skip, including 19 inspection groups), as do
profile/guest and documentation checks. All accepted artifact/tool/native-source
and build-output hashes match; the retained used stage matches its final proof
snapshot. Two fresh read-only used inspections pass and strict publication
inspection/reconciliation still refuse, preserving stage/source/receipt bytes.
Unchanged main/runtime inputs preserve the source-attributed two-boot and refusal
proof; no new VM is needed. Review receipt/logs/script survive at
`.local/installation-evidence/pr336-review`. Ready for the authorized merge.
Next proposal: read-only selected-profile export directly from a used image,
using a bounded read-only document reader, strict portable bundle codec and
fresh-install restoration proof. Dependencies and acceptance are in the
installation guide; no dependency on unmerged desktop-transfer/salvage work.

## Read-only used-installation inspection — qualified

User authorized a scoped PR with used-image and refusal proof. Reuse isolated
`SrvrOS-installation-io`, branch `device/apple-silicon/installed-state-inspection`,
main `5634cd9d`. Inspect current filesystem/profile contracts; own the separate
host inspector, inherited-read-only-fd native metadata reader, CLI/tests and proof.
Original publication/reconciliation rules remain unchanged; receipts stay
historical after guest use. No kernel/provisioning/profile-policy implementation.
Validate boot/root identity, current allocation/admission and bounded profile
metadata; refuse uncertain state without cleanup. Preserve review VMs
61610/6233/85869 and primary checkout, one build/harness at a time, no physical
media/reboot/Actions. Acceptance/evidence are recorded in the installation guide.

Clean `ff974bdf` passes the matched build, 19 new inspection groups and combined
installation/profile checks. The real two-boot proof records six successful
pristine/used inspections, 23 mutation refusals plus a busy lock, four unchanged
publication refusals and exact source/stage/receipt preservation. Guest profile
identity/preferences/data/env/apps and protected regions pass; native writes and
flushes stay zero. Reopened Text was visually checked. Raw proof and artifacts
are byte-verified outside build cleanup at
`.local/installation-evidence/used-proof-4k419y9m`. Review VMs remain live and
untouched; task VMs stopped. Publish a scoped draft PR; no merge requested.
Used v2 needs its retained profile snapshot, used v1 its exact old data source.
On-disk consistency is not authentication, live health or physical durability.
Final publication refresh integrates main `62b0708e`/PR335, an unrelated
host-only Haswell audit. Fresh combined host and documentation checks pass;
all proof tool/native-source and matched artifact hashes remain unchanged.
The accepted VM evidence retains its original clean-source attribution.

## PR330 merge review

Final refresh consumes main `10890eed`/PR329 as `740869f0`. Fresh file-fsync,
installation/profile/chooser/guest checks and the matched build pass; the full
seven-error/two-boot production proof was rerun because guest save uses the
changed syscall. All matrix component hashes still match. Verified raw proof
and artifacts survive at `.local/installation-evidence/pr330-fsync-review`.
This supersedes the earlier unchanged-main runtime-equivalence assessment.
Review VMs remain live and untouched; ready for authorized merge.

Initial review, authorized to merge if ready: main `c90f3522` was already
integrated in `5d0fa39e`; independent review finds no material blocker or unresolved
thread. Fresh installation/profile/chooser/guest checks and the matched build
pass. All matrix/smoke tool and artifact hashes, receipt hashes and the final raw
stage snapshot match accepted evidence, preserving the 436-case/seven-refusal/
two-negative-control/seven-production-error/two-boot proof without another VM.
Review logs and build metadata survive at `.local/installation-evidence/pr330-review`.
Original review VMs remain live and untouched; no physical work or Actions.
Ready for authorized merge. Next proposal is read-only post-boot installed-state
inspection with the existing filesystem/profile refusal boundaries and a bounded
read-only metadata reader; see the installation guide for dependencies/acceptance.

## Host publication I/O-failure qualification — complete

User authorized a scoped PR with write/sync/link/unlink failure, reconciliation
and refusal proof in isolated `SrvrOS-installation-io`, branch
`device/apple-silicon/install-publication-io-qualification`, main `c90f3522`.
Own host publication tool/fault fixtures and installation docs only. Latest
integrated filesystem/profile contracts, including PR326's identity metadata and
PR327/328 salvage boundaries, are inspected and consumed. Separate storage-guard
work stays intact in the earlier checkout, with private build outputs here. Reconciliation
must re-establish verified stage-file and namespace syncs after uncertain errors,
including an already visible ready receipt. No guest or kernel policy changes.
Preserve review VMs 61610, 6233 and 85869, primary checkout and accepted evidence.
One build/harness at a time; no physical media, reboot or Actions. Acceptance
and exact evidence are recorded in `docs/disk-installation.md`.

Clean `3eae3223` passes 436 bounded host fault cases, seven refusal controls and
two source-mutant negative controls. Every recorded matrix component hash still
matches after consuming `c90f3522`/Ports `ca749517`. Fresh host checks and the
matched full build pass at `b6ffb556`; the production image/driver proof reports
all seven selected errors, reconciles identical candidate bytes, and passes
two edit/save/reboot/readback boots. Source/profile/machine/protected-region
and allocation checks pass; guest-used images refuse recovery without mutation.
Reopened Text was visually checked. Byte-verified raw evidence and artifacts:
`.local/installation-evidence/io-proof-j8xbfmgc`. Original task VMs stopped;
review VMs remain live and untouched. Publish a scoped draft; no merge requested.
Power loss, media/device faults and automatic repair remain outside acceptance.

## PR326 merge review — complete

User authorized review, fixes and merge after passing checks. Review the chooser,
remembered selection and read-only recovery preflight, integrating main `1f177761`
(PR327's salvage helper) after starting from `4b5ddb7c`,
preserving guest import ownership, filesystem policy, authentication and
permission enforcement as separate concerns. The branch is conflict-free and
has no existing review findings. Review found no blocker within the documented
transaction/selection scope; preserve both test targets in the Makefile merge
conflict. Fresh host checks, 29 installation tests, ASan/UBSan CLI/catalog checks
and the full matched build pass. Because the rebuilt kernel/bootstrap differ
from the earlier proof, clean `115899c3` passes a fresh complete 14-lifetime VM
proof, with small-screen and recovery screenshots inspected. Exact receipt and
scope are in [portable profiles](../portable-profiles.md#pr326-merge-review).
Evidence/artifacts are verified outside build cleanup at
`.local/profile-326-review/proof-yrfw5f6o/`. Ready for authorized merge.
A bounded additional Escape/Enter guest probe passed without
starting a selection write or recovery. Only disposable copies were touched.
Use isolated `SrvrOS-profile-326-review` because separate proof-storage work is
active in `SrvrOS-profiles`; preserve its uncommitted files. Primary checkout and
review VMs remain untouched, with no Actions or physical trial requested.

## Desktop profile chooser — proof complete

User authorized the desktop chooser, installation-local remembered selection
and recovery status with two disposable VM installations. Isolated checkout
`SrvrOS-profiles`, branch `device/apple-silicon/profile-chooser`, consumes merged
main `4f5edb0e`, Apps `3cf32af2`, UI `9eaaa74e` and Ports `5e994f53`. Own startup
chooser/catalog, local choice persistence, shared read-only guest recovery
preflight, host tests and the new proof harness. Existing import ownership,
filesystem writeback/salvage policy, authentication and permission enforcement
remain separate. Installation publication recovery PR325 is independently owned.

Clean implementation `3f9c225a` passes the full matched build, focused host checks
and 14 VM lifetimes covering independent remembered choices, guest migration,
640 × 480 interaction, interrupted import recovery and blocked-state preservation.
Prior unchanged code also passes the desktop host suite and ASan/UBSan. The
legacy installed desktop and installation-profile regressions pass. Exact source,
receipt, screenshots, limits and reproduction are in
[portable profiles](../portable-profiles.md#desktop-chooser-milestone--complete-for-review).
Evidence and matched artifacts are byte-verified outside build cleanup at
`.local/profile-chooser-evidence/proof-3zcf8_ih/`; earlier failed proof attempts
remain retained. Final acceptance changes only documentation/screenshots.

All task VMs are stopped. Review VMs 61610, 6233 and 85869 remain running and
untouched, with the primary checkout preserved. No physical media, reboot,
Actions or merge requested. Delivery is a scoped draft PR for review.

Published as [PR326](https://github.com/PaulMDemers/SrvrOS/pull/326). Main
`4b5ddb7c` (PR325) is now integrated, preserving both unit-record sections.
Guest runtime trees and all recorded build payload hashes are identical to the
accepted chooser proof; its source attribution stays unchanged. Fresh combined
29 installation and chooser/profile host checks pass. The portable-profile
integration receipt records exact source/hash equivalence and host-tool changes.

## PR325 merge review

User authorized review and merge if ready. Candidate `8b4619b6` integrates main
`4f5edb0e` and PR323's host-only salvage qualification. Independent review finds
no blocker in the bounded recovery contract. Fresh 35 installation tests and
profile/session/bundle/adapter checks pass. The accepted sequential full build
and unchanged tools reproduce all original proof input hashes, preserving the
nine-interruption/sixteen-refusal/two-restart/two-boot evidence without another
VM. Review receipt/logs survive at `.local/installation-evidence/pr325-review`;
see the installation guide for the portable receipt and equivalence details.
Review PIDs 61610, 6233 and 85869 remain live and untouched; primary checkout
preserved. Ready for authorized merge. Next proposal is bounded host publication
I/O-failure qualification, retaining filesystem/profile refusal contracts.

## Interrupted publication recovery — proof complete

User authorized a scoped inspect/reconcile PR and interruption/refusal proof in
`SrvrOS-installation`, branch `device/apple-silicon/install-publication-recovery`.
Base main `0cba30a1` includes PR321 and the inspected test-only PR322 failure
qualification. Profile guest PR320's pending-import/unknown-state rules remain
separate. Own installation publication API/CLI/tests and a new proof; no guest,
kernel, filesystem or profile policy edits. Reconcile only previously verified
unbooted output; uncertain state is preserved. Process-restart boundaries are
qualified, not power cuts or transport-failure repair.

Preserve VMs 61610, 6233 and 85869 and accepted evidence. Run one build/harness
at a time. No physical media, reboot or Actions. Current acceptance and exact
evidence are recorded in `docs/disk-installation.md`.

Clean `291f469e` passes the matched full build, eight recovery host groups plus
existing installation/profile regressions, and nine actual apply-interruption
boundaries. Three early states refuse untouched; six verified states reconcile
without candidate-byte changes. Two reconciliation interruptions restart safely;
16 corruption/refusal cases preserve state. A reconciled profile image passes
edit/save and reboot/readback; subsequent guest writes cause reconciliation to
refuse while retaining its ready receipt. Verified raw evidence and artifacts:
`.local/installation-evidence/recovery-d86be75i`. Task VMs stopped; original
review PIDs remain live. Delivery is a scoped draft PR; no merge requested.

## PR321 merge review

Final candidate `d0910498` consumes newer main `a8e1fca1` and PR320's guest import
startup guard. Fresh full build, profile/installation/guest host checks and the
complete four-boot/12-refusal proof pass. Verified evidence and matching artifacts
survive at `.local/installation-evidence/pr321-current-tkf2agxs`. Task VMs stopped;
review VMs 61610, 6233 and 85869 remain live and untouched. This refresh supersedes
the initial equivalence-only readiness assessment below.

User authorized review/merge of profile plans. Candidate `6de308a1` integrates
current main `805519b2`; focused installation/profile host checks and full build
pass. Runtime equivalence preserves the four-boot/12-refusal acceptance from
`bf6efeb2`: only unused sysabi fixture payloads changed, including their archive
copies; kernel, minimal bootstrap, drivers and all exercised apps/data match.
Evidence/logs are retained at `.local/installation-evidence/pr321-review`.
No additional VM was required; review VMs 61610, 6233 and 85869 remain untouched.
See PR321 for merge state and the installation document for the proposed bounded
interrupted-publication inspection/recovery checkpoint.

## Profile-aware installation plans — proof complete

User authorized a scoped PR and two-installation persistence/refusal proof in
`SrvrOS-installation`, branch `device/apple-silicon/profile-install-plans`, main
`c7be6574`. Consume merged profile PR309/314, namespace PR313 and acceptance PR318;
Apps pin `3cf32af2`, Ports pin `5e994f53`. Own installation plans/tests and new
proof harness; shared profile preflight is extracted from the existing adapter.
The existing installed/canonical harnesses and guest namespace files remain the
filesystem owner's active scope. Preserve VMs 61610, 6233 and 85869; run one
build/harness at a time. No physical media, reboot or Actions. Current acceptance
and exact evidence are recorded in `docs/disk-installation.md`.

Clean `bf6efeb2` passes the matched build, four boots across two version-2 planned
EFI installations, 12 installation refusals and 12 invalid session selections.
Profile UUID/text/binary/preferences persist from A to B; disk GUID, home and
local bindings differ; both machine settings remain exact. Allocation/protected
region and source-preservation checks pass. Host checks include 27 installation
tests, profile/session checks, 13 bundle groups and seven adapter tests. Retained
evidence/artifacts are verified at
`.local/installation-evidence/profile-plans-l1_8_9zl`. The earlier v1 trial is
compressed and retained separately; v2 prevents older apply tools ignoring the
profile. PR319's optional filesystem fixture landed independently during proof.
Task VMs are stopped; the three original review VMs remain live. Delivery is a
scoped draft PR; no merge is requested by this milestone.

## PR315 merge review

Current-main `56e1f950` includes filesystem PR313 and profile-desktop PR314.
Clean reviewed candidate `57fcf482` rebuilt and passed all 11 canonical installed
VM cases after replacing the early Text startup-log wait with window-publication
readiness. Exact save/reopen and no-write recovery requirements remain enforced.
Focused installation, namespace and profile host checks pass. Evidence/artifacts
are verified outside build at `.local/installation-evidence/pr315-5wv8f9vm`.
All task VMs are stopped; original review PIDs 61610, 6233 and 85869 are preserved.
The user authorized merge if ready; see PR315 for the final state and the
installation document for the next profile-aware installation-plan scope.

## Canonical-path installation — proof complete

User authorized a scoped installed-boot PR with reordered-disk persistence and
recovery evidence. Integration branch `device/apple-silicon/installed-canonical-paths` in
`SrvrOS-installation` combines merged main `8f47f196` with exact filesystem PR313
`06d5a416` by local merge, retaining PR252 and SrvrPorts `0f51ba9`. The dependency
PR remains separately under review. Installation owns the fixture/harness and
`docs/disk-installation.md`; no competing namespace or profile implementation.
Latest profile contracts: PR309 `fa9717ad`, desktop PR314 `814aee20`; preserve
the existing home binding without activating that independent work.

Preserve primary QEMU PIDs 61610, 6233 and 85869. Only disposable task VMs run
here, one build/harness at a time; no physical media, reboot or Actions.

Clean `3e479119` with filesystem dependency `06d5a416` and merged profile
foundation main `9d642c75` passes the matched build and all 11 canonical installed
VM cases. Target and independent `/work` swap AHCI positions; canonical/raw
identity and persistence pass, Text reopens through `/home`, and nine recovery
cases retain both disk hashes before any alias/init startup. Protected regions
and both filesystems' allocation checks pass. Focused host/process checks and
133 filesystem/namespace sanitizer cases pass. The earlier `b8da407c` run was
superseded when dependency review fixes arrived, not accepted as final evidence.
Retained exact artifacts and byte-verified evidence:
`.local/installation-evidence/canonical-e5t4y5zz`. Task VMs are stopped; original
review VMs remain live. Delivery is a scoped draft PR, with dependency integration
and any later merge separate from this user's proof/publication request.
The delivery branch `device/apple-silicon/canonical-install-proof` contains only
the installation harness/evidence changes and remains dependent on open PR313;
the exact combined source stays available on the published integration branch.

## Installation identity milestone — November 3, 2026

The isolated `SrvrOS-installation` clone, branch
`device/apple-silicon/installed-root-identity`, builds on merged PR311
`3db1a1d3`. Scope: opt-in GPT identity/format/role selection, halted no-write
recovery, image manifests and a disposable two-disk proof. Shared block/exFAT
admission changes require filesystem-owner review; PR308 `88ea5524` owns
generic namespace attachment and PR309 `fa9717ad` owns portable profiles.
Neither profile activation nor physical installation is included. Main's boot
selection is independent of PR301's display hooks and PR252's lifetime work.

Primary review QEMU PIDs 61610, 6233 and 85869 remain running and untouched.
Only this clone's own short-lived test VMs may be stopped. One build/harness
runs at a time here. Acceptance and exact evidence are recorded in
[disk installation](../disk-installation.md); this is emulated x86_64 evidence,
not native ARM64 or physical boot acceptance.

Completed at clean source `9989878c`: matched full build, selector sanitizer,
21 image/provenance tests, real clean-mount zero-write checks, storage and exFAT
regressions, and all 11 two-disk VM cases. Same GUID selects `ahci0p2` then
`ahci1p2`, with Text/preferences restored and the decoy unchanged. Nine halted
recovery cases preserve both whole-disk hashes. Evidence and exact artifacts are
byte-verified under `.local/installation-evidence/proof-p0_hk8ic`. OVMF can repair
a bad primary GPT from a valid backup before kernel entry; the invalid-map case
damages both copies, and the earlier repair observation is retained separately.
All task-owned test VMs are stopped; the three original review VMs remain live.
The initial scoped PR was draft pending review. User subsequently authorized
review/merge of PR312: current PR308/PR313/PR309 contracts are compatible with the
bounded selector; retained evidence hashes and fresh focused host checks pass.
No implementation change or new VM run was needed. See the integration review
and next canonical-path installed-boot checkpoint in the installation document;
PR313's PR252/SrvrPorts dependencies remain separate integration work.

## Earlier host-tool milestone

Updated: 2026-10-25. This is a development host running an emulated x86_64
guest, not a native SrvrOS hardware target. Observed host model: MacBookPro18,2.

## Active scope

Branch `device/apple-silicon/macos-build-docs`, based on freshly fetched
`origin/main` at `eda8bfbc`. Move the working workspace macOS helpers into
`tools/macos/`, make installation and VM state paths portable, and audit the
current documentation against implementation and named acceptance records.
Touched areas: host tool bootstrap/checks, macOS VM tooling, current guides and
documentation audit notes. No kernel, driver, firmware or application behavior
changes are planned. The only open PR at scope inspection was Pro11,2 PR19,
which retains USB Wi-Fi ownership and does not overlap this host-tool work.

## Initial evidence

At source `46f6212f`, the complete ordinary image built with native ARM64 Zig
0.15.2, GNU Make 4.4.1, GNU tar and xorriso. QEMU 11.1.2 TCG ran one x86_64 CPU,
1 GiB RAM, Q35, AHCI, USB input, e1000 and HDA/CoreAudio at 1440×900. The desktop
ready marker appeared 6.61 seconds after launch. Rendering and injected typing
were observed; the user subsequently reported opening, running and testing it
successfully. That report is qualitative and does not establish a performance,
filesystem durability or audio-endurance bound. The automated Save As check was
not completed; it is not a save/readback pass.

This host has no native SrvrOS installation, physical USB acceptance or recovery
payload. No reboot is needed for this scope. Prior guest disks and evidence must
be retained during migration and excluded from Git; required VM state must
survive `make distclean`. Local logs identify their original source and paths.

## Completed host validation

The repo now owns the build/VM helpers and the [reproduction guide](../macos-development.md).
Previous workspace entry points forward to them. Existing VM disks moved to
`.local/macos-vm/` with before/after SHA-256 equality; historical manifests were
preserved. This state survives `distclean` and is ignored by Git.

Validation on baseline `eda8bfbc` with local host/docs changes:

- Fresh checksum-pinned ARM64 Zig bootstrap into an isolated test directory: pass.
- `sh tools/macos/build.sh check-host`: pass.
- `sh tools/macos/build.sh -j2 all`: pass; matched manifest written.
- `python3 tools/test_macos_host.py -v`: five tests pass, including provenance,
  disk preservation/failure, PID refusal and simulated-build refusal.
- `sh tools/macos/build.sh check-docs` and `git diff --check`: pass.
- Separate headless Q35/TCG VM with a long, spaced state path: console at 4.88 s,
  desktop/first frame at 5.13 s, QMP running, 1440×900 Text/desktop screenshot
  visually inspected. That disposable VM was stopped via its own QMP socket.

Local bootstrap/build logs, VM manifest and screenshot are under
`build/review/macos-host/` and `build/macos-build.log`. The session manifest
contains artifact hashes; runtime source and later documentation changes are
not conflated with another machine's native acceptance. The prior interactive
VM was already stopped before migration. No VM is intentionally left running
by this validation, and no native boot/install was performed.

## Audit checkpoint and next scope

The [first-pass audit](../code-docs-audit-2026-10-25.md) records current-guide
corrections and source references. Unresolved work includes condition-variable
clock attributes, Air6,1 native lifecycle latency, Node source-history recovery,
macOS test-suite portability and deeper subsystem review. The Linux host/core,
full desktop/VM suites, physical save durability, UEFI boot and audio endurance
were not accepted by this Mac build/boot check. Keep those gates separate.

## Continued audit scope — 2026-10-25 evening

PR175 publishes the first pass. The continued audit consumes `origin/main`
`aa7b7392` (PR173 adds three failed/incomplete Air6,1 timing attempts; no runtime
change). PR174 owns process startup/worker retirement under disk contention;
PR172 owns shared desktop application dispatch; PR19 retains Wi-Fi diagnostics.
This pass audits libc synchronization/stack contracts, creates bounded host
reproducers, and corrects current documentation. It does not implement changes
in those active runtime scopes or claim new native acceptance.

The continued pass confirms three defect categories in the
[synchronization audit](../synchronization-audit-2026-10-25.md): supplied-stack
bounds (two cases), selected condition clocks and semaphore count overflow.
`tools/audit_sync_contracts.py` compiles real library sources with prefixed
symbols and modeled OS services; no threads or VM are started. Default mode
exits 1 for the known defects; `--expect-known-defects` exits 0 for all four
observations. Separate JSON receipts under `build/review/sync-audit/default`
and `build/review/sync-audit/known` retain source/header hashes and output.
The exact-size stack and ordinary semaphore operations are positive controls.
This is source-level host evidence, not a full thread/concurrency suite. Current
guides exclude the affected contracts and distinguish libc from libuv semaphore
coverage. Documentation links and whitespace validation pass. Runtime corrections
remain follow-up work; no kernel/library implementation was changed.

## Synchronization corrections — active scope

The user authorized fixes for the three confirmed findings and continued audit.
Base is `a90ac149` with `origin/main` still at `aa7b7392`; inspected PR172/174/19
retain desktop dispatch, process admission and Wi-Fi ownership respectively.
This block owns libc supplied-stack bounds, condition clock retention/deadlines,
semaphore count bounds, focused host and guest regressions, and current-guide/PR
updates. Rebuild matched userspace after the condition-variable layout change.
No native hardware trial or expansion of those independent PR scopes is planned.

The three fixes are implemented. The positive host suite passes; all three
isolated reintroduced-defect controls fail as intended. Matched `all` rebuild,
targeted live guest synchronization cases, the complete thread suite and complete
libuv consumer suite pass. Logs and hashes are retained under
`build/review/sync-fixes/`; no physical boot occurred. `pthread_cond_t` is now
8 bytes and all consumers were rebuilt. Supplied stacks preserve exact bounds;
condition clocks/deadlines are honored; semaphore counts use an atomic checked
maximum. Process-sharing/timed semaphore support is not added.

Continued guest audit confirms a separate `pthread_getattr_np(self)` metadata
failure: a 64 KiB supplied stack is reported as a guessed 2 MiB region. The
explicit `stack-inspection-audit` mode returns a failure receipt and is excluded
from the passing `all` suite. See the synchronization report for exact output,
next correction scope and remaining publication/lifetime questions. No claim of
fixing that fourth finding accompanies the three completed corrections.

## Self-stack inspection correction — active scope

The user authorized the fourth confirmed fix, regressions, documentation/PR
updates and continued audit. This block consumes main `1eb60b8f` (merged
PR174/176 scheduler retirement work) into this branch. PR172/19 retain shared
desktop dispatch and Wi-Fi scope. Work is bounded to libc thread metadata,
focused host/guest inspection tests and current documentation. Worker metadata
must exist before the user callback, including when the child runs before
`pthread_create` returns. Initial-process stack reporting retains the kernel's
two fixed layouts; unknown raw-thread stacks must not receive guessed bounds.
Next checkpoint is a matched build, sequential guest thread/libuv checks and a
bounded audit of adjacent thread lifecycle contracts. No physical boot is armed.

The self-stack correction is implemented with private TLS start metadata and
atomic detach-state reads/updates. Host early-child regressions pass; removing
metadata installation in an isolated negative control produces exit 1. Matched
`all` build, targeted inspection, final complete thread suite and full libuv
consumer suite pass on this ARM64 host's x86_64 QEMU. The merged timing parser's
three Python tests also pass. Documentation validation checks 634 local links
with zero errors; whitespace checks pass. The complete Linux/core suite was not
run, and no native acceptance is added. Evidence, exact build manifest and source/
log hashes are under `build/review/stack-inspection/`. Disposable guests exited.

Continued audit's deterministic host model finds early self-detach returning
`EINVAL` before the parent publishes its record. That separate explicit audit
exits 1 and remains outside the passing regression gate; no live race frequency
is inferred. Next scope is record publication/lifecycle ownership, including
reaping. Current status also reconciles PR176's failed Air6,1 native trial; worker
pipe admission is still only a candidate explanation. This block is complete;
no VM or physical boot is left running/armed.

## Early self-detach correction — active scope

The user authorized the publication-order fix, regressions, docs/PR updates and
continued audit. Base is `d9b1e710`, consuming main `aa931093` (merged PR172 shared
Text/Preview runtime). PR19 retains Wi-Fi diagnostics; no overlapping libc edits
were found. This block owns libc pthread record publication and cleanup ownership,
focused host/guest lifecycle tests and documentation. Install the record before
callbacks, retain creator ownership until create returns, and serialize join/reap
claims without holding the table lock across kernel waits or memory teardown.
Next checkpoint is deterministic early-child/parent-resume coverage, matched
build and sequential thread/libuv guest suites. No native trial is armed.

The early-detach fix is complete. Child publication precedes callbacks; creator
reservations prevent early cleanup, and exclusive join/reap claims serialize
teardown without blocking under the table lock. Self-join returns `EDEADLK`.
Failed queries/joins retain ownership for retry. No public ABI change is added.

Matched `all`, positive host regressions (40 forced early detached lifetimes plus
ordinary join failure/interruption retry), ASan/UBSan, targeted guest self-detach
(96 lifetimes/three concurrent creators), full thread suite and full libuv suite
pass. The newly merged desktop-runtime host target passes on this Mac as well.
Four isolated lifecycle negative controls each fail with exit 1. Documentation
checks 639 local links with zero errors and whitespace checks pass. Host native
pthread execution forces ordering; it does not replace guest kernel validation.
Raw logs, manifests and source hashes remain under `build/review/self-detach/`.
All disposable VMs stopped; no native boot was performed or armed.

Continued audit confirms nonportable thread-name APIs are stubs: setting
`audit-worker` succeeds but reading returns `srvros-thread`. A separate explicit
host audit exits 1; current guides exclude real naming. Source review also
corrected the fork limit to include exited-but-unreaped workers. The next
bounded work can address naming storage/error semantics; abnormal once-initializer
exit remains an unconfirmed audit area. Detached cleanup is still lazy and raw
kernel lifecycle operations must not be mixed with libc-owned workers.

## Thread-name correction — active scope

The user authorized name storage/error semantics, regression tests, docs/PR
updates and continued audit. Base is `b095d9b4`, consuming main `fa5a6cc1`'s
Air7,2 shared-runtime acceptance documentation; runtime remains `aa931093`.
PR19 retains Wi-Fi scope. This block owns libc pthread names, focused host/guest
regressions and current docs. Use protected record storage plus an initial-thread
name, bounded 15-byte payloads, explicit range errors and lifecycle-aware lookup.
This does not add kernel scheduler/UI naming. Next checkpoint is deterministic
pre-parent worker naming, concurrent guest round trips, matched build and the
thread/libuv suites. No physical boot is armed.

The thread-name fix is complete: independent protected initial-thread/worker
storage, 15-byte maximum plus NUL, explicit range errors without partial writes,
errno preservation and cleanup-aware lookup. Default names reset with each record;
exited-but-unreaped workers retain their names. Kernel task labels are unchanged.

Matched build, default host regressions, ASan/UBSan and targeted name VM tests
pass. Three isolated negative controls each fail with exit 1. Host tests force
naming before parent completion and across 40 worker lifetimes; the guest covers
24 alternating joinable/detached lifetimes and 600 concurrent naming operations.
The final complete thread and libuv suites pass sequentially. Documentation
checks 641 local links with zero errors; whitespace checks pass. Exact manifests,
source/log hashes and receipts are under `build/review/thread-names/`. No full
Linux/core or physical acceptance is claimed; disposable VMs have stopped.

Continued audit confirms initial-thread `pthread_exit` kills a live worker:
`main-exit-audit` logs the initial exit, then process status 0 without the worker
survival/completion markers; harness exit 3. This separate failed audit is
excluded from `all`. The next correction needs kernel initial-thread/process
lifetime and last-thread retirement ownership. Once-initializer abnormal exit
remains unconfirmed. Current guides also consume Air7,2's new shared-runtime
acceptance without extending its scope to native playback or all UI edge cases.
No VM or physical boot is left running/armed.

## Initial-thread exit correction — active scope

The user authorized the initial-thread exit fix, regressions, docs/PR updates
and continued audit. Base is `2ba7edce`; fetched main remains `fa5a6cc1`.
PR19's current Wi-Fi branch is `58f68fe4`, with no overlapping pthread scope.
This block owns kernel initial-thread retirement, the libc last-thread cleanup
handoff, focused host/guest tests and current documentation. Retain the initial
kernel continuation and process resources while workers live; preserve explicit
process exit/fatal signals and run normal libc cleanup after the last thread.
Next checkpoint is matched-image subprocess regression coverage and sequential
full thread/libuv suites. No native boot is armed.


The initial-thread exit correction is complete. The retained kernel continuation
waits for active workers, then returns only to libc's `exit(0)` cleanup path.
Initial-thread pointer values do not become process status; explicit process
exit and fatal signals retain their behavior. Dead initial-thread signal targets
cannot terminate survivors. This syscall handoff requires matched kernel/libc.

Matched build, host synchronization regressions, ASan/UBSan, targeted 30-lifetime
initial-exit guest coverage, final full thread suite and final full libuv suite
pass. Kernel premature-teardown and libc cleanup-skipping negative controls
fail with exit 3 and 1 respectively. The corrected kernel was restored and
rebuilt before final suites. Documentation checks 641 local links with zero
errors; whitespace checks pass. Logs, source hashes and manifests are retained
in `build/review/initial-exit/`. No full Linux/core or native acceptance is claimed.

Continued guest audit confirms that joining the initial thread returns `EINVAL`
(22), with no return value, then the audit exits status 1 (harness exit 3).
`main-join-audit` is excluded from `all`; initial-thread join/detach remains
explicitly unsupported. The next bounded work is initial-thread joinability and
return-value/cleanup ownership. Once-initializer abnormal exit remains
unconfirmed. All disposable VMs have stopped; no native boot is armed.

## Initial-thread join correction — active scope

The user authorized initial-thread join, regression tests, docs/PR updates and
continued audit. Base is `b75fc79a`; fetched main remains `fa5a6cc1`, and PR19
remains at `58f68fe4` with Wi-Fi scope. This block owns initial-thread lifecycle
records in libc/kernel, retained return values, join/detach claims, wakeups and
focused tests. Preserve the initial kernel continuation and last-thread cleanup
even after its public pthread handle is collected. Next checkpoint is waiting
and late joins, detach/error ownership and matched full thread/libuv VM suites.
No physical trial is armed.


The initial-thread join correction is complete. A separate static libc record
uses exclusive claims and lazy detached collection. The kernel retains the
pointer return value and wakes waiting joiners; collecting the public handle
preserves the process-owned stack/TLS and last-thread cleanup continuation.
Self-inspection reports detach state, and names/signals reject collected handles.

Matched build, final host regressions and ASan/UBSan, targeted 24-lifetime
initial-join guest coverage, complete final thread suite and complete final
libuv suite pass. The libc initial-join refusal and kernel discarded-value
negative controls fail with exit 1 and 3; the fixed kernel was restored before
final suites. Documentation checks 641 local links with zero errors; whitespace
checks pass. Evidence is retained in `build/review/initial-join/`. No full
Linux/core, general SMP or native acceptance is claimed.

Continued source/host audit confirms that a valid negative absolute condition
deadline returns `EINVAL` (22) rather than `ETIMEDOUT` (110). The explicit
`--audit-negative-deadline` mode fails with exit 1 and is excluded from default
positive tests. Current guides state this edge case; the next bounded work is
absolute-deadline validation and timeout-path regressions. Once-initializer
abnormal exit remains unconfirmed. Disposable VMs stopped; no physical boot is
armed.

## Negative absolute condition deadline correction — active scope

The user authorized deadline validation, regression tests, docs/PR updates and
continued audit. Base is `cc955159`; fetched main remains `fa5a6cc1`. PR19 has
advanced to `b68df8cd` with bounded Wi-Fi diagnostics, without overlapping libc
scope. This block owns absolute condition-deadline validation and focused
host/guest tests. Preserve signed seconds, reject invalid nanoseconds, and route
expired deadlines through mutex release/reacquisition. Next checkpoint is
boundary/control cases, matched build and sequential thread/libuv VM suites.
No physical boot is armed.

The negative absolute condition deadline correction is complete. Signed seconds
reach the selected-clock comparison and normal mutex release/reacquisition path;
invalid nanoseconds remain rejected before unlocking. No layout/syscall change
is introduced.

Matched build, host regressions, ASan/UBSan, targeted guest coverage (15 expired
and six invalid deadlines), final full thread suite and final full libuv suite
pass. Two isolated host negative controls fail with exit 1: restored negative
seconds rejection and a timeout shortcut that skips the mutex transition.
Documentation checks 641 local links with zero errors; whitespace checks pass.
Evidence is retained in `build/review/absolute-deadlines/`. No full Linux/core,
general SMP or native acceptance is claimed.

Continued guest audit confirms that `clock_getres(clock, NULL)` incorrectly
returns `EINVAL` for realtime/monotonic clocks; CPU-clock probes and non-null
outputs succeed. The explicit `clock-resolution-audit` fails with guest status 1
and harness exit 3 and is excluded from `all`. Current guides document a real
output buffer as the workaround. Optional-output validation is the next bounded
correction. Once-initializer abnormal exit remains unconfirmed. All disposable
VMs have stopped; no physical boot is armed.

## Clock resolution optional output correction — active scope

The user authorized null-output clock resolution, regression tests, docs/PR
updates and continued audit. Base is `86498be3`; fetched main remains `fa5a6cc1`
and PR19 remains `b68df8cd` with separate Wi-Fi scope. This block owns libc
`clock_getres` validation and focused host/guest timing tests. Preserve unknown
clock errors, realtime/CPU source failures and existing resolution values. The
next checkpoint is modeled source/fallback tests, matched build and sequential
thread/libuv VM suites. No physical boot is armed.

The optional-output correction is complete. Realtime/monotonic resolution
queries accept null output while retaining unknown-clock and source errors;
CPU queries and resolution values are unchanged. No layout/syscall change.

Matched build, host timing/synchronization targets, final timing ASan/UBSan,
targeted clock-resolution guest group, complete thread suite and full libuv
suite pass. Three source-copy negative controls fail with exit 1 (original
rejection, unconditional null success, skipped realtime validation). Docs check
641 local links with zero errors; whitespace checks pass. Evidence, source
hashes and manifests are retained in `build/review/clock-resolution/`. No full
Linux/core, general SMP or native acceptance is claimed.

Continued source/host audit confirms a `times()` fallback defect: with the
high-resolution reference unavailable, 50 advancing modeled ticks produce a
zero elapsed return delta. Reference-enabled `times()` and fallback monotonic
`clock_gettime` controls pass. The explicit `--audit-times-fallback` fails with
exit 1 and is excluded from default positive checks. This configuration has
not been reproduced in the default VM or on native hardware. Current guides
state the monotonic-clock workaround. The next bounded correction is elapsed
source selection/fallback. Once-initializer abnormal exit remains unconfirmed.
Disposable VMs stopped; no physical boot is armed.

## times elapsed fallback correction — active scope

The user authorized `times()` fallback, regression tests, docs/PR updates and
continued audit. Base is `cb8985e6`; fetched main remains `fa5a6cc1` and PR19
remains `b68df8cd` with separate Wi-Fi scope. This block owns libc elapsed source
selection and focused host/guest timing tests. Preserve CPU accounting units,
usage errors and output ownership. Next checkpoint is reference/fallback and
boundary controls, matched build and sequential thread/libuv VM suites. No
physical boot is armed.

The `times()` elapsed fallback correction is complete. Valid reference readings
convert to 100 Hz ticks; unavailable reference time selects kernel ticks
directly. CPU accounting fields, optional output, error propagation and failure
output ownership are preserved. No layout/syscall or kernel change.

Matched build, host timing/synchronization targets, timing ASan/UBSan, targeted
`times` guest group, complete thread suite and full libuv suite pass. Three
source-copy negative controls fail with exit 1 (frozen return, wrong fallback
scaling, wrong CPU units). Docs check 641 local links with zero errors;
whitespace checks pass. Source/log hashes, manifests and receipts are retained
in `build/review/times-fallback/`. Guest targeted/full runs report the reference
source; absent-reference tests remain host-modeled. No full Linux/core, general
SMP, native or fallback-VM acceptance is claimed.

The bounded adjacent timing/accounting and timestamp-conversion review adds no
confirmed finding. Next audit scope is deterministic sleep interruption and
remainder behavior with unavailable clocks and boundary values. Abnormal
once-initializer exit remains unconfirmed; broader profile exclusions remain.
All disposable VMs stopped; no physical boot is armed.

## Sleep interruption and remainder audit — active scope

The user requested an audit, regression tests for confirmed issues, docs/PR
updates and continued review. Base is `820c1c3e`; fetched main remains `fa5a6cc1`
and PR19 remains `b68df8cd` with separate Wi-Fi scope. This block owns host/guest
sleep probes and documentation of observed contracts. Trace signal dispatch,
aliasing, remaining-time arithmetic, source fallback and boundary errors through
libc/kernel. Keep confirmed failing probes separate from positive default gates.
Next checkpoint is deterministic host cases and a matched guest reproduction for
any practical failure. No physical boot is armed.

The sleep audit confirms two pending remainder defects. P2: handler execution
is deducted from relative unslept time (24/24 modeled cases and six guest cases).
Guest signals arrived 25.553–32.760 ms into 500 ms requests; 600 ms handlers
left zero remainder. P3: extreme fallback tick scaling wraps (six modeled cases,
roughly 584,000 years; no VM/native occurrence). Libc/kernel runtime is unchanged.

Matched build, final host timing/synchronization tests, final timing ASan/UBSan
and complete positive thread suite pass, including six fast-handler remainder
controls. Explicit host handler/overflow audits fail with exit 1; the guest
handler audit returns status 1/harness exit 3. These are failure receipts and
are excluded from positive defaults. Docs check 641 local links with zero
errors; whitespace checks pass. Evidence is retained in `build/review/sleep-audit/`.
Prior libuv validation remains historical; no new full Linux/core, general SMP,
native or absent-reference VM acceptance is claimed.

Continued kernel wait/source and boundary review adds no third confirmed issue.
Next corrections are capturing suspension end before handlers, then splitting
fallback ticks before scaling. Once-initializer abnormal exit remains unconfirmed.
All disposable VMs stopped; no physical boot is armed.

## Sleep remainder corrections — active scope

The user authorized the signal-handler remainder fix and the P3 arithmetic fix
if bounded, regression tests, docs/PR updates and continued audit. Base is
`bd300e28`; fetched main remains `fa5a6cc1` and PR19 remains `b68df8cd` with
separate Wi-Fi scope. This block owns libc suspension-end capture and remainder
conversion plus focused timing tests. Preserve aliasing, pre-wait interruption,
error/errno and absolute-output semantics. The P3 fix is a small quotient/
remainder conversion in the same path. Next checkpoint is positive former
reproducers, boundary/reentrant controls and matched sequential guest suites.
No physical boot is armed.


Both sleep remainder corrections are complete. `nanosleep` captures unslept time
before post-wait handlers and retains the full request on pre-wait interruption;
a local snapshot survives nested sleeps and handler buffer changes. Tick fallback
splits seconds/fractions before scaling. Alias/error/absolute-output behavior
is preserved, with no layout/syscall or kernel change.


Validation of both corrections on ARM64 macOS/x86_64 QEMU TCG: matched build,
host timing/synchronization checks, timing ASan/UBSan, all 18 targeted sleep
remainder cases, the complete thread suite on retry and full libuv suite pass.
The targeted long/nested handlers retained 461818000–472246000 ns from interrupted
500 ms requests. Three isolated source-copy negative controls each exit 1:
restored old runtime, overflowing conversion and pre-wait handler subtraction.

The first full thread run failed before the sleep cases: an AHCI read command
reported the disk offline and the rename test returned EIO. A fresh disposable
VM using the same build passed the full suite. Both logs are retained; the
storage failure's cause is unresolved and a retry does not erase that failure.
Docs check 641 local links with zero errors; whitespace checks pass. Source/log
hashes, build manifest and test receipts are in `build/review/sleep-fixes/`.
No full Linux/core, general SMP, native or absent-reference VM acceptance is
claimed. All disposable VMs stopped; no physical boot is armed.

Continued bounded review covered wrapper rounding/errors, nested dispatch,
source/output ownership and initial futex wait/wake paths. No additional defect
was confirmed. Next scope is deterministic futex interruption/wake race coverage;
abnormal once-initializer exit remains unconfirmed. The observed AHCI failure
also remains an unresolved validation event, with both failure and retry receipts.

## Deterministic futex interruption and wake audit — active scope

User authorized deterministic race coverage, confirmed fixes, docs/PR updates and
continued review. Base `5503804e`; fetched main remains `fa5a6cc1`, and PR19's
`b68df8cd` retains separate Wi-Fi scope. Own the process futex wait/wake lifetime
and focused kernel/libc/guest regressions; inspect scheduler membership without
assuming general SMP acceptance. Next checkpoint is deterministic interruption,
pre-enqueue/re-wait queue reuse and wake ownership probes. No physical boot armed.


The bounded futex correction is complete. The deterministic fixture confirms
entry reuse could change the process/address binding while a syscall owned its
queue. Counted reservations now cover pre-enqueue and post-wake gaps, all return
paths and process-exit propagation. Final cleanup follows forced sibling
retirement. Wake lookup/count is IRQ-serialized as defensive hardening; the
current syscall gate already masks IRQs. No public layout/syscall change.

Full validation also confirmed a harness defect: child exit messages prematurely
ended the enclosing suite. The reader now waits for test completion/failure or
the monitor's top-level run receipt; five host regressions protect that behavior.


Final validation: matched build, host futex/synchronization/timing targets, five
smoke-reader tests, futex ASan/UBSan, targeted futex and main-join VM groups,
complete thread suite with the corrected harness and full libuv suite pass.
Three kernel source-copy controls abort at the intended assertions (old runtime,
restored wake gap and Boolean reservation); the old smoke reader fails its host
regressions. Docs check 641 local inline links with zero errors; whitespace
checks pass. Source/log hashes and build/test receipts are retained in
`build/review/futex-audit/`.

Retained unsuccessful runs: two full captures were truncated by the old harness;
a subsequent corrected-harness run encountered an AHCI read failure/disk offline
during fork, before the futex group. The fresh-VM retry passed. The storage
failure remains unresolved, consistent with the preceding sleep-fix checkpoint;
a retry is not evidence that it is fixed. Earlier test-development build and
signal-poll expectation errors are retained locally with the final passing runs.
No full Linux/core, general SMP or physical-hardware acceptance is claimed.
All disposable VMs stopped; no physical boot is armed.

Continued review traced syscall entry, condition recomputation, scheduler queue
removal and process-exit ownership without another confirmed runtime finding.
Next scope is raw futex timeout overflow and mapped-address lifetime. Abnormal
once-initializer exit and broader exclusions remain open; the AHCI fault is still
an unresolved validation event.

## Futex deadline and mapped-address audit — active scope

User authorized overflow/lifetime checks, confirmed fixes, tests and docs/PR
updates. Base `cca88cc0`; fetched main remains `fa5a6cc1`. PR19 advanced to
`44f41e72` with separate channel1 Wi-Fi candidate scope. Own futex deadline
arithmetic and safe word access/invalidation across mapping changes, plus focused
host/guest tests. Next checkpoint is boundary and removal/reprotection/remap
reproductions. Keep the current CPU0 IRQ model explicit; no physical boot armed.


Completed scope: reject overflowing relative futex deadlines before reservation;
validate/load words with CPU0 IRQ exclusion; invalidate and wake old bindings on
page removal or successful user-access revocation. Old calls fault even after
address replacement/restoration; readable or failed protection changes preserve
waits. No public layout/syscall-number change. Host regressions cover boundary
arithmetic and exact mutation interleavings; guest tests cover 20 mapping lifetimes.


Final validation for the deadline/lifetime correction: matched kernel/userspace
build; host futex, synchronization and timing contracts; five smoke-reader tests;
futex ASan/UBSan; targeted futex-lifetime guest; complete thread suite; and full
libuv consumer suite all pass. The full thread suite passed on its first run,
without recurrence of the historical AHCI failure. Both pre-fix probes fail and
all five source-copy controls compile then abort at their intended assertions.
Docs check 641 local inline links with zero errors; whitespace checks pass.
Build/test receipts and source/log hashes remain in `build/review/futex-lifetime/`.
No full Linux/core, general SMP or physical-hardware acceptance is claimed.
All disposable guests stopped; no physical boot is armed.

Continued review covered page removal/protection callers, mapping refresh and
rollback, MAP_FIXED refusal of live-region replacement, libc storage assumptions
and exec's refusal while worker records exist. No further confirmed defect.
Next checkpoint: futex table saturation/fairness and once-initializer exit.
The prior AHCI disk-offline event remains unresolved.


## Synchronization audit closeout — active scope

User authorizes completion of the documented synchronization audit, confirmed
fixes/regressions, docs/PR updates and merge of PR175 after validation. Base
`f5f6328f`; fetched main remains `fa5a6cc1`, Pro PR19 remains `44f41e72` with
separate Wi-Fi scope. Own futex saturation/fairness verification, once-initializer
exit recovery and a final pass over supported pthread/timing contracts and the
PR diff. Broader filesystem/network/native investigations remain separate from
this bounded audit. Next checkpoint: failing once-exit probe, saturation and
scheduler wake-policy evidence. No physical boot armed.


Completed closeout: fixed once-initializer exit with a private TLS frame chain,
reset/wake before destructors and nested/initial-thread coverage. Final review
also confirmed and fixed recursive mutex depth overflow and false semaphore
try-wait failure after a concurrent post. Capacity tests preserve existing futex
bindings across process pressure; scheduler tests characterize slot-order wakes
without promising FIFO or starvation freedom. No new public layout/syscall ABI.


Final closeout validation: matched build, host synchronization/futex/timing
contracts, five smoke-reader tests, synchronization/futex ASan+UBSan, x86_64
scheduler fixture under Rosetta, targeted once-exit guest, complete thread suite
and full libuv suite all pass. Five source-copy negative controls compile and
fail their intended assertions. Three pre-fix probes reproduce the new findings.
The complete thread suite passed first time; the historical AHCI disk-offline
failure did not recur and remains unresolved. Docs check 645 local inline links
with zero errors; whitespace checks pass. Source/log hashes, matched image hashes
and receipts remain under `build/review/sync-closeout/`.

Retained unsuccessful attempts: the first once guest used an incorrect subprocess
path (fixed to `/fat/bin/threadstress`); native ARM64 scheduler compilation rejected
existing x86 instructions, and an initial x86 command omitted the Limine include
path. The explicit x86_64/Rosetta fixture passes. None is recorded as a guest
runtime regression or full Mac test-core acceptance. All disposable VMs stopped.
No full Linux/core, general SMP or physical-hardware acceptance is claimed.

The bounded build/docs and synchronization/stack/timing audit is complete, with
nineteen corrected runtime findings and the smoke-reader correction. Broader
filesystem/network/native issues, Node recovery and Mac test-core portability
remain separate tracked work, as specified in the audit closeout matrix. PR175
is ready for the user-authorized merge after publishing this validated commit.
No physical boot or installation is armed; Actions remain disabled by policy.


## Historical AHCI disk-offline investigation — active scope

User authorizes reproductions, confirmed fixes, regression coverage, docs and a
new draft PR. Branch `device/apple-silicon/ahci-offline` starts at merged main
`f0cd6409`; PR19 at `44f41e72` retains separate Wi-Fi ownership. Review the two
retained READ DMA EXT failures during full thread runs, AHCI polling/deadlines,
service handoff and completion/error ownership. Use disposable x86_64 QEMU guests
and host driver models; do not change native gates or persistent VM disks.
Next checkpoint: classify the historical evidence and obtain a directed failing
reproduction before changing runtime behavior. No physical boot armed.


Two diagnostic-only full-thread runs passed without spontaneous offline. The
host driver model confirmed a stale completion/readiness decision at bounds and
carryover of the pre-submission service budget. The candidate rechecks phase
state once at bounds, preserves the final TFES check and resets the completion
phase's budget. Genuine failures keep the existing quarantine/pinned DMA policy;
limits and native gates are unchanged. New diagnostics record phase, reason,
sector, last elapsed sample, counters and port registers.

AHCI and block/cache host tests and AHCI ASan/UBSan pass under x86_64 Rosetta.
Four source-copy controls compile and fail the intended assertions. The
storage-service VM passes throttled I/O, USB input, cancellation, copy comparison,
fsck, three executable relaunch cycles and forced timeout/quarantine. Its genuine
pending write times out at 2,000,000 us with CI=1, IS=0, TFD=0x50 and SERR=0;
one failure is counted and later storage is refused. Historical READ DMA EXT
failures remain unattributed; these reproductions do not establish their cause.
Evidence is retained in `build/review/ahci-offline/`; no physical trial is armed.


Final candidate full-thread suite passes on its first run, with no spontaneous
AHCI failure. Documentation checks 648 local inline links with zero errors;
whitespace checks pass. Matched build, AHCI/block host checks, sanitizer and
storage-service receipts are saved with source/image/log hashes. The initial
diagnostic build rejected duplicate unused local declarations introduced while
adding diagnostics; those were removed before VM tests. That failed attempt is
retained alongside the passing build. All disposable VMs are stopped.


## AHCI repeat campaign — active scope (2026-10-26)

User authorizes repeated diagnostic VM tests and confirmed fixes on draft PR178.
Source is `8f7a9434`, based on `origin/main` `f0cd6409`; fetched remote refs and
open PRs show PR19 retains separate Wi-Fi work. Run six full-thread guests, two
complete storage-service guests (including deliberate timeout injection) and one
ten-cycle fsck/relaunch/comparison guest, sequentially on disposable disks.
Retain per-run logs and source/image hashes under `build/review/ahci-repeat/`.
Stop to classify any failure before changing code. If all runs stay clean, shelve
the historical incident as non-reproduced, preserve confirmed fixes and evidence,
and update investigation/status docs and draft PR178. No native trial is armed.

Repeat campaign complete: all nine fresh guests pass (six full-thread, two full
storage-service, one ten-cycle relaunch). No spontaneous offline event occurred.
Both deliberate write timeouts have completion/deadline receipts, pending CI=1,
IS=0, TFD=0x50, SERR=0 and exactly one failure each; subsequent sync is refused
and the shell remains responsive. All guests stopped and template hashes remain
unchanged. No new runtime finding or additional regression case was confirmed.

Active historical investigation is shelved as non-reproduced at the user's
direction, with cause still unknown. Preserve the existing corrections/tests,
original failed runs and the new per-run evidence under `build/review/ahci-repeat/`.
Update draft PR178 with this documentation-only checkpoint; no merge requested.
Next step is other project work unless new diagnostics justify reopening AHCI.
No physical trial is armed; Actions remain disabled.


## Eventfd/epoll audit — active scope (2026-10-26)

PR178 merged as `8bd43adc`; the clean checkout fast-forwarded to that baseline.
Branch `device/apple-silicon/eventfd-epoll-audit` takes the deferred experimental
descriptor-adapter review. PR19 retains independent Wi-Fi ownership. Scope is
`posix_node_compat.c`, libc descriptor close integration, focused host probes and
current-profile documentation: first establish lifecycle, wait and counter-bound
behavior with deterministic executable evidence. This is an audit checkpoint,
not acceptance of the full Linux API or Node port. Preserve native and Node
recovery work as separate follow-ups. No physical trial or Actions run is armed.

Audit checkpoint: three unfixed finding groups are confirmed by seven directed
host probes (all exit 1 at the intended assertion): epoll lifetime/reuse, immediate
wait timeouts and eventfd counter bounds. Real adapter code and public close are
compiled with modeled native services. Positive controls and ASan/UBSan pass;
no runtime changed and no new guest acceptance is claimed. Current profile/status
and the dedicated audit report now state these concrete limitations. Next bounded
correction is epoll ownership across close, duplication and descriptor reuse.
Evidence is in `build/review/event-adapters{,-audit,-sanitized}/`. No VM is running
from this work, no physical boot is armed, and Actions remain disabled.

Validation: `sh tools/macos/build.sh test-event-adapters` passes; sanitized
controls pass without diagnostics; explicit `--audit all` exits 1 with seven
intended failed contract assertions. Documentation checks 653 local inline links
with zero errors, and whitespace checks pass. Publish this audit-only checkpoint
in a new draft PR; it does not contain runtime corrections.


## Epoll lifetime correction — active scope (2026-10-26)

User authorizes close/duplication/reuse fixes, regression tests, docs and draft
PR179 update, then continued audit. Baseline is `526b6353` on merged main
`8bd43adc`; fetched PR179 matches and PR19 retains separate Wi-Fi ownership.
Scope: libc event adapter and close/dup/F_DUPFD integration, identity/alias
tracking and wait snapshots, host and matched-guest regressions. No new public
syscall ABI is planned. Preserve the separate unfixed wait/counter findings.
Next checkpoint: positive lifetime tests including duplicate-before-registration,
replacement, failure rollback and concurrent wait/control interleavings.

Lifetime correction implemented: protected libc identities cover native dup,
dup2, dup3 and F_DUPFD*, final-alias retirement, close-error cleanup and number
reuse. Wait snapshots validate instance/watch/descriptor identity after polling.
The original three lifetime probes and four broader lifetime modes pass with
ordinary controls and ASan/UBSan. Targeted and full-thread guests pass, including
concurrent lifetimes and post-cleanup fork. Four source-copy negative controls
compile and fail the intended assertions. Continued audit adds a fifth remaining
wait/counter failure: an eventfd post during the modeled sleep is missed.
No kernel/syscall ABI or native gate change; raw descriptor mutations, rights
transfer and general Linux epoll semantics are outside this bounded acceptance.


Final revision validation: matched build, eight positive host modes and their
ASan/UBSan runs, complete thread guest (including epoll lifetime), libuv guest
and five smoke-reader tests all pass. Four negative controls compile and fail
the intended assertions. The five explicit remaining wait/counter probes fail
as documented. Documentation checks 653 local inline links with zero errors;
whitespace checks pass. Source/build/log hashes and all directed receipts are
retained under `build/review/epoll-lifetime/`. All disposable guests stopped.
No physical trial is armed and Actions remain disabled. Draft PR179 now carries
the lifetime correction; the next bounded item is wait/notification handling.

## Epoll wait correction — active scope (2026-10-26)

User authorized proceeding with the wait/notification findings. Baseline is
`b51143cb`; fetched main remains `8bd43adc`, PR179 is draft at this branch head,
and PR19 retains separate Wi-Fi ownership. This block owns the libc epoll wait
loop, monotonic timeout accounting, rechecking synthetic readiness and control
changes, focused host/guest regressions and current docs/PR updates. Preserve
identity/revision validation and keep signal delivery outside the lifecycle lock.
Use bounded native poll intervals for synthetic state, since it has no kernel
notification source; document this latency/efficiency limitation. Counter bounds,
full Linux epoll semantics and native acceptance remain separate. Next checkpoint
is deterministic wait/deadline/interruption coverage, then a matched build and
sequential guest thread/libuv checks. No physical trial or Actions run is armed.


Final wait validation: matched `all` build, sixteen positive host modes and
ASan/UBSan, targeted epoll-wait guest, complete thread guest, full libuv consumer
guest and five smoke-reader tests pass. Five compiled negative controls restore
zero-timeout sleeping, ready-event delay, premature infinite-wait return, missing
deadline clamping or stale counter readiness; each fails its intended assertion.
Three explicit counter probes fail as documented, including the new full-counter
writable case. Local manifests, source/log hashes and receipts are retained under
`build/review/epoll-wait/`. All disposable VMs stopped. No kernel/syscall ABI,
physical acceptance or Actions setting changed. PR179 remains draft; next bounded
correction is eventfd counter bounds and matching writable readiness.

## Eventfd counter correction — active scope (2026-10-26)

User authorized proceeding with counter bounds and writable readiness. Base is
`1f83d29f`; fetched main remains `8bd43adc`, PR179 is draft at this branch head,
and PR19 retains separate Wi-Fi ownership. Scope: checked eventfd addition,
reserved-value rejection and matching EPOLLOUT capacity, host boundary/error
regressions, concurrent public-API guest coverage and current docs/PR updates.
Keep the existing lifecycle mutex and wait behavior. Blocking I/O, semaphore
mode and flag semantics remain separate audit areas; no new kernel ABI or native
acceptance is planned. Next checkpoint is preservation/readiness boundary tests,
then matched build and sequential thread/libuv VMs. Actions remain disabled.


Final counter validation: matched `all` build, twenty-one positive host modes
and ASan/UBSan, targeted eventfd-counter VM, complete thread suite and full libuv
consumer VM pass. Four compiled negative controls break reserved-value rejection,
capacity enforcement, writable readiness or failed-write preservation; all fail
their intended assertions. Five smoke-reader tests pass. Documentation checks
655 local links with zero errors; whitespace checks pass. Baseline three-counter
failures and two newly confirmed semaphore/flag failures are preserved separately.
Local source/build/log hashes and receipts are under `build/review/eventfd-counter/`.
All disposable guests stopped; no physical trial, ABI or Actions setting changed.
PR179 remains draft. Next bounded scope is eventfd semaphore mode and creation
flag validation; blocking I/O and broader Linux adapter semantics remain separate.

## Eventfd semaphore/flags correction — active scope (2026-10-26)

User authorized proceeding with semaphore reads and creation-flag validation.
Base is `e37213bf`; fetched main remains `8bd43adc`, PR179 matches and stays draft,
and PR19 retains Wi-Fi ownership. Scope: private eventfd mode storage, one-token
reads under the existing mutex, validation before slot allocation, host tests for
flag combinations/lifetime/readiness and concurrent guest consumption. Update
current guides and PR179, then continue the bounded adapter audit. Blocking I/O
and kernel notification remain separate; no new ABI or physical acceptance.
Next checkpoint is host regressions and matched sequential thread/libuv VMs.
Actions remain disabled and historical failed evidence must be retained.


Final mode/flag validation: matched `all` build, twenty-five positive host modes
and ASan/UBSan, targeted eventfd-modes VM, complete thread suite and full libuv
consumer VM pass. Four compiled negative controls break flag rejection, mode
storage, one-token subtraction or fcntl mode independence; each fails its intended
assertion. Five smoke-reader tests pass. Documentation checks 655 local links with
zero errors; whitespace checks pass. The baseline's two semaphore/flag failures
and the two new fairness/hangup failures are retained separately, under
`build/review/eventfd-modes/`, with source/build/log hashes. All disposable guests
stopped. PR179 remains draft; next bounded scope is epoll ready-set rotation and
zero-interest native error/hangup reporting. Blocking I/O, kernel notification,
physical acceptance and Actions settings remain unchanged.

## Epoll rotation/hangup correction — active scope (2026-10-26)

User authorized proceeding with ready-set fairness and zero-interest hangups.
Base is `87c48440`; fetched main remains `8bd43adc`, draft PR179 matches, and PR19
retains separate Wi-Fi scope. Own libc epoll delivery rotation and unconditional
native HUP/ERR observation, host stable-set/alias/control regressions and matched
guest coverage. Keep lifetime/revision validation and monotonic wait deadlines.
Use a per-instance cursor shared by aliases, advance only on delivered events,
and select against its current value under the lifecycle mutex. Snapshot all
watches so native/synthetic ordering cannot bias delivery. No new ABI, hardware
trial or Actions run. Next checkpoint: deterministic regressions, then matched
build and sequential thread/libuv VMs; continue auditing adjacent adapter modes.


Final rotation/hangup validation: matched `all` build, twenty-nine positive host
modes and ASan/UBSan, targeted epoll-rotation VM, complete thread suite and full
libuv consumer VM pass. Four compiled negative controls break cursor advancement,
full snapshot scanning, empty-mask polling or current-cursor selection; each
fails its intended assertion. Five smoke-reader tests pass. Documentation checks
655 local links with zero errors; whitespace checks pass. Baseline failures,
initial fixture corrections and two new one-shot/priority failures are retained
under `build/review/epoll-rotation/` with source/build/log hashes. All disposable
VMs stopped. PR179 remains draft. Next bounded scope is one-shot disabling/rearm
and native priority-bit conversion; broader event modes, signal masks and blocking
eventfd I/O remain separate. No physical acceptance or Actions setting changed.

## Epoll one-shot/priority correction — active scope (2026-10-26)

User authorized proceeding with one-shot disabling/rearming and priority mapping.
Base is `9a0f51a9`; fetched main remains `8bd43adc`, draft PR179 matches, and PR19
retains separate Wi-Fi scope. Own private per-registration armed state, delivery
serialization with overlapping waits, MOD rearming/revision checks, and both
EPOLLPRI/POLLPRI conversions. Add deterministic host and matched guest regressions,
update current docs/PR179, then continue the adapter audit. Preserve rotation,
timeouts and descriptor identities. Priority mapping tests model a native source;
no unsupported device/urgent-data capability is implied. Edge delivery, signal
masks and blocking eventfd I/O remain separate. No ABI/hardware/Actions changes.


Final one-shot/priority validation: matched `all` build, thirty-three positive
host modes and ASan/UBSan, targeted epoll-oneshot VM, complete thread suite and
full libuv consumer VM pass. Six compiled negative controls break disarming,
MOD rearming, disabled snapshot/delivery checks or either priority conversion;
each fails its intended assertion. Five smoke-reader tests pass. Documentation
checks 655 local links with zero errors; whitespace checks pass. Baseline failures
and the new EPOLLET failure remain separate under `build/review/epoll-oneshot/`,
with source/build/log hashes. All disposable VMs stopped. PR179 remains draft.
Next bounded scope is an honest edge-mode contract; blocking eventfd I/O, signal
masks, kernel notifications and native priority-source support remain separate.
No public ABI, physical acceptance or Actions setting changed.

## Epoll edge-mode contract — active scope (2026-10-26)

User authorized fixing the confirmed EPOLLET defect, regressions, docs/PR179 and
continued audit with a coverage inventory. Base `aba0999a`; fetched main remains
`8bd43adc`, draft PR179 matches and PR19 retains separate Wi-Fi ownership. Reject
EPOLLET on ADD/MOD with EOPNOTSUPP before mutating identity/watch state; preserve
level and one-shot behavior, including failed MOD on a disabled watch. Own only
the adapter, host/guest regressions and current audit documentation. Validate
sequentially with host/sanitizers, matched build and disposable thread/libuv VMs.
Actual edge notifications, signal-mask waits and blocking eventfd remain separate.
No ABI, hardware acceptance or Actions changes.

Final edge-contract validation: all 34 host modes pass normally and with
ASan/UBSan. Four compiled negative controls fail the intended refusal or
state-preservation assertions. Matched `all` build, targeted epoll-edge VM,
complete thread suite and full libuv consumer VM pass sequentially. Five
smoke-reader tests pass; docs check 664 local links with zero errors and
whitespace checks pass. Baseline failure, source/build hashes and logs are
retained under `build/review/epoll-edge/`. All disposable guests stopped.
The current audit inventory separates completed bounded work from the event
adapter queue and broader OS reviews. Next scope is epoll_pwait mask semantics;
edge delivery remains explicitly unsupported. PR179 stays draft; no physical
acceptance, public ABI or Actions setting changed.

## Event-adapter completion and merge — active scope (2026-10-26)

User authorizes completing the bounded event-adapter audit and merging PR179.
Base `c98dcded`; fetched main remains `8bd43adc`. PR179 is mergeable with no
required checks reported; Actions remain disabled. PR19 retains separate Wi-Fi
ownership and is not part of this merge. Use the WNP development completion
criteria in the existing audit inventory. Required: correct epoll_pwait masks,
blocking eventfd and native poll integration; check remaining event/control
contracts and ownership boundaries, rejecting unsupported modes explicitly.
Own libc event/poll glue, regression fixtures/guests and current docs. Preserve
public syscall ABI and device gates. Sequential host/sanitizer, matched build,
thread/libuv VM gates and final PR review precede merge. Actual edge notifications,
cross-process adapter sharing and full Linux parity are outside this target.

Closeout ownership review also covers process/rights glue: refuse adapter-handle
SCM_RIGHTS export and exec/spawn inheritance of non-CLOEXEC adapters, rather than
allowing a kernel placeholder with lost libc state. Pseudo-socket epoll registration
is explicitly refused until its connect/listener transitions share lifecycle
serialization; native socketpair readiness remains covered. Raw syscalls and
native watched descriptors transferred before/after registration remain outside
the process-local ownership contract. No general transfer-tracking ABI is added.

## Closeout validation and delivery

The bounded process-local event-adapter audit is complete. All 46 host modes pass
normally and under ASan/UBSan. Ten compiled negative controls fail their intended
mask entry/restoration, blocking retry/generation, signal dispatch/restart, poll
identity/immediacy, normal-event-bit or export-guard assertions. Native poll/select
host regressions and five smoke-reader tests pass. The matched x86_64 build,
corrected targeted event-adapter-final guest, complete thread suite (all eight
event-adapter groups) and full libuv consumer suite pass sequentially. Docs check
664 local links with zero errors; whitespace checks pass. No unresolved confirmed
defect remains within this selected contract. Failed diagnostic runs remain
separate evidence, not passes.

Source/build/log hashes and the validation receipt are retained under
`build/review/event-adapter-closeout/`. All disposable guests stopped. Delivery is
[PR179](https://github.com/PaulMDemers/SrvrOS/pull/179), whose GitHub record is the
source of the final merge state/SHA. The user authorized merge after these gates;
PR19 remains separate Wi-Fi work. No kernel/syscall ABI, native-hardware acceptance
or Actions setting changed. Broader filesystem/network/process/security reviews
and optional adapter capabilities remain in the current audit inventory.

## Broader audit: exFAT import contracts — active scope (2026-10-26)

User requests resuming the broader system audit. Main is `dfd63fef`; PR181 owns
desktop lifecycle work, PR180 owns Air6,1 retest/poll evidence and PR19 Wi-Fi.
This branch owns kernel exFAT import validation and initialized-data reads, host
fixtures, current docs and disposable VM validation. Source review identifies
unchecked boot shift/geometry fields and ignored ValidDataLength. First checkpoint:
reproduce both, correct confirmed defects with mount/read/rename/remount coverage,
then publish a draft PR. Preserve mutation ordering/quarantine and existing
hardware gates. No changes to the other agents' poll/UI/device implementations.
Acceptance: bounded source-level host tests/sanitizers, matched build and relevant
thread/libuv VMs, documentation checks and explicit remaining audit queue. Full
filesystem/security/crash consistency certification and native trials are excluded.

### Import-contract checkpoint results

The [filesystem audit](../filesystem-audit-2026-10-26.md) now records two fixes:
checked single-FAT boot geometry and zero-filled reads beyond imported
`ValidDataLength`, including rename/replacement preservation and overwrite cache
publication. Baseline: one control passed and thirteen directed modes failed.
Final: sixteen host modes pass normally and under ASan/UBSan; six independently
compiled negative controls fail their intended runtime assertions. The existing
entry-layout test passes with ARM host-test/Darwin linker options.

The matched build, dedicated imported-data VM and complete thread suite pass.
The libuv run with its default ten-second completion budget ended after
`fs poll ok` (exit 3); the failed log is retained. A fresh 60-second-budget run
passed all required markers. Evidence, source/output hashes and receipts live in
`build/review/filesystem-audit/`. No physical trial or Linux exfatprogs crash
campaign is claimed. GitHub Actions remain disabled.

During closeout PR180 merged into `main` (`afc8bb67`); its header/test/documentation
changes do not overlap this scope and were fast-forwarded into this branch.
PR181 and PR19 remain separate. The next checkpoint is entry integrity and
parser structure: source inspection plus a read-only fixture check confirms that
the generated disk's 169 root entries have unset checksums, while mount accepts
them. Allocation ownership, unmount lifetime and crash recovery follow; the
broader OS audit remains open.

The post-PR180 matched rebuild and all three VMs pass: dedicated import (16.47 s),
complete threads (39.69 s), libuv with the 60-second completion budget (10.01 s
including boot). `integrated-vms.json`, the integrated build receipt and runtime
hash inventory distinguish these runs from the earlier baseline. Documentation
link/gallery and whitespace checks pass. This checkpoint is ready for draft PR
review; the listed follow-up defects are not represented as fixed.

Published runtime commit `a8d1617b` in [draft PR182](https://github.com/PaulMDemers/SrvrOS/pull/182).
The broader audit remains active at the next entry-integrity checkpoint.

### Entry-integrity audit — active scope (2026-10-26)

User authorized the next checkpoint on PR182. Base is `352f9b0d` with main at
`afc8bb67`; refreshed PR181 remains desktop lifecycle and PR19 Wi-Fi. This pass
owns exFAT entry-set integrity/structure, bounded ASCII name import, generated
image entries and directed host/VM fixtures. Reproduce corrupt-checksum acceptance,
malformed stream/name layouts and unsafe names before correction. Fix the packer
and parser together; retain explicit unsupported-format refusal and document the
need to rebuild older generated disks. Acceptance: actual driver regressions and
sanitizers, independent generated-entry checks, matched build, import/thread/libuv
VMs and docs. Allocation cross-link ownership, unmount lifetime, full Unicode,
boot-region recovery and crash consistency remain subsequent checkpoints.

### Entry-integrity checkpoint results

The generator now emits correct file-entry checksums, name hashes and allocation
flags, and avoids allocating empty streams. The driver checks basic stream/name
layout, checksum, ASCII name identity/hash and empty-stream flags before
publication. Additional secondary extensions and unsupported names are refused.
Directory scans now reject traversed cycles, invalid FAT successors and chains
shorter than their declared directory data. Older generated disks must be rebuilt
with the matching kernel; persistent workspace/recovery media were not modified.

Ten new modes failed on the previous implementation. All 29 current host modes
pass normally and under ASan/UBSan; seven new compiled negative controls are
caught. Four generator groups and an independent recursive check of all 372 sets
in 16 directories pass. The existing entry-layout test, matched build and all
four VMs pass sequentially: import 16.48 s, corrupt-entry/fallback 16.35 s,
complete threads 40.39 s, libuv 10.22 s (60-second completion budget).

The first corruption harness wrongly assumed refusal prevented all application
startup. Its retained failure showed the intended clean initramfs fallback. The
corrected `exfat-refused` guest test verifies rejected paths are absent and the
fallback remains readable, alongside explicit mount-refusal/fallback diagnostics.
No timeout is counted as a pass. Evidence and tested source/output hashes are in
`build/review/exfat-entry-audit/`; documentation and whitespace checks pass.

PR182 remains the delivery branch. Next: allocation/cross-link ownership,
duplicate paths, bitmap/FAT disagreement and unmount/open-reference lifetime.
Source review confirms monitor and workspace callers have busy guards; trace
those against the direct filesystem teardown and other callers before declaring
a lifetime defect. Boot/upcase recovery, full Unicode and power-loss atomicity
remain outside this checkpoint. No physical tests or Actions dispatch occurred.

### Allocation/namespace audit — active scope (2026-10-26)

User authorized the next PR182 checkpoint. Base is `0ba50fe9`; refreshed main
`9f18d7d6` adds the Air6,1 failed stress report only. PR184 owns worker-pipe
admission, PR181 desktop lifecycle and PR19 Wi-Fi. This pass owns exFAT namespace
admission, imported allocation checks and exact-alias release, plus focused
host/VM evidence and docs. Reproduce duplicate-name and overlapping-mount damage,
shared-allocation release and writable acceptance of inconsistent ownership.
Preserve generated exact aliases; quarantine unsupported/corrupt allocation
layouts before mutation. Inspect open-reference teardown callers without assuming
a defect merely because the low-level unmount API lacks its own process check.
Acceptance: directed actual-driver tests, sanitizers/negative controls, matched
build and disposable guest consumers. Cross-device physical aliasing, general
hot-unplug, power-loss recovery and other agents' process admission code are out
of scope. No hardware or Actions dispatch is requested.

### Allocation/namespace checkpoint results

All thirteen initial modes reproduced defects at the previous runtime: duplicate
names were accepted, overlapping/occupied mounts could remove unrelated VFS
entries, surviving exact aliases lost data/allocation ownership, and inconsistent
allocation maps remained writable. The driver now admits exclusive canonical
mount namespaces, rejects duplicate imported names, retains published aliases'
extents until last-owner retirement, and checks supported allocation maps before
allowing mutation. Checker failures/inconsistencies latch read-only mode.

All 47 host modes pass normally and under ASan/UBSan; eight new compiled negative
controls are detected. Fault cases cover mount allocation retry, checker I/O/OOM,
pre-publication overwrite cleanup and uncertain-write quarantine. The initial
fault test's clean-map expectation after a metadata-write failure was incorrect:
existing quarantine deliberately retains the uncertain new allocation. Its failed
log is preserved; separate read/write failure tests now verify both contracts.

The matched build, independent 372-entry image check and existing entry-layout
test pass. Six sequential disposable VMs pass: exact aliases 16.52 s, corrupted
bitmap quarantine 16.43 s, import 16.53 s, corrupt-entry fallback 16.41 s, complete
threads 40.35 s, and libuv 10.17 s (60-second completion budget). Evidence and
source/output hashes are in `build/review/exfat-ownership-audit/`. Docs/whitespace
checks pass. Main's documentation-only PR183 was merged into this branch before
implementation; no other agent's runtime changes were incorporated.

The normal monitor/workspace/workspace-image busy guards cover retained linked
file/directory descriptions and current directories. Unlinked/replaced snapshots
are promoted to process-owned storage and detached from backend nodes. No new
open-reference defect was confirmed by this trace; no process-admission edits
were made. Caller serialization, overlapping physical device views, raw leases
and removal remain next, followed by recovery/crash consistency. This is bounded
source/host/VM acceptance, not a complete filesystem or hardware audit.

### Allocation checkpoint integration verification

After checkpoint `28bdb374` was pushed, main `0858e8e6` incorporated PR181's
desktop lifecycle and PR185's UART/startup trace updates. The only merge conflict
was the appended Makefile test targets; both target groups were preserved.
No filesystem implementation/test source changed during integration. The matched
build, 47 exFAT host modes, four generator groups, 40-case shared-window host test
and 372-entry image check pass against that baseline.

Six integrated VMs pass sequentially: aliases 16.46 s, bitmap quarantine 16.39 s,
import 16.45 s, corrupt-entry fallback 16.33 s, complete threads 39.89 s and libuv
10.06 s. Integrated logs, VM receipts and source/output hashes are retained in
`build/review/exfat-ownership-audit/`. PR184/19 runtime work remains separate.
No desktop UI, physical hardware or Actions acceptance is claimed by these runs.
The remaining queue is unchanged: overlapping device views, raw leases,
serialization/removal and recovery/crash consistency.

### Device-view/raw-lease audit — active scope (2026-10-26)

Continue PR182 from `0654615b`, integrating actual main `5bcd061c` (PR184
worker-pipe admission) before implementation. Own block range identity/claims,
exFAT mount admission and workspace scratch/image lease handoff, with directed
host regressions and matched disposable VMs. PR19 Wi-Fi work stays separate.
Reproduce overlapping mounts and raw-lease remounts, preserve disjoint partition
mounts and the authorized inner-image mount, and test release/failure/retry.
No hardware, Actions dispatch, general hot-unplug or crash-repair work is requested.

### Device-view/raw-lease checkpoint results

Confirmed overlapping block mounts and a parked outer-volume remount during an
active image lease against the previous runtime. Declared physical view identity
and range claims now reject overlap before media reads. Raw scratch/image leases
reserve the entire transport, delegate only the designated image view, reject
sibling-mount handoff and retain poisoned exclusion until reboot. Clean
refusal/close releases claims; stale tokens cannot release later owners.

All 51 import modes pass normally and under ASan/UBSan. The real block/partition,
workspace and USB callback tests pass; seven synthetic real image-adapter runs
cover successful mutation, two pause points, partial-write/flush quarantine and
two clean refusals/retries with outer-byte preservation. Block/workspace/image
adapters pass sanitizers. Six compiled negative controls are detected. Existing
AHCI host tests pass under x86_64 host execution. The first alias probe had an
incorrect assertion placement; only the corrected overlap failure is counted.
Unrelated aggregate Mac linker/assembly/sync-command failures are retained; the
focused Darwin/x86 wrapper recipe is in the Mac guide. No full Linux host-suite
or exfatprogs fault-sweep acceptance is claimed.

Matched build and 372-entry image validation pass. Six sequential VMs pass:
aliases 16.57 s, bitmap quarantine 16.42 s, imports 16.43 s, corrupt-entry fallback
16.40 s, complete threads 39.75 s and libuv 10.13 s. Evidence is retained in
`build/review/storage-claims/`, including tested hashes and build/VM receipts.
The checkout includes main `5bcd061c`/PR184. PR19's workspace-log changes were
inspected for overlap and remain separate; no logging files were changed here.

Follow-on source tracing found registered USB/AHCI devices are quarantined and
retained on failure, with no general runtime unregister/reuse path. No additional
device use-after-free was confirmed. Hot removal/generation changes, caller
serialization and mutation recovery/crash consistency remain next. No native
hardware, battery/EC trial or Actions dispatch occurred.

### Identity invalidation/teardown audit — active scope (2026-10-26)

Continue PR182 from `a35e2c33`; integrate main `19a6fe05` (PR186's Air6,1
documentation) before implementation. Inspect runtime media identity failures,
cache admission after quarantine, and exFAT operation/teardown serialization.
Registered-device reuse/hot-plug remains unsupported; add no recovery/rebind API
without a confirmed need. Own block/USB/AHCI health propagation, filesystem
lifecycle guards and focused host/VM evidence. PR187 desktop timing and PR19
Wi-Fi/workspace logging are separate. No hardware or Actions dispatch.

### Identity invalidation/teardown checkpoint results

Five original probes confirmed teardown during foreign ownership, read/write
callbacks, mount scans and checks. The filesystem now holds CPU0 storage
ownership for each complete backend operation, rejects same-owner callback
teardown and rechecks process files/directories/cwds under the teardown owner.
Boot-time fallback and normal nested metadata operations remain supported.

A published USB failed-command status previously allowed later writes using the
old geometry. Read/write/flush probes reproduce it. Runtime status/transport
failures now invalidate the block lineage, as do AHCI uncertain completions.
Cached aliases, writes/flushes, claims and parked-workspace admission cannot bypass
terminal invalidation. Published USB storage objects also refuse repeated attach
before any probe; the old entry point accepted unsupported republication. No
runtime rebind/generation recovery is added; descriptors/DMA stay pinned.

All 61 import/lifecycle modes pass normally and under ASan/UBSan. Actual block,
USB, AHCI and workspace tests cover invalidation and balanced admission; seven
synthetic real image-adapter cases still pass. Block/workspace/image fixtures
also pass sanitizers. Eight compiled negative controls are detected. The matched
build, four generator groups and 372-entry image check pass. Six sequential VMs
pass: aliases 16.55 s, bitmap quarantine 16.40 s, imports 16.55 s, corrupt-entry
fallback 16.40 s, complete threads 40.08 s and libuv 10.06 s. Evidence, tested
hashes and build/VM receipts are in `build/review/storage-lifetime/`.

The checkout includes main `19a6fe05`; PR187 desktop timing and PR19 Wi-Fi/logging
remain separate. Further tracing confirms dirty marking and uncertain-write
quarantine are not a journal. Next: namespace/sidecar interruption and durable
rename/replacement recovery. Live media replacement, arbitrary retained kernel
pointers and hardware hot-unplug remain unaccepted. No physical trial or Actions
dispatch occurred.

### Lifetime checkpoint integration verification

Checkpoint `801ac9ca` was then integrated with main `4d195e25`/PR187's desktop
loop diagnostics. The merge was clean; all 19 storage source/test manifest files
are unchanged. The integrated storage host gate, 61 modes, three loop-timing
tests, matched build and 372-entry generated-image check pass. Six VMs pass again:
aliases 16.52 s, bitmap quarantine 16.46 s, imports 16.45 s, corrupt-entry fallback
16.44 s, complete threads 40.03 s and libuv 10.06 s. Integrated logs, tested hashes
and build/VM receipts remain under `build/review/storage-lifetime/`. PR19 remains
separate. These runs do not extend native desktop or hardware acceptance.

### Rename/sidecar audit — active scope (2026-10-26)

Continue PR182 from `3c0c84a7`, including main `4d195e25`. Review namespace
replacement ordering, sidecar validation/promotion and interrupted mutation
quarantine. Add portable deterministic write/flush interruption probes and
focused parser regressions; retain the distinction between quarantine and
journaled recovery. PR19 Wi-Fi/workspace logging has no storage-file overlap.
Own exFAT, VFS metadata persistence results, host fixtures and related documentation. No physical trial,
Actions dispatch, live media rebind or automatic offline repair is in scope.


### Rename/sidecar checkpoint results

Nine directed before/after probes reproduce partial or out-of-mount sidecar
application, numeric overflow, truncated/NUL records, dirty-media replay,
destructive cleanup after temporary-read failure, and hidden chmod/time sync
failures. Sidecars now validate wholly from one buffer, stay within the mount,
and are not replayed on quarantined media. Unreadable temporary files survive
mount for retry. VFS metadata mutations propagate persistence failure and restore
live metadata. The original file-backed source also fails the committed-sidecar
presence assertion; sync/promotion now replace the committed file before retiring
the temporary record.

All 72 modes pass normally and under ASan/UBSan. The portable synthetic-volume
sweep passes 144 write/flush/failure/half-write boundaries across rename,
replacement and promotion. Storage/lease/VFS host tests, the matched build and
372-entry generator check pass. Six VMs pass: aliases 17.20 s, quarantine
16.71 s, imports 17.16 s, corrupt entry 16.70 s, complete threads 40.77 s and
libuv 10.74 s. Logs, source hashes, original failures, harness corrections and
build/VM receipts are retained in `build/review/rename-sidecar/`.

Main remains `4d195e25`; PR19 was refreshed at `35041be0` and its Wi-Fi/logging
and separate Make targets do not overlap exFAT/VFS. No native media or Actions
were used. The fault model covers ordered persisted prefixes; it does not accept
controller reordering, sector atomicity, journaled rename or physical power-loss
recovery. Follow-on tracing identifies boot-region/backup and system-stream
validation as the next bounded audit; full-format conformance remains open.

### Boot/system-stream audit — active scope (2026-10-26)

Continue PR182 from `bc024735`, including main `4d195e25`. Validate boot checksums
and backup handling before trusting geometry, then root bitmap/upcase descriptors
and their streams before metadata recovery or mutation. Own exFAT, generated
images, portable fixtures and related documentation. Refresh PR19 at `f70ee542`:
its Wi-Fi/logging changes and separate Make targets do not overlap this runtime
scope. Preserve offline recovery boundaries; no automatic boot repair, native
media trial, Actions dispatch or new Unicode filename support is planned.


### Boot/system-stream checkpoint results

Nineteen initial directed probes reproduce missing boot-region validation and
system-stream defects; the subsequent unknown-critical-entry probe also fails
against the previous behavior. Boot checks now cover complete regions and all
checksum words, required fields/signatures and exact header snapshots. A unique
valid backup can provide read-only fallback even with a damaged primary sector
size; ambiguous candidates are refused. A damaged or disagreeing backup also
prevents writes through a valid primary. No automatic repair is added.

Imports require single root bitmap/upcase descriptors, the correct bitmap
identity, exact system FAT chains, and valid upcase checksum, coverage and ASCII
mappings. Fragmented system allocations are followed correctly. Generated images
now contain both boot regions and a complete compressed upcase table; older
unchecked images must be rebuilt. Follow-on probes reproduce teardown during
statfs and truncated multi-cluster bitmap counts. Statistics now hold storage
ownership and traverse complete chains. Multi-cluster mutation/graph checking
remain unsupported. A suspected bitmap overread was ruled out and is not counted
as a confirmed defect; that evidence is retained.

All 100 directed modes and the 29-case sector-size/backup/bitmap matrix pass
normally and under ASan/UBSan. The 144 interruption boundaries, storage/lease host
gates, matched build, six generator groups and 372-entry image check pass. Six
VMs pass: aliases 16.44 s, quarantine 16.51 s, imports 16.67 s, corrupt entry
16.53 s, complete threads 41.49 s and libuv 10.84 s. Evidence and tested hashes
are retained under `build/review/boot-system/`.

The final refresh still has main `4d195e25` and PR19 `f70ee542`; no new runtime
overlap or integration dependency appeared. No Actions, native media test or
physical recovery was performed. Follow-on tracing reaches allocation exhaustion
and directory-growth rollback; full Unicode, automatic repair and physical
power-loss guarantees remain outside the accepted profile.

### Allocation/growth audit — active scope (2026-10-26)

Continue PR182 from `16b81077`, including main `4d195e25`. Inspect allocation
exhaustion, size bounds, directory extension and failure ordering. Own exFAT,
focused host fault fixtures and related documentation. PR19 remains `f70ee542`;
its Wi-Fi/logging and separate Make targets do not overlap this runtime scope.
Distinguish clean refusal before mutation from uncertain-write quarantine; add
no journal, native-media trial, Actions dispatch or automatic repair.


### Allocation/growth checkpoint results

Out-of-space and allocator-refusal probes reproduced premature directory tail
writes and quarantine. Oversized payloads/entry sets also reached mutation work.
The writer now selects space and initializes an extension before retiring end
markers; impossible requests are refused before dirty marking or scratch
allocation. Cluster rounding is overflow-safe and allocator counts are bounded.

Stale `ghost` records following an end marker reappeared after insertion and
remount, both within the same cluster and in later allocated directory clusters.
The writer now respects the logical end across the chain and clears undefined
records before publication. UBSan additionally caught signed overflow in the
long-name hash rotation; its bit-15 rotation is now bounded.

All 108 directed modes pass normally and under ASan/UBSan. The 193 root/subdirectory
growth fault boundaries pass normally and under sanitizers, alongside the
existing 144 rename/sidecar boundaries, 29 boot-sector cases, storage/lease gates,
six generator groups and 372-entry image check. The matched build and six VMs
pass: aliases 16.96 s, quarantine 16.98 s, imports 17.02 s, corrupt entry 16.63 s,
complete threads 40.39 s and libuv 10.72 s. Evidence, original failures, corrected
harness assumptions and tested hashes remain in `build/review/allocation-growth/`.

The publication refresh found main `ee5468f6` (PR188/189), changing only the Air6,1
machine record. The runtime was tested above on the `4d195e25` integration base;
the clean documentation merge and refreshed-build check are recorded next.
PR19 remains separate. Follow-on tracing reaches deletion/truncation retirement
and low-space sidecar updates. Partial mutations still quarantine; this adds no
compound space reservation, journal, new errno contract or native acceptance.


### Allocation checkpoint integration verification

Runtime checkpoint `497e2e6d` was merged cleanly with main `ee5468f6` (PR188/189).
The incoming change is confined to the Air6,1 machine record. Every tested source
and output hash is unchanged; `all` needed no recompilation and refreshed the
matched-build receipt. A fresh disposable import VM passes in 17.00 s. The full
six-VM results above remain evidence for the identical runtime artifacts.
`integrated-build.log`, `integrated-macos-build.json` and `integrated-result.json`
retain this verification under `build/review/allocation-growth/`.

## Deletion/truncation retirement — active scope

Continue PR182 from `91a4f594` on integration base `ee5468f6`. Audit exFAT
retirement ordering and low-space sidecar updates using healthy allocation
fixtures and write/flush interruption sweeps. Touched areas are exFAT, its host
fixtures, test targets and current storage guides; process unlink reconciliation
is inspected with its existing regressions. PR190 (`4c0b3fb7`) owns scheduler
wait attribution; PR19 (`3cb0a338`) owns USB Wi-Fi. Their actual diffs have no
exFAT implementation overlap; preserve their separate Makefile targets. Next
checkpoint is reproduced defects, bounded fixes, sanitized host checks and
matched disposable VM results. No physical install, native acceptance or merge
is planned.

### Retirement checkpoint verification

All 116 import modes pass normally and under ASan/UBSan; all 323 retirement fault
boundaries pass normally and under ASan/UBSan. The current 161 growth boundaries,
144 rename/promotion boundaries, 29 boot matrix cases, storage/lease tests and
process open-content/orphan tests pass. The matched build, six generator groups
and recursive 372-entry image check pass. Six sequential disposable VMs pass:
aliases 17.06 s, quarantine 16.60 s, imports 16.99 s, corrupt-entry 16.68 s, threads 40.18 s, libuv 10.61 s.

Evidence and source/artifact hashes are retained under
`build/review/retirement-sidecar/`; the two original regression failures match
`91a4f594` byte-for-byte for the backend source. The compiler-stub failure is
retained separately and is not a test pass. Main remains `ee5468f6`; refreshed
PR190 (`f1587a2f`) and PR19 (`9a50f4dc`) remain separate with no exFAT overlap.
Documentation/link and whitespace checks pass. No native hardware test or
Actions dispatch was performed.

The confirmed full-volume truncate/shrink defect is corrected and PR182 remains
a draft. Follow-on snapshot/teardown tracing confirmed no further defect. Next
bounded question: process writeback retry after a clean backend space refusal,
which currently shares the failed-content latch used for uncertain I/O. Native
unregister/rebind, physical power-loss recovery and broader subsystem coverage
remain open.


### Retirement checkpoint integration — active scope

After publishing `d34fcc97`, main advanced to `4ee18f69` with PR190's scheduler
wait attribution and desktop diagnostic ABI. The only textual conflict is the
Makefile tail: preserve both `test-storage-leases` and `test-scheduler-trace`.
No exFAT implementation conflict exists. Rebuild matched artifacts and rerun
the affected scheduler/syscall/desktop host checks, storage gates and six VMs
before republishing. The pre-integration evidence above remains tied to its
original runtime. PR19 remains separate at `9a50f4dc`.

### Retirement checkpoint integrated verification

Integration `9510ce8c` includes main `4ee18f69` (PR190). The Makefile conflict
was resolved by preserving both test targets; exFAT source was unchanged.
The scheduler admission/trace/syscall gates, five loop-timing tests, six scheduler
activity tests and desktop-session fixture pass. The latter uses the documented
Darwin wrapper with `-Wno-error=array-bounds` for only
`build/tests/desktop-session-host`: Clang diagnoses the unselected inline-array
branch at index 31 after asserted dynamic growth to 32/128 slots. Both warnings
remain visible, and the original strict compile failure is retained; no desktop
implementation was changed to silence it.

The rebuilt 116-mode import suite, 323 retirement boundaries, storage/lease and
process tests pass. The matched full build, six generator groups and recursive
372-entry image check pass. A fresh six-VM pass on the integrated artifacts gives:
aliases 16.53 s, quarantine 16.44 s, imports 16.51 s, corrupt-entry 16.38 s, threads 40.02 s, libuv 10.11 s.

Commands, failures, passing reruns, build receipt and tested source/artifact
hashes are under `build/review/retirement-sidecar/integration/`. The earlier
ASan/UBSan evidence still covers the unchanged exFAT implementation and fixtures;
it is distinct from the new integrated guest evidence. This final record changes
documentation only. PR182 remains draft; the next audit question is process
writeback retry after clean space refusal.

## Process writeback retry — active scope

Continue PR182 from `834fbff0` on main `4ee18f69`. Audit clean writeback refusal
versus irreversible/uncertain failure, including retained content, shared opens,
retry, close and process-exit reporting. Touched areas: process writeback, an
exFAT outcome contract, focused host fixtures and storage guides. Refreshed PR19
at `e04e2dc0` retains Wi-Fi ownership and has no process/exFAT/VFS overlap; keep
its Makefile additions separate. Next checkpoint is an original-source retry
reproducer, atomic backend outcome coverage, process regressions and matched
VM validation. No native install or PR merge is planned.

### Writeback findings and correction

Two confirmed defects are corrected: clean exFAT refusal no longer permanently
poisons shared process content, and global sync includes dirty reader-only
survivors after failed writer close, once per content object. Retry eligibility
is captured under filesystem ownership after proven clean refusal; partial or
uncertain mutations remain sticky. Original process source `834fbff0` fails the
real full-volume bridge, and a separate process regression reproduces the sync
omission. The new backend tests include reservation OOM and failed clean-marker
write/flush controls. Final-close and exit failure reporting remain intact.

### Writeback checkpoint verification

All 123 import modes, the real process/exFAT bridge and public process-output
regressions pass normally and under ASan/UBSan. The 323 retirement, 161 growth,
144 rename/promotion and 29 boot-matrix boundaries/cases pass normally, along
with storage/lease gates. The matched build, six generator groups and recursive
372-entry image check pass. Six sequential disposable VMs pass:
aliases 16.53 s, quarantine 16.47 s, imports 16.50 s, corrupt-entry 16.41 s, threads 40.53 s, libuv 10.18 s.

Evidence, original failures, harness corrections and tested hashes remain under
`build/review/writeback-retry/`. Main remains `4ee18f69`; PR19 at `e04e2dc0` has
no process/exFAT/VFS overlap. Documentation/link and whitespace checks pass.
No native install, Actions dispatch or PR merge occurred. Follow-on syscall/libc
tracing confirmed no further defect; descriptor inheritance and delayed-error
ownership across process exit are next.

## Descriptor inheritance and exit errors — active scope (2026-10-28)

Continue PR182 from `2a091597`, integrating fetched main `5a94d755` through
PR230 before implementation. Audit inherited file references, close-on-exec,
spawn/fork setup rollback and ownership of delayed writeback errors at exit.
Touched areas: process lifecycle/file descriptions, focused host regressions,
current storage/process guides and this host record. Main's spawn-image,
directory and scheduler changes are included rather than duplicated. Air6,1 and
Air7,2 branches are merged; PR19 at `114dee34` retains separate Wi-Fi scope with
no process/exFAT/VFS changes. Preserve its Makefile work. Next checkpoint is
bounded reproducers, fixes for confirmed defects and matched host/VM evidence.
No native install, hardware acceptance or PR merge is planned.

### Inheritance audit result

The scope above is complete at the production file-helper level. No new runtime
defect was confirmed. Added actual inheritance regressions cover both exit orders,
shared offsets, clean/sticky error ownership, explicit exit status, CLOEXEC/reuse,
partial clone failure, stdio snapshot rollback and reader-only survival after
parent exit. The supported profile now documents that nonstandard inherited files
may remain buffered after a process exits successfully. Main `5a94d755` was merged
cleanly in `647ee564`; the matched build and validation include it.

### Inheritance checkpoint verification

All directed inheritance cases pass without a production-code change. The final
process-output fixture and real process/exFAT bridge pass normally and under
ASan/UBSan. Normal storage validation passes all 123 import modes, 323 retirement,
161 growth, 144 rename/promotion and 29 boot-matrix boundaries/cases, plus storage
and lease gates. Integrated-main checks pass for process lifetime/service/events,
lifecycle waits, I/O scheduler, scheduler trace/activity and syscall I/O.

The matched OS build, six generator groups and 372-entry generated-image check
pass. Seven sequential disposable VM gates pass: aliases 16.80 s, quarantine 16.47 s, imports 16.55 s, corrupt-entry 16.50 s, threads 40.33 s, libuv 10.20 s, lifetime 22.95 s.
The lifetime gate covers two runs, 256 sequential apps, 32 competing-waiter pairs
and PTY/job-control regression. Documentation checks resolve 692 local links;
whitespace checks pass. Evidence, source/artifact hashes and VM receipts are under
`build/review/inheritance-exit/`. Earlier broader sanitizer evidence remains dated
under previous checkpoints; full Linux test-core and native hardware acceptance
are not claimed. No Actions dispatch or PR merge occurred.

Follow-on tracing reached socket references and creator cleanup. The next bounded
scope is queued TCP output/FIN completion across final close and creator versus
inheritor exit. No networking defect is claimed yet; full concurrent fork/exec/kill
fault injection and hot unregister/rebind remain outside completed coverage.

## TCP close and process exit — active scope (2026-10-28)

Continue PR182 from `98fb56ff` on unchanged main `5a94d755`. Audit outstanding
TCP transmissions, deferred FIN, creator/inheritor exit and listener backlog
ownership. Touched areas: `kernel/src/net.c`, focused host/network VM fixtures,
Makefile and current networking/audit guides. PR19 at `b8d63682` retains Wi-Fi
bring-up work without `net.c` or process-lifecycle changes; preserve its Makefile
additions. Air6,1 diagnostic AP ownership and Air7,2 native desktop evidence stay
separate. Next checkpoint is deterministic original-source reproductions,
bounded fixes and matched host/VM validation. No native install or PR merge.

### TCP exit audit result

The scoped audit confirms five related defects: creator cleanup erased outstanding
TCP data/FIN and inherited listener backlog; deferred FIN lacked the normal close
deadline and ignored send failure; peer FIN could prematurely retire a local FIN
that was not acknowledged. Final close now transfers pending work to protocol
ownership, inherited backlog clears stale creator pointers, and deferred/error/
simultaneous-close transitions retain bounded lifetime. See the
[TCP lifetime report](../tcp-lifetime-audit-2026-10-28.md).

## Verification

All eight new host modes pass on x86_64/Rosetta and under native ARM64 ASan/UBSan.
Original-source verification against `98fb56ff` fails seven modes, with abort
controls passing. Existing TCP accept, e1000 receive, network worker/device,
process output/lifetime and syscall I/O host gates pass. The packet accept
fixture's preexisting `kmalloc(uint64_t)` stub conflicted with the production
`size_t` declaration on macOS; matching the declaration fixes the harness build.
The failed build log is retained separately from final results.

The matched kernel/userspace build, six generator groups and 372-entry generated
image check pass. Six sequential disposable VMs pass: tcp-ready 11.17 s, tcp-stress 47.79 s, netabi 10.05 s, libuv 10.68 s, threads 40.00 s, lifetime 23.59 s.
TCP stress uses 12 clients. The lifetime gate repeats 256 sequential apps and
32 competing-waiter pairs over two iterations, followed by PTY checks. Current
source/artifact hashes, original failures, final host/sanitizer results and guest
receipts are retained under `build/review/tcp-exit/`. Earlier storage matrices
remain dated evidence; they were not rerun for this network-only production fix.
Documentation/link and whitespace checks pass. No native hardware acceptance,
Actions dispatch or PR merge occurred.

Next bounded question: established-state ACK bounds, sequence wrap and stale/
closing socket handles. No complete TCP or broad-system audit is claimed.

## TCP ACK admission and stale handles — active scope (2026-10-28)

Continue PR182 from `29df1192` on unchanged main `5a94d755`. Audit established
ACK admission, sequence wrap, FIN/data retirement and application references to
closing/reused TCP slots. Touched areas: `kernel/src/net.c`, focused packet/host
regressions, Makefile and current networking/audit guides. PR19 remains at
`b8d63682` with separate Wi-Fi work and no net/process overlap. Other machine
scopes remain the recorded AP diagnostic and native desktop work. Next checkpoint
is deterministic before/after evidence, bounded fixes and matched VM checks.
No native install, hardware qualification or PR merge is planned.

### ACK/handle audit result

Corrected unsent-data ACK retirement, invalid outbound SYN-ACK completion and
retired-handle application access. Reference checks cover hold, I/O, endpoint/
error queries, shutdown and readiness; final close wakes blocked I/O while
protocol-owned slots retain retransmission. Wrap and slot-reuse controls passed
without a runtime correction. The HTTP helper accepts an explicit bounded port
for the local outbound integration gate. See the
[TCP audit](../tcp-lifetime-audit-2026-10-28.md).

### ACK/handle verification

All fifteen modes pass under x86_64/Rosetta and native ARM64 ASan/UBSan. The
seven new modes reproduce five failures on original source `29df1192`; wrap and
slot-reuse/saturation controls pass. Existing TCP accept, e1000 receive, network
worker/device, process output/lifetime and syscall I/O host checks pass. The
matched kernel/userspace build, six generator groups and recursive 372-entry
image check pass.

Seven sequential disposable VMs pass: tcp-ready 11.08 s, tcp-stress 47.38 s, netabi 10.29 s, libuv 10.21 s, threads 40.17 s, lifetime 23.51 s, outbound-local 10.58 s.
TCP stress uses twelve clients. The outbound gate uses a temporary localhost
HTTP server, verifying `/ack-check` and the exact response body; no public web
service is required. The helper's new `--port` default is 80, with invalid bounds
refused before VM launch. Documentation/link and whitespace checks pass.

Current logs, original failures, sanitizer results and tested source/artifact
hashes are retained under `build/review/tcp-ack/`. Earlier storage matrices are
dated evidence and were not rerun for this network-core change. No native
hardware qualification, Actions dispatch or PR merge occurred.

Next bounded question: RST/receive-sequence admission and retained descriptor
errors after terminal expiry. Complete TCP conformance, handle-ID-space
exhaustion and concurrent fork/exec/kill remain outside the completed scope.

## TCP reset, receive admission and terminal errors — active scope (2026-10-28)

Continue PR182 from `8aa5999a` on unchanged main `5a94d755`. Audit reset/receive
sequence admission, duplicate/control packets and descriptor ownership after
terminal timers or TIME-WAIT pressure. Touched areas: `kernel/src/net.c`, focused
host/packet regressions, Makefile and current audit/network guides. Refreshed
PR19 is `a0f4f951`, with no net/process-core overlap; its Wi-Fi and Makefile work
remain separate. Air6,1 AP diagnostics and Air7,2 native evidence are unchanged.
Next checkpoint is original-source failure evidence, bounded corrections and
matched host/VM validation. No native install or PR merge is planned.

### Receive/terminal audit result

Corrected reset/receive admission, repeated-SYN sequence handling and loss of
referenced terminal errors/EOF under expiry or TIME-WAIT pressure. Retained
`CLOSED` state is application-only and frees on final close; diagnostic state 9
is named without changing information layout. The first candidate's SYN retry
assumption was corrected after an explicit initial transport-refusal probe.
See the [TCP audit](../tcp-lifetime-audit-2026-10-28.md).

### Receive/terminal verification

All 23 modes pass on x86_64/Rosetta and under native ARM64 ASan/UBSan. Against
original source `8aa5999a`, seven of eight added modes fail and the duplicate/
overlap control passes. The candidate initial-SYN-send-refusal probe fails before
its correction and passes in the final repeat-SYN mode. Existing TCP accept,
e1000 receive, network worker/device, process output/lifetime and syscall I/O
host gates pass. The matched build, six generator groups and recursive
372-entry image validation pass.

Seven sequential disposable VMs pass against the final source: tcp-ready 10.92 s, tcp-stress 47.35 s, netabi 10.29 s, libuv 10.13 s, threads 39.87 s, lifetime 23.86 s, outbound-local 10.57 s.
TCP stress uses twelve clients; the outbound gate checks `/receive-check` and
its exact response from a temporary localhost HTTP server. The first candidate
VM set remains separate under `vms-before-syn-retry/`. Current final logs,
original/candidate failures and tested hashes are under `build/review/tcp-receive/`.
Documentation/link and whitespace checks pass. Earlier storage matrices remain
dated evidence; no native hardware acceptance, Actions dispatch or PR merge.

Next bounded question: receive-buffer allocation failure and partial-ACK
retransmission, including overlap recovery. Full TCP conformance, identifier
exhaustion and concurrent fork/exec/kill remain outside completed coverage.

## TCP receive allocation and partial ACK — active scope (2026-10-28)

Continue PR182 from `903030ef`, integrating fetched main `7b0cc468` through
PR233 before implementation. Audit receive-allocation refusal, buffered-data/
descriptor survival, partial-ACK retransmission and overlap recovery. Touched
areas: `kernel/src/net.c`, packet/allocator host regressions, Makefile and current
network/audit guides. PR19 at `52e47a10` has no net/process-core overlap; preserve
its Wi-Fi and Makefile work. Air7,2's `0cbbb275` adds only separate parity/machine
evidence beyond main; Air6,1's AP diagnostic work is included in main and its
next coordinated C3 trial remains separately gated. Next checkpoint is bounded
reproductions, fixes and matched host/VM validation. No native install or merge.


### Allocation/partial-ACK result

Integrated main in `b36bb072`. Corrected receive-allocation lifetime loss and
inbound HTTP injection, partial-ACK outstanding-byte/retransmission accounting,
and duplicate-prefix overlap recovery. Thirty host modes retain earlier TCP
coverage. Original-source failures and candidate fixture corrections are retained
in `build/review/tcp-retry/`; see the [TCP audit](../tcp-lifetime-audit-2026-10-28.md).
Next bounded question is peer-window update ordering and zero-window recovery,
including lost reopening updates. Broader TCP conformance remains unqualified.


### Allocation/partial-ACK verification

All 30 lifetime modes pass on x86_64/Rosetta and under native ARM64 ASan/UBSan.
Eight directed new/updated cases fail against original network source `903030ef`;
close and future-ACK controls pass. Related TCP accept, e1000 receive, network
worker/device, process output/lifetime and syscall I/O host gates pass. The newly
integrated power, AP receipt/owner and lifecycle-wait gates also pass. The matched
build, six generator groups and recursive 372-entry/16-directory image check pass.

Seven sequential disposable VMs pass: tcp-ready 11.21 s, tcp-stress 47.85 s, netabi 10.21 s, libuv 10.75 s, threads 31.07 s, lifetime 22.95 s, outbound-local 10.82 s.
TCP stress uses twelve clients; outbound HTTP verifies `/retry-check` and the
exact local-server body. Logs, original failures, candidate fixture correction,
source diff and source/artifact hashes are under `build/review/tcp-retry/`.
Documentation/link and whitespace checks pass. Earlier storage fault matrices
remain dated evidence; no physical adapter/native AP acceptance, Actions dispatch
or PR merge is claimed. PR182 remains draft.


## TCP peer-window ordering and persist recovery — active scope (2026-10-28)

Continue PR182 from `1d1e106a`, integrating main `45b8647a` through PR234
(two device-evidence documents only). Audit stale window advertisements, original
segment sequence after overlap trimming, probe responses after a lost reopening
update, and responsive versus silent zero-window peers. Scope: network core,
packet/timer regressions, Makefile and current guides. PR19 `52e47a10` remains
separate Wi-Fi work. Air6,1 `c9639988` has opt-in coordinated-C3 diagnostics beyond
main; no network-core overlap and no native qualification is implied here.
Next checkpoint: original-source failures, bounded fixes and matched host/VM
validation. Keep PR182 draft; no Actions, native install or merge is planned.


### Peer-window/persist result

Integrated main in `82aecaf5`. Corrected stale window publication (including
original sequence after overlap), invalid passive-handshake mutation, ineffective
probes and premature timeout of responsive zero-window peers. Silent peer and
local close bounds remain. The 38-mode fixture includes a two-endpoint lost-update
exchange with refused probe send and sequence wrap. Evidence is under
`build/review/tcp-window/`; see the [TCP audit](../tcp-lifetime-audit-2026-10-28.md).
Next bounded question is IPv4/TCP packet-boundary validation, fragment admission
and option/MSS handling. No native C3/adapter or full TCP qualification is implied.


### Peer-window/persist verification

All 38 lifetime modes pass on x86_64/Rosetta and under native ARM64 ASan/UBSan.
Six added modes fail against original network source `1d1e106a`; silent-peer and
close-deadline controls pass. Existing TCP accept, e1000 receive, network
worker/device, process output/lifetime and syscall I/O host gates pass. The
matched build, six generator groups and recursive 372-entry/16-directory image
check pass. The integrated PR234 baseline changes only device-evidence docs.

Seven sequential disposable VMs pass: tcp-ready 11.36 s, tcp-stress 47.87 s, netabi 10.5 s, libuv 10.77 s, threads 30.99 s, lifetime 22.75 s, outbound-local 10.84 s.
Stress uses twelve clients; outbound HTTP verifies `/window-check` and the exact
local-server body. Logs, original failures, the candidate compile correction,
source diff and tested source/artifact hashes are under `build/review/tcp-window/`.
Documentation/link and whitespace checks pass. Earlier storage matrices and
native-device acceptance remain dated evidence; no Actions dispatch, physical
install or PR merge occurred. PR182 remains draft.


## IPv4 admission and TCP options/MSS — active scope (2026-10-28)

Continue PR182 from `811c4685`, integrating main `6cbfedab` through PR237.
Audit fragment admission, packet/header boundaries, TCP option lengths and
negotiated/default MSS. Scope: network core, host packet regressions, Makefile
and current network/audit docs. Main adds quiescent-I/O fairness, isolated AP C3
diagnostics and device evidence; rerun affected host checks with matched images.
PR19 `7ac6b0e6` remains separate Wi-Fi work without net.c overlap. Air7,2
`4f1ca2a1` records a bounded desktop-worker follow-up; preserve its ownership.
Next checkpoint: directed original-source failures, bounded fixes and sequential
host/VM checks. PR182 stays draft; no native install, Actions or merge is planned.


### IPv4/TCP parser result

Integrated main in `57d8f80a`. Refused fragments/reserved IPv4 flags before
transport dispatch, validated TCP option lengths before mutation, and added
negotiated/default MSS transmit caps with SYN/retry advertisement. Forty-six
modes include six original-source failures and two boundary/unknown-option
controls. Evidence is under `build/review/tcp-parser/`; see the
[TCP audit](../tcp-lifetime-audit-2026-10-28.md). The next bounded audit is UDP/ICMP
receive validation, datagram queue pressure/truncation and endpoint lifetimes.
Native-device results remain owned by their machine records.


### IPv4/TCP parser verification

All 46 lifetime modes pass on x86_64/Rosetta and under native ARM64 ASan/UBSan.
Six added modes fail on original network source `811c4685`; packet-boundary and
well-formed-option controls pass. TCP accept, e1000 receive, network worker/device,
process output/lifetime and syscall I/O host gates pass. The integrated baseline's
I/O scheduler/fairness, AP receipt/owner/C3 parser and lifecycle-wait gates pass.
The matched build, six generator groups and recursive 372-entry/16-directory
image check pass.

Eight sequential disposable VMs pass: tcp-ready 11.38 s, tcp-stress 47.84 s, netabi 10.43 s, libuv 10.84 s, threads 40.59 s, lifetime 22.72 s, web-segmented 19.34 s, outbound-local 11.1 s.
Stress uses twelve clients; web smoke includes a segmented static response;
outbound HTTP verifies `/parser-check` and the exact localhost-server body.
Logs, original failures, source diff and source/artifact hashes are retained
under `build/review/tcp-parser/`. Documentation/link and whitespace checks pass.
Earlier storage matrices and native-device acceptance remain dated evidence;
no Actions dispatch, physical install or PR merge occurred. PR182 remains draft.


## UDP/ICMP validation and endpoint ownership — active scope (2026-10-28)

Continue PR182 from `950a4ee0`, integrating main `2c34a460` through PR238
(desktop lifecycle-worker/capture changes). Audit UDP lengths/checksums, bounded
queue overflow and truncation, zero-length datagrams, ICMP echo validation, bind
identity, blocked-send/receive close/reuse and inherited endpoint ownership.
Include libc datagram recvmsg behavior if directed tests confirm a mismatch.
Scope: net.c, socket adapter/headers as needed, host/guest regressions, Makefile
and current guides. PR19 `7ac6b0e6` remains separate Wi-Fi work; main contains the
Air7,2 worker changes and Air6,1 C3 evidence. No network-core overlap was found.
Next checkpoint: original-source failures, bounded fixes and sequential matched
validation. Keep PR182 draft; no Actions, native install or merge is planned.


### UDP/ICMP and datagram result

Main is integrated in `300cbcb7`. Corrected ICMP echo checksum/code admission,
UDP ARP-wait endpoint reuse and rebinding, failed-bind cached-port publication,
recvmsg truncation/zero-capacity consumption and short source-address reporting.
Ten kernel and six libc modes include five and four original-source failures
against `950a4ee0`; all sixteen pass with x86_64/Rosetta and native ARM64
ASan/UBSan. Queue/drop, ordinary kernel truncation, blocked close/held-reference
cleanup and empty/OOM controls retain their behavior. The separate 46-mode TCP
suite plus TCP accept, network worker/device, process output/lifetime and syscall
I/O gates pass. Integrated lifecycle-wait, capture and desktop-session checks
pass with the documented Darwin feature/Clang warning accommodations. See the
[datagram audit](../datagram-audit-2026-10-28.md).

The matched build, six generator groups and recursive 372-entry/16-directory
image check pass. Eight sequential disposable VMs pass: tcp-ready 10.88 s, tcp-stress 47.35 s, netabi 9.87 s, libuv 10.18 s, threads 30.22 s, lifetime 23.09 s, web-segmented 18.72 s, outbound-local 10.6 s.
The guest confirms the new UDP netabi marker; outbound HTTP verifies
`/datagram-check` and the exact localhost-server body. Logs, original failures,
source diff and source/artifact hashes are retained in `build/review/datagram/`.
Documentation links and whitespace checks pass. PR182 remains draft; no Actions,
native install or merge occurred. PR19 `7ac6b0e6` retains independent Wi-Fi scope.
Earlier storage/native evidence remains dated separately. Continued tracing
reaches shared ARP/DNS query ownership and connected-UDP peer filtering/libc
close-reuse; directed overlap/filter tests are the next bounded checkpoint.


## Resolver and connected-UDP ownership — active scope (2026-10-28)

Continue PR182 from `c43a1749`, integrating main `4ae360ef` through PR243.
Audit shared ARP/DNS query state across blocking waits, connected UDP peer
filtering and libc receive/close/reuse and flag restoration. Scope: network
core, socket adapter, directed host/guest regressions, Makefile and current
guides. Main adds desktop pool-drain/scheduler attribution and opt-in read-only
package/platform diagnostics; rerun affected integration checks. PR19 now at
`88a2408a` remains separate Wi-Fi bring-up, without network-core overlap.
Next checkpoint: original-source failures, bounded fixes and sequential matched
validation. PR182 remains draft; no native install, Actions or merge planned.


### Resolver and connected-UDP ownership result

Integrated main `4ae360ef` through PR243 in `9f427171`, preserving both appended
Makefile test groups. ARP waits now consume only their own target's cached
mapping; unknown opcodes cannot update the cache. Native DNS holds one owner
through ARP/retries/completion, refuses overlap with `SRV_ERR_AGAIN`, checks the
configured source server, and releases on exit/failure/success. UDP receives share
connected-peer filtering and support unconnected recv; pseudo-slot generations
prevent close/reuse from redirecting retries or flag restoration. See the
[datagram audit](../datagram-audit-2026-10-28.md) for contracts and limits.

Thirteen query and twelve libc modes pass on x86_64/Rosetta and native ARM64
ASan/UBSan; eight and nine respectively fail against original source `c43a1749`.
The prior 46 TCP and sixteen datagram modes pass normally and under sanitizers.
TCP accept, network worker/device, process output/lifetime, syscall I/O and
lifecycle-wait pass. Integrated package/platform diagnostic, PCI, tool timing and
file-pool host checks pass. The matched build, six generator groups and recursive
372-entry/16-directory image check pass.

Eight sequential disposable VMs pass: tcp-ready 11.28 s, tcp-stress 47.76 s, netabi 9.95 s, libuv 10.63 s, threads 40.41 s, lifetime 23.1 s, web-segmented 19.21 s, outbound-local 10.94 s.
Netabi confirms connected filtering/unconnected receive as well as the earlier
truncation/queue group. Local outbound HTTP verifies `/query-check` and the exact
body. Original failures, final logs, candidate diff and tested source/artifact
hashes are retained under `build/review/query-ownership/`. Documentation links
and whitespace checks pass; PR182 stays draft. No Actions dispatch, native
installation or merge occurred. PR19 `88a2408a` retains independent Wi-Fi scope.
Continued tracing reaches DNS question/answer identity and compressed names,
libc resolver source/error handling, same-live-FD flag isolation and TCP
connect/accept publication after close/reuse. These need directed tests next;
full DNS/network and arbitrary SMP conformance are not claimed.


## DNS parsing and socket call ownership — active scope (2026-10-28)

Continue PR182 from `4cef0949`, integrating main `46b60b24` through PR246.
Audit native/libc DNS response identity and compressed names, per-call
nonblocking isolation and TCP connect/accept close/reuse. Scope: shared bounded
DNS parsing, network/socket adapters, a native nonblocking socket I/O entry if
needed, pipe nonblocking helpers, host/guest tests and current docs. Existing
syscall interfaces remain available; rebuild matched kernel/libc if adding the
new entry. Main adds clock-only logging and opt-in diagnostic corrections;
rerun their affected host gates. PR19 `5556641f` retains independent Wi-Fi scope.
Next checkpoint: original-source failures, focused fixes, sequential matched
verification and draft PR update. No Actions, native install or merge planned.


### DNS parsing and socket call ownership result

Main `46b60b24` through PR246 is integrated in `39210341`. Shared DNS parsing now
validates question/response identity, complete packet extents, bounded compressed
names and relevant A/CNAME answers. Libc retains immutable retry bytes, filters
the configured UDP peer, rejects truncation and maps native resolver contention
to EAI_AGAIN. MSG_DONTWAIT uses new per-call syscall 155 without shared flag
mutation, including UDP sends and pipe-backed socketpairs. Connect/accept
revalidate pseudo generations after waits/restarts; native accept retains a
CLOEXEC listener duplicate and captured options. See the
[DNS/socket report](../dns-socket-audit-2026-10-28.md) for limits and ABI details.

Forty-six parser cases, seventeen libc modes and the syscall dispatch matrix
pass on x86_64/Rosetta. The parser/libc and prior UDP/query matrices pass native
ARM64 ASan/UBSan; syscall dispatch uses x86_64 sanitizers for its inline assembly.
Original `4cef0949` fails fifteen kernel-parser and seventeen libc-parser modes;
fifteen of the seventeen initial socket modes differ, including the new
raw-accept descriptor-pressure contract. Stream modes additionally verify signal
checkpoints and EPIPE preservation. The prior 46 TCP modes pass normally and
under sanitizers. Pipe pressure/flags, TCP accept, network worker/device, process
output/lifetime, syscall I/O and lifecycle-wait pass. Integrated package/platform,
clock/clock-app, workspace-log and PCI gates pass. Matched build, six generator
groups and recursive validation of 372 entries in 16 directories pass.

Eight sequential disposable VMs pass: tcp-ready 10.88 s, tcp-stress 47.74 s, netabi 10.32 s, libuv 10.21 s, threads 40.5 s, lifetime 23.54 s, web-segmented 18.84 s, outbound-local 10.51 s.
Netabi confirms signal delivery, per-call flags/alias/pressure/recovery and both
earlier UDP groups. Local outbound HTTP verifies `/dns-socket-check` and the
exact `srvros-local-outbound-dns-socket` body. Resolver malformed-packet and
close/reuse schedules remain deterministic host evidence, not VM concurrency
claims.

Final source and image hashes, original failures and logs are retained under
`build/review/dns-socket/`; intermediate candidate runs are retained separately
from the final image. PR182 remains draft. No native install,
Actions dispatch or merge occurred. PR19 `5556641f` remains independent Wi-Fi
work. Continued tracing reaches resolver retry/deadline policy, duplicated
pseudo-socket peer/options state and remaining native-FD I/O continuations;
these need separate directed coverage before claiming a broader socket profile.


## Modular desktop overhaul — 2026-11-02

Scope: `overhaul/modular-desktop` from fetched main `74d38d59`; split native
widgets, desktop applications and ports into pinned repositories, update build
references, then shared widget and compositor appearance. PR19 retains unrelated
USB Wi-Fi work; no other open PR was found at inspection. This host runs local
builds and sequential disposable QEMU checks only. No physical boot, Actions
dispatch or device-acceptance expansion is planned. Next checkpoint is a matched
submodule build and recursive clone before theme implementation.

### Modular desktop verification result

Runtime source `e65e5938`; SrvrUI `4fdf91a`, SrvrApps `d386386`, SrvrPorts
`0494ae2`. Public repository creation, recursive clone and clean full build pass.
All upstream pins and port source hashes are retained. Full desktop/widget/I/O
host targets pass with the Darwin wrapper, plus theme ASan/UBSan, Python host
and boundary checks. The application workflow and compositor VMs pass, including
file readback and abrupt-owner console recovery. Final 1440×900 desktop and
widget-state images were inspected; disposable VMs stopped.

Evidence and limitations: [repository milestone](../repository-organization.md),
`build/review/modular-desktop/`, `build/review/desktop-apps/` and
`build/review/desktop-1280x800/`. This supersedes no physical acceptance. OS branch
awaits integration review; no main merge or Actions dispatch occurred. Next:
review the published submodule integration; application/port work can branch
from these documented boundaries.


## Glass and global appearance — 2026-11-02

Continuing `overhaul/modular-desktop` from completed `3b286e0`, main `74d38d59`.
Scope: SrvrUI resolver/glass primitives, compositor chrome and ordered presentation,
shared preference protocol/runtime, SrvrApps Appearance controls and live updates.
Required evidence is matched host/build and sequential VM checks with real images.
PR280 owns this work. PR278 `f11cbf8` overlaps desktop/application dispatch files
for Open With and descriptors; preserve that separate implementation at integration.
PR252 `badfeb3` owns process/I/O kernel audit work; this milestone changes no kernel
or framebuffer driver. PR19 retains USB Wi-Fi work. No physical trial or Actions.
Next checkpoint: host regression suite and live theme/persistence VM checks.


### Glass/theme verification result

Runtime `db89892768c34dd7c79e9b37ad1f8207806fe43d`; published SrvrUI `df2e311`,
SrvrApps `e4b9235`, unchanged SrvrPorts `0494ae2`. Matched full image build and
complete desktop/widget/I/O host targets pass. Native ARM64 ASan/UBSan theme
blending/clipping passes. Dynamic preserved-buffer old/new window drag and lower
client-content changes match fresh full rendering for all three glass levels.
Opaque-body culling matches the unculled oracle, including randomized overlap,
small/stale caches and 128-window stress. Composition completes before any upload.

Final-source application VM verifies existing/new Text selection colors, saved
Teal/Clear appearance and a new desktop process loading it, alongside file and
clipboard workflows. Final compositor VM verifies drag, resize, scrolling/capture,
client death and console recovery. The separate Settings VM passes clock,
wallpaper, pointer controls and config reload; it ran before the final title-frost
visual adjustment, which is covered by final host/app/compositor checks.

Evidence: `build/review/theme-global-*.log`, matched
`build/review/theme-global-manifest.json`, `build/review/desktop-apps/`,
`build/review/desktop-settings/`, `build/review/desktop-1280x800/`, and tracked
[images/provenance](../assets/screenshots/glass-manifest.json). All disposable VMs
stopped. Theme CPU stress cost and lack of physical tear-free evidence are explicit
in [the theme contract](../desktop-theme.md) and [driver plan](../display-presentation.md).
No native install, Actions dispatch or main merge. Next: PR280 integration review;
no further work is required for this scoped theme/presentation investigation.


## QEMU display transport probe — active scope 2026-11-03

Branch `device/apple-silicon/virtio-display-probe`, stacked on completed UI
`0b6b5e53`; fetched main remains `74d38d59`. The user requested proceeding toward
the remaining display work. QEMU is the initial target in this workspace.
Scope: boot-only opt-in VirtIO GPU probe on a separate modern PCI device, bounded
capability/queue/fence validation and safe reset/DMA retirement. The inherited
standard VGA display remains the boot/desktop owner; there is no scanout handoff.

Inspecting PR278 `f11cbf8`, PR252 `badfeb3`, and PR19 `71e7d4c` found separate
app-dispatch, kernel/process/I/O, and Wi-Fi work. This implementation uses new
probe files and a small explicit main boot-token hook; it does not alter syscall,
process lifetime, native Intel or firmware ownership policy. Main integration
must preserve other opt-in captures. No physical trial, Actions or main merge.

Implementation prerequisites: require a unique modern non-VGA VirtIO GPU and
exact inherited framebuffer base match to standard VGA; use validated PCI_CFG
access to confirm inactive device status before BAR sizing/ownership. The probe
runs once immediately after PCI initialization in kernel CR3, maps/unmaps only
that address space, and never consumes the kernel mapping registry (which lacks
a capacity/retirement API). Require reset completion before freeing DMA; uncertain
retirement retains allocations and reports failure. Next checkpoint: host fake
hardware tests, matched build and sequential positive/absent/disabled VM boots.


### Confirmed presentation scope

The user explicitly selected QEMU-first VirtIO presentation. Continue through an
opt-in primary `virtio-vga` backend with two resources, fenced transfer/scanout,
a desktop-owned presentation lease and console recovery. Additive graphics
syscalls and console retirement hooks are now included; preserve PR252's existing
copy bounds/checkpoint and process cleanup behavior. No process lifetime rewrite
or native modeset is included. The probe is an intermediate checkpoint, not the
completed user target. All three probe VM cases passed with a live shell and
nonblank inherited display; local evidence is `build/review/virtio-gpu-probe/`.


### QEMU presentation verified and published

Runtime `a8c21344367883edd07f5ee051caa9edc6cd8308` is locally verified and published
in draft [PR281](https://github.com/PaulMDemers/SrvrOS/pull/281), based on PR280's
UI branch. The final matched build and kernel/framebuffer image hashes are in
[the presentation manifest](../assets/screenshots/virtio-manifest.json). QEMU
11.1.2 primary scanout passes glass desktop/drag, scrolling, normal console
recovery, abrupt owner termination and reacquisition. The separate transport
harness passes all three opt-in/absent/disabled cases, including a standard VGA
desktop fallback. Final VM runs identify clean runtime `a8c21344`.

Transport/lease, PCI, console, graphics copy/service, I/O scheduler, syscall
cancellation and full desktop/widget host regressions pass. Driver/lease
ASan/UBSan pass. The final review fixes bound partial-copy service intervals and
block staging writes immediately when a failed frame cannot prove reset/restore.
Details and inspected screenshot are in [display presentation](../display-presentation.md).
All test VMs stopped; working runtime is complete for this opt-in QEMU target.
Next is review/integration after PR280, preserving PR252's independent lifetime
work. No main merge, hosted Actions, native modeset or physical acceptance.


### Desktop integration completed — 2026-11-03

The user authorized both merges. PR280 merged as `624e5e8f`; PR281 was retargeted
from the UI branch to `main` and merged as `1c0896e7`. The combined tree exactly
matches tested head `29bd1986`; all SrvrUI/SrvrApps/SrvrPorts pins remain the
published remote main heads. A matched build and the primary VirtIO presentation
VM pass on clean merged runtime `1c0896e7`, including drag, scroll, normal console
recovery, forced desktop exit and restart. Raw post-merge results are in
`build/review/desktop-merge/` and `build/review/virtio-gpu-present/`.

The workspace is being returned to current `main`; no test VM remains running.
Actions and native acceptance remain unchanged. The repository/theme and opt-in
presentation milestones are delivered. Next is an attended visible comparison
of VGA and VirtIO dragging/scrolling, with responsiveness measurements and no
assumption that command fences establish host-monitor vblank.


### VGA / VirtIO smoothness comparison — active scope 2026-11-03

The user authorized the next comparison after both milestones merged. Base is
`bfb6d2a7`; inspected PR278/252/19 retain their separate application/lifetime/Wi-Fi
scopes. This block owns a disposable sequential QEMU comparison harness and
measurement/reporting only. Hold resolution, scale, CPU, memory, source image and
input workload constant while changing the display backend. Reuse the existing
software cursor/frame timers, require actual VirtIO acquisition, and distinguish
software/fenced-completion timing from Cocoa/physical presentation. Inspect the
visible window separately. No native driver change or default-backend change is
accepted without evidence; no physical boot or Actions run is planned.


### Smoothness comparison completed

The user observed smooth dragging with no obvious tearing under VirtIO, followed
by visibly worse dragging under VGA in the 1440×900 Cocoa desktop. Six automated
runs (three per backend, alternating order; two fresh sessions per run) pass
verified 300-pixel drags, reversing scrollbar movement and normal teardown.
Median per-run frame-work means: VGA/VirtIO drag 7.953/14.354 ms; scroll
6.995/16.619 ms. Final scroll pixels match below the menu bar. These whole-session
software samples include startup/menu work and do not measure physical scanout.
See the [report](../display-presentation.md#visible-smoothness-comparison--november-3-2026)
and [manifest](../assets/screenshots/smoothness-manifest.json). No runtime/default
backend changes were made. The full-frame copy cost is an optional optimization
candidate. Automated VMs are stopped; the attended VirtIO desktop is restored for
continued user review using `.local/smoothness-visible-virtio/`.


### VirtIO QEMU baseline — active scope 2026-11-03

The user authorized making VirtIO presentation the QEMU default, reducing its
full-frame copy cost while preserving fenced complete frames, then implementing
input, RNG, block storage and networking. Base is measurement source `d643d2b8`
(PR283) over merged `bfb6d2a7`. PR252 retains independent process/I/O lifetime work,
PR278 app dispatch and PR19 Wi-Fi. This branch owns additive modern VirtIO drivers,
QEMU launcher defaults, GPU dirty-region ownership and focused host/VM evidence.
Keep legacy/native launch paths available; do not alter physical acceptance or
boot/limine.conf/native USB capture policy. Stage and run one matched VM at a time.

Acceptance: damage-aware alternating GPU resources equal full reference frames
and improve measured copy/transfer cost; QEMU defaults exercise the backend with
explicit VGA fallback; VirtIO input delivers keys/releases, motion/buttons/wheel;
RNG, block and network integrate with their existing kernel interfaces and pass
bounded malformed/failure/reset tests plus disposable QEMU positive/fallback runs.
Shared transport requires strict PCI/feature/queue validation and reset-before-free.
Device defaults advance only with their matching end-to-end evidence.

### VirtIO baseline accepted — November 3, 2026

Runtime `986c16e82f19f4da518190b092d84b27df0dc0bf` is clean and published in
[PR284](https://github.com/PaulMDemers/SrvrOS/pull/284), based on measurement PR283.
Its matched rebuild reproduces the accepted kernel SHA-256
`f32535f07875d7e951ec2e0d87613d7ce4008eeea311b832957f74f4e51b486d` exactly.
Submodules remain on their previous published pins; native boot settings and
physical acceptance are unchanged. Actions were not enabled or dispatched.

Verified: protocol host tests and sanitizers; keyboard/mouse, storage, network
provider/worker, graphics/lease/console, scheduler/cancellation and full desktop
host suites; docs checks; three repeated Cocoa damage workloads; two-boot modern
VirtIO VM with PS/2 disabled; explicit legacy launcher; default Cocoa launcher.
The modern VM types in the monitor and Text, saves a document, drags a window,
scrolls Help by a real wheel event, consumes entropy, changes link state,
completes DHCP and 4/4 pings, releases the presenter, flushes/reboots and reads
back its persistent marker. Observed firmware block admission is 15 with reset
proof. No device failures/quarantines occurred in the accepted run.

Copy/transfer volume falls 64–67% in the repeated drag/scroll workloads, while
median draw/present time falls 27–33%; host vblank remains unmeasured. The default
launcher uses VirtIO graphics/input/RNG/block/network; explicit legacy fallback
remains available. See [the driver contract](../virtio-baseline.md),
[measurement receipt](../assets/screenshots/virtio-damage-manifest.json) and
[acceptance receipt](../assets/screenshots/virtio-baseline-manifest.json).

All automated verification VMs are stopped. One attended default Cocoa desktop
from this clean runtime is left open at `.local/macos-vm/virtio-baseline-review/`;
its `current.json` identifies the owned process, disk, sockets and logs. Exit via
SrvrOS → Return to console before closing QEMU. Required work for this baseline
is complete; interrupt-driven queues, tablets and offloads remain optional.

### Active tablet scope — November 3, 2026

User authorized VirtIO tablet support, QEMU default selection, verification of
automatic cursor release, and merge. Branch `device/apple-silicon/virtio-tablet`
is based on freshly fetched main `f3f9a3c2`. Owns the input tablet profile,
normalized absolute pointer queue/ABI semantics, desktop pointer consumers,
QEMU launch defaults and focused tests. PR252 retains process/I/O lifetime work,
PR278 application dispatch, PR19 Wi-Fi; no overlapping driver work was found.

Acceptance: exact endpoints and scaling at supported desktop sizes; no jump on
first full sample or re-entry; ordinary relative devices still work; press/drag/
release and wheel preserved; automatic release on leaving the Cocoa VM window
and re-entry without a release shortcut; clean matched build, protocol/queue/
consumer regressions and VM evidence. Publish and merge after these checks.
Native boot settings, physical acceptance and repository boundaries are unchanged.

### Tablet verification — November 3, 2026

Clean runtime `0779dd72` built successfully and passed input, pointer/ABI,
keyboard and desktop host tests, launcher unit tests, both absolute-tablet VM
modes (1280×800 scale 1 and 1920×1080 scale 2), and the original two-boot relative
VirtIO baseline. All runs match kernel/exFAT hashes in the
[tablet receipt](../assets/screenshots/virtio-tablet-manifest.json).

The default Cocoa desktop is open in `.local/macos-vm/virtio-tablet-review/` and
reports the tablet as its active absolute pointer. First app frame: 4.32 seconds.
The user confirmed automatic host cursor release/re-entry without a shortcut and
outside-release/re-entry without a stuck drag, completing acceptance for PR285.
The former VM was exited cleanly,
with its unsaved blank Text document saved as `/fat/tablet-restart-recovery.txt`
on the retained `.local/macos-vm/virtio-baseline-review/` disk. Submodule pins and
native hardware acceptance are unchanged.

### Active theme profiles — November 3, 2026

User authorized complete Aero-inspired and Aqua-inspired presets, live Appearance
previews, application-wide switching/persistence, and rendering/performance
verification. Branch `device/apple-silicon/theme-profiles` starts at merged tablet
PR285 (`087029b4`). Owns SrvrUI style rendering, SrvrApps Settings, the shared
preference protocol/resolver, compositor chrome and focused tests. PR278 owns app
dispatch, PR252 process/I/O lifetimes, PR19 Wi-Fi; those scopes remain separate.
Retain Silver as the compatible default, accent/glass controls, content state,
input geometry and current device profiles. Extend the existing authoritative
preference path rather than introducing per-app stores. One shared build/VM owner;
publish child repository commits before OS pins. Native acceptance is unchanged.

### Theme profiles accepted — November 3, 2026

Clean runtime `50389359` pins published SrvrUI `89fb6d9` and SrvrApps `d245fe2`.
Full image build and desktop/widget host suite pass, including style migration,
live window/dialog updates and clipped/full redraw equality during drag/scroll.
Both profile and existing Settings/application VMs pass; the compact 280×240
Appearance fix has a focused host test and inspected render. The final matched
profile VM preserves selected Text content, updates existing/new windows, renders
dialogs and menus, drags/scrolls, saves Aqua and restores it after desktop restart.
See the [receipt](../assets/screenshots/theme-profiles-manifest.json) and
[rendered examples](../desktop-theme.md#profile-acceptance--november-3-2026).

The controlled host frame-paint medians are 334/334/283 µs for Silver/Aero/Aqua;
title-damage costs are 121/121/70 µs and cursor costs 4/4/4 µs. No host-vblank or
native-device claim is made. The previous tablet VM and its idle Terminal exited
cleanly, retaining its disk. The new default Cocoa VM is open in
`.local/macos-vm/theme-profiles-review/`, reached its first app frame in 4.37 s,
and shows Appearance with Aero selected. Integration is PR286 with SrvrUI PR1
and SrvrApps PR1; subsequent acceptance commits change documentation only.
No further feature is required for the approved profiles milestone.


### Files recovery — active isolated scope, November 3, 2026

User authorized reproducing and fixing Files mutation/refresh recovery in
`SrvrOS-app-quality`, an independent recursive checkout on `app/files-recovery`
from fetched main `e376669c`. The shared integration checkout and attended review
VM are untouched. This block owns SrvrApps `desktools/files_app.c` and focused OS
host/VM regressions; no widget, ABI, filesystem or worker-lifetime redesign.
PR278 and SrvrApps PR2 retain Open With ownership; their Files/test overlap must
be reconciled before integration. PR252 retains process/I/O, PR19 USB Wi-Fi.

Acceptance: reproduce stale selection and success-notice carryover; keep failed
mutation state from later refresh/navigation; accurately report successful
mutation plus failed refresh; preserve selection/history, keyboard/pointer,
independent windows, Text save/readback and live Silver/Aero/Aqua themes. Run
focused and desktop/widget/IO host checks, matched build and sequential disposable
VM checks in this checkout only. No hosted Actions or native acceptance.
Next checkpoint: failing focused regressions before runtime edits.


### Files recovery verified — November 3, 2026

SrvrApps `3aea2a95` fixes stale selection and success notices after failed Files
operations, tested against OS base `e376669c`, SrvrUI `89fb6d9` and SrvrPorts
`0494ae2`. All seven upstream pins remain unchanged. Failure cleanup is local to
Files: it retires the pending destination/notice, retains the current folder and
history, and distinguishes a completed mutation from its failed refresh. No shared
control, transport, filesystem, ABI or process-lifetime behavior changed.

Both defects reproduced against original SrvrApps `d245fe2`: the focused
`files-host selection` invocation aborted on a wrong selected inode after a
rename collision/refresh; `files-host notice` aborted when Copy verified leaked
into another folder after refresh start refusal. These negative controls are
retained in `build/review/files-recovery/baseline-failures.json`. The fixed tests
cover rename/copy collisions followed by refresh/navigation, refresh start
refusal, I/O failure, invalid listing, worker loss, original selection/history
retention and compound success/refresh-failure feedback.

Verification in the independent `SrvrOS-app-quality` checkout:

- Focused Files regression and ASan/UBSan app fixture: pass.
- Documented macOS `test-desktop test-desktop-io widget-theme-preview`: pass,
  110 PASS records. Added worker-loss cases also pass the focused rerun.
- Full matched image build: pass; recursive revisions/diff hashes are in
  `build/macos-build.json`.
- `TMPDIR=/tmp python3 tools/desktop_files_smoke.py`: pass, including actual
  Rename and Duplicate collision/refresh followed by a Rename prompt proving
  the original source remains selected, cancellation, history and saved bytes.
- `TMPDIR=/tmp python3 tools/desktop_apps_smoke.py`: pass, including Open/Save/
  Save As, replacement cancel/verified overwrite, unsaved cancel, clipboard/cut/
  undo, empty save, document routing, text-size/theme broadcasts and window menus.
- `TMPDIR=/tmp python3 tools/desktop_theme_profiles_smoke.py`: pass with VirtIO
  tablet input, one Files process with independent sibling history/close,
  selected filename retained across live Silver/Aero/Aqua and exact 722/600-pixel
  outer-width resizes, plus existing Text/theme/restart/readback checks. All six
  Files captures and the shared control specimen were visually inspected.

The first theme run also passed; its root-directory selection was offscreen in
the captures. That receipt remains under `build/review/files-recovery/themes-first-pass/`.
The final fixture saves in Home so the selected row is visible at both sizes.
Rare refresh/worker failures use deterministic host injection; they are not
claimed as device-fault VM trials. The Linux-only multi-Files exfatprogs harness
was not run on macOS; the focused sibling/history check above covers this scoped
workflow. No native acceptance or hosted Actions run is implied.

Evidence is under `build/review/files-recovery/`, `build/review/desktop-files/`,
`build/review/desktop-apps/` and `build/review/desktop-theme-profiles/`. All disposable
VMs exited; the shared integration checkout and its attended review VM were not
modified. Implementation and acceptance are complete for this milestone. Next:
integration review, reconciling the Files/test overlap with PR278/SrvrApps PR2;
no merge or broader app-suite work is included.


### Files recovery / Open With integration — active November 3, 2026

The user authorized reviewing and reconciling Apps PR3 / OS PR288 with Open With
without accessing the shared checkout or attended VM. All work remains in the
independent `SrvrOS-app-quality` checkout. Fetched main is OS `adc25965`, Apps
`d54b6cf`; inspected Open With is Apps `ff6c8dec` / OS `7bfa0e47`. Merge those
branches into the owned recovery branches without rewriting published history;
stack the recovery reviews on their Open With dependencies so the final review
diff stays bounded. Preserve PR278's SDK, runtime consumers and original tests.

App hunks merge cleanly; resolve the OS app gitlink to the combined app commit
and retain both Files fixture controls (failure injection and expected launch
app/path). Add explicit collision/refresh/navigation-to-Open-With checks. Verify
the combined host suites, build and owned/refused Open With consumer workflows,
including unsaved Text retention and file readback. A clearly labeled generated
disk fixture mode will permit the existing consumer harness on macOS without
claiming Linux exfatprogs validation. No main merge, hosted Actions, hardware
trial, or PR252 process/I/O / PR19 Wi-Fi work is included.

### Files recovery / Open With integration verified — November 3, 2026

Reconciled the recovery branches with Apps PR2 `efd9dba3`, OS PR278 `1454f9e5`
and main's merged SrvrUI renderer extraction (`5b2bf3c7`). The combined published
app is `4f2f3425`, UI is `76deeb30`, and Ports remains `0494ae2`; all seven upstream
pins are unchanged. Relative to the Open With app branch, only the 18-addition /
6-deletion Files recovery fix remains. Preserve both the failure-injection and
expected app/path fixture controls, the combined app gitlink, and both Makefile
rules: QMP input validation and canonical renderer dependencies. No Open With
SDK, runtime consumer, or existing assertion was removed.

The combined checks prove collision/refresh/navigation recovery followed by
Open With cancellation and captured-source routing, plus successful-copy
selection. The Files VM opens the recovered source in Text, saves it, and checks
both files byte for byte. Owned and refused-dialog lifecycle VMs retain unsaved
siblings, reuse existing Text/Preview processes, exercise default and explicit
handlers with keyboard/pointer input, verify exact file contents and restore the
process baseline. An explicit `--generated-fixture` mode preserves the complete
matched disk and checks generator metadata and offline allocation before/after;
the default Linux exfatprogs fixture remains unchanged. Independent exfatprogs
fsck was not run on this macOS host.

Validation in `SrvrOS-app-quality`:

- Original negative controls remain in `build/review/files-recovery/`.
- Desktop/widget/I/O plus QMP input checks: 112 PASS records; focused Files
  ASan/UBSan, matched full build, Python syntax and documentation checks pass.
- Clean `ebbe54e8` passes the owned Open With, refused-dialog Open With and
  Files action/recovery VMs sequentially with the above app/UI/Ports tuple.
- The first theme run exposed interleaved serial logging: the sibling closed,
  but compositor trace lines split its closure message. The failure log and
  screenshot remain in `build/review/files-open-with/theme-interleaved-failure/`.
  Reuse the existing `desktop_progress_end` matcher, preserving exact application
  bytes and raw offsets. Its four tests and replay of the captured failure pass.
- Clean `2fad3a65` passes the theme VM after that harness-only correction. All
  three kernel/initramfs/disk hashes are identical to the other three VM runs.
  Sibling Files history/close, selected filename, live Silver/Aero/Aqua, exact
  722/600-pixel outer widths, Text, restart and readback remain covered. Both
  chooser forms and all six Files profile/size captures were inspected.

The latest PR278 merge only updated its physical-trial record because its Help
app pin was already an ancestor of the combined app. Main `a89632eb` subsequently
adds Air6,1 design documentation only. Final clean-build identity and matching
runtime hashes are recorded in `build/review/files-open-with/receipt.json`;
the earlier integration round remains under `pre-main-refresh/`. All disposable
VMs stopped. The shared checkout and attended review VM were untouched. No hosted
Actions, native acceptance or main merge is claimed.

Integration preparation is complete for [Apps PR3](https://github.com/PaulMDemers/SrvrApps/pull/3)
and [OS PR288](https://github.com/PaulMDemers/SrvrOS/pull/288). This supersedes the
initial plan to stack both reviews: Apps PR3 targets the Apps PR2 branch for a
recovery-only diff; OS PR288 stays on main because main already contains the
renderer extraction. Merge the Open With pair (Apps PR2 then OS PR278) first;
retarget Apps PR3 to main and merge it before OS PR288. Do not merge recovery into
the Open With feature branch. The child dependency was published before the OS
pin. The reviews are ready for integration review, not merged by this task.


### Edge and corner resizing — active scope November 3, 2026

User authorized implementation, verification and merge. Branch
`device/apple-silicon/edge-resize` starts from fetched main `5b2bf3c7`, including
merged SrvrUI host/render separation PR289. This block owns compositor resize
hit testing, anchored geometry, directional software cursors and focused
host/VM verification. SrvrUI/SrvrApps/ports interfaces and pins are unchanged.
PR278 retains application registry/dispatch work in deskd; PR252 owns lifetime
work. PR288 Files recovery and PR291 awk retain their separate scopes.

Interaction: hover identifies the available resize direction; pressing an edge
or corner does not jump the window; dragging keeps the opposite edge fixed;
release ends capture. Minimum/client maximum bounds and reachable titlebars
remain enforced. Modal parents, maximized windows, overlays and application
controls must not accidentally start resizing. Failed reservations or application
refusal preserve usable prior geometry/content. Cursor changes must invalidate
both old and new pixels, including changes without pointer movement.

Acceptance: geometry and failure/lifetime host regressions, exact partial/full
render comparisons for every cursor, full desktop suite, matched OS build, and
actual Text/tablet resize checks at 1x and 2x with content retained and clean GPU
retirement. QMP tests do not establish host-monitor vblank or physical input
acceptance. The existing attended review VM/disk is preserved; disposable test
VMs run sequentially. Hosted Actions remain disabled. Next: implement and inspect
these transitions, verify the exact candidate, then publish and merge its PR.

### Pinned awk port — active isolated scope, November 3, 2026

User authorized a separate-checkout awk milestone. Base `e376669c`, branch
`ports/awk`, checkout `SrvrOS-ports-awk`; SrvrPorts branch `port/awk` starts at
`0494ae2`. Independent Git/submodule directories, build caches, staging and VM
disks preserve the integration checkout and attended review VM. This scope owns
the pinned One True Awk adapter, image wiring and focused host/guest tests.
PR278 remains application descriptors/Open With; PR252 remains process and I/O
lifetimes, PR19 USB Wi-Fi. The integration chat has been notified. Any shared
libc/kernel defect requires coordination before implementation.

Target: upstream awk `5739fd79bcfc75ba7526773d0cf634521f8aca3c` (20260426)
processes saved CSV in Terminal, writes a report and preserves exact output
after guest disk reopen. Acceptance includes selected unchanged upstream cases,
errors/status, pipe completion, two clean reproducible executable builds, matched
image provenance and existing ports regression. C-locale/ASCII is the initial
case-conversion contract; no full Unicode locale, POSIX certification or native
hardware claim. Keep upstream clean and adaptations under `ports/srvros/awk`.
No hosted Actions or physical boot is authorized by this scope.


### Awk milestone verified — November 3, 2026

Implementation `f563020a` pins published SrvrPorts `5e30ecd`. Two clean full builds
(after removing generated port objects/sources as well as `make clean`) pass.
Both produce awk ELF SHA-256
`4823d7df393bb56797db40f4b2d4f01d7d37ea2201bb26766cb6fda7e0be5963`
and identical preparation manifests/generated text. Kernel and exFAT hashes also
match; initramfs archives do not. This is executable/source reproducibility,
not a deterministic-whole-image claim. Bison 2.3 and the native host compiler
only produce build-time parser tables; target code uses pinned Zig 0.15.2.

The final matched guest run passes real Terminal quoted/multiline CSV reporting,
fields/arrays/regex/substitution/math/random, stdin pipes, subprocess/status,
three unchanged upstream regression programs, error/nonzero status and recovery,
process-baseline restoration, exact offline contents, and second-boot readback.
Guest fsck and offline allocation checks pass on both boots; external fsck.exfat
is unavailable on this host. Existing staged-ports smoke also passes. Host tests
compare two fresh preparations and reject modified source. Docs and whitespace
checks pass. No full upstream-suite or native acceptance is claimed.

The initial fixture run failed because shell quoted-token parsing consumed
backslashes in printf input; all other files/statuses matched. Its disk/logs are
preserved. The corrected fixture pipes a staged file and passes. This shell issue
was reported separately to integration/PR252 ownership and is not changed here.
The first host allocation-check build used a Linux linker flag; rerunning with
the documented Darwin wrapper passes. Failures were not relabeled as passes.

Raw evidence: `build/review/awk/`, final guest `20261103-145438-ov9g6a_o`.
[Portable hashes and screenshot](../assets/screenshots/awk-manifest.json) retain
source identities and limits. All task-owned VMs stopped; integration checkout
and attended review VM untouched. Main advanced to `105077d7` with Air6,1 HDA
work; read-only merge-tree validation reports no conflict, but the tested runtime
remains based on `e376669c`. Delivery is linked draft PRs, beginning with
[SrvrPorts PR1](https://github.com/PaulMDemers/SrvrPorts/pull/1); merge acceptance
remains with integration. No merge or Actions run.


### Awk draft integration reconciled — November 3, 2026

[SrvrOS draft291](https://github.com/PaulMDemers/SrvrOS/pull/291) and
[SrvrPorts draft1](https://github.com/PaulMDemers/SrvrPorts/pull/1) are published.
Main advanced again to `5b2bf3c7` (SrvrUI host extraction). Merge `6d1ae527`
resolves the sole Makefile EOF conflict by retaining both the awk include and
SrvrUI renderer dependencies; it consumes published SrvrUI `76deeb3`. No separate
libc/kernel fix was introduced. The final diff against main is the awk integration.

Two new clean full builds on merged runtime `6d1ae527` pass with identical
generated sources/tool manifests and debug awk ELF SHA-256
`4c51140ea29050172beb4b68a0564a7c1d2442efa5fee3f04df75361749db430`.
The packed runtime awk remains byte-identical to the pre-merge guest-tested one;
the debug ELF reflects relocated renderer source metadata. Kernel and exFAT
images match across these two builds; initramfs archives still differ. Final
matched Terminal, upstream/profile, errors, retirement, offline allocation and
second-boot persistence all pass again; staged-ports smoke passes again.
Integrated guest evidence is `20261103-150115-46mpl5g7`; earlier runs remain retained.
The portable manifest and screenshot now identify this merged runtime. Follow-up
commits change evidence/docs only. Task-owned VMs stopped; review VM untouched.


### Files integration delivery — active November 3, 2026

The user authorized merging the documented dependency sequence: Apps PR2,
OS PR278, retarget Apps PR3 to main and merge it, then OS PR288. Work and all
verification remain in the independent `SrvrOS-app-quality` checkout; the shared
checkout and review VM must remain untouched. Main `c609207b` adds the already
accepted awk integration. Resolve the Makefile tail by retaining QMP input
validation, the SrvrPorts awk include and canonical SrvrUI renderer dependencies;
preserve both host journal entries and main's Ports pin. The merged Open With
prerequisite will receive host/build checks before publication, and the final
merged Files candidate will receive matched host/build and sequential disposable
VM checks. Preserve source-specific evidence; do not enable hosted Actions or
claim native acceptance. No unrelated feature work is included.


### Edge resize verification — November 3, 2026

Runtime `bb7f9b17` passes the full desktop and desktop-I/O host suites, including
anchored geometry, all cursor partial/full redraw states, resize refusal and
capture invalidation. Clean candidate `d0053c89` differs only in harness/docs
and passes the matched build, actual Text/VirtIO tablet runs at 1280×800 scale 1
and 1920×1080 scale 2, plus the existing live theme/profile regression. All eight
edges/corners, no-jump presses, minimums, selection/saved bytes, Escape, modal
blocking (including opening a picker while held), edge release/re-entry and clean
GPU reset pass. Directional cursor and minimum-size screenshots were inspected.

Independent review found and resolved a queued-motion/modal race before final
verification: capture eligibility is checked before each resize, and after the
input batch. Initial harness failures are retained in ignored evidence: a modal
probe hit the dialog, a requested 2560×1600 mode fell back, and scale-2 outward
travel overlapped the dock. Final fixtures target the exposed parent, use the
established display mode, and leave room above the dock; no acceptance was waived.

[Portable receipt](../assets/screenshots/edge-resize-manifest.json) and screenshots
retain source/hash evidence. Raw results are under `build/review/edge-resize`,
`build/review/desktop-resize` and `build/review/desktop-theme-profiles`. Test VMs
are stopped. Merged awk PR291 is now included, retaining both independent scope
records and the published ports pin. The final combined source `84478be4` builds cleanly; kernel and compositor ELF
hashes equal the accepted runtime. Its default Cocoa desktop opens in 4.32 s
and commits a 40×25 corner enlargement to a 762×550 frame. This review VM is
left open at `.local/macos-vm/edge-resize-review/current.json`; the prior review
VM had already stopped and its persistent disk is retained. Required verification
is complete for [PR292](https://github.com/PaulMDemers/SrvrOS/pull/292). Native
acceptance and Actions are unchanged.


### Files integration sequence — November 3, 2026

Apps PR2 merged as `45cbd585`; OS PR278 merged as `c0fa8d90` after preserving
main's renderer/awk rules and edge-resize integration. Both intermediate matched
host/build candidates passed. Apps PR3 was then retargeted to main and merged as
`ae56bf71`; its tree exactly matches reviewed `4f2f3425`. OS PR288 now pins that
merged app and incorporates current main, preserving the Files regressions and
all host journal entries. The theme fixture uses the new outer right-edge halo
and anchored pointer delta, retaining exact 722/600-pixel width assertions.

Next: merge OS PR288 after host/build checks, then verify the actual merged main
with host/sanitizer checks and sequential owned/refused Open With, Files, theme
and awk VMs. Evidence belongs in `build/review/files-merged/`. Shared checkout
and review VM remain untouched; native acceptance and hosted Actions unchanged.

### Files integration delivered and verified — November 3, 2026

The authorized dependency sequence is merged: Apps PR2 `45cbd585` at 20:16 UTC,
OS PR278 `c0fa8d90` at 20:21, Apps PR3 `ae56bf71` at 20:21, then OS PR288
`eaf11488` at 20:23. The actual clean OS merge `eaf11488` was checked out in
`SrvrOS-app-quality` and tested with Apps `ae56bf71`, UI `76deeb30`, Ports
`5e30ecd3` and upstream awk `5739fd79`; the other seven upstream pins are
unchanged. Main's awk and edge-resize work is retained. The merged tree and its
kernel/initramfs/disk hashes match the prepared integration candidate.

Fresh verification on the actual merged source passes:

- Desktop/widget/I/O and QMP host checks: 112 PASS records; awk host smoke,
  Files ASan/UBSan, full matched build and documentation checks also pass.
- Owned and refused-dialog Open With VMs: cancellation, default/explicit routing,
  unsaved siblings, existing Text/Preview reuse, exact saved bytes, restored
  process baseline and pre/post generator/allocation checks.
- Files VM: collision/refresh selection recovery, chooser cancel/reopen, source
  routing through Text, exact source/output bytes and navigation/history.
- Theme VM: selected Files row, independent sibling history/close, all three
  live profiles and exact 722/600-pixel widths using the new outer resize edge,
  plus Text selection, persistence and restart/readback. Both chooser forms and
  all six Files theme/size captures were inspected.
- Awk VM: selected upstream regressions, Terminal workflow, errors, process
  commands, offline byte verification and reboot persistence. This is not the
  full upstream awk suite; independent exfatprogs fsck is unavailable here.

Evidence and exact recursive identities are in `build/review/files-merged/`,
including `tested-build.json`, merge timestamps and the final `receipt.json`.
The previous acceptance captures are preserved under its `prior-acceptance/`;
earlier failing controls and interleaved-log evidence remain retained. Every
disposable VM stopped. The shared checkout and attended review VM were untouched;
no hosted Actions or new native acceptance. Remaining changes in this closeout
are documentation only. The requested integration is delivered and verified;
no further implementation is required for this target.


### Window snapping — active scope November 3, 2026

User authorized Windows 7-style snapping, placement previews, verification and
merge. Branch `device/apple-silicon/window-snap` starts at fetched main
`da0db066`, retaining merged Files recovery/Open With, SrvrUI host extraction,
awk and edge resizing. This scope owns compositor titlebar drag placement and
preview rendering, transaction/capture tests and matched QEMU acceptance.
PR296 retains Air6,1 adoption diagnostics, PR252 lifetime work, and PR19 Wi-Fi.
No submodule, kernel, physical driver or hosted Actions change is planned.

Interaction: drag a titlebar toward a side for half-workarea placement or toward
the top for maximize; show the exact themed target before release. Move away or
cancel to remove the preview. A small drag threshold on an already placed window
restores its prior dimensions under the pointer, while a stationary click keeps
it placed. Preserve original restore geometry across side changes and existing
keyboard Arrange commands. Modal/focus/lifetime changes invalidate capture before
queued input can move a blocked window. Failed placement retains usable content
and geometry through the existing configure/refusal transaction.

Acceptance: geometry and capture/failure host fixtures, exact preview partial/full
redraw equality for all themes, complete desktop suites and matched build; actual
Text/VirtIO tablet preview/snap/restore/cancel/modal/content checks at 1x and 2x,
plus affected resize/theme regressions. The current review VM and persistent disk
are preserved; automated test VMs remain disposable and sequential. QMP evidence
is guest behavior, not new native or physical scanout acceptance.

Implementation `8d9e47fc` passes the complete desktop/I/O host suites, matched
build, snapping and resize VMs at 1280×800/1× and 1920×1080/2×. Preview-only
movement, exact release placement, direct side changes, stationary clicks,
drag restoration, Escape/modal cancellation, selection/save readback and GPU
presenter reset pass. Accepted preview/placement captures and hashes are in
[the evidence receipt](../assets/screenshots/window-snap-manifest.json).

Combined source `db713e6b` consumes merged PR296/main `fa8e503f`. Its clean
build produces byte-identical kernel, initramfs and disk artifacts to the
accepted snap runtime; display-owner, package/platform and CPU receipt tests
pass. Live theme/persistence and offscreen/dock recovery VMs also pass on this
combined image. The old free-drag fixture now releases at x=12, outside the
eight-pixel snap zone. Its macOS fallback uses a generated disposable image
with generator/allocation checks; independent exfatprogs fsck is unavailable.
The final fsck-only branch refinement does not change that exercised host path.

A new attended review VM starts in 4.37 seconds and completes preview/release
to frame 8,52,708,768 with a matching 706×723 Text client commit. It remains
open under `.local/macos-vm/window-snap-review/current.json`; the previous
edge-resize review VM and disk are preserved. Logs are in
`build/review/window-snap/`, `desktop-snap/`, `desktop-resize/`,
`desktop-theme-profiles/` and `desktop-window-dock/`.

[PR297](https://github.com/PaulMDemers/SrvrOS/pull/297) records delivery. The
requested behavior and acceptance are complete; no further implementation
is required. No native boot/install or hosted Actions change.

### SQLite CLI port — active scope November 3, 2026

Branch `ports/sqlite` starts at runtime `0fd0c5fc`. This isolated checkout owns
the SQLite 3.53.1 CLI adapter, static build/image integration and bounded guest
checks for single-connection exclusive-access SQL scripts and CSV import, query,
export and reopen. It depends on the pinned SrvrPorts SQLite amalgamation; the
existing `sqlitedemo` keeps its own engine configuration. WAL, process commands,
loadable extensions and readline are excluded. Full build, host VFS tests, guest ownership/temp/error probe and two-boot CSV
workflow pass. Two clean CLI builds match SHA256
`a23d90c8e8b49f7e36ff996e8cf009eee5ab7537faea6edabbe7a34cfd873b2f`.
Raw receipts are under `build/sqlite-cli-smoke/` and `build/review/sqlite/`;
external fsck is unavailable, while guest fsck and offline allocation pass.
No full-image reproducibility or power-loss durability claim is made;
the integration checkout and existing review VMs are preserved. No physical
boot or native acceptance is part of this scope.


### HiDPI and display settings — active scope November 3, 2026

User authorized native-density 1×/2× rendering, persisted display scale, resolution
selection/switching in Settings, coordination with the UI portability chat,
verification and merge. Branch `device/apple-silicon/hidpi-display` starts at
main `d5822348`; PR297 snapping remains complete. PR301 owns read-only inherited
Intel capture, PR299 Terminal quoting, PR252 process/I/O lifetime, PR19 Wi-Fi.

SrvrUI's separate portability checkout owns renderer/font/widget density APIs.
This integration owns logical-to-physical client surfaces, compositor copies,
presentation, desktop protocol/preferences and Settings; a bounded VirtIO driver
path adds real mode switching. Native and standard VGA remain inherited-mode
only. Existing review VMs and disks are preserved. No native boot or hosted
Actions is authorized by these QEMU changes.

Journey: open Display settings, inspect actual resolution/scale and available
backend presets, select a supported combination, Apply, then Keep or Revert.
The compositor owns a 15-second rollback deadline and client-disconnect recovery.
Only confirmed settings persist; saved-file status is distinct from active mode.
Keep unsaved application state, logical input coordinates and anchored placement.
Resource failure must preserve or explicitly report recovered actual geometry;
uncertain GPU reset retains/quarantines potentially owned memory.

Required evidence: native-density font/clip/damage tests, logical/physical
surface bounds and cache accounting, preference migration, apply/revert/timeout/
unsupported/failure host checks, full desktop suites and matched build, QEMU
resolution/scale switching with retained Text content, restart persistence,
resize/snap/theme regressions and measured memory/presentation cost. Coordinate
published child revisions before OS pins. The implementation and required
verification are now complete; delivery is recorded below.


Implementation `e613f8f0` consumes merged SrvrUI PR3 (`9eaaa74e`) and SrvrApps
PR4 (`edb51dc1`) on integration main `0fd0c5fc`. Its clean matched build and full
desktop/I/O, console/presentation/VirtIO, package/platform and CPU receipt suites
pass. QEMU applies and keeps 1920×1080 at 2×, trials 2560×1440, reverts on timeout
with Alt-Tab held, explicit Revert and Settings close, preserves selected/saved
Text bytes, and restores confirmed settings on a fresh desktop launch. Menus
and clock layout update immediately; overlay closure restores input focus.

Snapping and edge resizing pass at 1280×800/1× and 1920×1080/2×; live theme
changes, persistence and restart pass. Those regressions used the preceding
`b5eb540f` image; the final display-only fixes and integrated diagnostic changes
received the complete host suite and extended display VM. All exact build,
result and capture identities are in the
[acceptance receipt](../assets/screenshots/hidpi-display-manifest.json). The UI
portability chat owns the separately merged logical-canvas/physical-backing API
and native font atlas. At fixed logical size, 2× uses four times the pixel bytes;
the host full-scene sample measured 555/2231 µs at 1×/2×. This is drawing cost, not
a vblank or native latency claim.

The Settings form's control-capacity and trial-visibility defects found in live
verification are fixed and covered by actual host pointer routing. Older review
VMs/disks remain untouched. No physical boot/install, native modesetting or
hosted Actions change. [PR303](https://github.com/PaulMDemers/SrvrOS/pull/303)
records the requested integration; no further implementation is required for
this target.


Final combined runtime `6af9cfda` includes merged Terminal quoting PR299/main
`bfdcab65`. Its 56 quoting host checks, clean matched build and the extended
display VM pass again, including actual saved-document and preference readback
through the updated shell. The renderer, driver and Settings implementation is
unchanged from the preceding accepted run.

A new attended Aero review VM (PID 61610, separate persistent disk under
`.local/macos-vm/hidpi-display-review/`) reaches desktop in 4.38 seconds and
keeps/saves 1920×1080 at 2× through Settings. The native-resolution
[Aero capture](../assets/screenshots/hidpi-display-aero.png) was inspected.
Prior resize/snap review PIDs 85869/6233 and their disks are preserved.


### Terminal quoting recovery — completed November 3, 2026

User authorized reproducing/fixing quoted backslash handling and acceptance in
the independent `SrvrOS-app-quality` checkout, branch
`device/apple-silicon/terminal-quoting`, from fetched main `585ad8e5`. The shared
checkout and review VM remain untouched. Scope: `userspace/sh/sh.c` quote-aware
scanning/argument decoding, focused host/guest fixtures and shell documentation.
PR252 `badfeb37` changes arithmetic overflow and short-circuit evaluation in the
same shell file; it does not alter quoted-backslash handling. Preserve that
separate implementation and verify patch compatibility rather than merging its
broader kernel/I/O work. No cross-task messaging or native acceptance is included.

Acceptance: record failing exact-byte controls before edits; preserve single-
quoted backslashes and double-quoted ordinary backslashes while respecting
escaped quote/dollar/backslash behavior. Exercise inline awk/printf, empty and
spaced arguments, pipelines, redirection and script-file compatibility; run
focused host/sanitizer, matched build, CLI and Terminal/awk guest checks. Keep
existing limits; no general shell-language redesign or main merge is implied.
The original shell fails 27 of 40 focused checks; exact guest bytes confirm
`'left\nright'` becomes `leftnright`. Quote-aware scanners and argument/path
decoding now share one escape rule. Expansion values retain literal quote and
backslash bytes through the later decoder; parameter defaults preserve quoted
word boundaries and store raw assignment values. Expanded commands refuse
capacity overflow instead of silently losing closing syntax. The CLI sed case
now sends one backslash per backreference rather than compensating for the bug.

The focused host suite passes 56 checks with ASan/UBSan. PR252's unchanged
`badfeb37` shell/CLI patch applies cleanly to the corrected source in ignored
fixture storage; 63 combined checks include its arithmetic limits, overflow and
short-circuit behavior. This does not resolve or accept its wider kernel/I/O
conflicts. The shell/CLI patches also apply cleanly in the reverse order.

Clean runtime `d2fdc17d` passes all 482 CLI commands, the shell editing/history
regression, and 35 exact-byte quoting/boundary cases in the actual Terminal.
Keyboard-typed quoting, inline awk and existing program-file regressions,
process retirement, guest fsck, offline allocation checks and second-boot
persistence pass. Documentation links, Python compilation and three CLI-harness
checks pass. Independent `fsck.exfat` is unavailable on this host and is explicitly
skipped. Existing nested parameter/quote grammar limits remain; this milestone
does not claim general POSIX shell conformance.

[PR299](https://github.com/PaulMDemers/SrvrOS/pull/299) contains the reviewable
change. The final documentation head builds identical kernel, initramfs and disk
artifacts to the tested clean runtime. Source identities, recursive pins, hashes,
negative controls and check results are in local
`build/review/terminal-quoting/receipt.json`; Terminal evidence is in
`build/review/awk/20261103-163846-r5l071hz/`. All task VMs are stopped. The shared
checkout and review VM remain untouched; no main merge was performed.

### Terminal quoting integration — completed November 3, 2026

User authorized integrating PR299 and verifying merged main in the independent
`SrvrOS-app-quality` checkout. Current main `0fd0c5fc` adds Air6,1 platform
backing diagnostics and tooling; its Makefile additions combine cleanly with the
quoting fixture. Merge main into the PR branch, preserve both test targets, then
merge the reviewed PR and verify clean merged source with host checks, a matched
build, Terminal persistence and the full CLI suite. Shared checkout and review VM
remain untouched. Prior native-device and PR252 acceptance boundaries remain.

PR299 merged as `bfdcab65716c846645ef80fb7a4c3620308f4e27`. The isolated
checkout verified that exact clean source: 56 quoting host checks, package-audit
and platform-sample C fixtures, 3 package/22 platform/7 backing Python checks,
a matched build, all 35 Terminal quoting/boundary cases, keyboard input,
second-boot persistence, process retirement, guest fsck and offline allocation
checks. All 482 CLI commands and the editing/history regression pass. External
`fsck.exfat` remains unavailable and is explicitly skipped. An independent
read-only review confirmed both Makefile test groups and dependencies survived.

Concurrent PR303 landed before the documentation closeout, producing combined
main `3e9a045f` with Apps `edb51dc1` and UI `9eaaa74e`. After updating those pins
in this isolated checkout, its clean matched build, host checks, 35 Terminal
cases, persistence, all 482 CLI commands and editing/history regression passed
again. The final documentation-only update preserves this combined runtime.
Merged-runtime identities, hashes and results are retained in local
`build/review/terminal-quoting/integration-receipt.json`; Terminal evidence is in
`build/review/awk/20261103-170330-z6kf_bun/` for PR299 and
`build/review/awk/20261103-171352-3ik016sz/` for combined main. All task VMs are stopped; the shared
checkout and review VM remain untouched. Delivered and verified; no further work
is required for this integration target.
## Resolver and socket audit continuation, October 31

PR182 merged into main at `198c6a1c`. Its integration head `d7b2290c`
includes PR251 and retains both the network audit and platform diagnostic
Makefile targets. Storage lease, workspace log, scheduler, power, package,
query and socket host tests passed on the integrated tree. The matched x86_64
guest build and network ABI smoke also passed.

The Apple Silicon host continues on `device/apple-silicon/resolver-socket-audit`
from that merge. First, use the deterministic kernel query fixture to check
whether an answer accepted in the last poll survives the timeout decision.
Check the number of outgoing queries, the answer value, and the ordinary silent
timeout. The next boundaries are libc resolver retry deadlines, peer and option
state across duplicated sockets, and native descriptor I/O resumed after reuse.
PR19 tracks the Wi-Fi device work separately.

The first PR252 checkpoint reproduced both boundary failures against merged
main `198c6a1c`. A valid answer accepted during the last poll or final wait
was discarded when the caller observed the deadline. Checking the published
answer before starting another attempt fixes both schedules. All 15 query
modes and 46 parser cases passed. The two boundary cases, silent timeout and
exit also passed under native ARM64 ASan/UBSan. The matched guest built and
passed the network ABI smoke in QEMU. Logs are in
`build/review/resolver-deadline/`.

## November 2 resolver and socket continuation

PR252 resumes from `6f202ae5` with main through PR271 (`c9235491`).
Scope covers libc resolver polling and retry deadlines, socket state shared by
duplicate descriptors, and read/write continuations across signal dispatch and
descriptor reuse. Directed host fixtures and a matched QEMU guest will exercise
confirmed fixes. The desktop lifecycle and Wi-Fi branches retain their own work.

Implementation and regressions are in `34cb8ca5`. Against main `c9235491`,
six DNS injections failed their poll-budget or retry-count checks. The silent
server control passed. After a UDP connect through a duplicate, the original
descriptor reported `ENOTCONN`. A restarted read issued a second native read
against the replacement descriptor and returned three bytes in the host model.

The candidate passed all 33 socket-call modes, the alias and I/O restart
fixtures, 15 native query modes, 46 DNS parser cases, 12 UDP ownership modes
and 46 event-adapter cases. The new fixtures also passed native ARM64 ASan/UBSan.
The socket-call, alias and restart fixtures cross-linked for x86_64 Linux;
runtime host results came from macOS. Five disposable x86_64 QEMU guests
passed in sequence: netabi, full threadstress, libuv, TCP readiness and TCP
stress with 12 clients. The guest serial logs confirm both UDP deliveries
across duplicate close and that the replacement pipe retained its original byte.
Local logs and original-source probes are under `build/review/libc-socket-audit/`.

The next audit will exercise `SCM_RIGHTS` with a failing first payload transfer
and a surviving duplicate endpoint, including descriptor cleanup on receive
failure. This queue comes from inspection of the separate rights and payload
operations. It has no guest or fault-injection result yet.

## Unix-stream ancillary transfer scope

Starting from PR252 at `4353eee4` with main at `c9235491`, this pass covers
the boundary between pipe payload and descriptor ownership in sendmsg/recvmsg,
including failed writes, receive copyout, alias survival and control truncation.
It touches the process pipe queue, syscall access validation and libc socket
messages. The checkpoint is a matched x86_64 QEMU run with public API regressions.

### Ancillary transfer result

Before `1ff71432`, a guest probe linked to the `4353eee4` libc and run with
its kernel reproduced three failures. A nonblocking send on a full socketpair
left rights for a later write from a surviving duplicate. A zero-byte receive
took the descriptor. Reading a plain prefix discarded the following rights.
The same three public operations passed on the matched candidate. Two more
guest probes passed for peek followed by discard and for invalid output followed
by successful delivery of the original record.

The host process, syscall and libc fixtures passed queue pressure, descriptor
exhaustion, legacy send/receive, interrupted restart, alias replacement, and
copyout rollback cases. Libc passed 54 socket-call modes under ARM64 ASan/UBSan.
The process and syscall fixtures passed x86_64 ASan/UBSan under Rosetta. All
three fixtures cross-linked for x86_64 Linux. Logs, sanitizer output and the
pre-fix guest probe are in `build/review/ancillary/`.

The first full candidate thread suite failed its `dprintf` pipe readback check.
The original test passed both in isolation and on a full repeat. Its single
64 KiB nonblocking read could legally return short or be deferred during storage
I/O. The recorded assertion did not distinguish a read failure from a content
mismatch. Commit `3cad4dc1` handles short transfers in bounded chunks and
reports transfer and content failures separately. Ports, including posixdemo's
socketpair and pathname Unix socket cases, libuv and TCP readiness also passed.

After rebuilding with the test correction, the full thread suite, focused
`dprintf` run and network ABI guest all passed. The next ownership probe is
to close the last external descriptor while a receiving endpoint is held by
its own queue, directly or through a second socketpair.

## Queued socket reference cycles

Starting at `f6d182e7` on PR252 with main at `c9235491`, this pass tests
whether direct and indirect queued rights can exhaust pipe slots after external
close, and whether live receivers can still redeem those rights. Scope covers
process pipe ownership, legacy and atomic rights queues, and host and guest
lifetime regressions. The matched QEMU guest is the integration checkpoint.

While checking pipe-slot reuse, an isolated probe against `f6d182e7` found
that a blocked ordinary read could consume a byte from a newly allocated pipe.
This pass therefore also covers descriptor and slot identity across ordinary
pipe waits, including partial writes. It uses the existing process pipe and
threadstress fixtures alongside the rights-cycle checks.

### Queue lifetime and blocked pipe I/O result

Commit `1c07163e` covers both failures. On the previous guest image, closing
the last external receiver left an unrelated queued pipe writer open in both
the direct and two-queue cases. A duplicate kept the receiver redeemable as
expected. The matched guest passed 80 iterations of each cycle and the
duplicate-delivery check. A deterministic host probe also read byte `B` from
a replacement pipe on the previous kernel. With the fix it returned `EBADF`
and a fresh read got `B`. The public blocked `writev` probe failed against
the old kernel and passed on the matched image, returning its original one
byte of progress and leaving the replacement pipe empty.

The process host fixture exercised all 4,096 directed graphs and external
reader sets for three queues. Separate tests covered inherited descriptors,
pending accepts, interleaved final-file writeback and close, and blocked pipe
I/O across slot reuse. The complete fixture passed x86_64 ASan/UBSan under
Rosetta and cross-linked for Linux x86_64. Process lifetime, syscall I/O,
scheduler I/O, pipe message, socket call, restart, event-adapter, thread
harness and POSIX poll host targets passed. The x86_64 QEMU network ABI,
full threadstress, focused vector I/O, staged ports and libuv suites passed.
Evidence is in `build/review/rights-cycles/`.

The standalone process-lifetime guest failed its orphan-launcher exit check
on both the previous and candidate images at 512 MiB. The candidate also
failed at 1 GiB. Both images passed the preceding 128-child churn and 16
competing-waiter checks. A diagnostic kernel passed the first orphan iteration
and timed out in the second after ten seconds with both the launcher and its
new child present. The cause of the scheduling delay remains open. The serial
transcripts for these runs are also in `build/review/rights-cycles/`.

## Orphan launcher progress and pipe buffer mappings

Starting at `de44716f` on PR252, with main at `c9235491`, trace progress
while the lifetime monitor waits for its orphan launcher. Test scalar and
vectored pipe I/O across mapping changes during a wait. The checkpoints are
the standalone lifetime guest, deterministic pipe tests and a matched QEMU
threadstress run. PR272 also touches I/O admission for directory and PCM
operations. Keep those paths with their existing owner.

### Launcher and pipe mapping results

The failing lifetime trace showed both the orphan launcher and its child ready
while the monitor continued to poll. Commit `1a6b52f5` parks the monitor on
the process exit event and between retirement checks. It keeps the original
ten-second orphan deadline. Three fresh diagnostic boots completed all four
iterations each. The matched production image also completed all four, with
512 sequential child launches, 64 competing waiter pairs, unrelated-process
survival and the PTY check. The trace establishes the missed progress during
monitor polling. It does not identify a general scheduler fairness defect.

Against the previous kernel, separate guest probes unmapped a blocked scalar
reader's destination and a partially completed writer's source. Both panicked
in kernel mode. The fault addresses resolve to the byte copy in
`process_file_pipe_read_mode` and `process_file_pipe_write_mode`. A blocked
`readv` with two one-byte destinations returned one byte when the second
destination was unmapped, but consumed both pipe bytes.

Commit `e1b8ae7a` checks the active mappings before committing a pipe
transfer. In the new guest both reads returned `EFAULT` and recovered the
two original bytes. The scalar writer returned its completed one-byte prefix.
Focused and full threadstress and netabi passed. The process and syscall
mapping fixtures passed x86_64 ASan/UBSan under Rosetta. The Linux x86_64
host fixtures also passed. GCC 16 needed an exception for a pre-existing
`maybe-uninitialized` warning in the process fixture. The Darwin Make targets
passed with warnings treated as errors. The baseline ISO, serial logs and
symbolized fault sites are under `build/review/pipe-mapping/`. The three
diagnostic launcher boots are under `build/review/rights-cycles/diag-park-*`.

## Repository-wide audit and PTY transfer lifetime

Baseline `dc0a3fef` on PR252 includes main through `c9235491`. Inventory
all tracked paths, including test and build tooling, ports, shared interfaces
and documentation. Review source by subsystem and record verification depth
and external dependencies. Start with PTY transfer mappings and descriptor
reuse on both ends. This checkout owns the emulated guest and host checks.
PR272 covers directory EOF and PCM admission and PR19 covers USB Wi-Fi.


### Whole-tree audit integration result

The combined candidate fixes the remaining reviewed syscall buffer races,
including network receives and transmits, file/PCM access and console copies.
Against the pre-change `dc0a3fef` kernel with the new netabi test disk, unmapping
a blocked UDP destination faults in kernel `copy_bytes` at `net.c:282`.
The rebuilt guest returns a fault and preserves the datagram. Invalid peer
metadata leaves all output untouched, and replacing a descriptor while an alias
remains live preserves both endpoints' queued data. PTY tests cover scalar and
vectored mappings, partial writes and both ends of descriptor replacement.

User faults now terminate the affected process. Sysabi checks an unmapped
write, a worker invalid instruction and a delayed fault after the initial thread
calls pthread_exit. Process teardown reacquires storage admission on every exit
entry. Additional fixes cover PMM tags above 8 GiB, ELF and initramfs bounds,
libc allocation/integer parsing, libuv completion/async ownership, utility error
paths, boot payload verification and FAT image sizing.

A full native ARM64 cross-build passed. Nine sequential disposable x86_64
guests passed sysabi, netabi, full threadstress, libc, the local UV bridge,
upstream libuv, process lifetime, CLI and staged ports. Lifetime completed four iterations with
512 child launches and 64 competing waiter pairs. The final isolated Linux
GCC 14 `test-core` run passed all 173 direct targets, including 385 reported
Python unittest cases in 53 groups. The additional 30-case offline namespace
suite also passed (415 cases across the combined run). The expanded user-copy
fixture is included and also passed x86_64 Darwin ASan/UBSan under Rosetta. The
network queue and UV bounds/lifetime fixtures passed native ARM64 ASan/UBSan.
All six ASL fixtures compiled with iasl. Local documentation checks passed 765 inline links.

The first Linux run exposed missing exfatprogs, test fixtures that needed their
host IRQ shim after the copy changes, a missing fixture MAC, GNU feature-test
declarations and a fixture indentation warning. These were corrected before
the successful strict run. The bitmap fixture records that exfatprogs 1.2.9
can report an oversized bitmap clean, and independently checks geometry and
byte-for-byte repair. Current hosted Actions remain disabled. No physical
hardware, recovery media or firmware settings were changed.

Evidence is under `build/review/whole/` and
`build/review/process-lifetime/`. The [coverage ledger](../audit-coverage-2026-11-02.md)
accounts for every tracked baseline path and retains the distinction between
screening, sampled manual review and runtime coverage. The full manual/native
hardware and external-dependency review remains open.

Continued review fixed tracked uACPI root-scope resolution, physical-table error
propagation (distinguishing direct optional denial from required-child failure),
GAS access encodings and map/unmap spans. The rebuilt ordinary AML resource
fixture and full Q35 SCI/worker runtime suite passed, followed by sysabi and
netabi reruns. The local iasl wrapper initially failed before guest launch
because its container mount resolved the macOS `/var` alias inconsistently;
correcting the wrapper path made the synthetic ASL compile. No host firmware
was accessed.

The final shell arithmetic fix covers checked boundaries and parsing of skipped
logical branches. Native ARM64 and Linux sanitizer fixtures pass, and it is
included in the final full host run. Final source code is `e8491549`; later
documentation-only commits do not change these implementations.

The final CLI rerun passed all 491 commands, including exact signed-boundary,
short-circuit and overflow-status assertions. Its post-run filesystem check
reported zero leaks/FAT/errors. The full host transcript is
`build/review/whole/linux-core-173-final.log`; the CLI transcript is
`build/review/whole/cli-shell-final.log`.

The final staged-port guest also passed after the shell changes, with a clean
post-run filesystem check. Its transcript is
`build/review/whole/ports-shell-final.log`.


### SQLite merge review — November 3, 2026

Candidate `d3b5c321` integrates main `89c210db`, including merged PR252, and
pins SrvrPorts `525f422`. Review fixed a shell-helper rollback-journal collision;
guards now protect journal names before creation, case variants, inode aliases
and streams opened before the database. Independent source review and host VFS
checks pass. Fresh two-boot CLI/guest VFS tests, exact rollback rows after rejected
journal writes, guest fsck and offline allocation pass. Receipts are under
`build/sqlite-cli-smoke/20261103-212748-7tm1f5tf/` and
`build/review/sqlite-merge/`. A clean rebuild matches CLI ELF SHA256
`1107f624eba0a878c4d4440d4ddae9c77af7785de42a0647eeb60999f74d8e51`.
Independent fsck is unavailable. The original acceptance hash above remains
historical; no power-loss, physical-device or full-image reproducibility claim
is added. Integration checkout/review VM remain untouched. PR2 in SrvrPorts must
merge before OS PR306; the user authorized both merges conditional on checks.

### Unexpected app closure investigation — bounded audit completed November 3, 2026

User authorized a bounded app recovery investigation in the independent
`SrvrOS-app-quality` checkout, branch `app/closure-recovery`, from main
`62b0708e`. Earlier unexplained physical app closures remain historical evidence,
not a reproduced VM defect. Own investigation of Text/Files/Preview/Sketch
failed saves, canceled dialogs, closure during accepted file work and process
retirement. Reuse existing lifecycle/Sketch fixtures and fix only reproduced
in-scope defects. Keep native acceptance, PR252 kernel/I/O ownership, the shared
checkout and review VM untouched. Build and disposable VM harnesses run sequentially.

Acceptance: matched source/pins/build; bounded mixed-app lifetime workload;
normal and refused-dialog save/cancel/quit paths; exact saved bytes and retained
edits; Sketch revision isolation/failed replacement; no unintended app retirement,
surviving workers or unbounded heap/frame growth. Report physical closure cause
as unresolved unless evidence actually identifies it. The bounded audit is now
complete; results follow.

Clean source `4f01d9c1` (Apps `3cf32af2`, UI `9eaaa74e`, Ports `525f4229`)
produces byte-identical kernel/initramfs/disk artifacts to baseline `6f277f51`.
Three desktop sessions and 36 application lifetimes pass with no surviving GUI
workers. Post-session heap remains 1,075,712 bytes and physical-frame use is
identical across completed rounds. The directed normal/Open With, refused-dialog,
shutdown-cancellation and deliberate Preview-termination fixtures all pass with
retained edits, exact saved files and process-baseline restoration. Sketch passes
exact pixels, editing during save, saved-revision isolation, failed replacement,
close prompts and Preview import. A rendered failed-save capture confirms Text
retains its unsaved document and displays an error. This is functional/expert
inspection, not representative-user or physical-device evaluation.

No unintended closure or runtime defect was reproduced; no application/kernel
runtime change is justified. Source review likewise found no demonstrated normal
close/worker-retirement fault. The historical physical-device closure cause
remains unresolved. Reopening that investigation requires a new reproducer or
an affected-device exit reason/trace; another unchanged VM pass is not evidence
of a physical fix.

The only implementation change adds optional `--output` to the lifecycle and
Sketch harnesses. Defaults and assertions are preserved; each new run retains its
own evidence. The first directed run failed on missing host Pillow before image
inspection; its failure is retained, and reruns passed using local ignored
`build/tooling/recovery-venv` with Pillow. Generated-image and offline allocation
checks pass; independent exfatprogs fsck was not run. Evidence, hashes, source
identities and limits are in `build/review/app-closure-recovery/receipt.json`.
All task VMs are stopped. Shared checkout and review VM remain untouched. The
bounded target is complete; integration of this evidence/tooling review is a
separate delivery action.


### jq isolated port — active November 3, 2026

User authorized the bounded jq milestone on `ports/jq` in SrvrOS-ports-jq,
based on `5634cd9d`. Owns port adapter/build configuration, image entries and
disposable guest JSON report/CSV-to-SQLite checks. No integration checkout or
review VM access. No kernel/libc changes planned; confirmed gaps are coordinated
with integration. Implementation and scoped guest acceptance pass. The port builds static jq
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
Status: implementation complete and ready for review; no merge authorized.


### jq merge review — verified November 3, 2026

User authorized reviewing SrvrPorts PR6, merging it before SrvrOS PR337, and
verifying the combined build. Work uses only `SrvrOS-ports-jq`; integration
checkout and attended review VMs remain untouched. Main `62b0708e` contributes
read-only Haswell audit tooling/docs since the original `5634cd9d` jq baseline;
it changes no jq runtime dependency. Port `22c2660` receives independent source
review, archive identity checking, a clean matched candidate build, the two-boot
jq/SQLite guest workflow, existing ports regression and the added audit tests.
The single-thread/no-regex scope and untraced optional three-thread allocation
failure remain explicit; no shared libc/kernel edits or physical acceptance.

Clean candidate `e5db0a37` passes the full image/probe build, two-boot jq workflow,
existing ports regression, seven Haswell audit tests and documentation checks.
The original CLI hash remains identical. Independent archive verification covers
all 312 tracked entries (311 regular files and one archive hard link). Fresh
source provenance is clean, recorded in `build/review/jq-merge/tested-build.json`
and `build/jq-smoke/20261103-215652-k40favot/receipt.json`. No concrete scoped
review blocker remains. SrvrPorts PR6 merged first as `747cca28`; its merge tree
is identical to reviewed `22c2660`. The OS now pins that merged revision.

Final combined source `fcd79c0a` also preserves installed-state inspection
PR336/main `a37eb7b2`. Its clean build produces byte-identical kernel, initramfs
and disk artifacts to the freshly guest-tested candidate; 902 documentation
links pass. This refresh changes no runtime dependency. SrvrOS PR337 records
the authorized ports-first integration; final merge identities/build receipts
are retained in `build/review/jq-merge/`. All test VMs stopped; the integration
checkout and attended review VM remain untouched. No further work is required
for this bounded jq milestone.

### PR339 integration — verified November 3, 2026

User authorized integrating the bounded recovery audit/tooling PR339 in
`SrvrOS-app-quality`, preserving the shared checkout and review VM. Current main
`f0c88a6d` adds jq and installed-state inspection. Resolve only the appended
host-journal conflict by retaining both records; preserve the new Ports pin.
Verify Python/doc checks and a matched merged build, then exercise both changed
harnesses with explicit evidence directories. Existing recovery assertions and
the unresolved physical-device closure boundary remain unchanged.

PR339 merged as `b78e3ed5`. The exact clean merged source builds with Ports
`747cca28`, Apps `3cf32af2` and UI `9eaaa74e`. Python compilation and documentation
checks pass. Both changed harnesses pass with explicit independent evidence
paths: lifecycle/Open With retains edits after failed saves/canceled quits,
checks exact saved files and process-baseline restoration; Sketch verifies exact
pixels, edits during saves, failed replacement, close prompts and Preview import.
Existing audit acceptance remains valid; no assertions or app runtime changed.
Independent review found no remaining conflict or behavioral regression.

The documentation closeout preserves the tested kernel/initramfs/disk hashes.
Evidence and source identities are in local
`build/review/app-closure-integration/receipt.json`. All task VMs are stopped.
Shared checkout and review VM remain untouched. Integration is delivered and
verified; historical physical-device closures remain unresolved as documented.
No further work is required for this integration target.

### bc isolated milestone — active November 3, 2026

User authorized bc-only 7.0.3 on `ports/bc` from main `b78e3ed5`, with signal
recovery resolved before acceptance. Owns port adapter/configuration, image
entries and disposable guest decimal/report/signal checks. Integration checkout
and review VM remain untouched. No libc/kernel edits without concrete-gap
coordination. Implemented and verified: bc 7.0.3 static core arithmetic, scale/sqrt/bases,
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


### bc merge review — November 3, 2026

User authorized ports PR7 followed by OS PR341 if review passes. Clean candidate
`8e59f9d9` integrates main `819956be` and pins port `78fb8184`. Source review
found no signal implementation blocker within the cooperative scope, but
tightened the TERM probe: unique pre-loop marker, live-child check before the
signal, and expected normal exit zero. The fresh two-boot arithmetic/report,
seven unchanged upstream fixtures, actual stdout failure and repeated
blocked-input/busy-loop signal recovery checks pass. Guest fsck/offline
allocation, existing ports regression, nine salvage-scope tests and docs pass.
CLI SHA256 remains `62c0737baafa6b569bd917303600e5aee341c5f09b24776fce49615e5fa6af61`.
Raw receipts: `build/bc-smoke/20261103-222330-wjogff8d/` and
`build/review/bc-merge/`. Inactive owned proof disks were losslessly compressed
and hash-verified to reduce host disk pressure; `proof-archives.json` records
restoration paths/hashes. No evidence was discarded. No libc/kernel changes,
full asynchronous-signal or physical-device acceptance is added. Integration
checkout and review VM remain untouched. The final merge receipts record the
authorized dependency-order delivery; no additional feature work is needed.
