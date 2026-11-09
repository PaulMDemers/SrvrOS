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
