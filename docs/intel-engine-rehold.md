# Maintained engine hold after the bounded BCS reset

The accepted [original-entry observation](intel-held-interval.md) runs while the
software reset/preparation borrow is live. The actual post-reset hardware
controls are3/3/3/3/0: BCS reset cleared its request while the other four remain
held. A future native range provider needs a maintained hardware interlock,
not merely a software borrow or a completed receipt.

The new scope will re-arm only BCS RESET_CTL at0x220d0 using the existing
request0x00010001/ready protocol, invoke a child while all five requests remain
held, then cancel only that additional BCS request with0x00010000. Exact mixed
state must return before the existing outer cancellation/preservation. Every
access uses the original reset parent, epoch, registered borrow and100ms origin;
10ms/256-poll phase caps and300/500ms parent/outer limits remain unchanged.

The child pins the original scope. Copies, a lost parent/epoch, expired clocks,
unknown stores, changed controls and a still-live child prevent cancellation or
retirement. Failure retains the original preparation and aliases; it does not
force cleanup. No new GT borrow, GPU allocation, PTE write or completion grant
is supplied. All eight completion obligations remain open(mask255).

This is a new hardware operation requiring a separate image, exact recipe,
durable prerequisite checkpoint, progress and strict raw-prefix collection.
Native acceptance is pending. Host/VM checks must establish original-object
lifetime, child drain before cancellation, every callback/store/clock fault,
old-image isolation and saved safe return versus retained-state halt.

The request/cancel encoding follows the existing
[preparation reference](intel-engine-prepare.md#limits-and-references) and pinned
[Linux v6.12 reset preparation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c).
It imports no catastrophic-error bypass, non-ready reset, forced cancellation
or additional reset domain. The [managed-memory plan](intel-managed-memory-plan.md)
still requires actual interval, firmware, DMA/cache and reader witnesses before
the first native PTE store.
