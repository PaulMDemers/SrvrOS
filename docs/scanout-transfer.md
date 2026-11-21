# Complete scanout address-range transfers

`scanout_transfer` is the expanded software transfer contract for an audited
graphics owner. It captures up to4,096 exact original entries in owned storage,
enough for two full padded Air buffers. The legacy16-entry `scanout_handoff`
observation and its native profile remain unchanged. Opt-in native diagnostics exercise its missing-authority refusal path; no native
provider supplies its required hardware assurances or imports an interval.

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

The provider and context stay bound through close and every retained fault until
reset. Three explicit phases keep
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
An enclosing native owner must enforce its remaining whole-lifetime budget;
starting a transfer validation phase or waiting for a child cannot restart or
extend that parent deadline.

The optional `scanout_transfer_check_clocked` check lets an enclosing owner
account **every** nested clock sample through its own wrapper of the same
admitted source clock. The transfer I/O table is never replaced; its100ms phase
limit, progress check and terminal retention remain. The legacy
`scanout_transfer_check` path is unchanged. The
[command mapping composition](intel-command-mapping.md) exercises both enclosing
and nested limits, including zero/backward/frozen clocks and exact boundary+1.

Original-object and monotonic pool-epoch tokens reject copied, stale and rebound
owners. Callback reentry refuses. `scanout_transfer_original` retrieves an exact
captured entry for a child restoration plan; this is data, not a fresh hardware
validation. The arena now borrows the transfer's immutable original vector and
uses the leased page index to compare each entry at publication preparation
and immediately before exposure. This catches a changed PTE even when a live
provider permits differences for other published children. A conflicting
value faults the arena, stops sibling publication and retains the parent
exclusion. Unexposed backing may still be released. Publication also checks
its restore vector against these originals before any restore store and
checks the readback afterward. Callers still bind real hardware checks to the
same maintained owner and source epoch.

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

Source`68489058` passes32 fatal ASan/UBSan cases and12 compiled fault controls,
the full local host/core/build gate (installed-root omitted),22-image isolation
and the matched normal-init/shadow-publication regression guest. A deliberately
broken deadline control exposed an initially conditional-only refusal fixture;
failure cases now require refusal explicitly, copied-owner tests rebind the
copied graph, and recovered provider/clock state cannot retry terminal faults.
All controls are detected after rerunning qualification; initial receipts are
retained. The implementation's stack frame stays below2,048bytes. The Air range diagnostic now links the reservation API.

### Reserving the second framebuffer interval

Both the 16-entry handoff and the full-frame transfer register their half-open
GPU page intervals before calling a provider. Independent epoch pools cannot
admit an overlapping range on the same aperture. A conflicting BAR or inherited
fence description also refuses. Adjacent ranges can coexist.

A native caller can reserve before obtaining hardware rights. The reservation
pins the display exclusion and retains an immutable copy of every original
entry. Promotion requires the same reservation token, source epoch, range,
original entries and a fresh provider admission. Only then can a child reserve
pages for publication. Clean close releases the interval after the last reader,
original translations, provider and exclusion pin have all been accounted for.
A lost owner keeps the range reserved. These transactions run serialized on the
boot CPU.

On Air7,2 the adapter exercises the reservation and its clean return. It still
requires an independent transfer from the owner of the GPU mappings before it
can publish new translations. The local flip fixture exercises the full lifetime,
including rollback and conflicts with both handoff APIs during retirement.
[Air7,2 status](machines/macbookair7-2-current.md)

The [original-source bridge](intel-startup-transfer.md) now supplies the
provider-facing registration between this owner and the original live source,
full private RAM holds and CPU exclusion. All transfer phases share the original
100/300ms watermarks. Models cover positive admission/draining/close and failures;
native remains an explicit zero-assurance/all255-obligation refusal before any
provider/transfer-clock/arena activity. Its separate v32 profile now accepts one native kind 2 registration/refusal/
check/close lifetime, with full source/RAM/root retirement on `f638ae7c`.
This supplies no positive interval assurance; enabled entry and real hardware
transfer remain unaccepted. See the [native result](intel-startup-transfer.md#native-original-kind-2-lifetime-accepted).

### Broadwell one-page checkpoint

The Air7,2 diagnostic store targets GGTT index 8,704, the first entry after
the 34 MiB inherited fence. Its input is one private linear RAM page below
4 GiB. The preliminary interval reservation must be promoted by the live
hardware owner before preparation. Publication and restore check the transfer,
child lease, saved entry, CPU mapping and original publication deadline. Under
the pinned root, the register must use a supervisor-only, writable, NX, 4 KiB
uncacheable mapping. The store compares the current 64-bit word immediately
before replacement and reads it back. The generic publisher then requires a
separate translation flush. On restoration it also requires retirement of the
readers before returning the private page.

The range diagnostic registers the verified boot framebuffer's entire fence
before reserving a candidate. This registration lasts for the boot. Both
handoff APIs may reserve adjacent pages with matching aperture and fence
geometry. An earlier conflicting lease prevents registration, and a later
caller cannot shorten the fence after the last candidate lease returns.
The Broadwell page writer checks the registered geometry independently of
the candidate's transfer manifest. The version 11 diagnostic records 8,704
protected pages on successful registration. Older receipts cover only the
checks implemented in their release.

`make test-scanout-transfer` runs this store against an aligned register
image. It checks both neighboring entries, independent cached translation
state, changes to the PTE during admission, alias and cache failures, and a
deadline expiring during either page-table walk. Failed translation
invalidation after the store retains the backing even when raw readback
matches. The boot image and read-only range observer do not call this store.

To exercise the physical aperture on the Air from Ubuntu:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror tools/intel_bdw_bcs_host.c -o /tmp/air-i915-page
timeout 15s /tmp/air-i915-page --aperture-page
```

This uses i915's private GEM object and GTT mmap interface. The CPU changes
and restores every word through the aperture, and GEM reads verify both
patterns. The driver chooses the GPU address and manages cache transitions.
The SrvrOS trial still needs independent evidence for ownership of its own
candidate interval, invalidation, and exact restoration.
[Air7,2 result](machines/macbookair7-2-current.md#reset-free-primary-plane-flip)

### One-page aperture lifetime on MacBookAir7,2

The Broadwell test writer pins the CPU aperture exclusion immediately before
its first store. Restoration checks the original guard, root, register alias
and publication epoch. Release follows successful memory close, which includes
restored-entry readback, translation completion and reader retirement. A failed
return leaves the pin held. The GT requester now uses the publisher's transfer
object to reserve this slot before inhibiting PCI bus mastering. The original
token, entry and outer display exclusion identify the reservation on both sides
of the GT reset. A lender can promote the same object in place. An early
original-entry mismatch leaves it reserved. A lost owner or failure during
external validation retains the exclusion in the faulted transfer. Version 2
range reports carry a slot receipt. Version 1 archives predate that reservation.

The Broadwell test writer takes a traffic provider with a live block generation
and guarded MMIO reader. The provider must have witnessed fresh engine and L3
acknowledgements while holding the block. Immediately before **each** store,
the writer checks all four media counters, all five engine states, GAM idle,
L3 unblock and the arbiter request/response permissions. Each MMIO read must
start and finish under the original block. The same generation must survive
translation restoration and reader retirement before releasing the aperture
pin. A native firmware/graphics lender and block provider are still required
to run this writer on the Air.

The Air's register-window guard derives three addresses from the captured
BARs and locked GGTT layout. It denies CPU mappings of the aperture page
and the physical table page. Before the first store, a whole-root reverse
walk verifies that the MMIO register page has precisely one alias with
the required permissions. While pinned, the public VMM operations refuse
other mappings of that page and removal, replacement or protection changes
at its virtual address. Unrelated register windows can still be used for
drain checks. On rollback, losing the block during the last reader callback
quarantines the mapping and retains the private RAM even if the original
entry has already been restored.
