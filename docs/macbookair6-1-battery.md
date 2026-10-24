# MacBookAir6,1 battery/AC bring-up

Short parked SMBus sampling and bounded attended AC unplug/reconnect are
physically accepted on this unit, including two observed alarm acknowledgements
and clean runtime retirement. A bounded minute of battery sampling alongside
Text saves/reads in an exclusive USB image is also accepted. Battery gauge accuracy, broader alarm behavior,
sustained operation and desktop monitoring remain unaccepted. The
isolated five-sample runtime and short SMBus profiles are described below.
Ordinary startup stays off. The first v60 boot captured
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
writes. The replay also reads the HPTC DWORD at `fed1f404` (value `80`), which was mistakenly omitted from the initial native permission list. These traces are models, not native battery samples or live ownership.

| Domain | Air6,1 native permission |
| --- | --- |
| GNVS / PPMT / OpRegion | Reads within exact captured spans, subject to live ACPI-NVS checks |
| Memory writes | OSYS bytes GNVS+0/+1; boolean LIDS/PWRS at +15/+16; CLID at OpRegion+`1ac` as boolean low byte and three zero upper bytes |
| FACS | Existing exact Global Lock DWORD and bounded shared arbitration/release contract |
| AML EC | Byte read only at `60`; byte write only zero to ECSS `10` |
| SMBus | Existing serialized SBS read-word sampler at bank `20`; no charge/control writes |
| AML PCI | SMBus DWORD `20`; HDAU vendor bytes 0/1; LPC routing bytes `60..63`, `68..6b`; no PCI writes |
| AML GPIO / chipset MMIO | GPIO denied; opaque RCRB mapping `fed1c000/4000` permits only a 32-bit read at HPTC `fed1f404`. All other addresses, widths and every write denied. Native retry pending. |
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
The user confirms automatic recovery; the screen reset too quickly to capture.

All 199 prior USB files remain; only `.srvros/meta` changed and the two matching
logs were added. Offline exFAT and allocation checks pass: 201 files, 377 clusters,
zero leaks, bitmap mismatches or errors. Ubuntu returned on a new boot, with
empty one-shot and installed payload/recovery hashes verified. Evidence and
updated run records are retained outside build. No reboot is armed.

Next work is to locate the stopping point using the screen observation and
bounded diagnostics while keeping USB parked during uncertain EC/SCI ownership.
Do not widen firmware permissions or enable desktop battery startup based on
this result. The existing host/VM passes remain regression evidence only.

## Bounded failure screen

Terminal automatic battery faults now take over the emergency console after
masking sources and disabling interrupts. The final screen retains the failure
reason, setup stage/elapsed time, worker completion/in-flight state, interpreter
errors/denials and EC phase/error. Photograph that screen during the 30-second
pause; automatic Ubuntu return follows. The shared reset interlock still forbids
reset with mounted storage or active I/O. There is no attempted USB logging,
EC cleanup, scheduler yield or resumed worker during the pause. The pause uses
300 interrupt-independent 100 ms clock waits; a failed clock wait ends it early.
Successful samples and cleanly returned refusals use the existing saved logs.
A reset outside this containment path may still have no visible diagnostic.

## Failure-display native return (2026-10-23)

Release `b9a2d264ad3a0dd2` (compiled `453e063`), session
`00065e8929e2f258-00`, has boot/session-a logs but no final report. The user's
photo now identifies terminal containment during `namespace-initialize`:
`denied map address=0xfed1c000 bytes=16384`, operation region `\RCRB`, while
executing `\_SB.PCI0.LPCB.HPET._STA`. The screen shows errors=4, denied=1,
EC phase=6/error=0, queries=0, no SCI deliveries, worker completed=2/in-flight=0,
and the photograph prompt before automatic reset. It confirms the diagnostic
screen is visible on this unit; exact physical pause duration is not measured.
Battery sampling and clean retirement remain unaccepted.

The captured DSDT's HPET `_STA` reads HPAE in the DWORD at RCRB+`3404`
(`fed1f404`). The original connected replay already explicitly allowed and
logged this read with value `80`; the earlier claim that HPTC AML access was
unneeded was an audit-summary/native-policy mistake. The Air6,1 gate currently
excludes both the opaque RCRB mapping and that exact DWORD read. Next scope is
to admit only the captured read, test width/address/write refusals and replay
initialization; do not permit arbitrary RCRB accesses or writes. No code gate
has been widened in this closeout.

All 201 prior USB files remain; only `.srvros/meta` changed and two matching
logs were added. Offline exFAT/allocation pass: 203 files, 380 clusters, zero
leaks/mismatches/errors. Installed and independent payload/recovery hashes pass;
Ubuntu returned on a new boot with empty one-shot. Photo, logs and return records
are retained outside build. No reboot is armed.

## HPTC retry native return (2026-10-23)

Release `15f29b76df51bab0` (compiled `9316f8a`), session
`00065e895200284c-00`, has boot/session-a logs but no final report. The user's
photo shows namespace setup completed and the runtime reached `phase=sample`.
The final interpreter counters are errors=0/denied=0. This confirms the narrow
HPTC fix resolves the previous initialization denial on this unit; it does not
accept sampling or clean runtime retirement.

The first observed SCI query is decimal 16 (`0x10`), classified as OS-branch
class 5 by Air6,1 policy 3 and refused before dispatch. Terminal reason is
`query policy`. The query's Darwin branch notifies SMB0; its alternate branch
acquires SMTX, reads SMBus state and can clear SSTS. The existing refusal is
intentional; do not simply whitelist the AML method or assume a captured OSYS
value makes every reachable branch safe. The shared runtime already has a
separately owned SMBus host-event handler, but Air6,1 does not yet admit that
profile. Next scope is an Air6,1-specific status/completion/alarm audit and
bounded event-owner integration with tests, retaining exact firmware and storage
gates. No event permissions are changed by this closeout.

All 203 prior USB files remain, with only `.srvros/meta` changed and two matching
logs added. Offline exFAT/allocation pass: 205 files, 383 clusters, zero leaks,
bitmap mismatches or errors. Payload/recovery hashes and Ubuntu default pass;
Ubuntu returned on a new boot with empty one-shot. Photo, logs and return records
are backed up outside build. No reboot is armed.

## Air6,1 short host-event capture audit (v62)

The captured DSDT explicitly defines SMB0 `ACPI0001`, `_EC=2010`, SBS0
`ACPI0002` and `_SBS=1`, with no EC/SMB0 Global Lock request. The native contract
checks host ID, bank/query, SBS interface and absent/zero locks before use. Its
ECOR fields place SPTR/SSTS at `20/21`, SADR/SCMD at `22/23`, the data buffer at
`24`, and alarm address/data at `45/46/47`. `_Q10`'s non-Darwin branch confirms
alarm bit `40`, shifted address and zero-status acknowledgement. The same exact
DSDT/eight-SSDT/resource gates remain required. This is independently captured
Air6,1 evidence, not inheritance of Air7,2 physical acceptance.

v62 (`--battery-air61 --battery-smbus --laptop-probe-volume <unit-uuid>`) uses
the existing short event capture and strict collector proof. It waits at most
30 seconds for a natural event and fresh generation 3 or later, with a 128-event
cap. The owner requires automatic recovery, the exact parked volume, no mounted
workspace, image lease or active storage I/O. Five-sample v61 remains separate;
ordinary service, Home, transitions and file coexistence stay refused on Air6,1.

Query10 is handled directly by the existing OS event helper, never AML `_Q10`.
Completion-only/spurious events read idle protocol/status without writing. An
alarm copies address and both data bytes, rechecks idle protocol and unchanged
status, then acknowledges with the sole zero write to SSTS. Known senders 9–11
invalidate the cache and require a newer sample; unknown senders remain explicit.
No charge-control commands or AML bank permissions are added. Errors/reserved
status, races, partial reads and uncertain acknowledgement retain containment.
The single worker serializes sampler/event operations, with existing transfer,
query and one-second event deadlines. The source/worker/resource retirement
proof must pass before remounting USB to save logs.

A separate disposable Air6,1-policy VM binary exercises this admission without
native hardware, including alarms in both OS branches, wrong descriptors and
unsafe event handling. Native alarms, accuracy, sustained monitoring and AC
transitions remain unaccepted. On failure, photograph the existing 30-second
screen; missing final completion is not a pass.

## Short native capture accepted (2026-10-23)

Release `bfde0423543f20d0`, source `63bc06f`, session
`00065e89a7794b63-00` completed with fresh generation 3 and 45 natural completion
events. Three SCI deliveries were handled, zero unclaimed. Runtime retirement
reports three attempts, zero rejected, attached/detached 1/1, heap/errors/denied
all zero. Return-boundary proof has idle controller, masked sources and no
outstanding resource lease; both foreign-event masks were restored. The saved
exit report passes the strict native SMBus validator and records completed=yes.

No alarms, acknowledgements or unknown senders occurred. This accepts only
bounded parked completion-event sampling and clean retirement with AC connected;
it does not establish alarm behavior, gauge accuracy, AC transitions or desktop
coexistence. Ordinary startup remains off. Next is a separately scoped attended
AC unplug/reconnect capture, with coherent sample/event evidence and the same
storage and recovery boundaries; do not enable the existing guided profile
without auditing Air6,1 admission and query paths.

All 205 prior USB files remain; only session metadata changed and three logs
were added. Offline fsck/allocation pass: 208 files, 387 clusters, zero leaks,
bitmap mismatches or errors. Ubuntu returned on a new boot, the one-shot is empty,
and candidate/recovery hashes pass. Collected evidence is backed up outside build.

## Attended parked AC transition candidate

The separate v63 recipe is `--battery-air61 --battery-transitions` with this
unit's `--laptop-probe-volume`. It uses the same exact Air6,1 firmware profile,
parked volume, automatic recovery and serialized SMBus service as accepted v62.
It does not admit direct v32 diagnostics, mounted Home, image/file work or normal
startup. No EC/AML/NVS permissions change. `_Q21` reads EACP (EC `60` bit 1),
writes boolean GNVS PWRS and notifies ADP1; PNOT only reads namespace state and
notifies CPUs. Captured-table initialization, `_Q21`, `_PSR` and detach replay
in all four lid/AC states has zero errors/denials/leaks; PWRS is the sole added
private-memory byte write. This does not predict every query firmware may emit;
unsupported queries still contain the run rather than execute unknown AML.

The service consumer requires three consecutive coherent fresh readings for each
connected/disconnected/reconnected phase. Disconnected current must be negative;
connected current may be zero or positive. Duplicate cache reads never count;
rejected or missed generations restart stability. Each phase has a 120-second
wall-clock limit even when samples stop. Timeout retires normally and saves an
incomplete result; faults retain the existing 30-second photograph/reset path.

The attended capture has an explicit 4096-event ceiling (short v62 retains 128).
At the observed 15 completions per sample, 128 cannot cover even the minimum nine
readings. Existing per-query and per-transfer deadlines remain. Per-event UART
records are suppressed only in this longer capture to preserve the bounded log
and readable prompts; final aggregate counters, accepted-phase samples and
retirement proofs are saved. This provides aggregate event evidence, not a full
per-event history. No polling, alarm decoding or acknowledgement rules change.

Start with AC connected and leave USB connected throughout. Wait for **UNPLUG
AC**, disconnect only the charger, then wait for **RECONNECT AC** and reconnect
it. Prompts repeat every five seconds while waiting. On normal completion or
phase timeout, the machine saves logs and returns to Ubuntu. Collect the matching
release and require `battery_smbus_transitions`, phase/current/timestamp proofs,
clean retirement, preserved files and offline fsck/allocation. A modeled pass is
not native transition acceptance or gauge calibration.

## Attended AC native return accepted (2026-10-23)

Release `321ba9c91e681a4c`, clean source `95c5219` (MR101), session
`00065e8a183ee909-00` passes the strict native transition validator. Three fresh
coherent samples were recorded per phase: connected generations 1–3, disconnected
5–7 and reconnected 10–12. Final phase currents were 0, -1328000 and 0 µA,
respectively. Reported relative charge was 97–98%; this does not establish gauge
accuracy or calibration.

The runtime handled 182 events, including two alarms and two acknowledgements,
with zero unknown events or rejected samples. Twelve sample attempts completed;
attach/detach balanced 1/1, with zero heap, errors or denied accesses at retirement.
Post-alarm generation 12 exceeds invalidated generation 8. Controller/resource
retirement, foreign-event restoration, complete exit log and automatic Ubuntu
return are verified. This accepts the observed bounded parked transition and
alarm-refresh path only; ordinary/Home/file battery ownership remains disabled.

All 208 previous USB files remain unchanged except expected `.srvros/meta`.
Three matching logs bring the total to 211 files. Offline exFAT/allocation pass
with 391 clusters and zero leaks, mismatches or errors. Installed payload and
recovery hashes pass; Ubuntu remains default and the one-shot is consumed.
Console recovery `c35475f62f154f1e` and previous accepted captures remain retained.
Matching raw logs, validation and return records are backed up outside build.

Next recommended work is bounded battery service coexistence with a mounted USB
workspace and file work, reusing the existing ownership/retirement mechanism with
an independently audited Air6,1 admission. Desktop monitoring and longer sessions
follow that boundary; no new run is armed by this closeout.

## Bounded USB file coexistence candidate (v64)

The explicit `--battery-air61 --battery-smbus-files` laptop probe reuses the
existing [parked image capture](battery-smbus-events.md#parked-smbustextfile-capture-v47).
A fresh 16 MiB `srvros-work/battery-fs.img` contains disposable test data. The
outer workspace is unmounted and aliases revoked before runtime admission;
only the exclusive image view performs real USB writes. This is not ordinary
mounted Home or desktop battery enablement.

Air6,1 admission requires its exact captured platform, coherent service sampling,
service owner, automatic recovery, the same parked volume, no mounted workspace
and a ready unpoisoned image lease. Entry requires idle storage; once admitted,
the existing image I/O scopes allow sampling between bounded storage operations.
Ordinary service, controls, mounted Home and VM studies cannot use this admission.
The short v62 and attended v63 gates retain their separate storage requirements.
There are no new firmware, EC, AML, NVS or PCI permissions.

The workload runs 60 generated edits and five Text save/open cycles while the
serialized service samples and handles natural events. The event cap stays 512;
existing startup, 90-second desktop and 35-second stop deadlines remain. Clients
and file workers retire before the battery service, then the image flushes and
revokes before outer remount/log save/unmount. Terminal uncertainty preserves the
existing reset interlock: no reset during active storage I/O and no filesystem
cleanup from a live fault context. A missing completion is failed/incomplete.

Keep AC and USB connected throughout this automatic test; no typing or charger
changes are required. On return require the strict `battery_smbus_files` receipt,
all 60 edits/five cycles, fresh post-alarm cache, balanced resource retirement and
clean image revocation. Verify exact inner contents, both allocation graphs and
all other USB file hashes. Native coexistence acceptance remains pending.

## First battery/file return: image preparation refused (2026-10-23)

Release `fc1d1f4c45d30252`, source `cac24ef`, session `00065e8a98eec3bc-00`
saved `completed=NO` with `battery media parked=NO fault-reset=NO`. The exclusive
image was refused before outer unmount, battery runtime entry or Text/file work.
This is failed preparation, not coexistence acceptance or a runtime failure.

The 16 MiB image has the correct size/valid length but its 128-cluster FAT chain
jumps from cluster 392 to 394. The read-only native-driver `--battery-image`
check reproduces `image-range: REFUSED`. General allocation and fsck passed
before boot, but the dedicated contiguous-extent check was omitted. A valid
filesystem and matching image bytes do not prove exclusive-range admission.

All 212 prior files remain unchanged except expected `.srvros/meta`; the inner
image is byte-identical to its fresh baseline. Three logs bring the total to
215 files. Outer fsck/allocation pass (523 clusters, zero leaks/mismatches/errors);
inner checks pass (two files, seven clusters). Payload/recovery hashes, Ubuntu
default and consumed one-shot verify. USB is unmounted, no reboot is armed, and
matching logs/image/layout/return evidence is independently backed up.

Next prepare a fresh contiguous image, preserving this failed run's evidence,
and require `check_exfat_allocation.py --uuid 6ADA-85BB --battery-image` to pass
on the unmounted real drive before installation/boot. Do not relax exclusive
range checks or treat another general allocation pass as a substitute. Any new
payload built on the later integration baseline needs its own exact-stage
validation; this run does not accept the filesystem-scale changes from PR102.

## Exclusive-image file coexistence accepted (2026-10-23)

The contiguous-image retry of release `fc1d1f4c45d30252`, clean source `cac24ef`,
run `bcbabcafe3b8c68c`, session `00065e8ac0a116ff-00` passes the strict native
file receipt. It completed 60 generated/presented edits, five save/open cycles
and 20 file-worker operations over a minute, alongside fresh generations 1–24
and 360 natural completion events. There were 24 handled/zero unclaimed SCI
deliveries, 24 attempts/zero rejects, balanced 1/1 attach/detach and zero retired
heap/errors/denied. All three descendants retired before service retirement and
clean image revocation. No alarms occurred in this file-work run.

The exact final inner contents verify. All 215 previous outer files are
preserved except expected image contents and `.srvros/meta`; three matching logs
bring the total to 218. Outer fsck/allocation/extent pass (527 clusters); inner
fsck/allocation pass (eight files, 14 clusters), with zero leaks, bitmap mismatches
or errors. The extent remains contiguous. Installed/recovery hashes, Ubuntu
default and consumed one-shot verify. Evidence is independently backed up; USB
is unmounted and no reboot is armed.

This accepts only bounded battery/Text coexistence in the exclusive image with
outer Home parked. Ordinary mounted Home, desktop battery startup, physical
input latency, longer endurance and gauge accuracy remain unaccepted. The next
boundary is explicit mounted Home ownership and normal-return retirement, before
ordinary desktop integration. Preserve the failed fragmented-image session as
separate evidence; it is not reclassified by this successful retry.

## Explicit mounted Home candidate (v65)

`--battery-air61 --battery-home --laptop-probe-volume 6ADA-85BB` selects the
separate short diagnostic. The normal outer Home filesystem remains mounted,
logging pauses, and the owner acquires the existing exact device/parent/generation
hold. No desktop clients or concurrent file workload are started. It waits up to
30 seconds for fresh generation 3 with natural events, bounded to 128 events,
then stops within the existing 10-second deadline and releases the hold only
after runtime retirement. The existing disposable image is left untouched.

Admission requires the exact Air6,1 firmware profile, coherent service owner,
the explicit short diagnostic flag, a valid mounted hold, no image lease and
automatic fault reset disabled. Other Home/desktop/control/continuous entry paths
remain refused. Guards verify that unmount, scratch and image conversion cannot
escape the hold. There are no new AML/register permissions.

The existing query-return proof now recognizes Air6,1 only inside this held
short scope. It requires a completed transfer and worker callback, balanced AML
calls/returns, no recorded attempted effects, quiet sources and idle resources.
An acknowledged alarm followed by refusal has effects and cannot use this return.
Normal success or checked refusal retires before log save/unmount/Ubuntu return.
Uncertain runtime failure retains the mount and HALTS; no reset or filesystem
cleanup is attempted. Photograph a halt and report it; missing completion is
failed/incomplete. Keep AC and USB connected and do not interact during sampling.

Native mounted-Home acceptance remains pending. Require the matching strict
`battery_home` receipt, prior-file preservation, offline filesystem/allocation
and consumed one-shot before any desktop integration. The newer filesystem
baseline is validated locally but does not inherit the earlier image payload's
physical acceptance.


The v65 native session `00065e8b198ee6ba-00` on release `30b0d713f1a9ed55`
now accepts this short mounted boundary: three fresh samples, 45 events, clean
retirement and preserved prior file contents with clean offline allocation.

## Bounded Home desktop session candidate (v66)

The explicit `--battery-air61 --battery-home-session` profile reuses the existing
bounded Home controls/file diagnostic. A dedicated flag lives only across that
entry and the exact held mount; it admits three Start/Stop lifetimes with Text
jobs pending during control requests. It grants no ordinary or continuous
startup capability. The existing 512 aggregate event cap, 110-second owner
watchdog, 90-second generated study and 10-second Stop deadline remain intact.
No firmware, AML/register or codec permissions change. Every service epoch must
retire through the checked Home return, and clients retire before final mount
release. The normal-return proof is available inside this explicit scope;
uncertain live failures still halt without mounted cleanup/reset.

A fresh root `battery-study` directory is mandatory. The generated workload
makes 60 edits and five exact file cycles (20 operations), issuing five controls
across three lifetimes. Final close retires an active service. Require the strict
session receipt, five files containing `AA`, preservation of every previous
non-metadata file, clean offline allocation and Ubuntu recovery. Native v66
acceptance remains pending; successful v65 and other machines do not supply it.

The first v66 native session `00065e8b68f3ad24-00` failed before its first save:
12 edits generated, 11 presented, one Start still waiting for fresh generation 2.
The first-save helper rejects a waiting control, pointing to a test sequencing
race. Runtime retired cleanly with 30 events; prior file contents, offline
allocation and Ubuntu recovery passed. This does not accept bounded Home desktop
coexistence. Preserve the failed session and reproduce delayed sample readiness
before fixing the generated control/save ordering.
