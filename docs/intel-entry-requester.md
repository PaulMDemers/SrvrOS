# Original-owner entry/reset and requester restoration

This separate v27 diagnostic composes the accepted
[zero-geometry entry/reset](intel-state-aware-reset.md) with the existing
[original-claim requester hold](intel-gpu-requester-hold.md). Native acceptance
is pending. The [Air7,2 record](machines/macbookair7-2.md) owns exact physical
results; a prior reset receipt cannot start another operation.

The original full census selects the entry route. Only completed kind 2 entry,
with zero geometry, original registered-child return, a fresh full post-census
and both reader vectors, can feed its actual original post-census into takeover.
Preparation/reset/re-arm creates its original claim and registered consumer.
That live chain then performs the existing exact PCI Command halfword 7→3,
full profile/readback and bounded AF pending observation, followed by exact 3→7
restoration. It adds no Status/AF-control store, PTE, ring, flush, invalidation,
page-flip or managed-memory writer. Ordinary startup does not select it.

Requester admission retains the original claim, preparation/reset, epoch and
provider checks. New-image native checks additionally bind the original entry,
returned-child metadata, original post-census address, handoff provider and all
original takeover stamps while the epoch-1 claim remains live. This is source
continuity within the registered consumer, not a saved-receipt capability.
The historical handoff's source-age bound is never reapplied as a new clock or
extended into another operation. The live claim keeps its original deadlines.
Source/clock/alias/return failure retains the original root, even after a logical
child returns. Unknown stores are neither replayed nor optimistically restored.

All existing bounds remain: BCS/original source 100 ms, entry/readers 150 ms,
handoff/fresh-source age 100 ms, preparation/reset 100 ms, GT 300 ms, HDA 400 ms,
outer 500 ms and AF pending 32 polls/10 ms. Kind 1 enabled/nonzero entry may
complete its own distinct entry but refuses reset and requester admission.
Disabled nonzero geometry also refuses. No forced enable or geometry clearing.

The dedicated image is `build/intel-entry-requester/srvros.elf`; its firmware
request and `entry-requester-preinit` marker are separate. The exact 23-token
recipe adds `srvros.intel-gpu-requester=hold` to the entry/reset recipe. Stage
with `tools/hardware_boot.py stage --laptop-probe-volume <this-unit-UUID>
--intel-entry-requester` using the [Ubuntu workflow](ubuntu-device-agent-setup.md).
Old v21 requester and v26 entry/reset recipes, images and wire contracts remain
separate. An old public decoder cannot report new-image completion.

The new image alone uses canonical little-endian base64 census vectors and
fixed-width ordered entry/requester summaries. Every original value and partial
prefix survives. Decoders expand into the established typed validators, retain
full raw census/context/reader/capability/poll coverage, and reject mixed legacy
rows, noncanonical encoding, missing/duplicate vectors, wrong order or image,
false source/clock/epoch binding and unproved completion. The existing 700-byte
requester raw vector is unchanged. No saved-log limit grows: 64 KiB with the
original 1,536-byte reserve. The strict joined collector field is
`intel_entry_requester`; it does not populate the old standalone result fields.

The test is silent and unattended, with on-screen progress and a prerequisite
checkpoint. Keep volume at 1%. Safe saved/unmounted outcomes return to Ubuntu;
retained uncertainty or unsafe persistence halts visibly. Qualification must
cover actual adapters and source/read/store/pending/return faults, old-image
isolation, exact widths and final admission clocks, static lifetimes, 2 KiB
frames, fatal sanitizers, compiled omission controls, the saved-tail budget and
exact staged EFI/MBR refusal/save/return and missing-workspace halt.

PCI Command readback and AF TP clear do not prove all posted writes, internal
prefetch, GPU pipelines, cache/TLB, display readers or firmware/SMM retirement.
The original reference and scope remain in the requester contract above. All
255 completion obligations remain open; assurances and managed pages remain 0.
The [managed-memory plan](intel-managed-memory-plan.md) still requires genuine
interval/RAM transfer and completion/retirement before a native GPU mapping.
