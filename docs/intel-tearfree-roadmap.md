# Air7,2 native tear-free desktop roadmap

The next milestone is a boot-scoped Broadwell display driver that can bind
one private page while the inherited framebuffer stays live. The driver must
establish ownership before that first bind and keep it through rollback.
Native presentation can use CPU-rendered buffers and synchronous Pipe A
flips. GPU command submission has its own bring-up sequence.

This roadmap orders the work in the [managed-memory plan](intel-managed-memory-plan.md).
The [Air7,2 unit record](machines/macbookair7-2.md) owns physical acceptance;
the [coordination guide](agent-coordination.md) governs shared implementation.
It does not accept another Mac's firmware or graphics profile.

## Boot ownership decision

SrvrOS currently approaches the first bind as a loan of firmware's populated
address space. The native observer reserves an interval and records the original
entries, then intentionally leaves firmware, engine, DMA, cache and reader
assurances unset. The transfer needs those assurances before activation. The
single-page writer and seven-page command handoff also require the native owner
to request a device-wide traffic block and witness five engine ACKs and a fresh
L3 ACK before modifying inherited entries. The GPU command path needs those
same mappings to load its own context, so it cannot supply the first owner's
flush evidence. These gates are appropriate for that live loan. Making the
loan mandatory for the first framebuffer left a gap in boot ownership.
[Native observer](../kernel/src/drivers/intel_stolen_probe.c)
· [Transfer admission](../kernel/src/scanout_transfer.c)
· [Live slot writer](../kernel/diagnostics/intel_bdw_scanout_page_store.c)

Intel describes `GO_GAM_REQ` as power-manager-to-arbiter messaging and the
L3 acknowledgements as self-clearing when the power manager samples them.
The reference Air reported the same locked CPD policy under Ubuntu and
SrvrOS. Its RC1e request read back while GAM and WiDi stayed open in every
sample. The public register manual and Linux driver paths reviewed for this
decision expose no OS interface that holds that transaction open for a GGTT
loan. Extending the poll window would only widen observation of the same
request. [Broadwell register reference, printed pp. 587–592, 606–607 and 859](https://cdrdv2-public.intel.com/690789/intel-gfx-prm-osrc-bdw-vol-02c-commandreference-registers.pdf)
· [Physical RC1e result](machines/macbookair7-2-current.md#current-air72-work-on-pr-301)

Booted Ubuntu on this Air shows `simpledrm` giving way to i915 on `8086:1626`,
with the internal 1440 × 900 eDP panel enabled. In the reviewed Linux v6.12
source, probe sanitizes the GT with an engine reset where that reset preserves
display, takes over the framebuffer aperture, and reconstructs the initial
plane as a pinned buffer. The Gen8 bind path writes the allocator's exact GGTT
entries and then orders translation invalidation. Unbind synchronizes with use
of that mapping and checks its pins. These paths contain no device-wide GAM/L3
acknowledgement prerequisite for the bind. Gen8 also uses a no-op for the
generic clear-range callback, so this is no precedent for sweeping or clearing
the firmware's entire table. The running Ubuntu driver and reviewed v6.12
source are separate references. In three earlier i915 comparisons on this Air,
the driver rebound slot 12291 and restored the original PTE while eDP advanced.
The sampled GAM and WiDi permissions stayed open, with engine and L3 flush
ACK bits clear. Those runs started with i915 already owning the device.
[i915 early reset](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/i915_driver.c#L184-L192)
· [Aperture takeover](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/i915_driver.c#L452-L482)
· [Initial plane](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_plane_initial.c#L127-L243)
· [Gen8 binding](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c#L408-L472)
· [Gen8 callbacks](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c#L1157-L1174)
· [Mapping retirement](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/i915_vma.c#L1969-L1995)
· [Air slot comparison](intel-bdw-primary-flip.md#independent-slot-comparison-under-i915)

Implement the boot-owned path in this order.

1. **Take ownership while Pipe A stays live.** Before desktop clients start,
   one driver claims graphics MMIO and the CPU framebuffer aperture. It
   imports and pins the original backing, translations, fence and fetch tail,
   excludes competing writers, and performs the already qualified bounded GT
   reset and new census with the original scanout preserved. Keep render
   submission disabled. The same owner must establish what earlier traffic can
   still reach the candidate range through the remapper, GT, display proxy or
   firmware. Bind the existing remapper and System Agent operations where their
   documented scope applies. Queue idle, a reset receipt, GOP handoff and
   sampled register stability each supply only their own evidence. Fail
   admission if the source or continuing ownership of any relevant reader is
   unresolved. [Native guard result](machines/macbookair7-2-current.md#current-air72-work-on-pr-301)
   · [System Agent ordering](intel-ggtt-invalidation.md#hardware-scope)
2. **Bind and restore one private page.** Choose one allocator-managed interval
   outside every protected active extent. Prove that no previous or future
   requester can reference it during the test. The raw PTE value alone cannot
   make that decision, including a zero entry or a repeated scratch mapping.
   Write one entry to pinned OS RAM, order and invalidate the translation,
   verify a marker through the aperture, restore the original word and verify
   translation retirement before freeing RAM. Keep Pipe A on the original
   surface and keep the existing inherited-slot gates. Test refusal at every
   boundary, especially an active proxy, alias loss, stale translation and
   uncertain rollback. [Existing GGTT exchange](../kernel/diagnostics/intel_bdw_ggtt.c)
3. **Flip CPU-rendered buffers.** Map two full buffers in the driver-owned
   range, including the required alignment and 128-PTE fetch tail. Render
   into the inactive buffer in the captured format, establish visibility,
   and arm the synchronous `PRI_SURF` update. Verify live-base latch, frame
   progress and primary CRC, then restore the original surface and prove
   reader retirement. Keep backing pinned on any uncertain latch or return.
   This uses the display engine directly. [Existing flip and CRC trial](intel-bdw-primary-flip.md#reset-free-hardware-trial)
4. **Connect the presentation backend.** Reuse the current compositor lease
   and front/back lifetime. Qualify repeated physical flips and input under
   load, console restoration, and fallback when admission fails. Add blitter
   or render contexts later, with their own queue, cache and completion proofs.
   [Desktop presentation contract](display-presentation.md)

The one-page transaction in step 2 passed the host and Q35 models. It
requires the original, private and restored aperture markers in order. A PTE
readback alone cannot retire the lease. The native owner must preserve Pipe A
and verify frame progress while it lends the exact slot, old backing,
private RAM and uncached aliases across both stores. Wire the native producer
at the pre-PCI-reopen queue boundary. Use the existing CPU-root, engine and
remapper guards for their proven scope. Account separately for outstanding
traffic to the slot and continuing proxy and firmware exclusion before giving
the transaction write access. `ExitBootServices` ends firmware boot services
and device-handle protocol calls. Use that boundary for the boot-services
producer and establish coverage for any platform firmware access separately.
[UEFI ExitBootServices](https://uefi.org/specs/UEFI/2.10_A/07_Services_Boot_Services.html#efi-boot-services-exitbootservices)
[One-page transaction](../kernel/diagnostics/intel_bdw_boot_scanout.c)
· [Air qualification](machines/macbookair7-2-current.md#current-air72-work-on-pr-301)

The native pre-reopen checkpoint now records the exact PTE, the original
Pipe A registers, the boot CPU SMI count and the WiDi clock, GO and TLB state
while the five submission ports are sealed. Its observed state identifies
what a live provider must cover. It grants no write access. A maintained
proxy exclusion and evidence of retirement for earlier GPU accesses remain
required before the one-page writer can attach to this checkpoint.
[Pre-reopen checkpoint](../kernel/src/drivers/intel_gt_boot_page_trial.inc)

## Native rollback admission after the Air checkpoint

The Air's pre-reopen packet shows an occupied slot, empty WiDi translation
vectors, no WiDi invalidation in progress, a gated WIN clock, and open
GAM and WiDi permissions in the same guarded interval. The five queues
matched after reset, and the VT-d drain and System Agent invalidation
completed. These observations establish the original state from which a
new drain must start. They leave completion of earlier GT/WiDi traffic and
the ability to block new traffic unproven. [Native Air checkpoint](machines/macbookair7-2-current.md#current-air72-work-on-pr-301)

1. Acquire a documented request under the existing boot owner, before
   reopening PCI bus mastering. The request must keep the relevant GT
   and WiDi clients closed while Pipe A still fetches from the inherited
   framebuffer. Check the request, matching `GO=0` replies, fresh engine
   completions, and a fresh L3 acknowledgement in that same held interval.
   The saved reset or clock-gate state supplies no opening edge. Intel
   identifies GPM as the sender and the GAM register as the response.
   The previous RC1e request restored cleanly with traffic still open.
   A post-reset read of the responder cannot acquire a lease. [Broadwell
   register reference, pp. 587–592, 815–816, 859](https://cdrdv2.intel.com/v1/dl/getcontent/690789)
2. Preserve both producer yields through the two stores. The display WD
   fuse prevents enabling the wireless video function. Keep the wireless
   audio path disabled under the existing display HDA owner and pin the
   exclusive programming pages. Keep the separate WIN/GAM device hold
   even with those producers sealed. Bind the firmware side to the exact
   interval and the completed UEFI handoff. Keep firmware-call paths out of
   the transaction, retain the SMI change check, and require a platform
   contract for any runtime producer that can still reach the range.
   A stable SMI counter is evidence for the observed boot CPU and window.
   [Broadwell display fuse, pp. 497–498](https://cdrdv2.intel.com/v1/dl/getcontent/690789)
   · [UEFI ExitBootServices](https://uefi.org/specs/UEFI/2.10_A/07_Services_Boot_Services.html#efi-boot-services-exitbootservices)
3. Exercise that owner in a no-PTE-store trial first. Hold it while checking
   the original surface and the candidate, then verify clean release with
   both still intact. For the subsequent one-page bind, keep the same owner
   through PTE replacement, posted translation invalidation and the private
   aperture marker, then through restoring the original and observing its
   marker. `WTCR` is usable after earlier WiDi traffic is drained. Keep the
   range and its backing pinned if either translation or ownership becomes
   uncertain. [WiDi TLB control, p. 1823](https://cdrdv2.intel.com/v1/dl/getcontent/690789)

The no-store diagnostic now exercises this lifetime in the host suite and
Q35. It borrows the exact page reservation and display exclusion, checks
the current owner and open baseline, and requires fresh closed traffic
before returning the provider's ticket. After return it checks that
traffic has reopened and that the original PTE and live plane still match.
The provider owns the actual request, checks the pre-reopen boot context
and retains the firmware, proxy and cache-policy guards. The saved Air
baseline refuses before any request. [Diagnostic core](../kernel/diagnostics/intel_bdw_boot_drain.c)

The occupied-slot path borrows one exact page from its original registered
firmware interval. It can qualify the hold with zero PTE writes, or keep the
same loan and traffic ticket for one private bind and exact rollback. It
waits for fresh engine and L3 acknowledgements before the first store and
checks the WiDi and GAM hold around each store and invalidation. Before
returning the ticket it verifies the restored PTE and original aperture
markers while Pipe A remains on the inherited scanout. A stale translation
or lost owner keeps the loan, both backings and display pin live. A verified
return releases the child and keeps the original firmware interval reserved.
The captured Air `GO_GAM_REQ=0x7fff` still refuses before requesting the hold.
The physical test needs a native provider with a verified acquire, hold and
release sequence. [Occupied-slot core](../kernel/diagnostics/intel_bdw_boot_drain.c)

The separate-range entry point borrows an exact slot from a registered
driver interval and requires an exactly zero PTE before requesting a hold.
It keeps the loan and the inherited display pinned through the fresh
five-engine and L3 acknowledgement checks and through reopening. It
rereads the vacant entry and original plane under both the closed and
reopened states. A successful report records all five engine bits, the
fresh L3 acknowledgement and a clock observation after the request. The
no-store trial returns the ticket and leaves the range unseeded. The later
bind must acquire its own live hold.

For an occupied driver interval that aliases the inherited framebuffer,
the no-store entry point requires the exact first framebuffer PTE and
borrows its slot with the original display pinned. It checks the same
PTE and live plane while the request is held and after traffic reopens.
A competing reset reason invalidates the reset-free transaction, even
after all five engine acknowledgements and the L3 edge have been seen.
Refusal before issuance returns the slot. Uncertainty while the request
is held prevents its release and retains the slot and framebuffer pin.
[Boot drain verifier](../kernel/diagnostics/intel_bdw_boot_drain.c)

The held bind fixture exercises that next transaction from a vacant slot.
After a fresh hold, it seeds scratch, binds the private page, and restores
scratch through one original range loan and one traffic ticket. Each store
has a verified translation update. A matching scratch aperture marker and
readback precede release, and the original panel and scratch PTE must still
match after traffic reopens. Missing L3, a lost ticket after the private
store, or stale translation on rollback all retain the loan and recovery
state. Missing L3 stops before the first PTE store. The native provider
must still establish exclusive hardware control and pin actual RAM.

The native pre-reopen sampler reads `MISC_CTRL0` through the guarded
power-register alias before and after the PTE, WiDi and Pipe A packet.
This checks whether the contemplated CPD route can send GO messages while
the same owner protects the original surface. The captured Apple policy
sets the lock and clears CPD GO messaging. The other published message
registers record GPM/GAM requests or device acknowledgements. A CPU
write to one of those messages would require a documented ownership and
release protocol before it could serve as the provider.
[Air native request evidence](machines/macbookair7-2-current.md#current-air72-work-on-pr-301)

The Broadwell initialization paths in coreboot and U-Boot both program
`MISC_CTRL0` with CPD GO messaging off and set ECOLOCK during graphics
initialization. Intel specifies that FLR leaves this policy locked. U-Boot
also refuses its low-level Broadwell initialization after a prior EFI
boot. The boot application on the reference Air already inherits an active
GOP, so moving the same policy operation from the kernel to that application
does not put it in charge of early graphics initialization.
[Intel register reference, pp. 606–607](https://cdrdv2-public.intel.com/690789/intel-gfx-prm-osrc-bdw-vol-02c-commandreference-registers.pdf)
· [coreboot initialization](https://github.com/coreboot/coreboot/blob/a13e296046cfe95e6cdca9303a03a752aa520c67/src/northbridge/intel/broadwell/gma.c#L139-L155)
· [coreboot lock](https://github.com/coreboot/coreboot/blob/a13e296046cfe95e6cdca9303a03a752aa520c67/src/northbridge/intel/broadwell/gma.c#L221-L229)
· [U-Boot initialization](https://github.com/u-boot/u-boot/blob/ece349ade2973e220f524ce59e59711cc919263f/drivers/video/broadwell_igd.c#L209-L220)
· [U-Boot lock](https://github.com/u-boot/u-boot/blob/ece349ade2973e220f524ce59e59711cc919263f/drivers/video/broadwell_igd.c#L293-L305)
· [U-Boot EFI entry check](https://github.com/u-boot/u-boot/blob/ece349ade2973e220f524ce59e59711cc919263f/drivers/video/broadwell_igd.c#L660-L678)

The documented driver-to-PCODE mailbox command list contains no GAM hold.
The PCU block-request status bit is unused by GT cores. `GFX_PAUSE` controls
execution-unit and sampler clock transitions. These interfaces supply no
maintained hold that can be tied to fresh engine and L3 acknowledgements.
Keep the occupied-slot no-store test pending a documented GPM request
and release interface, or a platform firmware contract that exposes one.
[Intel PCIe register reference, pp. 23 and 31–33](https://cdrdv2-public.intel.com/690865/intel-gfx-prm-osrc-bdw-vol-12-pcie-config-registers.pdf)
· [Intel register reference, pp. 1476–1477](https://cdrdv2-public.intel.com/690789/intel-gfx-prm-osrc-bdw-vol-02c-commandreference-registers.pdf)

The GT's IDI path also exposes a request and a status. The Broadwell
register reference describes `IDIMSG[6]` at `0x8500` as a CPD-flow
request, and `MSG_GARB_MBC[4]` at `0x801c` as the interface's block
status. The reference supplies no CPU acquire, maintain and release
sequence for a GAM hold or its five engine and L3 handshakes. A fresh
read-only comparison on the Air under Ubuntu/i915 held driver forcewake
for eight bracketed samples. `IDIMSG` stayed zero, `MSG_GARB_MBC` stayed
at `0x0c` with IDI awake and unblocked, and GAM remained open at
request `0xff` and response `0x7f`. All five engine flush acknowledgements
and both L3 acknowledgement bits stayed clear. `MISC_CTRL0` remained
`0xc5280000`. This is an observation of the powered interval under i915.
[Broadwell register reference, pp. 544–545 and 801–802](https://cdrdv2-public.intel.com/690789/intel-gfx-prm-osrc-bdw-vol-02c-commandreference-registers.pdf)

If the occupied interval cannot be transferred, the boot allocator needs
an interval explicitly yielded before any firmware or device client can
use it. The existing Air capture identifies preserved mappings and reserved
RAM. Slot 12292 still requires its own transfer. Use an explicit ownership
contract to qualify any new candidate. The first independent checkpoint is
a one-page bind and rollback under the new owner with the original Pipe A
surface pinned. Its acceptance needs a named source for the range, exact
translation invalidation and proof that every possible user of that range
is excluded or accounted for through retirement. This is the driver-owned
path described under [Boot ownership decision](#boot-ownership-decision).

### Driver-owned range qualification

The takeover registers the exact page granted by the boot driver and verifies
an empty PTE while the panel is dark. It keeps the same loan across display
recovery, scratch publication, bind and rollback. The inherited framebuffer
fence and imported firmware ranges remain reserved. The page writer requires
fresh proof from the same driver, root and reset epoch, with its pinned scratch backing and
the original programmed and live Pipe A surface. It observes scratch, private
and restored scratch markers through the same aperture alias, with posted
translation invalidation after each store. Clean rollback returns the page
loan and leaves the driver range registered. Uncertain translation retirement
keeps the loan, range, both backings and display exclusion pinned.
[Range registry](../kernel/src/scanout_space.c)
· [One-page writer](../kernel/diagnostics/intel_bdw_boot_scanout.c)

The Q35 fixture established the driver range and its scratch translation
before calling the writer. On the Air, the native aperture survey found all
53,760 eligible entries populated, and the EFI handoff grants no GPU address
range. The physical trial needs a native takeover or firmware yield that
specifies a disjoint interval and excludes earlier and future GPU users of it
before the driver installs the initial scratch translation. The later
scratch-to-private-to-scratch trial tests that lifetime with Pipe A active.
[Air aperture survey](machines/macbookair7-2-current.md#reset-free-primary-plane-flip)
· [EFI handoff importer](../kernel/src/drivers/intel_air_boot.inc)

The native boot-range checkpoint samples GGTT slot 16385 (`0x04001000`)
after reset and same-epoch memory maintenance, with all five engine
submission ports sealed and PCI bus mastering still off. It takes two
high/low/high readings through temporary read-only UC aliases, retires both
aliases, and rechecks the inherited Pipe A surface, reset owner and SMI
counter within the existing 100 ms phase. The `intel-driver-range-preflight`
record carries the exact words and closing status. An unstable or incomplete
sample keeps bus mastering off. A stable zero records the empty encoding at
that boundary. A stable nonzero preserves the occupied entry. Native range
registration still requires proof that every reader is excluded or retired
through the subsequent bind and rollback.

Air7,2 release `22098cdc89f8d227` read `0x8c001001` at slot 16385 in
both pre-reopen samples. The same-boot backing guard identifies the mapped
physical page as the first page of the inherited framebuffer. The offset is
outside the inherited fence, but the translation still aliases live scanout
backing after GT reset and memory maintenance. Preserve that entry and keep
the range ungranted. The next bind checkpoint needs a driver-owned interval
established during takeover with the inherited framebuffer pinned and the
old requesters for that interval retired.
[Physical result](machines/macbookair7-2-current.md#current-air72-work-on-pr-301)

The first-publication transaction takes an existing grant and requires an
exact zero PTE inside it. It verifies the page under the same live Pipe A
owner, seals the parent range until reboot, and installs one pinned scratch
mapping. It reads the aperture only after ordered invalidation and checks the
scratch markers and closing PTE before returning the child loan. A nonzero
entry refuses before the first store. If a write or translation is uncertain,
the child and display pin remain held. The next loan uses the verified scratch
PTE as the baseline for private bind and rollback. The zero check is a
publication precondition. The current EFI sample still supplies no grant.
[Scratch publication](../kernel/diagnostics/intel_bdw_boot_scanout.c)

## Earlier qualifications

The read-only CPU backing guard has been qualified for two existing 6 MiB
candidates. The earlier original full-GT reset, memory and private CPU arena
lifetime remains separate evidence. See the
[current Air record](machines/macbookair7-2-current.md).
The mapping core now supports the selected practical timing aliases in host
qualification. A separate [primary-envelope classifier](intel-bdw-primary-range.md)
accounts for tiled padding,128 extra display translations and the full protected
fence using raw observations. Neither is a native ownership provider. The next
new native operation needs a documented outstanding-memory completion witness
and maintained firmware/all-reader exclusion before the exact-range transfer;
replaying or rebooting unchanged positive observations will not fill that gap.

The [private BCS submit step](intel-bdw-bcs-submit.md) now has a bounded
four-DWORD protocol and actual joined flush/nonce tests. The old MMIO sync-flush
shortcut is reserved on Broadwell, so it supplies no bootstrap mechanism.
The new host code still needs an independently initialized/owned context and
mappings; no queue sample, issued packet or nonce can create that prior authority.
Native startup memory-completion/reader ownership remains the next decision.

The [private CPU context-image trial](intel-bdw-bcs-context.md) now prepares
and verifies the complete byte image in the existing original-owner arena, with
post-retirement erasure and exact cleanup. This separates CPU construction from
actual GPU mapping, visibility and execution. Its own wire8/image/recipe retains
all255 obligations. One original Air7,2 CPU image lifetime now completes on
hardware (source4ce676bd/release08ae); exact source/evidence and the still-open
GPU ownership boundary are recorded in the linked contract.

The [primary-A flip protocol](intel-bdw-primary-flip.md) now implements the
separate synchronous arm/live-latch sequence with incremental bounded polling.
A queued surface, posting read or advanced frame alone cannot complete it.
Host qualification covers retained failures and late latch, but no native image
calls it. Actual mapping/visibility ownership and old-reader retirement remain
independent prerequisites; this is not native page-flip acceptance.

## Capability-sized trial plan

The next acceptance target is one complete **private one-page GPU mapping
lifetime**, not another collection of successful register observations. The
[current Air status](machines/macbookair7-2-current.md) separates CPU maintenance
already accepted from the still-open device obligations. Use the
[durable workflow](device-workflow.md) to freeze each candidate and collect it.

| Required part | Evidence required before advancing the original owner |
| --- | --- |
| Ownership | Maintained firmware/requester/engine exclusion tied to the actual source, epoch and exact range; every unknown keeps the grant refused |
| Completion | Actual outstanding DMA/posted-write/prefetch, GPU translation/cache and every remaining display/firmware reader have bounded completion witnesses; clear endpoints alone do not retire them |
| One-page transfer | Original entry captured, private backing held, encoding/cache policy checked, admitted store and required hardware visibility/completion confirmed |
| Publication | First private publication and any reader leases remain attached to the same owner; no reuse while any consumer remains |
| Restoration | Stop publication, drain consumers, restore exact original entry with required completion, retire aliases/holds and verify inherited display and USB/recovery integrity |
| Failure | Every partial store, missing ACK, stale identity, timeout and failed restoration retains the original exclusion/backing; no false success or free/retry |

Implement and host-test the whole protocol before scheduling the mapping trial.
Use pure-core tests, actual adapter fault points, mutation controls and fatal
sanitizers. Batch compatible subphases in one boot only when each live prerequisite
is established before the next action, with the **original deadlines unchanged**.
Never turn separate boots into a live ownership token. Where a required hardware
witness is still unknown, design and batch the discriminating read-only evidence
first; do not repeatedly boot an unchanged positive observation.

Freeze all required jobs and exact payload once, qualify it in QEMU, then run the
single bounded native question. Buffer compact source-bound timing, route,
failure and retirement data; persist after safe ownership return instead of
printing inside the critical interval. Save raw data before classification on
Ubuntu. Accept the whole capability only after strict original-chain, media and
recovery review. A retained unsafe outcome still halts; safe saved/unmounted
outcomes return automatically. No new GPU action is enabled by this plan.

The [Broadwell TLB protocol](intel-bdw-tlb-protocol.md) now implements and
host-tests the downstream request/self-clear sequence, with fresh mandatory
pipeline/memory proofs and original clocks. No native caller or image is added.
The next native decision remains the actual quiescence mechanism and its
memory-retirement guarantee; the current observations cannot supply it.

## Established boundary

| Native evidence | Accepted scope | Still excluded |
| --- | --- | --- |
| BCS reset provider integration, source `4873e6bd`, release `6365de27e20576b6` | Already-disabled BCS entry, bounded reset/preservation and clean retirement; core61,212/100,000us | Enabled shutdown, other reset domains, device-wide DMA retirement |
| Five-engine preparation, source `292163ac`, release `0cafc3a5551ab1b2` | Bounded prepare/hold/cancel and preservation;93,246/100,000us | Maintained readiness after cancellation, authority for a later reset |
| PCI AF requester capture, source `fe6677c5`, release `3ff42f33e6c01391`, session `session-00065fdf108c49b4-00` | Two matching nine-DWORD snapshots, BME enabled in both, TP clear, FLR control0;27,036/100,000us, borrow returned and10 aliases retired | Comprehensive DMA drain, firmware exclusion, cache/translation/reader retirement, memory transfer |
| Requester capture inside maintained preparation, source `8559ac44`, release `67a178af16c8983f`, session `session-00065fe0b883f61e-00` | Original preparation borrow, five ready requests, two matching PCI snapshots, cancellation/preservation/return;96,728/100,000us,13 aliases retired | Reset-capable action semantics, repeatability, all memory/completion obligations |
| Maintained preparation/PCI/BCS8 reset, source `9c788f3f`, release `8d3839aaa57f707d`, session `session-00065fe55d829da0-00` | Exact pre-user-init recipe; full reset/cancel/preservation/return in97,414/100,000us, wake280,135/500,000us;13 aliases retired | Repeatability, wider reset scope, all memory/completion obligations |
| Private backing lifetime, source `23af85e2`, release `d72619c4e02bb721`, session `session-00065fe6c815e551-00` | Four cases in16,563us; two disjoint1,356-page buffers, full patterns, CPU borrow drain, unknown-range refusal and one deliberate private-page quarantine; durable checkpoint, preservation and clean return | GPU interval import, native mapping/encoding, hardware cache/translation completion, real flip/reader retirement |
| Original GGTT entry inside held reset, source `3315df79`, release `e07848a7c00d893d`, session `session-00065fef0f0829d7-00` | Six read-only DWORD loads in59us under the original active post-reset borrow; original entry/source epoch bind, preparation97,379/100,000us, full preservation and14 aliases retired | Exclusive interval transfer, hardware/cache/DMA/firmware/reader assurances, PTE writes and page flips |

These are separate exact-source lifetimes. They cannot be combined into a
current owner or completion token. The [requester contract](intel-pci-requester.md)
records the accepted native capture; PCI AF TP clear is an observation, not
proof that all posted writes reached memory. Bus mastering remains enabled.
All eight completion obligations remain open (mask255), native hardware
assurances remain zero and no managed GPU pages are accepted.

The [post-reset BCS re-arm scope](intel-engine-rehold.md) is now accepted in
source`f2a6c116`, release`e83571c874bc17f1`, session
`session-00065ff078d4f70a-00`:358us scope/234us child, exact mixed-state
restoration,97,758/100,000us preparation and14 retired aliases with full
preservation. It closes no memory/completion obligation. The new
[source-bound interval claim](intel-interval-claim.md) registers its own child
dependent and owns opening/closing original-entry observations. Its software
lifetime and refusal path supply no native address transfer or positive assurance.

The joined claim now passes on Air7,2 source `e81caf7f`, release
`978d9be94b96243a`, session `session-00065ff636174005-00`: preparation
91,213/100,000 µs, HDA command phases 6/2/2/6, both original-entry observations
and clean alias/owner retirement. The next software layer registers an interval
consumer before callbacks and blocks early claim/HDA closure. Its conservative
one-page Broadwell codec calculates an entry for a private physical address
below 4 GiB; it performs no store or transfer. Actual interval ownership and
mapping visibility/completion remain the next native boundary.

The separate [complete transfer core](scanout-transfer.md), source`68489058`,
now supplies4,096-entry owned manifests and joined child drain/retirement with
host fault/sanitizer qualification. It preserves the legacy native16-entry
observer and has no native provider. The next boundary is genuine maintained
interval ownership and a one-page mapping, not another synthetic capacity trial.

The separate [original-owner HDA/BCS diagnostic](intel-hda-bcs-shutdown.md) now
accepts already-disabled observation on source`a720d774`, release
`bd77ce8cff00a4a3`, session`session-000660022da620b1-00`. Census and BCS entry
agree, full preservation and HDA6/2/2/6 pass, and GPU11/HDA1 aliases retire in
307,103/500,000us outer lifetime. BCS attempts/stores0 leave enabled shutdown
unexercised. This does not identify why an earlier requester boot saw enabled
entry, or supply a reset/claim/memory grant. Next compose legal entry/shutdown
and reset admission under the same original owner, retaining all existing
geometry, source, epoch and deadline checks; do not synthesize an enabled case
or repeat unchanged observation boots as a substitute for that integration.

The separate [disabled-entry/reset composition](intel-entry-reset-composition.md)
now qualifies a sealed zero-store child before the unchanged joint reset path.
Its original census age, epoch and borrow return remain mandatory. Host/VM
qualification is complete. Native source0fadb55b/release0994cd9762acfca1 now
refuses enabled inherited BCS before its child or reset enters; full native
composition remains unaccepted. The next step is an explicit source-bound
enabled-ring ownership transition with fresh epoch/census and valid nonzero-
geometry reset semantics, preserving the old gates. An unchanged retry, geometry
clearing or a saved-observation token cannot supply that route. No memory or
completion grant is added.

The [enabled-ring transition](intel-enabled-ring-transition.md) is now a separate
v24 hardware-ready candidate (`c43d30d0` / `811ff18f86cf3d17`), with host/VM
qualification complete and native acceptance pending. It uses the original HDA/GT
owners, the captured enabled ring, one guarded disable, a fresh full census at
an advanced epoch and exact engine/reader preservation. Its first native
milestone ends with disabled submission and clean retirement. Reset from that
nonzero geometry still needs a separate contract; no old reset gate is widened.

The first native v24 trial (`811ff18f86cf3d17`) instead observes already-disabled,
zero-geometry entry and safely refuses before its enabled-only child/write.
HDA and all mappings retire within bounds; this does not accept enabled shutdown.
These contrasting captures motivated explicit original-owner routes for both
naturally observed entry states, preserving old geometry/epoch gates.

A separate [state-aware entry contract](intel-ring-entry.md) has host/VM qualification
for both captured sources and native acceptance of its already-disabled route. It keeps enabled one-store/epoch
advance distinct from disabled NULL-writer observation, requires fresh full
post-census and reader preservation on both, and ends before reset. Both old
recipes and geometry gates remain unchanged; neither observed completion nor
host modeling accepts a physically unexercised enabled writer.

## First decision: how ownership begins

Before another destructive native operation, choose and document a complete
ownership route against generation-specific references and the captured profile.
The route must answer:

1. Who transfers the exact GGTT interval and which physical allocations remain
   pinned? The populated16-entry candidate and the protected34MiB inherited
   region are unavailable until that transfer is established.
2. Which render, video, blitter, display, firmware and other requester clients can
   still access or change that interval, and what maintains their exclusion?
3. What hardware operation completes earlier accesses, with what exact scope and
   postconditions? BCS reset acknowledgement or PCI TP clear cannot stand in for
   missing posted-write, prefetch, cache or translation evidence.
4. What can still be restored safely after each possible store, and when must
   memory, mappings and exclusion remain pinned instead?

The selected design route is an **explicit cooperative OS graphics-driver
takeover**, extending the [existing cooperative EFI policy](intel-gt-access.md#opt-in-cooperative-efi-wake-contract).
Conforming firmware yielding the OS requester is a declared platform assumption,
not something inferred from stable mailboxes or an assertion that SMM is absent.
There is no captured firmware service that grants a spare GGTT interval. The
native implementation still needs to maintain the expanded ownership lifetime;
the current bounded wake policy alone does not transfer graphics memory.

The now-accepted composition binds the real preparation/requester/reset
cores in one lifetime: fresh engine census; five reset requests held; requester
observation while that hold remains active; exact reviewed reset action; fresh
post-reset state; reverse cancellation; full engine/display/HDA preservation and
retirement. The original preparation API completes cancellation before return;
the separate maintained-action/reset API supplies the required live boundary. A cancelled preparation
receipt cannot serve as that boundary. Epoch invalidation must precede any
possible reset store, and old censuses must become unusable.

The first software piece is the separate
[maintained read-only action boundary](intel-engine-prepare.md#maintained-read-only-action-boundary).
It brackets an observation with original-borrow guards and matching held-state
samples inside the unchanged100ms budget. It has no reset permission. The
standalone requester observer acquires its own borrow and cannot be nested
unchanged. The [held requester sampler](intel-held-requester.md) explicitly
binds the active preparation borrow; its isolated native provider now accepts
one exact-source combined capture and clean return, with3,272us preparation
headroom. This answers whether requester observation can fit within the maintained
hold and existing budget. A reset-capable composition remains separate because it
changes epoch and control-state semantics.

The fresh CPU translation retry (`8b91de5b`, release`b63d89044865e7a8`) retains
all censuses and bounds. Its host/image/VM qualification passes; native combined
timing remains pending. Its first native run refuses excluded inherited
backing/window revalidation before wake/reset access; exact predicate attribution
is the next diagnostic. It grants no GPU range or completion assurance.

The next qualified diagnostic (`e0ab9800`, release`b726c22e43838e36`) retains
the original first failed predicate and query context without a second probe.
Its host/image/VM checks pass. The native row identifies an entry-context refusal:
two registered address spaces before the range walk (`phase=1 reason=7 value=2`).
System init precedes this diagnostic. Next qualify startup ordering to establish
the existing sole-root context without relaxing root/occupancy gates or any
deadline. No wake mapping/write/reset occurs and combined timing remains pending.

The complete composition needs host fault injection at every transition.
It must preserve the existing BCS-only writer gate until a separate review
settles wider domain semantics. Broadwell GDRST full-reset bit0 is distinct from
per-engine bits1/2/3/4/7. The pinned Linux reset path prepares engines, resets,
then cancels, and repeats reset with50us settling for delayed register stability.
Do not widen domain8 to0x9e or assume readiness bits survive reset. See the
[pinned reset references](intel-engine-prepare.md) and
[reset contract](intel-bcs-reset.md). That planning block selected no broader native reset writer. The separate
[held BCS reset composition](intel-engine-held-reset.md) now implements the
BCS8-only epoch-aware lifetime, with a distinct native image and qualification
gates. Its first native run completes the reset helper and clean preservation/
retirement but misses the preparation deadline at103,702/100,000us. The combined
lifetime remains a timing nonpass; no memory transfer is accepted.

Before staging that composition, record the exact native action, reference-backed
register/memory effects, per-obligation evidence and failure-state ownership.
Any unresolved completion/ownership dependency retains refusal. A synthetic
positive-assurance provider would not establish transfer. A hold-plus-requester
observation-only trial is warranted only if its result resolves a specific
composition decision, not merely because both operations exist separately.

The next isolated native prerequisite is the
[original-claim GPU requester hold](intel-gpu-requester-hold.md): exact PCI
Command 7→3, bounded AF pending-clear observation and 3→7 restoration while the
original interval consumer and HDA/engine owners remain live. It adds no address
or completion assurance and must qualify independently before being used by a
future memory provider.

## Ordered delivery blocks

| Block | Implementation | Observable exit gate |
| --- | --- | --- |
| 1. Ownership protocol | Encode the chosen route as a single original-object, same-epoch transaction; bind provider, resources, interval and clocks. Preserve raw attempted/completed prefixes and retained state. Keep unrelated ordinary startup unchanged. | Actual-core fault tests cover copied/stale objects, reentry, clock expiry, provider loss, every store boundary and retirement refusal. The reviewed native protocol identifies the witness for each claimed assurance; unsupported prerequisites refuse. |
| 2. Native ownership qualification | Implement only the exact reviewed operation and native profile, with its own immutable recipe, checkpoint and collector. Any all-engine preparation needed for reset must remain live through that reset and its cancellation/preservation path. | Matching hardware evidence proves the intended operation and its scope, full display/backing preservation and verified return, within existing bounds. A clean reset alone does not close the memory obligations. |
| 3. One-page GPU mapping | Lease a transferred non-display GPU interval and pin one private RAM page. Bind DMA addressing/translation, CPU aliases/cache policy and original PTE. Implement exact PTE write, posting and required invalidation/completion sequence. | The page is the only changed translation; exact readback and documented visibility/completion pass. No plane, route or visible framebuffer changes. Unknown completion retains both address and RAM leases. |
| 4. Owned command completion | Where required by the selected route, own ring/context/status/completion backing and implement a finite generation-specific command sequence with a fresh nonzero completion value. This block moves earlier if mapping/retirement depends on it. | Completion is tied to the submitted request, original mappings and exact scope. Stale completion values, timeouts and unrelated engine completion cannot release backing. Never submit through an inherited ring. |
| 5. Restore and retire | Restore original translations only while ownership remains established, complete the required translation/cache/reader retirement, then release address and RAM leases. | Success and injected failure paths have explicit ownership outcomes. A known store followed by an unknown result cannot free, replay or optimistically restore memory. Native trial verifies preserved original display and clean retirement. |
| 6. Full scanout buffers | Connect the existing complete4096-entry transfer contract to a real native owner, then own two full buffers with checked stride, padding, layout/tiling, swizzle, DMA and cache policy. | Host overflow/extent/alias/lease tests and native buffer validation pass. The legacy16-entry observer is not a full native import. No truncated import is accepted. |
| 7. Flip publication | Connect owned buffers to surface programming, latch detection and old-reader retirement while preserving link, route and brightness. Integrate [publication](scanout-publication.md), [buffer lifetime](scanout-buffer-lifetime.md) and [reservations](native-scanout-reservations.md). | A real latch witness identifies the displayed buffer; the previous buffer remains pinned until its readers retire. Failed/uncertain publication quarantines backing. A frame counter alone is insufficient. |
| 8. Desktop qualification | Enable the native backend as an explicit candidate, retain verified recovery, and exercise drawing, resize/drag, window churn and I/O/audio overlap. | Automated repeated flips and teardown, saved screenshots/timings and endurance pass; attended moving-content checks verify absence of visible tearing and normal interaction separately. Only then consider default enablement. |

Blocks3–5 are one ownership problem, not independent permissions. Their order
must follow the selected hardware protocol. In particular, no completion command
may be used to justify the memory ownership it already requires.

## Work that can proceed in parallel

- Audit the hardware bootstrap route and its exact generation-specific evidence.
- Audit generic handoff, address-space, buffer and publication lifetimes; implement
  bounded software fixes and actual-core failure regressions independently of
  positive native assurances.
- Maintain the milestone ledger, collector failure classification and acceptance
  documentation without editing another device's evidence.

Only the integration owner builds, stages or runs a VM harness in this checkout.
Integrate shared changes before preparing one candidate. Neither parallel work
nor a host test changes the installed native release's acceptance.

## Validation and stop conditions

Host tests establish software lifetime/order/failure behavior. Object and staged
image checks bind the real native addresses, permissions, widths and writer
isolation. Exact-stage VM trials establish refusal, persistence, safe automatic
return and missing-workspace behavior. None proves native GPU ownership.

Each physical candidate needs a matching model/profile, immutable source/release,
recovery, saved prerequisite checkpoint, in-guest progress and strict final
collection. Keep the existing100/300/500ms bounds for the current dependent,
owner and outer protocols; do not extend them to conceal a composition failure.
A distinct future protocol needs its own reviewed timing contract rather than
silently changing these receipts. Keep volume1% for automated trials.

Stop before issuing a new operation if its ownership, exact resource interval,
cache policy, completion scope or restoration safety is unresolved. After a
possible store, unknown ownership/completion retains resources and halts visibly;
a timeout is not permission to free them. Safe handled outcomes with verified
save/unmount return automatically to Ubuntu. Missing final markers, retained
state, incomplete raw coverage and failed persistence remain nonpasses. Keep
failed evidence and backups outside disposable build artifacts.

The next ready-for-hardware checkpoint must name the operation it will prove,
which obligations it can actually close, and what remains open. Do not schedule
another observation-only trial unless it resolves a specific protocol decision.


The pre-user-init combined retry is now qualified in source`9c788f3f`,
release`8d3839aaa57f707d` (main`486b6d2a`). Its isolated main defers user init
only for the exact terminal held-reset recipe; ordinary-image service startup
and diagnostic save/unmount/return/halt paths pass separate VM regressions.
Original sole-root/count/occupancy guards, all preservation work and100/300/500ms
bounds remain. One native trial will exercise the full maintained preparation,
PCI capture, BCS8 reset and cancellation lifetime. Native timing, all255
completion obligations and managed GPU-memory/tear-free acceptance remain open.


That pre-user-init native trial now passes in97,414/100,000us, including the
reset and complete preservation/return. The initial collector omission for the
healthy zero-code witness is fixed and reviewed against identical raw logs;
original classification is retained. This closes the bounded composition gate.
The next implementation is the exact interval/private-memory/completion owner
in the managed-memory plan, with no GPU PTE store before transfer is established.


The next hardware-ready checkpoint is source`23af85e2`, release`d72619c4e02bb721`:
[joined backing ownership](scanout-memory.md), full padded sizing and allocator/
CPU-borrow/range/publication retirement are qualified in host and guest tests.
The isolated native trial exercises two full private buffers and expected
unknown-range refusal, with no GPU writer or grant. Its PMM/frame-tag/CPU mapping
and quarantine accounting resolve the new backing-adapter qualification boundary;
it does not repeat or replace the accepted shadow publication. Exclusive GGTT
transfer, owned completion and real buffer/flip publication remain open.

## Current original-owner entry/reset result

Air7,2 source `9e67bce7` / release `74d46f42447d218e`, session
`session-0006600e477bf0c3-00`, accepts the bounded same-owner kind 2
zero-geometry entry/reset/re-arm/claim/preservation path. Original full initial
census, four read-only BCS8 samples, fresh full post-census/readers, source and
provider/epoch binding, registered returns and clean GPU/HDA/exclusion retirement
verify. Entry stores remain zero/epoch 0; the existing reset advances epoch 0→1.
Entry is 90,760/150,000 us, handoff 31,895/100,000 us, preparation
91,213/100,000 us, HDA 318,012/400,000 us and outer 368,571/500,000 us.
All deadlines remain unchanged; exact raw checkpoint, all prior files, offline
allocation and Ubuntu recovery pass. This one bounded pass does not establish
repeatability or native tear-free presentation. See the
[composition contract](intel-state-aware-reset.md) and [unit record](machines/macbookair7-2.md).

The entry/reset composition milestone is complete for this exact source and
observed route. Main `7c1a08a0` is integrated afterward with affected local
checks; the installed native payload remains frozen `9e67bce7`. Nonzero geometry
and enabled entry writing remain separately unaccepted. Next is the remaining
firmware/DMA/cache/translation/reader retirement and exclusive GPU address/RAM
ownership protocol, then managed backing, actual command completion where
required and real flip publication. Blocks 3–7 above remain open. A reset alone
does not close those obligations: mask 255, assurances 0 and managed pages 0.
Keep full original live-object/deadline rules; no geometry clearing, forced
enable, saved-receipt authority or unchanged observation/reset retry. No new
trial is armed.

## Current entry/requester trial: safe nonzero-geometry refusal

The v27 trial on Air7,2 source `cef27b1c` / release `127a222d11cc79fc`
verifies the naturally enabled kind 1 entry writer, fresh post-census/readers,
original return and clean HDA/GPU retirement. Its reset guard then refuses the
nonzero geometry, leaving preparation/reset/requester unentered. Full completion
is **not** accepted. The [composition result](intel-entry-requester.md#native-result-enabled-entry-completed-resetrequester-safely-refused)
and [unit ledger](machines/macbookair7-2.md) retain `completed=NO`, all alerts and
independent safe file/recovery proofs. Previous kind 2 zero-geometry reset
acceptance remains unchanged. A separate nonzero-geometry ownership/epoch protocol
is needed before this route can continue; no geometry clearing, forced state,
old-gate widening or unchanged retry is planned. Managed memory/real flips remain open.

## Current buffer ownership integration

The [original startup source](intel-startup-source.md) now accepts one native
already-disabled route, while its enabled route remains unaccepted. The next
[private-buffer binding image](intel-startup-buffers.md) connects original RAM
holds to that same live reset/source lifetime, without substituting a saved
receipt. Two full padded buffers must stay pinned through source, reset and
outer cleanup, then verify and release exactly once. Host/model evidence is not
native timing acceptance; this still grants no GGTT interval or GPU PTE writer.


The [v31 original-source buffer trial](intel-startup-buffers.md#native-result-original-already-disabled-route-accepted)
now passes one native kind 2 lifetime on runtime876b500e/releasec154. Both full
private buffers stay pinned through actual outer retirement, then pass full
patterns and release exactly once; preparation98,198/100,000 us. This closes
that route's RAM/source binding prerequisite only. Exclusive GPU address/range
transfer, completion/cache/reader ownership and actual GPU publication remain
open; no managed GPU pages or tear-free presentation are accepted.


## Current original-source transfer result and next ownership step

Source `f638ae7c`, release `8d0181c0ed0a4708`, now accepts one native v32
kind 2 source/buffer/transfer registration and known-authority refusal lifetime.
The transfer child is 45 us; preparation is 97,996/100,000 us and all original
source/root/alias holds retire, followed by full patterns and exactly-once RAM
release. The [unit result](machines/macbookair7-2.md#native-v32-original-source-transfer-lifetime-accepted)
records strict raw/checkpoint, media and recovery proofs. All five hardware
assurances remain absent and all eight completion obligations remain open.
No further identical refusal trial is needed for this completed prerequisite.

The next bounded implementation is a **maintained original-source requester
hold**, followed by an actual interval/completion proof. The existing
`intel_gpu_requester_hold_run` belongs to the older interval-claim graph and
restores Command 7 before returning. Its receipt cannot authorize a child of
the newer state-aware source, nor supply requester inhibition during mapping.
Do not transplant its claim pointer, use a receipt from another boot, or treat
Command 3/AF pending-clear as device-wide DMA completion.

1. Define an original one-use hold beneath the real state-aware source/buffer
   registration. It must retain the exact route, source/reset epoch, original
   provider, RAM holds and CPU exclusion. Its inhibited state must enclose the
   intended child and prevent parent return; restoring state follows child
   retirement, rather than preceding it.
2. Reuse the exact captured PCI capability/profile checks and Command-halfword
   7→3→7 operation semantics. Bind every full profile, pre-store read, issued
   prefix and pending sample to this original hold. Scope the expected Command
   change to the original native owner only. Lost source, ambiguous stores or
   unknown retirement retain the full chain and never replay restoration.
3. Enforce the original preparation 100 ms, GT 300 ms, HDA 400 ms and outer
   500 ms origins throughout. The latest 2,004 us preparation margin is measured
   once, not spare capacity promised to new work. Qualify batching or admission
   changes against actual guard/source/map/clock fault controls before adding
   the operation. Preserve each required full validation and final store clock;
   do not extend deadlines, skip witnesses or add an unbounded poll.
4. Exercise both naturally observed entry routes in actual-core/model tests,
   plus every read/store/clock/child/return failure. Native enablement needs a
   separate exact image/recipe/strict receiver, source-matched VM persistence,
   capacity and recovery checks, then one bounded physical trial. Its result
   accepts only the observed route and exact control/retirement scope.
5. After maintained requester control, specify the real cooperative firmware,
   engine/prefetch, translation/cache, inherited backing and display-reader
   witnesses for the proposed interval. These are independent requirements;
   neither this hold nor the BCS reset clears them. Only their reviewed live
   composition may reach the first one-page PTE transaction.

Steps 1–4 above are now implemented and one native enabled-entry lifetime is
accepted in the [v33 maintained requester result](intel-source-requester.md#native-result-original-enabled-entry-lifetime-accepted):
source `a7c84ce5`, release `6f8fea336425b123`, session
`session-0006602d3e4e59ec-00`. Original kind 1 entry0→1/reset1→2, both RAM
holds, transfer and CPU/root registration enclose PCI7→3→3→7, the inhibited child
and full closing validation. Requester scope is658us, child38us;
preparation98,884/100,000us, complete preservation and exactly-once RAM release,
strict raw/checkpoint and media/recovery proofs pass. Disabled v33 entry,
pending-drain branch and repeatability remain unaccepted. This closes the
requester-control lifetime prerequisite, not the actual interval/completion
requirements in step5. All255 obligations and zero positive assurances remain.

Main4016 is now integrated. Subsequent v34 all-five readiness holds and v35
original-source PCI/cache/reader observations are qualified as recorded in the
[unit history](machines/macbookair7-2.md). Latest source `4b4b4d5d` /
release `4751b92fb87ad132` accepts one kind 2 version3 requested-CPU-alias lifetime:
four full 1,356-page observations use actual PAT0/WB, writable/supervisor bits and
four 2 MiB leaves per span. Raw NX bits are clear. Diagnostic mappings/holds/RAM
retire cleanly and all prior files/recovery preserve. This neither donates whole
large leaves nor proves other aliases, coherence or any hardware completion bit.

Next bind exhaustive aliases/coherence and the actual step5 DMA/prefetch/cache/
translation/firmware/display-reader witnesses before a GPU PTE writer. The latest
preparation is 99,693/100,000us, leaving only 307 us
in one run; measure/reduce overhead before adding excluded work. No deadline is
extended, no full guard skipped, and no repeatability or timing cause is claimed.
A completed requester/readiness hold, clear AF status or saved reset receipt
must never be promoted to internal graphics/display DMA or cache retirement.
No new trial is armed. Ordinary desktop still uses the inherited framebuffer;
all 255 obligations, zero managed GPU pages and native tear-free remain pending.

The specialized VMM follow-up source `634ab653` /
release `5a204843df11446b` now passes one enabled-entry kind1 original lifetime,
including entry0→1/reset1→2, receipt3 requested mappings and full diagnostic/
media/recovery retirement. Preparation remains 99,679/100,000us,
with only 321us margin. Different routes prevent a
controlled comparison with the earlier4b kind2 run; no useful new native
headroom or repeatability is claimed. Next isolate full-admission/control and
census costs with bounded source-bound evidence before adding ownership work.
All checks/deadlines and all 255 hardware obligations remain; no trial is armed.

Air7,2 runtime42b17c5f now accepts one v35 kind2 span-admission/census timing
lifetime:93,392/100,000us preparation,6,608us margin, complete original retirement
and file/recovery preservation. The56 preparation census sample envelopes cover
83,843us and overlap control brackets; repeated census work remains the dominant
measured cost. Same-route4b was99,693us (one sample per source, no causation or
repeatability claim). No checks/deadlines were removed or extended. See the
[current unit result](machines/macbookair7-2.md#native-v35-span-admission-and-census-timing-accepted).
Next use this baseline to bound outstanding DMA/cache/translation/firmware/reader
protocol work. All255 actual ownership obligations and zero managed GPU pages
remain; ordinary desktop still uses the inherited framebuffer. No trial armed.

Air7,2 runtimeb10517bd/release3c9c4bfc2b632cc4 now accepts one original kind2
[v35 whole-root CPU alias inventory lifetime](machines/macbookair7-2.md#native-v35-whole-root-cpu-alias-inventory-accepted).
Two scans each visit110 logical table paths/56,320slots/47,369present leaves;
each1,356-page private span has only its expected present mappings. Preparation
94,806/100,000us leaves5,194us; original diagnostic retirement and all1,524prior
files except metadata preserve, with three exact logs and clean offline recovery
(1,527files/165sessions). Banks survive distclean; USB unmounted, no run/one-shot
outstanding. Fresh main4016 is integrated; PR19/379 remain independent. This is
point-in-time sole-root page-table evidence, with no stale-TLB/SMP/GPU/IOMMU,
DMA/cache/reader ownership or repeatability grant. Next establish remaining
completion/exclusion witnesses before GPU interval/PTE/submission authority.
All255 hardware obligations/zero managed GPU pages/native tear-free pending;
no sibling acceptance and no checks or deadline extensions.

Air7,2 runtime2bde9498/release3807a53c3829ba9c now accepts one original kind2
[private CPU translation/writeback lifetime](machines/macbookair7-2.md#native-private-cpu-translation-and-writeback-accepted):
2,712 local target invalidations,173,568 line operations and fourfences complete
in12,205/100,000us before GPU acquisition. The original held observations and
full retirement pass, with GPU preparation94,782/100,000us (5,218us margin),
exact2,712page release/DMA57→57 and offline file/recovery preservation
(1,530files/166sessions). Banks survive distclean; USB unmounted and no trial
or one-shot outstanding. Main4016 remains integrated, PR19/379 independent.
This accepts the targeted CPU operation for that lifetime only; all255 actual
GPU/DMA/prefetch/cache/translation/firmware/display-reader obligations and zero
assurances/managed GPU pages remain. Next establish device-side completion and
reader exclusion/retirement before GPU interval/PTE/submission authority.
Enabled route for this source, repeatability and native tear-free remain pending.

The [disabled-state graphics-unit VT-d drain protocol](intel-vtd-drain.md) is now
host-qualified against the prior captured CAP/ECAP. It deliberately avoids
graphics remapping activation and cannot satisfy a flushed GPU pipeline, cache,
prefetch or reader witness. Native integration must join its admitted command
alias, registered original source child, raw/strict receipt and clean retirement
in one bounded opt-in trial. No observation-only reboot or ownership-bit change
is scheduled from this host result.


Air7,2 now accepts one bounded original kind2 graphics-remapper drain and complete
diagnostic retirement on `e271fd2c` / `03ea83a4578614d5`; see the
[unit evidence](machines/macbookair7-2.md#native-disabled-state-vt-d-drain-and-diagnostic-return-accepted)
and [protocol scope](intel-vtd-drain.md#first-bounded-native-drain-acceptance).
The user-authorized version3 policy allows150ms aggregate preparation/verified
return, measured102,810us; original individual hardware and parent bounds remain.
This removes the observed software100ms aggregate refusal for one new sample;
older failures remain rejected. The operation drains only AT00 memory already
processed by this graphics remapper. It does not establish that all GPU work
has reached it, invalidate GPU translations/caches, exclude firmware/display
readers or permit a private GPU interval. Those remain the next ownership
protocol work before managed-memory publication. All255 obligations, zero
assurances/managed GPU pages and inherited-framebuffer desktop remain unchanged.


The subsequently integrated software component is the
[System Agent GGTT invalidation protocol](intel-ggtt-invalidation.md): one exact
MMIO101008 DWORD1 followed by an ordered posting read, with original source/
root/epoch/clock and retained-failure checks. Both100/150ms actual-core matrices,
sanitizers,16 weakening controls and image isolation are locally qualified.
The separate combined native image now accepts one Air7,2 kind2 lifetime on
`fe3919ec` / `451221606bdf74f7`: remapper drain plus exact GGTT operation and full
original retirement in103,582/150,000us, with752us GGTT child. Its write-only zero
read is not an ACK or a per-engine pipeline-flush proof. See the
[native scope](intel-ggtt-invalidation.md#first-bounded-native-combined-acceptance).
The next boundary remains actual outstanding DMA/prefetch, GPU pipeline/cache/
per-engine translation and every firmware/display-reader completion, tied to
the original owner before private mapping/publication. This bounded new witness
closes no whole-device assurance bit and creates no managed GPU page.

The next command layer now has a host-qualified
[private BCS packet/completion protocol](intel-bdw-bcs-flush.md). It requires an
already-owned stopped ring/context, fresh private completion page, admitted
mapping/cache policy and original source/clock. Its known publication/submission
and nonce completion are separate, bounded steps; unknown state retains backing.
It cannot bootstrap those prerequisites, execute through an inherited ring or
prove other engines/firmware/display readers retired. No native caller/image or
new hardware acceptance follows. Next establish the genuine private command
resource owner and its initialization/retirement before native submission.

One native original kind2 [CPU command-arena lifetime](intel-command-memory.md)
is now accepted on Air7,2 `4864ad3a` / `838082d16f6be16d`, with four WB CPU aliases,
full16KiB preservation, exactly-once release and complete parent retirement.
The subsequent [four-page GPU mapping transaction](intel-command-mapping.md)
is host-qualified: it registers that CPU hold, requires an already transferred
interval and fresh independent ownership, publishes/restores through the existing
cores and retains every uncertain lifetime. It has no native provider or image
caller. Next establish the actual firmware/engine/DMA/cache/reader interval and
ordered completion witnesses, then integrate the original native adapter and
receipt chain. Executable LRC and submission remain separate steps. All255 GPU
obligations, zero positive assurances/managed GPU pages and native tear-free
pending remain; host composition does not accept any new hardware operation.


The separate [full-GT reset/fence protocol](intel-bdw-gt-bootstrap.md) now has
host qualification for the next fresh-owner bootstrap. It preserves the old
BCS8-only writer and imports no memory-completion assurance. Both timing policies,
callback/clock/proof faults, exact 32-fence restoration, unknown issuance and
native compiler/image isolation are tested. Its new original native parent and
epoch-aware post-reset source are not implemented; the accepted BCS ancestry
cannot host it unchanged. Keep fenced CPU clients, including the console,
stopped through restoration, and qualify the new source/receipt/recovery chain
before a physical full-reset trial. Actual interval/DMA/cache/all-reader ownership
remains necessary before a native mapping or submission.

## Existing-mapping reservation bridge

The [native existing-mapping census](intel-bdw-stolen-scanout.md#native-v51-read-only-observation-complete)
now completes after isolated pre-init dispatch. A separate
[borrow backend](intel-bdw-stolen-borrow.md) connects an already owned reservation
of those two external spans to the existing memory/hold lifecycle without PMM
allocation/free or a GGTT store.7,360 host cases/16 controls, sanitizers/native
frames and lifetime regressions pass. No native producer grants its rights;
CPU aliases, first RAM store, live reader ownership and tear-free remain pending.
The next native integration must establish the original live provider before
wiring this bridge in; saved vectors and successful resets are insufficient.

The [native v52 reader/cache preflight](intel-bdw-stolen-scanout.md#native-v52-reader-and-cache-observations-complete)
now completes on Air7,2. Only primaryA is sampled active; existing PAT5 gives
uniform hypothetical WC throughout both full candidates. All aliases retire
and source/media/recovery evidence verifies. This closes those observation
questions. Next implement the original live reservation/firmware/DMA/cache/
all-reader provider before the first writable alias; then visibility, initial
plane transition, latch/old-reader retirement and compositor/endurance. Stable
snapshots and a valid cache-type combination supply no ownership right.

The [native v53 CPU backing guard](intel-bdw-cpu-backing.md#native-result) also
completes: all3,072 candidate pages have reserved carriers/PMM tags, and the
actual whole-root scan finds no intersecting CPU alias. The22.924ms phase fits
the original second-pass budget. This closes CPU eligibility observation only;
an exclusive maintained reservation and the firmware/DMA/cache/all-reader proof
producer are still needed. Do not repeat this unchanged diagnostic or treat its
saved admission result as a live borrowed-source token.

The [native v54 guard](intel-bdw-cpu-backing.md#native-guard-result) maintained
CPU mapping exclusion through pin, acquisition, check, close and unpin on both
candidates in 24.119 ms. The hardware ownership work now centers on the
original firmware reservation and maintained requester, engine, DMA, cache
and reader proof. Writable CPU aliases depend on that ownership.

The [native v55 ownership gate](intel-bdw-stolen-scanout.md#native-v55-ownership-gate-result)
completed with clean release. The first unmet condition was an explicit
firmware transfer of the two inherited candidates. UEFI GOP describes the
active framebuffer, and the captured Air7,2 firmware interfaces provide no
verified transfer for the two extra spans. The active route is OS-allocated
backing under a cooperative native graphics-driver takeover. Linux i915
provides a design reference: GGTT reservations and reclamation are part of
driver initialization, with separate handling for the inherited framebuffer.
[UEFI GOP](https://uefi.org/specs/UEFI/2.10/12_Protocols_Console_Support.html#graphics-output-protocol)
[Linux v6.12 GGTT initialization](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c#L761)
[Linux v6.12 initial plane](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_plane_initial.c#L127).

1. Use [owned linear backing with a translation tail](scanout-memory.md#broadwell-translation-tail).
   Keep the full inherited fence, all existing display translations and their
   physical backing pinned.
2. Implement native admission of an exact GGTT interval under the boot-scoped
   owner. Bind all protected ranges and original entries to the same live
   requester/engine/firmware lifetime. Require actual completion and exclusion
   evidence before granting the interval to the allocator.
3. Qualify one mapping and exact restoration with private backing, real
   ordering, translation invalidation and observed completion. Then qualify
   both complete surfaces and the initial tiled-to-linear primary transition.
   Keep the inherited surface pinned until independent reader retirement.
4. Exercise repeated flips and restoration before compositor integration and
   physical tear-free acceptance.

The [transfer-backed publication path](scanout-transfer.md#capture-and-maintained-ownership)
now binds the leased entry to the original GGTT vector across preparation,
exposure and restoration. Host tests cover the one-entry transaction and a
late conflict in the second full buffer. The next native checkpoint is an
interval provider inside the [original post-reset owner](intel-bdw-gt-bootstrap.md#same-owner-post-reset-memory-preparation-wire6).
It needs to revalidate the enabled display's full footprint, other readers
and the original backing in that lifetime. It also needs separate pipeline,
cache and reader completion witnesses before arming the first aligned QWord
PTE store. [Linux v6.12 GGTT insertion](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c#L405).

The [wire9 preservation image](intel-bdw-gt-bootstrap.md#held-display-translation-preservation-wire9)
adds exact comparison of the inherited display's 128 trailing translations
and one prospective slot before reset and after original memory maintenance.
The MacBookAir7,2 matched both vectors and retired all four read-only aliases.
The [wire10 hardware trial](intel-bdw-primary-range.md#native-wire10-result)
also verified the live primary footprint, reader controls and display wells
in the same held window after memory maintenance. The
[wire11 hardware trial](intel-bdw-primary-range.md#native-wire11-result)
showed VGA, framebuffer compression and panel self-refresh inactive in both
samples bracketing that footprint. The
[wire12 trial](intel-completion-prerequisites.md#native-wire12-result) reached
Ready for Reset on all five engines and observed no engine or L3 flush-and-block
acknowledgements. Before granting a writable interval, the hardware-backed
owner must maintain this exclusion and establish completion of outstanding
accesses, prefetched reads and cache effects. Physical flip qualification
remains open.

The [first candidate's physical backing check](intel-bdw-primary-range.md#first-candidates-original-physical-backing)
now checks aliases between the selected PTE's original physical target and the
complete sampled primary image and translation tail. Saved wires 10–12 confirm
that physical separation. Keep the target reserved while a live owner proves
firmware exclusion and interval-specific completion before the first write.

The [reset boundary review](intel-completion-prerequisites.md#reset-and-display-recovery-decision)
rules out PCI AF FLR for a trial that preserves the running display. A
reset-based takeover would first need independent eDP and audio reinitialization.
The exact diagnostic now has VM receipts for saving and resetting without a
framebuffer and for halting over serial when its workspace is missing.

The [Air7,2 eDP recovery core](intel-bdw-edp-recovery.md) now covers PPS
power-up, AUX/DPCD training, the DDI/pipe register sequence and backlight
admission in the joined host fixture. The next step binds original PHY,
clock and mapping evidence to native MMIO and an independent fetch proof.
Qualify that recovery before adding any GGTT store.
