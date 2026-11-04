# Kernel isolation and teardown audit — 2026-11-03

## Scope and coordination

Separate Apple Silicon audit checkout, branch
`device/apple-silicon/kernel-isolation-audit`, started at PR313 `3cd9ce86`
and advanced to merged main `56e1f950`. PR313's owner retained integration
and VM ownership; its merge includes PR252 and PR308 by ancestry and pins
SrvrPorts `5e994f53`. The initial VMM checkpoint changed no process, syscall, namespace,
port, or ABI implementation. The subsequent teardown checkpoint below adds
process IRQ test hooks and reuses existing IRQ primitives without changing
admission or retirement policy. It does not complete the broader audit.

## Confirmed VMM boundary defect

The public map/ensure, protect, unmap and physical-address lookup helpers
indexed page tables without first requiring a canonical virtual address.
Upper bits disappear during index extraction: `0x1000000001000` and
`0xffff000000001000` select the same entries as `0x1000`.
Thus an invalid input could return another mapping's physical address or
alter its PTE. The existing user-buffer and ELF checks already reject these
inputs at their boundaries; this finding does not demonstrate an unprivileged
syscall bypass. The low-level VMM API nevertheless failed its address contract.

All four shared boundaries now reject noncanonical addresses before table
access or allocation. Map and ensure share the same implementation. Both lower
and upper canonical addresses remain accepted. Refused operations leave the
existing PTE, absent destination slot and physical-output sentinel unchanged.
`tools/test_vmm_user_range.c` includes both malformed encodings, all five APIs,
a positive upper-half lookup, and the existing permission/large-page cases.
The baseline aborts on the physical-lookup refusal assertion; the fix passes.

## Teardown and write protection

The expanded actual-source fixture also verifies that destroying an active
user root switches to the kernel root before any tracked table frame is freed,
retires table frames in reverse order and the root last, removes registration,
and leaves zero/kernel roots alone. A host-only CR3 write hook makes the switch
observable. Production CR3 behavior is unchanged.

Source tracing covered `process_exit_gate`, worker versus initial-thread exit,
sibling cancellation before file cleanup, `finish_process_files`, final content
release, inherited-reader writeback ownership and address-space cleanup. No new
process-exit defect was confirmed. Existing process fixtures cover first fatal
cause, exit admission, mapping publication before service checkpoints, inherited
ownership, clean-space retry and sticky writeback errors. These fixtures do not
prove every scheduling interleaving or SMP behavior.

A separate disposable Q35/TCG baseline boot on this ARM64 Mac reached the monitor
with `CR0=0x80010033`: bit 16 (WP) is set. The kernel source still does not
explicitly enable WP; this boot relies on the bootloader-provided state. This
resolves the earlier unmeasured-state question for this BIOS/QEMU configuration
only, without claiming native or alternate-bootloader acceptance. Its register
and serial records are local under `build/review/isolation/baseline.*`.

## Verification

- Native ARM64 ASan/UBSan expanded VMM fixture passes. The old source fails
  the new noncanonical lookup assertion (`vmm-before.log`).
- x86_64 Darwin/Rosetta host targets pass: `test-vmm-user-range`,
  `test-process-fault`, `test-process-service`, `test-user-fault`,
  `test-syscall-user-copy`, `test-process-output`. Fault and service fixtures
  also pass ASan/UBSan.
- Freestanding x86_64 kernel build passes with warnings treated as errors.
- The rebuilt-kernel Q35 guest passes `sysabi_smoke.py`, including user-fault
  recovery (`sysabi-fixed.log`). Its initramfs and disk are retained PR313
  runtime artifacts; the kernel is newly compiled from this audit tree.
- Local Markdown checking found nine pre-existing missing license targets in
  uninitialized nested port submodules; no new documentation target is missing.
  This checkout does not claim a complete passing link check.

Local evidence: `build/review/isolation/`. The boot image uses the newly built
kernel with unchanged runtime files extracted from PR313's published image;
this is a kernel regression boot, not a complete clean image rebuild. Test
harnesses copy the source disk into disposable storage. Existing review VMs
and the primary and filesystem checkouts remain untouched. An initial ISO
boot-info replay emitted an xorriso error; its output was discarded. The next
extraction exposed read-only permissions on the extracted kernel, so that run
is not claimed as changed-kernel evidence. After explicitly making the private
kernel copy writable, the final ISO was rebuilt with the repository's boot
options and BIOS installation; its kernel was compared byte-for-byte with the
new build before the passing guest run.

## Deterministic fault/writeback teardown checkpoint

The continuation from `b6a0feb8` executes the actual IDT user-fault dispatcher,
`process_exit`, file writeback/close, futex cleanup and mapping-retirement loops.
The existing output fixture supplies deterministic scheduler and device callbacks;
it does not run a real context switch or real exFAT I/O. No sleeps or racing host
threads determine whether the intended interleaving occurs.

`make test-process-output` now also runs its `teardown` mode. The twelve schedules
cover normal initial-thread exit and an invalid-instruction worker fault after
the initial thread has exited, each with and without an inherited description,
and successful, clean-retryable and sticky-error writeback. The controlled foreign
owner flushes shared dirty content while fault/exit admission is pending. In the
fault cases another owner takes a dirty turn before the parked initial thread
resumes with IRQs enabled, forcing it to reacquire admission at `process_exit`.

Assertions require descriptors, mappings and futex state to remain live during
foreign ownership; sibling retirement and futex cleanup before exit writeback;
file release before frame retirement; user-context clearing before address-space
cleanup; correct cached/final status; and retained dirty data/error ownership for
the surviving description. Normal successful status becomes 1 on writeback error;
a fatal SIGILL status survives those errors. The existing first-fatal-cause test
continues to cover competing fault records.

No new production defect was confirmed. To execute the complete exit function
on the host, its duplicate CLI/STI instructions now use the existing
`process_irq_save`/`process_irq_restore` primitives (the worker still explicitly
enables IRQs before scheduler retirement). An opt-in host-test hook models IF;
other host fixtures retain their default behavior. Production admission,
writeback ownership, sibling cancellation and retirement order are unchanged.

Verification:

- Twelve controlled schedules and the existing output suite pass on strict Linux
  GCC 14 and Darwin/Rosetta. GCC exposed a missing explicit `stdint.h` include in
  the new fixture; it was corrected before the passing run.
- Darwin/Rosetta ASan/UBSan passes the twelve schedules. Two compiled negative
  controls abort: omitting `process_exit` admission reaches sibling cancellation
  under foreign ownership; omitting the gate's IRQ mask reaches admission with
  IRQs enabled. These are injected regressions, not newly discovered defects.
- Focused host targets pass: process output, process fault, process service,
  user fault, syscall user copy and the actual scheduler ownership fixture.
- The rebuilt kernel passes Q35 sysabi and full threadstress against retained
  PR313 runtime files on disposable disks. These guest suites supplement the
  deterministic host model; they do not force the same interleaving in a VM.
- Kernel compilation passes. Evidence is under `build/review/teardown/`; the
  focused host log is `build/review/isolation/teardown-host-suite.log`.

Adjacent source tracing checked worker TLS release after context clearing and
reapable publication. TLS release performs bounded page retirement under the
existing IRQ mask without checkpoints; the main thread therefore cannot run
between its context clearing and completion. Parent reaping requires completed
runtime retirement and no live scheduler reference. No additional defect was
confirmed in those inspected paths. The test's scheduler/device callbacks remain
modeled boundaries; real DMA and arbitrary scheduling interleavings are open.

## Mapping and protection service checkpoint

Three service gaps were confirmed in the continuation from `327cb50e`:

1. `mprotect` and private `msync` ran their generic validation/mapping scans
   without an owner scope or service checkpoints. For each page `mprotect`
   searched the unsorted tracking array twice, producing quadratic work over
   large mapped ranges. An actual-source 65-page regression aborts on the old
   implementation because the PTE update has no service owner; the corrected
   path permits service turns and retains inherited ownership.
2. Failed generic `mmap` rolled back each allocated page with another tracking
   search and no service checkpoint. A deterministic allocation refusal after
   65 new pages confirms that the old path frees all of them without a turn.
   The new regression requires at least two rollback checkpoints and verifies
   that two earlier mappings survive. This is independent of the separately
   bounded atomic startup-map path, which remains unchanged.

3. `user_space_write_mode` checked copied byte counts modulo 128 KiB. For an
   unaligned destination, page-end counts never reach those exact multiples,
   so a long successful copy can miss every checkpoint. It now counts page
   chunks instead. The 256 KiB copy regression starts at offset 17, requires
   two service turns, and checks both untouched boundaries. The original byte
   condition completes the copy but fails the turn-count assertion
   (`copy-before-corrected.log`). The first test attempt exposed an inaccurate
   host translation stub for unaligned addresses; correcting its page lookup
   and byte offset produced this isolated baseline failure.

`mprotect` and `msync` now use ordinary process service scopes, refusing an
unavailable foreign owner. Protection visits tracking entries once and applies
PTE/record changes atomically before each checkpoint. Missing-frame reservations
are populated with page-table lookups rather than repeated tracking scans.
Private `msync` counts unique tracked pages after validating registered-region
coverage; it still performs no shared-file writeback. `munmap`, `mprotect` and
`msync` validate coverage by advancing between region boundaries rather than
searching the region table for every page. Sparse PROT_NONE protection and unmap
therefore need not walk every unbacked virtual page.

Generic mmap rollback now retires only the newly appended tracking suffix in
reverse order. Every removal is published before a checkpoint; pre-existing
mappings remain owned. Service checkpoints occur after each validated region
and batches of 32 mapping/page visits or retirements. They retain the I/O owner,
exclude sibling mutation and defer cancellation under the existing scheduler
contract. No syscall-safe classification, ABI, mapping cap, or atomic startup
path changes. Existing partial mprotect failure behavior remains supported.

The expanded service fixture checks an unsorted 65-page mapping, coherent
modeled PTE/tracking flags at every turn, no more than 32 protection writes
between checkpoints, nested owner retention, foreign-owner refusal, PTE failure,
coverage-gap refusal, allocation failure/retry from PROT_NONE, whole-window sparse
reservations, and suffix rollback. Futex revocation tests use the updated private
index-based protection helper and continue to exercise the verbatim body.

Focused Darwin/Rosetta host tests and strict Linux GCC14 pass; the service
fixture also passes ASan/UBSan. Both old-behavior failures are retained under
`build/review/teardown/protection-before.log` and
`build/review/protection/rollback-before.log`; passing evidence is under
`build/review/protection/`. The freestanding kernel builds. Both the protection/rollback kernel and the
final unaligned-copy kernel pass sysabi and full threadstress; final logs are
`sysabi-final.log` and `thread-final.log`. The unchanged PR313 runtime files are
retained, so this is a rebuilt-kernel check, not a clean image of newer userspace.
Main `805519b2` was integrated at `d4e91913`; that merge changes no kernel/shared
source. The final tested implementation is `4ce73e79`; subsequent documentation
changes preserve its kernel implementation.

These checks bound batch work and show service opportunities; they do not set a
wall-clock IRQ-latency guarantee. PMM allocation, fixed-size table scans and
service scheduling still contribute to elapsed time. Clone copies already retain a service owner and map pages in batches; their
checkpoint can precede a private child page copy while the child is not runnable.
The unaligned-copy gap above was confirmed in the shared copy helper. Free-range
search and allocator scan costs remain separate audit work.

## Remaining work

Other long IRQ-disabled operations, including free-range search and allocator scans,
driver DMA/reset/removal ownership and broader manual isolation review remain
open. Deterministic fault/dirty-writeback ordering now has the host coverage above;
forcing it through actual VM context switches remains separate work. Future boot
paths must establish or verify WP rather than inheriting this configuration's
acceptance. The historical [coverage ledger](audit-coverage-2026-11-02.md) retains
the broader gaps.
