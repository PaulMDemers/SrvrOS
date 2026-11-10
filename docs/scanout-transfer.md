# Complete scanout address-range transfers

`scanout_transfer` is the expanded software transfer contract for an audited
graphics owner. It captures up to4,096 exact original entries in owned storage,
enough for two full padded Air buffers. The legacy16-entry `scanout_handoff`
observation and its native profile remain unchanged. No native provider calls
this new contract or supplies its required hardware assurances.

The mechanism does not decide that a range is free. Its input must describe an
already transferred, non-display interval, its full inherited protection fence,
source epoch, endpoint geometry and every original raw entry. All five handoff
assurances must be present and all eight completion obligations must be zero
before any provider callback or range import. These fields are requirements on
a trusted provider, not independent hardware proof. Absent, scratch or populated
PTEs cannot establish a transfer; neither can the accepted BCS reset or private
RAM allocation trial. Original physical targets remain pinned by the provider.

## Capture and maintained ownership

The original zero-initialized object owns the complete vector. Allocate it in
heap/static storage, never a kernel stack. Open captures the small descriptor
and CPU-exclusion token, pins exclusion, copies every raw entry, then calls the
provider. Changing the caller's vector, descriptor or token during a callback
cannot change the imported range. Partial range snapshots are not accepted.
Input vectors, epoch pools and output tokens cannot alias the object or each
other's mutable storage.

The provider and context stay bound through close. Three explicit phases keep
initial/terminal original-state checks separate from live mapped ownership:

| Phase | Provider requirement |
| --- | --- |
| `TRANSFER_ADMIT` | Fresh same-boot source epoch, endpoint and complete original vector; firmware/engine/DMA/reader/cache ownership for this exact interval |
| `TRANSFER_LIVE` | Maintain those ownership conditions while known child mappings may differ from the original vector; do not infer quiescence from matching software tokens |
| `TRANSFER_RETIRE` | Every child has retired/released, every exact original entry is restored, and the provider's real hardware completion and ownership conditions still hold |

Each validation phase has a100ms monotonic bound, including admission vector
capture and arena creation. Clock failure, regression, timeout and no measured
progress refuse. This does not extend existing GPU100/300/500ms limits or make
the native preparation lifetime longer. A source epoch is a provider lifetime
identity, not permission to reuse an old reset receipt.

Original-object and monotonic pool-epoch tokens reject copied, stale and rebound
owners. Callback reentry refuses. `scanout_transfer_original` retrieves an exact
captured entry for a child restoration plan; this is data, not a fresh hardware
validation. Callers must still bind publication guards and the mapping provider
to the maintained transfer, including source/epoch and original-state checks.

## Drain, restoration and faults

Close first stops all new reservations. It waits for existing buffers' address
leases to drain before invoking terminal validation. Child publication guards
may continue checking the maintained owner while it is closing, so existing
buffers can restore and retire. A reader that is still busy makes child close
retryable without freeing RAM, writing restore entries or releasing exclusion.

An ownership/clock/provider failure during a live check or terminal validation
stops reservation admission and latches a terminal fault. There is no optimistic
retry or owner reuse after lost ownership. A partial mapping/restore failure in
the existing publication core quarantines the range and RAM and also prevents
transfer close. Only verified child retirement, exact original-state validation
and successful unpin release this owner's CPU exclusion pin. Other owners' pins
are preserved. Clean read-only admission failures unwind; poisoned exclusion
remains pinned until reboot.

This is a CPU0-serialized trusted-kernel mechanism, not an SMP lock, IOMMU, or
security boundary against arbitrary direct arena/PMM calls. Real hardware
providers must implement the entire contract; a synthetic callback returning
true cannot stand in for native firmware, cache, translation or DMA ownership.

## Validation and native boundary

`make test-scanout-transfer` exercises the actual transfer, buffer, publication,
range and CPU-exclusion cores together. Host models test full-vector admission,
the4,096-entry capacity and final aperture page, copied/stale identities,
mutable inputs, callback reentry, bounds/aliases, missing assurances and each
open obligation, source/clock/preservation failures, and clean admission unwind.
One-page and two concurrent1,356-page buffer transactions publish and restore
their exact original entries. Pending readers, uncertain stores/restores and
loss of the mapped owner keep RAM, ranges and exclusion pinned. These are
synthetic provider tests; they accept no native hardware operation.

The [Air7,2 record](machines/macbookair7-2.md) accepts the separate private backing
trial. The native provider still observes only16 candidate entries and supplies
zero hardware assurances. The next implementation must maintain a real
exclusive interval and capture its complete native vector, with original
physical allocations retained. It must then bind generation-specific encoding,
write/posting/invalidation and every reader's retirement before a one-page GPU
store. See the [managed-memory plan](intel-managed-memory-plan.md) and
[tear-free roadmap](intel-tearfree-roadmap.md). No new hardware trial is warranted
by this software contract alone.
