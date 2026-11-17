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
The current workflow profile is `intel-gt-bootstrap-v41`; qualification and both exact
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


## Original GT maintenance before reset

Summary wire3 / workflow v41 adds one `intel-bootstrap-maintain` version1
receipt. At the original core's EPOCH callback, all32 fences have been freshly
captured and verified, all five readiness controls remain held, and no reset or
restoration store has been attempted. The native adapter checks the unchanged
original parent/binding/table, then invokes **`intel_gt_access_check` on the
original live GT object**. That API performs its original guarded requester/
ACK/thread snapshot, independent final guard and checked clock progress. Its
100ms maintenance phase and300ms total origin are unchanged. No direct `last`
assignment, copied owner or substituted callback supplies maintenance.

A full native guard and exact source/table/borrow/epoch checks follow the call;
only then may the existing epoch advance and first reset pulse occur. The full
original backing guard is never called during deliberately transient reset or
fence restoration. The next original live check occurs naturally at entry to
the restored new-epoch census. The whole added cost stays inside preparation150ms,
HDA400ms and outer500ms. Failure retains the original physical pins and aliases.

The maintenance row records begin/end, original phase and before/after snapshot
counts. Wire3 completion requires exactly one successful snapshot before reset
settling, strict monotonic order and a maximum100ms original phase. Version1 and
version2 logs retain separate historical parsing without invented rows; the
workflow freezes version3 and refuses new v39/v40 runs. No GPU memory, DMA/cache,
reader completion or tear-free authority follows from lease maintenance.

The actual-adapter host model includes an over100ms whole trial, original phase/
total/zero/backward clock refusals, requester drift, root/epoch/borrow/table
changes and a compiled check-removal control. Model success is not hardware
timing acceptance. Matching native qualification and retirement remain required.


### Native maintenance succeeds; closing budget remains incomplete

Air7,2 v41 source320df4fb/release8b73 now verifies the original live service in
3,170us with a49,434us maintenance phase and one actual snapshot12→13. Full reset,
exact preservation, one epoch, new92-word census and requester restoration all
complete. The clock immediately after logical borrow return instead reaches
153,113/150,000us preparation: pre-close guard ends at149,945us and return costs
3,168us. Final guard is not reached, physical parent/aliases remain retained and
the run is rejected. Next reduce closing/admission cost with margin for the final
guard, keeping every original clock and independent check. This is no full-owner
or native tear-free acceptance. See the
[unit result](machines/macbookair7-2.md#native-original-gt-maintenance-passes-return-deadline-refuses).


### Firmware identity cost without a retained admission

The closing follow-up fuses the existing Air DSDT checksum and FNV computations
into one fresh full byte stream. The exact35,034-byte length, DSDT header/length,
modulo256 checksum and captured64-bit FNV constant remain. Other ACPI resource
and Pro display matching keep their existing algorithms. No firmware copy,
cached identity, skipped byte or guard, callback, allocation or hardware access
is introduced. All original owner/epoch/requester/cache/alias checks and all
100/150/300/400/500ms bounds remain. Wire3/v41 stays unchanged because the receipt
predicates and protocol I/O are unchanged.

An independent original two-pass reference checks the actual matcher and digest
against this unit's captured table: every35,035 prefix, every35,034 byte mutation,
494 checksum-preserving changes, malformed headers/lengths and unknown tables.
Fatal ASan/UBSan and three runtime-detected compiled digest weakenings are required.
Host timing compiles without SIMD on x86, alternates method order and records
results without accepting native timing or making wall-clock speed a flaky gate.
Initial host samples are60–62us two-pass versus45–47us fused. Native closure
and timing still require the matching hardware run; the earlier rejected raw
receipts remain unchanged.

The frozen profile now adds a firmware-identity job at both development and
qualification (22 total jobs). Its captured DSDT is a hashed job input, remains
private/ignored and must come from this actual Ubuntu host. Generic
`make test-laptop-firmware-digest` uses generated unknown tables without a private
fixture; no native acceptance follows. See the
[workflow setup](device-workflow.md#separate-full-gt-owner-and-epoch-trial).


### Native fusion result and compiler correction

Air7,2 aa022502/9298 remains rejected at152,475/150,000us immediately after
logical return. Core/reset/preservation, original maintenance, new92-word census
and requester restoration pass; final guard/physical retirement are incomplete.
The guard before return ends at149,318us (682us left); return costs3,157us.

The earlier GCC/no-SIMD host timing is not representative of this kernel's
Zig/Clang output: the old installed checksum was already SIMD-vectorized. Actual
kernel-compiled old/new objects both measure about46us per call on Ubuntu. The
serial FNV dependency remains; observed native endpoint reduction753us is
insufficient and has no repeatability claim. Future performance qualification
must inspect/measure the actual kernel compiler and flags before another trial.
Pure witness/reference tests remain valid. Keep exact fresh identity and all
bounds while addressing serial hashing or redundant guards. See the
[unit result](machines/macbookair7-2.md#native-firmware-fusion-is-insufficient-for-closing).

### Separate multi-second diagnostic

The user requested three runs with aggregate timeouts of a few seconds to
measure the complete lifetime before choosing practical limits. This is the
separate `build/intel-gt-seconds/srvros.elf` image, exact
`srvros.intel-ring=gt-bootstrap-seconds-v1` recipe and workflow profile
`intel-gt-seconds-v42`. Ordinary images retain their original timing policy.

| Clock | Original full-GT policy | Diagnostic policy |
| --- | --- | --- |
| Preparation, including return/final guard | 150 ms | 3 s |
| Original GT maintenance phase | 100 ms | 3 s |
| Original GT total | 300 ms | 4 s |
| Original HDA total | 400 ms | 5 s |
| Original outer total | 500 ms | 6 s |

Individual hardware waits remain 10 ms with 256-poll caps. Source-age, bounded
census, identity, ownership, preservation and retirement checks remain in place.
No timestamp is refreshed to manufacture a younger parent. Separate GT-access,
HDA-owner, joint, bootstrap, boot and probe objects select the diagnostic policy;
ordinary builds do not link these relaxed objects.

Owner wire4 requires an exact `intel-bootstrap-timing` policy row and the actual
original GT/HDA/outer closing timestamps. The receiver checks each parent from
its original origin, retains the existing full native chain, and reports
`diagnostic_complete` separately from `native_complete`. Wire1–3 retain their
original policies. The new recipe is refused by the old image and vice versa.
Diagnostic completion cannot be passed to ordinary workflow acceptance. It is
closed with `diagnose`, preserving actual durations and comparisons with the
original bounds; no native tear-free or GPU-memory authority follows.

The host matrix includes a complete modeled lifetime over the former limits,
fatal ASan/UBSan, policy and closing-clock mutations, late-return/clock failures,
image isolation and all original default/core/legacy gates. Model and exact-stage
VM completion still do not establish physical completion or repeatability.
See the [bounded three-run workflow](device-workflow.md#multi-second-diagnostic-series).

### Three native wide-policy lifetimes complete

Air7,2 runtime `2a61bed1` / release `2281c7cc04bacba7` completes three native
kind2 lifetimes under this diagnostic policy. Each run repeats all26 frozen
jobs and both exact-stage VMs, saves/unmounts and returns automatically, with
matching raw/checkpoint, filesystem and recovery evidence. All32 fences,
display/backing, two pulses, five cancellations, requester restoration,
new92-word census and original parent/15-alias retirement verify; epoch0→1.

Preparation takes153.558–153.882ms, original GT313.256–313.681ms,
HDA332.965–333.396ms and outer384.835–385.264ms. The100ms original GT phase would
cover all observed maintenance intervals (49.038–49.193ms), while150ms
preparation and300ms GT total would fail all three. This is repeatable diagnostic
completion beyond the original limits, with no observed multi-second delay.
All255 GPU ownership/completion obligations remain open and ordinary timing
acceptance stays false. The normal image and wire1–3 policies remain unchanged.

A practical next policy candidate is250ms preparation,100ms maintenance phase,
500ms GT,750ms HDA and1s outer. It has not been implemented or qualified by
these runs. Individual operation waits, source age, identities and failure
retention must remain intact when selecting the next policy. See the
[unit evidence](machines/macbookair7-2.md#three-multi-second-gt-diagnostic-runs-complete).

### Measured practical policy candidate

The follow-up implements the measured policy as a separate
`build/intel-gt-practical/srvros.elf`, compiled with
`SRVROS_INTEL_GT_PRACTICAL`, exact
`srvros.intel-ring=gt-bootstrap-practical-v1` recipe and owner wire5.
Preparation250ms, original GT maintenance100ms/total500ms, HDA750ms and outer1s
are checked from the original origins, including logical return and final guard.
All individual operation waits, poll/source-age limits and identity/ownership/
preservation/retirement checks remain. Selecting both timing variants, or
selecting a variant without the original full-GT native graph, fails compilation.

Wire5 requires the exact timing-policy row and actual original parent closing
timestamps. Ordinary wire1–3 and seconds wire4 remain unchanged, including
receiver result shapes. The new image refuses the old recipes and vice versa.
Completion is a separate practical diagnostic, with no original timing,
managed-memory, DMA/cache/reader or tear-free grant. The old three-run controller
continues to accept only the seconds profile; this candidate uses a single
manifest-driven trial.

The candidate retains all26 original/seconds qualification jobs and adds
practical-model/policy and image gates at both levels (30 total). The actual
adapter model completes a158ms preparation under the250ms budget, both entry
routes, and refuses zero/backward/late clocks, stale owners and failed closing
checks. Tests include fatal ASan/UBSan, all three image/recipe pairings, exclusive
header gates, changed policy/parent clocks and exact replay of the three saved
native seconds reports. Qualification must also pass both exact-stage VM paths
before a new native run. See the [workflow](device-workflow.md#practical-gt-timing-candidate).

### Native practical policy completes

Air7,2 runtime `b7752e6f` / release `33d9e5dacde0d12b` now completes one native
kind2 lifetime under wire5. All30 frozen jobs and both exact-stage VMs pass.
The checked one-shot handoff boots at1%; the ordinary collector verifies the
matching saved/unmounted return. Preparation153910µs, GT maintenance49162µs,
GT total313696µs, HDA333403µs and outer385267µs fit250/100/500/750/1000ms.
Preparation retains96090µs margin, with1579µs final guard and3157µs borrow return.

Both pulses, all five cancellations, exact32-fence/display/backing preservation,
requester restoration, fresh92-word census and epoch0→1 verify. The fence state
already matches the preserved values, so changed/restored masks are both zero;
this is the preserved-fence path, not a new native test of fence-rewrite recovery.
The original parents and all15 actual aliases retire cleanly. Raw/checkpoint,
all58 banked files and USB/recovery/default/one-shot checks pass.

This is one completed practical diagnostic. Original-policy acceptance remains
false; enabled-entry practical hardware coverage, repeatability and all255
GPU memory/DMA/cache/reader obligations remain pending. No managed GPU page,
PTE write, submission or native tear-free authority follows. See the
[unit result](machines/macbookair7-2.md#practical-gt-timing-diagnostic-completes).

### Same-owner post-reset memory preparation (wire6)

The separate `build/intel-gt-memory/srvros.elf` image selects
`SRVROS_INTEL_GT_MEMORY_PREPARE` and the exact
`srvros.intel-ring=gt-bootstrap-memory-v1` recipe. Workflow profile
`intel-gt-memory-v44` freezes wire6; it uses the already measured practical
250ms preparation / 100ms maintenance phase / 500ms GT / 750ms HDA / 1s outer
policy. Individual hardware phases and poll caps stay unchanged. One bounded native
completion is recorded below; broader physical coverage and GPU ownership
acceptance remain pending.

After full reset and fence restoration, a registered child binds the **original**
GT borrow and new engine epoch while GPU requester command3, the HDA hold and
CPU/firmware exclusion remain active. It checks the actual GT lease before the
first post-reset census, then creates two exact supervisor/NX UC aliases. It
runs the existing [graphics-remapper drain](intel-vtd-drain.md) and
[System Agent GGTT invalidation](intel-ggtt-invalidation.md) cores on this graph.
It does not replay a receipt or copy an owner from the older BCS/source graph.

Every access independently checks original source/callback/progress identity,
requester/HDA/resource/root, the CPU cache context and actual UC alias after its
last clock. Success requires a second stable92-word census, exact prior engine
and context words, actual GT maintenance and reverse alias retirement. The parent
checks the terminal child independently before requester restore, logical borrow
return and final completion. Unknown stores, changed policy or failed retirement
keep exclusion and the remaining mappings pinned. No retry is attempted.

`intel-memory-*` receipts retain original bindings/clocks, ordered core contexts,
all256 remapper poll slots (including zero tail), both operations' attempt and
completion counts, actual alias/cache attributes, maintenance and final census.
The strict wire6 decoder derives counts/order and refuses a missing child or
foreign older recipe. Wire1–5 result shapes and historical timing evidence remain
unchanged. The raw log limit remains96KiB. Qualification keeps all30 prior jobs
and adds native composition, operation-core and image gates at both levels
(36 total), followed by both exact-stage VM paths.

Completion proves these two narrow operations in the full-GT owner's lifetime.
AT00 requests already processed by the remapper and a System Agent WO trigger
plus posting read do not establish GPU pipeline/prefetch/cache completion,
firmware/display-reader retirement, a GPU interval or PTE authority. All255
whole-device obligations, zero managed GPU pages and zero PTE writes remain.
Private ring/context/mapping ownership and tear-free scanout are subsequent work.

#### Provider boundary qualification

During wire6 qualification the candidate passed all30 prior gates, then the exhaustive child
matrix injected an epoch change at a successful alias callback. The next resource
callback was reached before ownership was rechecked, so qualification stopped
before installation or boot. The adapter now validates identity before and after
translation/alias callbacks, between alias and policy checks, and before and after
resource/cache callbacks. A failed resource admission no longer invokes the cache
callback. The original clocks and operation/poll limits are unchanged.

The early development gate now exercises every alias/epoch edge before legacy
qualification. Compiled weakening controls must first demonstrate clean refusal
by the valid adapter and success on an unmodified positive. A positive cache model
also exercises variable and fixed MTRR rows; CPU word widths are checked explicitly
by the decoder. Complete qualification and native composition evidence remain
required for the corrected candidate.

### Native full-GT memory child completes

Air7,2 runtime `c86685d8` / release `434e31fd8123e5bd`, session
`session-00066073af2a582b-00`, completes one kind2 wire6 original-owner lifetime.
All36 frozen slots and both exact-stage VMs pass before the1% trial. The two
operations complete in37.044ms (remapper, one poll) and2.323ms (System Agent),
with exact cache/alias identity, a matching second census and both actual aliases
retired before requester/parent return. The entire child takes83.188ms;
preparation219.728/250ms and largest GT maintenance77.601/100ms fit unchanged
practical clocks. Original requester/GT/HDA and all15 original aliases retire.
All705 banked files, raw/checkpoint reparse, prior-file/offline integrity and
recovery/consumed-one-shot checks verify; the run is closed as `DIAGNOSED`.

This is narrow native operation evidence, without managed GPU mapping/context/
submission, pipeline/cache/all-reader assurance or tear-free acceptance. All255
whole-device obligations remain open. See the [unit result](machines/macbookair7-2.md#native-full-gt-memory-child-completes)
for exact clocks, scope and remaining physical coverage.
