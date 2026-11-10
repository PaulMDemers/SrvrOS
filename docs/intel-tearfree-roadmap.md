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
| Requester capture inside maintained preparation, source `8559ac44`, release `67a178af16c8983f`, session `session-00065fe0b883f61e-00` | Original preparation borrow, five ready requests, two matching PCI snapshots, cancellation/preservation/return;96,728/100,000us,13 aliases retired | Reset-capable action semantics, repeatability, all memory/completion obligations |
| Maintained preparation/PCI/BCS8 reset, source `9c788f3f`, release `8d3839aaa57f707d`, session `session-00065fe55d829da0-00` | Exact pre-user-init recipe; full reset/cancel/preservation/return in97,414/100,000us, wake280,135/500,000us;13 aliases retired | Repeatability, wider reset scope, all memory/completion obligations |

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
