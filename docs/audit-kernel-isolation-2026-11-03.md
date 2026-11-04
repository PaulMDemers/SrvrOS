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

## Remaining work

Other long IRQ-disabled operations, including initial/churning PMM scans, heap validation and poisoning,
driver DMA/reset/removal ownership and broader manual isolation review remain
open. Deterministic fault/dirty-writeback ordering now has the host coverage above;
forcing it through actual VM context switches remains separate work. Future boot
paths must establish or verify WP rather than inheriting this configuration's
acceptance. The historical [coverage ledger](audit-coverage-2026-11-02.md) retains
the broader gaps.
