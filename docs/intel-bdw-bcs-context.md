# Broadwell private CPU context image

The isolated `intel-bcs-context-v46` / wire8 trial initializes a private **CPU**
image before GPU exclusion, verifies it under the original full-GT reset/memory
owner, then verifies and erases it only after actual GT/HDA/outer retirement.
It preserves the four-page arena and its independent cleanup contract. It does
not map the proposed GPU addresses, install an executable context, submit a list
or certify native DMA/cache/display-reader ownership.

## CPU submission access during the two held samples

In context receipt version 3, the reset owner revokes CPU writes to the five
command-streamer MMIO pages before the first held GGTT sample. CPU0 checks
CR0.WP, the original address space, and the effective supervisor, NX and
uncached mapping for each page. At both sample boundaries the VMM walks the
kernel page tables to check that each physical register page has exactly one
4 KiB read-only alias. PCI bus mastering stays disabled while the driver reads
slot 8704 and the four adjacent reserved slots. Two hardware snapshots bracket
these reads. Each records GO_GAM_REQ and the raw reference and prefetch
counters for both video engines. The receiver preserves all five register
values per sample, including open requests and nonzero counters. It restores CPU write access
in reverse order after the second sample.

The completed receipt has `sealed=5 reopened=5 ro_mask=0 alias_checks=6`
and `pte_reads=30`. A partial transition retains the reservation and records
the successful prefix. Older receipt versions remain readable. Replacing
slot 8704 still requires a hardware owner that can exclude firmware and other
GPU readers and witness fresh engine and L3 drain acknowledgements.

On the Air7,2, release `9a8fbc4f973644df` completed this interval with
all five mappings restored. Both samples had `GO_GAM_REQ=0x7fff` and
zero reference and prefetch counters on both video engines. The six client
permissions were open at both boundaries. The original entries in slots
8704–8708 matched on each read. See the [unit record](machines/macbookair7-2-current.md).

## Audited format

Intel's [Broadwell PRM Volume3](https://cdrdv2-public.intel.com/690792/intel-gfx-prm-osrc-bdw-vol-03-gpu-overview.pdf),
printed pages19–21, places the per-process hardware-status page first and the
five-cacheline register image at LRCA+4KiB. It also requires snoopable status
storage; stolen memory is unsuitable. All four pages in this trial are newly
allocated tagged CPU RAM with the original WB/supervisor/NX alias checks.

The PRM register table has inconsistent second/third LRI lengths. The actual
22-pair layout is cross-checked against Intel's MIT-licensed
[Linux v6.12 gen8_xcs_offsets](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_lrc.c):
11,9,2 pairs at state DWORDs1,33,65; the batch high registers are0x22168 and
0x2211c, rather than adjacent-low+4 guesses. The final pair contains BCS EXCC,
not RING_MI_MODE. The complete MIT notice is retained with the adaptation.
The constructor uses the PRM's posted LRI form with those actual lengths.

The [descriptor format in PRM2d](https://cdrdv2-public.intel.com/690870/intel-gfx-prm-osrc-bdw-vol-02d-commandreference-structures.pdf),
printed pages107–110, distinguishes legacy32 addressing and the separate PPGTT
bit. This narrow candidate uses GGTT-only legacy32, PPGTT disabled, valid/force
restore set and all PDP values zero. It has no inherited CPU page-directory
pointer. Reserved descriptor bit5 remains clear. The masked context control
inhibits synchronous switching and engine-context restore; private ring head0,
tail24 and one-page enable are initialized in RAM. The remaining fields and
unused bytes are deterministic zero. A direct-GGTT MI_FLUSH_DW/nonce packet
matches the existing [completion codec](intel-bdw-bcs-flush.md).

This is a candidate byte image, not hardware mode/cache/workaround admission.
Actual PPGTT mode needs independently owned initialized GPU page tables, not a
CPU root or a flag change. Context ID allocation/uniqueness, actual mappings,
visibility, coherency and mode policy must be established before execution.
The new [submit protocol](intel-bdw-bcs-submit.md) remains without a native caller.

## Native lifetime and evidence

A distinct image and exact `gt-bootstrap-context-v1` recipe preserve old image
and wire behavior. The CPU initializer materializes all16KiB and immediately
checks the complete image. Both original held-source guards check it again.
After actual enclosing retirement, cleanup checks the complete image once more,
erases it, independently verifies all2048 zero QWords, retires four exact aliases
and frees the same four pages once. Partial initialization, corruption or lost
source keeps the original backing pinned. No GPU access or release is inferred
from a constructed descriptor.

Wire8 adds `intel-bcs-context` with original physical/root binding, proposed
layout/descriptor, initialization and erase timestamps, exact verification/hold
counts and explicit zero mapping/submission/ownership fields. Its parser requires
initialization inside CPU preparation, erase inside cleanup after parent return,
and the original complete arena receipt. Older profiles refuse wire8 and foreign
context records. Unqualified wire8 logs can use the existing96KiB raw-copy bound
only with the original wake37 marker; that read allowance does not accept them.

The version 2 context record uses GGTT slots 8705–8708. The former version
placed the ring in slot 8704, which is now the target of the one-entry rollback
trial. The new layout reserves four disjoint GPU addresses in the software
interval registry. Both captured PTE vectors must distinguish the target,
the four command pages, and the private CPU backing. At both post-reset
arena checks the driver reads all five actual entries through the existing
read-only UC window, with PCI bus mastering disabled. Each QWord requires a
matching high/low/high read. After checking and erasing the CPU image, the
driver returns the reservation. Older version 1 reports remain readable.

These addresses still translate to inherited memory. The private hardware
status page requires snoopable storage. The driver must acquire the four
slots and install the private RAM mappings before it can submit the blitter
context. Its own completion command cannot establish that initial grant.
The address reservation and the logged 30 reads are software exclusion and
translation evidence only. This placement is for the one-page trial. A full
surface starting at 8704 spans these four slots. Retire the temporary context
before reserving that surface, or place a longer-lived ring outside both
framebuffers.

Local validation decodes each LRI block independently, compares the actual
completion packet, corrupts every arena DWORD, rejects invalid/overlapping
inputs and runs compiled layout/address-mode/extent weakening controls and fatal
sanitizers. The actual native adapter has faults at every initialize/held/cleanup
check on both modeled entry routes. Future frozen validation retains all42
previous slots and adds context image/model slots, followed by exact-stage VM
save/return and missing-workspace paths before any native boot.

## Remaining bootstrap boundary

The original full-GT reset and narrower VT-d DR/DW/System Agent invalidation
retain their established scope. They do not independently document completion
of every outstanding GPU pipeline/cache/prefetch operation or firmware/display
reader. A private command cannot prove ownership of the mappings needed to
submit itself. The proposed range and initialized CPU bytes cannot bridge that
cycle. All255 obligations, zero managed GPU pages/PTE writes/submissions and the
first-GPU-write refusal remain. This trial advances context preparation only;
actual native context execution and tear-free scanout remain pending.

No input is needed. The guest shows progress and saves its result to this
machine's exFAT workspace. Safe saved/unmounted outcomes return to Ubuntu;
retained uncertainty halts visibly. User-authorized trials keep host volume1%;
this CPU/reset profile plays no sound. Preserve current/recovery and accepted
payload/evidence banks through staging and collection.


## Native result: one CPU image lifetime complete

Frozen source `4ce676bd` / release `08ae0d63f7b821c4`, session
`session-0006607c842689c0-00`, completes one original Air7,2 kind2 epoch0→1
lifetime. All46 frozen slots and both exact-stage VM paths passed beforehand.
The image has four complete verifications, two held checks and one post-parent
erasure, with no fault. Construction takes9us and erasure2us; CPU arena setup/
held/cleanup take87/401/82us. All2048 zero QWords verify, four exact aliases retire
and the same four pages release once with DMA57→57.

Preparation219.532ms, maximum GT phase77.210ms, GT379.409ms, HDA399.125ms and
outer451.007ms remain within the existing practical policy; no deadline changes.
All1,590 prior files preserve apart from expected metadata, yielding1,593files/
187sessions. Strict raw/checkpoint, offline filesystem/allocation/writable/
headroom, boot-region, recovery/default and consumed one-shot checks pass.
The run closes as `DIAGNOSED`;732 banked files independently verify outside build.

This completes CPU image preparation only. Proposed addresses were never mapped,
no command was submitted and all255 GPU obligations remain. Actual executable
context/mode/cache/workaround policy, GPU ownership, enabled entry for this source,
repeatability and tear-free scanout remain unaccepted. See the [current unit
record](machines/macbookair7-2-current.md) for the remaining bootstrap boundary.

## WiDi submission coverage

At each of the two held checks, the version 4 context diagnostic walks the
sole CPU page-table root for BAR0 page `0x14000`. Any mapping of that page,
including a large direct-map leaf, ends the trial before the next GGTT read.
The report records each successful walk in `widi_unmapped`. Two are required
for completion. The five engine register pages stay read-only during both
walks. Earlier reports retain their original coverage.

Intel lists both the CPU and the display proxy as writers of the WiDi video
and audio tail pointers on that page. The walk covers the CPU only. The slot
writer still requires the WiDi client to acknowledge the same traffic block
as the five command engines while the original display runs.
[Intel Broadwell MMIO, p. 2](https://cdrdv2-public.intel.com/690867/intel-gfx-prm-osrc-bdw-vol-13-mmio.pdf#page=6)

Version 4 also records `MISC_CTRL0` at both traffic samples through the
existing UC register alias. On this Air under Ubuntu, three read-only samples
about 50 ms apart returned `0xc5280000` with i915 forcewake held and eDP
enabled before and after. Bit 31 locks the power-management policy and bit 25
is clear, disabling GAM GO messaging on CPD entry/exit in this configuration. The GAM
request and response were `0xff` and `0x7f`, WiDi returned `0x1f`, and L3
returned zero throughout. The native SrvrOS RC1e release `0b8aeb4fce7c1c8d`
read the same `MISC_CTRL0` value before the request and after restoring
the original controls. This register configures a transition and supplies
no software request to enter or hold it. Intel specifies that its locked
fields survive PCI function reset.
[Intel Broadwell register reference, pp. 606–607](https://cdrdv2.intel.com/v1/dl/getcontent/690789)

## Render indirect context for the command interval

When the firmware interval lends the final 32 pages, the native diagnostic
also constructs a Broadwell render indirect batch in CPU RAM. At slot 11744
its proposed batch address is `0x02de4000`, with two cache lines encoded in
`INDIRECT_CTX`. Its dedicated save word is at `0x02de5100`. The diagnostic
verifies every word, including trailing no-ops, at all three lease checks.
The version 5 receipt records the pointer. The earlier version 4 receipt
continues to describe only the blitter image.

The indirect batch disables arbitration, saves the original L3 policy, enables
coherent-line flushing, stalls and flushes the render data cache, and restores
the saved policy. It also contains Broadwell's context-switch SLM clear and
restores arbitration. A future submission needs a driver-owned RCS context
and status page, the uniform captured L3 policy, and exclusive, GPU-visible
batch and scratch mappings for the entire switch. The context-switch recipe
and its independent i915 hardware comparison are described in the
[flush guide](intel-bdw-bcs-flush.md#device-side-queue-hold-through-slot-8704-rollback).
