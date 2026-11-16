# Separate Broadwell full-GT reset and fence preservation

`intel_bdw_gt_bootstrap` is a host-qualified reset/preservation protocol for a
fresh graphics owner. Its separate native adapter/image is now implemented and
under qualification, with no physical acceptance yet. It cannot be
nested unchanged in the accepted BCS8-only reset/source graph: that graph binds
its own epoch transition, mixed post-reset controls and preservation rules.
The accepted [paired admission](intel-command-ownership.md) remains separate.

The new protocol performs no GPU PTE write, executable context initialization
or submission, and supplies no `HANDOFF_*` assurance. A completed reset is not
an outstanding DMA, pipeline/cache, prefetch or all-reader completion witness.
All 255 existing whole-device obligations remain open.

## Why a separate bootstrap is needed

The current native reset only addresses the blitter domain. A fresh driver
command path also needs an explicit source/epoch and ownership of the other
engines. Intel's [Broadwell PRM Volume 2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
printed pages 609–610, defines GDRST at `0x941c` and its separate full soft-reset
bit 0. The old BCS bit 3 gate remains unchanged.

The pinned [Linux v6.12 reset path](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c)
selects the full domain for all engines, performs two known pulses and waits
50 microseconds after acknowledgement for register stability. This component
imports neither retry after failure, non-ready reset nor catastrophic-error
bypasses. The settle delay is not a memory-completion fence.

PRM pages 611–615 distinguish the 32 CPU aperture detiling fences from GPU
surface tiling. Their state must be accounted for across a full reset. The
protocol conservatively accepts either exact preservation or a cleared fence;
it does not assume that every platform necessarily clears them. Other changed
values refuse before a guessed restoration. All CPU aperture/fence clients
must remain stopped while the vector may be incomplete, including the console.

## One original source, all original clocks

The source binds original owner/provider/lifetime, root, MMIO/aperture resources,
epoch, sequence, explicit policy and all 32 original 64-bit fences. The source
and result are consumed once. Registration precedes the first callback; the
future native parent must block closing while that result remains registered.
Copied sources, overlapping input/output storage, callback changes, reentry
and callback edits to progress or vectors refuse. Failure restores the result's consumed/retained marker even
when a callback changed its registration field; it does not restore hardware.

Each provider proof must independently establish the actual firmware contract,
MMIO/cache admission, CPU exclusion, requester and engine holds, pinned backing,
display prerequisites and stopped fenced clients. It names the original source,
owner/provider/lifetime/root/epoch/sequence and a timestamp between surrounding
source-clock samples. These protocol prerequisite bits are a different type
from memory-completion obligations and handoff assurances. Echoing the supplied
descriptor is insufficient. This is a trusted serialized kernel contract, not
a boundary against arbitrary kernel memory writes or an SMP/IOMMU mechanism.

Original preparation remains 100 ms by default and 150 ms under the selected
diagnostic policy; GT/HDA/outer remain 300/400/500 ms. Each reset/cancellation
phase has its own 10 ms and 256-read cap inside those original bounds. The
post-write guard and final admission count too. A slow admission cannot permit
a store after its phase deadline. Zero, backward, expired and frozen clocks
refuse; exact endpoints pass. No old deadline changes.

## Operation and exact restoration

1. Capture all 32 fences with high/low/high DWORD reads and match the complete
   original vector. Reject reserved encodings, invalid geometry and overlapping
   valid fence regions before admission. Read all five ready controls and clear
   GDRST before the first reset; the caller must already own their preparation.
2. Invalidate the actual parent's old engine epoch exactly once before the first
   possible store. Never roll it back, including a definitely unissued pulse.
3. Issue exactly `GDRST/1`, observe its self-clear, repeat the fixed successful
   pulse, then measure at least 50 microseconds of settling. Pending, floating
   or foreign bits stop progress; an uncertain first pulse forbids a second.
4. Read clear post-reset controls/domain and capture every fence. Retain exact
   preserved entries. Only a changed entry observed as zero may be restored.
   For each such entry, disable low, read low, write original high, write original
   low, then read low. Every read/store has a fresh admission.
5. Cancel the five original preparation requests in reverse order. Re-read all
   32 complete fences after cancellation and compare every bit with the original.
   Require fresh whole display/HDA/backing preservation and another clear GDRST
   endpoint before returning the logical child registration.

The split DWORD fence order follows pinned
[i965_write_fence_reg](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt_fencing.c)
(its generation-6-and-later branch), SHA256
`14148f25ad9e3a4b9d3bd6b3e221159a7237eade15cb7ca56cd8254719c142f0`.
Posting reads order those fence writes; they do not certify unrelated GPU DMA
or cache completion. A successful baseline with one cleared fence has two reset
stores, three fence stores and five cancellation stores. Preserved fences need
no stores; all 32 changed fences need 96 exact fence stores. No native resource
permission follows from the pure `write_ready` predicate.

All failed started operations retain registration. There is no retry, forced
cancellation, opportunistic rollback, epoch rollback, unmap/free or parent-release
callback. The native parent still owns its backing and physical pins and must
independently complete its return after successful child retirement.

## Host qualification and native integration boundary

`make test-intel-bdw-gt-bootstrap` qualifies the actual C protocol with an
independent register/epoch model. Both original timing policies pass 50,291
cases each: every callback's source/progress mutation, every clock/proof field,
every read/store failure, unknown issuance, pending/floating/foreign state,
all/mixed/preserved fences, exact/+1 deadlines and bounded polling. The device
model independently enforces disable/posting-read/high/low/posting-read order.
Fatal ASan/UBSan runs 826 selected cases per policy, including every read/store
failure and the complete 32-fence path. All 23 compiled weakening controls must
fail at runtime; a compile error is not a detected control. Actual native flags
pass the 2 KiB frame limit for both policies. Ordinary and existing BCS/audit/
remapper/command images remain isolated from this component.

### Original native owner and new epoch

The selected `build/intel-gt-bootstrap/srvros.elf` uses the exact15-token
`srvros.intel-ring=gt-bootstrap-v1` recipe. It defers user init, owns the inherited
display exclusion and original HDA/GT parents, and runs the unchanged state-aware
entry. That earlier entry/census is historical evidence once the new reset
advances the actual GT epoch. It is never reused as a live post-reset source.

A new registered GT borrow independently prevents parent closing. It inhibits
the actual GPU requester with command16 `7→3`, validates the AF capability prefix
before payload access, and owns all five preparation requests. Every native
admission checks the original root/CPU exclusion, parent epoch and callback
table, exact resource/firmware endpoint, HDA hold, complete fresh CPU cache
context and all15 actual UC/NX aliases/permissions. The existing whole framebuffer
translation check uses the bounded leaf walker under IRQ exclusion; it still
checks every covered mapping and both root endpoints. No translated-page result
is cached between callbacks. The extra fence page is mapped before wake and
retired only after the original GT parent closes.

Only this original owner may supply the bootstrap's prerequisite proof. The
first possible reset store is preceded by a real parent epoch increment. CPU
aperture/console clients remain excluded through complete fence restoration.
The critical-section guard deliberately does not interpret a fence vector while
the known reset/restoration is in progress. Whole GGTT/backing/display equality
is required before entry and after restoration, alongside paired39-word
programmed/live/stereo reader observations. A separately derived capture view
accounts for the actual inhibited command halfword; it does not edit the boot
capture, normalize another resource, or widen the existing backing core.

After core restoration and reverse cancellation, a new92-DWORD engine/context
census binds the actual new epoch. Four exact PCI vectors require `7,3,3,7`,
unchanged resource/capability fields and no AF pending bit. Only then can the
owner return its borrow. An independent failure latch prevents outer GT/HDA
return and alias retirement on uncertainty, including a clock fault after the
logical child return. Separate guard, borrow-return and final-guard timestamps
include a fresh checked clock after the final preservation guard; completed
local reset ownership never implies that an outer parent also retired.
The original150ms preparation and300/400/500ms parent
bounds remain; individual10ms/256-poll bounds are unchanged. The native image
selects the existing150ms policy directly, without linking the old VT-d/reset
graph or extending a parent clock.

Version1 `intel-bootstrap-*` records retain all32 before/after/final fences,
five controls, two pulses, partial PCI/display prefixes, original clocks and
the new epoch census. The strict receiver checks exact coverage/order, derived
counts, source/deadline binding and full parent retirement. Its explicit
wake37→25 view verifies only unchanged historical entry/HDA prerequisites;
raw files remain unchanged. It confers no new live authority. This profile has
its own96KiB ring/save/collection bound; ordinary and older profiles stay64KiB.
The workflow profile is `intel-gt-bootstrap-v39`; qualification and both exact
staged VM paths are mandatory before the unattended1% physical trial. VM
refusal/save/return evidence cannot accept the native owner or reset.

Development passes both original entry routes with preserved or restored fences,
405 focused adapter fault cases,407 receiver mutations and eight compiled
native weakening controls. The latter must keep a valid run working and fail at
runtime on their targeted fault; compilation failure is not detection. Late
return clock faults retain the independent physical parent. Full frozen native
fault matrices, fatal sanitizers, legacy regressions and exact-stage VMs remain
the launch gates; this document records implementation, not physical acceptance.

Before any physical trial, qualify the new original HDA/GT/requester/prepare
parent and its epoch-aware post-reset source. It must keep CPU aperture clients
stopped through fence restoration, map the exact fence page with fresh UC/NX
resource/alias checks, and independently admit only the documented stores.
Preserve the inherited GGTT/backing/display route and HDA state and capture the
new engine census at the new epoch; do not reuse an earlier census. A full reset
intentionally changes more engine state than the old BCS protocol permits, so
neither its writer nor its preservation predicate may be widened silently.

The native adapter uses a separate exact recipe/image, bounded raw receipts,
strict receiver/checkpoint/retirement gates, exact-stage VM qualification and
durable workflow profile. It must save progress before console exclusion and
make retained failure visible without relying on an invalid CPU fence. Only
then schedule the bounded unattended 1% trial. No reboot is warranted just to
repeat the already-known missing ownership grant. After native bootstrap,
exact interval and DMA/cache/reader ownership are still prerequisites of the
[GPU mapping transaction](intel-command-mapping.md).

## Single-use read admission and measured guard costs

The follow-up keeps the protocol core unchanged. Its load path already brackets
every read callback with full prerequisite admissions. The native callback now
consumes that immediate pre-read admission once, instead of repeating both full
admissions inside the callback. Standalone preparation reads and all write
admissions remain unchanged. This is confined to the exact CPU0/pre-init image:
IRQs and other aperture clients remain excluded, and only the known clock
callback and pure protocol bookkeeping occur between admission and read.

The receipt holds snapshot bytes, never a copied live ownership object. It binds
the original core/source/parent/root/epoch and exact native callback table. The
callback checks the whole snapshot; only one proof increment, one read-attempt
increment and the exact one returned clock sample may differ. Original borrowed
GT/HDA exclusion, root and every original deadline are checked again before the
read. The receipt is consumed before access and invalidated by another full
admission. A stale, replayed, extra-clock, modified-byte, replaced-table or
expired receipt cannot become an alternative admission. The unchanged core
checks its own callback snapshot and performs a fresh full post-read admission
before any subsequent GPU operation. No GPU completion assurance follows.

The full guard retains resource, CPU cache, every actual alias and requester/AF
checks. Five validated-clock cost counters separate original HDA/boot admission,
backing endpoint, cache context, fifteen aliases and requester/AF. They retain
calls, total and maximum duration plus the current incomplete stage; closing
still separately measures guard, borrow return and final guard. Cost sampling is
charged to the existing150/300/400/500ms deadlines. No bound is extended.

Native summary version2 requires a single admission receipt and exactly five
cost rows. Explicit historical version1 parsing remains unchanged and does not
invent the new records. New workflow `intel-gt-bootstrap-v40` freezes wire2;
v39 is historical and cannot start a new run from this source. Wake version37,
the exact boot recipe, fifteen UC/NX aliases and96KiB selected logging remain.
The model performs the same register reads/stores with about43% fewer full
guards; native timing and full retirement require a new physical trial. Tests
cover every receipt byte, single-use/clock/counter faults, component-clock edges,
compiled weakening controls and original failure/closing retention.

## First native original-owner result

Air7,2 source `d24b1ddd` / release `423dc0ae7ced8256` passed all18 frozen local
checks and both exact-stage VM paths, then failed its native150ms preparation
bound at150,005us during final fence verification. Both pulses, one actual epoch
advance and all five cancellations completed; final preservation/closing did
not. The owner remains registered/retained. Complete raw/checkpoint and offline
media/recovery proofs are banked; the run is REJECTED. Next measure and coalesce
repeated nested admissions (1,012 guards/15,180 alias checks in this run), retaining
fresh resource/cache/alias/requester checks and all original deadlines. See the
[dated unit result](machines/macbookair7-2.md#native-full-gt-bootstrap-preparation-timeout).
This is no native completion, managed-memory or tear-free acceptance.

## First sealed-read native result

Source `0d1f7315` / release `4fa2d3302fb0a0c6` passes all20 frozen local gates and
both exact-stage VMs. The native core completes/preserves in104,818us,123,941us
from original preparation entry, with both pulses, exact fences, epoch0→1,
all five cancellations and308 consumed/zero rejected read receipts. No150ms
preparation timeout occurs. However, the new census refuses on the original GT
owner's100ms maintenance phase:120,907us since its last live check. The parent
and aliases remain held; closing/full retirement are untested. The trial is
REJECTED, with raw/photo/qualification and offline recovery/media proof banked.
Next maintain the original GT through existing fresh guarded checks at safe
reset boundaries, preserving its callback table and every original deadline.
See the [unit result](machines/macbookair7-2.md#native-sealed-read-reset-completes-gt-maintenance-check-refuses).
