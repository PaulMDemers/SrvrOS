# Entered-user FPU proof: next bounded review

Design only, following metadata candidate `fea6bbb1`. This proposes a real
common-entry save witness and internal capture/replace helpers, without signal
delivery, user copying, syscall allocation or public ABI changes. The existing
kernel-FPU association is the owner; a user-FPU record never owns this pointer.

## Private layout and assembly boundary

Use a private 64-byte, 16-byte-aligned record with these eight 64-bit fields:

| Offset | Field |
| --- | --- |
| 0 | Exact common ISR-frame pointer |
| 8 | Immutable scheduler identity |
| 16 | Actual selected user CR3 base |
| 24 | Committed image generation |
| 32 | Selected user-FPU pointer |
| 40 | Owning scheduler kernel-FPU pointer |
| 48 | Process-slot pointer |
| 56 | Private state: invalid, valid or consumed |

A shared private C/assembler header defines saved RSP0, trap FXSAVE16, proof528
and total reservation592, with static assertions for size/alignment/offsets.
Only `isr_common` changes to pass `(frame, trap_area, proof)` to new no-SSE entry/
exit wrappers. Direct keyboard IRQ and initial user entry remain unchanged.
The expected proof address is `align_down(frame,16)-64`. Internal capture APIs
accept the current common frame, never a caller-supplied proof pointer. Compare
the active association to that derived address **before dereferencing it**;
stale arbitrary association values must refuse without being followed. These
APIs are borrowed within the live common continuation and cannot cache a frame.

## Save first, then bind the witness

The no-SSE entry wrapper first makes the supplied record invalid with one integer
state store; do not zero a struct or call a possible SIMD memset before saving
user registers. Remember whether genuine CPL3 entry has available, initialized,
loaded user state and an initialized selected kernel state. Perform the existing
actual `fpu_trap_enter` FXSAVE/kernel-FPU switch before calling any ordinary C
query. Only after that save may an entry observation bind the record to the
saved user and owning kernel records, exact frame/process/identity/root/image.
Require no pre-existing active association before publishing a new valid record;
otherwise refuse without dereferencing or overwriting the old pointer.

Add an IRQ-excluded read-only entry query: initialized/not-switching scheduler,
bounded current RUNNING record/nonzero identity, nonzero user binding/generation,
pure nonkernel root matching actual CR3, selected user FPU and embedded kernel
FPU. Report process/worker, FPU owners, recorded FS and kernel-stack selection.
Entry must not require already-kernel accounting, unsafe admission, or zero I/O
depth: those conditions have not yet been established by dispatch. It allocates,
waits, changes state and dereferences process data **zero times**. Extend the
return observation with the same owned fields but retain its existing strict
unsafe/kernel/no-I/O predicates.

Capture/consume/replace require the derived current proof, valid state, genuine
CPL3 frame, current initialized selected FPU records, not-loaded user hardware,
fresh return observation and exact agreement on all recorded identities. Then
resolve process/worker ownership again through the accepted process resolver,
compare exact FPU/FS/stack selection, and perform active-root checked preparation
as a no-store registry/preflight check. All remain in the same IRQ exclusion.
This certifies saved FPU selection, not signal deliverability or a mapping lease.
Process signal admission must later add cancellation/exiting/target checks.

## Internal operations and exit

Provide fixed-size kernel-only operations: snapshot512 bytes without consumption;
consume the witness after all delivery planning/preflight succeeds; and replace
the selected saved image from a trusted private512-byte record while consuming
the witness. Refusal leaves output, saved bytes and state unchanged. Replacement
never FXRSTORs user bytes while C executes. No user pointer enters these APIs.
Consume must succeed before any future user-frame publication; its caller must
not introduce a later recoverable failure or service window in that transaction.

Nested CPL0 and direct traps retain the outer association and normal private
kernel FXSAVE behavior; they cannot mint a witness. Ordinary scheduler A→B→A
preserves each embedded association. Common CPL3 exit clears only its matching
association before the existing user FPU restore, and makes no ordinary C call
after restoring user hardware. Binding/exec/exit/reuse retain the reviewed
invalidation ordering. Consumed witnesses refuse further capture/replace until
a new real common entry; neither pointer equality nor not-loaded state suffices.

## Qualification and decisions

Actual-source no-SSE host tests must perform real FXSAVE/FXRSTOR on legal images,
with x87 control/state, MXCSR and XMM patterns; ordinary entry-query code should
deliberately clobber SIMD to prove the save preceded it. Cover nested CPL0/direct
traps, guard-page short CPL0 frames, A→B→A, unavailable/uninitialized/loaded-state
refusals, stale arbitrary association pointers, mismatched image/root/lifetime,
consumption/replay and replacement preserving metadata. The current private
kernel trap area must never be mistaken for the user image. Add matched main/
worker register/TLS/preemption guests, plus the already-qualified thread/sysabi/
I/O regressions. Host hardware/address maps remain explicitly modeled where
privileged instructions cannot execute.

Owner decisions: accept this exact layout/entry query/derived-pointer rule,
unchanged direct paths, and live common wrapper with internal helpers only;
agree unexpected existing-association refusal without chasing it. No signal
hook, frame publication or public register-context ABI is approved by this packet.
