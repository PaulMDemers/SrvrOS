# Directed signal and interrupted-context runtime dependency

Status: internal helper slice implemented; delivery remains a proposal. The shared-runtime branch owns this dependency; process, VMM and teardown integration require the audit owner's review before implementation. The first acceptance target is one CPU, one process, and a thread-directed asynchronous `SIGUSR1` for a running or safely blocked thread. This is a real signal contract, not a JSC-only callback. It does not promise SMP, synchronous fault recovery, alternate signal stacks, arbitrary context editing, or unrestricted POSIX signal conformance.

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
