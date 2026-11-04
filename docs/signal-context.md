# Directed signal and interrupted-context runtime dependency

Status: internal context/scheduler/VMM helpers, common-return correction and a worker-spawn binding correction are implemented; delivery remains a proposal. The shared-runtime branch owns this dependency; process, VMM and teardown integration require the audit owner's review before implementation. The first acceptance target is one CPU, one process, and a thread-directed asynchronous `SIGUSR1` for a running or safely blocked thread. This is a real signal contract, not a JSC-only callback. It does not promise SMP, synchronous fault recovery, alternate signal stacks, arbitrary context editing, or unrestricted POSIX signal conformance.

## Existing behavior and the gap

`kernel/src/process.c:3541` queues a directed signal on the selected thread and wakes interruptible waits. Thread IDs currently increment in `process.c:3312` and wrap to 2; a slot or bare TID is therefore not a permanent lifetime identity. `userspace/lib/src/posix_signal.c:92` polls and dispatches a one-argument handler; its thread-local reentrancy guard prevents nested dispatch. `sigaction` rejects `SA_SIGINFO` and `SA_ONSTACK` (`posix_signal.c:163`). A compute-bound thread can run indefinitely without this cooperative poll. Exceptions currently terminate at `kernel/src/arch/x86_64/idt.c:215`, so this design does not reclassify them as catchable signals.

Ordinary interrupt/syscall entry saves GPRs and the hardware return frame in `isr.S:157` and `isr_frame.h`; timer preemption is in `kernel/src/scheduler.c:996`. The saved user FXSAVE image is managed by `fpu_trap_enter/exit` (`fpu.c:166`). Scheduler context switches also preserve FS base and address-space state (`scheduler.c:800`). These are the relevant authorities: constructing a context from the sender's thread, a syscall-only poll, or a partial register approximation would not stop a CPU-bound target correctly.

## Proposed ABI and frame

Reserve syscall numbers through the normal shared ABI allocation, not literal private numbers. Add a versioned atomic action operation and a `sigreturn` operation to `shared/include/srvros/syscall_numbers.h` and a new shared signal-context ABI header. Version 1 action input has fixed-width `size`, `version`, `signo`, `flags`, disposition, handler address, trampoline address, mask, and zeroed reserved fields; optional old-action output is validated before publication. Reject unknown sizes/flags, unsupported signals and noncanonical or unsuitable handler/trampoline addresses. Registration, replacement, and old-action reporting are one transaction; a failed copy or validation leaves the former action untouched. `SA_SIGINFO` initially supports directed SIGUSR1 only. Reject `SA_ONSTACK`, `SA_NODEFER`, and unsupported combinations explicitly. Preserve the existing meaning of supported `SA_RESTART` and `SA_RESETHAND`; define their exact interaction with interrupted waits in tests before admission.

Expose a versioned x86-64 snapshot with fixed byte layout: `uint32_t size, version` at 0; `uint64_t flags, token, interrupted_mask` at 8; 15 64-bit GPRs in `isr_frame.h` order (r15 through rax) at 32; `rip`, `rsp`, `rflags` at 152, 160, 168; and a 16-byte-aligned 512-byte FXSAVE snapshot at 176, total 688 bytes. Static assertions must enforce offsets and size. The FXSAVE bytes come from the interrupted thread's saved image, never kernel state. This exposes the register and stack roots and x87/SSE state; it does not claim AVX/XSAVE support. A separately versioned signal-information record contains signum, code, errno, sender PID/UID and validity bits; populate only facts the sending path actually records, and mark unavailable fields invalid. The libc `siginfo_t` and new `ucontext_t` adapter must describe these real semantics rather than claim Linux layout compatibility.

A registered libc trampoline receives `(signum, info*, context*, handler)` in the x86-64 argument registers and calls the actual `sa_sigaction`. The kernel uses the interrupted user stack, preserves its 128-byte red zone, checks arithmetic, canonical user addresses and the complete writable range, and arranges trampoline entry with SysV stack alignment (`rsp % 16 == 8`). The public information/context are snapshots; handler edits do not change resumed registers. Stack exhaustion, unmapped/nonwritable memory, or invalid trampoline must fail closed by a documented fatal delivery, not jump to unchecked memory or silently consume the signal. No alternate stack is claimed.

The authoritative interrupted GPR/return frame, FXSAVE image, previous mask and frame generation live in bounded kernel-owned per-thread records, not the user stack. `sigreturn` accepts the top record's one-shot token and frame identity, checks the current process/image/thread lifetime and nesting order, then restores only that private record. The token is an ordering/replay check, not a security boundary. Never accept user-supplied CS, SS, RFLAGS, FS base, CR3, FXSAVE/MXCSR or kernel pointers for restoration. The syscall path must bypass normal `rax` result writeback after restoring the frame. Deferred or interrupted syscalls retain their existing cancellation and I/O-service handoff rules (`idt.c:167`); deliver only after their result/restart decision is committed, so sigreturn cannot replay side effects or bypass cancellation. A bad token, out-of-order return, or replay is a deterministic error/fatal violation and cannot corrupt another record. The original FPU image must be installed before normal `fpu_trap_exit`, and the thread's existing FS base remains unchanged.

## Delivery, masks and lifetime

Use the actual target's return-to-user frame after trap entry saved its FPU state. Only commit delivery at a safe user-return boundary of that same thread with stable current address space, no kernel/service lock or active unsafe I/O admission, and interrupt exclusion protecting pending/action/record state. The common `iretq` path and syscall path need an explicit shared hook; `isr.S` has direct IRQ entry paths with different frame shapes, and initial entry/resume in `usermode.S:4` and `:55` must be audited separately. Never cast a different direct-IRQ frame into `isr_frame`. A queued signal may wait until the next safe timer return; no latency guarantee applies while interrupts are disabled or the target is in unsafe kernel work. Verify that the actual CPU0 timer route reaches an eligible return even in a pure user loop.

On CPU0, validate and copy the entire bounded frame while mapping changes and scheduling are excluded; `syscall.c:204` currently validates and copies per page and explicitly permits partial copies, so it alone is not transactional. Reuse its checks only with a reviewed, bounded all-pages exclusion or a VMM pin/copy primitive; do not sleep or acquire a blocking service lock with interrupts disabled. Validate the whole range and prepare private records before changing pending bits or the live return frame. If preparation fails, leave action, mask, pending and live frame coherent; fatal delivery cleanup must also release reservations. No handler runs with a kernel lock or borrowed I/O-service admission held. Establish exact lock order and safe-point predicates with the process/VMM owner before implementation; current `scheduler_user_enter` changes service admission (`scheduler.c:504`), so a guessed placement is insufficient.

At commit, atomically consume one eligible pending bit, capture the prior mask, block the action mask plus the signal, and publish the private record and trampoline frame. Normal pending signals retain existing bit coalescing. Existing one-argument actions remain on the cooperative path during migration, but the kernel action mode and libc action table must be switched transactionally so a pending signal is never delivered twice or lost when a disposition changes. Mask changes must use one authoritative per-thread state. `SA_RESETHAND` reset is part of the same commit.

Bound nesting to four private records per thread; if full, retain the signal pending until a frame returns. A handler may call `sigsuspend` with a temporary mask that unblocks the same SIGUSR1, allowing a nested resume handler; do not apply today's `signal_dispatching` suppression to the new path. Atomically install the temporary mask and wait, avoid lost wakeups, restore the previous mask exactly once, and return EINTR after a delivered handler. The sender must not assume queued duplicate bits count as multiple acknowledgments.

The stop/resume handshake must use an async-signal-safe semaphore operation and release/acquire synchronization, with one outstanding suspend request per live target and a distinct acknowledgement for each phase. Current `sem_post` uses an atomic increment (`userspace/lib/src/posix_semaphore.c:57`); prove its implementation and `sem_wait` progress are safe when the interrupted code holds arbitrary user locks, including nested delivery and overflow. No handler may acquire the interrupted thread's application or action-table locks. Target exit, process teardown and a failed send must cause the coordinator to abandon or time out, never wait forever for an acknowledgement. Tests must not mistake a timeout for successful suspension.

Use nonreused process-image and thread-lifetime generations in private records and delivery lookup, not merely the reusable slot, process job-lifetime token, or current wrapping TID. Allocate generations with explicit exhaustion failure. Nonlocal exit from a handler (`longjmp`, cancellation, or language unwind) cannot silently strand a private record: version 1 requires normal trampoline return, and these paths must fail closed or use a separately reviewed explicit unwind operation before they are supported. On thread exit invalidate pending records and wake/cancel dependent waits before memory reuse. On exec invalidate all records and registered caught handlers before the new image becomes runnable; handle ignored dispositions according to the existing contract. Fork must copy the coherent disposition/mask state but not another thread's pending frames; define and test behavior of fork or exec from an active handler before enabling it. Teardown, detached-thread reuse and signal/send races need process-owner review. Never transfer a saved frame to a new image or thread.

## Files and acceptance

Expected implementation ownership: shared syscall/context headers; `kernel/src/process.c` and its interface for action, lifetime, masks and pending state; `kernel/src/arch/x86_64/{isr.S,isr_frame.h,idt.c,syscall.c,fpu.c,usermode.S}` for audited return and private state; scheduler/VMM interfaces only where necessary for admission and transactional copy; `userspace/lib/{include/signal.h,src/posix_signal.c}` plus a real x86-64 trampoline and syscall wrappers. Exact allocator, executable-address validation and lifecycle hooks require owner review against the implementation, not assumed existing APIs. Existing pthread/kernel ownership remains with its maintainers; no parallel implementation is authorized by this proposal.

Phase 1: host tests for ABI layout, canonical/overflow validation, action transaction rollback, mask/nesting state machine, cookie replay and generation exhaustion. Include forced copy failure across a page and teardown between send and delivery. Phase 2: private CPU0 VM tests for CPU-bound directed delivery without libc calls; capture known GPR, stack and SSE patterns, restore them through a handler that clobbers them, and assert stack alignment, FS/TLS isolation and repeated returns. Exercise blocked masks, replacement, `SA_RESETHAND`, restart/EINTR, nested `sigsuspend` resume, pending coalescing, invalid/guard-page stack, frame-depth exhaustion, invalid sigreturn, signal-vs-exit, exec, fork and TID/slot reuse. Verify real sem handshakes and a root reachable only through interrupted registers or stack remains visible. Retain explicit timeouts and logs; test an isolated guest, not a shared VM.

Only after these pass should JSC's suspension and conservative-root scan be integrated and separately tested. A kernel-owned thread snapshot API could avoid signal-handler nesting, but would require its own stop/resume and lifetime contract and is not a substitute for the agreed generic signal/context design. No success is implied by this document.

## Review gates before implementation

The independent review found the snapshot offsets consistent, but requires the ABI type and each copied frame to be explicitly 16-byte aligned. Map each GPR field individually from the real ISR frame; its vector/error/selector fields are interleaved and a whole-struct copy is incorrect. The implementation must identify the exact private FPU image changed by sigreturn before the matching trap exit; verify x87, MXCSR and XMM across nesting and scheduling. A writable-address check alone is insufficient if COW or another fault can occur during the excluded copy: establish a bounded fault-free preflight/pin/copy transaction with the VMM owner.

Action selection must not acquire an interrupted libc action-table lock. Proposed resolution: the kernel owns the new-mode action descriptor and copies a stable selected handler, flags, mask and trampoline into its private delivery record; the public frame receives those selected values only after complete validation. Define how the legacy libc table and kernel mode change atomically and how cooperative polling skips an asynchronously consumed signal. Existing one-argument behavior stays unchanged. Initial new-mode flag admission must be an explicit supported subset, with unsupported combinations rejected before mutation. In particular, accepting SA_RESTART requires a real per-thread delivered/restart notification so existing post-I/O dispatch cannot lose a handler that ran before libc resumed; finalize this protocol before admitting the flag.

Nested sigsuspend needs its own private temporary-mask/wait record. A nested sigreturn restores that temporary mask; completion of the outer sigsuspend restores its pre-wait mask exactly once. Specify full-depth pending behavior, failed frame setup and exit atomically so no mask can leak or unblock early. Public context/info pointers must identify the exact committed aligned copies for their lifetime; canonical addresses alone do not establish suitable executable trampoline/handler mappings or stable callback publication.

WTF integration is a separate required code change after primitive verification. Upstream ThreadingPOSIX.cpp:575-625 waits indefinitely on one global semaphore; target exit after a successful send can strand GC. The adapter must serialize target publication, identify the live request/phase and use a bounded acknowledgement/exit protocol. Proposed conservative failure rule: any uncertain suspension, timeout, stale acknowledgement or failed resume terminates the process; do not return an ordinary failure that MachineStackMarker can interpret as permission to skip a live mutator's roots. A timed-out process must not issue a later request that could consume a late acknowledgement. The exact deadline/exit wake mechanism requires review. Implement a real SrvrOS PlatformRegisters/MachineContext view and original interrupted SP/red-zone handling; do not enable HAVE(MACHINE_CONTEXT) on the approximate fallback.

Nonlocal handler unwind is outside version 1. The implementation must reject known unsupported context/fork/exec operations while an active record exists, or specify their actual cleanup, before enabling them. A declaration cannot detect arbitrary user assembly or every longjmp: do not promise universal automatic detection. Kernel-owned restore state and bounded nesting must remain safe even when user code violates normal trampoline return; the collector handshake must fail terminally instead of continuing with absent roots. These are implementation/review gates, not claims that the current SDK provides such enforcement.

## Proposed first implementation slice for approval

Start with the explicit aligned wire layout, individual ISR-to-snapshot field mapping, private-record state/validation helpers and host tests. Do not admit SA_SIGINFO, advertise a capability, install a callback or connect WTF in this slice. This permits review of the actual production helpers before enabling asynchronous control transfer. Allocate syscall numbers and publish feature admission only with the delivery/return implementation and passing primitive tests; absent support must continue to reject the request honestly.

For delivery, the proposed narrow hook is the common CPL3 return in `isr_dispatch`, after `isr_dispatch_inner` fully returns and before the final user transition. The syscall cancellation wrapper must already have committed its result and released backend ownership. Do not add signal work to the differently shaped direct keyboard IRQ frame; leave its signal pending for a later common timer return. Do not deliver in `usermode_enter/resume` until those paths have a separate proof. Proposed admission predicate: current thread/image still live, actual frame is CPL3, cancellation does not take precedence, IRQ exclusion is held, and `scheduler_io_active()` is false. If an I/O owner exists, retain pending state and return; never wait for service ownership inside this hook. The process/VMM owner must approve this exact placement and predicate.

For FPU state, the user trap's authoritative image is `fpu.c`'s per-thread `current_user_state`, saved by `fpu_trap_enter` and restored by `fpu_trap_exit`; the assembly-local trap area serves kernel traps and must not be treated as the user's context. Add reviewed no-SSE snapshot/replace helpers in that module, valid only in the entered CPL3 trap state. Sigreturn replaces the current user's saved image from the private record, then the existing matching trap exit restores it. Do not call FXRSTOR on user data while C kernel code is still executing, and do not use `process_fpu_state()` for worker threads because it returns the initial process image. Scheduling must preserve the exact thread-selected image across any permitted boundary.

Keep the initial public frame at a statically bounded size of at most 1 KiB. With IRQs already excluded and no I/O owner, preflight every covered page for canonical user/write access and the whole frame's overflow, alignment, red-zone and active-frame overlap constraints, then perform the bounded copy without allocation, COW resolution, faulting work, interrupt reenable or scheduler callbacks. The current generic copy preserves an inherited masked state, but the all-pages preflight is additional and mandatory. This requires proof that the accepted mappings are resident and writable for the entire excluded transaction; otherwise reject or defer before committing pending/mask/frame state. Unexpected user mapping failure must terminate delivery safely rather than resume with a partially published handler.

The exact action publication, restart notification and nested-wait mask protocols remain review gates for the later enabling slice. No change to legacy action admission, polling, syscall-safe classification or cancellation is authorized by this first slice. Fork/exec from a new-mode active handler will initially be refused rather than attempt to clone a live saved-return record. Normal thread/process exit still needs explicit record invalidation and waiter failure handling before enabling delivery. Tests must establish that late or stale state cannot attach to a reused slot/image.


## Reviewed internal helper slice — November 3, 2026

The process/VMM/teardown owner reviewed design `d71c254c` and explicitly approved
only aligned internal layouts, individual ISR-field mapping, pure private-record
validation helpers and host tests. Their required conditions are 16-byte alignment,
CPL0 rejection before user-only field reads, unchanged state on validation failure,
generation exhaustion and replay refusal. They did not approve delivery, syscall
allocation, FPU/return-path changes, public `SA_SIGINFO` admission or capability
publication. The proposed `!scheduler_io_active()` gate alone was rejected because
the syscall wrapper already calls `scheduler_user_enter()` before the outer hook.

`kernel/src/arch/x86_64/signal_context.{h,c}` now implements that approved slice.
The 688-byte snapshot explicitly maps each of the 15 GPRs, RIP/RSP/RFLAGS and the
supplied 512-byte FPU image. Pure address planning reserves the red zone, aligns
entry/snapshot addresses and refuses arithmetic errors or overlapping active
frames. Planning does not validate mappings or publish a frame. A four-record
private LIFO checks image/thread identity, increasing one-shot tokens and exact
frame identity. Pop clears the retired record and retains the issued-token high
watermark. Generation allocation saturates rather than wrapping. No live process
owns these records yet; no production call site invokes the helpers.

`make test-signal-context` passes on the ARM64 macOS host with strict warnings.
A separate AddressSanitizer/UndefinedBehaviorSanitizer build passes. Tests include
distinct register/FPU patterns, exact offsets/alignment, invalid selectors and
flags, rejected operations leaving outputs/state unchanged, red-zone/overlap
planning, depth limits, wrong ownership/order/replay and generation exhaustion.
A real protected page immediately after a CPL0 ISR prefix proves capture refuses
before reading absent user RSP/SS fields. The actual Makefile x86-64 freestanding
object target also compiles with pinned Zig 0.15.2, `-mno-sse -mno-sse2`, separate
caches and at most two jobs. Evidence is retained under
`build/signal-context/`; this is host/object evidence, not kernel/guest delivery
acceptance. The helper target is included in `test-core`.

Next: source-level review of the common-return scheduler transition, actual
entered-user FPU image ownership and fault-free transactional copy. The owner
explicitly requested these stay as design until that review finishes. The
existing browser smoke still fails honestly at mandatory `SA_SIGINFO` registration;
the internal helper slice does not change that result.

## Next-slice source review packet (not approved or implemented)

This replaces the earlier proposed `!scheduler_io_active()`-only admission rule.
Source references below use the prerequisite tree `af470fff` plus the unwired
helpers at `1d7c62b6`. Fresh main `b78e3ed5` has no kernel/shared/lib source diff
from the main prerequisite `89c210db`; PR317's later driver work is separately
owned and is not implicitly consumed.

### Return routes and proposed transition

| Route | Existing source behavior | Proposed delivery boundary |
| --- | --- | --- |
| Common user syscall | `idt.c:167–211` gates cancellation before dispatch, after dispatch and after unsafe-call handoff; its final user transition duplicates `isr_dispatch:315` | Preserve all checks/result writes/handoff; remove only the inner final user transition, then use the common outer boundary |
| Common user timer/IRQ | `isr.S:157–213` saves the full ISR frame and enters FPU trap handling; `idt.c:240` dispatches IRQ; `irq.c:90–97` handles timer, sends EOI, then may schedule | Finish every callback/EOI and return from inner dispatch before testing admission; revalidate actual current scheduler state after any scheduling round trip |
| Common CPL0 entry, including a nested timer | No user RSP/SS is guaranteed; FPU uses assembly-local trap storage | No delivery, user-state access or user transition |
| Direct legacy IRQ1 | `isr.S:97–155` has CS at offset 128, no vector/error pair, and separate FPU enter/exit | No cast or delivery; retain pending until a common return |
| Direct legacy IRQ0 | `isr.S:77–95` saves only RAX/RDX and bypasses common FPU handling | No delivery; this route is not a full context |
| Initial entry and fork child resume | `usermode.S:4–94` calls user/FPU entry and constructs/restores an iret frame independently | Leave these entry transitions intact; defer delivery until a common return |
| Successful exec or terminal exit | Does not return normally to the original common continuation | No use of a stale original frame; lifecycle invalidation remains a separate required implementation |
| Fault containment or cancellation | Can terminate the thread/process instead of returning | Keep terminal priority; no callback after terminal cleanup |

At entry to `isr_dispatch`, only for a real CPL3 frame, account kernel time and
mark the current continuation `io_user_safe=false`. Marking only syscall entries
is insufficient: timer returns can also resume user code while another I/O owner
exists. At the fully unwound common outer boundary, explicitly exclude interrupts,
then test scheduler admission before accessing process signal state. Call the
bounded process operation only if admitted; otherwise retain pending state. End
with exactly one `scheduler_user_enter()` before normal assembly FPU exit/iret.
The existing temporary safe/unsafe pair around `scheduler_io_handoff()` remains;
it grants a scheduling turn after syscall work has unwound, not a signal callback.

The proposed narrowly named scheduler predicate must establish initialized CPU0
scheduler state, no switching, valid current RUNNING slot with user context,
`io_user_safe=false`, kernel accounting state, no global I/O owner/depth, and
current CR3 equal to that slot's actual user address space. It must also identify
the current scheduler lifetime and selected user-FPU pointer for comparison by
the caller; it must not infer worker state from the process's initial FPU image.
The callback cannot allocate, wait, schedule, acquire a service lock or change
address space. This design intentionally defers under either local or foreign
I/O ownership: `scheduler_io_wait()` admits its current owner and cannot prove
zero depth. No predicate may silently release another owner's scope.

`main.c:539` requests LAPIC vector 32, covered by the common stub table and
`IRQ_VECTOR_BASE` dispatch. This establishes the source route, not guest progress:
CPU-bound delivery through actual vector 32, deferral under I/O contention and
continued timer progress still require matched-guest proof before admission.
The old cancellation host fixture currently calls the inner wrapper directly
and expects it to enter userspace. It must instead check a still-unsafe inner
return and exercise the outer boundary, preserving pre/post-call/handoff kills,
result value, one user accounting transition and no CPL0 callback. Scheduler
fixtures must refuse each invalid slot/CR3/ownership/admission state.

### Actual saved FPU ownership

`fpu.c:166–182` saves the interrupted user image only on a real CPL3 entry when
FPU is available, `user_state_loaded` is true and `current_user_state` exists.
A later false `user_state_loaded` alone proves nothing: initialization, kernel
state selection and `fpu_switch_kernel_state()` also produce it. The proposed
accessor therefore needs an internal, stack-lived proof from that successful
entered-user save, bound to the exact common frame, scheduler lifetime and saved
user-FPU pointer. No user input can supply that proof. After a scheduler round
trip, admission must revalidate the same continuation, owner and selected pointer;
exec/exit or a changed image must invalidate it. Direct/initial/kernel entries
cannot fabricate this proof. The precise representation is still for owner review.

Snapshot/replace must refuse unavailable, uninitialized, stale, mismatched or
already-loaded state without mutation, copy exactly 512 bytes with the no-SSE
profile, and leave FS base and kernel FPU untouched. Restore replaces the saved
current user image only after private top-record/lifetime validation; the existing
`fpu_trap_exit()` performs FXRSTOR after C returns. Host rejection/copy tests and
actual x87/MXCSR/XMM main/worker patterns across timer/syscall/nesting/scheduling
are required; a compile-only check is insufficient.

### Whole-frame preflight and publication

Hold IRQ exclusion and scheduler admission over the complete nonzero, at most
1 KiB frame transaction. Validate frame arithmetic, lower-half canonicality,
red-zone/alignment, no overlap with active frames and the exact saved owner/frame.
`vmm_entry_for_address()` currently checks PRESENT at every level and intersects
USER/WRITABLE across all levels, including large-page leaves; use whole-range
`vmm_user_range_mapped()` before the first store. A present resident writable
mapping can be accepted; missing, nonwritable or future COW/demand-fault mappings
must refuse before writing. This proof relies on CPU0 mapping exclusion and no
mapping-mutating callbacks; owner review must confirm every accepted mapping is
fault-free and stable over this exact boundary. Hardware memory faults are terminal.

The generic page-at-a-time `copy_user_bytes()` explicitly permits a prefix and
is not a publication primitive. Only after whole-range validation may a bounded
scalar copy run without allocation, interrupt reenable or callbacks. Prepare the
private record and all public bytes before consuming pending state or replacing
the live return frame. Failed preparation leaves action/mask/pending/frame state
unchanged. Validate the complete old-action output before registration changes.
The existing walker does not propagate effective NX: executable handler/trampoline
checks need an explicit reviewed all-level NX rule and actual execution-protection
state, rather than treating readable memory as proof of executable admission.

Action publication, nested temporary-mask waits, per-invocation restart sequence
tracking and image/thread lifecycle remain enabling gates even after this narrow
source review. No new syscall number or public flag is allocated by this packet.


## Approved scheduler observation slice — November 3, 2026

After independently rerunning the first slice's strict and sanitized host tests,
the owner found no blocking helper defect and approved one next bounded slice:
an internal, unwired, read-only scheduler user-return observation plus actual
scheduler host-fixture coverage. The transition/FPU/VMM/process/syscall/libc
changes above remain unapproved and unimplemented.

`scheduler_observe_user_return()` now refuses unless IF is already clear,
scheduling is initialized and not switching, the current slot is in bounds and
RUNNING with a nonzero lifetime identity, user context, actual nonzero user CR3
matching the running CR3 and selected user-FPU pointer, both accounting/user-safe
flags are false, and global I/O depth is zero regardless of owner. This uses the
existing CPU0-only scheduler assumption, not an SMP synchronization claim. An
initial thread may have a NULL user-thread context. Outputs stay unchanged on
refusal. Success returns opaque process/worker/FPU identities, actual CR3 and the
immutable scheduler lifetime. The query neither dereferences process/FPU state
nor establishes image generation, cancellation, pending-signal or saved-FPU proof.
Its observation expires if the uninterrupted IRQ-excluded continuation ends.
There is no production caller.

The existing actual-source I/O-scheduler fixture passes strict x86-64 macOS/
Rosetta compilation/execution and ASan/UBSan. Added cases independently refuse
IF-set, absent initialization, switching, invalid/dead/ready/kernel slot, missing
identity/CR3/FPU, wrong CR3 including a temporary kernel CR3, user accounting or
user-safe admission, and both local and foreign I/O ownership. Initial/worker
positive cases return distinct opaque selected FPU identities. Each call checks
unchanged thread/scheduler/I/O/IRQ/clock/switch/free state and refusal output;
all existing fairness, handoff, growth, accounting and teardown fixture cases
continue to pass. The actual freestanding scheduler object compiles with Zig
0.15.2 and isolated caches. Evidence: `build/signal-context/scheduler-{host,
sanitizers,target}.log`. The host command uses `cc -arch x86_64` with strict
warnings, section splitting, the shared/kernel/Limine includes and Darwin
`-Wl,-dead_strip`; sanitized verification uses `-O1 -g
-fsanitize=address,undefined -fno-omit-frame-pointer`. This does not change the
Makefile's existing Linux host target or claim a matched guest result.

## Specific FPU proof representation for review only

Proposed prerequisite: allocate a nonreused process-image generation when an image
is committed; mirror that identity in its scheduler user binding alongside the
selected user-FPU pointer. A failed exec leaves the current binding unchanged;
new main/worker bindings carry the actual committed generation. This is a proposed
lifecycle/API change requiring process-owner review, not a field already provided
by `scheduler_observe_user_return()`.

Proposed representation: a private proof of size 64 bytes and alignment 16 on the *common*
ISR's kernel stack (`sizeof == 64`, `_Alignof == 16`), containing the exact ISR-frame pointer, scheduler lifetime,
actual CR3, committed image generation, selected user-FPU pointer, owning kernel-
FPU pointer and validity/consumption state. Reserve an additional 64 stack bytes
beside the existing private 512-byte kernel-trap area: retain saved RSP at offset
0, kernel-trap FXSAVE bytes at 16–527 and proof at 528–591, total reservation
592 bytes after the existing 16-byte RSP alignment. Static assertions define
these offsets; this proposes no 64-byte alignment guarantee. Add a kernel-only active-proof pointer associated with each
thread's kernel-FPU state, rather than a single CPU-global proof. Stable scheduler
records already retain each kernel-FPU state across growth and context switches.
This pointer is internal bookkeeping, never user restore input.

Only a new common-entry FPU wrapper may mint the proof. In the existing no-SSE
module, first remember whether this is CPL3 with available, initialized, loaded
user state and an initialized selected kernel state. Perform the *actual*
`fpu_trap_enter()` FXSAVE/kernel-FPU switch before calling any ordinary C scheduler
binding helper: such a helper may emit SIMD and must not run before the save.
Then obtain the IRQ-excluded current scheduler binding (no process dereference),
require its selected FPU pointer to match the image actually saved, and bind all
identities to the exact common frame before publishing the active-proof pointer.
That entry-binding query cannot use the return-admission query: user accounting
and user-safe flags are still set at pre-dispatch entry. Its exact API is another
review gate. A refused proof still follows normal FPU entry/exit, but cannot admit
signal work. The current void entry API is unchanged until this is reviewed.

During a scheduler switch, do not invalidate a suspended thread's proof: select
its own kernel-FPU state and active-proof association when it resumes. Nested CPL0
and direct IRQ handling continue using their private trap area and cannot mint,
replace or clear the common proof. At common return, require a fresh successful
return-admission observation, the same active proof pointer and exact frame,
scheduler lifetime, CR3, image generation, kernel-FPU owner and selected user-FPU
pointer. Also require initialized current user state and not-loaded hardware
state. Pointer equality or `user_state_loaded=false` alone remains insufficient.
The proof cannot outlive this common continuation or be cached by a caller.

Normal common FPU exit clears its own active association before restoring user
hardware state. Successful exec/context replacement and terminal exit clear the
old association before changing/releasing the binding; failed exec preserves it.
Fresh kernel-FPU initialization clears it on thread-slot reuse. Fork's new
scheduler/kernel-FPU state starts with no association. Initial entry and direct
legacy returns have none. These invalidation hooks must be reviewed together with
lifecycle changes; a declaration alone does not establish them.

Capture copies the authoritative saved user image into a kernel-owned record.
Sigreturn replacement accepts only a validated top private record for this same
live binding; it never FXRSTORs while C runs. Mark the current proof consumed
when a replacement or delivery commits, and defer a further signal to a later
common entry instead of reusing a pre-replacement proof. Normal assembly exit
then restores the trusted saved image. Refusal leaves outputs and saved images
unchanged. Host fixtures must interleave actual entered-user save, nested kernel/
direct traps, scheduler A→B→A selection, failure, image replacement and stale proof
attempts; guest x87/MXCSR/XMM/FS tests remain mandatory. This representation is a
review proposal; no FPU structure, assembly path or lifecycle hook implements it.


## Approved standalone common-return transition slice

The owner independently reran query `c03372ee` strict/sanitized scheduler tests
and found no blocking query defect. They approved a standalone return-transition
correction with regression coverage, preserving all other enabling gates. This
candidate removes only the inner syscall wrapper's final user transition, marks
real common CPL3 entry unsafe after kernel accounting, and explicitly excludes
IRQs before the sole outer user transition after complete dispatch. CPL0 nesting,
direct legacy IRQ and initial/resume entry paths retain their transitions. No
signal callback or production admission-query caller is introduced.

All cancellation checks, results and I/O waits remain. The owner explicitly
confirmed the existing intentional admission during an audited safe syscall and
the safe/handoff/unsafe window after an unsafe syscall must both remain; the
correction does not widen safe classification. Tests retain prior kill/side-
effect/handoff assertions and verify actual outer syscall/IRQ/spurious/breakpoint
routes, nested CPL0 state, IF state/final exclusion, one accounting/user transition,
terminal exits and result writeback. Actual scheduler preemption selects an
eligible service and the foreign I/O owner while the interrupted continuation is
unsafe/ineligible; only that owner ends its scope, then the continuation resumes
unsafe before its final user transition. Strict x86-64/Rosetta and ASan/UBSan
cancellation and full scheduler fixtures pass. Test-only IRQ/CR3 seams preserve
production instructions. Logs: `build/signal-context/transition*.log`.

A fresh full kernel and the selected thread/syscall/I/O test programs compile
with GNU Make 4.4.1, pinned Zig 0.15.2, separate caches and at most two jobs.
The failed initial old-Make build and missing-linenoise dependency attempt are
retained; only their generated userspace objects were rebuilt. Matched disposable
thread/syscall/accounting and I/O-contention guest verification is pending before
acceptance. FPU/copy/image-generation/action/restart/nested-wait/lifecycle and
public `SA_SIGINFO` support are still absent; no JSC success follows from this
transition correction alone.

### Matched transition verification at fa69b4dc

Clean code `fa69b4dc` passes the fresh matched Q35 full threadstress suite,
including CPU/thread/child accounting, times, targeted legacy signals and all
existing synchronization/lifecycle cases; top-level status is zero. Fresh sysabi
passes size/copy checks and faulting process/worker reaping. The sequential OVMF
AHCI/USB contention proof passes throttled writes with typing, Ctrl-C after real
I/O admission, copy comparison, fsck, three executable relaunches, then deliberate
timeout/quarantine and failed-sync refusal. These are fresh selected userspace
binaries from the same source, not retained PR313 files or a complete OS payload.

Receipt: `build/signal-context/vm-1793763902047506000/receipt.json`, SHA256
`f7a9e2a4f77de45806082c976f280bb022b079700b7780265a9e0b3727f06c77`.
It binds kernel, apps, pristine disk, ISO, harnesses and result logs. A wrapper
changes temporary-file retention only; failed disks remain, success disks are
removed after the harness stops its guest. Two initial I/O launches failed before
boot because the evidence path exceeded macOS's UNIX-socket limit; their logs and
images remain. Short temporary sockets and the explicit Homebrew OVMF path fix
that staging issue without changing checks. All task guests stopped; review VMs
61610/6233/85869 remain live and untouched. No physical/SMP or asynchronous signal
acceptance follows. Send this code/test result for owner review before new hooks.

## Image-lifetime and executable-query packet for owner review only

These proposals do not authorize fields, entry hooks, syscall allocation or copy
publication. Current code still lacks a committed image identity and real FPU
entry proof. Expected implementation remains on this dependency branch with the
process/VMM/teardown owner's review.

### Committed image and binding lifetime

Add a nonzero `uint64_t image_generation` to the process's committed image and
mirror it in the scheduler's user binding; zero denotes no committed image. A
CPU0 IRQ-excluded saturating allocator never reuses issued IDs. Reservation may
burn an ID after a later failed construction, but must not change the old image
or output on reservation failure. Job lifetime (`process.c:221,837–862`) survives
exec and may be shared; PID/TID, CR3, FPU pointer and scheduler lifetime are not
image identities. No dynamic registry is proposed.

| Source boundary | Proposed ordering requiring review |
| --- | --- |
| Slot allocation (`process.c:837–882`) | Start image generation at zero; do not publish an image merely because its slot exists |
| Initial/foreground/background/spawn construction (`:1852–2113`, `:2246+`, `:2332+`, `:2516+`) | Complete load/arguments/actions, reserve and commit a fresh ID before any binding, user entry or READY publication; refuse and clean staged resources on failure |
| Fork (`:2294–2323`) | Child gets a fresh image ID even for identical addresses/FPU bytes; no copied active proof |
| Exec (`:2172–2243`) | Reserve before irreversible stdio/binding mutation; failed load/setup/reservation leaves old identity/binding/proof unchanged. At commit invalidate old proof and detach binding before old CR3/mappings/FPU backing change, then move staged image plus fresh ID and publish new binding. No fallible allocation after retiring the old identity |
| Worker creation (`:3290–3355`, scheduler spawn) | Inherit committed image ID; fresh scheduler lifetime and empty proof before READY |
| Worker/main exit and final teardown (`:3399–3428`, `:7412–7447`) | Revoke proof admission and invalidate associations before TLS/FPU/CR3 release or slot reuse; main pthread exit's atexit continuation does not itself retire the whole live image |

The concrete API should pass the committed generation explicitly into scheduler
binding publication and worker publication, not dereference an opaque process
from an FPU entry helper. Audit every `scheduler_set_user_context[_fpu]` caller.
Foreground A→child B→A may use the same scheduler thread and restore A's unchanged
image identity; every actual setter/clear must invalidate the prior active proof
before changing the binding, and must never revive the old association on A's
restoration. Ordinary scheduler A→B→A context switching retains a suspended
thread's own proof instead. Failed exec preserves its old binding/proof; a setter
called on a separate failed-spawn restoration may conservatively invalidate it
and defer signal work to a fresh common entry.

The owner prefers mandatory active-association invalidation on every setter/clear
without introducing a fallible binding-epoch allocator. Preserve the existing
void binding API's role: generation reservation belongs to staged image creation,
not parent restoration. Pass or mirror the committed image ID explicitly, and
invalidate observations across every binding setter even when process/CR3/FPU
pointers are restored to their previous values. This preference does not approve
lifecycle/FPU fields or hooks; the exact commit/rollback packet remains required.
Also audit existing exec stdio failure atomicity rather than asserting it by
declaration. Directed bare-TID wrapping remains a separate enabling gate.

Required actual-source cases include failed/successful exec and reservation
exhaustion, failed stdio preparation, fork/worker empty proof state, slot/CR3/FPU
pointer reuse, foreground binding replacement versus pure scheduler switching,
main/worker exit, delayed service cleanup and stale-proof refusal. Memory cost is
bounded process/scheduler scalar identities and the fixed private proof; no
unbounded allocation or per-delivery service window is permitted.

### Read-only executable mapping query before any copy path

Propose internal unwired `vmm_user_exec_current(address, expected_cr3)`: require
already-excluded IRQs and the expected nonzero actual user CR3, a nonzero canonical
lower-half entry address, all-level PRESENT/USER, and effective NX clear. Compute
NX by OR over PML4/PDPT/PD/PT, including ancestors of 1 GiB/2 MiB leaves; retain
existing AND semantics for USER/WRITE. The present walker propagates only leaf NX,
and the tracked process leaf check alone does not establish effective execution.
The query is about the entry mapping, not the extent or validity of an arbitrary
handler's instructions.

Fail closed unless CPU extended CPUID advertises NX and actual EFER.NXE is set;
never enable NXE or modify permissions as a query side effect. No kernel source
currently establishes that policy explicitly, so measure it in an isolated guest
rather than infer it from Limine or successful boots. Host seams must model both
CPUID support and EFER, plus IF/CR3. Independently vary PRESENT/USER/NX at every
level and large leaf, test invalid/canonical/zero/root cases and unchanged state,
then compile the actual freestanding object. No live action/delivery caller or
public capability is proposed by this query slice.

Whole-frame copying remains a later separate review: the complete bounded range
must preflight resident effective writable mappings and exact ownership before
any store, under uninterrupted CPU0 IRQ exclusion and no I/O owner/callback/
scheduling/allocation. Define behavior for aliases, future COW/demand paging and
actual CR0.WP/SMAP policy before claiming fault-free transactionality. Missing or
nonwritable second pages must leave destination and pending/private/frame state
unchanged. Existing per-page syscall copy remains insufficient and unchanged.


## Approved unwired executable query

The owner accepted the transition code/evidence and independently reran its
strict/sanitized actual-source fixtures and verified the receipt's source and
artifact/log hashes. They then approved an internal unwired executable-entry query,
actual VMM host tests and target-object compilation. No copy/delivery or lifetime
hook was authorized by that approval.

`vmm_user_exec_current(address, expected_cr3)` requires IF already clear, a
nonzero lower-half address, a nonzero aligned expected root without policy bits,
a nonzero matching *raw* CR3 base, a nonkernel root and a used registered user-root
record. Registry membership establishes allocation bookkeeping, not image
identity. It refuses absent extended CPUID/NX before RDMSR or page-table reads,
then requires actual EFER.NXE. It never enables a policy or mutates a mapping.
The private walker now ORs NX over every used level while retaining USER/WRITE
AND semantics, rejects the unsupported PML4 large-page shape and reports bounded
private leaf size. The new query additionally requires CPUID Page1GB for a 1 GiB
leaf; lack of that bit does not reject valid 4 KiB/2 MiB entries. Existing public
range/map behavior is preserved, including readable NX pages. These checks follow
[Intel SDM volume 3A, sections 4.1.4 and 4.6](https://cdrdv2-public.intel.com/819714/253668-sdm-vol-3a.pdf).
This is entry-mapping permission observation within one excluded CPU0 continuation,
not pinning, arbitrary instruction validation, image ownership or future callback
safety. No production caller exists.

Strict native ARM64 host and ASan/UBSan actual VMM fixtures pass all original
noncanonical/range/teardown regressions plus independent PRESENT/USER/NX mutations
at each level and all three leaf sizes, realistic IF-set/clear flags, mapped zero
versus nonzero entry, the highest valid user byte, next-half refusal, root/registry/
raw-CR3 failures and independent NX/Page1GB/NXE capability combinations. Missing
capability never reads EFER/tables. Every call checks unchanged tables, registry,
CR3/EFER/IRQ policy and allocation/free/switch state. The actual freestanding VMM
object compiles with pinned Zig 0.15.2. Logs are
`build/signal-context/vmm-executable-{host,sanitizers,target}.log`; no live-query
guest invocation is claimed.

A separate read-only CPUID program actually ran in the matched Q35/qemu64 guest
and exited zero: NX is present and Page1GB is absent. QMP register readback in that
same guest reports EFER.NXE=1, CR0.WP=1, CR4.SMAP=0; this is distinct from the
previous QEMU model-property observation. Receipt:
`build/signal-context/vm-1793763902047506000/cpu-probe-2/receipt.json`, SHA256
`a586b0a4e926beb79b87f96bc801b0faee67ff9da9510903ed416b78ef500848`.
The probe uses kernel code `fa69b4dc`, not a hooked query, and changes no CPU
policy. Its first command reader stopped on an echoed prompt before execution;
that failed attempt remains retained, and the successful reader requires the
process exit receipt. Both guests stopped. This does not establish physical or
other virtual CPU policy, executable-query runtime behavior, or signal support.


The exact next identity/association metadata slice is specified in the
[image commit and invalidation packet](signal-context-image-lifetime.md). It
includes all constructors, exec stdio preflight/rollback, foreground restoration,
noncurrent teardown and invalidation before kernel-stack reuse. Binding commit
exclusion and the current void mapping-refresh preparation contract are explicit
owner decisions. It introduces no binding epoch and remains design only.


## Approved unwired kernel-mapping preparation

The owner accepted query `6fb08496`, independently reran its strict/sanitized
actual-source fixtures and verified the read-only CPU probe. They approved a
separate internal preparation helper so later binding commits need not replay
the allocating, failure-ignoring legacy refresh. No legacy caller or lifecycle
change was approved by this slice.

`vmm_prepare_user_kernel_mappings(target_root)` requires existing CPU0 IRQ
exclusion, a presently registered aligned nonkernel user root (staged/inactive is
allowed), initialized kernel-root bookkeeping and at most 128 registered kernel
mappings. Before any target write it validates every registry entry against the
kernel root: canonical upper/page-aligned virtual address, aligned physical page,
supported supervisor flags, matching effective RW/NX, exact leaf PWT/PCD and
physical address including large-leaf offsets. It refuses unsupported PAT while
distinguishing 4 KiB bit7 from large-leaf bit12; hardware A/D is ignored. This is
leaf PWT/PCD validation, not complete PAT/MTRR memory-type certification. Used NX
requires actual CPUID NX and EFER.NXE; used 1 GiB leaves require Page1GB. Empty or
no-NX preparation does not itself require NX support.

After complete preflight, an inactive target receives exactly the kernel's 256
upper PML4 entries. Lower 256 entries, registry and owned-table bookkeeping remain
unchanged; detached target-owned upper tables stay tracked for eventual teardown.
No map replay, allocation/free, service, IRQ/CR3 or permission-policy operation
occurs. Unregistered upper entries are copied as kernel-owned entries, not
individually certified by this validator.

For an active target, the owner approved only a true no-op: after full registry
preflight, all 256 upper entries must already match byte-for-byte; success returns
without stores and any difference refuses unchanged. This avoids new stale
translations without INVLPG/CR3 operations. It does not certify pre-existing TLB
coherence, fix missing shootdowns, establish a durable lease, or authorize binding
wiring. Future commit must revalidate atomically if kernel mappings can change
after preparation and must have its own TLB/lifetime contract.

`gmake test-vmm-prepare` and separate ASan/UBSan pass natively on ARM64 macOS.
The actual-source fixture uses independent host-page-sized mappings, a read-only
active target to trap even idempotent stores, and padding guards beyond each
logical 4 KiB table. It covers effective permissions, independent cache flags,
capabilities, all leaf sizes/offsets, PAT, active/inactive cases, late second-record
failure, stale/colliding records, malformed root/count/IF and complete state
preservation. Allocator/free/console callbacks abort if reached. Original VMM
query/range/noncanonical/teardown regressions still pass; the actual freestanding
VMM object compiles with pinned Zig0.15.2. Logs:
`build/signal-context/vmm-prepare-{host,sanitizers,target}.log` and
`vmm-range-after-prepare.log`. No live guest helper invocation or image-generation/
FPU/delivery acceptance is claimed.

The owner independently reviewed `439d274d` and reran strict Rosetta and
ASan/UBSan preparation fixtures successfully. The unwired helper is accepted;
creation/refresh/setter wiring remains a separate design decision.

## Worker foreground-spawn binding correction

Lifecycle review found that synchronous spawn from a user worker retained its
worker association while selecting a child/main-thread FPU and stack binding.
An unchanged `fa69b4dc` guest reproduces it: the worker calls raw `SYS_SPAWN` for
`echo` and never returns; the parent deadline reports status3. Source, ELF, ISO,
disk and UART remain under
`build/signal-context/vm-1793763902047506000/worker-binding-probe-2/`.

The owner approved an isolated refusal, implemented at `bfee424f`:
`process_spawn_elf`/args, redirect and non-detached `process_spawn_exec` return
`SRV_ERR_NOTSUP` for a worker before allocating or changing file actions or
bindings. Detached spawn remains supported. Its shared preparation-error path
no longer invokes a redundant parent setter; no child binding had been installed
there, and that setter could select the wrong FPU/TSS for the worker. Error codes
and child cleanup order are unchanged. Kernel-only run APIs remain unchanged;
their callers are main/monitor/trackpad launch paths.

The tracked `tools/test_worker_spawn_guest.c` uses raw INT instructions to check
all four foreground syscall forms, detached missing-image and file-action
failure, detached child success and wait, volatile TLS data, FS:0, XMM15, RSP and
a stack canary. It also checks that refused redirection created no file. On a
fresh kernel built from the correction, the disposable guest exits0 with
`foreground refusal, detached failure/success, TLS/FPU/stack ok`. Receipt:
`build/signal-context/vm-1793763902047506000/worker-binding-fixed-1/receipt.json`,
SHA256 `f209d39e69bc0be4ac4e1916b9f7fff7bcf6f9822396916ca6e6104c4d17fec2`.
It binds source, ELF, ISO and kernel hashes. The owner independently checked
these and the UART result. The guest is stopped; protected review guests remain
untouched. This check does not expose kernel TSS directly or establish signal
delivery. Actual-source host fixtures provide the separately modeled binding
and child-cleanup checks.

`tools/test_process_foreground.c` and `test_process_spawn_failure.c` include the
actual process implementation. Both pass strict x86-64/Rosetta and ASan/UBSan,
including an independent rerun. Foreground refusals preserve the entire parent
and worker object, allocation/PID/slot state and scheduler admission counters.
The detached vector-failure fixture successfully reads a real minimal ELF and
maps its stack, then passes non-NULL argv with argc0 directly to the internal
constructor. It returns `SRV_ERR_NOMEM`, frees every child frame/kernel stack and
address space, releases the child slot, performs no setter or admission, and
preserves the entire parent plus modeled worker/FPU/FS/CR3/TSS selection. These
are host models of external hardware facilities, not a claim of reading TSS
from user mode. Make targets: `test-process-foreground` and
`test-process-spawn-failure`; this Mac's strict commands use `clang -arch x86_64`
and `-Wl,-dead_strip`, and sanitizer commands add
`-O1 -g -fsanitize=address,undefined`. Evidence is under
`build/signal-context/process-{foreground,spawn-failure}-review{,-san}.log`.

The unchanged full threadstress suite also passes all markers and top-level
status0 on this corrected kernel, including foreground main-thread spawn,
fork/exec, signals, accounting and teardown. Combined source/test/build/guest
receipt: `build/signal-context/worker-spawn-receipt.json`, SHA256
`192f992b27d9854d7a6a2b46633af997923bcd37306a9fb0d763023f54c0f622`.
This references the new retained harness result without replacing the original
accepted transition result. No public ABI, signal admission or JSC execution
success is introduced by the correction.

## Checked user-root creation at d7466e1c

The owner approved and independently qualified the bounded source/host slice:
allocate and zero a fresh uniquely owned PMM root before saved IRQ exclusion;
hold exclusion across registry admission, checked mapping preparation and either
unregister rollback or success publication. Output remains zero until success.
On failure unregister before freeing only the new root; preserve the prior IRQ
state. No legacy map replay, lower/kernel/shared table free, scheduler setter,
process refresh or FPU change is included. PMM returning an already-live frame
would violate the existing allocator ownership contract and is not a modeled
recoverable admission failure.

The extended actual-source preparation fixture checks both initial IF states,
output/zeroing/publication order, allocation and full-registry failures, late
mapping-preflight refusal, unregister-before-free, unchanged kernel tables and
zero lower/correct upper entries. Strict native and ASan/UBSan pass, including
independent parent reruns; existing VMM range/teardown passes both profiles. The
owner separately reran strict Rosetta and sanitizer builds of both fixtures.

A fresh full kernel compiles with pinned Zig0.15.2/two jobs. Matched disposable
guests pass the worker TLS/XMM/stack/refusal/detached regression, the complete
threadstress suite, and sysabi including faulting process/worker reaping. All
exit0 and all task guests stopped. Combined receipt:
`build/signal-context/vmm-create-receipt.json`, SHA256
`2c62e69c812f1dd671b912dec388474de28186bb15dbaaf03452474609693791`.
It binds source, fixtures, logs, candidate kernel/ISO/probe receipt and unchanged
selected userspace/disk provenance. Candidate head `a0a7f474` has the same kernel
code as `d7466e1c`; its later change is the next design packet. Final guest
qualification is with the owner; there is no signal/JSC acceptance claim.

The owner accepted the complete tuple/caller/fatal contract at `a0a7f474` as the
next implementation boundary, after this root qualification is published. That
slice is limited to committed image-generation and opaque association metadata,
atomic validated binding wiring and corresponding failure/lifetime tests. It
must not add an FPU proof object, minting, assembly change or signal delivery.
