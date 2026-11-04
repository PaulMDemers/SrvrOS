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

## USB/xHCI and native display teardown checkpoint

Continuation from `2e304da2` confirms repeated xHCI entry cleared the entire
controller/device record before discovery. Existing workers and uncertain DMA
can still reference those records. A one-attempt guard now preserves state on
repeated boot entry, including an absent/failed first discovery. Recovery,
hotplug and reset/reinitialization are not supplied by this guard.

The actual-source host regression performs absent discovery, then seeds a
modeled live controller with DMA addresses, nonzero ring indices, a quarantined
device and a started-worker marker. Another init call must preserve the complete
record, allocation/free counts and worker marker without rediscovery. The old
function fails this assertion (`build/review/usb-dma/before.log`). This is a
seeded ownership test, not a full modeled successful hardware bringup. It runs
with the existing xHCI root-port, handoff, scratchpad, HID/storage/network,
late-event, timeout and quarantine tests. Normal, ASan/UBSan and GCC14 runs pass.

The reviewed USB storage timeout path leaves pending backing pinned and rejects
reuse; a trackpad endpoint can reclaim a ring on buffer-allocation failure before
configuration, but ambiguous configure completion retains both allocations.
The new guard prevents another init from discarding those retained identities.
Explicit recovery and device removal remain separate work.

Display review covers `intel_gfx.c`, `display_present.c` and the native diagnostic
`display_owner_core.c`. Intel discovery maps registers and reports planned
capabilities; it does not implement accelerated native DMA teardown. The native
owner model retains a fault guard even when references drain and grants no
hardware write authority. Its existing token/epoch/inflight/fault tests pass.
The runtime presentation layer and VirtIO GPU reset-before-free fixtures also
pass. A new staging regression requires uncertain end/reset to retain switched
backing, owner and blocked writes; only the private host arena is disposed after
assertions. No additional display defect was confirmed. No native-device reset,
physical removal or framebuffer ownership acceptance is inferred from these
models.

Implementation `68a74cdd` compiles and passes rebuilt-kernel sysabi and full
threadstress with retained PR313 runtime files. An additional Q35 guest includes
an emulated xHCI controller, USB keyboard and tablet: it reports `op=yes`, two
configured devices, one keyboard and one absolute pointer, then passes sysabi.
No guest HID input injection or physical teardown is claimed. The display
retention fixture also passes ASan/UBSan. Evidence is under
`build/review/usb-dma/`. No libc or filesystem policy changes.

## Command-ring identity, saturation and removal checkpoint

Continuation from `c7e03477` confirms a successful command completion for a
*different* TRB was accepted as the current command. A timed-out command also
left the producer available for reuse despite uncertain controller progress.
Completions now require exact TRB identity; failed completions leave slot output
untouched. Nested submission is refused, and an ambiguous timeout permanently
blocks command submission for this boot while preserving backing. This is safe
containment, not command abort/reset recovery. Reinitialization cannot clear it.

The actual-source regression fails on the old successful-pointer mismatch. It
also covers nested submission, a known error completion, two full command-ring
wraps and Link cycle transitions, timeout, an exact late completion, and more
than two rings of refused submissions without DMA mutation/free. The host timeout
budget is reduced; the production budget is unchanged.

Removal review follows storage and ECM transfer quarantine, pending-event
identity and allocation ownership. A new storage test delivers an exact late
completion after timeout: pending state retires, but failure remains sticky and
backing cannot be reused or freed. An ECM removal regression also delivers exact late events for pending endpoints
and requires sticky quarantine, unchanged ring producers and retained backing;
existing disconnect/pending-RX retention and stale-event fixtures also pass. No additional removal defect was confirmed.
The driver does not supply general slot teardown, hotplug/re-enumeration, command
abort/reset or safe unpinning after removal; these remain explicit gaps. The
single pending-command guard covers synchronous/nested callers, not a future
SMP submission API.

Normal/Rosetta, GCC14/Linux and ASan/UBSan xHCI suites pass, as do storage,
display presentation and native-owner fixtures. The rebuilt kernel passes Q35
sysabi with an emulated xHCI keyboard/tablet (seven matching commands, zero
pointer mismatches, two configured devices) and full threadstress. Guests use
retained PR313 runtime files and disposable disks; no complete userspace rebuild
or physical-removal acceptance is implied. All task guests stopped; existing
review VMs were preserved. Evidence: `build/review/command-ring/`. Documentation
checking retains only the nine known missing nested-submodule license targets.

## Reset/re-enumeration prerequisites and removal follow-up

Continuation from `7240b5ce` confirms two additional unsafe reuse paths. HID
polling and successful late transfer events could keep an unplugged endpoint
armed, and reconnecting the root port could resume the old boot identity. EP0
control submission also remained available after a timeout or error without an
endpoint recovery transition. A third defect appeared while tracing failed
re-enumeration: record allocation indexed the successful-address count. An
address failure after slot/context publication could therefore have its record
overwritten by the next root or hub child while the controller still owned it.

Root disconnection or a new root connection-change bit now permanently
quarantines every affected record when observed by polling, transfer dispatch,
control or storage submission. This also covers root-hub loss and a fast
unplug/replug where CCS is already set again. Keyboard held-state/repeat is
released, trackpad cancellation uses the existing source-specific path, and late
reports cannot be injected or rearmed. Slot, ring, buffer and provider identities
remain pinned. A second root enumeration refuses an existing record. Failed
EP0 submission blocks later TDs; ambiguous pending state remains retained.
Record admission scans unused records independently of successful address count,
retaining failed attempts through bounded table exhaustion.

Actual-source regressions fail against the previous source on removal and EP0
ring mutation, and on failed-record reuse. Removal cases cover CCS loss, a sticky
CSC after quick reconnect, a child behind a lost root hub, pending backing,
late-report suppression, source-specific trackpad cancellation and
repeat/enumeration refusal. EP0 tests cover both
timeout and a received error; every refused retry must preserve the ring. These
fixtures model MMIO and allocations, not physical USB acceptance.

Runtime command abort/reset and successful hotplug re-enumeration remain absent.
Review against [Intel xHCI revision 1.2b](https://cdrdv2-public.intel.com/625472/625472_xHCI_Rev1_2b.pdf),
sections 4.6.1.1–4.6.1.2 and 4.6.4, establishes the needed boundary: ring restart
requires stopped-state handling and event accounting; slot disable requires
endpoint/command retirement. Neither successful abort nor a reconnected cable
alone retires software provider references. The current failure latch cannot
safely be cleared into recovery by calling boot initialization. No reset/hotplug
API is added or accepted here. Downstream-hub-only changes, endpoint-stop/drain,
provider retirement and native reset qualification remain separate work.

The final candidate passes normal/Rosetta, strict GCC14/Linux and ASan/UBSan
xHCI fixtures, kernel compilation, QMP removal/replacement plus sysabi, and full
threadstress. Storage/display host fixtures also pass. The QMP check preserves
two old slot identities as unconfigured with no additional command completions
after replacing both devices. VM images use the rebuilt kernel and retained
PR313 runtime files, not a complete current-userspace rebuild. All task guests
stopped; review VMs remain untouched. Evidence: `build/review/usb-recovery/`.
The documentation checker still reports only nine known missing license targets
in uninitialized nested port submodules. Main `1a5a6dee` was inspected for shared
scope; this candidate retains integration base `1f177761` and does not incorporate
the newer unrelated userspace work.

## Endpoint/event draining and provider retirement checkpoint

Continuation from `cd6ec856` confirms root removal marked USB storage failed
without invalidating its published block identity. `block_read` could still
return an already cached sector without calling the failed transport. Removal
now calls the existing `block_invalidate` contract: the parent and aliases refuse
I/O and their cached data is discarded, while descriptors, claims and DMA stay
pinned. No block-layer, filesystem, process or fsync contract changes.

The xHCI host fixture now includes the actual block/cache and claim sources.
It primes parent/alias/independent-device caches, removes the USB root, requires
parent and alias reads to fail without modifying the caller buffer or invoking
the transport, and retains the independent healthy cache. The saved mount claim
can still be released exactly once. The earlier source fails its cached-read
assertion (`build/review/usb-retirement/before-storage.log`). This combines real
cache/claim code with a modeled physical transport; it is not a VM storage
hot-removal proof.

Two further baseline failures cover endpoint/provider admission. A fatal HID
completion or impossible residue cleared pending state but left polling enabled,
allowing a new TD without endpoint recovery. The interrupt endpoint now has a
sticky failure state, stops rearming and releases keyboard held/repeat state;
late matching reports cannot revive it. Trackpad and other endpoints retain their
independent ownership. Network send/poll/service callbacks now check root
identity before accepting/processing work, rather than waiting for the periodic
HID worker; a newly set CSC with CCS already high quarantines the old provider.
Callbacks retain pending endpoint backing and refuse fresh work. Provider
selection remains immutable for the boot; no unregister, failover or memory
reclamation API is introduced. Existing read-only link/readiness snapshots do
not perform hardware retirement.

A continuously refilled stale-event fixture drains exactly one event ring per
service turn over two cycle transitions, with no late input, rearm or frees.
64-bit MMIO writes now use the existing two ordered 32-bit write primitives,
allowing the host model to observe ERDP retirement as it observes doorbells.
This preserves hardware write order. Bounded software event consumption is not
a hardware stop/drain acknowledgement: endpoint Stop/Disable Slot ordering,
command abort/reset and provider-reference retirement before freeing/reuse are
still missing. Successful runtime re-enumeration and physical teardown remain
unaccepted. No additional event-drain correctness defect was confirmed in this
bounded pass.

Normal/Rosetta, strict GCC14/Linux and ASan/UBSan xHCI fixtures pass, along with
block/AHCI, USB storage/ECM and keyboard host suites. Kernel compilation, QMP
keyboard/tablet removal/replacement followed by sysabi, and full threadstress
pass. The VM evidence uses the rebuilt kernel with retained PR313 runtime files;
it does not exercise USB-storage removal or fatal HID hardware completions, and
is not a complete current-userspace build. All task guests stopped; existing
review VMs were preserved. Evidence: `build/review/usb-retirement/`. Link checking
retains only nine known missing nested-submodule license targets. New main
`c90f3522` and PR329's independent syscall-only fix were inspected for scope;
this branch retains integration base `1f177761` without that newer work.

## Hardware quiescence and retained-provider references checkpoint

Continuation from `e2457b1b` reviews the actual frees in xHCI, shared VirtIO PCI,
input, net, block, RNG and GPU paths. xHCI only frees scratchpad/setup allocations
before controller publication and a trackpad ring if its buffer allocation fails
before Configure Endpoint. Published xHCI allocations remain pinned; no runtime
stop/free implementation is implied. AHCI and E1000 retain published backing.
The sampled audio allocation-refusal cleanup also precedes stream publication;
its ordinary stop retains PCM backing.

The VirtIO ownership boundary is a completed device reset, not an empty event
ring. [VirtIO 1.2 section 2.4](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html)
defines status zero as reset completion and prevents further queue interaction
until reinitialization. Shared PCI teardown observes reset, checks PCI disable
and only then frees queue allocations. Caller drivers deactivate admission before
stop/free; failure retains caller backing. Block retirement invalidates its
published identity before reset, input retires source state, RNG erases its
private pool and GPU holds its presentation owner across reset/reclamation.
These are source checks plus modeled regression evidence, not physical-device
or hostile-controller acceptance.

A new actual-driver VirtIO-net regression retains the registered callback copy
while a receive-delivery callback triggers a TX fault and teardown. The delivered
packet must remain valid because it is a private copy, while reset succeeds and
DMA is reclaimed or reset fails and all backing stays pinned. Every free invokes
send/poll/link reentry; later repeated retained callbacks run with both IRQ states
from a foreign CR3. They must refuse work, preserve CR3/IRQs, avoid delivery or
further frees and refuse initialization/rebinding. An injected free-before-stop
control fails the new reset-proof assertion. The registration and transport
boundaries are modeled; the driver and reclamation helpers are actual source.

A new shared-PCI case accepts reset completion but rejects the PCI disable write.
It requires unchanged queue allocation identities, zero frees, inactive admission
and permanent quarantine even if later PCI writes would work. This passes the
existing driver. The header incorrectly promised decode/bus mastering was always
disabled on quarantine; it now states disable is attempted and failed readback
may leave those bits enabled, requiring retained backing. RNG stop's kernel-CR3
and IRQ-exclusion requirement is explicit. Network API comments now reflect
boot-lifetime callback/context retention and the fact that link snapshots may
read configuration and retire a faulting provider without delivering RX/TX.

No additional runtime premature-free or retained-callback defect was confirmed
in this cohort. There is no new reset, unregister, hotplug or reclamation feature.
SMP, physical reset/removal, xHCI stop/drain and generic provider-reference release
still need separate implementation/acceptance. Current CPU0 serialization and
stable contexts are prerequisites, not a general concurrent reclamation scheme.

The new net/PCI cases pass normal/Rosetta, strict GCC14/Linux and ASan/UBSan;
existing VirtIO input/RNG/block/GPU and xHCI suites also pass. The private
free-before-stop negative control fails at the intended proof assertion. All
production edits in this cohort are header comments; token comparison against
`e2457b1b` confirms unchanged runtime declarations/code. No kernel/VM rebuild was
needed or claimed for these test/documentation edits; the earlier VM acceptance
remains historical evidence at its recorded source. Review VMs were untouched,
and no task guest was started. Evidence: `build/review/dma-reclaim/`. Link checking
still reports the same nine missing nested-submodule license targets.

## Terminal xHCI halt/drain and network-reference release

Continuation from `72c3af97` adds a terminal shutdown boundary rather than
clearing the existing quarantine into reset/re-enumeration. `xhci_stop` requires
CPU0/kernel CR3 and successful firmware ownership. It first blocks new work,
marks existing devices unavailable, invalidates storage identities and retires
registered USB network callbacks. Synchronous command/control/bulk calls hold
explicit references through their return paths; an in-call stop is deferred until
they unwind. Cancellation preserves failure outputs. The poll worker loses
operational admission, while its records remain allocated.

The halt helper had a confirmed false-proof defect: all-ones MMIO could satisfy
HCHalted, as could a stale HCH bit after a rejected Run/Stop write. The previous
helper fails the new assertion in `build/review/xhci-stop/before-halt.log`; no
historical premature free is claimed, because the old path subsequently reset
and retained published xHCI backing. Halt now rejects inaccessible registers and
requires valid command readback with R/S, HCRST and interrupt enable clear, plus
HCH set and CNR clear. [Intel xHCI 1.2b section 5.4.1.1](https://cdrdv2-public.intel.com/625472/625472_xHCI_Rev1_2b.pdf)
defines completion of queued work before HCH acknowledgement. A full event ring
can block progress, so shutdown discards at most one ring while waiting and at
most one after halt, then requires an empty ring. Continuing production or an
unproven halt returns false with admission closed and backing pinned; explicit
retry can make progress. Poll budgets are finite, not calibrated IRQ deadlines.

`net_device_retire` supplies generic **network** registry reference release. Its
identity is the registered context/send pair. Retirement permanently blocks all
new send/poll/link/wake/inventory callbacks, while counted existing callbacks may
finish. False means unknown identity or outstanding references; retry after
unwind. True means callable references from that registration have detached. Owners
must retire every registration sharing a context before freeing it.
Names/MAC metadata are registry-owned; selection and opaque identity tombstones
remain stable, without failover, slot reuse or rebinding. Hardware ownership is
an independent requirement: this API does not stop DMA or authorize freeing
block, worker or diagnostic references. Existing providers can keep their prior
boot-lifetime retention policy; xHCI uses retirement during terminal shutdown.

Host cases cover nested retirement from every callback, two nested reference
holders, metadata ownership, unknown identity, context/name free followed by
repeated calls, and permanent tombstones. Actual xHCI helpers cover inaccessible
MMIO, rejected R/S, delayed acknowledgement, missing ownership/foreign CR3,
provider refusal, stop inside command/control and yielded bulk calls, failure
outputs, continued event production, bounded retry, idempotence and no frees.
Registry and hardware boundaries are tested separately; this does not qualify
concurrent SMP teardown or physical-device reset.

The monitor command `xhcistop` invokes the terminal boundary; `xhci` reports its
lifecycle, outstanding owners and drained events. It stops USB input for this
boot. `usb_removal_smoke.py --terminal-stop` exercises the command twice with
live keyboard/tablet endpoints before QMP removal/replacement, then requires
unchanged slot identities/command count and passing sysabi through serial.

No xHCI DMA or device record is freed: block registry descriptors/claims, worker
and diagnostic references still exist. Per-slot Stop Endpoint/Disable Slot,
controller restart/reset recovery, block/worker reference release and successful
hotplug remain open. A verified terminal schedule halt and released network
callbacks are now implemented; they are not complete reclamation authority.

The final implementation passes normal/Rosetta, GCC14/Linux and ASan/UBSan
registry/xHCI tests, plus VirtIO net/PCI, block/AHCI and USB-storage host suites.
Kernel compilation passes. A Q35 guest with live keyboard/tablet endpoints
reports `stopped-retained` twice, `owners=0`, `op=no` and no pending software events;
it then survives QMP removal/replacement and passes sysabi. E1000 netabi and full
threadstress pass in separate disposable guests. These use the rebuilt kernel
and retained PR313 runtime files, not a full current-userspace rebuild or native
acceptance. All task guests stopped; review VMs remained untouched. Evidence:
`build/review/xhci-stop/`. Documentation checking retains nine known missing
nested-submodule license targets. Branch integration base remains `1f177761`;
new main `10890eed` was inspected for scope, not merged into this candidate.

## Block, worker and diagnostic retirement before xHCI DMA release

Continuation from `c9f4f306` adds explicit `xhci_reclaim`/monitor `xhcireclaim`.
It first completes terminal stop, then requires zero command/control/bulk and
poll owners, a poll-worker retirement acknowledgement, zero active input-probe
diagnostics, retired network registrations and retired USB-storage callbacks.
The worker is allowed one retirement turn after operational admission closes;
it acknowledges before returning and does not touch DMA afterward. Read-only
status/snapshot paths retain static metadata and never dereference DMA backing.
An active input probe holds a separate reference and exits on terminal stop.

`usb_storage_retire` blocks new I/O/diagnostic admission and waits for initializer,
request and diagnostic owners. Startup has a distinct owner because partition
scanning itself needs ordinary block callbacks. A retirement request during
startup cannot publish a successful live device. Once drained, transport and
diagnostic context pointers detach. Block geometry/name/claims and storage
records remain boot-lifetime tombstones, with context-free refusing callbacks;
saved raw read/write/flush callbacks also safely reject a detached context.
Write-enable/disable metadata publication is IRQ-serialized with retirement.

Two retirement hazards were caught before publication: clearing `write` would
hide an uncertain writable disk from `block_flush_all`, and allowing new
post-request diagnostics could prevent reference drain. Writable tombstones now
retain refusing write/flush markers, so global sync/reboot refusal remains; new
diagnostic admission closes immediately. Private controls omitting either rule
fail their intended assertions. No historical xHCI premature free is claimed:
published backing was retained before this implementation.

After software retirement, PCI bus mastering must read back disabled before
freeing any DMA. Every reclaim call rechecks it. Each call frees at most sixteen
4 KiB frames, returns false for further progress or unresolved proof, and keeps
all remaining pages pinned. Partial progress cannot restart. Reclamation covers
controller rings/DCBAA/ERST, scratchpad frames and their recorded metadata pages,
per-device contexts/EP0/descriptor/HID/bulk/trackpad backing and network endpoint
rings/buffers. Freed pointer/physical fields are cleared. Static records, MMIO
mappings, block descriptors/claims and network metadata are retained; no slot
reuse, controller reset/restart or hotplug re-enumeration is added.

The actual-xHCI fixture allocates 513 scratchpads with two metadata pages plus
controller/device/network backing. It poisons each released frame and requires
exact-once accounting, no more than sixteen frees per call and refusal while
worker/poll/probe/storage or PCI proof is unresolved. Repeated stop/reclaim and
post-release polling/snapshots must stay safe. USB fixtures cover a retirement
request from startup and diagnostics, saved callbacks and unchanged buffers.
`test-usb-retire-block` combines actual USB retirement, block/cache and claim
sources: cached parent/alias reads refuse, healthy cached reads survive, the
claim releases once and global flush continues to fail. Hardware and worker
scheduling remain modeled at those host boundaries.

The QMP smoke's `--reclaim --usb-storage` mode attaches a disposable USB mass
storage copy alongside keyboard/tablet, stops, retires/reclaims, repeats reclaim,
then removes/replaces USB input and runs sysabi over serial. The storage disk is
read-only in this guest; writable-failure visibility is covered by the combined
host fixture. This does not qualify physical removal, SMP reclamation, managed
workspace salvage/unmount or global reboot after uncertain writable-media loss.
Automatic reclamation on a transport fault is not enabled; the separate managed
USB fault/salvage proof remains under filesystem ownership.

Normal/Rosetta, strict GCC14/Linux and ASan/UBSan USB, combined USB/block and
xHCI fixtures pass; related block/AHCI, network registry and VirtIO net/PCI hosts
also pass. Kernel compilation passes. The final Q35 guest initializes a USB
mass-storage disk plus keyboard/tablet, acknowledges worker retirement and frees
23 DMA frames; repeated reclaim leaves that count unchanged with poll/probe
owners zero. QMP input removal/replacement and sysabi pass after reclamation.
Separate E1000 netabi and full threadstress guests pass. Runtime files are retained
PR313 artifacts, not a complete current-userspace build. All task guests stopped;
review VMs stayed untouched. Evidence: `build/review/xhci-reclaim/`. Nine known
missing nested-submodule license targets remain in documentation checking.

## Per-slot retirement and reset/re-enumeration boundaries

The startup audit confirmed that a rejected Run write could be accepted when
USBSTS.HCH was already clear. `start_controller` now requires valid command and
status readbacks, Run set, reset/interrupt-enable clear and Halted/Not Ready clear.
The status wait helper rejects inaccessible all-ones MMIO. A negative control
restoring the old startup helper fails the rejected-Run assertion. Boot reset
refuses existing controller rings/DCBAA or terminal retirement state; startup
requires complete runtime metadata and refuses terminal reclamation. These guards
preserve the one-attempt initialization contract; they do not implement restart.

`xhci_retire_slot` and monitor `xhcislot <1..255>` explicitly close admission on
a slot and all descendants. Kernel address-space/controller ownership is required;
synchronous, poll/probe, USB initializer/request/diagnostic and registered network
callbacks must drain before Disable Slot. Each call submits at most one command,
children before parents, requiring both exact command pointer and matching returned
slot. False may mean bounded progress or pending/unproven ownership; retry explicitly.
Timeouts retain command quarantine. True retains all DMA and stable offline block,
claim, network and diagnostic records; only terminal halt/reclaim may free backing.
Disabled records cannot receive new transfer events. Other slots remain available.
No automatic retirement on ordinary BOT failure is introduced.

Enable Slot now refuses a duplicate live slot before writing DCBAA; this is a
malformed-controller-response containment check, not a QEMU-observed duplicate.
Continuing the hub audit found a separate route-depth alias: a sixth downstream
route nibble silently returned its parent's route. Exhausted/invalid routes now
refuse before child reset/allocation rather than addressing an ambiguous path.

Host coverage exercises a three-level subtree, child-first progress of one command
per call, other-slot preservation, owner/provider refusal, command timeout, wrong
returned slot, duplicate live-slot refusal, idempotence and zero per-slot frees.
It also checks startup acknowledgements, inaccessible/CNR refusal, reset/restart
refusal over retained backing and five valid hub-route nibbles followed by refusal.
Runtime re-enumeration remains unavailable: root-port guards and permanent records
prevent overwriting old identities; one-attempt init cannot revive reclaimed rings.
Future restart requires a new controller generation and fresh worker/rings plus a
policy for immutable block/network tombstones. Hub port-change processing, endpoint
reset/dequeue recovery and physical/SMP validation remain separate audit work.

Normal/Rosetta, strict GCC14 and ASan/UBSan fixtures pass, as do related USB,
combined USB/block and network-registry host tests and kernel compilation. Controls
restoring old startup and hub-route helpers fail their intended assertions. Final
Q35 storage+keyboard+tablet retirement succeeds twice per slot, with exactly three
new Disable Slot completions. Terminal halt/reclaim frees 23 frames and repeats
without further frees; post-reclaim QMP unplug/replug remains contained and sysabi
passes. Separate E1000 netabi and full threadstress guests pass. Runtime files are
retained PR313 artifacts, not a complete current-userspace build. All task guests
stopped; review VMs 61610, 6233 and 85869 remained untouched. Documentation checking
covers 888 local links with only the nine known missing nested license targets.

Evidence for this checkpoint is `build/review/xhci-slot/`. The current branch is
still based on main `1f177761`; main `a37eb7b2` was inspected for overlap, not merged.
PR338's managed USB fault/salvage proof remains independently owned; there are no
filesystem/process/generic block or installer changes here.

## Hub port changes and EP0 reset/dequeue recovery

Confirmed three further driver defects: downstream hub changes were invisible
while the root stayed connected; EP0 errors on Setup/Data TRBs were discarded
because only the Status pointer was accepted; and hub reset could succeed with
Enable set while Reset stayed asserted or without a fresh reset-change acknowledgement.

USB2 hubs now receive asynchronous four-byte port-status requests from the existing
worker, round-robin with at most one submission per turn. There is no synchronous
control wait in the runtime poll loop. Each hub owns its EP0 TD/buffer until its
completion; synchronous controls cannot overlap it. Connection/enable/reset,
suspend/overcurrent or any latched change quarantines the affected port's existing
child subtree, even if Connection is already high again. Sibling ports remain
available. A failed/short status request or two-second unanswered TD quarantines
the hub and descendants; unanswered TDs remain pending and pinned. Polling and
worker service latency bound observation, not physical plug detection itself;
this is containment, not zero-latency unplug or runtime re-enumeration. The worker
also runs when a hub has no initial HID/network child. Change bits deliberately
remain latched during runtime: old identities cannot be rebound or re-enumerated.
SuperSpeed hubs require a separate descriptor/status contract and are refused.

Control completion matching tracks all exact TRBs across ring wrap. Definite
Setup/Data errors retire the active TD immediately. A successful Data event does
not complete Status; Data short-packet events are requested and tracked so an
incomplete status buffer cannot publish stale bytes. Hub boot reset clears old
changes before reset, requires Connection+Enable, Reset clear and fresh reset
change, then requires successful change acknowledgements before publishing a child.

`xhci_recover_control` / `xhciep0 <slot>` permits one explicit recovery for an
acknowledged EP0 protocol STALL on a root-attached device, with no active owners,
command ambiguity, removed identity or published USB storage. Require Halted state,
Reset Endpoint completion and Stopped state, then Set TR Dequeue completion and
matching fresh ring pointer/cycle. Only after all proofs publish the new EP0 ring.
The old ring remains tracked until terminal reclaim; a failed command retains both
rings and the closed endpoint. Timeout, wrong state/slot/dequeue, repeated recovery,
USB storage or hub-routed/TT recovery refuse. No failed request is replayed and
no bulk/storage failure is revived. Other endpoint reset, device-side clear-halt,
TT cleanup and generic dequeue/cancellation recovery remain unavailable. The
[controller specification](https://cdrdv2-public.intel.com/625472/625472_xHCI_Rev1_2b.pdf)
sections 4.6.8 and 4.8.3 describe the Halted/Stopped/dequeue and control-STALL contract.

The explicit `xhciep0check <slot>` HID diagnostic requests an invalid descriptor,
recovers the acknowledged STALL, then verifies a new device-descriptor request.
It is an attended regression diagnostic, not automatic recovery or ordinary input
startup. Host tests cover every control stage and wrap, short reads, subtree and
sibling behavior, stalled reset, failed change acknowledgement, owners/storage/TT
refusal, command timeout/error/wrong-slot/dequeue and retained-ring reclaim accounting.
Normal/Rosetta, GCC14 and ASan/UBSan xHCI fixtures, related USB/block/registry
hosts and kernel compilation pass. Negative controls restoring Status-only EP0
matching, removing hub containment or restoring old hub reset fail their intended
assertions. Final Q35 hub+keyboard+tablet proof passes downstream remove/replacement
with hub/tablet survival, real protocol STALL followed by two recovery commands
and a valid descriptor, child-first parent retirement, terminal reclaim of 21
frames (including both EP0 rings), idempotence and sysabi. Separate storage+HID
retire/reclaim/replug, E1000 netabi and full threadstress guests pass. Retained PR313
runtime files were used; no complete current-userspace, physical/SMP, TT or USB3
acceptance is claimed. Task guests stopped and review VMs 61610/6233/85869 stayed
untouched. Docs check: 890 local links, nine known nested-license targets missing.
Continuing topology review identifies nested USB2 translator ownership/port
inheritance as the next source/specification checkpoint before any TT recovery.

Evidence: `build/review/xhci-hub/`. Main `819956be` inspected, not merged into the
branch's existing `1f177761` integration base. PR334 signal and the filesystem
late-flush proof remain separately owned; no process/VMM/filesystem/block edits.

## Nested USB2 translator ownership and shared endpoint recovery

Three confirmed context defects followed the hub audit. Low/full-speed descendants
used the immediate parent's slot/port as translator, including entirely full-speed
chains with no translator. High-speed hubs did not publish their descriptor's TT
think time. Hub/port metadata was sent via Evaluate Context rather than Configure
Endpoint. `device_tt_context` now validates the whole retained ancestry (live
configured hubs, valid ports/speeds/root consistency and no cycles) and selects
the first high-speed boundary plus its downstream port across intermediate
full-speed hubs. No high-speed boundary means zero TT fields. Invalid topology
refuses before Enable Slot or context mutation. High-speed functions also use zero
TT fields. Hub metadata uses Configure Endpoint with descriptor think time only
on high-speed hubs; default alternate-zero single-TT mode stays selected, MTT zero.
No multi-TT interface or USB3 hub policy is enabled. These fields follow sections
4.5.2 and 6.2.2 of the
[controller specification](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/extensible-host-controler-interface-usb-xhci.pdf).

EP0 and HID interrupt recovery now share one fresh-ring transaction. An acknowledged
HID STALL must have the exact terminal event, no pending TD/owners and Halted
context; Reset Endpoint must acknowledge Stopped, then EP0 sends device-side
CLEAR_FEATURE(ENDPOINT_HALT), then Set TR Dequeue acknowledges the new pointer/cycle.
Only then publish a fresh poll. A failed clear-halt cannot reach Set Dequeue.
Old/staged rings stay tracked until terminal reclaim, one attempt per endpoint;
wrong state/slot/dequeue, timeout, removed identity, published storage, network and
hub-routed/TT recovery refuse. `xhcireset <slot> <dci>` admits only EP0 or this HID
policy; generic bulk/network/isochronous recovery and CLEAR_TT_BUFFER remain closed.
An explicit, command-acknowledged Stop Endpoint may instead prove a root HID poll
Stopped. That path rebases to a fresh dequeue without reset or device clear-halt,
preserving both data toggles. A bare Stopped event or software flag supplied by a
caller does not authorize it; only the owned stop diagnostic records this proof.
No application transfer is replayed and no failed storage identity is revived.

`xhciintrcheck <slot>` explicitly stops a root HID input poll, verifies command/slot
and Stopped context, then requeues through the shared fresh-dequeue path. The initial
QEMU probe rejected SET_FEATURE(ENDPOINT_HALT) and stalled EP0 instead; its evidence
is a failed probe, not successful HID-STALL coverage. Host tests cover the actual
STALL/reset/device-clear/dequeue order and all stage failures. The Q35 test uses
Stop Endpoint and requires new tablet input after recovery. Both proofs are needed
and neither claims native HID/TT acceptance.

Host fixtures cover 32/64-byte contexts, nested high-speed/full-speed boundaries,
no fictitious full-speed translator, malformed/retired/cyclic parents, unchanged
failed context output and no Enable Slot submission, hub command type and all four
think times. Recovery fixtures cover STALL and owned Stop paths, toggle-preserving
ordering, failed clear-halt/commands, unchanged old ring, late old completions,
storage/TT refusal, and terminal poison-accounted reclaim of both old/staged rings.
The VM harness adds `--nested --interrupt-recovery` for two full-speed hubs plus
root tablet, EP0 recovery, explicit interrupt stop/rebase, new input, removal and
child-first retirement/reclaim/sysabi. Evidence: `build/review/xhci-tt/`.
Final normal/Rosetta, strict GCC14 and ASan/UBSan xHCI fixtures, related USB,
combined USB/block and network-registry hosts, and kernel compilation pass.
Controls restoring immediate-parent TT selection, wrong hub command, omitted
think time or omitted device clear-halt fail their intended assertions. Final
Q35 nested-hub/EP0/interrupt-stop/dequeue/new-input/removal proof passes, including
healthy/repeated/invalid monitor refusal, child-first retirement, 26-frame terminal
reclaim and sysabi. Separate storage+HID retire/reclaim/replug, E1000 netabi and
full threadstress guests pass. Retained PR313 runtime files used; no complete
current-userspace or native/physical/SMP/HS-TT qualification. All task guests
stopped, review VMs 61610/6233/85869 preserved. Docs check: 891 local links, nine
known missing nested-license targets. Next audit boundary is shared translator
buffer cleanup/error containment and class-specific dequeue cancellation; root
HID admission does not establish those contracts.

Main `7ef140ef` inspected without merging into the branch's `1f177761` base; PR334
signal and PR345 managed late-flush work remain separately owned, with no process,
VMM, generic block or filesystem edits here.

## Shared single-TT cleanup and class cancellation

Confirmed cancellation defects: root/slot removal marked bulk requests failed but
neither woke a yielded waiter nor made its readiness predicate observe cancellation.
XHCI mice also injected into the legacy PS/2 source and did not cancel their own
held buttons on removal/endpoint failure. Removal now publishes failure, wakes the
bulk queue and makes the predicate observe removal/failure/terminal stop. The
transfer exits without claiming completion, copying output, recycling DMA or
retrying. USB mouse input uses one stable source per software record, disjoint
from PS/2, VirtIO and trackpad sources. Removal, endpoint failure and owned stop
issue source cancellation, clear the report/absolute baseline and preserve other
physical buttons. Existing MOUSE_CANCEL semantics discard ambiguous queued input
without generating a click-producing release; the event ABI is unchanged.

Explicit routed EP0/HID recovery now validates ancestry and hardware TT context.
For a single-TT control endpoint, require the actual output-context USB address
(nonzero 7-bit, Addressed/Configured), owner hub Configured/Hub/single-TT state,
matching owner/downstream port, and an idle healthy owner EP0. Lease that EP0 against
hub status work across Reset Endpoint, two targeted CLEAR_TT_BUFFER requests (OUT
and IN, endpoint zero/type control, actual USB address, single-TT index one), then
Set Dequeue. Revalidate identities before/after each cleanup. Never substitute
slot ID for USB address or physical downstream port for single-TT request index.
This follows the targeted control/bulk-buffer model described by the
[USB core implementation](https://github.com/torvalds/linux/blob/master/drivers/usb/core/hub.c)
and [USB host API](https://www.kernel.org/doc/html/latest/driver-api/usb/usb.html).
Periodic HID recovery must not issue this nonperiodic buffer request; it retains
its device clear-halt or owned Stop/toggle-preserving policy.

Cleanup/identity failure leaves both rings tracked and closes the translator's
slow-speed dependent branches. If hub EP0 itself failed, close the whole hub
subtree. Unrelated HS siblings keep their admission metadata when the owner stays
trustworthy; an ambiguous global command path remains closed independently.
TT-backed bulk/network transport failure or removal with unproven nonperiodic
ownership likewise quarantines dependents rather than resetting the shared TT,
replaying a request or reviving a provider. No automatic cleanup, RESET_TT,
multi-TT interface, published-storage recovery or bulk/network retry is enabled.
Manual cleanup is modeled host coverage, not physical HS-TT acceptance.

Tests cover a shared HS owner above an FS hub, slot/address mismatch, both control
directions, index one, lease contention/poll exclusion, invalid context/address/MTT,
first/second request failures, address change and command timeout, periodic
exclusion and DMA retention. Bulk tests force cancellation inside the actual wait
seam and require readiness plus wake; driver/pointer fixtures cover USB source
identity, held-button cancellation and preserved PS/2/other-USB state. The Q35
fixture adds `--routed-recovery` to existing nested removal/EP0/interrupt/new-input
proof. Normal/Rosetta, GCC14 and ASan/UBSan xHCI/mouse fixtures, existing pointer/
mouse ABI/VirtIO input and related USB/block/registry hosts, plus kernel compilation
pass. Five controls removing a TT direction, using slot instead of USB address,
ignoring the owner lease, omitting the bulk wake or omitting mouse cancellation
fail their intended assertions. Final Q35 routed/nested EP0 and interrupt/new-input
proof passes with child-first retirement, 27-frame reclaim and sysabi. Separate
storage+HID retire/reclaim/replug, E1000 netabi and full threadstress guests pass.
All final guests use a private read-only fixture disk clone with recorded hash;
retained runtime fixtures are not a full current-userspace build. Task guests
stopped, review VMs 61610/6233/85869 preserved. Docs: 893 local links, nine known
missing nested-license targets. Evidence: `build/review/xhci-shared-tt/`.

Main `d791b949` inspected at start and `83e5d1ad` at publication, with no upstream
diff in the driver/mouse sources from the existing `1f177761` base; neither newer
main was merged. No process/VMM/filesystem/generic block edits overlap PR334 or
managed late-flush work. Continuing the audit leaves generic class recovery,
physical TT failure scheduling, multi-TT/USB3 and restart generations open.

## Class-report recovery and restart refusal checkpoint

Three confirmed HID defects are fixed. Mouse parsing used endpoint packet capacity
instead of actual transferred bytes, so short absolute reports could manufacture
coordinates from the zeroed DMA tail. Short keyboard reports skipped input release
and retained held-key/repeat state. Relative mouse reports were deduplicated like
absolute state, dropping repeated equal movement deltas.

Transfer dispatch now checks the accepted class minimum (keyboard 8, relative 3,
absolute 5). Incomplete successful transfers increment class errors, cancel only
that device's input/repeat and absolute baseline, then rearm the healthy endpoint.
Mouse parsing receives actual length. Relative deltas are delivered on every
accepted report; absolute state retains deduplication. This changes no event ABI,
endpoint recovery command, provider identity or DMA reclamation rule.

Actual-driver host fixtures deliver every length below each minimum, require
keyboard release and repeat cancellation, no fabricated mouse event, local pointer
cancellation, a fresh poll, repeated relative movement and valid-report recovery.
Published storage, registered network, ambiguous bulk and unsupported trackpad
recovery must refuse without commands, allocation or state changes. Inspection of
BOT/network/trackpad failure paths found no further confirmed restart defect in
this bounded pass: ambiguous transport ownership stays quarantined, and normal
ECM link changes do not reset/replay a failed endpoint.

Restart fixtures preserve the whole controller model, DMA/free accounting,
provider count, worker/probe flags and discovery/MMIO counters at failed handoff,
partial initialization, outstanding worker retirement, each reclaim batch and
completed reclamation. Public initialization remains one-attempt, and terminal
private start/reset refuses. These tests qualify refusal and retained identity;
new-generation controller/provider restart remains unimplemented.

Evidence: `build/review/xhci-class-restart/`. Initial fixtures reproduce short-HID
and relative-dedup failures. Three private negative controls remove keyboard
cancellation, bypass mouse short-report cancellation or restore relative dedup;
each fails its intended regression assertion.


Final normal/Rosetta, strict GCC14 and ASan/UBSan xHCI fixtures pass, with related
pointer/mouse ABI/VirtIO input, USB storage/block-retirement and network registry
hosts; kernel compilation passes. Rebuilt-kernel Q35 nested/routed EP0 and input
recovery, new input, child-first retirement and 27-frame reclaim pass. Separate
storage+HID retirement/reclaim/replug, E1000 netabi and full threadstress guests
pass. Guests use a private read-only source disk clone with recorded SHA256;
retained runtime fixtures are not a complete current-userspace rebuild. Malformed
HID reports are injected deterministically in the actual-driver host fixture,
not by physical hardware or the QEMU device. Task guests stopped; review VMs
61610/6233/85869 preserved. Documentation check: 894 local links, nine known
missing nested-license targets; no new missing link. Main `5ed7f015` inspected at
publication (new less integration; xHCI/fixture unchanged), not merged into the
existing `1f177761` integration base. Evidence: `build/review/xhci-class-restart/`.

Continued isolation review independently checked PR334's unwired kernel-mapping
preparation helper at `439d274d`: source plus strict Rosetta and ASan/UBSan fixtures
pass. Its active-root path is a byte-identical no-store check; inactive publication
follows complete registry validation. This accepts no live lifecycle wiring or
signal delivery. Foreground worker spawn was separately confirmed to retain the
parent worker association while switching the process binding; PR334 owns its
bounded refusal fix and guest reproduction. No process/VMM edits overlap PR317.

## November 4: coordinated worker-spawn review

Independently reviewed PR334's `bfee424f` production change and `d3986f00`
regressions. Synchronous worker spawn now refuses before allocation/actions/binding
across all three implementations and their alias. Detached spawn remains admitted;
its shared preparation-error cleanup no longer replaces the worker binding with
the parent's main-thread FPU/stack selection. The matched guest source/artifact
hashes and passing UART marker were verified. Actual-source foreground and real
ELF-load/vector-rejection fixtures independently pass strict Rosetta and
ASan/UBSan; the latter requires complete staged child cleanup and unchanged parent
and modeled worker binding. Independent logs: `build/review/pr334-worker-review/`.
PR334 retains production edits, full-thread evidence and its own documentation.

The next checked-root design requires saved IRQ exclusion over registration,
preflight and failure unregistration/success publication, with output zero until
success and exact caller IRQ restoration. Allocation/zeroing precedes exclusion;
failed roots unregister before free. Only this bounded creation slice proceeds
with its owner. Generation/association and trusted setter wiring remain pending
exact tuple/caller/error review. A bounded invariant diagnostic/halt is acceptable
for an impossible trusted commit violation, not ordinary constructor refusal;
no FPU proof minting or signal admission is approved.

## November 4: checked-root qualification

PR334's `d7466e1c` replaces root creation's unchecked mapping replay with saved
IRQ exclusion spanning registration, checked preparation and output publication
or rollback unregistration. Allocation/zeroing precedes exclusion; failure keeps
output zero and unregisters before freeing only the new root. Legacy setter and
process refresh paths remain unchanged in this slice. Independent actual-source
preparation/creation and existing range/teardown fixtures pass strict Rosetta and
ASan/UBSan; isolated logs: `build/review/pr334-root-review/`.

Verified the owner's combined `vmm-create-receipt.json` SHA256
`2c62e69c812f1dd671b912dec388474de28186bb15dbaaf03452474609693791`, its
17 source/artifact hashes and nested guest hashes. Matched kernel worker binding/
detached/TLS/FPU/stack probe, full threadstress and sysabi fault/reaping guests
all report status zero and stopped. Retained userspace fixtures are not a complete
current-userspace rebuild. This qualifies checked root creation; it does not
qualify the next metadata implementation.

Reviewed the `a0a7f474` design's exact process membership/image/root, main/worker
FPU/TLS and owned stack tuple predicates, including valid killed-parent restoration
and main atexit continuation. The next separately owned metadata-only slice must
validate before any association/accounting/hardware mutation and preserve failed
admission state. Fixed bounded serial diagnostic plus IF-disabled halt is accepted
for violated trusted commit invariants; recoverable staging errors still return
before publication. Its implementation remains subject to source/host/VM review.
No FPU proof object, trap capture, assembly change or signal admission is approved.

## November 4: image metadata source and host review

PR334 candidate `fea6bbb1` implements nonreused committed image generations,
process-owned membership/tuple resolution, staged preflight before publication or
exec stdio replacement, and IRQ-excluded scheduler bind/clear validation. It
invalidates opaque association metadata in stable scheduler kernel-FPU records
before binding changes, kill and stack reuse; user-FPU records keep NULL. Failed
staging preserves the caller's binding; exec invalidates/unbinds and retires the
old generation before backing cleanup, then publishes the new image. Main
pthread_exit retains the shared image for atexit, and worker clear precedes TLS
free. Normal scheduler switching retains each slot's association. No proof is
minted and no trap/assembly or signal-delivery path is enabled.

Independent production-source review found no new blocking issue in this bounded
candidate. All 20 strict Rosetta/ASan+UBSan host profiles independently pass,
including actual process metadata/lifecycle, scheduler/FPU metadata and existing
service/foreground/vector-failure/loader/lifetime/output/teardown fixtures.
Verified the owner's metadata host receipt and all 32 recorded hashes. Independent
commands/results/logs/source hashes: `build/review/pr334-metadata-review/`.
Hardware and PMM hooks are explicit models; numeric worker stack checks use a
logical HHDM test view. Source/host acceptance does not qualify target integration:
matched build/guest evidence and current docs remain pending with PR334's owner.
FPU proof capture, user copy/assembly and signals remain separate review gates.

## November 4: metadata matched-guest qualification

Closed `fea6bbb1` metadata qualification after independently verifying PR334's
`metadata-vm-receipt.json` SHA256
`066b793adf7a62522b82498d691ab0a51632f945a8ef359e5632657bc82d0451`,
20 combined and 13 nested source/kernel/ISO/stage hashes, plus matching logs and
zero-returncode harness results. Worker binding/detached/TLS/XMM15/stack probe,
full threadstress (fork/exec/main-exit/atexit/lifecycle) and sysabi fault reaping
pass. Final OVMF AHCI/USB contention covers typing, cancellation, copy comparison,
fsck, three relaunches and intentional timeout/quarantine. All task guests stopped;
review VMs 61610/6233/85869 confirmed live.

Two packaging attempts failed: a BIOS-only ISO could not boot OVMF, and a minimal
initramfs omitted `/sh`. Their evidence is retained. The final passing run uses
all 17 packed retained matched apps, checked byte-for-byte, and the same candidate
kernel. These fixture results are not a complete userspace rebuild or physical/
SMP acceptance. PR334 publishes current qualification docs at `7ae7bceb`.
The next entered-user FPU proof/query proposal remains design-only and requires
separate exact frame/lifetime/save/invalidation/failure review before implementation.
No saved-proof minting, trap assembly, user copy, context replacement or signal
admission follows from metadata qualification.

## November 4: entered-user FPU witness qualification

Independently qualified PR334 candidate `2a3b1a89` after source, host, target
code-generation and matched-guest review. Only common entry reserves the private
64-byte witness alongside the existing trap area; genuine loaded-user FXSAVE
precedes the ordinary entry query and witness publication. Derived association
comparison precedes witness dereference. Internal snapshot/consume require fresh
strict return ownership/preflight, disjoint resident caller-owned kernel output
and read-only CR4/EFER completeness checks. No replacement operation or production
snapshot/consume caller exists. Direct IRQ paths remain unchanged.

Independent actual-FXSAVE and scheduler strict Rosetta/ASan+UBSan profiles pass
4/4. Target disassembly confirms save-before-query, complete witness before
association publication, no common-exit calls or compiler SIMD operands, and the
direct IRQ macro is unchanged. Critical host save/restore functions exclude
sanitizer prologues; operations remain instrumented, guarded extents are tested,
privileged policy/address reads are modeled, and host patterns cover XMM15.
Evidence: `build/review/pr334-fpu-proof-review/`.

Verified the owner's final guest receipt SHA256
`202b4ee99bbbd1cd638df96143002f16608c2d1d931e608a3581a3f23da8c6f1`,
23 combined and four nested hashes, status-zero main/worker guest covering all
16 XMM registers, nonempty x87/CW/MXCSR, selected GPR/FS/TLS/RSP, and full thread,
sysabi fault/reaping and OVMF storage contention/three-relaunch/quarantine results.
QMP records CR4 `0x620`, EFER `0xd00`. Timer/preemption progress is sampled across
the test window, not attributed to an exact assembly interval. All 17 retained
packed apps were byte-verified; this is not a full userspace rebuild. Task guests
stopped; review VMs 61610/6233/85869 live. PR334's current qualification docs are
at `b643077f`. Physical/SMP/NMI and unsupported inherited CPU policies are not
accepted. Replacement image validation/alias/consume policy is the next separate
design review; frame-copy/action/delivery ABI remains later. No signals enabled.

## November 4: private FPU restore and continued runtime review

Qualified PR334 `c27d018f` private restore after independent source/host/codegen
review and matched regression receipt verification. Initialization invalidates
old availability/mask state, explicitly zeroes the capture area, then samples
actual FXSAVE MXCSR support with the defined zero-mask fallback. The helper checks
current witness/ownership/policy, full private source extent and selected FPU-object
nonoverlap before source reads; rejects unsupported/high MXCSR bits; copies only
the saved 512-byte prefix and consumes once. Original unmodified kernel capture
provenance remains a caller precondition, not authenticity from MXCSR checks.
There is no production caller, editable context or signal integration.

Independent init-mask and real-FXSAVE restore strict Rosetta/ASan+UBSan profiles
pass 4/4; target codegen confirms zero/capture/mask-publication order, validation
before reads/copy, scalar copy before consumption and no helper FXRSTOR or call
after copying begins. Evidence: `build/review/pr334-fpu-restore-review/`.
Verified final receipt SHA256
`377c5a8062bf44a3579dc0d9b8126c74586a47900142e9fffe584584564ca224`,
24 combined and four nested hashes, main/worker register status-zero, full thread,
sysabi and OVMF contention regressions. CR4 `0x620`/EFER `0xd00` and retained 17-app
payload attribution remain recorded. The private helper's actual restore is
host-tested; guests exercise initialization/common preservation, not an unwired
private call. Task guests stopped, review VMs preserved.

The next bounded frame-copy packet remains design-only until exact paging/access
policy, admitted encodings and non-DMA source/destination ownership are reviewed.
A separate confirmed thread-ID defect can publish a worker while returning a
negative signed ID after INT64_MAX; PR334 owns reproduction and the nonwrapping
positive-ID admission fix, with no signal implementation implied.

Read-only main `f7ec75e7` review for the separately human-authorized session-switch
milestone confirms kernel lifetime membership survives descendants/orphaning and
retains loading/I/O/teardown ownership, but there is no userspace empty-scope API.
Existing drain forces cancellation and must not back graceful logout silently.
A nongraphical supervisor/one scoped GUI root capability proposal is under
integration/profiles review; no new kernel interface implementation is approved.
It must preserve live-parent wait status, error attribution and legacy kernel
lifetime diagnostics, with revocation plus true zero before profile replacement.

## November 4: signed thread-ID qualification and session-scope review

Qualified PR334 `783c4eb8` after independently reproducing the pre-fix defect on
both ordinary and private routes: INT64_MIN returned while one active worker was
published. The fixed IRQ-excluded range gate admits only IDs 2 through INT64_MAX
before slot/TLS/allocation mutation, issues MAX once and leaves a nonwrapping
exhausted state. Later creation returns AGAIN without publishing a worker.
Actual-source boundary and existing service fixtures independently pass strict
Rosetta/ASan+UBSan 4/4, including both inherited IRQ states and nested restoration.
Independent logs: `build/review/pr334-thread-id-review/`.

Verified normal-runtime receipt SHA256
`966e54e35c01bc63422d5c54a60ce13510d83171aab5c1e14cc7795354d28df1`,
14 combined/nested artifact references, main/worker register/TLS status zero,
full threadstress and sysabi fault-reaping zero results. Boundary counter values
are actual-source host tests, not seeded guest cases. The next owned frame-copy
policy is accepted for an unwired helper/host-test slice only; test-kernel hook,
whole publication/FPU commit and signal integration remain separate reviews.

For the session-switch dependency, parent/profiles agreed a nongraphical
supervisor outside one scoped GUI root, owner-bound noninherited handle, unchanged
exec ABI, nonkilling revoke/reap and authoritative revoked+zero completion.
SYS_SESSION_SCOPE 163 is reserved; PR334 reports no collision. Implementation is
isolated in `SrvrOS-migration-review`; this audit owns independent review. Exact
metadata-only exec/final-exit/release detach seams were also reviewed by PR334.
Reconciliation must preserve its saved-IRQ clear/gen0/cleanup ordering.

First draft review found an owner-death reservation leak, replacement of the
ordinary kernel exec entry and insufficient reaping readiness checks; the owner
has addressed these in the uncommitted draft and expanded tests. New reaping
returns bounded progress, not a possibly negative signed PID. Actual owner-exit/
exec hooks, converted/unpublished loading exclusion, syscall failure paths and
matched runtime behavior remain under review; this is not interface acceptance
or a session-switch completion claim. No silent kill/timeout success is allowed.

## November 4: bounded session-scope runtime qualification

Independently accepted PR354 head `6f272777` (production `cd93e1eb`, comment/test
closure `e3eeaf21`) against `f7ec75e7`. Owner-bound noninherited full-width handles
control one scoped GUI root and inherited descendants. Nonkilling revocation
closes new reservations; admitted reservations remain counted through conversion,
and owner detachment cancels an unconverted reservation atomically. Complete
retirement and no live-parent wait right precede orphan reaping. Reaping reports
bounded progress, preserves sticky exit errors and permits release only after
revocation plus zero members. Owner exec/final-exit/release hooks preserve failed
exec and main atexit ownership; legacy kernel/battery scope admission refuses.

Initial six independent service/lifetime/syscall-validation profiles, four changed
service/loader profiles and both final closure profiles pass strict Rosetta and
ASan/UBSan within their recorded source versions. Actual exit/atexit/exec prefixes
use host terminal traps before privileged instructions; syscall backend and
hardware are explicit models. Source/IRQ/scheduler admission review and the
converted-unpublished checkpoint cover loading exclusion. Independent evidence:
`build/review/session-lifetime-{host,stable,final}-review/`.

Verified original and closure source manifests and clean matched `cd93e1eb`
kernel/initramfs hashes. Read the final harness and serial: twice scopeprobe
holds a descendant after root/owner exit, refuses early release/new scope, then
succeeds after natural retirement; init startup is complete and PID1 `/init`
remains live before/after. Legacy lifetime, PTY and sysabi/fault-reaping checks
pass. The first harness invoked sysabi at the monitor and failed; the corrected
passing run explicitly runs `/fat/bin/sysabi`. This qualification requires the
normal live init reaper and proves no new service or hidden kill. Task guest
stopped, review VMs preserved. Integration owner retains merge responsibility.

This is the bounded runtime dependency, not acceptance of graphical profile
switching, unsaved-work decisions, settings isolation, physical/SMP behavior or
combined PR334 image metadata. Reconciliation must retain PR334's saved-IRQ
clear/gen0/cleanup ordering and separately reviewed signal boundaries. The
profiles/integration owners continue the actual user workflow and matched proof.

## November 4: unwired owned frame-copy review

Independently reviewed PR334 source `641e6694` and maximum-table/test-hook closure
`3381ea71`. The private helper admits only a current owned kernel-stack source
and one or two ordinary writable user pages after strict current-root, hardware
policy, mapping identity and physical-alias preflight. Every refusal precedes
stores; the final bounded byte copy has no callback or fallible operation. The
proof validator is read-only. There is no production caller, signal delivery or
copy-to-FPU-consume transaction approved by this checkpoint.

Ten independent strict Rosetta/ASan/UBSan host profiles cover the VMM helper,
process wrapper, scheduler ownership, FPU proof and image metadata at the recorded
source version. Both additional maximum-table wrapper profiles pass at closure:
65,536 mappings, successful two-page copy, duplicate destination VA and a distant
physical alias. Privileged paging/CPU state is modeled; these are not guest tests.
Evidence is retained in `build/review/pr334-frame-copy-review/` and
`build/review/pr334-frame-copy-max-review/` with source hashes and results.

The wrapper can visit four full mapping tables: 262,144 records at the admitted
maximum while IRQs remain disabled. A 1 KiB copy bound is not a total work bound.
Target code-generation/build checks and an actual maximum-table helper-latency
measurement remain required before a production caller. The separate flag-only
INT3 test hook/build recipe has source review only: bounded serial diagnostics,
a complete-last scalar record and reference-timer measurement. Its `helper_us`
excludes source preparation and serial output, rather than measuring the entire
IRQ-disabled hook. No target build or guest result is claimed here. Normal and
test kernels must have distinct recorded artifacts. Integration released the
heavy-test window after its context VM passed; the PR334 owner has begun separate
normal/test builds, with guest evidence still pending. Existing review VMs remain
untouched.

The previously qualified session runtime PR354 merged as `f58e0725`; GitHub also
confirms graphical integration PR355 merged as `82face72`. The integration owner
reports Apps PR9 `c3ad11b1`, exact merged-tree/pin verification and three-VM
composite acceptance: unsaved-work decisions, direct app-crash cancellation/retry,
accepted I/O, natural orphan retirement, authoritative revoked-zero/release before
chooser, A–B–A settings/environment/cwd isolation and 640-pixel keyboard handling.
The original workflow failure is retained for a builtin-environment capture gap;
a source-identical external-environment supplement passes. Those workflow results
are owner-reported, not independently rerun by this audit. The separate PR334
frame-copy target/latency and combined image-metadata boundaries remain unchanged.

## November 4: effective-supervisor copy qualification

The first isolated guest never entered the hook: production IDT3 is DPL0. A
four-line test-flag-only DPL3 registration admits the private probe through the
existing stub; normal gate policy is unchanged. The second guest reached the
helper but refused before stores. Its Limine HHDM had U/S set in upper entries
and clear at the supervisor 2 MiB leaf. The original every-level-supervisor rule
was unnecessarily strict. The reviewed correction retains the accumulated U/S
permission and requires effective supervisor access for source translations;
destinations still require effective user access. Encoding, physical/cache/RW/NX,
owned-stack and full-FPU alias checks are unchanged. This follows Intel SDM3A
section 4.6's effective permission rule; no inherited page table is modified.

Independent strict Rosetta and ASan/UBSan fixtures both pass for the corrected
source. The permission matrix admits each sole supervisor level and rejects an
all-user path for 4 KiB/2 MiB/1 GiB stack translations and independent user/kernel
FPU spans. The owner retains an old-gate negative control failing the new admitted
fixture. Qualification is bound to source hashes in
`build/review/pr334-frame-copy-effective-review/` and final PR334 commit `6766c5ff`.
The frozen receipt outside cleanable build output at
`.local/pr334-evidence/6766c5ff/receipt.json` has SHA256
`709da289646e8c82c4c41198d15c234c8da4f442f037d00dcfc343a4d9021857`;
all 182 retained file hashes were independently verified. The owner also records
normal main/worker FPU, full threadstress and sysabi passes at this closure.

Verified the third isolated guest receipt's 19 artifact hashes and all 129 normal
kernel/object hashes unchanged by the test build. Actual target disassembly shows
scalar stores with no calls or failure path after the first user store. Main and
worker success/refusal, canaries and FS/TLS/RSP/R12 checks pass; genuine ordinary
mappings reach 65,536, the two-page copy succeeds and the guest exits zero/stops.
Both failed admissions remain evidence. No production helper caller is enabled.

The maximum-count sample reports 262,144 possible record visits and a 911 us
ACPI-PM helper interval on single-CPU QEMU qemu64/TCG with 1,536 MiB RAM. The first
small-count sample was 1,664 us. These are quantized observations from one guest,
excluding preparation and serial, not a hard bound or the whole IRQ-disabled
window. No numeric delivery budget was agreed. The required actual measurement
is complete; a future production caller still needs an end-to-end exclusion budget
and separately reviewed copy-to-infallible-state-commit transaction. No lookup
redesign, signal delivery or physical/SMP acceptance is implied.

## November 4: next internal action/state design boundary

PR334 may implement an initially unwired SIGUSR1 action descriptor and four
kernel-owned records per main/worker slot. This approves an implementation review,
not a delivery mechanism or public ABI. Process/worker membership must precede
access; image generation, nonreused public TID and observed scheduler ownership
bind records. Bounded capacity/LIFO/nonreuse refusal and complete staged descriptor
validation precede local nonfailing state commit. Existing legacy disposition
mirrors, pending discard and cooperative INFO exclusion must remain coherent.

Tests must cover whole-state rollback, reuse, worker/main retirement, ordinary
fork inheritance with empty records, failed exec unchanged, successful exec reset,
and active-record fork/exec refusal before side effects. Executable mapping checks
are not a lease for future delivery. Static storage cost, CPU0 saved-IRQ exclusion
and no asynchronous locks/allocations are explicit boundaries. No production
copy-to-consume commit, sigreturn, SA_SIGINFO admission or syscall is approved.
Session hooks in main `82face72` own separate fields: reconciliation must preserve
exec detach within saved exclusion before clear/gen0/cleanup, final exit detach
adjacent to trap invalidation, and release detach before backing retirement.
Main pthread_exit retains its session owner/image while retiring its signal records.

## November 4: internal action/state source and host review

The PR334 owner implemented that bounded, initially unwired slice. Independent
source review found no blocker in the private descriptor/exchange, four-record
identity/LIFO/range state, INFO cooperative exclusion and lifecycle changes.
All nine main/worker slots participate in active-record fork/exec refusal before
allocation or file effects. Legacy/default/spawn/fork/exec paths keep descriptor
and catch/ignore mirrors coherent. Actual process exit excludes IRQs before record
cleanup; successful exec and backing retirement clear records before invalidation.

Six independent strict Rosetta/ASan/UBSan module, actual process-wrapper and actual
image-lifecycle profiles pass. A first lifecycle link omitted the existing VFS
namespace dependency; corrected builds/executions pass and failed logs remain.
Evidence with unchanged source and test hashes:
`build/review/pr334-signal-state-review/results.json`. Privileged state and
scheduler/paging backends are explicit host models. The declared host layout adds
3,056 bytes per main/worker state and 1,763,328 bytes across 64 process slots;
target ABI size verification, no-SIMD/generated-code review and normal VM evidence
remain pending. Range checks establish shape/overlap only, not a user mapping
lease or proof of a future serialized frame's complete extent. No production
signal delivery, public ABI or fused state publication is accepted here.

## November 4: main-thread record retirement exclusion correction

Target review of the new PR334 internal state slice found that main
`process_thread_exit` could enter with IRQs enabled; trap invalidation saves and
restores that entry state, so the subsequent record clear lacked exclusion. The
owner added explicit saved-IRQ exclusion immediately before invalidation/clear;
the existing later enable for parking/atexit is retained. Both independently
rerun strict Rosetta and ASan/UBSan lifecycle profiles pass at corrected source.
Removing only that exclusion in a private negative control fails the new IF0-entry
assertion at trap invalidation. Evidence and source hashes:
`build/review/pr334-signal-state-irq-review/`. The earlier six profiles belong to
the pre-correction source; fresh corrected target/normal-guest closure is pending.

The owner reports target static-array growth of 1,764,352 bytes including
alignment, versus 1,763,328 bytes of declared state payload. Per-slot target size
grows 27,568 bytes to 1,701,280 bytes. These target figures await independent frozen
artifact verification and do not replace the earlier explicitly host-only result.

## November 4: internal state final target/guest qualification

Accepted bounded PR334 internal slice `729e4a84` after independently verifying
all 197 frozen files under `.local/pr334-evidence/729e4a84/`; receipt SHA256
`30cb85b3e55f4e5ef7fb8ae9e684fa5f6f76e3c84a7f31a1cb8a98d20f7b112f`.
Retained process/module/lifecycle sources match that commit. Actual module
object disassembly has no SIMD/x87 register use. Main exit orders CLI before trap
invalidation and record memset, with the later parking enable retained. Independent
ELF symbol comparison confirms 1,764,352 bytes of actual process-array growth
including padding. Evidence: `build/review/pr334-signal-state-final-review/`.

Matched retained corrected kernel digest to the normal guest's loaded-kernel
receipt. Its `kernel_source` names the pre-commit base `6766c5ff`; final source
hashes and loaded artifact identity bind the corrected candidate, rather than
claiming a clean `729e4a84` build. Main/worker FPU preservation exits zero, nested
full-thread and syscall-ABI harness results are zero, and owned guests stopped.
All 17 selected packed apps match the frozen earlier verified payload; the actual
payload-equivalence receipt SHA256 is
`902f6824c721485781016f24a158de4060cdf66819e22cebf078c8848bb1f8cb`.
This is retained-payload normal regression, not a full current-userspace rebuild.

Six independent pre-exclusion-correction host profiles and two corrected lifecycle
profiles plus the single-call-removal negative control retain their source
boundaries. The owner records 26 broader pre-correction and four affected final
host profiles. New internal operations are host-tested only; there is no private
operation guest caller, public signal ABI or asynchronous delivery. The next fused
copy-to-infallible-publication design remains a separate review.

## November 4: private fused-return pending-disposition finding

The next initially unwired PR334 private delivery/return candidate is under
review, with no public syscall, INFO registration or normal return hook. Its
candidate stages the complete 760-byte frame and kernel-private state before
copy, adds three bounded static-object physical exclusions and prepares a
same-continuation FPU commit. This is not yet qualification of that transaction.

Independent actual-source/model-backend reproduction confirms a return-path
mask-transition defect: raw restoration of the interrupted mask bypasses existing
handling of newly unblocked default pending signals. A USR2 masked by the handler
remains pending after return (`killed=0`, pending `0x1000`, restored mask `0x20`).
With RESET and both local/shared USR1 queued, delivery consumes local and resets
the action; return leaves shared `0x400` pending, and the next delivery reports
NONE under DEFAULT. That must follow the default disposition before user resume.
Default-ignored pending signals also need the existing unblock discard semantics.
Evidence with source hash: `build/review/pr334-signal-fused-review/return-mask-repro.json`
and adjacent source/log. Scheduler/FPU/VMM backends are explicit host models.

Final private-transaction acceptance is withheld pending staged restored-mask
pending/disposition effects, nonfailing commit, and an explicit outer termination
outcome before user resume. No fallible mask helper or admission query may be
inserted after commit. Required regressions cover main/worker, mixed pending
sources/RESET, default ignored/caught dispositions and unchanged-state refusal.
Target/post-copy code generation, nested/private guest execution and complete
maximum-mapping exclusion timings remain separate pending evidence.

## November 4: staged restored-mask correction

The PR334 owner corrected the private return candidate before final qualification.
It computes visible pending bits under the restored mask, stages default-ignore
discard, and selects the lowest fatal default bit with local-source priority.
All classification precedes FPU preparation; refusal leaves state unchanged.
Commit applies only prepared pending/mask/private-frame/FPU stores and any fatal
metadata. The internal result is explicit: zero refuses, one restores, and
128+signal requires the outer test caller to enter process exit directly before
user resume. It performs no post-commit admission query or further delivery with
the consumed proof.

Independent strict Rosetta and ASan/UBSan matrix profiles both pass main/worker
USR2 default-fatal handling, RESET mixed-source USR1, default-ignored CHLD/WINCH
discard, caught-signal retention and whole-state rollback on FPU preparation
refusal. Unchosen duplicate pending sources are retained. Evidence/source hashes:
`build/review/pr334-signal-fused-mask-fix-review/`. The earlier six prerequisite
profiles also pass independently for VMM exclusions, actual-FX prepare/commit and
the original copy wrapper; those do not qualify the complete fused operation.
Target suffix, expanded owner regressions and private/normal guest qualification
remain pending. No public delivery or ABI is admitted.

## November 4: first private fused target/guest checkpoint

Independently verified all 29 frozen first-run input hashes against the private
guest receipt retained outside cleanable build output at
`.local/pr334-evidence/private-guest-1/receipt.json`, SHA256
`a65166cb2728aca75dd5e2ade351f1380557644f52643f09b195d5d1e8faaabc`.
Main/worker/repeated register/FPU preservation with complete public snapshot
destruction passes, exits zero and stops. Pure user-loop delivery uses timer vector
32. At 65,536 genuine mappings the two-page 760-byte transaction reports 262,144
possible visits and 961 us for prepare/copy/commit. This quantized CPU0 TCG sample
excludes outer hook admission/diagnostics, not the whole IRQ-disabled interval or
a hard bound. No production hook or public signal capability is enabled.

Fresh disassembly of the frozen process/FPU objects and linked private kernel
confirms successful delivery/return suffixes contain bounded copies and prepared
state/frame/mask/RESET/default-fatal stores plus FPU commit, with no fallible query.
The FPU commit and linked memcpy are call-free; no FXRSTOR occurs while C continues.
Delivery reserves `0x1938` bytes of local stack plus register saves. Independent
evidence and available raw logs: `build/review/pr334-signal-fused-first-guest-review/`.
Expanded actual heap-pointer register preservation, DF and four-deep nesting,
fresh normal/I/O regressions and final immutable closure remain pending. This
checkpoint is neither final fused acceptance nor a garbage-collector/root proof.

## November 4: expanded private and normal/I/O evidence review

Verified the expanded private guest receipt SHA256
`1b5388ca31a4baadd46dbb3decca40ed0c469478bbb224a265dce757b67420ee`
and all 34 frozen inputs/logs at `.local/pr334-evidence/private-guest-2/`.
Main and worker allocated-pointer R15 preservation, handler DF clearing/original
DF restoration, four-level nesting with a deferred fifth pending delivery, and
public-snapshot destruction pass. Main/nesting/maximum markers are present,
status is zero and the task guest stopped. Full-mapping transaction evidence
again records `[65536, 262144, 961 us, vector 32]` under the same stated timing
limits. This proves pointer preservation, not sole-register rooting or collector
survival, and admits no atomic suspend or restart semantics.

Independently verified all 130 normal kernel/object hashes unchanged across the
isolated private build and the fresh normal loaded-kernel receipt hashes.
Normal main/worker FPU, full-thread and syscall-ABI checks pass. The OVMF storage
harness passes three relaunches, throttled writes, USB typing, Ctrl-C, copy
comparison, fsck and deliberate timeout/quarantine. The tail's single AHCI failure
belongs to that injected quarantine; it is not hidden as clean writable behavior.
The driver confirms original ISO/output restoration. Independent evidence:
`build/review/pr334-signal-fused-expanded-review/`.

Target stack-usage output records delivery 6,504 bytes, return 4,520 bytes,
copy preparation 264 bytes and ownership resolution 152 bytes. These individual
frames are not the cumulative path budget. Complete stack-chain audit and final
source/host immutable closure remain pending before final private-slice acceptance;
normal kernel delivery/public ABI/libc/WTF remain unwired.

## November 4: cumulative private-transaction stack audit

Independently parsed actual and stack-audit ELF sections: all 38 relevant function
bodies and relocation targets/addends match. Reconstructed the 37-node direct-call
graph, verified compiler static stack sizes, and recomputed every cumulative bound.
Actual disassembly has no indirect calls/jumps or recursive path; dispatcher stack
usage is 184 bytes. Adding a conservative eight bytes per call edge and the
983-byte common interrupt/frame/alignment/dispatcher prefix gives 12,207 bytes for
delivery and 10,991 bytes for return against the owned 131,072-byte stack. The
entered-user FPU save returns before the dispatcher rather than nesting on that
path. Evidence: `build/review/pr334-signal-fused-stack-review/receipt.json`.
This covers the fixed synchronous CPU0 IF0 transaction, not NMI/MCE or the entire
kernel's stack usage. Final source/host immutable closure still remains pending.

The next public contract may be proposed with INFO/optional RESETHAND and explicit
pre-effect rejection of INFO|RESTART, preserving legacy RESTART. This permits a
design packet, not public implementation/admission. An explicit SrvrOS WTF adapter
choice must audit nonrestart EINTR/partial-I/O/deadline effects; ordinary caller
flags must not be silently stripped. Source review confirms the kernel poll
pending branch currently returns zero and libc relies on cooperative dispatch to
turn it into EINTR, which needs an explicit contract once INFO is consumed before
libc resumes. Real machine contexts, ticketed nested atomic suspend and bounded
stop/resume remain mandatory separate reviews.

## November 4: bounded private-operation final acceptance

Closed the private test-only slice at PR334 `3afa5ee6`. Final receipt SHA256
`dea8c230b970db632109effa59f14fb2931133ae92e477cc697577e7f35afba8`
at `.local/pr334-evidence/3afa5ee6/receipt.json` and all 227 retained files verify;
18 retained source files match the final commit byte-for-byte. Both separately
frozen private-guest receipt digests verify. Precommit base/dirty-candidate labels
remain explicit; exact source/ELF hashes establish attribution rather than a
retrospective clean-build claim.

Independently refreshed final state/copy compatibility profiles pass 4/4 strict
Rosetta and ASan/UBSan. The earlier corrected-mask matrix source/header/IDT hashes
match final; prerequisite FPU/VMM hashes match final, with pre-mask-fix original
copy profiles retaining their recorded source scope. Independent suffix, cumulative
stack, expanded private guest and normal/I/O checks above carry forward through
verified final artifacts. Evidence: `build/review/pr334-signal-fused-final-review/`.
Documentation checking in the owner's checkout reports 917 local links with the
same eight missing nested-vendor license paths, not a wholly passing docs check.

This closes internal private delivery/return only. No normal kernel delivery hook,
public signal ABI, libc INFO registration, atomic nested suspend, restart epoch,
collector-root survival or JSC success is qualified. The pinned WTF code explicitly
requests RESTART|INFO and uses global semaphore waits; a future SrvrOS-specific
adapter and bounded ticket/lifetime protocol require separate review. The public
nonrestart subset remains design preparation only, with unchanged legacy behavior
and full action/mask/wait/return transaction requirements.

## November 4: public-v1 proposal review, implementation withheld

Reviewed the PR334 owner's public-v1 preparation packet. Proposed CONTROL/RETURN/
SUSPEND numbers 164–166 are not reserved: live main's syscall header blob
`ddb22cf84c92f7713ed1c159f284ebc021c51ca9` ends at session scope 163; allocation must
be rechecked/coordinated at implementation. Native 24-byte direct old-action
output, fixed versioned requests, honest opaque-CATCH query refusal, explicit
unsupported masked ppoll/pselect and INFO-without-RESTART remain proposals with
unchanged-output/pre-effect failure requirements. No public code is approved.

The proposed userspace INFO notification counter has a blocking design race:
legacy interruption permits retry, libc checks the counter, INFO runs after that
decision but before the next syscall, and the retry can block with INFO already
consumed. Neither a per-attempt resnapshot nor a call-wide baseline closes that
admission gap. Preserving mixed legacy RESTART and INFO nonrestart requires an
exact kernel-bound attempt/retry protocol or another proven atomic design before
implementation. This is a proposal counterexample, not a shipped-code regression.

Parked INFO-only waits also need exact action-change phases, wake and once-only
original-mask restoration; already delivered/INFO_DONE ownership cannot be reset
by a competing action update. Existing stable scheduler wake is a candidate
nonfailing primitive, with membership/lifetime, IF0 and capacity-scan costs audited.
All output/mask transitions must precompute default-pending/exit effects, as the
private-return fix does. Successful exec pending/default handling needs its own
actual-source reproduction. WTF's bounded ticket/phase protocol must prove the
last context/request access before matching RUNNING acknowledgement; skipping a
retired target remains unapproved without a mutator-deregistration proof. Real
native context layout, GOT-before-TLS, wrapper interruption/partial-I/O/deadline
semantics and atomic nested waits remain required. Private acceptance above is
unchanged while this public packet is revised.

## November 4: nonrestart attempt scope extends beyond legacy restart

A proposed sticky image profile would permanently exclude INFO after any legacy
RESTART registration and reject future RESTART after INFO enrollment. That would
honestly remove mixed registration but does not solve the whole admission gap.
Source inspection finds logical blocking operations that retry without RESTART:
event reads use internal 10 ms raw poll slices, epoll rebuilds after spurious or
retired snapshots, and synthetic poll repeatedly slices until its outer deadline.
An INFO handler can finish after a userspace retry decision and before the next
raw wait, leaving that logical operation blocked with its notification consumed.
An internal slice timeout is not completion of the enclosing logical operation.

The public packet therefore remains prepare-only pending a kernel-bound logical
attempt/retry protocol with atomic interrupted/stale admission before sleeping,
independent nested-handler ownership, lifecycle cleanup and explicit outer
progress/EOF/deadline rules. A userspace counter is not authority for that check.
The sticky profile and broad migration of all legacy actions are not approved as
a substitute. Existing legacy-only behavior and private-slice acceptance remain
unchanged; no public code is authorized by this design review.

## November 4: successful exec pending/default defect

Actual lifecycle reproduction confirms successful exec could reset caught
USR1/USR2 to DEFAULT yet reach the new image un-killed with those unblocked bits
pending. Caught CHLD/WINCH similarly became default ignored but remained pending.
This is an existing production-path issue, independent of public INFO admission.
Failed exec remains unchanged. The PR334 owner's separate bounded fix evaluates
pending/default handling after the new binding and exec diagnostic, with explicit
IRQ exclusion through the final decision and user entry. It skips evaluation if
already killed, preserving an earlier fatal cause, and enters existing process
exit with the selected status. `usermode_enter` builds user flags with IF set;
there is no intervening service/enable after the final decision.

Independent strict Rosetta and ASan/UBSan normal/fatal/ignored lifecycle runs pass
6/6. Current/shared ignored defaults clear, blocked and ignored dispositions stay
pending, and the lowest fatal bit consumes only its local-priority source. A
negative control removes just final evaluation/kill while retaining exclusion:
both fatal and ignored regressions fail their intended user-entry assertions.
Evidence/source hashes: `build/review/pr334-exec-pending-review/`. The terminal
host trap observes the actual process-exit gate under the new binding before
privileged/full teardown; no complete guest termination proof is inferred.
Earlier-fatal checkpoint, target codegen/build and finite raw-syscall guest
qualification remain pending. Explicit native exec signal-default attributes
retain their own contract/audit boundary; this does not infer every attribute
path from ordinary exec tests. Public-v1 design remains preparation only.

## November 4: exec and GOT/TLS dependency closure

Closed exec fix `1163ed75` after verifying all 65 frozen files and committed source
hashes; receipt SHA256 `d6f39053008cdbd8183988a860946657fe448c868944ecda0a3e9f1feda67dc6`
is retained at `.local/pr334-build-volume/evidence/exec-pending/receipt.json`.
Independent six lifecycle profiles plus two earlier-fatal profiles pass: injected
TERM at the final console checkpoint preserves status 143 over a lower pending
USR1. The identical raw native guest on old normal kernel reaches fatal sentinel
90 and fails ignored-state checking at 91; fixed kernel exits 138 before sentinel
and clears CHLD/WINCH while retaining blocked USR2, status zero. Both normal FPU
main/worker checks pass and guests stop. This now includes actual guest termination,
with no alternate IDT/INFO hook; explicit native exec attributes remain unqualified.

Closed exact GOT-before-TLS carry `8f446141` from browser `26e9e78f`, after live
main's pre-fix `app.ld` blob `212f56212292148f846b2277f4fd43a5cf62bddc` showed no
competing linker change. All 97 frozen files/committed source hashes verify;
receipt SHA256 `8ce57965d33a6c4ce81682793cda0ea1ed58becea5d250934ea2154f778dd7f4`
is at `.local/pr334-build-volume/evidence/got-tls/receipt.json`. Independent ELF
parsing verifies virtual/file-offset TLS coverage: old zero/initialized layouts
cover only 8/16 bytes; fixed layouts cover 2,148/2,164 bytes with the real 8-byte GOT
before TLS. Both current-normal main/worker TLS/GOT fixtures and FPU probe pass,
guests stopped. This is focused static layout/runtime evidence, not matched
SDK/ICU/WTF/JSC acceptance. Evidence: `build/review/pr334-exec-got-final-review/`.

## November 4: raw action-default fix and separate RESET peer finding

Independent actual-source reproduction confirms raw CATCH-to-DEFAULT strands
peer-local/shared USR1 and USR2 when the caller blocks the signal but a live peer
unmasks it. The PR334 owner's bounded fix stages eligible main/lowest-public-TID
local selection before shared selection, then commits exact pending/cause stores
without callbacks. Blocked/inactive/foreign-owner state and an existing fatal cause
are preserved; existing IGNORE and CHLD/WINCH discard behavior is unchanged.
Independent strict Rosetta/sanitizer matrix profiles pass. Normal-kernel guests
reproduce four old sentinel-90 cases; fixed exits are 138/138/140/140 with no
sentinels, plus normal main/worker FPU passes. Task guests stop. Closed raw fix
`2777eda7`: all 95 frozen files and five committed source hashes independently
verify against receipt SHA256
`47c5a5d9b4039c5d31bbfb444f9a5388c9f23178f4f422add0d8b0a47c96ee54`
at `.local/pr334-build-volume/evidence/action-default/receipt.json`.
Evidence: `build/review/pr334-action-default-review/`.

A distinct unwired private RESET path still bypasses that explicit-action helper.
Actual-source/model reproduction delivers main INFO|RESET with an unblocked peer's
local USR1 pending, restores main, then observes the peer boundary return NONE
under DEFAULT with `killed=0` and peer bit `0x400` stranded. This reopens that RESET
peer scenario beyond prior bounded private coverage; nonreset/native preservation
evidence is unchanged. Evidence: `build/review/pr334-reset-peer-review/`.
Correction must stage the first signal's exact consumption and prospective handler
self-mask before deciding peer defaults, with a reviewed exit outcome and complete
physical exclusions; the raw helper cannot simply be called after user stores.
The owner may implement that bounded correction for review: remove exactly the
first selected bit in a prospective view, apply current handler self-blocking,
then stage eligible peer defaults. A separate internal maximum-12-object copy
entry retains the existing private maximum-three API/refusal rules. RESET uses
the whole current state, process prefix including killed/cause and all eight
worker mask/pending extents. Committed-and-exit status 138 must reach existing
process exit before handler/user resume; refusal leaves every state unchanged.
Full object/span alias, prospective-mask/pending, target suffix/stack and private
guest regressions are required. This is implementation scope, not acceptance.

The public design is still prepare-only. Revised same-ticket FINISH rewait avoids
spurious clear/rearm gaps; pthread synchronization stays signal-transparent, and
poll invalid entries retain POLLNVAL. Exact legacy nonlocal-unwind ownership,
scoped masked-poll compatibility, fork constructor binding and public output
coverage remain reviewed contracts requiring implementation-specific evidence.

## November 4: private RESET prospective correction source/host review

The PR334 owner implemented the bounded RESET correction, still uncommitted and
without public admission. Its prospective view removes only the chosen initial
local/shared bit and applies only the current handler's self-blocking mask before
selecting a default-fatal peer. Successful copy commits prepared exact pending/
cause stores and returns explicit status 138; the opt-in caller enters process
exit before clock/diagnostics, accounting or handler/user resume. Current-only
duplicates remain pending under self-block and retain return-time default handling.

A separate internal maximum-12 exclusion entry protects the complete selected
state, process prefix including killed/cause, and all eight worker mask/pending
extents. Existing maximum-three API/count-four refusal and all original hardware,
root, stack, FPU and unique-user-page checks remain. Independent strict Rosetta
and ASan/UBSan VMM/delivery/raw-default profiles pass 6/6, including all 48 extended
object/span/destination-page alias refusals, prospective priority and complete
FPU/private/user rollback. New delivery tests against preserved `2777eda7` source
fail exactly at the required committed-exit assertion, independently confirming
regression sensitivity. Evidence/source hashes:
`build/review/pr334-reset-peer-fix-review/`.

The separately compiled test-only operation four installs INFO then invokes
existing delivery in the same final IF0 continuation, pinning the selected target
without a scheduler/user gap. Operations one/two/three and normal IDT are unchanged.
This fixture installation is separate from delivery's refusal transaction; it is
not a public fused-install contract. Updated whole-suffix/stack, mixed diagnostic
negative/current native guests and normal closure remain pending before acceptance.

## November 4: private RESET final qualification

Closed the bounded correction at PR334 `63984900`. Final receipt SHA256
`fa60ae294d585e5c6ebab3ad2e58e71b468e7d37de939a074f0aab3c3e4aa258`
at `.local/pr334-build-volume/evidence/reset-peer/receipt.json` and all 317 retained
files verify. All ten compiled/tested source hashes match the final commit.
Independent six host-profile hashes bind to these sources, including prospective
selection, complete refusal rollback, 48 alias controls and original three-object
limits. Source attribution remains precommit base plus exact dirty hashes, with
an explicit final commit closure rather than a retrospective clean-build claim.

The mixed diagnostic negative retains new VMM/FPU/op-four IDT and the same packed
payload while substituting the byte-matching frozen `2777eda7` process object;
it is not a clean old-kernel build. Six main/worker peer-queue cases reach forbidden
handler status 90 there. Fixed cases all exit 138 with no forbidden handler or
continuation. In each current-only control, exactly one delayed handler runs,
then private return exits 138. Both eight-case raw logs have complete exit lines,
independently checked rather than trusting parsed receipt numbers. Nonreset real
register/FPU/DF/pointer/red-zone preservation and depth-four/fifth-pending recovery
pass. The 65,536-map sample is 936 us for the nonreset path only, not a RESET
worst-case measurement, hard budget or complete IRQ-disabled-hook bound.

Independent strengthened target analysis verifies 41 actual/audit function bodies
and relocation identities, fully decodes all 40 reachable control graphs, rejects
indirect/unknown transfers, and checks target addends land at function entries.
Recomputed conservative stack bounds are 12,351/10,991 bytes including the 983-byte
common prefix, within 131,072 bytes. Scope remains fixed CPU0 IF0 transaction;
NMI/MCE and exit teardown are outside the bound. Actual suffix/IDT code routes
committed 138 to process exit before post-call time/diagnostic work.

Fresh normal closure two verifies complete exit-line/newline plus following
prompt, and passes raw defaults, exec, both TLS layouts, default full threadstress,
sysabi and main/worker FPU preservation. The first normal closure stopped on a
partial status parse (13 before 138 completed); its failure/driver/logs remain,
and no kernel pass/failure is inferred from that parser run. Task guests stopped;
retained application payloads are not a full current-userspace rebuild.
Evidence: `build/review/pr334-reset-peer-{native,stack,final}-review/`.
This closes the reopened private RESET case; public ABI/libc/WTF, atomic waits,
logical I/O gateway and nonlocal-unwind contracts remain preparation-only.

SDK carry review also checked live main: the queried C++/statvfs/regex/semaphore
files match pre-carry source and hyperbolic math is absent. However main's ports
pin `d760e541` diverges from browser `af466ba` (13 ahead, seven behind). Future
file-scoped dependency carries must preserve current ports integrations rather
than replacing the whole tree/pin. No matched SDK/ICU/JSC success is inferred.

## November 4: SDK source carry and reproducibility review

Reviewed PR334's dependency carry without modifying its checkout. SrvrPorts
`f88f1bd0` is pushed and is based on the actual root integration pin `d760e541`.
All 55 added overlay paths have the exact mode/type/blob of browser ports
`af466ba6`; all 1,230 pre-existing entries except README remain unchanged. README
adds five explanatory lines. The initial root manifest's 25 files match browser
`185db570` byte for byte, including executable bits. Kernel/shared signal and
existing salvage sources are untouched. This qualifies source reconciliation,
not a rebuilt SDK, ICU, JavaScript engine or guest runtime.

A bounded source-dependency scan found a confirmed incomplete carry:
`browser_math_probe.py`, `browser_evidence.py` and the ICU recipe require
`tools/test_math_hyperbolic.c` and `tools/mathprobe.c`, but both fixtures are
absent from the prepared root checkout and manifest. The implementation owner
added both exact browser fixtures; the expanded 27-file manifest and executable
modes independently verify. This closes the source omission, not fresh runtime
qualification.
Twenty Python helpers/recipes parse; the two fixtures are the only missing
literal source references found, which does not prove dynamic/generated paths.
Copied port notes also cite historical browser guest receipts and a root
`docs/browser-port.md` that is absent here. A new current
`docs/browser-port.md` explicitly attributes those runs to browser `185db570`
and keeps PR334 matched SDK/guest/JSC acceptance pending.

Independent host checks compile the carried hyperbolic source against the
browser fixture: ARM64 strict, ARM64 ASan/UBSan and x86_64 strict/Rosetta pass
threshold/edge values plus 200,000 deterministic float bit patterns within three
ULP. The fixture was read from the browser checkout; these are independent host
checks, not evidence that the corrected carry builds. Results and source hashes
are retained under `build/review/pr334-sdk-carry-review/`. Fresh matched C++ guard,
TLS destructor, linkage, SDK and guest qualification remains with PR334's owner.

The public logical-I/O packet remains preparation-only. The owner identified a
remaining interval between sticky kernel admission and a synthetic userspace
readiness/effect commit, and proposed a bounded mask guard around that local
commit. Review requires explicit guard state and prior-mask ownership, no waiting
user lock or callback inside the guard, and exact restoration before subsequent
admission. No public implementation or successful WTF/JSC suspension is inferred.
Documentation checking covers 895 local links with the same nine missing nested
submodule license targets; no new missing local link.

## November 4: local commit guard and sleep-return ordering

Reviewed the proposed LOCAL_END metadata path against current syscall admission.
Existing signal metadata calls can overlap owned I/O because they only mutate
fixed masks and cancellation state; process teardown remains gated in the IDT.
A register-only SIGNAL_IO form can follow that boundary if classification uses
only exact immutable registers and its helper independently checks live binding,
thread identity, top ticket/depth and ACTIVE ownership under IF0. The owner must
prove restoration does not wait behind foreign storage ownership, acquire a
copy/FPU lease, invoke callbacks or allocate. Wire-pointer operations remain
unsafe. This is design review, not implemented syscall admission.

Socket poll update currently calls unsafe NET_ERROR and may trace through
`fprintf`; neither belongs inside the proposed masked local commit. The owner
will hoist observations/tracing and revalidate the same synthetic/native identity
around bounded stores. A native-completion exception also needs immutable actual
result storage, a fixed root-kind/backend table and one-shot finalization; a flag
must not authorize another effect after interruption.

Actual `posix_time.c` has a confirmed integration gap for future public INFO:
its remainder clock sample occurs after `srv_sleep_ticks` returns. A handler at
the common user-return seam has already run by that sample, unlike today's
cooperative handler dispatch. An independent modeled-service fixture reproduces
this ordering with the actual libc: request 1.000000123 s, interrupted after
250 ms, handler 1.5 s. The cooperative control returns 0.750000123 s; moving the
handler duration into syscall return yields zero and fails the intended check.
The existing complete timing suite still passes. Public INFO is not enabled,
so this is not a native runtime regression claim. The proposed gateway must
capture interruption time/remainder before delivery, including tick fallback,
with exact bounded storage and nesting/alias behavior. Implementation and passing
matched regression remain with PR334's owner. Evidence/source hashes:
`build/review/pr334-sleep-return-review/`. No shared implementation was edited.

## November 4: immutable public protocol revision review

Reviewed proposal `37cd7382`, retaining exact hashes of both packet files.
Four 96-byte tickets capture signed native result, elapsed reference microseconds,
elapsed ticks and a nonreused outcome cookie. Together with the separate 32-byte
local guard, the stated selected-state arithmetic is 4,032 bytes. This is not a
compiled layout assertion. The revised sleep backend captures elapsed time at its
interrupt/completion decision before INFO, closing the modeled design finding;
its implementation and passing regression remain pending.

The local guard owns a separate nonreused cookie and original mask. Its compact
exit validates exact registers and live ticket/binding ownership; an ACTIVE
pre-admission filter rejects unsafe calls before waiting for I/O. The existing
mutex release source uses only THREAD_SELF, atomic stores and FUTEX_WAKE with
count one, matching the narrow exception. Native completion finalization is
one-shot and tied to the captured result/cookie; EPOLL slices and SEM yields
cannot declare their enclosing call complete. No new contract conflict found in
this revision. Implementation must preserve all 96-byte outcomes across permitted
fork, and delayed-handler tests must use supported monotonic/yield or nested wait
operations rather than assume nanosleep is supported inside INFO.

These findings are cleared for staged implementation review, not public runtime
qualification. Actual syscall allocation, C layout/offset assertions, backend-kind
mapping, target instruction/stack evidence and the full deterministic/native race
matrix remain required. Evidence: `build/review/pr334-public-packet-review/`.
Separately bound all 27 source manifest files to committed carry `68f18cd8`, with
exact ports gitlink `f88f1bd0` and no kernel/shared/salvage change; source acceptance
is closed, fresh matched SDK/ICU/WTF/JSC validation is still pending.

## Remaining work

Other long IRQ-disabled operations, including initial/churning PMM scans, heap validation and poisoning,
driver DMA/reset/removal ownership and broader manual isolation review remain
open. Deterministic fault/dirty-writeback ordering now has the host coverage above;
forcing it through actual VM context switches remains separate work. Future boot
paths must establish or verify WP rather than inheriting this configuration's
acceptance. The historical [coverage ledger](audit-coverage-2026-11-02.md) retains
the broader gaps.
