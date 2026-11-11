# Original-owner disabled BCS entry and reset composition

This separate diagnostic composes the accepted already-disabled BCS observation
with the existing joint-owner reset-admission pipeline. Native acceptance is
pending. Read the [HDA/BCS observation](intel-hda-bcs-shutdown.md),
[joint lifetime](intel-hda-gpu.md) and [machine record](machines/macbookair7-2.md).

The original HDA owner inhibits Command6→2 before GT wake. A new sealed
read-only child then observes four BCS groups through the existing zero-store
blitter core, binding both the initial and terminal words to the original full
completion census and unchanged parent/epoch. It blocks early GT close and
returns its original borrow before takeover admission. The original completion
object stays live; its age and clock origin are never refreshed to buy time.

Only already-disabled zero geometry, exact idle context and complete source
agreement may advance. Enabled or nonzero entry refuses without a new store.
The existing takeover/preparation/BCS-reset/re-arm/claim/cancellation and full
preservation sequence remains responsible for all its own gates. The enabled
shutdown writer is a separate unexercised branch; this composition does not
pretend it has run or clear ring geometry to admit it.

Keep the whole child at100ms including borrow return, BCS core100ms, GT300ms,
HDA400ms and outer500ms. Existing150ms shutdown/readers limits stay unchanged
in their separate image. Failed source/epoch/registration/return retains the
original borrow and exclusion. No saved report or prior boot supplies a live
owner. No requester/PTE/Stop Ring/ring-start writer, memory grant, positive
completion assurance or physical tear-free claim is introduced.

The image, firmware request, exact recipe and receipts are distinct. Ordinary
boot and existing image contracts remain separate. Qualification must cover
actual source/census binding, zero write callbacks, all read/guard/clock/return
faults and retained static lifetimes; a copied census/owner/provider must refuse.
Native object, frame, image isolation, saved-tail and exact-stage VM checks are
required before hardware installation. Safe saved outcomes return automatically;
retained uncertainty halts visibly. The original already-disabled observation
remains accepted on its recorded source, not on this new composition.
