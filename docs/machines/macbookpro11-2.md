# MacBookPro11,2 / A1398

Updated: 2026-10-26. State: **native desktop, input, brightness, sound, CPU/fan telemetry and captured USB Ethernet DHCP/DNS/HTTP and live cable-status UI accepted**.

The sections below retain the dated bring-up sequence. The latest acceptance
and next scope are recorded at the end; earlier failures are historical.

## Current handoff

Latest physical run: v93 release `6fcb4fe992b35b78`, source `774e9e9d`,
session `session-00065ec7f927dacb-00`. Whole prior/v92 calibration proof,
bounded passive receive window, independent MAC cleanup and exact retained
endpoint stop passed. One normal short completion supplied276 bytes; the
strict first-entry parser decoded a channel1 beacon (234-byte MPDU). The event
was processed during the post-calibration window. This accepts one bounded
native beacon observation on the captured adapter, not sustained reception or
FCS/RSSI/RF performance. Full logs, Ubuntu recovery and offline allocation
passed; workspace unmounted and no reboot armed. Current v93, accepted v92
fallback and e47 previous recovery are preserved. Next: bounded repeated
passive reception with safe endpoint rearm and teardown, then scanning,
association/security, Wi-Fi IP traffic and UI. Original strict helper stays
unchanged; RF0:6 semantics remain unresolved.

Active offline block (2026-10-26): merged actual main `a9d69554` (PR199 Air
attended-exit evidence only); PR182 owns filesystem imports and has no MT76
overlap. Prepare an allocation-free bounded repeated receive engine using eight
distinct retained4KiB buffers and non-wrapping TRB identities. Admit only one
pending request, ignore foreign/stale events, stop on bounded count/deadline or
failure, and inspect normal-completion bytes only after exact endpoint cessation
and fresh cleanup ownership. Touched: new MT76 receive-window helper/header,
host fixtures/Makefile and unit/Wi-Fi guides. Existing native v93 stays unchanged;
no physical profile, adapter rearm or reboot in this block. Native callback
integration and exact-stage/physical acceptance follow separately. Checkpoint:
fault-injected ownership/event/clock/stop tests, sanitizers, core/image and
archived evidence before publishing implementation.

Active block (2026-10-26): consumed actual main `a9e39e38` (PR197 attended
load and PR198 keyboard navigation). Resolved hardware_boot.py conflicts by
preserving all Wi-Fi recipes plus separate desktop v76/v77 flags/validation;
179 staging tests pass. PR182 still owns filesystem imports; no MT76 overlap.
Build explicit v93/profile71 passive main-RX observation after whole v92
calibration proof: keep one4096-byte IN, poll events under active MAC up to1s,
then drain/disable MAC and prove exact endpoint stop before reading bytes.
Require normal CC1/13 and bounded residue for actual size; never decode stopped
CC26/27/28. Parse only the first bounded entry; log copied non-identifying
metadata, never SSID/address/payload bytes. Record event timing relative to the
observation window; no rearm, extra IN, network data OUT, scan or association.
Touched: native RX/inspection hook/selector, parser integration and host tests,
main/staging/smoke and unit/Wi-Fi docs. Keep v1-v92 bytes/behavior and original
calibration helpers. Checkpoint: fault/length/teardown tests, sanitizers, merged
image/core, exact-stage VM, installed recovery/run/archive before cold trial.


Completed physical block (2026-10-26): v92 on main `43aff7d5` consumed PR194
window/dock and PR196 Air evidence; PR182 remains filesystem scope. Native
fixed-command adapter, selector70, main/staging/smoke recipes and host/proof
fixtures passed local/core/sanitizer/exact-stage VM and this unit's bounded
transaction checks. No packet inspection/delivery, rearm or network data OUT.
The earlier strict16-byte reply hypothesis is now observed for all five
commands on this adapter; this does not authorize widening other device gates.
No further physical run is staged or armed.


Completed offline block (2026-10-26): consumed actual main `9465c68f` (PR192 private
child-image ownership, PR193/195 evidence). PR194 owns window/dock behavior;
PR182 filesystem imports; neither overlaps this scope. Implement a separate,
allocation-free initial power-on calibration transaction for the captured
MT7610 NICfd11/1004, US channel1/20MHz profile. Touched: new power-on helper,
header, host tests/Makefile and unit/Wi-Fi docs. Reference requires R/VCO and
10us delay before FULL/LC/15ms/restoration/RXDCOC; NIC_CONF1 bit13 is clear,
so TSSI DC is excluded only for this exact profile. Require current ownership
and fresh mapped board fields before every forward callback, independently
verified temporary writes and reverse cleanup after failure. Preserve both old
helpers. No native caller/profile change, calibration claim or reboot in this
block. Checkpoint complete: host fault injection/sanitizers, merged core/image and
GPT/OVMF boot/input validation, reference audit and durable archive. Native
command adapter and physical trial remain next; installed v91 is unchanged.


Completed block (2026-10-26): v91 on actual main `d24dbc11` (PR191 evidence
only). Calibration-probe helper, optional MAC inspection hook, native selector,
host/staging/smoke tests and docs validated locally and on this unit. Scope
accepted only the reversible register observation and existing cleanup. Full
dated results are retained below. No new trial staged or reboot armed.


Completed physical block (2026-10-26): consumed actual main `4ee18f69` (PR190 scheduler/
desktop wake attribution),preserving all Wi-Fi profiles; PR182 owns filesystem
imports,no MT76 overlap. Build v90 bounded MAC startup/shutdown around one
retained main-RX request,then fresh TX_ALC0/IBI9 observations. Touched: MT76 MAC
cycle helper/host tests,xHCI callback adapter and actual selector,main/staging/
smoke recipes and machine/Wi-Fi docs. Require full prior proof,independent
MAC-disable cleanup,device TX/RX queue draining,exact endpoint stop and retained
DMA. No calibration command,packet delivery,rearm,scan or association. Unknown
IBI9 stays observational. Local/exact-stage VM,installed/recovery/run/archive checks and the native
bounded MAC cycle now pass;acceptance limits and next scope are above.


Completed offline block (2026-10-26): consumed actual main `ee5468f6` (PR186–189,
Air6,1 desktop timing/evidence; PR190 continues timing attribution and PR182
owns filesystem imports). No MT76 overlap found. Build an allocation-free,
callback-driven FULL/LC/RXDCOC transaction for the fixed US channel1/20MHz
profile,with fresh TX_ALC0/IBI9 capture,exact temporary writes,ordered waits,
verified restoration and cleanup after cancellation/failure. Touched: new MT76
transaction header/source,host tests/Makefile and unit/Wi-Fi docs. No native
caller,MAC enable,calibration command or boot recipe change in this block.
Dependencies: later native MAC/RX ownership and fresh board/power/TSSI proof;
current IBI9/RF0:6 observations do not authorize calibration. Host/sanitizer/build/VM validation is complete; evidence is archived outside
build. Native MAC startup/teardown and fresh IBI9/board/power context are next.


Current development block (2026-10-26; local validation complete): consume actual main `0858e8e6` (PR181/185
application window/progress fixes; both Makefile test families preserved).
PR184 owns worker admission and PR182 filesystem imports; neither overlaps
this MT76 scope. Built a bounded,allocation-free receive decoder for MT76x02
USB/RXWI and passive beacon/probe-response observations,with malformed packet
and sanitizer coverage. Touched subsystems: new MT76 receive header/source,
host tests/Makefile and Wi-Fi guide. No native endpoint scheduling,MAC enable,
calibration,transmit,scan UI or authentication yet. This is preparation for a
controlled MAC/RX lifecycle; calibration requires MAC start in pinned reference
USB ordering,save/restore and validated board/power context. The later corrected v89 native run below accepts bounded endpoint ownership;
source main merges alone do not establish native acceptance.


Active block: first controlled main-RX endpoint lifecycle after the whole v88
proof. Actual main `5bcd061c` consumes merged PR184 worker-pipe admission;
PR182 still owns filesystem imports and does not overlap. New explicit v89
profile will configure captured endpoint84/DCI9,post one bounded4KiB IN,
wait10ms,Stop Endpoint with exact command/slot and stopped-context proof,
retain all DMA and verify disabled MAC/CORE/DMA guards before/after. No Set
Dequeue,restart,retry,free,MAC enable,calibration or transmit. Unexpected
payload is metadata-only and not receive acceptance. Touched: xHCI receive
helper/dispatcher,host tests,main/staging/smoke recipes and unit/Wi-Fi docs.
Bounded RX shutdown is a prerequisite to later MAC/calibration work; old
profiles remain byte-identical. Corrected v89 now has bounded native endpoint lifecycle acceptance as recorded
above. MAC enable,calibration and reception remain pending; no reboot is armed.



The user considers the core Pro bring-up generally working. Named payloads below
have native acceptance for desktop/input, brightness, speakers, CPU/two-fan
telemetry and captured USB Ethernet DHCP/DNS/HTTP with live cable-status UI.
Battery, headphones and further trackpad-latency work remain deferred.

Active scope is the Edimax `7392:a711` USB MT7610 adapter. The central-table
native release `257e68678e55ba49` (source `83152ee0`) completed all44 central RF
writes and44 separate reads, with43 matches and only RF0:6 reading00 after41.
All command/guard/recovery checks passed. Live Linux reads08 there after its
initialization/calibration. Register semantics remain unresolved; the original
strict engine is unchanged and full radio initialization remains unaccepted.

Release `242e84b15d20babc` (source `8fbd311d`) completed the fixed68-entry
RF bank5 write/read observation in native session `session-00065ead199515d2-00`.
All68 readbacks matched:952 operations,816 guard reads,136 verified replies,
68 ACKs and272 valid guard triplets; both mismatch fields zero. The complete
159281-byte startup prefix and both completion markers were saved. Earlier
central-table proof remained43/44 with only RF0:6=00. This accepts the bounded
command/guard/readback observation, not calibrated radio operation.

The return collector's old128KiB ceiling initially refused this larger log;
its transport ceiling is now256KiB, with byte-preservation boundary tests.
Strict full-prefix validation passed after collection. Ubuntu default, consumed
one-shot and installed/recovery payload hashes verified. Known-good
`e47dca3a6b30c6c1` remains recovery. Offline workspace checks are clean:
277files/665clusters/112directories, zero leaks, mismatches or errors.
Evidence is durably archived outside build.

Release `7b329891c0606f1c` (source `00f04fb3`) completed the fixed38-entry
RF bank6 observation in native session `session-00065ead98009e52-00`.
All38 readbacks matched:532 operations,456 guard reads,76 verified replies,
38 ACKs and152 valid guard triplets; both mismatch fields zero. Full prior
central and bank5 proof passed. The entire191063-byte startup prefix and both
completion markers were saved. Strict validation, consumed one-shot, Ubuntu
default and installed/recovery hashes passed. Latest offline workspace checks:
279files/669clusters/113directories, zero leaks, mismatches or errors;
unmounted. This is command/guard/readback evidence, not calibrated radio
acceptance.

Release `de89daa6bdc3024e` (source `22f757bf`) completed the fixed35-entry
RF VGA (bank7) observation in native session `session-00065eadd298077c-00`.
All35 readbacks matched:490 operations,420 guard reads,70 verified replies,
35 ACKs and140 valid guard triplets; both mismatch fields zero. Full central,
bank5 and bank6 proof passed. The entire219878-byte startup prefix and both
completion markers were saved. Strict validation, consumed one-shot, Ubuntu
default and installed/recovery hashes passed. Latest offline workspace checks:
281files/673clusters/114directories, zero leaks, mismatches or errors;
unmounted. This is command/guard/readback evidence, not calibrated radio
acceptance.

Release `e75a52b56efa282b` (source `b4be310b`) completed the fixed19-target
default20MHz/G-band observation in native session `session-00065eae3ac86feb-00`.
All19 readbacks matched:266 operations,228 guard reads,38 verified replies,
19 ACKs and76 valid guard triplets; both mismatch fields zero. Full central,
bank5, bank6 and VGA proof passed. The entire235592-byte startup prefix and
both completion markers were saved. Strict validation, consumed one-shot,
Ubuntu default and installed/recovery hashes passed. Latest offline workspace
checks:283files/677clusters/115directories, zero leaks, mismatches or errors;
unmounted. This is command/guard/readback evidence, not calibrated radio
acceptance.

Release `4ec251f79c97532b` (source `b15867d7`) completed fresh mapped
board-field verification and RF0:22 trim write/read in native session
`session-00065eaec03cebac-00`. Fresh fields passed24 reads,2 writes,2 polls,
2 blocks and26 submissions with no guard errors or raw data logs. Trim68
read back68:14 operations,12 guard reads,2 verified replies,1 ACK and4 valid
guard triplets; both mismatch fields zero. Full earlier initialization proof
and both completion markers passed. Ubuntu default, consumed one-shot and
installed/recovery payload hashes verified; offline workspace clean:
285files/681clusters/116directories, zero leaks, mismatches or errors;
unmounted. Evidence is durably archived.

Release `d27f0d2f10e5ae0b` (source `cfc0cfdd`) completed the bounded DAC/VCO
initiation observation in session `session-00065eaf209e71b6-00`. All70
operations,60 guard reads,10 replies,4 ACKs,6 observations and20 guard triplets
passed, with full prior proof. RF0:73 showed93 ->13 ->93 across the DAC pulse;
RF0:4 showed30 ->b0 and remainedb0 at the final read. VCO self-clear was not
observed, so successful calibration is unconfirmed. Complete241416-byte startup
prefix and both completion markers saved. Recovery/default/one-shot/payload
checks passed; offline workspace clean287files/685clusters/117directories,
zero leaks/mismatches/errors, unmounted and unarmed.

Release `cd775567c529231f` (source `70a3f45d`) completed six delayed VCO reads
in session `session-00065eafa382c5ce-00`. All six values were30: bit7 was clear
at the first delayed sample and remained clear. Fixed requested inter-sample
delays totaled1000000us; transport/logging time is additional, so this does
not measure a precise10ms completion deadline. All42 operations,36 guard reads,
6 replies/delays and12 guard triplets passed with full prior proof. Entire
244743-byte startup prefix and both completion markers saved. Recovery/default/
one-shot/payload checks passed; offline workspace clean289files/689clusters/
118directories, zero leaks/mismatches/errors, unmounted and unarmed. Evidence
is durably archived. Release `0aaf20927414846c` (source `67e5e5af`) completed guarded one-chain
RX-path/TX-DAC setup in session `session-00065eb0051574ff-00`. AGC0 at2300
was21400 before/after; TXBE5 at2714 was0 before/after. Required bits were
already clear; both fixed writes preserved all other bits and exact readback
passed. All18 transfers,16 reads,2 writes,2 verified targets and4 guard
triplets passed with full earlier proof. Entire247000-byte startup prefix
and both completion markers saved. Recovery/default/one-shot/hashes verified;
offline workspace clean291files/693clusters/119directories, zero leaks/
mismatches/errors, unmounted and unarmed. Evidence durably archived. Release `80a6781c0b08290c` (source `4dd830fd`) failed the read-only inventory
in session `session-00065eb0816b56d4-00`: IBI9 at2124 returnedffffffff on a
successful4-byte USB read. The engine refused after12 reads/submissions;
full inventory and postguards did not complete. Eight earlier targets were
observed, including US_CYC2a4=0 and TXOP1340=583f. TX_ALC13b0, BCN_TIME1114,
USB_DMA238 and final guards were NOT_READ; printed zero tail rows are
placeholders. Full earlier path/RF proof verified separately against the actual
failed log, and the strict success collector correctly refused it. Entire
249442-byte prefix saved with both failure markers. Recovery/default/one-shot/
hashes verified; offline workspace clean293files/697clusters/120directories,
no leaks/mismatches/errors, unmounted and unarmed. Evidence archived as failure.
Independent live Linux reads returned IBI9=ffffffff twelve times with valid
ASIC/CORE and adjacent reads. Linux saves/restores this register around
calibration; that value alone does not prove transport failure. The original
strict profile remains unchanged. Separate v76 observation release
`2351cfa9efee6bb1`, source `5426417e`, is installed and unarmed. Only fixed
IBI9 may record all-ones; the other11 targets and every guard stay strict.
All18 reads, full postguards, derived mask and both completion markers are
required. Native v76 session `session-00065eb0f7e9e13b-00` stopped earlier:
RF0:4 had already cleared fromb0 to30 at the initiation final read. All70
initiation operations/guards/replies passed, but the historical pending-VCO
profile requires finalb0, so the wait owner refused before any wait operation.
Wait/path/inventory are NOT_RUN; no IBI9 observation obtained. Complete243533-byte
prefix saved with both failure markers; strict success correctly refused it.
Full prior proof through initiation verified separately on unchanged log.
Recovery/default/one-shot/hashes passed; offline workspace clean295files/
701clusters/121directories, zero leaks/mismatches/errors; unmounted/unarmed.
Separate v77 release `b2369fab9ef32f11`, source `f5549279`, completed native
session `session-00065eb1475df12a-00`. Early-clear receipt pending0/early_clear1
passed; all six delayed reads remained30. Full earlier proof and all18 inventory
reads/postguards passed, zero inventory writes. IBI9=ffffffff was recorded with
mask100; all other targets and both guard triplets valid. Entire250364-byte
prefix and both success markers saved. Recovery/default/one-shot/hashes passed;
offline clean297files/705clusters/122directories, zero leaks/mismatches/errors;
unmounted/unarmed. Observation evidence archived; original strict modes and
v75/v76 failures unchanged. Release `5c86ad12e5f185ca`, source `da8b7fce`,
completed USB-cycle setup in native session `session-00065eb17b66ef77-00`:
fresh02a4=0, masked write/readback1e; all9 transfers,8 reads/1 write/1 verification
and2 guard triplets passed. Full earlier proof and exact inventory passed.
This run used pending1/early_clear0 and all six delayed reads30; v77 exercised
the early-clear variant. Entire251629-byte prefix/both success markers saved.
Recovery/default/one-shot/hashes passed; offline clean299files/709clusters/
123directories, zero leaks/mismatches/errors; unmounted/unarmed. Timing setup
accepted on the captured adapter. Release `4c8563c12db92578`, source `83a869fc`,
completed bandwidth response observation in `session-00065eb1d61ee32d-00`:
sequence15 returned16 bytes4a0f0008/2/0/4a0f0008, matching function ID2/value0
with framing/trailer. All7 operations,6 guard reads/1 exchange and both guard
triplets passed with full earlier proof. Entire252903-byte prefix/both success
markers saved. Recovery/default/one-shot/hashes passed; offline clean301files/
713clusters/124directories, zero leaks/mismatches/errors; unmounted/unarmed.
Original v79 payload remains framing-only observation (ack_validated0).
Release `6a9fd32642e92a7b`, source `a89af59a`, completed native v80 layout
in `session-00065eb25d7baa54-00`. Exact bandwidth reply prerequisite passed;
all45 transfers,40 reads/5 writes/5 verified and10 guard triplets passed.
CORE1/AGC0/TXBE0/TX_BAND remained2/21400/0/4; EXT_CCA changedffe4->f1e4,
preserving ED_CCA upper nibblef. Entire258464-byte prefix/both success markers
saved. Recovery/default/one-shot/hashes passed; offline clean303files/717clusters/
125directories, zero leaks/mismatches/errors; unmounted/unarmed. Captured layout
accepted. Release `dde8fcb03f3ea0d5`, source `74d1599e`, completed v81 in
`session-00065eb2f077e6da-00`:116 ordered operations,101 reads/15 RF replies,
32 strict guard triplets and20 indexed observations, zero new data writes.
The entire274888-byte prefix exceeded the old256KiB limit and was preserved
with explicit512KiB capture; both completion markers and all prior proof passed.
Ubuntu default, consumed one-shot and installed/recovery hashes verified;
offline clean305files/722clusters/126directories, zero leaks/mismatches/errors;
unmounted/unarmed. MAC/RF values remain observations. Next scope is bounded
channel-band/synthesizer setup grounded in these fresh values and pinned source;
frequency/calibrated radio/connectivity remain untested.


Release `7e269653eebe03c3`, source `83edd6be`, completed v82 in
`session-00065eb38500e72c-00`. All70 RF targets matched:980 operations,
840 guard reads,140 exact replies,70 ACKs and280 strict guard triplets;
both mismatch fields zero. The two MAC words changed from0 to13c8=50007 and
13a0=3e0002(hex), with18 ordered transfers,16 reads/2 writes/2 verified and
4 more guard triplets. Whole earlier proof and the complete402661-byte prefix
with both completion markers passed. Ubuntu default, consumed one-shot and
installed/recovery hashes verified. Offline clean307files/728clusters/
127directories, zero leaks/mismatches/errors; unmounted/unarmed. Bounded
2GHz band setup/readbacks passed; synthesizer/channel tuning and calibrated
radio/connectivity remain untested. Next scope is pinned channel-frequency
plan/RMW fields and their fresh prerequisites.
Touched subsystems: fixed MT76 plan/MAC engine, gated xHCI diagnostic mode,
boot recipe, directed tests and this unit/current Wi-Fi guide. Dependencies:
v81 native proof, pinned Linux v7.0 set_band reference, explicit512KiB capture.
Fetched integration main `aa7b7392`; new changes concern Air lifecycle timing
and tools, with no MT76 overlap. Open PR172 owns desktop runtime; Pro PR19 owns
this diagnostic scope. Retain intentional integration baseline `d5bc4b3`
during this bounded trial. No remote task messaging or parallel staging.


Release `be37d545e93f1aed`, source `0ee9efb3`, completed v83 in
`session-00065eb3c5cb1785-00`. All21 fixed channel1 synthesizer steps passed
fresh-read/masked-or-full-write/independent-readback checks:441 operations,
378 guard reads,63 exact MCU replies,21 ACKs,42 observations and126 strict
MAC0/COREf000f200/DMA40000050 triplets. All earlier proof and the complete
458424-byte prefix/both completion markers passed. Ubuntu default, consumed
one-shot and current/recovery hashes verified. Offline clean309files/
734clusters/128directories, zero leaks/mismatches/errors; unmounted/unarmed.
Bounded synthesizer field/readback setup passed; frequency lock, calibrated
radio and connectivity remain untested. Next audit post-tune bandwidth/band
tables and fresh PA/gain prerequisites before VCO or MAC enable.
Touched: MT76 fixed engine, native diagnostic ownership, boot recipe/tests,
this unit and current Wi-Fi guide. Dependencies: exact v82 band/MAC proof,
v81 RF0:24=33hex and26..37=0 observations,512KiB full-prefix capture.
Fetched main remains `aa7b7392`; PR172 desktop runtime,174 process admission
and175 Apple Silicon documentation do not own this MT76 diagnostic. Pro PR19
continues this bounded scope; integration base remains `d5bc4b3`.


Release `e57afcf9eb1fab7e`, source `f67ee1be`, completed v84 in
`session-00065eb40f2ab69b-00`. All19 post-tune20MHz/G-band readbacks matched:
266 operations,228 guard reads,38 exact replies,19 ACKs and76 strict guard
triplets, both mismatch masks zero. Same-boot internal2G PA proof passed;
three fresh MAC targets independently read back518=1,13a8=63707400,
13b4=89440000(hex):27 transfers,24 reads/3 writes/3 verified and6 triplets.
Whole earlier proof and complete495284-byte prefix/both markers passed.
Ubuntu default, consumed one-shot and current/recovery hashes verified;
offline clean311files/740clusters/129directories, zero leaks/mismatches/errors;
unmounted/unarmed. Bounded post-tune setup/readbacks passed; frequency lock,
calibrated radio and connectivity remain untested. Next: channel-specific
BBP/filter/gain prerequisites before VCO/calibration or MAC enable.
Touched: MT76 private plan/MAC engine, gated native diagnostic, boot recipe,
directed tests and unit/current Wi-Fi guides. Dependencies: whole exact v83
synthesizer and earlier proof;512KiB capture. No VCO/calibration, MAC enable,
scan or traffic. Fetched main `1eb60b8f` adds Air process admission; PR172
owns desktop runtime and175 Apple Silicon setup/synchronization. No MT76
scope overlap; Pro PR19 continues this work on integration base `d5bc4b3`.


Installed/unarmed v85 candidate `2571de4ea5d87728`, source `e3ea53b3`,
passed local/sanitizer and both exact-stage VM checks. It captures mapped
logical40 RX LNA/RSSI fields and
CORE1 plus20 fixed channel-BBP targets after exact v84 proof. Decode pinned
reference gain arithmetic for observation only; no BBP/RF/MAC data writes.
One mode-zero EFUSE read-select command is required; no programming mode.
Touched: fixed EFUSE profile/capture, MT76 read-only inventory, gated native
mode, boot recipe/tests and unit/current guide. Dependencies: whole post-tune
proof and512KiB complete prefix; next gain writes wait for native fields.
Fetched main `aa931093` merges shared Text/Preview runtime; no MT76 overlap.
Open PR175 owns Apple Silicon synchronization; Pro PR19 continues this scope.
Retain integration base `d5bc4b3` during this bounded trial.

Calibration, channel setup, scanning, association/security and Wi-Fi
connectivity/UI remain pending. The dated sections below preserve full scope,
source, dependencies and evidence; observations are not radio acceptance.

## Original bring-up scope (historical)

Local on-device agent, branch `device/macbookpro11-2/bringup`, starting from
`main` at `db40ed510636847d9aad2c7d38fea9976de43252`.
Current objective: diagnose and enable the built-in `05ac:0262` keyboard.
Scope: captured descriptor fixtures and xHCI host regression coverage first;
collect unattended native USB/controller diagnostics before choosing any driver
change. The v19 capture path in `kernel/src/main.c` will explicitly print xHCI
controller/endpoint state into its saved snapshot; the diagnostic text will no
longer label every keyboard as the Air fixture. The first trial has no persisted USB log, so the failure layer is unknown.
PR #4 remains the Air EC/AML owner's scope; no overlapping implementation is
planned. Baseline `origin/main` remains `db40ed5` as fetched after this trial.

## Observed configuration

- Ubuntu 26.04.1 LTS, x86_64, kernel `7.0.0-38-generic`; UEFI boot with a
  separate mounted FAT ESP and Ubuntu GRUB default `0`.
- DMI model MacBookPro11,2; firmware `MBP112.88Z.0142.B00.1708080655`.
  Exact marketing year has not been independently confirmed.
- Intel Core i7-4870HQ at 2.50 GHz; 16 GB installed memory.
- Intel integrated graphics `8086:0d26`, Linux `i915`, boot VGA selected;
  connected eDP panel exposes 2880×1800. No NVIDIA GPU appears in PCI inventory.
- Internal APPLE SSD SM0256F, about 234 GiB, Samsung AHCI controller
  `144d:1600`. Original versus replacement history is unknown.
- Intel xHCI `8086:8c31`; Apple keyboard/trackpad `05ac:0262` with two captured
  HID report descriptors. Linux reports the Apple HID driver.
- Analog HDA controller `8086:8c20`, Cirrus Logic CS4208 codec `1013:4208`,
  subsystem `106b:5e00`; HDMI controller `8086:0d0c`, codec `8086:2807`.
- Linux applesmc exposes two fan channels (Left side and Right side).
- Broadcom internal Wi-Fi `14e4:43a0`; attached Edimax USB wireless adapter
  `7392:a711`; Bluetooth `05ac:8289`; FaceTime camera `14e4:1570`.
- Attached USB flash storage `13fe:6700` has an existing mounted exFAT partition.
  Its UUID and paths are retained only in the ignored local checkpoint.

The privileged Linux capture completed with **zero unavailable/partial reads**,
including ACPI tables, both Apple HID report descriptors, graphics topology,
audio codec dumps and CPU policy. The earlier unprivileged capture retained its
18 permission-denied ACPI reads for provenance. Raw captures remain local under
ignored `build/hardware/` storage.

## Acceptance and recovery

Physical release `a5b5ae578027e8c0` reached the console, confirmed by the user.
The built-in keyboard **failed**. No result file or log session was created;
USB persistence, trackpad, scaled display, audio, brightness, sensors, power and
networking remain untested. Boot visibility does not accept these providers.
The consumed one-shot has been checked after return; Ubuntu remains recovery.
No physically accepted SrvrOS recovery release exists. Optional AML stays off.

For future trials, prefer an unattended boot/check/save/unmount/return cycle.
If manual commands are necessary, display them in the boot environment or wait
for the user's confirmation before triggering reboot. This is the user's standing
instruction; do not require keyboard commands for diagnostic collection when
keyboard support itself is under investigation.

## Host and VM validation

At source `7fff5cc` (inventory-only change over `db40ed5`):

- `make check-host`: PASS, including all pinned submodules and OVMF.
- `make -j2 all usb-image test-core`: PASS; Zig 0.15.2 verified by bootstrap.
- `make smoke`: PASS (syscall ABI, 481-command shell regression, shell editing,
  UEFI USB keyboard and displayd). Final shell filesystem check reported no
  errors or leaks.
- `make check-docs`: PASS.
- Prepared the existing exFAT workspace without formatting or replacing files.
- Exact console release `a5b5ae578027e8c0`, v7 workspace profile:
  `hardware_boot_smoke.py --console-check` PASS (GRUB chainload, RAM filesystem,
  USB input, case/Caps/repeat and renderbench). This rehearsal does not attach
  the physical workspace and does not establish physical persistence.

The candidate and Linux/VM evidence have a durable local archive outside
`build`; its path is recorded in `build/bringup/checkpoint.json`. It is a tested
VM candidate, not an accepted physical recovery release.

## Next checkpoint

First console trial is collected as a keyboard failure. The captured Pro
configuration now passes the actual xHCI parser and endpoint-context tests:
interface 0, endpoint 0x83/DCI 7, packet size 10, interval 8. The Air fixture still
passes. This disproves a simple missing product-ID selection rule; physical
transport/report handling remain unresolved.

At `38ad189`, the v19 unattended profile explicitly saves controller/input
diagnostics even when PS/2 appears ready. Build, xHCI/keyboard host tests and
documentation checks pass. Exact release `bfc0b7492f2677b7` passes VM capture,
offline saved-byte verification, clean exFAT and automatic reset. Its missing
workspace rehearsal halts without reboot. The physical diagnostic is next;
it requires no commands and does not claim working keyboard support.
Preserve the previous console payload and Ubuntu default; collect the exact new
release on return before changing the driver or preparing another boot.

## Native diagnostic failure and current fix scope

Release `bfc0b7492f2677b7` halted without a saved USB session. The user's
photograph is archived locally. It shows the port-12 device stopped at the
device-descriptor stage and no keyboard registered. The captured Linux device
descriptor reports EP0 size 8; the driver initializes full speed at 64 and never
evaluates the endpoint context after learning its actual packet size.

The attached exFAT partition is on GPT. USB attachment only registers MBR
partitions, explaining why the workspace cannot be selected even when the
flash device is found. No reformat or partition-table mutation is planned.

Bounded shared changes now owned here: xHCI EP0 enumeration/packet-size update
and their host tests; validated GPT discovery for USB storage and block-layer
regressions. Existing Air fixture and MBR paths must keep passing. Air EC/AML
work remains separate. These are implementation findings, not physical fixes
accepted on this machine. Collect-before-restage is complete for the failed run.

## EP0 and GPT candidate

Source `dd5ba7e` corrects full-speed EP0 initialization to 8 bytes, reads the
8-byte descriptor prefix, and evaluates the live endpoint context when a larger
packet is advertised. Both Air and Pro descriptor fixtures pass; malformed
sizes and failed context commands are rejected. USB storage now registers
validated GPT partitions. The physical drive has a saturated protective-MBR
length; this is explicitly covered without trusting it as a writable extent.

Validation: full build/USB image and core tests PASS; focused xHCI, block/AHCI and
USB storage tests PASS; block tests under ASan/UBSan PASS; UEFI USB input/boot
regression PASS. Initial release `ad01d8a08f9b70eb` passes missing-workspace,
MBR capture and GPT capture rehearsals. Final release `fe8e4d3e22ba97eb`
adds saturated protective-MBR coverage and passes the exact GPT capture,
offline saved-log verification, clean exFAT and automatic reset rehearsal.

Next physical run uses final v19 release `fe8e4d3e22ba97eb`, with no manual
commands or optional AML. It must establish keyboard enumeration and saved GPT
workspace diagnostics before input acceptance. Actual typing and trackpad
behavior remain unaccepted. Retain the original console as previous and Ubuntu
as recovery; collect the matching session after return before another stage.

## Native EP0/GPT result and workspace repair

The next photograph of `fe8e4d3e22ba97eb` shows the built-in `05ac:0262`
configured with one keyboard registered, and `/work` mounted from `usb0p1`.
This establishes enumeration and GPT discovery on this unit, not actual typing.
The capture still halted without logs: exFAT correctly refused writable access.

Offline inspection found a clean flags field and a single FAT, but the allocation
bitmap entry declared 59,149 bytes for 473,184 clusters (59,148 required).
`fsck.exfat -n` reported the invalid size despite returning zero and printing a
clean summary. After archiving all files and the boot/FAT/root/bitmap metadata,
`fsck.exfat -p` corrected it. A byte comparison shows only root-directory offset
24 changed; the boot regions, FAT and bitmap contents are identical. A repeat
read-only check reports no errors, and all existing file hashes match.

Retry the same exact tested release with this corrected workspace. Save a new
run record rather than overwriting the failed trial. Ubuntu and the previous
console payload remain recovery; collect logs before further staging. Keyboard
reports/typing, trackpad and successful native log persistence remain pending.

## Successful native diagnostic retry

Release `fe8e4d3e22ba97eb`, session `session-00065e3b75192c41-00`, saved
matching boot and exit logs after the bitmap repair. The logs establish
`05ac:0262` keyboard enumeration (interface 0, DCI 7, packet 10), GPT workspace
mount, diagnostic writes and flush. The user confirmed an automatic return to
Ubuntu. The one-shot entry was consumed and Ubuntu remains the default; the
post-return offline exFAT check reports no errors. Evidence is archived outside
`build` along with the separate retry record.

Keyboard reports were zero at the diagnostic snapshot, so actual typing remains
untested. Trackpad, desktop interaction and other device providers are also not
accepted by this run. Next milestone is a visible typing test with instructions
in the boot environment, preserving the standing reboot policy.

## Visible keyboard acceptance scope

Next bounded work adds an opt-in, guided keyboard capture to the existing
save/unmount/return profile, with per-step deadlines and persistent results.
Touch kernel startup, boot-profile staging and exact-stage VM harness/tests.
Test letters, Shift, Caps Lock, Backspace, Enter and all arrows through the
keyboard character path (not serial injection). VM coverage must exercise both
correct and incorrect input. A timeout/failure must be recorded as such, then
return safely when persistence succeeds. Other device scopes remain unchanged.

Guided v27 release `5c3c1f8a0106166c` (kernel source `9238d40`) passes build,
keyboard host tests, 29 staging/profile tests and three exact-stage GPT VM runs:
ten prompted keys produce PASS; a wrong key produces FAIL; no input reaches the
30-second deadline and produces FAIL. All three save the matching result to the
exit log, pass offline exFAT verification and reset automatically. The instruction
screen was visually inspected. The same payload is next for physical typing;
no physical keyboard acceptance is claimed yet. Preserve the successful native
diagnostic as previous, with older payloads retained in the durable archive.

## First native guided typing result

Release `5c3c1f8a0106166c`, session `session-00065e3b9a24480c-00`, saved
`keyboard-test: result=FAIL passed=5 total=10 dropped=0`. Lowercase, Shift,
Caps Lock on/off and Backspace passed. At the Enter prompt, the final HID report
contains usage `0x38` (slash); six characters were pushed, with 17 completed
keyboard reports, zero transfer errors and zero drops. The test ended before
its first 30-second deadline, so this is wrong input rather than a timeout.
Enter and arrows remain unaccepted; clarification of the physical key pressed
is pending. Do not infer a driver mapping fault or a complete keyboard pass.

Matching logs and run metadata are archived outside `build`. The consumed
one-shot and Ubuntu default were verified; offline exFAT reports no errors.
No new reboot is armed.

The user reports running out of time and the system rebooting. Preserve that
observation alongside the saved trace: the exit snapshot is at about 24.6 seconds
after boot and the last report contains slash, so the existing 30-second step
deadline cannot explain this recorded failure. Automatic return is user-confirmed.
For a retry, improve the guided test with more time and recoverable unexpected
input rather than treating this as a proven Return-key driver defect.

## Forgiving guided retry scope

User authorized the retry. Extend the guided capture to 90 seconds per step,
keep unexpected keys within the same step, and explicitly log retries/timeouts.
Touch only startup capture, versioned staging, VM coverage and these guides.
Retain the successful diagnostic as previous. Prior native evidence is collected;
Enter and arrows remain pending. Existing EC/AML work is outside this scope.

Retry release `29af8019a1737d6e`, source `e8d89f3`, uses v28 with 90-second
per-step deadlines and recoverable unexpected input. Build, keyboard host tests,
29 profile tests and documentation checks pass. Exact-stage GPT VM tests prove
that no input saves FAIL after the full deadline and that ten deliberate wrong
keys followed by the correct keys save PASS with ten retries and zero drops.
Both runs verify saved logs, clean exFAT and automatic reset. Physical retry is
next; retain `fe8e4d3e22ba97eb` as previous and preserve all earlier run evidence.

## Native guided keyboard acceptance

Release `29af8019a1737d6e` (source `e8d89f3`), session
`session-00065e3bb98f1b54-00`, records `PASS passed=10 total=10 dropped=0`
and `retries=0`. Lowercase, Shift, Caps Lock on/off, Backspace, Enter and all
four arrows passed on the internal `05ac:0262` keyboard. The final input
snapshot records 25 completed reports, zero errors and zero dropped input.
These tested keyboard functions are physically accepted on this unit.

Matching boot/exit logs are verified and archived outside `build`; the
post-return offline exFAT check is clean. Ubuntu default and consumed one-shot
are verified. No reboot is armed. This acceptance does not cover every key,
long-duration repeat, trackpad, desktop interaction or other device providers.
Next milestone is trackpad and desktop bring-up; retain the successful diagnostic
as previous and the accepted keyboard payload in the durable archive.

## Trackpad and desktop scope

Begin exact `05ac:0262` Wellspring7 support from this unit's captured configuration:
keyboard interface0 endpoint83 packet10, vendor-HID interface1 endpoint81 packet64
interval2, boot mouse interface2 endpoint84 packet8. Type2 contact layout and
feature mode switch are documented in Linux bcm5974; Air0291 Type3 differs.
Touch bounded decoder/profile/mode transport tests and desktop trial staging.
Do not enable sibling IDs or infer physical gestures from fixtures. Preserve
accepted keyboard behavior and explicit AML exclusion. Fetch found integration
`origin/main=b6075ef` (merged EC/AML PR4); this checkout still builds from its
recorded db40ed5 ancestry plus device commits, not the new EC code. No overlapping
trackpad edits were found in that integration delta. Prior typing run is collected.

Trackpad/desktop candidate `c4b7b7bb5eb48b3a`, source `ca645d9`, is v29 at
explicit 2880x1800/2x scale. Tests cover Type2 synthetic framing/bounds, exact
Pro interface/DCI and endpoint isolation, feature read/modify/write, outbound
control setup direction, failed mode switches and actual Type2 dispatch. Existing
Air fixture, keyboard, pointer and desktop host tests pass. Build and 30 staging
tests pass. Exact GPT VM trial saves typed Text content, a 1440x900 screenshot,
matching logs, and cleanly unmounts; the final instruction screen was inspected.

Native trial next: no time limit, on-screen pointer/click/drag/scroll checklist,
then save and Return to console (F10, End, Return if pointer fails), followed by
`reboot` after `USB unmounted=yes`. Retain accepted keyboard `29af8019a1737d6e`
as previous and archive this candidate before arming. Trackpad motion, gestures
and Retina desktop behavior remain unaccepted until native evidence returns.
Type2 protocol reference: [Linux bcm5974](https://raw.githubusercontent.com/torvalds/linux/master/drivers/input/mouse/bcm5974.c).

## Native desktop and trackpad result

Release `c4b7b7bb5eb48b3a` (source `ca645d9`), session
`session-00065e3be5f7034d-00`, returned with matching boot/exit logs. The user
reports that apps, keyboard and trackpad worked, with no brightness control or
sound. The desktop exited with status 0. The Type2 trackpad recorded 12,163
reports, 11,860 contact reports, 872 button reports, zero malformed reports and
zero transfer errors. Gesture telemetry records 2,492 scroll events and two
primary taps. Keyboard telemetry records 70 reports, zero errors and zero drops.
This accepts the exercised desktop/input behavior, not every app operation or
all gestures. The profile requested 2880x1800 at 2x scale.

Native logs are copied and verified in a durable archive outside `build` with
the exact payload. Offline exFAT is clean, the one-shot is consumed and Ubuntu
remains default. No reboot is armed. Preserve this accepted desktop payload for
subsequent recovery; the installed previous payload is the guided keyboard trial.

Brightness and HDA were not enabled in v29. The read-only laptop probe reports
unsupported chipset, and audio telemetry remains disabled with zero commands.
The existing brightness path matches Air GPU/PCH/firmware and a fixed PWM period;
the CS4208 audio path matches Air subsystem `106b:7e00`, whereas this unit captured
`106b:5e00`. Neither gate may simply be widened to claim Pro support.

Next bounded objective: read-only Pro backlight/register and codec-routing
evidence, then separate evidence-matched providers and tests. Potential touch
points are laptop probe/backlight, HDA codec selection and explicit diagnostic
profiles. Optional AML, EC ownership, fan control and sibling models remain out
of scope. Refetched `origin/main` remains `b6075ef`; open PR6 changes Air ACPI lock
handling, not these providers. This result still uses the recorded device branch
ancestry, not merged PR4 code. Collect-before-restage is complete; no new stage
or physical trial is prepared yet.

## Pro backlight diagnostic implementation scope

Proceeding under the standing bring-up authorization. First add a read-only
Pro display probe, gated on this unit's captured DSDT and GPU/PCH identities.
It will read only the four documented CPU/PCH PWM registers, without binding a
brightness writer or accessing EC ports. Use the existing unattended v19 GPT
capture/save/unmount/reboot workflow. Touch laptop probe/resource matching and
host tests; preserve the accepted desktop as previous. Native boot register
state is required before choosing a brightness control profile. Linux reports
maximum 1953, distinct from the Air's 2777. Audio evidence also shows two speaker
pins (0x12 and 0x13), so the Air's single-speaker route is insufficient.
`origin/main` remains b6075ef and PR6 remains ACPI-only at this scope refresh.

Diagnostic release `a488fbd08bf01e44`, source `7f2d122`, implements the exact
Pro DSDT/GPU/HM87 gate and reads PWM offsets 0x48250/0x48254/0xc8250/0xc8254
through read-only uncached mappings. It binds no writer and bypasses all Air
EC/resource accesses. Register definitions were checked against
[Linux v6.12 Intel backlight registers](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/gpu/drm/i915/display/intel_backlight_regs.h).
Build, existing backlight/resource tests, actual Pro firmware identity/refusal
tests and documentation checks pass. The exact v19 GPT VM rehearsal verifies
saved log bytes, clean exFAT, unmount and reset. Physical capture is next, under
existing automatic-reboot authorization; no user input is required. Collect this
release on return before implementing a writable provider or staging again.

## Native read-only Pro backlight result

Release `a488fbd08bf01e44`, session `session-00065e3c79b9a84e-00`, completed
the diagnostic and saved matching boot/exit logs. The exact firmware/chipset
gate matched. Native registers: CPU control/duty both zero, PCH control
`0xc0000000`, PCH period/duty `0x07a101f9`: override mode, period 1953,
duty 505 (about 26%). This matches the independently captured Linux range and
duty. No brightness writes, provider binding, EC access or audio initialization
occurred. This accepts discovery only, not brightness adjustment.

Logs, payload and collection metadata are archived outside `build`. The offline
exFAT check is clean, Ubuntu default and consumed one-shot are verified, and no
reboot is armed. Installed previous is the accepted desktop `c4b7b7bb5eb48b3a`.
Next implement an explicit Pro brightness profile using the observed mode and
period, preserving the Air profile and rejecting changed registers/readback
failures. Then test its shared Settings path before physical adjustment. Audio
remains a separate two-speaker-route implementation and acceptance milestone.

## Pro brightness provider scope

Implement the captured 1953-period PCH override profile explicitly, retaining
Air's 2777-period default and all mode/readback refusal checks. Touch backlight
binding, Pro probe mapping, host coverage, an opt-in scaled brightness desktop
profile and its VM harness. Keep sound and EC/AML disabled in this trial. Add
on-screen Settings instructions for deliberate brightness adjustment and exit.
The prior capture is collected; origin/main is still b6075ef and PR6 owns ACPI.

Brightness candidate `c7dd411a4ea31af4`, source `1f2d677`, uses v30 scaled
desktop with brightness enabled and audio/AML/SMC off. Startup binds the exact
Pro profile without changing duty. Host tests exercise both period profiles,
wrong-profile refusal, all 5–100% requests, drift quarantine and failed readback;
Settings message/response tests and 31 boot-profile checks pass. Full build and
exact GPT VM desktop rehearsal pass (typed document, screenshot and matching
logs saved; clean unmount). The initial guide was visually inspected: brightness
steps and console/reboot instructions are visible without scrolling.

Next physical check: adjust Settings to 25/50/75%, confirm visible panel changes
and responsive input, restore a comfortable level, then Return to console and
reboot after USB unmounted=yes. No deadline or automatic reboot in this desktop
trial. Retain accepted desktop c4b7b7bb5eb48b3a as previous; archive the candidate
and run record before arming. Brightness adjustment remains physically pending.

## Native brightness acceptance

Release `c7dd411a4ea31af4` (source `1f2d677`), session
`session-00065e3c9d5e862e-00`, has matching saved boot/exit logs. The user confirms
brightness worked well across the whole range. The exact Pro provider initialized
at 26% without changing the panel. All 108 logged adjustment requests matched
their readback, spanning 5–100% and finishing at 41%. This accepts manual
Settings brightness adjustment on this unit; media keys and automatic dimming
are not covered.

Desktop exit status is zero. Trackpad telemetry records 5,374 reports with zero
malformed reports or transfer errors. No keyboard reports occurred in this saved
snapshot; prior keyboard acceptance remains separate. Offline exFAT is clean,
Ubuntu default and consumed one-shot are verified, and no reboot is armed.
Logs and the accepted payload are archived outside `build`. Current installed
release is now accepted for brightness; previous remains `c4b7b7bb5eb48b3a`.
Preserve this brightness payload when staging audio work next. Sound remains
disabled in this profile and requires Pro-specific CS4208 routing and tests.

## Pro audio implementation scope

Add exact Intel 8c20 / CS4208 1013:4208 / subsystem 106b:5e00 support.
Captured outputs are headphone pin10/DAC2, speaker pin12/DAC3 and bass speaker
pin13/DAC4; all speaker converters must receive the stereo stream and be muted
when headphones are selected. Captured GPIO0 enables the speaker amplifier.
Validate Pro revision, pin defaults, connections and capabilities before codec
mutation. Scope: audio controller selection, codec route model/tests, explicit
scaled audio trial and on-screen sound checks. Preserve Air and QEMU behavior;
no HDMI, microphone, EC/AML or sibling support. origin/main remains b6075ef;
PR6 remains ACPI scope. Accepted brightness release remains recovery.

The captured Pro codec revision is 0x100300. Main speaker pin12/DAC3 and bass
pin13/DAC4 receive the same stereo stream at channel offset zero. Headphone
pin10/DAC2 is separate. All three paths are validated before coefficient/GPIO
setup; route switches mute and disconnect both speaker converters. GPIO0 is
updated while preserving unrelated bits. Exact Pro pin defaults are checked;
Air, unknown subsystem, changed revision or invalid bass connection is refused
under the Pro profile. Linux's pinned
[CS4208 board table and GPIO fixup](https://raw.githubusercontent.com/torvalds/linux/v6.12/sound/pci/hda/patch_cirrus.c)
corroborate subsystem 5e00 and GPIO0; local codec capture supplies the routes.
The controller uses the existing Intel PCH traffic-class/coherent-DMA setup,
consistent with the [Linux PCH driver](https://raw.githubusercontent.com/torvalds/linux/v6.12/sound/pci/hda/hda_intel.c).

Candidate `e47dca3a6b30c6c1` (source `240893c`) is the explicit v31 audio,
brightness and scaled desktop trial. Build, HDA/controller/codec tests, audio
service/stream tests, Settings tests and 32 profile checks pass. Exact GPT VM
rehearsal passes saved Text/screenshot/logs and clean USB unmount; the initialized
codec also produces one captured 500 ms stereo tone with a silent tail (peak
1200). This emulated codec is not the physical Cirrus device. The initial Help
shows the sound checks; scroll for full exit and WAV playback instructions.
Physical speaker/headphone routing, quality and channel balance remain pending.

The separate stereo-48000 streaming VM regression also passes against this
build. No concurrent harness ran. Next install preserves accepted brightness
`c7dd411a4ea31af4` as previous. User listening checks use Settings Test/Stop at
low volume, optional wired headphones with speaker isolation, and the bundled
stereo WAV from the on-screen guide. Startup remains silent and this manual
trial has no deadline. Collect the exact session on return before another stage.

## Native audio result and trackpad latency report

Release `e47dca3a6b30c6c1`, session `session-00065e3cc8a4f706-00`, has
matching boot/exit logs and a clean desktop exit. The user reports sound worked
well. The exact CS4208/5e00 codec is ready with no error, two tone tests completed,
and PCM playback submitted 1,152,000 bytes with zero underruns. Final route is
speakers at 25%. Audible output is user-confirmed; headphone isolation and
individual main/bass output balance were not separately confirmed.

Trackpad telemetry records 3,020 reports, zero malformed frames and zero transfer
errors. The user reports noticeably slower cursor response than Ubuntu, estimated
at 15–45 ms. Preserve this as a subjective observation, not a measured bound or
a proven USB transport fault. Code inspection finds an input polling worker
waiting one timer tick and a compositor loop sleeping one tick, with a calibrated
100 Hz timer (10 ms ticks). Their scheduling plus rendering/scanout may contribute;
existing logs cannot apportion the end-to-end delay.

The matching evidence and sound-confirmed payload are archived outside `build`.
Offline exFAT is clean, Ubuntu default and consumed one-shot are verified, and
no reboot is armed. Accepted brightness `c7dd411a4ea31af4` remains previous.
Next bounded investigation: measure input polling, queue consumption and cursor
presentation timing under the scaled desktop, then choose a change supported by
the measurements. Inspect shared input/compositor/scheduler scope on origin
before editing; avoid a global timer-rate increase without affected regressions.

## Cursor timing scope

The refreshed Air PR6 now touches startup and boot-profile tools for battery
work. Avoid those overlapping files for this diagnostic. Add bounded timing
aggregates to the existing `--hardware-trial` compositor only, using the current
v31 stage unchanged: pointer-consumption cadence, drawing/blit duration and
first consumed pointer event to final framebuffer write. No event ABI, timer,
scheduler or xHCI behavior changes. Report measurements once at exit, not per
frame. They exclude device/USB arrival delay and scanout, and are not end-to-end
latency. Preserve the accepted sound build while measuring the scaled desktop.

The diagnostic records four software intervals in the existing hardware-trial
mode: each pointer-poll gap; gaps between consecutive polls that both consume
motion; first consumed pointer event through final successful framebuffer blit;
and drawing plus blit duration. Histogram percentiles are upper bucket bounds,
not exact percentiles. A coalesced input batch produces one presentation sample.
Idle poll gaps are separate from consecutive-motion gaps. Failed draws and
unavailable/backward clocks produce no duration sample. USB/device delay,
time already spent in the kernel input queue, and panel scanout are excluded.
No scheduler, xHCI or event ABI was changed; normal desktop mode has no added
clock reads. The standalone host test covers batching, gaps, failures and clocks.

Timing candidate `cae101ddceccbd1d`, source `74073ee`, passes build, standalone
host timing tests and exact v31 GPT desktop/audio rehearsal. A local wrapper
injects twelve VM pointer movements and verifies the saved timing records;
consume-to-final-blit has twelve samples. The sparse injected movements do not
exercise consecutive-motion sampling (zero samples, not a pass for that metric).
The host test covers its accounting; continuous physical movement is required.
The VM also saves document/screenshot/logs, cleanly unmounts and verifies a tone.
VM timing values are emulator observations and must not be used as laptop latency.

Next native run: move continuously over desktop and Text, then drag a window,
about 15 seconds each. Save and Return to console, then reboot after clean
unmount; all instructions remain in Help. Preserve sound-confirmed
`e47dca3a6b30c6c1` as previous. Collect the exact session and compare active
cadence versus draw/consume durations before selecting any latency fix.

## Native cursor timing result

Release `cae101ddceccbd1d`, session `session-00065e3d2bb52f21-00`, saved
all four timing aggregates and exited cleanly. Native measurements:

| Software interval | Samples | Mean | Maximum | p95 upper bound |
| --- | ---: | ---: | ---: | ---: |
| Pointer poll gap | 32,504 | 9,999 us | 12,650 us | 12,650 us |
| Consecutive motion poll gap | 5,713 | 9,999 us | 12,191 us | 12,191 us |
| First consumed event to final blit | 6,035 | 392 us | 2,412 us | 2,000 us |
| Draw plus blit | 6,098 | 402 us | 3,570 us | 2,000 us |

These results establish the 10 ms compositor sampling cadence and fast drawing
in this run. They support investigating input scheduling before rendering
optimization; they do not measure USB arrival, kernel queue age or panel scanout,
and do not prove the entire subjective 15–45 ms delay is software polling.
Histogram percentile bounds must not be represented as exact quantiles.
Trackpad reports total 6,667 with zero malformed reports or transfer errors.

Matching logs and collection metadata are archived outside `build`. Offline
exFAT is clean, Ubuntu default and consumed one-shot are verified, and no reboot
is armed. The USB UUID is unchanged but Linux assigned a different device node;
the local checkpoint now reflects it. Sound-confirmed `e47dca3a6b30c6c1` remains
installed previous. Next inspect event-driven pointer wake and controller polling
coordination, retaining timer/idle behavior until affected regressions are covered.

## Pointer wake implementation scope

Add a bounded pointer wait syscall and mouse-queue wake notification. The desktop
will wait for queued pointer input or one tick, replacing unconditional one-tick
sleep. Keep the 100 Hz timer, xHCI polling and storage ownership unchanged for a
controlled comparison. Dedicated queue readiness is rechecked with IRQ exclusion
by the scheduler to avoid lost wakes. Signal/cancel checks remain bounded by one
tick. Touch mouse queue, syscall number/dispatch/wrapper, compositor and existing
mouse/sleep host tests. Refreshed PR6 does not touch these files; avoid its startup,
profile and Makefile changes. Native timing baseline remains 9,999 us cadence.

Implementation `58884d6` adds syscall 133 (`srv_pointer_wait`) with a dedicated
mouse wait queue, one-tick deadline, non-consuming readiness and bounded
signal/exit checks. Scheduler readiness recheck closes the enqueue-before-block
race. Enqueue/overflow/cancel paths notify waiters; motion coalescing retains
existing readiness without redundant notifications. Desktop fallback remains
one-tick sleep if the new syscall is unavailable or interrupted. Hardware trial
logs count input-ready returns, timeout returns and errors.

Full build/USB image and `test-core` pass, including mouse order/overflow,
arrival at the wait boundary, arrival while blocked, timeout, signal/exit,
early-scheduler fallback and storage-safe syscall admission. The xHCI worker
still samples at the original rate, so this change can remove an extra desktop
wait but cannot by itself guarantee sub-10 ms USB sampling or total latency.

Candidate `9ae804d340bee2cb` (source `98fac37`, implementation `58884d6`)
passes the exact v31 GPT desktop/audio rehearsal. Twelve injected VM pointer
samples are saved, with ten pointer-ready returns, 1,264 timeouts and zero wait
errors. Document/screenshot/log persistence, clean unmount and captured tone
also pass. Emulated pointer delivery does not accept native USB responsiveness.

Matched 10-second idle checks use the archived pre-change timing release
`cae101ddceccbd1d` in an isolated image tree and the new build. Host QEMU desktop
CPU is 10.88% before versus 10.08% after; console CPU is 5.99% in both. Both
checks verify subsequent keyboard entry and saved file bytes. These short VM
observations show no busy-wait increase in the check, not a physical energy
measurement or statistically established CPU reduction. Preserve raw metrics
and baseline script with the candidate.

Next native run repeats continuous movement and dragging, compares perceived
response, and collects wait counts/timing. Keep sound-confirmed e47dca3a6b30c6c1
as previous rather than promoting the timing-only candidate. No timer-rate or
xHCI changes are in this release; remaining transport/scanout delay is separate.


## Native pointer wake result

Release `9ae804d340bee2cb`, source `98fac37`, session
`session-00065e3d67ca2723-00` returned with a clean desktop exit. The bounded
wait recorded 4,130 input-ready returns, 62,866 timeouts and zero errors.
Trackpad capture recorded 4,466 reports, zero malformed reports and zero
transfer errors. No audio tone was exercised in this run; the earlier audible
acceptance remains separate.

| Software interval | Samples | Mean | Maximum | p95 upper bound |
| --- | ---: | ---: | ---: | ---: |
| Pointer poll gap | 66,995 | 9,968 us | 12,494 us | 12,494 us |
| Consecutive motion poll gap | 3,820 | 9,854 us | 11,541 us | 11,541 us |
| First consumed event to final blit | 4,129 | 599 us | 3,962 us | 2,000 us |
| Draw plus blit | 4,162 | 601 us | 3,960 us | 2,000 us |

The input-ready path ran successfully, but active cadence remains close to
10 ms (previous mean 9,999 us). These differing manual workloads do not establish
a statistically significant improvement. Drawing remains below 1 ms on average;
USB arrival, kernel queue residence and scanout remain outside these intervals.
The user reports that response felt **worse**. This candidate fails perceived
responsiveness acceptance; clean logs do not override that regression.
Next diagnostic scope is controller delivery and queue residence, if the delay
remains, before choosing another scheduling or transport change.

Matching evidence is archived outside `build`; offline exFAT is clean, installed
payload hashes and Ubuntu default are verified, and the one-shot is consumed.
No reboot is armed. Current is the pointer-wake candidate; sound-confirmed
`e47dca3a6b30c6c1` remains previous and independently archived. Refreshed
`origin/main` is still `b6075ef`; Air PR6 at `0038ff4` owns the native battery
work and does not alter this input implementation. This return changes only
this machine's evidence record, not shared driver behavior.


Following that report, the installed current SrvrOS entry was restored to the
sound-confirmed `e47dca3a6b30c6c1`. The regressed pointer-wake release is retained
as previous for diagnosis only, with logs and payload independently archived.
Ubuntu remains the default; no reboot was requested or armed. Source retains
the published experimental implementation for review; it is not accepted as a
latency fix. Next work should measure USB delivery and queue residence and
isolate scheduling effects before another physical candidate.

## Queue and report timing scope

After the rejected wake trial, restore the compositor's unconditional one-tick
wait and add bounded aggregate diagnostics to mouse input and xHCI trackpad
processing. Record oldest/latest coalesced event queue residence, consecutive
contact report processing gaps, and processing-to-rearm time. No input event
ABI, controller polling rate or timer change. These timestamps cannot observe
hardware report arrival or display scanout. Touch mouse input/header, xHCI,
compositor and mouse host fixtures; avoid PR6's startup and harness changes.
Integration baseline remains b6075ef. Accepted installed payload stays unchanged
until exact-stage validation of a diagnostic candidate.

The diagnostic implementation restores fixed one-tick desktop sleeping and logs
that policy explicitly. Queue timestamps exclude mixed sources and synthetic
cancellation/overflow events. Fixed-size counters reject unavailable/backwards
clocks and aggregate overflow; no per-event logging is added. Full build,
USB image, test-core and documentation checks pass. Mouse fixtures cover
coalescing, mixed sources, backwards clocks, cancellation and overflow; xHCI
fixtures cover report/rearm samples and lift sequence reset. Native timing
and perceived response remain untested for this diagnostic build.

Diagnostic candidate `ad6d1b9700372be6` (source `6968afb`) passes exact v31
GPT desktop/audio rehearsal: scaled display, document/screenshot/log persistence,
clean unmount and a captured tone. The VM verifies fixed-one-tick policy and the
queue aggregate log format; its emulated mouse supplies zero Apple trackpad
queue samples, so it does not validate native residence values. On-screen Help
requests continuous circles over desktop/Text and window dragging for about
15 seconds each, with untimed manual exit and reboot instructions. Preserve
sound-confirmed `e47dca3a6b30c6c1` as previous. Next collect this exact session
before any further stage, compare queue and dispatch intervals, and ask whether
response returned to the earlier baseline.

## Final input diagnostic and remaining bring-up — 2026-10-20

Release `ad6d1b9700372be6`, session `session-00065e3db88dea73-00`, returned
with desktop exit 0, fixed-one-tick policy, 2,590 trackpad reports and zero
malformed/transfer errors. Queue residence: 2,375 samples, oldest mean 358 us
(max 10,460), newest mean 247 us (max 8,040). Consecutive contact dispatch gaps:
2,551 samples, mean 9,829 us, max 26,009. Dispatch-to-rearm: 2,590 samples,
mean 2 us, max 6. Consecutive motion polls averaged 10,000 us; consumed-event
to final blit averaged 77 us. These are software intervals, not finger-to-pixel
latency; this run does not prove a subjective improvement.

The user completed testing and requested moving on after this round. Further
trackpad latency work is deferred, not marked fixed. The failed pointer-wake
behavior remains disabled. Evidence is independently archived, offline exFAT
is clean, installed hashes/Ubuntu default are verified and no reboot is armed.
Current payload is this diagnostic; sound-confirmed `e47dca3a6b30c6c1` remains
previous and independently archived.

Current capability ledger and suggested next order:

| Area | Local status / remaining work |
| --- | --- |
| Boot/recovery, scaled internal display, keyboard, desktop apps, USB workspace | Exercised successfully; broaden cold/warm boot and storage-load regression coverage |
| Trackpad | Functional; latency investigation deferred; gesture combinations need separate exhaustive acceptance |
| Brightness | User confirmed full offered 5–100% range |
| Audio | User confirmed sound; headphone isolation, individual channel/route checks and microphone remain separate |
| CPU/thermal/fan telemetry | Native CPU temperature/package energy/active-frequency idle capture accepted below; native paired two-fan readout accepted below; fan control not accepted |
| Battery and charger | Native Pro ACPI/EC ownership and live battery/AC reporting not accepted; reuse reviewed shared groundwork, not Air firmware assumptions |
| Networking | No accepted physical network path; validate supported wired USB hardware first; internal Broadcom Wi-Fi is separate driver work |
| Power behavior | Lid events, orderly power-off and suspend/resume each need implementation/acceptance review; idle/load power baseline pending |
| Additional hardware | External displays/hotplug, Bluetooth, camera, SD and Thunderbolt peripherals remain separate later milestones |
| Storage policy | RAM root and USB workspace remain intentional; native internal-disk writes require a separate scope |
| Integration | Device changes published in PR5; review/integration and regressions on the shared baseline still required |

Recommended next bounded milestone: read-only native CPU temperature/fan and
idle/load telemetry matched to this unit's inventory. Keep fan control and
optional AML out of normal startup. Battery/AC follows its shared runtime
review and Pro-specific firmware evidence. No next boot has been scheduled.

## Read-only telemetry scope — 2026-10-20

Begin with native CPU temperature/package energy/CPU0 active frequency, then
separately implement this unit's two-fan readout. Linux CPUID is 0x40661,
GenuineIntel family 6/model 0x46; leaf 6 EAX=0x77 and ECX=9. CPU0 reads of
0x606, 0xce, 0x611, 0xe8, 0xe7, 0x1a2, 0x19c and 0x1b1 succeeded. Units are
0xa0e03, nominal ratio 25, temperature target 100 C with zero TCC offset.
Linux exposes Left side and Right side fans. Development-load samples are
fixtures, not an idle baseline or native acceptance.

Bounded first change: explicit 06_46 CPU profile alongside 06_3D in thermal
and power predicates, captured decoder fixtures and service refusal tests.
No MSR writes, fan control, EC/AML activation or frequency policy changes.
Use the existing v26 unattended desktop idle capture; SMC remains unavailable
on this Pro until an explicit two-fan profile and ABI are validated. Avoid
PR6 startup/boot-harness files. Refreshed origin/main remains b6075ef; PR6 at
6db5a12 retains battery/AC runtime ownership. Preserve accepted desktop recovery.

Preflight found that v26 launches idle capture without explicit scale, while
this Pro may supply 2880x1800 and the screenshot protocol caps BMPs at 8 MiB.
Extend scope to compositor idle-capture scale selection and capture host tests:
when no scale is supplied and the full framebuffer exceeds capture limits,
select 2x if its logical dimensions fit. Explicit scale and ordinary desktop
remain unchanged. This avoids editing PR6's startup/boot-profile paths.

The CPU profile and idle-capture sizing implementation passes full build,
USB image, and test-core. Captured Pro thermal/power values are replayed beside
the existing Air fixtures; unsupported sibling model IDs, hypervisors and
missing features remain refused. Capture tests cover Air/full-HD 1x, Retina/4K
2x and over-limit dimensions. The compositor preserves explicit scale choices.
No native Pro sensor acceptance is claimed before the unattended capture.

Candidate `de341a3cfcaa7acb`, source `f58dee1`, passes the exact v26 GPT
rehearsal at 2880x1800: automatic 2x logical desktop, 60-second idle window,
1440x900 saved screenshot, persisted logs, clean unmount and automatic reset.
VM power/thermal providers correctly report unavailable, with zero readings;
this validates the workflow and refusal, not native sensor values. Separate
power smoke verifies default-disabled and opt-in hypervisor refusal. Native
acceptance requires valid fresh thermal samples and a non-null continuous
power window within the idle measurement. SMC remains intentionally unavailable
on this Pro; two-fan support and controlled load measurements follow separately.

The same release's missing-workspace VM check also passes: it halts with
`saved=NO unmounted=NO` and does not reboot. Success and expected-failure
receipts are retained separately. The physical trial is unattended: leave input
alone for the idle interval; it saves/unmounts and returns automatically on
success. Keep sound-confirmed e47dca3a6b30c6c1 as recovery and collect the exact
release before starting fan implementation or another stage.

## Native CPU telemetry result and two-fan scope — 2026-10-20

Release de341a3cfcaa7acb, session session-00065e3debdfd870-00, completed the
59.992-second Text idle capture. Six valid power checkpoints cover 52.993 seconds:
9,161 mW package average, 3,466 MHz CPU0 active frequency, and 93.85% scheduler
idle samples. No rejected power samples. Thermal samples remained valid; initial
CPU/package was 81 C, final 66 C after screenshot work (100 C target). These
short-run readings are not equilibrium, whole-laptop power or a load baseline.
The screenshot is 1440x900, copied with matching hash. Logs and independent
archive verified; offline exFAT clean; Ubuntu default/consumed one-shot verified.

Next bounded change: Pro two-fan readout, preserving Air behavior. Captured
DSDT APP0001 at _SB.PCI0.LPCB.SMC declares I/O 0x300..0x31f, MMIO 0xfef00000
and IRQ6; use only the proven bounded I/O read protocol, no MMIO/IRQ ownership.
Linux-driver-mediated reads show FNum ui8 length1=02, F0Ac/F1Ac fpe2 length2
(raw 23a0 and 2120 at capture). Only those three keys were captured; the Linux
key selector was restored. No control keys or firmware mutations.

Touch SMC service, its host fixtures, versioned SMC status ABI and System fan
formatting. Exact Pro GPU/PCH plus existing captured DSDT identity gates access;
validate count=2 and both key metadata before publishing a complete fan pair.
ABI v2 appends a second fan value; all kernel/userspace callers build together.
Air remains exactly one fan. Quarantine either fan's transport failure and
invalidate the whole pair. Shared Air PR6 at 09b48ac does not touch these files;
origin/main remains b6075ef. CPU capture is collected before any new staging.

Two-fan implementation passes full build/USB image/test-core and focused SMC
host tests. Tests retain Air single-fan behavior, require exact Pro identities,
reject count mismatch and bad second-key metadata, verify F0/F1 read command
keys, publish only a complete pair (including valid zero RPM), and invalidate
both on a second-fan timeout. Protocol fixtures reject control-write commands.
The matched ABI v2 build updates System to display both speeds. Native readout
and the visible two-fan System presentation are not yet physically accepted.

Two-fan candidate `d2928b8e81f148cd`, source `3604d74`, passes the exact
v26 GPT/Retina VM capture: 60-second idle window, 1440x900 screenshot, saved
logs, clean unmount and automatic reset. The VM correctly refuses the native
SMC profile. A separate missing-workspace rehearsal halts without saving,
unmounting or rebooting. Receipts are retained separately. The next physical
run is unattended and must show a fresh complete two-fan pair with no failures,
as well as the completed idle capture/save/unmount/return. Visible System fan
formatting remains a separate interactive check. Keep the sound-confirmed
`e47dca3a6b30c6c1` recovery payload and collect this exact release on return.


## Native two-fan readout accepted — 2026-10-20

Release `d2928b8e81f148cd` (source `3604d74`), session
`session-00065e3e2405d9d1-00`, completed the automatic capture and returned to
Ubuntu. SMC reached active with two fans: 76 complete pairs, 156 transactions,
zero failures/errors. Final F0Ac/F1Ac readings were 2,162/1,991 RPM, published
with the same timestamp and count. Nine periodic snapshots were valid, fresh
under three seconds, and advancing. This accepts native read-only paired fan
telemetry; it does not accept fan control or the visible System presentation.

The 59.997-second Text capture had 93.88% scheduler idle samples. Six package
energy checkpoints covered 52.998 seconds at 9,246 mW average and 3,466 MHz
CPU0 active frequency. No rejected power samples; final temperature was 71 C
after screenshot work. These are short package-only measurements, not thermal
equilibrium, whole-machine power, or a controlled load baseline. The saved
1440x900 screenshot and matching logs were copied and independently archived.
Offline exFAT is clean; installed hashes, Ubuntu default and consumed one-shot
were verified. No reboot is armed. Current remains this diagnostic; accepted
sound-enabled `e47dca3a6b30c6c1` remains previous. Installation reclaimed one
obsolete unreferenced payload only after verifying its durable archive.

Next bounded milestone: controlled load/thermal baseline and visible System
readout validation. Battery/AC still depends on reviewed shared runtime work
and this Pro's firmware evidence. Trackpad latency remains deferred by request.

## Controlled-load preparation — 2026-10-20

Refreshed main remains b6075ef; Air PR6 is now e9af5d3 and owns active
battery/USB capture changes in startup, boot profiles, collectors and Makefile.
Prepare an opt-in single-worker load diagnostic inside existing threadstress,
with independent host fixtures, plus System two-fan display fixtures. No shared
boot/profile edits in this scope. Fixed idle/load/recovery phases, bounded work
chunks, fresh valid sensors and thermal stop conditions are required. This is
not a CPU policy or fan-control change. Native automatic profile wiring remains
a review dependency; do not label the existing idle profile a load capture or
reboot into a payload without the necessary save/return path.

The opt-in diagnostic is implemented as `threadstress --cpu-load-capture`;
ordinary threadstress behavior is unchanged. It has fixed 30/30/30-second
idle/load/recovery phases, one integer worker, 100 ms sensor checks, one-second
logs, and fail-closed version/state/freshness validation. It refuses load entry
at 80 C and stops at min(90 C, reference minus 10 C), on throttling/critical
flags, sensor failure or clock failure. No fan or CPU-policy writes.

Full OS/USB build, System fixtures (including both native fan values, valid zero
pair and invalid pair), and standalone load fixtures passed. Twelve negative
fixture scenarios and two successful model-shaped runs passed. Default-disabled
and opt-in hypervisor VM runs both refused load and returned to the monitor.
These establish implementation/refusal behavior, not native load performance.

Remaining dependency: a distinct reviewed capture profile and phase-aware power
collector, touching PR6-owned startup/staging/collection paths. Defer that shared
integration until ownership is resolved; do not reuse the idle acceptance label
or alter a staged release. The System display has host-formatting evidence but
still needs a native visible check. Existing installed fan diagnostic and accepted
recovery remain unchanged; no reboot is armed. Local test logs are under
`build/bringup/pro-load-*` and the standalone fixture command is documented in
`docs/power-telemetry.md`.

## Proposed load capture wiring — 2026-10-20

User requested proceeding after the dependency report. Main remains b6075ef;
Air PR6 is 556fec5, with v34 battery/USB overlap active. Prepare a distinct v35
CPU-load proposal on this device branch: a small opt-in startup branch, immutable
stage profile, exact-stage refusal rehearsal and separate phase-aware collector.
This overlaps PR6-owned startup/staging files, so defer shared integration and
resolve those hunks during review. Do not merge or replace the Air implementation.
The proposed profile runs the bounded standalone worker and saves its result,
unmounts and returns on handled sensor refusal as well as success. Missing
workspace/save/unmount remains a halt. No optional AML or EC ownership is added.

Proposed v35 implementation passes full build/USB image/test-core, existing
installer tests, bounded worker fixtures and new phase/profile fixtures.
Separate phase parsing refuses missing completion, failure markers, zero load
work and inconsistent boundaries; invalid power is null. Proceed to exact-stage
VM refusal/save/unmount/reset and missing-workspace checks before installation.

Candidate `9b7ff9eb6ebc3535`, payload source `c0f5fd5`, passes exact v35
GPT rehearsal: VM sensors/power remain unavailable, worker refuses without load,
failed result is persisted, exFAT verifies clean, and reset occurs. Separate
missing-workspace rehearsal halts with saved=NO/unmounted=NO and no reset.
The successful receipt explicitly records refused load and unavailable VM power;
it is workflow evidence, not load acceptance. Worker/collector completion checks
also require a successful process return. Rehearsal evidence is retained separately.

Next physical test is unattended: 30 seconds console idle, 30 seconds single
worker load, 30 seconds recovery, then save/unmount/return. A thermal/sensor
refusal may return earlier and must be collected as failure. No commands are
required. Preserve sound-confirmed recovery; inspect the exact new session before
any retry. System's visible two-fan presentation remains a separate check.

## Native bounded load capture — 2026-10-20

Release `9b7ff9eb6ebc3535`, session `session-00065e4a3e439b0a-00`, completed
all three console phases and returned successfully. Idle/load/recovery lasted
30.006/30.000/30.010 seconds; load completed 573,820 integer work chunks.
No thermal/sensor stop occurred. Logged core/package temperature peaked at
81 C during load; the last recovery sample was 60/62 C. Both fan readings
remained available, with 90 complete pairs and zero SMC errors at exit.

Three valid energy checkpoints cover 22.000 seconds inside load: 27,190 mW
package average and 3,500 MHz CPU0 active frequency. Recovery checkpoints cover
21.000 seconds: 9,293 mW and 3,462 MHz active frequency. Initial idle power is
unavailable: its two eligible checkpoints span 9,999,904 us, below the collector's
10-second minimum. Preserve that refusal rather than relaxing acceptance to
fit the result. Short console/single-worker data is not whole-machine energy,
thermal equilibrium, all-core stress or a desktop workload comparison.

Matching logs are copied and independently archived. Installed and accepted
recovery hashes, Ubuntu default and consumed one-shot are verified. Offline
exFAT is clean. No reboot is armed; current is the load diagnostic and previous
remains sound-confirmed `e47dca3a6b30c6c1`. The native visible System fan check,
shared integration review, and battery/AC dependencies remain open.

## Visible sensor trial scope — 2026-10-20

Prepare a distinct v36 scaled interactive desktop profile with accepted brightness,
audio and explicit CPU/SMC readout. Main remains b6075ef; Air PR6 at 41ecb6e
continues battery/USB work. Keep this small boot-profile proposal on the device
branch pending shared integration review. Update the opening Help checklist with
the two-fan check and exit commands. No timeout: user checks System/Sensors,
Pause/Resume and screenshot, then returns manually using the on-screen steps.
No AML, battery runtime, fan control or additional CPU load in this profile.

Candidate `c30ec628bf5d68aa`, source `831ba62`, passes full build/USB image,
System formatting fixtures, installer/profile checks and exact v36 GPT VM trial.
The trial verifies 2880x1800/2x, System startup, Text persistence, screenshot,
clean exit/unmount and an emulated audio tone. VM providers cannot accept native
fan visibility. The opening Help checklist carries the requested actions and
manual exit commands. Preserve the accepted sound payload as previous, archive
this candidate, then boot once for the user's untimed Sensors check. Collect
its matching session and screenshot before any further stage.

## Native System display accepted — 2026-10-20

Release `c30ec628bf5d68aa`, session `session-00065e4a63c1d349-00`, returned
with a saved 1440x900 screenshot showing Devices and Sensors side by side.
The user reported saving the devices/fans screenshot. Visual inspection confirms
readable L 2,161 / R 2,011 RPM, fan sample age 679 ms, core/package 47/51 C,
and no clipping of the paired values. Keyboard/trackpad and USB storage show
active drivers. Exit diagnostics show 6,330 paired samples, zero SMC errors,
and a clean compositor exit. Pause/Resume is unconfirmed in the retained evidence;
do not infer it from the screenshot. Battery/AC remains unavailable as expected.

Logs and screenshot were copied, hash-verified and independently archived.
Installed/recovery hashes, Ubuntu default and consumed one-shot were verified;
offline exFAT is clean. Current is the interactive sensor desktop, previous
remains sound-confirmed `e47dca3a6b30c6c1`; no reboot is armed. Shared integration
review and Pro-specific battery/AC groundwork are the next outstanding areas.

## Battery work deferred by user — 2026-10-20

The user explicitly deferred battery/charger work because another laptop is
handling it. Do not begin Pro battery/AC implementation or physical trials in
this workstream for now. Retain the unavailable status and consume reviewed
shared work later when requested. Next work should focus on shared integration
review and remaining non-battery acceptance; trackpad latency also remains
deferred under the earlier instruction. No reboot or new trial is scheduled.

## Rebase onto merged PR6 — 2026-10-20

At the user's explicit request, rebased the 75 device commits onto main
`d72d2af` (PR6), overriding the usual merge-without-rewrite preference. Local
backup `codex/pro-pre-rebase-20261020` preserves the published pre-rebase tip.
Resolved shared startup/staging/rehearsal conflicts by retaining both the merged
battery modes and Pro keyboard/load/scaled desktop paths. Battery implementation
and physical tests on this Pro remain deferred by user request.

Version numbers 31/36 exist in both probe and desktop namespaces. Scope Retina
resolution to desktop profiles so the merged battery profile recipes remain
unchanged. A compatibility matrix verifies all 42 historical main/Pro profile
configurations byte-for-byte; focused tests cover shared-number separation and
conflicting capture-mode refusal. Existing boot manifests and installed recovery
remain historical artifacts; rebasing source does not physically accept a new
build. No physical install or reboot is part of this rebase task.

Rebase validation passes: full OS/USB build and test-core; 40 installer/profile
checks, 12 desktop collector checks, load worker/parser fixtures, and docs links.
Exact GPT VM rehearsals pass for sensor desktop `8d8b85e8a68d1042` (Retina,
System startup, audio and USB save/unmount), load refusal `ea1a686c92c4fdce`
(saved failure and automatic return), and guided keyboard `896a375f17a793c4`
(wrong-key retries followed by all ten steps and saved return). These images
are VM-only. Installed accepted desktop/recovery are unchanged, and no reboot
is armed. Rebased history is published using an exact remote-head lease.

## Rebased physical regression scope — 2026-10-20

User authorized the next physical regression. Use VM-passed sensor desktop
`8d8b85e8a68d1042`, payload source `96840d9`, based on main `d72d2af`.
Fetch confirms main unchanged; Air PR7 adds host allocation checking and does
not change this payload. Its read-only checker was inspected and run separately
on this unit's unmounted USB: 65 files, 204 accounted clusters, zero leaks,
FAT/bitmap mismatches or errors. No shared implementation changes in this scope.

Check keyboard/trackpad, apps, brightness range, speaker audio, System's two
fan readings (including Pause/Resume), Text saving and a screenshot together.
Opening Help carries manual exit commands; this is untimed. Preserve Ubuntu
and sound-confirmed previous `e47dca3a6b30c6c1`, archive candidate and exact VM
receipt, and collect the matching new session before promotion or another run.
Battery/charger and trackpad latency work remain explicitly deferred.

## Rebased desktop native regression — 2026-10-20

Release `8d8b85e8a68d1042` (payload `96840d9`), session
`session-00065e4c52c62152-00`, exited cleanly with no collector attention.
The user confirmed typing, trackpad, apps, brightness, sound, both fan readings
and Pause/Resume worked; Text saving was explicitly skipped this round.
Do not claim a new native Text save/reopen pass. Screenshot persistence passed.

The saved screenshot shows L 2,156 / R 2,007 RPM, fan age 539 ms and core/package
46/51 C. Logs record pause/resume, 2,415 paired fan samples and zero SMC errors.
Matching logs and screenshot were copied, hash-verified and archived outside
`build`. Both offline exFAT checks pass, including allocation accounting with
no orphan/mismatch errors. Ubuntu default, consumed one-shot and installed/
archived recovery hashes verify. Current remains the rebased desktop; previous
remains `e47dca3a6b30c6c1`. No reboot is armed.

The named regression checks are physically accepted on this unit. Next remaining
non-battery bringup checks are headphone output/isolation and supported wired
USB networking, preceded by inventory and driver compatibility review. Battery
and trackpad latency remain deferred.

## Multiple-adapter groundwork scope — 2026-10-20

User is obtaining wired USB hardware; headphone tests are deferred for several
days. Prepare bounded provider inventory and System/Network Utility UI so every
registered adapter has a boot-local identity, driver/MAC/link state and explicit
active/inactive role. Preserve the single boot-selected IP stack and existing
provider priority; do not imply multihoming, live switching or arbitrary chipset
support. Touch net_device, the size-negotiated network status ABI, System and
Network Utility plus host/VM fixtures and current guidance. Main remains d72d2af;
Air PR7 owns filesystem/battery groundwork, with no network overlap identified.
No physical reboot is needed for this work. Hardware acceptance follows actual
adapter inventory. Battery and pointer latency remain deferred.

The first desktop network VM reproduced a hang after starting an unreachable
LAN ping: successful ping/DNS and report copies had already passed. ARP/DHCP/
DNS/ICMP timeout loops yield without blocking; the current idle-as-fallback
scheduler can select the same IRQ-masked caller and starve timer progress.
Replace these four polling yields with bounded RX wait-queue sleeps. Keep shared
scheduler/storage policy unchanged; rerun unreachable-target cancellation and
GUI responsiveness as well as adapter coexistence before accepting the fix.

Groundwork validation passes: full build/USB image/test-core, focused registry,
System, Network Utility and RX-worker fixtures, and 383 documentation links.
After the bounded wait fix, the mixed e1000/ECM desktop VM passes Ping/DNS,
adapter-attributed report copies into Text, typing during an unreachable ping,
cancellation, link down/up, compact layout, Help and clean exit. Screenshot
review confirms both adapter identities/roles are readable. The two-ECM VM passes
DHCP, successful and timed-out ping, 160 UDP echoes with packet-size/ZLP checks,
HTTP, USB keyboard coexistence, inactive traffic isolation and disconnect without
failover. Preserve the first failing desktop VM as regression evidence.

No native candidate was installed or rebooted. Current physical release stays
`8d8b85e8a68d1042`; its earlier acceptance does not cover this new networking
code. Headphone testing awaits hardware access. Next: capture the supplied USB
adapters' IDs/descriptors, assess chipset support, then stage a network-enabled
matched image with exact-stage VM validation before physical testing.

## Attached USB Ethernet trial scope — 2026-10-20

Captured adapter 0b95:1790 revision 0200 at SuperSpeed (5000 Mb/s USB).
Linux binds cdc_ncm to configuration 2 and reports an active 1000 Mb/s link.
The captured three configurations are vendor-specific, NCM, then ECM. Target
ECM configuration 3: control interface 0, data interface 1 alternate 1, MAC
string 5, 1514-byte segments, notification 0x81/16 bytes, bulk IN 0x82/1024
burst 5 and OUT 0x03/1024 burst 2. This is evidence for this adapter revision,
not blanket support for all products named AX88179.

Its SuperSpeed notification companion advertises zero bytes per interval, so
the strict generic ECM parser refuses it. Prepare a narrowly gated parser
compatibility path for the exact captured descriptor bytes and VID/PID/revision;
retain generic rejection and all transfer bounds. Add an immutable v37 scaled
network desktop with on-screen DHCP/diagnostic/return instructions. Validate
parser mutations, old recipes, full build and exact-stage USB networking VM
before an authorized native trial. Shared main remains d72d2af; Air PR7 has no
network overlap. Ubuntu and accepted desktop/recovery remain protected.

Candidate `debb2ddf3e9f4243`, source `1b2afb0`, passes exact v37 GPT VM
rehearsal: emulated USB ECM DHCP, saved successful router ping, 2880x1800/2x
Text/screenshot persistence, audio and clean return/unmount. The host ECM
fixture validates the captured configuration, all truncations, altered-byte and
sibling identity refusal; xHCI transport regressions and 41 boot-profile tests
pass. Two earlier rehearsals exposed Terminal close-confirmation/shortcut
requirements; retain their logs and use the corrected on-screen exit sequence.

Prepare one untimed native run with the adapter and cable attached. Opening Help
requests Terminal dhcp/ifconfig, exit and Command-W, then System/Network Utility
router ping/DNS and screenshot, followed by clean manual return. Preserve Ubuntu,
accepted current desktop archive and previous sound-confirmed recovery. Collect
the exact new session before any next trial; physical ECM support remains pending.

## First USB Ethernet acceptance — 2026-10-20

Release `debb2ddf3e9f4243` (payload source `1b2afb0`), session
`00065e4d6c789a85-00`, physically accepts DHCP and ping for the captured
`0b95:1790` revision `0200` adapter on this Pro. Native logs confirm ECM
configuration 3, the active usb-ecm provider, a DHCP lease and two ping processes
exiting status zero; the user confirms both DHCP and ping worked. No new
screenshot was recorded, and this run does not accept Network Utility GUI,
DNS, HTTP, throughput, cable cycling or multiple physical adapters.

Collection verified the matching logs with zero attention markers and a normal
desktop exit. Offline exFAT and allocation checks are clean (22 directories,
74 files). The one-shot entry is consumed; Ubuntu remains default. Installed
payload hashes and the previous sound-confirmed `e47dca3a6b30c6c1` recovery
match their archived manifests. Run, collection and checks are archived outside
build. No further boot is armed.

Next bounded scope: inventory another supplied adapter, or extend this adapter's
native DNS/HTTP/link coverage. Multiple-adapter inventory/UI and isolation remain
VM-tested; only one boot-selected provider owns IP configuration. Battery,
headphone testing and pointer latency remain deferred as previously agreed.

## Network maturity scope — 2026-10-20

Continue with the accepted adapter: persist DNS/HTTP results, exercise Ethernet
cable down/up with DHCP reacquisition and usable desktop diagnostics. Main
`76508bf` (PR7 filesystem fixes) is merged; Air's current published work remains
power/filesystem evidence with no network overlap. Scope: ECM link transitions
and queued traffic, HTTP client/local test fixture, USB network VM regression
and opening Help. Retain submitted DMA ownership and USB-removal quarantine;
USB hotplug registration and multihoming remain separate work. Rebuild and test
the merged filesystem baseline before another exact-stage native trial.

Saved-check review found shell `dhcp` and `dns` always returned success even
when their syscall failed. Extend this bounded scope to their exit statuses in
`userspace/sh/sh.c`, with VM failed/successful network checks, so saved reports
cannot label a failed lease as status zero.

Candidate `c7bf960edc860376` (source `193641e`) passes the exact v37 GPT
rehearsal: USB ECM DHCP/ping, scaled desktop, Text/screenshot/log persistence,
audio and clean unmount. Full build/test-core, added xHCI cable/DMA fixtures,
481 shell commands with a clean final filesystem, Help content/render fixtures
and 385 documentation links pass. The two-ECM VM passes three traffic outages
and recoveries, DHCP/DNS failure status, renewed leases, DNS, exact saved HTTP
response bytes, 160 UDP echoes/ZLP framing, inbound HTTP and keyboard/inactive
adapter isolation. Active-ECM desktop diagnostics pass saved Ping/DNS reports,
background typing/cancellation, bounded outage failure and restored ping.
Screenshots confirm readable adapter attribution, failure state and native Help.

QEMU's USB gadget keeps its CDC connection indication up during virtual cable
loss; the initial cable-notification rehearsal timed out and is preserved.
Actual down/up notifications are covered by host fixtures, not claimed as a
QEMU result. A prior serial-byte comparison failure (CR/LF translation) is also
retained; final validation compares the saved filesystem response bytes exactly.
Linux HTTP through the supplied adapter returns a response for the native target.

Next is an untimed native DNS/HTTP/cable trial. Opening Help contains the before
and after report commands, Ethernet-only unplug/reconnect checks, screenshots
and manual return instructions. Keep the USB adapter/workspace connected. This
new source has no physical acceptance yet. Preserve Ubuntu default, installed
previous `e47dca3a6b30c6c1`, and the independently archived accepted Ethernet
payload `debb2ddf3e9f4243`. Collect the matching session and saved reports before
any further trial; battery, headphones and pointer latency remain deferred.

## Native cable failure — 2026-10-20

Collected `c7bf960edc860376`, session `00065e4dd6afeb55-00`: the saved before
report confirms DHCP, DNS and HTTP 200 with successful statuses. Two screenshots
and the report verify by hash; no after report was found. The user reports a
slow disconnected indication and no reconnect indication. Logs show link state
unknown initially, no accepted CDC cable notifications, then a transmit-deadline
quarantine. This is a driver/link failure, not accepted cable recovery or merely
a repaint delay. Collection has one attention marker; normal desktop exit,
consumed one-shot, recovery hashes and offline filesystem/allocation checks pass
(23 directories, 83 files). Evidence is archived outside build; no boot armed.

Next bounded scope: capture this adapter's actual ECM notification format and
trace pending TX on cable loss before changing notification parsing or recovery.
Main remains `76508bf`; Air PR8 concerns battery/filesystem isolation. Preserve
DMA quarantine safety and exact device gates; do not hide failure with a longer
timeout or a UI-only connected indicator. Battery/headphones/latency stay deferred.

Linux ECM capture identifies the missing status: this exact adapter sends speed
and link-up notifications with wIndex 1 (data), not 0 (control). The previous
parser correctly rejected that unexpected interface. Configuration 2 and the
original Linux connection were restored after the bounded capture. Add the
notification-interface flag only in the exact captured descriptor compatibility
path. On a confirmed cable-down indication, retain the submitted TX/DMA and
suspend its deadline; on up, restore the normal two-second budget. Do not extend
unknown-link timeouts or recycle a live ring. Add capture-derived and ownership
fixtures before a native retry; raw interrupt-only capture stays local.

Cable-fix candidate `1212d33f6ccf6bd4` (source `f7bb195`) passes the full
build/test-core, capture-derived notification/ownership fixtures, two-adapter
ECM regression and exact v37 GPT desktop/audio rehearsal. Saved files/logs,
clean unmount and the readable opening Help were verified; documentation has
385 valid local links. Native cable status timing/recovery remains pending.
The next untimed retry saves fresh before/after reports and checks both UI
transitions with only the Ethernet cable unplugged. Retain Ubuntu default and
previous `e47dca3a6b30c6c1`; archive capture, payload and run state outside build.

Prompted automation scope: user requests automatic network checks/report saving,
with untimed unplug/Enter and reconnect/Enter gates. Touch desktop trial launch,
Terminal's explicit trial mode, bundled script, Help and VM fixtures. Main is
now `09207b0` (PR8 battery isolation), not yet consumed here; Air PR9 concerns
filesystem exclusion latency, not these network/terminal changes. Collected
`1212d33f6ccf6bd4`, session `00065e4e19aee836-00`, normal exit and no attention
markers: startup down/up notifications now parse; full cable recovery remains
unaccepted. No new trial starts before the automated flow is validated.

Prompted candidate `0875949083ad6ac7` (source `41375f1`) passes full build/core
checks, documentation checks and the two-adapter network regression. The actual
trial script saves exact local-server HTTP responses before/after a simulated
traffic outage, records disconnected DHCP failure, and rejects EOF at a gate.
The exact GPT/scaled desktop/audio rehearsal starts Terminal automatically,
waits at both gates until Enter, saves all phase reports and completion state,
then saves Text/screenshots/logs and unmounts cleanly. All three trial screens
were visually reviewed. Earlier EOF-fixture and argument-forwarding failures
remain in local evidence; final validation uses explicit positional targets.
Native cable timing/UI/recovery remains pending. Install with Ubuntu default
and `e47dca3a6b30c6c1` recovery retained; collect this candidate before another run.

Collected prompted `0875949083ad6ac7`, session `00065e4e72601882-00`: sequence
completed; after-reconnect DHCP, DNS and HTTP 200 all passed. Two physical
cable down/up cycles were logged without quarantine. System snapshots include
up and down states; visual update timing still awaits user confirmation. The
initial report failed because it ran before startup link negotiation completed
(link-up at about 4.44 seconds). Next bounded fix is only script startup
readiness: wait for the active provider's known link-up, with a finite budget,
before initial checks. Keep the two human cable gates untimed. Evidence and
reports are archived; offline filesystem/allocation and recovery checks pass.

The follow-up script now checks the active provider (ignoring inactive links)
for up to 30 one-second samples, saves the last startup snapshot, and reports
expiry explicitly. Host fixtures cover immediate readiness, delayed readiness
and budget exhaustion. It keeps both human cable gates untimed. This script
change is not installed or physically accepted; current installed payload
remains `0875949083ad6ac7` and Ubuntu remains the default.
The follow-up image build and two-adapter VM regression also pass, including
the actual startup gate and prompted sequence, saved HTTP response bytes,
disconnected DHCP failure, EOF refusal and the existing transport coverage.

User confirms reconnect UI remained stale in `0875949083ad6ac7`; other checks
worked. Do not accept UI recovery. Final native link-up was at 151.379 seconds,
with System close requested at about 151.98 seconds, so current logs cannot
separate late hardware detection from the one-second repaint interval. Next
bounded scope: System network transition timestamps and automatic (no Refresh)
VM down/up coverage in `system.c` / `desktop_network_smoke.py`. Main remains
`09207b0`; Air PR9/filesystem write-batching scope does not overlap. Preserve
successful native data-path evidence separately from unresolved UI timing.
The user estimates roughly a minute waiting and reports connectivity returned
quickly. Treat this as an unresolved live-update failure, not a confirmed
close-before-refresh case. Extend diagnostics to xHCI notification-completion
and processing timestamps; wake the network worker for status completions as
well as received packets. Log System state changes and a ten-second sampling
heartbeat. Test automatic up/down sampling, repaint dirtiness and no-F5 VM
recovery before another physical attempt. No native fix is claimed yet.

Full build/test-core pass with automatic System sampling/repaint assertions and
notification wakeup/timestamp coverage. The mixed e1000/ECM desktop VM now
shows automatic disconnect and reconnect without F5; the recovered System
screenshot visibly shows Connected / 1000 Mb/s. This validates the shared UI
path with e1000, not the native ECM notification timing. Native diagnostics
remain necessary; source changes are not installed and no reboot is armed.

Proceeding with the bounded UI diagnostic trial. Remote main is now `39b152d`
(PR9 filesystem exclusion and PR10 FAT batching landed); this trial retains
its already-tested `76508bf` integration base to isolate the network/UI changes.
No open overlapping device PR remains. New on-screen completion instructions
ask for a live System Network cable cycle, fifteen seconds in each state and
a screenshot if reconnect remains stale. Pending main integration is separate
from this native comparison; battery remains deferred.

Diagnostic candidate `81e3308ffcdfe637` (source `5d9ca16`) passes the exact
v37 GPT/scaled desktop/audio rehearsal: initial readiness, both prompted phases,
saved reports/sequence, Text/screenshot/log persistence and clean unmount.
The final live-UI instructions are visibly readable. Install retains Ubuntu
default and recovery `e47dca3a6b30c6c1`; native acceptance remains pending.
Collect startup/phase reports and correlate notification completion/processing
with System transition/heartbeat timestamps before changing the verdict.

Collected `81e3308ffcdfe637`, session `00065e4f10dd7857-00`: initial and
recovered DHCP/DNS/HTTP200 pass; startup readiness fix works on this unit.
User reports neither unplug nor reconnect visibly updated. System sampled the
final down at 40.629 s (driver 40.539 s) and up at 82.788 s (driver 82.518 s),
with continuing heartbeats and no quarantine. Thus sampled data is timely;
visible repaint/which displayed field remains unresolved. Next bounded scope
is a foreground idle-window VM transition test with no click/F5 after either
link change, since previous recovery screenshots focused System first.
The foreground, no-input down/up VM cycle passes; the recovered screenshot
shows Connected / 1000 Mb/s without focusing, clicking or Refresh. This is
shared-UI/e1000 evidence, not acceptance of the native report. Await the user's
identification of the unchanged field and whether Refresh/window movement helps.
No native screenshot was saved in this run. No new payload installed or reboot
armed; preserve the current diagnostic image for a focused follow-up.


Same-payload retest accepted (2026-10-20 local / October 21 UTC): release
`81e3308ffcdfe637`, source `5d9ca16`, session `00065e4fa158e3b8-00`.
User confirms everything worked and the UI updated. Fresh initial and recovered
reports pass DHCP, DNS and HTTP 200; cable-down DHCP fails as expected. Both
prompted gates and final sequence completed, with normal desktop exit and no
quarantine. System sampled the final down at 65.119 s (driver 64.599 s) and up
at 74.119 s (driver 73.299 s). This accepts visible disconnect/reconnect for
this captured adapter on this unit. Router/DNS lease values may stay unchanged
while the Link row and status message change. Earlier user-reported failures
remain historical evidence; this acceptance is from the explicit retest.

All five reports and matching session are hashed and archived in the dedicated
`retest-20261021T012530Z` directory beneath the durable release archive, without
overwriting the prior run. Offline exFAT checks pass: 27 directories, 104 files,
406 clusters, zero leaks, mismatches or errors. Ubuntu remains default, the
one-shot entry is consumed, and current/recovery payload hashes verify against
their archives. No reboot is armed. The bounded networking/UI task is complete;
next scope is separate integration review against newer main. Multiple physical
adapters remain untested; one boot-selected IP provider remains the limit.
Battery work, headphone testing and further trackpad latency work stay deferred.

Integration scope: user authorizes local validation and merging PR #5 while
Actions are disabled. Consume actual main `9d9c72e` (battery filesystem isolation,
operation scheduling and FAT batching), preserving Pro desktop/network profiles
and both machines' evidence. Resolve shared staging/kernel/tool conflicts by
behavior, run the CI baseline locally plus affected networking regressions,
then merge the reviewed PR. Installed native payload stays `81e3308ffcdfe637`;
the combined source tree requires its own later physical acceptance.


Integration validation completed locally on `8985b81` against main `b090d94`
(PRs #8–#12 included). The two staging-tool conflicts preserve both Pro
keyboard/CPU-load/scaled desktop/network profiles and Air v38 filesystem
capture, with explicit kernel selection and automatic-return policy retained.
All 66 sequential validation steps pass using Ubuntu QEMU 10.2.1: the complete
Actions baseline (host/docs/ACPI audit, BIOS/UEFI builds, 256 restart rounds,
481 shell commands, input/display and ACPI/battery fault scenarios), two-ECM
transport regression, mixed and active-USB desktop networking, CPU-load refusal,
and exact GPT/scaled/audio desktop plus v38 refusal rehearsals. Three additional
isolated-filesystem workload/automatic-deadline/active-deadline cases pass,
including verification that every outside byte is unchanged. The no-input
reconnect screenshot visibly shows the active adapter Connected / 1000 Mb/s.

Exact desktop release `d3e5117729c46e35` and filesystem diagnostic release
`49a0b0eef761b38b` save evidence and unmount cleanly in VM. These are not installed
or physically accepted. An initial run with the optional older bundled QEMU
stalled at Limine before kernel entry; its failed log is retained. System-QEMU
validation passes without changing test predicates. An intermediate test run
was intentionally stopped when PR #12 landed, then restarted on the final tree.
Actions remain disabled; this result is local evidence, not a GitHub CI pass.
The user authorizes merging PR #5 after these checks. Durable integration logs
and receipts are archived outside `build`. Installed desktop/recovery remain
`81e3308ffcdfe637` / `e47dca3a6b30c6c1`; no reboot or installation is armed.
Battery, headphone and further trackpad-latency acceptance remain deferred.

Post-merge physical regression scope: user requests pull/update and the next
native test. Main is now `cd732ec` (PR #13 bounded USB data batching). Fast-forward
this device branch, rebuild and run core/storage hardening, USB transport and
exact GPT/scaled/audio/network desktop checks before installation. Shared source
is unchanged by this device step. Use the existing untimed Terminal cable trial
and persistent Help checklist for input/apps, brightness, speakers, both fans,
Pause/Resume and Text save/reopen. Preserve current accepted network payload and
previous recovery independently; retain Ubuntu default. Battery/headphones and
further pointer-latency tuning remain deferred. Collect this exact new session
before another stage; local/VM passes do not accept the updated native tree.

Updated native candidate `2f8abe0e131023d8` (source `fd3ffd2`, main `cd732ec`)
passes local build/core and storage hardening: all 169 write/power-cut/partial
boundaries, 72 flush failures and 19 pause/revoke points. USB read/write/flush,
reload, keyboard and timeout-quarantine VMs pass, as does mixed-adapter live UI.
The exact GPT desktop rehearsal passes 2880x1800 at 2x, audio, prompted network
checks, Text/screenshot/log persistence and clean USB unmount. The physical
workspace contains `regression-checklist.txt` with all checks and return steps.
Install with previous recovery retained, save the on-disk run record, and select
one boot. Native acceptance is pending; collect this release first after return.

Collected post-merge release `2f8abe0e131023d8`, source `fd3ffd2` / main
`cd732ec`, session `00065e5123d266b4-00`. User reports everything appeared to
work as expected. Fresh before/after reports pass DHCP/DNS/HTTP 200; disconnected
DHCP fails as expected. Two cable cycles complete without quarantine, with the
final System down/up snapshots within 0.71 seconds of driver notifications.
Normal desktop exit and zero collector attention; both fans provide recent
readings with zero service failures. The logs record no audio playback and the
requested `postmerge-text-check.txt` is absent. The user subsequently confirms
only network/UI checks were performed. Brightness, sound, Text save/reopen and
the remaining desktop checklist were not retested on this payload; prior native
acceptance remains historical. Fan samples establish telemetry availability,
not a user-confirmed fan-display or Pause/Resume retest.

Matching logs and all five reports are hashed and archived with this release
outside build. Offline exFAT/allocation checks pass: 28 directories, 109 files,
412 clusters, zero leaks/mismatches/errors. Current and recovery manifests verify;
Ubuntu remains default, one-shot consumed, no reboot armed. Network/UI recovery
and the exercised persistence path are accepted on the updated tree. Battery,
headphones and further trackpad-latency work remain deferred.

USB Wi-Fi groundwork: captured Edimax `7392:a711` revision `0100`, high-speed,
one ff/02/ff interface with two bulk IN and six bulk OUT endpoints. A read-only
inventory utility and sanitized descriptor fixture validate this shape and
reject truncations/identity/topology mutations. Linux binds `mt76x0u`. No reset,
unbind, scan, firmware upload or SrvrOS reboot was performed. See
[USB Wi-Fi scope](../usb-wifi.md) for staged driver/authentication/UI work.
This phase touches inventory/tests/docs only; Air SCI work remains separate.

Next bounded implementation on main base `6c6bf9e`, draft PR #19: add a pure
MT76 identity-request/response contract and firmware layout validator, with
host fault tests. Touch only the new protocol module, tests, Make target and
Wi-Fi docs. Existing xHCI control completion lacks exact received lengths;
native wiring is deferred until short replies and timeout DMA ownership are
handled explicitly. No radio writes, firmware upload, IP provider or reboot.

Protocol checkpoint: identity request generation and exact-length response
validation plus firmware structural parsing are implemented and host-tested.
`make test-mt76-probe`, ASan/UBSan contract tests, `make -j2 all test-xhci
test-usb-ecm` and `make check-docs` pass. The module has no native caller; kernel
build success is not hardware acceptance. No firmware is packaged and no Wi-Fi
connectivity is claimed. Next: exact-byte EP0 completion and timeout ownership
fixtures before wiring the opt-in native diagnostic. Installed payload/recovery
remain unchanged, Linux still owns the adapter, and no reboot is armed.

Next bounded checkpoint: consumed main `7761200` (Air SCI evidence only).
Implement an opt-in `srvros.usb-wifi=probe` identity diagnostic for the captured
Edimax profile. Shared scope is an isolated strict EP0 IN completion path in
xHCI, new probe wiring in main, and host transport fixtures. Preserve existing
control callers. Track short data events through status completion, reject malformed residue,
and retain/quarantine the slot's EP0 ring and buffer on uncertain completion.
No firmware upload, transmitter, networking provider or physical boot is armed.

Opt-in identity implementation checkpoint: `srvros.usb-wifi=probe` now gates on
captured Edimax identity/revision/high-speed/EP0/configuration shape and performs
only ASIC/MAC vendor reads. Strict EP0 completion retains short-data length until
successful status; errors/timeouts quarantine the endpoint with DMA retained.
The diagnostic records raw identity or failure, never an active network provider.
Native silicon values and physical acceptance remain pending. The next step is
an immutable automatic-capture profile plus exact-stage VM rehearsal before any
installation. Current/recovery payloads remain unchanged; no reboot is armed.

Validation: full `make -j2 all usb-image test-core` passes; final guard/refusal
additions pass `make -j2 all test-xhci`. Host fixtures cover lengths zero through
four, malformed residue, setup/data/status errors, missing status, stale events,
duplicate short events, ring wrap, retained DMA and profile/opt-in refusals.
`python3 tools/usb_ecm_smoke.py --second-usb --wifi-probe` passes DHCP/DNS/HTTP,
three outage/recovery cycles, 160 UDP echoes, ring wraps, keyboard and disconnect
quarantine. `python3 tools/usb_storage_smoke.py` passes read/write/flush/reload and
keyboard; docs checks pass. An earlier local VM wrapper accidentally emitted two
command lines and failed to enable Ethernet; retained as harness failure evidence,
then corrected to the explicit `--wifi-probe` option with a single command line.

Authorized next run: prepare new immutable v39 Wi-Fi identity auto-capture,
using main base `7761200`. Scope: staging/profile tests, the capture branch and
exact-stage VM refusal/persistence checks. Air PR #21 is separate SCI observation;
its changes are not consumed. Capture reports failure if no matching radio or
identity read fails, saves diagnostics, and returns on handled failure. No manual
commands, radio writes or authentication. Preserve Ubuntu and accepted recovery;
verify actual USB UUID and both filesystem preflights before one-shot reboot.

Wi-Fi capture release `5d65ddcc4ccbeaf2`, source `b1aa1ca`, is installed for
the user-authorized automatic boot. New immutable v39 enables identity reads and
capture/return only. All 43 installer/profile tests and build/xHCI tests pass.
Exact-stage GPT rehearsal saved unavailable-radio/`completed=NO` evidence,
verified offline log bytes and clean exFAT, and reset automatically. The separate
missing-workspace rehearsal also returned. These are expected VM refusal paths,
not radio acceptance. Native results remain pending collection after this boot.

Preboot offline fsck and allocation checks pass. Current capture and accepted
previous recovery are verified; Ubuntu remains default. One obsolete test payload
was removed from ESP only after every file matched its durable archive. Payload,
VM receipts, prior-session list, authorization and run/checkpoint records are
archived outside build. The capture requires no keyboard input. After return,
collect the new matching session before further work, inspect raw identity and
quarantine/result markers, verify the consumed one-shot, and repeat offline
filesystem checks. Battery and radio initialization/authentication remain off.


Collected native Wi-Fi identity session `00065e52c454a4dd-00` from release
`5d65ddcc4ccbeaf2` / source `b1aa1ca`. ASIC_VERSION is `0x76100002`; MAC_CSR0 is
`0x76502000`. Both reads succeed with `quarantined=0`, and the matching saved exit
snapshot records identity read and capture completed=yes. Collector attention=0.
The two register reads are physically accepted for this adapter; no firmware,
radio, authentication or connectivity acceptance is implied. Updated host probe
fixture values reproduce the observed identity pair with distinct endian checks.

Boot/exit bytes and collection hashes are archived outside build. Ubuntu default,
consumed one-shot and current/previous payload hashes verify. Postboot fsck and
allocation checks pass: 29 directories, 111 files, 415 clusters, no leaks/mismatches
or errors. Capture is collected; no reboot is armed. Next: consume current main
before firmware work, preserve immutable v39, then establish firmware provenance,
hash and revision/calibration gates. Fetched main `8211ec1` includes Air v40/v41
capture/collection and shared process/UI changes; it is not present in this
physically tested payload. Preserve both Air recipes when resolving shared files.

Next scope: integrate main `8211ec1`, preserving Pro Wi-Fi v39 and Air v40/v41
exact recipes and refusal tests. Resolve shared staging/receipt/Make targets by
combining behaviors, then build/core and exact v39 VM checks. Independently audit
installed mt7610 firmware structure/hash/provenance and document calibration
requirements; no firmware upload or radio writes in this checkpoint.

Integration/audit checkpoint: merge `f1421f4` consumes main `8211ec1` and preserves
all laptop recipes byte-for-byte against both parents, including Pro v39 and Air
v40/v41. All 45 profile tests, full build/USB image/core suite and exact v39 GPT
VM rehearsal pass. VM-only stage `de4ec051e0ee2f06` records dirty=true because the
untracked host firmware-audit tools were being authored; it is not installed or
promoted as a clean native candidate. Saved refusal evidence and reset pass;
installed native identity release remains `5d65ddcc4ccbeaf2`.

Both local decompressed mt7610e/u images match upstream commit `431e7f03` hashes
and pass the actual kernel C layout parser and new host audit. The audit rejects
unknown hashes/names and malformed sizes; four tests plus docs checks pass.
Upstream WHENCE assigns LICENCE.mediatek. No firmware bytes are committed or
packaged. See the Wi-Fi guide for hashes, lengths, source links and explicit
calibration requirements. Next is a separately bounded EEPROM/EFUSE capture and
MCU/reset/firmware ownership implementation; accepted identities do not authorize
transmit without this adapter's validated calibration. No reboot is armed.

Next bounded implementation at base `8211ec1`: a transport-independent EFUSE
capture engine restricted to logical-read mode on the captured ASIC/MAC pair,
plus firmware segment planning. Host fixtures must cover failed operations,
busy/timeout/cancellation, erased blocks and clearing partial output. Do not
wire control-register writes into ordinary enumeration; native capture needs
separate opt-in transport, staging and exact-image rehearsal. Firmware planning
is not upload permission or radio initialization. Air's active SCI/filesystem
work is separate; main has not advanced since the previous integration.

EFUSE/planning checkpoint: added the transport-independent logical-read engine
for the accepted identity pair and a pure ILM/DLM/IVB firmware chunk planner.
Capture has a shared poll budget, read-mode-only control writes, unmapped-block
tracking and all-or-nothing private output. Host fixtures cover every operation
failure, cancellation, busy/empty/unmapped data and cross-block timeout budgets.
Firmware tests cover both pinned candidates' ILM lengths, alignment tails,
address overlap, invalid lengths and end-of-plan refusal. Build, xHCI/probe host
regressions, new engine tests and ASan/UBSan pass. Two early synthetic fixture
bugs (byte carry and stale AOUT state) were corrected; no hardware was involved.
There is no native caller, register-write transport or active upload yet.
Next is strict OUT transport/ownership and an opt-in calibration capture profile;
retain v39 unchanged. Installed release/recovery and reboot state are unchanged.

Next native calibration scope: add strict four-byte OUT completion and a guarded
EFUSE-only register transport. Run capture only after workspace logging starts,
never in ordinary enumeration. Use new v43 automatic profile; Air's active
branch already reserves v42 for battery load. Preserve v39 and all Air recipes.
Host tests must cover write rejection, short/error/timeout quarantine and no
retry. Log only MAC-redacted calibration bytes. No firmware upload or transmitter.

Calibration transport checkpoint: `4f6bf1b` adds strict OUT completion, guarded
logical EFUSE transport and opt-in v43. Shared event dispatch retains strict EP0
completion for its owner when HID/storage workers consume the event. Short/error
or uncertain completion quarantines the endpoint and retains DMA. Capture starts
only after workspace logging; successful logs redact the MAC, and failures emit
no partial calibration bytes. No firmware upload or transmitter is enabled.

Merge `ac6b9a4` consumes main `421a69b`, preserving Air v42 and Pro v39/v43;
all existing laptop recipes match both parents byte-for-byte. Full build/USB/core,
47 profile tests and docs checks pass. Clean stage `47997cb8604043ff` passes exact
GPT VM refusal/persistence/unmount/reset and missing-workspace automatic return.
Native calibration remains pending; the earlier identity acceptance is unchanged.

USB Ethernet VM regressions pass (DHCP/DNS/HTTP, three outage/recovery cycles,
160 echoes and keyboard), as do USB storage read/write/flush/reload regressions.
The clean v43 candidate is installed for the automated native trial. Ubuntu
remains default and the accepted previous recovery payload is retained. An older
network payload was removed from the EFI partition only after verifying every
file against its durable archive; accepted identity payload remains installed.
Preboot workspace fsck/allocation checks pass: 111 files, 415 clusters, zero leaks,
mismatches or errors. Local run record and archived payload/evidence are saved;
next is one-shot calibration capture and matching-session collection on return.


Native calibration collection: release `47997cb8604043ff`, source `ac6b9a4`,
session `00065e5e35f79801-00` saved all 32 logical EFUSE blocks (512 bytes),
result=0, quarantined=0, unmapped mask `cf899c42`. All unmapped blocks contain
FF bytes; MAC bytes 4–9 are zeroed. The matching exit log records both capture
and overall completed=yes; collector attention=0. Raw evidence and a reconstructed
MAC-redacted image are archived outside build. This accepts logical readout only;
board/version/regulatory calibration validation, firmware and connectivity remain
pending. Postboot fsck/allocation pass: 30 directories, 113 files, 418 clusters,
zero leaks/mismatches/errors. Ubuntu default, consumed one-shot and installed/
recovery manifests verify; no reboot is armed. Next: validate the captured layout
and calibration requirements before bounded firmware upload/readiness work.

Next scope at main `21a9fdf` (documentation-only update, merged locally): audit
captured EEPROM structure and mapped fields without claiming regulatory/radio
acceptance; add pure firmware packet encoding and a bounded, one-shot upload
engine behind an explicitly prepared/authorized transport contract. Touch only
mt76 bring-up helpers, host tests/audit, Makefile and Wi-Fi/unit docs. No native
reset/upload wiring, firmware packaging, boot recipe changes or Air subsystem
edits in this checkpoint. Preserve v43 physical read acceptance independently.

Audit/prepared-upload checkpoint: actual captured calibration passes the new
offline structural auditor (7610, version2/FAE1, one TX/RX chain, 15 consistently
erased unmapped blocks). Region bytes 10/30 remain raw metadata; no channels or
transmit permission inferred. Physical usage-map and power/gain/regulatory checks
remain open. The new packet encoder and one-shot upload engine are host-tested
only, requiring externally verified firmware and an exclusively prepared DMA/MCU
transport. No native reset/upload caller or packaged firmware was added. Next is
hardware preparation/ownership and pinned firmware packaging before a separate
native MCU-readiness diagnostic. Installed v43 and recovery remain unchanged;
no reboot is armed.

Next bounded scope at unchanged main `21a9fdf`: strict zero-data EP0 completion
for MCU reset/FCE setup, one-shot MCU/FCE preparation with fault injection, and
an explicit pinned firmware/license package for future opt-in integration. Touch
xHCI strict transport, mt76 helpers/tests, host package tool and unit/Wi-Fi docs.
Ordinary enumeration and immutable boot profiles remain unchanged until complete
native ownership and exact-stage rehearsal are available. No new physical run.

Preparation checkpoint: strict zero-data xHCI control completion and one-shot
MCU/FCE preparation are implemented with host fault tests. Inspection found that
upstream's MAC control 0x2c enables TX/RX; the proposed readiness preparation uses
0x20 with readback checks instead. Native compatibility remains unknown and no
hardware writes were issued. A hash-pinned mt7610e/license package was produced
locally; it is not in the boot image. Next: native endpoint/DMA ownership and
kernel-side verified package consumption, then an immutable readiness profile
and exact-image rehearsal. Installed calibration payload/recovery unchanged;
no reboot armed. No firmware or radio acceptance is claimed.

Integration at main `84cf41e`: Air PR #33 independently used v43 for battery app
files, colliding with Pro's published/native calibration v43. Preserve verification
of both exact historical recipes (config copies must agree); future calibration
stages use v44, while main's battery-app-files stays v43. Historical bytes from
both merge parents and installed-stage v43 verify. No host UUID is used to guess
a recipe. New scope: kernel-side pinned firmware hash verification and dedicated
native firmware EP08 DMA/control ownership; no shared battery implementation.

Native firmware implementation now connects pinned SHA-256 verification,
dedicated EP08 DMA, strict configure completion and guarded vendor callbacks to
the prepared/upload engines. It runs only in a new descriptive wifi-fw-v1 capture
with the explicitly staged firmware/license package. Runtime bulk waits share
HID/storage dispatch and quarantine uncertain completion. MAC control stays 0x20
in preparation and must read back 0x20 after MCU readiness; live-MCU preparation
is refused. Native firmware acceptance remains pending exact-stage rehearsal
and physical capture. No RF setup or network provider is enabled.

Firmware readiness candidate: clean source `fdd8d02`, release `6ec5ab2ce67afaee`,
profile `ram-usb-wifi-fw-v1`. Full build/USB/core suite and 50 boot-profile tests
pass, including both historical v43 recipes and mismatched-copy refusal. Native
transport fixtures cover EP08 context/aligned retained DMA, exact command pointer,
control write/reset gates, FCE halfwords, bulk residue/Event-Data/timeout quarantine.
Kernel SHA-256 vectors, actual pinned firmware mutations and actual staged
initramfs lookup/hash checks pass, including ASan/UBSan. Exact-stage GPT VM saves
the expected absent-adapter refusal, unmounts and resets; missing-workspace return
also passes. Native firmware startup remains untested. Package and test evidence
are archived outside build; existing physical v43 calibration acceptance remains
historical, not firmware acceptance.

Firmware candidate installation: Ethernet recovery and USB storage read/write/flush/
reload VMs pass. Release `6ec5ab2ce67afaee` is installed for an unattended native
readiness capture; Ubuntu remains default and recovery `e47dca3a6b30c6c1` is retained.
Preboot offline filesystem/allocation checks pass. Prior identity payload was
removed from EFI only after verifying its durable archive; accepted calibration
payload remains retained. Run record, exact VM receipt, payload and logs are saved
outside build. Next is the authorized one-shot capture, then matching-session
collection before any further staging. No physical firmware acceptance yet.

Native firmware trial `6ec5ab2ce67afaee`, session `00065e5f8a2199cf-00`,
returned with preparation refused (result=1), zero chunks, mac_disabled=0 and
quarantined=1; overall completed=NO. This is a failed readiness trial, not
firmware acceptance. Logs archived; Ubuntu default and consumed one-shot verified.
Offline checks pass: 115 files, 421 clusters, zero leaks/mismatches/errors.
Next bounded scope: cache and report the last prerequisite register read and
preparation/reset/upload progress in the explicit diagnostic, without additional
register operations or relaxing refusal gates. Main remains `84cf41e`; Air PR #34
owns separate desktop battery controls. No battery changes are needed here.

Cached progress diagnostic source `11da4e0`, release `56b0d84701bc99ff`, passes
build, xHCI transport, MCU preparation/upload and documentation checks. Exact GPT
VM saves diagnostics, verifies offline bytes and unmounts/resets. Candidate is
installed with Ubuntu default and previous recovery retained; preboot filesystem/
allocation checks pass. Calibration payload was removed from EFI only after
verifying its durable archive. Next authorized boot needs no input and will
record the refusal prerequisite; physical firmware startup remains unaccepted.

Native cached-progress trial `56b0d84701bc99ff`, session `00065e5fc1be293b-00`,
identifies a live MCU: first/only read 0x730 returns 1, prepared=0, reset=0,
upload=0. The explicit live-MCU refusal fires before firmware preparation writes.
Completed=NO remains a failed readiness trial; no upload accepted. Logs archived,
Ubuntu/consumed one-shot/recovery verified, offline checks pass (117 files, 424
clusters, zero leaks/mismatches/errors). Ubuntu currently uses this adapter for
its default network route, so unloading it would interrupt the agent connection.
Next: reuse the exact tested image after an attended cold adapter power-cycle
while Ubuntu is shut down. The local cold-run record/shutdown helper are prepared
but unarmed; wait for user readiness, arm one-shot and shut down, then have the
user unplug/reconnect only Wi-Fi while off and power on. No Linux driver blacklist
or firmware takeover workaround is installed.

The subsequent attended cold-start attempt returned to Ubuntu with the one-shot
entry consumed but no new USB session (latest remains `00065e5fc1be293b-00`,
the prior warm refusal). This is incomplete evidence, not upload acceptance.
Collection is archived separately; installed/recovery manifests and Ubuntu default
verify. Offline checks remain clean: 117 files, 424 clusters, zero allocation
errors. No next boot is armed. Await the user's description of the boot screen
and return path before deciding whether startup or workspace logging failed.

Attended cold retry saved session `00065e606b547868-00` for `56b0d84701bc99ff`.
The user saw SrvrOS console briefly and automatic Ubuntu return. Root port 1/slot 1
timed out on EP0 at device-descriptor-prefix, before VID/PID or Wi-Fi identity
detection; no firmware upload was attempted. Unlike the prior missing-session
attempt, this run saved a matching exit log with completed=NO. Workspace storage
initialized on SuperSpeed port 17 and remained healthy. Evidence archived;
Ubuntu/recovery/consumed one-shot verified. Offline checks pass: 119 files, 427
clusters, zero leaks/mismatches/errors. Next scope is cold-start USB enumeration
and EP0 timing/completion diagnosis; do not relax firmware ownership gates or
retry uncertain DMA blindly. No next boot is armed.

Cold USB scope: diagnose post-address descriptor timing in xHCI and add host
regressions without firmware gate changes. Fetched main `49ded19` (Air battery
controls); integration overlaps staging and reuses v44, so it is deferred while
isolating this USB change on the prior `84cf41e` baseline. The attempted merge
was aborted without changing either branch. Reconcile immutable profiles before
consuming that baseline; no Air EC or battery changes are part of this fix.

Address-recovery candidate `296927c`, release `205e2cc2fb7e616c`, adds the
10 ms controller-clock settling interval after Address Device, before descriptors.
Build/xHCI/docs pass; the new test covers clock wrap, stall quarantine, failed
address command and retained DMA. Exact GPT VM capture/save/unmount/reset passes,
as do USB Ethernet DHCP/DNS/HTTP/recovery and storage read/write/flush/reload VMs.
This is a plausible cold-start fix, not native acceptance. Main `49ded19` remains
unmerged pending resolution of the v44 staging collision; prior baseline retained.

Native address-recovery trial `205e2cc2fb7e616c`: the user observed brief console
output and automatic Ubuntu return, but collection found no new session (latest
remains `00065e606b547868-00` from the prior release). This is incomplete evidence;
the timing fix is not physically accepted and the failure location is unknown.
Installed/recovery manifests, Ubuntu default and consumed one-shot verify. Offline
filesystem/allocation checks pass: 119 files, 427 clusters, no errors. Evidence
and checkpoint archived. Next: prepare a visible failure-screen diagnostic so
missing-workspace/early failures can be photographed; no further boot is armed.

Next bounded scope: allow the existing firmware profile without auto-return via
explicit hold-on-failure, preserving all prior recipes. Add a visible prerequisite
summary and power-button recovery instructions after failed capture/save/unmount.
Extend exact-image VM checks to require a live halted guest both with and without
the workspace. No USB hardware or firmware gates change in this diagnostic.

Held diagnostic source `835f2e9`, release `d18b0b2574ed8501`, passes build,
50 profile tests and docs. Exact GPT VM saves/unmounts and remains halted on
absent-radio failure; missing-workspace VM also remains halted with logging=0
and manual recovery instructions. Installed with Ubuntu default, known-good
previous recovery and preboot filesystem/allocation checks verified. Prior
`56b0d84701bc99ff` was removed from EFI only after durable archive verification.
Run record/evidence are archived; no reboot armed. Await attended cold-start
readiness; on failure photograph the screen before manual power-button recovery.

Held native trial `d18b0b2574ed8501`, session `00065e637e1d2a93-00`,
successfully enumerated the adapter, read expected ASIC/MAC identity and configured
the firmware endpoint. Preparation made four reads, last 0x238=0x00c00000,
then failed with result=2 (I/O), reset_seen=1, prepared=0, upload=0, chunks=0,
quarantined=1. reset_seen means attempted, not completed: the reset request or
subsequent setup writes still need exact operation diagnostics. No firmware
upload accepted. The photographed held screen shows saved/unmounted success and
manual recovery instructions. Photo/logs archived. Offline checks pass (121 files,
430 clusters, zero errors); Ubuntu/recovery/consumed one-shot verified. This run
passes cold enumeration but does not establish repeatability or causation for the
10 ms delay. Next: instrument preparation control request/completion failures;
no further boot armed.

Next scope: cache the exact firmware control setup packet, actual result, matched
completion code/residue/stage and timeout, reporting after the attempt. Preserve
all transfer guards, sequencing and timeout behavior; add fault regressions before
staging another held diagnostic. Shared integration remains deferred as recorded.

Control-detail candidate `ab82b8f`, release `dfd7ab3b39432e73`, passes build
and xHCI regressions including reset setup/status errors, residue, timeout, stale
completion followed by success, no retry and retained DMA. Exact GPT VM saves
logs/unmounts and stays halted; missing-workspace also stays halted. Control
sequence, deadlines and refusal rules are unchanged. Native result pending.

Native control-detail release `dfd7ab3b39432e73`, session
`00065e639cbb404d-00`, locates the failure at control request 9: type=0x40,
request=6, value=0, index=0x800, length=4 (FCE enable write). actual=-1,
cc=0, stage=0, timeout=1: no matched completion. The prior reset completed and
its delay returned successfully, since the sequence reached this subsequent
write. Firmware upload remains unattempted (zero chunks); quarantine retained.
Photo/logs archived; Ubuntu/recovery/consumed one-shot verified. Offline checks
pass: 123 files, 433 clusters, zero errors. Next investigate reset recovery timing
and EP0 state before the first FCE write, without blind retries or weakening
completion checks. No next boot armed.

Next scope: firmware-only EP0 waits use a measured 300 ms deadline (the pinned
Linux mt76 vendor timeout), retaining a bounded stalled-clock fallback and no
retry. Existing generic control waits and reset delay remain unchanged. Cache
request elapsed time and controller-owned EP0 state/dequeue after the wait to
diagnose post-reset timeout; no endpoint recovery or new device writes.

Deadline candidate `49375c8`, release `20ffa06db0f23758`, passes build/xHCI
regressions, including delayed 250 ms completion, 300 ms timeout, frozen clock,
endpoint snapshot and no mutation/retry after uncertainty. Exact GPT saved-log
and missing-workspace held VMs pass. This measures the timeout and endpoint
state; it does not yet establish why the device stops responding after reset.
Native validation pending; recovery and the held-failure workflow are retained.

Native measured-deadline trial `20ffa06db0f23758`, session
`00065e63bd3755b9-00`, still times out on request9/write6/index0x800/length4
with no matched completion. elapsed_us=300001, endpoint state=1, cached output
dequeue=0x1eb001. Do not infer live ring progress solely from that context snapshot.
Reset completed, preparation failed and upload remained unattempted; no firmware
acceptance. Photo/logs archived, recovery/default/consumed one-shot verified.
Offline checks pass: 125 files, 436 clusters, no errors. Next investigate device
reset behavior and transport state rather than simply increasing the timeout.
No further boot armed.

Next bounded scope: add the missing MT7610 WLAN/clock enable prerequisite before
MCU reset, based on Linux v6.18 mt76x0_chip_onoff/set_wlan_state. Preserve GPIO
and antenna bits, refuse asserted reset/unreadable state, verify enable readback
and bounded crystal/PLL readiness. Keep MAC TX/RX disabled and existing quarantine.
Files: mt76 preparation engine, native xHCI allowlist/diagnostics, host fixtures.
Fetched main 065546f (Air battery audit); no open competing PR besides #19.
Integration remains deferred for the recorded immutable-profile collision.
Native evidence is required; no further boot armed.

Clock prerequisite candidate `7f06de6`, release `05573bcc8bb336a4`, passes build,
xHCI/preparation/upload/firmware host tests and docs. Fixtures cover preserved
GPIO bits, reset/unreadable/readback refusal, readiness timeout/cancellation and
every-operation failure without retry. Exact GPT VM saves/unmounts and holds;
missing-workspace VM holds with recovery instructions. Installed unarmed, Ubuntu
default and accepted recovery retained; offline workspace checks clean. Native
WLAN/clock and firmware readiness remain pending. Next: attended cold shutdown,
then collect this release before further changes.

Native clock trial `05573bcc8bb336a4`, session `00065e643293f201-00`, confirms
WLAN enable/readback (`0xff000002` → `0xff000003`) and crystal/PLL readiness
(CMB `0x00c017ff`). It still fails request13/write6/index0x800/length4, the
first FCE write after completed MCU reset: no matched completion in 300003 µs,
endpoint state=1, cached dequeue=0x1eb001. Preparation/upload remain incomplete,
zero chunks and quarantine retained. Clock initialization is observed working;
it did not resolve the post-reset failure. Photo/logs archived; Ubuntu default,
consumed one-shot and recovery verified. Offline checks: 127 files, 439 clusters,
zero leaks/mismatches/errors. Next investigate post-reset EP0/device behavior;
no next boot armed and no firmware-readiness acceptance.

Next scope: post-timeout Wi-Fi EP0 stop-and-snapshot diagnostic in xHCI, with
exact command completion, cached submitted TRB addresses and stopped transfer
event capture. Keep the endpoint quarantined, retain DMA, and never restart or
retry the request. Host fault fixtures and exact-stage held VMs precede native
test. Fetched main 961369b; only open PR remains #19. Prior integration/profile
collision remains deferred; this does not change Air/battery or firmware gates.

Stop-snapshot source `999cb37`, release `f617f5b921c5a71d`, passes build/xHCI
host tests and docs. Fault cases cover stale command completion, wrong slot,
command error/timeout, ineligible requests, one-shot execution and retained DMA.
Exact GPT VM saves/unmounts and holds; missing-workspace VM holds with recovery
instructions. Installed unarmed with Ubuntu default and accepted recovery intact.
Offline checks remain clean (127 files, 439 clusters). Native result pending;
this diagnostic does not claim a fix for the post-reset failure.

Native stop-snapshot trial `f617f5b921c5a71d`, session
`00065e64dd39e82b-00`, again times out at request13/FCE write0x800 after
300003 µs. Stop Endpoint completed successfully: state=3, dequeue=0x1eb381,
stopped transfer event cc26 at 0x1eb380 with residue4. Submitted setup/data/status
addresses were 0x1eb370/0x1eb380/0x1eb390. This locates the outstanding transfer
at the OUT data stage with its full four-byte payload remaining; it does not
establish the cause or prove a particular USB handshake on the wire. Clock
readiness passed; preparation/upload failed, zero chunks, quarantine retained.
Photo/logs archived, consumed one-shot/Ubuntu default/recovery verified. Offline
filesystem/allocation checks pass. Next compare post-reset OUT handling and
adapter initialization requirements; no next boot armed.

Next scope: separate opt-in wifi-fw-v2 MAC-startup trial. Linux v6.18 writes
MAC control 0x2c before MCU reset; v1 deliberately used 0x20. v2 tests that
startup prerequisite, restores/verifies 0x20 after FCE setup before upload, and
retains exact device/firmware gates and quarantine. No channel/RF configuration
or network frames. Files: preparation/transport, main opt-in, staging/VM recipes
and host tests. Fetched main e946f5e; only PR #19 open, Air scope battery query
receipt work. Integration profile collision remains deferred. Native proof is
required; MAC startup may remain enabled on failed I/O until physical power-off.

MAC-startup comparison source `75ea093`, v2 release `c18a4380b3f08235`,
passes build, preparation/xHCI fault fixtures, 51 staging tests and docs. The
fixtures preserve v1, require explicit one-shot startup ownership, verify final
disable before preparation succeeds, and fail closed on each operation. Exact
GPT saved-log and missing-workspace held VMs pass. Installed unarmed; Ubuntu
default/recovery verified; offline checks clean (129 files, 442 clusters).
Native result pending. Next attended cold trial compares the post-reset FCE
write with the upstream MAC startup value; no firmware readiness accepted.

Native MAC-startup comparison `c18a4380b3f08235`, session
`00065e653266c290-00`, confirms requested=1/written=1 but still times out
at request13/FCE write0x800 after 300000 µs. Stop Endpoint succeeds: state=3,
dequeue=0x1eb381, stopped event at data TRB 0x1eb380, cc26/residue4. The full
four-byte OUT payload remains outstanding, as in v1; MAC startup did not resolve
the failure. The initial MAC readback passed before reset; restoration to 0x20
was not reached. Preparation/upload remain incomplete, zero chunks, quarantine
retained; RF/connectivity not tested. This does not prove a particular USB wire
handshake or establish the cause. Matching logs/photo/run record archived in
`~/.local/share/srvros/macbookpro11-2/wifi-mac-75ea093`. Ubuntu default,
consumed one-shot and installed/current recovery payloads verified. Offline
checks pass: 131 files, 445 clusters, zero leaks/mismatches/errors. Next inspect
post-reset USB OUT transaction handling; no new boot armed or firmware readiness
accepted.

Workflow correction after the MAC-startup trial: the saved matching exit log
already includes control timeout, submitted TRBs, Stop Endpoint result, startup
flags and failure markers. Photos are redundant for this handled failure. Future
Wi-Fi trials use the existing default `-auto-return` recipe (omit
`--hold-on-failure`), save/unmount and return to Ubuntu automatically. Collect
and inspect the matching release/session on return; missing or incomplete logs
remain incomplete evidence. No routine photo request. Cold adapter power cycling
may still require the previously announced physical action. The installed held
recipe is unchanged and unarmed; apply automatic return when staging the next
candidate and verify that exact stage in the VM before installation. This is a
workflow/documentation change only. Fetched main remains e946f5e; open Air PR44
concerns battery query refusal with parked USB, outside this workflow change.

Next bounded scope (2026-10-22): xHCI EP0 transaction publication. Current code
publishes the setup cycle before building data/status; Linux queues setup with
inverse ownership and publishes only after the whole transfer is ready. Fix this
ordering with regression coverage for both cycles and ring wrap, leaving device
initialization and retry policy unchanged. This is a correctness issue, not yet
an established cause of the native stall. Files: xhci.c and host fixtures; then
exact-stage automatic-return VM checks and a cold native trial. Fetched main
24fd5f4 (Air SMBus ownership proof); only PR19 remains open. Integration profile
collision stays deferred; no shared battery changes.

EP0 publication candidate source `2389909`, release `5edf8b73bf8f8662`,
passes build/docs and xHCI host tests. Publication checks cover both cycles,
every ring start position, IN/OUT/no-data and existing strict completion/fault
fixtures. Setup remains inverse-cycle until all stages and the exact completion
owner exist; an ordered control store then releases ownership before doorbell.
Exact GPT VM saves and verifies log bytes, unmounts and resets; missing workspace
returns without starting the probe. Installed with automatic failure return,
Ubuntu default and accepted recovery intact; offline workspace checks pass.
Native result pending: no claim this fixes the stall. Next cold adapter trial
requires unplugging Wi-Fi for 10 seconds while off, reconnecting and powering
on; leave workspace connected. Logs replace photos. Installed unarmed pending
user readiness for that physical step.

Native EP0 publication trial `5edf8b73bf8f8662`, session
`00065e71f460e55a-00`, saved a complete matching exit log without a photo.
The change did not resolve the stall: request13/write6/index0x800/length4 times
out after 300003 µs. Stop Endpoint succeeds with state3, dequeue0x1eb381,
event0x1eb380, cc26/residue4; submitted setup/data/status remain
0x1eb370/0x1eb380/0x1eb390. MAC startup and clock prerequisite passed, reset
completed, preparation/upload did not; zero chunks and quarantine retained.
Firmware readiness and RF/connectivity remain unaccepted. The saved snapshot
locates the outstanding OUT data stage but does not establish a wire handshake
or its cause. Evidence archived under
`~/.local/share/srvros/macbookpro11-2/wifi-publication-2389909`. One-shot consumed,
Ubuntu default/current/recovery payloads verified; offline filesystem/allocation
checks pass (133 files, 448 clusters, no leaks/mismatches/errors). No next boot
armed. Next compare the small OUT data-stage representation and Linux's working
control sequence; avoid treating this ordering fix as a native Wi-Fi fix.

Next scope: compare immediate-data encoding for the firmware probe's four-byte
vendor OUT writes with Linux xHCI. Restrict the change to the active configured
Wi-Fi firmware owner and its descriptor buffer; keep IN, no-data, IVB and other
devices unchanged. Preserve exact completion, publication ordering and timeout
quarantine. Add a saved encoding flag and boundary/payload host tests, then
build and verify automatic-return stage. This is not a proven native fix.
Main remains 24fd5f4; Air PR46 covers SMBus/parked storage, outside this scope.

Immediate-data candidate source `88d1cd0`, release `825373ebb599fa0d`, passes
build/docs and xHCI host tests. Tests verify exact little-endian payload/high-word
zeroing, unsupported ownership/buffer/direction/length/request refusal, and an
actual guarded register transfer with IDT set. Saved diagnostics now include
`wifi-firmware-data: immediate=...`. IN, no-data and 64-byte IVB remain buffered
or unchanged. Exact GPT VM saved-log/automatic-reset and missing-workspace return
checks pass. Installed unarmed with Ubuntu default and accepted recovery intact;
offline checks pass (133 files, 448 clusters). Next cold hardware trial awaits
readiness for adapter power cycling; no firmware-readiness acceptance yet.

Native immediate-data trial `825373ebb599fa0d`, session
`00065e7298d3f135-00`, confirms `wifi-firmware-data: immediate=1` but still
fails request13/write6/index0x800/length4 after 300002 µs. Successful Stop
Endpoint reports state3, dequeue0x1eb381, stopped event0x1eb380, cc26/residue4;
setup/data/status addresses remain 0x1eb370/0x1eb380/0x1eb390. Clock and MAC
startup passed, reset completed, preparation/upload did not; zero chunks,
quarantine retained, RF/connectivity not tested. Immediate encoding did not
resolve the native stall; this does not establish a wire handshake or cause.
Matching exit log and run record archived under
`~/.local/share/srvros/macbookpro11-2/wifi-idt-88d1cd0`. Ubuntu default, consumed
one-shot and installed/current recovery verified. Offline checks pass: 135 files,
451 clusters, no leaks/mismatches/errors. No next boot armed. Next compare the
working Linux cold initialization sequence with SrvrOS before another speculative
hardware trial; preserve the live Linux network connection during investigation.

Next scope: Linux cold-initialization evidence, not another kernel candidate.
Fetched main 4ea10e1 (Air SMBus capture); only PR19 remains open. Compare the
actual adapter's control setup/payload/status/timing via bounded usbmon capture.
Linux source agrees on reset request1/value1, 5–6 ms delay and FCE write6/0x800;
its earlier WLAN disable/enable sequence and vendor retries need native evidence.
Add a host capture tool and parser fixtures; filter to initialization registers,
exclude bulk/network payloads, preserve the current network until an announced
physical replug. No driver unbind/reset or reboot as part of preparation.

Source review confirms the captured v6.18 Linux path first calls
`mt76x0_chip_onoff(false,false)`, waits for MAC, then enables WLAN during hardware
initialization. Its register transport allows up to ten attempts, with 5–10 ms
between failed 300 ms requests. SrvrOS intentionally quarantines after one
uncertain request. Neither difference is yet proven causal; do not add blind
retries or GPIO/RF writes. The new `tools/mt76_linux_capture.py` records only
selected initialization-register control URBs and reset, correlates completion
status/timing, and persists only addresses verified on this adapter's port.
Bulk/network payloads are excluded. Four parser fixtures and a 30-second passive
Ubuntu smoke run passed; no device reset or network disruption was performed.
A user-operated 10-second unplug/replug in Ubuntu is next; capture survives the
brief network loss locally. This observes host requests, not physical USB wire
handshakes. No new SrvrOS boot is staged or armed.

Linux cold-replug capture completed on kernel 7.0.0-38-generic, port1-1,
address2→8, with 53 selected requests. The captured first FCE write6/0x800/value1
has callback status -2, actual0, duration307755 µs; the identical next submission
is 10013 µs after that callback and completes status0/actual4 in95 µs. Later the
first MCU-ready read7/0x730 has status-2/actual0 after306038 µs; the next read,
10027 µs later, returns value1 successfully in71 µs. Status-2 is a cancelled URB,
consistent with Linux's 300 ms timeout/unlink path, not a wire-level error code.
This strongly motivates bounded timeout recovery/retry rather than another
payload-encoding change. A retry must first retire the old EP0 transaction,
account for late events and retain DMA until ownership is proven; current SrvrOS
quarantine must not simply be cleared. Both FCE setup and MCU-ready polling need
consideration. No SrvrOS fix or new boot is accepted/armed by this observation.

Capture limitations: some earlier setup records and a later completion are
missing; an implausible WLAN read payload exposed a parser correlation weakness
when Linux reuses an URB tag for an unselected submission. The original capture
is preserved, not silently corrected. The tool now invalidates such stale owners
and marks reuse, excludes transient address0, and passes five parser fixtures.
The retry pattern above is evidence for the next direction, not a complete
validated trace; repeat with the corrected parser before relying on exact values
or accepting a recovery implementation. Raw capture lives in
`~/.local/share/srvros/macbookpro11-2/linux-cold-1792696794998436335`.

Corrected-parser Linux cold capture confirms the retry pattern on the same
7.0.0-38-generic host, port1-1/address8→9. The first write6/0x800/value1
has callback status-2/actual0 after303066 µs; identical resubmission follows
10028 µs later and completes status0/actual4 in112 µs. The first read7/0x730
has status-2/actual0 after305986 µs; retry after10024 µs completes in1065 µs
with value1. These four records have matched completions and no tag-reuse
ambiguity. Other early missing records are now explicitly marked instead of
misattributed, so this remains a filtered, incomplete sequence rather than full
wire evidence. The cancellation/retry observation is independently repeated.
Archive: `~/.local/share/srvros/macbookpro11-2/linux-cold-1792697340613399648`,
with source/tool digest, original capture, validated comparison and checksums.
Linux default route recovered; no SrvrOS boot armed. Next bounded scope is safe
EP0 timeout retirement and retry for these observed operations, including exact
command ownership, stopped-ring dequeue advancement, late-event exclusion and
retained DMA fault tests. Never simply clear quarantine or replay the reset.
Native firmware acceptance remains pending.

Recovery implementation scope: one retry each for post-reset FCE write0x800=1
and post-IVB ready read0x730 only. Require successful Stop Endpoint with stopped
state/data-stage evidence; switch EP0 to a fresh ring and buffer using exact
Set TR Dequeue completion. Retain old/new DMA allocations on every failure;
never reuse retired transfer addresses. Delay10 ms, retry once, quarantine on
any uncertainty. Add fault/late-event fixtures and saved recovery diagnostics.
Fetched main53a24a0; Air PR48 covers sustained SMBus/Text, outside this scope.

Bounded-retry source `122fcda`, release `b7f7fb1c357d7ff5`, passes build/docs
and xHCI host fixtures. Tests cover fresh ring/buffer and copied OUT/zeroed IN,
exact Set Dequeue command failure/wrong-slot/timeout/context refusal, allocation
and delay failure, identity/phase/stop-evidence gates, one-shot limits, and full
write/read timeout→stop→dequeue→retry paths with stale old-TD events and a second
failure. All DMA stays allocated; no stale completion can retire a fresh-address
TD. Saved recovery diagnostics include attempts/successful/mask and ring/buffer.
Exact GPT saved-log automatic return and missing-workspace return VMs pass.
Installed unarmed; Ubuntu default and accepted recovery verified; offline checks
pass (135 files, 451 clusters). Native acceptance pending. Next attended cold
shutdown requires Wi-Fi unplugged10 seconds while off, reconnected before power
on; workspace stays connected. Results save and return automatically, no photos.


Native bounded-retry trial `b7f7fb1c357d7ff5`, session
`00065e7310c09789-00`: **firmware upload and MCU readiness accepted** for the
captured Edimax7392:a711 revision0100, ASIC76100002/MAC76502000. Recovery
attempts2/successful2/mask3; preparation/reset/upload complete; result0, chunks6,
mac_disabled1, quarantined0. Final MAC0x1004 read returns0x20 with exact status
completion; firmware and laptop capture both completed=yes. This validates the
bounded recovery path on this run, not Wi-Fi RF/network connectivity. No radio,
scan, association, authentication or IP acceptance is implied.

Logs, exact payload, run record and checksums retained outside build under
`~/.local/share/srvros/macbookpro11-2/wifi-retry-122fcda`. Ubuntu default, consumed
one-shot, current payload and accepted desktop recoverye47dca3a6b30c6c1 verified.
Offline checks pass:137 files,454 clusters,zero leaks/mismatches/errors. No new
boot armed. Next phase: interpret captured calibration/regulatory data and add
bounded radio/receive initialization before scan, then authentication and IP.

Next radio-phase scope: decode receive-side calibration from the accepted logical
EEPROM capture before hardware writes. Extend the offline audit with board/band
hints, PA/LNA flags, frequency correction and receive gain/RSSI/temperature data;
track missing fields explicitly and retain radio_authorized=false. Raw region
bytes do not select a country or authorize channels. Use synthetic boundary tests
and the local redacted capture; do not publish raw calibration or MAC data.
Fetched main598fe27; only PR19 open. No shared battery changes or reboot planned
for this offline prerequisite. Native RF initialization remains a later step.

Offline receive-calibration audit completed against the existing accepted
redacted512-byte capture, SHA2563969e4b2817fd02c1d5285614abc92fd9987af6c07a2444fdf68b85460a16749.
All required receive fields are mapped. Decoded frequency68, temperature−8,
5GHz LNA12/12/12, RSSI2GHz1/1 and5GHz0/0; hardware RF control and TSSI flags
are clear. Board hint both bands is not regulatory/channel authorization.
Five audit tests cover signed correction, RSSI boundaries, gain fallback,
missing/unmapped fields and band-hint separation. No radio registers changed;
firmware readiness remains accepted, RF/scanning pending. Next: per-channel power
and regulatory constraints, then a bounded native receive initialization path.
No reboot armed for this offline work.

Next bounded scope: offline 20MHz per-channel transmit calibration and signed
per-rate offsets for the captured non-TSSI layout, preserving unmapped/invalid
entries as unknown. EEPROM values are chipset calibration units, not dBm limits.
Compare against this adapter's Linux channel flags; disabled/no-IR/DFS channels
cannot become active-scan candidates merely because calibration exists. Country
confirmation is pending; Linux's current US setting is evidence, not user locale.
Main remains598fe27; only PR19 open. No hardware writes or reboot in this step.

Offline non-TSSI 20MHz power audit completed against the accepted logical image.
Values are5 for2GHz1–14 and17–20 across5GHz36–165; these are calibration units,
not dBm. Channels169/173 are invalid and177 outside the decoded map. Signed rate
offsets decoded without promoting erased/unmapped data. Eight audit tests pass,
covering channel/byte mapping, signed endpoints, invalid/missing data and TSSI
refusal. Linux PHY/regulatory snapshots saved locally; current Linux domainUS is
not treated as user country confirmation. Country question remains pending.
No regulatory policy applied, radio writes performed or reboot armed. Next work
is a country-scoped policy and bounded native receive initialization; firmware
readiness remains accepted, radio/network functionality remains pending.

User confirmed United States testing on2026-10-22. Use explicit countryUS for
the initial channel-policy work; the earlier pending-country question is resolved.
This records location, not completed regulatory-policy integration or permission
to bypass calibration/driver gates. No radio writes or reboot armed.

Next bounded scope (2026-10-22): offline US 20MHz candidate filter in
`tools/wifi_channel_policy.py` with synthetic refusal tests. Intersect a deliberately
small development channel set with the captured Linux PHY restrictions and valid
calibration. This is not a regulatory database or native transmit authorization;
unknown flags, DFS, no-IR and missing evidence fail closed. Main598fe27 inspected;
Air PR49 concerns battery UI, separate from this tool. Native RF/power validation
remains a dependency. No installed payload changes or reboot planned.

Offline US candidate filter completed against the existing paired Linux snapshots
and accepted calibration audit:20 candidates (1–11,36/40/44/48,149/153/157/161/165).
Snapshot ceilings are2dBm on2GHz,11dBm on36–48 and12dBm on149–165, distinct from
chipset calibration units. All other decoded channels refuse; transmit permission
remains false throughout. Five policy tests cover country/width, PHY flags,
regulatory band edges/flags, calibration refusal, duplicate records and lower-cap
selection. Calibration/firmware audit regressions also pass (12 tests). No native
policy installed or reboot armed. Next: bounded native RF/receive initialization
and power programming validation before any active scan.

Next scope: native MCU command wire codec, required before radio initialization.
Add freestanding bounded packet encoding and response correlation with host tests;
no hardware commands or RF enables in this checkpoint. Linuxv6.18 mt76x02 USB
command framing is the reference. Fetch shows main3c47b50 (merged battery PR49);
inspected scope does not overlap this new driver module. Device branch still
uses598fe27 integration baseline; no blind integration of capture-profile changes.
Dependencies: exclusive bounded USB command transport, native calibration/power
and RF tables remain unimplemented. No physical run or reboot armed.

MCU prerequisite implemented: bounded freestanding packet codec and serialized
non-reusing15-command diagnostic session. No xHCI command transport connection or
radio initialization is claimed. Tests cover independently specified wire bytes,
every payload length1–192, output canaries/padding, insufficient capacity,
invalid fields, response bounds, outstanding-command exclusion, sequence
exhaustion and uncertain/mismatched completion poisoning. Host and ASan/UBSan
checks pass; US policy tests are now included in test-core. Next checkpoint is
native command transport and saved response evidence, before RF initialization.
No payload installed, USB adapter disturbed or reboot armed by this work.
Full kernel build and existing preparation/upload/bring-up/xHCI regressions pass;
documentation link checks pass. Native evidence remains firmware readiness only.

Next scope: connect a one-shot MCU read diagnostic to xHCI for the captured
Edimax descriptor. Reuse the retired firmware OUT endpoint, add a separate
command-response IN endpoint/ring/buffer, and read only MAC_CSR0 through firmware.
Require accepted firmware completion and disabled MAC; retain all DMA on failure.
Record bounded transfer status and decoded response without accepting RF/network.
Main3c47b50 inspected; Air branch5fe69fa remains battery work. No concurrent
staging or physical run. Boot-profile integration follows transport tests.

One-shot MCU xHCI transport implemented behind firmwarev3 opt-in. Fixed random
read of MAC_CSR0 only; separate IN0x85/DCI11 response ring, existing OUT8 ring,
500ms waits, exact transfer ownership, short-IN bounds and retained DMA on every
failure. Saved header/first16bytes are evidence only (validated=0); final EP0 MAC
check must remain0x20. Full build/xHCI regressions and52 staging tests pass,
including new endpoint configuration, wrong slot/DCI/pointer, event errors,
residue, allocation failure and unready-firmware refusal. No native acceptance
or radio enablement claimed. Stage/VM checks precede any physical installation.

MCU transport sourcec588228, release8e87dabf5c918f1c, staged and installed unarmed.
Exact GPT saved-log/automatic-return VM and missing-workspace return VM pass;
Ubuntu default, pinned descriptor, model, workspace0E53-5900 and retained recovery
e47dca3a6b30c6c1 verified. Offline workspace fsck/allocation checks pass. Candidate,
receipts and run record archived outside build under wifi-mcu-c588228. Accepted
firmware payloadb7f7fb1c357d7ff5 remains retained; only the archive-verified older
825373ebb599fa0d payload was removed for space. Await attended cold adapter cycle:
shutdown, unplug Wi-Fi10 seconds, reconnect then power on, leave workspace drive
connected. Logs save and return automatically; no photo or commands required.
After return collect matching session first; MCU response/RF acceptance pending.

Native MCU trial8e87dabf5c918f1c session00065e73f7f56ce9-00 collected: firmware
readiness passes again (result0,chunks6,MAC0x20,recovery2/2). MCU OUT completes16
bytes; IN times out with received=-1,cc0,decoded0,failed1. Diagnostic completed=NO;
no MCU response or RF acceptance. Final mac_disabled0 denotes an unperformed
post-timeout read after quarantine, not an observed enabled MAC. Matching logs
archived under wifi-mcu-c588228/native-collection. Ubuntu default, consumed
one-shot and current/recovery manifests verified. Offline workspace clean:
139files,457clusters,zero leaks/mismatches/errors. Reboot unarmed.

Reference follow-up: Linuxv6.18 mt76x0_init_hardware sends functionQ_SELECT=1,
value1 before ordinary MCU operations; mt76x02_mcu_function_select deliberately
uses no response for Q_SELECT. Our one-shot diagnostic omitted that routing
setup. This is a candidate explanation for a successful OUT without an IN reply,
not yet proven. Next bounded change: explicit queue-selection setup with sequence0
(no expected reply), then the existing read-only command; preserve ordinary
command sequence guards and native MAC-disable checks. No new trial installed.

Next scope: add fixed Q_SELECT(1) command with sequence0/no expected reply before
one-shot MCU random read, preserving ordinary command sequence1–15 validation.
Extend host tests for wire bytes and ordered transport failures; retain firmwarev3
capture semantics and disabled-MAC checks. Main85d5cb7 fetched; battery work remains
separate. Native evidence remains prior IN timeout; this candidate is not accepted.

Fixed no-reply Q_SELECT packet added before read; generic sequence0 remains
refused. Full build, MCU codec, xHCI and52 staging regressions pass. New host
coverage verifies exact setup bytes, capacity refusal and ordered setup/read/IN
submission stopping after failure at each step, with DMA retention. Preparing
exact-stage VM checks; native response acceptance remains pending.

Queue-selection source456922b release4a47cd57ac90c504 installed unarmed. Exact
GPT saved-log/automatic-return and missing-workspace VMs pass; offline workspace
checks and Ubuntu/current/recovery verification pass. Durable archive:
~/.local/share/srvros/macbookpro11-2/wifi-queue-456922b. Known-good desktop recovery
e47dca3a6b30c6c1 retained; prior MCU trial8e87dabf5c918f1c retained. Accepted firmware
payloadb7f7fb1c357d7ff5 removed from ESP only after exact archive verification;
its durable payload/evidence remains available. Await attended cold adapter cycle,
then collect matching session before further work. No photos or commands needed.

Native queue-selection trial4a47cd57ac90c504, source456922b, session
00065e74311ea511-00: firmware result0/chunks6; Q_SELECT OUT16; random-read OUT16;
IN16,sequence1,event0,reported-length8,cc13,residue1008. Reply words are
4a010008/00411000/76502000/4a010008: requested WLAN MAC_CSR0 address and expected
chipset value, with repeated header trailer. Final MAC-disable check passes,
failed0, capture completed=yes. This accepts the observed one-shot queue/read
exchange on this adapter, not sustained MCU service or RF/network functionality.
The native logger still reports validated=0: strict command-specific response
validation is the next implementation step, using this evidence plus negative
length/event/address/value/trailer fixtures.

Matching collection archived under wifi-queue-456922b/native-collection. Ubuntu
default, consumed one-shot and installed/current recovery manifests verified.
Offline workspace clean:141files,460clusters,zero leaks/mismatches/errors. Run
record and checkpoint collected/unarmed; no new physical trial prepared.

Next bounded scope: strict validation of the native one-shot MCU identity reply
captured in session00065e74311ea511-00. Match exact16-byte envelope,sequence1,
event0,length8,register00411000,value76502000 and repeated header trailer. This
contract is specific to the captured diagnostic, not a generic MCU response
parser. Add truncation/extension and every-bit mutation tests, then require the
validator in the diagnostic. Main85d5cb7 inspected; no battery overlap, no new
radio writes or physical boot in this validation step.

Strict one-shot reply validator implemented and required by diagnostic success.
Host replay of the archived native response passes. All128 single-bit mutations,
truncated/extended lengths and a consistent stale sequence refuse; ASan/UBSan,
full build, MCU and xHCI regressions pass. This is host validation against native
evidence; the new validator has not itself been physically booted. Installed
release4a47cd57ac90c504 remains unchanged and unarmed. Next scope is bounded native
radio initialization using per-operation completion contracts; no RF acceptance.

Next scope: bounded MAC/BBP reset preparation engine, following the captured
MT7610 identity and Linuxv6.18 initialization order. Require firmware ownership,
idle WPDMA/MAC and disabled MAC before asserting CSR/BBP reset; bound the200ms
hold and verify reset release. No RF table, channel, power or TX-enable writes.
Host fault coverage before xHCI/native wiring. Main85d5cb7 remains integration
head inspected; Air battery scope separate. Installed payload unchanged.

MAC/BBP reset prerequisite engine completed with exact identity/disabled-state
checks, bounded idle polls,200ms cancellable reset hold, release readback and
post-reset idle checks. Full build, MCU and new reset tests pass; reset tests
also pass ASan/UBSan. Every operation failure stops, every later retry refuses,
and no write sets TX/RX bits. No xHCI binding or physical execution added yet;
installed release remains4a47cd57ac90c504, reboot unarmed. Next connect this engine
to a separately guarded native capture, then verify reset/MCU survival before
loading MAC/BBP/RF tables. Calibration/power enforcement remains required for TX.

Next scope: firmwarev4 opt-in native MAC/BBP reset capture, before the existing
queue-selection/strict identity-read exchange. Bind only reset-engine register
reads and ordered MAC_SYS_CTRL writes3 then0; never widen firmware write gates.
Require exclusive prepared firmware transport; final MAC must be0 after reset.
Preserve v1/v2/v3 recipes and automatic saved-log return. Host binding/profile
checks and exact-stage VMs precede installation; cold trial requires readiness.
Main85d5cb7 remains fetched baseline; Air changes remain separate.

Reset binding build and host checks pass, including exact ordered write values,
phase/identity/ownership refusals, early-release refusal and USB failure
quarantine.53 staging tests pass and documentation links check clean. New reset
engine and strict native reply validator are included in v4; hardware acceptance
remains pending. Preparing stage/VM checks before installation.

Native reset candidate source2bc3689 releasebfa7235c397175da installed unarmed.
Exact GPT saved-log/automatic-return and missing-workspace VMs pass; workspace
fsck/allocation and Ubuntu/current/recovery verification pass. Durable payload,
receipts and run record in wifi-reset-2bc3689 outside build. Accepted queue/read
payload4a47cd57ac90c504 and known-good desktop recoverye47dca3a6b30c6c1 retained;
older failed MCU payload8e87dabf5c918f1c removed from ESP after archive equality
verification. Await attended cold cycle; collect matching session first after
return. Require reset result0/prepared1/released1 and MCU validated1, disabled MAC;
no RF or connectivity acceptance follows from this trial alone.

Native reset trialbfa7235c397175da/source2bc3689/session00065e747b16d8cb-00:
**MAC/BBP reset and strict MCU identity exchange accepted** for this adapter.
Reset result0,released1,prepared1,reads9,writes2,hold200000us; final idle MAC
status1200=0. Queue OUT16/read OUT16/IN16, expected identity response, validated1,
failed0 and final disabled MAC_SYS_CTRL=0. Firmware and laptop completed=yes.
This accepts only this reset/identity sequence, not MAC/BBP tables, RF calibration,
receive/scan, association, authentication or IP connectivity.

Matching collection archived under wifi-reset-2bc3689/native-collection. Ubuntu
default, consumed one-shot and current/recovery manifests verified. Offline
workspace clean:143files,463clusters,zero leaks/mismatches/errors. Run record and
checkpoint collected/unarmed. Next: bounded MAC/baseband table initialization
with command-specific completions, then calibrated RF/receive setup.

Next scope: bounded MCU register-pair packet encoder before MAC/baseband tables.
Linuxv6.18 tables include PA and TX-power entries; do not treat them as uniformly
receive-only. Implement at most24 aligned WLAN register pairs per command, with
nonzero response sequence and atomic input validation. No table authorization or
native writes added. First native write diagnostic must establish acknowledgement
format with a disabled-MAC write before table execution. Main9ca1922 fetched and
inspected (battery/Home changes include shared staging/main); integration remains
598fe27 and profile reconciliation is deferred, not silently overwritten.


Register-pair prerequisite implemented: one bounded1–24-pair read/write packet,
complete validation before output, no implicit chunking or no-ACK writes. The
existing native identity read uses the shared builder with identical wire bytes.
Build, MCU/radio/xHCI host tests and MCU ASan/UBSan passed. No candidate installed
or reboot armed for this pure encoding change; native write acknowledgement and
MAC/baseband table execution remain pending.

Active scope: opt-in firmwarev5 MCU write-response capture after the accepted
reset and strict identity exchange. Permit exactly one sequence2 random write
of MAC_SYS_CTRL1004=0, require disabled MAC before and after, retain bounded raw
response words for inspection. No initialization tables or RF writes. Shared
main remains9ca1922; current device branch inspected clean atf2dc195. Dependencies
are accepted reset/identity transport and bounded register-pair encoder. Native
write-response format remains unaccepted until the matching saved run is read.

Write-response candidate source63d7714/released8dca7885de3c61f installed unarmed.
Build, MCU/radio/xHCI tests,54 boot-profile tests and docs passed. Exact-stage GPT
saved-log/clean-unmount/automatic-return VM and missing-workspace refusal passed.
VM does not validate native MCU writes. Durable archive wifi-write-63d7714 has
payload, receipts and run record. Workspace offline checks passed; Ubuntu remains
default, recoverye47dca3a6b30c6c1 and accepted resetbfa7235c397175da retained.
Older queue payload4a47cd57ac90c504 removed from ESP only after archive equality
verification. Native write reply pending; readiness needed for cold adapter
power cycle. Results will save automatically; no photograph or commands needed.

Native write-response run d8dca7885de3c61f/source63d7714/session00065e74d8311f56-00
collected and inspected. Reset and strict identity exchange passed. Fixed
CMD_RANDOM_WRITE sequence2, register1004=0 returned16bytes with words4a020008,
00411004,00000000,4a020008 (cc13,residue1008). EP0 readbacks before and after were0;
MAC remained disabled. Firmware/laptop completion markers present. This records
one fixed write's response format, not a general write/table acceptance. The
installed diagnostic correctly logs ack_validated=0; a strict command-specific
validator and mutation tests are next before table execution.

Collection and hashes archived in wifi-write-63d7714/native-collection. Ubuntu
default, consumed one-shot and current/recovery manifests verified. Workspace
unmounted and clean:145files,466clusters,zero leaks/mismatches/errors. Run record
and checkpoint collected/unarmed. RF, receive/scan and connectivity remain pending.

Active scope: strict validator for the fixed sequence2 MAC_SYS_CTRL=0 MCU reply
captured in session00065e74d8311f56-00, host mutation tests and diagnostic wiring.
Touches mt76 MCU codec, xHCI diagnostic, tests and these guides; no table or RF
writes. Main9ca1922 remains integration target; remote Air scopes inspected.
Native validator acceptance is pending; replay validates the saved fixture only.

Strict fixed-write ACK validator implemented and wired into v5 completion.
Saved native16-byte response replay,128 single-bit mutations, stale sequences,
length/address rejection, MCU ASan/UBSan, build and xHCI tests passed. This is
source/host validation; installedd8dca7885de3c61f is unchanged. No reboot armed.
Next table work must define bounded chunks, address/value policy and completion
contracts without assuming the single-pair echo proves multi-pair behavior.

Active scope: firmwarev6 two-pair MCU response capture, after strict single-write
ACK validation. Two identical MAC_SYS_CTRL1004=0 pairs, sequence3; no additional
registers/tables/RF. Capture bounded response words and disabled-MAC readbacks;
do not presume a multi-pair echo contract. Main9ca1922 fetched, other machine
scopes unchanged; touches MCU diagnostic, boot profile, tests and guides.

Two-pair candidate sourcefe5c0a8/release101abba9feeb8cfa installed unarmed.
Build, MCU/xHCI tests,55 boot-profile tests and docs passed. Exact-stage GPT
saved-log/clean-filesystem/automatic-return VM and missing-workspace refusal
passed. Durable archive wifi-pair-fe5c0a8 contains payload, receipts and run record.
Workspace offline clean:145files/466clusters. Ubuntu default and known-good
recoverye47dca3a6b30c6c1 preserved; prior single-write captured8dca7885de3c61f
retained. Reset payloadbfa7235c397175da removed from ESP only after archive/hash
verification. Cold hardware result pending; no multi-pair ACK or table acceptance.

Native v6 sourcefe5c0a8/release101abba9feeb8cfa/session00065e751cdb49ed-00:
strict single-write ACK accepted (ack_validated1), reset/identity prerequisites
passed. Duplicate two-pair OUT24 returned IN16, words4a030008,00411004,00000000,
4a030008; sequence3/event0/length8, cc13/residue1008. MAC before/after0. Capture
completed. This proves a bounded response to this duplicate-pair diagnostic,
not that arbitrary pairs are echoed or independently verified. Since both pairs
were identical, first-versus-last echo cannot be determined. Next establish a
distinguishing response/readback contract before initialization table execution.

Matching collection archived in wifi-pair-fe5c0a8/native-collection. Ubuntu default,
consumed one-shot and current/recovery manifests verified. Workspace unmounted,
clean147files/469clusters,zero leaks/mismatches/errors. Run/checkpoint collected
and unarmed. No RF, receive, scan or connectivity acceptance.

Active scope: v7 distinct timing-pair diagnostic, XIFS1100=33a41010 and
BKOFF1104=209 from Linuxv6.18 initialization/register definitions. Require strict
single-write ACK and disabled MAC; capture before/after EP0 values independently
of MCU reply. No RF/PA/power tables or TX/RX enable. Existing profiles retained.
Main9ca1922 fetched; other Air scopes inspected separately from this USB work.

Distinct timing candidate source607ad4f/release201acb491eb09532 installed unarmed.
Build, MCU/xHCI tests (including fixed timing packet and scoped read binding),
56 boot-profile tests and docs passed. Exact-stage GPT saved-log/clean-filesystem/
automatic-return VM and missing-workspace refusal passed. Durable archive
wifi-timing-607ad4f contains payload, receipts and run record. Workspace offline
clean:147files/469clusters. Ubuntu default/recoverye47dca3a6b30c6c1 and prior
pair capture101abba9feeb8cfa retained. Oldd8dca7885de3c61f removed from ESP only
after equality with durable archive. Native distinct response/readbacks pending.

Native timing source607ad4f/release201acb491eb09532/session00065e753e794b20-00:
accepted exact two-entry transaction with independent changed readbacks. OUT24
returned IN16, words4a030008,00411100,33a41010,4a030008: first entry echoed.
XIFS1100 changed33a4100a->33a41010; BKOFF1104 changed214->209. readback_ok1,
changed0/1 both1. Reset, identity and strict single-write ACK passed; MAC stayed0
before/after. Firmware/laptop completed=yes. No larger-batch, general table or
RF acceptance follows; installed pair ACK remains capture-only (validated0).
Next: strict timing reply validator and bounded table completion with readbacks.

Collection/hashes archived in wifi-timing-607ad4f/native-collection. Ubuntu
default, consumed one-shot and current/recovery manifests verified. Workspace
unmounted, clean149files/472clusters,zero leaks/mismatches/errors. Run/checkpoint
collected and unarmed. Receive/scan/association/connectivity remain pending.

Active scope: bounded timing initialization engine for the accepted exact pair,
strict sequence3 first-entry ACK plus independent readbacks and disabled-MAC
checks. No general table/register authorization or RF expansion. Main62067a1
fetched; shared battery/main/staging changes inspected, integration still598fe27.
Touches mt76 MCU engine, xHCI timing binding, tests and guides. Native evidence
is session00065e753e794b20-00; new engine acceptance remains pending.

Bounded timing initialization implemented: exact two-entry packet, strict
first-entry ACK, both independent readbacks and MAC0 before/after. One attempt
per session, authorization before every I/O and completion; failure quarantines
through xHCI with no retry. v7 now uses this engine; v6 remains capture-only.
Host checks passed: build, MCU/xHCI, per-operation failure/cancellation,
second-readback mismatch despite valid ACK, enabled MAC refusal,128 reply-bit
mutations,192 submitted-packet mutations, ASan/UBSan, native reply replay and docs.
No new physical acceptance: installed201acb491eb09532 is unchanged and no boot
is armed. General MAC/BBP tables, larger batches and RF remain pending.

Timing-engine source48879c2/release0c12150fc75d830c installed unarmed. Exact-stage
GPT saved-log/clean-filesystem/automatic-return VM and missing-workspace refusal
passed. Prior build/MCU/xHCI/fault/sanitizer/native-replay checks are archived with
payload and run record in wifi-timing-engine-48879c2. Workspace offline checks
passed. Ubuntu default/recoverye47dca3a6b30c6c1 and prior timing capture201acb491eb09532
retained; obsolete101abba9feeb8cfa removed only after durable archive equality.
Next hardware acceptance requires wifi-timing-init status0, ack_validated1,
complete1, reads6, both target readbacks and MAC0 before/after. Native pending.

Native timing engine accepted: source48879c2/release0c12150fc75d830c/
session00065e7570e64246-00. wifi-timing-init status0,attempted1,write_attempted1,
sent24,received16,ack_validated1,complete1,reads6. MAC before/after0; XIFS changed
33a4100a->33a41010 and BKOFF214->209. Firmware/reset/strict identity/single-write
prerequisites passed and firmware/laptop completed=yes. This accepts the exact
bounded two-register initialization only, not full MAC/BBP tables or RF.

Matching collection/hashes archived in wifi-timing-engine-48879c2/native-collection.
Ubuntu default, consumed one-shot and current/recovery manifests verified.
Workspace unmounted and clean151files/475clusters,zero leaks/mismatches/errors.
Run/checkpoint collected and unarmed. Next expand MAC/baseband initialization
with bounded commands and register-specific readback policies; RF/scan/network
connectivity remain pending.

Active scope: fixed MAC basics engine after accepted timing, two pairs per
command with sequences4/5. Targets legacy basic rate1408=13f, HT basic
rate140c=4003 (model final value), RX filter1400=17f97, maximum length1018=a1fff,
from Linuxv6.18 register/init definitions. No TX/RX enable, RF, PA/power or key
writes. Require exact first-entry ACK plus per-register readbacks and MAC0 at
each batch boundary. Pure engine/tests first; native binding remains separately
gated until validated. Main1454cb7 fetched/inspected; integration still598fe27.

Fixed four-register MAC basics engine implemented, with separate sequence4/5
packets, exact first-entry replies, per-register readbacks and MAC0 boundaries.
Host tests passed for all operation failures/authorization boundaries, reply
mutations, stale batch replies and each readback mismatch. Build, MCU/xHCI,
ASan/UBSan and docs checks passed. Not yet bound to native xHCI or accepted on
hardware; installed0c12150fc75d830c remains unchanged and no reboot is armed.
Next bind this fixed plan after accepted timing with exact packet/read allowlists,
then stage and rehearse the candidate before native acceptance.

Active scope: opt-in firmwarev8 binding of fixed MAC basics after accepted timing.
Exact sequence4/5 packets only, one submission each in order, four-register/ MAC0
read allowlist and prior timing completion required. Local transport/boot tests,
exact-stage rehearsal and preserved recovery precede native test. Main1454cb7
fetched, integration remains598fe27; no battery or sibling gates modified.

Native binding implemented; build, MAC/MCU/xHCI host suites, 57 boot-profile
tests and docs checks passed. Transport tests cover ordered submissions,
prior-batch completion, exact packet mutations, read scope, stale events and
retained-DMA quarantine. Preparing firmwarev8 exact-stage VM and hardware trial;
new register responses/readbacks are still physically unaccepted.

MAC basics native trial source facb862, release190ab5353ae2f8b6, firmwarev8:
exact-stage GPT save/unmount/return and missing-workspace return VM checks passed.
Installed unarmed with Ubuntu default and e47dca3a6b30c6c1 recovery preserved;
accepted timing engine0c12150fc75d830c retained. Prior201acb491eb09532 payload
removed from ESP only after matching its durable archive byte-for-byte.
Workspace preboot filesystem/allocation checks passed. Payload, VM receipts,
local checks and run record archived outside build in wifi-mac-native-facb862.
Next cold trial requires Wi-Fi disconnected for10 seconds while off, then
reconnected before power-on; workspace stays attached. Automatic log saving and
return enabled. Accept only both unique replies, all four exact readbacks, MAC0
and completion markers; RF/connectivity remain pending.

Native MAC basics accepted, 2026-10-22: source facb862,
release190ab5353ae2f8b6, session00065e75fad3378e-00. Firmware/reset/identity,
single-write and timing prerequisites all passed. Sequence4 and5 each sent24,
received16 with exact first-entry echoes. MAC initialization status0, submitted2,
batches_attempted2/completed2, ack_mask3, complete1, reads12; MAC stayed0.
Readbacks:1408 0->13f,140c 82008000->4003,1400 15f9f->17f97,
1018 a7fff->a1fff. Firmware and laptop completion markers present.
Matching collection archived in wifi-mac-native-facb862/native-collection.
Ubuntu default, consumed one-shot and installed/recovery manifests verified.
Workspace clean153files/478clusters with zero allocation errors; unmounted.
Run/checkpoint collected and unarmed. Next extend remaining MAC/baseband setup
with bounded commands and register-specific readbacks. RF, scan, association
and connectivity remain pending; this accepts only the fixed four-register plan.

Active scope: consume main22fd266 (PR55 shared service/workspace lifecycle and
Air6,1 input support) before extending Wi-Fi initialization. Preserve Pro MCU
commands/tests and all upstream battery profiles; no Pro battery acceptance.
Resolve overlapping boot profile numbers with a namespaced Wi-Fi calibration
recipe and exact historical verification. Build/core tests and exact fw-v8 VM
rehearsal precede any new hardware candidate. Current accepted native release
190ab5353ae2f8b6 and recovery remain installed and unarmed.

Merged baseline22fd266: full build/test-core,64 boot-profile tests and docs checks
passed. All48 upstream laptop profile variants generate identical configs.
Historical v43/v44 Wi-Fi recipes still verify exactly; new calibration uses
wifi-cal-v1 to avoid battery profile collisions. PR60 process-lifetime work is
separate and remains unmerged. Next rehearse/install firmwarev8 for a regression
of the accepted MAC plan on this baseline before extending initialization.

Merged-baseline regression installed unarmed: source285aa45,
releasecb8cc153be25dac9, firmwarev8. Exact GPT workspace/save/return and
missing-workspace VM rehearsals passed. Accepted190ab5353ae2f8b6 and desktop
recoverye47dca3a6b30c6c1 retained; prior timing-only0c12150fc75d830c removed
from ESP after full durable archive equality. Workspace preboot checks passed.
Run/payload/VM/local evidence archived outside build in wifi-main-merge-285aa45.
Next cold native run repeats the accepted sequence4/5/readback plan against
main22fd266. No new radio registers or physical battery acceptance in this run.

Merged-baseline native regression accepted, 2026-10-22: source285aa45
(main22fd266), releasecb8cc153be25dac9, session00065e765bdc87af-00.
Firmware/reset/identity/single-write/timing prerequisites passed; both MAC
commands sent24/received16 with exact sequence4/5 replies. Status0, submitted2,
completed2, ack_mask3, complete1, reads12; all four before/after readbacks match
the accepted facb862 run and MAC remained0. Completion markers present.
Collection/hashes archived in wifi-main-merge-285aa45/native-collection.
Ubuntu default, consumed one-shot and installed/recovery manifests verified;
workspace clean155files/481clusters, zero allocation errors, unmounted.
Run/checkpoint collected and unarmed. Remaining MAC/baseband initialization
can now proceed on this merged baseline; RF/scan/connectivity remain pending.

Active scope: fixed MAC frame-format plan after accepted basics. Disable TSO
0250=0 and header translation0260=0, then set HT control1410=1ff and PN padding
150c=3 using distinct sequences6/7. Values follow captured Linuxv6.18 model
initialization; no RF/power/key or TX/RX enable writes. Require accepted basics,
exact packets/replies, per-register readback and MAC0 boundaries. Native response
shape remains pending for these commands. Main340f566 inspected (process lifetime
work); this independent MCU step uses physically accepted integration22fd266.

Frame-format engine/native binding implemented with fixed seq6/7 and separate
phase ownership; prior basics must be complete. Build, basics/format/MCU/xHCI
tests,65 boot-profile tests, ASan/UBSan format tests and docs checks passed.
Firmwarev9 rehearsal is next; native replies/readbacks remain pending.

Frame-format candidate installed unarmed: source7713556,
release54d6def400fd2a6c, firmwarev9. Exact-stage GPT save/unmount/return and
missing-workspace VM checks passed. Ubuntu default, accepted merged MAC image
cb8cc153be25dac9 and desktop recoverye47dca3a6b30c6c1 retained. Original
190ab5353ae2f8b6 removed from ESP only after durable archive equality.
Workspace preboot checks clean. Payload/run/VM/local evidence archived outside
build in wifi-mac-format-7713556. Next cold trial accepts only exact seq6/7
replies, all four target readbacks, MAC0 boundaries and saved completion.
Physical frame-format acceptance remains pending; RF/connectivity untested.

Native frame-format initialization accepted, 2026-10-22: source7713556,
release54d6def400fd2a6c, session00065e76901356a1-00. All prior prerequisites
passed. Sequence6/7 each sent24/received16 with exact first-entry replies;
status0, submitted2, attempted2/completed2, ack_mask3, complete1, reads12.
MAC before/after0. Readbacks0250 0->0,0260 0->0,1410 100->1ff,150c 0->3.
Firmware and laptop completion markers present. Full matching collection and
hashes archived in wifi-mac-format-7713556/native-collection. Ubuntu default,
consumed one-shot and current/recovery manifests verified; workspace unmounted
and clean157files/484clusters with zero allocation errors. Run/checkpoint
collected and unarmed. Next extend remaining MAC/baseband initialization;
RF, scanning, association and network connectivity remain pending.

Active scope: read-only baseband prerequisite capture after accepted frame format.
Read MAC control/status, PBF clock control and BBP CORE0 (2000); bound zero-core
polling to20 reads, reject all-ones and require MAC disabled/idle before/after.
No write callback or new MCU sequences. Baseband readiness follows Linuxv6.18
mt76x0/phy.c; it does not accept baseband tables or RF. Main105dffd fetched and
inspected (Air lifecycle work); independent capture uses accepted base22fd266.

Read-only BBP engine/native binding implemented. Build, BBP/basics/format/MCU/
xHCI tests,66 boot-profile tests, BBP ASan/UBSan and docs passed. Native gate
requires completed frame format and admits only four reads. Firmwarev10
rehearsal follows; CORE0 value/readiness and clock observations remain pending.

BBP readiness candidate installed unarmed: source6e0bd27,
release6df00376834c37fd, firmwarev10. Exact GPT save/unmount/return and
missing-workspace VM checks passed. Ubuntu default, accepted frame-format
54d6def400fd2a6c and desktop recoverye47dca3a6b30c6c1 retained. Prior merged
cb8cc153be25dac9 removed from ESP only after durable archive equality.
Workspace preboot checks clean. Payload/run/VM/local evidence archived outside
build in wifi-bbp-ready-6e0bd27. Next cold trial checks responding CORE0 and
disabled/idle MAC before/after; PBF value is observation only. Baseband table
initialization and RF/connectivity remain unaccepted.

Read-only BBP readiness accepted, 2026-10-22: source6e0bd27,
release6df00376834c37fd, session00065e76bf8562b4-00. Firmware, reset,
identity, timing, basics and frame-format prerequisites passed. BBP status0,
attempted1, complete1, reads6, core_reads1; CORE0=f000f200, PBF=00080000,
MAC control/status0 before and after. Completion markers present. This accepts
core readiness only, not clock programming, baseband tables or RF.
Matching collection/hashes archived in wifi-bbp-ready-6e0bd27/native-collection.
Ubuntu default, consumed one-shot and current/recovery manifests verified;
workspace unmounted and clean159files/487clusters, zero allocation errors.
Run/checkpoint collected and unarmed. Next implement bounded baseband setup
with exact register readbacks; scanning/association/connectivity remain pending.

Active scope: first bounded BBP CORE setup after native readiness. Fixed
CORE1/4/24/32 offsets2004/2010/2060/2080 values2/0/0/4003000a from Linuxv6.18
model initialization, sequences8/9. Require captured CORE0=f000f200 before/after
each batch, MAC0 boundaries and all target readbacks. No RF, channel, power or
MAC enable writes. Main105dffd unchanged; accepted integration22fd266 retained.

BBP CORE implementation validation: full build, bounded engine and native xHCI
fixtures, prior MCU/timing/MAC/frame-format/readiness regressions, ASan/UBSan
and all 67 boot-profile tests pass. Physical acceptance remains pending.

BBP CORE candidate source23b97f2 / release5f7b66b688f803cb installed unarmed.
Exact-stage GPT save/return and missing-workspace refusal/return VM checks pass.
Workspace fsck/allocation checks pass; Ubuntu default and desktop recovery
e47dca3a6b30c6c1 retained, accepted readiness6df00376834c37fd retained. Old
format54d6def400fd2a6c removed only after full durable-archive equality checks.
Run record: build/bringup/wifi-bbp-core/run.json; durable evidence/payload in
~/.local/share/srvros/macbookpro11-2/wifi-bbp-core-23b97f2. Await cold-trial
readiness; collect the matching session first after return. RF remains untested.

BBP CORE native acceptance (2026-10-22): source23b97f2, release5f7b66b688f803cb,
session00065e76e3fff75c-00 passed exact sequence8/9 replies, both24-byte commands
and16-byte responses, status0/submitted2/completed2/ack_mask3/reads16.
CORE1 changed0->2, CORE4/24 stayed0, CORE32 changed40030001->4003000a;
CORE0 stayedf000f200 and MAC stayed0. All prior setup regressions and both
capture completion markers passed. Evidence and hashes archived outside build
in wifi-bbp-core-23b97f2/native-collection. Ubuntu default, consumed one-shot,
installed payload and desktop recovery verified. Workspace unmounted; clean
161files/490clusters with zero leaked allocations or bitmap mismatches.
Run/checkpoint collected and unarmed. Next scope is remaining bounded baseband
configuration; RF, scanning, association and connectivity remain pending.

Active scope: finish CORE42/44 and initialize IBI11/AGC0 with fixed values
0/0/0fde8081/00021400 at20a8/20b0/212c/2300, sequences10/11. Reuse the
bounded MCU engine with captured CORE identity and disabled-MAC guards,
requiring completed native CORE setup. Touch MCU/xHCI, isolated capture/profile
and tests only; no RF or transmit enable. Main105dffd inspected unchanged;
Air PR63 at182ee78 adds independent battery capture (shared profile/main files,
separate recipe). Retain integration22fd266 and preserve both scopes.

Validation for BBP front: full build, fixed-plan/per-operation failure tests,
xHCI prerequisites and transport quarantine, prior MCU/MAC/CORE/readiness
regressions, ASan/UBSan, 68 boot profiles and documentation checks pass.
Native acceptance remains pending.

Remaining CORE / initial IBI/AGC trial installed: sourcedaffe4c,
release1874caf6830abdc0. Exact-stage GPT save/return and missing-workspace
refusal/return VM checks pass. Workspace fsck/allocation clean; Ubuntu default
and desktop recoverye47dca3a6b30c6c1 retained, accepted CORE5f7b66b688f803cb
retained. Readiness6df00376834c37fd removed from ESP only after full durable
archive equality. Run record build/bringup/wifi-bbp-front/run.json; payload,
VM receipts and prepared collector archived in
~/.local/share/srvros/macbookpro11-2/wifi-bbp-front-daffe4c. Unarmed, awaiting
cold-trial readiness. Collect the matching session first after return; native
acceptance pending. No RF/scan/connectivity test is included.

Remaining CORE / initial IBI/AGC native acceptance (2026-10-22): source daffe4c,
release1874caf6830abdc0, session00065e772300653c-00 passed exact sequence10/11
replies, both24-byte commands and16-byte responses, status0/submitted2/completed2/
ack_mask3/reads16. CORE42/44 stayed0, IBI11 changed0->0fde8081, AGC0 changed
7400->21400; CORE0 stayedf000f200 and MAC stayed0. All prior setup checks and
both capture completion markers passed. Matching logs and hashes archived in
wifi-bbp-front-daffe4c/native-collection outside build. Ubuntu default, consumed
one-shot, installed payload and desktop recovery verified. Workspace unmounted;
clean163files/493clusters with zero leaked allocations or bitmap mismatches.
Run/checkpoint collected and unarmed. Next: remaining AGC/baseband initialization,
then RF/scan/association/connectivity, which remain untested.

Active scope: fixed initial AGC1/2/15/22 at2304/2308/233c/2358, values
3/003a6464/88a28cb8/1e21, sequences12/13. Require completed prior IBI/AGC0
setup, captured CORE identity and disabled MAC before/after each batch. Touch
MCU/xHCI, isolated main/staging recipe and tests. RF remains disabled. Fetched
main14f17d2 (PR63 battery capture) and PR64 Type feedback; neither adds Wi-Fi
dependencies. Retain physically accepted integration22fd266 for this trial.
The 4-bit command sequence space is not wrapped; larger remaining plans will
need explicit batching/session design before exceeding sequence15.

AGC validation: full build, all fixed MCU/MAC/baseband engines, xHCI
prerequisite/transport/read-scope/quarantine fixtures, ASan/UBSan, 69 boot
profiles and documentation checks pass. Physical acceptance remains pending.

Initial AGC trial installed: source56bf5da, releaseb245ac895a7d9396. Exact-stage
GPT save/return and missing-workspace refusal/return VM checks pass. Workspace
fsck/allocation clean; Ubuntu default and desktop recoverye47dca3a6b30c6c1
retained, accepted CORE/IBI/AGC0 release1874caf6830abdc0 retained. Earlier CORE
release5f7b66b688f803cb removed from ESP only after full durable archive equality.
Run record build/bringup/wifi-bbp-agc/run.json; payload, VM receipts and prepared
collector archived in ~/.local/share/srvros/macbookpro11-2/wifi-bbp-agc-56bf5da.
Unarmed, awaiting cold-trial readiness. Collect the matching session first on
return. Native acceptance pending; no RF/scan/connectivity test included.

Initial AGC native acceptance (2026-10-22): source56bf5da,
releaseb245ac895a7d9396, session00065e777b83c006-00 passed exact sequence12/13
replies, both24-byte commands/16-byte responses, status0/submitted2/completed2/
ack_mask3/reads16. AGC1 changed1->3, AGC2 changed6464->3a6464; AGC15/22
already matched88a28cb8/1e21. CORE0 stayedf000f200 and MAC stayed0. All prior
setup checks and both capture completion markers passed. Logs/hashes archived
in wifi-bbp-agc-56bf5da/native-collection outside build. Ubuntu default, consumed
one-shot, installed payload and desktop recovery verified. Workspace unmounted;
clean165files/496clusters, zero leaked allocations or bitmap mismatches.
Run/checkpoint collected and unarmed. Next design bounded larger command batches
and session sequencing before exhausting the4-bit sequence space, then finish
AGC/baseband initialization. RF/scan/association/connectivity remain pending.

Active scope: four-pair MCU batching comparison using accepted AGC1/2/15/22
values, one40-byte command at sequence14 replacing sequence12/13 after IBI/AGC0.
Order AGC15/22/1/2 puts changed targets last, proving the packet tail is applied.
Generalize only the private fixed-plan engine to two or four pairs; public
entry points retain fixed tables/sequences. Check every baseline/readback and
MAC/CORE identity boundaries. No new register settings or RF enable. Main9256faa
(PR64/65 Type feedback) and Air PR66 battery transitions inspected; independent
Wi-Fi scope, retain accepted integration22fd266. Touch MCU/xHCI/main/profile
and tests. Sequence reuse stays forbidden; successful batching can later
consolidate setup commands without exhausting the four-bit space.

Four-pair validation: full build, all shared fixed-plan regressions, new
13-operation/14-authorization-boundary faults, native exact40-byte packet and
quarantine fixtures, ASan/UBSan, 70 boot profiles and docs checks pass.
The comparison skips the old AGC phase; native expected tail baselines1/6464
must change to3/3a6464. Physical batching acceptance remains pending.

Four-pair comparison installed: sourced1c0811, releaseb3eaadb9ad679dbb.
Exact-stage GPT save/return and missing-workspace refusal/return VM checks pass.
Workspace fsck/allocation clean; Ubuntu default and desktop recovery
e47dca3a6b30c6c1 retained; accepted AGCb245ac895a7d9396 retained. Older
CORE/IBI1874caf6830abdc0 removed only after full durable archive equality.
Run record build/bringup/wifi-bbp-batch/run.json; payload, VM receipts and
strict tail-change collector archived in
~/.local/share/srvros/macbookpro11-2/wifi-bbp-batch-d1c0811. Unarmed, awaiting
cold-trial readiness. Collect matching session first after return; no native
batching acceptance yet. RF and connectivity remain untested.

Four-pair native acceptance (2026-10-23): sourced1c0811,
releaseb3eaadb9ad679dbb, session00065e7a17440518-00 passed one40-byte command,
exact sequence14/16-byte reply, status0/submitted1/completed1/ack_mask1/reads12.
AGC15/22 stayed88a28cb8/1e21; tail pairs AGC1/2 changed1/6464->3/3a6464.
The earlier sequence12/13 AGC phase was absent, so tail readbacks prove full
payload application. CORE0 stayedf000f200, MAC stayed0; preceding setup checks
and both completion markers passed. Matching logs/hashes archived outside build
in wifi-bbp-batch-d1c0811/native-collection. Ubuntu default, consumed one-shot,
installed payload and desktop recovery verified. Workspace unmounted; clean
167files/499clusters, zero leaked allocations/bitmap mismatches. Run/checkpoint
collected and unarmed. Next consolidate setup using accepted four-pair batches
to free command identifiers before remaining baseband work; sequence reuse,
RF/scan/association/connectivity remain unaccepted.

Active scope: consolidate five accepted four-register plans into sequences4..8
after unchanged firmware/reset/identity/write/timing steps1..3. Preserve MAC
basics -> format -> read-only BBP readiness -> CORE -> IBI/AGC0 -> AGC order.
Each phase uses one40-byte command and all four readbacks; phases2..4 retain
captured CORE identity guards. No new settings or sequence reuse. Touch fixed
MCU engine, xHCI phase ownership, isolated capture/profile and tests. Inspected
main80b567a and Air PR69: independent desktop/battery work, no Wi-Fi dependency;
retain accepted integration22fd266. Native consolidation acceptance pending.

Consolidation validation: full build, five-phase fixed engine and all older
fixed-plan regressions, native xHCI phase/prefix/readiness/read-scope/packet
fixtures, ASan/UBSan, 71 boot profiles and docs checks pass. No native
consolidation acceptance yet; prepare exact-stage VM and automatic cold trial.

Consolidated trial installed: source1b9bf8e, releasee459728ba57fea08. Exact-stage
GPT save/return and missing-workspace refusal/return VM checks pass. Workspace
fsck/allocation clean; Ubuntu default and desktop recoverye47dca3a6b30c6c1
retained; accepted batchingb3eaadb9ad679dbb retained. Older AGCb245ac895a7d9396
removed only after full durable archive equality. Run record
build/bringup/wifi-compact/run.json; payload/receipts and strict five-phase
collector archived in ~/.local/share/srvros/macbookpro11-2/wifi-compact-1b9bf8e.
Unarmed awaiting cold-trial readiness. Collect matching session first after
return; consolidated native acceptance pending. RF/connectivity remain untested.

Consolidated initialization native acceptance (2026-10-23): source1b9bf8e,
releasee459728ba57fea08, session00065e7a55ca88f1-00 passed all five40-byte
commands, exact sequence4..8 replies, status0 and one completion/ACK per phase.
Reads10/10/12/12/12 and all20 baselines/readbacks matched expectations; BBP
readiness occurred between phases1/2, CORE0 stayedf000f200 in phases2..4 and
MAC stayed0. Old phase log families absent; prior firmware/reset/identity/write/
timing checks and both completion markers passed. Logs/hashes archived outside
build in wifi-compact-1b9bf8e/native-collection. Ubuntu default, consumed one-shot,
installed payload and desktop recovery verified. Workspace unmounted; clean
169files/502clusters, zero leaked allocations/bitmap mismatches. Run/checkpoint
collected and unarmed. Next add remaining baseband settings using unused command
identifiers9..15; no sequence recycling or RF/connectivity acceptance.

Active scope: add AGC23/24/25/26 as optional compact phase5, sequence9,
addresses235c/2360/2364/2368 and values272c/2f3a/8000005a/007c2005 from
Linuxv6.18 initial MT7610 settings. All prior phases, exact replies, readbacks
and MAC/CORE guards required. Preserve five-phase v15 with explicit phase
limit; new v16 permits six. Touch fixed MCU plan, native phase ownership,
capture/profile and tests. Fetched main58b354f: Air desktop evidence only since
last inspection; Air PR69 remains independent battery scope. Retain accepted
integration22fd266. No RF or sequence reuse; native added settings pending.

AGC extension validation: full build, six-phase fixed engine and prior
regressions, native xHCI phase/limit/read-scope/transport checks, ASan/UBSan,
72 boot profiles and docs checks pass. Native phase5 acceptance pending.

AGC23..26 trial installed: sourcec9a84ec, release8bf9cd874ed0f02d. Exact-stage
GPT save/return and missing-workspace refusal/return VM checks pass. Workspace
fsck/allocation clean; Ubuntu default and desktop recoverye47dca3a6b30c6c1
retained; accepted consolidatede459728ba57fea08 retained. Older batching
b3eaadb9ad679dbb removed only after full durable archive equality. Run record
build/bringup/wifi-agc-more/run.json; payload/receipts and six-phase collector
archived in ~/.local/share/srvros/macbookpro11-2/wifi-agc-more-c9a84ec. Unarmed
awaiting cold-trial readiness. Collect matching session first after return;
new phase native acceptance pending. RF/connectivity remain untested.

AGC23..26 native acceptance (2026-10-23): sourcec9a84ec,
release8bf9cd874ed0f02d, session00065e7ad65bc92e-00 passed sequence9 sent40/
received16, exact reply, status0/submitted1/completed1/ack_mask1/reads12.
AGC23 changed272f->272c, AGC24 stayed2f3a, AGC25 changed0008005a->8000005a,
AGC26 changed005a2004->007c2005. CORE0 stayedf000f200, MAC stayed0. All24
readbacks, earlier phases, readiness ordering and both completion markers
passed. Matching logs/hashes archived outside build in
wifi-agc-more-c9a84ec/native-collection. Ubuntu default, consumed one-shot,
installed payload and desktop recovery verified. Workspace unmounted; clean
171files/505clusters, zero leaked allocations/bitmap mismatches. Run/checkpoint
collected and unarmed. Next remaining AGC/baseband settings using identifiers
10..15; RF/scan/association/connectivity remain pending.

Active scope: optional compact phase6 for AGC33/34/37/41, sequence10,
addresses2384/2388/2394/23a4 and values3238/000a0c0c/2121262c/38383e45
from Linuxv6.18 initial MT7610 settings. Require preceding six phases and
MAC/CORE boundaries; preserve v15/v16 phase limits, add v17 limit7. Touch
fixed MCU plan, xHCI limits, capture/profile and tests. Fetched mainaddbd02;
PR71 imported file-identity/workload replay fix inspected, outside isolated
Wi-Fi capture; Air PR69 remains battery scope. Retain accepted integration
22fd266 for this trial. No RF or sequence reuse; new settings native pending.

AGC33/34/37/41 candidate b3e4ee1 / releaseff59ebda5a2483ae installed
unarmed. Build, focused MCU/baseband/xHCI tests, sanitizer,73 profile tests
and documentation checks pass. Exact-stage GPT VM saved/unmounted/reset;
missing workspace returned without starting the probe. Payload, VM receipts,
run record and collector archived outside build in wifi-agc-next-b3e4ee1.
Current accepted8bf9cd874ed0f02d and desktope47dca3a6b30c6c1 retained;
obsoletee459728ba57fea08 removed from ESP only after archive equality check.
Workspace preboot filesystem/allocation checks pass, unmounted. Native
acceptance pending an attended cold adapter power cycle; Ubuntu stays default.

AGC33/34/37/41 first return is incomplete: one-shot consumed, but workspace
collection contained no session for releaseff59ebda5a2483ae and no new
session beyond00065e7ad65bc92e-00. Do not accept this phase. Installed
candidate and known-good desktop recovery match their archived bytes;
Ubuntu remains default. Workspace clean:171files/505clusters, zero leaks
or allocation mismatches, then unmounted. Collection archived as
wifi-agc-next-b3e4ee1/incomplete-return-collection; run/checkpoint now
returned/unarmed. Await boot observation to distinguish an early boot or
workspace discovery failure from a skipped test before scheduling a retry.

AGC33/34/37/41 retry accepted on this captured adapter: sourceb3e4ee1,
releaseff59ebda5a2483ae, session00065e8394ee6855-00. User observed SrvrOS
start and return automatically. Phase6 sequence10 sent40/received16 with
exact first-entry reply, status0, one submission/completion, ack_mask1 and
12 reads. AGC33 changed003c->3238; AGC34 stayed000a0c0c; AGC37 stayed
2121262c; AGC41 changed2727303a->38383e45. CORE0 stayedf000f200 and
MAC stayed0. All preceding six phases, firmware/reset/identity/timing,
readiness ordering,28 register readbacks and both completion markers passed.
Collection archived outside build in wifi-agc-next-b3e4ee1/native-collection.
Ubuntu default, consumed one-shot and candidate/desktop recovery verified.
Workspace clean:173files/508clusters, zero leaks/allocation mismatches,
unmounted. Run/checkpoint collected and unarmed. First attempt remains
incomplete with no saved session; its cause is unknown. Next remaining
AGC/baseband settings using unused identifiers11..15; no RF/connectivity
acceptance and no sequence recycling.

Active scope: phase7 AGC57/59/63 and TXC0 at sequence11, fixed offsets
23e4/23ec/23fc/2400 and values1010/baa20e96/1/00280403 from pinned
Linuxv6.18 MT7610 initialization. Require seven preceding phases and
MAC/CORE boundaries; v18 limit8, earlier limits unchanged. Touch fixed MCU
plan, xHCI limits, capture/profile/tests. Fetched main5089172; new PR72
contains only Air6,1 file identity acceptance documentation. Air PR69
remains independent battery work. Actual integrated base remains22fd266.
RF/connectivity and sequence reuse remain outside scope.

AGC57/59/63+TXC0 candidate sourcede17cb6 / releasec34215d727a60f08
installed unarmed. Build, compact/xHCI host tests, sanitizer,74 boot-profile
tests and documentation checks pass. Exact-stage GPT VM saved/unmounted
and reset; missing-workspace VM returned without probing. Payload, VM
receipts, run record and collector archived outside build in
wifi-agc-tail-de17cb6. Acceptedff59ebda5a2483ae and known-good desktop
e47dca3a6b30c6c1 retained. Obsolete8bf9cd874ed0f02d removed from ESP
only after archived-byte equality verification. Workspace clean before
install:173files/508clusters, zero allocation errors; unmounted. Native
acceptance pending cold adapter power cycle; Ubuntu remains default.

AGC57/59/63+TXC0 accepted on captured adapter: sourcede17cb6,
releasec34215d727a60f08, session00065e83ccc7f06a-00. Phase7 sequence11
sent40/received16 with exact reply, status0, one submission/completion,
ack_mask1 and12 reads. AGC57 stayed1010; AGC59 changedbaa20e23->baa20e96;
AGC63 changed0->1; TXC0 stayed00280403. CORE0 stayedf000f200 and MAC0.
All preceding phases, firmware/reset/identity/timing, readiness ordering,
32 register readbacks and both completion markers passed. Matching evidence
archived outside build in wifi-agc-tail-de17cb6/native-collection. Ubuntu
default, consumed one-shot and installed candidate/desktop recovery verified.
Workspace clean:175files/511clusters, zero leaks/allocation mismatches,
unmounted. Run/checkpoint collected and unarmed. Next remaining TXC/RXC
and other baseband initialization; identifiers12..15 unused. RF, scanning
and connectivity remain pending; no sequence recycling accepted.

Active scope: phase8 TXC1 and RXC1/2/3 at sequence12, fixed offsets
2404/2504/2508/250c and values0/12/11/5 from pinned Linuxv6.18 MT7610
initialization. Require all eight accepted phases and MAC/CORE boundaries;
v19 limit9, prior limits unchanged. Touch MCU plan, xHCI limits, capture,
profile/tests. Fetched mainbd1e834; PR73 workload refusal evidence and
ordinary desktop VM changes inspected, independent of Wi-Fi probe path.
Air PR69 remains battery scope. Actual integrated base remains22fd266.
No RF/connectivity or sequence recycling in this trial.

TXC1/RXC1/2/3 candidate sourcedf537ea / release7d6ccc2fe96d530c
installed unarmed. Build, compact/xHCI host tests, sanitizer,75 boot-profile
tests and docs checks pass. Exact-stage GPT VM saved/unmounted/reset;
missing-workspace VM returned without probing. Payload, receipts, run record
and collector archived outside build in wifi-rxc-start-df537ea. Accepted
c34215d727a60f08 and desktop recoverye47dca3a6b30c6c1 retained. Obsolete
ff59ebda5a2483ae removed from ESP only after archive equality verification.
Workspace clean before install:175files/511clusters, zero allocation errors,
unmounted. Native acceptance pending cold adapter power cycle; Ubuntu default.

TXC1/RXC1/2/3 accepted on captured adapter: sourcedf537ea,
release7d6ccc2fe96d530c, session00065e84abf0e3e4-00. Phase8 sequence12
sent40/received16 with exact reply, status0, one submission/completion,
ack_mask1 and12 reads. TXC1 stayed0; RXC1 changed14->12; RXC2 stayed11;
RXC3 stayed5. CORE0 stayedf000f200 and MAC0. All earlier phases,
firmware/reset/identity/timing, readiness ordering,36 register readbacks
and both completion markers passed. Evidence archived outside build in
wifi-rxc-start-df537ea/native-collection. Ubuntu default, consumed one-shot
and installed candidate/desktop recovery verified. Workspace clean:
177files/514clusters, zero leaks/allocation mismatches, unmounted.
Run/checkpoint collected and unarmed. Next remaining RXC/baseband settings
using identifiers13..15; no sequence reuse, RF or connectivity acceptance.

Active scope: phase9 RXC4/5/7 and TXO8 at sequence13, fixed offsets
2510/2514/251c/2620 and values0/f977c4ec/90/0 from pinned Linuxv6.18
MT7610 initialization. Require nine prior phases and MAC/CORE boundaries;
v20 limit10, earlier limits preserved. Touch MCU plan, xHCI limits,
capture/profile/tests. Refetched mainbd1e834 unchanged; Air PR69 battery
and Air6,1 desktop evidence remain separate. Actual integrated base22fd266.
No RF, connectivity or command identifier recycling. Native result pending.

RXC4/5/7+TXO8 candidate sourcefd54365 / release492306b5fccc17b9
installed unarmed. Build, compact/xHCI host tests, sanitizer,76 boot-profile
tests and docs checks pass. Exact-stage GPT VM saved/unmounted/reset;
missing-workspace VM returned without probing. Payload, receipts, run record
and collector archived outside build in wifi-rxc-tail-fd54365. Accepted
7d6ccc2fe96d530c and desktop recoverye47dca3a6b30c6c1 retained. Obsolete
c34215d727a60f08 removed from ESP only after archive equality verification.
Workspace clean before install:177files/514clusters, zero allocation errors,
unmounted. Native acceptance pending cold adapter power cycle; Ubuntu default.

RXC4/5/7+TXO8 accepted on captured adapter: sourcefd54365,
release492306b5fccc17b9, session00065e84c6495273-00. Phase9 sequence13
sent40/received16 with exact reply, status0, one submission/completion,
ack_mask1 and12 reads. RXC4 stayed0; RXC5 changed71550400->f977c4ec;
RXC7 changed0->90; TXO8 stayed0. CORE0 stayedf000f200 and MAC0.
All preceding phases, firmware/reset/identity/timing, readiness ordering,
40 register readbacks and both completion markers passed. Evidence archived
outside build in wifi-rxc-tail-fd54365/native-collection. Ubuntu default,
consumed one-shot and installed candidate/desktop recovery verified.
Workspace clean:179files/517clusters, zero leaks/allocation mismatches,
unmounted. Run/checkpoint collected and unarmed. Remaining TXBE/RXFE/RXO
and other baseband settings pending; identifiers14..15 unused. Further
expansion must respect sequence exhaustion. RF/connectivity not accepted.

Active scope: phase10 TXBE0/4/6/8 at sequence14, fixed offsets
2700/2710/2718/2720 and values0/4/0/14 from pinned Linuxv6.18 MT7610
initialization. Require ten previous phases and MAC/CORE guards; v21 limit11,
prior limits unchanged. Touch MCU plan, xHCI limits, capture/profile/tests.
Fetched mainf5e1525; PR69 opt-in Home battery startup and PR74 Air6,1
evidence inspected. Default battery remains off; those desktop paths are
independent of this isolated capture. Retain integrated base22fd266. Preserve
shared desktop recipes on future integration. No RF or identifier recycling.

TXBE0/4/6/8 candidate source56e1f4a / release6477738b1d3cf7d5
installed unarmed. Build, compact/xHCI host tests, sanitizer,77 boot-profile
tests and docs checks pass. Exact-stage GPT VM saved/unmounted/reset;
missing-workspace VM returned without probing. Payload, receipts, run record
and collector archived outside build in wifi-txbe-start-56e1f4a. Accepted
492306b5fccc17b9 and desktop recoverye47dca3a6b30c6c1 retained. Obsolete
7d6ccc2fe96d530c removed from ESP only after archive equality verification.
Workspace clean before install:179files/517clusters, zero allocation errors,
unmounted. Native acceptance pending cold adapter power cycle; Ubuntu default.

TXBE0/4/6/8 accepted on captured adapter: source56e1f4a,
release6477738b1d3cf7d5, session00065e85c7b7d806-00. Phase10 sequence14
sent40/received16 with exact reply, status0, one submission/completion,
ack_mask1 and12 reads. TXBE0 stayed0; TXBE4 changed0->4; TXBE6 changed1->0;
TXBE8 changed4014->14. CORE0 stayedf000f200 and MAC0. All prior phases,
firmware/reset/identity/timing, readiness ordering,44 register readbacks
and both completion markers passed. Evidence archived outside build in
wifi-txbe-start-56e1f4a/native-collection. Ubuntu default, consumed one-shot
and installed candidate/desktop recovery verified. Workspace clean:
181files/520clusters, zero leaks/allocation mismatches, unmounted.
Run/checkpoint collected and unarmed. Remaining TXBE/RXFE/RXO settings
pending; identifier15 remains unused. Further expansion must address
sequence exhaustion without recycling identifiers. RF/connectivity pending.

Active scope: phase11 TXBE9/10/12/13 at sequence15, fixed offsets
2724/2728/2730/2734 and values20000000/0/0/0 from pinned Linuxv6.18
MT7610 initialization. Require eleven prior phases and MAC/CORE guards;
v22 limit12, prior limits unchanged. This exhausts identifiers: compile-time
plan bound and runtime refusal forbid phase12/sequence16. Touch fixed MCU
plan, xHCI limits, capture/profile/tests. Fetched maind3670fe: Air6,1
exact-profile PWM capture and preference evidence inspected; PR76 owns
Home services/cached battery UI. No Wi-Fi overlap; retain base22fd266.
No RF or identifier recycling. Remaining setup needs validated wider batches
or a proven-drained new session before more commands can be added.

TXBE9/10/12/13 candidate source5d5488c / release9cc1888913aa7ff5
installed unarmed. Build, compact/MCU/xHCI host tests, sanitizer,78 boot-profile
tests and docs checks pass, including identifier exhaustion refusal. Exact-stage
GPT VM saved/unmounted/reset; missing-workspace VM returned without probing.
Payload, receipts, run record and collector archived outside build in
wifi-txbe-next-5d5488c. Accepted6477738b1d3cf7d5 and desktop recovery
e47dca3a6b30c6c1 retained. Obsolete492306b5fccc17b9 removed from ESP
only after archive equality verification. Workspace clean before install:
181files/520clusters, zero allocation errors, unmounted. Native acceptance
pending cold adapter power cycle; Ubuntu remains default.

TXBE9/10/12/13 native trial FAILED readback: source5d5488c,
release9cc1888913aa7ff5, session00065e85ec8dd3b8-00. Earlier eleven
phases and44 readbacks passed. Phase11 sequence15 sent40/received16 with
exact first-entry reply and ack_mask1; TXBE9 read0->20000000. TXBE10
read37dff before and38e30 after, not requested0. Engine returned status4
(MT76_TIMING_STATE), reads8, batches_completed0, complete0. TXBE12/13
and final MAC/CORE reads were not reached; their zero log fields are not
observations. Both capture completion markers are NO. No native acceptance
of phase11, no retry/recycled identifier. Failure logs archived outside build
in wifi-txbe-next-5d5488c/failed-native-collection. Ubuntu default, consumed
one-shot and installed payload/recovery verified. Workspace clean:
183files/523clusters, zero leaks/allocation mismatches; unmounted.
Run/checkpoint collected failed/unarmed. Next bounded read-only TXBE10
sampling to distinguish changing status/counter behavior from a write failure;
keep strict gates until evidence establishes semantics. No new reboot armed.

Active scope: read-only TXBE9/10/12/13 observation after the eleven accepted
compact phases. Eight fixed samples bracketed by MAC/CORE guards, no phase11
submission, retry or relaxed write verification. Touch pure MCU diagnostic,
xHCI ownership, capture/profile and tests. Main83a3f59 inspected (Air6,1
brightness documentation); PR76 Home services and PR79 Air6,1 brightness
remain separate. Integrated base22fd266. Native register semantics unknown.

Read-only TXBE observation candidate sourceffa4e17 / release70e0236472c320bc
installed unarmed. Build, compact/MCU/xHCI tests, sanitizer,79 boot-profile
checks and docs checks pass. Collector accepts a synthetic eight-sample receipt
and rejects nine incomplete/corrupt variants; this is not native evidence.
Exact-stage GPT VM saved/unmounted/reset; missing-workspace VM returned without
probing. Payload, receipts, run record and collector archived outside build in
wifi-txbe-observe-ffa4e17. Desktop recoverye47dca3a6b30c6c1 retained;
failed9cc1888913aa7ff5 remains installed for comparison. Accepted6477738b1d3cf7d5
removed from ESP only after archive equality verification. Workspace clean:
183files/523clusters, zero allocation errors, unmounted. Native observation
pending cold adapter power cycle; Ubuntu default and no reboot armed.

Read-only TXBE capture completed: sourceffa4e17, release70e0236472c320bc,
session00065e86348e7c75-00. All eleven compact phases/44 readbacks passed;
observation status0 attempted1 complete1 reads64 samples8. TXBE10 stayed
00037fff over2823351..2825238us (1.887ms); TXBE9/12/13 stayed0. MAC0 and
COREf000f200 held at all sixteen sample boundaries. Exactly eleven compact
commands; no phase11/sequence15. Both completion markers passed. No evidence
of spontaneous change in this short window; register semantics and the earlier
write mismatch remain unresolved, and phase11 is not accepted. Next audit
register mapping and write/read transport before choosing a further write trial.
Evidence archived in wifi-txbe-observe-ffa4e17/native-collection outside build.
Ubuntu default, consumed one-shot, installed payload and desktop recovery
verified. Workspace clean:185files/526clusters, zero leaks/mismatches/errors;
unmounted. Run/checkpoint collected and unarmed. RF/connectivity not tested.

TXBE mapping/transport audit completed without kernel or installed-payload changes.
Pinned Linux v6.18 definitions independently reconstruct all40 phase11 bytes;
USB request/index/value and MCU base/little-endian pairs agree. Compact/MCU tests
pass. No mapping defect found; Linux completion checking does not establish exact
readback semantics. Selected next bounded scope is TXBE9-only sequence15 with
before/after guarded samples after phases0..10, leaving TXBE10/12/13 unwritten.
Implementation and native evidence pending; do not relax failed v22 verification.
Fetched mainab99517 (merged Air6,1 brightness PR79); inspected shared staging
changes, preserve v56/basic brightness on future integration. PR76 remains Home
services/battery, PR19 owns Wi-Fi. Actual integrated base22fd266 unchanged.
Local audit receipts in build/bringup/txbe-audit, archived outside build.
Current70e0236472c320bc and recoverye47dca3a6b30c6c1 unchanged; no boot armed.

Active scope: implement TXBE9-only comparison after eleven accepted phases,
eight guarded samples on either side of one sequence15 write. No writes to
TXBE10/12/13, no identifier reuse, no v22 gate changes. Touch MCU/xHCI,
main capture, staging and tests. Mainab99517/PR76 inspected in preceding audit;
actual integrated base22fd266. Shared Air desktop profiles stay separate.

TXBE9-only comparison sourcef1785c6 / releaseb3a29375623082db installed
unarmed. Build, MCU/compact/xHCI tests, sanitizer,80 boot-profile tests and docs
checks pass. Collector validated with synthetic16 samples and12 rejection
fixtures. Exact-stage GPT VM saved/unmounted/reset; missing-workspace VM returned
without probing. Payload/receipts/run/collector archived outside build in
wifi-txbe-first-f1785c6. Accepted observation70e0236472c320bc and desktop
recoverye47dca3a6b30c6c1 retained. Failed9cc1888913aa7ff5 removed from ESP only
after archive equality verification. Workspace clean:185files/526clusters,
zero leaks/mismatches/errors; unmounted. Ubuntu default, no reboot armed.
Native result pending cold adapter power cycle; collect matching session first.

TXBE9-only comparison accepted on captured adapter: sourcef1785c6,
releaseb3a29375623082db, session00065e87131ed81c-00. Exact sequence15 reply,
sent16/received16, status0 attempted1 write_attempted1 submitted1 ACK1 complete1.
Both stages completed64 reads/eight samples. Before: TXBE9=0, TXBE10=37fff
through2836302..2839177us. After: TXBE9=20000000, TXBE10=38e30 through
2839544..2843407us. TXBE12/13 stayed0; all MAC0/COREf000f200 boundaries held.
Eleven compact phases/44 readbacks and both completion markers passed. The
TXBE9-only write reproduced the failed quartet's final TXBE10 value without
writing TXBE10; its semantics and original write result remain unresolved.
Next isolate TXBE10=0 with TXBE9 unchanged, retaining strict v22 verification.
Evidence archived outside build in wifi-txbe-first-f1785c6/native-collection.
Ubuntu default, consumed one-shot, installed payload/recovery verified. Workspace
clean:187files/529clusters, zero leaks/mismatches/errors; unmounted. Run/checkpoint
collected and unarmed. No further trial installed; RF/connectivity not tested.

Active scope: TXBE10-only zero write after phases0..10, sequence15 once,
TXBE9 required0 before/after, guarded samples and strict zero readback. No
phase11 quartet or retry. MCU/xHCI/main/staging/tests; integrated base22fd266.
Previous mainab99517/PR76 audit remains the coordination baseline. Native pending.
Fetched mainb99bb5b: merged PR76 Home services/battery UI and Air6,1 audio
changes inspected. They do not change this isolated Wi-Fi transport. PR83 owns
attended audio/Ethernet/secondary-input acceptance. Preserve desktop v55/56/57
and service dispatch on future integration; retain actual base22fd266 for this
comparison against the accepted captures.

TXBE10-only candidate sourcec96e810 / releasef8b4b41d736bd4e2 installed
unarmed. Build, compact/MCU/xHCI tests, sanitizer,81 boot-profile tests and docs
checks pass. Collector accepts synthetic16 samples and rejects14 corrupted
fixtures, including nonzero target readback and changed TXBE9. Exact-stage GPT
VM saved/unmounted/reset; missing-workspace VM returned without probing.
Payload/receipts/run/collector archived outside build in wifi-txbe-second-c96e810.
Accepted TXBE9 comparisonb3a29375623082db and desktop recoverye47dca3a6b30c6c1
retained. Observation70e0236472c320bc removed from ESP only after archive equality
verification. Workspace clean:187files/529clusters, zero allocation errors;
unmounted. Ubuntu default, no reboot armed. Await cold adapter power cycle;
collect matching session first and preserve failed readbacks as failures.

TXBE10-only comparison collected with a failed readback: sourcec96e810,
releasef8b4b41d736bd4e2, session00065e87382297d7-00. All eleven compact
phases/44 readbacks passed; sequence15 sent16/received16 with exact reply and
ACK1, but status4 complete0. Both stages completed64 reads/eight samples.
TXBE10 remained37fff before2867128..2873127us and after2873493..2875377us;
TXBE9/12/13 stayed0, all MAC0/COREf000f200 boundaries held. Both capture
completion markers NO. Unlike the TXBE9-only comparison, this isolated zero
write produced no observed TXBE10 change. Register semantics versus write/access
behavior remains unresolved; no relaxed gate or further initialization accepted.
Next scope is auditing that distinction before selecting another hardware trial.
No shared source changes in this collection. RF/connectivity not tested.
Evidence and failure-specific collector archived outside build in
wifi-txbe-second-c96e810/failed-native-collection. Ubuntu default, consumed
one-shot, installed payload and desktop recovery hashes verified. Workspace
clean:189files/532clusters, zero leaks/mismatches/errors; unmounted.
Run/checkpoint collected with failure recorded and no reboot armed.

TXBE10 access-path audit completed: Linux v6.18 MCU and vendor USB transport
agree with the compiled address/byte encoding; an older vendor-source mirror
corroborates the numeric table entry but supplies no readback semantics. Host
independent packet reconstruction and MCU/compact regressions pass. Receipts
in txbe-access-audit, archived outside build under wifi-txbe-second-c96e810.
Selected next bounded scope: one command10 TXBE10 MCU read at sequence15 after
phases0..10, bracketed by guarded EP0 observations, without a diagnostic write,
retry, identifier reuse or relaxed initialization gate. Native result pending;
no candidate installed in this audit. Currentf8b4b41d736bd4e2 and desktop
recoverye47dca3a6b30c6c1 unchanged, no reboot armed. Mainb99bb5b and PR83
attended-device scope rechecked; actual integrated base22fd266 unchanged.

Active scope: implement the selected MCU-versus-EP0 TXBE10 read comparison.
One command10 at unused sequence15, guarded before/after observations, exact
response framing and address, explicit mismatch reporting. Touch MCU/xHCI,
capture/profile and tests; no diagnostic register write or phase11 acceptance.
Dependencies remain the eleven accepted phases and pinned firmware. Current
mainb99bb5b/PR83 scope inspected; integrated base22fd266 retained for comparison.

MCU/EP0 TXBE10 comparison sourceb4417eb / releaseeb362bcf6da9fdfb installed
unarmed. Build, MCU/compact/xHCI tests, ASan/UBSan,82 boot-profile tests and docs
checks pass. Collector accepts synthetic agreement and rejects14 corrupted or
conflicting receipts. Exact-stage GPT VM saved/unmounted/reset; missing-workspace
VM returned without starting the probe. Payload, receipts, run record and collector
archived outside build in wifi-txbe-access-b4417eb. Desktop recoverye47dca3a6b30c6c1
and failed v25f8b4b41d736bd4e2 retained. Accepted TXBE9 comparisonb3a29375623082db
removed from ESP only after full archived-payload equality verification.
Workspace clean:189files/532clusters, zero allocation errors; unmounted.
Ubuntu remains default. Native comparison awaits cold adapter power cycle;
collect the matching new session first on return. No reboot armed.

Native MCU/EP0 TXBE10 comparison accepted: sourceb4417eb,
releaseeb362bcf6da9fdfb, session00065e876ba24dc3-00. Sequence15 exact16-byte
command/reply, status0 reply_validated1 complete1; MCU value37fff matched all16
EP0 samples,64 reads/eight samples per side. Before2882205..2885095us,
after2885446..2889299us; TXBE9/12/13 stayed0 and all MAC0/COREf000f200
boundaries held. Eleven prerequisite phases/44 readbacks and both completion
markers passed. This accepts only baseline read-path agreement; the v22/v25
write failures and RF/connectivity remain unaccepted. Next investigate write
behavior/register semantics before selecting another physical trial.
Evidence archived outside build in wifi-txbe-access-b4417eb/native-collection.
Ubuntu default, consumed one-shot and installed/recovery payload hashes verified.
Workspace clean:191files/535clusters, zero leaks/mismatches/errors; unmounted.
Run/checkpoint collected and unarmed. Fetched maind3875c7 (Air6,1 audio PR84);
actual integrated base22fd266 remains unchanged for these comparisons.

Active scope: isolate the TXBE10 write transport with one EP0 request6 zero
write after phases0..10 and guarded baseline samples, followed by one MCU read
at unused sequence15 and guarded EP0 samples. Exact address/value, single attempt,
no retry, TXBE9zero, MAC0/COREf000f200; nonzero readback remains failure.
Linux v6.18 usb.c confirms request6, local index2728, value0, four LE payload
bytes. Touch MCU/xHCI/main/profile/tests. Main nowd2d56a6 (Air6,1 audio work),
PR83 attended acceptance inspected; actual base22fd266 unchanged. No native
write or new candidate installed yet.

Isolated EP0 TXBE10 zero-write candidate sourcee821968 / releaseacaea56aaa88d52f
installed unarmed. Build, compact/MCU/xHCI tests, ASan/UBSan,83 boot-profile
tests and docs checks pass. Collector accepts synthetic zero readback and rejects17
corrupt/conflicting receipts. Exact-stage GPT VM saved/unmounted/reset;
missing-workspace VM returned without starting the probe. Payload, receipts,
run record and collector archived outside build in wifi-txbe-direct-e821968.
Accepted read comparisoneb362bcf6da9fdfb and desktop recoverye47dca3a6b30c6c1
retained. Failed v25f8b4b41d736bd4e2 removed from ESP only after full archived
payload equality verification. Workspace clean:191files/535clusters, zero
allocation errors; unmounted. Ubuntu default and no reboot armed. Await cold
adapter power cycle; collect the matching new session first on return.

Native direct TXBE10 zero-write readback failed: sourcee821968,
releaseacaea56aaa88d52f, session00065e87a87d976c-00. EP0 index2728 zero transfer
completed actual4; sequence15 MCU read returned37fff with exact framing/address.
Eleven prerequisite phases/44 readbacks passed. Comparison status4 complete0,
reply_validated1; both stages64 reads/eight samples. Before3106897..3108802us
and after3109221..3115210us, TXBE10 stayed37fff; MCU matched all eight post-write
samples. TXBE9/12/13 remained0, all MAC0/COREf000f200 boundaries held. Both
capture completion markers NO. A second write path reproduced the mismatch;
transport completion does not accept the register write. Next audit semantics
and initialization prerequisites; no relaxed gate, RF or phase11 acceptance.
Evidence archived outside build in wifi-txbe-direct-e821968/failed-native-collection.
Ubuntu default, consumed one-shot and installed/recovery hashes verified.
Workspace clean:193files/538clusters, zero leaks/mismatches/errors; unmounted.
Run/checkpoint collected with failure recorded; no reboot armed.

Post-v27 prerequisite audit found missing PBF/MAC clock setup: Linux v6.18
sets0400=80c00 before baseband initialization; current code only observes this
register. Five native v23..v27 readiness receipts consistently show80000,
missing bits11:10 (0c00), with MAC_SYS_CTRL0. Existing CORE readiness does not
establish clock enablement. Receipt/script/source and log hashes archived in
txbe-prerequisite-audit; reference ordering and all five receipts verified.
Causality remains unproven and other MAC-table omissions remain separate.
Selected next scope: exact80000->80c00 EP0 transition after MAC phases0/1,
before baseband readiness, with disabled/idle MAC checks, one attempt and exact
readback; then existing guarded phases and read-only TXBE10 comparison. No new
TXBE write, MCU sequence reuse, relaxed CORE gate or RF enable. Implementation
pending. No installed payload changed; currentacaea56aaa88d52f and recovery
e47dca3a6b30c6c1 retained, no reboot armed. Maind2d56a6 and PR83 attended-device
scope inspected; integrated base22fd266 unchanged. Shared implementation scope
would be MCU/xHCI/main/profile/tests; current change is audit documentation only.

Active implementation scope: opt-in PBF/MAC clock prerequisite before compact
phase2, followed by the existing read-only TXBE10 comparison. Exact baseline,
fixed one-time EP0 write and readback, MAC disabled/idle before and after;
no TXBE write or change to historical profiles. Radio/MCU/xHCI/main/staging/tests.
Fetched maind2d56a6; PR83 remains attended acceptance. Actual base22fd266.

Clock prerequisite candidate source1b3e323 / release67f92f9c2a4d9365 installed
unarmed. Build, radio/compact/MCU/xHCI tests, ASan/UBSan,84 profile tests and docs
checks pass. Collector accepts synthetic clock/read agreement and rejects17
corrupted, missing or out-of-order receipts. Exact-stage GPT VM saved/unmounted/
reset; missing-workspace VM returned without starting the probe. Payload,
receipts, run record and collector archived outside build in wifi-clock-1b3e323.
Failed direct-writeacaea56aaa88d52f and desktop recoverye47dca3a6b30c6c1 retained.
Accepted read comparisoneb362bcf6da9fdfb removed from ESP only after full archived
payload equality verification. Workspace clean:193files/538clusters, zero
allocation errors; unmounted. Ubuntu default, no reboot armed. Native clock
transition and subsequent comparison await a cold adapter power cycle; collect
matching session first after return. Existing TXBE write failures remain failures.


Native clock prerequisite accepted: source `1b3e323`, release
`67f92f9c2a4d9365`, session `00065e87eb5deca5-00`. Clock status0 complete1,
six reads, PBF80000->80c00, MAC disabled/idle before and after; readiness
retained80c00. Eleven compact phases/44 target readbacks passed. Sequence15
MCU read matched all16 EP0 samples at TXBE10=37fff; TXBE9/12/13 stayed0,
MAC0/COREf000f200 guards held. Before2866911..2872953us,
after2873320..2876195us. Both completion markers YES. This accepts the clock
prerequisite and read comparison only; no TXBE write or RF/connectivity acceptance.
Next bounded scope is an isolated TXBE10 zero-write comparison with clocks enabled,
keeping the exact zero-readback gate and historical failures intact.

Evidence archived outside build in wifi-clock-1b3e323/native-collection.
Ubuntu default, consumed one-shot and installed/recovery payload hashes verified.
Workspace clean:195files/541clusters, zero leaks/mismatches/errors; unmounted.
Run/checkpoint collected and unarmed. Fetched maindf44fa0 (Air6,1 audio endurance);
PR87 telemetry and PR83 attended acceptance remain separate scopes. Actual
integrated base22fd266 unchanged. No new candidate staged or reboot armed.

Active scope: separate v29 clock-plus-direct-write comparison, reusing the accepted
clock engine and isolated EP0 TXBE10 writer. Require clock completion before
access; one zero write, sequence15 MCU read and eight guarded post-write samples
must read zero. Touch xHCI/main/profile/tests; dependencies are the accepted
v28 prerequisite and eleven compact phases. Main df44fa0, PR83/87 inspected;
actual base22fd266 retained for this comparison. No reboot armed.

Clock-enabled isolated TXBE10 write candidate source `492fb20`, release
`19f40e64c606d997` installed unarmed. Build, radio/compact/MCU/xHCI tests,
xHCI ASan/UBSan, 85 boot-profile tests and documentation checks passed. The
collector accepts synthetic zero readback and rejects21 missing/corrupted or
conflicting receipts, including clock failures. Exact-stage GPT VM saved,
unmounted and reset; missing-workspace VM returned without starting the probe.
Payload, receipts, helpers and run record archived outside build in
wifi-clock-direct-492fb20. Accepted clock67f92f9c2a4d9365 and desktop recovery
e47dca3a6b30c6c1 retained. Failed direct-writeacaea56aaa88d52f removed from ESP
only after complete archived-payload equality verification. Workspace clean:
195files/541clusters, zero allocation errors; unmounted. Ubuntu remains default.
Await cold adapter power cycle; collect the exact new session first on return.
No reboot armed, no native write acceptance yet.


Native clock-enabled TXBE10 zero-write failed: source `492fb20`, release
`19f40e64c606d997`, session `00065e8821c93f58-00`. Clock status0 complete1,
PBF80000->80c00 retained at readiness; eleven phases/44 readbacks passed.
EP0 zero-write transport completed actual4. Sequence15 exact MCU reply returned
37fff; comparison status4 complete0 reply_validated1, matches8 expected8.
All16 EP0 samples retained37fff; TXBE9/12/13 zero, MAC0/COREf000f200 held.
Before3096597..3100465us, after3100884..3104735us. Both completion markers NO.
Clock enablement did not resolve this mismatch. Next audit remaining prerequisites
and register semantics; do not relax zero-readback or accept phase11/RF.

Failure archived outside build in wifi-clock-direct-492fb20/failed-native-collection;
separate failure collector retains the original success predicate, which rejects
this receipt. Ubuntu default, consumed one-shot and installed/recovery hashes
verified. Workspace clean:197files/544clusters, zero leaks/mismatches/errors;
unmounted. Run/checkpoint collected as failed and unarmed. Fetched main36dbc3d
(Air6,1 telemetry); PR88 two-finger tap scope is separate. Actual integrated
base22fd266 retained; no new candidate or reboot armed.


Post-v29 audit captured eight read-only Linux observations from this exact
7392:a711 adapter through mt76x0u debugfs. Kernel7.0.0-38-generic: TXBE10=0,
TXBE9=20000000, PBF80c00, COREf000f200, MAC0c; TXBE12/13zero. MAC_STATUS0
except one2. Selector restored; no register write, reset, unbind or connection
change. Fully running Linux is not equivalent to SrvrOS disabled initialization.
Numeric v6.18/v7.0 MAC/BBP tables match; TXBE9 precedes TXBE10. Additional
MAC-table/EXT_CCA/FCE/WMM setup remains absent, with causality unproven.

Next bounded implementation: accepted clocks plus TXBE9=20000000 followed by
TXBE10=0, retaining disabled MAC and strict readback. v22 lacks clocks; v29
keeps TXBE9zero, so neither tests this combination. Historical failures stand.
Audit script checks eight Linux observations and six native trials; script,
source hashes and raw receipt archived outside build in txbe-linux-audit-9d22c95.
Current19f40e64c606d997/recoverye47dca3a6b30c6c1 unchanged; no reboot armed.
Fetched main36dbc3d and inspected separate PR88 gesture scope. Integrated
base22fd266 unchanged; this work changes audit documentation only.

Active implementation scope: explicit clock-enabled TXBE9/TXBE10 ordered pair.
Reuse compact phases0..10, accepted clocks before BBP, then one command12
sequence15 containing only2724=20000000 followed by2728=0. Require both target
readbacks and MAC0/COREf000f200 before/after. No TXBE12/13 writes, sequence
reuse or RF enable. Touch MCU/xHCI/main/staging/tests; depend on accepted v28
clock setup and retain historical profiles. No reboot armed.
Fetched mainf310923 (Air6,1 telemetry acceptance); open PR90 passive battery
preflight and PR88 gestures are separate scopes. Actual integrated base22fd266.

Clock-enabled ordered-pair candidate source `0754633`, release
`29504434a7bc5aa6` installed unarmed. Build, radio/MCU/MAC/compact/xHCI tests,
compact ASan/UBSan, 86 boot-profile tests and documentation checks passed.
The collector accepts synthetic pair completion and rejects15 corrupt/conflicting
receipts. Exact-stage GPT VM saved/unmounted/reset; missing-workspace VM returned
without starting the probe. Payload, receipts, helpers and run record archived
outside build in wifi-clock-pair-0754633. Failed clock-direct19f40e64c606d997
and desktop recoverye47dca3a6b30c6c1 retained. Accepted clock67f92f9c2a4d9365
removed from ESP only after complete archived-payload equality verification.
Workspace clean:197files/544clusters, zero allocation errors; unmounted.
Ubuntu remains default. Await cold adapter power cycle; collect the exact new
session first on return. No reboot armed or native ordered-pair acceptance yet.


Native clock-enabled ordered pair failed: source `0754633`, release
`29504434a7bc5aa6`, session `00065e8878bfdf27-00`. Clock80000->80c00 and
readiness passed; phases0..10/44 target readbacks passed. Phase11 sequence15
sent24 received16 exact first-pair ACK. TXBE9 before0 after20000000; TXBE10
before37fff after38e30 instead of0. Status4 ack1 batches_completed0 complete0
reads6. Final MAC/CORE guards not reached; their zero fields are not evidence.
Both completion markers NO. Next investigate remaining initialization/state
differences from Linux; no relaxed gate or TXBE12/13/RF acceptance.

Failure archived outside build in wifi-clock-pair-0754633/failed-native-collection.
Separate failure collector verifies this receipt; unchanged success checker rejects
it. Ubuntu default, consumed one-shot and installed/recovery hashes verified.
Workspace clean:199files/547clusters, zero leaks/mismatches/errors; unmounted.
Run/checkpoint collected as failed, no reboot armed. Fetched maind5491c0
(Air6,1 battery preflight); PR88 tap diagnostics remains separate. Actual
integrated base22fd266 unchanged; no new candidate installed.


Post-v30 state audit: Linux sends the58-entry BBP table as24/24/10 pairs, only
last chunk acknowledged. TXBE10 is index37, followed by20 entries; upstream does
not perform our immediate target readback. Two read-only live snapshots of66
registers confirm TXBE10zero and nine runtime values differing from the initial
BBP table. Live Linux MAC0c/channel/calibration state differs from SrvrOS; no
causal claim or acceptance follows. Selector restored, no device writes/reset.

Selected next scope is a dedicated full-table observation diagnostic following
accepted MAC phases/clocks, fixed200/200/88-byte packets with seq0/0/6, final ACK
and post-table snapshots. Keep disabled-MAC/CORE guards, no RF/channel/MAC enable,
private fixed-packet no-response admission, explicit mismatch accounting and
historical strict tests unchanged. Do not equate snapshot completion with target
acceptance. MAC-table omissions remain uncontrolled. Implementation is pending.
Audit and proposed unsent packet fixtures archived in txbe-state-audit-e829cf1
outside build. Current29504434a7bc5aa6/recoverye47dca3a6b30c6c1 unchanged;
no reboot armed. Fetched main9bbc3b9 (Air6,1 preflight return), inspected separate
PR88 tap diagnostics; actual integrated base22fd266 retained. Documentation only.

Active scope: implement the separate full-BBP observation diagnostic with fixed
24/24/10 entry packets, seq0/0/6 and only final ACK. Snapshot all58 targets with
MAC0/COREf000f200 boundaries; mismatches are reported separately from completed
capture and never imply accepted Wi-Fi initialization. No RF/MAC enable. Touch
MCU/xHCI/main/staging/tests; reuse accepted reset/timing/MAC/clock prerequisites.

Implementation uses only MAC phases0/1 before clocks/readiness, then three full-table
commands and124 reads. Latest fetched main9bbc3b9 remains outside actual base22fd266.
PR92 Air6,1 battery sampling and PR88 tap diagnostics are separate active scopes.
Local packet/collector/sanitizer checks pass; exact-stage validation pending.

Full-table observation candidate installed: source `eb832da`, release
`4316c9963cb7ae6a`, firmware profile v31. Host build, MCU/compact/radio/MAC/xHCI
regressions, 87 staging tests, sanitizer fault coverage, independent488-byte
packet comparison and collector mismatch/rejection fixtures pass. Exact-stage
GPT absent-radio save/unmount/reset and missing-workspace return VM checks pass.
Physical table results remain pending; capture completion will not accept register
values or RF/connectivity. Candidate and evidence archived outside build in
wifi-bbp-table-eb832da. Previous known-good desktope47dca3a6b30c6c1 retained;
Ubuntu default unchanged. Workspace preflight fsck/allocation clean, unmounted.
Run record and strict observation collector ready. No one-shot armed; await cold
adapter power cycle, then collect matching session before further changes.

Native full-table observation completed: source `eb832da`, release
`4316c9963cb7ae6a`, session `00065e88f3984844-00`. Firmware/reset/timing,
MAC phases0/1, clock/readiness, all three200/200/88-byte commands (seq0/0/6)
and final ACK passed. All124 reads and four disabled-MAC/COREf000f200 boundaries
completed. Both capture markers yes; this accepts the observation workflow only.
Four mismatches remain: TXBE10/2728 before37fff after38e30 expected0;
TXBE17/2744 before/afterfb3fec3c expected0; RXO21/2954 before/after0 expected1;
RXO24/2960 before/after0 expected6. Other54 table values matched. Full-table
batching did not resolve TXBE10. Register initialization and RF/connectivity
remain unaccepted; no cause is established by this comparison.

Matching logs archived in wifi-bbp-table-eb832da/native-collection outside build.
Collector checks passed after fixing a local Python variable that shadowed the
hardware_boot module; no evidence predicates changed. Ubuntu default, consumed
one-shot and current/recovery hashes verified. Workspace clean:201files/550clusters,
zero leaks/mismatches/errors, unmounted. No reboot armed. Next audit the four
mismatching registers and remaining reference initialization prerequisites before
choosing another bounded trial; retain all earlier strict failure results.

Active audit after v31: compare the four mismatches with pinned Linux v7.0
initialization, later BBP tables and saved live-driver observations; inventory
omitted MAC prerequisites before selecting another native write trial. Scope is
source/evidence analysis and local numeric table fixtures, no device reset/write.
Fetched main90a1db8 includes PR92 Air6,1 isolated battery runtime; actual integrated
base22fd266 unchanged. PR88 tap diagnostics remains separate. Current/recovery
and unarmed boot state remain unchanged.

Post-v31 audit complete: RXO21/24 also read0 in both saved running-Linux samples;
TXBE17 readsff3fef3c in Linux versusfb3fec3c here, and TXBE10 reads0 versus38e30.
Neither later band-switch table nor DCOC table explicitly writes those four
addresses. This does not establish register semantics or eliminate indirect
calibration/state effects. Pinned Linux v7.0 MAC tables contain66 ordered writes
to60 distinct addresses; current reset/timing/MAC/clock setup matches the final
values at12 addresses, leaving48 table addresses unapplied. This is an inventory,
not a causal finding or authorization to blindly write power/PA/LDO settings.

Next bounded diagnostic selected: read-only prerequisite inventory of the60 MAC
table addresses plus EXT_CCA/FCE_L2/WMM before the full BBP observation, retaining
MAC-disabled/CORE guards. Use observed initial values to choose a justified write
set. No new candidate installed or reboot armed. Audit scripts, numeric C-extracted
MAC inventory, exact source copies and result hashes archived outside build in
bbp-mismatch-audit-47ce3d4. No live device writes/reset/unbind performed.

Implementation scope: separate v32 MAC snapshot profile before the existing full
BBP observation. Fixed63-address read allowlist,67 reads including boundary guards,
no write/exchange callback, cancellation/error/no-retry tests. BBP commands remain
blocked until snapshot completion; existing v31 behavior stays separate. Touch
MCU/xHCI/main/staging/tests and documentation; no new RF/channel/PA/LDO writes.

Fetched mainb17b80e (PR93 Air6,1 bounded battery return) and inspected PR88 tap
scope; neither is incorporated into the actual22fd266 base for this experiment.
The snapshot candidate adds no shared battery/input changes. Local staging88
cases and initial build/controller/MCU checks pass; final tests/VM pending.

MAC prerequisite snapshot installed: source `f3ba99c`, release
`b7a5985c12609971`, firmware profile v32. Build, MCU/xHCI fault/refusal/ownership
regressions,88 staging tests, ASan/UBSan, collector success/seven mutations and
docs checks pass. Exact-stage GPT absent-radio save/unmount/reset and
missing-workspace automatic return VM checks pass. Physical acceptance pending.
Candidate, VM receipts, collector/fixture and test logs archived outside build in
wifi-mac-snapshot-f3ba99c; run record saved. Ubuntu default and recovery
e47dca3a6b30c6c1 verified. Workspace201files/550clusters, zero allocation errors,
clean fsck and unmounted. No boot armed; await cold adapter cycle. On return,
collect the matching new session first, requiring complete63-value inventory and
67 reads before interpreting the unchanged full-BBP observations.

Native v32 snapshot incomplete: source `f3ba99c`, release `b7a5985c12609971`,
session `00065e892754f5ff-00`. Firmware/reset/timing, MAC phases0/1 and clock/BBP
readiness passed. Snapshot status4 complete0 reads37 writes0: initial MAC/CORE
checks plus35 target reads; target index34/address1030 returnedffffffff and the
all-ones policy stopped capture. Final guards and targets35..62 were not read;
their printed zero fields are not observations. No BBP table command ran and
both completion markers are NO. This is failed/incomplete evidence, not a pass.

The preceding34 valid targets include23 differences from final reference MAC
table settings: PBF_CFG0404=3fdc1cf5 vs77723c1f, WPDMA0208=40000050 vs30,
FCE_PSE0800=5 vs1. These support the missing-prerequisite inventory but establish
no cause of TXBE behavior. Pinned source identifies1030 as AMPDU_MAX_LEN_20M1S
and writesaaa99887; it does not document its reset read value. Do not infer a USB
transport failure or a legitimate reset value fromffffffff alone. Next distinguish
returned register data from transport validity with independent checks before
changing the snapshot admission policy or writing missing settings.

Separate failure collector archived the exact prefix and failure; success checker
unchanged. Ubuntu default, consumed one-shot and payload/recovery hashes verified.
Offline workspace clean:203files/553clusters, zero allocation errors, unmounted.
Evidence and partial comparison retained outside build in
wifi-mac-snapshot-f3ba99c/failed-native-collection. No new candidate or reboot armed.

Post-v32 AMPDU read audit: four read-only Linux samples show1030=aaa99887,
ASIC76502000, COREf000f200 and enabled MAC0c. Linux initialization writesaaa99887,
so this is not evidence of reset semantics. Debugfs selector restored; no register
writes, reset or unbind. Source audit confirms Srvr snapshot callback checks
successful control completion plus actual4 bytes before decoding; reads37/status4
therefore indicates rejection of returnedffffffff by our value policy, not its
transport-IO failure path. Individual transfer receipts were not logged, so this
does not rule out a driver defect or prove a valid reset value. Linux's USB read
API can itself return~0 on failed transfer, another reason not to infer semantics
from debugfs all-ones data alone.

Next scope selected: separate raw read-only inventory with explicit transfer
receipts, repeated ASIC/MAC/CORE guards and1030 read; retain all-ones only as
unvalidated observations and perform no subsequent BBP writes. Preserve v32's
strict stop and historical failures. Audit/capture/source hashes retained outside
build in ampdu-read-audit-6437ed0. Fetched mainbd55dd5 (PR94 Air6,1 failure display)
but actual base22fd266 unchanged. No candidate installed or reboot armed.

Active implementation: separate v33 raw read-only MAC inventory after the accepted
firmware/MAC/clock prerequisites. Log per-read USB completion, actual length and
residue, repeat ASIC/MAC/CORE guards and1030; record all-ones only as unvalidated
observations. No subsequent BBP table commands are admitted in this mode. Keep
v32 strict refusal behavior. Touch MCU/xHCI/main/staging/tests; no new RF or power
writes. Physical acceptance pending.

Fetched main6e608b0 (PR95 Air6,1 display return), inspected PR96 HPTC and PR88 tap
scopes; actual integrated base22fd266 remains unchanged for this experiment.
Initial build/MCU/xHCI regressions and89 staging cases pass. Separate collector
accepts raw all-ones observations only with70 exact successful transfer receipts,
all63 ordered values, repeated identity/guards and no BBP command; ten mutation
cases reject incorrect/incomplete evidence. No physical v33 result yet.

Raw MAC observation candidate installed: source `139f82c`, release
`2d2630e3bbbbe8d8`, profile v33. Build, MCU/xHCI fault and write-refusal tests,
89 staging cases, sanitizer checks, collector success/ten rejection cases and
docs checks pass. Exact-stage GPT save/unmount/reset and missing-workspace return
VM checks pass. Native results pending; no interpretation of all-ones as valid
settings is accepted. Archive outside build: wifi-mac-raw-139f82c, with payload,
VM receipts, collector/fixture and tests. Saved run record; Ubuntu default and
known-good e47dca3a6b30c6c1 recovery verified. Workspace203files/553clusters clean,
unmounted. No boot armed; await cold Wi-Fi cycle, then collect exact session first.

Native v33 raw capture complete: source `139f82c`, release `2d2630e3bbbbe8d8`,
session `00065e894bcf6265-00`. All70 ordered control transfers report ok1 actual4
cc1 residue0 timeout0. Initial/final ASIC76502000, MAC0 and COREf000f200 match;
all63 target values captured.1030 returnedffffffff on both reads (all_ones2).
Both completion markers yes. No BBP table command ran. This accepts the bounded
observation workflow, not register initialization, reset semantics or connectivity.
It supports repeated returned data rather than an observed USB transfer error;
no claim of independent bus-analyzer validation follows. v32 remains incomplete.

Comparison:16 of60 final reference MAC table values match,44 differ. Additional
controls: EXT_CCA141c=ffe4, FCE_L2_STUFF080c=3ff0233, WMM_CTRL0230=41040.
PBF_CFG0404=3fdc1cf5 and WPDMA0208=40000050 retain the earlier observed differences.
The inventory is now complete. Next select and audit a bounded MAC transport/
format prerequisite initialization set, using the captured starting state and
reference ordering, before returning to BBP comparison. No causal explanation of
TXBE10 follows yet; do not blindly enable omitted power/PA/LDO settings or RF.

Collector verified exact transfer/order/value/guard evidence and no table writes.
Native logs and comparison.json archived in wifi-mac-raw-139f82c outside build.
Ubuntu default, consumed one-shot and installed/recovery hashes verified; workspace
clean205files/556clusters, zero allocation errors, unmounted. No reboot armed.

Active scope: bounded FCE/WMM initialization after accepted MAC phases0/1 and
clock/readiness. Captured080c=03ff0233 ->03ff0223 clears only bit4;0230=41040
->41201 replaces low10 bits with201, preserving the rest. Reference init.c
performs these operations in this order. Captured EXT_CCA already has f000 set.
Exact initial-value gates, unused seq6 two-pair packet, strict ACK/readbacks and
MAC0/COREf000f200 boundaries; no subsequent BBP table writes. Defer PBF/WPDMA,
RF/power/PA/LDO changes. Touch MCU/xHCI/main/staging/tests; current base22fd266,
fetched main9488193 (Air6,1 HPTC) remains separate. Native behavior pending.

### 2026-10-23 — v34 MAC routing candidate installed, unarmed

Source `7cc6fe3`, release `c929ce1b090b820d`: FCE/WMM fixed-pair trial described
above. Build, compact/xHCI host tests, ASan/UBSan, 90 boot-profile tests and docs
checks passed. Exact-stage GPT VM saved diagnostics, passed offline filesystem
checks and returned automatically; missing-workspace VM returned without probing.
The collector accepts exact command/ACK, eight reads and both target readbacks;
synthetic success and five failure mutations passed. Hardware acceptance pending.

Installed with Ubuntu default and known-good previous `e47dca3a6b30c6c1` unchanged.
Raw inventory release `2d2630e3bbbbe8d8` retained; obsolete strict snapshot removed
from ESP only after comparison with its durable archive. UUID `0E53-5900` passed
preboot fsck and allocation checks (205 files, 556 clusters, no leaks), then was
unmounted. No one-shot armed. Run record: `build/bringup/wifi-mac-route/run.json`;
collector: `build/bringup/collect_wifi_mac_route_return.py`. Payload, evidence and
helpers: `~/.local/share/srvros/macbookpro11-2/wifi-mac-route-7cc6fe3/`.
Next: user-ready cold adapter power cycle, automatic capture/return, then collect
matching session before further changes. RF/connectivity remains NOT_TESTED.


### 2026-10-23 — v34 native FCE/WMM trial passed

Collected release `c929ce1b090b820d` / source `7cc6fe3`, new session
`00065e89795928b3-00`. The strict prepared collector passed: sequence6 sent24,
received16, cc13/residue1008; exact four-word ACK; status0, complete1, eight reads,
one submitted/completed batch and ack_mask1. FCE `03ff0233 -> 03ff0223` and WMM
`00041040 -> 00041201` both matched. MAC0 and COREf000f200 remained unchanged at
both boundaries. Firmware/reset/timing, compact0/1, clocks and readiness passed.
Both capture completion markers were present; no BBP table/raw/snapshot ran.

Ubuntu default, consumed one-shot and installed/recovery payload hashes verified.
Workspace fsck clean (207 files), allocation check passed (559 clusters, no leaks
or mismatches), then left unmounted. Full collection and result/checkpoint are
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-route-7cc6fe3/` with
checksums. Local collection: `build/bringup/wifi-mac-route/collection/collection-5pta14ud`.
No reboot armed. Acceptance is limited to these two MAC register operations;
RF/connectivity and the earlier four BBP mismatches remain unresolved. Next work:
review remaining reference MAC initialization prerequisites against the v33 raw
inventory, then select a bounded addition before repeating full baseband setup.

Active scope (2026-10-23): bounded common MAC policy group after accepted v34
routing. Reference common_mac_reg_table order: TX_LINK1350=1020,
TX_TIMEOUT1348=a2090, TX_RETRY134c=47d01f0f, AUTO_RSP1404=13. Captured starting
values 7f0020/10f0a90/4bb80407/7 must match before submission. One sequence7
four-pair command, exact ACK and all readbacks, MAC0/COREf000f200 boundaries.
No BBP-table or RF enable; unrelated PBF/DMA/power settings remain deferred.
Touch MCU/xHCI/main/staging/tests; retain integration base22fd266. Fetched
main2e2eb2b (PR88 taps, PR97 Air6,1); PR99 log capacity and PR98 battery SMBus
have separate active scope. No overlapping Wi-Fi implementation found.

### 2026-10-23 — v35 common MAC policy candidate installed, unarmed

Source `78f1944`, release `ee842dc8bcc56669`. Build, compact/xHCI host tests,
ASan/UBSan, 91 boot-profile tests and docs checks passed. Exact-stage GPT VM saved
diagnostics, passed offline checks and reset; missing-workspace VM returned
without probing. Collector fixture success and eight failure mutations passed.
Hardware acceptance pending for the four-register group; v34 prerequisites remain
strict and are rechecked first.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved; accepted route
`c929ce1b090b820d` retained. Raw inventory payload removed from ESP only after
byte comparison with its durable archive. UUID0E53-5900 clean207files/559clusters,
no allocation errors, unmounted. No one-shot armed. Run/collector:
`build/bringup/wifi-mac-policy/run.json`, `build/bringup/collect_wifi_mac_policy_return.py`.
Durable payload/evidence/helpers: `~/.local/share/srvros/macbookpro11-2/wifi-mac-policy-78f1944/`.
Next: readiness-confirmed cold adapter power cycle, automatic capture/return,
then collect matching session before any new run. RF/connectivity NOT_TESTED.


### 2026-10-23 — v35 failed TX_LINK strict readback

Collected release `ee842dc8bcc56669`, source `78f1944`, session
`00065e89a75cfd3a-00`. Firmware/reset/timing, compact0/1, clock/readiness and v34
FCE/WMM passed. Policy sequence7 sent40/received16, cc13/residue1008, exact ACK
including first requested value1020. Native state: status4, attempted1,
submitted1, complete0, reads7, batches_attempted1/completed0, ack_mask1.
TX_LINK1350 before007f0020, requested00001020, observed007f1020. Upper bits
remained set; no cause inferred. Other three target writes were submitted but
their readbacks and final MAC/CORE guards were not reached. Zero fields are not
observations. Both capture completion markers NO; RF/connectivity NOT_TESTED.

Prepared success collector remains unchanged. Separate failure collector
`build/bringup/collect_wifi_mac_policy_failure.py` verifies this exact failure
and recovery state. Full matching collection archived under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-policy-78f1944/failed-native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean209files/562clusters, zero leaks/mismatches, unmounted. Run and
checkpoint marked collected failure, no reboot armed. Next: inspect TX_LINK
field semantics/reference behavior before relaxing exact readback or testing
another write; remaining policy registers are unobserved.

### 2026-10-23 — TX_LINK readback audit (no device-register writes)

Read four live Linux samples from this exact7392:a711 adapter, mt76x0u, with
original debugfs regidx restored. TX_LINK1350 consistently007f1020, identical to
v35's native post-write value. Desired low control bits match1020; native changed
bit12 while retaining007f0000. Linux v7.0 rt2800 definitions identify upper bits
as remote feedback fields, but that related-chip definition is not a proven
MT7610 writable mask. Evidence suggests the whole-word equality was too strict;
it does not justify silently accepting v35 or ignoring arbitrary upper bits.

Linux also reads timeout1348=a1990, retry134c=47d01f0f, AUTO_RSP1404=3. mt76x02
runtime code updates ACK timeout and preamble selection after initialization,
so these live values are not cold-init expected values. Native remaining three
readbacks/final guards remain unknown. No driver reset/unbind or register writes.
Audit sources/results/capture and hashes:
`~/.local/share/srvros/macbookpro11-2/mac-policy-audit-44c2616/`.

Next bounded scope: separate observational policy profile with unchanged command,
ACK and initial-value gates; read all four targets plus final MAC/CORE guards,
report mismatches without aborting solely on a target value, and explicitly keep
register acceptance NOT_TESTED. Preserve v35 strict behavior. No new image
installed or reboot armed. Fetched main249bb97 (log capacity), PR101 AC transitions
separate; actual source integration base remains22fd266.

Active scope 2026-10-23: v36 policy observation, unchanged four-pair sequence7
command and initial-value gates after accepted FCE/WMM. Separate MCU entry point
collects all target readbacks despite ordinary mismatch, reports mismatch mask,
and requires final MAC/CORE guards. IO/cancellation/all-ones/ACK/ownership failures
still fail. No register acceptance or subsequent BBP writes; preserve v35 strict
behavior. Touch MCU/xHCI/main/staging/tests. Source base22fd266, fetched main249bb97;
PR101 AC transitions separate, no overlapping Wi-Fi implementation.

### 2026-10-23 — v36 policy observation candidate installed, unarmed

Source `d8e246a`, release `76e71b07371c6185`. Build, compact/xHCI tests,
ASan/UBSan, 92 boot-profile tests and docs checks passed. Observation tests cover
all16 mismatch masks and failure/cancellation/guard/ACK paths while strict v35
still fails mismatches. Collector covers all16 masks and seven invalid captures.
Exact-stage GPT VM saved diagnostics and passed offline checks; missing-workspace
VM returned without probing. Native observation result remains pending.

Ubuntu default and previous known-good `e47dca3a6b30c6c1` preserved. Failed v35
`ee842dc8bcc56669` retained; accepted route payload removed from ESP only after
full equality with its durable archive. UUID0E53-5900 clean209files/562clusters,
no allocation errors, unmounted; no one-shot armed. Run record/collector:
`build/bringup/wifi-policy-observe/run.json`,
`build/bringup/collect_wifi_policy_observe_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-policy-observe-d8e246a/`.
Next: readiness-confirmed cold adapter power cycle, automatic capture/return,
then collect matching session. Completion is observation only; register acceptance
and RF/connectivity remain NOT_TESTED.


### 2026-10-23 — v36 complete policy observations collected

Release `76e71b07371c6185`, source `d8e246a`, new session
`00065e89f951db06-00`. Firmware/reset/timing, compact0/1, clocks/readiness and
FCE/WMM passed. Policy sequence7 sent40/received16 cc13/residue1008, exact ACK;
status0 complete1 reads12 batches_attempted1/completed1 ack_mask1. All four
readbacks collected: LINK007f0020->007f1020 (request1020), TIMEOUT010f0a90->000a2090,
RETRY4bb80407->47d01f0f, AUTO_RSP7->13. Mismatch_mask1 correctly reports only
TX_LINK. Initial/final MAC0 and COREf000f200 verified; both capture markers yes.
Completion accepts observation only: register_acceptance=NOT_TESTED and
RF/connectivity NOT_TESTED. No BBP table ran; prior strict failure stays recorded.

Prepared observation collector passed unchanged. Complete collection and updated
run/checkpoint archived with checksums under
`~/.local/share/srvros/macbookpro11-2/wifi-policy-observe-d8e246a/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean211files/565clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: define an explicit bounded TX_LINK readback expectation supported
by this native capture and same-adapter Linux evidence, keeping the remaining
three exact checks and final guards; do not globally relax register comparison.

Active scope 2026-10-23: v37 exact policy validation, unchanged write bytes,
prerequisites/initial gates/ACK. Require LINK readback007f1020 (v36 native and
same-adapter Linux evidence), other three exact requested values, all12 reads and
final MAC0/COREf000f200. No ignored-bit mask; reject every other LINK value.
Preserve v35 strict and v36 observation modes. Touch MCU/xHCI/main/staging/tests.
Actual base22fd266; fetched maind753051 includes Air6,1 AC work, separate scope.

### 2026-10-23 — v37 exact policy candidate installed, unarmed

Source `c0ee877`, release `678a8d195cc57e83`. Build, compact/xHCI tests,
ASan/UBSan, 93 boot-profile tests and docs checks passed. Tests reject every
readback/final-guard bit mutation and retain IO/cancellation/no-retry behavior.
Collector rejects128 target-bit mutations and six invalid capture variants.
Exact-stage GPT VM saved diagnostics and returned with clean filesystem checks;
missing-workspace VM returned without probing. Native validation pending.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Complete v36
`76e71b07371c6185` retained; failed v35 removed from ESP only after comparison with
its durable archive. UUID0E53-5900 clean211files/565clusters, no allocation errors,
unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-policy-validate/run.json`,
`build/bringup/collect_wifi_policy_validate_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-policy-validate-c0ee877/`.
Next: readiness-confirmed cold adapter power cycle and automatic capture/return,
then collect matching session. RF/connectivity NOT_TESTED.


### 2026-10-23 — v37 exact policy validation passed natively

Collected release `678a8d195cc57e83`, source `c0ee877`, new session
`00065e8a2764f20b-00`. Firmware/reset/timing, compact0/1, clocks/readiness and
FCE/WMM passed. Policy command sequence7 sent40/received16 cc13/residue1008,
exact ACK; status0 complete1 reads12 batches_attempted1/completed1 ack_mask1.
Readbacks exactly LINK007f1020, TIMEOUT000a2090, RETRY47d01f0f, AUTO_RSP00000013.
Initial/final MAC0 and COREf000f200 verified. Validation complete1,
write_mismatch_mask1 exact_profile1; both capture completion markers yes.
No observation mode or BBP table ran. Prepared collector passed unchanged.
Acceptance is limited to the exact bounded policy operation on captured7392:a711;
RF/connectivity and remaining MAC/BBP initialization are still NOT_TESTED.

Complete matching collection, run/checkpoint and hashes archived under
`~/.local/share/srvros/macbookpro11-2/wifi-policy-validate-c0ee877/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean213files/568clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: select remaining reference MAC prerequisites against captured
inventory before revisiting full baseband initialization. Preserve all earlier
failed/observational records and the exact profile's device gates.

Active scope 2026-10-23: v38 bounded CCK/OFDM/GF20/GF40 protection group after
accepted v37 policy. Reference relative order1364/1368/1374/1378, requested
07f40003/07f42004/01742004/03f42084; captured starts00100003/00200003/01002004/
02002084. Exact starts, sequence8 four-pair packet/ACK, twelve reads and final
MAC0/COREf000f200. No PBF/DMA/RF changes or BBP-table writes. Touch MCU/xHCI/main/
staging/tests; base22fd266, fetched maind7ed773; PR102 filesystem scaling separate.

### 2026-10-23 — v38 CCK/OFDM/GF candidate installed, unarmed

Source `1947345`, release `89ee4a2fc2121ce2`. Build, compact/xHCI tests,
ASan/UBSan, 94 boot-profile tests and docs checks passed. Collector rejects128
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native protection-group acceptance pending.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v37
`678a8d195cc57e83` retained; v36 observation removed from ESP only after full
comparison with its durable archive. UUID0E53-5900 clean213files/568clusters,
no allocation errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-protect/run.json`, `build/bringup/collect_wifi_mac_protect_return.py`.
Durable payload/evidence: `~/.local/share/srvros/macbookpro11-2/wifi-mac-protect-1947345/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return, then
collect matching session. RF/connectivity NOT_TESTED.


### 2026-10-23 — v38 protection group passed natively

Collected release `89ee4a2fc2121ce2`, source `1947345`, new session
`00065e8a54a2695f-00`. Firmware/reset/timing, compact0/1, clock/readiness,
FCE/WMM and exact v37 policy passed. Protection sequence8 sent40/received16,
cc13/residue1008, exact ACK; status0 complete1 reads12, one batch completed,
ack_mask1. Exact before->after: CCK00100003->07f40003, OFDM00200003->07f42004,
GF20_01002004->01742004, GF40_02002084->03f42084. Initial/final MAC0 and
COREf000f200 verified. Both capture completion markers yes; no BBP table ran.
Acceptance limited to this bounded protection group on captured7392:a711;
remaining initialization and RF/connectivity NOT_TESTED.

Prepared collector passed unchanged. Complete collection/run/checkpoint archived
with hashes under `~/.local/share/srvros/macbookpro11-2/wifi-mac-protect-1947345/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean215files/571clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: select the remaining MAC/protection settings from the reference
inventory before revisiting full baseband initialization.

Active scope 2026-10-23: v39 mixed-mode/TXOP/RTS group after accepted v38.
Reference order136c/1370/1340/1344, writes01742004/03f42084/0000583f/00ffff20;
captured starts00402004/00802084/0400243f/00ffff07. Exact starts, sequence9 packet/
ACK, twelve reads and final MAC0/COREf000f200; no subsequent BBP or RF enable.
Touch MCU/xHCI/main/staging/tests; base22fd266, fetched mainf2e0945 battery-files;
PR102 filesystem scale separate, no Wi-Fi overlap.

### 2026-10-23 — v39 mixed-mode/TXOP/RTS candidate installed, unarmed

Source `d4c5fbc`, release `2cfc6b033353005e`. Build, compact/xHCI tests,
ASan/UBSan, 95 boot-profile tests and docs checks passed. Collector rejects128
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native acceptance pending for this group.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v38
`89ee4a2fc2121ce2` retained; v37 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean215files/571clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-contention/run.json`,
`build/bringup/collect_wifi_mac_contention_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-contention-d4c5fbc/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. RF/connectivity NOT_TESTED.


### 2026-10-23 — v39 mixed-mode/TXOP/RTS passed natively

Collected release `2cfc6b033353005e`, source `d4c5fbc`, new session
`00065e8a97cf1e48-00`. Firmware/reset/timing, compact0/1, clock/readiness,
FCE/WMM, exact policy and CCK/OFDM/GF protection all passed. Sequence9
sent40/received16 cc13/residue1008, exact ACK; status0 complete1 reads12,
one batch completed, ack_mask1. MM20_00402004->01742004,
MM40_00802084->03f42084, TXOP0400243f->0000583f, RTS00ffff07->00ffff20.
Initial/final MAC0 and COREf000f200 verified. Both capture markers yes; no BBP
table ran. Acceptance is limited to this bounded group on captured7392:a711;
remaining initialization and RF/connectivity NOT_TESTED.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-contention-d4c5fbc/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean217files/574clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: select remaining reference MAC prerequisites against the captured
inventory before revisiting full baseband initialization.

Active scope 2026-10-23: v40 TX_SW0/1/2 and header-translation-disabled group after
v39. Registers1330/1334/1338/0260 writes601/40000/0/0; captured starts0204080c/
000c0808/000c0408/0. Four live Linux samples agree with TX_SW final values;
active beacon offsets differ, deferred. Same-adapter read-only audit restored
regidx, no register writes/reset/unbind. Exact gates, sequence10 packet,12reads
and final MAC0/COREf000f200. Reference final table values, not replay of earlier
common TX_SW intermediate writes. Touch MCU/xHCI/main/staging/tests; base22fd266,
fetched mainc43d27c; PR106 power measurements separate.

### 2026-10-23 — v40 TX software-control candidate installed, unarmed

Source `e6e83ab`, release `656e4b9c37ae4baf`. Build, compact/xHCI tests,
ASan/UBSan, 96 boot-profile tests and docs checks passed. Collector rejects128
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native acceptance pending for this group.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v39
`2cfc6b033353005e` retained; v38 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean217files/574clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-txsw/run.json`,
`build/bringup/collect_wifi_mac_txsw_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-txsw-e6e83ab/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. RF/connectivity NOT_TESTED.

### 2026-10-23 — v40 TX software-control group passed natively

Collected release `656e4b9c37ae4baf`, source `e6e83ab`, new session
`00065e8acd64d0df-00`. Firmware/reset/timing, compact0/1, clock/readiness,
FCE/WMM, exact policy, protection and contention prerequisites all passed.
Sequence10 sent40/received16 cc13/residue1008 with exact ACK; status0 complete1,
reads12, one batch completed, ack_mask1. TX_SW0 0204080c->00000601,
TX_SW1 000c0808->00040000, TX_SW2 000c0408->00000000; header translation
remained00000000. Initial/final MAC0 and COREf000f200 verified. Both capture
completion markers present; no BBP table ran. Acceptance is limited to this
bounded group on captured7392:a711; RF/connectivity NOT_TESTED.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-txsw-e6e83ab/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean219files/577clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: review remaining reference MAC prerequisites, including beacon
layout, before revisiting the full baseband table and its unresolved readbacks.

Active scope 2026-10-23: v41 beacon-offset initialization after accepted v40.
Captured041c/0420 starts ece8e4e0/fcf8f4f0; reference common table writes
f8f0e8e0/6f77d0c8. Live Linux beacon offsets are later USB layout, not these
initialization values. Sequence11 two-pair command, eight guarded reads; no
beacon upload/timer enable, MAC TX/RX, RF or BBP table. Touch MCU/xHCI/main/
staging/tests. Base22fd266; fetched main7d2b903, PR106 power and PR109 Air6,1
Home policy remain separate; preserve their existing capture profiles.

### 2026-10-23 — v41 beacon-offset candidate installed, unarmed

Source `c9e2ab6`, release `015cce9347654d25`. Build, compact/xHCI tests,
ASan/UBSan, 97 boot-profile tests and docs checks passed. Collector rejects64
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Normal VM repeated to retain its shared receipt after the
missing-workspace run replaced it. Native acceptance pending for this pair.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v40
`656e4b9c37ae4baf` retained; v39 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean219files/577clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-beacon/run.json`,
`build/bringup/collect_wifi_mac_beacon_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-beacon-c9e2ab6/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. RF/connectivity NOT_TESTED.

### 2026-10-23 — v41 beacon-offset initialization passed natively

Collected release `015cce9347654d25`, source `c9e2ab6`, new session
`00065e8b0501527d-00`. All preceding firmware/reset/timing, compact0/1,
clock/readiness and bounded MAC groups passed. Sequence11 sent24/received16
cc13/residue1008 with exact ACK; status0 complete1 reads8, one batch completed,
ack_mask1. BCN_OFFSET0 ece8e4e0->f8f0e8e0 and BCN_OFFSET1 fcf8f4f0->6f77d0c8
matched exactly. Initial/final MAC0 and COREf000f200 verified. Both completion
markers saved; no BBP table ran. This accepts the initialization pair only on
captured7392:a711; operational beacon layout and RF/connectivity NOT_TESTED.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-beacon-c9e2ab6/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean221files/580clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: review remaining reference MAC prerequisites before revisiting the
full baseband table and its unresolved readbacks.

Active scope 2026-10-23: v42 VHT protection/fallback after accepted v41.
Registers13e0/13e4/13e8/1358: captured startse3f02004/e3f02084/e3f02104/edcba988,
reference targetse3f42004/e3f42084/e3f42104/edcba980. Four live Linux samples
match first/fourth; middle targets have extra10000 in active Linux, not a
cold-init expectation. Keep exact reference targets; no widened mask. Read-only
audit restored regidx and did not reset/unbind/write device registers. Sequence12,
four-pair packet,12reads and final MAC0/COREf000f200; no BBP table or RF enable.
Touch MCU/xHCI/main/staging/tests. Base22fd266; fetched mainde449b3 (Air6,1 Home),
PR106 Text polling/power separate; existing battery capture profiles preserved.

### 2026-10-23 — v42 VHT protection/fallback candidate installed, unarmed

Source `af42f25`, release `a5200b9a725a2ae5`. Build, compact/xHCI tests,
ASan/UBSan, 98 boot-profile tests and docs checks passed. Collector rejects128
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native acceptance pending for this group.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v41
`015cce9347654d25` retained; v40 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean221files/580clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-vht/run.json`,
`build/bringup/collect_wifi_mac_vht_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-vht-af42f25/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. RF/connectivity NOT_TESTED.

### 2026-10-23 — v42 VHT protection/fallback passed natively

Collected release `a5200b9a725a2ae5`, source `af42f25`, new session
`00065e8b2132c5a9-00`. All preceding firmware/reset/timing, compact0/1,
clock/readiness and bounded MAC groups passed. Sequence12 sent40/received16
cc13/residue1008 with exact ACK; status0 complete1 reads12, one batch completed,
ack_mask1. TX_PROT_CFG6/7/8 changed e3f02004/e3f02084/e3f02104 to
 e3f42004/e3f42084/e3f42104; VHT_HT_FBK_CFG1 edcba988->edcba980.
All four matched exact reference targets, including the two that differed under
active Linux. Initial/final MAC0 and COREf000f200 verified. Both completion
markers saved; no BBP table ran. Acceptance is limited to this bounded group on
captured7392:a711; RF/connectivity NOT_TESTED.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-vht-af42f25/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean223files/583clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: review remaining reference MAC prerequisites before revisiting the
full baseband table and its unresolved readbacks.

Active scope 2026-10-23: v43 packet-buffer limits/configuration after accepted v42.
Registers0408/040c/0404/0250: captured starts1f9f1f1f/fe9f/3fdc1cf5/0,
reference targets1fbf1f1f/fe9f/77723c1f/0. Four live Linux samples agree;
FCE0800 remains5 under Linux versus table1, deferred separately. Audit restores
regidx without device writes/reset/unbind. Sequence13 four-pair command,12reads,
final MAC0/COREf000f200. No PBF reset, DMA configuration, BBP table or RF enable.
Touch MCU/xHCI/main/staging/tests. Base22fd266; fetched maince1c0a6; Air6,1
Home-session PR111 and Air7,2 compositor-idle work separate.

### 2026-10-23 — v43 packet-buffer candidate installed, unarmed

Source `9d1ba79`, release `246fade6686cd2fa`. Build, compact/xHCI tests,
ASan/UBSan, 99 boot-profile tests and docs checks passed. Collector rejects128
single-bit target mutations and three invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native acceptance pending for this group.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v42
`a5200b9a725a2ae5` retained; v41 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean223files/583clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-buffer/run.json`,
`build/bringup/collect_wifi_mac_buffer_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-buffer-9d1ba79/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. RF/connectivity NOT_TESTED.

### 2026-10-23 — v43 packet-buffer configuration passed natively

Collected release `246fade6686cd2fa`, source `9d1ba79`, new session
`00065e8b57f9a360-00`. All preceding firmware/reset/timing, compact0/1,
clock/readiness and bounded MAC groups passed. Sequence13 sent40/received16
cc13/residue1008 with exact ACK; status0 complete1 reads12, one batch completed,
ack_mask1. TX_MAX_PCNT1f9f1f1f->1fbf1f1f; RX_MAX_PCNT remainedfe9f;
PBF_CFG3fdc1cf5->77723c1f; TSO_CTRL remained0. All four matched exact targets.
Initial/final MAC0 and COREf000f200 verified. Both completion markers saved;
no BBP table ran. Acceptance is limited to this bounded group on captured7392:a711;
frame-engine control0800 and RF/connectivity remain untested.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under `~/.local/share/srvros/macbookpro11-2/wifi-mac-buffer-9d1ba79/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean225files/586clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: audit remaining frame-engine/MAC prerequisites before revisiting the
full baseband table and its unresolved readbacks.

Active scope 2026-10-23: v44 FCE control observation after accepted v43.
FCE0800 is already written1 during firmware preparation, yet raw native and live
Linux later read5. Linux/vendor MAC table also writes1; no sourced bit2 writable
mask found. Observe a sequence14 pair0800=1/080c=03ff0223, retaining accepted
L2 routing. Captured starts5/03ff0223 mandatory; eight reads and final MAC0/
COREf000f200. First target differences are reported, never accepted; second target
must remain exact. No retry, BBP table or RF enable. Touch MCU/xHCI/main/staging/
tests. Base22fd266, fetched maindb10404; Air compositor PR112 separate.

### 2026-10-23 — v44 FCE control observation installed, unarmed

Source `8294ee1`, release `6e342f3cdd9f35db`. Build, compact/xHCI tests,
ASan/UBSan, 100 boot-profile tests and docs checks passed. Collector validates
raw first-target values with matching mismatch flags and rejects32 L2 mutations
and six invalid captures. Exact-stage GPT VM saved diagnostics, passed offline
checks and returned; missing-workspace VM returned without probing. Native
observation pending; completion cannot accept the FCE control register.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Accepted v43
`246fade6686cd2fa` retained; v42 payload removed from ESP only after full equality
with its durable archive. UUID0E53-5900 clean225files/586clusters, zero allocation
errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-mac-fce/run.json`,
`build/bringup/collect_wifi_mac_fce_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-fce-8294ee1/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. FCE register acceptance and RF/connectivity
NOT_TESTED; do not promote a captured difference into a writable-bit mask.

### 2026-10-23 — v44 FCE control observation completed natively

Collected release `6e342f3cdd9f35db`, source `8294ee1`, new session
`00065e8b8555a943-00`. All preceding firmware/reset/timing, compact0/1,
clock/readiness and bounded MAC groups passed. Sequence14 sent24/received16
cc13/residue1008 with exact ACK echoing the requested0800=1. Status0 complete1
reads8, one batch completed, ack_mask1. FCE control0800 remained5 before/after;
L2 routing080c remained03ff0223. Observation mismatch_mask1 and explicit
register_acceptance=NOT_TESTED saved. Initial/final MAC0 and COREf000f200 verified.
Both completion markers saved; no BBP table ran. This is complete diagnostic
evidence, not exact target acceptance or proof that bit2 is read-only.

Prepared observation collector passed unchanged. Complete collection/run/checkpoint
and hashes archived under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-fce-8294ee1/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean227files/589clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: use the captured write1/read5 evidence when defining strict profile
validation; do not infer a general writable-bit mask. Remaining MAC/baseband
initialization and RF/connectivity remain untested.

Active scope 2026-10-23: v45 exact FCE captured-profile validation after v44.
Reuse sequence14 pair0800=1/080c=03ff0223 with the same captured starts and
MAC/CORE guards. Require complete readback5/03ff0223 and mismatch_mask1;
reject every other full word, no masked bits. Preserve v44 observation semantics.
Touch MCU/xHCI/main/staging/tests. Base22fd266; fetched main180b542 with compositor
idle changes; PR114 battery Start/save and PR115 logger timing remain separate.
No new register writes, BBP table or RF enable in this trial.

### 2026-10-23 — v45 exact FCE profile validator installed, unarmed

Source `538fee1`, release `7301441289281aaf`. Build, compact/xHCI tests,
ASan/UBSan, 101 boot-profile tests and docs checks passed. Collector rejects64
single-bit readback mutations and five invalid captures. Exact-stage GPT VM saved
diagnostics, passed offline checks and returned; missing-workspace VM returned
without probing. Native validation pending for this exact captured profile.

Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved. Completed
v44 observation `6e342f3cdd9f35db` retained; v43 payload removed from ESP only
after full equality with its durable archive. UUID0E53-5900 clean227files/589clusters,
zero allocation errors, unmounted; no one-shot armed. Run/collector:
`build/bringup/wifi-fce-validate/run.json`,
`build/bringup/collect_wifi_fce_validate_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-fce-validate-538fee1/`.
Next: readiness-confirmed cold adapter cycle, automatic capture/return and
collection of matching session. Full-word5/03ff0223 validation only; no generalized
bit mask, BBP table or RF/connectivity acceptance.


### 2026-10-23 — v45 exact FCE profile validation passed natively

Collected release `7301441289281aaf`, source `538fee1`, new session
`00065e8bc3faaa90-00`. All preceding firmware/reset/timing, compact0/1,
clock/readiness and bounded MAC groups passed. Sequence14 sent24/received16
cc13/residue1008 with exact ACK; status0 complete1 reads8, one batch completed,
ack_mask1. FCE0800 remained5 and L2 routing080c remained03ff0223. Exact validation
reported complete1, write_mismatch_mask1, exact_profile1; observation marker absent.
Initial/final MAC0 and COREf000f200 verified, with both completion markers saved.
No BBP table ran. Acceptance is limited to the captured write1/read5 profile on
7392:a711; no generalized bit mask or RF/connectivity acceptance.

Prepared collector passed unchanged. Complete collection/run/checkpoint and hashes
archived under
`~/.local/share/srvros/macbookpro11-2/wifi-fce-validate-538fee1/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace clean229files/592clusters, zero leaks/mismatches, unmounted. No reboot
armed. Next: review remaining MAC prerequisites before revisiting the full baseband
table and its four unresolved readbacks. Scanning and connectivity remain untested.

Active scope 2026-10-23: integrate fetched main4a991df into the local device
branch before adding further MAC prerequisites. Preserve accepted v45 FCE sequence
and all historical Wi-Fi recipes. Shared baseline includes USB callback/logger
measurement, storage scaling and GUI waits; Air6,1 Home acceptance remains
model-specific and does not accept Pro battery ownership. PR19 is the only open
PR; no other active shared edit was requested. Resolve integration overlaps,
run local host and exact-stage VM checks, then prepare an unchanged-sequence
native regression. Installed7301441289281aaf and known-good recovery remain
untouched until the integrated candidate passes. No new RF or register scope.

### 2026-10-23 — integrated v45 regression installed, unarmed

Merged main `4a991df` as source `e1e595c`; staged/installed release
`9fadb1185a7592ea`, unchanged v45 firmware/FCE recipe. Full build and test-core
passed, alongside119 boot-profile tests and493 documentation links. All90
historical firmware and91 main boot configurations compare exactly against their
respective parents. Wi-Fi MCU sequence/native model gates are unchanged.
Exact-stage GPT VM verified saved/offline file bytes, clean allocation and automatic
return; missing-workspace VM returned without probing. Combined ECM device-checks
and Wi-Fi-probe VM passed DHCP/DNS/HTTP, three outage/recovery cycles,160 UDP echoes,
TCP serving, shared keyboard and disconnect quarantine. Human audio/gesture
acceptance is not implied by scripted VM answers.

Prepared collector retains all v45 predicates, passes the prior complete native
fixture with new release identity and rejects64 readback mutations plus three
incomplete captures. Run/collector:
`build/bringup/wifi-integrated/run.json`,
`build/bringup/collect_wifi_integrated_return.py`. Payload and test evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-integrated-e1e595c/`.
Ubuntu remains default, known-good previous `e47dca3a6b30c6c1` and accepted
v45 `7301441289281aaf` retained. Older v44 was removed from ESP only after full
archive equality. Workspace0E53-5900 clean229files/592clusters, zero allocation
errors and unmounted. No one-shot armed; native integrated acceptance pending.
Next: readiness-confirmed cold adapter cycle, automatic save/return, collect the
matching session before further implementation. RF/connectivity remains untested.


### 2026-10-23 — integrated v45 regression passed natively

Collected release `9fadb1185a7592ea`, source `e1e595c` (main4a991df integrated),
new session `00065e8c5588a9b9-00`. Prepared collector passed unchanged: all prior
firmware/reset/timing, compact0/1, clock/readiness and bounded MAC groups passed.
FCE sequence14 sent24/received16 cc13/residue1008 with exact ACK, complete1
reads8, one batch completed and ack_mask1. Full-word readbacks5/03ff0223,
write_mismatch_mask1 and exact_profile1 verified. Initial/final MAC0 and
COREf000f200 plus both completion markers saved. No full BBP table or RF ran.

Complete collection/run/checkpoint and hashes archived under
`~/.local/share/srvros/macbookpro11-2/wifi-integrated-e1e595c/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace0E53-5900 clean231files/595clusters, zero leaks/mismatches, unmounted.
No reboot armed. This accepts the prior Wi-Fi initialization sequence on the
integrated baseline, not general desktop or battery behavior. Next: remaining
MAC prerequisites before revisiting the four unresolved baseband readbacks.
Scanning, association and connectivity remain untested.


Active scope 2026-10-23: bounded v46 AMPDU1030 initialization following accepted
integrated v45. Sequence15 pair1030=aaa99887/080c=03ff0223; second target preserves
accepted routing. Initial1030=ffffffff is admitted only in this dedicated engine,
with successful exact USB4-byte transfer; it is not declared a valid reset value.
Require exact post-write targets, final MAC0/COREf000f200 and eight reads. Preserve
all existing all-ones refusals and v45 behavior. No retry, BBP table or RF enable.
Touch MCU/xHCI/main/staging/tests. Integrated base4a991df; fetched2702b41 adds
Air6,1 continuous desktop scope. PR118 draft edits storage waiting, not Wi-Fi
control/MCU dispatch; retain its existing shared storage path unchanged.

### 2026-10-23 — v46 AMPDU length initialization installed, unarmed

Source `9fa1ff6`, release `e14f396f3d59c1b6`. Build, compact/xHCI host tests,
ASan/UBSan,120 boot-profile tests and493 documentation links passed. All90 earlier
Wi-Fi boot recipes remain byte-identical. The collector rejects64 readback
mutations,40 transfer failures and two incomplete captures. Exact-stage GPT VM
saved diagnostics, verified offline bytes/allocation and returned; missing-workspace
VM returned without probing. Native acceptance remains pending.

Run/collector: `build/bringup/wifi-mac-ampdu/run.json`,
`build/bringup/collect_wifi_mac_ampdu_return.py`. Durable payload/evidence:
`~/.local/share/srvros/macbookpro11-2/wifi-mac-ampdu-9fa1ff6/`.
Ubuntu default and known-good previous `e47dca3a6b30c6c1` preserved, accepted
integrated `9fadb1185a7592ea` retained. Earlier7301441289281aaf removed from ESP
only after full equality with its durable archive. Workspace0E53-5900 clean231files/
595clusters, zero allocation errors and unmounted. No one-shot armed. Next:
readiness-confirmed cold adapter cycle, automatic save/return and collection of
matching session. Sequence15, eight exact transfer receipts and strict post-write
readbacks required; no BBP table, RF or connectivity acceptance.


### 2026-10-23 — v46 aggregation-length initialization passed natively

Collected release `e14f396f3d59c1b6`, source `9fa1ff6`, new session
`00065e8ccad60890-00`. Prepared collector passed unchanged: all preceding
firmware/reset/timing, compact0/1, clock/readiness and bounded MAC groups passed.
AMPDU sequence15 sent24/received16 cc13/residue1008 with exact ACK; complete1,
reads8, one batch completed, ack_mask1. Register1030 changedffffffff->aaa99887;
080c remained03ff0223. All eight ordered read receipts reportedok1, actual4,
cc1, residue0 and timeout0. Initial/final MAC0 and COREf000f200 verified; both
completion markers saved. No full BBP table or RF ran.

Complete collection/run/checkpoint and hashes archived under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-ampdu-9fa1ff6/native-collection/`.
Ubuntu default, consumed one-shot and installed/known-good payload hashes verified.
Workspace0E53-5900 clean233files/598clusters, zero leaks/mismatches, unmounted.
No reboot armed. Acceptance is bounded to this register write/readback, not
reset semantics, aggregation performance or connectivity. Next: remaining MAC
prerequisites before revisiting the four unresolved baseband readbacks. Further
MCU commands require deliberate sequence identifier reuse after15; do not use16.


Active scope 2026-10-23: v47 full BBP observation after accepted MAC/AMPDU chain.
Reuse the existing58-entry/three-chunk observation engine after every bounded MAC
step completes. Exact legacy table ACK uses sequence6, deliberately reused only
after all prior responses have been consumed; route's sequence6 ACK has a different
address/value and must be rejected. No new table values or RF enable. Require
all prerequisite modes/completions/submissions/inactive owners throughout table
reads/exchanges. Preserve v31 and v46 behavior. Touch xHCI/main/staging/tests;
collector reports mismatches without accepting them. Base4a991df, fetchedbbdbefb
adds Air6,1 AC work; PR118 remains draft storage-wait scope, kept separate.

Prepared v47 source `88809f0`, release `6d6d0b1a49c3de4b`, installed unarmed.
Native build, compact/xHCI fixtures, ASan/UBSan,121 boot-profile tests, collector
fixture plus121 rejection cases, and493 documentation links passed. Exact-stage
GPT VM saved/verified logs and returned automatically; missing-workspace VM
returned without starting the probe. Workspace remains clean233files/598clusters.
Ubuntu remains default, known-good `e47dca3a6b30c6c1` and accepted AMPDU
`e14f396f3d59c1b6` retained. Integrated `9fadb1185a7592ea` was removed from the
ESP only after byte-for-byte archive verification. Payload, checks, collector
and run record archived outside build under `wifi-bbp-after-mac-88809f0`.
Physical result pending; next is an attended cold adapter cycle. After return,
collect the new matching session first and compare all58 observations with v31.

### 2026-10-23 — v47 post-MAC baseband observation returned

Source `88809f0`, release `6d6d0b1a49c3de4b`, native session
`session-00065e8d0bce2741-00` completed. The prepared collector passed unchanged:
all prior MAC/AMPDU prerequisites, three200/200/88-byte chunks, deliberate
sequence6 reuse with exact ACK,124 reads, four MAC0/COREf000f200 guard pairs,
58 observations and both completion markers.54 targets matched; the same four
targets as v31 remained mismatched:

- TXBE10/2728: expected0, before37fff, after38e30.
- TXBE17/2744: expected0, before/afterfb3fee3c (v31fb3fec3c).
- RXO21/2954: expected1, before/after0.
- RXO24/2960: expected6, before/after0.

All other before/after values exactly match v31. This single comparison does
not establish the cause of the TXBE17 difference. MAC initialization through
AMPDU did not resolve the four mismatches; table completion remains observation
only, with register acceptance and RF/connectivity NOT_TESTED. Do not widen
acceptance predicates based on this run. Next: audit these four register
definitions and initialization ordering against the reference driver before
selecting a bounded diagnostic; avoid repeating unchanged table trials.

Ubuntu default, consumed one-shot, installed/known-good payload hashes verified.
Workspace0E53-5900 clean235files/601clusters, zero leaks/mismatches, unmounted.
Collection, comparison, run/checkpoint and hashes archived outside build under
`~/.local/share/srvros/macbookpro11-2/wifi-bbp-after-mac-88809f0/`.
No reboot armed.

Active scope 2026-10-23: audit the four persistent BBP mismatches after v47.
Compare pinned Linux register tables, initialization ordering and vendor-authored
register uses; capture fresh read-only Linux values if useful. Publish findings
and a bounded next diagnostic. No new native writes or acceptance relaxation.
Touch this unit record/current Wi-Fi guide only until evidence justifies code.
Integration base remains4a991df; fetched main54862de adds Air6,1 documentation.
PR118 storage-wait remains separate; PR19 owns this Wi-Fi work.

### 2026-10-23 — post-v47 read-only audit

Four fresh Linux7.0.0-38-generic samples on the same7392:a711 adapter read
TXBE10=38e30, TXBE17=fb3fee3c, RXO21=0 and RXO24=0: all four exactly match
v47. MAC=0c and CORE=f000f200. This differs from older Linux samples where
TXBE10 was0. The Ubuntu run followed SrvrOS without confirmed adapter power
loss, so warm state and later Linux initialization remain confounders.
Only debugfs read-address selection and register reads were used; selector
restored, no register writes, driver unbind or reset.

Pinned [Linux v7.0 initialization](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/init.c)
writes the BBP table, then the G-band20MHz switch table and DCOC table; it does
not test equality of these four readbacks. The vendor-authored source mirrored
at [commit4f1f50e](https://github.com/xtknight/mt7610u-linksys-ae6000-wifi-fixes/tree/4f1f50e3b5e5230f36a13f33112873765389dab3)
likewise initializes them, without documenting their bit-level readback semantics
in the inspected definitions/uses. These facts do not establish that a mismatch
is harmless, nor accept native RF/connectivity. No predicates were relaxed.

Next diagnostic: power-cycle only the Wi-Fi adapter while Ubuntu remains up,
then repeat the same read-only samples with dynamic PHY discovery and exact
descriptor/driver/model gates. This controls adapter power history without a
new SrvrOS write trial. Collector prepared locally as
`build/bringup/bbp-post-mac-audit/capture-cold.py`. Cold results pending.
Raw audit and source hashes archived outside build under
`~/.local/share/srvros/macbookpro11-2/bbp-post-mac-audit-c223f58/`.
No reboot armed; installed payload remains6d6d0b1a49c3de4b.

### 2026-10-23 — cold Linux BBP comparison

User confirmed Wi-Fi unplugged for10seconds and reconnected under Ubuntu.
Exact model/descriptor/driver gates passed; PHY changed fromphy1 tophy2.
Four read-only samples consistently returned:

| Register | Warm Linux / v47 | Cold Linux | Table target |
| --- | --- | --- | --- |
| TXBE10/2728 | 38e30 | 38e30 | 0 |
| TXBE17/2744 | fb3fee3c | ff3fef3c | 0 |
| RXO21/2954 | 0 | 0 | 1 |
| RXO24/2960 | 0 | 0 | 6 |

ASIC76502000, MAC0c, COREf000f200 and TXBE9=20000000 were stable.
Debugfs selector restored; no register writes or driver reset performed by
the capture. Normal Linux enumeration followed the physical reconnection.
All four equality mismatches also occur after cold Linux initialization; warm
SrvrOS carryover alone does not explain them. TXBE17 varies between captures.
This does not document register semantics or establish native acceptance.
No initialization-time Linux trace or connection/traffic test was captured.

Next: audit the reference G-band20MHz BBP switch and DCOC stages that follow
the full table, including RF/ownership boundaries, before implementing a
bounded continuation. Do not spend another boot trying to force equality
without evidence that equality is required. Existing strict diagnostics remain
unchanged. Cold samples/comparison archived alongside the previous audit in
`bbp-post-mac-audit-c223f58`; installed release unchanged, no reboot armed.

Active scope 2026-10-23: source-backed BBP switch/DCOC audit after cold Linux
comparison. Derive exact G-band20MHz selection, transport and later RF boundary;
prepare fixtures and read-only Linux inventory. Current source9b01d39, actual
integration base4a991df; fetched main7b0d1a0 carries Air6,1 acceptance docs.
PR118 is no longer open; inspect its status before any integration (not assumed
merged here). PR123 owns network-idle work; PR19 remains this Wi-Fi scope.
No shared transport edits or native writes in this audit.

### 2026-10-23 — next BBP stage audit completed

Pinned Linuxv7.0 `mt76x0_init_bbp()` orders full BBP table, G-band20MHz
switch settings, then DCOC. The exact selector yields20 entries: AGC4/6/8/12/13/14/
26/27/28/31/32/33/35/39/43/51/53/55/58 and RXFE0. All selected values are
non-all-ones. Linux writes each through direct USB request6, value0, register
index and a four-byte little-endian payload. This step needs no MCU sequence
reuse. The following DCOC table contains CAL47..55 (nine entries), sent through
MCU random-write. EEPROM and PHY/RF initialization occur later.

Checked exact selection/count/order/address range, transport definitions and
20 generated setup/data fixtures. Four fresh read-only Linux samples covered
32 registers (29 targets plus ASIC/MAC/CORE); selector restored.18 switch values
and CAL47 differ from initialization defaults. Most switch differences match
the source A-band variants, including all-ones AGC51/53/55. This illustrates
later band-dependent state; it does not prove an active channel or link. Keep
native G/20 all-ones rejection unchanged because that selected table has no
all-ones targets. Do not import live A-band values into the G/20 trial.

Selected next implementation: one explicit20-entry G/20 direct-write stage
after v47 completion, with all prerequisite ownership gates, MAC0/COREf000f200
guards, exact setup/payload admission, one attempt per entry, transport receipts
and before/after readbacks. No arbitrary register API. Table observation remains
distinct from exact readback acceptance. Abort on transfer/ownership/guard
failure and preserve the saved result. DCOC, RF/channel selection and TX/RX
enable are excluded from this first continuation. This is the reference's
initial baseband default, not a regulatory/channel or connectivity acceptance.

Audit sources, derived tables, packet fixtures, live samples and hashes archived
outside build under `~/.local/share/srvros/macbookpro11-2/bbp-switch-audit-9b01d39/`.
No new native candidate installed or boot armed. Actual integration base4a991df;
fetched main7b0d1a0 includes merged PR118 storage-wait (merge9087fd8), which
is not incorporated in this device checkout. Open PR123 owns network polling.

Active scope: v48 fixed20-entry G/20 BBP switch following v47 capture.
Touch mt76 pure engine, xHCI binding/main, boot profiles/tests and current guides.
Use direct request6 writes, exact fixed order,88 reads with MAC/CORE guards
before every write, one attempt, full receipts and observational readbacks.
Retain all prerequisite gates and old profiles. No DCOC/RF/TX/RX enable.
Base4a991df; main7b0d1a0/merged storage-wait not yet integrated; PR123 separate.

Prepared v48 source `e26dedf`, release `9bc9e99df950ae78`, installed unarmed.
Native build, compact/xHCI tests, ASan/UBSan,122 boot-profile tests,94 unchanged
historical recipes, collector fixture/45 rejection cases and493 documentation
links passed. Exact-stage GPT VM saved verified logs and returned; missing
workspace returned without starting the probe. Full fixture log is below the
65536-byte retained tail. Native result pending.
Ubuntu default and known-good `e47dca3a6b30c6c1` retained; previous observation
`6d6d0b1a49c3de4b` retained. AMPDU `e14f396f3d59c1b6` removed from ESP only
after full archive equality. Workspace clean235files/601clusters, unmounted.
Payload, VM receipts, tests, source audit, collector and run record archived
under `~/.local/share/srvros/macbookpro11-2/wifi-bbp-switch-e26dedf/`.
Next: attended cold adapter cycle; collect matching session first on return.

### 2026-10-23 — v48 G/20 baseband switch returned

Source `e26dedf`, release `9bc9e99df950ae78`, session
`session-00065e8d6470043c-00`: all20 ordered direct USB writes completed with
actual4/completed1;88 reads and all per-write/boundary MAC0/COREf000f200 guards
passed. Every after value matched its fixed G/20 target (16 changed,4 already
matched). All prior MAC/AMPDU and full-table prerequisites completed; the prior
58-entry table still reported its four known mismatches. Both capture completion
markers present. This establishes the bounded switch write/readback result on
this adapter; it does not accept RF/channel selection or connectivity. The
diagnostic retains its observational `register_acceptance=NOT_TESTED` label.

Collector hardware predicates all passed initially, but recovery bookkeeping
then failed because local readback variable `b` shadowed the hardware_boot
module alias. Renamed that variable to `before_value` in the local collector
and its generator; no predicate was changed. Rerun passed Ubuntu default,
consumed one-shot, installed/known-good hashes and offline workspace checks:
237files/604clusters, zero leaks/mismatches, unmounted. Original error and
original/fixed collector copies are retained in the durable archive.

Collection, summary, run/checkpoint and hashes archived under
`~/.local/share/srvros/macbookpro11-2/wifi-bbp-switch-e26dedf/`.
No reboot armed. Next: implement the separately audited nine-entry CAL47..55
DCOC table after the switch, with bounded MCU command ownership, exact ACK
validation and guarded readback capture; keep RF/TX/RX enable separate.

Active scope: v49 fixed nine-entry CAL47..55 DCOC observation after exact
v48 switch readbacks. Touch mt76 engine, xHCI/main, boot profiles and tests.
One80-byte MCU write, deliberate sequence7 reuse after all earlier replies,
exact first address/value ACK;26 guarded reads and mismatch observations.
No retries, RF/channel selection or TX/RX enable. Base4a991df, fetchedmain7b0d1a0;
PR123 network-idle and PR124 Air6,1 capture remain separate.

Prepared v49 source `cf84f67`, release `51c771ce50036ef4`, installed unarmed.
Native build, compact/xHCI tests, ASan/UBSan,123 boot-profile tests,96 unchanged
historical recipes, collector fixture/23 rejection cases and493 documentation
links passed. Collector test also checks that readback parsing preserves its
hardware_boot module binding. Exact-stage GPT VM saved/verified logs and
returned; missing workspace returned without starting the probe.
Ubuntu default and known-good `e47dca3a6b30c6c1` retained; accepted G/20
`9bc9e99df950ae78` retained. Earlier observation `6d6d0b1a49c3de4b` removed
from ESP only after full archive equality. Workspace clean237files/604clusters,
unmounted. Payload, VM receipts, tests, audit, collector and run record archived
under `~/.local/share/srvros/macbookpro11-2/wifi-dcoc-cf84f67/`.
Physical result pending. Next: attended cold adapter cycle; collect matching
session first after return, including nine readbacks and all prerequisites.

### 2026-10-23 — v49 DCOC native result

Source `cf84f67`, release `51c771ce50036ef4`, session
`session-00065e8d9c2b07e5-00` completed. The prepared collector passed unchanged:
one sequence7 command sent80/received16 (cc13/residue1008), exact ACK
4a070008/412cbc/10f0/4a070008,26 reads and all MAC0/COREf000f200 guards.
All nine CAL47..55 readbacks matched their target values (eight changed;
CAL51 already matched). All earlier prerequisites, including exact20-setting
G/20 switch readbacks, and both completion markers passed.

This establishes the bounded DCOC table write/readback result on this adapter,
not RF calibration, channel selection or connectivity acceptance. The diagnostic
retains its observational register_acceptance=NOT_TESTED label; earlier four
full-table mismatches remain separate evidence.

Ubuntu default, consumed one-shot and installed/known-good payload hashes
verified. Workspace clean239files/607clusters, zero leaks/mismatches, unmounted.
Complete collection, summary, run/checkpoint and hashes archived outside build
under `~/.local/share/srvros/macbookpro11-2/wifi-dcoc-cf84f67/`. No reboot armed.
Next: audit remaining reference MAC/security-table prerequisites and the entry
into PHY/RF initialization against existing captured calibration/regulatory
evidence, then choose a bounded next stage. Do not infer full MAC or RF readiness
from completion of these baseband tables.

Active scope: post-v49 reference MAC/security/EEPROM/PHY entry audit.
Derive exact reset ranges and identify remaining gates from saved calibration
evidence; no native writes, security-key reads or RF enable in this audit.
Current sourcefbafa66, actual integration base4a991df; fetchedmain92722b7
includes PR123 network-idle. OpenPR124 Air6,1 capture is separate from PR19.

### 2026-10-23 — post-DCOC prerequisites audit

Pinned Linuxv7.0 initializes64 shared-key slots (16 interfaces ×4 keys), then
256 WCID attributes and only128 WCID address entries. Null-key setup clears
one cipher nibble with a read/modify/write and writes32 zero bytes per shared
slot. Eight unique mode words cover the64 slots. WCID setup clears attributes
for all256 indices, but returns before address memory for indices128..255.
It does not clear pairwise-key or IV memory in that function; do not infer
those resets or add broad SRAM clearing.

Derived ranges: shared slotsac00–afff andb400–b7ff, mode wordsb000–b00c
andb3f0–b3fc, WCID attributesa800–abfc and address entries1800–1bf8.
Exact reference loops total512 writes/4352 payload bytes, including64 mode
updates.100 seeded randomized cases checked nibble preservation and final
mode clearing; range/count checks and64 zero-only USB setup/data fixtures
passed. No live security memory was read and no hardware writes were sent.

Implementation coverage audit leaves22 MAC reference table addresses untouched:
102c,0208,1380,1608,1204,0124,0a44,1218,1220,13b0,13c0,1314,1318,131c,
1320,1324,13d4,13d8,13dc,1238,006c,0070. Coverage means touched by a staged
implementation, not proof of exact full reference ordering or field equivalence.
These include power/PA/LDO/DMA-related settings and must not be bulk copied.

The existing logical calibration/receive/transmit audits remain useful; physical
EFUSE usage-map validation is distinct and still open. The reference's EEPROM
load path can use an externally supplied valid image; its EFUSE fallback performs
physical-map checks before logical data. It also programs the MAC address before
PHY entry. PHY init selects antenna state before RF tables/frequency/DAC work.
United States confirmation already exists; no new country question is needed.
Actual channel/power/regulatory validation remains required before transmit.

Selected next bounded implementation: reference-order64 shared-slot cipher
nibble clears plus64 zero32-byte writes, exact high-address USB allowlist,
MAC/CORE/ownership guards, no retries, zero verification without key-byte dumps.
Do not widen the general MCU register range. WCID reset, remaining MAC controls,
physical EFUSE and PHY/RF work remain separate tracked stages.
Audit/fixtures/source hashes archived at
`~/.local/share/srvros/macbookpro11-2/post-dcoc-audit-fbafa66/`.
No candidate installed or reboot armed by this audit. Main92722b7 fetched but
actual device integration base remains4a991df; PR124 remains separate.

Active scope v50: fixed64 shared-key reset slots after exact DCOC readbacks.
Reference-order cipher nibble clear and32-byte zero write, zero-only verification;
576 bounded operations,448 reads/128 writes, no key data logs or retries.
Touch pure engine/xHCI/main/profiles/tests. Base4a991df; fetchedmainc4265f8
and openPR125 TCP are separate. Preserve existing profiles and RF-disable gates.

### 2026-10-23 — v50 shared-key reset candidate

Source `1997865`, release `420bc4d5e39c8e04`, installed unarmed. The fixed
64-slot reset follows exact DCOC readbacks, preserves unrelated cipher nibbles,
verifies zero data without logging key bytes, and stops without retry on failure.
Host/USB tests cover every operation, cancellation and ownership refusal;
ASan/UBSan,124 boot-profile tests,98 unchanged historical Wi-Fi recipes,
collector fixture/14 rejection cases and493 documentation links pass.
Exact-stage GPT VM saved logs and returned; missing-workspace VM returned
without starting the probe. These VM results do not accept the physical reset.

Ubuntu default and known-good `e47dca3a6b30c6c1` retained; prior accepted DCOC
`51c771ce50036ef4` retained. G/20 release `9bc9e99df950ae78` removed from ESP
only after full archive equality. Workspace clean239files/607clusters, unmounted.
Payload, audit, tests, VM receipt, collector and run record are archived under
`~/.local/share/srvros/macbookpro11-2/wifi-skey-1997865/` outside build.
Actual integration base remains4a991df; fetchedmainc4265f8 and PR125 TCP
readiness inspected, separate from this bounded Wi-Fi work.
Next: attended cold adapter cycle, then collect the matching session FIRST.
Require operations576/submitted576, reads448/writes128, slots64 and both
mode_verified64/key_zero64 plus all prior checks. RF/connectivity not tested.

### 2026-10-24 — v50 shared-key reset native result

Source `1997865`, release `420bc4d5e39c8e04`, session
`session-00065e8e2bef4841-00` completed. The prepared collector passed unchanged:
576 operations/submissions,448 reads,128 writes,64 completed slots,
64 verified cipher-mode updates and64 verified zero-data slots. Exact DCOC
readbacks and all earlier prerequisites passed; both completion markers present.
No key bytes were logged. This accepts the fixed shared-key reset on the captured
adapter, not WCID initialization, RF, scanning, association or connectivity.

Ubuntu default, consumed one-shot and installed/known-good payload hashes pass.
Workspace clean241files/610clusters with zero leaks/allocation mismatches,
unmounted. Collection and updated run/checkpoint/hashes are archived outside
build under `~/.local/share/srvros/macbookpro11-2/wifi-skey-1997865/`.
No reboot armed. Next bounded scope: reference WCID station-table reset
(256 attributes and128 address entries), retaining fixed ranges, guards,
zero-only writes/verification and no retries. Pairwise keys/IV and RF remain
separate; do not infer their reset from successful shared-slot initialization.

Active scope v51: fixed WCID reset after accepted shared-key reset. Reset256
attributes and128 address entries with zero-only writes/readbacks, guards and
no retries. Touch mt76/xHCI/main/profiles/tests; no pairwise/IV or RF enable.
Actual base4a991df; fetchedmain76f98c4 includes separate PR125 TCP readiness.
Next checkpoint: local validation and exact-stage VM before native trial.

### 2026-10-24 — v51 WCID reset candidate

Source `666a4d3`, release `71bf958aa703f6b3`, installed unarmed. The fixed
reset covers256 attributes and128 eight-byte addresses in reference order,
with1792 operations (1408 reads/384 writes), per-entry MAC/CORE guards,
zero verification and one attempt per transfer. Completed shared-key reset is
required; pairwise-key/IV tables and RF remain outside this stage.

Native build, compact/xHCI failure tests, ASan/UBSan,125 boot-profile tests,
100 unchanged historical Wi-Fi recipes, collector fixture/15 rejection cases
and493 documentation links passed. Exact-stage GPT VM saved and verified logs
and returned; missing workspace returned without entering the probe.
Physical WCID acceptance remains pending.

Ubuntu default and known-good `e47dca3a6b30c6c1` retained. Accepted shared-key
release `420bc4d5e39c8e04` retained; DCOC `51c771ce50036ef4` removed from ESP
only after full archive equality. Workspace clean241files/610clusters, unmounted.
Payload, audit, tests, VM receipt, collector and run/checkpoint are archived at
`~/.local/share/srvros/macbookpro11-2/wifi-wcid-666a4d3/` outside build.
Next: attended cold adapter cycle, then collect matching session FIRST and
require256 attribute/128 address verifications plus all prior checks.
Actual integration base4a991df; fetchedmain76f98c4 contains separate TCP work.

### 2026-10-24 — v51 WCID reset native result

Source `666a4d3`, release `71bf958aa703f6b3`, session
`session-00065e8e5ad7fc6a-00` completed. The prepared collector passed unchanged:
1792 operations/submissions,1408 reads,384 writes,256 completed slots,
256 verified zero attributes and128 verified zero address entries. Exact
shared-key completion and all earlier prerequisites passed, with both capture
completion markers present. This accepts the fixed WCID reset on this adapter;
pairwise-key/IV initialization, RF, scan, association and connectivity remain
outside the accepted scope.

Ubuntu default, consumed one-shot and installed/known-good payload hashes pass.
Workspace clean243files/613clusters, zero leaks/allocation mismatches, unmounted.
Complete collection and updated run/checkpoint/hashes are archived outside build
at `~/.local/share/srvros/macbookpro11-2/wifi-wcid-666a4d3/`. No reboot armed.
Next: audit the remaining MAC initialization settings and physical EFUSE/PHY
entry prerequisites against the pinned reference and existing device evidence;
select a bounded next stage without bulk-copying power/PA/LDO/DMA controls.

Active scope: post-WCID initialization audit and remaining-work ledger.
Classify the22 untouched MAC table addresses; inspect EEPROM/PHY entry order
and select the next bounded probe. No new hardware writes or radio enable.
Base4a991df, fetchedmain76f98c4; PR127 storage timing is separate from PR19.

### 2026-10-24 — post-WCID audit and remaining scope

Classified22 untouched reference MAC targets: indicator1,DMA1,ACK/TXOP2,
board/power/PA/LDO6,TX power/gain10,unnamed2. Not all are mandatory connectivity
work; no blanket register-copy plan. Prepared26 fixed USB read request fixtures:
MAC/CORE guards,22 observations,then both guards. Next implementation is this
single read-only post-WCID snapshot, with target all-ones retained as observation
and guard failures rejected. No native I/O or new boot payload in this audit.

Verified reference ordering WCID→EEPROM→PHY and antenna→RF→RX path→TX DAC.
Physical EFUSE usage map is29 bytes1e0–1fc, transported as two16-byte reads;
logical EEPROM capture does not establish that check. Private device MAC setup,
remaining MAC decisions, RF/calibration/channel work,802.11 data/scan/security
and end-to-end network/UI acceptance remain. United States confirmation retained.
Offline checks/26 fixtures/source hashes archived outside build under
`~/.local/share/srvros/macbookpro11-2/post-wcid-audit-1f4d822/`.
Installed accepted release remains71bf958aa703f6b3, no reboot armed.
Fetchedmain76f98c4 and PR127 storage timing are separate; actual base4a991df.

Active scope v52: fixed22-target post-WCID MAC observation,26 reads total.
Preserve MAC0/COREf000f200 guards; all-ones targets remain observations. No writes,
RF enable or acceptance inferred. Touch mt76/xHCI/main/profiles/tests. Actual
base4a991df; other machine storage-timing work remains separate.

### 2026-10-24 — v52 post-WCID MAC observation candidate

Source `4ba7963`, release `619ce12c1d5d0f0b`, installed unarmed. This adds26
read-only operations after exact WCID completion:22 fixed MAC targets and two
MAC/CORE guard pairs. Values remain observations, including counted all-ones;
no new register writes or RF enable. Prior accepted initialization still runs.

Native build, compact/xHCI tests, ASan/UBSan,126 boot-profile tests,102 unchanged
historical Wi-Fi recipes, collector fixtures/55 rejection cases and493 docs
links pass. Exact-stage GPT VM saved/verified logs and returned; missing workspace
returned without starting the probe. Physical observations remain pending.

Ubuntu default and known-good `e47dca3a6b30c6c1` retained; accepted WCID
`71bf958aa703f6b3` retained. Shared-key payload `420bc4d5e39c8e04` removed from
ESP only after full archive equality. Workspace clean243files/613clusters,
unmounted. Payload, audit, tests, VM receipt, collector and run/checkpoint are
archived outside build at
`~/.local/share/srvros/macbookpro11-2/wifi-mac-tail-4ba7963/`.
Actual integration base4a991df; fetchedmainbf1b9a6 includes PR127 storage timing,
and PR128 screenshot receipt work is separate. Next: attended cold adapter cycle,
then collect matching session FIRST and compare all22 observations with reference
semantics. No register acceptance or connectivity claim from capture completion.

### 2026-10-24 — v52 post-WCID MAC native observations

Source `4ba7963`, release `619ce12c1d5d0f0b`, session
`session-00065e9176bea5a2-00` completed. Prepared collector passed unchanged:
26 reads/submissions,22 ordered observations,zero all-ones targets,both exact
MAC0/COREf000f200 guard pairs,all prior prerequisites and both completion markers.
This accepts the bounded capture, not the target register settings or RF.

Three targets match reference defaults:1380=002400ca,1608=0,0a44=0.
Nineteen differ:102c=00031e46,0208=40000050,1204=0000000a,
0124=a0140280,1218/1220=00550055,13b0=2f2f1b1b,13c0=18181818,
1314/1318/131c/1320/1324/13d4/13d8/13dc=0,1238=003700c8,
006c=00a647a4,0070=6b006868. WPDMA0208 has TX/RX enable and busy bits
clear; its clock-gate-disable,writeback and burst configuration differ from the
reference table value30. Do not overwrite these fields just to match that value.

Ubuntu default,consumed one-shot and installed/known-good hashes verified.
Workspace clean245files/616clusters,zero leaks/allocation mismatches,unmounted.
Collection,comparison JSON,run/checkpoint and hashes archived outside build at
`~/.local/share/srvros/macbookpro11-2/wifi-mac-tail-4ba7963/`. No reboot armed.
Next: reconcile the19 differences with reference field semantics and firmware/USB
ownership,then group appropriate MAC writes; keep board power/PA/LDO and calibrated
TX settings separately justified. Physical EFUSE and PHY/RF gates remain open.

### 2026-10-24 — MAC difference reconciliation

Rechecked pinned Linuxv7.0 sources and the saved vendor reference. The19
mismatches partition into4 fixed controls,9 calibrated power settings,4 board
IO/supply settings,1 optional LED and1 DMA configuration. Linux channel-power
setup rewrites ALC channel fields and all eight TX power registers after deriving
and limiting per-rate power; their table defaults are not final operating power.

Selected next group, in reference order:1218/1220 from00550055 to00500055,
13c0 from18181818 to0,1238 from003700c8 to001700c8. The PA-mode changes alter
bits16/18; Linux reads RF PA modes as two-bit rate fields. The vendor reference
identifies13c0 as BB attenuation and1238 bit21 as Bluetooth-abort control.
That commentary is supporting evidence,not a complete Linux field specification.

Prepared fixed32-operation plan: per target read MAC,CORE,DMA and initial target;
write once; read target,MAC,CORE. Total28 reads/four writes. Require exact captured
initial targets,MAC0,COREf000f200 and DMA enable/busy bits clear. Validate exact
readbacks,stop on error,no retries. No native implementation or new payload yet.

DMA40000050 has burst1,writeback and clock-gate-disable versus reference30
burst3; both have enable/busy bits clear. Ownership is unresolved: preserve it.
Defer board IO/supply and calibrated TX settings rather than treating all19
mismatches as bad. Physical EFUSE,private MAC setup and PHY/RF work remain open.
Audit checks/fixtures and hashed sources archived outside build under
`~/.local/share/srvros/macbookpro11-2/mac-tail-audit-636ecf9/`.
Installed release619ce12c1d5d0f0b unchanged,no reboot armed. Actualbase4a991df;
fetchedmain468ca20 includes separate storage work,PR19 remains Wi-Fi owner.

Active scope v53: four fixed PA-mode/attenuation/coexistence settings after v52.
Exact captured initial values,32 operations/28 reads/four writes,MAC/CORE guards
and disabled/idle DMA check per target. No retries or calibrated-power/DMA writes.
Touch mt76/xHCI/main/profiles/tests. Actualbase4a991df; fetchedmain468ca20,
other-machine window capacity work separate. Next checkpoint exact-stage VM.

### 2026-10-24 — v53 fixed MAC controls candidate

Source `88be3c3`,release `add8a479ff406c0d`,installed unarmed. Four settings
1218/1220→00500055,13c0→0,1238→001700c8 require exact captured initial values
both in the preceding snapshot and immediate pre-write reads.32 operations
contain28 reads/four writes with MAC/CORE guards and disabled/idle DMA checks.
No retries,calibrated-power,board-supply,DMA-configuration or RF-enable writes.

Native build,compact/xHCI tests,ASan/UBSan,127 boot-profile tests,104 unchanged
historical recipes,collector fixture/48 rejection cases and493 docs links pass.
Exact-stage GPT VM saved/verified logs and returned; missing workspace returned
without starting the probe. Physical write/readback acceptance remains pending.

Ubuntu default and known-good `e47dca3a6b30c6c1` retained; accepted snapshot
`619ce12c1d5d0f0b` retained. WCID `71bf958aa703f6b3` removed from ESP only after
full archive equality. Workspace clean245files/616clusters,unmounted.
Payload,audit,tests,VM receipt,collector and run/checkpoint archived outside build
at `~/.local/share/srvros/macbookpro11-2/wifi-mac-fixed-88be3c3/`.
Actualbase4a991df,fetchedmain468ca20; unrelated window-capacity work inspected.
Next: attended cold adapter cycle,then collect matching session FIRST and require
four exact before/after values,eight MAC/CORE guards,four DMA checks,all previous
prerequisites and clean recovery. Radio/connectivity remain untested.

### 2026-10-24 — v53 fixed MAC controls native result

Source `88be3c3`,release `add8a479ff406c0d`,session
`session-00065e99d8c02515-00` completed. Prepared collector passed unchanged:
32 operations/submissions,28 reads,four writes,four exact verifications.
1218/1220 changed00550055→00500055,13c0 changed18181818→0,
1238 changed003700c8→001700c8. All eight MAC0/COREf000f200 guard pairs
passed; all four DMA readings remained40000050 with enable/busy bits clear.
Preceding snapshot initial values,all earlier prerequisites and both completion
markers passed. This accepts these four fixed controls,not RF or connectivity.

Ubuntu default,consumed one-shot and installed/known-good hashes verified.
Workspace clean247files/619clusters,zero leaks/allocation mismatches,unmounted.
Collection,run/checkpoint,validation and hashes archived outside build at
`~/.local/share/srvros/macbookpro11-2/wifi-mac-fixed-88be3c3/`. No reboot armed.
Next bounded scope: physical EFUSE usage-map validation needed by the reference
EEPROM fallback,using read-only mode and fixed map range. Logical EEPROM capture
already exists but does not establish physical-map validity. Remaining board
IO/supply,DMA ownership,calibrated power,private MAC setup and PHY/RF remain open.
Do not infer the15 deferred reference-default differences are all mandatory writes.

Active scope: physical EFUSE usage-map engine and host validation. Fixed physical
read mode1 at1e0/1f0 only, bounded polling, reference span semantics, no fuse
programming. Preserve existing logical capture unchanged; native wiring follows
validated engine. Base4a991df,fetchedmain468ca20,PR19 owns this Wi-Fi scope.

### 2026-10-24 — physical EFUSE map engine host validation

Implemented a separate physical usage-map engine; existing logical EFUSE capture
remains unchanged. Exact mode1 commands address only1e0/1f0,preserve other control
fields,and cannot generate programming modes. Each block checks MAC0,COREf000f200
and disabled/idle DMA; polling has a shared1024-read budget with cancellation.
Completion requires matching mode/address,kick clear,and mapped output.

Map analysis uses the reference first-to-last zero span across29 bytes,excluding
three padding bytes. `reference_used` is29-minus-span,not an actual used-entry
count. Preserve upstream no-zero span1 arithmetic but reject uninformative all-ff
and unmapped data explicitly. A complete read can still fail validity; no native
acceptance is implied. Private scratch bytes are cleared after analysis.

Native build,physical/logical EFUSE and firmware-upload host tests,ASan/UBSan and
493 docs links passed. Tests cover every callback/cancellation fault,bounded busy
polling,guard failures,26 mode/address/kick corruption cases,all threshold counts,
fragmented zeros,padding exclusion and fixed command-bit preservation.
An initial fake-register test inconsistently modeled AOUT bits; corrected its
stored control state to match returned values. Production checks were not relaxed.
No native binding,profile,install or reboot in this work block; installed accepted
release remainsadd8a479ff406c0d. Next: add the independent native two-block transport
allowlist after completed v53,then exact-stage VM and hardware validation.
Engine/tests/logs archived outside build under
`~/.local/share/srvros/macbookpro11-2/efuse-map-engine-fec5fd9/`.

Active scope v54: independent native physical EFUSE map transport after accepted
v53. Restrict control commands to mode1 and blocks1e0/1f0, enforce ordered reads,
one-attempt transfers and prior MAC ownership; log summaries only. Touch mt76,
xHCI/main and capture profiles/tests. Base4a991df, fetchedmain468ca20; PR130
window capacity and PR131 Air6,1 battery remain separate. Next checkpoint:
transport fault tests, exact-stage VM and an unarmed hardware candidate.

### 2026-10-24 — v54 native physical EFUSE map preparation

The native binding now requires completed v53 and its exact four readbacks,
all guards and disabled/idle DMA. A separate transport gate consumes each
submission before USB access, validates both preserved mode1 commands and the
ordered control/poll/delay/data sequence, and retains failure/quarantine state.
Only the fixed physical usage map is read; no raw bytes are logged. Summary
validity remains distinct from completed transport and RF/connectivity acceptance.

Host engine/transport fault tests, ASan/UBSan,128 profile tests,106 unchanged
historical Wi-Fi recipes and collector fixtures/38 rejection cases pass. Native
xHCI admission/error tests cover prior-stage refusal and one-shot quarantine.
The four-guard/two-DMA receipts and bounded poll accounting are required by the
prepared collector. Exact-stage VM and native results remain separate gates.
Actualbase4a991df,fetchedmain468ca20. PR130 window capacity and PR131 Air6,1
battery profiles inspected; preserve their independent shared staging changes.

Source `40556a2`,release `b34ed974d2fa3095` installed unarmed after exact-stage
GPT VM normal save/return and missing-workspace refusal/return both passed.
Native build and xHCI regressions passed. Ubuntu remains default; known-good
`e47dca3a6b30c6c1` and accepted v53 `add8a479ff406c0d` retained. Older snapshot
`619ce12c1d5d0f0b` removed from ESP only after full archived-payload equality.
Offline filesystem/allocation checks passed before installation. Candidate,
tests,VM receipts,collector and run/checkpoint are archived outside build under
`~/.local/share/srvros/macbookpro11-2/wifi-efuse-map-40556a2/`.
Next: attended cold adapter cycle; collect the matching new session FIRST after
return. No physical EFUSE validity or Wi-Fi connectivity acceptance yet.


### 2026-10-24 — v54 physical EFUSE map native result

Source `40556a2`,release `b34ed974d2fa3095`,session
`session-00065e9a3fdde0f0-00` passed the prepared collector unchanged. Status0,
complete1,valid1:24 reads,two mode1 read commands,two polls,two blocks,
26 submissions,terminal phase15,no failure. Four MAC0/COREf000f200 guard pairs
and both DMA40000050 readings passed. All earlier prerequisites and both final
completion markers passed. Summary:9 zeros,first480,last488,span9,
reference_used20. No raw map bytes logged. This accepts physical-map validity
on this adapter,not RF/calibrated-power/association/connectivity.

Collection `build/bringup/wifi-efuse-map/collection/collection-0e8ujmdp` archived
with run/checkpoint and hashes under `~/.local/share/srvros/macbookpro11-2/
wifi-efuse-map-40556a2/`. Ubuntu default,consumed one-shot,payload/recovery hashes
verified. Workspace clean249files/622clusters,no leaks or allocation mismatch,
unmounted. No reboot armed. Next: audit private MAC programming and PHY/RF
prerequisites using the validated logical calibration and physical-map evidence;
board IO/supply,DMA ownership and calibrated power remain separately scoped.

Active scope: integrate main d5bc4b3 (storage service-gap fixes and Air6,1 battery
profiles) while retaining all Wi-Fi captures; then audit private MAC setup and
add its bounded plan/host validation. PR130 window capacity remains separate.
Touched merge surfaces include xHCI/main/staging; preserve storage-wait and Air
profiles and exact old Wi-Fi recipes. Installed accepted v54 remains unchanged.
The next MAC step must read factory bytes privately at boot: saved calibration
logs redact offsets4..9. Do not substitute an address from another host or a
portable fixture. RF/TX enable and channel/calibrated-power work remain separate.

### 2026-10-24 — integrated baseline and private MAC plan

Merged actual main `d5bc4b3` at `ee3e03b`,retaining the Pro Wi-Fi dispatch alongside
storage-wait diagnostics and Air6,1 ordinary battery profiles.133 profile tests,
108 byte-identical historical Wi-Fi recipes and full native build/core suite
passed. Installed accepted v54 remains `b34ed974d2fa3095`; newer source does not
inherit that payload's physical acceptance. PR130 window work remains separate.

Reference audit confirms EEPROM logical block0 offsets4..9 supply the factory
address. The existing saved capture redacts them,so future native code must
read a fresh mapped block privately. Implemented the host-only20-target plan:
station/BSSID words1008..1014 and eight unique BSSID table pairs1090..10cc.
Require captured7610/version2/FAE1 and nonzero unicast address; no random fallback.
Twenty guarded one-shot writes/140 reads verify exact results. BSSID high-word
upper bits are preserved; raw address/readback values stay out of result state.
The engine combines1014 fields and visits each masked table slot once; these
final-state equivalences still require native validation. No RF/PHY enable.

Synthetic-address tests and ASan/UBSan cover all160 I/O failures,161 cancellation
boundaries,guard/readback faults,all-ones target refusal,invalid headers/addresses,
BSSID upper-bit preservation and repeat refusal. Private header copy is cleared.
Native private block acquisition,independent admission and transport buffer
clearing remain pending. Next checkpoint: exact-stage merged Wi-Fi VM regression,
then native private-address wiring; no new physical run is armed.

Host-plan source `800afcf`,merged v54 regression release `88bbc5ece4477c0f`:
final native build/mt76/xHCI regressions and133 profile tests passed; all108
historical Wi-Fi and92 mainline recipes compare unchanged. Exact-stage normal
VM saved/verified diagnostics and returned. This is VM-only evidence; no install
or hardware boot was requested. Plan/audit/reference hashes,tests and staged
payload are archived outside build at
`~/.local/share/srvros/macbookpro11-2/mac-address-plan-800afcf/`.
Missing-workspace exact-stage refusal/return also passed. All local validation
is complete for this checkpoint; physical factory-address programming remains
pending native wiring and its own trial. Installed v54 and recovery unchanged.

Active scope v55: fresh private logical block0 acquisition and native station/
BSSID initialization after completed v54. Require mapped header,exact identity,
independent one-shot USB ordering and guard checks; no raw address logs. Touch
mt76/xHCI/main/staging/tests. Actualbase d5bc4b3; inspect fetched bc0edb9 window
capacity changes separately. Native trial follows local and exact-stage checks.

### 2026-10-24 — native private MAC implementation

Added fixed mode0 logical block0 acquisition after completed physical-map
validation,with independent ordered transport admission,bounded polling and
header validity checks. The20-target private address plan now has native
one-attempt admission,exact derived write/readback validation and per-target
guards. Header copy and confirmed descriptor scratch are cleared; diagnostics
contain counters and compact guard receipts only. Private stages explicitly
use DMA-backed USB data to keep address bytes out of immediate descriptors.
Uncertain DMA remains quarantined. A host harness dependency on firmware hash
verification became visible in the new stage test; link the real verifier and
SHA256 rather than stubbing or weakening firmware admission.

Actualbase remains d5bc4b3. Fetchedmain bc0edb9 adds independent window capacity
and collector display metadata; inspected and deferred from this bounded Wi-Fi
candidate. Installed accepted v54 remains unchanged until staging/install checks.

Source `825735c`,release `fbedd261796dd08f` installed unarmed. Native build,
mt76/upload/xHCI regressions,ASan/UBSan,134 profile tests,108 unchanged historical
Wi-Fi recipes and collector fixture/159 rejection cases passed. Private-header
faults clear output; native transport tests reject bad ownership/commands,verify
DMA rather than IDT,and retain timeout quarantine. The combined fixture is61766
bytes,leaving space in the bounded saved exit log. Exact-stage GPT VM normal
save/return and missing-workspace refusal/return passed.

Ubuntu default and known-good `e47dca3a6b30c6c1` retained; accepted physical-map
`b34ed974d2fa3095` retained. Older fixed-controls `add8a479ff406c0d` removed from
ESP only after full archived-payload equality. Workspace clean249files/622clusters,
zero leaks/allocation mismatches,unmounted. Payload,reference audit,tests,VM
receipts,collector and run/checkpoint archived outside build under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-address-825735c/`.
Next: attended cold adapter cycle,then collect matching session FIRST. Require
mapped valid private header,160 address operations/20 exact verifications,all
prior prerequisites,guard groups,save/unmount and recovery. Physical station/
BSSID programming,PHY/RF and connectivity remain unaccepted until their evidence.

### 2026-10-24 — private address trial failed; evidence collected

Source `825735c`, release `fbedd261796dd08f`, session
`session-00065e9aef0b871e-00` saved an explicit failure and returned to Ubuntu.
Private mapped logical header acquisition completed: valid header,12 reads,
one write,one poll,13 submissions and cleared private storage. Address setup
stopped at operation13 (target1,100c verification):13 completed operations,
14 submissions,11 successful reads,two writes,one verified target. The first
station word verified; complete station/BSSID initialization is not accepted.
The saved receipt does not distinguish a USB read failure from a readback
mismatch at this step. Do not infer a mask or weaken exact validation from it.
Both final capture markers explicitly report NO; the strict collector rejected
this run as expected. Firmware upload reports result0 and no quarantine.

Collected under `build/bringup/wifi-mac-address/collection/collection-hxc28qcm`
and archived with failure receipt,run/checkpoint and hashes outside build in
`~/.local/share/srvros/macbookpro11-2/wifi-mac-address-825735c/`.
Ubuntu default,consumed one-shot and installed/recovery payload hashes verified.
Offline workspace checks passed:251 files,625 clusters,no leaks or allocation
mismatches. Workspace unmounted; no reboot armed. Next: add privacy-preserving
transport versus mismatch diagnostics for100c before another bounded trial.
PHY/RF,association and connectivity remain untested.

Active scope: diagnose the failed v55 second-word verification without changing
register values, masks or retries. Add gate failure classification and a captured
USB receipt before later cleanup overwrites controller diagnostics; expose only
comparison booleans and public control bits, never factory-address bits. Touch
mt76/xHCI and their host tests, retaining the same v55 boot recipe. Actual base
is d5bc4b3; fetched main bc0edb9 remains the separate window-capacity change.
The Air desktop API audit branch has no overlapping Wi-Fi changes; PR19 remains
this unit's shared-code scope. Next checkpoint: host fault/privacy checks and
exact-stage normal/missing-workspace rehearsal before a cold native comparison.

Implemented transfer snapshots before cleanup and distinct gate failure codes.
Comparison receipts retain only equality flags and public control halfwords for
100c/1014; private data is cleared before printing. Register writes, exact
readback checks and no-retry behavior are unchanged. Native build,mt76/upload/
xHCI host checks,ASan/UBSan and134 profile tests passed. Tests cover640 individual
readback-bit mismatches,all160 transport failures,guard classification,stale
receipt clearing,short transfer/timeout quarantine and receipt survival after
shared controller metadata changes. Strict collector fixtures reject missing or
altered new receipts and the prior failed native log. Physical diagnosis pending.

Diagnostic source `df4a0c5`,release `d1d3486f342b9121` installed unarmed.
Exact-stage GPT normal save/return and missing-workspace refusal/return passed.
The unchanged v55 recipe includes the new receipts; its combined host fixture
is62029 bytes. Strict-success fixture and failure-classification tests passed;
classification never promotes a failed run. Ubuntu default/EFI order unchanged,
known-good `e47dca3a6b30c6c1` retained,failed `fbedd261796dd08f` retained and
archived. Accepted map payload `b34ed974d2fa3095` removed from ESP only after
full equality with its durable backup. Offline workspace clean251files/625clusters,
zero leaks/mismatches,unmounted. Candidate,collectors,tests,VM receipts and run
record archived outside build under `~/.local/share/srvros/macbookpro11-2/
wifi-mac-address-diagnostic-df4a0c5/`. Await attended cold adapter cycle; collect
matching session before further work. No new physical acceptance or reboot armed.

### 2026-10-24 — MAC diagnostic isolates control-field mismatch

Source `df4a0c5`, release `d1d3486f342b9121`, session
`session-00065e9b2722160a-00` saved the new diagnostic and returned to Ubuntu.
Address initialization still fails at operation13,target1/register100c readback:
13 completed operations,14 submissions,two writes,one verified target.
The dedicated USB receipt reports success,actual4,cc1,residue0,stage3,no timeout
and no quarantine. Gate failure8 identifies an exact readback mismatch: factory
address bits match; public upper control bits expected00ff0000,observed00000000.
Private header acquisition and all prior prerequisite receipts passed. Both final
capture markers explicitly report NO; full station/BSSID setup remains unaccepted.
No factory address bytes were logged. The pinned Linux reference writes U2ME
bits23:16 as ff; this result alone does not establish whether those bits are
readable,writable or device-specific. Audit their semantics before changing masks.

Strict-success collection rejected this run as intended. Classified failure,
full collection `collection-57bm3u7l`,run/checkpoint and hashes are archived under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-address-diagnostic-df4a0c5/`.
Ubuntu default,consumed one-shot and installed/recovery payload hashes verified.
Offline workspace clean253files/628clusters,zero leaks/allocation mismatches;
unmounted,no reboot armed. Next: scoped100c control-field audit/comparison.
PHY/RF,association and connectivity remain untested.

Active scope: audit the100c U2ME mismatch using pinned upstream source and a
read-only Linux comparison on this same adapter. Mask factory-address bytes
before persistence. No Linux register writes,driver reset or changed radio state.
Only prepare a bounded native comparison if evidence supports it; preserve
one-attempt writes and strict address verification. Actual base remains d5bc4b3;
fetched main76a55dd includes independent window capacity and desktop API docs.
PR132 Air6,1 combined services and PR134 POSIX work do not overlap this audit.

Read-only Linux comparison on the attached7392:a711/ASIC76100002/MAC76502000
returned100c upper16=0 and1014 upper16=003f in all four samples. Linux had MACc,
COREf000f200,DMA30; these are live-driver observations,not disabled-state evidence.
Factory address halfwords were masked before persistence; debugfs selector was
restored. Pinned/upstream Linux still writes100c U2MEff,without exact readback.
Together with two cold SrvrOS failures,this supports a distinct v56 comparison:
keep100c write unchanged but require its exact captured readback (private low16,
upper16zero). Reject every other bit change; retain historical v55 semantics.
No claim that U2ME is universally read-only or that RF/connectivity works. Native
binding stays on the existing exact identity/header/map gate. Touch mt76/xHCI,
main,staging and tests; no integration of unrelated main changes in this trial.

Implemented opt-in v56 captured readback profile while retaining v55 unchanged.
The engine and independent gate both require exact100c readback low16/upper0;
requested write still includes U2MEff. Mode mismatch refuses before USB submission.
Host tests pass for both gated and ungated engines:all20 targets/every bit,
160 transfer failures,161 cancellation boundaries,unchanged writes and legacy
refusal. Native build,mt76/upload/xHCI tests,ASan/UBSan,135 staging tests and110
byte-identical historical Wi-Fi recipes passed. Collector fixture requires the
new profile token/receipt and rejects both earlier native failures;62199 bytes.
Next: exact-stage normal/missing-workspace rehearsal and unarmed installation.

Exact-profile source `87395a2`,release `be58d4957e3c4d7a` installed unarmed.
Exact-stage GPT normal capture/save/return and missing-workspace refusal/return
passed. Ubuntu default/EFI order unchanged; known-good `e47dca3a6b30c6c1` retained.
Failed diagnostic `d1d3486f342b9121` retained and archived. Older failed private
address payload `fbedd261796dd08f` removed from ESP only after full equality with
its durable backup. Offline workspace clean253files/628clusters,zero leaks or
allocation mismatches,unmounted. Payload,read-only Linux evidence/reference hashes,
local tests,collector,VM receipts and run/checkpoint are archived outside build in
`~/.local/share/srvros/macbookpro11-2/wifi-mac-control-87395a2/`.
Next: attended cold adapter cycle,then collect matching release/session FIRST.
Require exact-profile marker and all160 operations/20 verifications plus prior
prerequisites,save/unmount and recovery. No native v56 acceptance or reboot armed.

### 2026-10-24 — exact private MAC initialization accepted

Source `87395a2`,release `be58d4957e3c4d7a`,session
`session-00065e9b722cf305-00` passed the strict v56 collector. Fresh private
mapped header completed12reads/1write/1poll/13submissions; valid and cleared.
Address initialization completed160operations/160submissions,140reads,20writes,
20exact verifications,gate failure0. The explicit captured profile records
100c write00ff0000/read00000000; no observed bits are ignored. Station/BSSID
words and eight unique table pairs passed. Forty MAC0/COREf000f200 guard pairs
and20 disabled/idle DMA checks passed. Final USB receipt operation159,actual4,
cc1,residue0,stage3,no timeout/quarantine. All earlier prerequisites and both
completion markers passed. Factory address remains private.

This accepts the scoped station/BSSID initialization on this adapter,including
combined1014 final fields and one pass over eight unique table pairs. It does
not accept RF/channel/power,association or connectivity. Earlier v55 failures
remain preserved; no blanket sibling-device or baseband-table acceptance.

Collection `build/bringup/wifi-mac-control/collection/collection-47tepf09`
archived with run/checkpoint,strict collector and hashes under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-control-87395a2/`.
Ubuntu default,consumed one-shot,installed/recovery payload hashes verified.
Offline workspace clean255files/631clusters,zero leaks/allocation mismatches;
unmounted,no reboot armed. Next: audit board IO/supply and PHY/RF/calibration
prerequisites; retain exact-profile evidence before advancing to radio work.

Active scope: remaining board power/IO prerequisites before PHY antenna/RF work.
Fresh read-only Linux samples on this adapter confirm1204=0,124=a0040080,
6c=00a647b6,70=6b006464; all four match Linux v7.0's MT7610 initialization table.
Accepted v56's native snapshot still has1204=a,124=a0140280,6c=00a647a4,
70=6b006868. The live Linux MAC/DMA state differs and is not a disabled-state
acceptance. Original debugfs selector restored; no Linux device register writes.
Prepare opt-in v57:four fixed writes with exact prior/readback checks,MAC0,
COREf000f200 and idle/disabled DMA around each target,after completed v56.
No RF bank,channel,power-rate,antenna,MAC-enable or DMA-configuration writes.
Actualbase d5bc4b3; fetchedmain e79e220 inspected (Air6,1 combined services plus
independent window/API work). PR134 POSIX does not overlap this scope; PR19 owns
this adapter work. Touch MCU plan,xHCI/main,staging and tests. Preserve the current
accepted payload and verify exact-stage save/return before native installation.

Implemented the opt-in v57 fixed board power/I/O plan with 32 operations,
28 reads, four writes and four exact verifications. Eight MAC0/COREf000f200
guard pairs, four disabled DMA checks and the recorded initial values are
required. The native binding also checks completed v56 ownership and cleared
private header storage; failures stop without retry and preserve uncertain DMA.
Build, mt76 bring-up/upload/compact and xHCI tests passed, including each transfer
failure/cancellation boundary, malformed requests and native ownership refusal.
ASan/UBSan passed; 136 staging tests and 112 unchanged historical recipes passed.
The collector's full prior native receipt plus synthetic board proof is 63591
bytes; 80 missing/duplicate/corrupt cases and the prior run without board proof
are rejected. Linux reference/read-only samples are retained locally. Next:
exact-stage GPT save/return and missing-workspace rehearsal, then unarmed install.
No native v57 acceptance, no reboot armed, and no RF/connectivity claim.

Candidate source `3af56f0`, release `69718a055422766a` is installed unarmed.
Exact-stage GPT capture/save/automatic-return and missing-workspace refusal/return
VM checks passed. Ubuntu remains the default; known-good `e47dca3a6b30c6c1` and
accepted private-MAC payload `be58d4957e3c4d7a` are retained. Failed diagnostic
`d1d3486f342b9121` was removed from the ESP only after full equality with its
durable backup. Offline workspace checks passed with 255 files/631 clusters and
zero allocation leaks/mismatches; the workspace is unmounted. Payload, Linux
reference/sample evidence, tests, strict collector, VM receipts and run/checkpoint
are archived outside build in
`~/.local/share/srvros/macbookpro11-2/wifi-mac-board-3af56f0/`.
Next: attended cold adapter cycle, then collect the matching release/session
first. Require all prior v56 proof plus the four exact board settings and saved
completion/recovery evidence. Physical board-setting acceptance remains pending.

### 2026-10-24 — fixed board power/I/O prerequisites accepted

Source `3af56f0`, release `69718a055422766a`, session
`session-00065e9c42605b54-00` passed the strict v57 collector. All 32 operations
and submissions completed, with 28 reads, four writes and four exact
verifications. PWR_PIN_CFG1204 a->0, IOCFG6 124 a0140280->a0040080,
LDO0 6c a647a4->a647b6 and LDO1 70 6b006868->6b006464 matched both required
initial values and target readbacks. Eight MAC0/COREf000f200 guard pairs passed;
all four DMA checks were40000050 with enable/busy low four bits clear. Every
earlier initialization prerequisite, including private address setup, and both
completion markers passed. RF and connectivity remain NOT_TESTED.

Collection `build/bringup/wifi-mac-board/collection/collection-3m102t0s` is
archived with run/checkpoint, collector and hashes under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-board-3af56f0/`.
Ubuntu default, consumed one-shot and installed/recovery payload hashes verified.
Offline workspace clean257files/634clusters,102directories,zero leaks or
allocation mismatches; workspace unmounted and no reboot armed. Next: audit
PHY antenna selection and RF/calibration prerequisites. Acceptance is bounded to
the four settings on this captured device; no sibling or usable-Wi-Fi claim.

Active scope: antenna prerequisite observation after accepted v57 board setup.
Linux v7.0 antenna selection touches WLAN_FUN_CTRL80,CMB20,CSR_EE_CFG1 104,
COEXCFG0 40 and COEXCFG3 4c, using EEPROM22/24/42 and board band capabilities.
Capture all five cold native values with MAC0/COREf000f200 and disabled DMA at
both boundaries before planning writes. New v58 adds only eleven fixed reads;
no antenna/RF/channel writes or enablement. Touch mt76/xHCI/main, staging and
tests; preserve completed board/private-address prerequisites and prior recipes.
Actual integration base remains d5bc4b3; fetchedmain36080fc adds Air6,1 reopen
documentation since e79e220. PR134 POSIX and Air6,1 service trials are independent
of this bounded PR19 scope. Next checkpoint: host/VM-tested unarmed capture.

The accepted MAC-redacted calibration image confirms mapped EEPROM22=17ff,
24=ffef,34=fd11,36=1004,42=ffff. Linux's single-antenna branch ignores NIC2
diversity flags and derives antenna low07ff. Four fresh read-only Linux samples
give WLANff000413,CMB00e007ff,CFG10033ffef,COEX06f008843,COEX300000019.
WLAN bit4 differs from the prior live Linux capture; no full-register cold
initial value is inferred. Debugfs selector restored, no Linux register writes.
Implemented v58 eleven-read observation with both boundary guard sets and exact
prior board-completion admission. Physical observation remains pending; no new
antenna writes or usable-Wi-Fi acceptance.

Native build and mt76 bring-up/upload/compact/xHCI checks passed. Snapshot tests
cover every read failure, cancellation boundary, guard bit, all-ones refusal and
replay; native tests cover each operation and failed prior-board ownership with
no USB request on refusal. ASan/UBSan,137 staging tests and114 byte-identical
historical recipes passed. Collector fixture uses the full accepted v57 receipt
plus synthetic antenna observations,64354bytes;47 missing/duplicate/corrupt cases
are rejected. Targets are observations, not exact-value acceptance. Next:
exact-stage GPT save/return and missing-workspace rehearsal, then unarmed install.

Antenna snapshot source `1d291a7`, release `4964d0724ea58faa` installed unarmed.
Exact-stage GPT capture/save/return and missing-workspace refusal/return passed.
Ubuntu default and known-good `e47dca3a6b30c6c1` retained; accepted board payload
`69718a055422766a` retained. Older private-MAC payload `be58d4957e3c4d7a` removed
from ESP only after exact comparison to its durable archive. Offline workspace
clean257files/634clusters,zero leaks/mismatches; unmounted. Payload, Linux source
hashes/read-only samples, calibration-word audit, tests, collector, VM receipts
and run/checkpoint archived under
`~/.local/share/srvros/macbookpro11-2/wifi-antenna-1d291a7/`.
Next: attended cold adapter cycle; collect matching release/session FIRST after
return. Require full v57 proof and eleven-read antenna snapshot with both guard
sets. Physical observation pending; no antenna or RF functionality accepted.

### 2026-10-24 — cold antenna prerequisite snapshot completed

Source `1d291a7`, release `4964d0724ea58faa`, session
`session-00065e9ca514ef7a-00` passed the strict v58 collector: eleven reads and
submissions, zero snapshot writes, five ordered observations. Both boundaries
were MAC0/COREf000f200/DMA40000050. Values: WLAN80=ff000003, CMB20=00c01fff,
CFG1 104=0033ffef, COEX0 40=6f008847, COEX3 4c=1. Every prior initialization
prerequisite and both completion markers passed. This accepts observation only.

Mapped EEPROM/reference-derived targets are ff000003/00c007ff/0033ffef/
6f008843/19 respectively: three changed values, two unchanged. Preserve native
WLAN/CMB control bits rather than copying Linux's enabled-driver values. Fresh
mapped-field validation and exact guard/readback checks are needed for the next
bounded antenna-write trial. No antenna function, RF or connectivity acceptance.

Collection `build/bringup/wifi-antenna/collection/collection-dlok17ft` archived
with collector, run/checkpoint and hashes under
`~/.local/share/srvros/macbookpro11-2/wifi-antenna-1d291a7/`.
Ubuntu default, consumed one-shot and installed/recovery payload hashes verified.
Offline workspace clean259files/637clusters,103directories,zero leaks or
allocation mismatches; unmounted,no reboot armed. Next: bounded antenna setup.

Active scope: v59 antenna initialization after completed v58 snapshot. Re-read
only mapped logical EFUSE20/30, require antenna17ff,cfg1ffef,NIC0fd11,NIC11004,
then apply the five reference-order antenna register writes (three changes,
two unchanged), with exact captured starts/readbacks and MAC/CORE/DMA guards.
Preserve native WLAN/CMB unrelated bits; no RF-bank, channel or MAC/DMA-enable
writes. Touch bounded EFUSE/mt76 plans,xHCI/main,staging and tests. Preserve old
EFUSE modes/profile behavior and fit the full saved proof within the exit log.
Actualbase d5bc4b3; fetchedmain36080fc unchanged. PR136 Air6,1 battery deadline
and PR134 POSIX scopes inspected; independent from this PR19 adapter work.

Implemented fresh logical20/30 capture with exact four-field validation, followed
by the five fixed reference-order writes. Forty initialization operations include
35 reads,five writes and five verifications, ten MAC/CORE guard pairs and five
disabled-DMA checks. Separate native gates enforce completed snapshot/board
ownership, mapped-read command modes/order, fresh fields and exact write bytes.
Failures stop without retry; uncertain USB DMA remains quarantined. Old EFUSE
profiles retain their semantics. Native build,mt76 bring-up/upload/compact and
xHCI checks passed, including each new failure/cancellation boundary, all64
configuration field bits, payload mutations, unmapped/control refusals and
no-USB ownership refusal. Bring-up and compact ASan/UBSan checks passed;138
staging tests and116 unchanged historical recipes passed. Full saved-proof
fixture65044bytes with51 negative cases; compact new guard summaries retain
counts and exact per-target before/after values. Next: exact-stage GPT VM normal
and missing-workspace checks,then unarmed installation. Native v59 pending.

Antenna initialization source `eaf911c`, release `28c2ba352d59b843` installed
unarmed. Exact-stage GPT capture/save/automatic-return and missing-workspace
refusal/return passed. Ubuntu default and known-good `e47dca3a6b30c6c1` retained;
completed antenna snapshot `4964d0724ea58faa` retained. Older board payload
`69718a055422766a` removed from ESP only after full equality with its durable
archive. Offline workspace clean259files/637clusters,zero leaks/mismatches;
unmounted. Payload,reference/configuration derivation,host tests,strict collector,
VM receipts and run/checkpoint archived outside build under
`~/.local/share/srvros/macbookpro11-2/wifi-antenna-init-eaf911c/`.
Next: attended cold adapter cycle,then matching release/session collection FIRST.
Require full earlier proof,fresh exact mapped fields and all40 operations/five
exact target verifications. No native antenna-write acceptance or reboot armed.


### 2026-10-24 — antenna register initialization accepted

Source `eaf911c`, release `28c2ba352d59b843`, session
`session-00065e9d031419f8-00` passed the strict v59 collector. Fresh mapped
EFUSE20/30 configuration matched the captured profile: 24 reads, two read-command
writes, two polls, two blocks and 26 submissions; no guard errors. The antenna
engine completed 40 operations/submissions, 35 reads, five writes and five exact
target verifications, with no guard errors. Before/after values in reference order:
WLAN80 ff000003→ff000003, CMB20 00c01fff→00c007ff, CFG1 104 0033ffef→0033ffef,
COEX0 40 6f008847→6f008843 and COEX3 4c 1→19. Full earlier initialization proof
and both completion markers passed. This accepts register setup on this adapter;
RF performance, radio calibration, channels, scanning and connectivity remain untested.

Collection `build/bringup/wifi-antenna-init/collection/collection-0k5432fp`
archived with strict receipts, payload and run/checkpoint under
`~/.local/share/srvros/macbookpro11-2/wifi-antenna-init-eaf911c/`.
Ubuntu default, consumed one-shot and installed/known-good recovery payload hashes
verified. Offline workspace clean: 261 files, 640 clusters, 104 directories,
zero leaks, allocation mismatches or errors. Workspace unmounted; no reboot armed.
The complete exit snapshot is 65045 bytes, leaving only 491 bytes of the 64KiB
limit: resolve proof-log capacity before extending the capture chain.

Next bounded scope: audit RF-bank initialization/calibration transport and the
saved-proof capacity. No new candidate is staged. Fetched main `85d5939` includes
Air6,1 deadline PR136; this trial remains based on `d5bc4b3`. PR134 POSIX and the
Air6,1 startup-timing branch remain separate work; inspect dependencies before
shared implementation rather than attributing their changes to this native run.


Active scope (2026-10-24): extend saved Wi-Fi diagnostic capacity before RF-bank
transport work. Keep ordinary desktop/battery snapshots at 64KiB; opt-in firmware
capture may retain/save 128KiB only if its startup prefix has not already wrapped.
Touch bootlog/workspace logger, the Wi-Fi capture call site, bounded host
collection and tests. No storage scheduling, battery ownership or RF writes.
Fetched main85d5939 and PR134 head e4b7b02; Air6,1 startup-timing4b41c06 owns its
capture profile. Preserve those independent scopes and publish the logger change
through PR19. Current integration base remains d5bc4b3. Next checkpoint: host
boundary/overflow tests, native build and exact-stage save/refusal VM checks.

Scope extension: pair the capacity fix with a fixed read-only RF0:22 MCU probe
(v60) after exact antenna completion. Command10/sequence8 reads address80000016
with zero payload; six MAC/CORE/DMA boundary reads, no RF write or calibration.
Require exact response framing/address and an eight-bit value; preserve response
words on mismatch. Touch mt76 MCU/xHCI and dedicated staging/tests in PR19.

Implemented the fixed RF0:22 read candidate and 128KiB Wi-Fi-only complete
snapshot. Host tests cover all seven transport-operation failures/eight cancel
boundaries, frame/address mutations and all six guard reads. Native binding
checks exact antenna completion, request bytes, order, one submission, refusal
without USB and transfer-error quarantine. Snapshot tests cover expansion at
zero/partial/exact64KiB boundaries, complete128KiB persistence, wrap/lost-prefix
refusal, ordinary-size restoration and collector bounds. RF/bootlog sanitizers
passed. Full synthetic proof is 65730 bytes;55 malformed/truncated receipt cases
refused. Native acceptance remains pending; no radio writes added.


RF read-only candidate source `5fb885e`, release `63fb5a9f9addbb21` installed
unarmed. Native build, RF/compact/xHCI/logger host tests,139 staging tests,
29 collection tests,118 unchanged historical Wi-Fi recipes, RF/bootlog sanitizers
and documentation checks passed. Exact-stage GPT save/offline-file verification/
automatic-return and missing-workspace refusal/return passed; VM confirmed the
128KiB snapshot marker. This rehearses refusal on VM hardware, not RF transport.
Ubuntu default and known-good `e47dca3a6b30c6c1` retained. Accepted antenna payload
`28c2ba352d59b843` retained; older snapshot `4964d0724ea58faa` removed from ESP
only after exact durable backup equality. Offline workspace clean261files/
640clusters/104directories,zero leaks or mismatches; unmounted.
Payload, pinned references, RF derivation, host/VM receipts, collector and
run/checkpoint archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-probe-5fb885e/`.
Next: attended cold adapter cycle, then collect matching release/session FIRST.
Require all preceding initialization proof, six guard reads, one exact RF read
response, no radio writes and a complete extended snapshot. RF register response,
radio function and connectivity remain unaccepted. No reboot is armed.


### 2026-10-24 — read-only RF transport observed

Source `5fb885e`, release `63fb5a9f9addbb21`, session
`session-00065e9d618d5674-00` passed the strict v60 collector on this
MacBookPro11,2/Edimax7392:a711. The fixed RF0:22 request returned one 16-byte
response with words `4a080008/80000016/00000000/4a080008`; the observed value
is zero. All six guard reads/submissions passed: MAC0, COREf000f200 and
DMA40000050 at both boundaries. No RF register writes occurred. Full prior
initialization proof and both completion markers passed. This accepts the
command/response observation only, not calibrated trim or radio function.

The complete exit snapshot is 65730 bytes, including its startup prefix and
128KiB limit marker: 194 bytes beyond the old limit. Collection
`build/bringup/wifi-rf-probe/collection/collection-s6sofics` and strict receipts
are archived with the payload and run/checkpoint under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-probe-5fb885e/`.
Ubuntu default, consumed one-shot and installed/known-good recovery payload
hashes verified. Offline workspace clean: 263 files, 643 clusters, 105 directories,
zero leaks, allocation mismatches or errors. Workspace unmounted; no reboot armed.

Next bounded scope: RF-bank initialization, fresh frequency-offset validation
and calibration; scanning, association/security and connectivity remain pending.
Fetched main `4f8acd2` includes Air6,1 startup-timing PR137 and was inspected,
but is not integrated into this native candidate (integration base `d5bc4b3`).
Preserve its shared startup/staging changes and PR134 POSIX scope during future
integration; do not attribute them to this run.


Active scope (2026-10-24): implement the fixed 44-entry central RF-bank table
following the accepted v60 read transport. Preserve the USB MT7610 settings from
the pinned Linux v7.0 numeric reference, with one ordered write/readback pair per
entry and MAC/CORE/disabled-DMA guards. Do not add arbitrary RF commands, frequency
trim, channel selection or MAC traffic. Touch mt76 MCU/xHCI, dedicated v61 staging,
main dispatch and host/collector tests through PR19. RF write response framing and
register readbacks are candidate contracts until native evidence accepts them.
Fetched main4f8acd2 and PR134 e4b7b02 inspected; actual integration base d5bc4b3.
Air6,1 startup/timing and POSIX remain independent scopes; preserve their changes
during integration. Next checkpoint: fault-tested bounded engine/native admission,
then exact-stage VM and unarmed installation before an attended cold trial.


Implemented the fixed central RF engine and v61 native admission. The 44-entry
plan matches the pinned numeric USB MT7610 reference. Host engine checks cover
all352 I/O failures,353 cancellation boundaries, all response-bit mutations and
short/oversized replies. Native binding tests cover88 exact commands,264 guards,
missing/altered prerequisites, every request-byte bit, duplicate submissions and
DMA quarantine. All140 staging tests pass; the historical profiles retain their
behavior (120 historical recipes byte-identical). Native build and MCU/compact/
bring-up regressions pass, as do ASan/UBSan and documentation checks. The strict
collector rejects1000 malformed proofs; complete fixture76963bytes fits128KiB.
Next: exact-stage VM, then install unarmed with existing recovery. Native central
RF writes pending.


Central RF candidate source `23cb72a`, release `d5f9b767d30876ce`, installed
unarmed. Exact-stage GPT save/offline byte verification/automatic return and
missing-workspace refusal/return passed; extended snapshot marker confirmed.
Ubuntu default and known-good `e47dca3a6b30c6c1` retained, with accepted RF read
payload `63fb5a9f9addbb21` also retained. Older antenna payload `28c2ba352d59b843`
removed from ESP only after full equality with its durable archive. Offline
workspace clean263files/643clusters/105directories, zero leaks/mismatches/errors;
unmounted. Payload, numeric plan/pinned references, host/sanitizer/collector
checks, both VM receipts and run/checkpoint archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-central-23cb72a/`.
Next: attended cold adapter cycle, then collect matching release/session FIRST.
Require all preceding proof and every central RF operation/readback; preserve
failed response/guard receipts. RF write framing/readbacks remain native-pending,
and calibration/connectivity remain untested. No reboot is armed.


### 2026-10-24 — central RF trial stopped at register0:6

Source `23cb72a`, release `d5f9b767d30876ce`, session
`session-00065e9da8109959-00` failed strict completion. RF0:1–5 readbacks matched;
the sixth write echo validated but RF0:6 read back00 instead of41. Its response
words were `4a050008/80000006/00000000/4a050008`. All12 USB exchanges completed
(sent16/received16/cc13/residue1008). State4 stopped at44 operations/45 submissions,
33 guard reads,12 exchanges,6 validated write echoes and5 verified readbacks.
No later entry was attempted; both completion markers are NO. This is saved
failure evidence, not acceptance of the central table or radio function.

The last reached pre-write guard was MAC0/COREf000f200/DMA40000050; its paired
post-read guard was not reached. `guard_errors=77` includes77 unexecuted guard
slots, not77 measured failures. Preserve this distinction in future diagnostics.
Full startup snapshot70943bytes retained. Collection
`build/bringup/wifi-rf-central/collection/collection-cl_8izlk`, strict rejection,
classified failure and recovery receipts are archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-central-23cb72a/`.
Ubuntu default, consumed one-shot and installed/recovery payload hashes verified.
Offline workspace clean265files/646clusters/106directories, zero leaks, allocation
mismatches or errors; unmounted. No reboot armed.

Next bounded scope: investigate RF0:6 zero readback versus write41, including
reference semantics and sequencing, before any continuation or acceptance change.
Separate completed guard observations from unexecuted slots in failure reporting.
Fetched main remains4f8acd2; actual candidate integration base remainsd5bc4b3.
New Air6,1 heap-startup and updated Air7,2 POSIX work remain independent scopes.
Calibration, scanning, association/security and connectivity remain pending.


Active scope (2026-10-24): bounded RF0:6 readback observation after at most the
first six central writes. Keep v61's strict engine/predicate intact; a dedicated
v62 authorizer stops it before operation45, and only an exact sixth-response
state may enter a fixed read-only comparison. Repeat RF0:6 with distinct sequences
and read RF0:1/2/22 controls between/after observations, with disabled MAC/CORE/DMA
guards. No later RF table writes, trim, calibration or traffic. Clarify reached
versus unexecuted guard counts in failure diagnostics. Touch MCU/xHCI, v62 staging,
main dispatch and tests in PR19. Main4f8acd2 inspected; PR138 heap-startup and
PR134 POSIX d5d0fa1 are separate scopes; integration base remainsd5bc4b3.
Next checkpoint: engine/binding fault tests and strict observational collector,
then exact-stage VM and unarmed installation. Observation completion must never
be labeled central-table or RF-function acceptance.


Implemented v62's capped central prefix and five fixed read-only comparisons.
The strict central engine is unchanged. Both the prefix authorizer and native
transfer admission reject operations after the first45 submissions; the observation admits only the
exact sixth-read mismatch or deliberate post-match cancellation. Host tests cover
every response byte/bit, transport/cancellation boundary, all256 observed byte
values and known control mismatches. The full saved-proof fixture is69766bytes;
306 malformed observations are refused, including attempts to claim central
completion. All141 staging tests and122 historical recipe comparisons pass.
Native binding tests, full build, MCU/bring-up regressions, ASan/UBSan and docs
checks pass. Final source regression and exact-stage VM checks precede unarmed
installation; no hardware acceptance or new install yet.


RF0:6 comparison source `c69c064`, release `f44fa8061c8014a5`, installed unarmed.
Final native binding regression and full build passed. Exact-stage GPT capture/
save/offline verification/automatic return and missing-workspace refusal/return
passed. Ubuntu default and known-good `e47dca3a6b30c6c1` retained. Failed central
trial `d5f9b767d30876ce` retained for evidence; accepted RF-read payload
`63fb5a9f9addbb21` removed from ESP only after full durable-backup equality.
Offline workspace clean265files/646clusters/106directories, zero leaks/mismatches
or errors; unmounted. Payload, pinned references, host/sanitizer/collector tests,
both VM receipts and run/checkpoint archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-six-c69c064/`.
Next: attended cold adapter cycle, then collect matching release/session FIRST.
Require bounded prefix and all read-only comparison receipts; classify the three
RF0:6 values independently of central-table acceptance. No reboot armed; native
observation pending, calibration and connectivity untested.


### 2026-10-24 — bounded RF0:6 comparison collected

Source `c69c064`, release `f44fa8061c8014a5`, session
`session-00065e9df83b5391-00` passed the strict v62 observational collector.
RF0:6 read zero in all three observations (sequences5/6/9). Interleaved RF0:1/2/22
controls read1/17/0 with exact framing and sequence/address matching. The separate
comparison completed23 operations/submissions,18 guard reads and5 requests.
All boundaries remained MAC0/COREf000f200/DMA40000050. The capped central prefix
retains state4,44 operations/45 submissions,33 guards,12 exchanges,6 write echoes
and5 verified entries. No later central-table write occurred. Reached guards had
zero errors;77 unexecuted boundary slots are reported separately.

Both completion markers and the complete69763-byte startup snapshot passed.
This accepts the bounded observation only, not central RF initialization or radio
function. Repeated zero with correct controls makes a one-off response mix-up less
likely; RF0:6 semantics and access/initialization requirements remain unresolved.
Next investigate those requirements before changing strict readback acceptance
or continuing the table. Calibration, scanning, association/security and
connectivity remain pending.

Collection `build/bringup/wifi-rf-six/collection/collection-zpb253f4`, strict
receipts and complete evidence are archived with the payload under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-six-c69c064/`.
Ubuntu default, consumed one-shot and current/known-good recovery hashes verified.
Offline workspace clean267files/649clusters/107directories, zero leaks, allocation
mismatches or errors; unmounted. No reboot armed.

Fetched main `27f0870` includes Air6,1 heap-startup PR138; it is not integrated
into this native source (base remains `d5bc4b3`). Air6,1 attended-reopen PR139
and Air7,2 POSIX PR134 remain independent scopes. Preserve their changes on
future integration; this return closeout changes documentation only.

Active scope (2026-10-24): compare the completed v62 MCU observations with five
fixed direct RF-CSR reads (RF0:6/1/2/6/22). Vendor source describes RF0:6 as a VCO
amplitude setting and reads/modifies it during calibration; no zero-read exemption
was found. Use the documented RF-CSR read selector with its write bit clear,
bounded idle/completion polling, exact bank/register identity and disabled
MAC/CORE/DMA guards. No RF data writes or additional central-table entries.
Touch the MCU diagnostic engine, xHCI binding, dedicated v63 staging and tests;
retain historical v61/v62 behavior. Source base remains d5bc4b3; fetched main
27f0870, Air6,1 PR139 and Air7,2 POSIX PR134 fcac611 inspected as separate work.
Next checkpoint: fault-tested fixed selector admission and exact-stage VM, then
unarmed installation. Direct USB RF-CSR access remains a hardware hypothesis;
observation completion must not accept central initialization or calibration.

Implemented fixed RF-CSR selector admission and the v63 comparison after the
completed v62 observation. Host checks cover every normal I/O/cancellation
boundary, delayed/busy polling bounds, malformed identities/controls, state
accounting and no replay. Native binding checks cover every selector bit,
prior-proof/ownership faults and USB quarantine. ASan/UBSan and142 staging tests
pass;124 historical Wi-Fi recipes are byte-identical. The strict full-prefix
collector rejects611 malformed proofs; normal/max-poll fixtures are77072/115161
bytes. Build and exact-stage VM checks precede unarmed installation. Direct
RF-CSR transport and values remain physically untested.

Direct RF-CSR source `697111b`, release `032ade0806c417ab`, installed unarmed.
Full build, MCU/compact/bring-up regressions and documentation checks passed.
The initial exact-stage VM exposed a missing v63 classification in the harness;
`f3f10109` adds that classification without changing the payload. The rerun passed
GPT capture/save/offline byte verification/automatic return, and the separate
missing-workspace refusal/return passed. These are VM recovery checks, not
native RF access acceptance.

Ubuntu default and known-good `e47dca3a6b30c6c1` retained, with completed v62
observation payload `f44fa8061c8014a5` also retained. Failed central candidate
`d5f9b767d30876ce` removed from ESP only after exact durable-backup equality.
Offline workspace clean267files/649clusters/107directories, zero leaks/mismatches
or errors; unmounted. Payload, pinned references, host/sanitizer/collector checks,
both VM receipts and run/checkpoint archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-csr-697111b/`.
Next: attended cold adapter cycle, then collect matching release/session FIRST.
Require the complete prior proof and every direct-selector/poll/guard receipt;
classify disagreement with MCU reads without promoting central RF initialization.
No reboot armed. Calibration and connectivity remain untested.


### 2026-10-24 — direct RF read comparison collected

Source `697111b`, release `032ade0806c417ab`, session
`session-00065e9e44b46d9c-00` passed the strict v63 collector. Direct reads of
RF0:6/1/2/6/22 returned0/1/17/0/0, matching the preceding MCU observations.
Each target required one idle and one completion poll, with exact returned
bank/register identity. All45 operations/submissions,40 reads,5 read selectors
and30 guards passed; every guard remained MAC0/COREf000f200/DMA40000050.
No additional RF data writes occurred. The strict central prefix still reports
state4 and only5 verified entries; it remains unaccepted.

Both completion markers and the full77073-byte startup snapshot passed. This
accepts the bounded direct read transport/observation only. Agreement with MCU
reads points away from reply decoding, but does not distinguish an ignored write
from RF0:6 semantics or missing initialization prerequisites. Next investigate
the write path and prerequisites before changing strict acceptance or continuing
the table. Calibration, scanning, association/security and connectivity remain
pending.

Collection `build/bringup/wifi-rf-csr/collection/collection-p_feovej`, strict
receipts and complete evidence are archived with the payload under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-csr-697111b/`.
Ubuntu default, consumed one-shot and current/known-good recovery hashes verified.
Offline workspace clean269files/652clusters/108directories, zero leaks, allocation
mismatches or errors; unmounted. No reboot armed.

Fetched main `211f17c2` includes Air6,1 attended-reopen PR139; this native source
retains integration base `d5bc4b3`. Air7,2 POSIX PR134 `61fc814a` remains separate.
This return closeout updates evidence/documentation only; no new candidate is
installed or armed.

Active scope (2026-10-24): one fixed direct RF0:6=41 write after reproducing
the complete v63 zero-read profile, followed by an independent direct RF0:6 read.
Use the reference CSR write selector c0000641 and read selector80000600, disabled
MAC/CORE/DMA guards and bounded idle/completion polls. No arbitrary values,
automatic rewrite, later table entries or calibration. This compares write paths;
even a41 readback does not accept the complete central table. Touch MCU/xHCI,
dedicated v64 staging and host/collector tests in PR19. Main211f17c2 and POSIX
PR134 964dbd15 inspected; integration base remainsd5bc4b3. Next checkpoint:
fault-tested one-write admission, then exact-stage VM and unarmed installation.

Implemented the one-write v64 comparison. Host tests cover all256 observed
readback bytes, every normal I/O/cancellation boundary, guards, poll exhaustion,
accounting and replay. Native binding tests cover every selector bit and complete
zero-profile/ownership admission with quarantine on transfer failure. All143
staging tests pass and126 historical Wi-Fi recipes remain byte-identical. The
strict collector rejects203 malformed proofs; the maximum-poll full-prefix
fixture is129097bytes. Exact-stage VM and unarmed installation follow the full
build. The direct RF write and resulting readback are physically untested.


One-write source `4fa96c66`, release `bc805e30aa1454bb`, installed unarmed.
Full build, MCU/compact/bring-up regressions and documentation checks passed.
Exact-stage GPT VM capture/save, offline bytes and clean exFAT verification,
automatic return, and separate missing-workspace refusal/return all passed.
These checks verify recovery mechanics; native direct-write results remain pending.

Ubuntu default and known-good `e47dca3a6b30c6c1` retained, along with the completed
v63 comparison `032ade0806c417ab`. Older v62 `f44fa8061c8014a5` removed from ESP
only after exact durable-backup equality. Offline workspace clean269files,
652clusters,108directories, zero leaks/mismatches/errors; unmounted.
Payload, pinned references, host/sanitizer/collector checks, both VM receipts and
run/checkpoint archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-write-4fa96c6/`.
Next: attended cold adapter cycle, then collect matching release/session FIRST
with the strict v64 collector. Require the full zero-read prerequisite proof,
all guards/selectors/polls and independent readback. Even a `0x41` readback does
not accept the complete central table. No reboot armed.


### 2026-10-25 — one direct RF write comparison collected

Source `4fa96c66`, release `bc805e30aa1454bb`, session
`session-00065e9fb28f464b-00` passed the strict v64 observation collector.
The full prior zero-read profile reproduced. The single direct CSR write
`c0000641` completed with word `40000600`; the independent read selector
`80000600` returned `00000600`, giving RF0:6 value0. Idle/write/read polling each
took one read. All14 operations/submissions,12 reads,2 selectors and9 guard
reads passed, with MAC0/COREf000f200/DMA40000050 throughout. No repeat write or
later central-table entry was attempted.

Both completion markers and the complete79461-byte startup snapshot passed.
This accepts the bounded experiment and observations only. Both direct USB and
MCU writes have now been followed by independent zero readback; changing only
the write transport did not resolve the mismatch. Hardware register semantics
or missing prerequisites remain unresolved. Next audit those prerequisites and
write sequencing against the pinned references before another trial. Keep the
strict central-table check; calibration, scanning, association/security and
connectivity remain pending.

Collection `build/bringup/wifi-rf-write/collection/collection-7jmtwomd`, payload,
strict receipts and recovery evidence are archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-write-4fa96c6/`.
Ubuntu default, consumed one-shot and installed/known-good recovery payload
hashes verified. Offline workspace clean271files/655clusters/109directories,
zero leaks, allocation mismatches or errors; unmounted. No reboot armed.


Active scope (2026-10-25): audit RF0:6 sequencing and add a read-only RF control
snapshot after the reproduced v64 zero-read experiment. Touch fixed MCU capture,
xHCI admission and opt-in v65 staging/collector tests only; no new RF data write,
calibration or packet traffic. The reference encoding matches v64; bypass/control
state remains unobserved natively. Four live Linux read-only debugfs samples show
RF bypass0/1 and setting0 all0, RF_MISC5. Linux is actively carrying Ubuntu's
default route; no driver reset/unbind or RF selector write was used. Live values
are comparison evidence, not an initialization profile to copy.
Fetched main `7a005184` contains merged Air7,2 POSIX PR134 (`c0ef70a8`);
its portable process/desktop work is separate from this RF diagnostic. Native
candidate integration base remains `d5bc4b3`. Next checkpoint: bounded fixed
snapshot with ownership/failure tests, exact-stage VM and unarmed installation.


Implemented opt-in v65 read-only RF controls after the reproduced v64 zero-read
profile. Host checks cover every read/failure/cancellation/guard boundary, prior
state mutation and replay; native checks cover14 positions with24 admission/
transport cases each. All144 staging tests pass;128 historical Wi-Fi recipes are
byte-identical. The strict collector rejects63 malformed snapshot proofs and
accepts the complete worst-poll130084-byte fixture within131072. Build and exact-
stage VM checks precede unarmed installation. Snapshot values remain untested
natively and are not an accepted register profile.


RF control snapshot source `48b79846`, release `6e49ad417d7ea11e`, installed
unarmed. Full build, MCU/compact/bring-up regressions, sanitizer and documentation
checks passed. Exact-stage GPT VM capture/save/offline-byte verification and
automatic return passed; separate missing-workspace refusal/return passed.
Ubuntu default and known-good `e47dca3a6b30c6c1` retained, as is the completed
v64 comparison `bc805e30aa1454bb`. Older v63 `032ade0806c417ab` removed from ESP
only after exact durable-backup equality. Offline workspace clean271files/655clusters/109directories, zero leaks,
allocation mismatches or errors; unmounted.

Payload, pinned reference audit, Linux read-only samples, host/sanitizer/collector
checks, both VM receipts and run/checkpoint are archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-state-48b7984/`.
Next: attended cold adapter cycle, then collect matching release/session FIRST.
Require the complete v64 zero-read proof and all14 snapshot reads/guards; compare
the eight observed targets with Linux without promoting live values to a native
initialization profile. No reboot armed; calibration/connectivity remain pending.


### 2026-10-25 — RF bypass/control snapshot collected

Source `48b79846`, release `6e49ad417d7ea11e`, session
`session-00065ea5459d3f13-00` passed the strict v65 observation collector.
The full v64 zero-read experiment reproduced, followed by all14 fixed snapshot
reads. Both boundaries remained MAC0/COREf000f200/DMA40000050. Target values:

| Register | Native snapshot | Earlier live Linux samples |
| --- | --- | --- |
| WLAN_FUN_CTRL80 | ff000003 | ff000403 |
| CMB_CTRL20 | c007ff | e007ff |
| OSC_CTRL104 | 33ffef | 33ffef |
| PBF_SYS_CTRL400 | 80c00 | 80c00 |
| RF_BYPASS0/1 504/508 | 0/0 | 0/0 |
| RF_SETTING0 50c | 0 | 0 |
| RF_MISC518 | 1 | 5 |

The bypass/setting registers show no live/native mismatch. The reference channel
setup changes RF_MISC bit2 for the external A-band PA; that later band-specific
behavior does not establish an omitted pre-table prerequisite. WLAN bit10 is in
the GPIO input field. CMB bit21 differs without a documented interpretation in
the pinned definitions. Do not copy either live state into native initialization.
The RF0:6 mismatch remains unresolved; next audit power/reset sequencing and
register semantics rather than repeat the same write or relax the table check.
Calibration, scanning, association/security and connectivity remain pending.

Both capture markers and the complete80450-byte startup snapshot passed.
Collection `build/bringup/wifi-rf-state/collection/collection-3bt1aq6d`, strict
receipts, comparison and recovery evidence are archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-state-48b7984/`.
Ubuntu default, consumed one-shot and installed/known-good recovery payload
hashes verified. Offline workspace clean273files/658clusters/110directories,
zero leaks, allocation mismatches or errors; unmounted. No reboot armed.
This accepts the observation only; central RF initialization remains unaccepted.


Active scope (2026-10-25): obtain five fixed RF observations through the live
Linux mt76x0 driver before another native write trial. A temporary local kernel
probe may call the loaded driver's RF read helper for0:6/1/2/6/22 through its
normal MCU mutex. Match this DMI model, exact USB fixture, bus1/port1/interface0,
driver and revision; hold device/runtime-PM references. No driver reset/unbind,
channel change, RF write or arbitrary address interface. Build against the
running kernel headers and inspect the loaded helper ABI before execution;
collect results and immediately unload. This is comparison evidence only.
Touch local ignored Linux probe/evidence and this record; no SrvrOS payload
change or reboot. Fetched main `ed2015a5` contains desktop PR140–142; their
clipboard/window-lifetime scope is separate. Native baseline remains `d5bc4b3`.


### 2026-10-25 — live Linux RF read comparison

The fixed local probe built against the running7.0.0-38-generic headers and
called the exact loaded `mt76x0_rf_rr` helper for0:6/1/2/6/22. Its binary ABI and
USB read dispatch were checked against the loaded module and pinned Linux
reference. It used no private structure offsets, RF data writes, driver
reset/unbind or channel changes. Device matching covered this model, captured
USB descriptor fixture, bus1/port1/interface0, ID/revision and bound driver.
An active-only parent-device runtime-PM reference, interface lock and module
reference protected the calls; each read used the driver's normal MCU mutex.

Results:8/1/17/8/68. Both RF0:6 reads were08; controls1/2 matched native1/17.
RF0:22 was44 after Linux's frequency/channel configuration. The probe unloaded,
the driver stayed bound, the default route matched before/after, and two gateway
pings passed. This accepts only a live Linux observation. It does not establish
what RF0:6 should return immediately after the early41 write before calibration.
It does establish that the final working radio need not retain41 there.
The original strict native engine remains unchanged and the full central table
is still unaccepted.

Two earlier probe loads stopped at interface runtime-PM acquisition with EACCES,
before any RF read. The driver does not advertise interface autosuspend; the
successful probe instead pinned the already-active parent USB device without
resume or PM configuration changes. Both failures are retained as incomplete
evidence, not passes. No security/lockdown policy was disabled.

Successful source/evidence and checksums are archived under
`~/.local/share/srvros/macbookpro11-2/linux-rf-read-7592f25-active-device/`;
the two failed attempts have separate adjacent archives. No new SrvrOS payload
was staged or installed, and no reboot is armed. Next implement a separate fixed
full central-table observation in reference order, with strict command/transport/
guard receipts and complete readback mismatch accounting. Do not treat that
observation as exact register acceptance or proceed to DAC/VCO calibration.


Active scope (2026-10-25): opt-in v66 full central RF table observation. A new
fixed engine writes the44 reference central entries in order, then performs a
separate44-entry read pass. Every command retains pre/post MAC0/COREf000f200/
disabled-DMA guards, exact packet/sequence/reply validation and no replay; reads
are observations with complete mismatch accounting. Preserve the original strict
central engine and v61–65 recipes. Exclude RF0:22 trim, later band tables and
DAC/VCO calibration. The completed antenna/RF0:22 read probe is the prerequisite;
do not rerun the six-write/CSR comparison experiments in this new branch.
Touch MCU/xHCI, v66 opt-in staging, host/native/collector tests and current docs.
Fetched main `a6d43616`; shared desktop/runtime work remains separate from this
RF diagnostic. Native integration base remains `d5bc4b3`. Next checkpoint:
fault-tested fixed two-pass engine, exact-stage VM and unarmed installation.


Implemented separate v66 two-pass central-table observation. Host tests cover
all616 I/O and617 cancellation boundaries, every guard, reply-bit validation,
arbitrary uint8 observations, exact mismatch accounting and proof/replay faults.
Native binding tests cover every command and guard position, every packet bit,
prerequisite faults and DMA quarantine. All145 staging tests pass and130 older
Wi-Fi recipes are byte-identical. The strict collector rejects2393 malformed
full-prefix proofs; four complete readback patterns fit at most102527bytes.
Build, sanitizer/regression and exact-stage VM checks precede unarmed install.
No native results for this table observation yet.


Two-pass RF table source `83152ee0`, release `257e68678e55ba49`, installed
unarmed. Full build, MCU/compact/bring-up and original strict RF engine regressions,
sanitizer and documentation checks passed. Exact-stage GPT VM capture/save,
offline-byte verification and automatic return passed; separate missing-workspace
refusal/return passed. Ubuntu default and known-good `e47dca3a6b30c6c1` retained,
as is completed v65 snapshot `6e49ad417d7ea11e`. Older v64 `bc805e30aa1454bb`
removed from ESP only after exact durable-backup equality. Offline workspace
clean273files/658clusters/110directories, zero leaks/mismatches/errors; unmounted.

Payload, pinned references, live Linux comparison, host/sanitizer/collector checks,
both VM receipts and run/checkpoint archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-table-83152ee/`.
Next: attended cold adapter cycle, then collect matching release/session FIRST.
Require complete v60 prerequisites, all88 exact command replies,176 guard triplets,
44 read observations and exact mismatch accounting. Both completion markers and
the full untruncated prefix are required. Do not promote this observation to
register or radio acceptance, and do not trigger calibration automatically.
No reboot armed; native table results pending.


### 2026-10-25 — full central RF table observation collected

Source `83152ee0`, release `257e68678e55ba49`, session
`session-00065eac64bddcbe-00` passed the strict v66 observation collector.
All44 reference-order writes were acknowledged, then all44 separate reads
completed. All616 operations/submissions,528 guard reads,88 exact16-byte command
replies and176 MAC0/COREf000f200/disabled-DMA triplets passed. Readbacks matched
43 table values; the only mismatch was RF0:6 expected41/observed00, index5 and
mask0000000000000020. No subsequent frequency trim, other RF banks/band tables,
DAC/VCO calibration or packet traffic ran.

The observation localizes the readback discrepancy to RF0:6 across the complete
central table. It accepts transport/guard/observation evidence only; it does not
change the original strict engine, explain RF0:6 semantics, or accept the whole
radio initialization. Next prepare a bounded observation of the next reference
RF bank table, requiring this exact complete central profile; maintain separate
register/function acceptance and do not trigger calibration automatically.

Both completion markers and the full102457-byte startup snapshot passed.
Collection `build/bringup/wifi-rf-table/collection/collection-sklpeom6`, strict
receipts and full evidence are archived with the payload under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-table-83152ee/`.
Ubuntu default, consumed one-shot and installed/known-good recovery payload
hashes verified. Offline workspace clean275files/661clusters/111directories,
zero leaks, allocation mismatches or errors; unmounted. No reboot armed.


Active scope (2026-10-25): v67 fixed68-entry RF bank5 observation, requiring the
exact complete central44-entry profile (43 matches; only0:6 read00). Write bank5
in reference order, then read all68 targets; maintain exact command/guard proof
and mismatch accounting, without register/radio acceptance or calibration.
Reuse the bounded two-pass mechanism with independently fixed central/bank5
plans; preserve all prior recipes and the original strict engine. Touch MCU/xHCI,
opt-in v67 staging/collector tests and startup logging: full receipts require a
separate256KiB opt-in window, preserving64KiB ordinary/128KiB existing capture
behavior and refusing lost prefixes. No unrelated main logging edits overlap as
fetched (`a6d43616`); desktop runtime work remains separate. Integration base is
`d5bc4b3`. Publish fault tests, exact-stage VM recovery and an unarmed candidate
before the next attended cold cycle. No RF0:22 trim, bank6, DAC/VCO or traffic.


Implemented opt-in v67 bank5 capture and the separately fixed256KiB logging
window. Logger tests cover both old sizes, direct/two-step growth, exact old/new
boundaries, complete persistence and lost-prefix refusal. The fixed bank5 engine
covers all952 I/O and953 cancellation boundaries, every reply bit, all guards,
full68-bit mismatch accounting and replay/proof faults. Central-prefix validation
checks every stored command/guard and the exact zero-read profile without a large
stack copy. Native tests cover every bank5 command/guard position, all packet bits,
central prerequisite faults and quarantine. All146 staging tests pass and132 old
Wi-Fi recipes are byte-identical. The strict collector rejects3690 malformed
full-prefix proofs; four complete patterns fit at most159350bytes. Build,
sanitizer/regression and exact-stage VM checks precede unarmed installation.


Bank5 source `8fbd311d`, release `242e84b15d20babc`, installed unarmed.
Full build, MCU/compact/bring-up/original strict engine and logger regressions,
sanitizer and documentation checks passed. Exact-stage GPT VM capture/save,
offline-byte verification and automatic return passed, with the selected
`tail_limit=262144` confirmed in its serial receipt. Separate missing-workspace
refusal/return passed. Ubuntu default and known-good `e47dca3a6b30c6c1` retained,
as is completed central observation `257e68678e55ba49`. Older v65
`6e49ad417d7ea11e` removed from ESP only after exact durable-backup equality.
Offline workspace clean275files/661clusters/111directories, zero leaks,
allocation mismatches or errors; unmounted.

Payload, pinned references, host/sanitizer/logger/collector checks, both VM
receipts and run/checkpoint archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-bank5-8fbd311/`.
Next: attended cold adapter cycle, then collect matching release/session FIRST.
Require the full exact central prerequisite plus136 bank5 command replies,
272 bank5 guard triplets,68 observations and both mismatch fields. Both completion
markers and the full untruncated262144-capacity prefix are required. No automatic
bank6, trim, DAC/VCO calibration or traffic. No reboot armed; native result pending.


## 2026-10-25 — RF bank5 native return

Collected source `8fbd311de9c02a398b992b508d03582d078abed2`, release
`242e84b15d20babc`, session `session-00065ead199515d2-00`. Exact native proof
passed:952 submissions,816 guard reads,136 verified16-byte command replies,
68 write ACKs and68 matching indexed readbacks. All272 MAC/CORE/DMA guard
triplets passed. Low64/high4 mismatch masks are both zero. Central prerequisite
remains complete with43 matches and only RF0:6 expected41/read00.

Both completion markers and the entire159281-byte exit snapshot are present
under the opt-in262144-byte capacity. The generic collector initially rejected
this valid log at its stale128KiB transport limit; updated to256KiB with tests
at old/new boundaries and oversized rejection. All29 collector tests passed.
This transport change does not grant hardware acceptance. Strict native
validation, Ubuntu default/consumed one-shot, payload/recovery hashes and offline
exFAT/allocation checks passed; workspace unmounted and no reboot armed.

Archive: `~/.local/share/srvros/macbookpro11-2/wifi-rf-bank5-8fbd311/`.
Next scope is bounded RF bank6 reference initialization/readback; bank5 readback
success does not accept calibration, channel selection, scan, association,
security or packet traffic. No automatic trim or DAC/VCO work in this run.


## 2026-10-25 — bounded RF bank6 scope

Pro PR19 remains the Wi-Fi owner on this checkout. Fetched main `16bbe494`
adds desktop/help acceptance guidance since `a6d43616`; no overlapping RF/MCU,
xHCI or logger work. Native integration base remains `d5bc4b3`; unrelated
main desktop work is separate. Current branch head `73e35e97` has collected and
archived v67; no outstanding run or armed reboot.

Bounded objective: fixed38-entry reference RF bank6 write/read observation after
full central zero-profile and exact68/68 bank5 proof. Touched scope: existing
private RF-table plan, xHCI capture dispatch, opt-in v68 staging and host/native
fault tests, strict saved-evidence collector and this unit record/current guide.
Use existing256KiB capture; verify complete prefix and exact-stage VM before
install. No VGA/bandwidth/band switching, frequency trim, DAC/VCO calibration,
scanning, association or traffic in this trial. No live Linux adapter reset.


Implemented the fixed38-entry bank6 plan using the existing private two-pass
engine; bank5 completion validates every stored guard/reply and68 exact values
before admission. New tests cover all532 I/O and533 cancellation boundaries,
malformed replies, guard errors, complete mismatch accounting and replay
refusal. Native wrappers exercise all76 command and456 guard positions,
central/bank5 proof faults, packet-bit mutations and USB error quarantine.
The147 staging tests and134 historical-recipe identity checks pass. Strict
saved-prefix fixtures cover all-match, all-different, all-ff and all-zero reads;
2138 malformed complete-prefix cases are refused. Largest fixture is191118bytes,
within existing256KiB capacity. Full build/regression/sanitizer and exact-stage
VM save/return checks precede installation; no native bank6 result yet.


Full xHCI host regression passed with all new bank6 command/guard positions and
prefix-fault/refusal/quarantine cases. Full build and MCU/compact/bring-up,
original strict central engine, two-pass central engine and logger regressions
passed. Documentation links checked cleanly. Exact-stage VM and missing-workspace
recovery checks are next; install remains contingent on those results.


Bank6 source `00f04fb342717a625719ce4e173837d0d948f05d`, release
`7b329891c0606f1c`, installed unarmed. Sanitizer and full build/regression checks
passed. Exact-stage GPT VM saved and byte-verified its capture, passed offline
exFAT checks and returned automatically, with tail_limit262144 confirmed.
Missing-workspace refusal/return also passed. Ubuntu default and known-good
`e47dca3a6b30c6c1` remain; completed bank5 release `242e84b15d20babc` retained.
Older central observation `257e68678e55ba49` removed from ESP only after full
byte-for-byte durable backup equality. Preboot workspace is clean:
277files/665clusters/112directories, zero leaks, mismatches or errors; unmounted.

Payload, pinned references, fault/sanitizer/regression/collector checks, both
VM receipts and run/checkpoint are archived under
`~/.local/share/srvros/macbookpro11-2/wifi-rf-bank6-00f04fb/`.
Ready for attended shutdown/cold adapter cycle. On return collect matching
release/session FIRST and verify every prerequisite and all38 observations,
76 command replies,152 guard triplets, mismatch accounting and complete prefix.
No automatic further tables, trim, DAC/VCO or traffic; native bank6 pending.


## 2026-10-25 — RF bank6 native return

Collected source `00f04fb342717a625719ce4e173837d0d948f05d`, release
`7b329891c0606f1c`, session `session-00065ead98009e52-00`. Exact native proof
passed:532 submissions,456 guard reads,76 verified16-byte replies,38 ACKs and
38 matching indexed readbacks. All152 MAC/CORE/DMA guard triplets passed;
both mismatch fields zero. Central remains43/44 with only RF0:6=00, and
bank5 remains68/68. Full191063-byte startup prefix within262144 capacity and
both completion markers verified; no partial evidence promoted.

Ubuntu default, consumed one-shot and current/recovery payload hashes verified.
Offline exFAT/allocation checks clean279files/669clusters/113directories,
zero leaks, mismatches or errors; workspace unmounted and reboot unarmed.
Archive: `~/.local/share/srvros/macbookpro11-2/wifi-rf-bank6-00f04fb/`.
Next is bounded reference VGA initialization/readback. No VGA/bandwidth/band
settings, trim, DAC/VCO calibration or traffic were added to this native run.
Command/guard/readback observations do not accept calibrated RF/connectivity.


## 2026-10-25 — bounded RF VGA scope

Pro PR19 remains the Wi-Fi owner. Native bank6 v68 is fully collected and
archived at branch head `a333a129`; no outstanding run or armed reboot.
Fetched main remains `16bbe494`; Air7,2 runtime branch advanced to `732c6146`,
which is inspected separately from RF/MCU work. Native integration base remains
`d5bc4b3`. This unit retains bounded Wi-Fi initialization work; unrelated
main desktop work is not mixed into the candidate.

Objective: fixed35-entry reference RF VGA (bank7) write/read observation after
complete central zero-profile, exact68/68 bank5 and exact38/38 bank6 proof.
Touched scope: private RF-table plan and predecessor validator, xHCI capture
wrappers/dispatch, opt-in v69 staging, host/native fault tests and strict
saved-evidence collection. Keep the existing256KiB full-prefix capture.
No bandwidth/band switching, trim, DAC/VCO calibration, scanning, association,
security or packet traffic. Build/local checks and exact-stage VM save/return
must pass before unarmed installation and the next attended cold cycle.


Implemented the fixed35-entry RF VGA plan with no new public arbitrary target
or value API. Complete bank6 validation checks every stored guard/reply and
38 exact readbacks at native admission. Host tests cover all490 I/O and491
cancellation boundaries, malformed reply bits/lengths, guards, full mismatch
accounting, proof mutation and replay refusal. Native tests exercise all70
command and420 guard positions, inherited prefix/bank6 faults, packet mutations
and uncertain-transfer quarantine. All148 staging tests and136 historical
recipe byte-identity checks pass. Full-prefix valid fixtures are at most
219914bytes within the existing256KiB window. Full build/regression/sanitizer
and exact-stage VM checks precede installation; native VGA acceptance pending.


Full build, MCU/compact/bring-up, original strict and two-pass central, bank5
and logger regressions passed. New VGA engine and the complete-bank6 proof
validator both passed ASan/UBSan runs. Documentation links checked cleanly.
Exact-stage GPT save/return and missing-workspace recovery checks precede
installation; no physical VGA result has been accepted.


RF VGA source `22f757bf67ec13fe4f177f58103ac122ef84aec1`, release
`de89daa6bdc3024e`, installed unarmed. Full build, regressions, ASan/UBSan for
the new plan and complete bank6 validator, and documentation checks passed.
All1946 malformed full-prefix collector cases were refused; all four valid
patterns fit at most219914bytes. Exact-stage GPT VM saved and byte-verified
capture, passed offline exFAT checks and returned automatically, with the
selected tail_limit262144 confirmed. Missing-workspace refusal/return passed.

Ubuntu default and known-good `e47dca3a6b30c6c1` remain, as does completed bank6
release `7b329891c0606f1c`. Older bank5 `242e84b15d20babc` removed from ESP only
after complete durable-backup byte equality. Preboot workspace is clean:
279files/669clusters/113directories, zero leaks, mismatches or errors; unmounted.
Payload, pinned reference, local/strict/sanitizer checks, both VM receipts and
run/checkpoint archived at
`~/.local/share/srvros/macbookpro11-2/wifi-rf-vga-22f757b/`.

On return collect matching release/session FIRST. Require full central+bank5+
bank6 proof, all70 VGA command replies,140 guard triplets,35 observations,
complete mismatch accounting and entire startup prefix with both completion
markers. No bandwidth/band switching, trim, DAC/VCO or traffic in this trial.
No reboot armed; native RF VGA results pending.


## 2026-10-25 — RF VGA native return

Collected source `22f757bf67ec13fe4f177f58103ac122ef84aec1`, release
`de89daa6bdc3024e`, session `session-00065eadd298077c-00`. Exact native proof
passed:490 submissions,420 guard reads,70 verified16-byte replies,35 ACKs and
35 matching indexed readbacks. All140 MAC/CORE/DMA guard triplets passed;
both mismatch fields zero. Central remains43/44 with only RF0:6=00; bank5
remains68/68 and bank6 38/38. Full219878-byte startup prefix within262144
capacity and both completion markers verified; no partial evidence promoted.

Ubuntu default, consumed one-shot and installed/recovery payload hashes passed.
Offline exFAT/allocation checks clean281files/673clusters/114directories,
zero leaks, mismatches or errors; workspace unmounted, no reboot armed.
Archive: `~/.local/share/srvros/macbookpro11-2/wifi-rf-vga-22f757b/`.
Next is bounded reference bandwidth/band initialization, followed by frequency
trim and DAC/VCO calibration. This run added no bandwidth/band settings, trim,
DAC/VCO calibration, scanning, association or traffic. Native command/guard/
readback observations do not accept calibrated radio operation/connectivity.


## 2026-10-25 — bounded RF default G20 scope

Pro PR19 owns this Wi-Fi work. VGA v69 is collected/archived at `9162edc1`;
no outstanding run or armed reboot. Fetched main `3647dc7c` grows desktop
scheduler storage; Air7,2 runtime `71f0a69a` adds its desktop evidence. Inspect
these changes separately from RF; this candidate retains native integration
base `d5bc4b3` and does not combine a scheduler capacity change with RF acceptance.

Objective: fixed19-target reference default20MHz/2.4GHz initialization after
complete central zero-profile and exact bank5/bank6/VGA proof. Preserve Linux
reference order:9 selected bandwidth settings then10 G-band settings, followed
by a separate19-read observation pass. Scope: private mixed-bank address plan,
predecessor validator, xHCI wrappers/dispatch, opt-in v70 staging, host/native
fault tests, strict collector and current docs. No caller-provided targets or
plans. Keep256KiB full-prefix logging. No channel-frequency plan, frequency
trim, DAC/VCO calibration, scanning, association/security or packet traffic.
Full local/sanitizer and exact-stage VM checks precede unarmed installation.


Implemented the fixed19-target mixed-bank G20 plan. The private plan selector
retains immutable full addresses for this plan and the original base/register
representation for prior plans. No arbitrary caller plan is exposed. Complete
VGA admission validates all stored guards/replies and35 exact values. Host
tests cover the reference-selected addresses/values, all266 I/O and267 cancel
boundaries, malformed replies, guards, complete mismatch accounting and no
replay. Native tests cover all38 command and228 guard positions, inherited
prefix/VGA faults, packet-bit mutations and uncertain-transfer quarantine.
All149 staging tests and138 historical recipe byte-identity checks pass.
Four complete-prefix fixtures fit at most235613bytes within256KiB. Full
build/regression/sanitizer and exact-stage VM checks precede installation;
no native G20 result accepted yet.


Full build and MCU/compact/bring-up, original strict and two-pass central,
bank5/bank6 and logger regressions passed with the optional private address
representation. New G20 engine and full VGA prerequisite validator both passed
ASan/UBSan. Documentation links checked cleanly. Exact-stage GPT save/return
and missing-workspace checks precede installation; native G20 pending.


Default G20 source `b4be310b1ba3a13e88ecf8067927188d37605f66`, release
`e75a52b56efa282b`, installed unarmed. Full build, Wi-Fi/old-plan/logger
regressions and ASan/UBSan for the mixed-bank engine and complete VGA proof
validator passed. All1079 malformed full-prefix cases were refused; four valid
patterns fit at most235613bytes. Exact-stage GPT VM saved and byte-verified
capture, passed offline exFAT checks and returned automatically with the
selected tail_limit262144 confirmed. Missing-workspace refusal/return passed.

Ubuntu default and known-good `e47dca3a6b30c6c1` remain, as does completed VGA
release `de89daa6bdc3024e`. Older bank6 `7b329891c0606f1c` removed from ESP only
after complete durable-backup byte equality. Preboot workspace clean:
281files/673clusters/114directories, zero leaks, mismatches or errors; unmounted.
Payload, pinned reference, fault/strict/sanitizer/regression checks, both VM
receipts and run/checkpoint archived at
`~/.local/share/srvros/macbookpro11-2/wifi-rf-g20-b4be310/`.

On return collect matching release/session FIRST. Require full prior central+
bank5+bank6+VGA proof, all38 G20 command replies,76 guard triplets,19 indexed
observations, exact mismatch accounting and complete startup prefix with both
completion markers. No channel-frequency plan, trim, DAC/VCO or traffic in
this trial. No reboot armed; native default G20 results pending.


## 2026-10-25 — default G20 native return

Collected source `b4be310b1ba3a13e88ecf8067927188d37605f66`, release
`e75a52b56efa282b`, session `session-00065eae3ac86feb-00`. Exact native proof
passed:266 submissions,228 guard reads,38 verified16-byte replies,19 ACKs and
19 matching indexed readbacks. All76 MAC/CORE/DMA guard triplets passed;
both mismatch fields zero. Central remains43/44 with only RF0:6=00; bank5
remains68/68, bank6 38/38 and VGA35/35. Full235592-byte startup prefix within
262144 capacity and both completion markers verified; no partial evidence
promoted. This is the fixed reference default20MHz/G-band initialization,
not a channel-frequency plan or calibrated radio acceptance.

Ubuntu default, consumed one-shot and installed/recovery payload hashes passed.
Offline exFAT/allocation checks clean283files/677clusters/115directories,
zero leaks, mismatches or errors; workspace unmounted, no reboot armed.
Archive: `~/.local/share/srvros/macbookpro11-2/wifi-rf-g20-b4be310/`.
Next: audit captured board frequency trim and the reference DAC/VCO sequence.
This run added no frequency trim, DAC/VCO calibration, channel-frequency plan,
scanning, association/security or traffic. RF/connectivity remain untested.


## 2026-10-25 — bounded fresh frequency-trim scope

Pro PR19 owns Wi-Fi; G20 v70 is collected/archived at `538b711a`. No outstanding
run or armed reboot. Fetched main remains `3647dc7c`; PR153/Air7,2 runtime
`182d5fc0` edits desktop capture dispatch/staging, not Wi-Fi MCU/EFUSE ownership.
PR154/Air6,1 owns desktop endurance. This candidate uses native integration base
`d5bc4b3`; keep bounded opt-in Wi-Fi changes separate from desktop/scheduler work.

Audit of pinned Linux v7.0 EEPROM code and shared helper confirms base byte3a,
correction bytedb, signed-magnitude correction with bit7 positive, uint8 wrap,
then trim capped atbf. The existing offline auditor incorrectly used two's
complement for frequency correction and temperature metadata; RSSI is ordinary
signed8 and must stay separate. This board has base44/correction00, so its trim
remains44, but temperature bytef8 decodes+120, not the older reported-8. Dated
journals preserve the prior audit; current interpretation is corrected before
any native frequency write.

Objective: fresh mapped logical blocks30/d0 only, requiring this captured
board's NICfd11/1004 and base44/correction00, then one fixed RF0:22 write44
and independent readback after complete G20 proof. No block0/factory-address
bytes, no raw data logging, no OTP programming. Scope: fixed EFUSE read profile,
private decoder/profile validator, guarded RF trim plan, xHCI/staging/tests and
strict collector. Keep256KiB full-prefix capture. DAC/VCO pulses, frequency-plan
tuning, scanning, association/security and traffic remain outside this trial.


Implemented fixed logical30/d0 capture and native single-submission gate,
correct signed-magnitude/uint8-wrap/cap decoding, exact board-profile admission,
and the fixed RF0:22 write/read observation. Fresh fields tests cover every
transfer/cancellation, field bit, command bit, unmapped/control/timeout refusal
and all65536 decoder inputs. Native tests cover every transfer phase of both
fixed blocks, G20 ownership and uncertainty quarantine, plus every RF command/
guard position, all packet bits and103 prerequisite/state faults. All150
staging tests and140 historical recipe byte-identity checks pass. Four valid
full-prefix proofs fit at most236822bytes, and108 malformed proofs are refused.
Full build/regression/sanitizer and exact-stage VM checks precede installation;
no physical trim readback accepted yet. DAC/VCO remains later work.


Full build, MCU/compact, original strict and two-pass RF plans and logger
regressions passed. ASan/UBSan passed for the trim engine, complete G20
validator and full bring-up/fresh-EFUSE suite. Native xHCI tests passed all new
command/guard and fresh-field transfer paths with no-I/O refusal and quarantine.
The historical redacted capture was re-audited without exposing address bytes;
trim remains68 and temperature metadata is+120 calibration units, unvalidated.
Exact-stage GPT save/return and missing-workspace checks precede installation.


Fresh-fields/trim source `b15867d7c87f817e54d5c7bec0c111db7b0c1f74`, release
`4ec251f79c97532b`, installed unarmed. Full build, relevant RF/EFUSE/controller/
logger regressions, offline audit tests and ASan/UBSan for trim, complete G20
proof and fresh EFUSE capture passed. All108 malformed full-prefix cases were
refused; four valid patterns fit at most236822bytes. Exact-stage GPT VM saved
and byte-verified capture, passed offline exFAT checks and returned automatically,
with tail_limit262144 confirmed. Missing-workspace refusal/return passed.

Ubuntu default and known-good `e47dca3a6b30c6c1` remain, as does completed G20
release `e75a52b56efa282b`. Older VGA `de89daa6bdc3024e` removed from ESP only
after complete durable-backup byte equality. Preboot workspace clean:
283files/677clusters/115directories, zero leaks, mismatches or errors; unmounted.
Payload, pinned references and hashes, corrected redacted metadata audit, local/
fault/strict/sanitizer checks, both VM receipts and run/checkpoint archived at
`~/.local/share/srvros/macbookpro11-2/wifi-rf-trim-b15867d/`.

On return collect matching release/session FIRST. Require full prior proof,
fresh mapped30/d0 profile/counters/guards, two RF trim replies, four guard
triplets, exact one-value mismatch accounting and entire prefix with both
completion markers. No DAC/VCO, channel-frequency plan or traffic in this
trial. No reboot armed; native fresh-field/trim results pending.


## 2026-10-25 — fresh board fields and frequency-trim native return

Collected source `b15867d7c87f817e54d5c7bec0c111db7b0c1f74`, release
`4ec251f79c97532b`, session `session-00065eaec03cebac-00`. Exact full-prefix
validation passed. Fresh fixed mapped logical30/d0 fields passed the captured
board profile:24 reads,2 writes,2 polls,2 blocks,26 submissions, phase15,
no failure/guard errors and no raw data logging. RF0:22 expected68/read68;
14 operations,12 guard reads,2 verified replies,1 ACK,1 matching observation
and4 valid MAC/CORE/disabled-DMA guard triplets. Both mismatch fields zero.
Central remains43/44 with only RF0:6=00; bank5 68/68, bank6 38/38, VGA35/35
and G20 19/19. Entire236812-byte startup prefix within262144 capacity and both
completion markers verified. This accepts bounded board-profile and trim
transport/guard/readback evidence; calibrated radio operation is not tested.

Ubuntu default, consumed one-shot and installed/recovery payload hashes passed.
Offline exFAT/allocation clean285files/681clusters/116directories, zero leaks,
mismatches or errors; workspace unmounted, no reboot armed. Durable archive:
`~/.local/share/srvros/macbookpro11-2/wifi-rf-trim-b15867d/`.
Next: implement a bounded reference DAC pulse/VCO initiation with preserved
register bits, guarded replies and appropriate self-clearing observation.
No DAC/VCO pulses, channel-frequency tuning, scanning, association/security
or traffic occurred in this run.

## 2026-10-25 — bounded DAC/VCO initiation scope

Fresh-field/trim native evidence is collected and archived at `2c40c368`.
Fetched main `6e06bfb0` includes desktop endurance/capacity work; open PR155
owns Air7,2 document progress and PR156 Air6,1 audio/USB overlap. Their inspected
changes do not own Wi-Fi RF/MCU state. Keep integration base `d5bc4b3` for this
bounded trial. Pro PR19 owns fixed RF initialization and native evidence.

Objective: reproduce the pinned Linux v7.0 power-up DAC bit7 set/clear/set in
RF0:73 and initiate RF0:4 bit7 VCO calibration, after exact fresh trim proof.
Each modification reads the register afresh and preserves other bits. Capture
one final read of each register; a self-cleared VCO bit is observed, never a
claim of successful radio calibration. Scope: separate fixed MCU engine,
native xHCI ownership, opt-in staging, fault tests and strict saved-log proof.
No channel tuning, MAC enable, scanning, association or traffic. Keep the
256KiB full-prefix capture; do not widen existing profile gates.

Implemented separate fixed DAC/VCO engine and native ownership after complete
exact trim proof. Tests pass all70 I/O and71 authorization boundaries, every
command reply byte/bit, guard/proof mutation, all256 preserved-byte patterns
with VCO clear/still-set final observations, and no replay. Native xHCI tests
pass all70 positions,113 prerequisite/state faults, packet-bit refusal and
transport quarantine. All151 staging tests and142 historical recipe identity
checks pass. Strict full-prefix tests accept three valid observation patterns
at most241410bytes and reject319 malformed proofs. Full build, prior RF/MCU/
EFUSE/logger regressions, ASan/UBSan for new engine/exact trim validator and
499 documentation links pass. Native DAC/VCO results remain pending; next
checkpoint is exact-stage GPT save/return and missing-workspace rehearsal,
then install with Ubuntu and known-good recovery preserved.


DAC/VCO source `cfc0cfddd8475250393a1797f1d327d1c242caf7`, release
`d27f0d2f10e5ae0b`, installed unarmed. Exact-stage GPT VM saved diagnostics,
verified offline bytes/clean exFAT and returned automatically;256KiB snapshot
capacity confirmed. Missing-workspace refusal/return passed. Ubuntu default
and known-good `e47dca3a6b30c6c1` remain, as does completed trim release
`4ec251f79c97532b`. Older G20 `e75a52b56efa282b` removed from ESP only after
complete durable-backup byte equality. Preboot workspace clean:
285files/681clusters/116directories, zero leaks, mismatches or errors; unmounted.
Payload, pinned references, tests/sanitizers, strict collector, both VM receipts
and run/checkpoint archived at
`~/.local/share/srvros/macbookpro11-2/wifi-rf-cal-cfc0cfd/`.

On return collect matching release/session FIRST. Require all prior proof,
fresh fields and exact trim68 before the fixed10-command DAC/VCO plan;
verify70 ordered operations,20 guard triplets, all replies and preserved RMW
bits. Record both final values and VCO-clear observation without claiming
successful radio calibration. Entire prefix and both completion markers are
required. No channel tuning, MAC enable or traffic. No reboot armed; native
DAC/VCO initiation results pending.


## 2026-10-25 — DAC/VCO initiation native return

Collected source `cfc0cfddd8475250393a1797f1d327d1c242caf7`, release
`d27f0d2f10e5ae0b`, session `session-00065eaf209e71b6-00`. Strict full-prefix
proof passed70 operations/submissions,60 guard reads,10 verified replies,
4 ACKs,6 read observations and20 valid MAC/CORE/disabled-DMA guard triplets.
Fresh mapped fields, trim68 and all earlier initialization proof passed.
Observed RF0:73 read/write sequence93/93/93/13/13/93, preserving all lower
bits; final93. RF0:4 initial30, writeb0, finalb0: bit7 remained set at this
immediate observation. VCO-clear flag0 is an observed result, not a transport
failure or proof of completed calibration. Do not promote to calibrated RF.

Entire241416-byte startup prefix within262144 capacity and both completion
markers saved. Ubuntu default, consumed one-shot and installed/recovery hashes
passed. Offline exFAT/allocation clean287files/685clusters/117directories,
zero leaks/mismatches/errors; unmounted, no reboot armed. Durable archive:
`~/.local/share/srvros/macbookpro11-2/wifi-rf-cal-cfc0cfd/`.
Next: bounded delayed read-only VCO observation after this exact initiation
proof. No repeated initiation pulse, forced bit clear, channel tuning, MAC
enable, scanning, association/security or traffic before reviewing that result.

## 2026-10-25 — bounded delayed VCO observation scope

DAC/VCO native results are collected/archived at `ce20d9ae`; no outstanding
run or reboot. Fetched main `2c1ffc6b` includes desktop document and audio/USB
work; PR157 currently records Air7,2 document lifecycle scope. Inspected changes
do not own Wi-Fi RF/MCU state. Retain integration base `d5bc4b3` for Pro PR19.

Objective: after the complete captured DAC/VCO initiation profile, read RF0:4
six times with fixed inter-sample delays10/40/50/100/300/500ms (one second total
requested delay, excluding transport time). No additional RF write or repeated
start pulse. Record all values, first observed clear and final bit state;
a clear alone does not establish calibrated radio operation. Scope: separate
bounded MCU observation/delay state, native single-submission/timer ownership,
opt-in staging, fault tests and full-prefix collector. Keep256KiB capacity,
MAC disabled and existing strict profiles unchanged. No tuning or traffic.

Implemented fixed delayed-read engine, exact captured initiation validator and
native independent delay/command admission. Host tests pass all42 I/O,
49 authorization and6 delay failure boundaries, all64 clear/set patterns,
reply/guard/proof mutations and no replay. Native tests pass all42 positions,
125 prerequisite/state faults, every packet bit, every fixed wait boundary,
wrong duration/order refusal and timer-failure quarantine. All152 staging tests
and144 historical recipe byte-identity checks pass. Strict whole-prefix tests
accept three valid patterns at most244749bytes and refuse229 malformed proofs.
Prior RF/MCU/EFUSE/logger regressions, ASan/UBSan for the new engine and exact
initiation validator, full build and499 local documentation links pass.
Next checkpoint: exact-stage GPT save/return and missing-workspace rehearsal,
then install while retaining Ubuntu/default and known-good recovery. Native
delayed VCO results remain pending; no reboot armed.


Delayed VCO source `70a3f45dca31abf135d87a60b801718c52700201`, release
`cd775567c529231f`, installed unarmed. Exact-stage GPT VM saved diagnostics,
verified offline bytes/clean exFAT and returned automatically;256KiB capture
confirmed. Missing-workspace refusal/return passed. Ubuntu default and
known-good `e47dca3a6b30c6c1` retained, as is completed initiation release
`d27f0d2f10e5ae0b`. Older trim `4ec251f79c97532b` removed from ESP only after
complete durable-backup byte equality. Preboot workspace clean:
287files/685clusters/117directories, zero leaks/mismatches/errors; unmounted.
Payload, references, local/fault/strict/sanitizer checks, both VM receipts and
run/checkpoint archived at
`~/.local/share/srvros/macbookpro11-2/wifi-rf-wait-70a3f45/`.

On return collect matching release/session FIRST. Require full earlier proof
and exact pending initiation profile before six fixed waits/read-only commands:
42 operations,36 guard reads,6 replies/delay receipts,12 guard triplets,
six indexed values, cumulative requested delay and derived first-clear result.
Both completion markers and entire prefix are required. A clear or still-set
bit is an observation; no calibrated-radio claim. No additional RF pulse or
forced clear, tuning, MAC enable or traffic. No reboot armed; native delayed
VCO results pending.


## 2026-10-25 — delayed VCO native return

Collected source `70a3f45dca31abf135d87a60b801718c52700201`, release
`cd775567c529231f`, session `session-00065eafa382c5ce-00`. Strict full-prefix
proof passed42 operations/submissions,36 guard reads,6 verified replies,
6 exact successful delay receipts and12 MAC/CORE/disabled-DMA guard triplets.
Complete captured pending initiation and all earlier proof passed. Six RF0:4
values were30, first_clear=1, final30. The start bit therefore cleared by the
first delayed observation and remained clear at all later samples. Requested
cumulative delays were10/50/100/200/500/1000ms, excluding command/guard/logging
time. This does not establish an exact completion latency or calibrated radio
operation. The delayed phase issued no additional RF writes or start pulses.

Entire244743-byte prefix within262144 capacity and both completion markers
verified. Ubuntu default, consumed one-shot and installed/recovery hashes
passed. Offline exFAT/allocation clean289files/689clusters/118directories,
zero leaks/mismatches/errors; unmounted, no reboot armed. Durable archive:
`~/.local/share/srvros/macbookpro11-2/wifi-rf-wait-70a3f45/`.
Next: audit the remaining pinned reference RX-path/TX-DAC settings and
calibration/channel prerequisites before a bounded next implementation. No
channel tuning, MAC enable, scanning, association/security or traffic accepted.

## 2026-10-25 — one-chain RX-path/TX-DAC scope

Delayed VCO results are collected/archived at `7144c8ed`; no outstanding run
or reboot. Main remains `2c1ffc6b`. PR157 owns Air7,2 application lifecycle;
PR158 owns Air6,1 continuous audio/USB and desktop harness changes. Inspected
files do not own Wi-Fi MCU/RF/baseband state. Pro PR19 retains integration
base `d5bc4b3`; no unrelated desktop changes in this bounded candidate.

Pinned Linux v7.0 mt76x02_init_device selects chainmask101 for MT7610;
fresh NICfd11 confirms one RX/TX stream. mt76x02_phy_set_rxpath clears AGC0
bits4/3, and mt76x02_phy_set_txdac clears TXBE5 bits1/0. Targets2300/2714 are
baseband controls, not MAC enable. Objective: after full delayed VCO clear
proof, fresh-read/modify/write/read these two targets, preserving all other
bits, with MAC/CORE/disabled-DMA guards before and after each target. Scope:
fixed two-target engine, native ownership, staging/tests and strict collector.
No RF data write, channel tuning, TX/RX MAC enable or traffic. Keep256KiB full
capture; refuse all-ones/uncertain transfers and never replay an attempted write.

Implemented two-target masked RMW with18 ordered USB transfers, four complete
MAC/CORE/DMA guard triplets and exact preserved-bit readback. Host tests pass
1024 initial-word pairs, every transfer/cancellation/guard/readback failure,
write-byte/proof mutation, all-ones refusal and no replay. Complete delayed
VCO clear validator passes every reply-bit/guard/counter mutation test. Native
xHCI tests pass all18 positions,134 prerequisite/state faults, every write bit,
no-USB refusal and failure quarantine. All153 staging tests and146 historical
recipe identity checks pass. Strict full-prefix tests accept three valid word
patterns at247004bytes and reject178 malformed proofs. Prior RF/MCU/EFUSE/
logger regressions, full build, ASan/UBSan for the new engine and VCO validator,
499 documentation links and nine pinned-reference hashes pass. Next checkpoint:
exact-stage GPT save/return and missing-workspace rehearsal before installation.
Native path setup remains pending; no reboot armed.


One-chain path source `67e5e5afa4c3d2fd4a8962d0d2f6088e24a2aa35`, release
`0aaf20927414846c`, installed unarmed. Exact-stage GPT VM saved diagnostics,
verified offline bytes/clean exFAT and returned automatically;256KiB capture
confirmed. Missing-workspace refusal/return passed. Ubuntu default and
known-good `e47dca3a6b30c6c1` retained, as is completed delayed VCO release
`cd775567c529231f`. Older initiation `d27f0d2f10e5ae0b` removed from ESP only
after complete durable-backup byte equality. Preboot workspace clean:
289files/689clusters/118directories, zero leaks/mismatches/errors; unmounted.
Payload, pinned reference audit/hashes, local/fault/strict/sanitizer checks,
both VM receipts and run/checkpoint archived at
`~/.local/share/srvros/macbookpro11-2/wifi-path-67e5e5a/`.

On return collect matching release/session FIRST. Require full prior/delayed
VCO clear proof,18 ordered path receipts,16 reads/2 writes/2 exact masked
readbacks and4 complete guard triplets, then both completion markers and
entire prefix. Review actual before/after at2300/2714 with only18/3 cleared.
This phase adds no RF data write, channel tuning, MAC enable or traffic.
No reboot armed; native one-chain path setup results pending.


## 2026-10-25 — one-chain baseband path native return

Collected source `67e5e5afa4c3d2fd4a8962d0d2f6088e24a2aa35`, release
`0aaf20927414846c`, session `session-00065eb0051574ff-00`. Strict full-prefix
proof passed18 ordered USB transfers,16 reads/2 writes/2 verified targets
and4 complete MAC/CORE/disabled-DMA guard triplets. AGC0 at2300 read21400
before and after clearing18; TXBE5 at2714 read0 before and after clearing3.
These bits were already clear; actual before/after words remained unchanged.
Both fixed writes and readbacks completed with all unrelated bits preserved.
Full earlier proof, including fresh one-chain board fields and six delayed
VCO30 observations, passed. This accepts captured-unit baseband path setup;
it does not establish calibrated radio function or network connectivity.

Entire247000-byte startup prefix within262144 capacity and both completion
markers verified. Ubuntu default, consumed one-shot and installed/recovery
hashes passed. Offline exFAT/allocation clean291files/693clusters/119directories,
zero leaks/mismatches/errors; workspace unmounted, no reboot armed. Archive:
`~/.local/share/srvros/macbookpro11-2/wifi-path-67e5e5a/`.
Next: audit remaining pinned calibration/channel prerequisites and define the
next bounded phase. No further RF writes, channel tuning, MAC enable, scanning,
association/security or traffic accepted by this run.

## 2026-10-25 — channel/calibration prerequisite inventory scope

Path native results are collected/archived at `05d2f888`; no outstanding run
or reboot. Main `afb2c3b0` merged Air6,1 automatic audio/USB; PR157 remains
Air7,2 application lifecycle. Inspected shared dispatch/staging changes do not
own Wi-Fi RF/baseband state. Pro PR19 retains integration base `d5bc4b3`.

Pinned v7.0 USB start calls MAC startup before power-on firmware calibration;
that calibration also uses channel context, TX ALC/IBI9 save/restore, timed
firmware commands and optional TSSI MAC transitions. Do not transplant an
isolated calibration command into the current MAC-disabled phase. The remaining
USB init tail configures beacon state, US_CYC low8=1e and TXOP583f; channel setup
has separate BBP bandwidth, band/CCA, synthesizer, gain and power prerequisites.
The user confirmed United States testing; this is context, not permission to
infer EEPROM power validity or enable unbounded channel/transmit operation.

Objective: after exact captured path proof, read a fixed12-target inventory of
USB timing/DMA, beacon timing, BBP bandwidth/path, band/CCA, TX SW/ALC and IBI9.
Pre/post MAC0/COREf000f200/disabled-DMA triplets,18 reads total, no writes.
Scope: fixed read-only engine, native gate, staging/tests and strict collector.
Keep256KiB complete capture. Target values are observations, not accepted
channel/calibration settings. No RF write, MAC enable, tuning, scan or traffic.

Implemented fixed12-target/18-read inventory and exact captured path validator.
Host tests pass every transfer/cancellation/all-ones/guard boundary and proof
mutation/no replay. Exact path validator rejects all before/after-word bits,
guard and completion/counter mutations. Native tests pass all18 reads and144
prerequisite/state faults with no-USB refusal and uncertainty quarantine.
All154 staging tests and148 historical recipe identity checks pass. Strict
whole-prefix tests accept three target patterns at249956bytes and reject204
malformed proofs. Prior RF/MCU/EFUSE/logger regressions, ASan/UBSan for inventory
and exact path validator, full build,499 documentation links and nine new
pinned-reference hashes pass. Next: exact-stage GPT save/return and missing-
workspace rehearsal, then install with Ubuntu/known-good recovery retained.
Native target inventory remains pending; no reboot armed.


Inventory source `4dd830fd0f7254ab1f02fbb6740b8b2dd5cb96cb`, release
`80a6781c0b08290c`, installed unarmed. Exact-stage GPT VM saved diagnostics,
verified offline bytes/clean exFAT and returned automatically;256KiB capture
confirmed. Missing-workspace refusal/return passed. Ubuntu default and
known-good `e47dca3a6b30c6c1` retained, as is accepted path release
`0aaf20927414846c`. Older delayed VCO `cd775567c529231f` removed from ESP only
after complete durable-backup byte equality. Preboot workspace clean:
291files/693clusters/119directories, zero leaks/mismatches/errors; unmounted.
Payload, pinned channel/path reference audits/hashes, local/fault/strict/
sanitizer checks, both VM receipts and run/checkpoint archived at
`~/.local/share/srvros/macbookpro11-2/wifi-channel-state-4dd830f/`.

On return collect matching release/session FIRST. Require full prior/path
proof,18 ordered reads/receipts,12 indexed non-all-ones target observations,
2 guard triplets, both completion markers and entire prefix. Review actual
values against the dependency audit before choosing any further settings.
No target acceptance from transport completion; no additional RF write,
tuning, MAC enable or traffic. No reboot armed; native inventory pending.


## 2026-10-25 — prerequisite inventory failed at IBI9

Collected source `4dd830fd0f7254ab1f02fbb6740b8b2dd5cb96cb`, release
`80a6781c0b08290c`, session `session-00065eb0816b56d4-00`. Strict success
collector refused the missing success markers. A separate failure collector
validated the actual unchanged log's complete earlier proof with required final
failure markers, exact bounded stop, saved prefix and recovery state. No failed
or partial inventory is promoted.

The12 successful USB receipts cover three initial guards, eight valid target
observations and the all-ones IBI9 read:2a4=0,1340=583f,2004=2,2300=21400,
2700=0,132c=4,141c=ffe4,1330=601,2124=ffffffff. USB reportedok1; this is a
value-admission refusal (status4), not a proven transport error. No writes
occurred in this phase. Remaining13b0/1114/238 targets and all postguards were
NOT_READ; zero tail rows must not be interpreted as observations. US_CYC0
still differs from the pinned USB-tail low8=1e requirement; TXOP matches583f.
IBI9 semantics need reference/independent evidence before deciding whether an
all-ones value is legitimate there. No gate relaxed or automatic replay armed.

Entire249442-byte startup prefix within262144 capacity saved with explicit
firmware/laptop completed=NO markers. Ubuntu default, consumed one-shot and
installed/recovery hashes verified. Offline exFAT/allocation clean:
293files/697clusters/120directories, zero leaks/mismatches/errors; unmounted.
Failure evidence archived outside build at
`~/.local/share/srvros/macbookpro11-2/wifi-channel-state-4dd830f/`.
Next: bounded IBI9 semantic/read-path audit, preserving this failure and the
original strict collector. No RF writes, tuning, MAC enable or traffic accepted.

## 2026-10-25 — IBI9 semantic audit and separate observation scope

The v75 failure is collected/archived at `a268708b`; no outstanding run or
reboot. Main `3ab4a385` merged Air6,1 audio-result documentation. PR160 owns
Air6,1 desktop I/O latency; PR157 owns Air7,2 application lifecycle. Inspected
files do not own Wi-Fi baseband/MCU state; Pro PR19 retains base `d5bc4b3`.

Independent live Ubuntu debugfs reads on this exact adapter returned IBI9
ffffffff twelve times in four interleaved samples, with ASIC76100002 and
COREf000f200 throughout, adjacent IBI8/10=0 and normal path/timing values.
Only the debug read selector changed and was restored; no regval write,
reset/unbind/channel change occurred. Driver binding and default route stayed
unchanged. This is the working post-calibration Linux state, not native parity.
Pinned Linux saves IBI9, writesffffff7e during calibration, then restores it;
the vendor reference describes it as MAC control of TXBE1/2. All-ones here is
therefore not sufficient evidence of transport failure or an erased device.

Objective: preserve strict v75 refusal semantics and add a separate read-only
observation profile admitting all-ones only at fixed IBI9 index8, recording an
explicit derived mask. Keep all11 other target all-ones refusals and all MAC/
CORE/DMA guards strict, require complete postguards before inventory completion.
No IBI9 value/register acceptance from this comparison. Scope: policy-separated
engine/native/staging/tests/collector; full earlier profile unchanged. No writes,
tuning, MAC enable or traffic. Keep256KiB capture and archive the Linux audit.

Implemented separate policy flag/admission and observation log family/mask.
Original strict engine still refuses all-ones at every target; new observation
permits it only at IBI9, with tests for cross-policy refusal and derived mask.
Host tests pass every transfer/cancellation/guard/other-target all-ones boundary,
proof mutation and no replay. Native tests pass all18 reads and146 prerequisite/
state/policy faults, including mode/state disagreement, no-USB refusal and
quarantine. All155 staging tests and150 historical recipe identity checks pass.
Strict full-prefix observation tests accept both IBI9 patterns at250247bytes
and reject204 malformed proofs. Full build, prior RF/MCU/EFUSE/logger regressions,
ASan/UBSan for both strict/observation engines and499 documentation links pass.
Independent live Linux audit is archived/hash-verified at
`~/.local/share/srvros/macbookpro11-2/linux-ibi9-audit-d6d1704/`.
Next: exact-stage GPT save/return and missing-workspace rehearsal before install.
Native observation results pending; failed v75 unchanged; no reboot armed.


## 2026-10-25 — Separate IBI9 observation trial installed

Source `5426417e8e5376211a4d012b96cc2482c44ea596`, release
`2351cfa9efee6bb1`, config `ram-usb-wifi-fw-v76-laptop-probe-0e535900-auto-return`.
Exact-stage GPT save/return and missing-workspace refusal/return checks passed.
Installed manifest and known-good recovery `e47dca3a6b30c6c1` verified. Ubuntu
remains default; no reboot is armed. Preboot offline exFAT/allocation checks
passed:293files/697clusters/120directories, zero leaks/mismatches/errors;
workspace unmounted. Prior failed v75 payload/evidence retained and archived;
accepted path payload removed from ESP only after durable archive byte equality.

Separate policy admits raw all-ones only for fixed IBI9 target8, derives the
mask from actual reads and requires every remaining read and postguard. It does
not accept IBI9 settings or calibration; strict v75 refusal remains unchanged.
Full prior path/RF proof remains required. Candidate, helpers, tests, VM receipts
and independent Linux audit are archived outside build at
`~/.local/share/srvros/macbookpro11-2/wifi-channel-observe-5426417/`.
Next: after readiness, cold-cycle Wi-Fi for10s while powered off, keeping the
workspace connected. Automatic capture saves and returns to Ubuntu. Collect
matching release/session first using the separate strict observation collector;
preserve any failure. No photos or commands required during the trial.


## 2026-10-25 — v76 saved return: earlier VCO-clear timing refusal

Source `5426417e`, release `2351cfa9efee6bb1`, session
`session-00065eb0f7e9e13b-00`; automatic disk collection verified the full
243533-byte startup prefix within262144 capacity. Both final markers explicitly
report completed=NO. Strict success collector refused; separate failure checker
validated unchanged log bytes and all earlier proof through DAC/VCO initiation.

Initiation completed70 operations,60 guard reads,10 exact replies,4 ACKs and
20 valid MAC0/COREf000f200/disabled-DMA guard triplets. Values were
147,147,147,19,19,147,48,176,147,48: final RF0:4 bit7 cleared before the
immediate final sample, unlike the historical final176 profile. This observes
self-clear, not successful radio calibration. `wifi_rf_wait_owned` requires
`mt76_rf_cal_pending_profile_valid`, whose exact final176 comparison refused
this variant. Wait reports status1/attempted0/operations0/delays0; later path and
channel observation never executed. Zero wait rows are placeholders, not reads.

Ubuntu default, consumed one-shot and current/recovery manifests verified.
Offline exFAT clean295files/701clusters/121directories, zero leaks/mismatches/
errors; unmounted and unarmed. Failure archived outside build under
`~/.local/share/srvros/macbookpro11-2/wifi-channel-observe-5426417/`.
Next: bounded explicit early-clear admission audit and regression coverage,
retaining strict historical profile and failed candidate. No automatic retry,
MAC enable, channel tuning or traffic; requested IBI9 inventory remains pending.


## 2026-10-25 — Early VCO-clear observation scope

Collected v76 failure at `c399ad56`; no outstanding run or armed reboot.
Fetched main `c4bee5cd` (PR157 desktop lifecycle merge); PR160 owns Air6,1
I/O latency. Their changed files do not own RF/MCU profile logic; retain Pro
integration base `d5bc4b3` for this bounded trial, without unrelated merges.
Objective: separate explicit v77 early-clear observation mode admitting only
captured final RF0:4=30 or historicalb0 with every other initiation byte/guard
unchanged. Keep the original pending-profile predicate and v76 path strict.
Run all six guarded delayed reads in either case; require exact all30 delayed
profile before path/inventory. Do not skip waits or repeat pulses. Scope:
MT76 profile helper, native mode, staging, host/USB tests and strict collector.
No new register writes, channel tuning, MAC enable, calibration acceptance or
traffic. Preserve both failed inventories and durable recovery.


Implemented mode55 with an explicit v77 token and exact cleared-profile helper;
the original pending helper/modes remain strict. Both patterns retain all six
waits and require the captured all30 delayed profile before inventory. Host
calibration tests mutate every profile word/guard and test all256 final bytes.
USB tests pass all42 wait operations,128 prerequisite/state/mode faults, all six
delays/no replay and18 downstream ownership positions. Full xHCI suite,156
staging checks,152 historical recipe identities,292 malformed full-prefix
collector cases, ASan/UBSan and full build pass. Native result remains pending.
Next: exact-stage VM save/return and missing-workspace checks, then install.


## 2026-10-25 — Early-clear-aware inventory installed

Source `f5549279821aeafb9b84226492bf1996523370af`, release
`b2369fab9ef32f11`, config `ram-usb-wifi-fw-v77-laptop-probe-0e535900-auto-return`.
Exact-stage GPT save/return and missing-workspace refusal/return passed.
Installed/current hashes verified; known-good `e47dca3a6b30c6c1` retained;
Ubuntu default unchanged. Failed v76 payload remains installed and archived.
Older failed v75 payload removed from ESP only after full byte equality to its
durable archive. Offline preboot checks clean295files/701clusters/121directories,
zero leaks/mismatches/errors; workspace unmounted and no reboot armed.

Candidate/VM receipts/helpers/tests archived outside build at
`~/.local/share/srvros/macbookpro11-2/wifi-vco-ready-f554927/`.
After readiness, cold-cycle Wi-Fi10s while off and leave workspace connected.
Automatic capture saves results and returns to Ubuntu; no photos or commands.
Collect matching release/session first with `vco-ready/collect_return.py` and
require complete prior proof, mutually exclusive VCO prerequisite receipt,
all six delayed reads and all18 inventory/postguard receipts. Preserve failure;
no automatic calibration, MAC enable, tuning or traffic.


## 2026-10-25 — v77 complete native inventory observation

Source `f5549279821aeafb9b84226492bf1996523370af`, release
`b2369fab9ef32f11`, session `session-00065eb1475df12a-00`. Strict collector
validated the full earlier proof, exact early-clear variant and all six delayed
reads (all30, first_clear1), path setup and complete read-only inventory.
Inventory:18 ordered successful USB receipts,12 target values,6 guard reads in
2 complete triplets, zero writes. Both boundary guards MAC0/COREf000f200/
DMA40000050 passed. The actual values were:

| Address | Observed value |
| --- | --- |
| 02a4 | 00000000 |
| 1340 | 0000583f |
| 2004 | 00000002 |
| 2300 | 00021400 |
| 2700 | 00000000 |
| 132c | 00000004 |
| 141c | 0000ffe4 |
| 1330 | 00000601 |
| 2124 | ffffffff |
| 13b0 | 2f2f1b1b |
| 1114 | 00000640 |
| 0238 | 00c00020 |

Derived all-ones mask100 records only IBI9; no register or calibration value is
accepted by this observation. Historical strict refusal remains unchanged.
Full250364-byte startup prefix within262144 capacity and both success markers
saved. Ubuntu default, consumed one-shot and current/recovery hashes verified.
Offline exFAT/allocation clean297files/705clusters/122directories, zero leaks,
mismatches or errors; unmounted and unarmed. Evidence durably archived at
`~/.local/share/srvros/macbookpro11-2/wifi-vco-ready-f554927/`.
Next: bounded USB-tail timing audit (US_CYC0 still lacks pinned low8=1e), then
channel-specific preparation/calibration dependencies. No automatic writes,
MAC enable, scanning or traffic; Wi-Fi connectivity remains untested.


## 2026-10-25 — USB microsecond-cycle timing scope

Collected v77 at `9819fcd1`; no outstanding run or armed reboot. Fetched main
`b35b8895` (PR160 desktop I/O merge); PR162 owns compositor modal readiness.
No RF/MT76 ownership overlap; retain Pro integration base `d5bc4b3` without
unrelated desktop merges. Pinned Linux USB initialization sets US_CYC_CFG low8
to1e after hardware/beacon initialization; native v77 observed full02a4=0.
Objective: explicit v78 timing-only stage after complete v77 inventory proof,
one guarded fresh-read/masked-write/independent-readback at02a4; preserve upper24,
require captured fresh0 before native write and readback1e. Nine USB transfers,
eight reads/one write, two MAC/CORE/disabled-DMA guard triplets, no replay.
Scope: bounded MT76 engine/native/staging/host+USB tests/strict collector.
No USB DMA changes, RF writes, channel tuning, MAC enable or traffic. Preserve
historical profiles, failures and known-good recovery; native result pending.


Implemented a separate nine-operation USB timing engine with one masked
low-byte write and independent verification. Native mode56 requires exact v77
inventory completion/values/guards and refuses a changed fresh value before
write. Host tests cover1024 upper-word preservation cases, every transfer/
authorization/guard/readback boundary, payload/counter mutation and no replay;
all12 prerequisite words and both full guards are bit-mutated. USB tests pass
all9 transfer positions and157 prerequisite/state/profile faults, no-USB refusal,
no replay and quarantine. Full xHCI suite,157 staging tests,154 historical
recipe identities,105 malformed collector cases, full build and ASan/UBSan pass.
Complete synthetic prefix251631bytes remains within262144 capacity. Next:
exact-stage VM save/return and missing-workspace rehearsal before install.
Native timing readback remains pending; no radio-function acceptance.


## 2026-10-25 — USB cycle timing trial installed

Source `da8b7fce6d4ff8bf6e4f18283f05cf0dc8e23441`, release
`5c86ad12e5f185ca`, config `ram-usb-wifi-fw-v78-laptop-probe-0e535900-auto-return`.
Exact-stage GPT save/return and missing-workspace refusal/return passed.
Installed/current hashes verified; Ubuntu default and known-good recovery
`e47dca3a6b30c6c1` retained. Completed v77 payload remains installed/archived;
failed v76 payload removed from ESP only after full durable archive byte equality.
Offline preboot exFAT/allocation clean297files/705clusters/122directories,
zero leaks/mismatches/errors; workspace unmounted and no reboot armed.

Candidate/helpers/tests/reference audit/VM evidence archived outside build at
`~/.local/share/srvros/macbookpro11-2/wifi-usb-cycle-da8b7fc/`.
After readiness, cold-cycle Wi-Fi10s while off; leave workspace connected.
SrvrOS saves results and returns automatically, no photos or commands needed.
Collect matching release/session first with `usb-cycle/collect_return.py`;
require full earlier proof, exact inventory, nine timing receipts, fresh0 to1e,
postguards and both completion markers. Preserve failures; no automatic DMA
change, RF write, channel tuning, MAC enable or traffic. Timing acceptance pending.


## 2026-10-25 — Native USB microsecond-cycle timing accepted

Source `da8b7fce6d4ff8bf6e4f18283f05cf0dc8e23441`, release
`5c86ad12e5f185ca`, session `session-00065eb17b66ef77-00`. Full earlier
initialization/path proof and exact12-target inventory passed. This run used
the historical pending VCO variant (finalb0; pending1/early_clear0); all six
delayed reads observed30. Together with v77's early-clear result, both explicit
timing variants now have native observation evidence; historical modes unchanged.

USB timing completed9 ordered transfers,8 reads/1 write,1 verified target and
2 complete MAC0/COREf000f200/DMA40000050 guard triplets. Fresh02a4=0, masked
low8 write1e and independent readback1e matched; upper24 remained0. No DMA
change, RF write, MAC enable or traffic occurred in this phase. This accepts
bounded captured-adapter timing setup, not calibrated radio functionality.

Entire251629-byte startup prefix within262144 capacity and both success markers
saved. Strict collector, Ubuntu default, consumed one-shot and current/recovery
hashes passed. Offline exFAT/allocation clean299files/709clusters/123directories,
zero leaks/mismatches/errors; unmounted and unarmed. Evidence durably archived at
`~/.local/share/srvros/macbookpro11-2/wifi-usb-cycle-da8b7fc/`.
Next: audit channel-specific bandwidth/band/synthesizer and calibration ordering
against pinned reference and fresh captured board fields. Scanning, association,
security and Wi-Fi connectivity/UI remain pending; no automatic next write/reboot.


## 2026-10-25 — Firmware bandwidth response observation scope

Collected v78 at `412bced8`, unmounted/unarmed. Fetched main `f4e9fbcb`
(PR162 modal readiness); PR164 owns Air6,1 Files startup/load. No MT76 overlap;
retain Pro integration base `d5bc4b3` without unrelated desktop merges.
Pinned USB channel setup starts with CMD_FUN_SET_OP1, BW_SETTING2,value0
(20MHz), wait-for-reply, before baseband/band/CCA and RF synthesizer setup.
Objective: explicit v79 observation after full v78 timing proof; one fixed
sequence15 command, six strict guard reads in two triplets, bounded16-byte
response observation. Validate framing/sequence, record payload words without
claiming firmware semantic acknowledgment or bandwidth acceptance. No retry,
follow-on baseband/RF/channel programming, MAC enable or traffic. Scope:
MT76 bounded engine/native/staging/tests/collector and channel dependency audit.
Calibration remains dependent on channel, gain/power and MAC lifecycle context;
do not insert isolated calibration commands into disabled-MAC initialization.


Implemented a fixed seven-operation bandwidth response observation engine and
native ownership chain after exact v78 proof. Framing/sequence is checked;
opaque payload words are never promoted to ACK acceptance. Host tests cover
all response lengths0..1025, every header/payload bit, all I/O/cancel/guard/state
boundaries and no replay. Native tests pass all7 positions,157 prerequisite/
state faults, every command bit, no-USB refusal and error quarantine; full xHCI
suite passes. All158 staging tests and156 historical recipe identity checks
pass. Collector validates whole earlier proof and opaque reply consistency;
89 malformed proofs rejected,252903-byte fixture within262144 capacity.
Full build,ASan/UBSan and documentation links pass. Next: exact-stage save/return
and missing-workspace VM checks before installing; native response pending.


## 2026-10-25 — Firmware bandwidth response trial installed

Source `83a869fc23f81602c0038f38dc420cad9dcaa6b5`, release
`4c8563c12db92578`, config `ram-usb-wifi-fw-v79-laptop-probe-0e535900-auto-return`.
Exact-stage GPT save/return and missing-workspace refusal/return passed.
Installed/current hashes verified; Ubuntu default and known-good recovery
`e47dca3a6b30c6c1` retained. Accepted v78 payload remains installed/archived;
v77 payload removed from ESP only after full durable archive byte equality.
Offline preboot exFAT/allocation clean299files/709clusters/123directories,
zero leaks/mismatches/errors; workspace unmounted and no reboot armed.

Candidate/helpers/tests/reference audit/VM evidence archived outside build at
`~/.local/share/srvros/macbookpro11-2/wifi-bw-observe-83a869f/`.
After readiness, cold-cycle Wi-Fi10s while off; leave workspace connected.
Automatic capture saves and returns to Ubuntu; no photos or commands needed.
Collect matching release/session first with `bw-observe/collect_return.py`;
require full earlier proof, fixed sequence15 response framing, seven operation
receipts/two guard triplets, opaque payload consistency, whole prefix and both
completion markers. Preserve failures; audit actual response before semantic
ACK admission. No later baseband/RF/channel programming, MAC enable or traffic.


## 2026-10-25 — Native bandwidth response observation complete

Source `83a869fc23f81602c0038f38dc420cad9dcaa6b5`, release
`4c8563c12db92578`, session `session-00065eb1d61ee32d-00`. Full earlier
initialization, selected VCO-timing profile, exact inventory and USB
cycle setup proof passed. This run used the pending variant; six delayed reads30.
Fixed CMD1/sequence15/function2/value0 sent16 bytes and returned16, cc13/residue1008.
Response words were4a0f0008/00000002/00000000/4a0f0008. Seven operations,
six strict guard reads in two MAC0/COREf000f200/DMA40000050 triplets and one
exchange completed. `ack_validated=0` remains deliberate: the observation
collector validates framing/sequence and payload consistency, not semantics.

Whole252903-byte startup prefix within262144 capacity and both success markers
saved. Ubuntu default, consumed one-shot and installed/recovery hashes passed.
Offline exFAT/allocation clean301files/713clusters/124directories, zero leaks,
mismatches/errors; unmounted and unarmed. Evidence durably archived at
`~/.local/share/srvros/macbookpro11-2/wifi-bw-observe-83a869f/`.
Next: audit this exact ID/value echo and trailer, add the captured reply
prerequisite with mutation tests, then validate bounded20MHz baseband/control-
channel/band/CCA setup in the same next trial. A redundant ACK-only physical
trial is unnecessary. RF synthesizer/channel selection, gain/power/calibration,
MAC enable, scanning and connectivity remain pending; no automatic next reboot.


## 2026-10-25 — Guarded20MHz baseband/channel-layout scope

Collected v79 at `200ab19b`, unmounted/unarmed. Fetched main `296f86bb`
(PR166 Air6,1 stress result); no other open shared-code scope beyond Pro PR19.
Retain integration base `d5bc4b3`. Objective: validate exact captured bandwidth
reply4a0f0008/2/0/4a0f0008, then apply five masked baseband/control-channel/
band/CCA targets. Adjacent same-register reference RMW fields are grouped:
CORE1 mask18 set0; AGC0 mask7300 set1000; TXBE0 mask3 set0; TX_BAND mask7
set4; EXT_CCA maskfff set1e4 (preserve ED_CCA upper nibble). No intervening
reference delay/command is removed by grouping. Native fresh before values
must remain2/21400/0/4/ffe4; expected after2/21400/0/4/f1e4.45 ordered USB
transfers,40 reads/5 writes/5 verified,10 strict guard triplets. Scope: MT76
reply/profile+RMW engine/native/staging/tests/collector. Keep original bandwidth
observation policy and historical recipes unchanged. Full proof must fit256KiB;
no RF synthesizer/channel frequency, calibration, MAC enable or traffic.


Implemented exact captured bandwidth response admission (every response word
and guard bit mutation refused) and five-target grouped RMW engine. Original
framing-only observation remains unchanged. Host tests cover5120 preserved-word
cases, all45 transfer and46 authorization boundaries, readback/guard/all-ones
refusals, packet mutation and no replay. Native tests pass all45 positions and
165 prerequisite/state/profile faults, no-USB refusal/replay and quarantine;
full xHCI suite passes. All159 staging tests and158 historical recipe identities
pass. Full-prefix fixture258463bytes leaves3681bytes in262144;420 malformed
proofs rejected. Full build,ASan/UBSan and documentation checks pass. Next:
exact-stage save/return and missing-workspace VM rehearsal, then install.
Baseband-layout native acceptance remains pending; no RF frequency/calibration.


## 2026-10-25 — Guarded20MHz baseband trial installed

Source `a89af59ab188f1767484b36d3f10e1eebc56c10b`, release
`6a9fd32642e92a7b`, config `ram-usb-wifi-fw-v80-laptop-probe-0e535900-auto-return`.
Exact-stage GPT save/return and missing-workspace refusal/return passed.
Installed/current hashes verified; Ubuntu default and known-good recovery
`e47dca3a6b30c6c1` retained. Completed v79 payload remains installed/archived;
v78 payload removed from ESP only after full durable archive byte equality.
Offline preboot exFAT/allocation clean301files/713clusters/124directories,
zero leaks/mismatches/errors; workspace unmounted and no reboot armed.

Candidate/helpers/tests/reference audit/VM evidence archived outside build at
`~/.local/share/srvros/macbookpro11-2/wifi-bbp20-a89af59/`.
After readiness, cold-cycle Wi-Fi10s while off and leave workspace connected.
Automatic capture saves/returns to Ubuntu; no photos or commands needed.
Collect matching release/session first with `bbp20/collect_return.py`; require
whole earlier proof/exact bandwidth reply prerequisite,45 ordered transfers,
all five captured fresh values/masked readbacks and10 guard triplets, complete
256KiB prefix and both completion markers. Preserve failures; no automatic RF
frequency programming, calibration, MAC enable, scan or traffic.


## 2026-10-25 — Native20MHz baseband/control-channel/band/CCA layout accepted

Source `a89af59ab188f1767484b36d3f10e1eebc56c10b`, release
`6a9fd32642e92a7b`, session `session-00065eb25d7baa54-00`. Whole prior proof
passed, including exact bandwidth response4a0f0008/2/0/4a0f0008 and separate
`wifi-bw-validated: exact_profile=1`. All45 ordered USB transfers,40 reads/
5 writes/5 verified targets and10 MAC0/COREf000f200/DMA40000050 guard triplets
passed. CORE1:2->2, AGC0:21400->21400, TXBE0:0->0, TX_BAND:4->4,
EXT_CCA:ffe4->f1e4. Every independent readback matched; ED_CCA upper nibblef
and all other unmasked bits preserved. This accepts the captured masked layout,
not RF frequency selection or calibrated radio functionality.

The first local collector completed proof validation then failed recovery checks
because a loop variable `b` shadowed the imported boot helper. Corrected only
the local validator variable; original payload and native log unchanged. Added
namespace-preservation regression to the full-prefix fixture/mutation suite;
original/fixed collector and both logs are retained. Strict rerun verified Ubuntu
default, consumed one-shot and installed/recovery hashes, then clean offline
exFAT/allocation:303files/717clusters/125directories, zero leaks/mismatches/errors;
unmounted/unarmed. Entire258464-byte prefix within262144 and both success markers
saved. Evidence durably archived at
`~/.local/share/srvros/macbookpro11-2/wifi-bbp20-a89af59/`.
Next: audit RF band/synthesizer channel setup and fresh gain/power/calibration
ordering. Current proof leaves3680bytes, so plan sufficient complete-prefix
capacity before another large RF phase. No automatic next write/reboot, MAC
enable, scanning or traffic; connectivity remains pending.


## 2026-10-25 — Batched RF-channel prerequisite capture and larger proof scope

Collected v80 at `c4af7dcc`, unmounted/unarmed. Fetched main `46f6212f`
(PR168 Air6,1 handoff); PR169 owns unsaved-dialog shutdown. No MT76/logger
ownership overlap; retain integration base `d5bc4b3`. Before frequency writes,
capture remaining channel prerequisites together: five fixed MAC gain/PA/ALC
registers13c8/13a0/13a8/13b4/518 and fifteen read-only RF targets5:0,6:0,
0:24,0:26..37. MAC block has two guard triplets; every RF command has its own
two. Total116 operations,101 reads (96 guards+5 MAC),15 fixed CMD10 reads,
zero new MAC/RF data writes. Sequence1..15 after completed bandwidth15 response.
Fixed addresses only; no arbitrary caller-selected register/command/payload.

The full prefix already consumes258464/262144bytes. Add explicit512KiB
capture only for new v81 profile, retaining existing64/128/256KiB behavior and
lost-prefix refusal. Raise bounded Ubuntu collection ceiling to512KiB with
byte-preservation and overflow tests. Scope: logger allocation/API/tests,
MT76 read-only engine/native/staging/strict collector and RF dependency audit.
Require exact complete v80 layout and full earlier proof; both final markers,
full prefix and offline/recovery checks. No frequency choice, RF data write,
calibration, MAC enable, scan or traffic. Larger capture must be tested with
this real prerequisite trial rather than a separate reboot only for logging.


Implemented fixed read-only116-operation channel prerequisite engine and native
ownership after exact v80 completion. Host tests cover every I/O/cancellation/
guard/all-ones/reply-bit/length boundary, packet/state checks and no replay.
Native tests pass116 positions,160 ownership/state faults, all packet bits,
no-USB refusal/replay and quarantine; full xHCI suite passes. Logger tests cover
512KiB complete byte persistence, boundary growth/wrap, lost-prefix refusal and
return to ordinary64KiB policy; older sizes still pass. All160 staging,29
collection and160 historical recipe identity checks pass.274891-byte fixture
validates whole earlier proof,512KiB marker and all116 receipts;1089 malformed
bounded proofs and3 full-prefix failures rejected. Full build andASan/UBSan
pass. Next: exact-stage GPT save/return and missing-workspace rehearsal before
install. RF values/native larger-capture acceptance remain pending.


## 2026-10-25 — Batched channel prerequisite/full-capture trial installed

Source `74d1599e14ee1170b6d1fe1b6827eacd1c475b66`, release
`dde8fcb03f3ea0d5`, config `ram-usb-wifi-fw-v81-laptop-probe-0e535900-auto-return`.
Exact-stage GPT save/return and missing-workspace refusal/return passed; VM
receipt confirms524288 capture selection. Installed/current hashes verified;
Ubuntu default and known-good recovery `e47dca3a6b30c6c1` retained. Accepted
v80 payload remains installed/archived; v79 payload removed from ESP only after
full durable archive byte equality. Offline preboot exFAT/allocation clean
303files/717clusters/125directories, zero leaks/mismatches/errors; workspace
unmounted and no reboot armed.

Candidate/helpers/tests/reference audit/VM evidence archived outside build at
`~/.local/share/srvros/macbookpro11-2/wifi-channel-probe-74d1599/`.
After readiness, cold-cycle Wi-Fi10s while off and leave workspace connected.
Automatic capture saves/returns to Ubuntu; no photos or commands needed.
Collect matching release/session first with `channel-probe/collect_return.py`;
require whole earlier proof,116 receipts/32 guards/5 MAC+15 RF observations,
whole512KiB-capacity prefix and both completion markers. Preserve failures;
no automatic RF data write/frequency selection,calibration,MAC enable or traffic.


### 2026-10-25 — v81 native batched channel prerequisite result

Source `74d1599e14ee1170b6d1fe1b6827eacd1c475b66`, release
`dde8fcb03f3ea0d5`, session `session-00065eb2f077e6da-00`, passed strict
whole-chain collection after the attended cold adapter power cycle. All116
ordered receipts,101 USB reads,15 exact MCU replies,32 disabled-MAC guard
triplets and20 indexed observations passed. No new MAC/RF data writes occurred.

Fresh MAC values (hex):13c8=0,13a0=0,13a8=6c6c6c6c,13b4=c9540000,518=1.
RF5:0=0,6:0=0,0:24=33(hex),0:26..37=0. These are observations; zero
synthesizer fields do not establish channel readiness. Full prior exact BBP20,
bandwidth response and initialization proof passed. Both completion markers
and the entire274888-byte startup prefix survived the new524288-byte capture,
including bytes beyond the prior262144 limit.

Ubuntu remained default; one-shot consumed; current and known-good
`e47dca3a6b30c6c1` recovery manifests matched durable backups. Offline exFAT
checks:305files,722clusters,126directories, zero leaks/mismatches/errors;
workspace unmounted and reboot unarmed. Full evidence retained outside build
in the local `wifi-channel-probe-74d1599` archive. Next: audit the pinned
channel-band/synthesizer sequence against fresh observations, then implement
and locally verify a bounded candidate. No frequency/calibrated-radio,
scanning, association/security, connectivity or network-UI acceptance claimed.


### 2026-10-25 — bounded v82 band implementation and local verification

Implemented pinned reference-order2GHz band preparation after exact v81 proof:
68 bank5 replay targets then RF5:0=45,6:0=44(hex),70 independent readbacks;
only exact completion admits MAC13c8=00050007 and13a0=003e0002 from fresh0,
with separate readbacks.980 RF operations/840 guards/140 replies/280 triplets;
18 MAC transfers/16 reads/2 writes/2 verified/4 triplets. No synthesizer tuning,
power selection, MAC enable or traffic. Fixed-plan storage grows from68 to70
entries; historical plans and predicates remain unchanged.

Host tests cover every RF/MAC transfer and cancellation boundary, guard and
reply faults, both mismatch masks, proof mutation and no replay. Native xHCI
admission checks cover980 RF and18 MAC positions plus162 prerequisite/state
faults, every command payload bit, refusal before USB and failure quarantine.
Strict collector fixture402659bytes preserves the full prior proof and rejects
4247 malformed captures. All162 historical boot recipes remain byte-identical;
161 staging tests pass. Pinned reference table/order independently compared.
Build/exact-stage VM/install checkpoint follows; native acceptance pending.


### 2026-10-25 — v82 installed, native result pending

Exact source `83edd6beefe281b351f335051282ef8ad1bdf707`, release
`7e269653eebe03c3`, config `ram-usb-wifi-fw-v82-laptop-probe-0e535900-auto-return`.
Full build, directed/new and earlier MT76 engine regressions, complete xHCI
suite, ASan/UBSan, staging/history and strict collector checks passed. Exact
GPT-stage VM saved/byte-verified diagnostics, cleanly unmounted and reset;
missing-workspace VM refused the probe and returned. VM confirms512KiB capture.
These validate the workflow/refusal paths; they do not simulate MT7610 RF.

Installed/current/recovery manifests and all payload bytes matched durable
backups. Ubuntu remains default, known-good `e47dca3a6b30c6c1` retained. Old
ESP v80 `6a9fd32642e92a7b` removed only after byte-for-byte archive equality;
v81 remains installed alongside recovery. Workspace preboot checks clean:
305files/722clusters/126directories, zero leaks/mismatches/errors; unmounted.
Run record and candidate evidence retained outside build. No reboot armed.
Await readiness for cold Wi-Fi power cycle; collect matching session FIRST
on return. No commands or photos needed; capture saves and returns automatically.


### 2026-10-25 — v82 native band setup/readback result

Source `83edd6beefe281b351f335051282ef8ad1bdf707`, release
`7e269653eebe03c3`, session `session-00065eb38500e72c-00`, passed strict
whole-chain collection after the attended cold adapter power cycle. All68
replayed bank5 entries and RF5:0=45,6:0=44(hex) matched independent reads:
980 ordered operations,840 guard reads,140 exact MCU replies,70 ACKs,
70 matching observations and280 MAC0/COREf000f200/DMA40000050 triplets;
low64/high6 mismatch masks zero. Only then the two MAC words wrote/read back
13c8=00050007 and13a0=003e0002, both from fresh0:18 ordered transfers,
16 reads/2 writes/2 verified and4 strict guard triplets.

The entire402661-byte startup prefix and both completion markers were saved
within512KiB; all earlier exact initialization/BBP20/v81 proof passed. Ubuntu
default, consumed one-shot and current/known-good recovery payload hashes
verified. Offline workspace clean307files/728clusters/127directories, zero
leaks/mismatches/errors; unmounted, reboot unarmed. Evidence retained outside
build in `wifi-band2g-83edd6b`. This accepts bounded reference-order band setup
and readbacks on this adapter, not calibrated RF or connectivity. Next: audit
and implement fixed channel-frequency/synthesizer fields against captured
values, preserving disabled MAC/DMA and complete evidence. No new run armed.


### 2026-10-25 — fixed channel1 synthesizer candidate scope

Pinned Linux v7.0 channel1/2412MHz frequency row mapped through declared fields
and source masks; independent audit verifies all21 steps and fresh-value replay.
Full writes37/36/35/34/33; ordered masks32[e0,1f],31[e0,1f],30[80,7c,02],
29[ff],30[01],28[c0,30,0c],26[ff],27[ff],28[03],24[03], all in RF bank0.
Fresh read, fixed masked/full write and independent exact readback per step;
require original captured24=33hex and26..37=0 with earlier in-phase writes
accounted for. No VCO/SDM pulse or arbitrary channel selector.

441 operations,378 strict guard reads,63 MCU replies,21 ACKs,42 observations
and126 disabled-MAC guard triplets; starts sequence6 after band plan ends5.
Whole prior proof,512KiB prefix and both markers required. Full synthetic
fixture458428bytes;1993 malformed proofs refused, boot-module namespace
preserved. Engine tests cover all transfer/cancel/guard/reply-bit/length
boundaries,5376 masked byte inputs, complete-proof mutation and no replay.
Native/VM/install evidence follows; physical result pending. No frequency
readiness, calibrated-radio or connectivity acceptance; no bandwidth/band
post-tune tail, power selection, MAC enable, scan or traffic.


Local v83 checks passed: full build; synth1 engine with all441 transfer and
442 cancellation boundaries, all reply-bit/length faults and5376 masked inputs;
complete native xHCI suite including441 positions/183 prerequisite-state faults,
all packet bits/refusal/no replay/quarantine; ASan/UBSan;162 staging tests and
164 historical recipes unchanged. Full-chain collector fixture458428bytes,
1993 malformed proofs rejected with namespace preservation. Exact-stage VM
and installation follow; no physical synthesizer result yet.


### 2026-10-25 — v83 installed, native result pending

Exact source `0ee9efb3a582cdddc3293f09664c9b60282fcc0e`, release
`be37d545e93f1aed`, config `ram-usb-wifi-fw-v83-laptop-probe-0e535900-auto-return`.
Full build, directed engine and complete xHCI suite, ASan/UBSan,162 staging
tests,164 historical recipes, independent reference mapping and strict
collector mutation checks passed. Exact GPT-stage VM saved and byte-verified
diagnostics, cleanly unmounted and reset. Missing-workspace VM refused the
probe and returned. Full512KiB capture selection confirmed. VM evidence tests
the workflow/refusal path; it does not simulate synthesizer behavior.

Installed/current and known-good `e47dca3a6b30c6c1` recovery payloads matched
durable backups. Ubuntu remains default; reboot unarmed. ESP v81
`dde8fcb03f3ea0d5` removed only after full archive byte equality; v82 band
payload retained alongside recovery. Workspace offline checks clean:
307files/728clusters/127directories, zero leaks/mismatches/errors; unmounted.
Run/checkpoint and candidate evidence archived outside build. Await readiness
for cold Wi-Fi power cycle, then collect the matching session FIRST. Results
save automatically and return to Ubuntu; no commands/photos needed. Physical
synthesizer/channel readiness and calibrated radio/connectivity remain untested.


### 2026-10-25 — v83 native synthesizer field/readback result

Source `0ee9efb3a582cdddc3293f09664c9b60282fcc0e`, release
`be37d545e93f1aed`, session `session-00065eb3c5cb1785-00`, passed strict
whole-chain collection after the attended cold adapter cycle. All21 fixed
channel1 steps preserved reference order, required fresh exact-profile values,
performed one masked/full write and matched independent readback.441 ordered
operations,378 guard reads,63 exact replies,21 ACKs,42 observations and126
MAC0/COREf000f200/DMA40000050 triplets passed. Final RF bank0 values (hex):
37=02,36=3f,35=28,34=dd,33=e2,32=42,31=42,30=02,29=28,28=30,26=00,27=00,
24=33. All unmasked fields retained their fresh values; no VCO/SDM pulse.

Whole earlier band/MAC/initialization proof and the complete458424-byte
prefix/both completion markers passed within512KiB. Ubuntu default, consumed
one-shot and installed/known-good recovery hashes verified. Offline workspace
clean309files/734clusters/128directories, zero leaks/mismatches/errors;
unmounted, reboot unarmed. Full evidence archived outside build under
`wifi-synth1-0ee9efb`. This accepts bounded synthesizer field writes/readbacks,
not PLL lock, calibrated RF or connectivity. Next: post-tune bandwidth/band
setup and fresh PA/gain prerequisites before VCO/MAC enable; no new run armed.


### 2026-10-25 — v84 post-tune band/gain candidate and local checks

New private post-synthesizer plan replays the19 reference20MHz/G-band entries,
sequences9..1:266 ordered operations/228 guard reads/38 replies/19 ACKs/
19 observations/76 strict guard triplets. All19 matching readbacks admit the
three-target internal-PA MAC tail: fresh518=1,13a8=6c6c6c6c,13b4=c9540000
must independently become1,63707400,89440000(hex).27 operations/24 reads/
3 writes/3 verified/6 strict triplets; fresh-profile or masked-readback mismatch
refuses. Same-boot complete mapped NICfd11/1004 proof required; internal2G PA
bit8 asserted, no external branch or transmit-power inference.

Independent pinned-reference table/order and PA/MAC audit passed. Directed
engines and complete xHCI suite cover266 RF/27 MAC positions,196 prior/state/
PA faults, packet bits, no USB on refusal/replay and failure quarantine.
Whole prior+new collector fixture495282bytes fits512KiB;1380 malformed proofs
refuse and boot-module namespace survives.163 staging tests and166 historical
recipes unchanged. Build/sanitizer/exact-stage VM/install follow; native result
pending. No VCO/calibration, MAC enable, scan or traffic.


v84 full build and both RF/MAC ASan/UBSan runs passed. All directed engines,
complete xHCI suite, independent reference audit, staging/history and strict
whole-chain collector checks passed. Exact-stage VM/install follows; these
local checks do not accept RF functionality or native post-tune readbacks.


### 2026-10-25 — v84 installed, native result pending

Exact source `f67ee1be1263e5d5d48a903739e34e90436b9450`, release
`e57afcf9eb1fab7e`, config `ram-usb-wifi-fw-v84-laptop-probe-0e535900-auto-return`.
Full build, RF/MAC ASan/UBSan, directed engines, complete xHCI suite,163
staging tests/166 unchanged historical recipes, independent reference audit
and1380 strict-collector refusals passed. Exact GPT-stage VM saved and
byte-verified diagnostics, unmounted cleanly and reset. Missing-workspace VM
refused the probe and returned. Explicit512KiB capture confirmed. These VM
checks validate workflow/refusal behavior, not MT7610 RF/PA function.

Installed/current and known-good `e47dca3a6b30c6c1` recovery payloads matched
durable backups. Ubuntu remains default; reboot unarmed. ESP v82
`7e269653eebe03c3` removed only after full archive byte equality; v83
synthesizer payload retained alongside recovery. Offline workspace clean:
309files/734clusters/128directories, zero leaks/mismatches/errors; unmounted.
Run/checkpoint and candidate evidence archived outside build. Await readiness
for cold Wi-Fi power cycle; collect matching session FIRST after return.
Results save and return automatically; no commands/photos needed. Native
post-tune readbacks, calibrated radio and Wi-Fi connectivity remain untested.


### 2026-10-25 — v84 native post-tune band/gain result

Source `f67ee1be1263e5d5d48a903739e34e90436b9450`, release
`e57afcf9eb1fab7e`, session `session-00065eb40f2ab69b-00`, passed strict
whole-chain collection after the attended cold adapter cycle. All19
post-synthesizer20MHz/G-band table values matched independent readbacks:
266 ordered operations,228 guard reads,38 exact MCU replies,19 ACKs and76
MAC0/COREf000f200/DMA40000050 triplets; both mismatch masks zero.
Same-boot mapped internal2G PA admission passed without raw-field logging.

MAC tail passed27 ordered transfers,24 reads/3 writes/3 verified and6 strict
guard triplets. Fresh/readback values(hex):518 1->1;13a8 6c6c6c6c->63707400;
13b4 c9540000->89440000. Masked fields preserved all other fresh bits. Entire
495284-byte startup prefix and both completion markers saved within512KiB;
whole prior proof passed. Ubuntu default, consumed one-shot and current/
known-good recovery hashes verified. Offline workspace clean311files/
740clusters/129directories, zero leaks/mismatches/errors; unmounted/unarmed.
Evidence archived outside build under `wifi-post-f67ee1b`. Bounded post-tune
setup/readbacks accepted, not PLL lock, calibrated radio or connectivity.
Next audit channel-specific BBP/filter/gain and VCO/calibration prerequisites;
no new run armed.


### 2026-10-25 — v85 mapped RX gain and BBP prerequisite implementation

Before channel-BBP gain writes, capture fixed mapped logical40 LNA44/RSSI46/47
and read CORE1 plus20 channel-BBP targets. No filter/BBP/RF/MAC data writes;
one mode-zero EFUSE read-select only, never programming. Decode reference
LNAff->0, signed RSSI[-10,10] clamp/mask and uint8/7-bit AGC8 arithmetic as
observations. Raw block is scrubbed/not logged; unknown output families refuse.
Same exact ASIC/MAC and whole v84 post-tune owner chain required.

Host tests cover old/new EFUSE profiles, every I/O/cancel/timeout/unmapped/
command-bit boundary,65536 decoder inputs and output clearing; inventory27
reads/all-ones/failure/cancel/late-state/no replay. Complete xHCI suite includes
13 mapped phases/27 inventory positions/216 ownership/state/profile faults,
refused USB and quarantine. Full build and both ASan/UBSan suites passed.
Independent pinned source offsets/arithmetic/21-target audit passed;164 staging
tests/168 historical recipes unchanged. Full501863-byte synthetic prefix passes
whole prior proof/namespace preservation,450 malformed refusals, unknown raw
family refusal and four valid alternate LNA observations. Exact-stage VM/install
follows; native gain values unknown. No calibrated-radio/connectivity acceptance.


### 2026-10-25 — v85 installed, native observations pending

Exact source `e3ea53b3c53e7d6dd009775cbd5160963c91b68e`, release
`2571de4ea5d87728`, config `ram-usb-wifi-fw-v85-laptop-probe-0e535900-auto-return`.
Full build, old/new EFUSE/inventory host suites, complete xHCI suite, both
ASan/UBSan suites,164 staging tests/168 historical recipes, independent source
audit and strict collector/alternative-observation/privacy-family checks passed.
Exact GPT-stage VM saved and byte-verified diagnostics, unmounted cleanly and
reset. Missing-workspace VM refused the probe and returned.512KiB selection
confirmed. These VM checks validate workflow/refusal, not native RX gain data.

Installed/current and known-good `e47dca3a6b30c6c1` recovery payloads matched
durable backups. Ubuntu remains default; reboot unarmed. ESP v83
`be37d545e93f1aed` removed only after full archive byte equality; v84 post-tune
payload retained alongside recovery. Offline workspace clean311files/
740clusters/129directories, zero leaks/mismatches/errors; unmounted. Candidate
and run/checkpoint archived outside build. Await readiness for cold Wi-Fi
cycle; collect matching session FIRST on return. Capture saves and returns
automatically, no photos/commands. LNA/RSSI and21 BBP values remain native
observations pending; no BBP/RF/MAC data write or calibrated-radio acceptance.


### 2026-10-25 — v85 native RX gain and baseband observations

Release `2571de4ea5d87728` (source `e3ea53b3`) completed v85 in
`session-00065eb4b8532f3d-00`. Fresh mapped block40 yielded LNA raw/effective0,
RSSI offsets+1/+1 with no clamps, and derived AGC8=16344ef0(hex), matching
the observed AGC8. One read-select,12 reads,1 poll,13 submissions and strict
identity/MAC/CORE/DMA guards passed; raw block bytes were scrubbed and not logged.
CORE1=2 and all20 channel-BBP values were captured with27 reads,zero data writes;
the20 values equal the pinned G20 table with captured LNA0. These are observations,
not calibration or permission to infer other board profiles.

The whole earlier proof, complete501580-byte startup prefix and both completion
markers passed strict collection. Ubuntu default, consumed one-shot and current/
recovery payload hashes verified. Offline workspace clean313files/746clusters/
130directories, zero leaks,mismatches or errors; unmounted and unarmed. Durable
archive outside build preserves the results. No filter/BBP/RF/MAC data writes in
this step; calibrated RF, frequency lock and Wi-Fi connectivity remain untested.
Next: bounded channel1 filter/BBP setup using fresh captured gain with independent
readbacks, then post-tune VCO/calibration prerequisites. Budget the whole next
prefix:501580 of524288 bytes used; explicitly increase diagnostic capacity if
needed while preserving historical limits and missing-prefix refusal.


### 2026-10-25 — channel1 BBP/filter bounded objective

Next scope: explicit channel1/20MHz CORE1 filter clear and20 channel-BBP
writes using the fresh mapped RX gain, each guarded with fresh input and an
independent readback. Depend on full v85 proof and same-boot mapped LNA0/
RSSI+1/+1; unknown board-field profiles refuse. No VCO pulse, MAC enable,
calibration or traffic in this candidate. Touched: MT76 fixed register engine,
xHCI diagnostic ownership, opt-in boot recipe/capture capacity, directed tests
and machine/current Wi-Fi guides. Budget complete prefix with a new explicit
1MiB diagnostic capacity; ordinary and historical capacities remain unchanged.
Fetched main `fa5a6cc1` adds Air shared-runtime acceptance documentation only;
retain intentional integration baseline `d5bc4b3` through this RF trial.
Open PR175 owns pthread/Apple Silicon work; Pro PR19 owns MT76 diagnostics.
No shared MT76 overlap, parallel staging or remote task messaging. Last run
v85 collected/archived; Ubuntu default/recovery retained; reboot unarmed.


### 2026-10-25 — v86 channel1 baseband/filter implementation

The explicit diagnostic now clears CORE1 bit5 for channel1 and writes the20
selected20MHz/G-band BBP values with AGC8=16344ef0, derived from fresh mapped
LNA0. Same-boot RSSI+1/+1, unclamped fields, exact21-target v85 inventory and
whole prior ownership proof are mandatory. Unknown profiles refuse. Each of21
targets uses three strict MAC0/COREf000f200/DMA40000050 preguards, a fresh exact
input, one fixed masked/full write, independent exact readback and three strict
postguards:189 operations,168 reads,21 writes/verified,42 guard triplets. CORE1
preserves all bits exceptbit5; captured input2 remains2. Each BBP fresh input must
match the captured profile; no arbitrary register/value admission or replay.

No post-tune VCO pulse, calibration, MAC enable, scan or traffic. Host tests cover
every transfer, cancellation, bad guard/fresh value/readback and authorization
mutation boundary, payload bits and no replay. Native callbacks cover189
positions and221 ownership/state/profile faults plus transfer-error quarantine.
Independent Linux v7.0 table selection/filter/source-order audit and ASan/UBSan
passed. Whole-prefix fixture526067bytes requires explicit1MiB capture, selected
only by this mode;64/128/256/512KiB historical limits are unchanged. Logger tests
cover1MiB persistence/wrap boundaries and lost-prefix refusal; collector checks
all receipts/order/guards/full-prefix arithmetic and rejects1721 malformed
proofs.165 staging tests,170 historical recipes and29 desktop-collection tests
passed; full build and xHCI regression passed. Exact-stage VM/install follows;
physical acceptance remains pending.


### 2026-10-25 — v86 installed, native channel-BBP result pending

Exact source `3614ddb2b8377e666a016e13d8f803752f44ae19`, release
`8af0efef4d2a53c2`, config `ram-usb-wifi-fw-v86-laptop-probe-0e535900-auto-return`.
Full build,189-position/221-fault native admissions, engine/logger/collection
regressions, ASan/UBSan, independent source audit and strict collector tests
passed. Exact GPT-stage VM saved and byte-verified diagnostics, cleanly unmounted
and reset; missing-workspace VM refused the probe and returned. Explicit1MiB
snapshot confirmed. These VM passes validate workflow, not the physical radio.

Installed/current and known-good `e47dca3a6b30c6c1` recovery payloads match
durable backups. Ubuntu remains default; reboot unarmed. ESP v84
`e57afcf9eb1fab7e` removed only after full archive byte equality; v85 retained
alongside recovery. Offline workspace clean313files/746clusters/130directories,
zero leaks,mismatches or errors; unmounted. Candidate and run/checkpoint archived
outside build. Await readiness for cold Wi-Fi cycle; collect matching session
FIRST on return with the strict v86 collector. Results save and return
automatically, no photos/commands. Native filter/BBP readback acceptance pending;
no calibrated-radio/connectivity acceptance.


### 2026-10-26 — v86 native channel1 BBP/filter result

Release `8af0efef4d2a53c2` (source `3614ddb2`) completed v86 in
`session-00065eb63c87b85a-00`. All21 channel1/20MHz filter/BBP writes passed
fresh-input and independent readback checks:189 ordered operations,168 reads,
21 writes/verified and42 strict MAC0/COREf000f200/DMA40000050 guard triplets.
CORE1 bit5 clear preserved input2; all20 selected BBP words matched the fixed
profile, including AGC8=16344ef0(hex). Same-boot mapped LNA raw/effective0,
RSSI+1/+1, no clamps and exact21-target inventory passed before writing.

Whole earlier proof, complete526069-byte startup prefix and both completion
markers passed strict validation. Explicit1MiB capture preserved the full log.
Ubuntu default, consumed one-shot and installed/recovery payload hashes verified.
Offline workspace clean315files/753clusters/131directories, zero leaks,
mismatches or errors; unmounted and unarmed. Results durably archived outside
build. Bounded channel1 BBP/filter setup/readbacks accepted; calibrated RF,
frequency lock and Wi-Fi connectivity remain untested. No repeat boot needed.
Next: audit post-tune VCO and calibration prerequisites before MAC enable;
preserve disabled MAC/DMA and explicit full-prefix capture.


### 2026-10-26 — post-tune VCO bounded objective

Next: after exact v86 channel1 BBP/filter proof, fresh-read RF0:4, setbit7 once
with preserved low bits, independently read it and observe six fixed delayed
samples. One bounded channel1 VCO initiation, not MCU full calibration or radio
acceptance. Keep MAC0/COREf000f200/DMA40000050 guards around every command;
no MAC enable/traffic or guessed power/regulatory settings. Touched: MT76 fixed
engine, native diagnostic ownership, boot recipe/tests and this unit/Wi-Fi guide.
Dependencies: collected v86 full proof, fresh mapped gain, pinned Linux v7.0
set_channel ordering, explicit1MiB full-prefix capture. Latest fetched main
`8bd43adc` merges pthread lifecycle and bounded AHCI timeout changes; no MT76
overlap. Retain intentional `d5bc4b3` integration base during this diagnostic.
Open PR179 owns epoll/eventfd; Pro PR19 owns MT76. No remote messages or parallel
staging. Current run collected/archived; Ubuntu recovery retained, reboot unarmed.


### 2026-10-26 — v87 post-channel1 VCO implementation

After the whole exact v86 BBP/filter and same-boot mapped gain proof, an explicit
mode fresh-reads RF0:4 and requires30(hex), setsbit7 once with lower bits
preserved, requires exact B0 write echo, then independently reads RF0:4 and
captures six more reads after10/40/50/100/300/500ms requested waits. Every
command has strict MAC0/COREf000f200/DMA40000050 guard triplets before/after.
63 ordered submissions,54 guard reads,9 exact replies,1 write ACK,8 observations,
18 triplets and6 waits totaling1000000us. Sequences2..10 continue from the
actual prior native sequence1. No DAC pulse, full MCU calibration, MAC enable,
scan, traffic or transmit-power inference. Immediate and delayed bytes are
observations; first_clear counts delayed samples only,0 if none. Self-clear
does not accept PLL lock or calibrated radio.

The fixed engine validates stored state at every transfer/delay and after
final authorization. Native ownership includes exact channel-BBP completion,
submitted counts and independent wait receipts. Wrong duration, profile,
packet or order refuses before USB; failed/uncertain transfers quarantine
without replay. Directed tests cover63 positions,229 prerequisite/state faults,
all packet bits, all reply-length/metadata/guard/cancellation/auth-mutation
boundaries, all64 delayed clear patterns and six timer-failure/no-replay paths.
Independent pinned Linux v7.0 source/order audit, ASan/UBSan, full build/xHCI
regression,166 staging tests and172 historical recipes passed. Complete
synthetic prefix534737bytes fits inherited explicit1MiB;616 malformed proofs
refuse and all64 consistent delayed-clear patterns pass as observations.
Exact-stage VM/install follows; physical results and calibration remain pending.


### 2026-10-26 — v87 installed, post-tune VCO observations pending

Exact source `816a4b30586ec944439f6b29a24e41c3c408da47`, release
`78e47b6219211cc9`, config `ram-usb-wifi-fw-v87-laptop-probe-0e535900-auto-return`.
Full build, fixed engine and full xHCI regression (63 positions/229 ownership
faults plus all six delay boundaries), ASan/UBSan, independent reference/sequence
audit,166 staging tests/172 historical recipes and616 strict-proof refusals
passed. Exact GPT-stage VM saved and byte-verified diagnostics, cleanly unmounted
and reset. Missing-workspace VM refused the probe and returned. These checks
validate workflow and admission, not physical VCO behavior or calibration.

Installed/current and known-good `e47dca3a6b30c6c1` recovery payloads match durable
backups; Ubuntu remains default. ESP v85 `2571de4ea5d87728` removed only after
full archive byte equality; accepted v86 retained beside recovery. Offline
workspace clean315files/753clusters/131directories,zero leaks,mismatches or
errors; unmounted and reboot unarmed. Candidate and run/checkpoint archived
outside build. Await readiness for cold Wi-Fi cycle. On return collect the new
matching session FIRST with the v87 strict collector; preserve failures and
actual immediate/delayed observations without guessing values. Results save
and return automatically; no photos or commands. No full calibration, MAC enable,
traffic or calibrated-radio/connectivity acceptance.


### 2026-10-26 — v87 native post-channel1 VCO result

Release `78e47b6219211cc9` (source `816a4b30`) completed v87 in
`session-00065ec046be9096-00`. RF0:4 fresh-read30(hex), setbit7 write/echoB0
and independent immediate readB0 passed. All six delayed samples read30:
bit7 was clear by the first delayed sample and remained clear. Requested
inter-sample waits were10/40/50/100/300/500ms,total1000000us; transport/logging
time is additional, so this does not measure a precise10ms completion deadline.
63 ordered submissions,54 guard reads,9 exact replies,1 ACK,8 observations,
18 strict MAC0/COREf000f200/DMA40000050 triplets and all six waits passed.
Sequences2..10 followed actual prior sequence1. Whole earlier initialization,
fresh mapped gain and all21 channel-BBP readbacks passed in this same boot.

The complete534735-byte startup prefix and both completion markers were saved
and strictly validated within explicit1MiB capacity. Ubuntu default, consumed
one-shot and installed/recovery hashes verified. Offline workspace clean:
317files/760clusters/132directories,zero leaks,mismatches or errors; unmounted
and unarmed. Evidence archived outside build. This accepts the bounded trigger,
independent read and timed observations; PLL lock, full radio calibration and
Wi-Fi connectivity remain untested. No repeat boot required. Next audit AGC gain
initialization and full calibration/MAC/power prerequisites, including the
previous IBI9 all-ones observation; do not blindly issue standalone calibration
or infer transmit power. No MAC enable or traffic in this trial.


### 2026-10-26 — integration refresh to main dfd63fef

Merge actual `origin/main` `dfd63fef` into the published device branch, preserving
its hardware-evidence history per coordination policy. Local backup ref retains
pre-merge `eababae2`; accepted native v87 payload/evidence remain outside build.
Resolved shared Makefile, main automatic-capture dispatch, boot profile/CLI/smoke
and desktop-guide conflicts by retaining both Wi-Fi and new POSIX/desktop paths.
No MT76 engine or descriptor/profile gate widened. Early combined checks pass:
172 staging tests and31 desktop collection tests. Next run full build/core and
exact-stage save/refusal VM checks, then a physical whole-chain v87 regression
on the merged baseline before extending calibration. Same model/USB/recovery,
no new hardware writes in the integration regression. AGC/full-calibration audit
can proceed locally; full calibration remains gated on native context evidence.
Open PR19 owns Wi-Fi; current main includes merged PR175/178/179 fixes. No other
open shared-code PRs found at refresh. Battery/headphones/trackpad deferrals stay.


### 2026-10-26 — merged-baseline local verification and AGC audit

Main `dfd63fef` is now an ancestor of this published branch through merge
`cb346475`; no history rewrite. All six MT76/xHCI source/header files match
accepted pre-merge v87 byte-for-byte. All174 historical Wi-Fi recipes and97
main laptop/desktop/POSIX recipes retain identical boot arguments;172 staging
and31 collection tests pass. Full image build and full `test-core` pass.

Verification exposed incoming host-test integration gaps. Added explicit poll
clock header/POSIX feature declarations, a process-service wake fixture with
wake-before-frame-free assertion, and freestanding compiler integer types for
SrvrOS-header event/time/synchronization fixtures; the host pthread shim keeps
host headers. No warnings disabled or kernel futex behavior changed. Focused
contracts and the final whole core run pass. Preserve original failed logs as
diagnostics. Baseline smoke and exact-stage VM/install follow.

Pinned Linux v7.0 AGC audit: initialization reads AGC8(2320) andAGC9(2324),
extracts bits14:8, copies initial values into current gain state, sets low_gain=-1
and marks initialization done. It performs no radio write. Known AGC8 implies
78; AGC9 remains uncaptured, so do not synthesize second-chain gain. USB
power-on full calibration follows MAC start in the reference; prior IBI9
all-ones observations remain unresolved. First physically regress the accepted
v87 sequence on the merged baseline; next implement bounded fresh two-register
capture/software gain state with strict failure handling. No new radio writes,
MAC enable, full calibration or inferred power/regulatory acceptance in this
integration regression. Desktop-runtime parity needs separate native evidence.


### 2026-10-26 — merged-main v87 regression installed

Integration baseline `dfd63fef8d569f49dc75bbaf744e4dadd25bba86` is merged,
preserving published history. Host integration fixes are `b546c66f`; exact
staged source `ae8fe63c6152b69f39869895cc5a6fe3375d8a88`, release
`5c9c8372adaf59d6`, existing v87 recipe unchanged. Full build/core, all482 CLI
commands, syscall ABI, shell editing, UEFI USB input, display startup,172 staging
and31 collection tests pass.174 Wi-Fi and97 main profile recipes are unchanged.
Focused contract receipts and source hashes retained, including wake-before-free
and event/time/synchronization coverage.668 local documentation links pass.

The exact GPT-stage VM saved and byte-verified diagnostics, cleanly unmounted
and reset; missing-workspace VM refused and returned. Whole v87 collector proof
passed for the staged release, with missing-marker/mode refusal and preserved
namespace. No new radio operations: this repeats the accepted v87 sequence on
the updated scheduler/process/filesystem/runtime baseline. AGC/full calibration
remains next after native regression, as audited above. These local/VM passes
do not accept native Wi-Fi or desktop-runtime behavior on the new baseline.

Installed/current and known-good `e47dca3a6b30c6c1` recovery hashes match durable
backups. Accepted pre-merge v87 `78e47b6219211cc9` remains on the ESP as well.
ESP v86 `8af0efef4d2a53c2` removed only after full archive byte equality.
Ubuntu default retained; offline workspace clean317files/760clusters/
132directories,zero leaks,mismatches or errors; unmounted and reboot unarmed.
Candidate/run/checkpoint and test evidence archived outside build. Await cold
Wi-Fi-cycle readiness. Collect matching merged-baseline session FIRST on return
with main-refresh/collect_return.py. Results save and return automatically;
no photos or commands. Native regression and radio/connectivity remain pending.


### 2026-10-26 — native merged-main Wi-Fi regression result

Release `5c9c8372adaf59d6` (source `ae8fe63c`, merged main `dfd63fef`)
completed the whole v87 regression in `session-00065ec13fa73dce-00`.
Full earlier initialization/firmware/guard proofs passed on the updated
scheduler/process/filesystem/runtime baseline. Fresh mapped LNA0/RSSI+1,+1,
all21 channel-BBP/filter independent readbacks and post-channel VCO proof
passed:63 ordered operations,54 guard reads,9 replies,1 ACK,8 observations,
18 strict MAC0/COREf000f200/DMA40000050 triplets,six waits/1000000us requested.
RF0:4 values were30,B0,B0,then six30(hex),matching the pre-merge result;
bit7 clear by the first delayed sample. Timing includes transport/logging,
so this is not a precise10ms completion bound or PLL-lock/calibration proof.

The full534735-byte startup prefix and both completion markers passed strict
validation within1MiB capacity. Ubuntu default, consumed one-shot and installed/
recovery hashes verified; offline workspace clean319files/767clusters/
133directories,zero leaks,mismatches or errors; unmounted and unarmed. Evidence
archived outside build. Bounded Wi-Fi initialization regression on this merged
baseline accepted; full calibrated radio/connectivity and desktop-runtime parity
remain untested. No repeat boot required. Next implement bounded fresh AGC8/9
capture and software gain initialization; retain MAC/power/IBI9 prerequisites
before full calibration. Never infer the uncaptured second gain or transmit power.


### 2026-10-26 — fresh AGC state bounded objective

Fetched main `9f18d7d6`: merged PR180/183 adopts the poll/header/GCC fixture
fixes and strengthens the process-service futex fixture. Merge the canonical
main tests while retaining our native records; production code has no further
difference from the physically accepted merged baseline. PR181 owns app lifecycle
and PR182 filesystem auditing; no MT76 overlap. Pro PR19 retains this scope.

Next fixed mode: after exact captured whole v87/VCO-clear proof, read AGC8(2320)
and AGC9(2324) between strict MAC0/COREf000f200/DMA40000050 triplets. Eight
ordered reads,zero new data writes. Require AGC8=16344ef0 from this captured
LNA0/channel1 profile; refuse either all-ones target. Decode bits14:8 into two
initial/current software slots only after both fields,postguards and final
authorization validate; low_gain=-1 and init_done=true then. The second slot is
reference software state,not evidence of a second physical RF chain. Unknown
AGC9 is freshly observed; no guessed default. Failures cannot expose partial
initialized state. No calibration,MAC enable or traffic; prior IBI9/MAC/power
prerequisites retained. Touched: MT76 engine/header,native xHCI diagnostic mode,
boot recipe/directed tests and current unit/Wi-Fi guides. Inherit explicit1MiB
full-prefix capture and Ubuntu/known-good recovery. Last run collected; no reboot
armed. Unattended trial prints progress and automatically saves/returns.


### 2026-10-26 — v88 fresh AGC capture and software initialization

After exact captured whole v87 proof (including immediateVCO B0 and six delayed
30 values), explicit `wifi-agc-state` reads MAC/CORE/DMA,AGC8/AGC9,MAC/CORE/DMA:
eight fixed reads,zero new register writes,two strict0/f000f200/40000050 guard
triplets. AGC8 must remain16344ef0 for this captured LNA0/channel1 profile.
AGC9 is freshly observed; either all-ones target refuses. Only after both reads,
postguards and final authorization validate are bits14:8 copied into both
initial/current software slots,low_gain=-1 and init_done=true. Before that,
initialized state stays absent; failure logs explicitly mark unread targets.
Second software slot does not accept a second physical RF chain. No calibration,
MAC enable,traffic or power inference. Guest prints a fixed progress line.

Engine tests cover all transfer/cancel/authorization mutations, guard/target
boundaries,65536 alternate second-word decodes,final-state corruption and no
replay/partial publication. Native callbacks cover8 positions,243 prerequisite/
state/profile faults,zero USB on refusal and uncertain-transfer quarantine.
Independent Linux v7.0 source/order/mask audit and engine ASan/UBSan pass.
Full image build/core suite,173 staging tests,31 desktop collection tests and
174 historical Wi-Fi recipes pass. Complete536025-byte fixture fits inherited
1MiB capture;93 malformed proofs refuse,while five alternative valid AGC9 words
pass with their actual decoded values. Old post-VCO capture behavior remains
unchanged; the new prerequisite predicate is exact and separate. Canonical
upstream host fixes/main `9f18d7d6` merged; no additional production changes from
that merge. Exact-stage VM/install follows; native gain1/init-state acceptance
remains pending. Existing IBI9/MAC/power calibration prerequisites remain open.


### 2026-10-26 — v88 AGC software-state trial installed

Exact source `9e7d2ef07d0519e537bf52a127fd91915e0f5783`, release
`413af14583fe167c`, config `ram-usb-wifi-fw-v88-laptop-probe-0e535900-auto-return`.
Main `9f18d7d6` is an ancestor,with canonical host-test fixes merged. Full image
build/core and all old/new MT76/xHCI tests pass; engine sanitizer,independent
reference audit,173 staging tests,31 collection tests,174 historical recipes
and93 strict-proof refusals pass. Native fixture completion-flag mutation was
corrected before the final complete suite; original failed test log preserved.

Exact GPT-stage VM saved and byte-verified diagnostics,unmounted cleanly and
reset; missing-workspace VM refused and returned. Guest mode has explicit
progress and saved result/return. These VM checks validate workflow,not native
AGC9 data or software-state acceptance. Complete1MiB-prefix policy retained.

Installed/current and known-good `e47dca3a6b30c6c1` recovery payloads match durable
backups. Accepted merged-baseline `5c9c8372adaf59d6` retained on ESP beside
recovery; old `78e47b6219211cc9` removed only after full archive byte equality.
Ubuntu default retained; offline workspace clean319files/767clusters/
133directories,zero leaks,mismatches or errors; unmounted and reboot unarmed.
Candidate/run/checkpoint/evidence archived outside build. Await cold Wi-Fi-cycle
readiness; on return collect matching session FIRST with agc-state/collect_return.py.
Results save and return automatically,no photos or commands. Native AGC9 and
initialized state pending; no new radio writes,calibration,MAC enable,traffic,
second physical-chain or power acceptance.


### 2026-10-26 — v88 native AGC software state accepted

Source `9e7d2ef07d0519e537bf52a127fd91915e0f5783`, release
`413af14583fe167c`, session `session-00065ec1bf446752-00` passed strict whole
prior initialization and fresh gain-state proof. AGC8=16344ef0 and AGC9=00000000
were actual successful reads; decoded initial/current gains78/0,low_gain=-1,
init_done=true. All8 reads and two0/f000f200/40000050 guard triplets passed;
zero new register writes. A zero second software slot does not accept a second
physical chain. Earlier21 channel-BBP readbacks and post-VCO profile remained
exact:30,b0,b0,then six30;first_clear1. Full536023-byte startup prefix and both
completion markers saved within1MiB capacity. Ubuntu default,consumed one-shot,
installed/recovery hashes verified. Offline workspace clean321files/774clusters/
134directories,zero leaks,bitmap mismatches or errors; unmounted and unarmed.
Evidence retained outside build. This accepts bounded register capture and
software state only; calibrated RF,PLL lock,packet RX/TX and connectivity remain
untested. Next audit MAC/power/IBI9 prerequisites before full calibration.


### 2026-10-26 — passive receive decoder groundwork validated

Main `0858e8e6` (PR181/185) merged as `bda9d178`; both Makefile test families
preserved. Source `c716dd63` adds allocation-free MT76x02 USB/RXWI management
views and copied passive infrastructure beacon/probe-response observations.
Binary/hidden SSIDs,optional L2 padding,strict length bounds,atomic output
publication,mandatory/duplicate IE checks and caller-supplied US channel1..11
context are covered. Security booleans report advertisements only; no suite,
credential or association acceptance. Raw RSSI is not calibrated signal level.

Host tests and ASan/UBSan pass,including every transfer truncation,unaligned
buffers,corrupt/unsupported frame forms and100000 hostile transfers. Pinned
Linux DMA/RXWI/padding/trim audit passes; calibration temporary-register/
delay/restoration ordering is separately recorded. Full `all test-core` passes.
Matched normal-image VM regression passes syscall ABI,all482 CLI commands,
shell editing,UEFI USB input and displayd. Documentation checks pass. These
validate local code/workflow,not native receive or desktop parity on this merge.

Native startup,MCU/radio engines,callbacks and boot recipes are byte-unchanged
from installed v88. No native receive caller,radio enable,calibration or
transmission was added. Known-good Ubuntu/recovery and installed v88 remain;
no new physical trial or reboot is armed. Required evidence is archived outside
build. Next: controlled MAC/RX endpoint lifecycle,explicit stop/restoration and
uncertain-DMA retention,then calibrated-radio admission using fresh board/
power/TSSI context. Prior IBI9 and RF0:6 observations remain unresolved for
full radio acceptance. Scanning,association/security,IP and UI remain pending.


### 2026-10-26 — retained packet-RX lifecycle implemented

The v89 explicit endpoint trial is implemented after exact full v88 admission.
It configures captured84/DCI9,posts once,waits at most10ms or completion,then
stops with exact command/slot/stopped-context/owned-dequeue proof. Six fixed
read-only guards bracket the attempt; all DMA is retained. No packet bytes,
MAC enable,RF writes,full calibration,Set Dequeue,rearm,retry or free.

Local full image/core tests pass,including native helper configuration/prior-
proof faults,guard transport/state boundaries,normal/short/stopped/invalid
completions,stale/wrong-slot/wrong-endpoint routing,failed-stop retention and
cleanup after admission loss. Staging174 tests and176 historical recipe byte
comparisons pass. Strict whole-prefix proof passes and79 malformed receipts
refuse; six completion variants are distinguished without interpreting stopped
lengths as data. Initial missing v89 regex entry and generated C string newline
errors were corrected; original failed logs preserved. Sanitizer/exact-stage
VM and install follow; native result pending. Main `5bcd061c` is merged; no
competing worker/filesystem scope was edited. Last physical v88 remains
accepted and no reboot is armed.


### 2026-10-26 — v89 retained RX trial installed

Final source `50e2b8929a29896f1dcae19acd36d7835d8c1de7`, release
`dbb5191dddbe1790`, recipe `ram-usb-wifi-fw-v89-laptop-probe-0e535900-auto-return`.
Full image/core suite passed before the final stricter CC26/28 residue guard;
matched image rebuild and targeted native-helper/ASan/UBSan tests passed after
that change. CC27 remains explicitly length-invalid. Staging174 tests,
176 historical recipe comparisons,whole prior+RX synthetic proof and79
malformed-proof refusals passed. Initial regex/C literal errors and a sanitizer
wrapper's renamed-main return warning were corrected without warning suppression;
original failed logs retained. Final exact GPT-stage VM saved/byte-verified,
unmounted and returned automatically; missing workspace refused and returned.
Prototype VM evidence remains separate. VM proves workflow,not native RX.

Installed/current payload and known-good `e47dca3a6b30c6c1` recovery match durable
backups. Accepted v88 `413af14583fe167c` is retained on ESP beside recovery;
older `5c9c8372adaf59d6` removed only after full equality with its durable
archive. Ubuntu default retained. Offline workspace clean321files/774clusters/
134directories,zero leaks,bitmap mismatches or errors; unmounted and unarmed.
Run/checkpoint/helpers and evidence archived outside build. Await cold Wi-Fi
cycle readiness before shutdown; leave workspace connected. Guest shows progress,
saves results and returns automatically; no photos or commands. On return,
collect matching release/session FIRST with rx-lifecycle/collect_return.py.
Require full prior and ordered guard/config/post/wait/stop/context/dequeue/
retention proof. Native lifecycle/receive/calibration remain pending.


### 2026-10-26 — v89 failed before receive endpoint execution

Release `dbb5191dddbe1790`, source `50e2b892`, session
`session-00065ec2cfa926d2-00` saved a complete526801-byte prefix with both
completed=NO markers. Whole prior initialization through all21 channel-BBP
readbacks passed. The native selector left post_vco_mode false for mode67;
no post-VCO operations ran. AGC refused status1/attempted0/reads0,with unread
zero placeholders; these are not observed register values. No RX configuration,
post or stop was attempted. Strict success collector refused unchanged.

A separate failed-run audit validated the complete prior v86 proof and exact
refusal/unread fields,then checked Ubuntu default,consumed one-shot,installed/
recovery/retained payloads and offline workspace. Clean323files/781clusters/
135directories,zero leaks,bitmap mismatches or errors; unmounted/unarmed.
Original logs/refusal preserved outside build. Failure-auditor variable shadowing
was corrected with an isolated proof namespace; initial auditor error retained.

The helper tests had manually constructed the prerequisite state and missed
native mode wiring. New regression calls the actual mode selector for modes64
through67; it reproduces the mode67 failure before the fix. Selector extraction
preserves existing mode policies; mode67 must enable post-VCO before AGC/RX.
Full build/core and targeted checks follow before any new candidate. No radio
initialization,receive or calibration acceptance follows from this failed run.


### 2026-10-26 — profile-selection correction validated locally

The mode64..67 regression reproduced the original missing post-VCO prerequisite
before the fix. Native mode assignment is now a separately testable helper;
mode67 includes post-VCO,AGC and RX in order. RX ownership fixtures also pass
through that actual selector. Full matched image/core suite passed after the
production fix,and the focused actual-selector/endpoint tests passed under
ASan/UBSan. Existing v1-v88 recipes remain byte-identical; staging174 tests,
whole-prior strict proof and79 malformed-proof refusals pass. No success gate
or hardware register predicate was weakened.

Accepted v88 `413af14583fe167c` restored as current with Ubuntu default and
e47 recovery retained. Failed v89 removed from ESP only after complete durable
payload equality,freeing space for a separately staged retry; its original
session and refusal remain preserved. No reboot is armed. Retry exact-stage
VM/install and native acceptance are pending.


### 2026-10-26 — corrected v89 RX trial installed separately

Source `35041be09bb274f814d9a551aca529dfdbf7c82f`, release
`0df0d7e9c8658836`, same explicit v89 recipe. Mode67 now actually selects
post-VCO before AGC/RX. The new selector regression failed on the old wiring
and passes after the fix; ownership fixtures use the actual selector as well.
Full image/core and focused ASan/UBSan pass. Staging174,176 old recipe-byte
comparisons,whole prior+RX proof and79 malformed-proof refusals pass. Exact
GPT-stage VM saved/byte-verified,unmounted and returned; missing workspace
refused and returned. Native retry acceptance remains pending.

Installed/current payload and e47 recovery match durable backups; accepted
v88 remains retained on ESP. Ubuntu default preserved. Offline workspace
clean323files/781clusters/135directories,zero leaks,bitmap mismatches or errors;
unmounted and unarmed. Failed `dbb5191dddbe1790` session remains separately
archived with381 verified files and its original refusal. Retry uses separate
run/evidence paths. Await cold Wi-Fi-cycle readiness; keep workspace connected.
Guest shows progress,saves and returns automatically,no photos or commands.
On return collect matching release/session FIRST with
rx-lifecycle-retry/collect_return.py; never rerun the failed-run preparation
script or replace its result with a passing retry.


### 2026-10-26 — corrected v89 receive endpoint lifecycle passed

Source `35041be09bb274f814d9a551aca529dfdbf7c82f`, release
`0df0d7e9c8658836`, native session `session-00065ec391da3a5c-00`.
Whole earlier proof passed,including post-VCO63 operations/9 replies/6 delays
with profile30/B0/B0/six30,AGC8/9=16344ef0/0 and software gains78/0.
Captured endpoint84/DCI9/512 configured; one4096-byte IN posted and waited
10001us. Exact Stop Endpoint succeeded,state3,dequeue31d001 for owned
TRB31d000 (DCS1). One stopped transfer event CC26 reported4096 remaining;
this is cancellation metadata,no received packet evidence. All six ordered
MAC0/COREf000f200/DMA40000050 guards passed. DMA retained,no rearm,retry,
free,Set Dequeue,MAC enable or added radio writes.

Strict whole-prefix collector passed on537104 bytes within1MiB,with both
completion markers. Ubuntu default,consumed one-shot,current/e47 recovery
hashes verified. Offline325files/788clusters/136directories,zero leaks,bitmap
mismatches or errors; unmounted/unarmed. Durable evidence is under
`~/.local/share/srvros/macbookpro11-2/wifi-rx-lifecycle-retry-35041be/`.
Original failed dbb5191dddbe1790 evidence remains separate and unchanged.
This accepts bounded receive-request ownership and retirement only. Next is
controlled MAC/RX and fresh board/power/calibration prerequisite work before
passive packet reception; scanning,association,security and IP/UI remain pending.


### 2026-10-26 — transactional calibration preparation (offline)

Main `ee5468f6` (PR186–189 desktop timing/evidence) merged at `ec355bbb`.
PR190 continues desktop timing attribution and PR182 filesystem imports;
no overlapping MT76 scope was found. New `mt76_calibration_run` provides the
fixed channel1/20MHz non-power-on FULL/LC/RXDCOC transaction through semantic
callbacks. It has no native caller and does not frame firmware commands.

Pinned Linux v7.0 `mt76x0_phy_calibrate` ordering requires TX_ALC0 zero,
500us minimum wait,IBI9 temporaryffffff7e,FULL600/LC0,15000us minimum wait,
IBI9/TX_ALC0 restoration,then RXDCOC1. SrvrOS captures both saved words before
the first temporary write,checks independent temporary readbacks,and restores
in reverse order on every subsequent failure or forward cancellation. A failed
write is treated as potentially applied. Cleanup tries both saved-word restores
even if one fails,requires independent exact readback,and prevents RXDCOC or
success after failed restoration. A caller must retain/quarantine uncertain
resources; the helper never frees DMA,retries or repeats a completed attempt.

Every forward callback requires live exclusive ownership authorization. The
fixed guard profile is MAC TX/RX enabled12,COREf000f200,WPDMA40000050 before
snapshot,after restoration and after RXDCOC. This profile is a future native
admission requirement,not evidence that MAC startup has passed. Unknown/all-ones
saved words are refused,including this unit's earlier IBI9 observation; no
observation exception is carried into calibration. Power-on R/VCO/TSSI and
board/transmit-power setup are separate prerequisites,not implemented here.

Host coverage checks exact command/wait/restore order,all22 callback failures,
all21 authorization cancellations,each invalid guard/readback,all-ones reads,
restoration mismatch,missing callbacks,channel/width refusal and reentrant/
repeated entry. This is synthetic transaction/restoration evidence only.
Installed corrected v89 and known-good recovery remain unchanged; no reboot
is armed. Native controlled MAC/RX and fresh board/power/IBI9 evidence must
precede firmware calibration,then passive packet reception and network work.

The next native startup/teardown design must also own device-side queue drains
and bounded MAC_STATUS TX/RX waits from pinned `mt76x0_mac_stop`; host Stop
Endpoint/context proof alone does not establish that the radio MAC is idle.
TXOP ED-CCA changes need their own saved-state/restore policy. These transitions
remain unimplemented/unaccepted in this offline block.

Source `fbd2573f` passes the full matched image/core suite and focused
ASan/UBSan checks. Normal-image VM regression passes syscall ABI,all482 CLI
commands,shell editing,UEFI USB input and displayd. The merged desktop timing
regressions also pass. Native startup/MCU/RX/staging/smoke source bytes remain
identical to accepted corrected v89 source35041be; the new unreferenced helper
is removed from the linked kernel by section garbage collection. No native
callsite or firmware command wiring exists. Installed v89,e47 recovery and
retained v88 payload hashes,Ubuntu default,consumed one-shot and unmounted
workspace verified. Source,reference audits,logs and artifact hashes are archived
outside build; no physical calibration acceptance or new reboot is claimed.


### 2026-10-26 — bounded MAC startup/shutdown candidate (v90)

Main `4ee18f69` / PR190 scheduler and desktop wake attribution is consumed;
PR182 filesystem work remains separate. New explicit `--wifi-mac-cycle`
requires the entire v89 chain. Actual selector68 enables post-VCO,AGC and RX
before MAC work; historical v1-v89 recipe bytes remain unchanged. One retained
main-RX request stays owned throughout MAC startup,snapshot and teardown.
No stopped v89 ring is reused,rearmed or freed.

The captured MT7610 profile starts MAC1004 at4,polls WPDMA busy bits1/3,
writes/verifies the captured17f97 receive filter,then sets/verifies MAC12 and
polls again. Fresh CORE/TX_ALC0/IBI9 observations follow,then a10ms hold.
IBI9 may be all-ones only as raw observation; this does not relax the separate
calibration helper's refusal. No calibration command,power selection,packet
inspection,network data OUT,rearm,scan or association is added. MAC enable is
not a claim that no autonomous radio activity can occur.

Teardown independently clears TXOP ED-CCA,drains device TX queues,polls TX
idle,disables/verifies MAC0,requires seven consecutive empty RX queue rounds,
polls RX idle,and restores/verifies the saved filter/TXOP words. All disabled
MAC/CORE/WPDMA guards must match afterward. Forward failure/cancellation still
attempts cleanup. Quarantined EP0,uncertain command ownership or failed cleanup
cannot pass; all DMA remains retained. Exact host Stop Endpoint/context/dequeue
proof is separately required after the MAC cycle,including on MAC failure.
Poll/read budgets bound further submissions; USB callback latency is not an
exact microsecond wall-time guarantee. Complete logs,not timing assumptions,
determine the native result.

Host fixtures cover every callback failure/cancellation,busy queues,clock/wait
failure,all-ones boundaries and no replay. Native adapter fixtures execute the
actual selector,USB request mapping and all53 forward-cancellation boundaries,
with separate admission/quarantine refusals. Strict whole-prior/MAC/RX proof
replays all ordered operations,queue rounds,restorations and snapshot fields;
408 malformed proofs refuse. Final full build/core and actual-adapter/pure-helper
ASan/UBSan pass,including additional admission/readback/cleanup-loss boundaries.
All eight native receipt formats have matching argument counts; the final source
corrects the new RX register-write counter and v90 progress wording. Exact-stage
VM/install and native acceptance are pending until recorded below. Guest shows
progress,saves the full1MiB startup prefix and returns automatically. No new
physical acceptance or reboot is claimed by this implementation.


The first uninstalled v90 stage `5b9e1bb49eed288b` saved and returned in Q35,
but its harness omitted v90 from the firmware-profile list and correctly failed
an incompatible generic-probe assertion. That receipt is preserved as a failed
VM rehearsal,not physical acceptance. The actual harness assignment now has
v89/v90-positive and unknown-v91-negative coverage. A separate timing regression
reproduced one extra poll read after a wait crossed the deadline; the engine now
checks the clock before every subsequent submission. The regression refuses
that extra read and still proves MAC-disable cleanup. Full final build/core and
175 staging tests pass; exact final-stage VM/install follow separately.


### 2026-10-26 — final v90 MAC-cycle trial installed, unarmed

Source `6af7ec600d1f360de88b52bb28aaff2391da35f6`, release
`b6c18039f1c36e8d`, recipe `ram-usb-wifi-fw-v90-laptop-probe-0e535900-auto-return`.
Final full image/core,actual native adapter and pure-helper ASan/UBSan pass.
175 staging tests include actual harness v90 classification;178 historical
recipe byte comparisons and408 malformed whole-proof refusals pass. Poll
regression reproduced the old post-deadline read before the fix and passes
with pre-submission clock checking. Exact final GPT-stage VM saved,byte-verified,
unmounted and returned automatically; missing workspace refused without probing
and returned. Q35 has no native adapter,so these are workflow/refusal checks.

Installed/current and known-good e47 recovery payloads match durable backups;
accepted corrected v89 remains retained on ESP. Accepted v88 payload was removed
from ESP only after full durable equality,freeing room for this candidate;
its accepted evidence/backup remain outside build. Ubuntu default and EFI order
are unchanged,no one-shot is armed. Offline325files/788clusters/136directories,
zero leaks,bitmap mismatches or errors;unmounted. Native result remains pending.
Guest shows progress,saves full logs and returns automatically;no photos or
commands. After cold-cycle readiness,power off,unplug only Wi-Fi10s,reconnect
and power on,leaving workspace connected. On return collect the matching v90
session FIRST with `build/bringup/mac-cycle/collect_return.py`;preserve failed
completion/cleanup markers. No calibration,reception or connectivity acceptance
is inferred from this install. Evidence/payload/run records are durably archived
under `~/.local/share/srvros/macbookpro11-2/wifi-mac-cycle-6af7ec6/`.


### 2026-10-26 — native bounded MAC cycle passed

Source `6af7ec600d1f360de88b52bb28aaff2391da35f6`, release
`b6c18039f1c36e8d`, session `session-00065ec5caf54f87-00`.
The strict collector accepted the whole earlier initialization/post-VCO/AGC
proof and all54 MAC-cycle operations:46 reads,7 writes,1 wait. MAC1004
progressed0/4/12/0 with independent readbacks. CORE/WPDMA guards matched.
Captured filter17f97 and TXOP583f were restored exactly. Raw TX_ALC0=2f2f1b1b
and IBI9=ffffffff were read with MAC enabled;IBI9 is still observational and
does not satisfy the separate calibration helper's non-all-ones admission.
Requested hold10000us,delay receipt10001us,total bracket10035us;these are
bounded observations,not latency or calibrated-radio acceptance.

TX drained in one round. RX drained in seven consecutive masked-empty rounds:
RXQ_STA=22 with occupancy mask00ff0000 clear,and a30/a34 zero. Both MAC_STATUS
idle checks passed,MAC0 and final disabled guards verified. One4096-byte host
IN remained owned until exact Stop Endpoint:state3,dequeue31d001 for TRB31d000,
DCS1,DMA retained. One stopped event CC26 reported4096 remaining;no packet
bytes or receive acceptance follow. Post-cycle RX wait12841us passed. No
calibration command,packet inspection,rearm or network data OUT was submitted.

Complete543084-byte startup prefix within1MiB and both completion markers saved.
Ubuntu default,consumed one-shot,current/e47 recovery payload equality verified.
Offline327files/795clusters/137directories,zero leaks,bitmap mismatches or errors;
unmounted/unarmed. Evidence is archived outside build under
`~/.local/share/srvros/macbookpro11-2/wifi-mac-cycle-6af7ec6/`.
This accepts only bounded MAC startup/device drain/restoration and endpoint
retirement. Next audit IBI9/register semantics and fresh board/power/TSSI
calibration prerequisites before calibration commands and passive reception.
No immediate repeat boot is needed;scanning,association/security,IP and UI
remain pending. Earlier failed or superseded candidates remain separate.


### 2026-10-26 — reversible calibration-register probe (v91 candidate)

Actual main `d24dbc11` (PR191 Air6,1 evidence only) is consumed. Linux v7.0
register definitions resolve IBI9 to2124;Linux and vendor MT7610 calibration
both save raw IBI9,writeffffff7e,and restore the saved word. Neither routine
special-cases an all-ones saved value. The vendor comment identifies the word
as controlling MAC ownership of2704/2708. This source audit does not establish
register semantics or calibration success on this adapter.

Separate `mt76_calibration_probe_run` preserves the original strict calibration
implementation byte-for-byte. The new explicit v91/profile69 uses the whole
v90 MAC/RX owner and an optional inspection hook after its raw snapshot,before
its hold/teardown. Captured parent TX_ALC0=2f2f1b1b/IBI9=ffffffff is required.
Fresh MAC12/COREf000f200/WPDMA40000050 guards precede TX_ALC0 capture,zero and
independent readback. After500us,fresh raw IBI9 is saved at the reference point,
thenffffff7e is written and read independently. Reverse IBI9/TX_ALC0 saved-word
writes and raw readbacks are mandatory even after forward cancellation/failure.
MAC-active guards must match afterward. The parent always attempts MAC drain/
disable/filter/TXOP restoration and exact host endpoint stop;DMA is retained.
Quarantined EP0 refuses additional writes and cannot claim cleanup success.

Probe transport success is17 operations:12 reads,4 writes,1 wait. Its
`ibi_write_matched` and `ibi_changed` flags are derived from actual raw words;
unchanged/read-only behavior can complete the observation but cannot authorize
calibration or claim functional restoration of an unreadable register. A failed
raw saved-word restoration remains incomplete. No MCU calibration command,
packet inspection,rearm,network data OUT or operational transmit-power selection
is introduced.
Original strict full-calibration admission still refuses IBI9 all-ones and has
no native caller. Historical v1-v90 recipe bytes remain identical.

Final image/core and focused pure/native ASan/UBSan pass. Host fixtures cover
all callback failures/cancellations,fresh post-delay IBI saving,read-only versus
matching write effects,reverse restoration,actual selector69/parent hook,
69 native cancellation boundaries,mid-write EP0 quarantine and no replay.
176 staging tests include the actual smoke classification. Strict whole prior/
MAC/probe/RX validation refuses158 malformed proofs and accepts four raw/write-
effect variants;180 historical recipe byte comparisons pass. The first host
compile rejected misleading indentation in the new test;it was fixed without
relaxing warnings. A malformed-progress-line test tightened the collector's
exact line check;the original failing receipt is retained. All12 new/current
native printf formats have matching argument counts. Exact-stage VM/install
and native results remain pending until recorded below. Guest shows progress,
saves results and returns automatically;no photos or commands required.


### 2026-10-26 — v91 calibration-register trial installed, unarmed

Source `90b8a2036d31c8cd1b1794f05e57cc8a1611934a`, release
`7c1728fccd4ae567`, recipe `ram-usb-wifi-fw-v91-laptop-probe-0e535900-auto-return`.
Final image/core and actual native/pure-helper ASan/UBSan pass;176 staging
tests,158 malformed-proof refusals,four write-effect/raw-restore variants and
180 historical recipe-byte comparisons pass. Exact GPT-stage VM saved logs,
verified file bytes,unmounted and returned automatically. Missing workspace
refused without probing and returned. Q35 lacks the native adapter;these are
workflow/refusal checks,not calibration-register or reception acceptance.

Installed/current and e47 previous recovery match durable backups;accepted
v90 remains retained on ESP. Accepted v89 was removed from ESP only after full
payload equality to its durable backup,freeing candidate space. v89/v88
accepted evidence remain outside build. Ubuntu default and EFI order unchanged;
no one-shot armed. Offline327files/795clusters/137directories,zero leaks,bitmap
mismatches or errors;unmounted. No physical result is claimed yet.

After cold-cycle readiness,power off,unplug only Wi-Fi10s,reconnect and power on;
leave workspace attached. Guest shows progress,saves full logs and returns to
Ubuntu automatically;no photos or commands needed. On return collect the new
matching session FIRST with `build/bringup/cal-probe/collect_return.py`.
Require whole prior proof,17 ordered child operations,effect flags derived
from actual words,raw reverse restoration,MAC teardown and exact retained RX
stop. Distinguish completed observations from calibration admission;preserve
failed masks/cleanup markers. Candidate payload/run/evidence are retained under
`~/.local/share/srvros/macbookpro11-2/wifi-cal-probe-90b8a20/`.


### 2026-10-26 — v91 native IBI9 write effect and raw restoration verified

Source `90b8a203`, release `7c1728fccd4ae567`, session
`session-00065ec686798b50-00` completed and returned to Ubuntu automatically.
The strict collector verified the whole earlier firmware/MAC/BBP/RF/channel/
post-VCO/AGC proof, parent MAC lifecycle, new observation and retained RX stop.
The child completed 17 operations: 12 reads, four writes and one 500us wait.
Fresh TX_ALC0 was `2f2f1b1b`, suppressed to zero with independent readback.
IBI9 saved after the delay was `ffffffff`; its temporary `ffffff7e` write
read back exactly (`ibi_write_matched=1`, `ibi_changed=1`). Reverse saved-word
writes independently read back IBI9=`ffffffff` and TX_ALC0=`2f2f1b1b`.
Both raw restore masks were complete. This demonstrates a writable register in
this bounded context; all-ones alone did not imply an unreadable register.

Parent MAC startup/drain/disable/filter and TXOP restoration completed in
54 operations (46 reads, seven writes, one wait; hold10033us; TX1/RX7 drain
rounds). The one4096-byte RX request waited10001us and stopped with owned
context/dequeue and retained DMA. CC26/4096 remaining is cancellation metadata,
not a received packet. Total new writes11; no calibration command or packet
inspection. Full exit prefix and both completion markers were saved.
Ubuntu default, consumed one-shot, current/e47 payload hashes and retained v90
backup equality passed. Offline329files/802clusters/138directories, zero leaks,
bitmap mismatches or errors; workspace unmounted and no reboot armed.
Evidence is retained outside build in `wifi-cal-probe-90b8a20`.

Next: audit fresh board/power/TSSI context and initial power-on calibration
ordering, then implement bounded calibration admission and verified cleanup.
The original strict helper remains unchanged and refuses saved IBI9 all-ones;
this experiment does not silently relax that contract. RF0:6 semantics remain
unresolved. Calibration, controlled reception, scanning, association/security,
Wi-Fi IP traffic and UI remain untested.


### 2026-10-26 — initial power-on calibration transaction groundwork

Main `9465c68f` was merged, preserving the accepted v91 source as historical
hardware evidence. PR192 adds private child-image ownership; PR193/195 carry
Air evidence. Open PR194 owns window/dock behavior and PR182 filesystem work;
no overlapping MT76 implementation was found.

Pinned Linux v7.0 `mt76x0u_start` starts MAC before calling power-on calibration.
`mt76x0_phy_calibrate(true)` issues R(0), VCO(channel), waits10–20us, then
conditionally performs TSSI DC before the TX_ALC0/IBI9 FULL/LC transaction.
TSSI is selected by mapped NIC_CONF1 bit13, not by a guessed chipset capability.
This captured board has NIC0=`fd11`, NIC1=`1004`; bit13 is clear. Existing fresh
mapped antenna/trim captures already enforce these exact words. Vendor reference
also orders initial R/VCO before the saved-word sequence, though its detailed
calibration API differs; the new helper follows pinned Linux ordering.

`mt76_poweron_calibration_run` is separate from both existing helpers and has
no native caller. It requires captured ASIC/MAC, valid mapped board capture and
fixed channel1/20MHz. Its callback must additionally prove current ownership,
firmware/channel/power setup and same-lifetime field freshness before every
forward operation. Copying a valid-looking struct does not establish freshness.
TSSI-enabled/unknown profiles refuse before I/O. The sequence is R(0), VCO(1),
10us minimum wait, active-MAC guards, fresh TX_ALC0=`2f2f1b1b`, zero/independent
readback,500us minimum wait, fresh raw IBI9=`ffffffff`, temporary `ffffff7e`
with mandatory independent matching readback, FULL(600), LC(0),15ms minimum
wait, reverse IBI9/TX_ALC0 saved-word writes/independent readbacks, active-MAC
guards, RXDCOC(1), final guards/authorization. The all-ones IBI saved value is
an explicit captured-profile condition supported by v91 write-effect evidence;
other register all-ones values and mismatched temporary readbacks refuse.
Original strict non-power-on admission is unchanged.

A reported failed temporary write is marked as possibly applied before calling
transport. Both touched saved words get independent cleanup attempts even after
forward cancellation, board-context loss or restoration failure. No RXDCOC is
issued after incomplete restoration. Successful calibration commands cannot be
undone by saved-word restoration: the caller must always drain/disable MAC and
retire RX, retaining uncertain DMA and refusing retry after transport failure.
Semantic commands are not wire IDs; a future native adapter must validate exact
framing, sequence, replies and deadlines. This block does not introduce a new
boot profile, install a payload or enable calibration on hardware.

Host fault-injection covers each of28 callbacks,27 authorization boundaries,
27 board-context losses, every read mismatch/all-ones case, read-only IBI,
both reverse restoration failures, reentry/no replay and invalid identities,
board fields, callbacks and channel/width. Successful model counts are16 reads,
two forward writes,five commands,three waits and two cleanup callbacks; cleanup
callbacks must themselves perform a write and independent read on hardware.
Native RF0:6 semantics and operational power setup remain unresolved; this is
transaction groundwork, not radio calibration or packet reception acceptance.


Validation for power-on helper source `a614bd7f`: full image/core suite passed,
including merged process/scheduler regressions and existing Wi-Fi/xHCI suites.
Focused final fault injection and ASan/UBSan passed;674 local docs links pass.
GPT/OVMF USB boot and injected `echo poweron-core-ok` input passed. Q35 checks
merged boot/input behavior only; it has no captured adapter and establishes no
native calibration acceptance. Link audit confirms the new power-on runner and
old strict runner have no linked native caller; the v91 observation stays linked.
Existing helper/header and staging/smoke files are byte-identical to v91.
Source, reference audit and validation evidence are retained outside build under
`wifi-poweron-core-a614bd7`. Installed remains accepted v91 `7c1728fccd4ae567`,
e47 remains previous recovery; no physical run or one-shot was staged/armed.
Next: add the native command adapter with exact MCU reply/sequence/deadline
validation and verified MAC/RX cleanup, retaining fresh board/power admission.


### 2026-10-26 — v92 native power-on command adapter candidate

The explicit profile70/v92 chains the whole prior initialization and v91
reversible register observation before the separate power-on helper. Native
admission requires exclusive retained RX/MAC ownership, fresh mapped antenna
and trim fields, the captured channel1/20MHz/PA/gain/post-VCO proof, and the
parent TX_ALC0=`2f2f1b1b`/IBI9=`ffffffff` snapshot. v91 must demonstrate the
IBI9 temporary write effect and both raw restorations in this same lifetime.
Mapped NIC1004 has TSSI bit13 clear; enabled/unknown board profiles refuse.
This is initial calibration under captured default context, not operational
transmit-power selection or authorization to deliver network frames.

The command adapter allows only ordered R(0), VCO(1), FULL(600), LC(0),
RXDCOC(1), encoded as command31, IDs1/12/ff/3/2 and sequences1..5.
Each OUT/IN uses the existing bounded MCU transport. Ownership and the OUT
half-budget are checked before posting IN; aggregate elapsed must be <=1s.
The initial strict reply hypothesis is16 bytes: CMD_DONE/sequence header,
exact ID/argument echo and matching trailer, with exact actual length/residue.
Reference validates sequence/event; the stricter echo shape is a candidate
contract, not yet native evidence for these commands. A mismatch stops before
any later command and logs the actual receipt. Never relax it automatically.

A successful transaction contributes30 native operations:18 USB reads,four
writes,three waits and five command exchanges. Mandatory minimum waits are
10us/500us/15ms. Reverse saved-word cleanup is independently admitted after
forward cancellation, followed by parent device draining, MAC disable,
filter/TXOP restoration and exact retained endpoint stop. MCU timeout uses the
existing quarantine policy: it cannot claim saved-word or MAC cleanup through
a quarantined EP0, never retries or frees uncertain DMA, and remains failed.
Both restoration callbacks are still attempted by the transaction. Total new
register writes are15 (parent7, v91 probe4, power-on4). Successful commands
cannot be undone by saved-word restoration, and command completion does not
establish calibrated RF, packet reception or frequency lock. No packet
inspection/delivery, rearm or network data OUT is introduced.

Host fixtures execute actual selector70, parent/probe hook, EP0 setup/readback,
OUT/IN ring submission and reply validation. They cover91 USB ownership losses,
each command reply mismatch, all ten OUT/IN timeouts and all ten bulk ownership
losses, invalid board gates and no replay. Serializer tests refuse every reply
bit mutation and incorrect length. The strict whole-chain collector refuses329
malformed proofs;182 historical recipe-byte comparisons remain identical.
Early staging tests caught missing v92 recipe recognition and a stale unknown-
version fixture; both were fixed. The first timeout fixture wrongly expected
cleanup through quarantined EP0; actual quarantine behavior was preserved and
the fixture corrected. Initial proof-generation errors were caught by the full
valid fixture before staging; original failed receipts remain archived.
Local/core/sanitizer/exact-stage VM and physical results are recorded separately.
Guest progress and disk evidence remain automatic; no photos or commands needed.


### 2026-10-26 — v92 power-on command trial installed, unarmed

Source `b22a83eeb849e7c4a3aa1d58ee08e3610fe4eef4`, release
`7406fc019cb09f47`, recipe `ram-usb-wifi-fw-v92-laptop-probe-0e535900-auto-return`.
Full image/core suite passed on merged main43aff7d5; final expanded native
fault fixtures and pure/native ASan/UBSan passed.177 staging tests,329 malformed
whole-proof refusals and182 historical recipe-byte comparisons pass.17 native
printf argument counts were audited. The final rebuilt kernel and independently
firmware-packaged initramfs match the staged hashes. Exact GPT-stage VM saved
and byte-verified logs, unmounted cleanly and returned automatically; missing
workspace refused before probing and returned. Q35 lacks the captured adapter:
these are workflow checks, not native calibration or reception acceptance.

An initial stage `837f872d148a6ec3` preceded the explicit incremental rebuild
after the final comment edit. It was superseded before any installation or
one-shot; its separate pre-rebuild archive/VM receipt remains historical.
Only rebuilt release7406fc019cb09f47 was installed. Ubuntu default and EFI order
are preserved; e47 remains previous recovery and accepted v91 remains on ESP.
Accepted v90 was removed from ESP only after exact durable payload equality
was verified to free candidate space; its acceptance evidence remains archived.
Preboot workspace:329files/802clusters/138directories, zero leaks, bitmap
mismatches or errors; unmounted. No one-shot is armed.

After readiness, power off, unplug only Wi-Fi10s while off, reconnect and power
on; leave the workspace drive connected. Guest shows progress and saves results
before returning automatically; no photos or commands needed. On return collect
the matching v92 session FIRST with `poweron-native/collect_return.py`. Require
whole prior/v91 proof,30 ordered power-on operations and all five strict replies,
reverse saved-word restoration, parent MAC/device drain and retained RX stop,
full1MiB prefix and both completion markers. Unexpected replies, quarantine,
halts or missing markers are failures/incomplete, never passes. Evidence and
payload are retained outside build in `wifi-poweron-native-b22a83e`.
Command completion does not accept RF performance, packet reception, scanning,
association/security, Wi-Fi IP traffic or UI. No payload inspection or rearm.


### 2026-10-26 — v92 native power-on command transaction verified

Source `b22a83ee`, release `7406fc019cb09f47`, session
`session-00065ec7483fb865-00` completed and returned to Ubuntu automatically.
The strict collector accepted the whole prior firmware/MAC/BBP/RF/channel/
post-VCO/AGC proof, v91 reversible write observation, new power-on transaction,
parent MAC teardown and exact retained RX stop. All five command31 exchanges
matched the candidate16-byte CMD_DONE sequence/ID/argument/trailer contract:
R(0), VCO(1), FULL(600), LC(0), RXDCOC(1), sequences1..5, IDs1/12/ff/3/2.
Each sent/received16 bytes, CC13/residue1008. Observed command durations were
446/2417/5139/14156/762us; required waits10/500/15000us took11/501/15002us.
This establishes the exact reply contract for this adapter/context only.

Power-on completed30 operations:18 USB reads, four writes, three waits and five
commands; ACK mask31. Fresh TX_ALC0=`2f2f1b1b` was suppressed to zero and
independently verified; fresh post-delay IBI9=`ffffffff` accepted `ffffff7e`
with matching readback. Both reverse saved-word restores independently matched;
saved/modified/attempted/restored masks3 and cleanup complete. Active-MAC guards
passed before/after. The same-lifetime v91 observation also passed.

Parent MAC completed54 operations (46 reads,seven writes,one wait), TX1/RX7
drain rounds,10031us hold, MAC0 and exact filter/TXOP restoration. One4096-byte
main RX request waited10000us, then stopped with exact owned context/dequeue
and retained DMA. CC26/4096 remaining is cancellation metadata, not packet
reception. Total new register writes15. Full startup prefix and both completion
markers saved. Ubuntu default, consumed one-shot and current/e47 payload hashes
passed; accepted v91 remains retained. Offline331files/809clusters/139directories,
zero leaks, bitmap mismatches or errors; unmounted/unarmed. Evidence and payload
remain outside build in `wifi-poweron-native-b22a83e`.

This accepts bounded initial calibration command execution and cleanup; it does
not measure calibrated RF performance, frequency lock or received frames.
RF0:6 semantics remain unresolved, and original strict helper admission is
unchanged. Next is controlled passive main-RX observation with explicit actual
byte counts, strict frame parsing and bounded teardown. Scanning, association/
security, Wi-Fi IP traffic and UI remain pending. No new trial/reboot armed.


### 2026-10-26 — controlled passive RX candidate (v93)

Main `a9e39e38` (PR197 attended load and PR198 navigation) was merged. Staging
conflicts were reconciled by behavior, preserving all Wi-Fi recipes and separate
v76 navigation/v77 attended profiles; merged staging tests passed before new
work. PR182 remains filesystem scope, with no MT76 overlap.

New explicit profile71/v93 chains the entire accepted v92 transaction. It
retains the same single4096-byte main IN and the active MAC context, then polls
transfer events until completion or a1s deadline. No extra transfer/rearm,
network data OUT, probe request or channel change is submitted. Every poll
revalidates current ownership. Backward clocks,1024 consecutive no-progress
checks, the20M absolute poll cap or elapsed accounting beyond1.1s refuse.
The ordinary parent10ms hold remains separate; device drain/MAC disable/filter/
TXOP restoration and exact Stop Endpoint/dequeue proof still precede inspection.

Only a normal CC1/13 with one matching event and residue <=4096 supplies actual
bytes. Zero-sized normal completion, missing event and stopped CC26/27/28 give
zero usable bytes; even CC26 residue0 never licenses inspecting4096 bytes.
After all six disabled guards and proven stop, the unchanged strict RX parser
views only the first DMA/RXWI entry. It may decode a beacon/probe response;
malformed/unsupported/channel-mismatched data is recorded as rejected, not a
received-beacon pass. Raw payload, SSID and network addresses are never logged.
Only copied sizes/frame-control and non-identifying advertisement metadata are
retained; temporary beacon identifiers are scrubbed. The first-entry-only limit
and no FCE/FCS/calibrated-RSSI inference remain explicit.

Event processing is tagged as during the post-calibration window or outside it;
this is software timing, not an RF timestamp. An early completed transfer can
be decoded after teardown but cannot be relabeled as a post-calibration event.
The observation can complete with no data or rejected data without accepting
packet reception. It does not add scanning, association/security, Wi-Fi IP
traffic, reconnect policy or UI.

Host fixtures execute the actual mode71/whole-v92 hook, event polling, EP0/MCU
traffic, stop command/context and parsers.53 scenarios cover short/full/empty,
malformed/unsupported/off-channel data, early and late/foreign events, ownership
loss and backward/frozen/forward-jump clocks, pre-stop refusal,28 stop/length/
ownership guard mutations, poison pointers for cancellation/no data and no
replay/rearm/free. The first frozen-clock run repeated the full historical
ownership proof too long; it was terminated unaccepted. The1024 no-progress
bound fixed that behavior without caching or removing ownership checks.
The full log proof accepts14 whole-chain packet/empty/cancellation variants and
refuses185 malformed proofs.184 historical Wi-Fi recipe comparisons and both
merged desktop recipes remain byte-identical;180 staging tests pass. Pure and
native sanitizer, final source/image and VM results are recorded separately.


### 2026-10-26 — v93 passive RX trial installed, unarmed

Source `774e9e9d684e4cef21212a03cc8a8a99008f0a2d`, release
`6fcb4fe992b35b78`, v93 automatic-return laptop probe. Full image/core and
actual native observation plus strict parser ASan/UBSan passed, including the
100000 hostile-transfer cases.180 staging tests,14 whole-proof outcome variants,
185 malformed-proof refusals and184 historical Wi-Fi recipe comparisons pass;
both merged navigation/attended desktop recipes remain unchanged.20 native
printf formats match their argument counts. Original parser and calibration/
MAC transaction implementations remain byte-identical to the v92 source.

The final committed-source rebuild matches the staged kernel, and independently
adding the verified firmware package reproduces the staged initramfs hash.
Exact GPT-stage VM saved and byte-verified diagnostics, unmounted and returned;
missing workspace refused before probing and returned. Q35 lacks this adapter,
so these are workflow/refusal checks, not received-frame acceptance.

Installed/current and e47 previous recovery match durable payloads; accepted
v92 remains on ESP. Accepted v91 was removed from ESP only after exact durable
payload equality, freeing candidate space; its earlier evidence remains
archived. Ubuntu default/EFI order preserved; no one-shot armed. Workspace
331files/809clusters/139directories, zero leaks, bitmap mismatches or errors;
unmounted. Durable candidate/run/evidence: `wifi-passive-rx-774e9e9`.

After readiness, power off, unplug only Wi-Fi10s while off, reconnect and power
on; leave workspace connected. Guest shows progress, saves logs and returns to
Ubuntu automatically; no photos or commands needed. Collect the matching v93
session FIRST with `passive-rx/collect_return.py` on return. Require full prior/
v92 proof, bounded event window, independent MAC/device drain and exact stop,
then explicit normal-completion byte count and strict parser outcome. Empty or
rejected data can finish the observation but cannot accept frame reception.
Stopped residue never licenses reading bytes. No new IN/rearm, network data OUT,
scan/association or connectivity claim. No physical result is claimed yet.


### 2026-10-26 — v93 native passive beacon reception verified

Release `6fcb4fe992b35b78`, source `774e9e9d`, session
`session-00065ec7f927dacb-00` completed and returned automatically. Entire prior
firmware/MAC/baseband/RF/channel/AGC proof, reversible probe and all five v92
power-on exchanges passed. Passive polling processed a normal CC13 event after
10086us/534 polls during its post-calibration window. One4096-byte request had
3820 remaining, providing276 actual bytes. After independent MAC/device drain,
filter/TXOP restoration, all six guards and exact endpoint stop, the unchanged
parser accepted the first272-byte entry and234-byte management MPDU as a
channel1 beacon (frame control0080, interval100, empty SSID, privacy/RSN present,
WPA absent, DS channel present). No network identifiers or payload were logged.

This establishes one bounded native beacon observation only. It does not
measure sustained reception, FCS validity, calibrated RSSI, frequency lock or
RF quality. No endpoint rearm, network data OUT, scan, association/security,
Wi-Fi IP or UI was exercised. Parent MAC completed54 operations/46 reads/7
writes/1 wait,10ms hold10033us and full restoration. The single receive buffer
remains retained; stopped ownership was proven before inspection.

Full startup prefix and both completion markers passed the whole-chain checker.
Ubuntu default, consumed one-shot, current/e47 payload hashes and accepted v92
fallback were verified. Offline workspace checks:333files,816clusters,
140directories, zero leaks/bitmap mismatches/errors; unmounted and unarmed.
Durable evidence: `wifi-passive-rx-774e9e9`. Next is a separately reviewed bounded
repeated passive receive lifecycle with safe rearm/cancellation and teardown,
followed by scanning and connection support. No further reboot armed.
