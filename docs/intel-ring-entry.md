# State-aware original-owner ring entry

This is a separate v25 diagnostic following the v24
[already-disabled refusal](intel-enabled-ring-transition.md#native-result-already-disabled-entry-safely-refused).
It uses the original static HDA/GT owners and selects an explicit route from the
live full completion census. No source is synthesized and no enabled state is
forced for coverage. The older enabled-only and zero-geometry reset contracts
remain separate and unchanged.

The original census must be complete, stable, source/parent/epoch-bound, parser
idle 3, without pending/flip/TLB flags. Other engines retain the existing exact
zero-geometry idle gate. Only two BCS inputs are eligible:

| Kind | Original BCS start/control | Action | Epoch | Writer evidence |
| --- | --- | --- | --- | --- |
| 1, enabled | `0x3eef000` / `0xf001` | Existing idle-only single fenced `0xf000` control DWORD | Advance before possible issue | Requires the actual one store and complete postconditions |
| 2, already disabled | `0` / `0` | Existing disabled-entry observer with a NULL write callback | Unchanged | Always false, even when the observation completes |

Both routes acquire the original registered child, block early GT closure,
preserve the original immutable 92-word source and take four source-bound BCS8
observations. Kind 2 never registers a BCS writer. Kind 1 retains the existing
final original-source/ownership/native-clock checks immediately before its sole
store. Disabled nonzero geometry is not eligible: it cannot stand in for an
owned transition or pass the old reset gate.

After BCS verification, both routes require a fresh full 92-word census under
the same original parent and their actual resulting epoch. All engine/context
words must preserve, except the enable bit for kind 1. No start, length, control
geometry or context is cleared. Two full plane/HDA endpoint vectors preserve
all 54 plane DWORDs and both ten-field PCI snapshots, with original inherited
primary/cursor binding and HDA Command 2. Post-data and original post timestamps
remain bound through the last guard and return.

BCS and original source admission age remain 100 ms; entry including child
return and reader preservation remains 150 ms; GT 300 ms, HDA 400 ms and outer
500 ms remain unchanged. The source-age admission must finish within the
original 100 ms on both routes; fresh post-proof does not refresh that origin.
The first failed clock sample is retained. After either verified BCS route,
failed postconditions or terminal timing retain the original root, even if the
logical child already returned. An early known unmodified refusal may retire
only with intact original source/provider and valid cleanup clocks. Objects
and providers survive reporting frames; no retry or copied ownership object
can recover an uncertain result.

The `build/intel-ring-entry/srvros.elf` image uses its own firmware request,
exact 15-token `srvros.intel-ring=entry-v1` recipe and `ring-entry-preinit` marker.
It maps the same 14 original joint pages; only the wake-request and BCS control
pages may be writable. Other engines, GDRST, planes and GGTT are read-only.
Ordinary startup gains no optional AML, submission or writer. Stage with
`tools/hardware_boot.py stage --laptop-probe-volume <local-UUID> --intel-ring-entry`
using the normal [Ubuntu workflow](ubuntu-device-agent-setup.md).

Wake v25 and distinct `intel-ring-entry-*` records cannot be combined with older
wire versions. The v2 entry summary includes `kind`; complete kind 1 requires
one store/epoch advance, complete kind 2 requires zero store attempts and no
epoch advance. Source, BCS, fresh post-census, readers and HDA phases must form
one ordered chain. The collector records `native_complete` for that specific
entry route and separately records `native_enabled_writer_complete`. A modeled
chain or successful disabled observation never supplies physical enabled-writer
acceptance.

This milestone ends at entry, preservation and clean retirement. No reset,
requester/translation/cache/firmware retirement, interval claim, managed backing
or memory grant is added. All 255 broader obligations remain. Future reset
integration must consume the original live objects with valid source/epoch/
geometry rules; neither a serialized receipt nor a completed previous boot is
live authority.

The guest shows progress without requesting input. Safe saved and unmounted
outcomes return automatically to Ubuntu. Retained uncertainty halts visibly.
Before any physical trial, qualify both actual adapter routes and their faults,
old-contract refusal, image isolation, native access widths/permissions,
2 KiB frames, the unchanged 64 KiB tail and exact-stage VM save/unmount/return.
Physical acceptance remains pending until the matching native session is
collected; a disabled-route pass must explicitly leave the enabled writer
unexercised.

## Qualification checkpoint

Source `c150c6ad`, release `dd91b264c73a48a6`, passes the full 30-image local
build/USB/host/core/docs gate, fatal actual-adapter ASan/UBSan fault matrix,
117 further native-clock faults and 11 compiled omission controls. Both
routes pass the full synthetic native collector chain while remaining distinct
from physical acceptance. Historical native formats replay unchanged. Image
isolation, exact staged instruction widths/fences, original static lifetimes,
14-page permissions, 2 KiB frames and the 57,031/65,536-byte projected saved tail
with 1,536 bytes of reserve pass. Exact-stage EFI QEMU/MBR save/unmount/safe return and
missing-workspace halt pass. The [unit record](machines/macbookair7-2.md) records
the scope, test-only provider-reason correction and preserved recovery/media.
Candidate is durably banked, uninstalled and unarmed. Physical entry and enabled
writer acceptance remain pending; there is no reset or memory grant.
