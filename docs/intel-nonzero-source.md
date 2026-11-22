# Original live post-reset source binding

The separate opt-in v29 `intel-nonzero-source` image extends the Air7,2
[v28 nonzero reset](intel-nonzero-reset.md) with a registered read-only child.
It binds the actual original epoch-2 post-census and preserved display/HDA
readers to the original candidate GGTT entry. This is source observation,
not re-arm, interval claim, requester inhibition, managed memory or tear-free
presentation. Native acceptance is pending.

## Original lifetime

The child runs only after both reset pulses, settling, cancellation and full
92-word post-census/37-reader preservation, before the original reset returns.
Its source is the same live reset object, actual post-census address, original
entry and HDA/GT root. The exact post geometry is either the preserved vector
or the strict reset-default vector accepted by v28. Copies and stale epochs
cannot supply this source. The legacy kind 2/zero-geometry APIs stay unchanged.

A single registered dependent and monotonic borrow/return counts block reset
return. Lost registration, source, clock, mapping or a partial/changed read
makes the observer fail; the enclosing reset retains its original root and
aliases. There is no forced unregister, cleanup, retry or release after unknown
state. A returned child is historical data and supplies no later live authority.

The child shares the original preparation **100 ms** origin and original GT
**300 ms** origin. It advances their common time watermark. HDA 400 ms and outer
500 ms remain fixed. Its read/return times stay inside that same preparation
budget; no new observation budget is started. The accepted v28 lifetime had
5,949 us remaining, so the six loads are batched within one hardware-validation
bracket. Every DWORD still has exact source/mapping validation and a final
same-origin clock immediately before the load. Direct source checks after that
clock perform no further callback or hardware access.

## Exact observation

The original read-only window snapshot supplies the existing candidate at
GGTT index 8704. Its raw value is data: a zero or populated entry does not
establish that an address is usable. Under the existing pinned RO/NX/UC alias,
the adapter performs `0x811004`, `0x811000`, `0x811004`, twice. Both high/low/high
vectors must equal the exact original entry. No mapping/PTE/config/geometry
store, cache operation or hardware grant is added. All 255 obligations remain;
assurances and managed pages stay zero.

The strict receiver binds the raw entry to the original window vectors,
actual post-state/mode/epoch, complete native reset, read prefixes, final read
clocks and original return/retirement. It keeps v28 receipts distinct; an old
receiver must not accept v29 as the old image. Generic nested-capability alerts
remain raw evidence, separate from dedicated protocol completion.

## Build and trial

Build `build/intel-nonzero-source/srvros.elf`. Stage with
`python3 tools/hardware_boot.py stage --laptop-probe-volume LOCAL_UUID --intel-nonzero-source`.
Its own exact 15-token recipe uses `srvros.intel-ring=nonzero-source-v1`, its own
firmware request and `nonzero-source-preinit`. No user services start.
The collector field is `intel_nonzero_source`; the new typed rows are
`intel-nzsource-read` and `intel-nzsource`. Mixed profiles and old recipes refuse.

Follow the [device workflow](ubuntu-device-agent-setup.md) for immutable staging,
exact-stage VM, prior-file hashes/offline integrity, recovery, one-shot boot and
matching collection. The guest shows progress and requires no input. Keep
volume at 1%. A proved safe saved/unmounted result returns automatically to
Ubuntu; uncertain ownership or persistence halts visibly. A VM identity refusal
checks persistence/recovery, never native source reads or timing acceptance.

Frozen source `a9418c83` / release `451cd4d6a9f36faa` is locally hardware-ready.
The 34-image/core/USB/host/docs gate, seven-test fatal ASan/UBSan native-adapter
suite, 780 additional clock faults, seven historical native replays, image/
receipt isolation, native object/staged-source inspection, unchanged writer/
legacy-guard/map checks and 2 KiB frames pass. Projected saved tail is
61,089/65,536 bytes including reserve. The existing 81 baseline-known strict
allocation/repair gaps remain documented. Exact-stage EFI/MBR VM tests verify
zero-store refusal, persistence, automatic safe return and missing-workspace
halt. None of these accepts native target reads or timing.

## Native result: already-disabled source safely refused

Air7,2 source `a9418c83` / release `451cd4d6a9f36faa`, session
`session-0006601c19f940ac-00`, observes kind 2/epoch 0 with already-disabled,
zero BCS geometry. Original entry completes in 97,092/150,000 us without stores.
The nonzero-reset child correctly refuses the absent kind 1/epoch 1 source
before admission or target operations; the source child does not enter and its
six target reads remain unperformed. Exact recipe, prerequisite/final checkpoint
and strict collector verify `completed=NO`: **incomplete, not native acceptance**.
The differing initial state supplies no source timing data and does not authorize
an unchanged retry or widening the legacy kind 2 gate.

HDA 6/2/2/6, 14 GPU aliases, HDA mapping and CPU exclusion retire cleanly.
Prior-file hashes, three exact new logs, boot digest, offline filesystem and
recovery/Ubuntu default/consumed one-shot verify. Current v29, accepted v28 and
recovery are retained; no reboot is armed. Raw logs and strict proof are banked
outside build storage. See the [Air7,2 unit record](machines/macbookair7-2.md).
All 255 obligations remain, with zero assurances/managed pages; source binding,
re-arm/claim/requester and native tear-free remain pending.
