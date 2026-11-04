# Public signal v1: logical I/O review supplement

Proposal only; no public implementation is admitted. This replaces the libc
notification counter as retry authority in [the public packet](signal-public-v1.md).
The owner reproduced gaps in synthetic poll, eventfd and epoll loops, even with
no legacy RESTART disposition. An internal ten-millisecond timeout is not the
completion of the enclosing operation. A check followed by a separate syscall
cannot close the gap. Sticky image profiles were considered and rejected.

## Authority and exact proposed storage

Add syscall167 SIGNAL_IO, subject to allocation review alongside164..166. Its
operations are BEGIN1, CALL2, END3, UNWIND4, LOCAL_BEGIN5, LOCAL_END6 and RESULT7. All operations require the current live
image/public TID/scheduler identity; tickets are private to that binding. A
ticket exists before BEGIN can return through INFO delivery. INFO delivery marks
**every live ticket** interrupted in its existing private commit before entering
the handler. No counter or userspace retry decision can undo that bit.

The normal register ABI is RDI request pointer, RSI128, RDX output pointer,
RCX output bytes and R8/R9zero. BEGIN output is a separate descriptor-generation
vector of count*16 bytes; zero count requires NULL/0. Other output-bearing forms
use their exact documented size. Input/output exact alias is supported only after
full input normalization; a fault refuses before ticket/mask/output publication.
CALL/END/UNWIND/LOCAL_BEGIN have NULL/0 output. LOCAL_END uses the compact form below.

The proposed128-byte request has these offsets, all fields fixed width:

|Offset|Field|
|---|---|
|0|uint32 size128, uint32 version1|
|8|uint64 operation|
|16|uint64 ticket; zero for BEGIN|
|24|uint64 raw syscall number; zero except CALL|
|32|uint64 arguments[6]|
|80|uint64 native descriptor vector address|
|88|uint64 descriptor count,0..64|
|96|uint64 flags; BEGIN poll-list1/LEGACY_MASKED2, LOCAL_BEGIN FINALIZE_NATIVE1, otherwise0|
|104|uint64 outer absolute monotonic deadline in microseconds; UINT64_MAX infinite|
|112|uint64 reserved[2], both zero|

BEGIN takes descriptor input entries {int64 fd,uint64 reserved0},16 bytes each,
and an output vector {int64 fd,uint64 generation}, also16 bytes each. Count<=64
and the output is<=1024 bytes/two destination pages. No descriptor is valid if its
generation is UINT64_MAX. Negative poll entries retain fd with generationMAX and
are skipped. Scalar-I/O BEGIN uses flags0 and invalid positive descriptors
return EBADF with no ticket/output. Poll-list BEGIN uses flags1: missing positive
descriptors are returned with generationMAX and remain POLLNVAL entries even if
that descriptor number is later opened. Other ready entries still count. A live
descriptor whose generation counter is exhausted returns EOVERFLOW before
effects; it cannot be safely enrolled by pretending MAX is a reusable identity.
BEGIN also returns its positive1..INT64_MAX ticket in RAX. Input is normalized
before output, so exact input/output alias is supported. BEGIN args[0] is the
logical root's stable user frame anchor (__builtin_frame_address(0)); it forces a
real frame pointer in that owning libc function. BEGIN with LEGACY_MASKED also
uses args[1] for the temporary mask. BEGIN args[2] fixes the root kind:
RAW1, READ2, WRITE3, POLL4, EPOLL5, SEM_WAIT6, WAITPID7 or SLEEP8. Internal wrappers
borrow that kind/ticket; they cannot relabel a poll slice as another outer call.
BEGIN args[3] binds the primary raw syscall and args[4] its vector/socket
operation selector (zero where absent). Kind/primary/selector are immutable and
validated against the per-kind backend table; unsupported combinations refuse.
POLL deadline0 means genuinely nonblocking, UINT64_MAX means infinite; other
finite values are the original absolute outer deadline. Other unused args are
zero. Neither anchor
nor ticket grants cross-thread authority.
The fixed request is staged by value; its descriptor-vector pointer is staged,
not the1024-byte vector. At the common completion seam, after ordinary I/O
admission and with IRQs excluded, read/validate the complete vector into bounded
owned kernel stack scratch, then capture current generations and prepare the
output. The input vector's linearization point is this read, not dispatch entry.
The matching libc retains its lifecycle lock throughout. An input fault refuses
without output/ticket; all vector bytes are normalized before any aliased output.
No pointer into dispatcher's expired stack or half-copied user vector is retained.

BEGIN's output and ticket publication use the qualified common-return transaction:
fresh binding/proof, copied inputs, descriptor observations under IF0, all output
validation/exclusions, then output followed by private ticket stores. The entire
selected state is excluded. If output copying refuses, no ticket is allocated and
the ticket sequence does not advance. Captured descriptor generations and ticket
publication share the same uninterrupted continuation. A later INFO at that
return sees the newly published ticket, even before libc has received its value.

CALL copies the request and descriptor expectations before backend work. It
requires exact live ticket, original image/thread identity, expected originating
INFO depth and top-of-logical-operation stack. Shape/version/whitelist violations
return EINVAL/ENOTSUP before effects. After that structural validation, an
interrupted ticket returns EINTR before examining user buffers, descriptor
readiness or entering a backend. Otherwise validate all descriptor generations
and copied operation-specific arguments, then invoke the selected backend in
the **same syscall entry**. There is no separate admission syscall. A descriptor
generation mismatch returns EBADF (poll-specific retired entries produce POLLNVAL).
The expectation vector must exactly cover the copied backend operands: one entry
for each scalar fd, every nonnegative POLL descriptor in order, and both the
endpoint plus every SCM_RIGHTS descriptor for message send. No unrelated vector
can satisfy admission. Reject count/order/fd mismatch before backend invocation;
for duplicate fd operands, every duplicate expectation must agree. A receive
rights operation validates its existing endpoint only; newly received descriptors
are results published by the existing backend transaction, not old expectations.
No post-unlock generation capture may silently attach the operation to a recycled
descriptor. CALL does not clear interruption, release the ticket or create a new
deadline. Success/partial results are returned exactly once.

END takes only ticket and zero body, succeeds only for the current top ticket at
its originating INFO depth and clears that slot. Duplicate/stale/out-of-order END
returns EINVAL unchanged. END does not change the already chosen operation result
and does not report a late INFO as a replacement for positive progress/EOF/timeout.
BEGIN after ticket exhaustion returns EOVERFLOW before effects, never wraps.
Four simultaneously live logical operations are proposed; full capacity returns
EAGAIN before effects. Internal wrapper composition borrows one explicit ticket;
it does not allocate a fresh ticket for each internal poll slice.

Proposed private ticket96: ticket, parent ticket, originating INFO depth, flags
(LIVE/INTERRUPTED/LEGACY_MASKED plus fixed rootkind/outcome syscall/validity bits),
outer deadline, owner frame anchor, previous mask, temporary mask, captured signed
native result, captured elapsed microseconds, captured elapsed ticks and outcome
cookie. The last four words are immutable for a completed native result. Masks are zero for ordinary operations. Pack rootkind in flags bits0..7,
primary number8..23, primary selector24..31, last outcome number32..47, and LIVE,
INTERRUPTED, LEGACY_MASKED, COMPLETED_NATIVE, FINALIZE_USED, MONO_VALID and TICKS_VALID
in bits48..54; remaining bits zero. Identity is the selected state's
existing24-byte identity, never a user argument. Add four96-byte tickets plus
next-ticket/top-ticket16. Each active record gets wait association16 (64 total).
Budget from the qualified3056-byte selected state: +256 wait slots +208 request
+16 request sequence +64 record associations +384 I/O slots +16 I/O counters +32 local guard
=4032 bytes, leaving64 bytes for alignment/additional proven fields. The proposed
request208 is identity24, sequence8, trap8, operation8, output address/size16,
normalized wire128, request-kind8 and completion-flags8. This is a
budget, not a compiled size assertion. No implementation may exceed4096 or hide
additional mutation outside the excluded state; revise/review if the real layout
does not fit. Update actual offsets, stack/call-graph and copy-alias matrices.

## Admission, sleeping and completion

SIGNAL_IO is initially an ordinary unsafe syscall. Existing admission waits for
the storage owner before entering its implementation. Gateways must not emulate
a raw call by recursively invoking the interrupt dispatcher or fabricating a
trap/FPU proof. Instead share a typed backend switch with the existing raw syscall
implementation. After argument snapshots and the normal I/O gate, re-exclude IRQs,
validate ticket/identity/interruption and expected descriptor generations, then
enter that backend. No handoff, callback or userspace return intervenes.

Proposed whitelist and exact argument order follow the current native ABI. Unused
register arguments are zero; CALL never changes a nonblocking try into a blocking
operation. Descriptor expectations include all fd arguments/embedded rights.

|Number/name|Arguments|
|---|---|
|1 WRITE /4 READ|fd,buffer,length|
|138 PREAD /139 PWRITE|fd,buffer,length,offset|
|148 VECTOR_IO|fd,iov,count,operation,offset|
|50 POLL|fds,nfds,timeout_ms|
|51 FCNTL|fd,command,arg; only SRV_F_SETLKW admitted|
|155 SOCKET_TRY_IO|fd,operation,buffer,length,peer|
|9 NET_ACCEPT|listener,buffer,capacity,length_out|
|66 NET_CONNECT|ip,port,flags|
|69 NET_UDP_SENDTO|fd,ip,port,buffer,length|
|70 NET_UDP_RECVFROM|fd,buffer,capacity,ip_out,port_out|
|97 NET_PEEK|fd,buffer,capacity|
|102 UNIX_CONNECT|path; copied through existing checked pathname backend, no namespace mutation|
|103 UNIX_ACCEPT|fd|
|162 PIPE_MESSAGE|fd,layout,message,send_or_recv; validate embedded SCM_RIGHTS generations too|
|34 WAIT|pid,status_out,flags|
|43 SLEEP_TICKS|ticks|
|23 YIELD|no arguments; zero-FD sem_wait retry only|

Unlisted operations return ENOTSUP. Namespace mutation, arbitrary syscall
recursion and SIGNAL_IO itself are excluded. CLOSE/dup never run as a retry
through this gateway. PREAD/PWRITE keep their existing single-call/partial
semantics; this gateway does not add a new legacy restart loop to them.

THREAD_JOIN81 and FUTEX_WAIT85 are deliberately excluded from sticky logical
interruption for libc pthread mutex/join/condition operations. Those APIs absorb
signal interruptions and do not expose EINTR. Their existing atomic native
wait/condition queues and raw interruption handling remain, including condition
mutex reacquisition and original absolute timeout. An INFO interrupt can still
mark an enclosing read's ticket without converting an internal lock wait into a
user-visible EINTR. Never retry those waits against an already-interrupted sticky
ticket, clear an outer ticket or spin in a permanently refusing gateway. Exercise
mutex contention, join completion and cond signal/timeout/reacquisition in the
matched JSC threading tests. INFO-handler restrictions apply to the handler
context, not the ordinary interrupted caller's later mutex/join continuation.

While a backend sleeps, INFO cannot execute in that kernel continuation: delivery
only occurs at the common CPL3 seam. A newly eligible pending INFO must cause the
backend to finish EINTR before zero-progress sleep/retry. Include it in each
backend's existing atomic scheduler condition check; existing process wake queues
remain the wake source. Do not add a userspace condition-check/sleep split.
The poll correction checks genuine readiness first, then nonblocking/actual
deadline completion, then eligible pending INFO before another sleep. Legacy-only
raw poll keeps its current behavior. Completion may return through a handler;
the committed INFO transaction marks the live ticket before the handler runs.

Once INFO has run, another CALL on that logical operation always refuses EINTR.
This also covers INFO after legacy dispatch chose RESTART, after a zero internal
slice, during libc deadline calculation and immediately before the next CALL.
Real returned positive/partial progress or native read EOF is final. A finite
outer operation whose absolute deadline actually expired may return its timeout
result; an internal slice deadline cannot be substituted for that outer deadline.
No wrapper replays a positive result because a signal was observed afterward.

CALL reserves an outcome cookie from the same nonreused I/O sequence only after
admission/argument validation and before backend effects; exhaustion refuses
before effects. Each actual backend return captures its signed result and elapsed
wait snapshot before any INFO handler can execute, then publishes those words and
the raw syscall number into that exact root. RESULT7 takes the ticket and returns
64 bytes: size/version8, outcome_cookie8, syscall_number8, signed_result8,
elapsed_us8, elapsed_ticks8, validity8 and reserved0_8. Query does not overwrite
or authorize a new attempt. A sticky admission refusal with no backend attempt
leaves the previous snapshot unchanged; SLEEP has only one backend attempt and
an outcome cookie0 means it never began sleeping.

COMPLETED_NATIVE is kernel-derived from the BEGIN root kind and actual result:
RAW admits only its single mapped backend and completes on nonnegative return;
READ/WRITE complete only on their matching byte/vector/socket data backend's
nonnegative return, including readEOF and zero-length completion; WAITPID completes
on a nonnegative WAIT result (zero only for actual WNOHANG); SLEEP completes on
native sleep0. POLL completes on native POLL positive readiness, or native0 only
for its actual nonblocking/expired outer deadline. EPOLL's internal POLL never
completes its outer operation; SEM_WAIT's internal YIELD never completes it.
The exact raw-number/operation mapping is checked by the backend dispatch table,
not a user-supplied completion flag. Once complete, its captured result/cookie is
immutable, all further CALLs refuse and only RESULT, one matching pure finalization
and END are permitted. A new actual data operation cannot be admitted as cleanup.

For relative nanosleep, measure elapsed wait inside the sleep backend at its
interrupt/completion decision under IRQ exclusion, before returning through INFO.
Capture both monotonic-reference elapsed microseconds (valid only when available)
and elapsed timer ticks. The wrapper copies request before any aliased rem write,
then computes unslept duration from this snapshot and the original request; it
never subtracts post-handler wall time. If no backend attempt occurred, the full
request remains unslept. Tick fallback uses captured elapsed ticks at10ms each,
not ticks read after the handler. Nested handlers/operations cannot overwrite the
outer root's outcome. Test a1.000000123s request interrupted after250ms by a1.5s
INFO handler: remainder stays0.750000123 in both reference/tick models, with
aliased request/rem and separate nested roots. This is required future integration
coverage, not a currently enabled native regression.

## Descriptor and libc ownership

At a public libc operation's logical entry, acquire the existing event lifecycle
lock, resolve pseudo aliases and capture synthetic identity/native fd together.
Keep that lock through BEGIN's native generation capture/output. Close/dup/fcntl
mutators using the same lock cannot replace the alias during capture. Release the
lock before any blocking CALL or cooperative legacy callback. Native direct
close/replacement after capture is detected by the gateway generation comparison.
GenerationMAX is exhausted/unusable and is never treated as an ordinary identity.

Retain native generation expectations and synthetic identity in the operation's
stack-local object, not mutable global last-operation state. Recheck synthetic
identity under its lifecycle lock before each local attempt/readiness commit.
A synthetic consume/publish/readiness commit must use the LOCAL protocol below;
an unmasked local retry cannot manufacture new progress after a delivered INFO.
If the guarded attempt is still would-block, pass the **same ticket** into its
next wait.
Returning EAGAIN is final only for a genuinely nonblocking operation.

Epoll retains its existing watch identity/revision/oneshot checks. A new snapshot
after an explicit watch mutation may capture a different native fd under the same
lifecycle lock, but must not reinterpret old readiness as belonging to a recycled
watch. A retired/spurious snapshot with no delivered event borrows the original
ticket and deadline for its next wait; it cannot start a new logical operation.
Native poll's fixed descriptor list retains initial generations across slices;
negative descriptors and POLLNVAL follow its actual public contract.

All matching-libc blocking entrypoints enroll, even when no INFO is installed
yet: action registration by another thread cannot leave a cached pre-registration
retry unenrolled. Internal functions take an explicit operation pointer/ticket;
they never infer borrowing from an ambient TLS pointer that an INFO callback could
inherit. Raw native syscalls retain their single-call contract; a native caller
implementing its own userspace retry loop must opt into this protocol. An old
libc image never acquires INFO delivery through its old cooperative registration.
Do not claim new-libc logical retry guarantees for arbitrary hand-written raw
assembly. Kernel public delivery/return integrity is independent of that policy.

## Bounded local effects and restoration

LOCAL_BEGIN5 uses args[0]=0 for an incomplete root, or its captured outcome cookie
for FINALIZE_NATIVE; all other body/vector/output fields are zero. It runs after
the lifecycle lock is acquired with INFO still allowed.
It validates exact live/top/depth/root identity and sticky interruption, reserves
a nonreused guard cookie and atomically blocks USR1 before returning that cookie.
The per-thread32-byte guard stores owner_ticket, guard_cookie, previous_mask and
flags ACTIVE/NO_MASK. There is at most one guard. LEGACY_MASKED already excludes
INFO registration process-wide: its guard uses NO_MASK and leaves that root's
previous/temp masks intact. It still owns a cookie and must close normally.

Only bounded callback-free C counter/readiness/metadata effects follow. Release
the already-held lifecycle lock before LOCAL_END; never acquire or wait for a
user lock with the guard active. Refactor eventfd/poll/epoll to use locked helpers
instead of re-locking public helpers. Hoist tracing/getenv/fprintf and kernel
queries outside. Existing socket_poll_update's NET_ERROR query occurs before the
guard with INFO allowed, and only its identity/generation-validated cached flag
stores occur inside. A CAS loop is bounded to32 attempts; close the guard and
yield outside it on contention. No cooperative signal dispatcher, allocator,
callback, fault-resuming user handler or known longjmp can originate in the
supported guarded region; prove that source closure and inspect target code.

For an incomplete ticket, interruption before LOCAL_BEGIN causes EINTR before
local effects. If INFO arrives after blocking, it stays pending until LOCAL_END;
a successful consume/publish is already real progress when the handler runs.
For pure finalization of a COMPLETED_NATIVE result, LOCAL_BEGIN FINALIZE_NATIVE
also supplies its exact captured outcome cookie in args[0]. It permits one bounded
format/cache commit for that result despite a later interruption, sets FINALIZE_USED
before effects and never admits another data/wait call. RESULT and read-only
native metadata queries may run before the guard with INFO enabled; they cannot
replace the captured result or mutate/disarm an epoll watch. Internal epoll/native
slice readiness is not a completed outer epoll result. Finalization retries,
wrong cookies and a second finalization refuse unchanged.

Compact LOCAL_END uses R9=1,RDI6,RSI root ticket,RDX guard cookie,RCX/R8zero.
Classifier uses these immutable registers, never a mutable user request pointer.
Before ordinary I/O admission, an ACTIVE guard filter permits only matching
compact LOCAL_END, THREAD_SELF with zero unused registers, FUTEX_WAKE(address,1) with zero unused
registers for releasing the held mutex,
and terminal EXIT; all other operations, BEGIN/CALL/END/UNWIND/MASK/SUSPEND/fork/exec
included, refuse before an unsafe wait or mutation. Wrong neighbor register
shapes do not reach the restoration helper. The compact helper independently
checks CPL3/IF0, scheduler_observe_user_entry, strict membership/root/TLS/image/TID/
scheduler identity, top/depth, ACTIVE and both cookies. It stages generic mask/
pending/default decisions, restores previous mask unless NO_MASK, and clears the
guard with fixed metadata stores only: no I/O wait, FPU lease, usercopy, callback
or allocation. It does not terminate; outer IDT cancellation may regain ordinary
admission after restoration. Already-killed terminal exit may precede restoration
only because it never resumes userspace. Stale/replayed close refuses unchanged.

Test a held foreign storage owner: matching close must restore without waiting;
wrong registers/cookies/identity/top/depth/ACTIVE must not mutate or enter the
safe helper. Inject INFO before guard, after guard before local effect, after
actual effect before close, and at restored-mask return. Test synthetic data,
epoll oneshot/revisions, sem_wait0 success versus no progress and native positive
result finalization separately. No unmasked check-then-local-effect shortcut is
admitted. The4032-byte total budget includes the guard and four96-byte outcome
slots; compiled size/offset and stack evidence must precede admission.

## Nested callbacks, cancellation, fork and exec

Cooperative legacy callbacks run after releasing event/action/socket locks. Any
logical operation they call allocates its own ticket above the outer one, even
at the same INFO depth. Their END restores the previous top. INFO delivered in
that nested operation marks both inner and outer live tickets. The original
legacy restart policy remains in libc, but cannot override either sticky bit.

The required INFO handler API subset is lock-free TLS errno access, kernel MASK,
SUSPEND, signal send/self identity, monotonic clock, yield, sem_post/sem_trywait
and terminal _exit. SIGUSR1 CONTROL is kernel-owned. Other sigaction operations,
FD/event/socket operations, fork/exec, allocation and APIs that can take interrupted
libc locks return ENOTSUP before acquiring those locks (or are explicitly outside
async handler support). INFO depth is published before calling C and removed only
on normal trampoline return. This v1 does not pretend arbitrary libc I/O is safe
inside asynchronous callbacks. Native raw operations in a handler do not borrow
an outer ticket. If a later version admits such logical I/O, it requires its own
ticket and lock-free descriptor ownership, not removal of this guard.

Cancellation/worker exit/process exit retire all selected tickets before TLS,
stacks or identities can be reused. Unwinding through a live INFO record remains
terminal as specified in the main packet. Ordinary legacy-only fork/exec is not
rejected merely because interrupted I/O owns a ticket. Failed exec preserves all
ticket state; successful exec retires it and old TLS without returning. Fork's
existing active-INFO-record rejection remains. For a permitted sole-main fork,
clone the depth0 logical-ticket stack, numeric tickets, interrupted flags,
deadlines and next counter into the child, and preserve parent state. Clone the
parent native file-generation array after all successful file cloning and before
child publication: inherited libc expectation vectors must still match the same
inherited open descriptions. Child descriptor mutation subsequently advances its
own copied counters independently; no parent mutation changes the child vector.
Initialize the child's private state with fresh image/main identity and an
explicit constructor-only NEEDS_SCHEDULER_BIND flag (scheduler identity0); it is
not eligible for any delivery/gateway yet. In the running child's enter_process,
after scheduler_set_user_context and before usermode_resume, under IF0 bind its
actual nonzero scheduler_current_thread_id, validate the expected image/main
membership and clear that flag. No guessed scheduler identity is published.
An ordinary lookup cannot use this flag to rebind arbitrary stale state. Failed
bind is terminal before user entry; constructor rollback clears copied tickets.
Inherited libc stack objects continue to name the child's own cloned tickets.
Parent and child releases are independent; no ticket is global authority. Existing
synthetic/socket fork refusals remain. Out-of-order/malformed stacks reject fork
before allocation; no new blanket live-ticket prohibition is introduced.

## Supported legacy nonlocal unwind

Preserve the existing64-byte jmp_buf and actual shared longjmp/_longjmp assembly
entry. Before restoring saved registers/RSP, preserve env/value and call the libc
unwind guard with saved targetRSP env[6]. Active INFO remains terminal: the known
entrypoint must not abandon authoritative private records. With no active INFO,
UNWIND4 passes targetRSP in args[0], zero ticket/vector/body elsewhere. Under IF0
validate current identity, canonical mapped target and the complete logical stack
before stores. Drop the top ticket while its owner frame anchor is below targetRSP
on the supported downward-growing single C stack. Stop at the first anchor at or
above targetRSP. Nested anchors must strictly descend at allocation; BEGIN refuses
malformed roots before effects. No heap allocation or destructor/FD release is
needed: these tickets own only bounded metadata and generation expectations.

For example an ancestor setjmp outside read has targetRSP above read's real frame
anchor and abandons that ticket. A setjmp inside a cooperative callback has a
lower targetRSP than the still-live enclosing read anchor; it preserves that read
and drops only nested operations whose frames it actually exits. This uses the
owning C function's frame anchor, not the deeper BEGIN helper's transient trapRSP.
Fork preserves virtual stack addresses, so inherited anchors retain this ordering.
Raw stack switches/coroutines, cross-thread/stale jmp_buf and undefined jumps to
returned frames remain outside this supported contract; they cannot authorize
private INFO restoration.

Apply the same owner-anchor test to an abandoned depth0 LEGACY_READY SUSPEND
slot. Both longjmp forms explicitly do not save/restore signal masks: retirement
leaves the current mask unchanged and transfers it to the jump destination; it
does not pretend to complete normal sigsuspend restoration. The cooperative
dispatcher also publishes its actual stable frame anchor alongside its TLS guard.
Clear signal_dispatching/that anchor only when targetRSP is above the dispatcher
anchor and therefore abandons that dispatcher. A jump within its still-live
callback retains the guard, preventing recursive cooperative dispatch. Both the
root and dispatcher anchors require compiled frame/noinline proof; capture cannot
be delegated to an already-returned helper. Known longjmp from INFO remains
terminal, while ordinary legacy longjmp is supported and does not leak tickets.
UNWIND refuses malformed metadata atomically; the libc guard terminates rather
than jump with uncertain ownership. Tests must retain an outer ticket for a
callback-local jump, drop inner+outer tickets for an ancestor jump, repeat beyond
slot capacity without leaks, preserve dispatcher guard for a callback-local jump,
clear it only on ancestor escape, cover both symbols and fork, and prove no output,
pending or mask mutation on refused cleanup. No jmp_buf ABI enlargement or
unbounded checkpoint side table is proposed.

## Masked-poll legacy compatibility

Do not globally reject previously supported cooperative masked waits. Matching
libc uses BEGIN LEGACY_MASKED (with poll-list flag where applicable) before any
mask change. Under the same IF0 completion as ticket/output publication, require
that the process has no INFO disposition, save/install the temporary mask and
stage generic pending/default effects. Conversely INFO action registration scans
all live tickets and returns EBUSY before effects while any LEGACY_MASKED ticket
exists. Thus a legacy-only masked wait cannot acquire asynchronous INFO midway
through its old cooperative mask/poll window. With INFO already installed,
nonnull-masked ppoll/pselect/epoll_pwait return ENOTSUP before effects. Null-mask
forms use normal logical I/O tickets. Neither path advertises new atomic masked
poll semantics.

END of a LEGACY_MASKED ticket restores its previous mask through the generic
pending/default completion transaction. Known non-restoring longjmp abandons its
metadata and preserves the actual current mask; failed exec preserves it and
successful exec retains the exec mask contract while retiring old tickets. Fork
clones it under the ordinary depth0 ticket rule. Action registration and masked
BEGIN checks serialize under IF0, so neither can slip through the other's check.
Old statically linked libc binaries cannot request INFO through their cooperative
registration and retain their existing behavior. Mixed obsolete precompiled libc
objects are not a matched SDK and cannot claim the new logical-IO contract.

## Required wrapper and race matrix

|Family|Logical owner and retry rule|
|---|---|
|read/write/vector/pread/pwrite|Public call owns one ticket/initial alias; native retries borrow it; positive/partial/EOF final|
|socket receive/send/accept/connect|Public call owns ticket; retain original pseudo/native identity; would-block waits borrow it|
|sem_wait|Zero-FD ticket, atomic successful decrement is final, still-zero retry uses CALL(YIELD) and sticky INFO causes EINTR|
|eventfd|Local state check under lifecycle lock; same ticket across all10ms zero slices; nonblockingEAGAIN final; timerfd is not presently implemented|
|poll/select/ppoll/pselect|Outer deadline and fixed original generations; synthetic/native internal slices borrow; nonnull mask pre-effectENOTSUP with INFO; legacy-only LEGACY_MASKED serialization|
|epoll_wait/epoll_pwait|Outer ticket/deadline retained across spurious or retired watches; nonnull pwait mask pre-effectENOTSUP with INFO; legacy-only LEGACY_MASKED serialization|
|waitpid/nanosleep|One public call owns ticket; preserve returned child/elapsed-time rules; nanosleep EINTR ignores SA_RESTART and keeps actual remainder|
|pthread mutex/join/cond|No sticky ticket; absorb raw interruption under existing API contract, reacquire mutex and retain original timedwait deadline|
|stdio/adapter internal calls|Borrow explicit enclosing I/O operation only where the public call is one logical operation; never TLS ambient borrowing|

For every family inject INFO before BEGIN publication, at its return, after
zero-progress kernel return, in a legacy RESTART callback, after cached restart
decision, during internal deadline calculation and immediately before CALL. The
next CALL must refuse without backend invocation. Inject while the atomic
wait condition arms, while sleeping and at wake; prove wake/no lost notification.
Exercise partial data, real EOF, real outer timeout, internal10ms zero, FD
close/reuse, alias replacement, retired epoll snapshot, readiness then EAGAIN,
independent nested callbacks, all tickets marked, full slots, ticket exhaustion,
stale/duplicate/out-of-orderEND, cancellation, child/parent independent fork,
failed/successful exec and INFO registration during an already-live operation.
Use backend-entry counters in host fixtures and real main/worker guests. Re-run
legacy-only fixtures to prove restart/fork/exec behavior is preserved. These tests
are requirements, not results. Public implementation remains awaiting review.
