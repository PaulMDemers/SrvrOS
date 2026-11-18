# Borrowing existing Broadwell scanout backing

The [native layout census](intel-bdw-stolen-scanout.md#native-v51-read-only-observation-complete)
found two complete, stable6MiB candidate mappings beyond the inherited34MiB
fence. This backend connects an **already owned reservation** for those spans
to the existing [memory lifecycle](scanout-memory.md). It does not obtain that
reservation, allocate firmware RAM, create a CPU alias, publish a plane, write
PTEs or grant hardware ownership. There is no native producer or boot-image
caller. The inherited-framebuffer desktop remains unchanged.

This differs from [GPU address reservations](native-scanout-reservations.md):
`scanout_space` manages offsets that a provider already owns, while this backend
keeps external physical backing reserved while ordinary memory children use it.
It neither opens a GGTT arena nor treats populated entries as available space.

## Original source and reservation

A trusted, CPU0-serialized provider must publish an original-address source with:

- Its provider context, lifetime identity, owner, kernel root and source epoch.
- Original admission and absolute expiry in the same source clock.
- Fresh classified layout and original facts for the complete inherited range
  and both candidate ranges, including the padding/translation tail.
- All seven independently established reservation/firmware/engine/DMA/cache/
  reader/mapping rights. Unknown or partial rights refuse before callbacks,
  registration, exclusion pins or child creation.

The backend recomputes the geometry plan, checks every physical extent against
usable stolen-memory bounds, verifies complete counts and rejects overlapping
old/candidate spans. Geometry is necessary but supplies no ownership right.
Saved census JSON, a previous boot's vector, EFI reservation and reset ACK cannot
construct a live source. The native factory must remain in the original live
census/owner lifetime; the completed diagnostic returned its exclusion and
cannot now be borrowed as a source.

Begin pins the real participating CPU display exclusion and registers its
consumer **before** any provider/clock callback. The source is consumed once;
clean close clears registration without making that source reusable. Owner,
source, context and child storage must survive retained failures. Copying a
live object cannot transfer its pin or register a second consumer.

This is a trusted-adapter contract, not protection against malicious kernel
code or opaque firmware. A source mask alone is insufficient: each operation
also requires a fresh, matching independent provider proof.

## Validation and clocks

| Phase | Provider obligation |
| --- | --- |
| ADMIT | Exact pair has an exclusive physical reservation; firmware/engines/DMA/cache/readers and existing translations are admitted under this original owner |
| LIVE | Maintain that ownership, source/root/epoch, mapping and reservation while registered children exist; any owned publication must obey its independent reader lifecycle |
| RETURN | Children drained; CPU aliases, pending publication and all affected readers retired; original hardware state/mappings preserved or restored and visibility obligations complete |

Proofs match source/provider/lifetime/owner/root/epoch, exact phase and all
rights. Their timestamp lies inside the current validation bracket. Value-only
snapshots detect callback mutation of binding, callbacks, registration, slots,
progress and exclusion identity. Reentrant public operations refuse. A failed
proof or clock never becomes a successful return.

Each validation has a100ms operation bound, nonzero/forward clock checks and
the original absolute source expiry. A later check starts a new bounded check,
not a new source lifetime. The provider must also preserve its own enclosing
clock/deadline accounting. There is no automatic lease renewal or timer-driven
free: expiry or uncertainty retains backing and exclusion.

## Reusing the existing memory implementation

One `scanout_memory` object may be registered for each candidate. Registration
precedes allocator callbacks. The backend's allocation callback lends the exact
candidate base; its validation callback checks the original owner again. A
child may use fewer pages, but the **whole** candidate, including unused padding
and display translation tail, stays reserved until outer return.

These callbacks do not call PMM. The child's `allocations`/`releases` counters
count reservation tickets, not physical allocation/free. Returning one child
marks its ticket returned; it does not release the outer reservation or make
any firmware RAM available to another allocator. Each candidate gets at most
one child per source lifetime, with ordinary CPU borrows/holds reused inside
that child. A second or overlapping child is refused.

A memory CPU ticket describes a physical span, not a writable virtual mapping.
The native alias/visibility adapter is still required before actual pixel
access. This backend grants no format/tiling/cache or plane-programming policy;
the memory publication API still requires its separate address owner.

## Closing and retained failures

Close permanently blocks new children and sets the existing children's closing
flags before waiting. This stops new CPU borrows, holds and publication while
allowing an original CPU borrow/hold to finish through the ordinary memory API.
The real memory objects must close and return their tickets before outer RETURN
validation. A pending child keeps the original reservation and display pin.

Only a fresh RETURN proof permits unpinning and clearing the original source's
consumer. It never frees the external physical spans. Any callback/clock/source/
exclusion failure after pinning latches FAULT with backing and registration
retained. No forced cleanup, retry, source reuse or uncertain free is provided.
Even an admission failure after callbacks retains its pin; callers must preserve
the failed object and source storage until external recovery.

## Qualification and next native work

`make test-intel-bdw-stolen-borrow` executes the actual backend with the real
memory/hold and display-exclusion implementations. The host-only ownership
provider has independent source identity and time; it does not represent native
hardware authority.7,360 cases cover per-callback faults, binding/proof/slot
mutation, missing rights, expiry/frozen/backward clocks, copied/stale objects,
closing during CPU/hold use, exact physical child return and exclusion retention.
All16 compiled weakening controls must fail at runtime. Fatal ASan/UBSan and
freestanding2KiB frame checks pass. Native object imports contain no PMM, PCI,
VMM or heap-allocation calls, and ordinary image linkage excludes this backend.
Existing memory/hold/transfer/handoff regressions also pass.

No new hardware trial is justified by these software tests alone. Before wiring
this backend into a native producer, supply and review all of the following:

1. An authoritative reservation/transfer for the exact two external spans,
   preserving all firmware allocations, the full inherited fence and original
   mappings. The factory must register this consumer under the original owner.
2. Maintained firmware/requester/engine control, actual outstanding DMA/posted-
   write/prefetch and GPU cache/translation completion, and every affected
   display/firmware reader. Clear samples or a rights mask cannot replace these.
3. Exact CPU alias/cache/visibility admission and bounded access under that
   same lifetime. The completed read-only census supplies neither aliases nor
   a memory-store authorization.
4. Ordered first tiled-to-linear primary transition, live latch and independent
   old-reader retirement, then original-state restoration and endurance evidence.

The [current Air record](machines/macbookair7-2-current.md) remains the authority
for physical acceptance. This work closes the reservation-to-memory software
bridge only; native ownership, first RAM store and tear-free output stay pending.

The next native producer input is the [live reader/cache preflight](intel-bdw-stolen-scanout.md#live-display-reader-and-candidate-cache-preflight-wire5).
It gathers new observations within the original census exclusion. Its active
reader bases and hypothetical cache policies do not satisfy any rights bit;
complete reader footprints, maintained exclusion and actual alias visibility
still need their own native implementation and evidence.
