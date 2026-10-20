# MacBookAir7,2 / A1466

Updated: 2026-10-20. State: **active reference machine; partial device acceptance**.

Latest accepted bounded **scratch write/flush overlap**: release
**`5792b6ece45b85dc`** / artifact **`e2d779b`**, retry checkpoint **`d4a21cb`**,
session **`session-00065e4c30fcddac-00`**. It completed 1,894 write/flush/readback
cycles (5,682 transport commands), including 62 cycles during sampling and three
sample-generation advances inside 55 I/O scopes (maximum 349,839 microseconds).
Five fresh samples and 5,414 cached reads completed, 574 during sampling, maximum
read time 18 microseconds. Cancellation, restart, in-flight stop and a short stop
timeout followed by clean retirement passed. Both initialized lifetimes restored
retained enables, balanced attachments and ended with zero heap/errors/denials.
SCI recorded six handled and six unclaimed active deliveries, none during setup
or retirement and none in flight afterward; the unclaimed cause remains open.

The exclusive scratch capability was revoked before remount. Collection verified
the final 4096-byte payload against the cycle count and preserved its bytes/hash.
Completed exit, new Ubuntu boot, consumed one-shot and installed/recovery hashes
were verified. Offline fsck reports clean (181 directories, 871 files); SrvrOS's
allocation checker accounts for 1,512 clusters with zero leaks or FAT mismatches.
The accepted payload and 47 hashed evidence files are backed up outside `build`.
The earlier failed attempt of this same release remains separate evidence.
This accepts bounded transport writes/flushes during sampling, not ordinary
filesystem metadata mutation, human input, sustained desktop operation or gauge
accuracy. No new reboot is armed; ordinary desktop battery access stays disabled.

Latest accepted bounded **read** run: **`b73c1ae3e4643dc9`** / artifact **`0cb9f2e`**,
session **`session-00065e4a0a43b75e-00`**. Battery sampling progressed during
103,094 uncached USB reads; all 4096-byte reads matched their baseline. Five
sample-generation advances occurred inside storage I/O scopes, with 153 reads
observed during sampling. The cached service completed five fresh samples and
106,806 reads (921 during sampling), maximum observed read time 18 microseconds.
Both initialized lifetimes retired cleanly and restored retained event enables.
SCI counted six handled and six unclaimed active deliveries, none during setup
or retirement and none in flight afterward; the unclaimed cause remains open.
Native read overlap is accepted within this bounded test. Writes, human input,
continuous desktop operation and gauge accuracy are not accepted by this run.

Current unattended-test policy (user correction October 20): announce required
human actions before boot; tests requiring no input must return to Ubuntu on
handled failures as well as success. The `-auto-return` capture checkpoints and
unmounts USB before battery runtime entry. Normal returns remount, save the result
and reset even on failure. Contained faults disable interrupts and reset without
filesystem reentry or unsafe interpreter cleanup. Missing completion remains
failure, never a pass merely because Ubuntu returned.

**Bounded cached battery service accepted:** release **`d9e6f4f75da4e871`**,
artifact **`a24836a`**, session **`session-00065e4922d1be3d-00`** completed the
v33 automatic test. It produced five fresh samples, 49,344 cached reads (10,368
during sampling), and an observed maximum read time of 18 microseconds.
Immediate cancellation, two initialized lifetimes, restart and in-flight stop
with a short wait timeout followed by successful retirement passed. Both retained
event enables were restored in each initialized lifetime; together they handled
90 queries/92 notifications with balanced attach/detach and zero reported runtime
errors, denied accesses or remaining heap. Setup took 3.997 and 3.978 seconds.

The matching exit log contains clean retirement and `completed=yes`. Ubuntu has
a new boot ID, the one-shot entry is consumed, installed/previous payload hashes
match, and read-only exFAT checking reports clean (176 directories, 852 files).
The accepted payload and 33 hashed evidence files are preserved outside `build`.
This establishes service completion and host recovery; failure-triggered reset
has VM evidence, not a physical fault-injection result.

Acceptance is deliberately bounded: USB logging was parked during runtime.
Long-lived desktop use, concurrent storage/input responsiveness and gauge
calibration remain pending. An unhandled SCI line 9/vector 41 warning appeared
after the first setup; that initial run did not establish its count or cause.
The later counter capture above quantifies deliveries. Do not hide that warning
or infer a fault-free interrupt path from the runtime's zero error counters.
The remaining block is scheduling/USB coexistence validation before
enabling an ordinary desktop provider. Battery values remain relative SOC:
the capture's 98% is not acceptance of agreement with Ubuntu's 86% display.

Builds, workspace pause/remount logging, bootlog/cache/autotest host checks and
44 staging/collector checks passed for this candidate. VM setup-deadline injection
verified actual reset; explicit legacy halt still passed. Exact-stage UEFI/GRUB
checks verified missing-workspace reset and native Q35 refusal with USB
park/remount, saved failed report, clean exFAT and reset. Earlier failed v33 runs
remain preserved; this success does not identify their exact causes or prove
the reader-wait change alone resolved them.

## Identity and ownership

Development host: Ubuntu on the 13-inch Air, Broadwell-U, Intel HD Graphics 6000
`8086:1626`, xHCI `8086:9cb1`, Apple topcase `05ac:0291`, HDA `8086:9ca0` /
CS4208 `1013:4208` subsystem `106b:7e00`. Firmware, descriptors and routing evidence
are in the linked device guides. The shared integration baseline is now `main`,
including the completed `codex/linux-development-baseline` work. Subsequent
device work uses separate branches based on `main`.

## Active work and shared dependencies

### Allocation-check discrepancy — October 20 (resolved)

Base `d72d2af` (merged PR #6), Pro PR #5 inspected at `7c9372b`.
Independent read-only allocation traversal and the actual SrvrOS checker both
found orphan clusters 114, 159, 162, 210–213 and 606. The kernel guard was correct;
`fsck.exfat -n` (1.3.2) reported clean without resolving those orphan allocations.
Their originating write or interruption is unknown. No kernel or native battery
policy change was needed.

Preserved boot/FAT and all 1,506 allocated clusters in a sparse snapshot outside
`build`. Recovery was rehearsed on a copy, verified against the unchanged device,
then applied with exfatprogs orphan rescue. All 862 existing files remained
byte-for-byte identical. Five `LOST+FOUND` files preserve all eight orphan
clusters, also copied into the durable evidence archive. The recovered volume
passes the real SrvrOS allocation checker: 867 files, 1,507 accounted clusters,
zero leaks/FAT mismatches/errors. Offline fsck reports 180 directories, 867 files.
The workspace was remounted and the unchanged automatic v36 payload
`5792b6ece45b85dc` subsequently passed the bounded trial summarized above.

Evidence is under `~/.local/share/srvros/failed-evidence/exfat-orphans-20261020`.
The snapshot contains boot/FAT and allocated data; unallocated bytes were not
captured. New host tooling exposes read-only allocation checking on an unmounted
USB partition. Disposable 4 KiB/128 KiB fixtures exercise orphan refusal,
byte-preserving rescue and renewed scratch eligibility; the kernel guard remains
unchanged. Scratch/entry tests, the full filesystem hardening suite (including
write/flush/power-cut boundaries), wrapper selection and documentation checks
pass. The later successful retry accepts bounded scratch write/flush overlap.
See the [offline workflow](../ubuntu-device-agent-setup.md#7-test-return-and-collect).

### Native scratch write/flush overlap — October 20 (preflight failed)

Bounded objective: extend coexistence testing to USB writes and cache flushes
without filesystem metadata updates during native battery ownership. Prepare a
new 4096-byte scratch file, verify its exclusive allocation, flush/unmount the
workspace, and retain a private write capability for only that file's data range.
Normal retirement revokes that capability before remount and final collection;
contained firmware faults may leave scratch contents incomplete but must not
reenter the filesystem. No general USB write aliases are enabled during runtime.

Touches block range translation/cache invalidation, exFAT allocation inspection,
workspace scratch lifetime, battery diagnostics and host/VM tests. First gate is
a tested scratch capability including refusal, partial-write and flush failures;
then wire the automatic capture with strict offline verification. Base `b6075ef`,
Pro PR #5 `7c9372b` inspected (battery work deferred on that unit). Its CPU-load
capture overlaps shared boot staging/startup files: keep its implementation separate and reconcile those hunks
at integration; do not copy or replace the Pro proposal. Ordinary desktop battery
access remains disabled. The accepted b73 read trial is collected; its evidence and recovery payload
remain preserved.


The v36 scratch capture is implemented. Block/workspace host checks and disposable
exFAT extent tests pass for 4 KiB and 128 KiB clusters, including sector-boundary
tears, bad allocation/alias refusal, partial writes, flush/read failures and
poisoned-lease remount refusal. All 46 staging/collector tests pass. Ordinary,
native and model builds pass. Positive write-overlap and injected automatic
reset VMs verify clean exFAT and unchanged disk bytes outside scratch data;
existing service, read overlap and disposable filesystem-write regressions pass.
Collection independently verifies and copies the final scratch payload.
Candidate `5792b6ece45b85dc`, artifact `e2d779b`, passed exact-stage native
Q35 refusal with saved failed result, scratch revocation, clean exFAT, unmount
and reset. Missing-workspace automatic return also passed.
The candidate is installed with previous desktop `b427b78f21644bd2` retained;
accepted read payload `b73c1ae3e4643dc9` remains on EFI. The unreferenced `a528`
EFI duplicate was removed only after its independent backup/hashes were verified.
The physical run returned automatically but **failed preflight**, session
`session-00065e4a6f889c64-00`: SrvrOS's allocation check reported eight leaked
clusters, first cluster 114. It refused the scratch capability before native
battery runtime entry; no service/write-overlap acceptance is claimed. The
matching exit saved `completed=NO`. Ubuntu's read-only offline exFAT check
reports clean (179 directories, 862 files). The new boot, consumed one-shot and
installed/recovery hashes were verified. No new reboot is armed.

The failed report and 45 hashed evidence files are retained outside `build` at
`~/.local/share/srvros/failed-evidence/5792b6ece45b85dc`. The accepted b73 read
baseline is unchanged. The later recovery above resolves this preflight refusal without bypassing the
guard. It does not accept native writes or identify the historical orphan source.
Both full CI baseline runs passed at `800d84e`, which contains the exact scratch
implementation. See the [v36 contract](../battery-service.md#native-scratch-writeflush-overlap-capture-v36)
and [integration handoff](../agent-coordination.md#october-20-battery-baseline-handoff).

### Native read-only USB overlap capture — October 20

Scope: add a distinct v34 automatic battery-service capture that checkpoints and
unmounts the validated workspace, then repeatedly reads the same USB device with
writes revoked while sampling. Compare read bytes with a pre-runtime baseline;
require sample generation progress inside actual scheduler I/O scopes. Save after
clean retirement/remount; contained faults reset without filesystem reentry.
Touches the diagnostic, workspace parked-device identity, staging/collector and
VM fixtures. Base b6075ef and Pro PR #5 at 8fc3e1c remain unchanged. No desktop
enablement or new firmware permissions. Native write/flush overlap and human input
acceptance remain separate gates. No human action is required by this trial.

Implementation and host/VM checks pass: 45 staging/collector tests, workspace
identity/write-revocation host coverage, ordinary/native/model builds, new
read-overlap and injected-reset cases, and existing service/reset regressions.
The modeled overlap issued 83,547 real USB read commands, with 57 reads while
sampling and five generation-progress bursts; all bytes matched. Whole-disk
hashes were unchanged and exFAT clean after both overlap and injected reset.
These are VM results. The exact-stage candidate `b73c1ae3e4643dc9`, artifact
`0cb9f2e`, also passed Q35 native refusal with saved failed result, clean exFAT,
unmount/reset and missing-workspace return.

The physical trial subsequently passed with no human input. Its 103,094 read
commands included 153 reads during sampling and five generation-progress bursts
inside 58 storage scopes, maximum observed duration 289,851 microseconds.
Immediate cancellation, restart, in-flight stop and short stop timeout followed
by clean join passed. The two initialized lifetimes handled 90 queries and 92
notifications, balanced attachments, and ended with zero heap/errors/denials.
Setup took 3.984 and 3.982 seconds. SCI accounting is summarized above.

The matching exit contains complete service/read proof and `completed=yes`.
A new Ubuntu boot, consumed one-shot, installed hashes and clean offline exFAT
(178 directories, 858 files) were verified. The persisted exit precedes final
unmount/reset; recovery is corroborated by those host checks. At that checkpoint current was `b73c1ae3e4643dc9`,
previous desktop is `b427b78f21644bd2`, and prior accepted `a5281d842c8b6160`
remains available. Accepted payload/evidence backups are outside `build`, under
`~/.local/share/srvros/known-good/b73c1ae3e4643dc9` and
`~/.local/share/srvros/accepted-evidence/b73c1ae3e4643dc9`.
The old unreferenced `ed93e03b2585dcaf` EFI duplicate was removed only after its
durable backup was verified. No new reboot was armed at that checkpoint.

At collection, integration remains `b6075ef`; Pro PR #5 was inspected at
`cdbefc2`. Its isolated load diagnostic changes do not overlap this native battery
scope. Next is bounded native write/flush coexistence with durable failure
recovery. Human input, sustained SCI behavior and ordinary desktop enablement
remain separate gates.


### Battery scheduling repair — October 20

Scope: audit and grant explicit I/O-service eligibility only to the separate
cached battery service's owner/worker while its validated runtime is engaged.
The generic ACPI worker remains ordinary by default; other adapters must not
inherit eligibility. Touches battery runtime, an opt-in worker admission API,
worker host tests, a scheduler readiness-scan lifetime correction, and VM overlap fixtures. Base `b6075ef` and Pro PR #5
`8fc3e1c` were fetched unchanged. No desktop startup, native resource gate,
transport deadline or scheduler-wide policy change is part of this block.
Preflight mappings precede engagement; service callbacks use kernel mappings,
serialized heap/PCI/console/queue operations and runtime-owned wait dependencies.
The repair permits EC progress before the I/O owner releases its scope. In the
final VM build the 250 ms hold measured 268.379 ms with 83 EC commands; the
five-second scope measured 5.017 seconds with 293 commands and sample generation
1→3. The disposable USB case completed 94 write/flush cycles over 5.524 seconds,
417 EC commands and generation 1→3. It retired cleanly, unmounted, verified the
4096-byte final payload offline and passed read-only exFAT checking.

The audit also closed worker reclamation gaps: stopped publication retains
masked IRQs until the scheduler marks the thread dead, and readiness scans skip
retired threads before invoking eligibility callbacks. Host regressions cover
both paths. See the [dependency audit](../battery-service.md#audited-storage-eligibility).

Ordinary/native/VM kernels and scheduler/worker/cache/SCI host tests pass.
Twelve targeted battery scenarios passed before the final scheduler scan
correction; long/sustained/actual-USB overlap, query deadline and automatic
fault reset were rerun on the final build, along with the ordinary ACPI worker
lifecycle VM. Staging/collection has 44 passing host checks. Evidence is in
local ignored `build/battery-io-fix-20261020`; the full CI matrix was not rerun
locally. CI now includes the USB fixture and its exfatprogs dependency.

The subsequent v34 trial above accepts bounded native USB-read coexistence.
Native writes/input and sustained SCI behavior remain pending. Ordinary desktop
battery access stays off.

### Earlier SCI accounting and storage scheduling block — October 20

This checkpoint preceded the scheduling repair above.

The Air agent owns the next bounded runtime change: phase-scoped SCI delivery,
claimed/unclaimed counts and timestamps, plus disposable VM tests of service
progress under the existing storage scheduling exclusion. Touches the separate
battery runtime/model, its VM harness and validation documentation; no changes
to native resource gates, IRQ acknowledgement, scheduler exemptions or ordinary
desktop startup are planned. Base `b6075ef` and Pro PR #5 `8fc3e1c` were fetched
again before work; no overlapping native EC scope was found. Preserve accepted
`d9e6f4f75da4e871` while establishing the interrupt warning's measurable scope
and whether storage exclusion can exhaust the existing operation/query budgets.
A modeled scheduling hold is not physical USB or desktop responsiveness evidence.

Implementation: ready/retired/fault SCI records now separate setup, active and
retirement callbacks, preserving claimed/unclaimed results and timestamps without
new event-register access. The VM unclaimed-dispatch fixture counts five active
deliveries in the first sampled lifetime and one after restart, while the generic
warning prints once. A 20.311 ms scheduler I/O exclusion permits full service
completion; a 250.234 ms exclusion at a transport read reproduces a sample timeout
(EC error 1) with live state retained. Cached readers work during both holds.
The latter is containment evidence, not a coexistence pass. Ordinary desktop
enablement remains blocked on an explicit worker/owner scheduling audit; do not
extend deadlines or add a blanket ACPI storage exemption.

Native/VM/ordinary builds, SCI-owner/IRQ-registry/cache host tests, 44
staging/collector checks and all seven targeted VM cases pass: the three new
fixtures, normal service, stale recovery, contained automatic reset and native
refusal. The physical capture repeated the accepted parked-USB service test
with counters; it needed no human input and used automatic failure return.
It measured the native warning, not the known-failing storage interleave.

Release **`a5281d842c8b6160`**, artifact **`6938813`**, completed that unattended
capture. Exact-stage UEFI/GRUB rehearsal passed native Q35 refusal,
saved failed-report byte verification, clean exFAT, unmount and actual reset.
The prior accepted service payload and previous desktop are preserved; the
unreferenced failed `a3df674e8d6e5c65` payload was archived and hash-verified before
EFI space was reclaimed.

The matching native session `session-00065e497cc34d56-00` has a completed exit and
strict service proof: 48,768 reads/9,728 during sampling, maximum 18 microseconds,
five fresh samples and clean retirement of both initialized lifetimes. Epoch 2
recorded ten active SCI deliveries (five handled/five unclaimed); epoch 3 recorded
two (one/one). All setup/retirement and final in-flight counters were zero.
Setup took 3.999/3.978 seconds. Both event restores, balanced attach/detach and
zero runtime errors/denials/heap were preserved. The new Ubuntu boot, consumed
one-shot and clean exFAT (177 directories/855 files) were verified. Payload and
36 hashed evidence files are backed up outside `build`. No new boot is armed.
Unclaimed interrupts remain unexplained; no continuous desktop or concurrent
storage acceptance follows from this parked-USB run.

### Cached battery service block — October 20

Following acceptance of the latest v32 transition round, this Air branch owns
the cached battery/AC provider. Scope: a pure snapshot/freshness model, an
asynchronous lifecycle owner around the existing serialized EC/AML worker,
explicit start/stop and bounded waits, and host/VM lifecycle/reader regressions.
Preserve the accepted diagnostics and exact native resource gates. Ordinary
desktop startup remains disabled until service lifetime and responsiveness have
separate evidence; UI/syscall integration follows this kernel contract.
Base remains `b6075ef`; Pro PR #5 is `8fc3e1c` (accepted paired-fan telemetry docs),
confirmed by fetch after this physical return.
No overlapping native EC implementation or new sibling acceptance is inferred.
The next checkpoint is a tested cached service with safe retirement, stale
readings during missed/invalid updates, and repeatable start/stop lifetimes.
Reserve **v33 laptop-probe** for a bounded automatic service-lifecycle capture
after those checks pass, including exact-stage refusal rehearsal and strict
collection. It exercises cached readers and stop/restart, not desktop acceptance.

Historical implementation and trial sequence (the acceptance above supersedes
the pending statuses below):

The pure cache and asynchronous lifecycle are in
place; [service contract](../battery-service.md) documents freshness, retirement
and ownership. Host cache checks also pass ASan/UBSan; native, VM and ordinary
kernel builds pass. All 21 battery VM scenarios pass, including cached readers
during sampling, invalid/incoherent update recovery, repeated lifetimes,
in-flight stop, native refusal, independent deadline containment and the earlier
transition timeout/reentry regression. The 31 staging and 11 collector tests,
375 local documentation links and whitespace checks pass. The collector also
accepts the modeled service transcript after explicitly translating fixture
port fields; that replay is parser validation only. Native v33 lifecycle
acceptance, long-lived desktop service and responsiveness under I/O remain
pending. No battery widget or userspace interface is enabled in this block.

Native candidate **`a9be2dc78d213dee`**, artifact source **`1a12f2d`**, is the
v33 automatic service trial. Its exact staged native image passes GRUB/UEFI
rehearsal: unsupported Q35 is refused, the failure log is persisted and verified
offline, exFAT is clean, and failure remains halted. The hardware test keeps AC
and USB connected and needs no input; it saves/unmounts and returns only after
complete retirement. Each initialized lifetime must restore both retained event
enables, and the collector requires the reader/sample/retirement proofs.
Accepted v32 `aa976febab59c128`, v31 `13d8083de35b51af` and previous desktop
`b427b78f21644bd2` remain backed up outside build. The unreferenced v25 payload
`06c646455f08555e` was archived and hash-verified before reclaiming EFI space.
Collect the matching v33 session before another candidate; physical result is
pending, and this does not accept persistent desktop operation.

The v33 physical trial failed during namespace/EC initialization with the
five-second operation deadline: errors/denials zero, heap 674132 retained,
EC phase/error and queries zero. Both new sessions
`session-00065e3eecadaeef-00` and `session-00065e48a9f0e993-00` lack exit records;
the latter matches the supplied halt photograph. They are failures/incomplete
evidence, not service acceptance. Current repair scope is the diagnostic reader's
scheduling: its `HLT` waits keep it runnable during worker setup. Add a slower
VM initialization fixture, compare before/after blocking reader waits, and add
setup progress/timing diagnostics while preserving native deadlines and access
gates. Pro PR #5 advanced to `8fc3e1c` (accepted paired fan telemetry docs), with
no overlapping runtime work; integration base remains `b6075ef`.

Repair validation: a VM-only 3.5-second executing setup workload reproduces the
five-second halt with the old runnable reader. Changing only the reader's waits
to scheduler-backed sleeps completes that workload in 3.578/3.565 seconds
across two initialized lifetimes. Final native/VM/ordinary builds and all 12
affected VM cases pass, including the new load regression, all service modes,
normal diagnostic, setup/query/disconnect containment and guided transitions.
The 42 staging/collector checks and 376 local documentation links pass.
Setup now logs substage timing and containment reports worker progress. Native
deadlines, firmware gates and storage scheduling eligibility are unchanged.
The physical explanation and corrected service still require a hardware retry.
Failed evidence is retained outside build; the local USB is clean after the
halt (174 directories, 846 files), and the one-shot entry was consumed.

Corrected v33 candidate **`a3df674e8d6e5c65`**, artifact source **`c8be037`**,
passes exact-stage GRUB/UEFI rehearsal with expected native refusal, persisted
log verification, clean exFAT and persistent halt. It is prepared for the same
automatic lifecycle trial, with setup timing now visible. Previous desktop
`b427b78f21644bd2` and all accepted battery/interactive/power backups remain
verified. The older unreferenced `fed370ffbe33d554` payload was archived and
hash-verified for EFI space; failed `a9be2dc78d213dee` is preserved separately
as diagnostic material. Collect this new candidate's session before further
work. No physical acceptance is implied by the repair's VM results.

- Owner: local Air7,2 device agent; branch `device/macbookair7-2/reliability-input-power`.
- Objective: read-only desktop power/frequency baseline, then bounded native
  battery/AC runtime groundwork; CPU policy changes and suspend/resume follow
  measurement and separate acceptance.
- Shared scope: power telemetry/collector first; subsequent ACPI/EC runtime,
  worker/IRQ ownership and battery interfaces require explicit coordination.
  No new native EC/SCI enablement is part of the power capture.
- Integration baseline: `main` merge `b6075ef`; PR #4 merged after review and
  both exact-head CI runs passed at `869cbc7`. Merge tree matches the tested head.
- Power checkpoint: v26 `ed93e03b2585dcaf` accepted for bounded read-only
  telemetry, 6.458 W package average / 2,083 MHz CPU0 active over 52.999 seconds.
- Current implementation block: complete the connected EC/IGPU access audit,
  then build an opt-in native battery/AC diagnostic through a separate runtime
  adapter, serialized EC/SBS worker, owned SCI/GPE path, bounded lifecycle and
  durable test reporting. Scope includes runtime/transport, build/staging tests
  and guides. Keep normal desktop startup and accepted recovery unchanged.
  Dependencies: chipset/handoff `a51c4a5`, PM1 notification `2d29126`; integration
  baseline `b6075ef`. Pro11,2 PR #5 has advanced to `203e2d1` with exact Pro
  brightness and shared staging changes. Its profiles occupy v27–v30; this
  battery capture reserves **v31**. Native ACPI code does not overlap. Shared
  staging/test hunks need integration reconciliation without changing existing
  release recipes; neither branch is silently merged into the other.
- Native battery candidate: connected captured initialization now passes the
  offline model, including ECSS, GNVS lid/AC and IGPU CLID effects. The separate
  Air-only runtime performs five bounded SBS/AC samples, explicit connection,
  owned SCI/GPE handling and clean retirement; ordinary desktop retains a
  refusal stub. The same lifecycle passes two Q35 lifetimes and six injected
  faults (denied region, EC stall, storm, setup/query/disconnect deadlines).
  The native image refuses Q35 before hardware access. Host memory policy,
  transport/SBS/event/owner tests and collector/staging tests pass. Physical
  Initial VM checkpoint only; the first physical runtime result below supersedes
  native-startup uncertainty. Battery dynamics and general firmware handoff remain pending.
  See [native runtime gate](../native-ec-runtime.md#explicit-native-battery-diagnostic).
- PM1 notification checkpoint: all 65,536 starting words and each single-bit
  command mutation pass host/sanitizer checks. The Q35 release case performs
  exactly one actual emulated PM1 write, refuses permission replay, preserves
  readable control bits and cleans up through two lifetimes. Injected SLP_EN is
  rejected before any write and halts with state retained. Firmware Pending is
  modeled; this does not accept native firmware reception. Generic PM1 writes
  remain denied. Connected Air EC `_REG` additionally writes the IGPU OpRegion's
  CLID field; its mapping needs a captured layout/per-access audit before binding.
- Chipset/handoff checkpoint: read-only Linux capture confirms missing PCI
  IDs/configuration and RCBA/HPTC; all nine live firmware hashes match. Complete
  offline initialization now passes with an exact read-only DWORD model and
  captured PCI snapshots (zero errors/denials/leaks, no PCI writes on this path).
  Q35 modeled firmware-to-OS lock notification and an owner-side operation
  deadline pass. This accepts neither native SCI nor OS-to-firmware handoff;
  connected EC `_REG`, native access policy and controller ownership remain.
- Earlier Global Lock checkpoint: Q35 full-uACPI tests pass two acquire/release
  lifetimes, firmware-held lock containment and denied PM1 handoff detection.
  Owner drain now precedes interpreter reset; a stuck worker retains heap,
  Owned/Pending bits and the masked SCI callback. Exact fixture lock-word reads
  are allowed; the later PM1 notification fixture above adds a scoped release.
  Native contention/handoff, operation ownership and live mappings are pending.
- Earlier full initialization refusal checkpoint (superseded by chipset replay): offline `--initialize` refuses errors,
  warnings and denied accesses even when uACPI returns success; skips follow-up
  queries on refusal. Twenty-five synthetic tests cover opt-in execution,
  ignored errors, malformed status and prior load faults. Captured Air replay
  with two private GPIO byte models failed on HPET chipset memory and
  missing PCI snapshots (four AML errors / 11 denials / zero remaining
  allocations). GN46 maps to input-sensing disable; native access remains gated.
  Full-runtime FACS contention and PM1 Global Lock release need their own tests.
- Pipe-restart checkpoint (`3e70395`): the sending helper blocks its process-wide
  test signal so its own yields cannot consume it. The probe verifies the handler
  ran on the reader; 256 rounds with varied yields and the full 481-command CLI
  suite pass, with clean exFAT. This corrects the test protocol, not the broader
  POSIX signal implementation. The previously failing old-head CI passed on
  retry; the new repeated regression is retained in CI.
- Memory audit checkpoint: private memory/PCI access traces and budgets pass
  21 namespace tests normally and under ASan/UBSan. Captured initialization's eight writes only change OSYS;
  one PCI read and the constrained GPIO access are recorded. Separate `_SB._INI`
  correctly refuses an additional GPIO read at `0xa74` for `GN46`; its live
  meaning/ownership remain unaccepted. Input captures remain unchanged. See the
  [runtime guide](../native-ec-runtime.md) for exact scope and counts.
- SCI ownership checkpoint (`fbdf014`): the reusable owner now prepares a masked
  route, registers its callback, enables delivery, and retires/restores the
  route with bounded rollback. Host fault tests and ASan/UBSan pass, including
  immediate delivery, partial writes and poisoned-state retention. The separate
  Q35 runtime passes three SCI lifetimes and failed-install rollback after
  actual route preparation; early EC load passes two lifetimes with queries,
  SBS samples and disconnect cleanup. Blocked-worker and interpreter self-reset
  cases halt visibly with live state retained. These are VM/host results only;
  native source ownership and live mappings remain pending.
- Query containment checkpoint: a drain now retains its attempt/time budget
  across worker batches and additional notifications. Host/sanitizer cases pass
  for finite/repeated drains, queued and AML time, clock regression and sticky
  faults. The early-load VM regression passes; continuous and zero-reply storms
  each halt at 128 attempts, and a three-second AML query fails its two-second
  cooperative budget after returning. All retain live state with interrupts off.
  CI now runs the separate SCI/early-EC and query-fault VMs sequentially and saves
  their serial evidence. Native GPE masking/rearm and a worker that cannot return
  still need the external ownership/lifecycle path; no physical result is claimed.
- Event-register checkpoint (`843b66c`): complete supported FADT event layout
  decoding and exact-width/direction IO policy pass host tests and ASan/UBSan,
  including the captured Air FADT. Q35 now restricts fixed enables to its Global
  Lock/power button and enables no GPEs. SCI lifecycle and early EC/SBS/teardown
  regressions pass under the policy. An injected early write denial is refused
  despite upstream API success, then cleans up with zero heap before a normal
  start. Full startup clears all GPE status bytes: native ownership must cover
  that domain, and cleared pending events cannot be rolled back from a snapshot.
  Live native register/NVS/FACS/PCI/GPIO permissions remain unaccepted.
- Dependencies: early worker/handler lifetime fixture merged in PR #3; actual
  captured Air firmware, legacy/extended address consistency, namespace resource
  agreement, `_OSI` policy and SCI/GPE ownership must all be resolved before use.
- Resource checkpoint: [offline EC contract audit](../native-ec-runtime.md)
  matches captured ECDT/FADT to the loaded EC Device/HID/UID/GPE/CRS, with absent
  `_GLK`, zero errors/denials/leaks and private snapshots only. Generic table
  and namespace refusal tests pass. A separate PCI0 `_INI` replay refuses the
  GPIO mapping at `0x800`. The follow-up private byte replay completes PCI0
  `_INI` with one read/write at `0xb90`, setting only bit 3. The same replay
  passes a set-only bit-3 rule and rejects a wrong mask; native GPIO effects
  and ownership still need acceptance. Eighteen namespace tests and the pure
  per-access rule's exhaustive byte transitions pass, including ASan/UBSan.
  This is not controller ownership acceptance. The production build passes and
  its PT_LOAD contents match installed `ed93e03b2585dcaf` exactly.
- First native battery trial: v31 `cc486bf6b6581626`, source `8dc2408`,
  refused platform/resource preflight before runtime initialization. Session
  `session-00065e3d0d7c3f18-00` was copied and hash-verified after return;
  exit logs were saved and the USB unmounted before the persistent halt.
  No battery samples or native EC/SCI acceptance. The screen's `saved=NO`
  combined diagnostic failure with log saving and did not describe the saved files.
- Granular native trial `339211cceb5d894d` (source `e4c8d7e`) refused
  GPE enable byte 12 at `0x189c`, value `0x40` (GPE `0x66`), before EC/runtime
  ownership. Session `session-00065e3d35a068b6-00` verified; logs saved, USB
  unmounted, persistent halt photographed. One-shot consumed on return.
- Second enabled-event refusal: `c0f2c3af0dbacc66` (source `8dc82e7`),
  session `session-00065e3d7491a8fe-00`, refused GPE enable byte 14 at `0x189e`,
  value `0x04` (GPE `0x72`), with PM1 control zero. Logs verified and USB
  unmounted before halt. The GPE66 lease and battery runtime did not start.
- Earlier bounded scope: capture the complete fixed/GPE enable and status
  snapshot before admission; audit the second source (chipset wake-alarm timer)
  and extend the existing exact-source preservation mechanism only with evidence.
  All 12 battery VM cases pass, including two-source preservation and partial
  mask/retirement failures. Host admission/lease checks pass with sanitizers;
  collector (8) and staging (29) tests pass. Runtime, platform, collector and tests are this Air branch's scope; normal desktop unchanged.
- Existing handoff implementation passed host/sanitizer and full-runtime VM
  pending-status preservation, failed mask/restore and unexpected reenable
  containment. Those are model results; neither native handoff nor battery
  readings were accepted at that checkpoint. Candidate `6a2f90f72a0e4b96` / source `ac033a5`
  passes its exact GRUB/UEFI Q35 rehearsal: expected native refusal, verified
  saved log bytes, clean exFAT and persistent halt. Next is the AC-connected
  physical trial; full event snapshot and both retirement records are required.
- Integration remains `b6075ef`; Pro PR #5 is at `fd6d2a2` with input queue residence
  diagnostics and bounded queue work. Shared startup/staging hunks require later
  integration reconciliation, not concurrent changes to the battery mechanism.
- Native trial `6a2f90f72a0e4b96` / `ac033a5` passed preflight and masked
  both retained sources, then halted during namespace load: the captured CPU
  SSDT requests PNVS length `0x53` (83 bytes), but the adapter used decimal 53.
  The photograph records this denied mapping, a later PM1 enable denial
  (`0x120` at `0x1802`), and the contained reset attempt;
  no samples or restoration completed. Matching session
  `session-00065e3db0a6917a-00` has a verified boot log, no exit log.
  Next bounded scope: correct PNVS admission/mapping/read bounds to the captured
  length, add independent boundary coverage, and rerun native staging/VM checks.
  Scope also checks the post-mode fixed enables: the later Global Lock write
  suggests firmware armed power-button bit8. Mask only this exact bit before
  namespace load, without status acknowledgement or widening permitted enables;
  unexpected sources halt. All 14 battery VM cases pass; the final fixture
  also verifies both mode cases after actually placing Q35 in legacy mode.
  Host/sanitizer mapping/read boundaries, connected captured-firmware replay,
  9 collector and 29 staging tests pass. Missing exit snapshots now receive an
  explicit incomplete-session flag. Read-only USB fsck after this halt was clean.
  Follow-up v31 `13d8083de35b51af` / source `57de926` passes the exact staged
  GRUB/UEFI Q35 rehearsal (expected native refusal, saved log bytes, clean
  exFAT, persistent halt). Next physical trial remains AC-connected and requires
  complete samples, both event restorations and clean capture/unmount.
- Recovery before this candidate: current `c0f2c3af0dbacc66`, previous
  `b427b78f21644bd2`; accepted power and interactive backups are verified outside
  build. Retain that previous release when installing the next candidate; the
  local run record owns installed/armed state.

## Active transition-test block

Air branch owns a separate guided AC-connected → battery-only → reconnected
capture, with three fresh consistent samples per phase, bounded waits, clean
retirement even when the user misses a phase, durable phase summaries and strict
collection. Scope: battery diagnostic/runtime, a pure phase checker, v32 staging,
VM fixtures, collector and guides. Preserve accepted v31 and ordinary desktop.
Baseline remains `b6075ef`; Pro PR #5 at `522c36a` now uses desktop v31 as well
as its prior profiles. The recipe prefixes remain distinct; do not rewrite
installed v31 recipes. Reserve **v32 laptop-probe** for this Air transition test;
shared main/staging integration still needs reconciliation. No new physical
acceptance or reboot is implied by source changes. Next checkpoint is host/VM
validation and an exact-stage rehearsal, then a guided physical round.

The v32 implementation brackets each battery sample with AC reads and requires
three consecutive coherent observations per phase. Unplugged acceptance requires
negative battery current; connected phases require nonnegative current. Each
phase has a 120-second deadline. Timeout retires normally, saves an incomplete
report and halts; hardware faults retain immediate containment. Final phase
proofs survive a bounded log tail, and collection requires both exact event
restorations and balanced teardown. Host phase tests (including ASan/UBSan),
30 staging tests, 10 collector tests and documentation links pass. Modeled
transitions and a real missed-unplug timeout followed by successful runtime
reentry pass. All 16 battery VM scenarios pass, including existing transport,
query, event-restoration and mode-entry containment. Exact staged **v32
`aa976febab59c128`**, artifact source `c25b32c`, passes the GRUB/UEFI rehearsal:
expected native refusal, saved refusal bytes, clean exFAT and persistent halt.
None of these results accepts actual AC changes.

That candidate is now installed for the guided round. Start connected to AC;
unplug and reconnect only when the persistent instructions request it, leaving
USB attached throughout. Successful retirement/save/unmount returns automatically;
timeout or fault halts visibly. Previous desktop `b427b78f21644bd2` is retained.
Accepted bounded diagnostic `13d8083de35b51af`, its evidence, and interactive/power
recovery copies remain outside `build`. The redundant, unreferenced failed
`6a2f90f72a0e4b96` EFI payload was archived and hash-verified before removal for
space. The local run record owns actual armed/reboot state; on return collect
the matching v32 session before another trial. PR #6 remains draft.

### Guided round collected — October 20

Two new sessions for `aa976febab59c128` were copied and hash-verified on return:

- `session-00065e3e608567ed-00` has complete three-phase proofs in `exit.log`,
  17 samples, 259 queries / 260 notifications, one attachment/detachment,
  both retained event enables restored, and zero errors/denials/heap. AC-connected
  current was 0 A; the accepted discharge sample was −1.455 A at 8.190 V;
  reconnection returned to 0 A at 8.335 V. Each phase has three consecutive
  coherent samples. The collector reports transition/runtime completion with
  no attention items. Relative SOC remained 98%; final remaining/full capacity
  6.107/6.293 Ah matches Linux after return.
- `session-00065e3e400fee10-00` has boot and periodic logs but **no exit
  snapshot**. The last saved state, around 44 seconds after startup, was waiting
  for AC unplug with AC still present. Its cause is unresolved; do not count it
  as a pass or infer a clean timeout.

The Ubuntu boot ID changed, the one-shot was consumed, and read-only exFAT
checking is clean (172 directories, 844 files). Both sessions and the exact
payload have hash-verified copies outside `build`. The user confirmed the latest
round was good and explicitly excluded earlier attempts from this acceptance.
The bounded AC-connected → discharge → reconnect milestone is **accepted on this
unit**, against the latest session only. The incomplete session remains historical
evidence and is not promoted to a pass. This does not establish calibrated gauge
accuracy or a persistent desktop service.

The v32 diagnostic payload and accepted evidence are now retained outside
`build`, alongside the v31 diagnostic and desktop recovery. No new reboot is
armed. Next is a cached battery/AC provider on the serialized worker, with explicit
lifetime, stale-data reporting and input/storage responsiveness validation before
ordinary desktop integration.

## First completed native battery diagnostic — October 20

Release `13d8083de35b51af`, artifact source `57de926`, completed five samples
in session `session-00065e3de26bb9fa-00`. Three logs were copied and hash-verified;
the collector reports complete runtime evidence with no attention flags.

- Actual mode entry enabled fixed power-button bit8; the new mask/readback path
  completed before namespace load. No PNVS mapping denial remained.
- Five samples: AC present, relative charge 98%, 8.374 V, 0 A, remaining
  6.094 Ah, full 6.265 Ah, temperature 30.45 C. Voltage/current/charge/full values
  match Linux snapshots before and after. This is comparison, not calibration.
- 75 EC queries / 76 notifications; one region attachment and one detachment;
  zero errors, denials or remaining interpreter heap. Both retained event enables
  restored with protected-write counts; diagnostic returned cleanly.
- Ubuntu boot ID changed and the one-shot entry was consumed. Read-only exFAT
  checking was clean (170 directories, 837 files). Accepted diagnostic payload
  and full evidence have durable copies outside `build`; previous desktop
  recovery `b427b78f21644bd2` remains installed.

This accepts the bounded AC-connected initialization/sample/retirement path on
this unit. It does not accept a long-running desktop service, discharge, AC
unplug/replug, suspend/resume, independent gauge calibration or other Macs.
That accepted **diagnostic** `13d8083de35b51af` remains a durable recovery copy;
the later transition block above records the next candidate. Ordinary desktop
startup still has no battery runtime.

Linux reports 86% while the diagnostic reports 98%. SrvrOS reads relative SOC
(command `0x0d`); upstream Linux SBS reads absolute SOC (`0x0e`). These use full
and design capacity respectively. Linux design capacity is 7.150 Ah, consistent
with that distinction. Raw absolute SOC has not yet been captured by SrvrOS;
do not change the relative gauge merely to force equality. Sources and limits
are in the [native runtime guide](../native-ec-runtime.md#first-physical-completion).

The subsequent guided round above completes bounded discharge and AC-change
acceptance. Next is a cached desktop provider with clearly labeled gauge/health
data. Keep EC work on the serialized worker and preserve the accepted diagnostics.

## Accepted scope and open items

| Capability | State | Evidence / limit |
| --- | --- | --- |
| Desktop boot, rendering, keyboard, trackpad, USB documents | Physically exercised | User rounds and durable workspace history in the development journal |
| Terminal backspace redraw | User validated | `5282cfde546ea2b5`, fix `c6e5538`; reported stray characters no longer appear |
| Text Ctrl+C / Ctrl+V / Ctrl+A | User validated | `5282cfde546ea2b5`; copy, paste and select-all explicitly checked |
| Held-click second-finger steering | User accepted | `d61e60a82a295d84`; counters do not independently prove every handoff |
| Brightness and audible volume adjustment | User accepted | `b427b78f21644bd2`; requested brightness 16/83 applied, three tone tests completed |
| Temperature and fan sampling | Live output confirmed | Fresh screenshot readings 83 C / 1202 RPM; logs reached 85 C, no SMC errors; calibration/power behavior not accepted |
| Two-finger physical right-click | Implemented; physical acceptance pending | Press role latches from initial contact count; one-finger press plus later steering finger remains primary |
| Context menu from trackpad taps | Physical acceptance open | Exit counts 5 primary and 1 secondary tap; user reported no visible right-click result |
| System window stability | No recurrence in latest physical round | `fed370ffbe33d554`: System and Help exited cleanly, no collector attention flags; earlier timeout cause remains unproven |
| PCM files / headphones | VM-tested / physical confirmation incomplete | Volume report does not separately accept these routes/workflows |
| Package power / CPU0 active frequency | Physical diagnostic accepted | `ed93e03b2585dcaf`; bounded Text idle capture, no CPU controls or whole-laptop power |
| Battery/AC diagnostic | Bounded sampling and AC transitions accepted | `13d8083de35b51af`: connected sampling; latest `aa976febab59c128` session: three AC/discharge/reconnect phases, both event restores and clean retirement; persistent desktop service pending |
| Suspend/resume | Not implemented | Separate ownership and acceptance milestone |
| Optional isolated AML probe | Native completion accepted | `4e5f438c4069752b`; resource queries, zero errors/denials/leaks; v22 still omits this diagnostic |
| USB Ethernet | Implemented CDC ECM, VM-tested | No actual adapter accepted |

## Latest interactive physical record

Release `5282cfde546ea2b5`, v22, source `dad7aa8`, contains the Terminal cursor
correction `c6e5538`. The user reports that the character artifact appears
resolved and explicitly validated Ctrl+C, Ctrl+V and Ctrl+A in Text. Those two
workflows are now physically accepted on this Air.

Matching session `session-00065e37be14b48c-00` has four verified logs, a saved
exit snapshot, clean desktop process exit and no collector attention flags.
There were no new screenshots in this round. Offline read-only exFAT checking
was clean (164 directories, 822 files). Evidence and the run record are local
under `build/native-desktop-trials/5282cfde546ea2b5/`.

At that interactive checkpoint installed was `5282cfde546ea2b5`; previous remained accepted recovery
`b427b78f21644bd2`, and no one-shot boot is armed. Secondary press/tap cases were
not individually confirmed in the report; power telemetry was not enabled.

## Earlier October 19 interactive round

Release `fed370ffbe33d554`, v22, source `c6e50ec` (same tree as merged baseline
`e2d172b`), was exercised on October 19. The user reported generally good
behavior, with a new Terminal backspace display defect. Session
`session-00065e37230cc95f-00` has four verified logs, a clean desktop process exit
and no collector attention flags. Two saved screenshots are retained locally in
`build/native-desktop-trials/fed370ffbe33d554/physical-after/`.

System and Help remained present in the screenshots and exited without failure.
PCM playback completed 1,152,000 bytes with zero reported underruns; the user did
not separately confirm channel separation or headphones. The Sensors screenshot
shows 67 C core/package and 1,209 RPM with fresh sample ages. The offline exFAT
check was clean (163 directories, 818 files). General success does not establish
every gesture case: the final two-finger tap counter was zero, so specific
secondary-tap acceptance stays open.

At that checkpoint installed was `fed370ffbe33d554`; previous remained accepted recovery
`b427b78f21644bd2`. The one-shot boot selection was consumed and no next boot is
armed. The Terminal screenshot's stray slash was reproduced in the host editor
and actual desktop VM: erasing padding left the output cursor beyond the edited
line. The source correction restores that cursor; the subsequent physical
retest above accepts that correction. See the [journal](../desktop-progress-2026-10-19.md).

## Earlier interactive physical record

Release `b427b78f21644bd2`, v22, built from source `5b1ed05`.
Session `session-00065e281812e807-00` has saved logs and clean desktop exit.
Local evidence is in `build/native-desktop-trials/b427b78f21644bd2/physical-after/`.
The detailed [journal](../desktop-progress-2026-10-18.md) preserves failures,
partial acceptance and the previous implementation sequence.

At this checkpoint current is `b427b78f21644bd2`, previous is `f05dfcc01c28dc85`,
and the independent held-drag backup is under
`~/.local/share/srvros/known-good/d61e60a82a295d84`. Re-read actual installed state
before any action; these are historical identifiers, not values for other Macs.
The local trial record marks this run returned/reviewed, not armed for reboot.

## Next actions

Current shared-code work is on `device/macbookair7-2/reliability-input-power`
from `main` at `3b0b43a`: physical click latching, compositor delivery deadlines,
idle/sleep scheduling and console polling. The [October 19 journal](../desktop-progress-2026-10-19.md)
records host/VM evidence. Native automated batch `54a944ae1f7849ac`
(source `686edd8`) passed all five filesystem/shell rounds with retained files
verified and a clean offline exFAT check. The following bounded console-idle capture `1fe693c9889f131f`
(source `b3f0e31`) saved 60 seconds of samples: 94.55% idle, package 80→75 C,
1,206 RPM final fan, no SMC errors. It returned automatically with a clean
offline exFAT check. Subsequent isolated AML capture `4e5f438c4069752b` (source `17c6ed3`)
completed cleanly in 5.13 seconds and returned with clean exFAT. Unattended
desktop capture `06c646455f08555e` (source `e6fd681`) subsequently measured
60 seconds with Text open: 94.15% CPU0 idle samples, one frame presentation and
76,384 framebuffer bytes during the measurement. All four logs and the 1440×900
BMP were verified, with clean automatic return and offline exFAT. At that earlier checkpoint the current boot
payload was the desktop-idle diagnostic, with accepted desktop
`b427b78f21644bd2` retained as previous. The newer interactive record above
supersedes that installed-state snapshot; power consumption still needs
physical acceptance. Timer samples measure CPU0 scheduler occupancy, not energy.
The generalized Linux collector completed with zero partial reads on this host.
Additional IRQ, semaphore and deferred-worker foundations now pass host/VM tests.
A separate full-uACPI Q35 kernel passes real SCI, contended AML mutex/sleep and
teardown tests, including worker-owned EC disconnect I/O and halting with
state retained on a poisoned controller or a disconnect deadline. It is not the
installed payload or a native EC/battery provider. Its immutable `6b7c2bc8`
artifact passed a verified 30-minute, 37-round runtime/EC soak. Later explicit
synthetic namespace initialization checks worker ownership and refuses readiness
when firmware methods log denied accesses even if the API returns success.

1. Physically accept two-finger secondary click while preserving established primary drag; host/transport tests precede this check.
2. Follow up the System timeout after the delivery fix; the native cause is not yet proven.
3. Extend the accepted console and unattended desktop idle measurements to interactive activity and actual power/frequency evidence, retaining USB/input responsiveness.
4. Battery/AC runtime, headphone route acceptance and actual USB networking.
5. Shared inventory/profile/scaling work for the other model agents, coordinated
   through the [expansion plan](../apple-hardware-expansion-plan.md).

Shared desktop preparation now includes explicit integer 2× scaling. UEFI VM
checks passed at 2880×1800/2× and the Air's 1440×900/1×, including exact capture
pixels and input/window geometry. The installed Air capture exercised the default 1× path; the additional Pro
models still require their own framebuffer and input acceptance.


Read-only [package-energy and CPU0 active-frequency telemetry](../power-telemetry.md)
has host arithmetic/service tests, VM refusal evidence and the accepted
October 19 v26 native capture. It remains opt-in. The Linux MSR capture confirms register availability
on this host but is not an SrvrOS power result. No CPU power controls have changed.

Trackpad transport/palm faults now cancel a held interaction rather than deliver
an ordinary release. The compositor also preserves queued typing on pointer
cancellation. Host fault tests cover both paths; native acceptance is pending,
and this is not claimed as the cause of earlier unexplained window closures.

The generalized Linux inventory now includes `cpu-policy.json`. The October 19
capture had no partial reads: four online logical CPUs, `intel_idle`/`menu`, and
four `intel_cpufreq`/`schedutil` policies with 500–3,200 MHz limits. CPU0 listed
POLL through C10 as enabled. These are Linux policy observations, not SrvrOS
energy or deeper-idle acceptance. The [power guide](../power-telemetry.md)
documents units, non-atomic capture and missing-field semantics.

## Latest automated power record

Release `ed93e03b2585dcaf`, source `0f2038a`, session
`session-00065e38449c2146-00` completed its 60-second Text idle capture and returned
to Ubuntu. Valid power checkpoints cover 52.999 seconds: 6.458 W average package
power and 2,083 MHz CPU0 active frequency, with 94.10% scheduler-idle samples over
the full desktop interval. See [power telemetry](../power-telemetry.md#accepted-air72-desktop-baseline)
for units, conditions and limits. Four logs and the screenshot verify; no collector
attention or rejected power samples; offline exFAT clean (165 directories,
827 files). Evidence is backed up under `~/.local/share/srvros/evidence/ed93e03b2585dcaf`.

Current installed is this automated diagnostic; previous remains accepted
`b427b78f21644bd2`. The one-shot is consumed. Interactive `5282cfde546ea2b5`
is independently backed up. No additional physical trial is armed.
