# Original-owner state-aware entry and zero-geometry reset

This is a separate v26 diagnostic after the accepted
[already-disabled entry](intel-ring-entry.md#native-result-already-disabled-route-accepted).
It keeps the original static HDA/GT owners and composes that entry's actual fresh
post-census with the existing takeover/preparation/reset/re-arm/claim/preservation
pipeline. Air7,2 accepts the exact bounded native route recorded below.
The [machine record](machines/macbookair7-2.md) owns physical results; the earlier v23 [composition](intel-entry-reset-composition.md)
and v25 entry remain separate exact-source contracts.

The entry core still selects from the full live initial census. Enabled/nonzero
kind 1 may perform its existing guarded disable and epoch advance, but cannot
advance to this reset pipeline. The old zero-geometry gate is unchanged. Kind 2
uses its existing NULL-writer observation, keeps epoch 0 and requires all four
BCS8 observations, fresh full 92-word census and both complete reader vectors.
Neither source is forced and no ring geometry is cleared.

The new handoff validates the original entry object, returned registered child,
unchanged parent/provider/epoch, all source and post data, original timestamps,
BCS words and preserved reader vectors. It passes the address of that same
`entry.post` object directly into the original takeover child. It copies no
owner, census ownership object or borrow. Takeover registers its own child,
blocks early closure and must return before preparation starts. The existing
preparation core consumes the original takeover and census under its own source,
epoch, geometry and registered-borrow rules. After reset advances the epoch, the
old entry is immutable history, not a new live capability.

BCS and original entry source age remain 100 ms; entry/readers 150 ms; the whole
handoff/takeover and its fresh source age 100 ms; preparation/reset 100 ms; GT
300 ms, HDA 400 ms and outer 500 ms. A fresh measured post-census has its own real
time; no origin is rewritten to buy time. Source/provider/clock/return failure
after eligible handoff retains the root, including a late clock after takeover's
logical return. The new-image takeover receipt also retains its original last
clock, and all three takeover stamps remain bound through the final guard/time.
Adjacent entry-end/handoff-begin clocks may tie while remaining monotonic.
Nonzero entry refuses reset and retires only through its existing
verified entry/reader/HDA cleanup. No receipt or successful prior boot can start
a later reset.

`build/intel-ring-reset/srvros.elf`, `intel_ring_reset_firmware_request`, the exact
22-token `srvros.intel-ring=reset-v1` recipe and `ring-reset-preinit` marker are
unique. Stage with `tools/hardware_boot.py stage --laptop-probe-volume <local-UUID>
--intel-ring-reset` using the [Ubuntu workflow](ubuntu-device-agent-setup.md).
The original joint 14-page mapping and existing engine-control/reset destinations
are retained; no new MMIO destination or GPU PTE/requester writer is introduced.
Ordinary boot gains no optional AML, writer or diagnostic owner.

The entry uses distinct `intel-reset-entry-*` records and a single
`intel-reset-admit:` summary. New-image post-census vectors encode every DWORD
as exactly eight hexadecimal digits. Reader vectors encode all 37 DWORDs per
sample as canonical little-endian base64. Both preserve partial progress and
expand into the established typed validators; mixed legacy rows, missing or
duplicate records, noncanonical encoding and incomplete raw coverage reject.
The old entry and reset wire formats are unchanged.

The collector joins both full censuses, all BCS and reader data, handoff source
stamps, takeover return and the entire original joint reset graph. A completed
entry alone never makes the composed test pass, and kind 1 cannot claim reset.
Modeled native chains remain model evidence. Physical acceptance requires a
matching saved prerequisite/final checkpoint, full preservation, clean retirement,
prior-file/offline allocation checks and Ubuntu recovery.

The guest displays progress and requires no input. Safe saved outcomes return
automatically; retained uncertainty halts visibly. Qualification must cover the
actual adapter, all callback/source/clock/return faults, original static lifetimes,
compiled omission controls, old-contract isolation, exact native access widths and
permissions, 2 KiB frames, the existing 64 KiB tail with 1,536 reserve, and exact
staged EFI VM save/unmount/return and missing-workspace halt. No deadline or log
limit is extended. Firmware/DMA/cache/translation/reader retirement, managed backing
and native tear-free remain open: obligations 255, assurances 0, managed pages 0.

## Local qualification

Source `9e67bce7` / release `74d46f42447d218e` passes the full 31-image
local gate, all 12 fatal ASan/UBSan actual-adapter tests, 54 additional
native-clock faults, 16 compiled omission controls, historical native replay,
image isolation and exact staged EFI QEMU refusal/save/return and
missing-workspace checks. Object and linked instructions preserve original
widths/fences, static lifetimes, unchanged reset/re-arm/map code and 2 KiB
frames; the new handoff core has no MMIO or writer imports. The projected
saved tail is 65348/65,536 bytes with the unchanged 1,536-byte reserve.
These are host/VM proofs. At qualification the candidate was banked, uninstalled
and unarmed. The subsequent native result below accepts the bounded composition;
all remaining memory ownership is pending. Prior dd91 acceptance is entry-only. See the machine record for
exact coverage, recovery and the next automatic native trial.

## Native result: original-owner entry/reset accepted

Air7,2 source `9e67bce7` / release `74d46f42447d218e`, session
`session-0006600e477bf0c3-00`, completes kind 2 entry and the original-owner
zero-geometry reset/re-arm/claim/preservation lifetime. Entry uses zero stores
and epoch 0; the existing reset advances epoch 0→1. All raw census, BCS, reader,
takeover, preparation, reset, re-arm, claim, HDA and prerequisite/final bindings
verify. All 14 GPU aliases/HDA root, registered children and CPU exclusion retire.

Entry is 90,760/150,000 us, handoff 31,895/100,000 us with
fresh-source age 50,985/100,000 us, preparation 91,213/100,000 us,
HDA 318,012/400,000 us and outer 368,571/500,000 us. Limits remain
unchanged. Zero clock faults/collector alerts, full file preservation, offline
filesystem/allocation and Ubuntu recovery pass. The [unit record](machines/macbookair7-2.md)
owns exact evidence and the later main integration's separate local validation.
This is one bounded pass. Enabled entry writing, nonzero-geometry reset,
managed memory, remaining ownership obligations and native tear-free remain open.
A saved result cannot authorize a later reset or memory operation.
