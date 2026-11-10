# Maintained engine hold after the bounded BCS reset

The accepted [original-entry observation](intel-held-interval.md) runs while the
software reset/preparation borrow is live. The actual post-reset hardware
controls are3/3/3/3/0: BCS reset cleared its request while the other four remain
held. A future native range provider needs a maintained hardware interlock,
not merely a software borrow or a completed receipt.

The separate `--intel-rehold` image re-arms only BCS RESET_CTL at0x220d0 using the existing
request0x00010001/ready protocol, invokes a child while all five requests remain
held, then cancels only that additional BCS request with0x00010000. Exact mixed
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
The bounded native result is recorded below. Host/VM checks establish original-object
lifetime, child drain before cancellation, every callback/store/clock fault,
old-image isolation and saved safe return versus retained-state halt.

The request/cancel encoding follows the existing
[preparation reference](intel-engine-prepare.md#limits-and-references) and pinned
[Linux v6.12 reset preparation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c).
It imports no catastrophic-error bypass, non-ready reset, forced cancellation
or additional reset domain. The [managed-memory plan](intel-managed-memory-plan.md)
still requires actual interval, firmware, DMA/cache and reader witnesses before
the first native PTE store.

## Implemented source and wire boundary

The original reset object owns a one-use registration of the re-arm object.
Source, preparation, sole registered GT borrow, current epoch and original GT
start remain bound. Rebound copies and a different fresh re-arm object cannot
claim the same reset. The child is separately address-sealed, exclusive and
one-use; a live or lost child prevents the cancellation store. There is no new
GT borrow or deadline. Core and native admission check both the original100ms
preparation and300ms owner clocks, including the final sample before a store;
the500ms outer diagnostic bound remains unchanged.

Four raw six-DWORD groups retain mixed entry, held entry/exit and restored mixed
state. Two acknowledgement rows retain first/final values, polls and phase clocks.
The child performs the original-entry observation under19 fresh native checks
of all five controls(95 additional control DWORDs). The last completed control
prefix, counts and failure are retained. A separate summary binds the original
clock origins, epoch, child borrow/return, restoration and uncertainty. The
strict decoder requires joint reset/preparation/interval/wake completion and
full preservation; a child observation alone is not acceptance of its parent.
Modeled native fixtures continue to fail real native backing prerequisites.

Only wake version19 and the exact20-token recipe add
`srvros.intel-rehold=request-cancel`. The new image has its own firmware request
and pre-user-init deferral. It uses the same14 aliases and profile/cache gates,
with only the existing BCS control page writable. No new reset domain, PTE store
or aperture-data alias is introduced. Older images keep their exact recipes,
control paths and wire formats.

To fit the unchanged65,536-byte saved tail and1,536-byte reserve, this image
packs raw inherited PTEs in version2 chunks of64 entries. Every little-endian
64-bit word survives; the final partial chunk and canonical base64 are checked.
The older version1 chunks stay16 entries. Version2 requires the new image or its
exact preflight boot context; mixed versions, gaps, duplicates, truncation and
wrong-image data refuse. Actual older/newer C producers must decode to the same
complete1,356-entry vector. Plane/HDA preservation still retains all74 DWORDs.

Qualification requires full regressions,24-image isolation, fatal sanitizers,
compiled negative controls, native object checks, worst-width log projection and
exact-stage VM persistence/refusal/missing-workspace. The original native hold
has only2,621us one-run preparation margin; do not extend any bound to fit this
additional work. A bounded native pass is recorded below. Preserve the accepted
older payload and Ubuntu recovery; the automated trial needs no input, saves
safe outcomes and returns, while retained uncertainty/persistence failure halts.

## Qualified candidate and bounded native acceptance

Source`f2a6c116`, exact release`e83571c874bc17f1`, passes the full local build,
host/core/docs suite and24-image isolation. Fatal ASan/UBSan covers1,111 cases:
328 core,764 actual native adapter,4 boot and15 directed delayed-ACK/poll/phase
cases. All14 compiled behavioral/native-object controls are detected. The actual
native object and C ABI verify the BCS DWORD target, masks, child drain gate and
both original clock origins at final store admission. Initial object-checker
assumptions about an inlined helper and compiler memset were corrected in the
ignored runner; original nonpass receipts remain and kernel source is unchanged.

Exact-stage QEMU verifies unsupported-GPU refusal with zero target loads/stores,
checkpoint/results saved, offline file bytes, unmount and automatic safe return.
A separate missing-workspace guest halts. The conservative saved-log projection
is63,485/65,536bytes with the unchanged1,536-byte reserve. All1,356 PTEs and74
plane/HDA words remain lossless. Accepted payload/recovery and USB evidence are
preserved independently; see the [machine checkpoint](machines/macbookair7-2.md#ready-post-reset-bcs-re-arm-and-drained-original-entry-child).

The subsequent automated native trial passes in release`e83571c874bc17f1`,
session`session-00065ff078d4f70a-00`. Raw controls move from3/3/3/3/0 to all five
ready, stay ready through the original-entry child, and return exactly to the
mixed state. Two extra BCS stores acknowledge without polling; the sealed child
borrows/returns once,19 live checks retain95 control DWORDs and neither child
nor uncertainty remains. Scope358us/child234us, preparation97,758/100,000us and
outer wake281,933/500,000us pass unchanged bounds. Original borrow return,
14 alias retirements, engine/backing/74-word plane/HDA preservation, strict
checkpoint/source/recipe/completion and clean saved return all verify. The
58,936-byte exit log and all prior USB files survive; offline integrity and
recovery pass, with no one-shot armed. See the
[accepted machine result](machines/macbookair7-2.md#current-checkpoint-native-post-reset-re-arm-and-restoration-accepted).

This accepts one bounded re-arm/child/restore lifetime. The2,242us preparation
margin is one-run evidence. QEMU did not establish that acceptance, and the native
pass supplies no other-client exclusion, GPU range, command/PTE permission,
DMA/cache/reader retirement or completion bit. All eight obligations remain open.
