# Private backing and GPU mapping ownership

`scanout_memory` joins allocator-owned RAM, exclusive CPU write borrows, an
already admitted GPU address range and the existing publication/restore core.
It is a CPU0-serialized trusted-adapter mechanism. It does not import a native
GGTT interval, choose PTE encoding, prove DMA/cache policy or supply hardware
completion. The [managed-memory plan](intel-managed-memory-plan.md) owns those
remaining native requirements.

The owner is sealed to its original address and a monotonically increasing
pool epoch. It captures the allocator and layout before allocation and binds
them through release. Copies, stale tokens, output aliases and callback reentry
cannot release or restart the original owner. A failed release is attempted
once: its effects may be partial, so the owner retains a terminal fault and
cannot retry. The range arena is also sealed to its original address; copying
its available bitmap cannot manufacture a second allocator.

## Storage and CPU access

Storage arithmetic supports linear rows aligned to64bytes and X-tile rows
padded to8 with512-byte pitch. On the Air7,2 profile,1440×900/pitch6144 has
1,350 visible pages but requires1,356 allocated pages. The owner supports up to
the existing publication limit of1,536pages. This calculation does not admit a
native format, tiling/swizzle mode or cache alias.

An allocator must return a uniquely owned pinned contiguous span or zero with
no allocation. The platform validator checks its complete physical range.
An active CPU borrow excludes mapping and release. Closing blocks new borrows,
then waits for the exact outstanding borrow to return. The borrow covers every
allocated page, including row/page padding; initialize that complete span before
GPU exposure. Ending a borrow revalidates backing and rejects stale or duplicate
returns. No copying, allocation or hardware access happens inside a UI callback.

## Mapping and retirement

Preparation requires an original active `scanout_space` with an actual admitted
managed interval. It reserves the complete buffer and calls a trusted encoder
for each exact owned physical page. All desired entries are captured before the
existing publication transaction reads the original mappings. Unknown ranges
remain unavailable; neither reset completion nor a populated/scratch/absent PTE
is an import. The provider's guard must retain actual parent/exclusion, cache,
DMA and requester proofs for its entire lifetime.

CPU writes remain blocked after preparation. Apply uses the publication core's
exposure-before-first-write ordering. Partial/uncertain stores quarantine both
the address range and RAM. Close waits for device readers, restores exact
original translations, and requires the provider's translation/completion and
reader retirement checks. Only then does it release the address lease and
attempt the allocator release once. A CPU fence, raw PTE readback or matching
software token cannot substitute for those hardware proofs. Provider contexts
and the original object must remain pinned through every fault.

The legacy hardware-handoff snapshot still carries at most16entries. The
separate [complete transfer contract](scanout-transfer.md) captures up to4,096
original entries and exercises two full buffers with synthetic providers. It
does not expand the observed native snapshot or claim a full-buffer native grant.
Native mapping remains gated until exclusive interval transfer, full manifests,
owned completion resources and hardware retirement are established together.

## Isolated backing qualification

`--intel-memory-capture` selects an isolated image and the exact Air7,2
display/backing profile. It uses the actual PMM, CPU translations, frame tags and
new owner to run four cases:

1. One-page allocation, CPU borrow, blocked close, borrow return and one release.
2. First full1,356-page buffer, complete pattern write/read, stale-token rejection
   and refusal to expose an unknown GPU range.
3. Concurrent disjoint second full buffer, distinct complete pattern, independent
   first-buffer release with second-buffer contents preserved, and clean release.
4. One deliberate release refusal before PMM is called, retaining one known
   private page until reboot and proving duplicate release is blocked.

The diagnostic allocates2,714frames, releases2,713 and deliberately retains1.
It verifies every owned page/tag/CPU translation and all padded content. A fresh
arena imports **no managed range**, so both full buffers must refuse reservation
before any encoder, GPU read, writer or completion callback. Inherited native
display/backing is independently revalidated afterward. No GPU PTE/plane/route
writer exists in this image. All255 completion obligations and zero native
hardware assurances remain unchanged.

The whole allocation/copy trial keeps the existing shadow-trial30second bound;
it does not change the separate100/300/500ms GPU protocols. Unexpected retained
state halts visibly after evidence persistence. The single expected private-page
fault is separately accounted, has zero GPU exposure, and permits automatic
return after save/unmount. Progress and versioned case/summary records are saved.
Before allocation, a durable `session-a.log` prerequisite checkpoint is required.
Native collection independently binds its complete display/backing observations
to the final log; missing, late or uncommitted checkpoints cannot pass.
`--intel-memory-fixture` runs the same actual allocation/owner code with synthetic
geometry in QEMU and reports `kind=fixture`; it cannot accept native backing.

Host tests exercise actual joined publication/restore, pending readers, every
one of1,356 partial-write positions, restore faults, allocator/guard failures,
CPU close/drain, copied owners/arenas, stale tokens and reentry. The actual trial
producer and strict decoder test accounting, malformed records, missing cases,
forged authority and incomplete native prerequisites. The Air7,2 native private
backing trial now passes all four cases in16,563us on source`23af85e2`, with
source-matched saved prerequisites, full preservation and clean return. Native
GPU mapping and completion remain unaccepted. Exact hardware scope belongs in
the [unit record](machines/macbookair7-2.md).

```sh
make -j2 all test-scanout-memory test-intel-memory build/intel-memory/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume VOLUME_SERIAL --intel-memory-fixture
python3 tools/hardware_boot_smoke.py --probe-check
# Separately freeze/rehearse the exact native candidate before installing:
python3 tools/hardware_boot.py stage --laptop-probe-volume VOLUME_SERIAL --intel-memory-capture
python3 tools/hardware_boot_smoke.py --probe-check
python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace
```

## Hardware reference boundary

Linux v6.12's [GGTT implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c)
keeps memory objects, address bindings, mapping policy and invalidation separate.
The pinned [managed-memory references](intel-managed-memory-plan.md#pinned-instruction-and-mapping-references)
describe why copying a PTE store/flush operation would omit required ownership
context. This implementation therefore adds no native encoding or invalidation
callback. New actual hardware operations require a separate source-bound audit.
