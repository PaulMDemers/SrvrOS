# MacBookAir6,1 battery/AC bring-up

Live SrvrOS battery/AC telemetry is not enabled yet. The first v60 boot captured
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
