# Air6,1 display/HDA ownership and recovery design

Design checkpoint: 2026-11-03, integration base `5b2bf3c7`. Milestone A now has a diagnostic-only software
model; the hardware transition operations below remain proposed and unavailable. The native diagnostic payload is
`9b485ca12b4c2482`, compiled from `728bcbeb`. Expected no-write refusal,
software retirement and the first tiled framebuffer readout are physically observed. Policy, limits and recovery are
unchanged. See the [observations and access contract](ap-ownership-diagnostic.md#version-7-display-hda-read-only-capture)
and [unit record](machines/macbookair6-1.md).

## Starting evidence and remaining gaps

The two actual endpoints show pipe A / eDP / DDI A, fitters disabled and eDP
input A_ON, clear graphics-audio outputs, masked/disabled sampled display IRQs,
and the BIOS well request set. Display HDA 00:03.0 `8086:0a0c` / `106b:011a`
is D0 with decode and bus mastering enabled. GCAP describes two output streams;
GCTL is 1; INTCTL, CORBCTL, RIRBCTL and DPLBASE are zero, and both stream RUN
bits are clear. These are non-atomic snapshots, not an ownership transfer.

The switchable-well domain map excludes pipe A/eDP/DDI A, but includes fitter A
and audio references. The inherited route therefore looks compatible with a
future unused-well release; this remains an inference, not a grant or proof.
Captured ULT excludes CRT and port D. Do not add sibling/general Haswell probes.
RC6 timing still fails its original bound and package C3 is zero; neither tells
us that the BIOS request is the cause.

The present graphics stub enables PCI decode/bus mastering and maps four pages.
The Limine framebuffer remains live through the console and desktop. There is
no adopted scanout/domain graph, display-HDA owner, modeset recovery path or
proof excluding firmware interference. A saved set of control words fills none
of those gaps.

## Ownership contract

One boot-scoped kernel owner must serialize domain use and all future control
writes. This is separate from compositor/window ownership and from the analog
`8086:9c20` CS4208 service. Closing a desktop or observing no audio process must
not drop the kernel's references. No ordinary application can request this
transition or become the MMIO owner. Existing diagnostic pages stay read-only;
future control access needs a separately reviewed, exact write allowlist and
owned aperture. Do not cast away read-only access or reuse a generic initializer.

Proposed owner data consists of an immutable captured profile, monotonically
allocated epoch, state, per-domain reference counts, immutable adopted-route
identity, in-flight operation count and a retained failure record. Tokens include
owner and epoch; stale, duplicate, wrong-owner or wrong-state operations refuse.
Acquisition pins the owner until the operation and hardware completion retire.
Refcount overflow/underflow refuses without altering hardware. The proposed
release eligibility is all of the following:

- Exact model/firmware, CPU/GPU/PCH/HDA identity and all existing access gates.
- An adopted and pinned active route, framebuffer and its backing lifetime.
- A held initialization/guard reference while discovery and adoption run.
- No unclaimed users or concurrent setters, including audio, AUX/hotplug and
  diagnostic consumers; unsupported ownership stays unknown and refuses.
- Explicitly owned, quiesced display HDA and established IRQ/DMA retirement.
- No switchable-domain references, queued work or in-flight callback remaining.
- A validated restoration target, bounded recovery and independent evidence path.

The owner lock protects short publication/refcount/state changes only. Do not
hold it, a GUI lock or an IRQ-masked section while waiting for MMIO, vblank,
filesystem writes or worker retirement. A timeout retains the owner, references,
mappings and any possibly hardware-owned memory. It never manufactures an idle
state, releases alien buffers or lets another epoch reuse uncertain resources.
Firmware returning or changing a request invalidates the operation; no automatic
fight with firmware, repeated clear loop or inferred firmware exclusion is allowed.

## State machine and refusal behavior

The hardware states and transitions in this table remain proposed. The implemented
dry run models observations/references and retirement only, with no control writer.
Ordinary startup must not enter a transition path.

| State | Required action/evidence before leaving | Allowed next state |
| --- | --- | --- |
| Observed | Exact profile and source-bound snapshots; no authority inferred | Adopted or Refused |
| Adopted | Pin immutable current route/framebuffer and software domain users; perform no hardware writes | Guarded or Refused |
| Guarded | Acquire actual MMIO authority, hold well on, observe driver request/state, then transfer firmware request only under an established takeover contract | HdaOwned or RetainedFault |
| HdaOwned | Exclusively bind display HDA, freeze new users, verify access and controlled restore target | Quiesced or RetainedFault |
| Quiesced | Stop only owned activity, complete controller/link/IRQ retirement and reconcile every domain reference | Eligible or RetainedFault |
| Eligible | Recheck original access gates, zero users/in-flight work, preserved route and recovery readiness | WellOff or Restoring |
| WellOff | One allowlisted release and bounded state observation; no dependent MMIO while off | Restoring or RetainedFault |
| Restoring | Hold well on and verify first, restore only owned resources in dependency order, validate display/HDA and evidence | Restored or RetainedFault |
| Restored | Controlled target verified; release only temporary references, retain boot owner | End of bounded trial |
| Refused | Before first write: save reason, leave firmware state unchanged, use existing save/return path | End of trial |
| RetainedFault | After any uncertain write/completion: retain ownership/memory and avoid further unsafe access | Bounded proven restoration or recovery reboot |

A transition cannot skip states. A stale source/profile, active foreign DMA,
nonzero domain user, live callback, changed BAR/well, missing restore contract,
unknown firmware writer, unavailable timer or evidence failure is a refusal.
Unexpected activity after the first write is a retained fault, not a pre-write
refusal or a pass. A failed state read must never be decoded as powered off.

Guarded is a real implementation milestone, not a software flag. Pinned
[Linux well takeover](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_display_power_well.c)
asserts the driver request before clearing the BIOS request; this ordering keeps
the well powered, but does not by itself establish SrvrOS's authority or complete
its dependency graph. Its
[domain initialization](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_display_power.c)
holds INIT through resource readout and reference establishment. SrvrOS must
supply equivalent ownership prerequisites before using either write pattern.

## HDA shutdown and the restoration target

The first future mutating profile must refuse any initial command/position DMA,
stream RUN, unsupported interrupt ownership or unclaimed live producer. It must
not stop another owner's activity, inspect/free firmware DMA buffers, replay
firmware ring/BDL pointers, or clear status merely to obtain a quiet snapshot.
Existing read-only gates remain mandatory immediately before and after each
accessible stage. Analog audio and brightness remain independent services.

After actual ownership is established, the proposed order is freeze submissions,
retire owned work, disable and observe owned command/stream/position DMA and
interrupt sources, enter display-HDA link reset with bounded observation, then
release its audio domain reference. This is not an instruction to call ordinary
`audio_init()` or `hda_core_reset()` as a probe: those initialize/reset hardware
and have additional effects. Any status acknowledgement needs its own owned
source, explicit access review and retained pre-ack evidence.

Pinned [Linux HDA shutdown](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/sound/hda/controllers/intel.c)
stops the chip, enters link reset and clears pending IRQs before releasing display
power. The [graphics audio component](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_audio.c)
holds an AUDIO_PLAYBACK reference. These are distinct from ELD/output-enable
readbacks and from masking display IRQs.

The [HDA 1.0a specification](https://www.intel.com/content/dam/www/public/us/en/documents/product-specifications/high-definition-audio-specification.pdf)
sections 4.2–4.3, 5.5 and 7.3.3.10 describe reset, discovery and codec state
persistence. Codec settings are not guaranteed to persist through reset. Restoring
GCTL to 1 and copying old controller words therefore cannot claim restoration
of the original firmware codec state. A future owner must define its supported
post-reset codec/controller state and validate discovery and any required codec
configuration. It must not quietly present a normalized reset state as a
bit-for-bit firmware restore.

For the first future write trial, the target must be explicit: unchanged visible
eDP route/framebuffer and brightness behavior, plus an owned, known quiet display-
HDA configuration with DMA/interrupts still disabled. Audio playback is a separate
milestone. If this controlled target cannot be established without unsupported
codec operations, refuse the write trial. A recovery reboot remains required
on restoration failure and is reported as failure, never successful restoration.

Restore order is well-on request and verified state, owned clocks/resources
without touching the preserved active route, HDA reset exit/discovery and controlled
configuration, IRQ enablement only if an actual owner requires it, then final
route/display/ownership verification. Access to switched-domain registers while
the well is off is forbidden. Re-read/reconcile live state; do not bulk replay
an old register image, reserved bits, W1C status or stale DMA addresses.

## Bounds, evidence and independent recovery

The 32-attempt, 20 ms masked C3-round bound and HLT fallback stay unchanged.
HDA reset/recovery is outside measured C3 rounds and sparse power hooks, with
interrupts available. Existing HDA waits cap at 100,000 us and 2,000,000 polls;
reuse or tighten applicable bounds, never extend them to make a trial pass.
Specification minimum settle/discovery times are separate from timeout ceilings.
Well transition deadlines need an explicit hardware-contract review before
implementation; do not borrow a larger reset limit or retry after expiry.

Before a write trial, park and flush the USB workspace through its existing
ownership protocol, save a start/progress receipt, and establish a retained
failure record plus an evidence/return mechanism that does not depend on the
possibly lost framebuffer or audio/well domain. A RAM record alone is not durable;
a screenshot alone is not independent recovery. Validate a platform-appropriate
bounded reset/Ubuntu return path before relying on it. If that path or proof of
safe subsequent USB use is missing, refuse before the first write.

Final receipts must identify source/release/profile, epoch, state/transition,
pre/post raw values, reference/in-flight counts, operation/whole-phase timing,
fault and restore outcome. Keep partial/failure records; do not acknowledge or
free uncertain hardware state just to finish logging. Preserve the unchanged
64 KiB retention and source-fingerprint gate. Power/temperature and residency
comparisons use the same workloads and limits only after restoration succeeds.
A valid zero package-C3 result is not a reason to relax the transition gates;
RC6's separate original timing failure remains a failure.

## Implementation sequence and acceptance gates

| Milestone | Concrete deliverable | Required verification |
| --- | --- | --- |
| A: adoption dry run | Boot-scoped immutable profile/epoch, reference ownership and eligibility state machine; no hardware-control writer | Host fixtures: exact captured case, every unknown/changed/active/stale/refcount/in-flight refusal, cancellation and retained lifetime; write callback count always zero; matched VM refusal and source-bound native receipts |
| B: readout completeness | Bind the active route and backing framebuffer lifetime to actual hardware; explicitly classify unsupported domains and firmware/takeover assumptions | Positive/negative readout fixtures, invariant and retention checks; native read-only comparison; no hardware write permission from this step |
| C: guard and HDA owner | Exact gated well-on ownership plus controlled HDA shutdown/restore, with no well-off operation | Mock posted-write/timeout/interleaving/reset-change failures, quarantine/lifetime and restore checks; matched VM and separately bounded native qualification |
| D: single well-off trial | One release/restore cycle only after A–C and independent recovery pass | Refusal/fault matrix, exact-stage rehearsal, durable run/recovery records, native display survival and restoration; unchanged latency/ownership/integrity limits |

Milestone A passes host/VM checks and the source-bound native expected-refusal
and software-lifetime capture. It retains unknowns and cannot acquire hardware
authority. The broader platform trial still fails the unchanged RC6 bound;
package C3 remains zero. Milestone B readout/backing-lifetime work is next.
Milestones C/D require a separately reviewed explicit write allowlist and
completed recovery/codec contract; this design does not supply either. No
physical trial or new power policy is enabled by this document.


## Milestone A implementation contract

`display_owner_core.c` is a pure, allocation-free diagnostic model with no I/O
or writer callback. One boot pool mints monotonically increasing epochs; it must
not be cloned or reset during the boot. An owner copies its profile, including
all 96 observed-data words. Token identity binds object address, epoch, slot and
monotonic slot serial. Reusing a slot or retired object cannot revive an old token.
The bounded ledger has eight route/audio slots and an internal software guard;
these counts do not measure unknown firmware or other kernel hardware users.

Callers serialize access on CPU0. This is not an SMP locking primitive or an
untrusted-client security API. Closing refuses new references/operations, retains
busy references until completion, and retires only after release/drain. A fault
retains the guard and prevents reuse even after software users drain. Overflow,
underflow, duplicate/wrong-owner/stale tokens and invalid states refuse.

The immutable assurance mask has seven bits: backing lifetime, known domain
users, HDA ownership, IRQ ownership, restore target, independent recovery and
firmware exclusion. All seven remain **unknown** in the native adapter. Only
synthetic host cases may supply the full mask to exercise the predicate. A zero
predicate result still grants no write authority and enters no hardware state.
Changing either assurances or an observed word requires a new, validated lifetime;
it is not an in-place upgrade to an existing token.

The adapter consumes existing resource/audio/IRQ/HDA snapshots only, outside
measured rounds and sparse sampling. It adds no MMIO reads or hooks to ordinary
startup. It checks active control bits with ULT applicability, including fitter
forced-through routing. It opens an Observed lifetime, compares the second
immutable snapshot, retains refusal reasons and closes its own empty software
ledger. Expected native refusal is mask 127 (all seven missing assurances),
Observed → Retired, guard 1 → 0, no software users/in-flight operations, and
hardware authority zero. This is successful dry-run evidence, not adoption of
the actual framebuffer or permission to release a domain.

Version 8 adds one `ap-platform-dry: v=1 data=...` record: a canonical base64
encoding of 28 little-endian bytes (`uint64 epoch`, `uint64 profile digest`,
`uint16 refusal mask`, then ten bytes: before/after state, before/after software
guard, route/audio reference counts, in-flight count, assurance mask, active mask,
and hardware authority). The digest identifies all 96 words, using FNV-1a in
little-endian word order; it is not authentication. The strict host decoder
recomputes it and active/change context from independently required raw records.
It rejects malformed/duplicate/missing/authority-claiming records. Earlier
versions remain readable; version-8 final coverage requires the expected refusal
and every earlier raw proof, without relaxing the original RC6 predicate.

Host tests exercise unknown/active/changed profiles, every assurance bit, wrong
identities, bounded references, serial/epoch exhaustion, ABA/stale/wrong-owner
refusal, cancellation/drain and retained faults. The actual adapter/emitter is
checked for no extra hardware reads and field-by-field wire preservation.
Sanitizer checks cover the pure owner; host decoding and conservative tail tests
cover the matched schema. Runtime source/isolation and VM/native validation
remain required before physical acceptance. No well/HDA ownership or restore
milestone is granted by these tests.


## Milestone B first readout (native endpoints observed)

The isolated version-9 capture records primary-A geometry/surface/live surface,
PCI aperture context, inherited framebuffer descriptor, per-page mapping
continuity and firmware classification at both endpoints. See the
[exact readout contract](ap-ownership-diagnostic.md#version-9-primary-a-and-inherited-framebuffer-readout).
It adds no mappings, GGTT/PTE/data reads, control writes or assurance bits.
Readout refusal and ambiguous ranges remain explicit. A compatible linear
aperture relation still cannot prove GGTT/stolen backing or an adopted lifetime.
Milestone B remains incomplete until these dependencies are resolved; A's native
software retirement is not a lifetime release of actual scanout storage.


The native version-9 comparison on source `728bcbeb`, release
`9b485ca12b4c2482`, has two stable complete readouts and 1,056 contiguous
inherited aperture pages with matching geometry/firmware FRAMEBUFFER coverage.
The primary plane reports tiled mode (`0x98004400`), so the linear relation is
correctly unbound. These endpoint observations do not identify the GGTT/stolen
backing, interpret fences, pin real storage or exclude firmware changes between
endpoints. All seven assurance bits remain unknown. Next is a read-only tiled
aperture/GGTT backing audit; B is not complete. The broader platform trial still
fails the unchanged RC6 bound and package C3 remains zero. See the unit's dated
native result for source/session/hash, integrity and recovery evidence.
