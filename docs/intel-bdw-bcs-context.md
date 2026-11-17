# Broadwell private CPU context image

The isolated `intel-bcs-context-v46` / wire8 trial initializes a private **CPU**
image before GPU exclusion, verifies it under the original full-GT reset/memory
owner, then verifies and erases it only after actual GT/HDA/outer retirement.
It preserves the four-page arena and its independent cleanup contract. It does
not map the proposed GPU addresses, install an executable context, submit a list
or certify native DMA/cache/display-reader ownership.

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
