# MacBookAir7,2 / A1466

Updated: 2026-10-31. State: **active reference machine; partial device acceptance**.

### Native workspace refusal and preflight follow-up (2026-10-31)

Release `e3370f00781fb60c` halted before the automated workload: the workspace
mounted read-only and no new session was saved. The supplied photograph records
write refusal and failed workspace retirement, not an endurance pass. All 1210
prior files retain their exact hashes; offline fsck, allocation and headroom
checks pass. The primary volume flags remain `2` (dirty) even when unmounted;
backup flags are zero and the bitmap has the expected 59149-byte size. The time
and cause of the dirty flag are not established by this run. Do not clear it to
make the importer accept writes.

The Air agent owns a bounded preflight follow-up in `exfat_host_driver.c`,
`check_exfat_allocation.py`, `test_exfat_boot.py` and the Ubuntu guide: expose a
read-only check of the production importer's write eligibility. Allocation
consistency and capacity alone did not catch this write refusal. Keep the guest
filesystem gates unchanged; preserve media and rehearse any proposed recovery
on a disposable copy before modifying the physical drive. No new reboot is
armed. PR #252 resolver/socket and PR #19 Wi-Fi remain independent.

The follow-up passes 65 boot/sector/write-eligibility cases and both partition
selection tests; all 745 documentation links pass locally. The physical drive
reproduces `writable-check: REFUSED` despite a passing allocation graph. Boot/FAT
and every allocated cluster are preserved outside `build`. A disposable-copy
`fsck.exfat -y` changes only primary flag byte106 from2to0; after verifying the
physical source still matches that backup, the same filesystem-tool recovery
produces an exact allocated-range match to the rehearsal. All1210 prior files
remain unchanged; offline fsck/allocation/write-eligibility/headroom pass.
Installed candidate and independent recovery are verified; Ubuntu default is
unchanged and the one-shot is consumed. No endurance work ran and no retry is
armed. The dirty flag's origin remains unknown. A retry needs a separately saved
run record and clean offline write-eligibility check before reboot.

The user authorized a separate automatic endurance retry after recovery. It
uses the unchanged exact-VM-tested `e3370f00781fb60c` payload from `d299c6ac`,
with host preflight tools at `3d616681` (PR #255). Preserve the failed preparation
attempt. The retry record is `build/air-io-admission-20261031/native-retry1/run.json`;
require fresh prior-file/session baselines, verified recovery/default and offline
fsck/allocation/write-eligibility/headroom, then leave USB unmounted through
Ubuntu shutdown. No human input is required. Collect the matching new session
before another stage; the 1800-second workload and existing deadlines remain
unchanged, and native endurance acceptance stays pending.

### Asynchronous file transport and model retirement scope (2026-10-31)

On branch device/macbookair7-2/desktop-runtime, baseline695db843 plus PR258,
the Air agent owns optional file-pool opener hooks and a small transport value
in desk_file_job.c/h, an explicit mapping-retirement callback in window_set.c/h,
and a Text/Files process-owned helper adapter. Create one helper before opening
GUI windows; it alone performs pipe/fcntl/spawn startup and queued model unmaps.
Release/acquire slots transfer small transport or mapping values, never mutable
GUI/job ownership. UI keeps FIFO jobs and bindings until validated responses
drain; pending start/error/full queues retain their owner and retry without
synchronous fallback. A callback may accept a mapping only after GUI disposal
and accepted work drain, with bounded pending records and completion/error
receipts. No allocation/free happens on a queue-full refusal. Join/drain the
helper before pool/session stack release and final child reaping. Keep legacy
synchronous pool users unchanged. Stop repeated no-op dispose tracing.
Touch directed pool/window/runtime tests and add blocked real-pthread adapter
coverage; run matched builds and loaded VMs under unchanged deadlines. No
kernel/firmware/driver or I/O-admission changes are planned; PR252 resolver and
PR19 Wi-Fi remain independent. Native a4 run is collected/integrity-verified;
no new reboot is armed. Publish test receipts before another physical trial.

The initial implementation passes the complete desktop host suite and matched
ordinary/native/VM builds. Directed real-pthread tests hold actual transport
construction and unmapping while UI admission continues, exercise the16-record
retirement backpressure, preserve failed mappings, close/reap an unconsumed
startup and retain storage through a transient join error. Pool FIFO pending/
error publication and window callback refusal/ownership-transfer tests pass.
Per-process service receipts are required by new synthetic VM checks; truncated,
interleaved, duplicate/conflicting, failed, pending or unjoined records refuse
acceptance. Actual unmap timing runs on the helper, not around queue admission.
Loaded/multiwindow VM validation is next; no new physical trial is armed.

Initial source0b64ded9 normal VM passes20cycles/UI128696us with complete
service/integrity receipts. Its populated50IOPS run remains FAIL: first Text
reaches10.006714s total deadline after helper initialization2.066s, canvas
disposal1.033s and helper join3.674s. Model retirement itself is18ms; the helper
joins cleanly and the child eventually exits0. Preserve this failed candidate.
Extend the existing userspace scope to an explicitly supplied64KiB helper stack
(the current pthread owned-stack policy otherwise allocates at least2MiB), and
an optional canvas retirement callback in desk_app/desk_connection plus the
window-owner layer. Queue canvas mappings alongside models, retain them on full
refusal, and record actual helper canvas-unmap phases. Keep default disposal,
pthread/kernel semantics and all deadlines unchanged. Validate callback refusal/
retry and supplied-stack lifetime through verified join before new VM trials.

Refined host desktop suite and matched builds pass after supplying the64KiB
stack and moving canvas unmap through the same bounded service. Actual host
pthread attributes are checked; canvas refusal retains pixels/connection and
retry does not destroy the surface twice. Main/helper timing lanes preserve
per-lane clock checks and full phase/lifecycle validation without treating valid
concurrent log order as a clock fault. New VM checks require actual canvas,
service-init and verified-join phases. No kernel or general pthread policy
changed. Loaded VM validation remains pending; no hardware boot armed.

Source23806300 still fails the first Text50IOPS cycle at10.005767s:
canvas disposal is now7.7ms but actual helper canvas/model waits are0.894/0.951s
and explicit join3.449s. All unmaps complete and join ultimately succeeds; keep
this failure. Extend only the app-owned lifetime contract: Text/Files keep one
static, nonreentrant runtime/service context until process exit. Stop, drain and
acquire a helper-body quiescence acknowledgement; no service/stack storage is
freed or reused. Then finish backend child retirement and return through normal
kernel process exit, which kills user threads before freeing process mappings.
The parent's verified wait still measures/accepts full process retirement.
Reusable/non-process-owned service callers keep verified join. Receipts must
explicitly distinguish process-owned quiescence from a joined reusable service;
never emit joined=yes for the former. Host tests must hold the thread after its
body acknowledges quiescence, prove storage stays live/nonreusable, then join.
No kernel, general pthread or lifecycle deadline change is authorized here.

Process-owned quiescence host checks pass: a real helper is held after its body
acknowledges stop, static storage/stack stay live, reinitialization is refused,
and the host fixture then joins before its own teardown. Reusable services
still require join and report a transient join error even after safe recovery.
Version2 receipts require quiescence and exactly one explicit lifetime mode;
only completed whole-process waits authorize the process-owned interpretation.
Full desktop host and matched builds pass. Loaded VM checks must still pass
before another test; no physical trial or kernel admission change is armed.

Source3e66c0cf50IOPS clears the first Text lifetime but FilesPID10 still fails
phase3 at10.003698s. Close reaches the app promptly (18.606s), but listing
submission occurs only after startup finishes (~23.25s); no header/channel had
been leased when Close arrived. Keep this failure. Add desk_file_job queued
read-only cancellation only for READ/LIST/STAT with no channel and zero sent/
received bytes. Unlink FIFO membership without fabricating a response or
reusing IDs; all mutations and accepted/partial requests retain ownership.
When cancellation leaves no queued demand, signal the helper's startup record;
the transport constructor checks between operations and closes acquired fds.
A late successful spawn stays owned and exits/reaps through EOF, never kill.
Explicit cancellation/unused-transport counters must account for every startup;
new version3 receipts distinguish this from transport errors. Validate FIFO
head/middle/tail, mutation/leased refusal, held construction cleanup and late
spawn cleanup. No kernel/firmware changes or deadline relaxation are planned.

Queued-read cancellation host checks pass for FIFO head/middle/tail, monotonic
IDs, STAT, and mutation/leased/partial refusal. A held real pipe constructor
cancels and closes its acquired descriptors without spawning; a cancellation
inside an already executing spawn retains and reaps its late child. A canceled
read does not stop a constructor still needed by another queued request. New
version3 receipt tests require exact request accounting and cancellation evidence.
Full desktop/matched builds pass before the final additional queued-demand fixture;
rerun that directed fixture before final VMs. Prior failed candidates remain
preserved. No new kernel admission, timeout change or physical reboot is armed.

### Native Files lifecycle return — delayed Close delivery (2026-10-31)

Releasea4f7080c77fda748/source0fe284e2 saves matching session and returns to
Ubuntu. It completes595 app cycles before FilesPID804 fails phase3 at10009854us,
roughly20minutes into the1800-second workload. UI maximum120070us, ready maximum
2332012us, completed-close maximum6738763us. This is an endurance FAIL, not a
30-minute acceptance. The third screenshot is incomplete; cache retirement is
zero bytes/zero retained/613reuses, with clean battery runtime retirement.

Saved atomic records place Close request at1236385064us and app receipt at
1242430551us:6045487us delivery delay. A distinct retained PID8 I/O-owner scope
runs1236382822–1241407892us (5025070us), overlapping5022828us of that interval.
Worker submission is logged just before Close receipt. This implicates synchronous
file-worker startup/submission on the app loop; overlap alone does not identify
its complete blocked syscall or an exact filesystem/USB operation. At the deadline
Files is still background with4730880mapped bytes and no queued reap. Once Close
is received, ACK takes20317us; document unmap takes131us with zero blocks and
final pool retirement115370us. Do not loosen the10-second lifecycle limit or
claim that metadata alone authorizes cleanup.

All1222 prior files are preserved apart from expected workspace metadata. Offline
fsck is clean (263directories/1230files);3468clusters show no leaks, bitmap/FAT
mismatches or errors. Write eligibility, headroom, current/independent recovery,
Ubuntu default and consumed one-shot verify. Eight new log/image/payload files
are copied and hash-verified. Native endurance remains unaccepted; no new boot
is armed. Keep PR #258 draft. Next scope is removing file-worker startup from
GUI dispatch while preserving accepted-job/descriptor ownership; also avoid
repeated no-op window-dispose tracing while a retired model drains. Publish the
chosen asynchronous ownership contract before implementation.

### Files Close and retired-child claim scope (2026-10-31)

Continue on PR #258/sourcea42c1ac9, integration695db843. The Air agent owns
Files window Close dispatch in `text_runtime.c` and a narrow exact-PID wait
prelude in process/scheduler/syscall code, with directed host fixtures. Permit
visual Files Close during read-only LIST/READ/STAT while retaining model/job
ownership until the accepted response drains; mutations keep their close gate.
For teardown, attempt only an already published reapable direct child with no
address space, mappings, kernel stack, descriptors or used thread records and
no scheduler/private-image reference. Under IRQ exclusion consume status and
accounting once, release the empty process slot/lifetime metadata, and defer
record clearing to existing gated allocation. No freeing, allocation, yield or
filesystem cleanup in this attempt. Other selectors/POSIX, incomplete cleanup,
foreign ownership/image alias and all exit paths retain ordinary admission.
No deadlines change. Validate ownership/refusal, repeated claim, accounting and
slot reuse plus loaded VMs before considering native tests. PR #252 resolver
and PR #19 Wi-Fi remain independent. No reboot is armed.

Candidate host validation passes full core/desktop and matched ordinary/native/
VM builds. Final focused checks additionally pass bad status pointers, unsupported
flags/POSIX prelude bypass, empty direct-child ownership and residue refusal,
single status/accounting/lifetime transfer, unchanged foreign I/O depth and
ordinary allocator slot clearing/reuse. Files read-only close guards preserve
busy jobs and keep mutation/Text gates. Final loaded VM validation is pending;
this is not native or whole-process endurance acceptance.

Clean565e2a14 populated100IOPS passes11cycles/UI285221us/ready3232283us/
close4657827us with full integrity/retirement. Its50IOPS run fails first Files
startup: queued listing starts a worker before first publication and client
awaiting timeout kills PID10/status143. The atomic final log is retained. Extend
this bounded Files scope to publish the initial window before advancing queued
file work; preserve accepted-job ownership and all client/lifecycle limits.

Final source0fe284e2 ordinary KVM passes23cycles/UI109766us/ready1151192us/
close3057594us. Two populated50IOPS runs pass10/9cycles, UI108375/109408us,
ready3842286/3258555us and close3718134/5370180us with complete PCM, screenshot,
prior-file, allocation, follow-up and cache retirement proof. Preserve the earlier
565 startup failure. Candidatea4f7080c77fda748 has an exact-stage Q35 refusal/
save/unmount/reset rehearsal; that does not accept native firmware or endurance.
All1222 prior USB files match the collected-return baseline; offline fsck,
allocation, write eligibility and headroom pass, flags0. Preparation first stops
at EFI space; an unreferenced old diagnostic is fully verified in cache/archive
before its EFI copy is removed. Current and independent recovery are preserved.
The new payload is installed with Ubuntu default unchanged. A separate automatic
1800-second native PCM/disk/app trial is prepared, with guest progress and clean
retirement/unmount/return; no input is required. Collect its matching session
before any next stage. Native endurance and physical interaction acceptance
remain pending. Raw run state and evidence are in the local ignored checkpoint
and reviewed-evidence backup; do not infer a pass from successful installation.

### Bounded anonymous retirement scope (2026-10-31)

From main `695db843` plus collected-return documentation, the Air agent owns
`scheduler.c`/`scheduler.h`, `process.c`/`process.h`, x86 syscall dispatch and
directed scheduler/process-service/syscall-I/O fixtures. Add a read-only scheduler
independence predicate, then an IRQ-masked whole-anonymous-region retirement
attempt. Require the current main thread, no live sibling/process thread records,
matching active address space, distinct foreign owner/private image, at most
4096 scheduler/mapping records and a complete region no larger than2MiB. Track
anonymous provenance through mmap, fork copying and region splitting; prevalidate
tracked physical translations. No service yield, allocation, file cleanup or
foreign ownership release occurs in the attempt. Any precondition refusal leaves
state unchanged and takes the normal gated munmap path. Unexpected unmap failure
must retain remaining frame/region ownership and never free a still-mapped frame.
No mapping, exit or threaded-cleanup blanket admission is authorized by this
scope. Existing syscall ABI and500ms/10second limits stay unchanged. Validate
negative ownership/provenance/bounds and futex/frame ordering before host/VM
load tests. Air6,1 idle work and open PR #252 resolver/PR #19 Wi-Fi remain
independent; no AP/idle/device gates change and no physical trial is armed.

Focused scheduler/process-service/syscall fixtures pass, including foreign
ownership retention, worker/sibling/image/owner/CR3/record-bound refusal, full
anonymous versus file/partial/split provenance, PTE translation mismatch, futex
invalidation before frame release, partial unmap failure with remaining frames
tracked, retry without double free and PROT_NONE retirement. Full core/desktop
and ordinary/native/VM builds pass before the final fallback-handoff refinement;
rerun focused admission and matched builds for that change. The active document
model's DWARF size is0xa1748 bytes plus owner metadata, fitting the2MiB bound.
No native result or general exit-latency fix is inferred; exit remains separately
gated and can still account for a whole-process lifecycle deadline.

Final focused admission and matched builds pass after preserving gated
fallback handoff. The first new fallback test attempted native CLI in the host
harness and faulted; a dedicated host IRQ adapter fixes the fixture without
changing production IRQ masking. That failed attempt is retained separately.
The candidate is ready for ordinary and populated100IOPS disposable VM checks;
physical acceptance and the broader exit/lifecycle deadline remain open.

Clean `e42f75c4` normal KVM passes25cycles/UI117595us/ready710004us/
close900428us and full integrity. The first populated100IOPS VM passes13cycles/
UI109420us/ready4081548us/close2660620us. Saved atomic records prove seven
normal and three throttled document-unmap phases fully inside distinct retained
I/O-owner scopes, with zero blocks (maximum465us/235us respectively).
A second identical throttled run remains FAIL at PID22's phase3 deadline
10006898us after10cycles; UI111477us, child metadata `exited`, mapped_bytes=0.
Document-unmap takes164us with zero blocks. This is not a stable endurance pass.
Extend this scope only with immutable queued/running/completed reaping timestamps
and a failure-time operation snapshot, plus host fixtures. Distinguish worker
queue delay from gated wait before changing reaping policy; keep old failures,
all ownership gates and test limits. No new physical trial is armed.

Diagnostic `d3af5211` passes two populated100IOPS VM runs (11/14cycles,
UI119637/110405us). A heavier50IOPS run fails10cycles/10001410us at Files
PID22: process still background with6115328 mapped bytes and no pending reap;
Close was received while operation3 was busy. Preserve this distinct failure.
Include `audio_io_stress_smoke.py` in the diagnostic scope: retain final atomic
session logs after clean QEMU exit before asserting the guest PASS receipt.
Failed workload collection must not emit a passing result or relax validation.

Collector `2ce59ceb` passes a populated50IOPS KVM run:11cycles,
UI109335us/ready3450870us/close5128973us with full integrity and retirement.
Four saved document unmaps wholly overlap foreign ownership, max903us with zero
blocks. A25IOPS diagnostic run fails at PID10's phase3 deadline10007513us;
the harness correctly exits nonzero, retains four final atomic session logs
including the failure-time snapshot, and emits no passing result. This validates
failure collection, not workload acceptance. Host collector34cases and745local
documentation links pass. PR #258 remains draft: independent anonymous retirement
is locally validated, while stable whole-process/native endurance remains open.
No new physical run is armed; preserve existing recovery and all failed runs.

### Native cache-reuse return — memory cleanup gate identified (2026-10-31)

Release `216844aaab4b2363` / source `695db843` saves matching session
`session-00065f294af061e4-00`. The trial fails after nine completed app cycles
at PID21's phase3 deadline (10008796us). Maximum UI loop is102131us; maximum
ready/close times are8523202/9220618us. The screenshot is incomplete and the
full1800-second workload does not complete. Cache retirement records zero
reserved bytes, zero retained buffers and eight reuses. Battery retirement is
clean and PCM records zero observed underruns; this is not endurance or audible
quality acceptance.

Five complete owner snapshots validate33 distinct long scopes. Text's document
unmap spans60011315–67138541us, including7125109us blocked and1982us ready.
Disk child PID7 owns the I/O gate from60010071–67136426us (7126355us). The
7125111-us overlap differs from blocked time by2us. This directly attributes
that wait to unrelated disk-worker serialization, without identifying a
particular filesystem operation or USB command. A separate9119227-us disk scope
also remains in the retained evidence. Next investigate independently safe
anonymous-memory retirement; do not admit arbitrary mapping/exit work during
filesystem ownership or extend test limits.

All1216 prior files remain unchanged apart from expected metadata. Offline
fsck is clean (261directories/1222files);3341clusters have no leaks, mismatches
or errors; write eligibility and headroom pass. Ubuntu default, consumed one-shot
and independent recovery verify. Partial screenshot/payload and logs are copied
with hashes and backed up outside build. Current candidate remains installed;
no next reboot is armed and native endurance remains unaccepted.

### Retired compositor buffer reuse scope (2026-10-31)

Continue draft PR #257 from `3a551549`. The Air agent owns compositor cache
policy, immutable operation diagnostics and host fixtures (`deskd.c`,
`operation_trace.h`, `test_desktop_session.c`, runtime contract). The saved
atomic checkpoint002 actually contains the complete prior FAIL summary: one
cycle, UI188596us, ready7442781us, close3211829us; it must supersede the earlier
conservative note about interleaved UART aggregates. It also records a pending
cache memory job between window admission and first publication. Preserve that
failure; no final recovered integrity or native pass is inferred.

Reuse at most two completed retired compositor buffers, counted within the
unchanged96MiB budget. Only the UI transfers these job/buffer references;
queued/discarded/failed allocations are never pooled. Published pixels remain
immutable until full copy success. Pressure and shutdown release retained
buffers through the existing worker and reclaim budget only after unmap.
Expose immutable reuse/retention counters. Validate repeated create/retire,
partial-copy isolation, pressure and full shutdown before normal/throttled VMs.
No kernel mapping/exit gate, deadline or workload change is planned. PR #252
resolver/socket and PR #19 Wi-Fi remain independent. No physical boot is armed.

The focused desktop fixture passes100 retire/reopen transfers with one original
mapping, no intervening frees, unchanged reservation and exactly one final
release. Actual COMMIT handling refuses to publish a partially copied reused
buffer, then publishes only a complete successful copy. The real96MiB pressure
fixture includes two retained buffers and reclaims their budget only after
worker unmap. Closing during queued allocation does not pool it. Full desktop,
matched builds and disposable VM validation follow; no native result is claimed.

Clean `26448c80` passes desktop/lifecycle hosts and matched builds. Normal KVM
passes23 cycles (UI120341us, ready1115152us, close1751348us), with26 observed
reuses. Populated100IOPS KVM passes13 cycles (UI380074us, ready2599839us,
close4860113us), PCM60seconds/zero underruns, verified files/screenshot/
allocation/follow-up and clean battery retirement, with14 reuses. This first
heavy pass does not establish stable endurance. Add an explicit post-cleanup
zero-reservation/reuse receipt and require it from the saved atomic exit log;
34 collector tests reject partial, duplicate, nonzero or unused retirement
receipts. Repeat the VMs at the final clean source before considering hardware.
No native trial is armed.

Final clean `75845008` passes34 collector tests and matched desktop/build gates.
Normal KVM passes22 cycles (UI135885us, ready1621458us, close1385299us), with
19 reuses and zero final cache reservations. The repeated populated100IOPS VM
passes12 cycles (UI119918us, ready2260003us, close5309400us),13 reuses, zero
final reservations, PCM60seconds/zero underruns and full verified file/screenshot/
allocation/follow-up/retirement evidence. All recorded cache snapshots stay
within the96MiB budget and two-buffer retention bound. Both throttled passes
are retained alongside earlier failures; no native result is inferred. The next
checkpoint is a separate exact-payload rehearsal and automatic1800-second native
trial, preserving current/recovery, prior files and offline write eligibility.
No input is required; success returns automatically, uncertain ownership halts.

PR #257 merged as `695db843`. Clean native stage `216844aaab4b2363` passes exact
Q35 refusal/save/unmount/reset rehearsal and is installed for a separate automatic
1800-second trial. Actual model/USB, independent recovery and Ubuntu default
verify. Fresh1216-file and75-session baselines are saved; offline fsck/allocation/
write-eligibility/headroom pass with clean volume flags. USB stays unmounted
through Ubuntu shutdown. No human input is required. Collect the matching new
session after return and require full workload, zero cache reservations with
actual reuse, preserved files and recovery checks. Native acceptance is pending.

### Simple pipe-close contention scope (2026-10-31)

From main `0386d8ed`, the Air agent owns a bounded process/syscall follow-up:
`process.c`, `process.h`, x86 syscall dispatch and directed process-output/
syscall-I/O fixtures. The slow VM's final Files app returns from pool retirement
at52.924s, then misses total lifetime at~53s while still active. Ordinary channel
EOF publication currently takes gated CLOSE syscalls despite touching only
fixed pipe/refcount/poll state. Add an atomic close attempt only for simple
read/write pipe endpoints with valid references and no queued rights. Revalidate
under IRQ exclusion in the operation; all other descriptors use the existing
gated close after fresh lookup. Keep rights/socket/PTY/duplex/VFS lifetime gates,
allocation/flush and failure handling unchanged. No SYS_EXIT gate change,
workload pacing, deadline extension or native acceptance is planned. First
validate exclusion/EOF/references and fallback admission, then full hosts and
normal/throttled disposable VMs. PR #252 resolver and PR #19 Wi-Fi are independent.
Installed e337/recovery9aa and collected native evidence stay unchanged.

Focused syscall/process-output fixtures pass: atomic pipe close during simulated
foreign I/O avoids the gate, fallback waits and reads the current binding, all
eight rights slots and unsupported types refuse without mutation, and aliases,
generations, buffered drain, EOF and final-reference release remain correct.
The existing20000-transfer pipe/rights, VFS writeback/failed-close, inheritance,
PTY, poll generation and stale-handle cases pass. Full hosts/builds and matched
normal/throttled VM validation follow; no physical result is inferred.

The first candidate normal VM passes21cycles/UI349604us and complete receipts.
Its populated100IOPS VM completes PCM,12app cycles, screenshot and follow-up
without a lifecycle deadline, but FAILs the UI bound at526147us (trial stage
524456us). Keep that failure. The terminal follow-up diagnostic was printed
synchronously inside the final UI trial turn; extend this scope to retain its
scalar receipt and emit it only after UI-loop retirement/timing capture. Actual
app/exit work stays in the measured loop and all limits remain unchanged. Rerun
both workloads; this is diagnostic output relocation, not acceptance of the
failed candidate or a native result.

Implementation `416847db` passes full core/desktop and ordinary/native/VM builds.
Diagnostic follow-up `b67a7ea1` passes desktop and matched builds again; all745
links pass. Final normal KVM passes22 cycles (UI399244us, ready1278077us,
close1891561us), PCM60seconds/zero underruns, full file/screenshot/allocation
and clean retirement. Final populated100IOPS KVM still FAILs: an atomic saved
checkpoint records cycle1 startup7442781us; PID10 later reaches phase3 deadline.
Only two checkpoints survive, and the serial aggregate summary is interleaved;
it is not promoted into a validated final timing/count receipt. Strict final
FAIL/unmount returns, but the harness stops before final recovered integrity
checks. Draft PR #257 retains both candidates' failures and the source-proven
pipe-close improvement without claiming stable endurance or native acceptance.
No physical stage/install/reboot occurred. Next work is remaining process
startup/exit admission under owned I/O, retaining500ms UI/10second lifecycle
limits. Neither a slower worst normal render nor workload variance is called a
performance improvement.

### Client termination follow-up scope (2026-10-31)

From main `d299c6ac` with PR #255 host-preflight work retained, the Air agent
owns `deskd.c`, `lifecycle_worker.h` and their host fixtures. Client/control/reply
timeouts currently invoke gated `srv_kill` directly on the compositor. Move
worker-backed termination to bounded release/acquire requests, retain PID
ownership until completion, then retire every matching client on the UI. Keep
queue-full retries, shutdown draining, process checks and deadline policy; no
kernel syscall eligibility widening. Emit immutable termination timing receipts
from the worker. Reproduce with a blocked host syscall and validate normal and
throttled disposable VMs. The native service stall's exact call is not proven;
other gated retirement/clipboard paths remain possible. No physical trial is
armed by this implementation scope. PR #252 resolver/socket and PR #19 Wi-Fi
remain independent; native release/recovery remain unchanged.

Focused host fixtures pass: a simulated 2.5-second termination delay leaves the
UI service clock unchanged until worker execution; clients retain ownership
until DONE, and duplicate/full/stopped queues do not issue direct termination.
A real pthread fixture holds the signal syscall and verifies UI-side publication,
exactly one execution and shutdown join ownership, including a failed signal
result. This fixes a source-proven gated UI path, not a claim that the native
stall has been uniquely attributed. Integrated host/build and VM checks follow.

Runtime fix `7847c37c` and contract `1d7a89e0` integrate preflight main
`a1a88ed7` as `0c6220af`. Full core/desktop hosts and ordinary/native/VM builds
pass using the documented host-tool wrapper; the initial invocation without it
failed because `xorriso` was absent from the default PATH. All745 links pass.
The old service with passive new-worker scaffolding fails the same delayed-kill
UI assertion, while the candidate passes. Normal KVM passes22 cycles with UI
120085us, ready789572us, close1323791us and complete audio/file/screenshot/
allocation/retirement evidence. Populated100-write-IOPS KVM remains FAIL after
10cycles at phase3/10006069us (UI102159us, ready3601643us, close3482020us).
Five checkpoints and full serial failure evidence are retained. Neither VM
executes the termination branch; the host reproducer supplies that coverage.
No matched native result or unique cause of the native spike is claimed. No
new stage/install/reboot occurred. Remaining storage/lifecycle admission work
must retain the500ms UI and10-second total lifecycle limits.

### Endurance retry return — client service stall (2026-10-31)

The recovered workspace admits writes and saves session
`session-00065f286ae7ca1a-00`, matching release `e3370f00781fb60c` / source
`d299c6ac`. Seven app cycles complete before PID18 is killed for a client timeout.
Maximum desktop loop is 2431516 us, with 2418029 us attributed to `service()`;
maximum ready/close times are 2921524/3250206 us. The 1800-second audio/disk
workload reports `completed=NO`; zero PCM underruns are observed before clean
battery retirement. The screenshot fails; its partial BMP and disk payload are
preserved separately. This is an endurance failure, not a pass or physical
input/audio-quality acceptance.

Five complete saved owner snapshots validate 37 distinct long scopes, including
a 6734599-us disk-child scope. These wall-time receipts do not establish the
precise service call or distinguish hardware delay from software contention.
Next work is client-service subcall/scheduler attribution and bounded UI work;
retain the 500-ms UI and total 10-second lifecycle limits.

All1210 prior files remain intact apart from the expected metadata update.
Offline fsck is clean (259 directories,1216 files); allocation checking reports
3293 clusters with zero leaks/mismatches/errors, and write eligibility/headroom
pass. Ubuntu default, consumed one-shot and independent recovery verify. Raw
logs and copied partial artifacts are retained under the separate retry record;
no further reboot is armed and native endurance remains unaccepted.

### READY admission follow-up scope (2026-10-31)

From main `7e249356`, the Air agent owns a bounded scheduler follow-up in
`scheduler.c` and `test_io_scheduler.c`: request a quiescent turn when an ordinary
kernel thread is created, an unsafe continuation yields/is preempted, or an
unsafe blocked continuation becomes READY. Current
registration waits until an I/O selection refusal, leaving a newly ready thread
unregistered at the writer's preceding syscall boundary. A subsequent long scope
can therefore start before that continuation receives a kernel turn.

Add deterministic before/after creation and signal/timeout wake regressions,
plus preemption/yield coverage. Transport wakes of an active owner do not reserve
unneeded later turns.
Retain the existing rotating handoff cursor, ownership, address-space checks,
private-image exclusion, service eligibility and cancellation rules. No yielding
inside a live filesystem mutation, syscall/ABI or workload/deadline change is
planned. This does not bound device latency or admit unsafe work during I/O.
Validate focused and integrated hosts, then normal and throttled disposable VMs;
no physical acceptance is inferred. PR #252 resolver/socket and PR #19 Wi-Fi
remain independent. Previous native evidence/recovery remain unchanged until a
new matched candidate passes staging/preflight.
The final VM also exposed a collector race: it acted on a partial final UART
status line before PASS/unmount text finished arriving. Extend the bounded scope
to `audio_io_stress_smoke.py` and its host fixture: require a complete LF/CRLF
completion record, keep failure handling and time/workload limits unchanged.
Retain the incomplete collection and rerun; clean retirement alone is not proof
of the full saved-file workload.

The current regression fixture fails against original main `7e249356` before
any I/O refusal; corrected READY admission passes creation, preemption/yield,
signal/timeout, active/nested-owner, cancellation, safe-user/service and boot
cases. Full core/desktop hosts and matched builds pass at `38ac7cbb`; final
owner/preemption coverage and matched rebuilds pass at `7043a20f`. The collector
fix passes 33 host tests, including every completion-line split point.
Clean `bb93c5ee` normal KVM passes 22 cycles, UI 119829 us, ready 1032246 us,
close 1440827 us, zero observed underruns and full file/screenshot/allocation/
lifetime/retirement proof, with six retained live checkpoints. Five complete
saved owner snapshots validate 30 distinct long scopes. The 100-write-IOPS
populated workload still fails the total lifecycle deadline after ten cycles
(UI 110909 us, ready 6090692 us). Its final Files pool wait records 1471462 us blocked,
28672 us ready and zero eligibility exclusions. This is not an endurance pass or
a general device-latency bound. Retain that failure and the earlier partial-line
collection. PR #254 carries this bounded scheduler/collector fix; native
validation is the next separate checkpoint.
PR #254 merged as `d299c6ac`. Clean stage `e3370f00781fb60c` passes exact-payload
Q35 refusal/save/unmount/reset rehearsal and is installed for an automatic
1800-second native trial. The actual model, USB identity, prior current and
independent recovery are verified; offline fsck/allocation/headroom pass, with
1210 prior-file hashes saved. Recovery `9aa8dbf6fbda8252` and Ubuntu default remain
intact. The trial needs no human input. Collect its matching session after
return; the known throttled failure and physical endurance remain unaccepted.

### PR182 integration and local regression scope (2026-10-31)

The clean device branch fast-forwards to main `198c6a1c`, which merges PR #182
at `d7b2290c`. This supersedes the earlier integration overlap hold; the logger
barrier and bounded owner diagnostics are retained. PR #252 owns continued
resolver/socket audit and PR #19 Wi-Fi; neither is changed here. Native installed
payload/recovery and prior endurance evidence remain unchanged; no reboot is
planned for this integration block.

Run integrated core/desktop hosts and matched ordinary/native/VM builds locally
with Actions disabled. The first full run exposes an older scratch fixture that
requires corrupt media to mount before checking scratch refusal. The hardened
importer may refuse it earlier. Bounded repair scope is the host exFAT driver's
negative scratch mode and its Python fixture: accept import refusal or mounted
scratch refusal, but require zero writes/flushes and byte-identical media.
Retain positive scratch-range/remount coverage and production validation gates.
No filesystem, scheduler or driver policy change is planned for this repair.
The rerun also reaches GCC/glibc portability failures in libc socket host tests:
IPv6 aggregate nesting, const ancillary-header traversal and bounded Unix-path
copy warnings. Add narrow portable C fixes in `posix_socket.c` without changing
network retry/deadline/ownership policy; retain `-Werror`. PR #252 at `6f202ae5`
currently changes kernel resolver/query code, not these libc lines; its ongoing
resolver/socket audit remains independent.
After local gates, rehearse the combined desktop workload in a disposable VM;
physical endurance and the identified long I/O-owner fix remain pending.

Integration validation at `96215580` passes on Ubuntu GCC15.2: full core and
desktop hosts, storage leases, matched ordinary/native/VM builds and 745 local
document links. Clean `b6c4a8d8` adds only the host/guest path-capacity fixture
bound; its 19 socket modes pass again and runtime sources are identical. The host driver/fixture repair preserves both strict import
refusal and mounted extent refusal. Socket fixtures isolate guest entry points
from ELF host-libc exports; portable initialization, const traversal and validated
path copies retain strict warnings. Directed cases cover unchanged ancillary
inputs, host/guest path capacity and unchanged DNS answer storage on rejection.
No retry/deadline, durability, public ABI or scheduling policy is relaxed.

The matched ordinary image passes the network ABI VM. The Air7,2 KVM audio/disk/
app workload passes 21 cycles, maximum UI loop 114912 us, ready 1430050 us and
close 968706 us, zero observed PCM underruns, exact files/screenshot/allocation,
four retained live checkpoints and clean retirement. This is disposable VM
validation, not native endurance acceptance. No native stage, install, USB workspace
write or reboot was performed; historical recovery and the failed physical run
remain unchanged. PR #253 carries the narrow integration repairs. The next
runtime work remains bounded long-transaction/admission improvement on this
integrated baseline; PR #252's resolver/socket policy remains independent.

### Eighth endurance return — long owner identified (2026-10-28)

Release `b70f04d3166380e2`, clean `a8da6076` (PR #250), returned automatically
from `session-00065eeda3a00e66-00`. The first Text app (PID8) misses its startup
deadline: zero completed cycles, maximum UI loop 100908 us, zero observed PCM
underruns, incomplete screenshot and clean native battery retirement. Three
complete saved owner snapshots validate 13 distinct long scopes. PID7, the
separate disk-stress child, owns serialization from 18942270 to 26078440 us
(7136170 us, 6784 READY selection refusals). A subsequent logger save runs
26078942–29531547 us, including 3402316 us writing and 720 us syncing. Text first
enters main at 29531575 us, after its deadline. This identifies contributing
owners and intervals, not a particular syscall, command or CPU cost.

All 1206 prior files are preserved except metadata; offline fsck/allocation pass
(1210 files, 3246 clusters, zero errors). Ubuntu default, independent recovery
and consumed one-shot are verified. Installed remains this diagnostic candidate;
no new reboot is armed and endurance remains unaccepted.

Next work is a bounded storage/admission proposal, not another identical reboot.
A large file mutation currently retains the scheduler gate across device waits;
audited UI services may run, while new/retiring kernel continuations must wait.
Source review confirms the disk child uses ordinary 256 KiB write/fsync/rename/
readback/delete/sync cycles, and exFAT serializes complete mutations, including
dirty/clean flags and physical durability barriers. Do not shorten the workload,
weaken those barriers, expose partial metadata, widen unsafe kernel admission or
raise the existing deadlines. Establish which operation holds the long scope,
then separate long device work from short shared-state transitions with explicit
file/mount ownership, error retention and concurrent app regression coverage.
That implementation overlaps PR #182 (`c5e9d2ac`) in process/syscall/exFAT/block;
its owner remains independent of the user-assigned logger-only fix. Prepare the
reproduction/proposal and resolve shared ownership before conflicting integration.
PR #19 Wi-Fi remains independent. The remaining admission/performance fix is not
claimed by these diagnostic results.

### Seventh endurance return — scoped logger barrier (2026-10-28)

Release `3b5821fefe34b721`, clean `bb8561f7` (PR #247), returned automatically
from `session-00065eecca209c8d-00` after seven cycles. Files PID18 misses its close
deadline during final file-pool retirement: 5151107 us, including 4913448 us
ready, 1593 gate exclusions and 237401 us blocked. UI maximum 102541 us, zero
observed PCM underruns, incomplete screenshot and clean battery retirement do
not accept endurance. Measured logger sync is now 711–802 us; some logger write
intervals still take seconds. The earlier global-sync coupling is removed, but
this run does not identify the remaining gate owner. All 1200 prior files are
preserved except metadata; offline fsck/allocation pass (1206 files, 3239 clusters,
zero errors), default/recovery/consumed one-shot pass.

Next bounded scope: kernel-only long I/O-owner receipts in scheduler diagnostics
and workspace log snapshots, with focused scheduler/logger fixtures. Record stable
thread identity, process ID, sanitized worker name and interval, outside measured
scopes; no syscall/wire ABI, gate, cadence, workload or deadline changes. Preserve
PR #182 process/VFS audit and PR #19 Wi-Fi ownership. Consume actual main PR #249
before matched validation; its RC6 diagnostics do not confer Air7,2 acceptance.

Implementation `72454534` passes integrated core/desktop hosts, matched ordinary,
native and VM builds and PR #249 diagnostics. Normal Air7,2 KVM passes 23 cycles,
maximum UI loop 117491 us, ready 1120527 us and close 961646 us, zero observed
underruns, exact files/screenshot and clean retirement. Four saved complete owner
snapshots validate 24 distinct long scopes, including copied PID/thread identity,
interval arithmetic, ordering, bounded retention and consistent repeated records.
Live UART interleaving is retained as a diagnostic-stream limitation; use saved
atomic records. The 100-write-IOPS populated fixture still fails the total
lifecycle deadline (four completed cycles, UI maximum 111161 us). Its retained
atomic checkpoint validates three owner snapshots and 16 distinct long scopes;
a PID7 workload scope lasts 848646 us with 927 READY selection refusals. This
is VM attribution coverage, not native acceptance or a performance fix.
PR #250 merged as `a8da6076`. Clean stage `b70f04d3166380e2` passes exact-payload
Q35 native-ownership refusal/save/unmount/reset rehearsal and is installed for an
automatic native attribution run. All 1206 pre-existing file hashes and offline
integrity/headroom checks are saved. Recovery `9aa8dbf6fbda8252` remains independent;
Ubuntu stays default. No human input is required; collect this release's matching
session and correlate saved owner intervals after return.

### Sixth endurance return — logger invokes global file sync (2026-10-28)

Release `c98e4c162b411126`, clean `c23ed928` (PR #245), returned automatically
from `session-00065eeb62e3edb4-00` after seven cycles. Files PID 18 misses the
startup deadline while awaiting first pixels. UI maximum 101097 us, zero observed
PCM underruns and clean native battery retirement do not accept endurance.
Logger save 59755534–64831507 us takes 5075973 us: write 173052, verify 4232 and
sync 4898644 us. Its USB CSW callbacks total 4326717 us. This is workload wall time,
including concurrent traffic, not CPU or pure device latency. Source review shows
logger `workspace_sync()` invokes `process_file_sync_all()` before the device
barrier, so log persistence can force unrelated dirty process buffers to disk.
All 1194 prior files are preserved except metadata; offline fsck/allocation pass
(1200 files, 3191 clusters, zero errors), default/recovery/one-shot pass.

The bounded proposal adds a kernel-only committed-data flush API in
`workspace.c/.h`, logger use/explicit durability receipt, focused workspace/logger
fixtures and checkpoint VM harness compatibility. Explicit workspace sync and
unmount must retain global process-file sync; logger may flush only its already
written/read-back exFAT data plus the physical device barrier. No gate, cadence,
deadline, dirty-file ownership or failure-poisoning weakening. Main `46b60b24` /
PR #246 isolated diagnostic fixes are consumed, along with documentation-only
main `00338707` / PR #248. The user assigned the Air7,2 agent ownership of the
narrow logger barrier in PR #247. PR #182 at `c5e9d2ac` retains its separate
filesystem audit: preserve its block-claim/online-state checks and raw scratch
lease handling when integrating. The original no-argument full-sync helper is
retained for compatibility. This change does not take over process/VFS auditing.
PR #19 Wi-Fi remains independent. Ubuntu stays default with no one-shot armed.

The committed-data barrier proposal passes integrated core/desktop hosts,
ordinary/native/VM builds and documentation checks. Final wrapper preservation
passes focused workspace/logger tests and matched rebuilds. Source `7e3a325f`
passes normal Air7,2 KVM: 22 cycles, loop 115699 us, ready 1269412 us, close 940387 us,
zero observed PCM underruns, exact files/screenshot and clean retirement. Five
live checkpoints use the new matching durability marker. The 100-write-IOPS
populated fixture still fails cycle 1 total lifecycle (UI 109720 us); preserve it
as a remaining performance failure. No native barrier acceptance is claimed.
PR #247 merged as `bb8561f7` under that explicit user ownership assignment.
Clean stage `3b5821fefe34b721` passes exact-payload Q35 refusal/save/unmount/reset
rehearsal and is installed for an automatic 1800-second native trial. This VM
result does not accept native runtime or endurance. Independent recovery
`9aa8dbf6fbda8252` is retained; Ubuntu remains default. Prior 1200 file hashes and
offline filesystem/allocation/headroom checks are saved before the trial.
No human input is needed. Collect the matching session after return; retain the
known throttled-VM failure and unchanged lifecycle/UI limits.

### Fifth endurance return — ready-thread gate exclusion (2026-10-28)

Release `57d0ba0d7e1f2f71`, clean `b6f3dcad` (PR243 runtime), returned
automatically from `session-00065eeb1e2a58e8-00` after nine completed cycles.
The UI maximum is 101671 us; zero observed PCM underruns and clean native
battery retirement do not accept endurance. Observed Files final-pool spans are
253716–624531 us. Text pid21 document release takes 5862416 us, with 176216 us
blocked, 5686057 us ready, 2178 I/O eligibility exclusions and zero invalid
trace counts. It later reports exited/mapped0 before being collected. The
remaining large delay is readiness exclusion, not measured execution time.
All 1188 prior files are preserved except metadata; offline fsck/allocation pass
(1194 files, 3143 clusters, zero errors); default/recovery/one-shot pass.

The slow release starts immediately after a periodic logger snapshot at
53286891 us. This is correlation, not owner identification. Logger cost receipts
are currently suppressed unless CPU power telemetry is enabled, so this normal
v80 run cannot measure that snapshot's duration. Next bounded scope is
`kernel/src/workspace_log.c` timing eligibility and its focused host fixture:
measure existing save/write/verify/sync intervals whenever the monotonic clock
is ready, independent of optional MSR power service. Retain original writes,
readback, flush, cadence, deadlines and I/O gate. No process/VFS/driver policy
change is justified yet. Consume main `4ae360ef` including PR242 PCI/isolated
Haswell diagnostics and run the integrated build/host gates before new staging.
PR182 retains VFS/process ownership and PR19 Wi-Fi; neither is changed here.

Logger-clock implementation `71b6ae75` passes full integrated core/desktop hosts
and matched ordinary/native/VM builds, including PR242 PCI/package/platform
fixtures. Positive Air7,2 KVM passes 23 app cycles, maximum loop124974 us,
ready1152550 us and close1679743 us, zero observed PCM underruns and complete
file/screenshot/lifetime/retirement proof. Four completed logger-cost records
validate interval arithmetic and all matching USB aggregates with power service
disabled. These are workload wall-time receipts, not idle or CPU measurements.
Exact staging precedes the next automatic attribution trial; no endurance fix
or native acceptance is inferred from this instrumentation.

### Fourth endurance return — final file-pool retirement (2026-10-28)

Release `e7d06c056ba13a58`, clean `eef772a0` (PR240), returned automatically
from `session-00065eeabc2cba54-00` after seven completed cycles. Maximum UI loop
is 101621 us, zero observed PCM underruns, clean native battery retirement.
Files pid18 passes document release in 9290 us but spends 6469495 us retiring
its idle file-worker pool, exceeding the unchanged total lifecycle deadline.
Another release records 175219 us blocked and 348823 us ready (197 exclusions,
zero invalid); the final Files release records 9128 us blocked and 7 us ready.
The six-second document-release delay did not recur, so it has not been explained
by this run. Screenshot completion and full endurance remain failed. All 1182
prior files are preserved except metadata; offline fsck/allocation pass (1188
files, 3095 clusters, zero errors). Default/recovery/consumed one-shot pass.

Next bounded scope: explicit blocking final drain for an already-detached file
pool in `userspace/lib/src/desk_file_job.c` and desktools final teardown, with
host gate-deferral/interruption/ownership coverage. Interactive pool shutdown
stays nonblocking. Close all idle channels before waiting; retain each PID until
kernel wait returns that PID; never kill accepted work or discard an interrupted
wait. This reuses existing syscall admission, changes no kernel gates or limits,
and makes no claim to fix every lifecycle delay. Add scheduler attribution around
final pool drain. PR182 process/VFS and PR19 Wi-Fi remain independent; PR242
Air6,1 graphics/latency audit is isolated. Main `ac259fcb` / PR241 docs consumed.

Final-drain implementation `94c85456` passes full core/desktop hosts and matched
ordinary build. The focused fixture checks 32 deferred NOHANG attempts, EOF on
both channels before blocking, error/interruption PID retention and partial reap
without double collection. The first Air7,2 KVM attempt completes 22 cycles and
its screenshot but fails a 538224 us final exit loop (trial stage 536190 us).
Retain that failure; no exit-stage fix is claimed. An identical clean repeat
passes 23 cycles, maximum loop 119954 us, ready 1058614 us and close 1346273 us,
zero observed PCM underruns and complete file/screenshot/retirement receipts.
The throttled100-write-IOPS fixture still fails cycle1 lifecycle, with UI102344us
and Files final drain961197us (941002us blocked,19536us ready). Air6,1 control
passes complete file/screenshot/lifecycle/retirement evidence against94c85456
with only this unit Markdown changed. No universal latency or endurance fix is
claimed. Exact staging and native integrity checks precede the next candidate.

### Third endurance return and attribution scope (2026-10-28)

Release `5f4ed45c3634e22a`, clean `99e4412d` (PR238 runtime), returned
automatically from `session-00065eea4580440f-00` after nine completed app cycles.
The UI loop maximum improved to 101753 us, below the unchanged 500000 us limit.
Text pid21 exceeded the total lifecycle deadline: document unmap took 6067465 us
of wall time, after close acknowledgement and window disposal. Wall time alone
does not identify CPU work or I/O gate delay. Screenshot completion failed during
shutdown; zero observed PCM underruns and clean native battery retirement do
not accept the 30-minute endurance workload. All 1177 prior files are preserved
except expected metadata. Offline fsck/allocation pass (1182 files, 3047 clusters,
zero errors); Ubuntu default, consumed one-shot and recovery are verified.

Next bounded scope is current-thread scheduler attribution around document
release in `userspace/desktools/text_runtime.c` and its timing helper, with host
failure-path checks and positive/slow VM evidence. BEGIN/END surround only the
release, excluding diagnostic writes; unavailable or invalid traces stay explicit.
No memory syscall admission, teardown ownership, workload pacing or deadline
changes are authorized by wall time alone. PR182 retains process/VFS lifetime
scope and PR19 Wi-Fi. Consumed main `ec7bad42` (PR239 isolated Haswell package
audit); Broadwell gates and ordinary startup remain unchanged. Native endurance,
Broadwell AP/MWAIT and package idle parity remain pending.

Current diagnostic implementation `7052e1bc` passes full core/desktop host tests
and matched ordinary build. The Air7,2 KVM workload passes 23 cycles, 339196 us
maximum loop, 936454 us ready and 1246771 us close, zero observed PCM underruns.
Seventeen complete scheduler receipts have zero invalid counts. The slowest
release interval is 557948 us: 548975 us blocked and 8077 us ready. The separate
100-write-IOPS populated fixture still fails the lifecycle deadline after one
completed cycle (UI loop 104429 us); its Files release records 551251 us blocked
and 4177 us ready. These are VM scheduler-state observations, not native gate
causation or endurance acceptance. The Air6,1 control also passes with complete
file/screenshot/lifecycle/retirement evidence. Both VMs ran implementation
`7052e1bc` with only unit/parity documentation changes in the worktree.
Exact-stage checks precede another automatic native diagnostic run.

### Endurance retry — exited child reap and capture startup (2026-10-28)

Release `8003479c17f01644` (clean `96a8d307`, merged by PR236) failed early
in session `session-00065ee9cdb1fcee-00`: cycle 3 close deadline, process 13
reported `state=exited mapped_bytes=0`, but remained in the compositor child list.
The loop maximum was 1001940 us, including 995335 us in the post stage where
screenshot startup currently allocates/spawns synchronously. No PCM underruns
were recorded. This does not accept endurance or establish that every close
delay shares one cause. Preserve both failed releases and existing accepted
CPU0/v82 results. Collect/offline integrity and recovery checks precede new work.

Next bounded scope: `userspace/deskd` lifecycle worker, screenshot job preparation
and focused host/VM fixtures. Move terminal-child reaping to the existing worker
with one retained immutable PID/result record and kernel-owned blocking wait;
process-list `exited` is only admission evidence, never permission to reclaim.
Move screenshot allocation/spawn off the UI with an owned prepare/publish/copy
boundary. Keep accepted writes/resources until retirement, original deadlines,
workload intensity and all kernel I/O eligibility gates. PR182's process/VFS
cleanup and PR19 Wi-Fi remain independent; this block makes no kernel changes.

The retry's 1171 prior files are preserved except expected metadata; offline
fsck/allocation pass (1177 files, 3001 clusters, zero errors). Ubuntu default,
consumed one-shot and recovery are verified. The worker implementation now passes
full core/desktop hosts and a matched ordinary image build. Directed fixtures
hold worker reap/preparation calls stalled while the UI polls, verify unique PID
ownership and one-time result consumption, and reject image transfer before a
validated canvas copy. Positive and exact-stage VM checks remain pending; no
hardware acceptance is claimed for these changes.

Candidate `d534cec5` passes normal positive KVM audio/I/O workloads for both
Air7,2 and Air6,1, with full screenshot/app timing/retirement proofs. Air7,2
completes 23 app cycles, worst loop 119982 us, maximum ready 975600 us and
close 1211610 us. The 100-write-IOPS fixture remains failed on an active Files
close deadline; its worst loop is 110246 us. Preserve that failure; the change
does not claim arbitrary slow-media latency or native endurance acceptance.
Main `6cbfedab` / PR237 adds Air6,1 coordinated core-C3 acceptance documentation
only; it is consumed without changing Broadwell admission or this workload.
Window-command and exact-stage checks precede the automatic native retry.

Window-command VM checks pass after the worker changes: offscreen recovery,
window switching, minimized/dock lifetime, modal filtering, exact files, process
baseline and offline allocation. Exact v80 release `5f4ed45c3634e22a` (clean
`99e4412d`) passes GRUB/native Q35 ownership refusal/save/unmount/reset. The
native retry must collect matching completion, file/capture/lifecycle receipts
and integrity before acceptance. All 1177 existing files are hashed; independent
recovery remains `9aa8dbf6fbda8252`. No human input is required for this replay.

### Parity endurance — early native failure under investigation (2026-10-28)

Release `0d1ffb15c35f639a`, source `45b8647a`, returned automatically from
`session-00065ee96cb7cab7-00` after an early failure, not a 30-minute pass.
The seventh admitted app (Text, cycle 6) reached `main-return` but remained in
the compositor's child list beyond the 10-second total lifecycle deadline.
A concurrent 1440x900 screenshot was still in progress. Maximum desktop loop
was 250232 us, below 500 ms; recorded PCM underruns were zero. Screenshot and
workload completion failed; clean battery retirement does not accept endurance.
All 1165 prior files are preserved except metadata. Offline filesystem and
allocation pass (1171 files, 2953 clusters, zero errors); recovery and consumed
one-shot pass. Retain the failed logs and new files for diagnosis.

Bounded follow-up owns reproducing the lifetime/screenshot overlap in the
existing disposable audio-I/O VM and adding diagnostic attribution if necessary.
Do not relax deadlines, pace the workload or infer the kernel gate is at fault
from wall time alone. PR182 owns active process/file lifetime changes; inspect
its scope before any proposed shared kernel edit. PR19 Wi-Fi remains independent.
A slowed positive VM also fails, but at the loop bound rather than the native
close deadline; retain it as a different failure. Source review identifies a
separate testable fairness risk: I/O handoff restarts at the writer's slot, so
one repeatedly requesting waiter can win every gap ahead of another waiter.
Extend this scope to `kernel/src/scheduler.c` handoff selection and its host
fixture only if a deterministic multi-waiter regression reproduces starvation.
This does not edit PR182's process/file teardown, widen I/O safety gates or
relax diagnostic limits. The two-contender host fixture fails before the correction and passes afterward
with 16/16 admissions. Existing owner/eligibility regressions and full core/desktop
builds pass. The implementation rotates after the last admitted waiter, including
ordinary scheduling admissions. Normal Air7,2 KVM stress passes 23 cycles with zero underruns. The same slowed
100-write-IOPS fixture remains failed: a 679 ms loop and cycle 4 close deadline.
Thus this correction is **not a complete slow-I/O fix**. Retain both failures.
Add bounded read-only process metadata to diagnostic deadline receipts to separate
an active process from an inactive record with retirement/reaping pending. It changes neither ordinary
startup nor failure policy. The follow-up slowed VM records Text still `background` at its cycle 6
deadline, with 2.808 seconds in window disposal and 6.940 seconds to ready;
its loop maximum is 119593 us. This reproduces a total lifecycle failure but
is not identical to the native post-main-return delay. Preserve that distinction.
Main `7bad3681` / PR235's isolated coordinated AP/C3 mechanism is integrated;
its Haswell-only gate and separate MP-request kernel remain unchanged. Rebuild
and rerun merged host/VM checks before the exact staged diagnostic retry.

Merged-tree validation (source `96a8d307`) passes full core/desktop hosts, normal,
both native/VM Air kernels and isolated AP build. The Air6,1 KVM control passes
PCM/disk/app/screenshot verification and normal retirement; the ordinary Air7,2
kernel has no MP request. Exact v80 release `8003479c17f01644` passes GRUB/Q35
native-ownership refusal, save, unmount and reset. This is rehearsal, not a native
endurance pass. The matching retry preserves the original failed session and all
1171 current files, retains independent recovery, and runs automatically with
unchanged limits. No human input is required; collect before any further trial.

### Air6,1 parity — native desktop power accepted (2026-10-28)

Automatic v82 release `775c5051149c0e38`, compiled `0cbbb275`, completed
`session-00065ee927f533d1-00` and returned to Ubuntu. All three 60-second
idle/apps/recovery phases have matching power, thermal and worker checkpoints.
Thirteen app cycles completed; maximum desktop loop was 116162 us, ready time
1470959 us and close time 103350 us under unchanged limits. Native battery
controls passed and runtime retirement reported zero heap/errors/denied accesses.
Covered package averages were 6827/6722/6666 mW, with package temperatures
72–74/71–72/70–71 degrees C. These are bounded measurements, not energy savings.
All 1145 prior files are unchanged except `.srvros/meta`; offline fsck and
allocation pass (1165 files, 2905 clusters, zero leaks/mismatches/errors).
Ubuntu default, consumed one-shot and independent recovery are verified.
Evidence: `build/air-parity-20261028/native-power/`, also copied outside build.

Main `7b0cc468` / PR233 adds Air6,1's independently accepted AP rendezvous
record; it changes documentation only. That acceptance does not admit Broadwell
AP/MWAIT. This unit's next bounded scope is v80 automatic **30-minute** PCM,
verified disk, app lifecycle and screenshot endurance. No human input is needed;
progress/results must be saved and the machine must return automatically on
clean completion. Preserve independent recovery and all prior files. Attended
input/audio-quality acceptance, AP/MWAIT and deeper idle remain separate.

### Air6,1 parity — CPU0 telemetry accepted; desktop phases next (2026-10-28)

Consumed main `5a94d755` (PR230), then `80b0a8aa` (PR232) during validation.
Compare shared model support and independently
recorded acceptance against Air6,1; preserve its active secondary-CPU ownership
investigation and PR182 filesystem/PR19 Wi-Fi scopes. This block owns Air7,2
parity documentation, explicit Air7,2 automated desktop reopen/audio-I/O/power
staging profiles, seven-counter Broadwell read-only telemetry and affected host/VM
fixtures. The [gap matrix](../air-profile-parity.md) records independent scope. Review Broadwell read-only idle
counter prerequisites from this unit's inventory before any admission; do not
copy Haswell MWAIT selectors or firmware permissions. Shared worker/event waits
are already present in main and need no duplicate implementation.

The initial Air7,2 audio/I/O positive fixture exposed repeated desktop-loop failures
under both TCG and KVM (570/527 ms versus the unchanged 500 ms limit); PCM,
verified disk work and cleanup passed. The Air6,1 control fixture passed. Preserve
both failures. Extend this block narrowly to scheduler attribution around the
reported lifecycle interval before changing policy or claiming stress parity.
This does not assign a cause or relax any workload/deadline.

Candidate runtime `37744164` passes Air7,2 positive power, automatic reopen,
attended audio/unsaved-exit and CPU-refusal VMs. Audio stress passes both TCG
(26 app cycles, 122 ms worst loop) and KVM (23 cycles, 140 ms), after moving
launch receipt writes to the existing worker; a stalled-writer host fixture
checks UI-side progress and retained job ownership. Limits remain unchanged.
Full core/desktop builds and the window-command VM pass. Clean source `a1ff0b3a`
was rehearsed through exact native v79/v80/v81/v82 and CPU-load stages: all
unsupported Q35 ownership paths save, unmount and return safely. Earlier failed
runs remain separate; none is promoted to a pass.

PR232 adds an isolated AP probe after those checks. The merge preserves its
Haswell-only native gate, separate kernel and absence of ordinary MP requests.
Merged host/build checks and a fresh exact CPU-load rehearsal precede physical
use. Broadwell AP/MWAIT admission remains a separate evidence-backed follow-up.
The [gap matrix](../air-profile-parity.md) distinguishes these prerequisites from
already shared desktop/device mechanisms. First native test is automatic CPU0
idle/load/recovery, not a human input trial or the 30-minute desktop workload.
Accepted desktop `060fffa453e26d5e` and independent `9aa8dbf6fbda8252` recovery
are protected outside build. USB preflight is clean (1141 files, 2879 clusters)
and prior file hashes are saved. No physical acceptance of this candidate is
claimed. Run/evidence: `build/air-parity-20261028/native-cpu/` and its parent;
PR231 owns the parity implementation and logging correction.

After PR232 integration, complete core/desktop hosts and normal/native/VM builds
pass again, including 103 focused profile/collector/AP tests. Documentation
checks pass (694 links). Fresh CPU candidate `71049508c4a1fbd0`, clean source
`5bab654b`, passes exact GRUB/native-kernel save/unmount/reset rehearsal. Its
ordinary ELF has no `ap_probe_request` symbol. No AP or MWAIT profile is installed
by this trial. Collect its matching native session before another run; the new
counter mask/audit, desktop power phases and 30-minute workload each need their
own physical evidence. Local evidence is retained under the parity root, with
verified copies outside build before the one-shot boot.

Native CPU release `71049508c4a1fbd0` (compiled `5bab654b`, integrated by
PR231/main `951135b0`) completed session `session-00065ee8fc3ded29-00` and
returned automatically. Three ordered 30-second idle/load/recovery phases have
valid power and seven-counter coverage, paired HLT entry/return receipts and
unchanged E2/APIC/misc snapshots. Covered package power averages were
6782/8677/6825 mW; these are package measurements, not whole-machine power.
All seven residency deltas were zero. This accepts bounded read-only telemetry
and audit capture, **not deep idle or energy savings**. Other CPU state remains
unknown; no AP request or MWAIT was executed. Prior 1141 files are preserved
except expected `.srvros/meta` bookkeeping. Offline fsck and allocation pass
(1145 files, 2884 clusters, zero leaks/mismatches/errors); Ubuntu default,
consumed one-shot and independent recovery are verified. Evidence and hashes
are retained locally and outside build.

Next bounded native objective is automatic v82 desktop power phases: normal
Air7,2 battery ownership with idle/apps/recovery, worker receipts and saved
logs, followed by automatic Ubuntu return. No human input is required. Native
30-minute audio/I/O endurance and separate AP/MWAIT experiments remain pending.

### Shared baseline refresh — local validation complete

Consumed actual main `f5917467` (PR226), then documentation-only `a2a2c56f`
(PR228), on the existing device branch. All prior
Air7,2 implementation and acceptance commits are merged; the checkout was clean
with no local commits outside main. PR182 filesystem audit and PR19 Pro Wi-Fi
remain separate ongoing work, not accepted dependencies. This housekeeping block
owns current documentation and local validation only: rebuild matched normal/
native/VM artifacts, run core/desktop hosts and affected serial VM consumers,
remove stale future-trial wording and publish a coherent documentation checkpoint.
Installed accepted `060fffa453e26d5e` and independent `9aa8dbf6fbda8252` recovery
are untouched. New shared power/idle/screenshot behavior does not inherit the
older payload's physical acceptance. No stage, install or reboot is planned.
Local gates pass: `make -j2 test-core test-desktop all acpi-battery-kernel
acpi-battery-vm-kernel check-docs`, the Air6,1 synthetic VM kernel build,
window-command/navigation/dock VM (exact three files, process baseline and clean
offline allocation), ten-second console/desktop idle with post-idle titlebar drag,
and synthetic desktop power/battery app/recovery phases with clean retirement,
prior-file preservation, allocation and automatic guest return. Normal/native/VM
artifacts are matched to the unchanged runtime base `f5917467`. The first two VM
receipts include concurrent Markdown-only edits; no runtime source changed.
The power VM ran clean source `b21c6c58`. These checks establish local integration,
not Air7,2 power savings or fresh physical acceptance. PR229 carries this refresh.
Evidence: `build/baseline-sync-20261027/`, with a hash-verified durable copy outside
build under `~/.local/share/srvros/reviewed-evidence/baseline-sync-20261027/`.

### Focused Close/window-list trial — bounded hardware smoke accepted

Release `060fffa453e26d5e`, clean source `bd94284a` on PR201 main `5db81dd5`,
returned in tested `session-00065ed4b3827d6c-00`. The tester reports opening apps,
using modals/hotkeys and dismissing them normally, with no defects noticed. This
accepts the bounded attended Close/window-list smoke; it is not an independent
attestation of every Save/Discard/Cancel subcase, audio or active endurance.
The automatically opened Help checklist has no timer and remains available.

All four matching logs are copied/verified, exit is present and collector
attention is empty. Desktop lifetime drained to zero members (peak thirteen)
and released; battery retirement reports zero heap, errors, denied accesses and
rejected samples. One ten-byte test document was recovered, plus four logs;
there are no new screenshots. All 1,135 prior non-metadata files retain their
hashes, only `.srvros/meta` changed, and no files were deleted. Offline fsck,
allocation and headroom pass: 1,141 files / 2,879 clusters, zero leaks, bitmap
mismatches or errors. File persistence is observed; a fresh reopen is not
independently established by the retained logs.

The initial `session-00065ec9dc6970fb-00` was explicitly not tested and lacks
an exit log. It remains incomplete historical evidence, separate from this
successful retry. Installed payload, consumed one-shot, Ubuntu default and
independent `9aa8dbf6fbda8252` recovery are verified. The accepted payload and
bounded acceptance note are backed up outside build, with prior `1d36da62e183a295`
and archived/known-good a906 retained. Evidence and run state live under
`build/window-commands-native-20261026/` with a verified durable copy.
No next boot is armed. Actual main `3bbf80b7` was fetched/inspected after return;
its newer idle/power/screenshot work was not in this tested release and does not
inherit its acceptance. Build/test that integration before another native stage.

### Window commands and discovery — implementation and local validation

PR201 consumes actual main `a9d69554` (PR197/199), local merge `31d519f1`.
Implementation `3467439b` adds standard **Window > Close window / Command-W**
through the existing cooperative close request. Unsaved decisions remain app-owned;
repeated requests cannot bypass them, and closing an owned dialog cancels it.
Control shortcuts remain application-owned. Window/launcher lists now share
UTF-8-safe minimized labels, omit duplicate dialog tasks and mark the parent
when its dialog is focused. Existing dynamic storage and protocol v15 are reused.
No driver, audio, firmware gate or resource budget changes are included.

Full desktop/widget hosts and normal build pass on the consumed shared baseline.
Focused host and ASan/UBSan session gates pass modal close/cancel requests,
deduplication, Ctrl-W passthrough, parent checkmark, complete root discovery and
multibyte label truncation. The actual command consumer passes the prior movement/
dock/navigation checks plus unsaved Text close/cancel via Command-W, Preview close
through the Window menu, exact files, process baseline and offline integrity.
Terminal's real-shell/clipboard/Ctrl-C/close-cancel gate, pointer/lifecycle and
context-menu VMs also pass. Final Help shortcut prose and stronger UTF-8 fixture
were rebuilt/rechecked after those VMs; compositor behavior is unchanged.

Evidence: `build/window-commands-20261026/`, with a verified durable copy outside
build. The focused bounded native smoke is recorded above; broader subcases remain
scoped to their evidence. Installed accepted payload is `060fffa453e26d5e`;
independent recovery remains `9aa8dbf6fbda8252`. PR182 filesystem/PR19 Wi-Fi remain
separate. No next boot is armed.

### Combined navigation trial — bounded general smoke accepted

Release `1d36da62e183a295`, clean source `8847894a`, returned in
`session-00065ec78173ab2c-00`. All four logs were copied/verified; exit is present
and collector attention is empty. The desktop lifetime drained/released and
battery retirement has zero heap, errors, denied accesses and rejected samples.
The tester reports a general smoke test working normally, including Alt-Tab.
This accepts that bounded reported window/dock/navigation smoke, not exhaustive
completion of every guide subsection or a new save/reopen/audio/endurance test.

Only four session logs were added. All 1,128 prior non-metadata files retain their
hashes; only `.srvros/meta` changed. No new documents or screenshots were found.
Offline fsck/allocation/headroom pass: 1,133 files / 2,869 clusters, zero leaks,
bitmap mismatches or errors. Ubuntu default, consumed one-shot, installed payload
and independent recovery are verified. Accepted payload was copied/hash-verified
outside build with the bounded acceptance note; prior `a90629c8a2060e66` and
recovery `9aa8dbf6fbda8252` are retained. The unreferenced `986d6c68fa19ba18`
payload remains archived and known-good. Evidence/run state is in
`build/window-navigation-native-20261026/` and its durable copy. No boot is armed.

### Window switching and keyboard navigation — implementation and local gates

PR198 follows main `43aff7d5` (PR194). Source `8847894a` implements a stable
most-recent-window snapshot while Alt-Tab is held: Tab advances, Shift reverses,
Alt release activates and Escape cancels. Minimized roots can restore; owned
modals route through their document. Reused/retired identities cannot activate a
replacement window, and new admissions do not steal a held selection.
Command-grave / Command-Shift-grave visit the active app's roots in creation
order; Command-M minimizes the parent/modal group. Minimize/retirement choose the
most recent visible root without reviving other minimized windows. F10/Ctrl-F2
and the existing menu navigation expose desktop/app commands. The switcher uses
the reusable palette's selection-only mode and existing dynamic storage.

Matched normal/native/VM builds, full desktop/widget hosts, 82 installer/profile
checks and ASan/UBSan session coverage pass. An 80-entry host fixture checks MRU,
reverse/cancel, stale identity refusal, same-app order, modal minimize/restore and
all-hidden recovery. The actual navigation VM also passes PR194's offscreen/dock
checks, real held-key selection, exact saved documents, focus after close, process
baseline and offline exFAT integrity. Pointer/lifecycle, context-menu and legacy
compositor consumers pass. Screenshots of switcher and guide were inspected.

Candidate `1d36da62e183a295`, v76, was staged from clean `8847894a`. Exact
GRUB -> staged Limine -> native-kernel Q35 rehearsal passes automatic untimed
Help checklist delivery, saves, complete retirement, ordinary reopen and monitor
reboot with clean filesystem checks. Q35 correctly refuses native battery and
reports unavailable physical sensors; this does not accept native providers.
The first guide fixture expected log messages in the wrong order; corrected
coverage requires both complete markers in either order. Initial attempts remain
retained in `build/window-navigation-20261026/` and verified outside build.

The subsequent combined attended **Window navigation trial** at `1d36da62e183a295`
is recorded above as a bounded general smoke, followed by the focused Close trial
at `060fffa453e26d5e`. The staging and VM results here describe the earlier local
gate, not an outstanding reboot. Installed accepted is now `060fffa453e26d5e`;
independent recovery remains `9aa8dbf6fbda8252`. No boot is armed. Audio and
exhaustive edge/pressure coverage remain outside these window trials.

### Desktop movement and dock — implementation and local gates

PR194 follows main `ab7a7c93`, after closing PR193 with the tester's non-audio
report. Ordinary windows can move past the sides and bottom of the screen while
retaining a reachable titlebar. Center, tiling and maximize provide recovery;
owned dialogs still open visibly. Movement preserves surface dimensions.

The dock includes running unpinned applications, running/active/minimized marks
and main-window counts. Clicking restores or cycles main windows; secondary or
Control-click opens the existing launcher filtered to that app's windows and
New app. Owned dialogs retain focus routing. No new window table or limit is
introduced. Changes are confined to compositor/scene/input/launcher, Help and
focused fixtures; no audio, driver, kernel or protocol work is included.

Full core/desktop builds and documentation checks pass. Scene ASan/UBSan and host
coverage pass geometry extremes, clipping/stride guards, dock state, cycling,
modal routing and closing groups. The actual QEMU consumer passes left/right/
bottom movement, Center recovery, offscreen-parent dialogs, three-window cycling,
minimized restore, temporary Preview/Help entries, the filtered chooser and first
subsequent app click, ordinary launcher filter reset, exact saved files and clean
process retirement. Offline exFAT checks pass. Existing pointer/lifecycle,
context-menu and legacy compositor VMs also pass. Earlier fixture failures remain
in local evidence; the final consumer requires graceful QMP exit and returns zero.

Help contains an untimed **Window movement and dock trial**; the combined
**Window navigation trial** supplies automatic checklist delivery. The subsequent
bounded native general smoke is recorded above, separately from these local
fixtures. Installed accepted is `060fffa453e26d5e`, with independent recovery
`9aa8dbf6fbda8252`. PR182's filesystem audit and PR19's Wi-Fi remain separate.
Evidence: `build/window-dock-20261026/`, with a verified durable copy outside build.
No reboot is armed.

### Shared application/window layer — bounded non-audio hardware acceptance

Release `a90629c8a2060e66`, source `0858e8e6` (PR181 plus PR185), returned in
`session-00065ec1cc8ad4c2-00`. All four matching logs are verified, the exit
snapshot is present and collector attention is empty. The retained exit records
show five Text models and one Files model admitted/retired, both file pools
reaped, and the desktop lifetime drained to zero members (peak eleven).
Battery retirement reports zero heap, errors, denied accesses and rejected
samples. The session reached about five hours uptime; this does not establish a
five-hour active workload or endurance result.

Two saved documents and one screenshot were recovered. The screenshot shows
both documents reopened alongside Files and the persistent guide. All 1,121
prior non-metadata files retain their hashes; only `.srvros/meta` changed.
Seven files were added: the documents, screenshot and four session logs.
Read-only fsck, allocation and headroom pass: 1,129 files / 2,864 clusters,
zero leaks, bitmap mismatches or errors. Ubuntu default, consumed one-shot,
installed payload/cache hashes and independent recovery remain verified.

The tester confirms completing every checklist section except sound, with all
completed behavior working properly. This accepts the reported Text and Files
window/dialog/command behavior, Terminal controls and cancellation for this
bounded release. Ordinary reopen is tester-reported; only one new session is
present in the collector, so a second logged lifetime is not separately proved.
No PCM stream is recorded (`handle=0`, submitted/position zero); native playback
and app/save overlap remain deferred while another device tests audio. That
device cannot accept Air7,2 sound. New feedback requests offscreen window movement
and clearer dock running/restore affordances; neither is in this payload.
Current installed remains `a90629c8a2060e66`, recovery `9aa8dbf6fbda8252`;
accepted prior payload `986d6c68fa19ba18` remains backed up. Matching evidence and
new files are under `build/app-window-native-20261026/`, with durable copies
outside build. No next boot is armed.

For the next work block, the device branch consumes actual main `33edbafc`
(PR184–192 pipe-admission and retained wait/scheduler diagnostics, plus private
child-image ownership). Those later changes were not in this native payload.
PR182's filesystem audit and PR19's Wi-Fi remain separate active scopes.

### Shared application/window layer — implementation and local consumer validation

Rebased the existing device branch onto actual main `9f18d7d`, consuming PR175,
PR178, PR179 and PR180. Kept main's stronger futex-before-frame-free fixture and
its Linux header fixes; no duplicate poll include or competing fixture remains.
PR181 merged this bounded D3e application-layer block as `f8b593fc`. PR182 owns exFAT import
validation and PR19 owns Wi-Fi; neither is included or overwritten here.

Normal Text and Files share stable allocated window/models, owned-dialog routing,
sequential cancellable Quit and explicit accepted-work retirement. Files New
window creates siblings in one process with independent navigation/history and
one lazy two-channel file-worker pool. Rename/duplicate/delete use owned dialogs
with embedded refusal fallback. Terminal uses shared dispatch, PTY read/write
readiness and command guards while preserving raw shell Ctrl-C/D/L and its
existing separate session processes. Pure widgets remain allocation/transport
free. No protocol, resource-budget, kernel-driver or firmware behavior is changed.

Forty-model host and ASan/UBSan ownership tests pass, including refusal, explicit
work drain, failed-unmap retry and fatal-transport retention. Actual consumer VMs
pass twenty same-process Files windows, independent history, owned rename/cancel/
collision, accepted 8 MiB copy retained during close, concurrent Text saving,
exact file bytes, bounded workers and process-baseline restoration on standard
exFAT metadata. The general Files mutation test passes pointer Delete confirmation.
Terminal's real-shell test passes clipboard, output/backspace, interruption,
independent sessions, close cancellation and cleanup. Lifecycle pointer/shutdown/
crash/refusal tests pass exact saves and offline integrity; emergency-exit orphans
must return to the exact baseline within init's bounded reaper interval. The
load gate passes forty verified 64 KiB writes/readbacks and all forty-eight key
acknowledgements with two reaped workers. Thirty-two Text documents/thirty-three
exact files retire cleanly. Four mixed app cycles overlap a complete twenty-second
PCM stream with exact samples and retained editable Text; this is VM evidence.
Initial fixture failures and their corrections are retained locally.

Final `make test-core all acpi-battery-kernel acpi-battery-vm-kernel check-docs`
passes, with fresh normal/native/VM kernels and initramfs, ASan/UBSan ownership
and clean whitespace. Exact-stage rehearsal precedes the next attended native
trial. The initial exact-stage attempt preserved an interleaved Text-startup
UART marker: the window admitted/published, but the collector missed the split
message. The shared progress matcher accepts only complete compositor traces
between exact app bytes; saved-file/record proofs remain separate. Its captured
replay and malformed/partial/offset controls pass. The Help guide also clarifies
opening a fresh live Terminal after Ctrl-D before playback. Its complete untimed
Help checklist includes Files siblings,
Terminal controls/close, Text decisions, Preview cancellation, two-minute playback
with app/save work, and ordinary exit/reopen. Installed accepted current remains
`986d6c68fa19ba18`, independent recovery `9aa8dbf6fbda8252`; the last return is
collected and no next boot is armed. New source does not inherit that acceptance.
Air6,1's separate continuous-I/O Files-close failure remains unresolved by this
workload. Local evidence: `build/app-window-layer-20261026/`; next native run
record: `build/app-window-native-20261026/`.

### Shared application runtime — concurrent-app hardware trial accepted

PR172 is merged as `aa93109`, consuming PR174's process-admission baseline.
Release `986d6c68fa19ba18` returned in matching session
`session-00065eb407f09862-00`. The user reports applications working properly
with several open concurrently. Logs confirm a successful Text save, Preview
loading an existing 1440×900 screenshot, and normal app/worker retirement. The
session has all four logs, a normal desktop exit and no collector attention.
The desktop lifetime drains to zero members (peak seven); battery service/runtime
retirement has zero heap, errors, denied accesses and rejected samples.

All 1,116 prior non-metadata files retain their hashes; only `.srvros/meta`
changed. One new document and four session logs were added; no new screenshots
were saved. Offline fsck, allocation and headroom checks pass: 1,122 files,
2,817 allocated clusters, zero leaks, bitmap mismatches or errors. The exact-stage
VM rehearsal separately completes all two-minute playback bytes with zero
underruns, application/save overlap, ordinary reopen and clean retirement.

This closes the bounded shared-runtime implementation and concurrent-app
regression milestone. It does not claim physical latency/energy measurements,
native playback overlap (no PCM stream ran here), or separately confirmed
menu/button/shortcut parity, maximum-image cancellation, sequential multi-document
Quit decisions or repeated desktop exit/reopen. Those paths retain host/VM
evidence and remain focused physical follow-ups when relevant.

Installed accepted current is `986d6c68fa19ba18`; independent recovery
`9aa8dbf6fbda8252` and prior accepted `1d1d5edce03523db` are retained. Ubuntu is
default, the one-shot was consumed and no next reboot is armed. The run record
and matching collection live in `build/app-runtime-native-20261025/`, with
verified durable evidence and an acceptance-scoped payload copy outside build.
PR19 retains Wi-Fi scope; sibling models do not inherit this acceptance. This
closeout changes documentation only; no next shared implementation is claimed.

### Shared application runtime — implementation and local validation checkpoint

Base main `eda8bfb`; PR170's app-timing diagnostics are already consumed.
Air7,2 owns the next bounded desktop-runtime block: shared bounded connection
dispatch, deadline/timer and worker-readiness wait preparation, plus reusable
standard-action enablement/shortcut/menu plumbing. Migrate normal Text and
Preview first, retaining their document/image models, accepted-job lifetime,
modal cancellation and sequential Quit behavior. Keep pure controls in the
widget library and orchestration in `userspace/desktop_runtime/`; no kernel,
protocol, driver, process-budget or firmware changes are planned.

Touched consumers are `userspace/desktools/text_runtime.c`, its standard action
plumbing and `userspace/deskview/deskview.c`, with shared headers/library, focused
host/consumer VM tests and current guides. PR19 retains Wi-Fi scope; Air6,1 owns
the separate heavy-I/O latency investigation. Preserve its timing receipts and
consume any new dependency explicitly. Validate dispatch fairness, timers,
worker completion/cancellation, close/quit/save retention and the existing actual
Text/Preview/pointer/shutdown/failure gates before preparing a hardware retest.
The existing timed GUI/file-worker load diagnostic also consumes the runtime,
including its registered phase timer and merged readiness; its workload and
acceptance predicates remain unchanged.
Installed accepted current remains `1d1d5edce03523db`, recovery
`9aa8dbf6fbda8252`; the prior return is collected, and no new boot is armed.

Text, Preview and the GUI/file-worker load diagnostic now use the optional
allocation-free shared runtime. It bounds dispatch and due-timer callbacks,
merges existing pipe readiness with deadlines and a service heartbeat, and keeps
accepted-job/document teardown with its application owner. Standard command
snapshots guard Text/Preview menus, toolbar buttons and application shortcuts.
Widgets remain pure; no protocol, driver, resource-budget or native model gate
is changed by this block.

Local consumer validation through clean `ce413bf` passes actual pointer actions,
unsaved desktop shutdown, crash/refused-dialog recovery, maximum-image cancel and
format failures, 32 same-process documents with 33 exact files and complete
retirement, the separate sixteen-document quota/refusal run with 17 exact files,
and four app cycles during a captured 20-second stream with zero sample error.
The growth fixture now uses actual monotonic document identities, separates owned
pickers from document counts, waits for picker first-frame/retirement receipts,
and distinguishes allocated registry capacity from live windows. Earlier failed
fixture assumptions remain in ignored evidence; deadlines and byte/resource
checks are retained.

Main `a679115` (PR174 process entry/reap admission) is consumed in clean
`1854676`. The combined tree passes full core/desktop host suites, matched
normal/native/VM builds, actual 16-idle/32-write-key and forty 64 KiB write/readback
wait-load checks, and unsaved desktop shutdown with exact files and process
baseline recovery. Runtime ASan/UBSan and current documentation links pass.
PR172 publishes this shared block; Actions remain disabled. Local evidence is
`build/app-runtime-20261025/`; the new attended run record is
`build/app-runtime-native-20261025/`. Native runtime/input/audio acceptance is
pending an exact-stage rehearsal and the in-guest persistent checklist. The prior
accepted current/recovery above remain installed; no new reboot is armed at this
source checkpoint. PR19 retains Wi-Fi, and Air6,1 owns its physical process-
admission acceptance; consuming its shared fix does not claim that result here.

### Desktop modal and unsaved-exit retest accepted

Release `1d1d5edce03523db`, clean source `9bca084` (PR169 integration
`f658f6b`), returned in session `session-00065eb2f55d330d-00`. The user reports
that this round worked well, resolving the previously reported modal button and
unsaved Return to console failures for this payload. The matching session has
all four logs, a normal desktop exit and no collector attention. App lifetime
drains to zero members; battery/runtime retirement has zero heap, errors, denied
accesses and rejected samples. Both saved test documents are retained. All 1,110
prior non-metadata files keep their hashes; only `.srvros/meta` changed. Offline
fsck/headroom/allocation passes: 1,117 files, 2,811 clusters, no leaks, bitmap
mismatches or errors.

Native file playback was exercised with zero recorded underruns and its owner
released. Its final PCM state is canceled, so do not claim full two-minute native
completion from this run; complete 120-second playback/save overlap remains the
exact-stage VM result. The human report is qualitative, not an input-latency or
audio-endurance bound. No new screenshots were saved in this round.

PR165's modal capture correction and PR169's cooperative shutdown/workload guide
are merged. This bounded milestone is closed. Evidence and hash inventories are
in `build/unsaved-exit-native-20261025/` with durable verified copies outside
build. Installed current is the tested `1d1d5edce03523db`; independent recovery
`9aa8dbf6fbda8252` is retained. Ubuntu remains default, the one-shot was consumed,
and no new reboot is armed. Main `acc50f9` adds PR170's separate timing diagnostics;
that newer source does not inherit this image's physical acceptance. PR19 retains
Wi-Fi scope. Start any next feature from a freshly inspected baseline.

### Unsaved desktop exit and sustained playback — implementation and trial preparation

Base main `b537c2a` includes PR167's completed-I/O handoff. Air7,2 now owns a
bounded follow-up in the desktop shutdown/modal admission path, its host/VM
lifecycle tests, a longer bundled playback fixture and the attended Help guide.
No audio driver, PCM engine, firmware gate or process-limit edits are planned;
PR19 retains Pro Wi-Fi scope. Publish this follow-up as a separate PR.

Release `336d81743ca03a33`, session `session-00065eb1e8675a07-00`, returned.
The user reports modal controls generally working, but Return to console with an
unsaved file repeatedly opens/closes its modal. The saved session has no exit
snapshot, so retirement is incomplete, not passed. It records two short Settings
tones and no PCM file-playback owner: sustained listening/I/O overlap is not
accepted. All 1,106 prior non-metadata files retain their hashes; only
`.srvros/meta` changed. Three logs and one saved test document were added.
Local evidence: `build/modal-pointer-native-20261025/physical-after/` and its
return proof. Offline fsck/headroom/allocation pass: 1,111 files, 2,804 clusters,
zero leaks/mismatches/errors.
Installed current remains `336d81743ca03a33`, independent recovery
`9aa8dbf6fbda8252`; Ubuntu default and consumed one-shot are verified. No new
reboot is armed. Collect/diagnose, reproduce shutdown with pending edits, preserve
Save/Discard/Cancel decisions during exit, then validate locally before staging.

The first new host regression fails on forced modal closure before the fix and
passes afterward. Full core/desktop and matched builds pass. Clean `e4ebd0c`
passes actual Text/Preview shutdown checks: stable prompts, pointer Cancel,
canceled Save picker, exact saved edits during exit, ordinary reopen, existing
prompt Discard, clean filesystem allocation and restored process baseline. The
first VM attempt missed its final click because the fixture retained the prior
session's pointer origin; reset that origin on desktop reopen and retain the
failed attempt separately. The asset in the built image is verified as 120
seconds/23,040,000 PCM bytes and twenty unchanged channel-check cycles. The final
exact-stage rehearsal will play it fully while saving work; native listening and
shutdown acceptance remain pending. The first long-sample stage `9fb28b83ea370cb3`
passes its complete playback/save/exit/reopen rehearsal, but final provenance
review found its prebuilt native kernel had not been relinked after PR167. Keep
that as preliminary UI/audio evidence only; it was not installed. Rebuild matched
normal/native/VM kernels, then stage and repeat the exact-image gate before
hardware installation. Main `46f6212` adds only Air6,1 result documentation; PR170
owns separate app-timing instrumentation and PR19 remains Wi-Fi.

Final matched stage `1d1d5edce03523db` from clean `9bca084` rebuilds the native
and VM kernels explicitly. Its exact GRUB → Limine → native rehearsal passes
all 120 seconds/23,040,000 PCM bytes with zero underruns, Calculator open/close and
exact USB document-save overlap, normal exit/reopen, provider retirement and clean
allocation. Q35 native battery refusal remains expected. The full guest guide
puts these two retests first and stays open without a deadline. This is VM and
host evidence; physical listening and unsaved desktop shutdown remain pending.
PR169 publishes the correction; local `build/unsaved-exit-native-20261025/run.json`
owns installed/armed/return state. Preserve current/recovery and durable evidence
before any attended reboot; collect the matching new session on return.

### Modal pointer correction validated; physical retest pending

Release `b6f4c7525115168a` returned with session
`session-00065eb18ba89a8d-00`. Modal windows display, but the user reports
Save/Discard/Cancel highlighting without acting: physical pointer acceptance
failed. Runtime retirement itself is clean (collector attention empty, battery
heap/errors/denied/rejected zero). All 1,100 prior non-metadata files retain their
hashes; only `.srvros/meta` changed. Four logs and two test documents were added.
Offline fsck/headroom/allocation passes: 1,107 files, 2,799 clusters, no leaks or
bitmap mismatches. Unreported lifecycle/audio checks remain unaccepted.

Repeated modal configuration canceled the UI capture between press and release.
Shared dialogs now invalidate capture only on effective frame/button geometry or
enabled-state changes; pickers skip identical-bounds configuration, retaining
loading/listing and resize cancellation. No driver, protocol, worker, process
limit or firmware change. Before-fix dialog and picker host regressions fail;
fixed desktop/widget suites and matched builds pass. Actual Text/Preview QMP
pointer validation on clean `f8e5fe2` passes seven held-click actions, exact saved
`Click draft remains` bytes plus normal lifecycle files, clean allocation and
process baseline recovery. The normal lifecycle sequence and forced-dialog
refusal fixture also pass. A test-only first attempt stopped when the software
cursor obscured a button border during visual discovery; its raw evidence is
retained and the locator now moves the pointer away before scanning.

Scope and code are in PR165. Main `cd5ec0a` (PR164) is now integrated:
Air6,1 startup/loop instrumentation remains diagnostic, not a resolution of its
Files-under-I/O issue. PR19 owns Pro Wi-Fi. The initial clean `eb66c74` exact v75
stage `d203e4b1c640bde8` passes GRUB → staged Limine, the automatically opened
Help guide, saved work, playback, exit/reopen and clean retirement/unmount; native
battery correctly refuses Q35. Both guide screenshots were visually checked.
The combined desktop/build and real-click fixture pass on clean `60dbb2d`. Its
final v75 stage `336d81743ca03a33` repeats the exact boot/save/playback/exit/reopen
rehearsal successfully with clean allocation and retirement; the focused guest
instructions remain visible. This is the staged candidate for the attended
pointer retest, not new physical acceptance. Evidence is in `build/modal-pointer-20261025/` and the
collected `build/lifecycle-native-20261025/` return. The next v75 attended guide
opens with a focused physical button/picker checklist and has no timed exit.
Validate its exact stage before installation; preserve independent recovery
`9aa8dbf6fbda8252` and record install/one-shot/return state locally. No new physical
acceptance is claimed until that retest. No reboot is armed at this checkpoint.

### Combined desktop lifecycle and playback trial prepared

Integrated main `b35b889` (PR160) and independently validated clean `7ddf215`:
core/matched builds and normal/crash/refused-dialog VMs pass. The 60-second
PCM/unpaced verified USB fixture passes 21 real app cycles, maximum loop gap
299,756 us, zero app errors/underruns, 227 audible write-overlap windows and clean
retirement/return. A separate four-app playback fixture has zero sample error,
exact saved document bytes, clean allocation and restored process baseline.
These are VM results, not native responsiveness or listening acceptance.

The two-round 24-app diagnostic exposed an asynchronous launch assumption: it
read the final Text PID immediately after queueing exec, recording zero. All
24 windows rendered but no edit acknowledgement followed; round one reached
its unchanged 180-second deadline and failed. Round two was interrupted after
the same symptom and remains incomplete. Clean `ff1c1ad` retains the reserved
child slot, obtains its PID after all frames are ready and rejects empty PID-zero
targets. Both fixed rounds pass: 24 normal child reaps, zero errors, 60 editing
acknowledgements, five exact `AA` files and chooser drain checks per round, saved
screenshots/logs, clean filesystem allocation and automatic return. The process
limit, worker implementation, audio and device gates are unchanged.

Exact v75 stage `b6f4c7525115168a` from clean `ff1c1ad` passes GRUB → staged
Limine, automatic persistent Help, saved Text/Terminal work, playback, ordinary
exit/reopen and clean retirement/unmount. Native battery correctly refuses Q35;
it is not physical battery acceptance. Both guide screenshots were visually
checked. The attended checklist now includes app open/close, resizing, typing and
pointer movement during the bundled stereo sample. No timed exit or automatic
reboot is imposed on this human check. Save `lifecycle-notes.txt` and screenshots,
reopen the ordinary desktop, then use monitor `reboot` to return to Ubuntu.

The original modal failure at `29c75d8` and failing host regression are retained;
PR160's `433480a` is the single first-canvas readiness implementation. The Air7,2
duplicate was reverted before consuming it. Full desktop/build and 569 local
documentation links pass. Evidence: `build/pr160-review-20261025/` and
`build/lifecycle-native-20261025/`, with required payload/evidence backups outside
build. USB preflight passes fsck plus headroom/allocation: 1,101 files, 2,792
clusters, zero leaks or mismatches; existing file hashes are recorded. Independent
recovery `9aa8dbf6fbda8252` and prior current `f01d4049daae0b4a` are backed up.
The local run record owns the subsequent install/one-shot/return state. Physical
acceptance of this new payload remains pending the announced attended trial.

### Document-routing failure paths covered; compositor integration still pending

Base main `c4bee5c`; draft PR160 `29c75d8` already consumes this matched v15
baseline and owns asynchronous compositor launch/cache implementation. Focused
host tests now cover a copied 160-byte chunked path across a wrapped queue,
PID/window/request authentication, stale and duplicate acknowledgements, explicit
refusal fallback, queue headroom, request-ID exhaustion, malformed sender
fragments, timeout uncertainty, owner retirement and refusal while closing or
exiting. No uncertain request is replayed; no late refusal resurrects an app
behind shutdown. The full local desktop suite passes on this branch. The same
new fixture also passes against PR160's exact `29c75d8` compositor/registry/worker
source extracted into ignored review storage, including its existing worker
checks. That host replay is not a combined VM or native latency result.

This scope changes tests and current lifecycle documentation only. No competing
worker, sound, process-capacity or device-profile changes. Evidence is in
`build/lifecycle-route-review-20261025/`; installed/recovery state is unchanged
and no reboot is armed. PR160 remains draft while its owner completes integrated
VM/exact-stage validation. Consume its reviewed result before a combined trial;
the existing v75 attended guide remains available for physical acceptance.

### Kernel lifecycle service windows validated; physical acceptance pending

PR157 now includes bounded non-audio kernel work from clean `2eb23c1`: process
record/image/mapping/refresh/retirement service windows and graphics private-copy
and surface-fill windows. The original 57 ms IRQ-masked retirement walk is split
at completed page batches while retaining the existing serialization owner,
unsafe-entry/sibling exclusion, cancellation lifetime, frame poisoning, surface
quota/epoch checks and failure semantics. Process limits and native gates remain
unchanged. No HDA, codec, PCM or player implementation was edited.

An audio-free Q35 fixture cycles 16 real Calculator/Files/Text/Preview apps,
resizes/closes them, preserves an editable document and checks its exact saved
bytes plus the process baseline. Baseline maximum timer/USB-poll gaps were
61,684/79,743 us; the combined candidate measured 46,319/57,520 us. These are
whole-fixture VM observations including desktop entry/exit and host scheduling,
not physical guarantees. Whole-operation spans include scheduling and QMP wall
timings include automated typing/shortcut delays. Remaining stalls are not hidden.

Full core/desktop, ownership/failure/CR3/retirement fixtures and matched normal,
native battery, Air7,2 VM battery and Air6,1 VM battery builds pass. Normal,
Preview-crash and forced-dialog-refusal lifecycle VMs pass. Two unchanged
60-second PCM/USB/battery stress VMs pass with clean retirement/automatic return;
the earlier EC timeout has not recurred in those two runs, which does not prove
its root cause. Exact v75 stage `2f871ca3f7401849` passes GRUB → staged Limine,
automatically opened Help, save/exit/reopen, playback and clean unmount. It remains
staged, not installed. The guide was visually checked.

A separate clean `9746b0a` regression opens, resizes and closes four apps during a
20-second stream using the unchanged player/HDA. Captured PCM has zero sample
error and the player reports zero underruns. Saved document/process baseline and
filesystem checks pass. The attempted 80-second fixture was terminated (exit 143)
without a completion report; preserve it as incomplete, not a pass. Test fixture
size/setup failures are retained separately. This is VM audio evidence only.

Main `3ab4a38` (Air6,1 result documentation) is integrated at `038633d`; production
sources match the tested `2eb23c1` tree. PR160 owns asynchronous compositor
lifecycle/cache work; preserve its worker design and integrate it against this
matched v15 baseline explicitly. Same-PID siblings remain excluded while their
worker owns mutable kernel memory operations; service windows do not make app
startup asynchronous. Larger UI/gate-wait stalls remain PR160's scope.

Evidence is in `build/app-latency-20261025/`, retained with hash verification under
this unit's `reviewed-evidence/app-latency-20261025` outside build. Installed
current `f01d4049daae0b4a`, independent recovery `9aa8dbf6fbda8252`, Ubuntu default
and no armed one-shot are verified. Neither new UI nor kernel changes have new
Air7,2 physical acceptance. No physical reboot was requested.

### Application lifecycle checkpoint before kernel service-window work

Base main `e253a60`; D3 follow-up is implemented on this device branch/PR157.
Matched protocol v15 adds one-level same-process document-modal windows, parent
focus/input redirection and child-before-parent retirement. The shared presenter
hosts Text's unsaved/replacement prompts and file pickers, and Preview's picker;
admission refusal retains an embedded fallback. Text now distinguishes Ctrl-W
from Ctrl-Q, resolving Quit one document at a time and stopping on cancel or
failed save. Existing capable Text/Preview processes accept copied file-open
requests with bounded chunks/cookies, acknowledgement retries and explicit
refusal fallback. Unconfirmed requests are not blindly replayed. Process limits
and device firmware gates are unchanged.

Initial Text and Preview consumer VMs pass. The lifecycle VM passes independent
sibling input during a modal prompt, Quit cancellation, failed-save retention,
existing-process Text/Preview routing, exact saved contents and process baseline.
Crash (Preview killed with a picker live) and forced dialog-refusal cases pass.
The baseline check exposed uncollected exited workers; explicit idle-file-worker
retirement now retains/reaps PIDs, and Preview drains image-worker retirement.
Compositor preferences shutdown also collects its worker. Host tests cover the
new ownership, quit, routing/chunk and worker-retirement contracts. Full core,
desktop and matched normal/native/VM battery builds pass. Clean source `6eb9495`
passes the normal, crash and forced-refusal lifecycle fixtures with exact saved
bytes, clean formatted exFAT allocation and process-baseline recovery. The
combined-service VM passes two rounds of 24 apps and exact document persistence.

Main `afb2c3b` (Air6,1 audio/USB work from PR156/158) is integrated at `c0ce49b`;
the guide uses distinct profile v75 and session selector 5, preserving v74 and
selectors 3/4 for audio stress. Integrated core/desktop builds, 81 staging tests and all three lifecycle cases
pass on clean source `3699b58`. Exact v75 release `1564f1d0adb0916b` passes
GRUB → staged Limine guide startup, saved Text/Terminal work, audio playback,
normal exit/reopen and clean unmount; its native battery profile refuses Q35
as expected. Both guide pages were visually checked.
The explicit lifecycle profile opens the full attended checklist in Help with
no timed exit; ordinary reopen remains ordinary. No hardware reboot is armed.

One extra integrated audio/USB stress run completed its full minute (508 bursts,
133,169,152 bytes, zero underruns), then halted on a synthetic EC sample timeout
at shutdown. The unchanged candidate retry passed with clean retirement and
return. Three diagnostic main-compositor-only runs also passed; they do not
prove a cause or constitute full-main/native comparisons. At that checkpoint
PR157 was held for a bounded teardown/latency reproduction. No timeout or model
gates were widened. The candidate compositor source and build were restored.

Evidence: `build/desktop-lifecycle-20261025/`, with 280 files hash-verified under
this unit's `reviewed-evidence/desktop-lifecycle-20261025` outside build, including
the exact stage, original failure, isolation provenance and raw logs.
The installed/recovery payload remains the prior physically accepted v14 image;
this v15 change has no new physical acceptance yet. Future attended testing must
open its full checklist in the guest before relying on input. Pro PR19 and Air6,1
PR156 retain separate Wi-Fi/endurance scope. No reboot is armed by this work.

### Automated many-app documents accepted

Release `f01d4049daae0b4a`, clean source `044d0b4`, passed both automatic native
rounds on October 25 and returned to Ubuntu. Sessions
`session-00065eaef7987f3c-00` and `session-00065eaefa2f6651-00` each rendered 24
independent app processes, acknowledged 60 editing/clipboard inputs in the
ordinary pooled Text runtime, saved/stat'ed/renamed/cleared/reread five files,
and passed legacy/pooled chooser submitted-cancel/drain/reopen/stale checks.
All ten files contain exact `AA` bytes in distinct fresh Home directories.
Both 1440×900 screenshots show the reopened fifth document and readable progress
banner. Each round reaped all 24 children normally, drained its lifetime (peak
32 descendants), and retired battery service with zero heap/errors/denials and
zero rejected samples. Collector attention is empty for both sessions.

All 1,080 prior non-metadata files retained their hashes; only `.srvros/meta`
changed. Ten document files, eight logs and two screenshots were added. Offline
read-only fsck/allocation/headroom passes: 1,101 files, 2,792 clusters, zero leaks,
bitmap mismatches or errors. Current payload, independent recovery
`9aa8dbf6fbda8252`, Ubuntu default and consumed one-shot were verified after return.
No reboot is armed. Installed current remains the explicit automatic diagnostic.
Evidence/run records are in `build/documents-native-20261025/`, with hash-verified
backup under the unit's `reviewed-evidence` directory outside build.

PR155 publishes the diagnostic, visible progress and instruction-delivery rule.
Matched builds/desktop suite, 30 collector tests, 565 documentation links,
two-round positive VM and 384 MiB admission-failure/clean-return fixture passed.
The final clean native stage also passed GRUB → staged Limine Q35 rehearsal and
its banner was visually inspected. No Actions used. Main base `6e06bfb`; Pro
PR19 `2c40c36` and Air6,1 PR156 `ee96e1d` retain separate Wi-Fi/endurance scope.
This accepts automated capacity, editing/clipboard, file persistence, shared
chooser transport and reuse under the tested load. Synthetic inputs do not
measure physical input latency or accept manual Preview-picker visuals. The
prior attended mid-20s-window/sensor observation remains recorded below.

### Attended high-count return collected

Release `80904d5e5fd6f6ac` / source `7c2880a` returned on October 25. The user
opened a mid-20s count of windows and checked sensors successfully. Chat
instructions were not visible before reboot, so the detailed document checklist
is not accepted from this observation. Sessions `session-00065eae867725d0-00`
and reopened `session-00065eaeb5319670-00` have clean exit snapshots, lifetime
drains (peaks 33 and 22), and battery retirement with zero heap/errors/denials.
The second monitor-started session has no boot release token; it is associated
with the same boot by its inherited prior tail and lifetime epoch progression.
All prior files are unchanged except `.srvros/meta`; two user-created artifacts
and eight logs were saved. Offline fsck/allocation/headroom passes: 1,081 files,
2,690 clusters, zero leaks/mismatches/errors. Evidence and artifacts are backed
up outside build under `reviewed-evidence/runtime-interactive-20261025`.

### Automated 24-app native capacity accepted

Release `c9e59c1b11d5c239`, clean source `7fc5077`, completed two automatic
combined-services rounds on October 25 and returned to Ubuntu. Matching sessions
`session-00065eae2764edfa-00` and `session-00065eae28c055f4-00` each record 24
independent mixed app processes rendered, a saved 1440×900 screenshot, 24 normal
successful child reaps and zero child errors. Both inherited lifetimes drained
all members (peak 31 descendants) and retired the battery runtime with heap zero,
zero rejected samples/errors/denials. This accepts the tested capacity and reuse
past the old fifteen-app scheduler failure; it does not claim unlimited admission,
physical typing/pointer quality, or manual document editing under this load.

All 1,062 prior non-metadata files retained their hashes; only `.srvros/meta`
changed, with six new session logs and two screenshots. Offline read-only fsck and
allocation/headroom checks pass: 1,071 files, 2,671 clusters, zero leaks,
bitmap mismatches or errors. Ubuntu default, consumed one-shot, current payload
and independent recovery `9aa8dbf6fbda8252` were verified after return. Previous
installed `2750e5efa04573ba` retains its independent backup. No reboot is armed.
Local proofs are in `build/capacity-native-20261025/` and the unit's
`reviewed-evidence/capacity-native-20261025` backup outside build.

PR153 adds the explicit v73 capture, bounded compositor/owner deadlines,
normal retirement, screenshot/result persistence and automatic return on handled
success or failure. Local core/desktop suites, builds, 79 staging tests, successful
2 GiB Q35 and handled admission-failure 384 MiB Q35 fixtures pass; the exact native
GRUB → staged Limine payload rehearsal also passes. The initial 256 MiB Limine
memory failure is preserved separately. Thirty collector tests pass, including
strict exclusion of routine opened/retired resource traces while retaining
capacity failure detection. No Actions used. Base main `3647dc7`; Pro PR19 at
`db95022` remains independent RF/Wi-Fi work, and Air6,1 PR154 at `44b5706` adds
a separate paced endurance harness without overlapping runtime changes. Firmware gates and ordinary startup
are unchanged. Next acceptance work is interactive document/chooser behavior
with many apps, not another automatic capacity reboot.

### Scheduler capacity fix: host/VM validated

Base main `16bbe49` plus this unit's collected-run notes; Pro PR19 inspected at
`2f9032d` remains independent Wi-Fi work. Scope: scheduler storage grows in stable chunks
under a kernel-stack/metadata byte budget; wait queues must support slots above
63 without bit shifts or aliasing. Preserve identities, I/O ownership, quarantined
stacks and existing wait semantics. Add host rollback/reuse/high-slot coverage,
combined-services mixed-process VM coverage, and desktop launch-failure alerts.
Touch scheduler/header, process launch diagnostics, desktop session policy and
focused tests/docs. No new firmware gates or physical reboot in this work block.

Host/core/desktop suites and matched kernel/userspace builds pass. The new
mixed-app QEMU fixture reproduces the native symptom on staged old release
`2750e5efa04573ba`: fifteen windows followed by scheduler-table exhaustion.
Runtime `8d1a05a` passes the fixed combined-service VM with 2 GiB guest RAM:
24 independent app windows, exact saved text after growth, PCM playback, all
35 tracked descendants drained, battery/logger retirement, desktop reopen and
offline filesystem/allocation checks. A prior 512 MiB run reached 23 windows
then exhausted physical memory; its visible resource warning and failure
artifacts are retained separately. The final harness change only matches the
2 GiB exact-stage allocation and fails promptly on logged launch refusal.
Actual `threadstress` (all cases) and the explicit desktop quota-refusal/reuse,
save/capture and complete-cleanup VM tests pass. Native 24-app capacity/reuse is now accepted by the trial above.
The throttled-I/O desktop regression also passes: all 48 input events acknowledged
(32 while writes were active), forty exact 64 KiB writes/readbacks, both workers
reaped, process baseline restored and no leaked filesystem allocation.
Thread storage is charged to a 32 MiB stack/dynamic-record budget; 32-slot stable
chunks and non-bitmap waits preserve addresses and identities above slot 63.
Host fixtures cover growth beyond 95 live slots, PMM failure rollback, quota
refusal/reuse, high-slot waits/timeouts/cancellation and quarantine retention.
Launch failures now produce visible rate-limited app alerts and unconditional
error records. Worker attribution version 2 is bounded to 256 directory slots;
version-1 evidence remains readable. PR152 publishes the change; local validation
uses no GitHub Actions. Evidence is in scheduler-capacity-20261025 and copied to
the unit's reviewed-evidence directory outside build. The automated native 24-app/reopen trial above completes capacity acceptance;
interactive documents remain a separate check.


### Runtime native return: scheduler capacity blocker

Release `2750e5efa04573ba` / source `16bbe49`, session
`session-00065ead489f2265-00`, returned and was collected on October 25. The user
reports sensors/controls working and about fifteen application windows, after
which no additional app would open. Matching logs contain three explicit
`exec: scheduler thread table full` errors. That release's scheduler has
32 global thread slots shared by app main threads, workers and boot/device
services. The launcher is an overlay, not another admitted client window.
This is a scheduler admission failure, not evidence of a restored compositor
sixteen-window ceiling. Large single-process window fixtures and ordinary
mixed-client tests did not cover this services-heavy native workload.

The app lifetime drained all members (peak 21 descendants); battery retirement
reports zero errors/denials and heap=0. Collection has an exit snapshot and no
attention flags. All 1059 prior files are unchanged except expected `.srvros/meta`
bookkeeping; four new session logs were saved, with no new documents/screenshots.
Offline checks pass: 1063 files, 2583 allocated clusters, zero leaks, bitmap
mismatches or errors. Ubuntu is default, the one-shot is consumed, and recovery
`9aa8dbf6fbda8252` remains verified. Do not promote this run to full capacity,
chooser/document persistence or reopen acceptance.

Next bounded work: reproduce mixed-app admission with combined device services
in QEMU; replace the small scheduler slot ceiling with explicitly budgeted,
stable thread storage while preserving stack/FPU/context and I/O-owner lifetime;
retain refusal behavior under real memory pressure. Make application-launch
resource failures visible and logged without requiring trace mode. Cover slot
reuse, failed admission rollback, waits/cancellation, retirement and service
coexistence, then repeat the native 17+ mixed-window check. Independent process,
per-process thread, stack and surface budgets still apply. No new candidate is
installed or reboot armed by this collection; Pro PR19 at `5341990` remains
independent Wi-Fi work. Native evidence and file hashes are backed up outside
build under the runtime-native-20261025 reviewed-evidence directory.

### Runtime native trial prepared (2026-10-25)

Release `2750e5efa04573ba`, clean source/main `16bbe49`, is installed for the
attended D0–D3c/D4a acceptance round. Combined Home/battery v55 retains the
existing device gates and recovery `9aa8dbf6fbda8252`; Ubuntu remains default.
Matched kernel/userspace rebuild, ordinary combined-service QEMU lifecycle,
audio/file persistence/reopen and exact staged GRUB → Limine → native-kernel
rehearsal pass. Q35's native-profile refusal is expected and not native battery
acceptance. The USB's 1059 files have pre-run hashes; offline filesystem and
allocation/headroom checks pass with zero leaks/mismatches/errors. Current and
recovery payloads are independently backed up. One unused EFI payload was
archived and verified before its EFI copy was removed for installation space.

Interactive instructions live in Help → Topics → Runtime trial. Exercise 17+
windows, independent documents, chooser cancellation, redraw and save/input
coexistence; save notes/screenshots, exit normally, reopen with `desktop battery`
and use monitor `reboot` for the final flushed/unmounted return. Human input was
announced before boot. Physical acceptance remains pending. On return collect
matching sessions before another installation; local run state/evidence is in
`build/runtime-native-20261025/`, backed up outside build. Pro PR19 inspected at
`73e35e9` remains independent; this preparation changes no shared runtime code.

### Runtime hardware-round preparation

Bounded scope on main `a6d4361`: prepare a dedicated on-screen Runtime trial
Help topic, remove stale capacity/tap wording from the general trial entry,
and rehearse topic navigation and readable instructions in QEMU. Touch Help
content, its existing VM harness and current validation docs only; no new
runtime/device features, host installation, physical USB writes or reboot.
PR19 inspected at `b9240eb` remains independent Edimax Wi-Fi work. The next
physical round is interactive; its exact stage, recovery and USB preflight are
still required immediately before installation.

Validation passes: Help host content/history/clipboard/layout suite, full image
build, documentation links, and actual Help QEMU navigation/clipboard/app routing/
retirement. The new topic's beginning, return instructions and compact layout
were visually inspected. Instructions distinguish ordinary Home's retained mount
and `desktop battery` reopen from the basic trial's unmount-on-exit behavior.
Evidence is retained in the October 25 overnight archive and durable reviewed
backup. The installed payload/recovery remain unchanged; physical acceptance is
pending and no reboot is armed.


### Shared chooser controller checkpoint (D3c)

PR150 runtime `2689a8d`, based on merged renderer baseline `a354eed`, shares Text
and Preview's asynchronous file-picker ownership through an optional application
runtime library above the pure widget library. It borrows existing job/widget/
listing storage, correlates completions with nonwrapping controller IDs, retains
canceled submitted reads until drained, and rejects stale/foreign/duplicate
responses. Selection copies a normalized path before hiding. Document/image
operations, overwrite decisions, clipboard, focus and worker retirement remain
caller-owned. Preview now drains its pending listing before exit.

Full desktop host tests/image build and final focused chooser/Text/Preview tests
pass. Actual QEMU submits and cancels LIST with both legacy and pooled workers,
then drains, reopens, rejects an old completion and retires the worker. Normal
Preview PNG/JPEG/BMP, image/error retention, chooser cancellation/clipboard,
maximize/restore/compact/Files launch and exit pass. Normal multi-document Text
and app workflows pass; 32 documents preserve 33 exact files and retire their
shared workers. Throttled-write regression retains all 16 idle/32 active keys,
40 exact 64 KiB files/readbacks, both worker reaps and clean offline integrity.
Screenshots are inspected, source/artifact hashes recorded and evidence backed
up outside `build`. An initial collector used an obsolete status-line spelling;
the corrected collector passes and its failed receipt is retained separately.

See the [chooser contract](../desktop-chooser.md) and [runtime validation
checklist](../desktop-runtime-validation.md). This completes the overnight
capacity/shared-runtime block; parented windows, broader activation/document
services, native-density text and accessibility remain separate milestones.
Physical acceptance of this new runtime is pending. No host reboot, installation,
physical USB mutation or automatic 10AM reboot was performed or armed. Pro PR19
remains independent.

### Renderer overdraw checkpoint (D4a)

PR149 merged `a354eed`; runtime `36f8800`, based on PR148 merge `a3f959c`, reuses one stack ordering
per frame and skips only fully covered clipped opaque frames/shadows. It leaves
window/cache/ACK lifetime, damage policy and framebuffer copying unchanged.
Full desktop host tests and image build pass. Exact reference comparisons cover
160 deterministic randomized scenes with multiple clips, up to 128 records,
shadow-union holes, hidden/minimized windows, stale-size caches, tiny-frame
fallback, overlays and stride guards. The actual 128-window growth and normal
multi-document Text QEMU gates pass, as do mixed 16-client clipboard, normal apps,
compositor lifecycle/relaunch and throttled-write input gates. The last records
16 idle plus 32 during-write keys, all 48 frame acknowledgements, 40 exact 64 KiB
files/readbacks, both workers reaped and clean offline integrity. Growth retires
all 129 window lifetimes with zero cache/registry bytes. Screenshots were inspected;
source hashes and full evidence are retained in the overnight checkpoint and its
durable backup.

Same-scene host measurement at 640×480, 128 stacked 300×285 frames, full repaint:
44,581,463 pixel writes unculled versus 737,871 culled. Fifteen alternating timing
samples gave median 25.59 ms versus 0.382 ms; these are descriptive host painting
measurements, excluding sorting/IPC/blit/scanout, not native latency or energy.
The dispersed 128-window case reduces full-repaint writes from 24,398,798 to
20,967,867; the optimization deliberately does not subtract partial occlusion.

The [runtime validation guide](../desktop-runtime-validation.md) describes the
completed blocks, repeatable local gates and next interactive acceptance round.
This is one rendering optimization, not completion of native-density text,
accessibility or the entire D4 roadmap. Pro PR19 `e409c07` remains independent.
Host/QEMU only: no installation, USB mutation or reboot. Native installed
`284e53065081672f` and recovery `9aa8dbf6fbda8252` remain unchanged; new desktop
runtime acceptance is pending and no automatic 10AM reboot is scheduled.

### Active combined desktop wait work (D3b)

PR147 merged as `0a89d5f` and consumed. Next bounded scope: let a client wait
for its authenticated GUI queue and explicit nonblocking pipe/PTY interests in
one request, with an absolute monotonic deadline and cancellation generation.
Capture immutable interests, reject close/dup2 descriptor reuse with lifetime
generations, and recheck readiness atomically before scheduler registration.
During owned storage I/O preserve GUI responsiveness and defer pipe readiness
without entering descriptor cleanup or spinning. General POSIX poll/VFS/network
admission remains unchanged. Add library wrappers and use them for the shared
Text/Files worker service when windows are quiescent; keep accepted jobs owned
when a wait is canceled. Also audit job snapshot reuse while extending its wait
helpers. Touch GUI/syscalls, process descriptor lifetime, client/runtime helpers
and focused host/actual-ABI/desktop QEMU tests. Native timing remains pending.
Pro PR19 `e409c07` remains independent Wi-Fi diagnostics. Host/QEMU only;
no boot/USB installation or reboot, and no automatic reboot at the deadline.

D3b PR148 final runtime `13d8ac8` completes the local validation checkpoint. Full host/core suites and image gates pass;
actual GUI wait tests use real threads/pipes and cover cancellation, descriptor
replacement, simultaneous readiness and peer exit. The first guest run caught an
uninitialized cancellation epoch in reused queue memory; a poisoned-allocation
host fixture reproduces it, and explicit initialization fixes both. A separate
throttled QEMU trial receives 16 idle and 32 active-write keystrokes during forty
64 KiB writes/readbacks, acknowledges all 48 in app frames, reaps both workers and
checks exact offline files, fsck and allocation. Its initial collector sampled
processes before the compositor's ancillary child retired; bounded collection
now confirms the restored baseline. Native latency/energy remain unaccepted.
Normal Text/Files and 32-document/33-file growth regressions also pass. Clipboard/apps and the matched diagnostic regressions also pass. Review added
an ownership-transition recheck at atomic wait registration, reproduced against
the earlier source in a focused host test. A later lifecycle run exited cleanly
but its live UART success line interleaved; the collector now requires exact
atomic saved-console completions as well as successful child exits. Final actual-ABI,
load and matched-capture checks pass on that corrected checkpoint. The matched
capture validates 20 worker operations, 60 scripted and 44 injected key
presentations, descendant retirement, replacement compositor, outside-image
preservation and offline allocation. Public source guides are refreshed; no
installed native payload or hardware acceptance changed.

### Active shared file-worker lifetime work (D3a)

PR146 is merged as `91b258e` and consumed. Next bounded scope: reusable,
caller-owned file-worker pools for normal Text and Files. Empty documents must
not each spawn a process; queued requests keep their own snapshots and completion
ownership while a small lazy pool leases nonblocking transports. Preserve FIFO
admission, validated responses, visible failure without automatic mutation retry,
and busy-owner retention. Retire poisoned channels and reap their children before
reuse; last-document/application close drains accepted work and then retires the
pool. Dedicated legacy workers remain for the existing diagnostic captures.
Touch the file-job library, Text/Files lifetime and focused host/QEMU fixtures.
Validate actual document growth beyond scheduler-sized worker counts and existing
file/dialog/clipboard/capture behavior. Combined GUI/FD waits and other desktop
services remain separate blocks. Pro PR19 `e409c07` is independent Wi-Fi work.
Host/QEMU only: no installation, USB writes or reboot; native acceptance pending.

D3a candidate `fc30d06` passes full desktop host/image gates, forty queued
actual 64 KiB writes/readbacks using two workers, exact offline contents, fsck and
independent allocation traversal, and restored process baseline. Thirty-two
normal Text documents in one process save independently; a replacement ID33
opens, all 33 files match exactly, and the single reused worker is reaped at
last-window close. The first growth collector relied on individual log lines
and rejected an interleaved diagnostic; application retirement totals now verify
all 33 lifetimes. Dedicated-worker reply draining also has a failing-before
regression for a complete large reply retained after child exit. Existing
Text/Files/capacity/app regressions pass. The matched battery/Text capture also
passes 20 worker operations, all 60 scripted and 44 injected key presentations,
replacement compositor, outside-image preservation and offline allocation.
PR147 carries the reviewed host/QEMU checkpoint; native acceptance is pending.

### Active dynamic compositor and enumeration work (D2c)

PR145 merged as `11a4ef2` and consumed. Next bounded scope: remove the
compositor's sixteen-window admission ceiling using stable allocated records,
separate child-connection storage, transactional registry/scratch growth and
explicit metadata/cache budgets. Extend menu/palette widgets with caller-owned
storage while keeping their allocation-free boundary and inline defaults.
Window menus, launcher, overlay return and drag/resize must retain lifetime
identity rather than select a reused slot. Test growth/refusal preservation,
large window enumeration and actual 32/64/128 small windows in few processes;
existing document persistence and desktop lifetime regressions remain required.
Touch compositor/scene, widgets, shared budgets and focused host/QEMU consumers.
The scheduler/process and surface-dimension bounds remain separate. Pro PR19
`e409c07` still covers Wi-Fi; no native installation or reboot in this block.

D2c runtime `bba1c39` passes the full desktop host/image build and final
QEMU 32/64/128-window workload in one process: complete launcher/Window-menu
discovery, input to first/middle/last windows, maximize/restore, ID129 replacement,
129 retirements, zero registry/cache charges and process-baseline/relaunch checks.
The matched battery/Text file capture also passes 20 worker operations, 60/60
scripted and 44/44 injected key presentations, replacement compositor, unchanged
bytes outside the scratch image and clean offline allocation.
Peak requested registry reservation was 2,020,656 bytes and compositor pixel
cache 38,088,904 bytes. These are separate pools, not total memory. Final sixteen
Text document persistence/refusal/reuse, normal Text editing/dialogs, mixed
clipboard and existing-app/lifecycle QEMU regressions also pass. Screenshot QA
found a partially clipped menu row shifting its glyph origin; a failing-before
pixel regression and corrected VM screenshot cover the fix. Native acceptance
and p95/p99 input/frame measurements remain pending. PR146 carries this work.

### Active dynamic GUI queue storage (D2b)

PR144 is merged as `58bd3cf` and consumed. Next bounded objective: replace
32 fixed kernel receive queues with stable allocated records under a 2 MiB
requested-byte budget, retaining the 64-message per-connection FIFO. Prepare
outside GUI-owned critical sections, then revalidate route, server identity,
retirement epoch and surface transition before atomic publication/delivery.
Failures must preserve seals, FIFO state and byte accounting. Touch kernel GUI,
shared budgets/resource traces and host/actual transport fixtures. Add allocation,
retirement, server replacement and competing-publication cases before integration.
Compositor/scene sixteen-window storage remains a separate D2c step. Pro PR19
at `e409c07` still covers independent Wi-Fi diagnostics. No hardware changes;
keep Ubuntu running and preserve installed/recovery payloads.

D2b runtime `1825ee1` passes the full desktop host/image build and final
fixture rebuild, queue fault/quota/race fixtures, actual 25 simultaneous FIFOs
with 72 client lifetimes reaped across three rounds, multiwindow clipboard/input/
resize/relaunch and sixteen Text documents with seventeen exact files and all
workers retired. The initial 40-client probe failed at fork index26 (`EAGAIN`)
before queue pressure; the independent scheduler task budget remains unchanged.
Host tests separately exercise queue storage beyond 32 records. No native result
or installation changed. PR145 carries this checkpoint.

### Active dynamic surface storage (D2a)

PR143 is merged as `fd38097` and consumed on the same device branch. Next
bounded objective: replace the kernel's fixed 64-surface array with stable,
demand-allocated records and explicit metadata/pixel byte budgets, including
in-flight allocation and retired storage. Preserve atomic seal/queue transitions,
owner/window/serial checks and close/server-death cleanup. Never recycle surface
IDs. Allocate/initialize outside the GUI metadata critical section; recheck
retirement before publishing. Do not claim shorter syscall interrupt exclusion
or native latency from that structural change.

Touch kernel GUI storage and syscall output-fault rollback, shared budget
definitions, resource tracing and host/actual-protocol lifetime and pressure fixtures. Compositor admission remains
sixteen windows in this block; dynamic queues and compositor/scene registries
follow independently. Pro PR19 `e409c07` remains Wi-Fi diagnostics. No hardware
or boot changes; host/QEMU only through 10AM, and no automatic reboot then.

D2a PR144 runtime `028ded1` passes full desktop host/build, 128 live surface
records with independent byte-quota and allocation/retirement failure injection,
actual ABI readback/reuse/stale-ID and multiwindow/relaunch, and sixteen Text
documents with exact persistence and worker retirement. Compositor client-death,
resize and console return/relaunch passed the preceding surface implementation.
The malformed output-buffer probe reproduced guest memory exhaustion before the
syscall fix; 300 read-only requests now consume no IDs before normal rendering.
No kernel heap allocation/free occurs inside the GUI-owned metadata section in
the instrumented host fixture. Native input timing remains untested.

### Active multi-document Text migration (D1c)

PR142 is merged as `ed2015a` and consumed. Next bounded scope: migrate normal
Text to one connection dispatcher with dynamically allocated document records,
shared tool event handlers and independent undo/save/dialog/clipboard state.
Add New Window while preserving Ctrl-N's current document-reset behavior.
Close must drain accepted file work, await window retirement and reap the file
worker without closing siblings. Keep diagnostic capture recipes compatible.
Touch desktools/runtime, file-job ownership, connection liveness checks and
host/QEMU document/persistence tests, launcher window identity and recycled
kernel surface binding; no hardware or boot changes. Fixed global
sixteen-window admission and dynamic compositor registries remain separate.
Pro PR19 `dbf46c3` continues independent Wi-Fi RF work. Host/QEMU only overnight.

D1c PR143 candidate `f4f4c9c` passes full desktop host/build, normal Text
multi-document/launcher, undo/history, Find, Replace, and sixteen mixed clipboard
client VM regressions. Sixteen same-process Text documents each save independently;
the seventeenth refusal preserves edits, a replacement uses ID18, seventeen files
match exactly and every worker is reaped. The original nine-document brk refusal
is retained locally; large records now use anonymous mappings. Actual close/reopen
also exposed the recycled kernel surface binding, fixed with a failing-before
host regression. Launcher selection now distinguishes same-PID documents and
rejects stale lifetime identities.

The legacy file-work capture needed its dedicated Q35 kernel rebuilt for v14.
With matched binaries the guest completes, but its collector had not learned the
existing five write-readiness diagnostics. Ordered-record validation and negative
fixtures now pass; the final full capture/relaunch/offline-integrity run passes: 60/60 scripted and 44/44 injected key events presented, five file cycles,
20 worker operations, three descendants retired, replacement compositor works,
all bytes outside the scratch image unchanged and offline allocation clean.
No native acceptance is added; installed/recovery payloads are unchanged.

### Active connection and multiwindow lifetime work (D1b)

PR141 is merged as `b51c21b`; this checkout consumes it. Next bounded scope:
one process-level dispatcher, monotonic window handles, correlated creation
refusal and asynchronous close acknowledgement. Route input/capture, menus,
clipboard and notifications by window. Preserve the single-window source wrapper
and existing limits. Touch shared protocol, kernel packet validation, desktop
client runtime, compositor admission/lifetime and host/actual-protocol VM tests.
A two-window transport consumer will precede Text document migration; do not
claim full D1 acceptance until real independent documents and close/save paths
are exercised. Dynamic registries and combined GUI/FD waits remain later blocks.
Host/QEMU only; installed/recovery remain unchanged. No parallel harnesses.

The candidate connection contract is documented in
[desktop connections](../desktop-connections.md). Focused host fixtures pass
identity/lifetime, blocked delivery, typed refusals, independent capture/notices
and local canvas failure. Runtime `e5ad6c2` passes the matched build/full host gates and actual-protocol
VM consumer: two independent editable documents in one process, focus/clipboard,
close preserves sibling, fresh ID, resize, cleanup and relaunch. Existing apps,
sixteen clipboard clients/refusal/reuse/exact data, and compositor lifecycle
regressions also pass. PR142 is ready; normal Text saving/dialog migration remains
next. Screenshots were inspected; native payloads remain untouched.

### Active explicit window identity foundation (D1a)

D0 clipboard PR140 is merged as `d8f6c71` and consumed on
`device/macbookair7-2/desktop-runtime`. Next bounded scope is matched protocol
v13 with an explicit 64-bit window ID, kernel surface seal/release checks and
client-side event filtering. The current one-window wrapper uses ID1; other
window IDs are not yet admitted by the compositor. This is a staged migration,
not multiwindow app or dynamic capacity acceptance. Touch shared protocol,
kernel GUI seal validation, desktop/clipboard/notification client helpers,
compositor routing and their host/VM fixtures. D1b will add connection dispatch,
multiple window admission and lifecycle semantics before multiwindow Text.

D1a runtime `d9a678b` passes cross-window event filtering, clipboard/notification
reply identity, atomic surface binding/release, full desktop host coverage and
the matched image build. QEMU passes desktop app/file/menu regressions, sixteen
clipboard clients with refusal/reuse/exact saved contents, and compositor
input/resize/client-death/server-exit/relaunch. PR141 holds this foundation;
no new native acceptance is claimed. Raw receipts and inspected screenshots
remain in the overnight evidence root and durable backup.

Pro PR19 at `7b95685` remains independent Wi-Fi work. Host/QEMU only through
10AM America/New_York; no boot installation or laptop reboot. Preserve the
native accepted payload/recovery. The same overnight checkpoint/evidence root
and automation apply. Tests and PRs must identify which stage they accept.

### Active desktop runtime work

PR134 is merged as `7a00518`; this branch consumes that exact shared baseline.
The native POSIX capture below is complete. Branch:
`device/macbookair7-2/desktop-runtime`. Overnight work is host/QEMU only; no
laptop reboot or boot-install changes before 10:00 AM America/New_York on
October 25, and no automatic reboot scheduled for that time.

First scope (D0): eliminate the separate eight-peer clipboard ceiling by owning
transfer state with each admitted desktop client, cover many clients and queue
pressure, and document resource/refusal behavior. Touch compositor clipboard,
client lifecycle, shared budget definitions and host/VM tests. Next D1 adds
connection/window identity and a single dispatcher, with multiwindow Text as
its consumer; dynamic window storage follows identity. The existing
[desktop API plan](../desktop-api-evolution.md) owns the sequence and gates.
Pro PR19 at `2674fa7` remains separate Wi-Fi work. Shared kernel transport changes
will be published explicitly before D1. Evidence/checkpoints:
`build/desktop-overnight-20261025/`, backed up outside build. Actions stay disabled;
serialize all builds and VM harnesses. Existing native acceptance remains scoped
to its recorded releases.

D0 runtime `18d92f1` passes full desktop host tests and image build. The
32-owner clipboard fixture covers 64 KiB transfers, blocked readers, focus
refusal, writer timeout and slot reuse; compositor tests cover retiring a writer
before its client record is cleared. The corrected mixed-app VM now passes
sixteen real clipboard connections, seventeenth launch refusal, slot reuse,
exact cross-client saved contents, screenshot and full process/cache cleanup.
Existing mixed-capacity and desktop app regressions also pass, including saved
file contents and process/cache retirement. The standalone capacity rerun exits
zero; an earlier combined wrapper ended with143 after printing the complete
capacity PASS and before launching the next test. Both receipts are retained.

The first all-Text run reached thirteen clients: clipboard worked, but the
thirteenth file worker was unavailable. Its document stayed intact and the UI
reported the refusal. Keep that result as a separate process/worker capacity
limit for the shared application runtime; the mixed clipboard pass does not
accept sixteen simultaneous file workers. An intermediate fixture expected
empty-selection Copy to send a request; the UI correctly sends none. That
fixture was corrected to nonempty data. Failed receipts remain local.

### Native POSIX regression accepted

Runtime `1882029`, release `284e53065081672f`, completed unattended on this
Air7,2 in session `session-00065e9fe3b89b2a-00`. Full thread contracts,
posixdemo, TLS, SQLite and the desktop/Text capture all exited zero; all five
owned process trees retired. The matching four saved logs have no collector
attention. The 1440x900 screenshot was hash-verified and visually inspected.
The desktop's 60-second sample recorded 98.48% idle scheduler ticks; this is
an idle workload observation, not a measure of energy or interactive latency.

All 1054 previous files retained their bytes; only expected `.srvros/meta`
bookkeeping changed. Offline fsck is clean (1059 files); independent allocation
traversal reports 2578 clusters, zero leaks, mismatches or errors. Ubuntu returned
with its default preserved and the one-shot selection consumed. Installed and
recovery payload hashes match; recovery `9aa8dbf6fbda8252` and independent copies
remain available. Full raw evidence is retained under
`build/posix-core-20261024/native` with a durable copy outside `build`.

Preparation added an explicit `--posix-capture` and independent volatile RAM disk.
It exposed a pending signal after an already-completed pthread join; `dddb07f`
fixes the delivery checkpoint after record retirement. The directed test fails
before and passes afterward; full core/build, libuv and exact-stage positive and
missing-workspace VM rehearsals pass. Failed attempts remain in local evidence.
Normal device startup gates are unchanged. This bounded test does not establish
new physical gesture, battery, sound, networking or full POSIX acceptance.

Main `211f17c` remains the integration base; Pro PR19 `631105b` has independent
Wi-Fi scope. PR134 is ready for integration with these native results. Next is
the desktop connection/window identity milestone described in the
[API evolution plan](../desktop-api-evolution.md), before dynamic capacity and
combined GUI/descriptor event waits.

### POSIX portable-application milestone checkpoint

The selected profile is implemented and locally validated at runtime source
`7e2cf54`. Full core, forced partial-tick poll timing, focused readiness and full
thread suites pass. The final combined gate passes libc, ports, process/shell,
libuv, utilities, Lua files/strings/UTF-8/bitwise, Terminal and Files. Public scope,
bounds, remaining Unix/Linux gaps and reproduction commands are in the
[portable C profile](../posix-supported-profile.md). This is not POSIX certification.

Key final corrections: shared endpoint state/errors `6c83966`, libuv descriptor
mapping `0d0258b`, atomic socketpairs `050a837`, stream reopening `12b33d9`, and
monotonic poll deadlines `7e2cf54`. The measured wait fixture found and retained
a real early-timeout failure before the corrected host/VM passes. Evidence:
`build/posix-core-20261024/final-gate-checked.json` plus its prerequisite receipts;
backup: `~/.local/share/srvros/reviewed-evidence/posix-core-20261024`.

Main remains `211f17c` (consumed as `88c2cbb`); Pro PR19 `e66d47d` retains Wi-Fi
scope. PR134 contains the reviewable shared changes. Desktop implementation is
deferred; combined GUI/FD wait is a documented design only. No physical trial is
armed. Installed `bf68e4e9b88b8ae7`, recovery `9aa8dbf6fbda8252` and USB
`37D8-C5D0` are unchanged. Next discussion: integration/native regression of this
profile and the planned connection/window identity migration; do not repeat the
completed POSIX blocks or claim untested broader APIs.


User authorizes sustained implementation of the POSIX plan before the desktop
overhaul. Base `76a55dd` (merged architecture audit PR133), branch
`device/macbookair7-2/posix-core`. First shared changes: libc error state,
positional I/O syscalls/process-file operations, poll/select and signal/time
contracts, guest semantic probes and local host/VM harnesses. Continue through
the audit's compatibility blocks with explicit unsupported features rather than
claiming certification. Desktop identity/capacity work is deferred.
Pro PR19 Wi-Fi and Air6,1 PR132 combined device services remain independent;
preserve their profiles and acceptance. Publish coherent commits and validate
locally with Actions disabled. No native reboot is needed for the first blocks;
installed `bf68e4e9b88b8ae7`, recovery `9aa8dbf6fbda8252`, USB `37D8-C5D0`
remain unchanged. Progress/evidence: `build/posix-core-20261024/`.

Published implementation commits through `a9e63e5`: per-thread errno, kernel
positional I/O, select/readiness corrections, interrupted/absolute sleep,
reclaimed worker TLS, thread-directed signals, actual CPU accounting and peak
mapped-memory usage. Focused host tests and full thread/libc/TLS/libuv guest
suites pass for their respective blocks. Pthread key/read-write lock semantics
now pass their semantic guest tests. A same-process scheduler CR3 regression
exposed by stress is fixed in `fad4bf4`. Regular-file access/exclusive-create/shared flags and safe truncation pass the
full core and thread VM suites. Kernel file-size limits, inheritance across
spawn/exec, thread-directed SIGXFSZ and explicit unsupported priority controls
also pass full core/thread validation; wider port regressions are running.
Wait-status/child-group selection and pselect/ppoll mask restoration now pass
full core/thread tests. Descriptor coherence and the broader profile remain
in progress. Resource libc/libuv/utility suites pass; the full ports suite exposed
an old poll fixture which sent its signal before waiting, now replaced by a
delayed targeted signal and being rerun. Main advanced to `e79e220` with Air6,1 PR132; its device-profile/test
changes do not overlap these runtime files. Pro PR19 at `31638a9` remains Wi-Fi
work. No native reboot has been armed or physical acceptance claimed.

Integrated shared main `e79e220` as `1cd62ae`, retaining Air6,1 changes. The
pre-fork image passes full local core, thread, ports (including SQLite/posixdemo),
process, libuv, utility and libc/TLS VM suites. Expanded tests exposed outdated
signal recipient/timing assumptions and an optional shell-banner assertion;
corrected fixtures pass and original failures remain in local evidence. Native
installed/recovery payloads are unchanged. PR134 remains a draft because the
compatibility milestone is not complete.

Next bounded block: replace the fork stub with eager private-address-space cloning,
new child register/TLS/FPU continuation, inherited regular descriptors/pipes,
atfork callbacks and guest failure/exec/reaping tests. Start with a single-thread
CLI profile; refuse active-worker and unsupported pseudo-descriptor cases rather
than copying inconsistent state. Shared-file coherence and general multithreaded
fork remain separate acceptance. Touch process/syscall/usermode continuation,
libc process glue and focused tests; no hardware profile changes or reboot.

The first native fork VM fixture passes private memory/TLS, shared descriptors,
parent/child returns, callback ordering, resource/mask inheritance, repeated
reaping and fork/exec. Expanded exec-family/error/signal inheritance checks pass in the full
`thread-fork-contracts.log` guest suite. Next shared-files block will separate cached contents from open
description offsets, preserve bounded in-memory reads during owned I/O, and test
cross-open writes/truncation/stat/close ordering and allocation failure. Native
filesystem publication and unsupported unlink-while-open remain explicitly
separate from this coherence block. The shared-content host tests, full core/build
and full `thread-shared-files.log` suite pass, including four concurrent appenders.
Main advanced to `36080fc` with Air6,1 documentation-only PR135; no runtime overlap.
Ports and process/shell regressions also pass. The libuv duplex-spawn regression
exposed missing execute metadata on an initramfs program; the loader correctly
refuses it. Current follow-up preserves tar modes/stages executable ELF permissions.
Orphan-file/lock lifetime host checks, full core/runtime-ELF tests and all six
thread/libuv/ports/process/posixutils/libc VM regressions pass. Deleted open files
retain data/identity/locks until close, pathname reuse stays isolated, and
`tmpfile` creates an unlinked stream. A focused guest failure exposed libc
turning a correct zero link count into one; the correction passes the rerun.
Original failures remain in the evidence directory.
Evidence backup: `~/.local/share/srvros/reviewed-evidence/posix-core-20261024`.
Next bounded scope: kernel-owned working directory inherited by fork/spawn and
preserved by exec, checked path traversal, native shell/Terminal handoff, then
directory descriptors and portable consumer tests. Touch process/syscall/libc,
shell/Terminal and semantic tests; retain existing file and I/O guarantees.
No reboot is needed. The working-directory block passes full core, focused path
VM and process/libuv regressions, including empty-environment exec and shared
thread CWD. The first build missed the shell's unistd include; corrected build
and tests pass. Next: descriptor-backed directories/openat/fchdir and real
file timestamp updates. Main remains `36080fc`; Air6,1 PR136 battery deadline and
Pro PR19 `d7fb8f9` Wi-Fi changes do not overlap this compatibility block.

Directory/timestamp candidate passes full core, focused host/guest and all
thread/libuv/ports/process/posixutils/libc/directory VM regressions. One process
run stopped at the bootloader; headless rerun with checked admission passed.
Unattended serial tests no longer open graphical windows. Directory handles
pin paths; timestamp updates use real metadata with NOW/OMIT and second resolution. Next bounded compatibility scope is a pinned Lua
5.4.8 consumer gate with its unmodified file tests, enabling the OS library and
POSIX build profile. Its rename expectations also require reviewing exFAT
cross-directory placement and replacement publication. Air owns any resulting
shared `kernel/src/fs/exfat.c`, process-content identity, libc stdio and regression
fixes in PR134. Preserve the native guarded file-manager rename contract and
quarantine on uncertain I/O; no native USB trial or acceptance is implied.

The next consumer block passes the focused rename and stdio-access guest tests
and pinned Lua file/date tests plus subprocess output/EOF/exit checks. It adds
regular-file replacement/open-handle lifetime, exact binary32/64 parsing, missing
OS-library primitives and noninteractive shell output cleanup. Full local
regressions are running. Fault injection explicitly exposes non-atomic dirty
rename recovery: fsck can retain the source and truncate the duplicate target.
Do not claim journaled/old-or-new recovery. Original failure logs and images are
under the existing evidence directory. Main advanced to `85d5939` (Air6,1 PR136
battery deadline); integrate after this checkpoint. Air6,1 PR137 startup timing
and Pro PR19 `b754d92` Wi-Fi remain outside this compatibility scope.

The ports regression exposed a native spawn namespace collision: closing a
parent-numbered pipe descriptor could close a newly allocated child standard
stream. Removing those invalid close actions restores the full ports probe;
ordered inherited file actions require a separate descriptor-table block.
The default-ignored signal cleanup also has a focused host regression. Remaining
Lua/libuv/utility checks are running. Main is now `4f8acd2` (Air6,1 startup timing);
PR138 heap optimization and Pro PR19 `d5010dc` Wi-Fi are separate active scopes.
Next bounded block is path/limit accuracy and exec fallback: shared path capacity,
checked directory import, pathconf/fpathconf and semantic guest/remount tests.
General descriptor inheritance, atomic mode/umask creation and asynchronous
signals remain required follow-ups before the compatibility milestone can close.

Published through `d5d0fa1`, consuming main `4f8acd2`. Consumer block completes
all thread/process/libc/ports/Lua-files/libuv/utility checks locally. Current
1,024-byte path/255-byte name and exec-shell-fallback candidate passes full core
and full thread VM tests; exhaustive exFAT remount/fault checks pass, including
long paths on 4K/128K clusters and independent offline traversal.
The first fallback test exposed quoted shell exit operands returning zero;
corrected parsing passes the full guest suite. Remaining boundaries include
resolved-absolute-path capacity, native GUI message limits, 16 arguments/256-byte
strings, general FD inheritance and atomic mode/umask creation. No hardware test
or desktop architecture change is claimed by these compatibility results.

### Desktop architecture, scalable windows and POSIX audit

Base `bc0edb9` / merged PR130. Review connection/window identity, compositor
storage and scheduling, GUI transport and surface ownership, public application
and widget APIs, service boundaries and portability. Plan removal of the fixed
window count through demand allocation and explicit resource admission. Audit
the actual POSIX/libc implementations and tests, distinguishing working subsets,
restricted semantics, unsupported calls and compatibility placeholders.
Deliver source-backed inventories, phased API/resource migration and test gates;
this planning block changes documentation and audit evidence, not the native ABI.
Pro PR19 at `3996f9f` owns independent Edimax Wi-Fi work. No overlapping device
implementation is planned. Installed `bf68e4e9b88b8ae7`, recovery
`9aa8dbf6fbda8252`, USB UUID `37D8-C5D0`; the previous run is collected and
accepted for its bounded checks. No physical test or reboot is needed here.

Review complete: [desktop evolution](../desktop-api-evolution.md) inventories
PID/window coupling, dynamic-registry prerequisites, resource/service limits and
the proposed API migration gates. The [POSIX audit](../posix-compatibility.md)
classifies implemented subsets, semantic defects and compatibility placeholders.
Local libc, thread and libuv guest smoke suites pass; host fixtures reproduce
the eight-peer clipboard ceiling and process-shared errno storage. These are
known gaps, not passing semantics. Evidence and image hashes are in
`build/desktop-api-audit-20261024/`; runtime source and installed payload are
unchanged. Current architecture, status, roadmap, porting and toolkit guides now
link the review and correct stale window/PTY/provider descriptions.

Next bounded implementation: current clipboard service capacity/refusal and
thread-local errno with focused regressions, followed by connection/window
identity. Publish that scope before editing shared runtime code. No new native
acceptance is needed for this documentation checkpoint; dynamic windows and new
POSIX semantics remain proposals.
Final integration check: main remains `bc0edb9`; Pro PR19 advanced to `b071f38`
and Air6,1 PR132 adds its combined-services profile. Neither owns this desktop
API/POSIX review's files; their device acceptance remains in their own records.

### Desktop window capacity: native result and integration

On return from `bf68e4e9b88b8ae7`, tester reports all sixteen windows opened
well. Session `session-00065e9a46b48da1-00` registers slots 0–15, saves
`capacity-proof.txt`, retires the desktop normally and saves its exit snapshot.
All 1049 prior files retain their bytes except expected exFAT metadata; offline
filesystem/allocation checks and Ubuntu/default/recovery checks pass. No new
screenshot was saved. This non-trace session omits cache totals and per-frame
commit traces, so native cache retirement and pressure injection are unmeasured;
those mechanisms have host/VM coverage. Acceptance here is sixteen-window
opening and saved-file/clean-exit integrity, not an additional physical resize,
pressure-failure or screenshot claim.

Collection initially flagged the normal exact `trackpad-tap-reject` movement
counter because its generic rule treats `reason=` as actionable. Correct that
specific expected telemetry with regression tests while retaining malformed,
unknown and actual failure lines. Keep the original collection receipt and
review the saved evidence again; no rerun is needed to correct host analysis.
Integrated main `d5bc4b3` (Air6,1 PR131) as `e22f328`, preserving its battery
gates and staging work. Full local build/core/docs and the sixteen-window VM
regression pass on that integration. PR130 is ready for baseline promotion;
GitHub Actions remain disabled. Raw/reviewed evidence and the accepted payload
survive `make distclean` in local pending-trials/reviewed-evidence storage.
No reboot is armed. Installed payload remains `bf68e4e9b88b8ae7`; source merge
validation does not silently install a different native image.

### Window capacity implementation checkpoint

Base `468ca20`. Replace eager full-screen compositor cache pairs with bounded
buffers allocated for live window sizes. Preserve the published frame and input
ownership on failed growth, release buffers when a window retires, and test
allocation/copy failures. Audit process, GUI queue/surface, menu and launcher
budgets before raising the client limit toward sixteen; exercise opening beyond
eight windows, repeated maximize/restore, close/reopen and pressure refusal.
Scope: compositor/client buffer lifecycle, shared desktop capacity/protocol if
needed, GUI resource budgets, host/VM regressions and current desktop guides.
Candidate implementation and local tests cover sixteen windows, 96 MiB bounded
private cache storage, current-size client canvases, transactional resize refusal,
and delayed redraw under pressure. Protocol v12 requires a matched image. Native
acceptance is pending the attended capacity trial; Help includes a persistent
checklist. Pro PR19 at `d0a0b57` remains independent Wi-Fi work; Air6,1 PR131
at `75ddde8` changes opt-in battery startup and separate staging/tests.
Neither overlaps the compositor/client/menu changes. At `b96e990` the full local
build/core/docs checks, sixteen-window exact text/screenshot/cleanup workload,
800x600 and 1440x900 arrange tests and 640x480 compositor lifecycle tests pass.
The exact staged two-session desktop persistence rehearsal passes for
`bf68e4e9b88b8ae7`; its sixty-round workload, saved text/preferences, logger
snapshots and both clean unmounts pass. Physical acceptance remains pending.
Installed attended candidate `bf68e4e9b88b8ae7` (compiled `b96e990`), recovery
`9aa8dbf6fbda8252`. Ubuntu remains the default; native acceptance awaits the
capacity round. No timer closes this interactive desktop; tester exits and
reboots from the monitor. Local run/collector:
`build/window-capacity-20261024/run.json` and `collect-native.py`; durable payload
and recovery are under the local pending-trials archive outside `build`.
Publish this branch/PR, validate one build/VM at a time, then prepare a physical
trial with explicit on-screen instructions if human interaction is required.

### Pending USB event-service gaps: bounded native result

Based on `bf1b9a6` / PR127, this unit added conservative bounds on
intervals between timestamped event-ring checks while a measured storage request
is pending, including the final interval through matching-event dispatch.
Scope: xHCI poll/event bookkeeping, BOT timing aggregates, logger/comparison wire
records and backward-compatible collectors, directed host and VM regressions.
Sample timestamps at most once per 256 ring checks to limit diagnostic overhead;
submission and matching dispatch close the interval. These are software sampling
bounds, not hardware completion timestamps. Preserve waits/deadlines, DMA
quarantine, input service, readback and flush. The normal 20 ms policy stays.
Pro PR19 was rechecked at `636ecf9` (documentation since `d80fb47`): MAC Wi-Fi state,
capture recipes and tests remain separate from the storage path. Preserve both
changes on integration. Air6,1 PR128 screenshot-receipt changes are independent;
its collector regression edits are in another part of the shared test file.

Source `00ad3cc` passes the full local build/core suite, injected steady/pause and
clock/saturation checks, versioned collector regressions, throttled USB copy with
input/exact readback, and timeout/quarantine VM checks. An exact staged idle VM
persists six logger snapshots with complete service-gap fields. The staged
fixed-file VM passes all 18 version-3 samples / 54 version-2 transfer records,
exact payload and policy restoration. Missing-workspace automatic return passes.
These validate instrumentation and workflow; the native result follows below.

PR129 release `46b9132589405099`, compiled `00ad3cc`, returned automatically in
`session-00065e8eb5c8f180-00`. All 18 version-3 measurements / 54 complete phase
records and exact offline fixture pass, with zero callback failures. All 1046
prior files retain their bytes except expected exFAT metadata; only boot/exit
logs and the fixture were added. Offline checks pass at 1049 files / 2527
clusters, zero leaks, bitmap mismatches or errors. Ubuntu default, consumed
one-shot and recovery `9aa8dbf6fbda8252` are verified. USB is unmounted; no new
reboot is armed. The installed current payload remains the automatic comparison.

Across 2124 CSW callbacks, 47 exceeded 100 ms and none exceeded one second.
Total callback time was 10108415 us, including 10103993 us before software event
observation and 1403 us after it. Maximum callback was 281928 us; maximum
observed-event-to-caller delay was 3 us. Five data callbacks also exceeded 100 ms
(maximum 175237 us). The slowest whole operation was 5724579 us: its 33 slow
status callbacks occurred under continuous polling, with 286274049 actual ring
checks and 1118319 timestamped samples; the maximum sampled gap was only 13 us.
It was not one request exceeding the two-second deadline.

| Poll-before-wait | Median operation | Maximum operation | CSW >100 ms | Maximum sampled CSW service gap |
| --- | --- | --- | --- | --- |
| Poll only | 264.755 ms | 5724.579 ms | 33 | 13 us |
| 4 ms | 181.763 ms | 918.210 ms | 2 | 10011 us |
| 20 ms | 181.765 ms | 2345.151 ms | 12 | 10011 us |

The wait modes retain the expected approximately one-tick servicing bounds.
These measurements exclude long lapses between software ring checks as the
principal cause of this run's long callbacks. Controller/media behavior and any
event backlog remain unresolved; no hardware completion timestamp or device
fault is established. Unequal tail incidence in one boot cannot rank policies or
energy efficiency. Keep the normal 20 ms threshold and verified flush/readback.
This bounded diagnostic milestone is complete; further timing investigation can
wait for a reproducible workload problem rather than changing transport policy
from variable flash-drive tails alone. Physical interactive or normal-desktop
logger acceptance is not supplied by this fixed-file run.

Main `8ec864c` (Air6,1 PR128 screenshot receipts) was merged as `34934f6` after
collection. Its storage kernel is unchanged from the native-tested source.
Combined-tree local build/core, exact idle logger/screenshot capture and all
18 fixed-file capture samples pass. This combined-tree validation is VM-only;
the installed release and native source above remain distinct. Local evidence
is in `build/storage-service-gaps-20261024`, with payload/results archived outside
build at closeout. Fetch the actual integration baseline and active Pro scope
before the next feature block.

### Individual USB timing: bounded native result

Starting from merged main `76f98c4`, this unit is instrumenting synchronous USB
storage callbacks to distinguish submission-to-software-event-observation from
observation-to-caller-resumption. Neither interval is pure hardware/device time.
Scope: xHCI storage request/event bookkeeping, BOT timing aggregates, workspace
logger/counterbalanced capture records, collectors and directed host/VM tests.
Keep the 20 ms poll threshold, two-second deadline, DMA quarantine, input service,
readback and flush policies unchanged. Inspect individual maxima and slow-request
counts before deciding whether a scheduling or transport change is justified.
Pro PR19 was rechecked at `1893150`: WCID Wi-Fi structs/capture work is separate
from the synchronous storage bookkeeping here; both must survive integration.

Source `ce1c537` passes the full local build/core suite, directed clock/order/
overflow/stale-event and parser regressions, throttled 4 MiB USB copy with input
and exact readback, and timeout/quarantine VM checks. The exact EFI/GPT idle VM
persists six logger saves with complete per-phase timestamps. The fixed-file VM
persists all eighteen version-2 samples / 54 phase records and exact payload;
missing-workspace automatic return also passes. These validate instrumentation
and workflow, not physical latency or improved performance.

PR127 candidate `b824d64a95b27a3b`, compiled `ce1c537`, returned automatically in
`session-00065e8e6e515108-00`. All 18 measurements / 54 phase records pass, with
exact offline fixture bytes, zero callback failures and unchanged bytes for all
1043 previous files except expected exFAT metadata. Only boot/exit logs and the
fixture were added. Offline checks pass at 1046 files / 2523 clusters, zero leaks,
bitmap mismatches or errors. Ubuntu default, consumed one-shot and recovery
`9aa8dbf6fbda8252` are verified; USB is unmounted and no reboot is armed.

Across 2124 CSW callbacks, 17 exceeded 100 ms and none exceeded one second.
Maximum callback time was 307216 us; total was 5121771 us, including 5117657 us
before software event observation and only 1616 us after observation. Maximum
observed-event-to-caller delay was 20 us. The longest operation, 2427278 us,
included twelve CSW callbacks above 100 ms, rather than one transfer exceeding
the two-second timeout. Two data callbacks also exceeded 100 ms (maximum 173286 us).
The software observation boundary still includes controller/media and event-ring
service delay, so this does not identify a device fault or establish hardware
completion latency. It excludes large post-observation caller delays for this run.

| Poll-before-wait | Median operation | Maximum operation | CSW callbacks >100 ms |
| --- | --- | --- | --- |
| Poll only | 256.757 ms | 447.558 ms | 3 |
| 4 ms | 263.926 ms | 628.466 ms | 2 |
| 20 ms | 181.353 ms | 2427.278 ms | 12 |

Unequal tail incidence in one boot cannot rank policies; retain the normal 20 ms
threshold. The bounded instrumentation/workflow milestone is complete. Sustained
normal-desktop logger timing remains covered by VM rather than this native fixed
fixture. Next storage scope is bounding event-service gaps while a transfer is
pending, to further distinguish device/controller delay from event servicing.
Local evidence and collector are in `build/storage-timing-20261024`, backed up
outside build under the release's pending-trial archive. The installed payload
is the automatic comparison, not an ordinary desktop session. Fetch the actual
integration baseline and active Pro scope before starting another change.

### TCP readiness and e1000 receive ownership follow-up

PR125 resolves the legacy readiness failure retained by the network-idle
milestone. Packet capture on the accepted kernel showed SYN/SYN-ACK/ACK complete
before the listener timed out: only arriving payload queued a connection for
acceptance. The fix queues a validated final ACK, independently of data, once.
Invalid handshakes remain hidden and queue exhaustion resets/retires the new
connection. Original guest deadlines and host payload ordering are unchanged.

Compatibility testing also exposed e1000 receive-ring reentrancy: a woken
application could poll the DMA descriptor still owned by the outer callback.
The accepted baseline reproduced intermittent HTTP stalls too. An atomic,
non-spinning receive-owner guard prevents duplicate dispatch or skipping the next
packet, including direct monitor polling. Both packet-level TCP and nested-poll
host regressions fail before their respective fixes and pass afterward.

At source `4e9e7ad`, full local build/core passes; original readiness and
descriptor-lifetime VMs pass. ECM traffic passes against the same TCP change
(before the e1000-only guard): DHCP/DNS/HTTP, UDP echoes, TCP serving, cable
recovery, shared keyboard and removal quarantine. Refined e1000 HTTP requests,
slow-peer coexistence and closed-port refusal pass. The web fixture now explicitly
syncs buffered open service logs before reading them and requires successful
sync; all previous content assertions remain. Corrected web fixtures pass with
shorter and default host waits. The idle/input/save fixture passes at 2.095
network switches/second, with titlebar drag, mixed-case typing and exact save.

Main `c4265f8` (Air6,1 PR124) is merged as `3a94ab2`; Pro PR19 `9b0d63c`
retains independent Wi-Fi work. Combined-tree build/core and original readiness,
web serving/log readback and idle/input/exact-save VMs pass. Network activity
remains about two switches/second. No physical reboot, installed-payload replacement or
sibling acceptance is implied; installed Air release remains `27b239f96a183fb8`,
recovery `9aa8dbf6fbda8252`. Details and commands are in
[TCP readiness](../tcp-readiness.md). Raw failed and successful comparisons stay
under `build/tcp-readiness-20261023`, with an outside-build archive at closeout.

USB logger variability is the next separate scope. Multi-second saves also
occurred before network idle changes; aggregate CSW time cannot distinguish
media delay from completion/scheduling delay. Preserve verification and flush
policy, and measure individual transfer/owner timing in a controlled comparison
before changing thresholds. No new hardware test is armed.

### Network idle milestone accepted for the bounded workload

Release `27b239f96a183fb8`, compiled source `ceaa2ce`, returned automatically in
`session-00065e8d8f28fd65-00`. The one-minute desktop capture passed, with five
complete periodic saves, a valid 1440×900 Text desktop screenshot and clean exit.
All 1038 previous files retained their bytes except expected exFAT metadata;
only four logs and the screenshot were added. Offline checks pass at 1043 files /
2519 clusters, zero leaks, bitmap mismatches or errors. Ubuntu default, consumed
one-shot, current payload and previous recovery `9aa8dbf6fbda8252` are verified.
USB is unmounted; no further reboot is armed.

| Observed workload | Previous `f340e0695d1e2492` | Notification-ready worker |
| --- | --- | --- |
| Network switches / measured interval | 5005 / 51.529 s | 102 / 47.728 s |
| Network switches per second | 97.13 | 2.14 |
| xHCI input worker switches per second | 100.00 | 100.00 |
| Compositor switches per second | 10.033 | 10.036 |
| CPU0 idle timer samples | 98.30% | 97.52% |
| Completed periodic saves / bytes | 6 / 136710 | 5 / 106892 |
| Total save wall time | 1.883555 s | 7.942433 s |
| Individual save range | 149.316–622.689 ms | 200.222–3792.337 ms |
| Package power / measured interval | 6.786 W / 51.999 s | 6.840 W / 47.998 s |

This accepts the reduction in redundant network worker activity, about 97.8%,
with the independent USB service cadence unchanged. These are scheduler switches,
not hardware wakeups. Native traffic and typing were not exercised in this idle
recipe; directed VM traffic/input coverage is recorded below. Battery service was
deliberately off. No energy, whole-system performance or physical input improvement
is established: idle percentage fell slightly and package power did not improve.

USB saves were slower in this run, despite zero callback failures and exact file
preservation. CSW callbacks account for 7.531335 seconds of the 7.942433-second
save total; callback wall time includes scheduling and is not pure device wait.
The single comparison does not identify the cause or rule out a regression.
Retain this as a storage-latency follow-up, alongside the unresolved legacy TCP
readiness failure below; neither is silently accepted as fixed by this milestone.

Final review uses unchanged main `7b0d1a0`, Pro PR19 `933f244` (independent Wi-Fi)
and Air6,1 PR124 `022d468` (attended Home capture). Shared harness edits are in
separate capture branches and must both survive integration. PR123 contains the
provider contract, bounded fallback/recovery and local regressions. Runtime is
unchanged since the build/core and VM validation below; final docs checks pass.
Raw evidence, failure receipts and installed/recovery payloads are retained outside
`build` in local pending-trial/reviewed-evidence storage. Next scope is diagnosing
legacy TCP readiness and explaining USB save variability before further idle
changes; no new implementation or physical run is armed.

### Network idle validation before the physical comparison

Kernel implementation `ceaa2ce`, test harness `5f2322a`, PR123: full local
build/core checks pass. Refined e1000 and ECM idle fixtures measure 2.10 and
2.00 network worker switches/second, with unchanged USB polling cadence and
successful post-idle drag, mixed-case typing and exact save. ECM DHCP/DNS/HTTP,
160 UDP echoes, TCP web serving, three traffic outage/recovery cycles, shared
keyboard and physical-device removal quarantine pass in QEMU. e1000 desktop
DNS/ping, link down/up, cancellation, typing during a pending job, exported
reports and clean exit pass. Throttled USB copy/input with exact binary/document
readback and collision preservation passes.

The older `tcp_ready_smoke.py` fails `ready-listener` on both accepted kernel
`f340e0695d1e2492` and this candidate in a controlled disposable comparison;
launching through the shell also fails. This remains an unresolved preexisting
test/behavior issue, not a pass or a claimed fix. Do not equate the narrower
passing traffic tests with general TCP readiness acceptance. Raw failure logs
and comparison recipes are retained for follow-up.

Exact EFI/GPT release `27b239f96a183fb8` (source `ceaa2ce`) passes the explicit
`--probe-network` rehearsal: ECM worker 101 switches over 50.301 seconds
(2.008/s), six valid logger cost records, offline persistence and automatic
reset. Missing workspace returns safely. The initial adapter-free EFI rehearsal
passed boot/persistence but could not qualify network activity; its receipts
remain separate. Harness changes after the staged source do not alter its
kernel or userspace payload.

The prepared one-minute unattended Air7,2 capture is now collected above.
Recovery remains `9aa8dbf6fbda8252`. The unreferenced comparison EFI payload
`2218ef4113730414` was hash-verified into outside-build archive storage before
freeing its EFI space. Raw run state, collector and evidence remain under
`build/network-idle-20261023`, with durable backups. No physical input or energy
saving is accepted by this unattended trial.

### Implementation scope and retained initial failure

Base `7b0d1a0` includes merged PR118 and Air6,1 acceptance notes. Pro PR19
`9b01d39` retains Wi-Fi bring-up; this scope avoids its MCU/baseband and control
completion paths. Audit the selected network provider's receive notification
readiness and use the existing 50-tick recovery cadence only when a live IRQ or
independent USB service can wake the stack. Unknown/unready providers retain
one-tick polling. Keep xHCI input cadence, USB transmit servicing, TCP timers,
DMA ownership and hardware gates unchanged.

Touched scope: network provider contract/worker, e1000 readiness, ECM readiness,
focused host fixtures, VM network/idle regressions and documentation. Require
empty-poll race, active traffic, missed notification recovery, unavailable IRQ/
service fallback, cable recovery and input/file coexistence coverage. Publish
measured worker activity before selecting a native unattended capture. No energy
saving or physical acceptance is inferred; no new reboot is armed.

Initial implementation `6555944` passes full local build/core checks and an
e1000 idle/input VM at 1.995 network switches/second. The ECM idle ceiling fails
at 32.715/s: diagnostic counters show repeated link/speed notifications, no
received packets. Refine ECM to signal only changed link state/faults from its
existing independent service; receive completions still signal immediately.
The failed observation is retained under `build/network-idle-20261023`.

### Storage completion wait milestone accepted for bounded workload

Release `f340e0695d1e2492`, compiled source `c258822`, returned automatically in
`session-00065e8ceccc2baa-00`. The one-minute desktop capture passed with six
complete periodic saves, a valid 1440×900 screenshot, no callback failures and
clean exit. All 1033 previous files retained their bytes except expected exFAT
metadata; only four logs and the screenshot were added. Offline checks pass at
1038 files / 2474 clusters, zero leaks, bitmap mismatches or errors. Ubuntu default,
consumed one-shot and previous recovery `9aa8dbf6fbda8252` are verified. USB is
unmounted and no further reboot is armed.

| Observed workload | Polling baseline | 20 ms default |
| --- | --- | --- |
| Desktop CPU0 idle timer samples | 95.00% | 98.30% |
| Periodic saves / bytes | 6 / 136678 | 6 / 136710 |
| Total save wall time | 3.005369 s | 1.883555 s |
| Save range | 148.681–2002.016 ms | 149.316–622.689 ms |
| Logger sampled ticks / worker interval | 282 / 52.819 s | 84 / 51.529 s |
| Compositor switches per second | 10.034 | 10.033 |

The refined policy admits idle during long storage waits without the previous
run's multi-second periodic saves. This closes the bounded wait milestone with
the earlier fixed-file native comparison and VM input-under-copy evidence.
Run-to-run media stalls remain; these single runs do not prove an optimal
threshold, universal throughput improvement, physical input latency or battery
life. Package power was 6.786 W over 51.999 seconds, versus baseline 6.949 W;
thermal conditions were not controlled, so no energy saving is accepted.
Battery was deliberately off in this idle recipe; the existing ordinary Home
battery acceptance remains separate.

Final review consumes main `54862de` (Air6,1 evidence-only PR121); kernel/tools
are identical to the tested source. Pro PR19 `c223f58` remains independent Wi-Fi
bring-up. PR118 contains the shared owner-checked wait, 20 ms policy, strict
counterbalanced diagnostic and local regressions. Full build/core and matched
VM receipts remain those recorded below; final documentation checks pass.
Raw evidence, payload/recovery and reviewed results are hash-verified outside
`build`, under the local `pending-trials` and `reviewed-evidence` release folders.
Next work can address remaining HID/network polling or controlled energy
measurement as separate milestones; no new scope or hardware run is armed here.

### Previous checkpoint: 20 ms normal desktop validation

Source `4896805` changes the default poll-before-wait threshold to 20 ms and
preserves historical 4 ms diagnostic collection. Main `bbdbefb` is integrated;
the staging conflict was resolved by retaining both the descriptive storage
comparison recipe and Air6,1 v68. Full local build/core/docs passes. Networked
post-idle drag, typing and exact save pass. Source `c258822` additionally lowers
the USB copy fixture write limit to 128 KiB/s: the previous 1 MiB/s run exercised
no waits at 20 ms and therefore failed its coverage assertion. The revised run
exercises 2,648 waits / 25.795 seconds of helper wall time, with zero transport
errors, concurrent typing, exact binary/document readback, collision protection,
rename and clean exit. The stalled USB quarantine/keyboard regression passes.

Exact EFI/GPT v26 `f340e0695d1e2492` saves six valid logger-cost records,
verifies persisted bytes and resets automatically. Missing-workspace return
also passes. The next physical run measures the normal one-minute desktop
logger workload with the 20 ms default; no input is required. All 1033 existing
USB files were hashed and offline allocation/fsck pass. Recovery remains
`9aa8dbf6fbda8252`. Run/reboot state and collector are under
`build/storage-wait20-20261023` with outside-build backups before arming.
PR118 stays draft until that normal-workload result is reviewed.

### Controlled native result and 20 ms candidate

Release `2218ef4113730414`, source `7347ccc`, returned automatically in
`session-00065e8ca277c1a9-00`. All 18 counterbalanced 32 KiB overwrite/readback/
flush operations passed, and the final fixture matched exactly after Ubuntu
return. All 1030 previous files were preserved except expected filesystem
metadata. Only boot/exit logs and the fixture were added; offline fsck and
allocation pass at 1033 files / 2429 clusters, zero leaks or mismatches.
Ubuntu default, consumed one-shot and previous `9aa8dbf6fbda8252` are verified.

| Policy | Median save | Range | Total, six saves | Idle timer samples |
| --- | --- | --- | --- | --- |
| Polling | 251.303 ms | 171.657–717.536 ms | 1.895296 s | 0 / 189 |
| Wait after 4 ms | 172.592 ms | 171.819–330.844 ms | 1.193865 s | 16 / 120 |
| Wait after 20 ms | 171.813 ms | 171.635–334.427 ms | 1.351771 s | 29 / 135 |

Each policy has fast operations around 172 ms and slower outliers. The first
polling sample was 718 ms despite an unmeasured allocation/warm-up write.
One boot and six samples per policy do not establish that one threshold is
optimal, or that the earlier logger regression was caused wholly by sleeping.
Different stall incidence also prevents ranking CPU efficiency from these totals.
Both wait policies admitted idle during some stalls, with zero callback failures.

Choose **20 ms as the next conservative candidate**, preserving polling for
5–15 ms completions that the host fixture showed could be delayed by the 4 ms
threshold. Normal desktop logger latency/idle must still be measured before
merging PR118; no energy or physical interactive acceptance is claimed.
Current installed is `2218ef4113730414`; USB is unmounted and no reboot is armed
at collection. Evidence under `build/storage-compare-20261023` is backed up
outside build. Follow-up scope: default threshold, test/collector compatibility,
docs and matched desktop validation. Consume main `bbdbefb` (Air6,1 PR120)
before staging. Pro PR19 `5e98a56` retains nonoverlapping Wi-Fi/control dispatch.

### Historical preparation: controlled storage wait comparison

Consume main `2702b41` (Air6,1 continuous Home capture) before the next build.
PR118 remains draft. Add an explicitly staged unattended diagnostic that writes
and verifies the same 32 KiB file in its new session directory, comparing polling,
4 ms and 20 ms wait thresholds in counterbalanced order. Pause periodic logging
while measuring; retain per-phase USB costs, CPU samples and exact readback.
Restore the original policy on every handled exit, persist results, and use the
existing automatic return path. Ordinary desktop startup does not run it.
This scope touches xHCI diagnostic policy controls, workspace logging/capture,
main capture dispatch, staging/collection tests and documentation. Pro PR19
`2a4079a` was inspected; its Wi-Fi/control-dispatch changes do not overlap storage
bulk waiting. No hardware wait-policy refinement is accepted in advance.

Controlled comparison implementation `7347ccc` consumes main `2702b41` and
passes full local build/core/docs, policy owner/pending/quarantine refusal,
controlled completion-delay matrix, logger corruption/flush-failure restoration,
and strict parser/offline-fixture tests. A 5 ms injected completion is detected
at 5.001 ms with polling or a 20 ms threshold, but at 10.006 ms with a 4 ms
threshold. This establishes possible tick quantization, not native media latency.
Fresh USB copy/input/persistence and stalled-transfer quarantine VM checks pass.
Exact EFI/GPT `2218ef4113730414` completes all 18 measurements, persists the
fixture exactly, unmounts and resets; missing-workspace automatic return passes.

Next native run is the dedicated `--storage-wait-capture` profile. No typing,
trackpad interaction or screenshots are needed; it saves a new fixture and logs
and returns automatically on handled completion/failure. All 1030 existing USB
files were hashed; offline fsck/allocation pass. Recovery remains
`9aa8dbf6fbda8252`; the old baseline `9c57ca38a3771eef` was hash-verified into
archived EFI storage before removing its unreferenced EFI copy for space.
Run/reboot state: `build/storage-compare-20261023/checkpoint.json`, also backed up
outside build before arming. Default 4 ms policy remains unchanged pending these
measurements; PR118 remains draft.

### Original scope: bounded storage completion waits

Branch `device/macbookair7-2/storage-wait` starts at main `4a991df` (PR116
Air6,1 acceptance docs). Pro PR19 `3ea205a` was inspected: it adds Wi-Fi/control
dispatch before the existing storage branch, but does not change storage bulk
submission, completion or scheduler I/O helpers. This Air branch owns the narrow
storage wait change: a per-device completion queue, wake after a matching bulk
completion, and an owner/address-space-checked scheduler wait of at most one tick
after four milliseconds of polling. Preserve the fast path, command deadline,
clock-stall fallback, DMA quarantine and input service. No Wi-Fi/control path
edits; this published scope is the shared-file coordination notice.

Require race/ownership/refusal and delayed-completion/timeout/HID host fixtures,
full local checks, exact EFI/GPT capture/recovery, and input during slow-copy VM
coverage before a matched native logger-cost capture. Compare cost, idle samples
and throughput; do not infer power savings. Recovery stays `9aa8dbf6fbda8252`;
the pretrial installed baseline was `9c57ca38a3771eef`.

Implementation `3687a98` is in draft PR118. Full local build/core/docs and
scheduler/xHCI host tests pass. Networked post-idle drag/type/save passes. The
new USB mode of the throttled large-copy VM preserves a 4 MiB+17 byte binary,
typed document, collision target and renamed file; input overlaps the copy.
It exercises 272 accepted waits / 1,896,206 us helper wall time with zero USB
errors. The stalled USB VM retains keyboard progress and bounded quarantine.
Exact EFI/GPT v26 `58a61b892c43d2f5` persists six valid logger-cost records,
verifies offline file bytes, unmounts and resets. Missing workspace also returns
without starting the probe. VM observations do not establish native efficiency.

Native preparation: all 1025 existing USB files hashed; offline fsck and allocation
pass; actual Air7,2 inventory captured. Recovery remains `9aa8dbf6fbda8252`.
The next unattended one-minute capture compares logger costs, idle/worker samples
and package energy with accepted `9c57ca38a3771eef`; no input is required.
Run/reboot state is recorded in `build/storage-wait-20261023/checkpoint.json`
and copied outside build before arming. Pro PR19 `a2db9de` and Air6,1 PR117
`503dd05` were rechecked; neither changes the storage bulk wait or scheduler
helper. No native acceptance or energy saving is claimed before collection.

### Native storage wait result: functional pass, performance pending

Release `58a61b892c43d2f5`, source `3687a98`, returned automatically in
`session-00065e8c5b4cfd37-00`. The 60.009-second desktop capture completed,
saved its screenshot and logs, exited cleanly and consumed the one-shot boot.
All 1025 previous files were preserved except expected filesystem metadata;
only four logs and one screenshot were added. Offline fsck and independent
allocation pass at 1030 files / 2425 clusters, zero leaks or mismatches.
The saved 1440x900 desktop screenshot was reviewed. Ubuntu default, EFI order
and previous recovery `9aa8dbf6fbda8252` remain intact. USB is unmounted;
current is `58a61b892c43d2f5`, and no reboot is armed.

CPU0 idle samples rose from 95.00% to 98.77%. Logger samples fell from
282 over 52.819 seconds to 56 over 45.699 seconds (5.34% to 1.23% of
those separate worker windows). The candidate logger recorded 518 timer wakes,
consistent with exercising timed waits. Compositor activity stayed at 10.000
switches/second, with one frame in the idle window.

However, five completed saves (106,942 bytes) cost **5.889351 seconds**, compared
with six baseline saves (136,678 bytes) costing **3.005369 seconds**. Candidate
save durations were 2.049146, 2.505198, 0.992459, 0.171650 and 0.170898 seconds.
USB status callbacks consumed 5.374180 seconds across 572 calls, with zero
callback failures. This is not a single-transfer timeout. A one-tick polling
wait can delay noticing completion; the current evidence does not separate
that added latency from flash/media variability. It does not qualify a throughput
improvement. Keep PR118 **draft** pending a controlled latency comparison and
wait-policy refinement; functional success is not performance acceptance.

Package power observed 6.765 W over 45.999 seconds versus 6.949 W over
52.999 seconds previously. Different coverage, temperatures and a single pair
prevent a power-saving claim. No physical interactive input or ordinary battery
service acceptance is added. Evidence and comparison are in
`build/storage-wait-20261023`, backed up under this release outside build.
Main advanced to `2702b41` (Air6,1 continuous Home capture PR117); inspected but
not yet consumed by this candidate. Pro PR19 remains `a2db9de`.

### Active checkpoint: native logger cost captured

Branch `device/macbookair7-2/logger-timing` starts at main `180b542` (PR112).
Measure logger preparation/write/verification/sync wall time and scoped USB BOT
callback time by CBW/data/CSW phase. Keep cadence, persistence, device polling and
scheduler policy unchanged. Timing includes yielded service time and is not CPU
runtime or pure device latency. Emit each completed record into the next saved
snapshot, so measurement does not require an extra flush. Collector summaries
must select only completed operations wholly inside the idle window.

Touched subsystems: workspace logger, generic USB-storage timing hooks, collector
and host tests; no xHCI edits. Pro PR19 `603b7a6` owns USB Wi-Fi/xHCI work and has
no overlapping generic storage/logger edits. Air6,1 PR114 `f3e0763` changes its
battery/Text generated trial and has no overlap. Require host phase/failure tests,
full local build/core/docs and exact-stage saved/missing-workspace VM checks
before a new native capture. PR115 implementation `1f93899` passes the full local
build/core/docs and directed host cases (failure, nested refusal, clock regression,
overflow, phase attribution and collector rejection). Networked post-idle drag,
typing and exact save pass. Exact EFI/GPT v26 `9c57ca38a3771eef` saves six valid
in-window periodic cost records, verifies persisted bytes, exits cleanly and
resets automatically. These VM timings are not native performance evidence.

Native release `9c57ca38a3771eef`, source `1f93899`, returned automatically in
`session-00065e8bb03c4641-00`. Six valid completed periodic saves fit wholly
inside the 60.009-second idle window. They saved 136,678 snapshot bytes and cost
**3.005369 seconds** total: preparation 325 us, write 2,985,075 us, verification
14,184 us and sync 5,785 us. Generic USB callback wall times were CBW 4,554 us,
data 546,660 us and CSW **2,437,047 us**, with 691 commands/status callbacks,
2,200 data callbacks and zero callback failures. The callback sums cover 99.43%
of recorded save wall time; status callbacks alone cover 81.09%.

One 18,538-byte save lasted **2.002016 seconds**, including 1.938771 seconds
across its 108 status callbacks. The other saves lasted 0.149–0.343 seconds.
This locates the dominant observed wall time inside USB completion callbacks,
not text formatting or final sync. It does not identify one longest transfer,
a firmware/media cause or pure device delay: callback duration includes copies,
event dispatch and yielded tasks. The two-second save spans many commands and
is not evidence of a single transfer timeout. No transport/device errors occurred.

CPU0 idle samples were 95.00% with 19,401 switches and one frame. Stable worker
coverage was 52.819 seconds: compositor 530 switches (10.034/second), logger
282 timer samples / 287 switches. Package energy covered 52.999 seconds:
368.323 J, **6.949 W**, active CPU0 2,080 MHz; package temperature fell 76–73 C.
Previous uninstrumented compositor capture was 92.88% idle and 6.946 W.
Neither run is thermally settled, and probes add overhead: no power saving or
causal explanation of the earlier run-to-run difference is established.

All 1020 prior files were preserved except expected filesystem metadata. Only
four logs and one screenshot were added; offline fsck and independent allocation
pass at 1025 files / 2380 clusters, no leaks or mismatches. The 1440x900 screenshot
was reviewed. Completed capture, clean exit, consumed one-shot, Ubuntu default
and recovery pass. USB is unmounted and no reboot is armed. Installed current
is `9c57ca38a3771eef`, previous recovery `9aa8dbf6fbda8252`. Evidence under
`build/logger-timing-20261023` is backed up outside build in this release's
pending-trials and reviewed-evidence directories. Physical post-idle interaction
and ordinary battery-service efficiency remain outside this unattended recipe.

Main `adf40df` (PR114 generated battery Start/save sequencing) is consumed as
`eaa35e5`; Pro PR19 `5e9d88a` still owns xHCI/Wi-Fi. Integrated source `ca67790`
passes full local build/core/docs and fresh networked post-idle drag/typing/save.
Its changed userspace produces v26 `7af8afd122619ce0`; exact EFI/GPT VM capture
persists six valid timing records, verifies file bytes and returns automatically.
Missing-workspace automatic recovery also passes.
That integrated payload has VM evidence only and is not installed; native
acceptance above remains bound to `9c57ca38a3771eef`.

Next: bound and test how the storage owner waits for completion, or measure
individual callback maxima before
changing that behavior. Preserve command deadlines, DMA ownership/quarantine,
input service, verification and flushing. Coordinate xHCI overlap through the
published scope before editing it; do not silently mix Wi-Fi bring-up with this
storage investigation.

### Active checkpoint: compositor polling reduction verified

Branch `device/macbookair7-2/compositor-idle` starts from main `ce1c0a6`
(PR110 Air6,1 acceptance documentation), and now consumes `89b19ec`
(PR111 bounded Air6,1 Home session and PR113 failed-run evidence).
Pro PR19 `8294ee1` retains USB Wi-Fi;
its diff has no overlapping GUI, syscall, keyboard queue or mouse queue changes.
This bounded follow-up adds a server-only combined input/message wait and opts
in only the otherwise idle ordinary compositor. Keyboard, pointer and client
messages must wake it immediately; unfinished frame/input/control publication,
clipboard/preferences/capture work and timed diagnostic workloads retain the
existing cadence. A 100 ms maintenance cap bounds unrelated housekeeping.

PR112 implementation `92edfcb` passes the full local build/core suite and directed
wait/ownership/race/publication tests. Networked VM post-idle pointer drag and
USB typing/exact save pass; contextual menus, clipboard selection/isolation and
file actions pass. The 4 MiB+17-byte guarded-copy VM also passes input overlap,
exact text/binary readback, collision preservation and rename. Exact EFI/GPT v26
`dc06bc673689e011` passes saved capture, screenshot, automatic reset and missing
workspace recovery. Six worker checkpoints show 499 compositor switches over
50.170 seconds (9.946/second), with no partial lifetimes. VM power is unavailable.

Native release `dc06bc673689e011`, compiled `92edfcb`, returned automatically
in session `session-00065e8b5e2897aa-00`. Completed capture, clean desktop exit,
1440x900 screenshot, USB preservation and recovery pass. All 1015 prior files
survived except expected filesystem metadata; only four logs and one screenshot
were added. Offline fsck and independent allocation pass at 1020 files / 2335
clusters with no leaks or mismatches. No storage/device error was recorded.

The compositor fell from 100.00 to **10.015 switches/second** (540 switches over
53.920 seconds); Text remained at 10.015/second and xHCI input at 100/second.
The 60.010-second window recorded 19,278 total switches versus the prior 25,102,
with one frame. **Overall CPU0 idle samples worsened from 97.85% to 92.88%.**
The logger accounted for 392 timer samples in the worker interval, versus 96
in the prior 50.960-second interval. These are samples, not measured CPU runtime.

Six energy checkpoints covered 54.000 seconds: 375.135 J, **6.946 W package
average** versus 6.802 W previously, and 2,086 MHz CPU0 active frequency.
Temperature fell 76–73 C versus 75–72 C previously; neither sample was thermally
settled. AC/14% brightness/Ethernet/workspace/audio settings and the v26 recipe
matched; the ordinary battery owner remains disabled. This accepts reduced
compositor polling and clean persistence only. It does **not** establish lower
overall CPU activity, energy savings, ordinary Home efficiency or physical
post-idle input latency. Post-idle drag/typing/save are VM evidence only.

Inspection shows periodic logging writes, reads back and flushes each snapshot.
USB bulk completion still polls while yielding service opportunities at 1 ms;
yielding does not block the storage owner. That path can accumulate logger timer
samples, but these counters do not distinguish transfer wait from filesystem
work or explain the difference between runs. Keep that attribution question
open; measure checkpoint duration/transfer wait before changing polling or
claiming a power improvement. Do not weaken persistence to improve a benchmark.

Touched subsystems: GUI transport/syscall, input queue wake notifications,
compositor idle admission and directed host/VM fixtures. No xHCI/network polling,
CPU/firmware policy or hardware profile changes. Evidence is under
`build/compositor-idle-20261023`, with payload and returned evidence preserved
outside build in this release's pending-trials/reviewed-evidence directories.
Installed current is `dc06bc673689e011`, previous recovery `9aa8dbf6fbda8252`;
Ubuntu remains default, USB is unmounted and no reboot is armed. Actions remain
disabled. Integrated source `beba433` passes full local build/core/docs and a
fresh networked post-idle pointer/typing/exact-save VM regression. Re-staging v26
produces the same release: kernel, initramfs and EFI loader hashes match the
native-tested payload exactly, so the existing exact-stage VM receipts remain
applicable. Shared Air6,1 diagnostic changes do not confer sibling acceptance.
Next power work should measure the logging/storage cost and repeat matched
captures; dynamic window capacity follows the bounded idle work.

### Historical checkpoint: first idle Text optimization accepted

Branch `device/macbookair7-2/power-idle`, PR106 adds measured worker diagnostics
and an interruptible GUI receive-readiness wait. Only quiescent Text opts in,
with a 100 ms maximum maintenance deadline; input/close messages wake immediately.
File/clipboard work, unfinished frame/menu/title publication and diagnostic
workloads keep their existing cadence. Compositor, xHCI, network polling and CPU
policy are unchanged. The shared client helper is available for future callers
that explicitly exclude their own timers/jobs.

Native release `7afccbd6fbc3cfd4`, compiled `d6f3136`, session
`session-00065e8b0600c122-00` returned automatically with complete capture evidence
and no attention items. The 59.999-second Text idle window recorded 97.85% CPU0
idle samples, 25,102 switches and one frame. Its stable Text worker fell from
100.00 to 10.01 switches/second (510 switches over 50.960 seconds), about 90% less
activity. Total window switches fell 17.66% versus the baseline below. Other
input/compositor polling stayed at 100 Hz. These are software transitions, not
hardware wakeups or measured thread runtime.

Six energy checkpoints covered 51.000 seconds: 346.927 J, 6.802 W package average
and 2,054 MHz CPU0 active frequency. Baseline was 6.695 W; **no energy saving is
established**. Candidate package temperature fell 75–72 C, versus 74–71 C in the
baseline, so neither was at thermal equilibrium. AC was connected before boot,
brightness stayed at 14%, Ethernet/workspace remained attached and audio idle.
The v26 diagnostic omits the ordinary battery owner. Physical post-idle typing
was not requested or exercised; native input latency remains outside this capture.

All 1010 original files were preserved except expected filesystem metadata;
only four logs and one screenshot were added. Offline fsck and independent
allocation pass at 1015 files / 2290 clusters, zero leaks/mismatches. The 1440x900
screenshot was reviewed. Ubuntu recovery/default and consumed one-shot pass;
installed current is `7afccbd6fbc3cfd4`, previous `9aa8dbf6fbda8252`. USB is unmounted
and no reboot is armed. Evidence and payloads survive build cleanup in local
pending-trials and reviewed-evidence directories for this release.

Implementation passes full local build/core/docs and queue/signal/race/teardown/
publication fixtures. Exact EFI/GPT v26 VM capture, screenshot, persistence,
automatic return and missing-workspace recovery pass. Ordinary networked VM
post-idle typing and exact save pass. VM client switches fell 98.31 to 9.96 per
second; unavailable VM energy remains null. After native collection, main
`de449b3` (Air6,1 PR109 mounted Home diagnostic) was consumed as `1f7c5f2` without
conflict. Pro PR19 `af42f25` retains USB Wi-Fi scope. Actions remain disabled;
integrated full build/core/docs and networked post-idle input/save passed. Its v26
payload is byte-identical to the native release; exact EFI/GPT completion and
missing-workspace return were revalidated. No sibling or new battery acceptance
is implied.

The bounded first optimization is complete. Next power work should target the
remaining measured compositor/device wake sources with input/IO latency evidence;
repeat matched, thermally controlled captures before claiming energy savings.
Dynamic desktop/window capacity remains a separate planned milestone.

### Native worker/power baseline accepted

Release `dc7c0eaea0dca69b`, compiled `7e0d8b9`, session
`session-00065e8ac591746a-00` returned automatically with complete logs, no
collector attention and a verified 1440x900 screenshot. The 59.999-second Text
idle window recorded 97.60% CPU0 idle samples, 30,487 switches and one frame.
Six energy checkpoints covered 50.999 seconds: 341.451 J package energy,
6.695 W average package power and 2,064 MHz CPU0 active frequency. Six worker
snapshots covered 51.269 seconds with no partial lifetimes. Bootstrap (the
foreground compositor), xHCI input and one userspace client each woke about
100 times/second; network polling had 95.65 deadline and 1.97 signal wakes/second.
These transitions are not hardware wakeups or per-thread energy attribution.

AC was connected before boot; native brightness stayed at 14%, audio was idle,
the Ethernet adapter and USB workspace stayed attached, and the battery owner
was disabled by this diagnostic recipe. Package temperature fell from 74 to
71 C inside the covered interval: this is a short post-boot baseline, not thermal
equilibrium or a comparison with October 19. No energy saving is claimed.

All 1005 original files were preserved except expected filesystem metadata;
only four logs and one screenshot were added. Offline allocation is clean at
1010 files / 2245 clusters, no leaks/mismatches; fsck and Ubuntu recovery pass.
Current is `dc7c0eaea0dca69b`, previous remains `9aa8dbf6fbda8252`; the one-shot
is consumed, USB unmounted and no reboot armed. One obsolete unreferenced EFI
payload was hash-archived before removal to make installation space. Raw evidence
and payloads survive cleanup in the local pending-trials directory for this release.

### Previous checkpoint: filesystem scale milestone complete

[PR102](https://github.com/PaulMDemers/SrvrOS/pull/102) replaces the old static
filesystem tables with stable demand allocation, direct VFS enumeration,
pre-write node/metadata reservation and reusable deleted slots. Budgets are
8192 files/2048 directories per volume and 32768 VFS nodes overall. Paths and
disk formats are unchanged. This is bounded growth, not arbitrary filesystem
size or power-loss atomicity; see the [capacity contract](../filesystem-capacity.md).

Native release `eba663ccd2a40909`, source `768f6b2`, request `931553fd2910f285`
returned with a complete CRC-protected PASS report. A disposable 256-file /
64-directory fixture brought the starting USB to 1236 files and 270 directories,
above both former limits. Write/flush/read, file operations and shell/editor
save/remount/reopen passed. All existing files and directories were preserved;
25 expected test files were added, with only the request marker and filesystem
metadata modified. Before cleanup, offline checks passed at 1261 files / 275
directories, 2265 allocated clusters, no leaks or FAT/bitmap mismatches.

Only the verified temporary fixture was removed. A final read-only comparison
confirmed every remaining file and all original directories. Post-cleanup fsck
passed at 1005 files / 210 directories; allocation passed at 2200 clusters with
no leaks or mismatches. The first Ubuntu cleanup attempt stopped because udisks
removed its old mount directory; it made no deletions. Recreating the mountpoint
and repeating collection completed safely. This was a host cleanup issue, not
an OS test failure. Evidence survives build cleanup under
`~/.local/share/srvros/reviewed-evidence/eba663ccd2a40909` and
`~/.local/share/srvros/pending-trials/eba663ccd2a40909`.

Host tests cover 6000 VFS nodes, 5000 imported files plus 96 growth/reuse files
and 320 directories, stable references/identities, allocation-failure rollback,
both exhausted budgets and write/flush/power-cut/partial-write recovery. Original
exact v8 EFI and desktop EFI/GPT positive/refusal checks passed. After consuming
main `d7ed773` (Air6,1 PR101/103), integrated source `d32e02e` passed full
build/core/docs, the scale fixture, exact v8 EFI `432e416b0d7b0dd2` with 5000 preserved
files and desktop `8855701b88562f2a` positive/audio/save/reopen and 8191-file refusal.
Those integrated payloads are VM-validated, not newly physically accepted.
The subsequent PR104 baseline `f2e0945` was consumed as `d892afa`; full build,
core and docs checks passed again. Its ordinary v8/v58 payloads are byte-identical
to those already VM-tested, so no additional boot or hardware acceptance is claimed.

Installed current remains `eba663ccd2a40909` (automated request consumed);
accepted previous remains `9aa8dbf6fbda8252`. Ubuntu default and consumed one-shot
are verified, USB is unmounted, and no reboot is armed. The milestone accepts
bounded native persistence, not new physical gesture, battery, power or sibling
hardware behavior. Pro PR19 remains Wi-Fi; Air6,1 PR104's merged bounded
battery/file coexistence work retains that machine's separate acceptance. No additional physical test is needed
for this filesystem closeout.

### Previous checkpoint: bounded capacity recovery milestone complete

Native release `a39bb31d4031693a` (source `2c68476`), request
`cda6574dd2dedfa8`, returned with a valid complete CRC-protected PASS report.
Write/flush/read, file operations and shell/editor save/remount/reopen pass;
all expected file trees verify independently in Ubuntu. There are 25 new test
files, no missing files and no unexpected modifications: only the request and
filesystem metadata changed. Offline fsck passes (979 files, 205 directories),
and independent traversal finds 2165 allocated clusters, no leaks or mismatches.
This accepts one automated persistence round, not physical gestures or battery.

Ubuntu default and consumed one-shot pass. Installed current remains
`a39bb31d4031693a`; accepted previous remains `9aa8dbf6fbda8252`. USB is unmounted,
no reboot is armed, and returned evidence is preserved outside build at
`~/.local/share/srvros/pending-trials/a39bb31d4031693a/returned`.

PR88 tap work is merged; the user's subjective tap feel is accepted, while its
missing full native receipt remains incomplete. PR99 closes the independent
logging-capacity follow-up. Actual main `5d195a9` adds Air6,1 PR98's separately
gated SMBus capture; integration preserves both Makefile targets and v62 profile.
Pro PR19 `1b98c71` remains separate Wi-Fi work. No sibling acceptance is inferred.
Final integrated source `1e78461` passed full build/core/docs and exact EFI/GPT
capacity refusal plus desktop/audio/save/reopen. The ordinary payload is byte-identical
to already rehearsed `c4f6502fdd232e6c`; no integration payload was installed.
Receipts survive build cleanup at
`~/.local/share/srvros/validation/log-capacity-integration-1e78461`. PR99 carries
the completed change for the shared baseline. No further test is required for
this bounded milestone. Larger filesystem limits and archival of other kinds
of evidence remain separate work. The subsequent main `61aa054` update contains
only Air6,1 acceptance documentation (PR100); it is consumed without changing
the validated code or installed payload.

Verified archival previously moved 22 completed old sessions (70 files) to
`~/.local/share/srvros/log-archives/20261023-capacity-recovery` before pruning
only their verified USB copies. All other 953 files were unchanged. Newest 32,
incomplete/unknown sessions and user documents were preserved. After this test,
44 file slots remain after metadata reserve: enough for the 32-file logger
admission, but the conservative 64-file automated-test preflight will refuse
another round until additional space is deliberately recovered. No further
cleanup or trial is scheduled as part of this closeout.

### Capacity implementation and pre-boot validation (PR #99)

Source `2c68476` adds independent file/directory admission before session creation,
visible attended-trial refusal, mounted preparation checks and an offline real
exFAT/VFS headroom check. Explicit Ubuntu archival keeps the newest 32 sessions
and incomplete/unknown contents; at most 32 completed old sessions are copied,
hashed and flushed outside the checkout/on another device before optional prune.
No boot-time deletion or filesystem limit increase. The setup guide documents it.

Full build/core/docs, nine archive safety cases, logger refusal, host preparation
and exhaustive filesystem write/flush/power-cut regressions pass. Desktop release
`c4f6502fdd232e6c` passed exact EFI/GPT full-capacity refusal without session
creation and normal desktop/audio/save/reopen. Offline real USB checks after
archival pass: 953 files, 200 directories, 2130 allocated clusters, no leaks or
mismatches, 70 usable file slots. All remaining files were hash-preserved.

One unattended native v8 persistence round is prepared as release
`a39bb31d4031693a`, request `cda6574dd2dedfa8`; its exact automatic-return VM
rehearsal passed persisted CRC/file-tree verification, default return, replay
halt and read-only failure halt. The candidate is installed with the accepted
previous payload retained; local run records own the arming state. It exercises USB write/flush/read,
file operations and shell save/remount/reopen, with no human input, automatic
Ubuntu return on success and visible failure halt. Native acceptance is pending;
this does not retest physical gestures or battery. Accepted recovery remains
`9aa8dbf6fbda8252`. Local run state/evidence lives under
`build/log-capacity-20261023`; verified receipts/payloads are preserved outside
build in `~/.local/share/srvros/validation/log-capacity-a39bb31d4031693a` and
`~/.local/share/srvros/pending-trials/a39bb31d4031693a`. The pre-install real USB
has 954 files including the request; offline fsck/allocation/headroom pass.
Collect this exact request on Ubuntu return before another test.

Actual main remains `2e2eb2b`. Pro PR19 `37fa564` remains Wi-Fi work; Air6,1
PR98 `7a1b014` is a distinct bounded SMBus capture. Both touch some shared guide
and harness files; preserve their separate profiles when integrating. This work
adds no device/firmware gates. Actions stay disabled; all validation is local.

### Earlier tap acceptance and capacity diagnosis

Returned settling trial `95db354d397095b4` (source `c619cdd`): the tester says
"it's dialed in well enough now". Accept the subjective tap feel and stop further
threshold tuning. This is not a claim that all native regression checks passed.
The USB has only an empty new `session-00065e891a178922-00` directory and no
saved report, counter summary, screenshot or exit snapshot. Collection cannot
match a saved release; full trial evidence remains **incomplete**. The return
route was queried; it is not inferred from the Ubuntu boot alone.

A separate capacity issue explains the missing files: the driver has 1024 file
slots, and creation reserves a temporary metadata slot. The real USB contains
1023 files. A disposable host-image run of the real driver reproduced refusal
after 1022 ordinary files plus the metadata file, then successful delete/recreate
and clean remount/fsck. Directory creation can still succeed at this boundary.
Do not treat clean fsck as evidence that new logs can be saved. Native error text
was not captured. Before another hardware run, address file-table headroom and
archival/retention of old logs; preserve all evidence outside build before any
USB cleanup. No storage code or USB file deletion was performed in this review.

Prior files are unchanged except expected metadata; offline fsck/allocation pass
(1023 files, 2222 clusters, no leaks/mismatches). Ubuntu default, consumed one-shot
and current/recovery payloads pass. Current is `95db354d397095b4`, previous remains
`9aa8dbf6fbda8252`; USB is unmounted and no reboot is armed. Raw collection is in
`~/.local/share/srvros/pending-trials/95db354d397095b4/returned`; scoped review and
capacity reproduction survive cleanup in
`~/.local/share/srvros/reviewed-evidence/95db354d397095b4`.
Fetched main `9488193` includes separate Air6,1 battery work and was inspected;
Pro PR19 `4f991e5` owns its raw MAC read-only Wi-Fi trial. Actual tap candidate
base remains `f310923`; no sibling changes or firmware gates were imported.

### Tap diagnostics and preparation history

Returned diagnostic `efc90fce7ae6849d`, session `session-00065e8898366d73-00`:
tester saw 8–9 menus from roughly 20 attempts; exit counters show 9 secondary taps
and 14 movement rejections whose peak reached two fingers. There are no two-finger
timing rejections. The whole-session 55 movement rejections and maximum 352 raw
units include ordinary pointing and cannot be assigned to those 20 attempts.
The answer contains a leading Escape byte but says "8-9 appeared"; started/completed
and a clean exit are saved. No screenshot. This is not physical tap acceptance.
Prior files are unchanged apart from metadata; offline fsck/allocation pass
(1023 files, 2220 clusters, no leaks/mismatches), recovery passes and one-shot is
consumed. USB is unmounted. Review is retained outside build at
`~/.local/share/srvros/reviewed-evidence/efc90fce7ae6849d`.

Candidate `95db354d397095b4` / v58, source `c619cdd`, implements the bounded
follow-up: increase only secondary-tap displacement allowance from 64 to
128 native Manhattan units (4 to 8 unaccelerated base pixels), retaining all time
limits and primary-tap limits. Boundary/contact-order/lift/scroll arbitration tests pass.
The corrected checklist dismisses only visible menus so failed attempts do not
send Escape into Terminal's read; use fresh Home/tap-diagnostics-v2 reports.
Touched subsystems: pointer policy/tests, trial script/Help/VM checks, current docs.
Main `90a1db8` was fetched/inspected: separate Air6,1 battery runtime work; this
focused change retains actual base `f310923`. Pro PR19 `5735a13` records separate
validated full-table Wi-Fi candidate; no sibling driver/firmware gates are copied.

Full build/core/docs, pointer/mouse/ABI, xHCI, eight host checklist tests, actual
Terminal guest script execution and exact EFI/audio/GPT save/reopen all pass.
The corrected Help page was visually reviewed. Source is committed and shared;
PR88 stays draft pending native taps and unwanted-menu/scroll acceptance.
Validation/payload evidence survives cleanup at
`~/.local/share/srvros/validation/tap-settling-trial-95db354d397095b4`.
The user authorized the attended trial. Candidate `95db354d397095b4` is now
installed, retaining accepted `9aa8dbf6fbda8252` recovery and Ubuntu default.
Real USB preflight passed (1023 files, 2220 clusters, no leaks/mismatches); earlier
reports are hashed and preserved, and the fresh Home/tap-diagnostics-v2 is absent.
Run, payload, recovery checks and return collector are backed up outside build in
`~/.local/share/srvros/pending-trials/95db354d397095b4`. The local run record owns
arming state. Human actions were announced: 20 taps, Escape only for visible
menus, record count, clean desktop exit/unmount and monitor reboot. No timed exit.
Collect the matching session before another trial. Native acceptance is pending.
Fetched main `b17b80e` adds Air6,1 incomplete runtime evidence, and Pro PR19
`09f2c0d` records a separate Wi-Fi prerequisite audit; both were inspected. The
exact validated candidate retains its prior base; no source changes or sibling
profile assumptions were introduced while preparing this install.

### Earlier diagnostic preparation (returned above)

Installed diagnostic `efc90fce7ae6849d` / v58 (source `106da96`) for the
authorized attended round; accepted recovery `9aa8dbf6fbda8252` remains previous.
Full build/core/docs, eight host checklist checks, actual Terminal guest execution
and exact EFI/audio/GPT save/reopen checks pass. The real USB passed offline
fsck/allocation (1017 files, 2212 clusters, no leaks or mismatches); prior files
are hashed. Ubuntu default and firmware order are unchanged. Durable run/payload/
collector: `~/.local/share/srvros/pending-trials/efc90fce7ae6849d`; validation:
`~/.local/share/srvros/validation/tap-diagnostic-trial-efc90fce7ae6849d`.
The local run record owns arming state. Collect this matching session first after
return; physical tap usability and rejection interpretation remain pending.

`tap-diagnostics.sh` creates
fresh Home/tap-diagnostics reports and asks for 20 natural two-finger taps,
Escape between attempts and keyboard-only return to Terminal. New Help-first
instructions, image dependency/packaging and host/guest script checks accompany
the already tested rejection counters. Prior gesture/audio/network reports stay
intact; no additional threshold, DHCP or device-profile changes are part of this
trial. Base remains `f310923`; fetched main `d5491c0` adds the distinct Air6,1
passive battery preflight and will be consumed separately after this capture.
Pro PR19 `21c3117` owns an installed, unaccepted TXBE9/TXBE10 ordered-pair trial;
its xHCI changes are not copied into this pointer diagnostics candidate.

Returned `8e70766e16933891`, session `session-00065e883cb81885-00`: tap timing
remains **unaccepted**. The saved response says "It triggers about 1 in 3 attempts";
other input regressions say "All good". Do not merge PR88 as a completed fix.
Next bounded work adds aggregate tap-rejection reasons/age/displacement diagnostics
in the pointer policy and xHCI summary, with host replay coverage; no further
threshold change until native evidence distinguishes the cause. The diagnostics
pass pointer/mouse/ABI, xHCI and full build/core/docs checks, including exact
reason counts, secondary classification, first-rejection retention and reset.
The diagnostic candidate above supersedes this earlier installed-state checkpoint.
Main `f310923` adds Air6,1 telemetry acceptance docs and is merged.
Pro PR19 `c408949` retains failed clock-enabled TXBE write evidence and a separate
next TXBE9/TXBE10 prerequisite audit; no Wi-Fi implementation is imported.

System logs record cable down/up and matching live Network refreshes twice. The
user reports cable changes display properly. Both screenshots show connected,
and all three script snapshots were taken after reconnection; the network-up
observation is blank. Retain that screenshot limitation. Missing router/DNS and
failed ping came with `dhcp=no`, default 10.0.2.15: this focused script never
requested DHCP. This does not reverse the prior successful explicit DHCP/DNS/HTTP
round or establish a new connectivity failure. The follow-up should acquire a
lease explicitly before testing connectivity.

The matching session exited cleanly with no collector attention. All prior files
are unchanged except expected `.srvros/meta`; offline checks pass: 1017 files,
2212 clusters, no leaks/mismatches. Ubuntu default, consumed one-shot and both
installed/recovery payloads pass. Current stays `8e70766e16933891`, previous
`9aa8dbf6fbda8252`; USB is unmounted. Raw results and screenshots survive cleanup
under `~/.local/share/srvros/pending-trials/8e70766e16933891/returned`.

Candidate `8e70766e16933891` / v58 (source `03bb41d`) is installed for the
authorized attended timing/live-link trial, retaining `9aa8dbf6fbda8252` recovery.
Final full build/core/docs, guest script/paste and exact EFI/audio/GPT save/reopen
checks pass. Help visibly shows `sh /fat/share/examples/tap-checks.sh` first.
The real USB passed offline fsck/allocation: 1006 files, 2121 clusters, no leaks
or mismatches; prior reports were hashed and remain in place. Ubuntu default and
firmware order are unchanged. Human gestures/cable changes are required; prompts
have no timeout and the user returns through clean desktop exit and monitor reboot.
Run/payload/collector backups survive cleanup in
`~/.local/share/srvros/pending-trials/8e70766e16933891`; validation is retained in
`~/.local/share/srvros/validation/tap-timing-trial-8e70766e16933891`.
The local run record owns the exact arming state. Collect its matching session,
new tap-checks observations/interface reports and down/up screenshots before
accepting the revised timing or System link display. No new physical pass yet.

PR88's next attended trial adds `tap-checks.sh`, image packaging and Help-first
instructions, with host and guest script regression coverage. It preserves prior
report directories and separately records tap feel, unintended menus/dragging,
and System link-down/up observations plus interface snapshots. User gestures and
Ethernet cable changes are required; USB stays attached. Preparation is authorized;
announce before arming the one-shot. Keep accepted `9aa8dbf6fbda8252` recovery.
Main `36dbc3d` (Air6,1 telemetry/v59) was inspected and merged; its exact sibling
profile does not widen Air7,2 firmware access. Pro PR19 `fea9f67` owns the separate
clock-enabled TXBE write comparison, installed but not yet natively accepted.
No overlapping pointer policy change or Wi-Fi code is imported.

Trackpad-only release `4414f888007a8407` (compiled `b1486c6`, staged `d510c39`)
returned and was collected from session `session-00065e87d79869ca-00`.
Saved observations explicitly confirm physical secondary click and secondary tap;
the user reports that tap requires nearly simultaneous finger contact. The prompted
primary drag/steering, post-scroll suppression and Sketch Escape checks are recorded
as "All worked properly". The screenshot shows those observations and the separate
final prompt, and `sequence.txt` contains started/completed. Accept these scoped
gesture checks while retaining the tap timing usability issue. Hardware transport
fault cancellation remains synthetic evidence, not accepted by Sketch Escape.

The matching session has clean desktop retirement, an exit snapshot and no collector
attention. All earlier files, including audio/network reports, are unchanged; only
expected `.srvros/meta` changed. Offline fsck and allocation pass: 1006 files,
2121 clusters, zero leaks/mismatches/errors. Ubuntu default, consumed one-shot and
installed/recovery hashes pass. Current remains `4414f888007a8407`; previous
`9aa8dbf6fbda8252` retains the accepted battery desktop. USB is unmounted and no
reboot is armed. Raw collection, observations and screenshot survive build cleanup
under `~/.local/share/srvros/pending-trials/4414f888007a8407/returned`; scoped
acceptance is in `~/.local/share/srvros/accepted-evidence/4414f888007a8407`.

PR83 merged as `cba50a8`, closing the guided-checklist/paste correction with the existing local build,
core, terminal guest and exact EFI/audio/GPT validation. Main `df44fa0` was fetched,
inspected and merged: Air6,1 headphone/Stop acceptance documentation only. Pro PR19
`e66c665` owns an opt-in PBF/MAC clock prerequisite candidate; native comparison
is pending and earlier TXBE write failures remain failures. No Wi-Fi code is copied.
The next bounded change touches only trackpad tap policy, host regressions and
current documentation: 150 ms arrival/release allowances and a 300 ms total
secondary-tap limit; one-finger taps retain 200 ms. Physical click ownership,
displacement/scroll suppression, model gates and installed payload are unchanged.
Timing boundaries across both contact orders and either surviving finger are
covered by 140 host replays. Full build/core/docs, pointer/mouse/ABI, xHCI and
the disposable-VM context-menu regression pass. The revised feel remains
physically untested; the candidate above is prepared for that check. Validation is retained
outside build under `~/.local/share/srvros/validation/trackpad-timing-20261023`.

### October 23 audio/network round (collected)

Collected release `b6306e33e9590538` / v58 (source `2ae5e7a`) after the
October 23 attended round. The tester reports normal operation. Speaker and
headphone observations each say success; both six-second files completed with
zero underruns. The Stop replay exited 1 with an empty queue and no owner;
the user recorded successful audio controls. HDA remained ready with no error,
five test requests completed, and the jack was connected at final exit. Audible
channel/isolation acceptance is based on the prompted tester observations;
final unplug/speaker recovery is not independently established by that snapshot.

The exact `0b95:1790` rev0200 adapter passed native DHCP, DNS and HTTP 200 both
before and after cable removal. The disconnected report showed link down and
DHCP failure. Network Utility's DNS query also completed. Live System link-down
refresh was not captured (saved refreshes show link up), so retain that narrow
visual check separately. This does not establish USB hot-unplug recovery.

A 138-byte pasted Network Utility report supplied newline-delimited answers to
the remaining reads. The script completed normally, but the secondary-input and
drag-cancel fields contain network report lines; **trackpad acceptance is not
established**. The follow-up added Terminal paste normalization when bracketed
paste is not enabled, host/guest regressions, and a trackpad-only attended script.
Touched scope: deskterm, reusable terminal helper/tests, shell read overflow handling, checklist packaging/Help
and this unit's documentation. No device/firmware gates change. PR83 owns this
work; Pro PR19 retains Wi-Fi ownership. Main `d2d56a6` adds Air6,1 audio evidence
and endurance instructions and is merged here; it adds no new driver code.

Session `session-00065e8778243499-00` matches the release, has an exit snapshot,
clean desktop retirement and no collector attention. All prior data was retained;
only expected `.srvros/meta` changed. Offline fsck and allocation passed (999
files, 2073 clusters, zero leaks/mismatches). Ubuntu default, consumed one-shot
and recovery were verified. At collection, current was `b6306e33e9590538`, previous accepted
battery desktop is `9aa8dbf6fbda8252`. USB is unmounted and no reboot is armed.
Raw reports and run records are retained outside build under
`~/.local/share/srvros/pending-trials/b6306e33e9590538/returned`; validation is in
`~/.local/share/srvros/validation/device-checks-b6306e33e9590538`.

The trackpad-only follow-up was installed as `4414f888007a8407` (compiled
`b1486c6`, staged documentation head `d510c39`), retaining `9aa8dbf6fbda8252` as
previous recovery. No reboot is armed. Full build/core/docs, host paste/checklist
cases, guest multiline paste across consecutive reads, oversized-read draining,
trackpad script reports and the exact EFI/audio/GPT save/reopen rehearsal passed.
Help's first screen shows the trackpad-only command. The initial VM observations were explicitly marked skipped; native results follow above. Before
installation the existing USB passed fsck/allocation with all 999 files/2073
clusters and no leaks. Earlier device reports are included in the pre-run hashes.
Validation, payload and run/collector survive cleanup under
`~/.local/share/srvros/validation/trackpad-followup-4414f888007a8407` and
`~/.local/share/srvros/pending-trials/4414f888007a8407`.

### Integrated v55 desktop physically accepted (2026-10-23)

Release `9aa8dbf6fbda8252` (compiled `7ab571f`, staged `d5bb51d`) passed the
attended combined Home trial. The tester reported everything worked as intended:
input during playback, brightness/volume, Sensors, battery/AC, document save,
desktop exit/reopen and monitor reboot. Screenshot `Screen-20261023-183835-4-1`
shows Text's saved-and-verified `integrated-v55.txt` and `AC · 96%`; the archived
file contains `Test document`. Only one new screenshot was saved, so reopening
is supported by the tester report and second lifetime, not a second image.

Sessions `session-00065e86405f5247-00` and `session-00065e864b291935-00` drained
nine and four peak members respectively. Both battery runtimes retired with one
attach/detach, zero heap/errors/denials and no rejected samples. All 83 and five
active SCI deliveries were handled, with zero unclaimed or in-flight interrupts.
The second boot log confirms the first logger and Home hold release. Final
release follows its own exit snapshot; the instructed return report, changed
Ubuntu boot and recovery checks complete that evidence without inventing a final
log receipt. An earlier same-release session is retained separately.

Five playback processes ran. Final PCM state was done, 1,152,000 bytes submitted
for the last stream, zero underruns, owner zero and queue empty; HDA was ready
with no error/pending operation. Brightness requests 58% and 22% applied. The
final read-only snapshots reported 57/59 C and 1191/1199 RPM, with fresh samples
and no SMC failures. Boot-scoped providers remain available at the monitor;
app resources, battery and session logging retire before Home's hold releases.

All existing data files were preserved; only expected `.srvros/meta` changed.
Offline fsck and independent allocation passed: 985 files, 2057 clusters, zero
leaks/mismatches/errors. Ubuntu default, consumed one-shot and recovery verified;
USB is unmounted and no reboot is armed. Current is `9aa8dbf6fbda8252`, previous
`b427b78f21644bd2`; accepted v54 is independently retained. Payload and evidence
survive cleanup under `~/.local/share/srvros/known-good/9aa8dbf6fbda8252` and
`~/.local/share/srvros/accepted-evidence/9aa8dbf6fbda8252`.

### Integration and remaining parity scope

PR76 combines existing brightness, HDA, read-only CPU/SMC and CDC ECM discovery
with ordinary battery-owned Home. It adds cached 20%/5% warnings and battery-label
access to System, without firmware I/O in UI paths or new native model gates.
An Air network adapter is now attached; Ethernet acceptance remains separate.
The October 23 rounds above close prompted headphone/channel and secondary-click/
app-cancellation checks. Physical low-charge alerts, calibration, battery life and
suspend remain open; secondary tap timing still needs refinement.

The full build/both runtime kernels/core/docs and combined normal, PCM playback,
pending-save cancellation, unavailable battery and legacy ordinary VM cases
passed at `7ab571f`. Test follow-up `d5bb51d` checks the final provider snapshots;
exact staged GRUB/Limine/native EFI also passed with native battery refusal and
usable apps. Host tests cover warning hysteresis, stale/unavailable suppression,
notification focus and System routing. Low-charge behavior is synthetic evidence,
not a physical battery-drain test. Validation artifacts are archived outside build.

After the trial, main `e741c58` was inspected and merged at `7376018`, preserving
Air6,1's exact brightness gate and separate v56 recipe alongside v53/v54/v55 and
Pro v37. Full build/both runtime kernels/core/docs passed at `7376018`. Combined
HDA/battery/save/reopen and exact merged EFI stage `bbd585f9114b6185` passed
at `bc8bda9`, including final provider ownership and offline allocation. These
merged artifacts are VM-tested; physical acceptance above stays bound to the
installed `9aa8dbf6fbda8252` payload. Pro PR19 head `f1785c6` owns bounded USB Wi-Fi
MCU/baseband work: eleven compact phases/readbacks and read-only TXBE observations
are recorded; the later write mismatch remains unresolved, and RF/scan/association/
connectivity are unaccepted. No draft Wi-Fi implementation was copied into this
branch. See the [parity ledger](../device-parity.md) for shared versus native scope.

During closeout, main advanced to `b01ae95` (Air6,1 exact audio profile and v57).
It was inspected and merged at `5c975aa`, retaining separate v55/v56/v57 recipes.
Full build/both runtime kernels/core/docs, combined HDA/battery/save/reopen and
exact EFI stage `67d88f2345ef97ff` all passed locally at that revision. Updated
integration evidence is retained outside build; this later payload is not
installed or physically accepted. No additional Air test is queued.


### Ordinary Home v54 physically accepted (2026-10-23)

Release `fc5b73b074db7884` returned to Ubuntu. The two screenshot-matched sessions
`session-00065e84a072f3e3-00` and `session-00065e84a5ba22cc-00` record distinct
inherited lifetimes, with eight and five peak members. Both drain to zero and
retire the battery runtime with one attach/detach, zero retained heap, errors or
denied operations. There were 29 and 14 sample attempts, no rejected samples,
and 29 and 24 handled SCI deliveries with zero unclaimed. The second boot log
also verifies the first session's logger completion and Home hold release.

Both saved screenshots show `AC · 96%`: Text first reports the document saved
and verified, then opened with identical contents after the desktop restart.
The document and screenshots are backed up; all prior non-metadata files are
unchanged. Offline fsck and independent allocation pass: 972 files, 2,002 allocated
clusters, zero leaks/mismatches/errors. Ubuntu default, consumed one-shot and
installed recovery are verified; USB is unmounted. Current is `fc5b73b074db7884`,
previous remains `b427b78f21644bd2`. No reboot is armed.

An additional earlier same-release session is retained separately; it is not
substituted for the paired save/reopen evidence. Final logger/mount release is
after the final exit snapshot. The tester confirmed normal app/input behavior and return using monitor
`reboot`, completing this scoped physical acceptance. Evidence and payloads survive
build cleanup under `~/.local/share/srvros/accepted-evidence/fc5b73b074db7884`
and `~/.local/share/srvros/known-good/fc5b73b074db7884`. Main `bd1e834` was fetched and inspected: PR71 file identities and PR73
preference trial tooling are now merged upstream, but are not part of this
physical candidate. Main was subsequently merged at `bda091c`; full build/both runtime kernels,
core/docs, workload regressions and normal/cancel/profile-refusal ordinary Home
VMs passed locally. Integration evidence is archived outside build. These later
file-identity changes are VM/host validated here; physical acceptance above
remains tied to the installed `fc5b73b074db7884` payload.

### Attended v54 trial preparation (2026-10-23)

Preparing tested release `fc5b73b074db7884` for ordinary Home app/save/exit/reopen
acceptance. Main `58b354f` was inspected and merged; it adds only Air6,1 evidence,
so staged binaries remain unchanged. PR71 (`9c0b09c`) owns imported file identities
and PR19 (`d4f8be5`) owns Wi-Fi; neither is included in this candidate. Read-only
USB metadata inspection found no duplicate IDs within Home; existing cross-path
collisions are retained as evidence rather than repaired for this trial. Scope
is this unit's run records, recovery checks, installation and collection. Human
app use is required. See the local checkpoint for actual armed/returned state.

### Ordinary Home battery startup: locally validated (PR #69)

Base `80b567a` includes Air6,1 PR #68. The distinct v54 `--desktop-battery`
profile opens ordinary Home apps with continuous battery monitoring, no capture
timer and no forced exit. Ctrl+Alt+Esc drains inherited apps, stops the service,
finishes logging and releases the mount hold. Home stays mounted; `desktop battery`
reopens the session. Default startup remains off and exact native gates remain
unchanged. Air6,1 v53 and Pro PR #19 (`b816acd`) are preserved.

The core/build/docs suite and ordinary boot matrix passed on `538c2e9`: normal
files/orphan jobs, pending-save exit, unavailable profile, native refusal with
usable apps, missing Home/runtime/opt-in, uncertain acknowledgement containment,
legacy command refusal and System alarm/Stop/restart. An initial EFI fixture
had no reference clock and exposed unsafe session entry before blocking was
possible. The entry now refuses before ownership when clock/context prerequisites
are missing. Full build/core passed again on `e1ee996`; `a37ef21` changes only
the VM fixture command. Missing-clock and normal/reopen checks then passed.

Exact stage `fc5b73b074db7884` (source `a37ef21`) passed GRUB → staged Limine →
native kernel on disposable Q35 EFI. The expected native profile refusal kept
Text, Files and Terminal usable; two distinct inherited lifetimes and logs retired,
files/allocation were verified, and monitor reboot completed. The first lifetime
peaked at eleven members. The shared cache-to-ABI-to-view test covers expiry,
rejected AC coherence, recovery and unavailable presentation without a stale
percentage. Screenshots show fresh and unavailable labels in the appropriate VMs.

Evidence and payloads are retained outside `build` under
`~/.local/share/srvros/local-validation/home-session-a37ef21549f9`.
These checks were host/VM evidence; the subsequent attended acceptance is
recorded above. Before that trial, installed current was `c06f94a5893a402d` and
previous was `b427b78f21644bd2`. Gauge calibration, battery life and sibling
hardware remain separate. Actions remain disabled.

### Guided Home AC/indicator transitions accepted (PR #66)

Release `c06f94a5893a402d`, staged source `1dcf202`, compiled/tested source
`91df3d8` on main `52ef3ea`, passed the attended v52 trial on this Air.
Session `session-00065e77908a58e7-00` recorded 61.111 seconds, generation 1→23,
339 events and two acknowledged battery alarms. All 51 SCI interrupts were
handled, with zero unclaimed interrupts. All five inherited process members
retired before battery ownership, logging and the mount were released.

Three consecutive fresh readings accepted each AC/battery/AC phase. Saved
1440x900 screenshots show `AC · 97%`, `97%`, then `AC · 97%`; unplugged current
was −1.467 A and reconnected current was zero. Four logs and all three BMPs
were verified. All 948 prior data files stayed unchanged; only expected metadata
and seven new evidence files changed. Offline fsck/allocation passed: 956 files,
1,905 allocated clusters, zero leaks/mismatches/errors. Ubuntu recovery/default,
EFI order and consumed one-shot were verified. USB is cleanly unmounted.

The accepted payload/evidence survive cleanup under
`~/.local/share/srvros/known-good/c06f94a5893a402d` and
`~/.local/share/srvros/accepted-evidence/c06f94a5893a402d`.
Full build/both battery kernels/core/docs, all five guided VM cases, existing
three-minute capture, pending-save exit and alarm/restart regressions passed.
Exact-stage native refusal/save/return and missing-workspace return passed.
Later integration of main `208423f` changes only Air6,1 documentation; tested
compiled source is unchanged. Pro PR19 `20d48a5` remains separate Wi-Fi work.
Actions remain disabled; validation is local.

Ubuntu reports 85%, while SrvrOS reports 97%. Read-only inspection confirms
Ubuntu's installed `acpi-sbs` module reads command `0x0e` (absolute SOC); SrvrOS
reads `0x0d` (relative SOC). The [SBS specification](https://sbs-forum.org/specs/sbdat110.pdf)
defines these against design capacity and present full-charge capacity,
respectively. The post-return remaining/full capacities agree (6,035/6,283 mAh;
design 7,150 mAh). This explains the differing percentages without changing the
relative gauge. Physical calibration, battery life and health are not accepted.

This closes explicit AC transition/indicator acceptance. No reboot is armed.
Next: opt-in ordinary native Home startup using the accepted owner, with graceful
unavailable/stale presentation and exit/restart regressions. Ordinary automatic
startup remains OFF; sibling firmware/resource gates are unchanged.

### Explicit continuous Home desktop capture accepted (PR #63)

Release `8666698f5e5d4ed5`, staged source `182ee78`, compiled/tested source
`42d074f` on main `105dffd`, passed the automatic v51 trial on this Air.
The followup commit only corrects a VM fault-test assertion; compiled payloads
are unchanged. Session `session-00065e76fefc7bdc-00` recorded 201.276 seconds,
74 samples (generation 1 through 74), 1,110 battery events and 74 handled SCI
interrupts with zero unclaimed interrupts. All five inherited process members
retired before battery ownership, logging and the mount were released.

The ordinary Home desktop kept Text open for a measured 179.994 seconds.
During that interval, 13,332 of 18,000 sampled ticks were idle (74.1%); it presented
three frames and wrote 305,184 framebuffer bytes. This is one idle workload,
not a physical key-latency, battery-life or power measurement. The saved
1440x900 screenshot shows the global indicator displaying `AC · 97%`; this
confirms rendering, not gauge accuracy or AC transition behavior.

Four logs and one screenshot were saved. All 943 pre-existing data files stayed
unchanged; only expected filesystem metadata changed. Offline fsck and allocation
validation passed with zero leaks, bitmap mismatches or errors. Ubuntu returned,
the one-shot entry was consumed, and default boot/EFI/recovery were verified.
The accepted payload and evidence survive build cleanup under
`~/.local/share/srvros/known-good/8666698f5e5d4ed5` and
`~/.local/share/srvros/accepted-evidence/8666698f5e5d4ed5`.

Local full build/both diagnostic kernels, full core and docs passed. The actual
Q35 boot capture passed completion, clean profile/query refusal and retained
uncertain-ack ownership, plus pending-save exit and continuous alarm/restart
regressions. Exact-stage native refusal and missing-workspace return passed.
Actions remain disabled. Pro PR19 `acd4ae7` remains separate Wi-Fi work;
its later integration must preserve both sets of boot-staging/main changes.

No reboot is armed. Next: attended AC unplug/replug and gauge/indicator acceptance,
then ordinary native startup integration. The explicit capture has physical
continuity acceptance; arbitrary interactive app workloads and accuracy remain
separate. Native startup stays OFF; sibling profiles are unchanged.

### Ordinary Home desktop lifetime: VM/host validation (PR #62)

Source `4351910`, based on main `340f566`, connects the inherited lifetime to
ordinary launcher apps, Home file workers and periodic workspace logging in the
explicit VM-only `battery-home-desktop` command. Text, Files and Terminal use
normal paths; session-changing orphan jobs remain owned through final release.
Shutdown drains apps, battery work and logging before releasing the exact mount.
Logger quiescence has a deadline; failure retains live ownership. A reproduced
UART TX timeout now preserves RX and allows later TX recovery, with bounded
polling. Timed-out serial bytes can still be lost; persisted logs remain required.

Full build/both battery kernels, complete core suite and 446 documentation links
passed locally. All five ordinary Home cases passed: normal, exit with a submitted
save, unavailable profile, uncertain-runtime containment and native refusal.
Normal/cancel peaked at ten members, retired all of them and processed 424/552 battery events;
unavailable-profile apps remained usable (eleven peak members). Clean cases
verified exact Text/Terminal/orphan file bytes, the complete workspace file set,
two periodic logs plus boot/exit logs, release ordering and offline allocation.
The inherited-process/PTY regression (256 launches/32 waiter pairs), continuous
alarm/Stop/restart and existing bounded Home pending-write cancellation passed.
A submitted save proves outstanding app work, not physical USB DMA timing.

Exact payloads, disposable images and checked receipts are retained outside
`build` in `~/.local/share/srvros/local-validation/home-desktop-43519102297f`.
Pro PR19 `cd2f274` remains separate USB Wi-Fi work; sibling gates are unchanged.
No reboot or physical run is armed. Installed current `fd6f6b9d397c339f` and
previous `b427b78f21644bd2` remain unchanged. Actions remain disabled.

Next: explicit native ordinary-app continuous session testing, then attended
AC/gauge/indicator acceptance before enabling ordinary native startup. This is
VM/host acceptance only. See the [Home session lifecycle](../battery-home-lifecycle.md).

### Inherited desktop lifetime: VM/host validation (PR #60)

Source `8ea35fd` integrates main `50b14fe` (Air6,1 PR59/61). A kernel-created
lifetime now tracks every descendant across spawn, exec, orphaning and job-control
changes. Membership includes loading and final resource release; revocation
blocks new reservations before shutdown drains the selected lifetime. Competing
reapers claim completion atomically and final removal wakes other waiters.
The continuous VM battery session uses this owner; native bounded diagnostics
and ordinary startup retain their existing behavior.

Local full build/both battery kernels, complete core suite and docs passed.
The full-boot fixture passed 256 sequential app launches, 32 competing waiter
pairs, failed loads, synchronous/detached spawn, orphan setsid/exec/group changes,
unrelated-process survival and the existing PTY regression. All nine continuous
battery cases passed, including 1,608 + 72 events across two lifetimes with 29
summary records, alarms, early exit, refusals and uncertain-ownership containment.
Clean cases preserved workspace bytes and passed offline allocation. Existing
Home capture (60 edits/five file cycles) and pending-write cancellation also passed.
Those file regressions still use the bounded diagnostic owner; they do not yet
accept file/logging overlap under the new ordinary desktop owner.

Exact payloads and 99 evidence files (18 compressed artifacts verified by
re-reading) are retained outside `build` in
`~/.local/share/srvros/local-validation/process-lifetime-8ea35fde079a`.
Pro PR19 `301a640` remains USB Wi-Fi; Air6,1 profiles and records are preserved.
No physical run/reboot is armed; current `fd6f6b9d397c339f` and previous
`b427b78f21644bd2` are untouched. Actions remain disabled.

Next: wrap the arbitrary-app desktop in this owner, validate periodic logging
and pending file-work overlap, then native continuous-session testing and
attended AC/gauge/indicator acceptance. Ordinary native battery startup stays OFF.
See the [ownership follow-up](../battery-home-lifecycle.md#ordinary-desktop-ownership-follow-up).

### Continuous service accounting: VM validation (PR #55)

Compiled source `07c06da`, based on merged PR53 (`62067a1`), passed all nine
continuous-session cases locally. The first lifetime handled 1,608 events beyond
120 seconds between summaries; Stop/restart/close retired a second lifetime with
72 events. Both lifetimes used 28 total bounded event-summary records. Alarms,
early close, native/unmounted/profile refusal, normal-return query refusal,
uncertain acknowledgement and effects preceding refusal passed their respective
completion/containment checks. Clean cases preserved every workspace byte and
passed offline allocation; fatal cases retained ownership without cleanup claims.

The new `battery-home-monitor` command is VM-only. Native diagnostic limits and
ordinary startup remain unchanged. Event telemetry is fixed-size and saturating,
with periodic and final records emitted by the service thread. This preserves
transport/query/shutdown deadlines; it does not validate native continuous
monitoring, physical input latency, alarms or gauge accuracy.

A development run lost its UART observation channel. A repeat captured
`serial_ready=0`, an idle CPU and successful session retirement/monitor return on
screen. Those incomplete captures remain separate from passing collections.
The harness now retains UART/register/screen evidence when output stalls. The
general serial transmit-timeout policy remains an open reliability issue.

Integrated actual main `1b88c8a` (Air6,1 PR54/56/57/58), preserving that agent's
measured profile and record. Source `05a50bc` passed full build/both kernels,
the extended 1,680-event session (29 summaries), accepted Home capture, pending-write
cancellation, full core and docs checks. Evidence is hash-verified outside `build`
in `~/.local/share/srvros/local-validation/battery-continuous-05a50bc16bdf`
(128 files, 13 compressed images verified by decompression). Directed and integrated
payloads are retained separately, together with the incomplete UART-loss capture.
Pro PR19 `45db188` retains Wi-Fi work. Next battery scope is
an inherited kernel desktop lifetime identity, app churn/Terminal orphan handling
and periodic logging overlap before ordinary startup or another native trial.
See the [ownership follow-up](../battery-home-lifecycle.md#ordinary-desktop-ownership-follow-up).

No physical run or reboot is armed. Current `fd6f6b9d397c339f` and previous
`b427b78f21644bd2` remain the accepted PR53 payloads below. Actions remain disabled.

### Explicit Home desktop battery session accepted (PR #53)

Base `9ca1922` (PR52); compiled source `faf40ab`; collector correction `7f1438d`.
Release `fd6f6b9d397c339f`, session `session-00065e752131c4aa-00`, passed the
explicit v49 held-Home session: five Start/Stop requests, three clean service
lifetimes, all 60 generated edits, five exact file cycles and 20 worker operations.
The workload lasted 62.170 seconds, with file checkpoints taking 2.91–6.06 seconds.
It handled 180 natural SMBus completion events and 12 SCI deliveries with zero
unclaimed deliveries. No alarms occurred. Every lifetime returned masked/idle
with the exact mount held, checked resources and restored foreign sources.
Clients and runtime retired before the mount was released and logs saved.

All 935 pre-existing non-metadata files remained unchanged, including the earlier
failed study and disposable filesystem image. Exactly five `AA` files and three
session logs were added; only `.srvros/meta` changed. Offline exFAT/allocation
passed (944 files, zero leaks). Automatic Ubuntu return, consumed one-shot,
Ubuntu default and EFI recovery were verified. Current is `fd6f6b9d397c339f`;
previous remains `b427b78f21644bd2`. Evidence/payload and an independent recovery
copy are hash-verified outside `build` under
`~/.local/share/srvros/accepted-evidence/fd6f6b9d397c339f`. No reboot is armed.

The collector initially reused a filename variable for the JSON session label.
`7f1438d` fixes that host-only error and adds successful/rejected-collection
identity checks; all 21 collector tests pass. Both collections are retained;
the corrected report references identical raw log bytes and verified file backups.
This did not change the physical payload or relax its completion predicate.

The first v49 attempt (`63333d27f5e5c064`, source `b67d179`, session
`session-00065e74e07682d9-00`) remains a failed workload: 24 generated/23 presented
edits and two retired lifetimes. All 929 previous non-metadata files were preserved,
and filesystem/recovery checks passed. Its 4.7-second diagnostic progress budget
was reproduced in Q35 with four-second EC connections and throttled USB writes.
The corrected study gives only control/file checkpoints 15.7 seconds and the
whole workload up to 90 seconds. Normal input/client deadlines and the 110-second
owner limit remain unchanged. The slow VM completed all five checkpoints in
7.98–8.43 seconds each, all edits and three lifetimes in 79.70 seconds.
Failed native evidence is retained outside `build`; it is not a pass.

Validation remains local with Actions disabled. The original ownership code
passed 15 directed cases and nine full boot/regression cases, including actual
System mouse controls, cancellation and fault containment. The corrected payload
passed full build/core, both kernels, normal and slow full-boot workloads, query
and profile refusal, uncertain-ack containment, cancellation and the existing
VM-only file workload. Exact-stage refusal/save/return and missing-volume return
passed before installation. This accepts the explicit automatic workload, not
physical input latency, native alarms, gauge accuracy or unlimited monitoring.

Next: ordinary desktop-session ownership/startup, attended AC and charging/gauge
checks, then the finished graphical indicator in the
[delivery plan](../battery-home-lifecycle.md#delivery-through-the-desktop-indicator).
Ordinary startup remains OFF. Pro PR19 `af779b0` retains Wi-Fi work and Air6,1
PR54 `c8dc0a0` retains inventory/console bring-up; their own hardware results do
not inherit the Air7,2 firmware gate or this acceptance.

### Mounted Home battery boundary accepted (PR #52)

Base `85d5cb7` (PR51); compiled implementation `7fac77d`; final validators
`171b307`. Release `71ab6b95892a9a8c`, session
`session-00065e7468b78060-00`, passed the explicit v48 mounted-Home diagnostic:
fresh generation 3, 45 natural completion events, resource-checked runtime
retirement, then release of the exact mount hold. Home stayed mounted throughout;
periodic logging was paused and no desktop clients ran. No alarms occurred.

All 925 existing non-metadata files remained unchanged, including the disposable
filesystem image. Only workspace metadata changed; exactly three matching logs
were added. Offline exFAT and allocation checks passed (929 files, no leaks).
Automatic Ubuntu return, consumed one-shot, Ubuntu default and EFI recovery were
verified. Current is `71ab6b95892a9a8c`; previous remains `b427b78f21644bd2`.
Evidence and payload are hash-verified outside `build` under
`~/.local/share/srvros/accepted-evidence/71ab6b95892a9a8c`. No reboot is armed.

All 16 final local checks passed: eight command cases, four boot-flow cases,
normal/cancel controls-file regressions, exact-stage refusal and missing-drive
return. Full build, core/docs and corruption checks passed with Actions disabled.
A controls/file regression exposed saved-log interleaving; bounded 1 KiB saved
record appends now precede rendering/UART, whose render batches remain 64 bytes.
Deterministic preemption tests cover the fix. Original rejected evidence remains
in the local validation archive. Cancellation distinguishes reported incomplete
exit from compositor termination during bounded close; neither is full workload
completion.

Next bounded scope: explicit native desktop-session ownership through System
controls, asynchronous file work and shutdown, retaining the same exact Air
profile and mount lifetime. Ordinary startup remains OFF. Physical charging/gauge
accuracy and the finished indicator remain pending in the
[delivery plan](../battery-home-lifecycle.md#delivery-through-the-desktop-indicator).
Pro PR19 `92a1bbc` retains independent Wi-Fi work; preserve its main/staging and
Makefile additions during integration. No sibling firmware acceptance is implied.

### Controls during submitted Text jobs (PR #51)

Implementation `23837b9`, base `fb96ace` (PR50). The explicit VM fixture sends
Stop/Start/Stop/Start through the shared System control syscall only after each
Text save request is completely transmitted to its file worker. The shared
`desk_file_job_submit` helper separates bounded submission from reply consumption;
ordinary polling retains its combined behavior.

All six directed cases and three regressions passed locally. Normal completion
proved five requests, three lifetimes, 60 edits, five file cycles/20 operations,
26 synthetic events/10 alarms, exact saved contents and clean allocation. The
last active service retired on session exit before releasing the mount hold.
Cancellation after the first submitted write finished the accepted `AA` draft
and retired all three descendants; its workload remains explicitly incomplete.
Native/unmounted/profile refusal and uncertain-ack containment also passed.
Actual System mouse controls and the sustained SMBus/Text workload passed as
regressions. Full build, both diagnostic kernels, final core/docs and strict
receipt-corruption checks passed; Actions remain disabled.

Evidence is hash-verified outside `build` at
`~/.local/share/srvros/local-validation/battery-control-files-23837b98fd45`.
Pending application jobs do not prove active USB DMA or physical-input latency.
Pro PR19 `77ba966` retains independent USB Wi-Fi scope. No native payload or
reboot change: current `fda709928307b1b1`, previous `b427b78f21644bd2`.
Native Home/startup remain OFF. Next: native mounted normal-return/refusal and
desktop-session ownership, followed by physical charging transitions and the
finished indicator in the [delivery plan](../battery-home-lifecycle.md#delivery-through-the-desktop-indicator).

### Mounted-volume ownership (PR #50)

Implementation `5fe69fa`, base `3c47b50` (PR49). VM battery control and SMBus/file
sessions now hold the exact workspace device, parent and mount generation until
clients and runtime retire. Non-reused tokens reject stale release across
Start/Stop/restart, separate sessions and unmount/remount. Unmount, scratch-file
allocation and image conversion refuse while held; ordinary file I/O remains
available. Host checks cover stale/duplicate tokens, identity mismatch, exhausted
counters and storage failure; a retired owner can release unhealthy storage
without bypassing the existing failed-sync/unmount policy.

Seven directed controls cases passed: two sessions/four runtime lifetimes/eight
requests, remount with distinct tokens, mouse Start/Stop/restart and active close,
early cancellation, native/unmounted/profile refusal and contained uncertain
acknowledgement. Clean cases preserve every disk byte and pass fsck/allocation;
the fault retains ownership and makes no retirement/reset claim. The mounted
SMBus/Text regression completed 60 presented edits and five file cycles/20 worker
operations, 31 events/15 alarms and generation 30, then released its hold after
all three descendants and runtime. Parked controls and the v47 parked-file
VM regressions also pass. Full build, both diagnostic kernels, core/docs, strict receipt
checks and the complete image write/flush/power-cut fault sweep passed locally.

Evidence is retained outside `build` at
`~/.local/share/srvros/local-validation/battery-workspace-hold-5fe69fa59d02`.
No physical install/reboot; current `fda709928307b1b1`, previous
`b427b78f21644bd2`, native Home/startup OFF. Pro PR19 `fdec140` has independent
Wi-Fi scope. Actions remain disabled. Next: combine held System controls with
actual asynchronous Text file operations/cancellation in the same VM session,
then native mounted return/desktop ownership and charging-transition acceptance.
The existing cached percentage/AC menu-bar label and System controls remain the
UI foundation; the [delivery plan](../battery-home-lifecycle.md#delivery-through-the-desktop-indicator)
tracks ordinary startup and the finished graphical indicator.

### Mounted-workspace lifecycle VM study (PR #49)

Implementation `d223325`, base `598fe27` (PR48). The new explicit
`battery-workspace-controls` diagnostic reuses System Start/Stop and its scoped
mailbox while a disposable Home volume stays mounted. Two sessions completed
four lifetimes/eight requests and handled 12 synthetic events/four acknowledged
alarms, with epoch-specific drained resource receipts and final revocation.
Mouse Start/Stop/restart followed by closing System while active also retired
cleanly, as did early cancellation. All clean runs allowed a replacement
compositor and preserved the entire volume byte for byte with offline
fsck/allocation checks. No additional file-load or physical-input claim.

Uncertain acknowledgement stayed halted with no runtime retirement, unmount or
reset claim. Native invocation and unmounted admission refused before runtime;
a VM without the controller profile returned unavailable and released the scope.
All seven directed cases and four strict receipt-corruption tests passed. Full
build, both diagnostic kernels, core/docs and four existing regressions passed:
parked controls, v47 Text/file capture, v46 completion capture and query-refusal
save/reset. Five saved-capture proof tests include UART interleaving versus
corrupt saved evidence. Validation and retained disposable disks are archived
outside `build` at
`~/.local/share/srvros/local-validation/battery-workspace-controls-81d6fbdf3075`.
The [Home lifecycle audit](../battery-home-lifecycle.md) records the exact
remaining mounted-volume binding and refusal/cleanup requirements.

The parked-file regression exposed UART interleaving in its harness: the saved
boot-log proof was valid while serial writers split an event line. Follow-up
`81d6fbd` strictly validates the atomic saved proof and matches its final summary
to the retired UART output; reset/contents/allocation checks remain required.
Corrupt saved evidence still fails. The original failure and disposable image
are retained with the validation artifacts; this does not change runtime logging.

No physical payload or recovery change; current `fda709928307b1b1`, previous
`b427b78f21644bd2`, no pending run/reboot. Native Home/startup remain OFF. Pro PR19
`3d51e4f` was inspected and retains independent Wi-Fi scope. Actions stay disabled.
Next: bind lifetime ownership to the specific mounted volume, then combine
explicit controls and asynchronous Text file activity in VM before native work.

### Accepted parked SMBus/Text/file capture (PR #48)

Release `fda709928307b1b1`, source `e6b4b03` (implementation `029e344`), session
`session-00065e731a64ed84-00` completed the automatic v47 capture. The service diagnostic ran for 60.371
seconds, including the 60.019-second desktop workload. It processed 345 natural
completion events and refreshed the battery cache from generation 1 to 23. The real Text worker
completed all 60 generated/presented edits, five file cycles and 20 operations;
61 cached UI reads reached generation 23. All three descendants retired, followed
by the battery service and exclusive image. The 23 SCI deliveries were handled,
with zero unclaimed/in-flight interrupts; heap/errors/denied were zero at retirement.

The outer Home filesystem stayed parked. Exact inner contents, inner/outer fsck
and independent allocation checks passed. All 921 unrelated USB files remained
identical; only `.srvros/meta` and the dedicated `battery-fs.img` changed, and
three matching logs were added. No collector attention. The Ubuntu boot ID changed,
the one-shot was consumed, and Ubuntu default/EFI order and recovery were verified.
Current is `fda709928307b1b1`; accepted desktop `b427b78f21644bd2` remains previous.
The accepted payload and evidence are independently hash-verified outside `build`
at `~/.local/share/srvros/accepted-evidence/fda709928307b1b1`. No reboot is armed.

No alarms occurred: native alarm acknowledgement/invalidation, gauge accuracy
and physical-input latency remain untested by this automatic capture. This
accepts bounded parked-image coexistence, not ordinary mounted Home, automatic
battery startup or a sibling firmware profile. The short v46 limit stays 128;
v47 caps the full minute at 512 and retains all compact event receipts.

Local validation passed both diagnostic builds, all/core/docs, 50 staging tests,
20 collector tests and strict proof corruption cases. Nine full-boot VM cases
cover modeled native-rate completion, recurring alarms, Stop during read/ack,
uncertain acknowledgement, event limit, no events and native/profile refusals.
Positive cases verify saved proof, contents, metadata and both allocation graphs.
Interrupted Stop runs remain incomplete; uncertain acknowledgement/event-limit
runs reset without clean-retirement or saved-completion claims. The v46 capture
and ordinary mounted Text/file regressions passed, as did exact staged native
refusal/save/return and missing-workspace return. The local validation archive
has 111 hash-verified files, including 22 round-trip-verified compressed disks:
`~/.local/share/srvros/local-validation/battery-smbus-files-029e34478cdc`.

Base `53a24a0` (PR47). Pro PR19 `c20917f` was inspected; its current Wi-Fi control
retirement/capture edits do not overlap this implementation. Shared main/staging
profiles must still be preserved during integration. Actions remain disabled.
Next: audit lifecycle/storage ownership for explicitly controlled ordinary Home
coexistence, with VM proof before native enablement. Home/startup remain OFF.

### Completed VM SMBus/Text/file study (PR #47)

Implementation `b1e80f0`, base `4ea10e1`. The VM-only mounted-workspace command
reuses the real Text/file worker: both completion-only and mixed-alarm runs
completed 60 generated/presented edits, five file cycles and 20 worker operations.
The mixed run handled 31 events/15 acknowledged alarms: seven known senders
invalidated the cache, eight unknown senders did not request refresh. Generation
30 was fresh after the last invalidation at 28. Both runs retired all three
clients and the service, launched a replacement compositor and passed exact
filesystem contents/metadata, allocation and fsck checks.

Directed Stop during alarm read and acknowledgement retired cleanly with a
nonfresh stopped cache, while correctly reporting an interrupted workload.
An uncertain acknowledgement remained halted without retirement or filesystem
cleanup claims. Native command invocation refused before runtime activity.
The initial read/Stop test caught the VM study checking idle resources before
an in-flight event drained; the check now follows the existing masked worker
drain, before disconnect. Native capture already used that ordering.

Six strict-decoder tests reject missing/duplicate/torn receipts, invalid stop
ordering, contradictory counts, false freshness and false completion. All six
new VM cases passed. Both diagnostic builds, full build/core/documentation checks and three existing
regressions passed: legacy late alarm, full-boot parked capture/save/reset and
ordinary mounted Text/file workspace. The failed original fixture is retained
with the passing results in the hash-verified archive outside `build`:
`~/.local/share/srvros/local-validation/battery-smbus-workspace-b1e80f066271`. Pro PR19 `79132d6` remains separate
Wi-Fi work; only independent Makefile test additions overlap.

No physical stage, installed payload, reboot or hardware acceptance changed.
Native alarms, mounted Home/startup and gauge accuracy remain unaccepted. Next:
design an explicit parked, exclusive-image Text/file capture with sustained host
events and a bounded event budget informed by the native completion rate; verify
limit/stop failures in VM before a new physical trial.

### Accepted parked SMBus completion capture (PR #46)

Release `a1451825851dc5f0`, source `10a4cbe`, session
`session-00065e7202038212-00` passed the automatic v46 native capture.
The exact Air profile handled 45 natural query10 completion events (status 128)
without entering `_Q10` AML, observed fresh generation 3, then masked sources,
drained the worker, restored event enables and retired cleanly. All three SCI
deliveries were handled, with zero unclaimed/in-flight interrupts; retirement
reported heap/errors/denied all zero. The stopped cache was correctly nonfresh.

No alarms occurred: native alarm acknowledgement, unknown senders and alarm-driven
cache refresh remain unexercised; those paths have VM evidence only. This accepts
bounded completion handling and sampling with parked storage, not gauge accuracy,
mounted Home coexistence, ordinary startup or another machine's firmware.

All 919 prior ordinary USB files remained identical; only the metadata sidecar
changed and three matching logs were added. The filesystem image hash stayed
unchanged. Offline fsck and independent allocation checks passed with zero leaks,
bitmap mismatches or errors. The changed Ubuntu boot ID, consumed one-shot,
Ubuntu default, EFI order and installed/independent recovery were verified.
No reboot is armed. Installed current is `a1451825851dc5f0`; accepted desktop
`b427b78f21644bd2` remains previous. The query-refusal payload is independently
backed up. Accepted evidence and exact payload are hash-verified outside `build`
at `~/.local/share/srvros/accepted-evidence/a1451825851dc5f0`.

Full builds/core tests, both diagnostic kernels, 16 full-boot capture cases,
five regressions and exact-stage refusal/save/return plus missing-workspace
checks passed locally. All 16 disposable disks passed offline allocation/fsck.
The 190-file validation archive is at
`~/.local/share/srvros/local-validation/battery-native-smbus-10a4cbe30263`.
Actions remain disabled. Base was `24fd5f4` (PR45); Pro PR19 `79132d6` retains
independent Wi-Fi scope, with overlapping main/staging/tests to preserve.

This bounded milestone is complete. Next: test sustained event handling and
alarm/stop races with the existing Text/file workload in VM before proposing
another native coexistence capture. Ordinary startup and mounted Home stay off;
do not broaden the firmware/resource gates or infer physical alarm acceptance.

### Query10 host-controller event study (PR #45)

Base `origin/main` `40944e8` (PR44), implementation `1797e8a`. The explicit
VM-only command owns SMBus controller events instead of selecting a mutable
OSYS-dependent AML branch. Completion events do not write; alarms are captured
before one acknowledgement, with serialized sampling and cache invalidation for
known SBS senders. Uncertain protocol state retains terminal containment.
See the [event ownership contract](../battery-smbus-events.md).

All 13 focused Q35 cases pass, including both OS states, a later alarm invalidating
cached generation 2 before fresh generation 3, uncertain acknowledgement, native
command refusal and ordinary dispatch refusal. Both diagnostic builds, full core
host tests and proof-parser corruption checks pass locally. Three existing VM
regressions also pass: parked natural query refusal/save/reset, 60 generated Text
edits plus five file cycles with continuing samples, and notification-return
retirement. Actions stay disabled.
No physical enablement or installed/recovery payload changes are included.
The 63-file validation archive is hash-verified outside `build` at
`~/.local/share/srvros/local-validation/battery-smbus-1797e8afb8e1`.

Pro PR19 `9ecf749` retains independent Wi-Fi scope. Shared touched scope is the
battery runtime/model, EC SMBus helper, fixtures, tests and an appended Makefile
target. Native query10 still refuses; mounted Home and automatic startup stay off.
Next: an explicit parked native host-controller capture with strict saved event,
cache and retirement proof, exact-stage VM validation and recovery preserved.
The firmware ownership review must precede any native capability grant; VM
results do not authorize sibling profiles or general firmware bypass.

### Accepted parked native query-return capture (PR #44)

Release `a72409383258a4a0`, tested source `6f6381c`, session
`session-00065e65247fca49-00` passed the automatic physical v45 capture.
A natural query `0x10` arrived before the first sample and was refused before AML:
sequence 1, generation 0, completed transport, zero method calls/effects, idle
worker and no retained EC/SMBus/Global Lock resources. One SCI was handled with
an observed return and complete controller snapshots. The owner disconnected the
EC, restored foreign event enables and retired with heap/errors/denied all zero.
The service remained faulted with an empty, nonfresh cache, as required.

USB stayed parked during the runtime, then remounted for the final report.
All 916 prior ordinary files remained identical; only the metadata sidecar changed
and three matching session logs were added. Offline fsck and allocation checks
passed with zero leaks/mismatches/errors. The changed Ubuntu boot ID, consumed
one-shot, Ubuntu default, EFI order and installed/independent recovery were
verified. No further reboot is armed. Accepted evidence and the exact payload are
hash-verified outside `build` under
`~/.local/share/srvros/accepted-evidence/a72409383258a4a0`; recovery remains
`b427b78f21644bd2`, and the prior accepted controls payload remains backed up.

Full builds/core host tests, strict proof/collector/staging tests, all 14 new
full-boot VM cases and three existing workspace regressions passed locally.
The latter include normal Text/file coexistence, legacy notification return and
outside-scope refusal. Exact staged GRUB/native-profile refusal, save/unmount/reset
and missing-workspace return passed. Local validation is archived under
`~/.local/share/srvros/local-validation/battery-native-query-6f6381c5f0fc`.
A local collection helper's desktop-exit requirement was corrected: this capture
opens no desktop. The strict native query receipt required no relaxation.

This accepts only natural predispatch refusal and orderly failed-service
retirement with parked storage. It does not accept battery readings, ordinary
mounted Home, automatic startup, arbitrary AML recovery or another model.
Next: define query10 SMBus alert/notification ownership across both OS branches,
with VM evidence before another native expansion. No query injection/substitution
or widened firmware/register gate is authorized by this result.

Base `origin/main` was `e946f5e`. Touched: battery runtime, main capture dispatch,
staging/collector, VM fixtures, tests and guides. Pro PR19 at `43a1154` continues
independent Wi-Fi work but still uses v44 for calibration, colliding with merged
battery-controls. Integration must preserve historical exact recipes and use a
unique identity for new Wi-Fi calibration stages. Do not modify another device's
branch or infer its acceptance from this Air result.

### Native query admission policy (PR #43)

Base `origin/main` `961369b` (PR #42), tested code `33d4f9d`. The exact Air7,2
adapter now classifies EC queries before AML dispatch, after the existing
firmware/resource gate. Unknown codes and GPU/power query80 refuse before any
method executes. Query10 also refuses in both OS states: shared GNVS OSYS can
change between a Darwin check and AML's own read, and its other branch owns SMBus
alert/mutex state. No snapshot, OSYS shadow or forced protocol cleanup is used.

Query20/21 retain audited shared writes; 40/5a/cf are bounded notifications across
all branches; ce/d0 are empty. At PR43 native refusal remained terminal with live state
retained; PR44 now separately accepts the explicit parked capture above.
This is a dispatch restriction, not native recovery acceptance or a new IO grant.
Nine captured firmware table hashes, lengths and checksums were reverified.

Ten directed VM cases passed: unknown/high/OS-state refusals, both OS variants of
a five-method notification/no-op batch, shared20/shared21 effect refusals and
ordinary-command high/OS refusal. Positive batches require exact live/saved
policy/code/call/notification receipts, runtime retirement, automatic reset and
offline filesystem/metadata/allocation checks. Outside-study refusals show zero
interpreter errors/denied accesses, proving they stop before AML. Shared-write
batches halt without claiming retirement or saved reports.

The final 16-case matrix passed, including normal Text/file coexistence under the
Air policy (60 generated edits, 20 file-worker operations, five cycles and three
retired clients), legacy notification/effect/lock/refusal and I/O recovery checks.
Full kernel builds and core host tests passed, including 512 hazard combinations,
65,536 policy/code receipts and six parser tests; 418 documentation links passed.
Evidence is hash-verified outside `build` in
`~/.local/share/srvros/local-validation/battery-policy-33d4f9d`.

Touched: battery runtime/native+VM bindings, query receipt/classifier, fixtures,
host/VM proofs and documentation. Pro PR19 `4dfde33` is independent Wi-Fi work
(separate Makefile targets only). No physical install/reboot, accepted payload,
USB contents or recovery changed. Native mounted Home/startup remain off.
Next: scoped native normal-return admission and an automatic parked-workspace
capture with an explicit failed-service outcome, before ordinary Home acceptance.

### Returned-query failure study (PR #42)

Base `origin/main` `065546f` (PR #41), final code `cbd04bd`. The explicit VM-only
query study now rejects unsupported queries after completed QR_EC and before AML
starts. Whole-batch receipts track transport, normal method returns and attempted
shared writes. The worker returns normally; the owner drains notifications and
checks resources before ordinary disconnect, interpreter/worker retirement and
foreign-event restoration. The cache stays faulted and stale. This is not native
query policy, arbitrary AML recovery or external-state rollback.

All ten query fixtures passed locally: unknown code, a present high-code method
with a forbidden write, queued notification drain, earlier shared effect, real
Global Lock hold, poisoned transport, failed disconnect, native admission refusal
and both ordinary-command scope refusals. The first three retire cleanly, continue
the unrelated storage scope, save failure logs, unmount and reset automatically;
exact tree/metadata/allocation/fsck checks pass. The other fault paths halt without
claiming retirement or saved reports; native refusal leaves the image unchanged.

The first notification run exposed a scheduling gap after runtime close but before
final owner bookkeeping. An explicit finishing phase now keeps that owner eligible
through unrelated I/O until release, and clears before the next start. A forced
post-close yield makes this regression deterministic. The initial failure is
retained with the successful evidence outside `build`.

The complete final-source matrix passed all 15 cases, including I/O watchdog,
returned callback, interrupted worker and retained AML Global Lock regressions.
Normal Text completed 60 generated edits, 20 worker operations and five file cycles,
then retired three clients and passed offline filesystem/metadata/allocation checks.
All kernel builds, full core host tests, 512 query hazard combinations, five proof
parser tests and 415 documentation links passed. Evidence is hash-verified under
`~/.local/share/srvros/local-validation/battery-query-cbd04bdd8590`.

Touched: battery runtime/model fixture, pure query receipt, host/VM tests,
Makefile and battery documentation. Pro PR #19 at `faed5b7` has only independent
Makefile test additions in common. No native gate, installed payload, physical
trial or reboot changed. Native mounted Home and automatic startup remain off.
Next: define the exact captured-firmware/OS-state query policy, replay its refusal
boundaries in VM, then prepare an automatic parked-workspace native capture.

### Completed native AML side-effect audit

Base `origin/main` `d49ef02` (PR #40). The
[native side-effect map](../battery-native-aml-audit.md) covers the captured DSDT
and all eight admitted SSDTs: loading, two `_INI` methods, EC connect/disconnect,
all nine EC queries, notification delivery and status traversal. Raw tables match
the prior SHA-256 manifest, lengths, checksums and every native fingerprint.
Per-table iASL runs with peer externals completed; the unresolved guarded HDOS/HNOT
references and failed multi-target disassembly are recorded as evidence limits.

Startup writes shared OS/lid/AC state, graphics CLID, ECSS and two GPIO bits;
disconnect does not roll those back. Query 80 reaches denied GPU/chipset controls.
The next experiment is a structured query-admission failure after a completed
QR_EC exchange, before unapproved AML begins, returning normally to the owner.
It needs operation/effect receipts, drained callbacks, existing resource guards
and proved orderly retirement in VM before a parked-workspace native capture.
Partial initialization, shared writes, poisoned protocols and arbitrary AML faults
remain outside that recovery scope. Native mounted Home and automatic startup
remain off.

Existing native-memory, recovery-resource, I/O-mask, EC and SMBus host checks
passed; documentation links passed. This was a static/host audit, with no new VM
or physical acceptance claim. Evidence is archived with verified hashes outside
`build`. Pro PR #19 at `e871a1e8` remains independent Wi-Fi work with no edited-file
overlap. No installed payload, USB contents, recovery or reboot state changed.

### Resource audit and recovery admission (PR #40)

Base `origin/main` `3c98c42` (PR #39), tested code `2cc02b2`. The
[resource audit](../battery-resource-audit.md) distinguishes thread retention from
firmware Global Lock ownership and unfinished EC/SMBus protocols. All three VM
quarantine boundaries now require an idle external-resource snapshot. `_GLK=0`
and a clear public API ownership flag are insufficient: AML ownership/depth,
the underlying semaphore, firmware pending state and FACS bits are checked too.
An active lease, partial/poisoned EC, active sampler or incomplete SMBus transaction
refuses recovery. No lock is released or protocol reset by this check.

Ten disposable VM cases passed. Actual public-API and AML `Acquire(\_GL)` holds
both refused with FACS Owned set, an occupied semaphore and blocker mask 96.
The AML case had API ownership clear, demonstrating why that flag alone is not
sufficient. A pause after EC command submission refused with lease=1, phase=address
and blocker mask 6. These are expected halts without quarantine, clean-storage or
automatic-reset claims. An initial AML fixture used a sleep longer than the
interpreter's two-second cap and correctly failed; the final fixture uses repeated
bounded sleeps across the watchdog. Failed evidence is preserved separately.

Watchdog/I/O, returned-callback, IRQ-entry and both interrupted-target recovery
passed with zero blockers, retained-state proofs, both storage flushes, saved logs,
unmount, automatic reset and offline filesystem checks. Native study admission
still refused before platform access with the disposable volume unchanged. Normal
Text passed 60 edits, 20 worker operations, five file cycles and clean retirement.
All kernel builds, full core host tests, 2,048 resource-policy combinations,
21 proof-parser tests and documentation links passed. Evidence is archived outside
`build` with verified hashes; Actions remain disabled.

No physical reboot, installed payload or firmware gate changed. Pro PR #19 at
`67f62a4` shares only independent Makefile test additions, with no battery/runtime
overlap. Next: map reachable native AML/NVS/SMI side effects and define which fault
boundaries can reach resource-idle state through normal returns. A clear snapshot
is not a firmware ownership lease or native recovery acceptance. Native mounted
Home and automatic battery startup remain off.

### Completed IRQ handoff (PR #39)

Base `origin/main` `a1b9802` (PR #38), tested code `6c23fbd`. A token-bound
terminal IRQ hook now runs after EOI, every completion callback and registry
bookkeeping. The explicit VM battery study can retain an interrupted owner or
worker without returning to that thread, then resume unrelated storage and save,
unmount and reset. Touched: IRQ registry/dispatcher, scheduler contract, battery
runtime/model, host/VM proofs and documentation. Pro PR #19 at `426f0cd` remains
independent Wi-Fi work with no touched-file overlap. No hardware run or installed
payload change occurred; native startup and mounted Home remain off.

### Local terminal handoff results

All ten disposable VM cases passed. Both new target cases balanced 12 callback
spinlock acquisitions with no pending query. Worker 9 or owner 8 was interrupted;
all shared handlers/completions finished before retaining the pair, and storage
thread 7 resumed with its original scope and successful registry removal. Both
flushes completed across a 28-second scope. Generation 2, completion 10 and 9,411
bytes of interpreter heap remained held across wake attempts, sleep expiry and a
late SCI. Three clients retired; saved failure logs matched serial, followed by
unmount, automatic reset and clean exact-tree/metadata/allocation/fsck checks.
The retained runtime is deliberately not reported as cleanly retired. No target
resumed after the fault. Zero Text saves in these fixtures are not an input
responsiveness or pending-DMA claim.

Native study admission refused before platform access, leaving the disposable
volume byte-identical. The older callback scope still halted for target
interruption, unmatched callback locks and out-of-scope query refusal. Previous
returned-callback, IRQ-entry and watchdog/I/O recovery passed. Normal Text passed
60 edits, 20 worker operations, five file cycles and clean service retirement.
All ordinary/native/VM builds, the full core host suite, 20 proof-parser tests and
397 documentation links passed. Host IRQ tests prove token/request exclusions,
EOI-before-completions-before-handoff ordering and fully released registry state.
Evidence with verified hashes is retained outside `build`. Actions remain disabled.

Next boundary: audit native firmware/EC/shared-resource ownership and the still
terminal callback failures before enabling mounted Home with the battery service.
This completes the interrupted-target VM milestone; it does not accept arbitrary
interpreter faults, physical level-triggered SCI recovery or outstanding DMA.

### Returned GPE callback failures (PR #38)

Base `origin/main` `a35cd5e` (PR #37). Add VM-only structured propagation for
EC query-admission failures from the raw GPE callback, allowing the actual uACPI
SCI callback to return before retaining noncurrent battery threads. Check balanced
callback spinlock calls, no queued query, IRQ completion and unrelated storage
continuation. No nonlocal unwind or arbitrary exception recovery. Native startup
and mounted Home remain off. Touched: battery runtime/model, disposable VM harness
and proof-parser tests; no scheduler or upstream interpreter changes planned.
Pro PR #19 at `10caff8` remains separate Wi-Fi work. No hardware run is pending.

### Local callback recovery results

Code `14d4b39` passed nine disposable VM cases. The modeled enqueue refusal ran
through the real uACPI SCI/GPE/raw-handler call chain and returned normally with
12 callback spinlock acquisitions balanced and no pending query work item. The
interrupted storage thread 7 continued after battery threads 8/9 were retained;
secondary handler, post-EOI completion, registry removal and both flushes passed.
Generation 2, worker completion 10 and 9,411 bytes of interpreter heap remained
held across sleep expiry, wake attempts and a late SCI. Three clients retired;
saved failure logs matched serial, followed by unmount, automatic reset and clean
exact-tree/metadata/allocation checks. Zero Text saves in this fixture are not
an input responsiveness claim.

Interrupting the battery worker, leaving a modeled callback spinlock unmatched,
and triggering the fault outside the explicit callback scope each halted without
false storage retirement. Native admission refused before firmware access with
the disposable disk unchanged. Previous IRQ-entry recovery/refusal and watchdog
recovery during a storage scope passed. Normal Text completed 60 edits, 20 worker
operations and five file cycles with generations 1→25 and clean retirement.
Ordinary/native/VM builds, full core host tests, 19 proof-parser tests and 397
local documentation links passed. Evidence is retained outside `build` with
verified hashes. Actions remain disabled; no physical payload or reboot changed.

The new boundary covers enqueue refusal after successful GPE suspend/clear and a
normal interpreter return. Other callback failures still halt; PR #39 above covers
an explicit VM scope for IRQs interrupting a battery thread. Shared native
firmware/resource ownership and physical SCI/DMA recovery remain unaccepted; native mounted Home and automatic startup stay off.

### IRQ entry fault containment (PR #37)

Base `origin/main` `07dfa77` (merged PR #36). Add a VM-only recovery boundary
for the foreign-event guard at SCI callback entry when both battery threads are
noncurrent. Preserve the interrupted task, return through dispatcher/EOI and
completion callbacks, and verify an unrelated storage owner can finish before
saving failure logs/unmounting. Deep interpreter callback faults and interrupts
of a battery thread remain terminal; no stack unwinding is implied. Touched:
scheduler retention API, battery runtime/model, host proofs and disposable VM
harness. Pro PR #19 at `6fb3af5` remains independent USB Wi-Fi work. No physical
trial, installed payload change or native admission expansion is planned.

### Local IRQ entry recovery results

Code `b2bc9c1` passed all seven disposable workspace cases. The injected SCI
returned to storage thread 7 after retaining battery threads 8/9. The next handler
and post-EOI completion ran once; registry removal succeeded, callback in-flight
count returned to zero, and the original stack sentinel and I/O ownership survived.
Both flushes completed across the 28-second scope. Three clients retired, the
9,379-byte interpreter heap and generation 2/completion 10 stayed retained, and
saved failure logs matched serial evidence. Unmount, automatic reset and offline
exact-tree/metadata/allocation checks passed. The IRQ fixture completed zero Text
saves and is not an input latency or pending-DMA acceptance test.

Both unsafe recovery cases halted as required: interrupting a battery worker and
refusing the source-mask write. Neither claimed storage retirement. Native backend
admission was refused before firmware access with the disposable disk unchanged.
Previous watchdog recovery with/without a separate I/O owner passed. Normal
mounted Text passed 60 generated/presented edits, 20 worker operations, five file
cycles, generations 1→25, three client retirements and clean service teardown.
Full ordinary/native/VM builds, core host tests, 18 proof-parser tests and 397
local documentation links passed. Actions remain disabled. Evidence is archived
outside `build` with hashes; no laptop reboot or installed-payload change occurred.

This completes the bounded IRQ entry guard. Deep interpreter callback faults,
IRQs interrupting battery threads, native firmware/shared-resource ownership and
physical level-triggered SCI recovery remain open. Native mounted Home and
ordinary automatic battery startup stay off.

### Owner-watchdog quarantine and workspace recovery (PR #36)

Base `origin/main` `49ded19` (PR #35). Add a CPU0 kernel-service quarantine
primitive that retains exact identities/stacks, excludes wakeup and reuse, and
refuses to quarantine a storage owner. Exercise only the battery owner's sample
deadline outside IRQ context in the disposable VM backend, followed by client
retirement, saved failure evidence, workspace drain/unmount and automatic reset.
Native enablement and IRQ-origin faults remain excluded. Touched: scheduler,
battery runtime/VM fixtures, recovery harness and host tests. Pro PR #19
`b884060` remains independent Wi-Fi work; no subsystem overlap beyond Makefile.
No physical run is pending; installed/recovery payloads remain unchanged.

### Local owner-watchdog recovery results

Code `ff8ddb5` passed both disposable VM recovery cases. Each retained two service
threads and 9,379 bytes of interpreter heap, left generation 2 and worker completion
10 unchanged across the old sleep expiry, queue wakeups and a late SCI dispatch,
and refused restart/stop. Three clients retired before saving boot/exit failure
logs, normal unmount and automatic reset. Saved logs match serial evidence; the
exact file tree, metadata, anchors and offline allocation checks pass. The normal
recovery case retained three completed Text files. The I/O-owner case completed
both flushes across a 28-second storage scope and had zero Text saves; it is an
ownership/recovery proof, not a responsiveness or outstanding-DMA claim.

The negative fixture gives the modeled battery worker a storage scope; quarantine
refuses it and halts without a scheduling window or reset. No storage retirement
is claimed in that case. Native recovery admission is refused before firmware access, with the whole
disposable disk unchanged. The previous mounted-runtime fault still halts when
the explicit recovery study is absent. Normal mounted Text and two repeated
scoped-control sessions still retire cleanly. Full builds/core host checks,
malformed-proof rejection and 397 documentation links pass locally; Actions OFF.
Full core/normal regressions ran on `3c98771`; the final terminal-refusal change
was rebuilt and checked with focused host tests and all three recovery fixtures.
The scheduler tests cover stale/user/dead/idle/storage-owner refusal, atomicity,
wait unlink, late wake/expiry, and no slot or stack reuse, including slot exhaustion.

No installed or recovery payload changed; no physical test is armed. Native Home
remains disabled. Next is fault handling inside IRQ callbacks and a shared
lock/resource audit before any native recovery trial. IRQ-origin faults must not
abandon an interrupted storage owner's stack or skip IRQ completion accounting.
Ordinary startup remains off; these results do not accept native EC/SCI recovery.

### Current scope: mounted workspace coexistence and reset admission (PR #35)

Base `origin/main` `7d8c58d` (merged PR #34). First add a checked reset policy:
arming while parked must not authorize a later immediate reset with ordinary
workspace writes live. Exercise the mounted `/work` application-file path in a
disposable VM while retaining existing model/resource and I/O eligibility gates.
Native mounted-workspace trials stay disabled until failure handling can preserve
storage ownership and return safely. Automatic battery startup remains off.
Touched scope: battery reset policy/runtime, opt-in application-file workload and
VM/host tests; workspace recovery design. Pro PR #19 at `bb0284b` remains separate
Wi-Fi firmware work. No physical run is pending or armed.

### Mounted workspace VM study (PR #35)

Code `207a52a` adds a fatal reset interlock and a VM-only mounted `/work` study.
The normal Q35 run passed 60 Text edits, 20 file-worker operations (five
save/stat/rename/reopen cycles), battery generations 1→25 and 60 cached UI reads.
All three clients and the service retired, a replacement compositor succeeded,
and the workspace unmounted normally. Offline checks found the exact expected
tree and metadata, untouched anchors, nine files, 15 allocated clusters and no
leaks or allocation mismatches. These are VM results, not physical input latency
or gauge-accuracy acceptance.

Early desktop timeout retired clients/service and left the entire disk unchanged.
The native backend refused the mounted-study command before firmware access and
also left the disk unchanged. Injected runtime timeout with `/work` mounted
blocked reset and retained live state; it did not claim clean storage retirement.
The parked controls timeout still reset automatically. The prior exclusive-image
Text workload passed unchanged. Full ordinary/native/VM builds, core host tests,
malformed-proof rejection and 397 documentation links passed locally; Actions
remain disabled. Fault regressions retain isolated-image recovery checks and
accept a halt only with explicit current I/O-ownership evidence.

No installed payload or recovery changed and no reboot is armed. Automatic
battery startup remains off. Native mounted Home remains disabled until failed
ACPI owner/worker and IRQ callbacks can be contained without stack reuse or
wake-up resurrection, followed by safe storage drain, logging and automatic
return. The next block is that recovery mechanism and its injected-fault tests;
this VM study does not authorize skipping it.

### Native scoped controls checkpoint (PR #34)

Release `87524e9fe7788772`, source `134f0d6`, session
`session-00065e5f9bef533b-00` completed the automatic v44 trial and returned to
Ubuntu. System admitted and dispatched all four commands, reached fresh sample
generation 2 in each of two lifetimes, and showed both stopped states. Both service
lifetimes detached their handler with zero remaining runtime heap, errors or denied
operations. Two clients retired and control admission was revoked. The desktop
made five fresh cached reads. SCI handled four deliveries, with none unclaimed.

All 913 prior data files were byte-identical, including the old filesystem image.
Only workspace metadata changed and exactly three matching logs were added.
Offline checks passed: 196 directories, 917 files, 1,700 allocated clusters, no
leaks or mismatches. The new Ubuntu boot, consumed one-shot, default/EFI order and
installed/independent recovery payloads were verified. Evidence and payload hashes
were verified outside `build`; current `87524e9fe7788772`, desktop recovery
`b427b78f21644bd2`, and former accepted `27c39e5404ab573b` remain recoverable.

This accepts the automatic native System action/syscall/lifecycle path with the
outer workspace parked. Actual pointer controls and active-window close passed in
Q35; attended physical clicking, input latency, ordinary Home coexistence and gauge
accuracy remain separate acceptance work. Automatic battery startup stays disabled.

### Scoped control implementation and local validation (PR #34)

System Sensors now has shared Start/Stop presentation backed by a separate bounded
command syscall and an owner-scoped mailbox. Four requests drive two service
lifetimes; pending requests, stale epochs and invalid transitions are rejected.
Client exit revokes admission before owner-controlled service retirement. The
ordinary kernel offers no control scope and automatic startup stays disabled.

Local Q35 checks pass two complete automatic sessions in the same VM, actual
pointer Start/Stop/restart and window-close while active, forced compositor exit,
native platform refusal, and injected live-runtime deadline with automatic reset.
A replacement compositor succeeds after every clean case. Full host core and
ordinary/native/VM builds pass. The previous Text file-worker workload and ordinary
desktop applications also pass, alongside staging/collector and documentation
checks. The new v44 unattended capture parks the outer
workspace and saves completion logs only after retirement; it neither opens nor
rewrites the prior filesystem image. Exact-stage refusal/save/return rehearsal and
the native trial passed; see the bounded acceptance above.

Two exploratory fixture problems were retained and corrected: the automatic
study originally stopped before the injected third-sample fault, and concurrent
UART output split a short request record. Fault testing now leaves monitoring
active. Completed VM cases validate the atomic in-memory boot-log records, the
same source saved by hardware captures; strict proof checks were not relaxed.
The initial manual fixture also now waits for scope availability and its first
frame. Ordinary workspace coexistence and attended input/AC accuracy remain open.

### Previous checkpoint: application files and cached battery presentation accepted

Release `27c39e5404ab573b`, source `977a554` (PR #33), session
`session-00065e5ed9440af5-00` passed the automatic v43 hardware trial and returned
to Ubuntu. Text presented all **60 generated edits** and completed five real
save/stat/guarded-rename/reopen cycles (20 worker operations). All five files
contained `AA`; normal Open left the final caret/anchor at zero. No extra keyboard
events occurred. The compositor, Text and file worker retired before the image
lease was released, followed by clean service teardown.

Battery generation advanced **1→23**, with 61 fresh cached UI reads. The desktop
recorded 81 frames, 5,306 loops and a maximum loop gap of 30,013 microseconds;
this is not physical input latency. SCI recorded **23 handled / zero unclaimed**.
The image's exact file contents and metadata passed, with eight files, 14 allocated
clusters and no leaks. All 909 other prior files remained unchanged; the intended
image and workspace metadata changed and exactly three matching logs were added.
Outer offline checks passed: 195 directories, 914 files, 1,696 allocated clusters,
zero leaks or allocation mismatches. The new Ubuntu boot, consumed one-shot,
default/EFI order and recovery payload were verified. Evidence and the exact
accepted payload are backed up outside `build`.

Collection initially encountered a missing expected path in an older session's
now-replaced disposable image. The host collector now converts that lookup failure
into invalid evidence for that session and continues. The matching v43 session has
no attention items. The collector regression passes; this host-only correction
does not change the physically tested kernel or userspace payload.

This completes the bounded application-file and cached-presentation checkpoint.
Explicit user start/stop, ordinary workspace coexistence and attended input/AC
accuracy remain pending; automatic battery startup stays disabled. Installed
`27c39e5404ab573b`, prior accepted `c3746988ed5c0d5b` backed up, desktop recovery
`b427b78f21644bd2` retained. No further reboot is armed; the used image is not fresh.
Pro PR #19 at `c263221` retains its independent Wi-Fi scope. Actions remain disabled.

### Application file/cached battery checkpoint (PR #33)

Opt-in v43 `--battery-app-files` exercises Text's normal asynchronous file worker:
60 generated edits, five saves/stats/guarded renames/reopens into distinct files,
and final `AA` with caret/anchor zero after normal Open. The owner retains all
three process identities through retirement before revoking the exclusive image.
The outer workspace stays parked. A versioned read-only battery syscall supplies
the shared desktop label and System Sensors row; UI reads never start EC/AML work.
Explicit lifecycle controls and ordinary workspace coexistence are still pending.

The final automatic and injected VM runs passed all 20 worker operations, exact
file contents and metadata, unchanged bytes outside the image, offline allocation
and a replacement compositor. The injected run acknowledged all 44 key events.
Missing-program, deadline, early-exit, platform-refusal and active-runtime-fault
fixtures passed their expected cleanup/containment checks. These are VM results,
not physical input latency or native v43 acceptance. Automatic startup stays off.

Exploratory failures are retained locally: stale ISO userspace (the harness now
uses the explicit matching initramfs), a fixture's incorrect post-Open caret
expectation, and a checker that omitted normal `.srvros/meta`. The final checker
requires exact sidecar paths, unique inode identities, modes and link counts;
it does not ignore extra files. Existing retirement deadlines remain unchanged.

Final local validation also passed the previous combined-load VM (78 filesystem
cycles), ordinary desktop application regression, all/native/VM builds, full host
core, final System test, 46 staging tests, 15 collector tests and 13 strict record/
metadata tests. Desktop screenshots were inspected for the active and disabled
battery states. v43 exact-stage rehearsal and native acceptance follow separately.
Actions remain disabled; all validation is local.

### Previous accepted checkpoint: combined Text/filesystem/battery milestone

Release `c3746988ed5c0d5b`, source `421a69b` (merged PR #31), session
`session-00065e5e243ecee0-00` passed the automatic v42 hardware trial and returned
to Ubuntu. The editor presented all **60 generated operations**, five exact `AA`
checkpoints and final `AA` with caret/anchor at byte two. No extra keyboard events
occurred. Text used a RAM document with its file worker disabled; the separate
filesystem workload held an exclusive lease on the disposable 16 MiB image.

The common desktop/filesystem interval covered **60.017 seconds**. The compositor
recorded 66 frames, 5,323 loops and a maximum loop gap of 29,998 microseconds.
Filesystem work completed **45 cycles / 810 operations / 18,120 USB commands**,
with 21 sampling-busy cycles and 18 bursts containing sampling progress. The
slowest filesystem operation took 4,551,918 microseconds including admitted
service work; that wall time is not input latency. Battery generation advanced
1→25; 811 cached reads included 38 during sampling. App, service and image lease
retired cleanly with balanced attachment and zero rejected samples, heap,
errors or denied operations. SCI recorded **25 handled / zero unclaimed**.

Strict collection found one matching new session with no attention items. Exact
image contents, inner exFAT and allocation checks passed (five files, 75 clusters).
All 906 other prior data files were unchanged; only the intended image and
workspace metadata changed, with exactly three matching logs added. Outer offline
checks passed: 194 directories, 911 files, 1,692 clusters, zero leaks or allocation
mismatches. A new Ubuntu boot, consumed one-shot, default/EFI order and recovery
payload hashes were verified. Evidence, pre/post images and the exact accepted
payload are archived outside `build`.

This completes the bounded combined-load milestone on this Air. Physical input
latency, normal application filesystem access with the service running, gauge
accuracy and automatic battery startup remain separate work. Ordinary desktop
battery startup stays disabled. Current installed diagnostic is
`c3746988ed5c0d5b`; accepted desktop recovery `b427b78f21644bd2` remains previous.
No further reboot is armed. The used test image must not be replayed as fresh.

This closeout changes documentation only, based on actual `origin/main` at
`421a69b`. Pro PR #19 inspected at `39c25ce` retains its independent v39 Wi-Fi
scope; no sibling hardware gate or acceptance is changed. Actions remain disabled;
local validation and native evidence are the acceptance basis.

### Combined Text/filesystem/battery: implemented and VM validated

PR #31 starts at `8211ec1` (PR #30); implementation `72fb430`, compositor cleanup
fix `0f22d3e`. The bounded owner interleaves one operation from the existing
exclusive filesystem-image workload with cached service observation and process
polling. Opt-in v42 `--battery-load` adds exact-stage and strict collection proof.
No scheduler eligibility, filesystem format, desktop default or native model gate
changed. Pro PR #19 inspected at `e9320d9` retains its independent v39 Wi-Fi scope.

All eight loaded Text VM cases passed: automatic script, injected keyboard,
missing program, deadline, early exit, platform refusal, partial-cycle owner
abort and active runtime fault/reset. Standalone RAM Text also passed. Both normal
loaded runs presented all 60 edits and five exact `AA` checkpoints, completed
78 filesystem cycles / 1,404 operations while sampling advanced 1→25, and covered
over 60 seconds of common UI/filesystem interval. The injected run delivered all
44 keyboard events and ended with `ZbZb`. Offline contents/allocation and every
byte outside the image extent passed; clean-return cases started a replacement
compositor. The abort fixture is owner cancellation, not a transport fault.

The first injected run failed compositor shutdown accounting despite correct
editing and a successful Text-app exit report. Final cleanup now checks for and
records an already-completed child before forcing termination; deadlines remain
unchanged. That failed evidence is retained separately. Its raw VM disk was not
saved by the old harness; future assertion failures now retain the disposable
raw disk as well. The final matrix passed after the fix.

Native/VM/ordinary builds, full host core checks, 45 staging tests, 14 collector
tests, nine strict overlap-record tests, ordinary desktop-app regression and
394 documentation links passed. Binaries and validation evidence are archived
outside `build`. The exact-stage native Q35 refusal/save/unmount/return rehearsal
passed, followed by the native acceptance recorded above. Fresh fixture preparation
archived the used image, preserved other files and verified outer allocation and
the exclusive extent offline. The earlier v41 diagnostic remains independently
backed up; accepted desktop recovery is retained.

### Previous checkpoint: native full Text/battery RAM overlap accepted

Release `2fd49386c4ecf0d9`, source `01e64a9` (merged PR #29), session
`session-00065e5d96dd1b73-00` completed the automatic v41 Text trial and returned
to Ubuntu. The real editor presented all **60 generated operations**, all five
`AA` editing checkpoints and final `AA`, with caret/anchor at byte two and no
clipboard operation pending. No extra keyboard events occurred. Its RAM-only
file worker remained disabled. The capture ran 60.018 seconds, with 66 frames,
5,321 loops and a maximum loop gap of 30,015 microseconds; this is not measured
physical input latency.

Battery sampling advanced 1→23; 3,007 cached reads included 706 during sampling.
The compositor exited successfully and the service retired with one attachment
and detachment, zero rejected samples, heap, errors or denied operations. SCI
counters recorded **23 handled / zero unclaimed** interrupts. Strict collection
found one matching new session and no attention items.

All 904 prior data files, including the used filesystem-test image, were unchanged.
Only workspace metadata changed and three matching logs were added. Offline
exFAT and allocation checks passed: 193 directories, 908 files, 1,688 clusters,
no leaks or mismatches. A new Ubuntu boot, consumed one-shot, default/EFI order,
installed payload and accepted desktop recovery were verified. Evidence and exact
payload are archived outside `build`; the flash drive is remounted in Ubuntu.

At that checkpoint installed was this diagnostic; previous remained accepted desktop
`b427b78f21644bd2`. This accepted generated full Text editing
with concurrent cached battery sampling in RAM. Simultaneous filesystem load
was the next scope and is now accepted by the v42 trial above. Physical input
latency and gauge accuracy remain separate acceptance. Ordinary desktop battery
startup remains disabled. Future trials must preserve the exclusive image lease
and bounded process/service retirement; do not reuse the used image as fresh.

### Native full Text capture: implementation and local validation

Starting from merged PR #28 (`529eee4`), add opt-in v41 `--battery-text`
selection, exact-stage rehearsal and strict native collection. Shared edits are
limited to the battery diagnostic entry point, boot selection/staging, collectors
and their tests; editor/compositor and native hardware gates stay as validated.
Pro PR #19 inspected at `e9320d9` reserves v39 Wi-Fi; preserve that independent
scope. The automatic trial needs no input: require 60 presented edits, five `AA`
checkpoints, clean app/service retirement and continued sampling. Rehearse the
exact native payload's unsupported-platform save/unmount/return before install.
Implementation `98bebc9` passed native/VM builds, full host core tests, 44 staging
and 13 collector tests, seven strict overlap-record tests and 393 documentation
links. The rebuilt automatic Text VM presented all 60 events, retained all five
`AA` checkpoints and final `AA`, with zero extra keyboard input. Sampling advanced
1→25 and a replacement compositor started after clean retirement. Eleven files
including exact binaries are archived outside `build`. PR #29 carries this work.
The exact-stage native Q35 refusal/save/unmount/return rehearsal passed; native
acceptance is recorded above. Actions remain disabled.

### Previous checkpoint: native RAM typing-study/battery overlap accepted

Release `ae2851607a9bf0c1`, source `1df9a24` (merged PR #26), session
`session-00065e5c8e37f13c-00` completed the automatic v40 trial and returned to
Ubuntu. The typing-study compositor ran for 60.019 seconds, generated and
presented all **60 changes**, and completed 61 frames across 5,314 loops.
Maximum loop gap was 30,009 microseconds; this includes scheduling and is not
measured physical input latency. Battery sampling advanced generation 1→23;
3,004 cached reads included 707 while sampling. The compositor exited with
status zero and the service retired with balanced attachment and zero rejected
samples, heap, errors or denied operations. Full SCI counters recorded
**23 handled / zero unclaimed** interrupts.

The collector found one matching session, complete overlap proof and no attention
items. All 901 prior data files, including the used filesystem-test image, were
byte-identical. Only workspace metadata changed, with three new matching logs.
Offline exFAT and allocation checks passed: 192 directories, 905 files, 1,684
clusters, no leaks or mismatches. A new Ubuntu boot, consumed one-shot, default
and EFI order, installed payload and accepted desktop recovery hashes verified.
Evidence and the exact payload are archived outside `build`.

Current installed is this diagnostic; previous remains accepted desktop
`b427b78f21644bd2`. No reboot is armed. This accepts the bounded RAM typing-study
and generated-event overlap, not physical typing/gesture latency, full Text-app
keyboard events, filesystem-load overlap or gauge accuracy. Ordinary desktop
battery startup remains disabled. Next validate the rich Text keyboard-event
path in RAM, then combine the isolated disposable filesystem workload; retain
separate attended input acceptance.

### Full Text input in RAM: implemented and VM validated

PR #28 builds on `3b20bfb`; runtime/compositor/Text implementation `9ee553a`.
The new monitor-only `battery-desktop-text` mode runs the real Text editor with
its file worker disabled. It retains normal rich keyboard events, editing,
clipboard IPC and surface publication. The existing v40 study profile and the
installed/accepted native payload above remain unchanged; ordinary desktop
battery startup stays off. Pro PR #19 remains `e9320d9`, with no shared-file
conflict in this change.

Seven Text VM cases passed: automatic script, injected keyboard editing, missing
program, owner deadline, early exit, unsupported platform and active-runtime
fault/reset. Both positive cases presented all 60 generated operations and five
exact `AA` editing checkpoints while sampling advanced generation 1→25. The
injected case delivered 44 keyboard events, acknowledged the final serial, and
ended with visible `ZbZb`; ordered events may share frames. Every clean-return
case subsequently started a replacement compositor. The old study, full host
core suite, strict record/corruption tests and ordinary desktop-app regression
also passed. No guest timeout or native hardware gate changed.

The initial injected VM lacked final serial records while its document was
visible. Its failed evidence is retained; draining serial between input bursts
resolved the repeat. Refreshing the older apps harness also corrected stale
folder-field coordinates, implicit `/fat` defaults after the move to Home,
and maximized restore coordinates. Screenshots/raw disposable disks established
the path mismatch; a split child exit log was replaced by checking the parent's
status for that exact PID. The refreshed suite verifies actual file contents,
clipboard editing and maximize/restore transitions. All failed attempts remain
separate from final validation.

The separate native v41 recipe and strict collection landed in PR #29 and are
accepted above. Concurrent isolated filesystem load, attended input latency and
battery gauge accuracy remain pending. No physical trial is armed.

### Previous checkpoint: EC GPE rearm ordering accepted for the bounded trial

Release `54dce7fb0a41b718`, source `b7bd60c` (merged PR #24), session
`session-00065e5b4cd48eb4-00`, returned automatically. Full retired counters
recorded **24 handled / zero unclaimed** interrupts across two lifetimes,
compared with 32 handled / 32 unclaimed in the preceding query-context run.
Nine callbacks were retained and fifteen overwritten. All retained entries
observed an idle query worker; all post-EOI samples had ISR/IRR and remote-IRR
clear (LAPIC bits 4, route low `0x8029`). The pre-resume pending check eliminated
the observed duplicate-delivery pattern in this trial. This is bounded native
acceptance, not a guarantee against late arrivals or all future unclaimed IRQs.
Claims, EOI ordering, route polarity and hardware gates remain unchanged.

The isolated filesystem completed 42 cycles / 756 operations in 61,319,529
microseconds with 16,910 USB commands. Maximum operation time was 3,157,927
microseconds; this is wall time, not measured input latency. Generation 23 and
61,290 cached reads (maximum 18 microseconds), including 2,194 during sampling,
were recorded. Both lifetimes retired with restored enables, balanced attachments
and zero heap/errors/denials. The deliberate short stop wait timed out, then
retirement completed normally.

Exact inner contents/fsck/allocation (75 clusters), outer fsck/allocation (191
directories, 902 files, 1,680 clusters) and preservation of 897 unrelated files
passed. Only the disposable image/root metadata changed, with three new matching
logs. Ubuntu return, consumed one-shot, default/EFI order and recovery hashes
verified. Twenty-eight hashed evidence files and the exact payload are archived
outside `build`. Current installed is this diagnostic; accepted desktop
`b427b78f21644bd2` remains previous. No reboot is armed. The image is used and
must not rerun. Ordinary desktop battery startup remains disabled; this trial
has no physical input or gauge-accuracy acceptance.

### Implementation and local validation: RAM desktop and battery overlap

Branch `device/macbookair7-2/sci-controller`, PR #26, integration base `4daf2c2`.
Runtime `aaab764`, bulk unmap fix `054b6cf` and harness `39aa9fa` implement an
opt-in v40 `--battery-desktop` laptop probe. Ordinary desktop battery startup
remains disabled. The workspace is parked during the RAM typing-study capture;
Home, preferences, screenshots and storage load are excluded. The used filesystem
fixture is left unchanged. Pro PR #19 (`e9320d9`) retains v39 for its Wi-Fi probe;
its hardware profiles and xHCI scope are unchanged.

The independent runtime owner starts active cached sampling, observes the desktop
for one minute, checks process exit and sampling progress, then retires the
service before remounting storage and saving logs. Generated font-toggle input
requires a matching client serial and successful compositor presentation. The
study uses legacy text input, not the rich Text-app keyboard-event path. Startup
proof is printed after retirement to avoid concurrent console-writer interleaving;
its saved timestamp still identifies actual start. Maximum compositor loop gap
includes client startup and is not physical input latency.

The first VM presented all 60 events and typed `Ab`, then hit the existing EC
sample deadline during compositor cache cleanup. This failed evidence is retained.
`process_munmap` performed two linear mapping-table searches per page. Single-pass
range removal preserves full validation and frame ownership, and the repeat VM
completed cleanly. Bulk/split/unrelated-mapping regression coverage was added to
posixdemo. EC deadlines, IRQ claims, EOI ordering and native hardware gates have
not changed.

Final normal, missing-program, deadline, early-exit and unsupported-platform VM
cases passed, each followed by successful replacement compositor startup. Normal
captured generation 1→25, 60 generated/presented changes, 66 frames, and injected
`Abx` followed by Backspace produced `Ab`. Full local `make smoke` and the
software SCI regression passed. Record tests reject missing, conflicting and
non-overlapping proof. The active-runtime deadline fixture also returned automatically without unsafe
cleanup or a false success record. All six cases and exact binaries are archived
outside `build` under `local-validation/coexist-39aa9fa`. Exact native stage
rehearsal is recorded separately before installation. GitHub Actions
remain disabled. The native v40 result above accepts bounded RAM overlap; none
of this accepts physical input latency, gauge accuracy or loaded filesystem coexistence.

### Previous native query correlation accepted

Release `df04f96a16d0f850`, source `68aac71` (PR #23), session
`session-00065e5b05b8b6c9-00`, returned automatically after 45 cycles / 810
operations in 60,749,550 microseconds (18,116 USB commands; maximum operation
2,439,655 microseconds). Both lifetimes retired cleanly with restored enables,
balanced attachments and zero heap/errors/denials. The service reached generation
23; 64,685 cached reads completed, maximum 3 microseconds, including 2,263 during
sampling. The intentional short stop-wait timed out before clean retirement.

Counters recorded 32 handled / 32 unclaimed callbacks; ten were retained and
54 overwritten. All five retained unclaimed entries found a **new queued batch**
aged 5–6 microseconds, with zero transfers and no known response code. They
followed handled callbacks by 558,139–717,617 microseconds; selected EC event
status/enable was `0xa0`/zero and fixed status/enable `0x8001`/`0x20`. Each retained
post-EOI snapshot again had ISR/IRR clear and remote-IRR clear, with LAPIC bits 4
and route low `0x8029`. The query worker was not executing a transfer or AML at
these entry points. Source inspection finds GPE resume **before** the pending
check that can suspend/clear it and queue this new batch. That ordering is a
candidate cause of a latched delivery whose event is gone by callback entry;
the correlation alone does not prove the cause or justify claiming that IRQ.

Inner exact contents/fsck/allocation (75 clusters), outer fsck/allocation (190
directories, 899 files, 1,676 clusters) and preservation of 894 previous files
passed. Only the disposable image/root metadata changed, with three new logs.
New Ubuntu boot, consumed one-shot, default/EFI order and recovery hashes passed.
Twenty-nine hashed evidence files and the exact payload are archived outside
`build`. USB remounted; previous desktop `b427b78f21644bd2` retained. No reboot
armed; the used image must not rerun. Desktop/input/gauge acceptance stays pending.

### Rearm implementation scope and validation (PR #24)

Branch `device/macbookair7-2/sci-controller`, base main `68aac71`. Add a pending
check while the owned EC GPE is still suspended: queue known work directly,
otherwise resume and keep the existing post-resume pending check. Preserve
budgets, ownership, genuine handler claims, raw-handler semantics and late-arrival
coverage. Touch `battery_runtime.c`, a small `ec_query_rearm.h` ordering helper,
its host model test/Makefile target and current guides. Model known-pending and
late arrivals with a latched-interrupt fixture; run the battery VMs before a fresh
automatic native trial. A late arrival during resume can still latch a delivery;
this change does not promise to eliminate every unclaimed IRQ. Pro PR #19 at
`e9320d9` remains USB Wi-Fi/xHCI/staging work; only additive Makefile tests overlap.
Ordinary desktop battery remains off; GitHub Actions remain disabled.

Rearm implementation `3129374` (PR #24) passes the latched-delivery/late-arrival
host model, core suite, native/VM builds and six focused VMs: software-unclaimed,
normal service, foreign event, isolated filesystem, active filesystem deadline
with automatic reset, and query deadline containment. The isolated image and
all outside bytes pass verification. The ordinary kernel is byte-identical to
PR #23's full-smoke baseline; that unchanged baseline was not rerun. Docs and
whitespace checks pass. Local evidence and binary hashes are archived outside
`build`; physical ordering efficacy is accepted for the bounded trial above.

### Previous native post-EOI capture accepted

Branch `device/macbookair7-2/sci-controller`, integration base `2c90dfe` (PR #21).
Release `71b16a6e59709895`, session `session-00065e52c3a9ec0c-00`, completed the
automatic native trial and returned to Ubuntu. The workload completed 45 cycles /
810 operations in 60,719,544 microseconds, using 18,116 USB commands; maximum
operation time was 1,968,515 microseconds. Both service lifetimes retired with
restored enables, balanced attachments and zero heap/errors/denials. Generation
23 and 64,749 cached reads (maximum 18 microseconds), including 2,517 during
sampling, were recorded. The deliberate short stop-wait timeout was followed by
clean retirement; it is not an abandoned worker.

Full counters recorded 24 handled / 24 unclaimed callbacks. Ten completed
callbacks and thirty controller rows survived ring retention; 38 older callbacks
were overwritten. All snapshots were available on vector 41 / GSI 9. Before
and after each callback, LAPIC ISR/TMR were set and IRR clear; after generic EOI,
ISR and IRR were clear, TMR remained set (`lapic_bits=4`), and route low was
`0x8029`: remote-IRR and delivery-pending clear, level trigger retained. The
pre-EOI route was `0xd029` before handled callbacks and `0xc029` afterward;
unclaimed callbacks had `0xc029` at both pre-EOI points. LINT0 remained `0x10700`,
PIC masks `0xffff`, route high zero. Thus the existing acknowledgement takes
effect in every retained sample. Sequential snapshots do not establish line
voltage or exclude a subsequent interrupt.

All ten completion rows were observed with query work pending. Entry-to-post-EOI
time was 26 microseconds for each retained unclaimed callback, 33–34 for the
first lifetime's retained handled callbacks, and 368 for the second lifetime's
first handled callback (callback time 357). These are not `iret`, worker-duration
or input-latency measurements. The five retained unclaimed callbacks followed
handled callbacks by 553,381–754,688 microseconds, with EC status/enable
`0xa0`/zero and fixed status/enable `0x8001`/`0x20`; their callback duration was
15–16 microseconds. The extra-delivery cause remains unresolved. No evidence
here justifies altering EOI order, route polarity, event enables or claim policy.

Exact inner contents, fsck and allocation (75 clusters) passed. Outer fsck and
allocation passed: 189 directories, 896 files, 1,672 clusters, no leaks/mismatches.
891 existing files remain byte-identical; only the disposable image and root
metadata changed, with three new matching logs. New Ubuntu boot, consumed
one-shot, default/EFI order and installed/recovery hashes verified. Twenty-eight
hashed evidence files and an independent exact payload survive outside `build`.
The USB is remounted, previous desktop `b427b78f21644bd2` retained, no reboot
armed. The image is used and must be freshly prepared before another trial.
This accepts the isolated workload and diagnostic lifecycle, not ordinary desktop
battery startup, physical gauge accuracy or interactive input coexistence.

### Query execution correlation implementation (PR #23)

The source audit confirms `query_scheduled` spans queued work, EC query transfer,
AML evaluation and rescheduling until GPE resume. Existing observations cannot
distinguish those stages. Next add bounded passive query phase/sequence/timing
correlation to the diagnostic runtime and host/VM validators, reusing successful
reads and existing scheduling transitions. Keep output outside IRQ context; no
new EC/event reads, policy changes or sibling hardware gates. Implementation scope: add a pure query-phase observer to `sci_observe.h`,
entry snapshots in `battery_runtime.c`, software query-transfer IRQ injection
in `battery_vm_platform.c`, and matching host tests and
`tools/acpi_battery_smoke.py` record validation. No IRQ dispatcher or USB edits.
Base is main `a3aaa27`; Pro PR #19 remains separate. Native acceptance will require
a fresh isolated automatic trial after local validation. Ordinary desktop battery
remains off. Pro PR #19 at `e9320d9` concerns USB Wi-Fi inventory, xHCI and staging;
no IRQ/runtime overlap was found. Preserve both branches' Makefile test additions.
GitHub Actions remain disabled; validation is local.

Query correlation implementation `f3fa54d` (PR #23) passes host snapshot/lifecycle
checks, seven malformed-record cases, the core suite and native/VM builds.
Five focused VMs pass: software-unclaimed, normal service, foreign event,
isolated filesystem and active filesystem deadline/reset. The injected case
retains five query-transfer and five idle entry contexts; the transfer contexts
have a known batch/attempt and correctly unknown response code. The workload
checks preserve all outside bytes and verify final image contents. Full local
baseline smoke passes syscalls, 481 shell commands, shell editing, USB keyboard
and display startup. Documentation links and diff whitespace pass. Validation
is archived outside `build`; exact-stage VM and native query correlation are now
accepted above.

### Post-EOI implementation and local validation (PR #21)

Implementation `4b2560c` adds an optional token-owned IRQ completion observer
after the existing EOI and before interrupt return. It records a third controller
snapshot and entry-to-observation time/query state without changing claim, route,
mask, EOI order or EC/event-access policy. Core, registry ownership, real dispatcher
ordering, six record-corruption/unknown-state cases and native/VM builds pass.
Full baseline smoke passes syscalls, 481 shell commands, shell editing, UEFI USB
keyboard and display startup. Five focused battery VMs pass: injected-unclaimed,
normal service, foreign event, isolated filesystem and active filesystem deadline
with automatic reset. Outside bytes, retirement and recovery checks pass as
applicable. Exact-stage Q35 platform refusal, diagnostic save, unmount and return
also pass. Local validation and payload hashes are archived outside `build`;
physical acceptance is recorded above.

### Native SCI controller capture accepted

Release `7357a2eee058622e`, source `d9950fb` (merged PR #17), session
`session-00065e523d6edce6-00` completed the automatic isolated-filesystem trial.
42 cycles / 756 operations ran in 60,968,560 microseconds, with 16,910 USB commands
and a 3,669,507-microsecond maximum operation. Both service lifetimes retired
cleanly with restored enables, balanced attachment and zero heap/errors/denials;
generation 23 and 61,290 cached reads (maximum 18 microseconds) were recorded.
This workload timing does not establish desktop/input latency or observer cost.

The full counters recorded 29 handled and 29 unclaimed callbacks. Ten completed
callbacks were retained; 48 were overwritten. All twenty paired controller
snapshots were available on vector 41 / GSI 9: ISR and TMR set, IRR clear,
LINT0 `0x10700`, PIC masks `0xffff`. Each retained handled callback showed route
low `0xd029` before and `0xc029` after; each unclaimed callback showed `0xc029`
at both points. All route high words were zero. The route's remote-IRR bit was
set throughout these observations. These are sequential **pre-EOI** reads.

All five retained unclaimed callbacks still had EC status `0xa0`, enable zero,
fixed status/enable `0x8001`/`0x20`, and query work pending before and after.
They followed handled callbacks by 578,961–751,884 microseconds and lasted
15–16 microseconds including the controller reads. This is evidence of a
level-triggered APIC delivery with no enabled event in the selected register
reads; it does not establish its cause or justify swallowing the interrupt.
A checksum-validated, read-only MADT audit confirms source 9 / GSI 9, flags 13:
explicit active-high/level, matching the observed route. Do not substitute an
assumed active-low SCI configuration for this machine's firmware evidence.

Exact inner contents, fsck and allocation (75 clusters) passed. Outer fsck and
allocation passed with 188 directories, 893 files, 1,668 clusters and no leaks or
mismatches. 888 previous files are byte-identical; only the disposable image and
root metadata changed, with three new matching logs. New Ubuntu boot, consumed
one-shot, default/EFI order and recovery payload hashes were verified. Accepted
evidence contains 29 hashed files outside `build`, plus an independent exact
payload backup; accepted desktop `b427b78f21644bd2` remains the recovery entry.
The current test image is used and must not be rerun without fresh preparation.

Next bounded investigation: observe controller state after generic EOI and IRQ
return timing alongside query scheduling. The source audit finds EOI directly
after dispatch; these snapshots cannot verify its effect. Keep claim, route,
mask and event acknowledgement policy unchanged until evidence supports a fix.
Ordinary desktop battery startup and physical gauge/input acceptance remain off
or pending. Integration baseline `6c6bf9e` includes Pro's updated unit record;
Pro PR #19 at `f2af3f2` concerns USB Wi-Fi inventory and does not overlap this
scope. GitHub Actions remain disabled; validation is local.

### Controller snapshot implementation (PR #17)

Branch `device/macbookair7-2/sci-controller`, base main `bd0fe58`.
Added a bounded xAPIC vector-state read API and diagnostic-only snapshots before
and after the uACPI SCI callback. Records include ISR/IRR/TMR, LINT0 configuration, the
owned I/O APIC route and PIC masks; both snapshots precede generic EOI. No EOI,
route, mask, event acknowledgement or claim-policy change. Availability and
sequential-read limits remain explicit. Changes cover LAPIC helper/tests, battery
observer/runtime, VM record validation and documentation. Pro scope inspected
at implementation time (`ecad3df`) concerned network/UI acceptance, with battery
deferred. Host and VM validation preceded the accepted native trial above.

Implementation: paired controller rows in the existing eight-entry ring; xAPIC
reads are bounded to ISR/IRR/TMR/LINT0, and no IRQ return or acknowledgement
policy changes. Host core suite, all vector/bit combinations, observer pairing
and corrupted/truncated record fixtures pass. Source `6e7c93f` passes five local
Q35 cases: injected unclaimed, normal service, foreign event, isolated filesystem
and active filesystem deadline/reset. All available controller identities match
the fixture; twelve injected-callback snapshots report no APIC vector state.
The initial stale VM binary was rejected for missing controller records; the
final runs used the rebuilt VM kernel. The isolated test verified the inner tree
and all outside bytes, and deadline recovery returned automatically. Validation
is archived outside `build`; GitHub Actions remain disabled. Physical acceptance
is now recorded above.

Prior accepted **passive SCI observation**: release `57ac1e9954e445e9`,
source `d9d5afc` (merged PR #15), session `session-00065e5157c6e854-00`.
The isolated workload completed 43 cycles / 774 operations in 60,859,549
microseconds, with 17,311 USB commands and a 2,796,485-microsecond maximum
operation. This single run does not attribute timing variation to the observer.
Both service lifetimes retired with balanced attachments, restored enables and
zero heap/errors/denials; 62,443 cached reads completed, maximum 18 microseconds.

The ring retained ten callbacks across the two lifetimes, including five
unclaimed callbacks; 38 older callbacks were overwritten. All five retained
unclaimed callbacks had complete first-read evidence: EC mask `0x40` was clear
in status `0xa0`, EC enable was zero, and fixed status `0x8001` did not overlap
fixed enable `0x20`. Raw and filtered EC bytes agreed. Query work was pending
before and after each callback. Each followed a handled callback by
536,339–741,498 microseconds and took 4–5 microseconds. The preceding retained
handled callbacks saw EC status `0xe0`, enable `0x40`, and scheduled query work.
Total active deliveries remain 24 handled / 24 unclaimed; no setup/retirement
or in-flight deliveries remained. This narrows observed callback conditions;
it does **not** establish an interrupt source, queued-delivery cause or safety
of ignoring unclaimed interrupts. Keep the actual handler result unchanged.

Exact final image, inner fsck/allocation (75 clusters), outer fsck (187 directories,
890 files) and outer allocation (1,664 clusters, no leaks/mismatches/errors) passed.
885 existing files are byte-identical; only the disposable image and root inode
sidecar changed, with three matching new logs. New Ubuntu boot, consumed one-shot,
boot default/EFI order and recovery hashes verified. Twenty-nine hashed evidence
files and the payload are archived outside `build`. Installed is this diagnostic;
previous accepted desktop remains `b427b78f21644bd2`. No reboot is armed.

Next: bounded controller-delivery/IRQ-return observations alongside query timing.
The source audit finds EOI after dispatch and a level-triggered SCI route; there
is no demonstrated ordering repair yet. Ordinary desktop battery remains off;
input coexistence and physical gauge accuracy remain separate acceptance work.

### Passive SCI observation implementation — October 20

Branch `device/macbookair7-2/sci-observation`, base main `3c1739b`.
Investigate the repeated 24 handled / 24 unclaimed active SCI deliveries before
ordinary desktop battery integration. Touch the diagnostic battery runtime,
a pure bounded observation helper, host tests and the battery VM harness. Capture
only existing successful uACPI event reads (fixed status/enable and the EC GPE
byte), first-read values, callback result, timing and query scheduling state.
No additional event/EC reads, acknowledgements, event enable changes, native model
gates or IRQ claim-policy changes. Report outside IRQ context and retain missing
reads as unknown. Timestamps use the existing read-only PM timer. Validate bounded storage and the injected unclaimed VM case
before a native automatic capture. Implementation `2da3043` passes the host core
suite, observer unit/record-corruption tests, native/VM builds and all five focused
VM cases (injected unclaimed, normal service, foreign event, isolated filesystem
and active deadline). The native result is recorded above. Pro branch inspected at `4f9fa06`; its current
scope is post-merge desktop validation, with battery deferred.

Previous accepted **bounded USB data batching**: release `e1ba87c6133b2e9d`,
merged source `cd732ec` (PR #13, including the Pro PR #5 baseline), session
`session-00065e50be1d9e42-00`. The automatic run completed 45 cycles / 810
operations in 60,779,542 microseconds with 18,116 USB commands. Maximum storage
scope was 2,176,320 microseconds; the slowest operation was the initial large
write at 2,176,314 microseconds. Its data callbacks used 1,446,984 microseconds
in 18 calls / 270,336 bytes; flushing used 5,023 microseconds. The previous
profile below needed 66 data calls and 4,702,530 microseconds for that operation.
Throughput rose about 37% and commands per cycle fell about 19%. These are
single-run comparisons; callback wall time includes admitted services and does
not measure desktop input latency.

Both service lifetimes retired with balanced attachments, restored enables and
zero heap/errors/denials. The service reached generation 23; 64,685 cached reads
(maximum 18 microseconds), including 2,261 while sampling, completed. Active SCI
still totals 24 handled / 24 unclaimed; the cause remains unresolved. Exact final
image contents, inner fsck and its 75-cluster allocation passed. Outer fsck is
clean (186 directories / 887 files), with 1,660 clusters and zero leaks,
mismatches or errors. 882 existing files are byte-identical; only the disposable
image and root inode sidecar changed, with three new matching logs.

New Ubuntu boot, consumed one-shot, boot default/EFI order and recovery hashes
were verified. Evidence and payload are archived outside `build`. At that checkpoint installed was
`e1ba87c6133b2e9d`; accepted desktop recovery remains `b427b78f21644bd2`.
No reboot is armed. This accepts the isolated workload and runtime retirement,
not ordinary desktop battery startup, physical gauge accuracy or input latency.
The used diagnostic image cannot be rerun without fresh preparation. Next:
investigate unclaimed SCI deliveries, then establish desktop/input coexistence
with explicit service admission and lifecycle checks. Actions remains disabled.

Previous accepted **write profiling**: release `f99f6759a1ed8f68`, source
`b090d94`, session `session-00065e4ff61b5b24-00`. The automatic run completed
33 cycles / 594 operations in 61,198,562 microseconds with 16,454 USB commands.
Slowest operation: cycle 1, step 1, 4,702,530 microseconds. Data-write callbacks
used 3,737,960 microseconds (66 writes / 270,336 padded bytes); all flushes used
24,519 microseconds; only 44,808 microseconds fell outside raw I/O callbacks.
The maximum data callback was 274,351 microseconds. This is callback wall time,
including admitted services, not CPU or input latency; the higher maximum than
the preceding run is not enough to attribute a performance regression.

Both service lifetimes retired cleanly with restored event enables, balanced
attachments and zero heap/errors/denials. Generation 23 and 50,785 cached reads
(maximum 18 microseconds) were captured. Active SCI remains 24 handled / 24
unclaimed. The final image/tree and 75-cluster allocation check passed. Outer
fsck is clean (185 directories / 884 files), with 1,656 clusters and zero leaks,
mismatches or errors. 879 prior files are byte-identical; only the disposable
image and root inode sidecar changed, with three new session logs. New Ubuntu
boot, consumed one-shot and recovery hashes verified; 29 evidence files and the
payload are archived independently. At that checkpoint this profiling diagnostic was installed;
previous accepted desktop remained `b427b78f21644bd2`. No reboot is armed.

### Bounded USB data batching — October 20

Branch `device/macbookair7-2/usb-data-batching`, original base main `b090d94`.
Pro PR #5 has now landed as main `6dd6574`; combined source is `88f09c9`.
Air scope: bounded 16 KiB SCSI commands segmented into existing 4 KiB bulk calls,
block-cache batch admission, contiguous exFAT file-data grouping, isolated image
capability and host/VM fault fixtures. No xHCI DMA geometry, retry, flush, model
or scheduler-admission changes. Follow USB BOT 1.0 data-phase ordering and short
transfer rules; preserve quarantine on ambiguity. Validate local boundaries,
then compare a fresh automatic native profile. Ordinary desktop battery remains
disabled and Actions remains disabled. The native batching result is recorded above.

Integration: main `6dd6574` was merged without replacing either device's scope.
Pro GPT map validation and GPT-first USB discovery merge with the batching
implementation. The USB fixture's two conflicts were resolved by keeping GPT
scan/reset assertions, segmented transport counters and cases, and the larger
mock disk's `sizeof(disk)/512` capacity. The staging profiles merge cleanly in
this landed baseline; the earlier dry-merge conflicts are superseded. Combined
ordinary/native/Q35 builds, full baseline smoke (481 CLI commands, system calls,
editing, UEFI USB input and display), USB persistence/keyboard/timeout tests and
all five battery VM cases passed at `88f09c9`. Pro acceptance remains limited to
its own machine; the later Air result above accepts only the named isolated checks.

Local batching comparison (implementation `ca6f274`, added fixtures `d0e98d1`):
265 -> 169 image writes, all 72 flushes retained, and identical final disk bytes.
All/core builds/tests and exFAT hardening passed, including fragmented allocation.
The isolated image passed 169 write failures, 169 power cuts, 72 flush failures,
169 sector-aligned partial failures, 169 partial cuts and 19 pause/revoke points.
USB read-only/write/flush/reload/keyboard and timeout/quarantine VM checks, plus all five battery VM cases
passed. The profiled Q35 workload completed 78 cycles / 31,400 commands, compared
with 78 / 38,888 before batching. Native timing was pending at this local-validation
checkpoint; neither VM result accepts physical input latency or ordinary desktop
battery startup.

Latest accepted **contiguous FAT batching**: release **`2ca59bc8678efc17`** /
source **`39b152d`**, session **`session-00065e4f02718396-00`**. The automatic
trial completed 34 cycles / 612 operations in 61,130,508 microseconds with 16,954
USB commands and sampling progress in 17 scopes. Maximum storage scope was
2,721,271 microseconds; maximum operation 2,721,267 microseconds at step 1
(initial large draft write). Compared with the previous 25-cycle run below,
commands per cycle fell about 34% and maximum scope about 15%. These are
single-run measurements on the same USB drive, not a desktop latency guarantee.

The service reached generation 23 with 51,938 cached reads (1,423 while sampling),
maximum 18 microseconds. Both lifetimes retired with balanced attachment,
restored event enables and zero heap/errors/denials. SCI still recorded 24
handled / 24 unclaimed active deliveries; setup/retirement deliveries and
in-flight count were zero. The unclaimed cause remains open.

Exact final payload/tree, inner fsck (4 directories / 5 files) and 75-cluster
allocation checks passed. Outer fsck is clean (184 directories / 881 files),
with 1,652 clusters and zero leaks/mismatches/errors. All 876 prior files except
the disposable image and root inode sidecar are byte-identical. New Ubuntu boot,
consumed one-shot and installed/recovery hashes were verified. The payload and
29 hashed evidence files are archived outside `build`; the USB is remounted.
Accepted desktop recovery remains `b427b78f21644bd2`.

PRs #9/#10 are merged. Actions is disabled at the user's request; validation
runs locally. No reboot is armed and ordinary desktop battery startup remains
disabled. Next: measure the remaining large-write data/flush costs and reduce
blocking time while retaining failure guarantees, then establish desktop/input
coexistence. This result accepts the isolated workload and runtime retirement;
it does not accept ordinary desktop battery startup or gauge accuracy.

Previous accepted **operation-stepped isolated filesystem overlap**: release
**`9eb84deab70d240c`** / source **`89edb98`**, session
**`session-00065e4e5218f777-00`**. The trial completed 25 cycles / 450 operations
in 60,768,457 microseconds, issuing 18,754 commands, with sample progress in 21
scopes. The maximum storage scope was 3,210,080 microseconds; maximum operation
was 3,210,076 microseconds at step 1 (the initial large draft write). The prior
whole-cycle scope maximum was 7,303,034 microseconds. These single-run timings
show shorter exclusion, not accepted desktop/input latency; the remaining
three-second operation needs further investigation before desktop integration.

The service captured generation 23 and 41,561 cached reads (1,226 while sampling),
maximum 18 microseconds. Both initialized lifetimes retired cleanly with balanced
attachments, restored enables, zero heap/errors/denials, and clean image revocation.
SCI counted 24 handled / 24 unclaimed active deliveries, none during setup or
retirement; the cause remains open. Final image payload/tree and inner fsck plus
75-cluster allocation checks passed. Outer fsck is clean (183 directories,
878 files); 1,648 clusters are accounted for with zero leaks/mismatches/errors.
All 873 previous files other than the disposable image and inode sidecar are
byte-identical. New Ubuntu boot, consumed one-shot and recovery hashes verified;
the payload and 30 hashed evidence files are archived outside `build`.

At that checkpoint PR #9 was awaiting validation. It later merged using local
results after the user disabled Actions; the newer batching result is above.

Previous bounded **whole-cycle isolated filesystem overlap**: release
**`c473b63c44d32cd0`** / source **`8822cba`**, session
**`session-00065e4ddc1d41d5-00`**. The v38 trial completed 25 create/grow/shrink,
rename/delete/flush cycles over 61,510,180 microseconds, issuing 18,754 USB
commands. Sampling advanced inside 21 of 25 filesystem scopes; generation 22
was captured. Cached reads completed 14,425 times (130 while sampling), with a
17-microsecond maximum. Cancellation, restart and in-flight stop/timeout followed
by clean retirement passed. Both initialized lifetimes restored retained enables,
balanced attachments and ended with zero heap, errors and denied accesses.

The exclusive image lease was revoked before workspace remount. Collection
verified the final payload, empty file, unchanged anchor and image identity.
Inner fsck/allocation checks passed (75 clusters); outer fsck reports clean
(182 directories, 875 files), with 1,644 accounted clusters and zero leaks or FAT
mismatches. All 870 pre-existing files other than the SrvrOS inode metadata sidecar
are byte-identical; that sidecar was rewritten by normal session logging. The
new Ubuntu boot, consumed one-shot and installed/recovery hashes were verified.
The accepted payload and 33 hashed evidence files are archived outside `build`.

This accepts the bounded isolated workload, not ordinary desktop filesystem
concurrency or input responsiveness. Maximum filesystem scope duration was
7,303,034 microseconds; bound and measure that work before desktop integration.
SCI recorded 24 handled and 24 unclaimed active deliveries across both lifetimes,
with none during setup/retirement or remaining in flight; the cause remains open.
Gauge accuracy and long-lived service are untested. Ordinary desktop battery
startup remains disabled. No reboot is armed; the used fixture must be archived
before replacement and cannot be reused as a fresh trial.

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

### Large-write profiling — October 20

Branch `device/macbookair7-2/write-profiling` starts from merged main `9d9c72e`.
Pro scope inspected at `992e8b9` concerns network UI repaint/integration, with battery
work deferred. Air scope: read-only exFAT phase labels, per-operation bounded
image callback timing, diagnostic battery reporting, collector and host/VM
fixtures. Keep mutation order, flush barriers, I/O admission and model gates
unchanged while measuring allocation/data/free/metadata read/write/flush costs.
Record the slowest operation without printing inside its measured interval.
I/O durations include any admitted service execution; scope wall time is not
an input-latency measurement. After local validation, use a fresh automatic
native diagnostic to identify the next targeted change. Ordinary desktop
battery startup remains disabled. Actions remains disabled; validate locally.

Implementation `e1dce19` passed ordinary/native/Q35 builds; deterministic callback
accounting; 265 write-failure, 265 power-cut, 72 flush-failure and 19 pause/revoke
boundaries; generic exFAT hardening; collector and documentation checks. Five
Q35 cases passed: isolated workload, setup/active deadlines, generic filesystem
and write failure. The profiled VM completed 78 cycles with clean payload/tree
and allocation checks. These are VM results; native profiling is pending in
PR #12. The remaining long write and unclaimed SCI cause are still open.


### Contiguous FAT write batching — October 20

Branch `device/macbookair7-2/fat-write-batching` consumes PR #9 source
`89edb98` and native acceptance notes. PR #9 merged as `5bf63b7` after
review of its local host/VM and native evidence; this branch consumes that main. Pro scope inspected at `14e2a99` remains Ethernet readiness
and prompted cable testing; no overlapping filesystem change was found.

Bounded objective: reduce repeated writes of the same FAT sector during
contiguous allocation/free. Scope: `kernel/src/fs/exfat.c`, entry/isolated-image
host regressions and this record. Preserve the existing on-disk FAT values,
copy-on-write publication and flush/dirty ordering; fragmented chains retain
their existing traversal. First record per-operation FAT/data write counts,
then batch adjacent equal FAT entries into bounded writes. Validate neighboring
FAT entries, boundary failures, directory growth and fragmented allocation,
then run the full image fault matrix and battery filesystem VM workload.
The native comparison is recorded at the top of this file; no reboot is armed
and ordinary desktop battery startup remains disabled.

Host tracing measured 142 writes (67 FAT) for initial 256 KiB+37-byte creation
before batching, versus 78 (3 FAT) afterward. One full cycle falls from 457
to 265 writes; all 72 flushes remain. These are fixture transfer counts, not
hardware latency measurements. The change retains each FAT entry's prior value
and the existing write-blocked recovery behavior; it batches contiguous ranges
in at most 512-byte slices while leaving fragmented traversal intact.

Local range/sentinel/failure tests, all 19 pause/revocation boundaries, all 265
write failures + 265 power cuts + 72 flush faults, exFAT hardening (4K/128K,
copy-on-write recovery, directory growth, capacity and fragmented allocation),
scratch isolation and ordinary/native/modeled builds passed. Local `make smoke`
passed the full core suite, 481-command CLI workload (424 files / 5,159 clusters,
zero leaks/FAT mismatches/errors), syscall, line-editing, UEFI USB/input and
display-service checks. All six affected battery VM cases passed: sustained
isolated filesystem, isolated setup/active deadline automatic reset, filesystem
normal/write-failure, and scratch writes. The sustained run completed 75 cycles /
1,350 steps in 60,494,657 microseconds, 37,391 commands, 21 scopes with sampling
progress, and a maximum step of 499,405 microseconds (regrow, step 5). Payload,
outside bytes, allocation and clean retirement passed. Staging (35) and collector
(12) tests also passed. Functional code is `6ca3f4f`, tested after integration
with main as `880efc3`; later documentation commits do not change that code.

Repository Actions is disabled at the user's request; no hosted result is used.
PR #10 merged as `39b152d`. Its native comparison is accepted for the bounded
isolated workload above. Every subsequent trial still needs a fresh disposable
fixture, exact-stage rehearsal and independently verified recovery.


### Filesystem operation scheduling — October 20

Branch `device/macbookair7-2/filesystem-latency`, based on PR #8 source `8822cba`
and its acceptance notes. PR #8 passed CI and merged as `09207b0`; this branch
consumes that integration baseline. Pro scope inspected at `a19ce5c` remains
Ethernet bring-up; preserve its branch and profiles.

Bounded objective: split the diagnostic filesystem cycle into explicit operations,
release storage exclusion between them, measure each operation and require a
complete cycle before accepting its payload. Files: `workspace_image.c`/header,
battery runtime workload, host fault/continuation tests, VM harness and collector.
Keep each filesystem mutation atomic under existing exclusion; do not change
shared exFAT transactions, scheduler admission or ordinary desktop startup.
Host tests must reject skipped/repeated cycle identities and cover failures and
revocation between operations; VM checks must prove complete payloads, sampling
progress, step accounting and automatic fault return. Native latency acceptance
remains pending; a bounded operation count does not guarantee device completion
within a fixed time. No new physical test is armed.
Host validation passed all 19 clean pause/revocation boundaries plus 457 write
failures, 457 power cuts and 72 flush failures, preserving outer bytes and clean
post-recovery inner allocation. Collector tests cover complete step accounting,
invalid counts/timing/phase and legacy reports. Ordinary/native/modeled kernels
build with the local `tools/host-env.sh` wrapper. The sustained VM completed
73 cycles / 1,314 operations over 60,354,688 microseconds, with 54,874 transport
commands and 23 scopes containing sampling progress. The maximum operation was
532,726 microseconds (step 1, the initial large draft write); maximum scope was
532,748 microseconds. Payload, outer isolation and clean retirement passed.
Setup and active-sample deadline fixtures also passed automatic reset and
outer preservation/inner recovery; the scratch-write regression passed.
The physical comparison above accepts the operation sequence and data integrity;
it does not accept interactive latency.


### Isolated filesystem coexistence — October 20

PR #7 passed CI and merged as `76508bf`; this device branch consumes that baseline.
Pro PR #5 inspected at `304bc55`. Air scope: exclusive exFAT file-extent admission,
a diagnostic bounded block view over a prepared disposable filesystem image,
workspace lease exclusion, battery workload and VM/host isolation verification.
Avoid Pro's partition-parser and CPU-load changes. The outer workspace must be
unmounted with public writes revoked throughout the lease; all test allocations
remain inside the image's proven data extent. Validate interrupted inner writes,
byte-for-byte preservation outside that extent, lease revocation and refusal paths
before adding a native capture. Ordinary desktop startup remains unchanged.
Air reserves **v38 laptop-probe** for `--battery-filesystem`; preserve Pro's
v37 desktop network profile during integration. The isolated VM completed 332
cycles / 249,772 transport commands over 60,183,438 microseconds, with 25 progress
bursts inside I/O scopes and generation 24 at completion. Service retirement,
exclusive range revocation, offline image contents/allocation checks and exact
preservation of every byte outside the image passed. Filesystem traffic now waits for ACTIVE runtime ownership. Both the setup and
active-sample deadline fixtures reset automatically; outer bytes remained unchanged
and a separate copy of the interrupted inner image recovered successfully.
All 457 write-failure, 457 power-cut and 72 flush-failure host boundaries passed
outer isolation and post-recovery inner/outer allocation checks. The exact staged
Q35 native-refusal/automatic-return check and the bounded native trial above
passed. PR #8 publishes this work; Pro PR #5 was subsequently inspected at
`f74595d` (Ethernet recovery work). Keep that branch and its capture profiles intact.

### Sustained filesystem coexistence — October 20 (VM-first)

Base `d72d2af`, host/CI follow-up PR #7 at `dd4245d`; Pro PR #5 inspected at
`fec1a41` with the merged battery baseline. Scope: the disposable Q35 battery
model and smoke harness, exercising a minute of actual filesystem allocation,
grow/shrink, rename, deletion and flushes during sampling, plus a write-failure
case. Keep native profiles and ordinary desktop startup unchanged. Validate
sample progress inside storage scopes, clean retirement, offline file contents
and allocation accounting. Failure must retain the previous committed file and
block further writes; recovery is rehearsed only on the disposable image.
The failure fixture also exposed an overly strict shared exFAT checker: it
mistook nonzero FAT entries of bitmap-free clusters for allocation mismatches.
Scope now includes correcting that check against the exFAT allocation-bitmap
contract, with stale-free-FAT admission and actual orphan/missing-allocation
refusal regressions. This does not change allocation or repair on-disk bytes.
A real workspace trial needs a separate crash/recovery contract before enabling
concurrent filesystem metadata mutation on hardware.

Final disposable VM validation completed 134 filesystem cycles in 60,539,284
microseconds, with sample generation advancing from 1 to 25 inside the I/O
scope. The service completed 8,320 cached reads (1,472 during sampling), maximum
603 microseconds, and retired cleanly. Offline payload/tree checks, fsck and the
kernel allocation checker passed (five files, 12 clusters, zero errors/leaks).
The injected write failure preserved the old payload before and after recovery,
refused later writes, advanced sampling and retired with zero errors/denials/heap.
Recovery was performed only on a disposable VM image; the failed image is retained.
These results do not extend native acceptance beyond the v36 scratch range.


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

## Historical October 19–20 scope and open items

This table records the early checkpoint. Later acceptance above supersedes its
pending input, PCM/headphone, battery-provider and Ethernet entries; it is not
the current capability ledger. Consult the current status and latest run first.

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

## Historical October 19 interactive physical record

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

## Historical October 19 next actions

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

## Historical automated power record

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
