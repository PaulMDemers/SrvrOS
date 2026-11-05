# Internal SIGUSR1 action and private-record lifecycle

This owner-approved slice is initially unwired. It publishes no syscall, signal
capability, SA_SIGINFO admission or asynchronous callback. The private APIs take
stable caller-owned kernel objects; numeric checks do not authenticate pointers
or make a userspace copy transactional. No SMP guarantee is claimed.

`signal_state.h/.c` defines DEFAULT/IGNORE/LEGACY/INFO descriptors and bounded
per-thread state. Flags are internal RESTART/RESET bits, not a public ABI. Opaque
LEGACY handler0 represents the old configuration syscall, whose handler remains
in libc. Actual handler addresses exclude disposition sentinels; INFO requires
both handler and trampoline. Shape validation rejects unknown flags and mask
bits0/9/19. The process layer validates actual executable entry mappings before
installing a nonzero pointer; this is not a mapping lease. Delivery must check
again after any intervening execution or mapping change.

`process_signal.h` exposes only internal exchange/push/pop helpers. They save
IRQs, obtain a fresh strict scheduler observation and resolve process/worker
membership before dereference. They reject dead/exiting images, main-thread
retirement and inconsistent FPU/FS/stack bindings. Action exchange stages input,
validates all entries, then commits the descriptor, generation and catch/ignore
mirrors together. Replacement and old-output may alias; other storage cannot
alias live state. Generation exhaustion refuses unchanged. Query does not
advance it. IGNORE discards the same pending bit in process/main/worker queues.
Legacy configuration clears INFO coherently, and both cooperative poll and
consume exclude INFO so they cannot steal a future asynchronous delivery.

Identity is the committed image generation, nonreused public TID and observed
scheduler identity. The current main has TID1; worker IDs are2..INT64_MAX. Four
records are permitted; full depth refuses unchanged, retaining pending state for
the future delivery caller. Active records must have consistent private frames,
strictly increasing nonzero tokens, matching identity, nonoverlapping canonical
half-open public ranges of at most1024 bytes, and token<=last issued token.
Pop is exact LIFO; an emptied stack retains its last token. UINT64_MAX can be
issued once and then exhausts that lifetime. An explicit retirement reset clears
identity/records/tokens; binding never silently converts an old identity.
Ranges validate shape and overlap only, not actual serialized frame fit or
mapping lifetime. Push/pop remain fallible and cannot follow a successful user
copy as an assumed infallible publication step.

Storage is embedded in the existing static process records:3056 bytes per
thread, nine states plus40-byte descriptor and8-byte action generation per
process. Target `sizeof(process)` grows from1673712 to1701280, or27568 bytes per
slot including alignment. The64-slot array grows1764352 bytes; the explicit
payload accounts for1763328 bytes. There are no allocations or user-backed
links. This provenance still needs the complete private-storage nonalias proof
in a future copy/publication transaction.

Main pthread_exit invalidates the active FPU proof and clears only its own
records under explicit IRQ exclusion, preserving the shared image and worker
records. Worker exit clears before binding/TLS/slot retirement. Process exit
clears all records after its existing IRQ-excluded exit gate. Image cleanup,
runtime release and slot release clear before generation/backing retirement.
Successful exec clears caught descriptors only at commit; failed loading,
preflight or stdio leaves the old action/state unchanged. Fork/exec refuse any
active embedded record before allocations or other effects, including stale
records in unused worker slots. Ordinary fork inherits the action and mask with
fresh empty private state. Spawn inherits ignored disposition as before.

On later reconciliation with main82face72, preserve session detach inside the
saved-IRQ exec commit before clear→generation0→cleanup; keep exit FPU invalidation
before/adjacent detach; release detach precedes backing retirement. Main
pthread_exit preserves the session owner. These changes are coordinated with
the process owner, without merging another branch or changing session ownership.

Focused module/action/lifecycle and existing metadata/service/loader/lifetime/
output fixtures pass26 strict Rosetta/ASan+UBSan profiles. Generated-code review
found the main retirement could otherwise run with IF1; the explicit IRQ save
and stronger lifecycle assertion correct that, with affected profiles rerun.
Target code confirms the new helper module has no SIMD/x87 and main retirement
executes CLI before invalidation/clear; release/exec/cleanup preserve their
IRQ-excluded retirement ordering. Fresh corrected normal-kernel FPU main/worker,
full thread and syscall ABI guests pass and stop. Internal INFO/record operations
remain host-tested, with no private-operation or production delivery guest. This does not qualify signal delivery, sigreturn, atomic suspend/restart,
libc/WTF integration or the JavaScript smoke test.
