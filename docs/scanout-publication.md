# Leased scanout publication and restoration

`scanout_publish` is a CPU0-serialized transaction over an explicitly granted
[scanout address lease](native-scanout-reservations.md). It stores an immutable
copy of a proposed entry vector, snapshots the original vector, publishes only
after exposure, and restores the exact prior vector before address retirement.
There is **no native GGTT caller or hardware I/O implementation**. The new guest
fixture uses private arrays, not device registers. Host/model success grants no
GPU visibility or physical presentation acceptance.

## Storage and input contracts

`scanout_linear_storage` calculates a bounded 32-bit linear CPU footprint with
64-byte pitch alignment and overflow-safe page rounding. Air1440x900/pitch6144
uses5,529,600 bytes /1350 pages. This is distinct from the inherited X-tiled
1356-page extent; the whole inherited34MiB fence stays protected. This helper
admits no display format, tiling, cache policy, DMA address or native mapping.

One transaction stores up to1536 complete64-bit entries in each before/after
vector. This is a bounded internal diagnostic mechanism, not a desktop window
or process limit. Large objects belong in stable storage, not on kernel stacks.
Preparation authenticates the complete reserved address token, owner, arena
state and bounds. It copies callbacks, lease identity and requested entries;
entry vector and output token cannot alias the object. The callback context and
physical RAM remain pinned through retirement, or through reset after a fault.
The entry values are opaque: an audited native encoder still must validate
physical/DMA addresses, PTE format, cache policy and every RAM pin.

A monotonically increasing pool epoch binds object/owner tokens and blocks
same-owner reuse of stale work. Reset/lower pools cannot reopen a used object;
exhaustion refuses. Caller operations are serialized on CPU0. Busy state is
published before callbacks, so callback reentry cannot apply, cancel, restore
or replace the same transaction. There is no hidden allocator, IRQ mask, GUI
lock, background receiver or PMM release.

## Publication sequence

1. Prepare under the owner guard: snapshot every old entry without exposing the
   lease. Failed preparation remains unexposed/cancelled and can be discarded.
2. Apply: recheck the guard/lease and every old entry. Changed originals refuse
   before any write; never replace another writer's new state with stale data.
3. Expose the address lease **before the first write callback**, including one
   which returns failure or may have written only part of a word.
4. Write the requested vector. Each attempt revalidates the lease. Complete
   posted writes and relevant translation retirement through the adapter's
   `flush` contract, then compare every readback and recheck the guard.
5. Only the complete sequence reaches Published. A display owner must not arm
   this address range before that state. Requested vectors are copied, so later
   producer edits cannot silently change an in-flight plan.

The flush callback is a hardware-specific proof obligation. A CPU barrier or
matching PTE readback is insufficient evidence that translation caches retired.
The [pinned Linux GGTT implementation](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/gt/intel_ggtt.c)
separates entry writes and invalidation, with behavior depending on mapping
policy. Intel's [Broadwell Memory Views manual](https://cdrdv2-public.intel.com/690798/intel-gfx-prm-osrc-bdw-vol-05-memory-views.pdf),
pp44–46, describes the Gen8 GGTT; neither source alone provides this kernel's
ownership, cache, aperture mapping or completion implementation. No native
write width, cache policy or invalidate recipe is selected by this core.

## Restoration and failure

A Published transaction stays pinned while `readers_retired` reports busy.
Restore returns without a write and can be retried. True must mean independently
confirmed route completion and retirement of all readers, not a fixed delay,
a sampled frame counter, a cancellation request or a matching software token.
The adapter must exclude new readers throughout restoration.

Restore verifies the current vector still equals the published one. It then
writes every exact original64-bit value, completes posted writes/translations,
compares all originals, checks the guard and reconfirms reader retirement. Only
then does it call address retirement with all three proof bits: original entries
restored, translations retired and readers retired. It releases no address or
RAM itself. The address owner and separate RAM lifetime must both permit release.
Closing an arena prevents publication but permits an already published range's
clean restore/retirement and final release.

Any uncertain write, flush, readback, guard, identity or clock outcome after
exposure latches Fault and quarantines the address lease. There is no automatic
rollback/retry after that fault. Retain RAM, vectors and context until an external
reset: even a failed first callback may have queued a partial write. Unrelated
safe leases retain their existing release rules. Prepared cancellation performs
no I/O; it is unavailable once exposed.
Cancellation only establishes that this transaction did not publish entries.
A native adapter must separately retire any temporary CPU mappings, pins or
other context it created while preparing; this core has no callback-context
cleanup or RAM-release operation.

Phases check monotonic time, a100ms deadline and positive elapsed progress; loops
have fixed1536-entry caps. Callback implementations must themselves be bounded;
the core cannot interrupt a hung callback. Each phase has its own origin, so a
long valid interval between publication and a later restore is not mistaken for
a publication deadline. No clock observation establishes hardware retirement.

## Validation and remaining native work

The host model separates visible entries, queued posted writes and a translation
cache. An independent whole-arena oracle checks protected8704-page fence and
unrelated mappings, publication, exact restore and translation state. Coverage
includes every callback failure in a four-page lifecycle, partial failed writes,
all clock positions, corrupted reads and successful-but-wrong writes, stale and
foreign tokens, repeated use, busy readers, close, reentry, copied input vectors,
epoch exhaustion, alias refusal and a1536-page transaction. Failed exposed work
cannot release; zero-write preparation failures can discard safely.

The explicit `--intel-lifetime-fixture` version2 guest also runs two software publication
banks: a complete32-page publication/busy-reader/close/restore/release and an
injected first-write failure retaining32 model pages. Its strict receipt requires
65 attempted writes, two flushes and explicit model/no-hardware/no-DMA fields.
It adds no physical GGTT writes or real GPU completion to the existing PMM shadow
trial. The exact version2 VM harness requires the receipt; version1 fixture payloads
retain their original checks and configuration. Ordinary/native paths do not
run this model. A model callback's promise is not native hardware proof.

Before connecting a writer: establish a boot-scoped device owner and managed
native arena; preserve every inherited mapping/fence and reserved physical page;
admit private layout/DMA/cache policy; audit native posted-write/translation
completion; then integrate exact plane completion and inherited-route restore.
The accepted [native read-only DMA observation](macbookair7-2-dma-observation.md#native-observation-accepted-2026-11-04)
removes an inventory uncertainty, but supplies none of those grants by itself.

## November 4 software checkpoint

Exact code `4fb8fb33` passes the host whole-arena oracle, ASan/UBSan and four
compiled negative controls (missing exposure, stale translations, missing restore
readback and missing quarantine). Version2 Q35 fixture release
`0ddd22b8f7fdc62e` produces the complete model receipt; missing workspace visibly
halts. Ordinary VirtIO validates normal exit, separate compositor/supervisor
death, console recovery and five presenter acquisitions. The session-owner
cancellation regression found during integration is fixed without changing
explicit graceful revoke or releasing live resources. Host and VM evidence
remain distinct from native mapping and physical presentation acceptance.
