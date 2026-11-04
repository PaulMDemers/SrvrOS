# Proposed private delivery/return transaction for owner review

Status: proposed, not approved or implemented. This advances the qualified
[internal state](signal-internal-state.md) and [owned copy](signal-user-frame-copy.md)
toward real interrupted-register delivery. First qualification uses an opt-in
private test kernel; no public syscall or SA_SIGINFO capability is allocated.
Public action output, atomic suspend/restart and libc/WTF remain required later
wiring, with syscall163 reserved for session scope.

## Stable prepare and nonfailing commit

Use the common CPL3 return after complete dispatcher unwind, final CLI and
cancellation priority, before scheduler_user_enter. The private test build alone
calls the new delivery operation at that seam; direct IRQ entry and initial
usermode entry remain unchanged. Obtain strict current observation, exact
membership/image/TID/scheduler identity and the actual common user FPU proof.
Refuse unsafe I/O and never wait or enable IRQs in this transaction.

Under that same exclusion, select exactly one eligible SIGUSR1 bit, preferring
the current local bit to the shared process bit. Capture the selected INFO
descriptor/generation, old mask, current GPR/frame and authoritative saved user
FXSAVE. Revalidate handler and trampoline executable entries. Construct the
entire private candidate state with existing fallible push helpers, including
the exact new public range and next token; do not publish it yet. Full depth
defers with the bit pending; permanent token exhaustion or invalid delivery
state fails terminally. Prevalidate any RESET descriptor generation and all mask,
pending, identity, record and FPU commit conditions before a user store.

Add a private FPU prepare/commit pair: preparation validates the current proof
and caller-owned commit object and retains its exact kernel pointers. Commit
only writes CONSUMED, or copies a prevalidated private512-byte restore image and
writes CONSUMED. It has no queries, callbacks, IRQ transition or failure return.
It never FXRSTORs while C continues. The caller owns single use in the same
uninterrupted IF0 continuation; it is not an externally usable capability.
An abandoned preparation changes nothing. The existing fallible snapshot,
consume and restore APIs retain their contracts.

Extend the copy preflight with a fixed small trusted exclusion list, retaining
the old no-exclusion wrapper for existing callers. The process layer supplies
the full selected embedded private state, contiguous action/generation/main
pending-mask metadata, and worker pending-mask metadata when applicable. Admit
at most three kernel objects, each at most4096 bytes/two physical page spans;
assert the selected state fits this bound. Resolve their full effective
supervisor/RW mappings before any store and reject destination whole-page
physical aliases with every span. Retain all current stack/FPU/unique USER-page/
policy checks. These are known embedded static-image objects, not user pointers;
their provenance also relies on reserved kernel image and unique PMM ownership.
No allocator, general pinning subsystem or mapping mutation is introduced.

After successful user copy, commit only prepared local stores: candidate private
state, exact chosen pending-bit consumption, handler mask, optional RESET action
and mirrors, consumed FPU proof, and live trampoline GPR/return frame. No existing
fallible push/consume/query is called afterward. The complete source, candidate
and FPU commit object are stack-local and protected by the existing whole-stack
physical exclusion. Inspect actual target code for the entire post-copy suffix.
Copy refusal leaves destination and all signal/live/private state unchanged;
delivery failure is then terminal outside the transaction, after abandoning the
candidate. Unexpected faults during admitted copy remain kernel invariant faults.

## Private frame and return operation

Plan the exact serialized extent below the original128-byte red zone, retaining
SysV entry RSP%16==8 and aligned public context at entry+8. Proposed internal
test frame is760 bytes: zero return slot8, existing aligned688-byte snapshot,
and64-byte versioned information. Construct aligned local typed objects and
serialize bytes; do not cast the source's unaligned +8 offset to a snapshot.
Only signum/target identity facts actually known are marked valid; unavailable
sender facts are zero/invalid. This is not a Linux ucontext or public ABI claim.

Trampoline arguments carry signum, info/context addresses, selected handler,
token and frame identity. A test assembly trampoline calls the real user
handler and requests private return; it must terminate on return failure.
Handler entry uses fixed user selectors and RFLAGS0x202, including clear DF for
the C ABI; the original validated flags remain private and are restored exactly.
The handler may change public snapshot bytes, but return uses only the private
top record. Validate current lifetime, token and exact frame identity, stage
the LIFO pop, validate a typed private FPU restore image/MXCSR, then commit only
private saved frame, FPU prefix/consumed proof, previous mask and staged stack.
Never restore selectors, flags, FS, CR3 or FX bytes from user memory. No ordinary
syscall result writeback may overwrite restored RAX. Bad/replayed/out-of-order
return cannot change any state and is terminal for the active test handler.
Successful return skips additional delivery at that same boundary: its proof
is consumed, so another pending signal waits for the next eligible trap.

## Private qualification and later public wiring

Use a separately built IDT object/kernel and test-only DPL3 INT3 gate. Recognized
magic operations install an internal descriptor or request return only after
the normal outer IRQ exclusion; other breakpoint behavior remains unchanged.
The guest queues directed signals through existing thread-signal calls, then
uses a pure user loop to prove timer delivery without libc polling. Capture
known GPR/stack/red-zone/FS/TLS/x87/MXCSR/all-XMM patterns, clobber them in the
handler, and verify repeated main/worker restoration, nested delivery, masks,
coalescing, range guards, depth/token refusal, stale identities and independent
original-register roots. Keep deterministic timeouts and failure receipts.

Measure the complete excluded prepare/copy/commit interval at genuine65536
mappings, retaining4N scan complexity and TCG/PM quantization limits. The earlier
911us copy-only sample is not the full transaction budget. Normal kernel/object
hashes stay isolated; rerun normal FPU/thread/sysabi and affected I/O checks.

Public action and sigreturn syscalls must later stage fixed per-thread requests
inside dispatch and complete them at the strict common-return seam, after
cancellation/service cleanup. This avoids treating io_user_safe syscall state
as return admission or restoring a frame before the dispatcher overwrites RAX.
Bind each request to its exact trap/image/thread, clear on cancellation/retirement,
and review old-action user copy before descriptor commit. Exact ABI allocation,
restart epochs, nested atomic temporary-mask wait and libc transition remain
separate design requirements before production wiring or JSC claims.
