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
before invoking a provider or creating an arena. Only the distinct
[joint HDA/GPU image](intel-hda-gpu.md) links the claim core. Legacy images keep
their ordinary zero-dependent child path.
The original claim now has native joint-lifetime acceptance in source `e81caf7f`,
release `978d9be94b96243a`, session `session-00065ff636174005-00`.
Preparation completed in 91,213/100,000 µs with both original-entry observations,
HDA inhibition maintained until GPU retirement and all aliases retired.
The registered consumer and page planner described below are subsequent software
work, with no new native caller or hardware acceptance.

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

## Registered interval consumer

`intel_interval_borrow_open` registers one original, zero-initialized consumer
before the first source callback. Its pointer, independent original registration,
epoch, sequence and borrow/return counts must continue to agree. Only one consumer
may be active; a distinct object can borrow after a clean return. Returned
objects and exhausted counters refuse reuse. Consumer storage cannot overlap
the claim or its parent graph.

The pure `intel_interval_borrow_live` predicate validates the same original
source during a serialized callback without permitting reentry or I/O.
`intel_interval_borrow_deadline` accepts a fresh timestamp from that same clock
and advances the original claim watermark; bad samples latch the original
clock failure and retain the consumer. It takes no clock sample and cannot
restart either parent budget. Direct native operations must publish accepted
samples so later source checks/return cannot accept an older time.

Checks and return use the claim's original source guard and both original clock
origins. They do not open another GT borrow or restart a deadline. Callback
reentry refuses without touching either owner. A copied consumer cannot return
or fault the original. A registered original that loses its source latches a
fault; restoring its pointer later cannot authorize a retry. Failure retains
the consumer and parent pin. Caller storage/context must remain alive.

Claim close refuses before callbacks while a consumer or unmatched count exists.
Joint HDA/GPU retirement independently checks the consumer pointers and counts;
even a falsely closed claim cannot authorize early HDA restoration. A successful
consumer return removes only that registration. Claim close must still perform
its own exact original-entry observation before the re-arm child can return.

`intel_interval_borrow_manifest` exposes captured data with assurances 0 and
obligations 255. `intel_interval_borrow_page` also calculates one conservative
Broadwell entry through `intel_bdw_ggtt_page_plan`: a nonzero aligned physical
address below 4 GiB, the present bit only, a 64-bit exact original, a byte GPU
address and modification-aperture offset. It rejects a page intersecting the
protected fence or falling outside the aperture. The planner leaves output
unchanged on refusal and has no hardware callback. The borrow wrapper prevents
output from overwriting its lifetime graph.

This is a plan for a future owned transaction, not an ownership receipt.
Planning does not establish that the supplied physical page is allocated,
pinned, DMA-addressable or cache coherent. Data access takes no fresh time
sample; an actual provider must check its original live source at each operation
and establish all transfer/completion prerequisites before any PTE write.

The encoding follows [Broadwell PRM Volume 5 (Memory Views)](https://cdrdv2.intel.com/v1/dl/getcontent/690798), printed pages 7 and
44–45, and pinned
[Linux v6.12 `gen8_ggtt_pte_encode`](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c).
No high-address encoding, cache-policy bit or translation invalidation sequence
is inferred from the low-address codec. In particular, a zero read from the
write-only GFX_FLSH_CNTL register cannot witness completion.

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
The registered-consumer matrix covers two successive borrowers, every consumer
clock/guard position, lost/rebound pointers, parent/epoch/sequence changes,
reentry, overflow, retained faults and output aliasing. The joint core tests
clean consumer return, a live consumer and an orphan after a forged claim close.
`make test-intel-bdw-ggtt` exhausts all 1,048,575 supported physical page
encodings below 4 GiB and checks original high bits, invalid addresses, protected
fence and aperture limits.

Do not schedule a redundant hardware trial solely to rerun this software
contract. The joint native operation uses a separate exact image/recipe, actual
adapter admission and object checks, a durable
checkpoint, strict raw collector, unchanged bounds and useful physical exit
criteria under the [Ubuntu workflow](ubuntu-device-agent-setup.md).


## Consumer and codec qualification (2026-11-10)

Source `02545e7a` passes the local build/check-host/test-core/check-docs and
USB-image gate with all 25 images. Installed-root testing was explicitly omitted
because no installer changed; installed current/recovery hashes were checked
against their independent banks. Fatal ASan/UBSan passes the full claim and
7,735-case joint-core matrices and the exhaustive codec test. Eight compiled
controls detect missing parent-loss latching, claim drain, original registration,
borrow accounting, callback owner retention, HDA drain, physical alignment and
protected-fence checks.

The claim, joint, probe and page objects pass a 2,048-byte freestanding frame
limit. The new consumer/planner APIs are unused and absent from all 25 linked
images. Only the joint image differs from the prior accepted timing build;
the other 24 images match byte for byte. Exact uninstalled release
`2ec70810586e32e7` passes QEMU unsupported-hardware refusal with zero native
stores, matching saved checkpoint/result, offline bytes, clean unmount and
automatic return. Missing workspace halts. This tests the existing no-consumer
refusal path and grants no native consumer, mapping or completion acceptance.

Current `978d9be94b96243a` and recovery `9aa8dbf6fbda8252` remain installed,
with no reboot armed. The unit checkpoint and independent backup retain the
qualified source and receipts, including the intentionally interrupted earlier
gate and the initial harness failure. Hardware interval transfer and all eight
completion obligations remain open.

## Historical software-qualified checkpoint

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

## Joined native scope: maintain display-audio inhibition through the claim

The [display-HDA owner](intel-display-hda-lease.md) and GPU preparation
were first accepted separately. The standalone GPU lifetime preserves HDA
command 6 with bus mastering enabled. The joined diagnostic instead binds the
real original HDA token/borrow and GPU/re-arm/claim lifetimes under one CPU
exclusion owner. It pins HDA inhibition before GPU preparation, keeps it
registered through claim retirement, then restores only after both sources and
their children retire cleanly.

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

The [joint lifetime core](intel-hda-gpu.md) now implements the software
composition with registered HDA dependents, fresh endpoint checks and the
original clocks. Its distinct native adapter/image/recipe/collector are
implemented and the joint timing trial above is physically accepted. HDA bus-master
inhibition and non-posted completion still do not establish posted/prefetch,
GPU pipeline/cache/TLB, firmware, FBC/PSR/other-reader retirement or GPU interval
transfer. All255 obligations remain open until their actual witnesses exist.
