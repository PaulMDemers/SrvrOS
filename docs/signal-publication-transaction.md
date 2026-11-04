# Proposed private delivery/return transaction for owner review

Status: owner-approved private implementation/test scope, implemented and under
final evidence review. This advances the qualified
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

Return also stages the existing generic mask-unblock decisions: discard newly
unblocked default-ignored CHLD/WINCH bits from current local/shared queues;
preserve caught bits; select the lowest default-fatal bit with local-source
priority. Preparation refusal leaves everything unchanged. Private
return reports0 for refusal,1 for restored, or128+signal for restored-and-exit.
The test caller passes the prepared exit status directly to process_exit before
user accounting/resume, without a post-commit admission query. An independent
review reproduced the former raw-mask-store bug for USR2 and RESET/mixed-source
USR1; main/worker strict and sanitizer regressions now cover the corrected
fatal/ignored/caught decisions and rollback.

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

## November 4 private qualification

Strict/sanitized actual-source VMM, FPU and existing process-copy tests pass.
The new process transaction fixture separately models scheduler/FPU/VMM inputs
and guards actual query calls after user store through the complete commit.
Every new excluded object crosses a distinct physical page pair; twelve alias
controls independently cover both spans against both destination pages.

The separate1536MiB Q35/qemu64 single-CPU TCG guest passes main/worker/repeated
timer-only delivery, all15 GPRs, all16 XMMs, x87/control/MXCSR, FS/TLS/RSP and the
original128-byte red zone. Its interrupted R15 points to a real allocated object;
this verifies pointer capture, not sole-root or GC survival. Handler entry clears
DF and private return restores original DF. Every public snapshot is destroyed
by the handler before a clobbering trampoline requests private return.
Main/worker nesting reaches four records; the fifth queued signal defers at
full depth and is delivered after unwind. Tokens remain monotonic per lifetime.

The probe uses mapped stack page+0x300 so its complete760-byte frame crosses two
pages, including the genuine65536-mapping case. The complete prepare/copy/commit
sample is961us, vector32. This is a quantized emulated-profile observation,
excluding outer hook/diagnostics and not a hard or complete IRQ-off bound.
All130 normal objects/kernel retain identical hashes across isolated test builds.
Fresh normal FPU/thread/sysabi and OVMF storage contention pass, including the
intentional timeout/quarantine injection. Original ISO/review outputs restore.
Exact first and expanded guest inputs/raw logs are retained outside distclean in
workspace `.local/pr334-evidence/private-guest-1` and `private-guest-2`.

Target audit of the full successful suffix finds only bounded private copies,
prepared scalar/mask/RESET/frame/default stores and the call-free FPU commit.
No FXRSTOR executes while C continues. Compiler stack reports give delivery6504,
return4520 and process-copy preparation264 bytes. A37-node direct-call audit
adds8 bytes per call edge, including conservative tail-call overcount: roots are
11224/10008 bytes. Adding983 bytes for the common ISR frame/reservation/alignment,
private dispatcher and call addresses yields12207/10991 bytes against the owned
128KiB stack. All38 relevant function bodies match actual object sections; no
indirect call, recursion or dynamic frame appears in the audited paths. This
bounds these maskable-IRQ-excluded transactions, not NMI/MCE or all kernel work. Static embedded state remains the
previously qualified1764352-byte process-array increase including alignment.
No production/public ABI, restart, atomic suspend or JSC acceptance is implied.


## Automatic RESET with pending peers

A later actual-source reproduction found an additional private RESET case:
resetting the global INFO disposition to DEFAULT could strand an unblocked peer's
already pending USR1. The earlier nonreset, register and nested-delivery evidence
remains historical; it did not cover this peer-default transition.

The correction prepares a prospective view before copying: remove exactly the
first selected local/shared bit, apply self-blocking only to the current handler's
next mask, then select an eligible pending default across other live threads.
Local pending precedes shared, with main/lowest live owner-matching publicTID
priority. Inactive/foreign-owner/masked peers do not qualify. A current-only
duplicate remains pending while self-blocked and is evaluated on private return.

RESET uses a separate internal copy entry with at most12 exclusions; the old
three-object API still refuses count4. Its ten generated objects are the complete
selected state, process prefix from0 through main_signal_state (including kill
flags/cause), and all eight worker mask/pending extents, including inactive slots.
All extents/translations/physical aliases validate before output. After copying,
only prepared state/pending/mask/RESET/kill/frame stores and the typed FPU commit
remain. Result138 means committed delivery plus fatal default; the opt-in caller
directly exits before post-call diagnostics/accounting/user resume.

Private operation4 combines test-only installation then delivery in one final
IRQ-excluded continuation to pin the selected main/worker. Installation is a
separate transaction, not covered by delivery refusal rollback. The diagnostic
negative links the same new IDT/VMM/FPU/payload with the frozen2777 process object;
it is not a clean2777 kernel. Six peer cases enter a forbidden handler on that
negative, whereas all six corrected cases exit138 without entering it. Both
current-only controls enter exactly one handler then private-return138 before
continuation. Nonreset/full register/nesting/max65536 and FPU fixtures remain0. The936us
maximum-mapping observation is nonreset, not a RESET latency bound.

The strengthened40-function static call/control-flow audit compares actual bytes
and normalized symbol/section/value/type/relocation/addend identity. It rejects
unknown/indirect edges, recursion and dynamic frames and validates function-entry
targets. Delivery/return cumulative bounds are12351/10991 bytes including983
common bytes, against131072. Successful-copy suffixes call only scalar memcpy
and prepared FPU commit. The scope excludes NMI/MCE and process-exit teardown.
Normal raw-default, exec, GOT/TLS, threadstress, sysabi and FPU guests pass; a
serial parser's partial-line failure is retained separately from the successful
strict-line rerun. Frozen317-file receipt outside distclean:
`fa60ae294d585e5c6ebab3ad2e58e71b468e7d37de939a074f0aab3c3e4aa258`.
Public registration, atomic suspend, restart and JavaScript remain unqualified.
