# Entered-user FPU proof: next bounded review

The owner qualified metadata candidate `fea6bbb1` and approved this bounded
common-entry witness and snapshot/consume slice with the corrections below.
Replacement is not implemented or authorized by this slice. This adds no signal
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
Worker ownership is checked again using the current observed worker slot and
exact user-FPU record; immutable scheduler identity plus image generation prevent
slot-reuse authority. A bare TID is never a proof identity.

## Save first, then bind the witness

The no-SSE entry wrapper first makes the supplied record invalid with one integer
state store; do not zero a struct or call a possible SIMD memset before saving
user registers. Remember whether genuine CPL3 entry has available, initialized,
loaded user state and an initialized selected kernel state. Perform the existing
actual `fpu_trap_enter` FXSAVE/kernel-FPU switch before calling any ordinary C
query. Only after that save may an entry observation bind the record to the
saved user and owning kernel records, exact frame/process/identity/root/image.
Compare both pre-save user/kernel owner pointers and the successful loaded-state
save against the query's exact owners before minting. Require no pre-existing
active association before publishing a new valid record;
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

Snapshot/consume require the derived current proof, valid state, genuine
CPL3 frame, current initialized selected FPU records, not-loaded user hardware,
fresh return observation and exact agreement on all recorded identities. Then
resolve process/worker ownership again through the accepted process resolver,
compare exact FPU/FS/stack selection, and perform active-root checked preparation
as a no-store registry/preflight check. All remain in the same IRQ exclusion.
This certifies saved FPU selection, not signal deliverability or a mapping lease.
Process signal admission must later add cancellation/exiting/target checks.

## Internal operations and exit

Provide a private `struct fpu_user_snapshot` containing exactly512 bytes with
16-byte alignment. Snapshot copies into a caller-owned resident typed kernel
object without consumption; consume marks the witness after all planning and
preflight succeeds. Numeric extent checks require alignment, no overflow and
canonical upper addresses through the last byte. Refuse overlap with the entire
live common reservation/frame and either selected user/kernel FPU object,
including association metadata, before any output store. Residency and lifetime
are explicit trusted caller preconditions: a numeric kernel address alone does
not prove allocation. Production callers pass their own resident private object,
never a wire/user pointer; all callers are audited. Host fixtures use real private
backing with an explicit logical-address view for canonicality. Refusal leaves
the entire output object, saved bytes and proof byte-identical.
Consume must succeed before any future user-frame publication; its caller must
not introduce a later recoverable failure or service window in that transaction.

Nested CPL0 and direct traps retain the outer association and normal private
kernel FXSAVE behavior; they cannot mint a witness. Ordinary scheduler A→B→A
preserves each embedded association. Common CPL3 exit clears only its matching
association before the existing user FPU restore, and makes no ordinary C call
after restoring user hardware. Binding/exec/exit/reuse retain the reviewed
invalidation ordering. Consumed witnesses refuse further snapshot/consume until
a new real common entry; neither pointer equality nor not-loaded state suffices.

## Qualification and decisions

Actual-source no-SSE host tests must perform real FXSAVE/FXRSTOR on legal images,
with x87 control/state, MXCSR and XMM patterns; ordinary entry-query code should
deliberately clobber SIMD to prove the save preceded it. Cover nested CPL0/direct
traps, guard-page short CPL0 frames, A→B→A, unavailable/uninitialized/loaded-state
refusals, stale arbitrary association pointers, mismatched image/root/lifetime,
consumption/replay, exact/partial alias refusals and output guards. The current private
kernel trap area must never be mistaken for the user image. Add matched main/
worker register/TLS/preemption guests, plus the already-qualified thread/sysabi/
I/O regressions. Host hardware/address maps remain explicitly modeled where
privileged instructions cannot execute.

The owner accepted the layout, entry query, derived-pointer rule, unchanged
direct paths, common wrapper and unexpected-association refusal. Implementation
is snapshot/consume only. Actual no-SSE target codegen and real-FXSAVE host
evidence require review before acceptance; sanitizer/runtime prologues cannot
stand in for the save-first instruction sequence.

Replacement waits for a separate legal-image/provenance/alias policy and its
malformed-image controls. Restored MXCSR values must respect the current
hardware's supported mask; reserved bits can fault. See the
[Intel instruction reference](https://cdrdv2-public.intel.com/812383/253666-sdm-vol-2a.pdf)
and [AMD system programming manual](https://www.amd.com/content/dam/amd/en/documents/processor-tech-docs/programmer-references/26569.pdf).
No trial FXRSTOR is permitted while C runs. No replacement stub, signal hook,
frame publication or public register-context ABI is approved by this packet.

## Read-only CPU policy restriction

Owner-approved proof authority requires actual CR4.OSFXSR=1, CR4.OSXSAVE=0
and EFER.FFXSR=0 before minting and again on snapshot/consume under IF0.
Integer-only inline privileged reads precede the actual save; no ordinary query
may run before that save. Denial mints no proof, and snapshot/consume leave
association, saved state and output unchanged. Exit still clears a matching
association before restoration, with no policy/query call after it.
This changes no CR4/EFER/XCR0 setting and does not repair or accept the legacy
FPU path on unsupported inherited policy. Host policy is explicitly modeled;
real host FXSAVE execution does not accept the host's OSXSAVE configuration.
[AMD APM volume2](https://docs.amd.com/v/u/en-US/24593_3.44_APM_Vol2)
describes FFXSR omitting XMM at CPL0; Intel SDM volume3A section2.5 describes
OSFXSR controlling XMM/MXCSR preservation. This implementation supports the
reviewed 512-byte x87/SSE state only.

Proof publication and use belong to the same CPU0 IRQ-excluded continuation.
No NMI/SMP observer is provided or authorized by these APIs. Target codegen
must show all proof fields valid before association publication. The host
fixture deliberately patterns XMM15, x87 state/control and MXCSR; it does not
claim independently distinct patterns in all16 XMM registers.
