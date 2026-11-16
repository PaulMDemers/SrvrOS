# Separate Broadwell full-GT reset and fence preservation

`intel_bdw_gt_bootstrap` is a host-qualified reset/preservation protocol for a
future fresh graphics owner. No native adapter or image calls it. It cannot be
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

Before any physical trial, implement the new original HDA/GT/requester/prepare
parent and its epoch-aware post-reset source. It must keep CPU aperture clients
stopped through fence restoration, map the exact fence page with fresh UC/NX
resource/alias checks, and independently admit only the documented stores.
Preserve the inherited GGTT/backing/display route and HDA state and capture the
new engine census at the new epoch; do not reuse an earlier census. A full reset
intentionally changes more engine state than the old BCS protocol permits, so
neither its writer nor its preservation predicate may be widened silently.

The native adapter needs a separate exact recipe/image, compact raw receipts,
strict receiver/checkpoint/retirement gates, exact-stage VM qualification and
durable workflow profile. It must save progress before console exclusion and
make retained failure visible without relying on an invalid CPU fence. Only
then schedule the bounded unattended 1% trial. No reboot is warranted just to
repeat the already-known missing ownership grant. After native bootstrap,
exact interval and DMA/cache/reader ownership are still prerequisites of the
[GPU mapping transaction](intel-command-mapping.md).
