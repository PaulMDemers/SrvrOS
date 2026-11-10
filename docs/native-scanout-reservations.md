# Native scanout address reservation

This software mechanism is the next step after the accepted
[Air7,2 shadow lifetime trial](scanout-buffer-lifetime.md#air72-native-shadow-acceptance).
It reserves offsets inside a bounded GPU address arena; it does not establish
GGTT ownership, encode or write PTEs, map physical RAM to a GPU, or advertise a
native desktop backend. The [publication transaction](scanout-publication.md) now consumes leases in a
software-only guest fixture. There is currently **no native caller**. The tested
managed ranges are synthetic authority supplied by host fixtures.

## Import and leases

`scanout_space` supports the observed 256 MiB CPU aperture, at most 32 concurrent
leases and 64 managed/protected intervals per import. These are limits on an
internal boot-scoped resource mechanism, unrelated to desktop window/process
capacity. Page units are 4096 bytes. Intervals are half-open and validated without
overflow before the object changes. The caller serializes operations on CPU0;
the core neither masks interrupts nor acquires GUI/storage locks.

Every page starts unavailable. An audited owner must explicitly import intervals
it actually owns. Duplicate/overlapping grants form a union; protected intervals
then remove pages from that union. Unknown gaps remain unavailable. An absent
PTE, a scratch PTE, a stable snapshot or a firmware-reserved physical range is
not a grant. A future import must preserve all existing mappings, other GPU
clients, fences, guard pages, firmware allocations and unobserved regions.

On this Air, the inherited X fence spans the whole 34 MiB interval starting at
offset0. Protect that complete interval, not just the visible 1350 or padded 1356
pages. No part of the unobserved fence tail is known free. The synthetic full
aperture test excludes all 8704 fence pages before allocating either 1356-page
lease. It does not establish that addresses after the fence are available on
the actual machine.

Reservations use first-fit contiguous runs with power-of-two page alignment.
Exhaustion, invalid input, wrong owner/epoch, serial exhaustion and slot capacity
refuse without changing the object or output token. Tokens bind object, epoch,
serial, slot, start and size; released slot/address reuse cannot revive stale
work. A non-reset pool supplies monotonically increasing epochs, including for
same-owner reuse. Reopening requires a fully closed object, never a fault.

## Exposure and retirement

| Lease state | Release contract |
| --- | --- |
| Reserved | No writer has seen the lease. Cancellation may release it immediately. |
| Exposed | Marked before entering any writer. Both the address range and its physical backing remain pinned; cancellation alone cannot release them. |
| Retired | An audited adapter has separately established exact prior PTE restoration, translation retirement and retirement of all device readers. Only then may the address range be released. |
| Quarantined | Publication or retirement is uncertain. The range remains unavailable until an external reset; this mechanism cannot clear it. |

`scanout_space_retire` requires the complete proof mask. Like the lifetime core's
completion token, the mask is a necessary software contract, not independent
evidence of hardware retirement. No current native adapter supplies that proof.
An exposed lease cannot be retried or rolled back to Reserved even after a
reported zero-write refusal; this conservative implementation requires the
retirement path. Separate RAM and address leases must both permit release before
the future adapter frees physical backing. Releasing an address token never
calls PMM and never frees borrowed framebuffer memory.

Close blocks new reservations and exposure, but permits already exposed work to
retire and safe leases to release. It reaches Closed only after every lease is
released. Quarantine latches arena Fault and blocks new admission/reopen/close;
unexposed or independently retired *other* leases can still release safely.
There is no hardware reset, timer or recovery callback inside this pure core.

The arena is now address-sealed: a copied bitmap/epoch cannot reserve, expose,
retire or release through a second arena. The [joined backing owner](scanout-memory.md)
keeps physical RAM pinned alongside these leases and the publication transaction.
Its native qualification imports no managed interval and must refuse exposure.

## Boot-client exclusion and populated entries

The [boot-client claim](intel-boot-client.md) prevents competing participating
Intel initialization. It grants no native interval or hardware ownership. The
[Air7,2 window result](intel-ggtt-window.md#reference-native-result) observes
all16 sampled entries immediately beyond the full34MiB fence already populated.
Do not import index8704 as free space or use a software claim as the missing
firmware/engine/reader handoff. There is still no native managed range.

## Native authority still required

| Requirement | Current evidence and next proof |
| --- | --- |
| Inherited backing | Accepted full 1356-PTE/fence readout and final shadow-trial preservation. Preserve the complete fence and all firmware allocations. |
| Private RAM | Accepted bounded PMM allocation/free/quarantine trial. It did not give the GPU access to those allocations. |
| Scanout layout | The shadow allocation has1350 visible pages; the inherited X-tiled extent has1356. Preserving this tiled route requires the complete padded extent and verified tiling/swizzle/cache layout. The current visible-only shadow profile is not that proof. |
| GGTT arena | No managed native interval. Establish a boot-scoped exclusive owner and audited reservation import before a PTE writer. Do not reclaim arbitrary invalid/scratch entries. |
| DMA addresses | Native source `d5ad0c54` / release `53460fd7c2c0f337` observes the exact GPU-specific unit with TES clear and RTADDR0 in two stable read-only samples. This accepts the bounded [state observation](macbookair7-2-dma-observation.md#native-observation-accepted-2026-11-04), not DMA/GGTT ownership, private-buffer visibility or an immutable translation policy. No IOMMU bypass or control writes are proposed here. |
| Firmware exclusion | Stable observations are insufficient. Establish which kernel/firmware users can change mappings, plane, route and power while an owner exists. |
| Publication ordering | Specify exact PTE encoding/cache policy, write width/order, posted-write completion and required invalidation using generation-specific references. A CPU memory barrier alone is not the completion proof. |
| Route/restore | Preserve panel link, timing, brightness, power and inherited surface/layout. Audit the minimal register write set and restoration sequence before enabling it. |
| Physical retirement | Independently observe the exact armed surface becoming live and old readers retiring; advancing frame counters alone remain shadow pacing. |

The pinned [Linux GGTT implementation](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/gt/intel_ggtt.c)
separates address management, PTE writes and translation invalidation. Its
`gen8_ggtt_invalidate` depends on mapping/cache policy. This is a reference for
the future write audit, not a write recipe or an ownership grant. The
[Air display contract](macbookair7-2-display-presentation.md#bounded-backing-readout-implementation)
retains the Intel memory-view and fence references already used by the reader.

## Linux firmware reference

A read-only Ubuntu firmware-table inventory on this Air identifies a checksum-
valid 136-byte DMAR table: one segment 0 DRHD explicitly scoped to 00:02.0 at
`0xfed90000`, a separate include-all DRHD at `0xfed91000`, and an RMRR scoped to
00:02.0 covering `[0x8b800000,0x90000000)`. The latter spans the observed GGTT
table and stolen data memory. Header address width is 39; flags are 3. The local
table hash and bounded decoder output are retained with the run evidence.

This describes firmware topology only. It neither reads live IOMMU registers
nor establishes SrvrOS translation state, DMA mapping authority, runtime identity
mapping or usable private buffers. The next native diagnostic must validate its
own table/resources and remain read-only; Linux's running driver state cannot
be transplanted into SrvrOS. Optional AML probing remains outside desktop startup.

## Validation and next checkpoint

`make test-scanout-space` covers default-unavailable imports, overlapping grants,
the full inherited fence, alignment and boundaries, stale/foreign/altered tokens,
same-owner/slot reuse, close during exposure, incomplete retirement proofs,
sticky quarantine, safe unrelated cancellation and epoch/serial/slot exhaustion.
An independent page-by-page first-fit oracle checks 50,000 deterministic mixed
allocations/releases against the actual availability bitmap.

Host tests and sanitizers cannot accept native managed ranges or device reader
retirement. Keep the mechanism disconnected from the ordinary Intel/VirtIO
paths. The next physical work is source-bound read-only DMA/ownership inventory;
a writer requires a separate reviewed implementation, exact-stage VM checks,
persisted trial results, independent recovery and explicit native acceptance.

Source `96b5cc0e` passes the complete local core/profile/build gates, ASan/UBSan,
three compiled negative controls, real guest PMM shadow fixture, VirtIO
presentation ownership/reacquisition and desktop/Open With regression. The
allocation oracle includes10,803 successful allocations,10,792 releases and
17,564 refusals at193 distinct starts. These are host/VM results; they grant
no managed native interval and do not accept real hardware mapping retirement.

The [boot reader/range qualification](intel-handoff.md) adds participating CPU
exclusion and a pinned admission transaction. Native grants remain refused;
no hardware handoff or managed interval is inferred from these observations.

## Next native implementation order

The [post-reset managed-memory plan](intel-managed-memory-plan.md) separates
exclusive interval transfer, a one-page mapping/restore trial, command-backed
completion and full-buffer/flip publication. Air7,2's accepted bounded reset and
preservation do not import a range into this allocator. The16-page transaction
limit also remains distinct from a complete scanout allocation.
