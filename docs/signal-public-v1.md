# Public signal/context v1: exact review packet

Status: owner review cleared staged source implementation at37cd7382; public admission remains disabled pending exact layout, target and guest proof. Target is
real SIGUSR1 context delivery, atomic nested waiting and a bounded WTF collector
handshake, followed by the required no-JIT JavaScript smoke. The qualified
[private transaction](signal-publication-transaction.md) remains the authority.

## Admission and allocation

The shared owner reserved syscalls164 CONTROL,165 RETURN,166 SUSPEND and167 SIGNAL_IO;163 stays session scope. Initial INFO supports SIGUSR1 and SA_SIGINFO,
optionally SA_RESETHAND. INFO|SA_RESTART returns ENOTSUP before any effects.
Other unsupported flags/signals reject explicitly. Ordinary callers never have
RESTART silently stripped. LEGACY RESTART remains supported through cooperative
dispatch. The SrvrOS WTF adapter deliberately requests INFO alone and documents
nonrestart behavior; its exact interruption paths and tests are required.

CONTROL input is a96-byte fixed-width/versioned kernel wire object:

| Offset | Field |
| --- | --- |
|0|uint32 size=96, uint32 version=1|
|8|uint32 operation, uint32 signum|
|16|native action24: uint64 handler, uint64 mask, uint32 flags, uint32 reserved=0|
|40|uint64 trampoline|
|48|uint64 mask_how, uint64 mask|
|64|uint64 reserved[4]=0|

The native action matches the existing x86-64 libc sigaction24-byte layout,
including flags at16 and zeroed trailing padding. Kernel constants use the
existing native SIGINFO/RESTART/RESETHAND bit values, with explicit conversion
to private descriptor mode/flags. DEFAULT/IGNORE derive from handler0/1;
LEGACY requires a real executable handler and no trampoline; INFO requires both
executable entries. INFO with disposition sentinels rejects rather than quietly
dropping flags. Action masks canonicalize unmaskable/bit0 fields; thread masks
retain the existing convention except unmaskable9/19.

CONTROL registers are request pointer/size and output pointer/size. Every unused
request field is zero. Proposed operations and exact outputs:

| Operation | Inputs | Output |
| --- | --- | --- |
|FEATURES|body zero|64-byte versioned capabilities|
|QUERY_ACTION|signum10|native action24|
|EXCHANGE_ACTION|signum10, action, optional INFO trampoline|optional old native action24|
|MASK|mask_how and mask|optional old mask8|
|LEGACY_TAKE|signum10|64-byte selected-action/old-mask/generation result|
|BLOCKED_TAKE|mask|selected native int32 signum4, only signals already blocked by caller|

FEATURES64 contains size/version at0, capabilities at8, INFO signal mask at16,
INFO native flag subset at24, context bytes688 at32, info bytes64 at40,
maximum depth4 at48 and reserved0 at56. LEGACY_TAKE64 contains size/version8,
selected signum8, native action24, previous mask8, action generation8 and
reserved8. It selects only an unmasked LEGACY disposition, never INFO; complete
output precedes exact pending consumption, self/action blocking and RESET commit.
BLOCKED_TAKE lets sigwait consume blocked INFO without pretending a handler ran.
Raw old cooperative poll/consume continues to exclude INFO.

Old-action output points directly to the caller's actual sigaction, not a libc
intermediate subsequently copied after publication. Validate/copy all24 bytes
before descriptor commit. Normalize input before output for act==oldact. Query
or exchange requiring old output refuses an opaque old-native CATCH without a
known handler (ENOTSUP, unchanged); replacement without old output can replace
it. Keep old native cooperative callers functional.

## Common-return completion and lifetime

Dispatch copies/validates the fixed request into the current thread's private
state and stages a request sequence, exact trap pointer, image/TID/scheduler
identity and output addresses. Request sequence1..INT64_MAX advances only on
successful staging, never reuses a value, and refuses EOVERFLOW at exhaustion.
Completion requires both exact request sequence and exact current trap association;
clear the association before return, including cancellation and every refusal.
No action/mask/user-output publication occurs
inside unsafe/safe syscall admission. After full dispatcher cleanup and
cancellation priority, regain ordinary I/O admission outside the transaction if
needed, re-exclude IRQs and obtain fresh owner/proof/policy observations.
Complete bounded output through the qualified copy/exclusion primitive, then
commit only prepared stores and RAX. Complete/clear the request before any
handler entry. No request survives a user return, cancellation, exit or image
retirement. Existing same-address trap storage is not a reusable request token.

RETURN stages token/frame identity, then runs authoritative private restoration
at that seam. It bypasses ordinary RAX writeback and skips another delivery on
its consumed proof. Prepared default-fatal status goes straight to process_exit.
Invalid return is contained terminal failure. No user selectors/flags/FX are
accepted for restoration.

CONTROL MASK, action changes and SUSPEND completion stage generic newly-unblocked
pending/default effects and prepared fatal outcomes before publication, just as
private RETURN does. A prepared fatal outcome goes directly to process_exit;
there is no raw mask store followed by a later, optional signal check.

Public exchange can reuse the separate internal extended copy entry qualified
for private RESET, without widening the original three-object API. It allows
at most12 explicit trusted
kernel objects, each1..4096 bytes/two spans, all extents checked before translation
and all translations/physical aliases checked before any output byte. Keep output
limit1024/two destination pages, the existing whole owned-stack and both complete
FPU exclusions, actual current root/hardware policy, owned ordinary mappings and
no reusable lease. The original private API still rejects counts above3. No excluded
object comes from a user-supplied pointer or length.

For process-wide action exchange the exclusion set is: complete selected
state<=4096; process prefix from offset0 through main_signal_state (currently112
bytes, includes killed at3, kill_signal at8, catch16, ignore24, shared pending32,
main mask40, main pending48, action56 and generation96, with final alignment);
and each of8 worker signal_mask/pending16-byte extents. Ten objects suffice;
the12-object cap leaves bounded room for a separately identified object, not an
unbounded array. Assert every actual offset/length in target C/C++. Default
selection stages the eligible lowest-TID local before shared decision across
live threads under the proposed new action and preserves an earlier fatal cause.
IGNORE stages discard across all pending slots, including inactive slots. A
prepared default fatal commits killed/cause/exact pending stores after old-action
output, then the normal outer terminal path exits without user resume. No wake,
allocator, callback, policy query or fallible operation is hidden after output.
Other control/wait operations include every mask/pending/kill object they mutate,
with the same complete prefix coverage. The private qualification covers count13, malformed last extent and48
physical-alias cases; each public operation still needs its own generated-object
and output/state rollback fixtures, plus updated actual stack/call-graph bounds.

Keep all existing membership, image/TID nonreuse, source/exclusion and IRQ rules.
The complete selected state must remain<=4096 bytes/two spans. New fixed request,
ticket and association fields require size/offset assertions and an updated
static/cumulative-stack receipt. Fork/exec rejects active INFO records or waits
before effects. Requests cannot survive a user return. Logical I/O tickets have
the explicit fork/exec rules in the linked supplement; they do not justify a
blanket legacy-only fork/exec refusal. Failed install/exec remains unchanged. Retirement
clears metadata before bindings/TLS/stacks/slots are reused. Reconcile session
detach ordering exactly as already reviewed.

## Atomic temporary-mask wait

SUSPEND BEGIN takes a temporary mask and returns a positive1..INT64_MAX ticket
only when the wait has reached its interrupt-completion path. FINISH(ticket,
legacy_handler_ran) releases that exact completion and returns EINTR once when a
handler completed. With no handler it atomically rearms the same wait and returns
the same positive ticket only after the next wake; it never exposes an internal
EAGAIN/restore/reallocate gap to libc. Ticket exhaustion refuses before effects.
Tickets are nonreused within the image/thread lifetime; identity and depth are
part of private ownership, not user-provided authority.

Reserve one64-byte slot per allowed starting depth0..3: ticket, owner frame
anchor, previous mask, temporary mask, request/trap sequence, phase/depth/flags,
completed record token and reserved0. Image/publicTID/scheduler identity belong to
the enclosing selected state and are checked on every operation, not duplicated
per slot. SUSPEND registers: RDI BEGIN1/FINISH2, RSI temporarymask/ticket,
RDX ownerframeanchor/legacy_handler_ran, other args0. The libc sigsuspend root
provides its real stable frame anchor for known legacy nonlocal-unwind cleanup. BEGIN at full context depth4 returns EAGAIN with no mask change.
Under one saved-IRQ scope, reserve, save the old mask, install the temporary
mask, apply existing generic pending/default decisions, and test/arm the existing
signal wake queue via scheduler_wait's atomic condition check. Do not use a
userspace mask-change-then-yield sequence. Cancellation wakes and retires state.

The initial BEGIN return seam is tagged explicitly. An eligible INFO delivery
stages an association between the exact wait ticket and new private record;
the record's restore mask becomes the wait's original mask, while the public
snapshot still reports the actual temporary interrupted mask. Wait phase moves
RETURNING→HANDLER in the same delivery commit. Its sigreturn restores the original
mask once, applies generic newly-unblocked default decisions and marks INFO_DONE.
FINISH at the original depth verifies ticket/owner/INFO_DONE and clears the slot
without restoring again. Nested waits use distinct slots/tickets.

If the first seam has no INFO delivery, clear the initial-return tag and mark
LEGACY_READY. Later unrelated traps cannot steal the initial mask-override
association. They can still complete the logical wait: INFO delivery at this
slot's exact originating depth while it is LEGACY_READY associates the new record
with the exact ticket in COMPLETION_ONLY mode, leaving that record's restore mask
equal to its actual interrupted mask. Private return marks the ticket's sticky
INFO_COMPLETED bit after successful restoration, without restoring the pre-wait
mask or clearing the slot. FINISH at the original depth returns EINTR if either
that kernel-owned bit is set or a legacy callback actually ran; it restores the
pre-wait mask once through the generic pending/default transaction and clears
the ticket. Without either completion it retains the same ticket and temporary
mask, transitions atomically to PARKED and calls the existing atomic scheduler
condition/wait again inside FINISH. It does not restore, clear or return to libc
between that decision and rearm. On the next wake, RETURNING/common completion
returns the same positive ticket, and libc dispatches legacy then calls FINISH
again. A fresh initial-seam MASK_OVERRIDE association is allowed for that exact
new wake. There is no EAGAIN return after an already-installed wait. It cannot
blindly rearm after a real late INFO handler. An INFO record in the
initial MASK_OVERRIDE mode instead transitions to INFO_DONE and already restores
the pre-wait mask; FINISH only clears it. These modes are explicit record flags,
not inferred from an address or a libc notification counter.

Outside INFO, libc cooperatively dispatches after BEGIN. INFO may interrupt that
legacy callback: COMPLETION_ONLY restores its actual action-blocked mask, then
the legacy callback's existing cleanup restores the temporary mask, and FINISH
performs the sole final pre-wait restoration. Legacy-nested sigsuspend is refused
before effects, so it cannot reuse this originating-depth slot. Nested INFO waits
use deeper slots and exact distinct tickets. Test stolen/spurious/coalesced wake
through LEGACY_READY, false FINISH with originalmask permitting INFO,
INFO during legacy dispatch or FINISH's syscall, nested
resume, completed INFO followed by no legacy callback, and newly-default pending
effects at each mask owner. A FINISH already staged in the interrupted trap does
not clear state until its common completion; any delivery before that commit
must either be associated/finished first or occur after the operation linearizes.

Inside INFO, the wait must unblock only supported INFO among caught signals;
reject incompatible legacy-caught mask combinations before effects. A competing
action replacement that removes INFO is proposed to return EBUSY before output
or descriptor publication while any INFO-only wait is PARKED, RETURNING or
LEGACY_READY and
not yet associated with a record. Validate this bounded nine-thread/four-slot
scan before output copying. Preserve the wait/mask/descriptor unchanged: there
is no invalidation, wake or second restoration to strand the sleeper. The
existing INFO sender/resume still completes that wait. Once HANDLER/INFO_DONE,
the delivered record owns restoration; action changes cannot restore or clear
its wait. COMPLETION_ONLY records also retain their own actual interrupted mask;
only FINISH owns that ticket's final pre-wait restoration. This replaces the
earlier cancellation proposal and needs owner review.
INFO-only BEGIN also refuses an INFO action with RESET before effects. Installing
INFO|RESET while an unassociated INFO-only wait exists returns EBUSY, as does
removing INFO through the old raw SIGNAL_CONFIG path. Thus RESET's internal
delivery commit cannot change a parked wait's only resume action to DEFAULT.
Every descriptor mutation path shares the same precommit barrier; the public
exchange cannot be the only guarded path. Exec rejects active waits, and teardown
retires them. Test two-thread parked wait versus RESET delivery and raw config.
Do not invoke a legacy action-table lock interrupted by
INFO. Legacy-nested sigsuspend remains explicitly unsupported rather than hanging.
With INFO installed, masked ppoll/pselect/epoll_pwait reject nonnull signal
masks before effects. Legacy-only compatibility uses the supplement's atomic
LEGACY_MASKED ticket/registration exclusion, so no old cooperative masked wait
can acquire INFO midway through its mask/poll window. Null-mask forms retain
ordinary poll/select behavior. No new atomic masked-poll semantics are advertised.

## Libc and native context

For SIGUSR1, registration/query and cooperative selection use kernel descriptors;
do not publish a separate libc-table entry. The selected legacy callback runs
after its kernel transaction returns and without an action lock. Other legacy
signals retain existing table ownership. While INFO is active, defer cooperative
legacy dispatch and reject operations that would acquire an interrupted legacy
action lock. INFO entry/return itself never takes that lock.

The real assembly trampoline receives the kernel-selected handler, context/info,
token and frame identity. A fixed stack-local C adapter builds native siginfo and
ucontext and calls the actual sa_sigaction. Preserve the existing greg indices;
append a16-aligned embedded688-byte native snapshot to mcontext_t (target size944, native ucontext1008 with16-byte alignment), populate all
real GP fields explicitly and point fpregs into its embedded512-byte FX area.
Zero unavailable fields and expose a validity extension for siginfo facts; do
not claim Linux layout or unavailable sender/stack/trap facts. The kernel wire
remains688+64 in the760-byte private frame. Handler edits never alter private
return. Target C/C++ offsets/sizes and full stack requirements are asserted.

getcontext's present zero-filled success is not real capture: return ENOSYS with
unchanged output. setcontext/swapcontext/makecontext remain unsupported honestly.
Guard known longjmp/_longjmp through active INFO by terminal _exit before jumping;
raw assembly/unrestricted nonlocal unwind is not universally detected or admitted.
Kernel records remain bounded and the collector must fail terminally on uncertainty.

Use an assembly-safe shared ABI header for the three numbers and C/C++ wire
assertions; sys.h and syscall_numbers.h include that single allocation.

A userspace notification counter is explicitly rejected as retry authority:
INFO may run after the check and before another blocking syscall. Sticky image
capability profiles do not close the synthetic I/O slicing gap either. The
[logical I/O supplement](signal-logical-io-v1.md) proposes kernel-bound tickets
and validation plus backend dispatch in one syscall entry, guarded synthetic
effects, bounded compact mask restoration and captured pre-handler sleep outcomes. It preserves actual
progress/EOF/outer deadlines, spans internal polling slices, retains descriptor
identity and defines legacy-only fork/exec behavior. Public implementation remains
blocked on that protocol and this packet's wait/completion review.

The confirmed poll defect is a pending-signal branch returning0, followed by
libc's reliance on a newly cooperative callback. Proposed INFO-only change:
compute real readiness, preserve nonblocking/expired-timeout completion, then
return SRV_ERR_INTR for eligible INFO with no readiness before sleeping. Retain
legacy-only native behavior and cooperative dispatch; test both mixed modes.
Console/pipe/sleep already return INTR before progress; audit socket, wait and
user-backed eventfd/timerfd loops end to end, including gaps and FD close/reuse.

## Pinned WTF adapter and verification

Pinned source is browser `build/browser-jsc/run-nxi_i90d/wpewebkit-2.54.2/Source/`:
WTF/wtf/posix/ThreadingPOSIX.cpp, WTF/wtf/PlatformRegisters.h and
JavaScriptCore/runtime/MachineContext.h, plus Threading.h and MachineStackMarker.
SrvrOS uses the real embedded native snapshot for original SP/FP/IP/GPR/FX and
red-zone roots. Enable context reading only after this SDK/kernel path works;
audit editing/profiler consumers against read-only return semantics.

Replace SrvrOS's unbounded global semaphore assumption with one outstanding
ticketed request per live retained target. Publish native nonreused TID and
phase/ticket with release/acquire synchronization; handler validates the actual
current target and phase. Capture registers before SUSPENDED acknowledgement.
Use atomic sem_post and a monotonic bounded sem_trywait/yield loop for coordinator
acks; nested resume marks its distinct phase, outer handler completes atomic
sigsuspend, clears context and posts RUNNING only after its last request access.
No allocator, application/action lock or user-lock acquisition in the handler.
The outer handler rechecks the request phase after every sigsuspend completion;
a spurious/duplicate USR1 that did not request resume cannot release the saved
context or post RUNNING. Rewait with the exact outer context retained. Save and
restore errno in this WTF control handler around its mask/wait operations; the
general libc adapter does not override a user handler's errno policy.
No request-owned memory is freed while a handler can still reference it.

Propose10-second monotonic suspend/resume deadlines. Any uncertain target exit,
send failure, stale/late ack, overflow or failed resume terminates the process;
never return success or ordinary failure permitting the collector to skip a live
mutator. Confirmed pre-publication retirement needs a separately proven safe
callsite rule, otherwise use the same terminal policy. Retain original stacks
and contexts until matching resume acknowledgement.

Required tests include ABI/version/flags/output guards and alias rollback,
legacy/INFO transitions, real public main/worker context/FX/native pointer checks,
mask/wait arrivals at every boundary, nested resume/full-depth rejection,
coalescing/reset/default decisions, token/ticket/exit/reuse races, INFO during
interrupted libc locks, mixed legacy restart and INFO nonrestart, FD identity,
partial I/O/readiness/EOF/timeout, poll/pselect refusal semantics, and known
nonlocal-unwind failure. Then matched SDK/ICU/WTF build, bounded suspension and
original-register/stack conservative roots, positive and terminal-negative
coordinator probes, and the minimal no-JIT JavaScript smoke with actual0+42.


## Explicit review decisions and dependencies

- Approve/revise the proposed164..166 allocation and native24-byte direct old-action
  output; opaque old CATCH query/refusal semantics need agreement.
- Review the kernel-bound logical I/O supplement, especially initial descriptor
  capture under the libc lifecycle lock, syscall whitelist, independent nested
  callbacks, per-thread size budget and fork/exec clone/retirement rules. INFO
  RESTART is still rejected; the userspace-counter proposal is withdrawn.
- Approve/revise INFO-only nested SUSPEND masks and explicit masked ppoll/pselect
  refusal, including action replacement while a wait is parked.
- Review pre-publication target retirement vs terminal coordinator policy and the
  exact10-second monotonic budget before changing WTF.
- The browser checkout already contains the explicit GOT-before-TLS app.ld fix,
  whereas this dependency checkout predates it. Carry that reviewed correction
  into the shared SDK with a GOT+TLS regression before matched ICU/JSC rebuilding;
  do not regress to the previously diagnosed zero-TLS layout.
- Successful exec pending/default stranding is reproduced in the actual lifecycle
  fixture. A separate reviewed fix evaluates the committed image's pending bits
  after its final console checkpoint under IRQ exclusion through exit/user entry.
  Strict/sanitized baseline, fatal, default-ignored and earlier-fatal profiles
  pass; target compilation passes. A native guest remains to be run. This does
  not qualify untested explicit native exec attribute-default/mask semantics.
