# Broadwell all-engine prepare, hold and cancellation

The [accepted read-only preflight](intel-takeover-preflight.md#accepted-air72-observation)
observes reset controls; it does not maintain exclusion. This next isolated trial
requests reset readiness on all five captured Air7,2/GT3 engines, observes the
held state twice, then cancels in reverse order. There is no GDRST pulse, command
submission, PTE store or managed GPU allocation. Ordinary desktop startup stays
on the inherited framebuffer.

## Authority and lifecycle

The dedicated image requires its exact 16-token recipe, including
`srvros.intel-engines-prepare=request-cancel` and the existing takeover preflight.
It retains the original Air7,2/1626/GT3 firmware/resource/cache/loader and
cooperative EFI gates. An original address-sealed preflight must have returned
its borrow, share the live parent/epoch and follow a complete fresh two-batch
92-word census. The trial independently checks disabled, zero-geometry rings,
no saved waits or pending invalidations and both parser-idle indications.
Copied or stale prerequisites, another dependent and a foreign parent refuse.

The core registers its own one-use borrow and verifies premature parent close
is blocked without requester stores. Four six-word snapshots cover entry,
two held observations and final cancellation. Each of the five controls at
`0x20d0`, `0x120d0`, `0x1a0d0`, `0x1c0d0` and `0x220d0` receives exactly
`0x00010001` in that order. Only mask bit16 and request bit0 are written. The
first acknowledgement is captured, with bounded additional polling for low
two bits `0b11`. After both all-ready snapshots agree, cancellation writes exactly
`0x00010000` in reverse engine order and waits for low two bits `0b00`. Upper mask halves
remain observations; reserved low bits and floating reads refuse.

The native transition checks current translation and wake requesters, completes
full provider admission and samples the dependent clock immediately before the
store. A fenced DWORD store is followed by fresh requester checks and a first
acknowledgement load. A possibly issued write with an unknown result retains
ownership. No failure path replays a store, forces cancellation or frees a live
borrow. The provider must not replace the dependent deadline with the longer
parent deadline. Full core guards also check clocks and parent identity after
provider validation, before permitting another access.

The complete operation retains the existing 100 ms dependent, 300 ms owner and
500 ms outer limits. Each readiness/cancellation phase, including its first acknowledgement, has a
10 ms/256-poll cap;
those caps do not extend the aggregate deadline. Verified return is included in
the dependent budget. A known-safe return finishing late is incomplete timing
evidence; an unknown write or unverified preservation remains pinned.

## Preservation and receipts

The registered borrow stays held through a fresh two-batch 92-word post census
and exact comparison of all pre/post engine/context words. Two snapshots also
preserve all27 display primary/cursor/sprite words and10 HDA PCI words. These
are equality observations, not cache, outstanding DMA or display-flip retirement
witnesses. No reader or hardware ownership grant follows from equality.

Wake version14 binds13 mapped/retired aliases and the total observed read count.
Only the existing requester page and the five engine control pages are writable;
GDRST, TLB and all three display pages remain RO/NX/UC. Dedicated receipts retain
raw snapshot prefixes, first/final acknowledgements, per-phase polls, held and
cancelled masks, failed clock samples/stage, complete raw post census and
reader/HDA snapshots. Compact census and v2 positional acknowledgement rows preserve every DWORD while fitting
the existing log budget. Strict decoding independently checks coverage, order,
raw preservation, inherited reader binding, clocks and zero unsupported grants.
A summary flag alone cannot accept the operation.

```sh
make -j2 all test-intel-engine-prepare build/intel-engine-prepare/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-engine-prepare
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Follow the [Ubuntu device workflow](ubuntu-device-agent-setup.md) for the actual
local UUID/table, immutable image, backups, recovery, one-shot boot and collection.
No human input is required. Safe saved/unmounted outcomes return automatically;
retained uncertainty or failed persistence halts visibly. Keep volume at1%.
Physical acceptance is pending until this machine's matching evidence is collected.

## Limits and references

This trial qualifies a bounded engine-preparation lifetime only. It does not
perform a full reset or prove posted-write/prefetch/cache/translation/reader
retirement, maintained firmware exclusion or an exclusive GGTT interval. All255
completion obligations remain open. The [managed-memory plan](intel-managed-memory-plan.md)
still requires a separately justified native transfer before its first PTE writer.

Intel's [Broadwell PRM Volume2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
November2015, defines the mask/request/ready fields on printed pages165,288,1704
and1749; VCS1 remains GT3-only. The pinned source SHA256 is recorded in the
[read-only preflight reference](intel-takeover-preflight.md#register-evidence).
Linux v6.12 [gen8 engine reset preparation/cancellation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c)
is a protocol reference. SrvrOS imports no catastrophic-error bypass, non-ready
reset, full-reset request, retry or failure-path forced cancellation.

## First native result

The [Air7,2 trial](machines/macbookair7-2.md) on source`ba7fa11b` completed
all ten request/cancel acknowledgements and raw preservation comparisons, but
exceeded the unchanged dependent deadline:130,444/100,000 us. Preservation
plus its final guard occupied42,175 us after88,269 us elapsed. Reason514
retains the borrow, parent and13 aliases. This is incomplete evidence; no
engine preparation, reset, memory or tear-free acceptance follows. Optimize
measured admission/preservation overhead before retry, keeping all limits.

## Bounded backing-read optimization

The preparation image revalidates the full inherited1356-entry backing at each
existing guard. Its IRQ-disabled path now groups at most64 GGTT entries from
one already-owned page into a synchronous read-only callback. Every entry still
uses high/low/high DWORD loads with torn/floating rejection. The observer claim,
current CR3 and exact physical alias are checked before and after each group;
there is no yield, mapping change or cached hardware sample inside the native
callback. Interrupt-enabled callers keep the scalar path. This optimization is
compiled only into the engine-preparation observer adapter.

Both complete passes, every raw PTE/fence/route/PCI/framebuffer comparison, raw
vector equality, layout/hash checks, periodic clocks and verified map retirement
remain. Each group is bracketed by the same100 ms sample clock; the overall
100/300/500 ms budgets and10 ms phase deadlines are unchanged. The wire format
and load counts are unchanged. This removes repeated per-DWORD callback/software
admission work, not observations or ownership checks. Local fixtures compare
scalar/batch vectors, hashes, load counts and mapping lifetimes at aligned and
page-crossing inherited offsets. Provider tests exercise bounds, partial loads,
torn reads and post-batch IRQ/claim/translation/address-space loss. Native
performance remains unmeasured until the next matching hardware run.

The locally qualified retry is source `6a641caf`, release `ac555dcb12463fa3`.
The full local gate (installed-root omitted),18 images,355 sanitizer modes plus
backing regression,14 semantic controls,19 existing native controls and7 batch
controls pass. Exact-stage VM save/unmount/return and missing-workspace refusal
pass; the unchanged log bound projects65,456/65,536 bytes. All8,136 PTE loads
remain;44 callbacks replace per-word dispatch. This is local evidence only.
The native retry must still meet the original deadlines and preservation checks.

## Native batching retry: still incomplete

Air7,2 source `6a641caf`, release `ac555dcb12463fa3`, records121,296/100,000 us
for the dependent lifetime:9,148 us (7.01%) less than the previous130,444 us,
but still21,296 us over the unchanged limit. Preservation plus its final guard
takes39,266 us after82,030 us elapsed. Both candidates are single observations.
All10 ACKs, both held snapshots, final zero controls and complete raw engine/
context/display/HDA equality verify;144 guards and230 target words remain.
Reason514 retains the borrow, parent and13 aliases. No native preparation or
memory acceptance follows. The287,256 us outer interval is within500 ms; its
start differs from the parent's300 ms clock, which records no fault. Do not
attribute an owner overrun from the outer interval. Continue measured admission/
preservation optimization while retaining every deadline and observation.

## Consolidated native callback admission

The native preparation adapter now gives each successful sample, control load,
transition and preservation callback a full entry and exit backing/endpoint guard.
The core's unchanged before/after checks use fresh IRQ exclusion, observer claim,
current-root, exact alias and requester validation. They still check parent
identity and clocks around the provider. This removes the duplicate full core
census around a provider that already performs those same checks. No successful
full guard is cached or carried to another callback; each provider starts anew.

A failed full exit poisons the adapter permanently and retains native ownership.
The provider still returns the completed load/store prefix, so the core counts
the first ACK before its next guard reports failure. An uncertain write remains
uncertain and is never replayed. Store admission still performs full validation
and the original dependent clock check before the fenced DWORD store; the first
ACK phase budget includes all exit-validation time. The state machine, engine/control/display/HDA observation vectors, each complete
backing census, preservation comparisons and every deadline remain unchanged.

For this image only, wake admission also consumes the endpoint verified at the
end of its full backing census within that same call. Framebuffer/PCH/GPU/CR3
adapter binding is checked, then ASLS/SWSCI are read directly and compared to the
firmware snapshot. This avoids a sixth complete endpoint walk after the five
inside the census. No endpoint result survives the guard call. Other images
retain their existing admission path. Host fault cases cover completed stores
followed by endpoint/backing exit failures, and reject foreign adapters/tokens.
The bounded native preparation lifetime is accepted by the retry below; broader
ownership and repeatability remain unproved.

The locally qualified consolidation retry is source `ee4a15bc`, release
`de0b58f1dfbcdc94`. The complete synthetic path uses144 full guards instead of
158, with all48 raw protocol/preservation rows identical; this is not a native
latency claim. Local qualification passes376 sanitizer modes plus backing
regression,14 semantic/28 native/7 batch controls,18 images and exact-stage VM
save/unmount/return and missing-workspace halt. The subsequent native run,
`session-00065fdb81ba0b28-00`, safely refuses before preparation: BCS is enabled
(mask16/control`0xf001`/start`0x03eef000`), takeover geometry15/quiescent0.
The unchanged quiescence gate issues zero preparation attempts or writes.
The wake lease returns and all13 aliases retire; no clock fault is recorded.
This is not a preparation timing result. Checkpoint/filesystem/recovery checks
pass with all1,410 prior files preserved and three new logs. Review enabled-ring
shutdown prerequisites separately; do not widen preparation admission or infer
reset/managed-memory authority from the stable raw census.

## Native preparation acceptance (2026-11-09)

A fresh retry of unchanged source`ee4a15bc`/release`de0b58f1dfbcdc94`, session
`session-00065fdc32eefa9e-00`, completes after fresh zero-geometry admission in
98,274/100,000us. All10 first/final ACKs match without extra polling, both held
snapshots agree, cancellations restore zero controls, and all92 engine/context
words,54 plane words and both10-field HDA snapshots preserve. Both borrows
return, all13 aliases retire, exclusion releases and no clock fault is recorded.
Outer293,384/500,000us passes; parent elapsed is not serialized. The complete
path records144 guards/230 target words with unchanged100/300/500ms and10ms bounds.

The host collector's missing post-summary dispatch initially produced a false
attention flag. Fix`0594750d` routes that exact summary through the existing
strict completion parser, retaining malformed/incomplete/authority refusal.
Preparation and desktop-collector regressions pass. The original report remains;
full read-only recollection verifies identical raw hashes, no attention and the
unchanged strict native/checkpoint proofs. Prior failures remain failures.
All1,416 prior files preserve, three new logs give1,419 files/129 sessions, and
offline media/recovery/default/one-shot checks pass. Evidence is retained outside
build; volume1%, no reboot armed.

The1,726us margin is narrow: this is one bounded native pass, not a timing or
repeatability guarantee. It accepts no enabled-ring shutdown writer, reset,
managed memory, DMA/prefetch/cache/translation/reader ownership or native tear-free.
All255 memory obligations remain open. Every successor needs fresh admission.

## Nested preservation census boundaries

The next preparation adapter applies the same callback contract to the nested
post-preservation census. Core brackets perform fresh live IRQ/exclusion/claim/
root/all-alias/requester checks and unchanged parent/epoch/clock validation.
Each actual ordered sample still enters a full backing/endpoint guard, checks
exact translations and fresh requesters between groups, and exits through full
validation. A failed exit poisons both preparation and native adapters before
the core accepts the sample; completed control-write/ACK counts remain intact.
Scalar fallback group/context callbacks also have full entry/exit validation.
The generic completion and preparation cores are unchanged.

A same-path host comparison reduces144 full guards to141 with all48 raw protocol/
preservation rows identical and230 target observations. This is a structural
count only. All92 engine/context words,54 plane words, HDA fields, final controls,
registered pins and100/300/500ms/10ms limits remain. The existing version14 wire
format and image recipe are unchanged. Native timing of this change is pending;
the accepted98,274us run belongs to the earlier exact payload.

The locally qualified nested-census candidate is source`292163ac`, release
`0cafc3a5551ab1b2`: full local gate/18 images,380 fatal sanitizer cases plus8
scalar-fallback cases,14 semantic/33 native/7 batching compiled controls, and
exact-stage VM persistence/refusal/missing-workspace checks pass. The16-token
compiled parser matches saved options; unchanged full-width tail projection is
65,456/65,536 including1,536 allowance. The candidate is installed and unarmed
at volume1%; native timing remains pending. Accepted preparation/recovery payloads
and1,419 prior USB files are retained. No reset or memory capability is added.
