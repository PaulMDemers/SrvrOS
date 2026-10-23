# MacBookAir6,1 battery/AC bring-up

Live SrvrOS battery/AC telemetry remains physically unaccepted; the isolated
five-sample runtime candidate is described below. Ordinary startup stays off. The first v60 boot captured
passive firmware handoff state with AC connected on 2026-10-23. It is a prerequisite for an
independent EC/SCI/AML runtime profile, not a battery percentage test. The
Air7,2 runtime gate and ordinary desktop startup remain unchanged.

## Captured evidence (2026-10-23)

The fresh DSDT/ECDT/FADT are byte-identical to this unit's October 22 capture.
The Air6,1 DSDT is 31777 bytes, FNV-1a `af9a47bc352dc6bf`; GPU `8086:0a26`,
LPC `8086:9c43`. ECDT names `\_SB.PCI0.LPCB.EC`, UID 0, ports `62/66`, GPE
`4e`. FADT describes SCI 9, PM1 status/enable/control `1800/1802/1804`, and
GPE0 status `1880..188f` / enable `1890..189f`. Legacy and extended fields
agree. There is no second PM1 or GPE block. All numbers here are hexadecimal
except table length, UID and SCI.

Offline loading of the DSDT and eight SSDTs with private GNVS (`8cd3ea90`,
length `26d`) and SMBus PCI `0:0:1f.3` snapshots matches EC HID `PNP0C09`, UID,
GPE and the exact two one-byte IO descriptors. There are zero interpreter errors,
denied accesses, snapshot writes or remaining allocations. No namespace
initialization was requested. Initial runs without these snapshots refused;
those logs are retained, not treated as successful initialization.

The DSDT exposes Darwin SMB0 `ACPI0001`, `_EC=2010`, and SBS0 `ACPI0002`,
`_SBS=1`. `_GLK` is absent, but EC fields request Lock and `_REG` writes ECSS
and updates AC/lid/display state. Query methods also affect AC, graphics and
notifications. This contract match does not authorize full initialization,
query dispatch, Global Lock behavior or copying Air7,2's NVS/PCI/GPIO permissions.

Ubuntu reports AC online, battery present/Full, capacity 70%, 8.423 V, current
0 A, charge 3.540 Ah, full 3.637 Ah, design 5.100 Ah, and 500 cycles. These are
Linux observations, not SrvrOS gauge acceptance. Relative/full/design measures
must stay distinct; do not assume all percentages have the same denominator.
Raw tables, snapshots and replay logs stay in ignored evidence storage.

## Passive native boundary

`laptop_air61_power_resources` requires the exact unit DSDT, GPU/LPC IDs,
validated ECDT and exact read-domain FADT agreement. Its gate is separate from
the native battery runtime. On success, the preflight reads three 16-bit PM1
registers, EC **status only** at `66`, and 32 GPE status/enable bytes. Reading
these status registers does not acknowledge their W1C bits. No EC data read,
EC command, event acknowledgement, GPE enable change, ACPI mode transition,
AML execution, battery transaction or interrupt ownership is performed.

Unknown hardware refuses before any port access. The log identifies a complete
36-read/zero-write capture; the collector requires the header, all 16 ordered
GPE pairs and completion marker. A refusal or incomplete capture is attention,
not native acceptance. Generic QEMU must refuse and still save/unmount/return.

## Build and first boot

```sh
tools/host-env.sh make -j2 all usb-image test-core test-desktop
build/tests/laptop-resources-host build/hardware/macbookair6-1-2026-10-22/acpi air61
python3 tools/hardware_boot.py stage --laptop-probe-volume 6ADA-85BB --battery-preflight
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table gpt
```

Keep AC and the prepared USB connected. This is an automatic diagnostic: no
unplug/replug or desktop actions. It saves boot/exit logs, cleanly unmounts USB
and returns to Ubuntu. If it stops instead, photograph the diagnostics; missing
completion is incomplete evidence. Ubuntu remains default and the accepted
console and telemetry payloads are retained with independent backups.

After return, collect the matching release, inspect PM1/EC busy and all enabled
GPE sources, and verify prior USB files plus offline filesystem/allocation.
Then complete the model-specific initialization/NVS/GPIO/query/rollback audit
before attempting five bounded AC-connected battery samples. AC transitions,
ordinary desktop telemetry, accuracy, low-charge behavior, charging control,
lid policy and suspend remain later milestones.

## First native result

Release `9b528e09c13e1d4b`, session `00065e8886bf8b00-00`, completed the
36-read/zero-write capture. PM1 control/enable were zero, status `8001`, EC
status zero; only GPEs `66` and `72` were enabled, with neither pending at the
snapshot (hexadecimal). Other masked status bits were set and left untouched.
Matching logs, Ubuntu return, prior-file preservation, offline filesystem and
allocation checks passed. This accepts passive preflight only; live telemetry
and the runtime ownership audit remain pending.

## Connected runtime audit and bounded sampling candidate

The separate `acpi-air61-battery-kernel` binary is selected only by v61
`--battery-air61`. It shares the existing owner, transport, Global Lock,
SCI/event lifecycle and sampler; it does not add Air6,1 to the Air7,2 binary or
ordinary desktop. Platform admission requires the exact captured DSDT and all
eight distinct SSDTs, GPU `0a26`, LPC `9c43`, HDAU `0a0c`, SMBus `9c22`, and
ECDT/FADT agreement. Unknown, missing, duplicate or changed tables refuse.

Fresh GNVS ASLB and GPU ASLS agree at `8cd18190`. The Linux i915 OpRegion export
is 8192 bytes, `IntelGraphicsMem` version 2 with mailbox 1. GNVS is
`8cd3ea90/26d`; CpuPm declares **PPMT `8cd1bf90/3a`**, not Air7,2's PNVS.
The native reader requires containing ACPI-NVS ranges, a 64-byte FACS with free
Global Lock, OpRegion header agreement, RCBA/HPTC sanity and idle EC. Firmware
MADT describes IO-APIC 2 at `fec00000`, GSI base 0, SCI override IRQ9→GSI9 flags
`d`; the shared owner still requires a free masked route and validates readback.

Connected offline initialization used those private memory/PCI snapshots and a
256-byte synthetic EC. All four lid/AC combinations (EC byte `60` values 0–3)
pass initialization, allowed queries and detach: zero errors/denials/leaks.
Initialization writes 14 private memory bytes in total, plus ECSS zero at EC
`10`; reads of EC `60` obtain lid/AC bits. Allowed-query replays add six memory
writes. These traces are models, not native battery samples or live ownership.

| Domain | Air6,1 native permission |
| --- | --- |
| GNVS / PPMT / OpRegion | Reads within exact captured spans, subject to live ACPI-NVS checks |
| Memory writes | OSYS bytes GNVS+0/+1; boolean LIDS/PWRS at +15/+16; CLID at OpRegion+`1ac` as boolean low byte and three zero upper bytes |
| FACS | Existing exact Global Lock DWORD and bounded shared arbitration/release contract |
| AML EC | Byte read only at `60`; byte write only zero to ECSS `10` |
| SMBus | Existing serialized SBS read-word sampler at bank `20`; no charge/control writes |
| AML PCI | SMBus DWORD `20`; HDAU vendor bytes 0/1; LPC routing bytes `60..63`, `68..6b`; no PCI writes |
| AML GPIO / chipset MMIO | Denied: the Air6,1 connected audit requires no GPIO or HPTC AML access |
| Query dispatch | `_Q20`, `_Q21`, `_Q40`, `_Q5A` only; zero means no query |

`_Q10` has a shared SMBus-writing non-Darwin branch and is refused even though
its Darwin-only offline replay succeeds. `_Q80` is device policy and its offline
replay reaches denied power-limit SystemIO at `310..319`; it remains refused. Air7,2-only
`_QCE/_QCF/_QD0` and unknown queries are refused. A refused query in this first
native diagnostic invokes existing containment; it is not ignored or treated as
successful telemetry. No Home service/query-recovery ownership is admitted.

The runtime entry guard requires the diagnostic owner, automatic recovery,
parked/unmounted USB, no active image or storage I/O, and neither service nor
transition mode. The bound is five samples. Before ownership, foreign GPE66/72
are retained under the shared event leases; they are masked temporarily without
acknowledging their status. Initialization, ACPI transition, EC/SCI ownership,
Global Lock release and retirement necessarily perform audited hardware writes.
“Read-only telemetry” means no battery charging/control writes, not zero
controller/initialization writes. GPIO policy stays fully denied on this model.

On success the runtime stops new work, drains/retires its worker, disconnects
EC, restores prior mode/masked SCI route and foreign enables, and verifies zero
interpreter allocations/errors/denials. Only then may the outer capture remount
USB to save logs. A contained fault resets with USB parked and can leave no exit
log; that is a failed/incomplete trial, never a pass.

```sh
tools/host-env.sh make -j2 all usb-image acpi-air61-battery-kernel test-core test-desktop
python3 tools/hardware_boot.py stage --laptop-probe-volume 6ADA-85BB --battery-air61
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table gpt
```

Leave AC and USB connected. Do not unplug AC during this first bounded sample
trial. It should return automatically to Ubuntu. The evidence must contain five
fresh, coherent samples, balanced attach/detach, both foreign-event restorations,
clean completion and matching saved logs. Physical gauge calibration, AC
transitions, long-lived desktop monitoring and charge controls remain unaccepted.

## Bounded battery/AC runtime return (2026-10-23)

Release `674c597b082f2494` (compiled `308e4bd`), matching session
`00065e88fbc08db1-00`, returned without an exit log or five-sample completion
receipt. The collector flags the missing exit. Saved `boot.log` and
`session-a.log` end immediately before the unattended battery probe; they do
not establish entry, sampling, retirement or the cause of the interruption.
This trial is incomplete/failed acceptance, not a native battery pass.
Whether recovery was automatic or manual awaits the user's observation.

All 199 prior USB files remain; only `.srvros/meta` changed and the two matching
logs were added. Offline exFAT and allocation checks pass: 201 files, 377 clusters,
zero leaks, bitmap mismatches or errors. Ubuntu returned on a new boot, with
empty one-shot and installed payload/recovery hashes verified. Evidence and
updated run records are retained outside build. No reboot is armed.

Next work is to locate the stopping point using the screen observation and
bounded diagnostics while keeping USB parked during uncertain EC/SCI ownership.
Do not widen firmware permissions or enable desktop battery startup based on
this result. The existing host/VM passes remain regression evidence only.
