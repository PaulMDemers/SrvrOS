# Original state-aware startup reset and source

The opt-in v30 `intel-startup-source` image handles both startup states actually
observed on Air7,2. It binds the original live entry, reset and read-only GGTT
source child under one original HDA/GT root. It grants no managed GPU memory,
requester inhibition, re-arm, claim or tear-free presentation. One native
already-disabled route is accepted below; enabled-route acceptance remains pending.

## Explicit routes

| Original entry | Required captured state | Entry stores | Reset epochs | Source epoch |
| --- | --- | --- | --- | --- |
| Kind 1 | Enabled BCS; exact inherited START `0x03eef000`, CTL `0xf001`, then disabled CTL `0xf000` | One enable-bit clear | 1 → 2 | 2 |
| Kind 2 | Already-disabled BCS; exact zero START/CTL, unchanged remaining engine/context vector | Zero | 0 → 1 | 1 |

Selection uses the actual original completed entry object, not a saved receipt,
copy, refreshed census or invented zero geometry. Full initial/post vectors,
original timestamps, returned entry borrow and preserved 37 display/HDA words
must match their route. The selected kind is captured before admission and bound
to the call identity. Route/mode/epoch drift cannot switch paths mid-operation.
Unsupported or incomplete entry refuses before reset admission. Lost continuity
after eligible admission retains the original root.

The new entrypoint is `intel_startup_reset_observe`, available only in the separate
compile variant. Existing `intel_nonzero_reset_run`/`observe` remain kind 1 only,
even when built into v30. Old zero-geometry admission/reset helpers and their
predicates are unchanged. The existing reset/source implementation is shared;
conditional route fields and validation are absent from old image layouts.
An older image is not made permissive by a new boot token.

## Original lifetime and limits

Both routes register one original reset child, inhibit its parent close, sample
two admission endpoints, prepare all five engines, issue two BCS8 reset pulses,
settle at least 50 us, cancel in reverse order and preserve the complete 92-word
engine/context census and all 37 readers. The enabled route permits only the
existing whole-preserved or strict reset-default postcondition; the disabled
route must preserve its original zero geometry. Software never clears or restores
geometry to manufacture eligibility or preservation.

The registered source child runs after that preservation and before reset return.
It binds the actual post-census address, original entry/root and original GGTT
candidate snapshot. Under the existing RO/NX/UC alias, it reads
`0x811004`, `0x811000`, `0x811004` twice. All six raw DWORDs must match the original
candidate; zero/populated data does not establish an allocatable address.
Final clocks and direct source/mapping/liveness checks precede every load.

Entry 150 ms, admission/source age 100 ms, preparation 100 ms, each request/reset
phase 10 ms/256 polls, original GT 300 ms, HDA 400 ms and outer 500 ms remain fixed.
The source shares preparation/GT origins and advances their time watermark;
selection never restarts a deadline. The new image rechecks the complete pure
route/source predicate after its final store clock, before the existing fenced
DWORD store. No provider callback or MMIO operation is inserted between that
clock's pure checks and the store.

Registration/borrow counts block enclosing reset return while the child is live.
Partial/changed reads, clock loss, route/source drift or failed return retain the
original root and aliases. No forced cleanup, unregister or retry follows unknown
state. A returned child supplies historical observation, never later authority.
All 255 memory/cache/translation/reader obligations remain; assurances and managed
pages are zero.

## Separate image, recipe and collector

Build `build/intel-startup-source/srvros.elf`, then stage with
`python3 tools/hardware_boot.py stage --laptop-probe-volume LOCAL_UUID --intel-startup-source`.
Its exact 15-token recipe uses `srvros.intel-ring=startup-source-v1`, own firmware
request and `startup-source-preinit`; no user services start. Mixed storage,
legacy GPU, desktop or held-on-failure options refuse. Ordinary boot and old
image recipes remain unchanged.

The collector field is `intel_startup_source`. A distinct `intel-startup-route`
row binds selection, original entry kind and all entry/reset/source epochs.
The existing full `intel-nonzero-*` and six `intel-nzsource-read`/`intel-nzsource`
rows are accepted for this image only through its explicit version-30 receiver;
old v28/v29 receivers refuse it. The strict receiver requires the route row,
complete source and enclosing reset, original checkpoint identity, reader and
HDA preservation, mapping/owner retirement and matching native prerequisites.
A successful zero-entry path must not claim its enabled entry writer ran.

Follow the [Ubuntu device workflow](ubuntu-device-agent-setup.md): freeze source,
qualify the exact stage in VM, independently bank payload/evidence, verify prior
USB files/offline media/recovery, install and record a one-shot run, then collect
the matching session before another run. Trials need no human input and stay at
1% volume. The guest shows progress; safe durable save/unmount results return to
Ubuntu, while uncertain ownership or persistence halts visibly. VM success and
safe refusal do not accept either native route.

## Local qualification

Frozen `82dd3da8` / release `327a401a69ca3855` passes the full 35-image/core/USB/host/
docs gate, eight-test actual adapter suite under fatal ASan/UBSan (both routes
and every reset/source clock point), eight historical native contract replays,
image/receipt isolation, native object/staged DWORD/alias/lifetime inspection
and 2 KiB frames. Saved tail is 61,194/65,536 bytes including 1,536 reserve.
The existing 81 baseline-known strict allocation/repair gaps remain documented.
Exact staged EFI/MBR VM tests prove zero-store foreign-board refusal, checkpoint
identity, persistence/unmount/automatic return and missing-workspace halt.

Prior-file/offline media and recovery checks passed before installation, with
payload and qualification evidence independently banked outside build. Only the
unreferenced older d260 EFI duplicate was retired after full durable-bank,
accepted-evidence and boot-reference checks. That local capacity decision does
not authorize deleting another host's recovery or accepted payloads.

## Native result: original already-disabled route accepted

Runtime `82dd3da8` / release `327a401a69ca3855`, session
`session-0006601f0dd19f83-00`, completes one native kind 2/zero-entry lifetime:
zero entry stores/epoch 0, reset 0→1 and the registered original epoch-1 source
child returned before its parent. Six read-only DWORDs match the original
candidate in 3,456 us; source scope is 3,464 us. Preparation is
97,434/100,000 us (2,566 us margin), original GT return 263,671/300,000 us,
HDA 300,188/400,000 us and outer 354,630/500,000 us. No deadline changed.
Full census/readers, HDA 6/2/2/6, four inspections, borrows, 14 GPU aliases,
HDA root alias and CPU exclusion preserve/retire cleanly.

The first collector rejected shared completion-census image binding; it omitted
v30 dispatch. Collector-only `c0dcda2e` adds explicit v30/exact-recipe dispatch
without widening old entry helpers. Both modeled native chains and complete
collection/corruption paths now have regressions. All 31 affected host tests
and eight historical accepted/refused native classifications pass. Strict replay
of the unchanged native bytes validates prerequisite/final provenance and
complete source/reset/outer retirement. Original errors and five raw nested
capability alerts remain archived; the alerts do not grant the unclaimed
capabilities they describe. No installed kernel or hardware behavior changed.

All prior files preserve except expected workspace metadata; three logs yield
1,494 files/154 sessions. Offline fsck/allocation/writable/headroom, boot digest,
current/recovery banks, Ubuntu default and consumed one-shot verify. Raw evidence
and proof survive `make distclean`; no new reboot is armed. See the
[unit ledger](machines/macbookair7-2.md#native-v30-original-already-disabled-resetsource-accepted).
This accepts only the actually observed already-disabled route and one bounded
lifetime. The enabled v30 route, requester/memory/cache ownership, managed pages
and native tear-free remain pending; a saved receipt never supplies future live
authority. All 255 obligations and zero assurances/managed pages remain.
