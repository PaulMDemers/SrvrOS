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

## Free-range search and allocator scans

The continuation from `8115e7e4` confirms two further issues:

- Virtual free-range search advanced by the request length on conflict. With
  one occupied page, a two-page gap, then occupied remainder, a two-page request
  missed the only valid fit and reported failure. The old actual-source test
  fails its result assertion (`build/review/search/range-before.log`). Search
  now advances to the greatest end of an overlapping region and checkpoints
  before continuing. All intervening starts overlap an already intersecting
  interval, so this skips no valid fit. The hint-first then wrap order remains.
  Search progress depends on the bounded region table rather than virtual-page
  count; a fully occupied address window does not probe every requested-size
  stride. Atomic startup mapping still uses its unchanged bounded helper.
- PMM hints keyed only by minimum address retained the first small free hole
  even for requests needing a larger contiguous run. Identical failures on
  unchanged fragmentation rescanned the full eligible bitmap. The allocator now
  keeps its shared lowest-free frontier and adds run hints keyed by
  `(minimum frame, run length)`, retaining successful progress and exhaustion.
  Allocation cannot create a new earlier run. Free rewinds each affected hint
  to at least `freed frame - (run length - 1)`, clamped at its minimum, because
  an eligible run may start before a newly freed bridge. Hint eviction loses
  only the optimization. First-fit addresses, tags, poisoning and IRQ exclusion
  remain unchanged; no service window is added inside the allocator.

Regressions include the missed fit through both the helper and public mmap,
full-window exhaustion, hint wrap, inherited ownership, and 3,072 comparisons
against an independent page-by-page oracle across all 8-page occupancy masks,
four request sizes and three hints. PMM retains its 12,000-operation randomized
first-fit oracle and high-frame/tag/poison tests. The new fragmented case scans
32,515 positions once; 64 further identical refusals inspect zero additional
positions. Freeing a bridge must then return the earlier first-fit start, and a
smaller request must still see its remaining small hole. A dense-allocation
case cycles 192 requests through twelve sizes (more than the eight run slots),
requiring at most 1,248 visited positions. This caught a regression in the
initial size-only cache replacement (`mixed-before.log`); restoring the shared
frontier alongside the size-specific hints preserves dense mixed-size progress.
That intermediate regression was corrected before publishing this slice. The old implementation
fails the repeated-scan assertion (`pmm-before.log`).

Focused host tests pass normally; PMM and the service fixture pass relevant
ASan/UBSan and strict GCC14 runs. Kernel build passes. The updated kernel passes
Q35 sysabi and full threadstress with retained PR313 runtime files and disposable
disks. Final implementation `513199d5` includes the shared-frontier correction.
Main `a8e1fca1` was integrated at `16af9d76`; later main `0cba30a1` was integrated
at `10cbd087`, both without kernel/shared changes. The latter's new actual-source
workspace writeback matrix passes all 52 cases on this branch
(`workspace-compat/result.json`).

Logs and oracle/negative-control evidence are retained under
`build/review/search/`. A compiled control that omits the freed-prefix rewind
fails the first-fit oracle on the bridging allocation. This guards the cache
optimization's correctness, separately from its reduced scan count.

### Heap cost evidence

The existing `tools/bench_heap_startup.c` replay was compiled natively for ARM64
on this Mac with optimization and no sanitizers. Each epoch allocates 11,436
objects, churns 7,411 allocations, then frees the live set. The two epochs record
600,648,287 and 600,648,363 validation visits, with zero extra selection traversal
visits. Setup/free host times are respectively 816/230 ms and 1,669/372 ms.
These are one replay's host timings, not guest IRQ measurements or a native
SrvrOS acceptance claim. The unchanged heap test passes 6,000 live allocations,
fragmentation, distal corruption and invalid-free refusal, including exact
entry/exit validation visit counts.

No new heap correctness defect was confirmed. Full pre/post validation is an
existing integrity contract; permitting scheduling while that chain is being
validated or mutated would need a separate ownership design. It is retained.
The first PMM search for a new key, cache eviction/free churn, large poisoning
and full heap validation can still take substantial IRQ-masked work. This batch
removes redundant search work; it establishes no wall-clock latency bound.

## Initial PMM search and free-range poisoning costs

Continuation from `89a088eb` confirms two redundant-work cases without changing
allocator exclusion or poison policy. A cold occupied prefix was skipped only
one bitmap byte (eight frames) per search iteration. The allocator now checks
bounded groups of eight bytes and skips 64 fully occupied frames at once, with
no alignment/aliasing assumption and no load beyond the final bitmap tail.
Free bits still take the original per-frame path, so first-fit and runs crossing
byte/group boundaries remain visible. A cold-prefix counter regression fails
before the change and passes a bound of `FRAMES/64 + 16` afterward.

Multi-frame free previously traversed both hint families for every poisoned
frame. All of those updates happened inside the same IRQ-masked operation.
It now rewinds hints once after the range is poisoned and marked free, before
restoring IRQ state. Each hint uses the earliest freed frame eligible for its
minimum, retaining the size-specific bridging-prefix rule. A 16-frame release
regression reduces the hint-loop count from 128 to eight. Full poison bytes,
tag accounting, invalid-free halts and IRQ-state preservation remain unchanged.
No scheduling or reuse is admitted during poisoning. This does not add recovery
from fatal invalid frees.

Tests preserve the 12,000-operation first-fit oracle, mixed-size progress,
exhaustion/bridging behavior and high physical-frame coverage. An explicit
319/320-frame boundary run catches incorrectly skipping a group containing a
free bit. Existing release checks inspect every poison byte. Normal host tests,
PMM ASan/UBSan and GCC14 checks pass; kernel build passes. Baseline failures and
passing evidence are under `build/review/poison/`. Implementation `d9b69e36`
passes rebuilt-kernel Q35 sysabi and full threadstress against retained PR313
runtime files on disposable disks. Main `4f5edb0e` is integrated with no
kernel/shared delta. This is a kernel regression check, not a complete rebuild
of newer userspace. The newly integrated actual-source salvage matrix also
passes all 49 cases on this branch (`salvage-compat/result.json`).

The poison data writes still scale with the number of released bytes. Heap
validation remains unchanged, and first scans still scale with physical-memory
coverage. The changes reduce redundant bookkeeping and loop iterations; they do
not establish a hardware IRQ-latency deadline. Adjacent review confirms kcalloc
zeros its payload after the heap lock is released, but an already IRQ-masked
caller retains that state; caller-specific large initialization work remains
separate audit scope.

## Caller-masked initialization and storage DMA lifetimes

Continuation from `4107a6f8` traces caller-masked initialization and AHCI/VirtIO
storage ownership. Two bounded issues are fixed:

- Exec used kcalloc for its large temporary process image with the syscall's
  IRQ mask inherited. The heap lock ends before zeroing, but there were no
  effective service turns during initialization. A private image allocator now
  holds the ordinary process owner, allocates without yielding inside heap
  mutation, then zeroes in 64 KiB batches with checkpoints. No image reference
  or PTE is published at those turns. Tests check complete zeroing, multiple
  turns, nested owner retention, OOM and foreign-owner refusal. A compiled
  negative control suppressing these turns fails the checkpoint assertion.
- AHCI allocated four DMA pages before checking allocation success and retained
  earlier allocations on OOM. The real-source baseline fails the release-count
  assertion. Initialization now collects pages locally and releases only that
  unpublished attempt on failure; disk fields are assigned after all four
  succeed. Each of the four refusal positions is covered. The caller stops the
  port before allocating and publishes CLB/FB only after success. No timeout or
  uncertain active-DMA path is permitted to reclaim backing by this change.

The reviewed AHCI command-failure path marks the disk offline and pins command,
FIS and bounce pages. VirtIO block invalidates block identity and frees its DMA
backing only after transport stop succeeds; failed reset retains quarantine.
The transport verifies reset status and bus-master disable before queue backing
release. Existing actual-source timeout, stuck CI, status/error, completion,
reset-failure and quarantine fixtures pass. This is sampled source/protocol
coverage, not physical-device or arbitrary hot-removal acceptance. Driver
reinitialization and broader network/input/display DMA lifetimes remain open.

Focused storage, process-service/output and VirtIO PCI/block tests pass. AHCI
and service fixtures pass ASan/UBSan and GCC14; a GCC fixture indentation warning
was corrected before the final strict pass. Kernel build passes. Evidence is
under `build/review/dma/`. Implementation `01071cfb` passes rebuilt-kernel
Q35 sysabi and full threadstress with retained PR313 runtime files and disposable
disks. These guests exercise normal DMA setup/exec paths; deterministic host
fixtures supply allocation failures and modeled reset/quarantine boundaries.
General kcalloc and poisoning behavior remain unchanged. Other caller-masked
initialization sites still require individual publication/ownership review.

## Network, input and display DMA checkpoint

Continuation from `fe8e0b69` confirms unsafe repeated E1000 initialization.
`e1000_init` allocated replacement rings, reset software indices and rewrote
live DMA registers on another call, without a stop/reset proof. The actual-source
regression first initializes the driver over modeled MMIO/DMA and records a live
RX descriptor and nonzero ring indices; the old implementation fails the
no-new-allocation/state assertion (`build/review/network-dma/before.log`).

E1000 now records its boot initialization attempt and result. Repeated calls
return that result without enumeration, allocation, PCI/MMIO writes, ring or
provider changes. A failed or absent first attempt stays refused; recovery and
hotplug require a separate quiescence protocol. No DMA pages are newly freed by
this change. The guard uses the existing exclusive CPU0 boot-owner contract;
it is not an SMP initialization primitive. A test-only MMIO address override
allows the normal driver code to use a host register array.

The new `test-e1000-init` core target checks success, missing device, MMIO failure
and allocation failure in separate processes. It verifies complete MMIO/DMA
snapshots, indices, PCI writes, enumeration, allocation and registration counts.
Existing RX reentrant-poll and link tests pass. The new fixture passes strict
GCC14 and Darwin/Rosetta ASan/UBSan.

Sampled VirtIO network failure paths clear readiness before transport stop and
retain buffers on failed reset. Input retirement cancels logical key/pointer
state before stopping, retaining event DMA when stop fails. GPU presentation
stop retains backing/queues after reset failure and blocks restart; proven reset
precedes freeing backing. Existing real-source net/input/GPU/transport fixtures
pass reset-failure, malformed completion, stop, restart, quarantine and callback
slicing cases. These tests model PCI/MMIO/device responses; they do not establish
physical-device or arbitrary hot-removal acceptance. No additional confirmed
VirtIO defect was found in this bounded pass.

Implementation `8db6e2df` compiles and passes rebuilt-kernel Q35 netabi
(explicit E1000 device) and sysabi against retained PR313 runtime files and
disposable disks. Evidence is under `build/review/network-dma/`. Main `1f177761`
was integrated at `ec22a216` without kernel changes. An appended-Makefile conflict
was resolved preserving both E1000-init and salvage test targets, which pass
together. The newer salvage helper was not staged into these kernel-only guests. Existing review VMs and other
checkouts remain untouched. Broader native E1000 admission, xHCI/USB, display
hardware ownership, DMA concurrency and explicit recovery remain audit work.

## Remaining work

Other long IRQ-disabled operations, including initial/churning PMM scans, heap validation and poisoning,
driver DMA/reset/removal ownership and broader manual isolation review remain
open. Deterministic fault/dirty-writeback ordering now has the host coverage above;
forcing it through actual VM context switches remains separate work. Future boot
paths must establish or verify WP rather than inheriting this configuration's
acceptance. The historical [coverage ledger](audit-coverage-2026-11-02.md) retains
the broader gaps.
