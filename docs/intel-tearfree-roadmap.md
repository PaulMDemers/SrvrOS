# Air7,2 native tear-free desktop roadmap

The next milestone is a reviewable ownership protocol that can support one
private GPU mapping. Repeating successful observations will not by itself
make inherited graphics memory available. The inherited framebuffer remains
the desktop backend until the native ownership and publication gates pass.

This roadmap orders the work in the [managed-memory plan](intel-managed-memory-plan.md).
The [Air7,2 unit record](machines/macbookair7-2.md) owns physical acceptance;
the [coordination guide](agent-coordination.md) governs shared implementation.
It does not accept another Mac's firmware or graphics profile.

## Established boundary

| Native evidence | Accepted scope | Still excluded |
| --- | --- | --- |
| BCS reset provider integration, source `4873e6bd`, release `6365de27e20576b6` | Already-disabled BCS entry, bounded reset/preservation and clean retirement; core61,212/100,000us | Enabled shutdown, other reset domains, device-wide DMA retirement |
| Five-engine preparation, source `292163ac`, release `0cafc3a5551ab1b2` | Bounded prepare/hold/cancel and preservation;93,246/100,000us | Maintained readiness after cancellation, authority for a later reset |
| PCI AF requester capture, source `fe6677c5`, release `3ff42f33e6c01391`, session `session-00065fdf108c49b4-00` | Two matching nine-DWORD snapshots, BME enabled in both, TP clear, FLR control0;27,036/100,000us, borrow returned and10 aliases retired | Comprehensive DMA drain, firmware exclusion, cache/translation/reader retirement, memory transfer |

These are separate exact-source lifetimes. They cannot be combined into a
current owner or completion token. The [requester contract](intel-pci-requester.md)
records the accepted native capture; PCI AF TP clear is an observation, not
proof that all posted writes reached memory. Bus mastering remains enabled.
All eight completion obligations remain open (mask255), native hardware
assurances remain zero and no managed GPU pages are accepted.

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

The next implementation block composes the real preparation/requester/reset
cores in one lifetime: fresh engine census; five reset requests held; requester
observation while that hold remains active; exact reviewed reset action; fresh
post-reset state; reverse cancellation; full engine/display/HDA preservation and
retirement. The preparation API currently completes cancellation before return,
so it needs an explicit maintained-action boundary. A cancelled preparation
receipt cannot serve as that boundary. Epoch invalidation must precede any
possible reset store, and old censuses must become unusable.

The first software piece is the separate
[maintained read-only action boundary](intel-engine-prepare.md#maintained-read-only-action-boundary).
It brackets an observation with original-borrow guards and matching held-state
samples inside the unchanged100ms budget. It has no native caller or reset
permission. The standalone requester observer acquires its own borrow and
cannot be nested unchanged; a future sampler must bind the active preparation
borrow explicitly. A reset-capable composition remains separate because it
changes epoch and control-state semantics.

The complete composition needs host fault injection at every transition.
It must preserve the existing BCS-only writer gate until a separate review
settles wider domain semantics. Broadwell GDRST full-reset bit0 is distinct from
per-engine bits1/2/3/4/7. The pinned Linux reset path prepares engines, resets,
then cancels, and repeats reset with50us settling for delayed register stability.
Do not widen domain8 to0x9e or assume readiness bits survive reset. See the
[pinned reset references](intel-engine-prepare.md) and
[reset contract](intel-bcs-reset.md). No new native reset writer is selected in
this planning block.

Before staging that composition, record the exact native action, reference-backed
register/memory effects, per-obligation evidence and failure-state ownership.
Any unresolved completion/ownership dependency retains refusal. A synthetic
positive-assurance provider would not establish transfer. A hold-plus-requester
observation-only trial is warranted only if its result resolves a specific
composition decision, not merely because both operations exist separately.

## Ordered delivery blocks

| Block | Implementation | Observable exit gate |
| --- | --- | --- |
| 1. Ownership protocol | Encode the chosen route as a single original-object, same-epoch transaction; bind provider, resources, interval and clocks. Preserve raw attempted/completed prefixes and retained state. Keep unrelated ordinary startup unchanged. | Actual-core fault tests cover copied/stale objects, reentry, clock expiry, provider loss, every store boundary and retirement refusal. The reviewed native protocol identifies the witness for each claimed assurance; unsupported prerequisites refuse. |
| 2. Native ownership qualification | Implement only the exact reviewed operation and native profile, with its own immutable recipe, checkpoint and collector. Any all-engine preparation needed for reset must remain live through that reset and its cancellation/preservation path. | Matching hardware evidence proves the intended operation and its scope, full display/backing preservation and verified return, within existing bounds. A clean reset alone does not close the memory obligations. |
| 3. One-page GPU mapping | Lease a transferred non-display GPU interval and pin one private RAM page. Bind DMA addressing/translation, CPU aliases/cache policy and original PTE. Implement exact PTE write, posting and required invalidation/completion sequence. | The page is the only changed translation; exact readback and documented visibility/completion pass. No plane, route or visible framebuffer changes. Unknown completion retains both address and RAM leases. |
| 4. Owned command completion | Where required by the selected route, own ring/context/status/completion backing and implement a finite generation-specific command sequence with a fresh nonzero completion value. This block moves earlier if mapping/retirement depends on it. | Completion is tied to the submitted request, original mappings and exact scope. Stale completion values, timeouts and unrelated engine completion cannot release backing. Never submit through an inherited ring. |
| 5. Restore and retire | Restore original translations only while ownership remains established, complete the required translation/cache/reader retirement, then release address and RAM leases. | Success and injected failure paths have explicit ownership outcomes. A known store followed by an unknown result cannot free, replay or optimistically restore memory. Native trial verifies preserved original display and clean retirement. |
| 6. Full scanout buffers | Expand the existing handoff contract to describe complete allocations, then own two full buffers with checked stride, padding, layout/tiling, swizzle, DMA and cache policy. | Host overflow/extent/alias/lease tests and native buffer validation pass. The current16-page contract cannot describe the inherited1356-page extent. No truncated import is accepted. |
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
