# Image commit and active-association packet

Design for owner review, based on signal-context checkout `6fb08496`. This slice adds image identity and invalidation metadata only. It does not mint a proof, capture or restore FPU state, alter assembly, deliver a signal, or expose a public ABI. CPU0 scheduler and IRQ exclusion remain explicit assumptions; this is not an SMP contract.

Current preparation prerequisite: the owner independently accepted unwired
`vmm_prepare_user_kernel_mappings()` at `439d274d`. Its inactive-root upper copy
is allocation-free and validates the complete registered kernel mapping set
before writing. An active root succeeds only as a byte-identical no-store
operation; this is not a TLB coherence or lifetime guarantee. It is still not
called by creation, scheduler setters or process refresh. The owner-approved
worker foreground-spawn refusal and detached-error binding preservation are
isolated at `bfee424f`; a matched guest passes the foreground refusal and detached
failure/success checks. These prerequisites do not approve the lifecycle design
below.

## State and internal API

Add `uint64_t image_generation` to `struct process`, initially zero. A CPU0/IRQ-excluded monotonic allocator reserves a nonzero value exactly once; saturation refuses rather than wraps or reuses. Reservation can burn a value on later failure. Process job `lifetime`, PID, TID, CR3 and scheduler identity are not image identities. A committed image retains its generation across normal scheduler switches and foreground parent suspension/restoration; a successful exec or fork child gets a fresh generation. Process slot reuse starts at zero. Audit direct retirement paths such as `process_try_wait_retired` as well as `release_process`; clear the private image ID before an allocated slot is reusable, after bindings/proofs/backing have retired.

Mirror this value in each scheduler user binding, and extend the existing internal setters with explicit `uint64_t image_generation`:

```
scheduler_set_user_context(process, cr3, kernel_stack_top, image_generation)
scheduler_set_user_context_fpu(process, cr3, kernel_stack_top, user_fpu, image_generation)
```

`process_image_generation(process)` may provide the value to process-owned call sites; the setter must not infer it from CR3 or dereference an unvalidated arbitrary pointer. The internal observation structure may report the mirror; zero refuses observation. It must not claim independent process validation by dereferencing the opaque process. A later proof compares its bound generation to this fresh mirror. All bindings for a committed image use its same generation. `scheduler_spawn_user_thread` atomically publishes the parent's nonzero committed generation with process, worker, FPU, FS and CR3 before READY, and starts with no active association. A fresh fork scheduler slot also starts empty. Clear association metadata in the copied child user-FPU record without reinitializing/clobbering its copied 512-byte register image. Invalid/null process or zero image generation must not publish a valid binding; define a fail-closed internal assertion/refusal policy with the owner before changing existing void setter behavior.

Add a private active-association pointer to each scheduler **kernel-FPU** context and pure initialize/invalidate operations, with no allocation or proof interpretation. Invalidation sets the pointer NULL; it never dereferences or frees an opaque target. Its non-NULL meaning and lifetime are deliberately unspecified until the later proof design. It must be initially NULL in scheduler init, fresh/reused slots and worker/fork construction. There is no binding epoch or fallible allocation on a binding change. Every setter and clear, including a redundant same-value setter, invalidates the association **before the first binding, FPU, FS, CR3 or TSS mutation**. A→B→A within one scheduler thread cannot revive A's former association. Ordinary `yield_to` scheduler A→B→A (`scheduler.c:822–889`) does not call setters or invalidate either thread's association.

## Constructor and commit inventory

All staged images reserve/commit before user entry or READY publication; failed later admission cleans staged resources and burns the reserved generation. Audit these eight distinct loader paths, including their wrapper aliases:

1. `process_run_elf_args` (`process.c:1882–1907`), before `enter_process`.
2. `process_spawn_elf_args` (:1915–1959), before foreground child bind.
3. `process_spawn_elf_args_redirect` (:1962–2010), before bind; maintain redirect cleanup.
4. `process_spawn_exec` (:2014–2115), after successful image/vector setup and before foreground binding or detached scheduler publication; preserve file-action/signal/group cleanup. Its loader is `load_spawn_image_checked`, not the ordinary wrapper.
5. `process_exec_replace` (:2172–2243), staged image reservation and special commit below.
6. `process_start_background` (:2332–2363), before scheduler spawn.
7. `process_spawn_background_elf_args_quiet_flag` (:2516–2546), covering `process_lifetime_spawn` and quiet/nonquiet wrappers, before scheduler spawn.
8. `process_spawn_background_elf_args_fds` (:2562–2599), before scheduler spawn; preserve descriptor cleanup.

Additionally, `process_fork` (:2282–2329) reserves a fresh child generation after successful address-space clone and before `scheduler_spawn`; the child has no inherited association even though its FPU/image bytes are copied. Failed reservation follows the existing child cleanup path. Private exec staging objects can carry a reserved generation but must not be treated as published merely because `address_space` became nonzero in `load_elf_image_checked_impl` (:1643). Check all aliases and future constructors rather than rely on loader success as publication.

For foreground spawn, the same scheduler record is rebound parent→child at :1940/:1992/:2097 and child→parent at :1956/:2008/:2113. Each setter invalidates its current association and binds the specified committed image; the parent resumes with its original generation and no newly allocated ID. The failed-child restoration calls at :1934/:1986/:2058 also conservatively invalidate if the setter executes, even if the value was already parent. Avoid gratuitous setter calls if preserving a failed operation's old association is required.

## Exec transaction and binding exclusion

`process_exec_replace` loads and builds vectors in a private image (:2190–2202). Reserve the new generation after those succeed and **before** `process_apply_stdio_fds` (:2203). Its `process_configure_stdio_fds` snapshots/clones all sources before replacing slots and drops cloned references on false; nevertheless verify its exact failure paths and tests during implementation. Failed load, vector setup, generation reservation or stdio application leaves old image ID, scheduler binding and active association untouched. A reserved generation is never returned to the allocator. After stdio succeeds, current code has no error return before user entry; preserve that property or explicitly design rollback.

At commit, exclude IRQs and invalidate the active association before `scheduler_clear_user_context` or any old CR3, FPU, TLS or backing mutation. Clear the scheduler mirror/binding and switch off the old CR3 before old address-space destruction. Keep the scheduler binding NULL and association invalid throughout `cleanup_process_address_space`, including service/checkpoint windows; do not hold IRQs disabled over potentially yielding/allocating cleanup. Move staged image and its generation, reinitialize process FPU, reset signal/image fields while unbound, and rebind the new generation only when fully committed. After unbinding, set the live process image generation to zero before old backing cleanup; staged image retains its reserved new ID. Move/publish that new ID only with the staged image. External metadata users must not interpret an unbound/zero-generation transition as a deliverable image; no new public signal semantics are introduced here. No rollback to the retired image is assumed after the successful stdio boundary.

The current setter (`scheduler.c:1048–1073`) is **not** one IRQ-excluded transaction: it mutates binding, FPU and FS before `vmm_refresh_kernel_mappings`, CR3 switch and TSS update. Preparation that may allocate, including `vmm_refresh_kernel_mappings`, must occur before the binding-commit exclusion and before replacing an existing binding, with old binding stable; validate that preparation's failure/side effects and service-gate conditions are real and bounded. Then, without callbacks, waits, allocation or IRQ reenabling, invalidate association and atomically commit mirror/binding fields, FPU pointer, FS, CR3 and TSS. The current VMM refresh path has no scheduler/service calls; its `ensure_next_table` can allocate missing tables and the void refresh ignores map failures. Do not call it a yielding routine without evidence. It is bounded by 256 upper-root copies plus at most 128 registered kernel mappings, but bounds alone do not prove allocation-free success. Owner may instead approve an audited excluded fast path if source invariants and actual-source failure tests prove it cannot allocate or fail. Recheck current thread/staged identity after any preparation that can actually schedule. If existing mapping refresh cannot safely prepare without changing the old address space or cannot report failure, owner must choose a reviewed split/preparation API or a different serialization design; do not silently assert current code is atomic. Clear follows the same excluded invalidate→unbind→FS/CR3/TSS sequence. Preserve exact previous IRQ state after commit; no service wait inside exclusion.

## Exit, teardown and ownership

- Worker exit (`process.c:3420–3429`) invalidates via clear before TLS release; detached/joined slot reuse must not retain association. Worker-originated process exit (:7412–7428) clears that worker but deliberately leaves sibling bindings alive until final kill; mark exiting so future proof admission refuses.
- Final main process exit (:7431–7447): invalidate every current and noncurrent matching scheduler association before `scheduler_kill_user_threads` clears their FPU/CR3 and before process mappings/FPU backing are retired. `scheduler_kill_user_threads` (:909–934) itself invalidates each matching noncurrent record before first mutation. Clear current association before its own binding/CR3 changes. Process image becomes invalid before backing reclamation; slot release/reuse cannot retain generation or association.
- Main `pthread_exit` (:3396–3417) can return for libc atexit while workers remain. Invalidate any existing association before treating the thread as exited; do not retire the shared image generation until actual final process teardown. Do not assume this path never returns.
- Before scheduler slot reuse frees an old kernel stack (`scheduler_spawn_impl`, before `pmm_free_frame_range`), clear that slot's association without dereferencing its target. Initialization after the free would be too late. Initialize all association metadata at scheduler slot construction (:725), including reuse. Never dereference an opaque association after invalidation. Explicitly audit other backing retirement and scheduler kill/clear callers, not just normal exit. Bare TID wrap (`process.c:3312`) and directed-send lifetime safety are separate gates.

## Tests and review decisions

Update declarations, all production setters, the scheduler observation fixture, and mocks in `tools/test_process_service.c`, `tools/test_process_teardown.inc`, `tools/test_io_scheduler.c` and other source-including fixtures found by search. Host tests must cover each constructor and reserve exhaustion, no READY/bind with zero ID, all failed-exec stages preserving old state, successful exec ordering across cleanup checkpoints, stale association invalidation before first mutation, setter same-value and parent→child→parent, ordinary scheduler switch preserving association, fresh worker/fork and slot reuse, worker-origin exit with live siblings, main atexit return and final teardown. Model interrupts/preemption around every preparation/commit boundary and allocation failures. Target tests need actual foreground spawn/restore, fork/exec success and failure, workers, and teardown under I/O contention. No active-association metadata alone establishes safe FPU capture or signal delivery.

Proposed generation reservation failure is `SRV_ERR_AGAIN` (or false for boolean constructors), with staged cleanup and unchanged old binding; agree this before code. Owner decisions required before code: exact generation allocator/overflow error propagation; staged/transition visibility and IRQ/service ordering; mapping-refresh preparation/failure contract; setter treatment of invalid internal bindings; and teardown ordering with process/VMM owners. The proposed changes must not duplicate their implementation work.

## Proposed first metadata-only API boundary

For a bounded first implementation, propose a process-private `process_commit_image_generation(process)` boolean helper using the existing saturating scalar primitive under `process_irq_save/restore`; all eight constructors above call it after staged preparation and before publication. Exec reserves into its private staged image before applying stdio; `move_process_image` moves the reserved ID only after old unbind/retirement. No new public getter is necessary: process-owned callers pass their known field explicitly to the internal scheduler setters. Extend the internal scheduler observation with a reported nonzero mirror, without dereferencing process state.

Propose a forward-declared opaque `struct fpu_user_trap_proof` pointer in the internal `struct fpu_state`, used as an association only in scheduler kernel-FPU records. User-FPU records keep it NULL. `fpu_init_state` clears it even on unavailable-FPU initialization; a pure `fpu_invalidate_user_trap(struct fpu_state *)` clears it without dereferencing a target or executing FPU instructions. A scheduler wrapper `scheduler_invalidate_current_user_trap()` performs the current-record lookup under IRQ exclusion for process main-exit/terminal paths. Noncurrent kill/reuse code calls the pure invalidator on that stable slot's kernel-FPU record before mutation/free. Fork clears association metadata after copying the user register image, without reinitializing the copied 512 bytes. No proof object, minting, capture/replace or assembly call is introduced in this metadata slice.

The binding setters remain void and perform no generation allocation. All user-binding callers must already hold a committed nonzero image ID; reject an invalid internal tuple before publishing rather than turn it into a valid zero-generation binding. The exact assertion/refusal behavior for violating a trusted internal precondition remains an owner decision. Parent restoration reuses its already issued image ID. Clear publishes a zero mirror. The future proof is invalid across any setter, even same-value restoration; ordinary scheduler state selection preserves each slot's association.

Source clarification for commit preparation: `vmm_refresh_kernel_mappings` copies 256 upper PML4 entries and loops at most `VMM_MAX_KERNEL_MAPPINGS` (128), then ignores `map_page_in_space` failure. The path has no scheduler or service callback, but `ensure_next_table` can allocate via PMM if a level is absent. Existing kernel mappings are upper-half entries already present in `kernel_pml4`; copying them usually shares existing tables, but that invariant and failure cases need actual-source proof before declaring the entire refresh allocation-free. Preferred bounded ordering for review is refresh/preparation while the old binding is intact, then an IRQ-excluded, callback/allocation-free association-invalidate and binding/FPU/FS/CR3/TSS commit. Do not move a possibly failing preparation after old image retirement without an explicit fatal-failure contract; the current void API cannot silently report rollback.

`process_configure_stdio_fds` clones all three selected sources before its first file-slot replacement. Its false path releases cloned references and returns without replacing those slots; `process_apply_stdio_fds` only calls that helper with parent==child. Generation reservation precedes it, and no association/binding mutation occurs on its false path. After it returns true, current exec code has no normal error return before new user entry. Tests must validate this exact boundary, including clone failure, rather than merely assert failed-exec preservation from a plan.

## Concrete checked-binding proposal after the preparation helper

This section supersedes the earlier suggestion to run the void refresh as
preparation. It is a proposal for the next owner decision, not implemented code.

1. Change new user-root creation to run the checked preparation under saved IRQ
   exclusion after registering the fresh, inactive root. On failure unregister
   and free only that root, publish output zero and return false. Allocation and
   initial zeroing precede this exclusion. Remove only creation's legacy refresh
   call; do not change generic process refresh or user mapping replay in this
   slice. Test actual creation admission and failed-preflight cleanup as well as
   the helper itself.
2. Before an image can reach READY or an irreversible foreground/exec commit,
   prepare its root and reserve a nonzero committed image generation under IRQ
   exclusion while its parent/old binding remains intact. Recoverable failure
   cleans only staged resources and returns the existing constructor error (or
   the agreed `SRV_ERR_AGAIN` on generation exhaustion). Exec performs this
   before stdio replacement. File actions may themselves have external effects;
   this proposal does not introduce rollback promises for those existing APIs.
3. An internal scheduler setter keeps its existing void interface but saves and
   excludes IRQs around the entire trusted binding commit. Before changing any
   association/accounting/binding/FPU/FS/CR3/TSS state, validate the complete
   tuple and revalidate/prepare its root. The explicit image generation must be
   nonzero; no generation allocation occurs here. After successful validation,
   invalidate the old association, then commit the tuple and hardware selection
   without allocation, callbacks, service windows or IRQ reenabling. Restore
   the caller's exact prior IRQ state on normal return. Clear likewise excludes
   IRQs for invalidate through kernel-root/FS/TSS selection.
4. Preparation is not a lease. A staged root can become stale during stdio,
   cleanup or a background thread's delay before first binding; therefore the
   final excluded setter revalidates even after successful earlier preparation.
   If this trusted internal precondition fails after an irreversible boundary,
   halt with a bounded invariant diagnostic rather than silently keep an old
   binding, publish a partial binding, or return to user mode. A process-local
   recovery mechanism is not claimed: current `process_exit` cannot perform
   ordinary teardown when the current scheduler process binding is NULL. Agree
   the diagnostic/fatal policy explicitly before changing the void setters.
5. Worker READY publication validates the inherited nonzero image/root under
   exclusion before publishing the complete worker tuple; recoverable admission
   failure frees only staged scheduler resources. It must preserve the parent's
   association on failure. Fresh fork and background bindings use their reserved
   generation. Ordinary scheduler switches retain per-slot association state.

The foreground-spawn audit found that process loading and vector writes use
physical direct-map access rather than switching the scheduler binding. The
detached shared error-path parent setter was removed in `bfee424f`; the two
main-thread-only foreground load-error setters remain candidates for removal in
this lifecycle implementation. A worker cannot synchronously rebind through the
user spawn APIs after that fix. Kernel-only `process_run_elf_args` callers remain
main/monitor/trackpad launch paths; this packet does not broaden that API.

Owner decisions still required: accept creation/preflight wiring and exact
failure cleanup; accept the trusted setter's fail-closed fatal invariant policy;
approve the metadata-only generation/association implementation and tests. This
does not approve FPU proof minting, interrupted-context capture/restore, signal
admission or a public ABI.

## Complete tuple predicates and caller matrix for owner review

The owner accepted the checked-preparation/fail-closed direction, but authorized
only checked root creation first. The following is the exact proposed metadata
boundary; it is not implementation approval. Keep **binding validity** separate
from **signal deliverability**: a parent killed while a foreground child runs
must be able to restore its intact binding and reach ordinary cancellation, and
main `pthread_exit` can return to its atexit continuation. Neither condition is
an internal tuple corruption or justification for a kernel halt.

Process-owned validation runs with IRQs already excluded and no callbacks. Match
an opaque process pointer by equality against the bounded static process-slot
array before dereferencing it; then require allocated, active, not reapable and
not reserved for release/reuse. Require a nonzero issued image generation equal
to the argument, and its exact nonzero aligned registered nonkernel root equal
to the argument. Root preparation is independently repeated under the same
exclusion immediately before publication. Job/PID/CR3 or a nonzero number alone
does not authenticate the image. A staging heap object is never a publishable
process-slot pointer: exec moves its reserved image/generation into the validated
live process while unbound before calling a setter.

| Component | Exact proposed predicate |
| --- | --- |
| Scheduler | Initialized, not switching, current index within published capacity, current record RUNNING, nonzero immutable scheduler identity; current kernel-FPU record belongs to that stable slot. Require io_depth==0 or io_owner==current_thread: no setter during a foreign owner's service window. Existing owned-I/O calls retain their audited process/service contract; no wait is added inside the commit. |
| Main selection | The current scheduler worker association is NULL. Resolve the validated process's main FPU and FS directly; never infer main FS from `process_fs_base`'s current-worker fallback. An explicitly supplied FPU must equal that main record. Ordinary main restore may retain killed/exiting/main-exited flags for cancellation/atexit; it must still own a live intact image. |
| Worker selection | Match the worker pointer by equality against one of that validated process's eight slots before dereference. Require used, active, exact owner, tid >=2 and exact FPU pointer equal to that slot's FPU. Fresh scheduler identity is issued before READY; parent must still hold the same live image generation/root. A bare/recycled TID is not proof authority. Directed-send TID wrap is a separate gate before signal admission. |
| FPU | Selected record address is 16-byte aligned and exactly the validated main/worker record; when FPU is available its record is initialized. Unavailable hardware follows the existing no-FPU initialization semantics and cannot mint a future proof. User records have no active association; the opaque association belongs only to the selected scheduler kernel-FPU record. |
| FS/TLS | Selected FS equals that exact main/worker TLS pointer. For zero TLS size require zero file size/base/pointer. Otherwise require file<=memory<=template limit, power-of-two alignment<=page, checked block/TCB size, deterministic slot base within USER_TLS bounds, and FS==base+aligned block size. Do not dereference user TCB bytes during this metadata transaction. This validates the owned TLS selection; future signal/copy proof must separately validate actual resident page permissions. |
| Main stack/TSS | Supplied top equals the process-owned kernel-stack top; nonzero physical allocation is page aligned, frame count equals PROCESS_KERNEL_STACK_SIZE/page, virtual base equals its PMM direct-map address, and checked base+size is a canonical upper aligned top. No fallback to default TSS for a publishable main image. |
| Worker stack/TSS | New scheduler slot owns its fresh SCHEDULER_STACK_SIZE allocation with matching direct-map base, checked canonical upper/aligned end and frame extent. Worker kernel_stack_top remains zero so TSS selects this verified scheduler-owned stack, rather than the process main stack or default fallback. Validate before READY. |
| Clear | Initialized current RUNNING slot with valid identity, and a nonzero valid kernel root/default TSS stack established before mutation. Invalidate first, clear process/worker/generation/FPU/FS/root fields, select kernel root/FS0/default TSS under the same saved exclusion. Before scheduler initialization clear can remain a no-op because no user binding exists. |
| Delivery (later) | In addition to valid binding, refuse killed/exiting/reapable/zero-image, inactive or main-exited target, cancellation, I/O ownership, and missing exact entered-user FPU proof. No delivery predicate is enabled by metadata alone. |

The scheduler must not dereference a caller's arbitrary process/FPU pointer before
process-owned membership validation. A private common main-binding implementation
can serve both existing main setter entry points and resolve FPU/FS only after
that check; the public wrappers must not call today's unchecked
`process_fpu_state` first. `scheduler_set_user_thread_context` has no production
caller (only declaration/definition and fixture mocks). Propose removing that
unused standalone partial-binding setter in this slice, rather than leaving a
way to change worker identity independently of FPU/FS/root/TSS. Worker publication
remains one complete `scheduler_spawn_user_thread` transaction.

| Caller | Recoverable prepublication failure | Committed binding and failure boundary |
| --- | --- | --- |
| Kernel `run` and native foreground args/redirect | Image/vector/preparation/reservation failure cleans staged image; existing false/-1 result; old binding/association untouched. Remove the two redundant foreground load-error setters. | Main child generation committed before first bind; parent restore reuses its old generation. Final setter revalidates; trusted invariant failure halts before its first mutation. |
| `process_spawn_exec`, foreground or detached | Load/vector/fd/action errors keep their current errors and cleanup. Root preflight failure returns IO; generation exhaustion AGAIN; both before publication. Existing file-action external effects are not promised rollback. | Foreground main-only bind; detached scheduler admission failure cleans child and returns AGAIN. Delayed background first bind revalidates the complete committed main tuple. |
| `process_exec_replace` | Load/vector/preparation/ID reservation/stdio-clone failures preserve old image/binding/association; IO for prep and AGAIN for exhaustion. Reserve before stdio; an unused ID can burn. | After stdio, invalidate/unbind, zero old generation, retire old backing while unbound, move staged image+generation and rebind. Late trusted validation failure halts; no false rollback or silent setter refusal. |
| Background start/quiet/lifetime/fds constructors | Existing false/-1 and cleanup on preflight/reservation/admission failure; never READY with zero ID. | Commit staged generation before scheduler publication; revalidate on delayed first bind. |
| Fork | After clone, prep/reserve before READY; IO/AGAIN as above with existing child cleanup. Descriptor-clone/admission errors unchanged. | New child generation, copied register image but empty association; parent generation/association unchanged. |
| Worker creation | Validate current parent tuple before admission, then new worker tuple/TLS/stack and root again before READY; existing -1/false with staged TLS/stack cleanup. Uninitialized scheduler returns false. | Inherit committed process generation; fresh immutable scheduler identity/empty association. Failure never invalidates the parent's association. |
| Main setter before scheduler init or malformed internal tuple | No user constructor is allowed to reach this case; main initializes scheduler before every production launcher. | Trusted invariant failure, not silent return. Host fixtures must cover it and verify zero tuple/hardware/accounting mutation. |
| Exit/clear/kill/reuse | No generation allocation or recoverable commit work. | Invalidate before first binding/FS/FPU/CR3/TSS or backing mutation. Main atexit continuation keeps the shared image generation; final image teardown retires it. |

For the fatal path propose one fixed short `serial_write` message followed by
`halt_forever`, entered with IRQs already excluded. Bypass `console_write`, which
can request framebuffer presentation. Audit the entire serial path: fixed bytes,
bounded SERIAL_TX_SPIN_LIMIT per byte, no allocation/lock/scheduler callback,
nested IRQ save/restore cannot enable interrupts, and optional latency recording
only performs bounded clock/counter reads. The halt must retain IF0; diagnostic
is best-effort if UART is absent/stalled. Host fixture substitutes a terminal
test trap and verifies that no binding/accounting/hardware mutation preceded it.
Owner must approve that audit or select a dedicated bounded diagnostic before
the metadata slice changes any trusted void setter behavior.
