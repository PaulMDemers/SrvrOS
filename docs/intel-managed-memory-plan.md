# Air7,2 managed GPU memory after reset qualification

This is the implementation boundary after the [accepted bounded reset and
preservation trial](machines/macbookair7-2.md#accepted-full-census-retry-reset-and-preservation-complete).
It is a plan, not an authorization receipt or a native mapping implementation.
The current [tear-free roadmap](intel-tearfree-roadmap.md) selects the cooperative
driver-takeover route and orders the next composition and software work.
The inherited framebuffer remains the desktop backend. The current native
provider still supplies zero hardware assurances to `scanout_handoff`.

## What is established

The accepted source `f68ab030` completes an already-disabled, zero-geometry BCS
reset with fresh engine/display/HDA preservation and clean retirement. It does
not reset the render/video engines, submit a command, invalidate a translation,
flush a GPU cache or transfer a GGTT interval. A successful BCS-domain reset
must not clear all eight `intel_completion_obligation` bits.

The [GGTT survey](intel-ggtt-window.md#reference-native-result) observes populated
entries at index8704 and the next15 slots. The whole inherited34MiB fence stays
protected, including its unobserved tail. Neither the candidate window nor its
physical targets are an allocator. The accepted private-RAM experiment also
supplies no GPU-visible address or native transfer token.

The first follow-up integrated main `928555f3` as `15c13b15`. The handoff lifetime
correction subsequently integrates main `e85a6846` as `15f8dc84`, including Air6,1
PR433 sync diagnostics. Its host/VM qualification remains separate from physical
acceptance of source `f68ab030`; see the machine record for exact tested sources.
PR19 Pro11,2 Wi-Fi and PR379 WebCore remain independent. None supplies an Air7,2
native graphics ownership grant.

## Ordered implementation milestones

| Milestone | Required implementation and reviewable exit evidence |
| --- | --- |
| 1. Exclusive address-space owner | Define who transfers the exact GGTT interval, which GPU/firmware clients are excluded, and how that exclusion is maintained. Choose an explicit firmware transfer or an audited graphics-address-space takeover; populated/scratch/absent PTEs are insufficient. Keep all inherited physical allocations pinned. The native provider must continue refusing until this proof exists. |
| 2. One-page mapping transaction | On a genuinely transferred non-display interval, pin one private RAM page and the address lease, bind DMA address width/translation policy, CPU aliases/cache type and original raw PTE. Validate the generation-specific PTE write, posting and invalidation sequence before adding the first writer. First trial changes no plane, route, tiling or visible framebuffer. |
| 3. Command completion | If completion uses a BCS command, separately own the ring/context, status/completion page and their translations. Define a finite sequence, fresh nonzero completion value, ordering and retirement; prohibit submission through the inherited ring. Establish all render/video dependencies rather than treating BCS completion as device-wide. |
| 4. Restore and retire | Restore exact original entries, establish translation/cache and every reader's retirement, then release both RAM and address leases. A known write followed by an unknown result pins both leases. A timeout does not authorize replay, restore through uncertainty, or freeing. |
| 5. Full scanout buffers | Only after the single-page transaction passes, support two complete scanout allocations with verified stride, padding, tiling/swizzle and cache policy. The current16-page handoff cannot describe a1356-page inherited tiled extent. Expand the transfer contract and tests before attempting framebuffer-sized imports. |
| 6. Display publication | Bind surface programming to the owned buffers, preserved route/brightness/link and a real flip latch/old-reader retirement witness. Integrate with the existing publication/quarantine lifetime. Keep the inherited framebuffer available for verified restoration; then qualify interactive desktop use and endurance. |

The first milestone is a prerequisite to the first GPU PTE store. A command
flush cannot bootstrap ownership of the ring and completion memory needed to
execute that command. Do not make a synthetic positive-assurance callback the
native provider. Existing software leases remain useful for enforcing a proven
transfer; they cannot create the transfer.

## Completion evidence must remain specific

| Open bit | Evidence still needed before clearing it |
| --- | --- |
| Posted writes | A documented completion witness covering the particular writes and mapping cache policy; an unrelated CPU fence or MMIO readback is insufficient. |
| Prefetch | Retirement of work that may still use the transferred interval, with all relevant engines and address-space users covered. |
| Pipeline | A completed generation-specific operation with owned command/completion backing and its exact scope established. |
| TLB | Required invalidation and its completion after the appropriate prerequisites, for the changed translations and users. |
| Cache | Maintained CPU/GPU cache policy, permitted alias topology and ordered visibility/retirement for those buffers. |
| Flip | Proof the requested surface latched and prior readers retired; a frame counter or unrelated command completion is insufficient. |
| Firmware | Declared cooperation plus maintained exclusion of submissions/mapping/power changes over the entire ownership lifetime; stable OpRegion samples alone are insufficient. |
| Backing | Exact imported GPU interval and pinned private physical pages, with inherited/firmware allocations retained and checked. |

These bits are separate from the five coarser `HANDOFF_*` assurances. Document
how each actual provider derives and revalidates each assurance; do not translate
an all-zero engine census or reset success directly to `HANDOFF_ALL`.

## Pinned instruction and mapping references

Intel's [Broadwell Volume2a](https://cdrdv2.intel.com/v1/dl/getcontent/690783),
November2015, SHA256
`b035c834a6e1dce2c7057165f2157f8597748bd2b0ccf63a982e0fc4f2dc95b5`,
describes BlitterCS `MI_FLUSH_DW` on printed pages834–837. Its post-sync write,
address-space and invalidation rules must be considered together with the
owned ring/context and completion location. The command's flip caveat on
printed page825 means a completed flush cannot certify a completed display flip.
This is a possible future completion mechanism, not a command currently emitted
by SrvrOS.

The pinned Linux v6.12
[non-render flush emitter](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/gen8_engine_cs.c)
uses an owned request/ring and post-sync scratch location. Its
[GGTT implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c)
separates insertion, mapping policy and invalidation. In particular, inspect
`needs_wc_ggtt_mapping`, `gen8_ggtt_invalidate` and their callers together;
copying only a flush register or one PTE store omits required context. These are
implementation references, not evidence that this Air's old allocations or
candidate entries are available.

Reference downloads and their SHA256 inventory are in ignored
`build/air-memory-next-20261109/reference`. Further implementation must also check
applicable Broadwell workarounds and the existing [completion prerequisites](intel-completion-prerequisites.md).

## Qualification and physical trial boundary

Keep ordinary startup, older diagnostics, exact device/firmware matching and
native gates unchanged until a separately reviewed provider exists. Host tests
must exercise the actual transaction's copied/stale owner, mapping, clock and
uncertain-store failures. Object/staged-image checks must bind actual addresses,
write widths/values, source and image isolation. VM evidence tests refusals and
persistence; it cannot accept native memory ownership.

Each new native operation needs its own exact recipe, immutable staged artifact,
in-guest progress, saved prerequisite checkpoint, bounded execution and strict
collector. Preserve existing diagnostic deadlines; do not lengthen reset or
owner limits to disguise a slower new operation. Capture completion separately
from grants. Saved safe outcomes return to Ubuntu; retained uncertainty halts
visibly. Keep volume1%, no interactive input for automated trials, and the
accepted reset payload/recovery until replacement qualification is complete.

## Software prerequisite: handoff lifetime enforcement

The [handoff core](intel-handoff.md#software-handoff-lifetime) now seals ownership
to the original object, pins admission validation, captures input storage, binds
the provider and rejects reentry. Closing stops new reservations before checking
retirement; copied owners and alternate callbacks cannot release the pin. This
is necessary groundwork for milestone1, not its native transfer proof. The
populated candidate interval remains unavailable and no hardware assurance bit
changes. Host cases exercise the actual handoff/space/exclusion implementation;
they do not substitute for the future native provider.

## Accepted native prerequisite capture

The separate [cold-takeover preflight](intel-takeover-preflight.md) reads all five
reset controls and GDRST with a fresh same-parent/epoch engine census. It addresses
a gap left by BCS-only qualification before choosing a broader takeover sequence.
Its completed readout and sampled quiescence remain separate from any reset or
GGTT transfer authority; this does not complete milestone1.

Air7,2 source `d860b63f` / release `b8afda22fe3f051e` now accepts that bounded
read-only capture: two identical zero reset-control/domain samples after a
complete stable census, verified borrow/parent return and alias retirement.
See the [accepted observation](intel-takeover-preflight.md#accepted-air72-observation).
It narrows the sampled-state uncertainty; posted-write/prefetch/cache/display
retirement and an explicit exclusive interval remain unproved. No milestone1
GGTT grant or mapping writer is accepted.

The isolated [all-engine prepare/hold/cancel trial](intel-engine-prepare.md)
now has one accepted bounded maintained readiness interval under a registered borrow
on source`ee4a15bc`:98,274/100,000us, fresh zero geometry, ten exact acknowledgements,
raw engine/display/HDA preservation, both borrows returned and13 aliases retired.
The1,726us margin is narrow and does not guarantee repeatability. The next adapter
work targets duplicate nested-census validation while retaining full provider
boundaries and every deadline, before considering additional reset work.
It must preserve all inherited backing and return after exact engine/display/HDA
comparison. It adds no GDRST, PTE, command or memory-transfer capability and does
not itself complete milestone1.

## Reset integration boundary after preparation qualification

Air7,2 has separate bounded passes for disabled-entry BCS reset/preservation
(source`f68ab030`) and five-engine prepare/hold/cancel/preservation
(source`292163ac`,93,246/100,000us). They cannot be concatenated as a maintained
ready interval: the latter explicitly cancels every request and returns its
borrow. A broader reset must obtain fresh readiness and keep its original
registered borrow through reset, settling, cancellation and preservation.
It must advance the owner epoch before any possible reset store, reject stale
censuses and preserve completed prefixes on uncertainty.

The immediate implementation step is the existing BCS-only adapter's
[provider-boundary integration](intel-bcs-reset.md#native-provider-boundary-integration),
with its generic core and exact disabled-entry/domain8 gate unchanged. This
reduces redundant full admission while preserving raw observations; it does
not turn an old preparation result into reset permission. Before a broader
writer, review each engine/domain and reset side effect against pinned PRM
register definitions and upstream ordering, define post-reset controls rather
than assuming ready3 survives, and qualify the whole single-lifetime path
within existing budgets. Do not stretch deadlines to compose separately tested
operations. No all-engine reset or GGTT store is introduced in this step.

After reset, a stable empty census still does not retire outstanding DMA,
prefetched requests, translation caches, firmware/SMM clients or display
readers. The reset register's non-posted completion applies to that register's
operation; it is not a witness that earlier memory writes reached their target.
An owned ring/status/completion location is required before using a command
as a cache/TLB completion witness. The candidate populated GGTT window and
its physical targets remain inherited allocations, not an available arena.
Therefore all eight completion obligations (mask255) and the zero-native-
assurance gate stay intact.

The provider-boundary BCS retry now has a matching native pass on source
`4873e6bd`:61,212/100,000us core,207,311/300,000us owner and279,649/500,000us
outer, with full engine/display/HDA preservation and clean retirement. The
[reset contract](intel-bcs-reset.md#native-provider-boundary-acceptance-2026-11-09)
records the exact-source evidence and already-disabled-only scope. This closes
that adapter qualification step; it does not close any of the eight ownership
obligations, qualify enabled shutdown or authorize a broader reset/GGTT writer.

## Next requester prerequisite: conventional PCI Advanced Features

The reference Air7,2's GPU exposes MSI90/PMd0/AFa4, with AF TP/FLR capabilities,
not a PCIe Device Status capability. The new isolated
[PCI requester capture](intel-pci-requester.md) observes that chain, bus-master
command and AF control/status twice under an original registered GT borrow.
Raw read completion and TP/BME masks stay separate from comprehensive DMA
completion, firmware exclusion and interval ownership. No configuration/reset
writer or positive hardware assurance is added. Native capture is now accepted
on source `fe6677c5`, release `3ff42f33e6c01391`:27,036/100,000us, both snapshots
match, BME enabled and TP clear, borrow returned and10 aliases retired. This is
one read-only capture, not DMA drain or an interval transfer. The next block
composes maintained preparation and an exact reviewed action under one original
lifetime; separate cancelled receipts cannot provide that authority.


The [native held requester capture](intel-held-requester.md#native-combined-result)
now accepts one read-only composition under the original preparation borrow:
source `8559ac44`, release `67a178af16c8983f`,96,728/100,000us with cancellation,
preservation and clean return. Its explicit provider witnesses and lossless
receipts create no reset or GPU interval authority.

The separate [reset-capable composition](intel-engine-held-reset.md) is now
locally/VM qualified: payload `d22194cd`, release `a25250e9a1f62f9e`, qualified
harness `41bb435f`. It owns the epoch change and mixed post-reset control state
under the same original borrow, retains the exact BCS8-only gate and original
bounds. The first native run completes both pulses, mixed controls, preservation
and clean retirement but misses the whole preparation deadline at103,702/100,000us;
it remains a timing nonpass with no native combined acceptance. All eight ownership obligations remain
open. GPU range transfer, owned command/mapping completion and reader retirement
remain distinct reviewed gates; neither a reset ACK nor PCI TP clear supplies
those assurances.

The next held-reset timing retry is source`8b91de5b`, release`b63d89044865e7a8`.
A fresh CPU translation walk shares ancestors inside each excluded endpoint
comparison while checking every leaf; no census or ownership check is removed.
Host/native-image/VM qualification passes, but timing acceptance remains pending.
This CPU page-table observation grants no GGTT or GPU-memory authority.

Its first native run (`session-00065fe36794a009-00`) refuses the excluded
inherited backing/window revalidation before any wake/reset write or mapping.
It returns cleanly with prior files preserved, but produces no preparation
timing measurement. The exact failed admission predicate remains to be captured;
no native optimization or ownership acceptance is added.

The next bounded diagnostic is source`e0ab9800`, release`b726c22e43838e36`:
one original first-failure witness, retained through cleanup and printed after
exclusion. Host/image/VM qualification passes. Its native run now records the
entry-context refusal: two registered address spaces (`phase=1 reason=7 value=2`)
before walking the framebuffer range. No wake mapping/write or reset occurs.
Next qualify diagnostic startup ordering to establish the existing sole-root
precondition; retain root/occupancy checks and all deadlines. This evidence
neither accepts combined timing nor adds GPU-memory authority.


The pre-user-init combined retry is now qualified in source`9c788f3f`,
release`8d3839aaa57f707d` (main`486b6d2a`). Its isolated main defers user init
only for the exact terminal held-reset recipe; ordinary-image service startup
and diagnostic save/unmount/return/halt paths pass separate VM regressions.
Original sole-root/count/occupancy guards, all preservation work and100/300/500ms
bounds remain. One native trial will exercise the full maintained preparation,
PCI capture, BCS8 reset and cancellation lifetime. Native timing, all255
completion obligations and managed GPU-memory/tear-free acceptance remain open.
