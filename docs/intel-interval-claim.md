# Source-bound read-only interval claim

`intel_interval_claim` supplies the lifetime between the accepted
[post-reset re-arm child](intel-engine-rehold.md) and a future native address
provider. It owns the original-entry observations and registers a dependent on
that exact child. The child cannot return while the claim is live or lost, so
outer cancellation cannot run ahead of an unfinished interval operation.

This is a read-only source claim, not a transferred GPU address lease. It emits
no hardware assurance, clears no completion obligation, allocates no GPU-visible
memory and has no PTE, command, flush, invalidation or register-write callback.
The [complete transfer core](scanout-transfer.md) rejects its data manifest
before invoking a provider or creating an arena. The current native images do
not link the claim core; their ordinary zero-dependent child path is preserved.
The new child-return bookkeeping is host/VM qualified separately from accepted
native source`f2a6c116`.

## Original source and terminal proof

The one-use, address-sealed object captures the existing exact Air7,2 geometry,
one original candidate at index8704, original reset/preparation/GT owner,
source epoch and both original clock origins. It copies the caller's descriptor
before any callback. Storage overlapping the claim, child, reset, preparation or
GT owner refuses without corrupting that graph. A data output cannot overwrite
those objects either. A fully rebound copy still cannot match the registered
original dependent. Callback reentry and a second claim on the same child refuse.

Opening registers the dependent before its first callback, then performs the
actual [original-entry observer](intel-held-interval.md): high/low/high twice,
six DWORDs. Closing performs a second owned observation, again six DWORDs, and
requires exact original state, fresh source/guard and positive clock progress
before unregistering. All twelve completed reads and partial observations stay
in the original claim, including a value read before its post-read guard fails.

Both the original100ms preparation and300ms GT-owner clocks remain mandatory;
no opening, check, close, helper or child starts a replacement budget. The
500ms enclosing diagnostic bound remains unchanged. Each external callback is
bracketed by source/clock validation. Zero, backward, expired or non-progressing
clocks refuse. The first invalid clock sample and previous accepted time survive.
A check validates maintained source/guard conditions, not a fresh PTE vector;
only the opening/closing observations supply those raw entry samples.

| State | Required behavior |
| --- | --- |
| Opening | Register the original dependent; capture and verify the original entry under the live re-arm child |
| Active | Keep source identity, epoch, original clocks and the dependent registration; expose only data with assurances0/obligations255 |
| Closing | Verify the exact original entry again and drain before unregistering; stop all new operations |
| Closed | Refuse reuse, manifests, checks and repeated close; child return is permitted only with matching dependent counts |
| Fault | Retain the dependent and original child/GT exclusion; refuse retry or forced cleanup |

A lost registration pointer does not imply retirement: unmatched borrow/return
counts independently block child return. The claim keeps its pin after changed
entries, callback/source/epoch loss, expired clocks or failed close. It does not
repair a lost source, release inherited physical targets or replay hardware work.
All objects and callback context must survive retained faults until reboot.
These are CPU0 serialized trusted-kernel contracts, not an SMP lock or protection
against arbitrary kernel writes to their public storage.

## Hardware transfer remains a separate boundary

The manifest contains the exact captured original, but always supplies
assurances0 and all eight open obligations(mask255). Matching raw entries,
engine requests, endpoint snapshots or a diagnostic child are insufficient to
make the candidate free. No native range is imported by this component.

The [managed-memory plan](intel-managed-memory-plan.md) still requires a real
maintained interval provider, exact physical-backing and DMA/cache policy,
firmware/other-reader exclusion, documented posted/prefetch/pipeline completion,
translation invalidation and terminal retirement. Generation-specific update
code also needs its own mapping/cache policy. Pinned
[Linux v6.12 GGTT insertion/invalidation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c)
uses an owned address-space allocator, PTE encoder and invalidation path together;
copying its PTE store cannot supply those ownership prerequisites. Its
[reset preparation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c)
does not turn a BCS-only reset into a device-wide retirement witness.

This bounded source claim is not the eventual persistent desktop GPU service.
Returning/cancelling its parent ends its usable source. A future runtime owner
must explicitly maintain its own audited lifecycle and publication guarantees;
it cannot keep this diagnostic alive by restarting its clocks.

## Validation

`make test-intel-interval-claim` drives the real CPU exclusion, GT, census,
preparation, reset, re-arm, original-entry and transfer cores. The baseline owns
twelve entry reads. The fault matrix covers every clock/guard/read position,
changed entries, expired original preparation/owner budgets, copied/rebound
owners, reentry, mutable caller data, lost registration, unfinished claims,
repeated close/reuse and early transfer refusal. Existing re-arm tests exercise
the zero-dependent path. Deliberate retained-fault cases use fresh processes.

Native integration is intentionally absent. Do not schedule a redundant hardware
trial solely to rerun this software contract. A new native operation requires a
separate exact image/recipe, actual adapter admission and object checks, durable
checkpoint, strict raw collector, unchanged bounds and useful physical exit
criteria under the [Ubuntu workflow](ubuntu-device-agent-setup.md).


## Software-qualified checkpoint

Source`5466bdd3` builds on main`72d49457`, integrated as`2c5f1df1`. The integrated
baseline passed the full local build/check-host/test-core/check-docs gate with
installed-root explicitly omitted, and its GPU objects matched the accepted
native source byte for byte. The subsequent claim/header changes pass the
matched build/check-host/docs,24 images and affected claim/re-arm/reset/interval/
transfer/memory/publication regressions. These are separate exact-source gates;
the integrated full test run is not relabeled as a full run on the later delta.

Fatal ASan/UBSan passes1,200 cases:872 claim and328 re-arm. All ten compiled
behavioral controls and two native-object controls are detected. The actual
freestanding claim compiles with a2,048-byte frame limit, and actual C ABI and
native disassembly bind the dependent pointer/count return gate. The new claim
is absent from all24 boot images. No native source capability was added.

Exact v19 stage`0faa0ddbacda88c1` passes QEMU unsupported-GPU refusal with zero
target stores, checkpoint/result persistence, offline file bytes, unmount and
automatic safe return; missing workspace halts. Its63,485/65,536-byte projected
log keeps the1,536-byte reserve. Normal desktop QEMU also passes real GUI/pipe
waits and descriptor replacement,72 clients reaped,128 surface readback/reuse,
two compositor lifetimes, independent documents/focus/clipboard/resize and clean
close/relaunch with all cache bytes retired. These are VM results only.

The stage is retained but not installed. Accepted currente835/recovery9aa8 and
their original native evidence remain unchanged; no reboot is armed. The
[Air checkpoint](machines/macbookair7-2.md) and independent SHA-verified backup
record the qualified source, all nonpass receipts and remaining hardware gates.

## Next native scope: maintain display-audio inhibition through the claim

The existing [display-HDA owner](intel-display-hda-lease.md) and GPU preparation
were accepted separately. The current GPU lifetime preserves HDA command6,
with bus mastering enabled; it does not maintain the earlier HDA inhibit lease.
A next explicit diagnostic should join the real original HDA token/borrow and
GPU/re-arm/claim lifetimes under one CPU exclusion owner. Open and pin HDA
inhibition before GPU preparation, keep it registered through claim retirement,
then restore only after both sources and their children retire cleanly.

Retain the original HDA400ms, preparation100ms, GT300ms and full500ms bounds.
A child cannot restart a parent clock. Map/admit all required aliases before
control stores; save the prerequisite checkpoint before either writer. The
adapter must cheaply revalidate the exact inhibited endpoint between full
entry/exit inspections, rather than run a whole HDA census on every PTE guard.
Such a guard still needs independent native admission and fault tests.

Use a separate image/recipe/collector. Bind the command6→2→6 relation to the
correct original/inhibited/restored HDA phases instead of requiring all those
values to equal the boot snapshot. Preserve every raw engine/display/HDA/PTE
word with canonical packed records if necessary; do not enlarge the log tail or
drop evidence. Unknown stores, lost ownership or failed retirement retain both
owners and halt, with no forced cleanup. Only safe saved/unmounted outcomes
return automatically. No audio or human input is needed for that diagnostic.

This proposed joint trial is not yet implemented or accepted. HDA bus-master
inhibition and non-posted completion still do not establish posted/prefetch,
GPU pipeline/cache/TLB, firmware, FBC/PSR/other-reader retirement or GPU interval
transfer. All255 obligations remain open until their actual witnesses exist.
