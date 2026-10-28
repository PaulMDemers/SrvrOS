# USB Wi-Fi bring-up: captured Edimax adapter

Captured scope: `7392:a711` revision `0100`, Edimax EW-7711MAC, attached to
MacBookPro11,2. Linux binds `mt76x0u`. SrvrOS has bounded opt-in native transport,
initialization and management-frame reception diagnostics; channel1/channel6
beacons and bounded cleanup are observed. The fixed US1/6/11 passive lifecycle
and one pre-enable completion retry are accepted with a channel6 beacon;
sustained scanning and channel11 reception
remain unaccepted. Association/authentication, an active Wi-Fi IP provider and
configuration UI remain unimplemented. Ordinary startup leaves these diagnostic
radio operations disabled. See the latest milestone at the end of this guide.

## Captured baseline

USB high speed (480 Mb/s), one configuration, interface 0 / alternate 0,
vendor class/subclass/protocol ff/02/ff. Eight 512-byte bulk endpoints in order:
IN 84, IN 85, OUT 08, OUT 04, OUT 05, OUT 06, OUT 07, OUT 09. Endpoint roles
must be established from the chipset protocol; endpoint order alone is not a
safe data/firmware queue mapping. The descriptor SHA-256 is
`b06817886254add84af0fd5a7cbc8cd0b36c622bcecbc58f3e880e75f8de4eac`.

Run the read-only Linux capture against the actual sysfs device path:

```sh
python3 tools/wifi_usb_inventory.py --device /sys/bus/usb/devices/1-1 --output build/wifi-new-capture
python3 tools/test_wifi_usb_inventory.py
```

The tool reads cached descriptors and driver binding, refuses unknown topology
and never resets/unbinds the device, scans networks or reads credentials. Raw
radio capability/regulatory reports remain in local ignored evidence. Linux
advertised capabilities are not SrvrOS support or permission to transmit.

## Implementation sequence

1. Establish vendor register requests, endpoint roles, reset/MCU ownership and
   bounded cancellation. First native diagnostic should read identity only,
   under explicit opt-in, without registering an active IP provider.
2. Pin and validate the firmware format, lengths, hash and redistribution
   terms before packaging. Linux advertises both `mediatek/mt7610e.bin` and
   `mediatek/mt7610u.bin`; its current loader prefers the former and falls back
   to the latter. Installed filenames alone do not prove which image ran.
   Capture actual ASIC/MAC revision and EEPROM/regulatory calibration before
   enabling a transmitter. Keep firmware failure and removal paths bounded.
3. Receive and parse management frames; add controlled scan/channel handling
   subject to calibrated regulatory limits. Then implement station association,
   WPA2-Personal/CCMP authentication and key lifecycle, with explicit disconnect
   and stale-key cleanup. Never silently fall back to an open connection.
4. Expose Ethernet frames to the existing IP provider only after association and
   authorization. Validate DHCP, DNS, HTTP, reconnect, USB removal, input and
   workspace coexistence. Keep one active IP interface initially.
5. Add background scan/connect/cancel and clear radio/association/IP states to
   the desktop UI. Credentials must not enter logs or committed fixtures.

Each step needs fixtures and bounded fault tests before a native trial. Generic
QEMU USB Ethernet coverage does not emulate this radio. The next implementation
checkpoint is calibrated radio initialization and receive/scan support; identity
and firmware readiness now have native evidence for this exact adapter.

## Implemented identity diagnostic (native register read accepted)

`kernel/src/drivers/mt76_probe.c` implements a transport-independent identity
probe. It issues exactly two device-recipient vendor IN requests (`c0/07`),
with value zero, indices `0000` (ASIC_VERSION) and `1000` (MAC_CSR0), and length
four. Responses are little-endian. A short/oversized reply, transport failure,
zero or all-ones register stops immediately, without retries or publishing a
partial identity. Other register values are reported as raw observations, not
accepted silicon revisions. The caller must gate on the captured USB topology.

The xHCI transport uses `ISP` on the single data-stage TRB and `IOC` on the
status stage. It records a short packet's actual length (including zero), then
requires successful status completion. Malformed residue, duplicate short events,
endpoint errors or a bounded-loop timeout quarantine EP0. Its ring/context/buffer
allocations remain retained, and later control requests cannot reuse them. Other
slots' transfer events are dispatched normally. Existing control callers retain
their prior completion behavior; only the diagnostic uses this strict path.

`srvros.usb-wifi=probe` explicitly enables the diagnostic during USB enumeration.
It requires the captured vendor/product/revision, high speed, EP0 size and exact
configuration topology. It reads ASIC_VERSION and MAC_CSR0 without selecting a
configuration, resetting the radio, loading firmware or registering a network
provider. `xhci-wifi` lines record the raw values, failure and quarantine state
both at probe time and in the boot diagnostic snapshot. Default boots leave it
disabled. Read success is not acceptance of the revision or permission to transmit.

The immutable v39 capture profile is staged
with `python3 tools/hardware_boot.py stage --laptop-probe-volume <local-uuid>
--wifi-capture` (one command). Rehearse with `python3 tools/hardware_boot_smoke.py
--probe-check --workspace-table gpt` before installing. It saves identity/status
diagnostics, marks missing or failed identity as `completed=NO`, unmounts and
returns automatically even on handled failure. QEMU has no captured radio, so
the rehearsal requires explicit unavailable identity plus saved logs and reset;
this is a refusal-path pass, not physical identity acceptance.
Do not detach the adapter from Linux while it carries the development connection.

The firmware parser validates the 32-byte header, little-endian ILM/DLM lengths,
64-byte initial vector block inside ILM, and exact total file length. It returns
bounded offsets for IVB, remaining ILM and DLM plus build/firmware versions.
Subtraction-based checks avoid length overflow. This only establishes structural
validity: hash pinning, licensing, actual ASIC compatibility and calibration are
still prerequisites for upload. No firmware bytes are bundled, uploaded or
selected by this parser; tests use synthetic headers, not proprietary payloads.

Run `make test-mt76-probe` for the contract and captured-topology tests. These
cover every fixture truncation, request encoding, failure on either read,
invalid identities, firmware boundary lengths, oversized lengths and cleared
outputs after failure. They do not emulate a radio or accept physical Wi-Fi.

## Primary references

- [Linux device table and initialization](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/usb.c)
- [Linux MCU firmware loader](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/usb_mcu.c)

These are protocol research references, not imported driver code. Check each
file's license before any code reuse; the SrvrOS root license does not relicense
third-party sources or firmware. Firmware packaging is not part of this change.

- [Vendor read encoding](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/usb.c)
- [Vendor request numbers](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76.h)
- [Register offsets](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_regs.h)
- [Firmware header](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_mcu.h)
- [Vector block size](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/mcu.h)

- [xHCI control transfer semantics, sections 4.11.2 and 6.4.1](https://cdrdv2-public.intel.com/625472/625472_xHCI_Rev1_2b.pdf)

## Native identity result, October 21

MacBookPro11,2 release `5d65ddcc4ccbeaf2`, source `b1aa1ca`, session
`00065e52c454a4dd-00` read ASIC_VERSION `0x76100002` and MAC_CSR0 `0x76502000`.
Both reads completed with `quarantined=0`; the saved exit snapshot records
`wifi-capture: identity=read` and `laptop-capture: completed=yes`. Boot/exit logs
match the installed release and are independently archived with hashes. This
accepts the two vendor register reads on this captured adapter only. It does
not accept firmware upload, calibration, radio initialization or connectivity.

Ubuntu return and consumed one-shot were verified. Postboot offline filesystem
and allocation checks pass: 29 directories, 111 files, 415 clusters, zero leaks,
bitmap mismatches or errors. The next driver step is firmware provenance/hash
pinning and revision/calibration checks before enabling any radio writes.

## Firmware audit checkpoint

The Ubuntu `linux-firmware` package `20260319.git217ca6e4.1ubuntu` supplies the
following compressed files. After local decompression, their bytes match the
same files at upstream linux-firmware commit
`431e7f03d2c77c3d027d02c9160b15f0f245d882` exactly:

| Candidate | Bytes | ILM including 64-byte IVB | DLM | SHA-256 |
| --- | --- | --- | --- | --- |
| mt7610e.bin | 80680 | 69172 | 11476 | `538d08c08f8fb5aa05cacf34e49aeb307e8b2dc094407fc655fbf3580c893953` |
| mt7610u.bin | 80288 | 68780 | 11476 | `5a4268e9021bb587426ba624b425f1e660bfc82cd63b36ad3ce6fb9ce6751760` |

Both headers report build `0x7640`, firmware `0x0100`. Their build-time fields
are `201404011018____` and `201308221655____` respectively. These are candidate
provenance observations, not evidence of which image Linux loaded on this boot.
The pinned Linux loader prefers mt7610e and falls back to mt7610u; SrvrOS has not
yet selected or uploaded either. The MAC register value does not justify choosing
a different firmware filename by itself.

`python3 tools/wifi_firmware_audit.py /local/path/mt7610e.bin` audits a locally
decompressed file. It bounds the input, checks header/payload lengths, and rejects
unknown names or hashes. It never uploads, installs or bundles firmware. Run
`make test-wifi-firmware-audit` for malformed/truncated/unknown-image tests. Raw
firmware and research files remain in ignored local evidence, not Git or initramfs.

The upstream [WHENCE entry](https://kernel.googlesource.com/pub/scm/linux/kernel/git/firmware/linux-firmware/+/431e7f03d2c77c3d027d02c9160b15f0f245d882/WHENCE)
assigns these images to [LICENCE.mediatek](https://kernel.googlesource.com/pub/scm/linux/kernel/git/firmware/linux-firmware/+/431e7f03d2c77c3d027d02c9160b15f0f245d882/LICENSES/LICENCE.mediatek).
Future packaging must retain that license and provenance separately from SrvrOS's
MIT source license. No firmware redistribution is added by this checkpoint.

Before a transmitter is enabled, capture and validate this device's EEPROM/EFUSE
layout/version, chain/band capabilities, frequency/temperature offsets and power
calibration. Reject erased, malformed or unsupported data; do not substitute
another device's values or Linux's current regulatory setting. The upstream
[EEPROM implementation](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/eeprom.c)
and [initialization order](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/init.c)
are protocol references. Calibration capture needs a separate bounded diagnostic:
indirect EFUSE access may require control-register writes, so it is not equivalent
to the accepted two-register read. Firmware/reset/DMA ownership, MCU readiness and
failure teardown also remain implementation work. Hash acceptance alone must not
turn on the radio.

## Capture engine and upload planning (host tested, not wired)

`mt76_bringup.c` adds a transport-independent logical EFUSE capture for the
accepted ASIC/MAC pair. It reads 32 blocks into a private 512-byte buffer, records
unmapped blocks, and refuses an entirely erased/empty capture. Control writes
are limited to EFUSE_CTRL (`0x24`), clearing all mode bits to select logical read,
preserving timing/selection and setting the aligned address and KICK. There is
no programming-mode API. Busy entry, I/O failure, cancellation or exhaustion of
the shared 1,024-poll budget stops immediately and clears all partial output.
No new block is started after the budget expires. Delay requests are bounded;
the transport must also bound each I/O and retain DMA on uncertain completion.
A failed capture must not be retried or used to initialize the radio without an
explicit recovery path. Successful raw capture is not calibration validation.

The same module provides a pure firmware chunk plan: remaining ILM at destination
`0x40`, DLM at `0x80000`, then the 64-byte IVB control payload. Bulk payload chunks
are at most `0x38f0` bytes, with four-byte padding and an eight-byte envelope
budget. Overlapping ILM/DLM windows and out-of-range indices are rejected. The
planner does not encode or send USB packets, reset the MCU, verify a hash, wait
for readiness or activate firmware. Its caller must verify the pinned candidate
and acknowledge every previous transfer before advancing to the IVB activation.

`make test-mt76-bringup` exercises every failure position in a complete capture,
initial busy state, delayed/stuck completion, shared-budget exhaustion across
blocks, unmapped/empty data, cleared outputs and segment/tail/alignment boundaries.
The tests use synthetic calibration only. Address/length references are pinned
[EFUSE protocol](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_eeprom.c),
[register definitions](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_regs.h)
and [firmware transport](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_usb_mcu.c).
Neither module is called during native enumeration. No firmware is bundled or
uploaded. The existing v39 identity-only recipe remains unchanged.

## Opt-in native calibration capture

Stage with `python3 tools/hardware_boot.py stage --laptop-probe-volume UUID
--wifi-calibration` (one command), then rehearse that exact stage with
`python3 tools/hardware_boot_smoke.py --probe-check --workspace-table gpt`.
The immutable v43 profile starts workspace logging before attempting capture,
saves the result, and returns automatically. QEMU without this adapter must
record completed=NO and no calibration data; this tests refusal and recovery,
not physical calibration support.

The transport permits four-byte reads only at EFUSE control/data registers and
writes only to EFUSE_CTRL in logical-read mode, using a fresh idle control value.
It rejects changed timing/selection, programming modes and invalid addresses.
Strict IN/OUT completions share an event owner with HID/storage dispatch, so a
worker cannot discard capture completions. Short writes, errors and timeouts
quarantine EP0 and retain its DMA; failed captures cannot retry in that boot.
Successful data logs redact EEPROM MAC bytes 4–9. Partial data is never logged.
A successful capture still requires board/version/regulatory validation before
radio initialization. Native v43 release `47997cb8604043ff` captured all 32 blocks
on the Pro11,2 adapter with result=0 and no quarantine. Unmapped blocks match
the erased-byte representation; the six MAC bytes are redacted. This accepts
the logical read transport, not the contents as usable radio calibration.

## Calibration audit and prepared firmware transport

`python3 tools/wifi_calibration_audit.py IMAGE --unmapped HEXMASK` audits a
512-byte logical image offline without printing its MAC. It requires a recognized
chip/version, single TX/RX chain, mapped identity/configuration fields and erased
bytes for every unmapped block. The captured Pro adapter passes: chip 7610,
EEPROM version 2 / FAE 1, NIC configuration fd11/1004, raw region bytes 10/30,
frequency byte 68 and 15 unmapped blocks. These raw region fields are not a
country selection or permission to use channels. Physical usage-map validation,
power/gain decoding and regulatory channel policy remain separate requirements.
The audit deliberately reports radio_authorized=false even on structural success.
Field interpretation follows pinned Linux v6.18
[EEPROM handling](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/eeprom.c)
and [layout definitions](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_eeprom.h).

The pure firmware packet encoder emits the CPU-port command header, unpadded
length, data, zero alignment padding and zero trailer. The new one-shot upload
engine requires an explicitly prepared transport whose authorization callback
verifies image hash, captured device and exclusive DMA ownership. It orders FCE
address/length, exact bulk completion, descriptor advancement and bounded pacing;
only then sends IVB activation and polls MCU readiness for at most 1,000 attempts.
Every error/short transfer/cancellation stops without retry. All-ones reads never
count as readiness. Transport callbacks must retain/quarantine DMA after uncertain
completion; callers must keep image/scratch alive and disjoint through teardown.
The engine does not perform MCU reset, configure USB endpoints/DMA, verify hashes
itself or initialize RF. It has no native caller. Firmware remains unbundled.
Wire ordering follows the pinned
[USB firmware transport](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_usb_mcu.c).
`make test-mt76-upload test-wifi-calibration-audit` covers structural refusals,
packet bytes/padding, operation ordering, every-operation failure, short bulk/IVB,
no retry, overlapping buffers and readiness timeout. Native upload is untested.

## Preparation and explicit firmware packaging

`mt76_prepare_firmware` models one-shot MCU/FCE preparation. It refuses an
already-running/unreadable MCU and busy/unreadable DMA state, checks every
operation, bounds the post-reset delay and never retries a failed session.
The xHCI strict control path now also retires zero-data requests by their status
completion, covering reset and the two half-word FCE vendor writes without
falling back to permissive legacy completion handling.

There is an intentional difference from Linux's loader: its `MAC_SYS_CTRL=0x2c`
sets TX/RX enable bits. This diagnostic preparation uses `0x20` and checks the
readback before reset and after preparation. Nonmatching readback refuses the
trial. This is a proposed disabled-MAC readiness path, not physically accepted
firmware compatibility. Native integration must preserve exclusive endpoint/DMA
ownership and check MAC state after firmware activation as well. The explicit
native firmware-readiness caller is described below.
See the pinned [MAC control bits](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_regs.h)
and [upstream preparation sequence](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/usb_mcu.c).

`tools/wifi_firmware_package.py IMAGE --license LICENSE --output NEW_DIRECTORY`
creates an explicit local package containing the hash-pinned binary, exact
hash-pinned MediaTek license and manifest. It refuses existing destinations,
unknown firmware, malformed layout and changed license bytes. Inputs are bounded;
output is staged before publication. The local mt7610e candidate was packaged and
verified; no firmware bytes enter Git, ordinary OS images or the installed boot
payload. Consuming that package in an immutable opt-in image still requires
kernel-side identity/hash verification, endpoint transport and exact-stage tests.
`make test-mt76-prepare test-wifi-firmware-package test-xhci` tests ordering,
every-operation failures, busy/live/refusal cases, disabled-MAC checks, package
refusals and strict zero-data completion/error/quarantine semantics.

## Native firmware-readiness diagnostic

The opt-in native transport now configures only captured OUT endpoint 08, before
runtime workers start. It uses dedicated retained ring/DMA allocations; the 16K
buffer is aligned so no transfer crosses a 64K TRB boundary. Configure-endpoint
requires the exact command pointer and slot; uncertain configuration disables
further controller commands. Runtime firmware events share the common dispatcher
with HID/storage and require successful completion with zero residue. Errors,
short writes, Event Data events and timeouts quarantine the device with DMA held.
FCE writes are restricted to the current planned destination/length and encoded
as two exact zero-data vendor requests. Descriptor advancement follows successful
bulk completion; IVB is available only after every chunk. Register callbacks
refuse TX/RX-enabling MAC values and unrelated addresses.

The kernel verifies the exact 80,680-byte mt7610e SHA-256 before MCU writes and
again before upload. The firmware lives in immutable initramfs data; no workspace
firmware, mutable manifest trust or fallback image is accepted. Preparation still
refuses a live MCU, and the final diagnostic requires MAC control 0x20 after MCU
readiness. An unexpected MAC value after successful startup fails acceptance and
triggers one bounded disable attempt. There is no RF initialization or network
provider. A successful firmware readiness test is not connectivity acceptance.

Stage with `python3 tools/hardware_boot.py stage --laptop-probe-volume UUID
--wifi-firmware-package PACKAGE_DIRECTORY` (one command). The descriptive
`ram-usb-wifi-fw-v1` profile avoids the shared numeric namespace. Only its staged
copy of initramfs receives the pinned firmware and exact license; both are
revalidated during payload verification. Default build images remain unbundled.
The existing automatic capture saves diagnostics and returns to Ubuntu; rehearse
with `tools/hardware_boot_smoke.py --probe-check --workspace-table gpt` first.
Native readiness has not yet been accepted.

For attended investigation of missing logs, add `--hold-on-failure`. This selects
that profile without `-auto-return`; existing recipes remain unchanged. Failed
capture or save/unmount stays on screen with a cached prerequisite summary and
instructions to photograph it, hold the power button until off, then power on
for Ubuntu. Success still returns automatically. Rehearse both `--probe-check
--workspace-table gpt` and `--probe-check --probe-missing-workspace`; the held
fixtures require a live halted guest, not a reset. Confirm the user is ready for
manual recovery before the cold-start shutdown.

Integration note: Pro calibration and Air app-file branches independently
published v43. Verification now recognizes both exact historical recipes and
requires identical config copies; it does not infer the mode from the host UUID.
Main integration22fd266 also introduced battery controls under v44. New
calibration stages now use `ram-usb-wifi-cal-v1`; battery app-file/control stages
retain v43/v44. Verification accepts only the exact historical Wi-Fi or battery
recipe for those old versions and still requires identical config copies.
Installed payload bytes are unchanged. Firmware stages use their descriptive name.

### Cold-start prerequisite for the native upload diagnostic

On Pro11,2, the warm-boot firmware diagnostic observed MCU register 0x730=1
on its first read, before reset or upload. Ubuntu had bound mt76x0u and used the
adapter for its default route. This is the intentional already-running-MCU
refusal, not firmware upload acceptance. Cached progress records the last
successful read and preparation/reset/upload state without adding hardware I/O.

To test upload from a cold adapter, retain the exact rehearsed image and recovery,
prepare an attended shutdown run record, and confirm the user can perform the
physical step before shutting down. Once fully off, unplug only the Wi-Fi adapter
for ten seconds, reconnect it, then power on into the one-shot diagnostic. Leave
the workspace drive connected. Collect the new matching session on Ubuntu return.
An ordinary reboot, USB logical reset or unbind does not establish removal of
adapter power; do not relax the live-MCU gate to bypass this prerequisite.

The attended cold retry on Pro11,2 timed out at the first device descriptor on
root port 1, before Wi-Fi identity or firmware preparation. The driver now allows
10 ms of controller-clock recovery after successful Address Device completion
before requesting descriptors, consistent with the settling delay in
[Linux v6.18 hub enumeration](https://github.com/torvalds/linux/blob/v6.18/drivers/usb/core/hub.c).
Host coverage checks the interval across MFINDEX wrap, stalled-clock quarantine
and failed address commands. This fixes a missing recovery interval; whether it
resolves this adapter's cold-start timeout still requires native acceptance.

Firmware control diagnostics cache the last submitted setup packet and matched
EP0 completion without extra reads or retries. `wifi-firmware-control` identifies
type/request/value/index/length; `wifi-firmware-control-result` reports actual
length, success, completion code, residue, stage (1 setup, 2 data, 3 status, 0 no
matched event) and timeout. Unrelated/stale event pointers do not populate these
fields. A reset-attempt flag alone never proves reset completion.

Firmware-active EP0 waits now use the pinned reference driver's 300 ms vendor
request deadline, measured by the monotonic clock, plus a bounded stalled-clock
fallback. Generic control waits remain unchanged. There is no automatic retry
or endpoint reset. `wifi-firmware-endpoint` records elapsed microseconds and the
controller-owned EP0 state/dequeue after the wait. The MCU reset delay remains
5 ms on the independent ACPI PM timer; timing changes require native validation.

The cold-start preparation now enables WLAN and its clock (0x80 bits 0–1),
preserving GPIO and antenna selection. It refuses an unreadable register or
asserted reset bits, checks readback (allowing live GPIO input changes), and
bounds crystal/PLL readiness checks at 100 polls with 20 µs spacing. This follows
the prerequisite in [Linux v6.18 chip initialization](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/init.c),
without replaying its optional GPIO/RF reset. MAC TX/RX remains disabled.
`wifi-firmware-clock` reports cached before/after WLAN and final CMB state.
Host verification does not accept native clock or firmware readiness.

On a firmware EP0 timeout, a one-shot Stop Endpoint command now requests a
stopped output-context snapshot. `wifi-firmware-td` lists submitted stage TRBs;
`wifi-firmware-stop` records exact-command success, endpoint state/dequeue and
the first late or stopped transfer event. Interpret the dequeue only after a
successful stop with state 3; a snapshot does not prove why the device stalled.
A failed command blocks later controller commands. Most failures remain
quarantined. The bounded recovery below permits one retry for each of two
observed operations after verified retirement; no old DMA is reused.

### Explicit MAC-startup comparison

Add `--wifi-mac-startup` to the firmware-package staging command to select the
separate `ram-usb-wifi-fw-v2` recipe. v1 and its command line remain unchanged.
The v2 kernel opt-in allows one 0x1004=0x2c write before MCU reset, matching
[Linux v6.18 firmware preparation](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/usb_mcu.c).
This temporarily enables MAC TX/RX. After FCE preparation it writes and verifies
0x20 before allowing upload; the final readiness check still requires disabled
MAC. No channel/RF configuration or network frames are submitted. v2 is included
in the accepted readiness run below; that does not prove the startup value was
necessary or that disabled MAC caused the earlier timeout.
The transport requires the opt-in, preparation ownership, no prior reset/upload
and a single startup write. Only the two operations described below may retry
after verified retirement. Other failures can leave MAC startup enabled until
power-off. Default stages save diagnostics and return automatically; photos
are not required when matching logs are available. Cached
`wifi-firmware-mac-startup` records requested
and written state; these flags do not prove restoration or readiness.

### Native firmware readiness accepted (October 22)

MacBookPro11,2 release `b7f7fb1c357d7ff5` (source `122fcda`) completed the
captured Edimax firmware trial: result0, six chunks, MAC disabled, no quarantine,
and both capture completion markers yes. Two bounded recoveries succeeded.
The exact session and recovery checks are in the
[machine record](machines/macbookpro11-2.md). This accepts firmware loading and
MCU readiness only; it does not accept RF, scanning, association or connectivity.

Two Linux cold captures showed cancellation near300 ms followed by a successful
retry about10 ms later for the first FCE write0x800=1 and post-IVB ready read0x730.
SrvrOS permits one retry per operation only after exact Stop Endpoint completion,
state3 and a stopped data TRB with four bytes remaining. Exact Set TR Dequeue
completion and context readback switch EP0 to a fresh ring/buffer; retired DMA
remains allocated, excluding late events by address. A10 ms delay precedes the
single retry. Command/allocation/delay uncertainty or another failure quarantines;
reset, bulk upload and other writes are never blindly replayed. Saved
`wifi-firmware-recovery` reports attempts/successful/mask. Successful recovery
alone is not firmware acceptance: require the final result/chunks/MAC/quarantine
and completion markers. Next work is calibrated radio initialization and
receive/scan support, followed by station authentication and IP integration.

### Receive-calibration audit

`tools/wifi_calibration_audit.py` now decodes receive-side EEPROM metadata in
addition to the structural checks: frequency base/correction, temperature,
LNA gains, RSSI offsets, band hints and PA/LNA flags. The interpretation follows
[Linux v6.18 mt76x0 EEPROM helpers](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/eeprom.c)
and [shared receive helpers](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_eeprom.c).
Unmapped fields remain unknown and are listed; decoded fallback/clamped values
are labeled separately from raw fields. Frequency arithmetic still needs hardware
range validation. The audit never prints the MAC and never authorizes radio use.

The accepted redacted logical capture has all receive fields mapped. Its decoded
frequency offset is68; the corrected reference decoding of temperature metadata
bytef8 is+120 calibration units (not a physical temperature). The older-8
audit used the wrong encoding. 5GHz low/mid/high LNA gains are12/12/12,
and RSSI offsets1/1 for2GHz and0/0 for5GHz. The board hint is both bands; it is
not a country/channel policy or proof of usable bands on the finished product.
Per-channel transmit power, physical usage-map validation and regulatory policy
remain separate prerequisites. These offline results do not accept native RF
initialization or scanning and need no reboot.

### Transmit calibration and channel constraints

The offline audit now decodes the non-TSSI 20MHz channel table and signed
six-bit rate offsets from the same pinned Linux EEPROM interpretation. It
reports chipset calibration units, not dBm/EIRP. Invalid or unmapped bytes stay
unknown; TSSI and wide-channel interpretation remain unsupported. Rate-table
entries describe the EEPROM groups (paired rates except VHT8/9), not a final
hardware power table.

The captured adapter has values5 on2GHz channels1–14; 5GHz values17–20 through
channel165. Channels169/173 have invalid calibration bytes and177 lies beyond
the decoded map; no fallback makes them eligible. The saved Linux PHY snapshot
also supplies separate disabled, radar/DFS and no-IR restrictions. A populated
EEPROM entry alone never overrides these restrictions or establishes a country.

Initial radio work must keep calibration separate from channel policy: require
an explicitly confirmed country, a dated authoritative regulatory rule set,
verified device capabilities and valid calibration. Keep transmit disabled until
these are integrated with hardware power/range validation. Initial bring-up will
exclude DFS, no-IR, unsupported bandwidths and channels with missing calibration;
active scanning must not precede this gate. Linux's current domain is only a
local observation, not automatic country selection for SrvrOS. Native receive,
scan and transmit remain pending.

### Offline US development channel filter

`tools/wifi_channel_policy.py` intersects the calibration audit with paired
`iw phy ... info` and `iw reg get` snapshots from the same adapter and explicitly
confirmed country US. Its development set is 20MHz channels1–11,36/40/44/48 and
149/153/157/161/165. This is a limited development set, not a regulatory database.
Only whole unflagged PHY rows and unflagged/AUTO-BW regulatory snapshot rules
covering the entire20MHz channel qualify. Unknown flags, no-IR, DFS, disabled or
missing rows, inconsistent frequencies, missing calibration and other countries
or widths refuse. Duplicate channel records are errors.

```sh
python3 tools/wifi_channel_policy.py --audit audit.json --phy linux-phy.txt \
  --regulatory linux-regulatory.txt --country US > policy.json
python3 -m unittest discover -s tools -p test_wifi_channel_policy.py
```

The report hashes its inputs and distinguishes `candidate` from
`transmit_authorized` (always false). Its `snapshot_ceiling_dbm` is the lower PHY
and regulatory snapshot ceiling; EEPROM calibration units are never interpreted
as dBm. Snapshots are historical reference evidence, not proof of current native
radio constraints. This tool neither changes the driver nor authorizes active
scan. Native RF initialization, validated power programming and native regulatory
enforcement remain required before transmission.

### Native MCU command prerequisite

The freestanding `mt76_mcu` module provides bounded command framing and response
field decoding, based on Linuxv6.18
[USB framing](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_usb_core.c)
and [MCU command transport](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_usb_mcu.c).
It emits little-endian CPU-port headers, four-byte payload alignment and zero
trailer, with a192-byte payload ceiling. It does not authorize command IDs.
Response decoding is explicitly not completion validation: event meanings,
reported length and command-specific bodies still require a validated contract.

The session helper serializes commands and refuses sequence reuse after15 issued
commands. Failed or mismatched retirement poisons the session; an uncertain
transfer cannot be retried simply by clearing the struct. A future transport must
retain DMA on uncertainty and prove endpoint teardown/draining before starting a
new session. This deliberately limited diagnostic model is not yet a sustained
radio command scheduler. No MCU transport is connected to it and no RF registers
are changed. Native endpoint ownership, command-response capture, initialization
tables and calibration/power programming remain pending.

`make test-mt76-mcu test-wifi-channel-policy` checks command framing, all payload
lengths and padding, capacity refusal, malformed response bounds, sequence
exhaustion, stale completion and poisoned-session behavior, plus the offline
country/channel policy. Both targets participate in `test-core`.

### One-shot native MCU transport capture

Firmware profilev3 (`--wifi-mcu-probe`, requiring `--wifi-mac-startup` and the
pinned firmware package) adds one CMD_RANDOM_READ of WLAN MAC_CSR0 after successful
firmware upload and a disabled-MAC check. It reuses the retired OUT8 ring and adds
an isolated bulk IN0x85/DCI11 ring and buffer, matching the captured descriptor.
Exact TRB/DCI completion, bounded500ms monotonic waits, short-IN residue bounds,
command slot checks and permanent uncertainty quarantine protect this diagnostic.
All DMA allocations remain retained, including failed configuration/timeout.

The first16 response bytes and decoded header fields go into the existing saved
workspace log. They contain this fixed register-read reply, not received wireless
packets. A second EP0 read checks that MAC remains disabled. `validated=0` is
intentional: capturing a matching-sequence response does not validate its event,
length/body contract, RF state or networking. Transport failure makes the capture
incomplete; normal automatic-return behavior is retained. Older firmwarev1/v2
profiles are unchanged. No ordinary desktop startup invokes this diagnostic.

The MCU diagnostic now sends fixed Q_SELECT=1,value1 with sequence0 before the
read command. Linuxv6.18 sends this setup without requesting a reply; ordinary
commands continue to reject sequence0. Both requests use the same ordered OUT
ring, and failure of selection stops the trial before the register read or IN
submission. `wifi-mcu-queue` records selection transfer completion, which alone
is not proof of firmware processing. This addresses the missing setup identified
after the first native MCU response timeout; native confirmation is pending.

The Pro11,2 queue-selection trial (source456922b, release4a47cd57ac90c504)
received a16-byte response after both16-byte OUT transfers. Observed response:
header4a010008,address00411000,value76502000,trailer4a010008; decoded sequence1,
event0,length8. xHCI short-packet completion13 with1008 bytes residue from a1024
byte IN request is consistent with those16 received bytes. Final MAC-disable
check and saved capture completed successfully. This is bounded exchange evidence
only; strict native body/trailer validation and RF initialization remain pending.

Strict identity-reply validation now requires the complete observed16-byte
response, including header flags,sequence1,event0,length8,address00411000,
value76502000 and matching trailer. It is deliberately specific to this pinned
one-shot diagnostic; other commands/identities need their own contracts.
Malformed lengths, unknown header bits, wrong values and even internally
consistent stale sequences refuse. The diagnostic reports validated=1 only for
that exact reply, and still requires the final disabled-MAC check for success.
This validates no RF state or general MCU service. The installed historical
queue-selection trial retains its original validated=0 behavior.

### MAC/baseband reset prerequisite engine

`mt76_radio_prepare_reset` implements only the bounded reset prerequisite, following
Linuxv6.18 `mt76x0_init_hardware`/`mt76x0_reset_csr_bbp` ordering. It requires the
captured identity, live transport authorization before each operation, a matching
MAC_CSR0, disabled MAC0x20, and idle DMA/MAC. Idle polls have at most101 reads and
100 one-millisecond delays. Reset writes are limited to MAC_SYS_CTRL values3 and0;
TX/RX enable bits remain clear. The200ms reset hold uses40 bounded five-millisecond
delays with cancellation checks. Release and idle state must then verify.

Each session attempts at most once; uncertainty stops immediately. A failure can
leave reset asserted, so callers must not retry by clearing the session. The
future xHCI binding must preserve transport quarantine and hardware-lifecycle
ownership. This module is host-tested but not yet called by a native diagnostic.
It programs no MAC/baseband tables, RF, channel or transmit power. `prepared`
means only this reset prerequisite completed, not that the radio is operational.

`make test-mt76-radio` (included in test-core) covers the successful order/hold,
every operation failing, ownership loss, busy timeout, all-ones registers,
initial enabled MAC, failed reset release and repeated-attempt refusal.

Firmware profilev4 adds `--wifi-radio-reset` (requires MCU probe, MAC-startup and
pinned firmware package). After upload it runs the reset prerequisite through a
separate xHCI binding, then selects the response queue and performs the strict
identity read. The binding permits only reads1000/1004/0208/1200 and ordered
MAC_SYS_CTRL writes3 then0, with a minimum200ms hold before release. It refuses
outside the active reset phase or after MCU response endpoint configuration.
Existing firmware write permissions are unchanged. Reset or I/O failure stops
and quarantines the device; no retry or DMA free follows uncertainty.

`wifi-radio-reset` logs status, reset/release/prepared flags, operation counts,
delay and last register/value. Success additionally requires the exact observed
MCU reply and final MAC_SYS_CTRL=0 (v3 still requires0x20). No RF/channel/power
tables or receive operation are enabled. Older profile recipes remain unchanged;
v4 uses the existing saved-log automatic-return workflow.

Native firmwarev4 reset capture accepted on the captured Pro11,2 Edimax adapter:
source2bc3689/releasebfa7235c397175da/session00065e747b16d8cb-00. Reset completed
with9reads,2writes and200000us hold; firmware identity reply passed strict
validation afterward and final MAC_SYS_CTRL remained0. Saved capture completed,
return/recovery and offline allocation checks passed. This establishes the reset
prerequisite and firmware survival only; MAC/BBP tables and RF remain pending.


### Bounded register-pair encoding

`mt76_mcu_register_packet` builds one random-read or random-write command from
1–24 WLAN register pairs. It validates every aligned offset (0..0x3ffc), the
nonzero sequence and output capacity before changing the output. Read values
must be zero. Each pair carries base0x410000 plus the offset and its little-endian
value; the packet retains the existing MCU header and zero trailer. The native
identity read now uses this builder and matches its previous16-byte wire fixture.

This is an encoder, not a native write authorization or table executor. No MAC,
BBP, RF or power table is sent. Linuxv6.18 initialization tables include PA and
transmit-power settings, so later execution needs an explicit register/value and
phase policy. The first native write trial must establish a command-specific
acknowledgement with the MAC disabled before any table is allowed. Multi-packet
sequencing, response validation and table provenance remain pending.

Host tests cover all supported pair counts, exact wire bytes, buffer canaries,
maximum offsets, oversized counts, invalid sequences/addresses, short capacity
and unchanged output on rejection. Build, MCU/radio/xHCI tests and MCU ASan/UBSan
checks passed; these checks do not establish native write acceptance.

### One-shot disabled-MAC write-response capture

Firmware profilev5 (`--wifi-write-probe`, requiring `--wifi-radio-reset` and its
prerequisites) adds one diagnostic after strict identity validation. It reads
MAC_SYS_CTRL1004 through EP0 and requires0, then sends a fixed sequence2
CMD_RANDOM_WRITE pair1004=0. The helper accepts no register/value arguments and
requires prepared reset, validated identity, active ownership and no pending or
previous write. OUT must complete exactly before one bounded IN request. There
are no retries or DMA reclamation on uncertainty.

The log records at most16 response words, transfer status, decoded sequence/event
and an EP0 MAC readback that must remain0. `observed=1` means a bounded response
with sequence2/event0 was collected; `ack_validated=1` now requires the exact16-byte native fixed-write fixture,
including the header, address, zero value and repeated header trailer. A completed capture accepts neither general MCU
writes nor tables. The strict validator applies only to this sequence2 disabled-MAC write; it
does not validate arbitrary registers or multi-pair responses. The v1–v4 profiles retain their previous commands. No RF,
channel, PA, transmit-power or packet receive setup is added.

Native v5 capture on the Pro11,2 Edimax adapter (source63d7714,
released8dca7885de3c61f,session00065e74d8311f56-00) returned16bytes for the fixed
1004=0 write: words4a020008,00411004,00000000,4a020008. The reply echoes the
register/value with sequence2/event0/length8 and a repeated header trailer.
EP0 before/after readbacks were0. Capture completed and post-return recovery and
filesystem checks passed. This supplies a fixture for the next strict validator;
the installed diagnostic still reports ack_validated=0. It does not establish
multi-pair or arbitrary-register responses, table execution or RF readiness.


The current source now requires `mt76_mcu_validate_disabled_write_reply` before
completing v5. Its fixture is the native response above. Host tests reject all128
single-bit mutations, every other four-bit sequence (including consistent
header/trailer mutations), other registers, truncated/extended lengths and null
buffers. The installed63d7714 diagnostic remains the historical capture version;
source validation is not yet a new physical acceptance result.

### Two-pair response capture

Firmwarev6 adds `--wifi-pair-probe` after the v5 prerequisites. It first requires
the strict single-write ACK and a disabled-MAC readback, then sends sequence3
with two identical MAC_SYS_CTRL1004=0 pairs. This introduces no new register or
value. The helper requires live ownership, prepared reset, validated identity,
attempted and validated single write, and no pending/previous pair attempt.
OUT24 must complete exactly before one bounded IN; failures quarantine without
retry or DMA reclamation. The diagnostic reads MAC_SYS_CTRL again and requires0.

At most16 response words are logged. `observed=1` only means a bounded sequence3,
event0 response was captured; `ack_validated=0` preserves the distinction between
captured evidence and an accepted multi-pair completion contract. No native
response length/body is assumed. This does not execute initialization tables or
enable RF, receive, transmit, channel or power settings. Earlier profiles remain
unchanged apart from v5's now-strict single-write ACK validation.

Native v6 observation (sourcefe5c0a8,release101abba9feeb8cfa,
session00065e751cdb49ed-00): strict single-write validation passed. Two identical
1004=0 pairs produced a16-byte response, not a24-byte full echo: words4a030008,
00411004,00000000,4a030008. Sequence3/event0/reported length8; MAC readbacks0.
Capture and recovery/filesystem checks passed. This does not identify whether
the body reflects the first or last pair, nor independently confirm two distinct
writes. General table completion needs a distinguishing response/readback test;
do not infer a full-table echo validator from this fixture.

### Distinct timing-pair diagnostic

The initial firmwarev7 capture (`--wifi-timing-probe`, requiring v6's flags) replaces the duplicate
pair with two fixed timing-register initial values: XIFS_TIME_CFG1100=33a41010
and BKOFF_SLOT_CFG1104=209, as specified in Linuxv6.18 mt76x02_regs.h and
mt76x0/initvals_init.h. No table source is imported. It runs only after prepared
reset, strict identity/single-write ACK and MAC_SYS_CTRL=0. Sequence3 carries
both pairs. It does not set MAC TX/RX enable, RF, PA or transmit-power registers.

A separate read-only EP0 binding permits only1100/1104 during this diagnostic.
It captures each value before the command and reads both afterward, requiring
the fixed target values. All-ones reads, transport errors and lost ownership
fail; no retries or reclamation follow uncertain DMA. Before/after MAC reads
must remain0. `changed0/1` records whether each baseline differed: equality at
baseline cannot prove that particular write executed. Reply words remain raw
capture (`ack_validated=0`) until the distinct native body is inspected; this
trial must distinguish first/last entry before a general validator is proposed.
No ordinary table executor or radio startup is enabled by this profile.

Native v7 source607ad4f/release201acb491eb09532/session00065e753e794b20-00
confirmed the first entry is echoed for this two-entry transaction: response
words4a030008,00411100,33a41010,4a030008 (IN16 after OUT24). Independent EP0
readbacks changed XIFS1100 from33a4100a to33a41010 and BKOFF1104 from214 to209.
Both writes therefore have changed-value evidence; the response alone does not
prove the second write. MAC remained0, strict prerequisites and capture passed,
and post-return recovery/filesystem checks were clean. Use this exact fixture
for strict validation; larger batches and other register classes remain untested.

### Bounded timing initialization

`mt76_mcu_apply_timing` promotes only the accepted two-entry timing transaction
into an initialization step. It accepts no caller-supplied table, addresses,
values or sequence. After checking MAC_SYS_CTRL=0 and capturing both baselines,
it issues the exact sequence3 packet once. It requires the native16-byte
first-entry reply, then independently reads both target values and MAC_SYS_CTRL.
Only matching readbacks and a disabled MAC allow `complete=true`. Authorization
is checked before every operation and again before completion; callbacks must
pin identity, firmware, reset and strict earlier replies, bound waits and retain
uncertain DMA. A failed attempt cannot be retried by reusing the session.

The v7 xHCI binding restricts the exchange to the exact24-byte timing packet and
uses the existing bounded transport. `wifi-timing-init` records status, attempts,
ACK validation, completion, read counts and before/after values. Failure stops
and quarantines the device. The earlier v6 duplicate-pair capture remains a
capture-only profile. This is not a general MAC/BBP table executor or radio
readiness: larger batches and additional register classes require their own
completion and readback policy.

Native bounded-engine acceptance: source48879c2/release0c12150fc75d830c/
session00065e7570e64246-00. status0,ack_validated1,complete1,reads6; OUT24/IN16.
Both timing registers changed to the exact targets and MAC stayed0. Firmware,
reset, strict identity/single-write prerequisites and capture completion passed;
post-return recovery/filesystem checks were clean. This accepts this exact
two-register engine on the captured adapter, not larger tables or RF readiness.

### Fixed MAC basics engine (accepted on captured Pro adapter)

`mt76_mcu_apply_mac_basics` defines exactly two two-pair commands after timing
initialization: sequence4 sets legacy basic rate1408=13f and HT basic rate140c=4003;
sequence5 sets RX filter1400=17f97 and maximum length1018=a1fff. Numeric settings
come from Linuxv6.18 register/initialization definitions; the HT rate uses the
model's final setting. No caller-supplied addresses, values, batches or sequences
are accepted, and no RF/power/key or MAC TX/RX enable writes are included.

Every batch checks MAC0, records both baselines, sends once, validates an exact
16-byte first-entry echo with its distinct sequence, and reads back both targets
and MAC0. Acknowledgement alone never accepts the second register. Failure stops
before the next batch; the session cannot be reused. Authorization is checked
before each I/O and before completing each batch. The transport must enforce
pinned identity/firmware, accepted timing and exclusive unused sequences4/5.
The first-entry reply shape and all four register readbacks were validated on
the captured Pro11,2 adapter in release190ab5353ae2f8b6,
session00065e75fad3378e-00 (source facb862). Both commands sent24/received16;
status0, submitted2, completed2, ack_mask3, reads12, MAC before/after0.
The opt-in fw-v8
diagnostic (`--wifi-mac-basics`, requiring `--wifi-timing-probe`) binds this
engine after accepted timing initialization. Its xHCI transport enforces the
five read offsets, exact packets, sequential unused submissions and completion
of each prior batch before submitting the next. Failed transfers quarantine
the session and retain DMA storage. It logs both replies and all four readbacks
to the workspace before returning to Ubuntu. RF and connectivity remain untested.

`make test-mt76-mac` is included in test-core. Independent wire fixtures, reply
bit mutations, all14 operation failures, all16 authorization boundaries, stale
batch replies, all four readback failures and enabled-MAC boundaries are tested.

The same plan passed again after integration with main22fd266: source285aa45,
releasecb8cc153be25dac9, session00065e765bdc87af-00. Both replies, all four
readbacks, disabled-MAC boundaries and saved completion markers matched the
prior run. This accepts the merged runtime for this bounded diagnostic only.

### Fixed frame-format setup (accepted on captured Pro adapter)

Firmwarev9 (`--wifi-mac-format`, requiring `--wifi-mac-basics`) adds two fixed
commands after completed basics. Sequence6 disables TSO0250 and header
translation0260 with zero writes; sequence7 sets HT control1410=1ff and PN
padding150c=3. Numeric settings follow Linuxv6.18 model initialization.
The shared private two-batch engine exposes only separate fixed-plan entry
points; callers cannot supply tables, addresses, values or sequences.

Each batch requires MAC0, baseline reads, one exact command, an exact16-byte
first-entry reply, both target readbacks and MAC0 afterward. Native sequence6/7
replies and all four readbacks passed in release54d6def400fd2a6c, source7713556,
session00065e76901356a1-00. Both sent24/received16; status0, submitted2,
completed2, ack_mask3, reads12 and MAC0. Registers0250/0260 remained0,
1410 changed100->1ff and150c changed0->3. Native admission
requires accepted timing and completed basics, including both prior ACKs.
Only1004/0250/0260/1410/150c reads are admitted in this phase. Ordered submissions
burn each sequence before transfer; failures retain/quarantine DMA and prevent
retry. No RF/power/key or TX/RX enable writes are added. Logs record each reply,
all readbacks and completion before automatic Ubuntu return.

`make test-mt76-format` covers independent wire fixtures, reply bit mutations,
all14 operation failures,16 authorization boundaries, stale replies, four
readback mismatches, enabled-MAC boundaries and refused retry. Existing basics
tests cover the shared engine's original plan. Native transport tests additionally
cover missing basics prerequisites, packet mutation, scoped reads, ordering,
stale completions and failure quarantine.

### Read-only baseband readiness (accepted on captured Pro adapter)

Firmwarev10 (`--wifi-bbp-ready`, requiring `--wifi-mac-format`) checks the
baseband prerequisite after all accepted MCU setup phases. It requires MAC
control1004=0 and idle status1200 bits0/1 clear, records PBF control0400, and
reads BBP CORE0 at2000 at most20 times while zero. Any all-ones read is refused.
A responding nonzero core is followed by repeated disabled/idle MAC checks and
a final ownership check. PBF is recorded only, not accepted as a complete clock
configuration. This does not initialize baseband tables or RF.

The readiness register/condition follows the
[Linuxv6.18 MT7610 PHY driver](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c).
The independent engine has no write callback, caller-defined read list or MCU
commands. USB reads use the existing bounded exact-length EP0 path. Transport
errors quarantine retained DMA. Failed sessions cannot be reused. Successful
first-read readiness takes6 reads;20 zero core reads terminate with timeout
after23 total reads. Native readiness passed in release6df00376834c37fd,
source6e0bd27, session00065e76bf8562b4-00: coref000f200 on the first core read,
6 total reads, MAC control/status0 before and after. PBF was00080000; this is
an observation, not acceptance of clock programming or baseband tables.

`make test-mt76-bbp` exercises exact read ordering, each I/O failure and all-ones
result, every authorization boundary, all20 successful polling positions,
timeout/cancellation and disabled/idle MAC boundaries. Native tests cover the
four-offset allowlist, completed frame-format prerequisite, stale events and
transport failure quarantine.

### Fixed BBP CORE setup (accepted on captured Pro adapter)

Firmware v11 (`--wifi-bbp-core`, requiring `--wifi-bbp-ready`) adds two
fixed commands, sequences 8/9, for CORE1/4/24/32 at 2004/2010/2060/2080:
values 2/0/0/4003000a. These numeric settings follow the Linux v6.18
MT7610 initialization table. Each batch checks disabled MAC and the captured
CORE0 identity f000f200 before and after, validates the exact first-entry
reply and reads back both targets. No RF, channel, power or MAC-enable
operation is included. Failure quarantines the attempt without retry.

`make test-mt76-bbp-core` covers exact packets, every reply bit mutation,
all 18 I/O failures, 20 authorization boundaries, stale replies, target
mismatches, MAC/core guards and refused retry. Native transport fixtures
cover prerequisites, exact packets and read scope, ordering, stale events
and quarantine. Full build, these tests, earlier MCU/MAC/readiness regressions,
ASan/UBSan and all 67 boot-profile tests pass. Native release5f7b66b688f803cb,
source23b97f2, session00065e76e3fff75c-00 passed both commands: sent24/received16,
exact sequence8/9 replies, status0, submitted2/completed2, ack_mask3 and16 reads.
CORE1 changed0->2, CORE4/24 remained0, CORE32 changed40030001->4003000a.
CORE0 remainedf000f200 and MAC remained disabled. Both capture completion
markers were saved. This accepts only these four settings; remaining baseband
configuration, RF, scanning and connectivity are still pending.

### Remaining CORE and initial IBI/AGC setup (accepted on captured Pro adapter)

Firmware v12 (`--wifi-bbp-front`, requiring `--wifi-bbp-core`) adds fixed
sequences10/11 after completed CORE setup: CORE42/44 at20a8/20b0 are zero,
IBI11 at212c is0fde8081 and AGC0 at2300 is00021400. Numeric settings follow
the Linux v6.18 MT7610 initial register table. This is initial baseband
configuration, not a selected/calibrated channel or radio enable.

The private two-command engine requires disabled MAC and captured CORE0
f000f200 before/after each batch, exact first-entry replies and all target
readbacks. Native admission requires completed earlier CORE setup with both
ACKs/batches, in addition to the prior phases. It permits only these four
target reads plus MAC/CORE0; arbitrary packets and retries are refused. Failed
transfers retain DMA and quarantine the session. Logs save both replies and
readbacks before automatic return. RF, power and connectivity remain pending.

`make test-mt76-bbp-front` covers independent wire fixtures, every reply bit
mutation, all18 I/O failures and20 authorization boundaries, stale replies,
readback/identity/MAC mismatches and retry refusal. xHCI tests add prerequisite
refusals, packet mutation, ordered submissions, read scope and DMA quarantine.

Native source daffe4c / release1874caf6830abdc0, session00065e772300653c-00
passed both sequence10/11 commands and exact replies: sent24/received16,
status0, submitted2/completed2, ack_mask3 and16 reads. CORE42/44 stayed0,
IBI11 changed0->0fde8081 and AGC0 changed7400->21400. CORE0 stayedf000f200
and MAC stayed disabled. Prior setup regressions and both capture completion
markers passed. This accepts only the specified settings; remaining AGC/baseband
configuration, RF, scanning, association and connectivity are still pending.

### Initial AGC settings (accepted on captured Pro adapter)

Firmware v13 (`--wifi-bbp-agc`, requiring `--wifi-bbp-front`) adds two fixed
commands, sequences12/13, for AGC1/2/15/22 at2304/2308/233c/2358. Values are
3/003a6464/88a28cb8/1e21 from the Linux v6.18 MT7610 initial register table.
Admission requires completed earlier CORE/IBI/AGC0 phases and both ACKs. Each
batch verifies disabled MAC and captured CORE0 identity before and after, an
exact first-entry reply and both target readbacks. No RF or MAC-enable write
is included; channel-specific overrides and calibration remain pending.

The native read allowlist contains only those four targets plus MAC/CORE0.
Malformed packets, out-of-order batches and retries are refused; transfer
failure quarantines retained DMA. `make test-mt76-bbp-agc` covers fixed wire
fixtures, reply mutations, all18 I/O failures, all20 ownership boundaries,
stale replies, target mismatches and MAC/CORE guards. xHCI fixtures exercise
all prerequisite refusals, bounded transfers, exact reads and quarantine.
Command sequence identifiers are not wrapped; expanding initialization beyond
sequence15 requires a separately validated batching/session design.

Native source56bf5da / releaseb245ac895a7d9396, session00065e777b83c006-00
passed exact sequence12/13 replies, sent24/received16 for both commands,
status0, submitted2/completed2, ack_mask3 and16 reads. AGC1 changed1->3 and
AGC2 changed6464->3a6464; AGC15/22 already matched88a28cb8/1e21 and stayed
unchanged. CORE0 stayedf000f200 and MAC stayed disabled. Prior setup checks
and both saved completion markers passed. Remaining AGC/baseband configuration,
RF, scanning, association and connectivity are not accepted by this result.

### Four-pair command comparison (accepted on captured Pro adapter)

Firmware v14 (`--wifi-bbp-batch`, requiring `--wifi-bbp-agc`) replaces the
two-command AGC phase with one40-byte sequence14 command after completed
CORE/IBI/AGC0 setup. It writes the same accepted AGC values in order15/22/1/2.
Putting AGC1/2 last allows their expected changes1->3 and6464->3a6464 to prove
that the adapter processed the packet tail. Sequence12/13 is skipped in this
profile; older profiles retain their original commands and bytes. No new
register values, RF enable or sequence recycling is introduced.

The private engine now supports fixed two- or four-pair batches. It retains
exact first-entry reply validation, every target readback, disabled MAC and
CORE identity guards, cancellation boundaries and one-attempt behavior. Public
entry points still expose only fixed plans. Four-pair success requires one
completed batch, ack_mask1 and12 reads. The strict expected reply is
[4a0e0008,0041233c,88a28cb8,4a0e0008]; the native capture below confirms this fixed command contract. Unexpected replies fail the trial.

`make test-mt76-bbp-batch` covers the independent40-byte fixture, reply bit
mutations, all13 I/O failures and14 authorization boundaries, stale/short/long
replies, all readbacks and identity/MAC guards. Existing fixed-plan tests
regress the shared engine. Native fixtures cover prerequisites, every packet
bit mutation, exact40-byte admission, one submission, read scope and quarantine.
A passing trial will support consolidation of setup commands; larger tables
and sequence-space exhaustion still require explicit design, not counter reset.

Native source d1c0811 / releaseb3eaadb9ad679dbb, session00065e7a17440518-00
passed: one40-byte sequence14 command, exact16-byte reply, status0, submitted1,
completed1, ack_mask1 and12 reads. AGC15/22 remained88a28cb8/1e21; the final
two pairs changed AGC1 from1->3 and AGC2 from6464->3a6464, proving application
of the packet tail. The old sequence12/13 phase was absent. CORE0 remained
f000f200 and MAC0, preceding setup checks and saved completion markers passed.
This accepts the fixed four-pair command on this adapter, not arbitrary batch
sizes, command types, sequence reuse or RF/connectivity. Consolidating setup
with this width is the next step toward freeing command identifiers.

### Consolidated initialization (accepted on captured Pro adapter)

Firmware v15 (`--wifi-compact`, requiring the batching recipe) replaces the
older per-phase paths with five fixed four-pair commands at sequences4..8.
Firmware upload, reset, strict identity/write replies and sequence3 timing
remain unchanged. Order is MAC basics, frame format, read-only BBP readiness,
CORE, remaining CORE/IBI/AGC0, then AGC15/22/1/2. All register settings remain
those accepted above. The readiness check stays between format and CORE;
phases2..4 require captured CORE0=f000f200 before/after each command.

The transport pins one phase and requires every previous phase's complete
status, ACK and single submission. Exact40-byte packets and phase-specific
read allowlists are enforced. A phase advances only after all readbacks and
identity/MAC guards pass. Any failure quarantines the session; command IDs
are never recycled. The five commands require five exact16-byte first-entry
replies and20 target readbacks, with10/10/12/12/12 total reads per phase.
Successful consolidation ends at sequence8, leaving9..15 unused. Older
profiles retain their original sequences, logs and behavior. RF remains off.

`make test-mt76-compact` checks independent packet/reply fixtures for all five
plans, exhaustive register allowlists, reply-bit mutations, every I/O failure
and ownership boundary, readback/MAC/CORE faults and refused retry. Native
transport fixtures cover every phase's prerequisites, prior completion, BBP
readiness, packet mutation, read scopes, short buffers, stale events and
failure quarantine. Native acceptance is recorded below.

Native source1b9bf8e / releasee459728ba57fea08, session00065e7a55ca88f1-00
passed all five40-byte commands and exact16-byte replies at sequences4..8.
Each phase completed once with ack_mask1; reads were10/10/12/12/12. All20
baselines/readbacks matched the prior native values. Read-only BBP readiness
completed between phases1/2, CORE0 stayedf000f200 during phases2..4, and MAC
remained0. Old per-phase log families were absent. Both completion markers
were saved. This accepts consolidation of these settings on the captured
adapter, leaving command identifiers9..15 unused; it does not accept remaining
baseband settings, sequence recycling, RF, scanning or connectivity.

### AGC23/24/25/26 extension (accepted on captured Pro adapter)

Firmware v16 (`--wifi-agc-more`, requiring `--wifi-compact`) adds compact
phase5 at sequence9. Fixed addresses235c/2360/2364/2368 receive
272c/2f3a/8000005a/007c2005 from the Linux v6.18 MT7610 initialization table.
The phase requires all five accepted phases to complete, then checks captured
CORE0 and disabled MAC before/after one40-byte command. The strict expected
reply is[4a090008,0041235c,272c,4a090008]; all four values must independently
read back correctly. This is initial AGC configuration, not channel calibration
or RF/receive/transmit enable.

The five-phase v15 profile retains an explicit limit5; only v16 selects limit6.
Unknown limits and attempts beyond the selected limit are refused. Existing
per-phase read allowlists, single submission and failure quarantine apply.
Host tests extend independent packet/reply fixtures, all read allowlists,
every I/O/authorization failure, target/identity guards, packet mutation and
native prerequisite checks to the new phase. The previous profile is tested
for both allowed earlier phases and refusal of phase5.

Native sourcec9a84ec / release8bf9cd874ed0f02d, session00065e7ad65bc92e-00
passed phase5: sequence9 sent40/received16, exact reply, status0, one submission
and completion, ack_mask1 and12 reads. AGC23 changed272f->272c, AGC24 stayed
2f3a, AGC25 changed0008005a->8000005a and AGC26 changed005a2004->007c2005.
CORE0 stayedf000f200 and MAC0. All earlier phases, readiness ordering and both
completion markers passed. This accepts these four initial values only;
remaining AGC/baseband settings, RF and connectivity remain pending.

### AGC33/34/37/41 extension (accepted on captured Pro adapter)

Firmware v17 (`--wifi-agc-next`, requiring `--wifi-agc-more`) adds phase6
at sequence10. Fixed addresses2384/2388/2394/23a4 receive
3238/000a0c0c/2121262c/38383e45. The six preceding phases must complete
before one40-byte command, exact16-byte first-entry reply, four independent
readbacks and MAC/CORE guards. Profiles v15/v16 retain limits5/6; v17
selects7. No command identifier is reused and RF remains disabled.

Host checks cover all seven packet/reply fixtures, allowlists, I/O and
authorization failures, readback guards and xHCI ownership/quarantine.
The host fixture correctly distinguishes baseline reads from post-command
readbacks; sanitizer checks pass. All73 boot-profile tests pass.

Native retry sourceb3e4ee1 / releaseff59ebda5a2483ae,
session00065e8394ee6855-00 passed phase6 at sequence10: sent40/received16,
exact reply, status0, one submission/completion, ack_mask1 and12 reads.
AGC33 changed003c->3238, AGC34 stayed000a0c0c, AGC37 stayed2121262c,
and AGC41 changed2727303a->38383e45. All preceding phases and both
completion markers passed; MAC remained0 and CORE0 remainedf000f200.
The first attempt saved no session and remains incomplete; the retry accepts
these initial settings only. Remaining baseband settings, RF, scanning and
connectivity remain pending. Identifiers11..15 remain unused.

### AGC57/59/63 and TXC0 extension (accepted on captured Pro adapter)

Firmware v18 (`--wifi-agc-tail`, requiring `--wifi-agc-next`) adds phase7
at sequence11: fixed offsets23e4/23ec/23fc/2400 receive
1010/baa20e96/1/00280403. One40-byte command requires seven prior phases,
strict16-byte reply, four independent readbacks and MAC/CORE guards.
Prior profiles retain their phase limits. RF remains disabled and sequence
identifiers are never recycled. Host fixtures cover all eight fixed packets,
read allowlists, failure/cancellation paths and xHCI ownership/quarantine;
74 profile tests and sanitizer checks pass. Native evidence is pending.

Native sourcede17cb6 / releasec34215d727a60f08,
session00065e83ccc7f06a-00 passed phase7 at sequence11: sent40/received16,
exact reply, status0, one submission/completion, ack_mask1 and12 reads.
AGC57 stayed1010, AGC59 changedbaa20e23->baa20e96, AGC63 changed0->1,
and TXC0 stayed00280403. All earlier phases,32 register readbacks and
both completion markers passed; MAC0 and CORE0f000f200 preserved.
This accepts these initial settings only, leaving identifiers12..15 unused.
Remaining TXC/RXC/baseband initialization, RF and connectivity are pending.

### TXC1 and RXC1/2/3 extension (accepted on captured Pro adapter)

Firmware v19 (`--wifi-rxc-start`, requiring `--wifi-agc-tail`) adds phase8
at sequence12: fixed offsets2404/2504/2508/250c receive0/12/11/5.
The40-byte command requires all eight prior phases, exact16-byte reply,
four independent readbacks and disabled MAC/captured CORE guards. Prior
profiles retain their limits. No RF enable or command identifier recycling.
Host fixtures cover nine fixed packets, read allowlists, failure/cancellation
and ownership/quarantine paths.75 boot-profile tests pass.

Native sourcedf537ea / release7d6ccc2fe96d530c,
session00065e84abf0e3e4-00 passed phase8 at sequence12: sent40/received16,
exact reply, status0, one submission/completion, ack_mask1 and12 reads.
TXC1 stayed0; RXC1 changed14->12; RXC2 stayed11; RXC3 stayed5. All
earlier phases,36 register readbacks and both completion markers passed.
MAC0 and CORE0f000f200 preserved. Identifiers13..15 remain unused;
remaining RXC/baseband settings, RF and connectivity remain pending.

### RXC4/5/7 and TXO8 extension (accepted on captured Pro adapter)

Firmware v20 (`--wifi-rxc-tail`, requiring `--wifi-rxc-start`) adds phase9
at sequence13: fixed offsets2510/2514/251c/2620 receive0/f977c4ec/90/0.
One40-byte command requires nine previous phases, exact16-byte reply, four
independent readbacks and MAC/CORE guards. Prior profiles retain limits.
RF stays disabled; no identifier recycling. Host coverage includes all ten
fixed packets, read allowlists, failures/cancellation, ownership/quarantine;
76 boot-profile tests pass. Native acceptance remains pending.

Native sourcefd54365 / release492306b5fccc17b9,
session00065e84c6495273-00 passed phase9 at sequence13: sent40/received16,
exact reply, status0, one submission/completion, ack_mask1 and12 reads.
RXC4 stayed0; RXC5 changed71550400->f977c4ec; RXC7 changed0->90;
TXO8 stayed0. All earlier phases,40 register readbacks and both completion
markers passed; MAC0 and CORE0f000f200 preserved. Identifiers14..15 remain
unused. Remaining baseband initialization, RF and connectivity are pending.

### TXBE0/4/6/8 extension (accepted on captured Pro adapter)

Firmware v21 (`--wifi-txbe-start`, requiring `--wifi-rxc-tail`) adds phase10
at sequence14: fixed offsets2700/2710/2718/2720 receive0/4/0/14.
One40-byte command requires ten prior phases, exact16-byte reply, four
independent readbacks and MAC/CORE guards. Prior profiles retain limits.
No RF enable or identifier recycling. Host fixtures cover eleven fixed
packets, allowlists, failures/cancellation and ownership/quarantine;
77 boot-profile tests pass. Hardware acceptance remains pending.

Native source56e1f4a / release6477738b1d3cf7d5,
session00065e85c7b7d806-00 passed phase10 at sequence14: sent40/received16,
exact reply, status0, one submission/completion, ack_mask1 and12 reads.
TXBE0 stayed0; TXBE4 changed0->4; TXBE6 changed1->0; TXBE8 changed4014->14.
All prior phases,44 register readbacks and both completion markers passed;
MAC0 and CORE0f000f200 preserved. Identifier15 remains unused. Remaining
baseband initialization, RF and connectivity are pending.

### TXBE9/10/12/13 extension (native readback failure)

Firmware v22 (`--wifi-txbe-next`, requiring `--wifi-txbe-start`) adds phase11
at sequence15: fixed offsets2724/2728/2730/2734 receive20000000/0/0/0.
One40-byte command requires eleven previous phases, exact16-byte reply,
four independent readbacks and MAC/CORE guards. This reaches the last
identifier: a compile-time plan bound and runtime phase12 refusal prohibit
wrapping to sequence0. Session codec exhaustion tests also refuse a16th
command. Prior profiles retain limits. No RF enable or identifier recycling.
Host fixtures cover twelve packets, allowlists, failures/cancellation and
ownership/quarantine;78 boot-profile tests pass. Hardware result pending.
Remaining initialization will require independently validated larger batches
or a proven-drained transport session, not simply clearing sequence state.

Native source5d5488c / release9cc1888913aa7ff5,
session00065e85ec8dd3b8-00 failed phase11 with status4 after8 reads.
Sequence15 command/reply and first TXBE9 readback matched, but TXBE10
changed37dff->38e30 instead of reading0. Earlier eleven phases passed.
The engine stopped before TXBE12/13 and final MAC/CORE reads; zero-filled
log fields for those unread values are not evidence. Both completion markers
are NO. This phase remains unaccepted; next investigate TXBE10 with bounded
read-only samples. Do not infer counter semantics or relax readback from
this single observation. RF/connectivity remain untested.

### Read-only TXBE observation (captured Pro adapter)

Firmware v23 (`--wifi-txbe-observe`, requiring `--wifi-txbe-start` and
excluding `--wifi-txbe-next`) executes accepted compact phases0..10, then
collects eight fixed samples of TXBE9/10/12/13 at2724/2728/2730/2734.
Each sample reads MAC1004 and CORE2000 before and after; all64 reads must
succeed with MAC0, COREf000f200 and no all-ones response. The diagnostic
has no write/exchange callback, does not submit phase11, and cannot retry.
Native ownership requires all eleven prior completions and no phase11 attempt.

`wifi-txbe-observe` reports status/read/sample counts; `wifi-txbe-sample`
records only fully guarded samples, with monotonic time at the TXBE10 read.
Changing values are recorded without claiming counter/status semantics or
accepting a write. Strict v22 verification remains unchanged. RF/connectivity
remain untested. Results save to the normal workspace log and return to Ubuntu.

Native sourceffa4e17 / release70e0236472c320bc,
session00065e86348e7c75-00 completed all eleven initialization phases and
64 observation reads. TXBE10 remained00037fff in all eight samples at
2823351..2825238 monotonic microseconds (1.887ms); TXBE9/12/13 stayed0.
All sample boundaries retained MAC0/COREf000f200. This short window shows
no spontaneous change and does not establish register semantics or explain
the previous write mismatch. Phase11 remains unaccepted. Next audit the
register mapping and write/read transport before selecting another write trial.
Both completion markers passed; logs saved, Ubuntu recovered, and offline
workspace allocation was clean. RF/connectivity remain untested.

### TXBE write/read audit (2026-10-23)

The local pinned Linux v6.18 references and independent packet reconstruction
agree with the current code. [Register definitions](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_regs.h)
use TXBE base2700 plus four times the register index. Thus TXBE9/10/12/13
are2724/2728/2730/2734; [initial settings](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/initvals_init.h)
are20000000/0/0/0. The MCU WLAN base410000 yields412724/412728/412730/412734.
An independent reconstruction from those local references matched every byte
of the actual phase11 codec output, including sequence15, length and padding.
Compact and MCU regression tests passed. No address/endianness error was found.

The [USB register path](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/usb.c)
uses vendor-IN request7, address high16 in value and low16 in index, four bytes
little-endian. Our direct read of2728 uses value0/index2728 and checks actual4.
The [MCU path](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_usb_mcu.c)
sends little-endian address/value pairs with command12. Linux waits for the
matching sequence/event; its initialization path does not perform our strict
per-register equality readback. That difference does not prove our observed
mismatch harmless, and does not justify removing validation.

The failed phase11 changed TXBE9 as well as attempting TXBE10/12/13. Its first
reply/first-register readback passed, but it cannot isolate which write caused
TXBE10 to differ. The read-only control sampled before any of those four writes
and was stable for only1.887ms. These captures neither establish a free-running
counter nor prove that TXBE10 ignored a write. Register semantics remain unknown.

Selected next comparison: after accepted phases0..10, record guarded baseline
samples, issue only TXBE9=20000000 once at unused sequence15, validate its exact
reply and readback, then record the same guarded samples. Leave TXBE10/12/13
unwritten. This separates the first write's effect from the failed batch without
recycling a sequence or weakening v22 checks. A TXBE10 change would identify an
effect associated with TXBE9 in this setup, not automatically establish counter
semantics; stability would leave the TXBE10 write itself for a later isolated
trial. The v24 implementation below now supplies this comparison; native evidence is pending.

### Isolated TXBE9 comparison (captured Pro adapter)

Firmware v24 (`--wifi-txbe-first`, requiring `--wifi-txbe-start`, excluding
`--wifi-txbe-next` and `--wifi-txbe-observe`) implements the comparison above.
It completes phases0..10, records eight guarded baseline samples, sends one
16-byte sequence15 command writing only2724=20000000, then records eight
more guarded samples. The reply must match4a0f0008/00412724/20000000/4a0f0008.
All eight subsequent TXBE9 values must equal20000000. All128 reads must pass;
MAC0/COREf000f200 remain required at each boundary. TXBE10/12/13 are observed
without writing them or imposing an unproven expected value. No phase11 batch
or sequence reuse, and no retry after an attempted operation.

`wifi-txbe-first-command` and `wifi-txbe-first-word` record the exchange;
`wifi-txbe-first` records completion and per-stage read/sample counts.
`wifi-txbe-first-sample` stage0/1 means before/after the write, with timestamp
at each TXBE10 read. Only fully guarded samples are emitted, even on failure.
The capture automatically saves results and returns to Ubuntu. A completed
comparison does not accept the original four-register batch or RF/connectivity.

Native sourcef1785c6 / releaseb3a29375623082db,
session00065e87131ed81c-00 completed the comparison. Sequence15 sent16/received16
with the exact reply; status0, one submission, ACK1, complete1,64 reads/eight
samples on each side. Before the write, TXBE9 stayed0 and TXBE10 stayed37fff
at2836302..2839177us. After TXBE9=20000000, all eight TXBE9 readbacks matched
and TXBE10 stayed38e30 at2839544..2843407us. TXBE12/13 stayed0; every boundary
retained MAC0/COREf000f200. All eleven prerequisite phases and both completion
markers passed. Workspace allocation and Ubuntu recovery checks were clean.

The isolated TXBE9 write reproduced the failed quartet's final TXBE10 value
without writing TXBE10. This identifies a TXBE9-associated change in this setup;
it does not establish TXBE10 semantics or prove that the original TXBE10 write
succeeded or failed. Keep the original v22 mismatch unaccepted. Next comparison:
TXBE10-only zero write at unused sequence15 after phases0..10, leaving TXBE9
unchanged, with guarded before/after samples. No new trial is installed or armed.

### Isolated TXBE10 comparison (native readback failed)

Firmware v25 (`--wifi-txbe-second`, requiring `--wifi-txbe-start`, excluding
next/observe/first alternatives) repeats the eleven accepted phases, records
eight guarded samples, then writes only2728=0 once at sequence15. TXBE9 must
be zero in every baseline sample before submission. The exact expected reply
is4a0f0008/00412728/0/4a0f0008. Eight subsequent samples must retain TXBE9=0
and read TXBE10=0. Both sets require MAC0/COREf000f200 boundaries,128 total
reads, and single-attempt ownership. No TXBE9/12/13 writes or sequence reuse.

This uses the same bounded `wifi-txbe-first-*` receipt family as v24, with an
explicit `wifi-txbe-target: register=10` marker and distinct boot flag/profile.
All fully guarded samples are logged even if final readback verification fails;
a nonzero TXBE10 yields failed completion, never silently accepted status.
Both alternatives retain their own exact packet/reply/readback tests. Results
save and return automatically; RF/connectivity and the v22 quartet stay unaccepted.

Native source `c96e810`, release `f8b4b41d736bd4e2`, session
`00065e87382297d7-00` completed collection but failed the strict readback check.
All eleven prerequisite phases and 44 readbacks passed. The isolated sequence15
command sent16/received16 with the exact expected reply and ACK validated.
The result was status4, attempted1, write_attempted1, submitted1, complete0;
both before/after stages completed64 reads and eight fully guarded samples.
TXBE10 stayed `0x37fff` in all sixteen samples, before at2867128..2873127us
and after at2873493..2875377us. TXBE9/12/13 stayed zero; MAC0 and
COREf000f200 held at every boundary. Both capture completion markers were NO.

Together with v24, this shows that the TXBE9-only write changed the observed
TXBE10 value, whereas a TXBE10-only zero write produced no observed change in
this window. A matching MCU reply does not establish a zero register readback.
This does not yet distinguish register semantics from a write/access-path issue.
Next audit that distinction before choosing another hardware trial; retain the
strict failure and do not enable the rest of phase11 or RF on this evidence.
Raw logs and run receipts are archived outside build in
`wifi-txbe-second-c96e810/failed-native-collection`. Automatic Ubuntu recovery,
consumed one-shot, payload hashes and retained desktop recovery were verified.
Offline workspace checks passed:189 files/532 clusters, zero leaks, allocation
mismatches or errors. Workspace unmounted; no reboot armed.

### TXBE10 access-path audit after v25

The v25 failure was checked against the pinned Linux v6.18
[MCU read/write implementation](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x02_usb_mcu.c)
and [USB register access](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/usb.c).
Command12 sends little-endian WLAN-base/address and value pairs; command10
provides an independent firmware register-read path. USB request7 reads the
local register offset directly. No address or byte-order discrepancy was found.
An independent host reconstruction matched the compiled TXBE10 write packet
`0800cf50282741000000000000000000`. Its single-register MCU read counterpart
is `0800af50282741000000000000000000`, using unused sequence15 after phases0..10.
Host packet checks and compact/MCU regressions pass; no hardware access occurred.

An older vendor-source mirror at commit
`092d218723b8d50816b3bfb377675f5658a8ac4a` corroborates the address and zero table
value (`include/phy/rlt_phy.h`, `chips/mt76x0.c`); its `mcu/mcu_and.c` also
encodes random reads as address/zero pairs. This is corroboration, not a register
specification. Neither source establishes that TXBE10 must read back its written
value. The observed TXBE9-associated change does not establish a counter or
other particular register semantics. Do not import a sibling chip's register
manual or waive the failed check on that basis.

Selected next diagnostic: replace the isolated write with one MCU TXBE10 read
at sequence15, bracketed by the existing guarded EP0 observations after the
accepted eleven phases. Require exact response length, sequence/event, echoed
address and framing, then compare the returned value with the before/after
control reads. Log disagreements as evidence, never as initialization acceptance.
No new diagnostic register writes, retries, sequence reuse or RF enablement.
Agreement would support consistent reads through both paths; disagreement would
focus investigation on access-path behavior or timing, not by itself prove which
path is correct. Implementation and native evidence remain pending. The original
quartet and v25 zero-write checks remain failed and unchanged.

### MCU/EP0 read comparison implementation (native pending)

Firmware v26 (`--wifi-txbe-access`, requiring `--wifi-txbe-start` and excluding
next/observe/first/second) runs the accepted eleven phases, eight guarded EP0
samples, one command10 TXBE10 read at sequence15, and eight further samples.
The distinct `wifi-txbe-access-*` records contain the exact reply, MCU value,
match count and every fully guarded sample. Completion requires all16 target
readings to agree and TXBE9 to remain zero. A mismatch records status4 and
failed capture completion while retaining the observations. Malformed framing,
short transfers, changed MAC/CORE guards and cancellation also fail; no retry
or sequence reuse is allowed. This diagnostic adds no register write. An agreed
read would accept only this comparison, not TXBE10 initialization or RF activity.

Native v26 comparison accepted on the captured adapter: source `b4417eb`,
release `eb362bcf6da9fdfb`, session `00065e876ba24dc3-00`. Sequence15 sent16
and received16 with exact framing/address and MCU value `0x37fff`. Status0,
attempted1, request_attempted1, submitted1, reply_validated1, complete1; both
observation stages completed64 reads/eight samples, matches16. Before timestamps
2882205..2885095us and after2885446..2889299us; every TXBE10 sample was37fff,
TXBE9/12/13 stayed0, and all MAC0/COREf000f200 boundaries held. Eleven prerequisite
phases/44 readbacks and both capture completion markers passed.

This supports consistent baseline reads through MCU and EP0 in this setup. It
neither explains the ignored zero-write readback nor accepts the earlier v22/v25
writes. Next investigate write-path behavior and register semantics before a
further hardware trial; no retries, relaxed checks or RF enablement follow from
this result. Evidence is archived outside build in
`wifi-txbe-access-b4417eb/native-collection`; read-path agreement is its full
acceptance scope.

### Isolated USB control write (native pending)

The agreed baseline reads leave MCU write handling versus register behavior
unresolved. Linux v6.18 `usb.c` uses vendor OUT request6 with local register
index and four little-endian data bytes for direct register writes. The new
firmware v27 (`--wifi-txbe-direct`, requiring start and excluding all four
alternative TXBE trials) uses precisely one such write: index2728, value0,
length4, after the eleven accepted phases and eight guarded baseline samples.
It then reads TXBE10 using command10 at unused sequence15 and takes eight guarded
EP0 samples. TXBE9 must remain0 and MAC0/COREf000f200 guards must hold.

The direct write has its own fixed-value callback and one-attempt ownership;
it does not expand the general firmware-write allowlist. The common read receipt
reports expected_matches8 in this mode, comparing only the post-write samples
with the MCU result. Completion additionally requires that value to be zero.
`wifi-txbe-direct-write` records the control completion and
`wifi-txbe-direct` records attempted/submitted/written. Nonzero or disagreeing
readbacks remain failed captures with saved evidence. This test does not retry
the earlier MCU write, accept phase11, reuse a sequence, or enable RF.

Native v27 readback failed: source `e821968`, release `acaea56aaa88d52f`,
session `00065e87a87d976c-00`. The single EP0 index2728 zero transfer completed
with actual4, followed by the exact sequence15 MCU read reply returning37fff.
All eleven prerequisite phases/44 readbacks passed. The comparison reported
status4, attempted1, request_attempted1, submitted1, reply_validated1, complete0;
control attempted/submitted/written1 denotes completed transport, not a zero
register value. Each observation stage completed64 reads/eight samples.
TXBE10 stayed37fff before3106897..3108802us and after3109221..3115210us;
the MCU value matched all eight post-write EP0 samples. TXBE9/12/13 stayed0,
and every MAC0/COREf000f200 boundary held. Both capture completion markers NO.

The same unchanged value after MCU and direct EP0 zero writes makes an MCU-only
write-path defect less likely. This still does not prove that TXBE10 is read-only,
that writes are ignored, or that the current initialization prerequisites are
sufficient. Next audit register semantics and prerequisite ordering before
selecting another trial; retain the strict failed checks and keep RF disabled.
Evidence archived in `wifi-txbe-direct-e821968/failed-native-collection` outside
build. No initialization acceptance follows from transport completion.

### PBF/MAC clock prerequisite discrepancy

The post-v27 ordering audit found a concrete omitted prerequisite. Linux v6.18
[initialization ordering](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/init.c)
runs MAC register setup before baseband setup and identifies SYS_CTRL bits11:10
as PBF/MAC clock enables. Its
[MAC table](https://github.com/torvalds/linux/blob/v6.18/drivers/net/wireless/mediatek/mt76/mt76x0/initvals_init.h)
sets PBF_SYS_CTRL (local0400) to00080c00. Our current compact setup never writes
0400; the readiness probe records PBF but does not require those clock bits.

All five saved v23 through v27 diagnostic sessions report PBF00080000 at readiness,
with MAC_SYS_CTRL0. The difference is exactly0c00, bits11:10; reset bits4:0 remain
zero in both values. The CORE readiness value and successful register readbacks
therefore do not establish that these clocks were enabled. This is a plausible
missing prerequisite, not proof of the cause of TXBE10's nonzero readback.
The partial MAC initialization also omits other reference settings; do not claim
full Linux sequence equivalence or bulk-import power/RF settings as a shortcut.

Next bounded scope: an explicit clock prerequisite diagnostic before compact
phase2/baseband readiness, after the two accepted MAC phases. Require the exact
captured PBF baseline80000 and disabled/idle MAC, write only80c00 once through
EP0, verify exact readback and disabled/idle MAC, then continue existing guarded
phases and the read-only TXBE10 MCU/EP0 comparison. This isolates the prerequisite
without another TXBE10 write or relaxing a failed write check. Reuse no MCU
identifier and retain the current CORE guards; any changed prerequisite fails
and saves evidence. Implementation/native validation remain pending. No RF
transmit/receive enable, other MAC-table expansion or startup change is accepted.
The local `txbe-prerequisite-audit` receipt checks source ordering, bit arithmetic
and five exact native logs; it is not a hardware trial.

### Explicit clock prerequisite implementation and native result

Firmware v28 (`--wifi-clock`, requiring TXBE start and excluding all TXBE
alternatives) adds one guarded clock transition between compact phases1 and2.
The pure engine reads MAC_SYS_CTRL, MAC_STATUS and PBF_SYS_CTRL, requires MAC0,
idle status and exact PBF80000, then writes0400=80c00 once. Exact PBF readback
and disabled/idle MAC checks must pass before baseband readiness is attempted.
The readiness receipt must also retain80c00. No MCU sequence is consumed by
this control transfer. Phases2..10 retain their existing CORE and target guards,
then the v26 read-only MCU/EP0 comparison uses sequence15. TXBE10 is not written.

`wifi-clock` records attempted/write_attempted/submitted/written/complete,
six reads and all before/after values. Failed or uncertain operations retain
single-attempt state and native transport quarantine. The fixed-value callback
is separate from the generic firmware writer and admits only completed MAC
phases0/1 with no baseband attempt yet. Historical profiles do not enable it.
This tests the prerequisite and subsequent observations; a pass would not
retroactively accept the failed TXBE write tests or enable RF connectivity.

The native v28 trial passed on MacBookPro11,2: source `1b3e323`, release
`67f92f9c2a4d9365`, session `00065e87eb5deca5-00`. The one-time clock write
changed PBF from `80000` to `80c00`; all six clock reads passed, with MAC
still disabled and idle. Baseband readiness retained `80c00`, and all eleven
compact phases / 44 target readbacks passed. The sequence15 MCU read matched
all sixteen EP0 samples: TXBE10 remained `37fff`, TXBE9/12/13 remained zero,
and every MAC0/COREf000f200 boundary held. Both capture completion markers
passed and the session saved, unmounted and returned to Ubuntu.

This accepts the explicit clock prerequisite and read-path agreement with it
in place. It does not establish that missing clocks caused the previous write
failures: no TXBE10 write occurred in this run. The next bounded comparison
should repeat the isolated zero write with the accepted clock prerequisite,
retaining exact zero readback as its success condition. RF and connectivity
remain untested. Historical write failures remain failures.

### Clock-enabled isolated write comparison (native write failed)

Firmware v29 (`--wifi-clock-direct`, requiring TXBE start and excluding all
other TXBE alternatives) repeats the v28 clock prerequisite before phases2..10,
then reuses the v27 single EP0 zero write to TXBE10. The subsequent sequence15
MCU read and all eight guarded post-write EP0 samples must agree on zero.
A nonzero value still fails. Access refuses incomplete clock setup, active clock
ownership or missing PBF readback. Historical profiles and failed results retain
their original meaning. No RF enable or additional initialization table is added.

Native v29 source `492fb20`, release `19f40e64c606d997`, session
`00065e8821c93f58-00` reproduced the zero-write readback failure with clocks
enabled. Clock transition80000->80c00, readiness and all eleven compact phases /
44 target readbacks passed. The fixed EP0 index2728 zero transfer completed
actual4; the subsequent exact sequence15 MCU reply returned37fff. All sixteen
EP0 samples also read37fff, with TXBE9/12/13 zero and MAC0/COREf000f200 guards
intact. The comparison reported status4 complete0, matching all eight post-write
samples; both capture completion markers were NO. The failure was saved and
Ubuntu recovery completed, with clean offline workspace checks.

Enabling the PBF/MAC clock bits did not resolve the observed write/readback
mismatch in this sequence. This does not establish register semantics or exclude
other missing initialization prerequisites. Preserve the exact zero-readback
requirement and these failed results; audit remaining setup and reference behavior
before selecting another trial. No RF or connectivity acceptance follows.

### Same-adapter Linux readback and remaining initialization audit

A read-only capture on the same 7392:a711 adapter under Ubuntu kernel
7.0.0-38-generic returned TXBE10=0 in all eight samples. TXBE9 was20000000,
PBF80c00, COREf000f200 and MAC_SYS_CTRL0c; TXBE12/13 were0. MAC_STATUS was
0 except one sample at2. This was a running Linux driver, not the disabled-MAC
initialization state used by SrvrOS, so it does not prove immediate write
readback or explain the register's semantics.

The capture used the driver's debugfs selector and read `regval`, then restored
the original selector. It neither wrote device registers nor reset/unbound the
adapter. Upstream v7.0 [debugfs](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/debugfs.c)
and [mt76x02 binding](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_debugfs.c)
confirm this access pattern; exact Ubuntu downstream source equivalence was not
established. No network credentials or packet contents were captured.

The v6.18 and v7.0 numeric common-MAC, chip-MAC and baseband initialization
tables are identical. Both put TXBE9=20000000 immediately before TXBE10=0.
The [reference initialization](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/init.c)
also has MAC-table settings and EXT_CCA/FCE/WMM updates absent from our compact
sequence. The audit inventories those omissions; they are not evidence that
any particular omitted write causes the mismatch. Do not import power/RF
settings indiscriminately.

The comparison matrix leaves one small combination untested: accepted clocks
plus the reference TXBE9-then-TXBE10 order. v22 wrote both without enabled
PBF/MAC clocks; v29 enabled clocks but deliberately kept TXBE9 zero. v24 showed
that writing TXBE9 alone changed TXBE10's observed value, without establishing
why. Next bounded scope is that combination, with MAC still disabled, original
phase guards and exact target readbacks. Preserve all earlier failures; do not
weaken the zero-readback condition based on a Linux steady-state observation.
The local `txbe-linux-audit` script verifies eight samples, six historical trial
receipts, table identity, addresses and ordering; raw evidence is archived outside
build. No new candidate was installed for this audit.

### Clock-enabled ordered pair diagnostic (native readback failed)

Firmware v30 (`--wifi-clock-pair`, requiring TXBE start and excluding all other
TXBE alternatives) performs accepted compact phases0..10 with the clock
prerequisite, then sends one 24-byte command12 at sequence15: TXBE9=20000000,
TXBE10=0. Phase11 now has exactly two targets in this explicit profile, eight
reads, one acknowledgement and one completed batch. Both target values and
MAC0/COREf000f200 before/after must match. The old quartet profile is unchanged.
TXBE12/13 are not written; no MCU identifier wraps or gets reused. The native
binding refuses a 40-byte quartet, missing clock completion or unrelated target
reads. Failure remains failure and is saved for collection after automatic return.

Native v30 source `0754633`, release `29504434a7bc5aa6`, session
`00065e8878bfdf27-00` failed at TXBE10. Accepted clock transition and readiness
passed, as did phases0..10 and all44 target readbacks. Sequence15 sent24,
received16 with exact first-pair acknowledgement. TXBE9 changed0->20000000;
TXBE10 changed37fff->38e30 instead of zero. Phase11 reported status4,
ack_mask1, batches_completed0, complete0 and reads6. The final MAC/CORE reads
were not reached; their zero-initialized log fields are not observations.
Both capture completion markers were NO. Failed results saved, unmounted and
returned to Ubuntu; offline filesystem/allocation checks passed.

The reference pair plus enabled clocks still reproduces the nonzero readback.
It matches the TXBE10 value observed after TXBE9 in the earlier unclocked
comparison, but does not establish register semantics or the cause. Linux's
fully initialized, TX/RX-enabled zero observation remains a different state.
Next investigate remaining initialization/state differences before another
candidate; retain exact readback checks and all historical failures. TXBE12/13
were not written, and RF/connectivity remain untested.

### Post-v30 observation-boundary audit

Linux's [register-pair transport](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_usb_mcu.c)
partitions the 58-entry baseband initialization table into24/24/10 entries using
the192-byte command-payload limit. Only the final chunk requests a response;
preceding chunks use the driver's no-response sequence0. TXBE10 is table index37
(zero based), in the second chunk, with20 entries still to follow. The
[baseband initializer](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/init.c)
does not read back TXBE10 immediately after its write. Our acknowledged short
commands and immediate checks observe a different point in initialization.
Neither this difference nor an omitted MAC setting is yet proven causal.

Two additional read-only snapshots of66 selected registers on this adapter under
Linux confirm TXBE10zero and show nine baseband entries differ from their initial
table values: CORE1, AGC0/33, TXC0, TXBE0/17 and RXO21/24/28. These are live,
TX/RX-enabled observations after channel/calibration setup; they cannot establish
what those registers read immediately after initialization writes. The capture
restored the debugfs selector and made no device-register writes or resets.

Next bounded implementation scope: a separate full-table observation diagnostic,
after the accepted two MAC phases and clock prerequisite. Reproduce24/24/10
entry boundaries, no-response first chunks and a single final response at unused
sequence6; retain disabled-MAC and CORE guards and collect target readbacks after
the complete table. Do not enable MAC TX/RX, calibrate RF or select a channel.
Keep no-response admission specific to these fixed packets, not generic writes.
Record mismatches explicitly; completion of a transport/snapshot diagnostic must
not be presented as target-value or Wi-Fi acceptance. Historical strict tests stay
unchanged. Full MAC-table equivalence remains outside this comparison.

The `txbe-state-audit` receipt checks source boundaries, all58 entries, two Linux
snapshots and the v30 failure. It includes proposed200/200/88-byte packet fixtures
with sequence0/0/6, archived outside build; these packets have not been sent to
hardware. No new kernel/profile or boot payload was installed during this audit.

### Full-table observation implementation (native pending)

Firmware v31 (`--wifi-bbp-table`) explicitly runs reset/identity/timing, the two
accepted MAC phases, clock setup and BBP readiness before the fixed58-entry
table. Three fixed packets200/200/88 bytes use sequence0/0/6; only the final
packet reads an MCU acknowledgement. The generic register builder still refuses
sequence0. All58 targets are sampled before and after, with four MAC0/COREf000f200
boundaries (124 reads). Transport, ownership, all-ones reads, malformed ACKs and
guard failures stop the capture. Target mismatches are counted and saved without
being classified as accepted initialization. The summary explicitly states
`register_acceptance=NOT_TESTED`, including when every observed target matches.
No compact BBP phases or other TXBE experiment runs alongside this profile.
Historical strict checks remain unchanged; no RF calibration/channel/MAC enable.

The Pro11,2 v31 physical observation (release `4316c9963cb7ae6a`) completed all
three table commands and124 reads with MAC disabled and CORE guards intact.
54/58 target values matched; TXBE10 remained38e30 rather than0, TXBE17 retained
fb3fec3c rather than0, and RXO21/24 remained0 rather than1/6. Full-table batching
alone did not remove the earlier TXBE10 mismatch. This validates capture only,
not register initialization or RF/connectivity; prerequisites remain under audit.

The separate firmware v32 `--wifi-mac-snapshot` profile requires
`--wifi-bbp-table` and its prerequisites. After MAC phases0/1, clocks and BBP
readiness, it reads a fixed63-address inventory (the60 distinct reference MAC
table addresses plus EXT_CCA, FCE_L2_STUFF and WMM_CTRL), bracketed by MAC0 and
COREf000f200 checks:67 reads total. The read-only engine has no exchange callback.
Incomplete inventory blocks the subsequent unchanged full BBP observation.
Values are evidence, not a register-acceptance test; no omitted MAC/power settings
are written. Existing v31 and strict earlier profiles retain their behavior.

Firmware v33 `--wifi-mac-raw` requires the snapshot prerequisites but ends after
the raw read-only inventory; it does not run the full BBP table. It adds an ASIC
identity check to each MAC/CORE boundary and repeats1030, for70 reads total. Each
USB read logs completion, actual length, residue and timeout. Target all-ones
values are counted as unvalidated observations; transport failures and incorrect
ASIC76502000/MAC0/COREf000f200 guards still stop capture. Completion is not register
acceptance. v32 retains its strict all-ones refusal and subsequent BBP workflow.

### Bounded MAC routing trial (v34; Pro11,2 readbacks accepted)

`--wifi-mac-route` requires `--wifi-bbp-table` prerequisites, excludes raw and
strict snapshot modes, and ends before any baseband-table submission. After
compact MAC phases 0/1, the accepted clock sequence and read-only BBP readiness,
it applies the Linux v7.0 `mt76x0_init_mac_registers` FCE/WMM operations in their
reference order: clear FCE_L2_STUFF bit 4 at `080c`, then replace WMM_CTRL's low
10 bits at `0230` with `201`. The fixed pair preserves unrelated bits from this
unit's v33 capture. Native submission requires exact starting values `03ff0233`
and `00041040`; a different initial state refuses the write.

One 24-byte MCU command (sequence 6) requests `03ff0223` and `00041201`. Exact
ACK framing, both readbacks, and initial/final MAC-disabled and CORE guards are
required. There is no retry. This diagnostic adapts the reference EP0 operations
to the existing bounded MCU write engine; it does not claim full reference MAC
initialization equivalence. No new PBF, DMA, power, RF or MAC-enable operation is
introduced. On 2026-10-23, Pro11,2 release `c929ce1b090b820d`, session
`00065e89795928b3-00`, passed the exact ACK, eight reads, both target changes and
MAC0/COREf000f200 boundaries. Both completion markers were saved; no BBP table
ran. This accepts the bounded FCE/WMM operation on this unit. It does not resolve
the earlier baseband mismatches or accept RF/connectivity.

### Bounded MAC policy group (v35; first native readback failed)

`--wifi-mac-policy` requires the v34 route prerequisites and appends one fixed
40-byte sequence7 command for common-table TX_LINK (`1350=1020`), TX_TIMEOUT
(`1348=a2090`), TX_RETRY (`134c=47d01f0f`) and AUTO_RSP (`1404=13`), in reference
relative order. Exact initial values from the v33 inventory are required:
`7f0020`, `10f0a90`, `4bb80407`, `7`. Submission requires completed v34 routing.
Twelve reads cover MAC/CORE boundaries and all four before/after values; the
firmware ACK and each readback must match. No retry, full BBP table, RF enable,
or new PBF/DMA/power setting is included. This is a bounded subset of Linux
v7.0 common_mac_reg_table, not complete reference-order initialization.

On 2026-10-23 Pro11,2 release `ee842dc8bcc56669`, session
`00065e89a75cfd3a-00`, completed v34 routing but failed the first policy readback.
The 40-byte sequence7 command received the exact 16-byte ACK, including the
requested first value `1020`. TX_LINK read `007f1020` after starting at `007f0020`,
rather than the required `00001020`. Status4 stopped at read7 with ack_mask1;
the other three after-values and final MAC/CORE guards were not read. Their zero
summary fields are unpopulated state, not hardware observations. Both capture
markers report NO. This is a failed strict comparison; retained upper bits need
reference/field-semantics investigation before changing the acceptance rule.

The follow-up same-adapter Linux audit read TX_LINK `007f1020` in four samples,
matching the native result. The related [Linux v7.0 rt2800 register definitions](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/ralink/rt2x00/rt2800.h)
identify upper feedback fields, but are not an MT7610 writable-mask guarantee.
This supports investigating a too-strict whole-word comparison. The v35 failure
remains recorded; no check was relaxed. Live timeout and AUTO_RSP values differ
from the initialization table because the reference driver can change ACK timeout
and preamble selection at runtime. Next capture should observe every submitted
policy target and final guards before deciding field-specific validation rules.

### Policy observation capture (v36; complete native observations)

`--wifi-mac-policy-observe` requires v35 policy prerequisites. It submits exactly
the same four-register sequence7 command after accepted routing, with the same
captured initial-value gates and exact ACK validation. A separate observation
engine records all four target readbacks and a mismatch bitmask, then requires
MAC0/COREf000f200 final guards. Ordinary target inequality does not stop this
capture. Transport, authorization, all-ones, malformed ACK and guard failures
still stop it. There is no retry or subsequent BBP table. Completion means that
observations were collected; `register_acceptance=NOT_TESTED` is explicit even
when the mismatch mask is zero. v35 retains its strict first-mismatch failure.

Pro11,2 release `76e71b07371c6185`, session `00065e89f951db06-00`, completed
all12 reads with an exact sequence7 ACK and initial/final MAC0/COREf000f200.
Observed after-values: TX_LINK `007f1020`, TX_TIMEOUT `000a2090`, TX_RETRY
`47d01f0f`, AUTO_RSP `00000013`. Only TX_LINK differs from the requested whole
word (mismatch_mask1); the other three targets match. Both completion markers
were saved, with register_acceptance and RF/connectivity explicitly NOT_TESTED.
The separate v35 strict failure remains unchanged. These observations support
an explicit TX_LINK readback rule; they do not accept unrelated upper-bit values.

### Exact captured policy validation (v37; Pro11,2 native passed)

`--wifi-mac-policy-validate` requires policy prerequisites and excludes observation
mode. Write bytes and initial-value gates are unchanged. After the exact ACK,
all four readbacks and final guards must complete. TX_LINK must equal `007f1020`,
the value observed in native v36 and live Linux on this captured7392:a711 device;
the remaining three must equal `000a2090`, `47d01f0f`, `00000013`. No bit mask
ignores arbitrary values. Failure clears completion and stops capture. The log
reports `exact_profile=1` with completion status and write_mismatch_mask1 because
the first expected readback differs from its write operand. v35 and v36 retain
strict historical and observation semantics respectively. This validates only
the bounded MAC policy group, not RF, BBP setup or wireless connectivity.

On 2026-10-23, Pro11,2 release `678a8d195cc57e83`, session
`00065e8a2764f20b-00`, passed the prepared exact validator. All12 reads, the
sequence7 ACK, all four expected readbacks and final MAC0/COREf000f200 matched;
`complete=1 write_mismatch_mask=1 exact_profile=1` and both capture completion
markers were saved. This accepts this bounded four-register operation on the
captured7392:a711 adapter, not other adapters, remaining initialization or RF/
connectivity. v35's historical failure and v36's observation evidence remain.

### CCK/OFDM/GF protection group (v38; Pro11,2 native passed)

`--wifi-mac-protect` requires v37 exact policy validation. It appends one fixed
sequence8, 40-byte command for CCK1364=07f40003, OFDM1368=07f42004,
GF20_1374=01742004, GF40_1378=03f42084, preserving their Linux v7.0 common-table
relative order. Exact captured initial values00100003/00200003/01002004/02002084
are required before submission. Exact ACK, twelve reads, all four after-values
and initial/final MAC0/COREf000f200 must pass. No retry, PBF/DMA/power change,
RF enable or subsequent BBP table is introduced. This is a bounded table subset,
not a claim of complete reference-order initialization or wireless connectivity.

Pro11,2 release `89ee4a2fc2121ce2`, session `00065e8a54a2695f-00`, passed on
2026-10-23: exact sequence8 ACK, all12 reads, captured starts and all four target
values matched. Initial/final MAC0/COREf000f200 and the earlier exact policy
validation passed; both capture completion markers were saved. This accepts the
bounded CCK/OFDM/GF group on captured7392:a711. Other protection settings, remaining
MAC/BBP setup and RF/connectivity remain outside this acceptance.

### Mixed-mode/TXOP/RTS group (v39; Pro11,2 native passed)

`--wifi-mac-contention` requires accepted v38 protection prerequisites. One fixed
sequence9, 40-byte command writes MM20_136c=01742004, MM40_1370=03f42084,
TXOP1340=0000583f and RTS1344=00ffff20 in Linux v7.0 common-table relative order.
Captured starting values00402004/00802084/0400243f/00ffff07 are mandatory before
submission. Exact ACK, all four target readbacks and MAC0/COREf000f200 boundaries
are required (twelve reads). No retry, RF enable, PBF/DMA/power change or BBP table
follows. Acceptance remains specific to captured7392:a711 and this bounded group.

Pro11,2 release `2cfc6b033353005e`, session `00065e8a97cf1e48-00`, passed on
2026-10-23: exact sequence9 ACK, all12 reads, expected starts and target values,
and initial/final MAC0/COREf000f200. Both capture completion markers were saved,
with all preceding bounded initialization checks passing. This accepts this group
on captured7392:a711; remaining MAC/BBP initialization and RF/connectivity are
outside its scope.

### TX software controls (v40; accepted on captured adapter)

`--wifi-mac-txsw` requires v39 prerequisites. One sequence10, 40-byte packet
writes TX_SW0_1330=601, TX_SW1_1334=40000, TX_SW2_1338=0 and header translation
0260=0. Captured starts0204080c/000c0808/000c0408/0 are mandatory. These are final
mt76x0 table values; this subset does not replay common-table intermediate TX_SW
writes. Four read-only same-adapter Linux samples agreed on601/40000/0; active
Linux beacon offsets differed from the init table and remain deferred. Exact ACK,
four readbacks, twelve reads and final MAC0/COREf000f200 are required; no retry,
RF enable or BBP table follows. Header translation is explicitly kept disabled.

Pro11,2 source `e6e83ab`, release `656e4b9c37ae4baf`, session
`00065e8acd64d0df-00` passed natively on 2026-10-23: exact sequence10 ACK,
all twelve reads and four expected targets, final MAC0/COREf000f200 and both
capture completion markers. Prior bounded groups passed; RF remains untested.

### Beacon-offset initialization (v41; accepted on captured adapter)

`--wifi-mac-beacon` requires v40 prerequisites. One sequence11, 24-byte packet
writes041c=f8f0e8e0 and0420=6f77d0c8 from mandatory captured starts ece8e4e0/
fcf8f4f0. Exact ACK, eight reads and initial/final MAC0/COREf000f200 are required.
No retry, beacon upload/timer enable, BBP table or RF/MAC TX/RX enable follows.

These are the Linux v7.0 common initialization table values, not the eventual
USB beacon layout. `mt76x02u_init_beacon_config` selects five 1600-byte slots;
`mt76x02_set_beacon_offsets` packs offsets in 64-byte units. That produces the
live Linux readbacks4b321900/64 captured previously, explaining the difference
without treating the live values as cold initialization requirements. This trial
only accepts the table writes; operational beacon configuration remains separate.
References: [initialization table](https://raw.githubusercontent.com/torvalds/linux/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/initvals_init.h),
[USB layout](https://raw.githubusercontent.com/torvalds/linux/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_usb_core.c),
[offset packing](https://raw.githubusercontent.com/torvalds/linux/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_beacon.c).

Pro11,2 source `c9e2ab6`, release `015cce9347654d25`, session
`00065e8b0501527d-00` passed natively on 2026-10-23. Exact sequence11 ACK,
both expected offset readbacks, all eight reads and final MAC0/COREf000f200
were verified. Both completion markers and all preceding bounded groups passed.
This accepts only the initialization pair; operational beacon layout and
RF/connectivity remain untested.

### VHT protection/fallback (v42; accepted on captured adapter)

`--wifi-mac-vht` requires v41 prerequisites. One sequence12, 40-byte packet
writes TX_PROT_CFG6/7/8 (13e0/13e4/13e8) to e3f42004/e3f42084/e3f42104
and VHT_HT_FBK_CFG1 (1358) to edcba980. Mandatory captured starts are
e3f02004/e3f02084/e3f02104/edcba988. These are the final four entries in the
Linux v7.0 mt76x0 MAC initialization table referenced above. Exact ACK, four
readbacks, twelve reads and initial/final MAC0/COREf000f200 are required.
There is no retry, BBP table or RF/MAC TX/RX enable after this group.

Four read-only same-adapter Linux samples agree on the first/fourth target;
active Linux13e4/13e8 have an extra10000. Runtime settings are not cold-init
acceptance: this trial requires the exact reference values, without masks or
alternative accepted values. Physical acceptance remains pending.

Pro11,2 source `af42f25`, release `a5200b9a725a2ae5`, session
`00065e8b2132c5a9-00` passed natively on 2026-10-23. All four readbacks matched
exact initialization targets, with sequence12 ACK, twelve reads, initial/final
MAC0/COREf000f200 and both completion markers verified. All preceding bounded
groups passed. The active-Linux differences required no acceptance exception;
RF/connectivity remain untested.

### Packet-buffer configuration (v43; accepted on captured adapter)

`--wifi-mac-buffer` requires v42 prerequisites. One sequence13, 40-byte packet
sets TX_MAX_PCNT0408=1fbf1f1f, RX_MAX_PCNT040c=fe9f, final PBF_CFG0404=77723c1f
and TSO_CTRL0250=0. Mandatory captured starts are1f9f1f1f/fe9f/3fdc1cf5/0.
These are Linux v7.0 table values: final buffer configuration, not replay of its
common-table intermediate configuration. Four read-only live Linux samples agree.
Exact ACK, four readbacks, twelve reads and initial/final MAC0/COREf000f200 are
required. No retry, PBF reset, DMA configuration, BBP table or RF/MAC TX/RX enable
follows. Segmentation offload remains disabled.

FCE_PSE_CTRL0800 is excluded: active Linux reads5 versus the initialization
table's1, requiring a separate audit/trial. This group does not claim to accept
frame-engine routing beyond earlier bounded FCE_L2/WMM evidence, packet traffic,
or the full baseband table. Physical acceptance remains pending.

Pro11,2 source `9d1ba79`, release `246fade6686cd2fa`, session
`00065e8b57f9a360-00` passed natively on 2026-10-23. All four expected readbacks,
sequence13 ACK, twelve reads, initial/final MAC0/COREf000f200 and both completion
markers were verified. All preceding bounded groups passed. FCE_PSE_CTRL0800,
full baseband initialization and RF/connectivity remain outside this acceptance.

### FCE control observation (v44; native capture complete, target unaccepted)

`--wifi-mac-fce-observe` requires v43 prerequisites. One sequence14, 24-byte
pair writes0800=1 and080c=03ff0223, from mandatory starts5/03ff0223. The second
write preserves the accepted L2 configuration. Eight reads, exact ACK and
initial/final MAC0/COREf000f200 are required. This recipe records the first
readback and mismatch bit without accepting any value. L2 must remain exact;
all-ones, I/O, reply and guard failures still fail. No retry, BBP table or RF/MAC
TX/RX enable follows. Completion means observation finished, not FCE acceptance.

The existing firmware preparation already writes0800=1, and both the Linux and
vendor MAC tables write1, but later native/Linux snapshots read5. Neither inspected
register header supplies a bit2 writable-mask definition. This trial separates
fresh post-table-write evidence from a blanket mask assumption. Firmware-load
reference: [Linux v7.0 USB MCU](https://raw.githubusercontent.com/torvalds/linux/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/usb_mcu.c).

Pro11,2 source `8294ee1`, release `6e342f3cdd9f35db`, session
`00065e8b8555a943-00` completed the observation on 2026-10-23. The exact
sequence14 reply echoed write1, while0800 read5 both before and after. L2 routing
remained03ff0223; all eight reads, MAC0/COREf000f200 guards, preceding groups and
both completion markers passed. Mismatch mask1 and register acceptance NOT_TESTED
were recorded. This is evidence for an exact captured-profile validator, not a
proof of bit2 semantics or permission to ignore arbitrary bits. RF/connectivity
remain untested.

### Exact FCE captured-profile validation (v45; native accepted)

`--wifi-mac-fce-validate` requires the v44 prerequisite flags and selects a
separate validator. The same sequence14 pair writes1/03ff0223, with mandatory
starts5/03ff0223. All eight reads, exact ACK and MAC0/COREf000f200 guards remain.
The validator requires full-word readbacks5/03ff0223 and mismatch_mask1; it rejects
all other words, including write-target1. There is no writable-bit mask. Evidence
is the v44 native write1/read5 capture on this exact adapter, not inferred bit
semantics. v44 observation behavior remains unchanged and selectable.

Success emits `wifi-mac-fce-validation` with complete1/exact_profile1; observation
records cannot satisfy collection. No additional writes, retry, BBP table or
RF/MAC TX/RX enable follows.

Pro11,2 source `538fee1`, release `7301441289281aaf`, session
`00065e8bc3faaa90-00` passed on 2026-10-23. Sequence14 returned the exact ACK;
all eight reads completed with full-word readbacks5/03ff0223, mismatch_mask1 and
exact_profile1. Initial/final MAC0 and COREf000f200, all preceding initialization
groups and both completion markers passed. The prepared collector passed unchanged.
Ubuntu recovery and payload hashes verified; offline workspace checks found229files,
592clusters and zero allocation errors. Acceptance covers this captured write1/read5
profile only; it establishes no general writable-bit mask. Remaining MAC/baseband
initialization, scanning and connectivity remain open.


### Integrated baseline regression after v45

The Pro branch now incorporates main `4a991df`, including storage scaling,
workspace log admission/timing and GUI waits. Integration preserves all90 firmware
boot recipes and all91 current main boot recipes exactly; no Wi-Fi register
sequence, firmware or native model gate changes. The unchanged v45 diagnostic passed natively on Pro11,2: source `e1e595c`,
release `9fadb1185a7592ea`, session `00065e8c5588a9b9-00` (2026-10-23).
The prepared collector passed unchanged, including all preceding groups, exact FCE
readbacks5/03ff0223, MAC0/COREf000f200 guards and both completion markers.
Ubuntu recovery/payload hashes and clean offline allocation verified (231files,
595clusters, zero errors). The full collection is archived outside build.
Acceptance remains bounded to this initialization sequence; four full-table BBP
readbacks, remaining MAC prerequisites and RF/connectivity are still unresolved.
Battery work and its Air-specific hardware acceptance remain separate.


### Aggregation-length initialization (v46; native accepted)

`--wifi-mac-ampdu` follows exact FCE validation with one sequence15 pair:
1030=aaa99887 and080c=03ff0223 (retained L2 routing). The first target is
MT_AMPDU_MAX_LEN_20M1S in the Linux v7.0 mt76x0 MAC initialization table.
Four read-only Linux samples on this7392:a711 agree with both target words;
these running-device samples do not establish reset-state semantics.

Prior raw native inventory recorded1030=ffffffff with successful four-byte USB
completions. This dedicated engine admits only that exact initial observation,
then requires exactaaa99887 after the acknowledged write. Existing generic reads
and earlier profiles retain their all-ones rejection. It refuses failed transfers,
any different initial value, altered ACK/readback, lost ownership, nonzero MAC or
wrong CORE. Eight control reads record actual length/completion/residue/timeout;
collection requires all eight successful transfers, exact sequence15 ACK, both
readbacks and both completion markers. No retry, BBP table, RF or MAC TX/RX enable.
Sequence15 is the last nonzero four-bit identifier; future commands must explicitly
address reuse instead of incrementing into16. This trial does not accept scanning,
aggregation performance or connectivity. Source: Linux v7.0
[mt76x0 initialization table](https://raw.githubusercontent.com/torvalds/linux/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/initvals_init.h).


Pro11,2 source `9fa1ff6`, release `e14f396f3d59c1b6`, session
`00065e8ccad60890-00` passed on 2026-10-23. Sequence15 returned the exact ACK;
1030 changedffffffff->aaa99887, while080c remained03ff0223. All eight ordered
control transfers reportedok1/actual4/cc1/residue0/timeout0. Initial/final
MAC0/COREf000f200, all preceding groups and both completion markers passed.
The prepared collector passed unchanged; Ubuntu recovery and payload hashes
verified, with clean offline allocation (233files/598clusters, zero errors).
This accepts the bounded write/readback on this adapter; it does not establish
reset semantics, aggregation performance, RF or connectivity. The complete
collection and hashes are archived outside build. Remaining MAC prerequisites
and the four unresolved full-table BBP readbacks are still open.

### Post-MAC baseband observation candidate (v47)

The opt-in `srvros.wifi-bbp-after-mac=1` profile repeats the existing 58-entry
baseband table after all accepted MAC groups through AMPDU initialization.
Every prerequisite must be complete, submitted and inactive. The unchanged
200/200/88-byte packets use sequences0/0/6 after sequence15 has completed.
The final exact address/value ACK distinguishes this response from the earlier
sequence6 routing response; replay tests reject that response and sequence15.
All124 reads and all58 before/after values are retained. Completion remains
observation only (`register_acceptance=NOT_TESTED`), including any mismatches.
The old v31 and v46 profiles retain their behavior. No RF or connectivity
acceptance is claimed; physical v47 results are pending.

The v47 native run on Pro11,2 completed with all prerequisites,124 reads and
58 observations validated. The same four v31 mismatches remain: TXBE10/2728
read38e30 instead of0, TXBE17/2744 readfb3fee3c instead of0, RXO21/2954
read0 instead of1, and RXO24/2960 read0 instead of6. Only TXBE17 differed
from the earlier before/after capture (fb3fec3c); causality is not established.
Thus MAC initialization through AMPDU did not resolve the table mismatches.
Register acceptance remains NOT_TESTED. The next investigation is reference
register semantics and initialization ordering, before any new native writes.

A subsequent read-only Ubuntu capture found all four values identical to v47
(including TXBE10=38e30), unlike older Linux TXBE10=0 samples. Since this Ubuntu
boot followed SrvrOS without confirmed adapter power loss, the next comparison
is a cold adapter cycle under Linux, not another native table write. The pinned
Linux initialization writes the table without requiring these four readbacks
to equal the written values. Their readback semantics remain unresolved; neither
the current Linux match nor complete table transport accepts RF/connectivity.

The user-confirmed10-second cold adapter cycle under Ubuntu produced four
consistent samples: TXBE10=38e30, TXBE17=ff3fef3c, RXO21=0 and RXO24=0.
PHY re-enumerated fromphy1 tophy2; exact descriptor/model/driver gates passed.
Thus all four table-equality mismatches also occur after cold Linux initialization.
Warm carryover alone is insufficient to explain them; TXBE17 varies, and its
semantics remain undocumented by this audit. No native acceptance predicate
changes follow from this observation. Next audit the reference BBP switch/DCOC
stages and ownership/RF boundaries before a bounded continuation.

### Next continuation: fixed G-band20MHz baseband switch

The [Linuxv7.0 initialization sequence](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/init.c)
applies the [bandwidth/band table](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/initvals.h)
after the full BBP table. Its G-band20MHz selector chooses20 entries (19 AGC
registers and RXFE0), using direct USB request6 writes with four-byte LE data.
The subsequent nine-entry CAL47..55 DCOC table uses MCU random-write; later
EEPROM/PHY initialization reaches RF configuration. These are separate stages.

The next bounded implementation is the full20-entry switch group after v47,
with fixed address/value/ordering admission, exact transport receipts, disabled
MAC/CORE guards and before/after observations. It must not allocate another
MCU sequence or enable TX/RX. DCOC and RF/channel work remain subsequent scope.
Native implementation and acceptance of this stage are pending.

Fresh live Linux samples differ from19 of29 switch/DCOC initial targets. Many
match A-band variants, including valid all-ones values in AGC51/53/55. That does
not justify admitting all-ones in the proposed G/20 stage, whose fixed values
are all non-all-ones. Runtime samples are comparison evidence, not initialization
expectations or proof of association. Exact selection/transport fixtures and
the source audit are archived in the Pro11,2 evidence directory.

The v48 candidate now implements this fixed20-entry switch through direct USB
writes after v47 completes. Each write is preceded by MAC0/COREf000f200 checks;
88 reads cover snapshots and guards. Exact index/address/value, prerequisite
completion and transport ownership are required; attempted writes cannot retry.
The full capture reports mismatches as observations (`register_acceptance=NOT_TESTED`).
No RF/DCOC/channel/TX/RX enable is added. Native acceptance remains pending.

The v48 native trial completed on Pro11,2:20 direct writes,88 reads and every
MAC/CORE guard passed; all20 G/20 targets matched (16 changed,4 unchanged).
All preceding prerequisites and both completion markers passed. The earlier
full-table four mismatches remain separately recorded. Recovery/filesystem
checks passed after a local collector variable-shadowing fix; validation
predicates were unchanged. This is bounded write/readback evidence, not RF
or connectivity acceptance. Next is the separate nine-entry CAL47..55 DCOC
stage with exact MCU framing/ACK and guarded observations.

The v49 candidate adds the fixed nine-entry CAL47..55 DCOC table after
exact G/20 switch readbacks. It uses one80-byte MCU random-write packet and
26 guarded reads. Sequence7 is deliberately reused after all previous responses;
the exact ACK address/value (412cbc/10f0) rejects the earlier policy response.
Ownership and prerequisite completion are checked throughout; no retries or
RF/channel/TX/RX enable are added. Mismatches remain observations rather than
accepted register targets. Native results pending.

The v49 Pro11,2 native capture passed unchanged collector checks: one80-byte
sequence7 write, exact16-byte reply,26 guarded reads and nine exact target
readbacks (eight changed, CAL51 unchanged). All preceding prerequisites and
both completion markers passed; recovery and offline workspace checks passed.
This is bounded DCOC table evidence, not RF calibration or connectivity acceptance.
Next audit the remaining MAC/security-table prerequisites and PHY/RF entry
against the existing device calibration/regulatory captures before continuing.

### Post-DCOC initialization roadmap

The [Linuxv7.0 MAC helpers](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_mac.c)
and [register layout](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_regs.h)
define the next non-RF stage:64 shared-key slots across eight cipher-mode words.
Each slot receives a masked cipher-field clear and32 zero bytes. The following
WCID initialization clears256 attributes but only128 eight-byte addresses; it
does not clear pairwise-key or IV memory in that helper.

The selected continuation is shared-key reset with fixed USB address admission,
guards, no retry and zero verification without logging key bytes. Its high
addresses must not widen the generic MCU register-write interface. Station
reset is separate. Derived operation plans,64 zero-only fixtures and100 seeded
mode-preservation checks are archived; native implementation is pending.

22 reference MAC-table addresses still lack corresponding initialization stages;
baseband table completion is not complete MAC initialization. Remaining controls,
physical EFUSE-map validation, private MAC-address setup and PHY/RF entry need
separate evidence. Existing logical calibration and US confirmation are retained.
[EEPROM initialization](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/eeprom.c)
distinguishes external-image loading from its physical-EFUSE fallback; logical
decode alone does not validate the latter. No RF/channel/TX/RX enable follows
from this audit.

### Fixed shared-key reset diagnostic (v50)

`--wifi-skey` requires the complete DCOC profile and exact nine-register DCOC
readbacks. It resets only the reference's 64 shared-key slots: for each slot,
read MAC/CORE guards, clear its cipher nibble while preserving other fields,
verify the mode word, write 32 zero bytes, verify all bytes are zero, and recheck
the guards. The fixed plan contains 576 operations (448 reads and 128 writes).
Mode words are limited to b000–b00c and b3f0–b3fc; shared slots to ac00–afff and
b400–b7ff. Each control transfer has one attempt and an exact length check.
Failures stop the sequence; uncertain DMA ownership retains quarantine.

The saved summary reports operation, slot and verification counts without key
bytes. There are no initial key-memory reads, WCID/pairwise/IV writes, RF or
TX/RX enable. All 64 mode and zero verifications plus prior-stage checks are
required for native acceptance. Host/VM checks alone do not accept hardware.

### Fixed WCID station reset diagnostic (v51)

`--wifi-wcid` requires the shared-key profile and exact completion counts for
all 64 slots. In reference order it writes and verifies 256 zero attributes at
a800–abfc and 128 eight-byte zero address entries at1800–1bf8. Entries128–255
have no address write. Each entry starts and ends with MAC0/COREf000f200 guards;
the fixed plan totals1792 operations (1408 reads and384 writes). Ownership,
address, direction, length, order and zero-only payload are checked per transfer.
There is one attempt per operation, with failure stopping the sequence.

Only aggregate verification counts are logged. This stage does not touch
pairwise-key or IV tables and does not enable RF, transmit or receive. Hardware
acceptance requires all256 attributes and128 addresses verified, all earlier
prerequisites, both capture completion markers and clean save/recovery checks.

### Remaining work after native shared-key and WCID resets

The v50/v51 resets are accepted on the captured Pro11,2 adapter. They complete
those two reference loops, not the whole MAC/PHY initialization path.
The post-WCID audit classifies22 untouched MAC table addresses: one indicator,
one DMA setting, two ACK/TXOP settings, six board/power/PA/LDO settings,
ten TX power/gain settings and two unnamed reference settings. Untouched does
not mean every value must be copied; an LED setting is not a connectivity gate.
Previous touched-address coverage also does not prove complete field/order parity.

The next bounded diagnostic is a fixed read-only post-WCID snapshot: MAC/CORE
guards,22 target reads, then both guards again (26 reads, no writes). Target
all-ones values remain raw observations; guard failures remain failures. This
collects the whole remaining table in one trial without enabling DMA or radio.
The offline plan/USB request fixtures are prepared; native wiring remains next.

The reference then loads EEPROM and programs the device MAC before PHY entry.
Its physical fallback reads32 bytes at1e0/1f0 and validates the29-byte usage map
through1fc. Logical EEPROM bytes at those offsets are not equivalent evidence.
Preserve the externally supplied image path versus physical fallback distinction;
program private validated MAC bytes without logging them.

PHY work still includes antenna selection, RF bank tables/bandwidth/band setup,
frequency/DAC/VCO initialization, RX path and TX DAC. Channel and transmit-power
settings must use the captured calibration and US policy already recorded.
After device initialization come USB data RX/TX,802.11 frame handling, scanning,
authentication/association and WPA2/key lifecycle, followed by DHCP/DNS/ping/HTTP,
reconnect and UI acceptance. Existing Ethernet success does not accept the
wireless link layer. No percentage or number of remaining boots is established.

### Post-WCID MAC observation implementation (v52)

`--wifi-mac-tail` requires the completed WCID profile and exact prior counts.
It reads the22 audited targets in fixed order between MAC0/COREf000f200 guards,
26 reads total. Each USB request is read-only request7, length4, with exact
address/order/ownership checks and one attempt. Errors stop; uncertain DMA
ownership remains quarantined. No new writes or RF enable are added.

Logs contain two boundary guards and22 indexed address/value observations.
Target all-ones values are counted and retained without accepting them as valid
settings. The collector requires exact ordering/counts, all previous prerequisites,
both completion markers and recovery/filesystem checks; register acceptance stays
NOT_TESTED. Prior boot recipes retain their exact configuration.

### MAC difference decisions after v52

The saved22-target snapshot has19 reference-default differences. Partition them
into four fixed controls (1218,1220,13c0,1238),nine calibrated power settings,
four board IO/supply settings,one optional LED and one DMA configuration.
The [shared PHY power path](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_phy.c)
rewrites ALC channel fields and eight per-rate power registers; the
[MT7610 channel-power path](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c)
derives and limits those values. Do not equate initialization defaults with
accepted transmit power.

Next planned diagnostic groups1218/1220→00500055,13c0→0,1238→001700c8,
with exact initial-value admission,MAC/CORE guards,DMA-disabled checks and
single-attempt transfers.32 operations comprise28 reads and four writes.
Native implementation/acceptance remains pending. DMA burst/writeback/clock-gate
fields and board IO/supply settings require separate ownership/semantic review.
The snapshot alone does not justify copying every reference default.

### Four fixed MAC controls implementation (v53)

`--wifi-mac-fixed` requires v52 and the four exact captured starting values.
It updates1218/1220 from00550055 to00500055,13c0 from18181818 to0,and1238
from003700c8 to001700c8. For each target: read MAC,CORE,DMA and initial target;
write once; read target,MAC,CORE. The32 operations contain28 reads/four writes.
MAC must remain0,COREf000f200,and DMA enable/busy bits clear before each write.
The transport separately validates order,address,length,payload and prior reads.
Errors stop without retry; uncertain DMA ownership retains quarantine.

Eight guard pairs and four before/after/DMA observations accompany exact counts
in the saved log. The collector requires exact initial/final values and all prior
stages. No calibrated power,DMA configuration,board supply or RF-enable writes
are included. Hardware acceptance remains pending until a matching native run.

### Physical EFUSE usage-map engine (host validation)

The isolated `mt76_capture_efuse_map` engine permits only physical-read mode1
at1e0/1f0. It preserves other control fields,checks captured identity,and guards
MAC0/COREf000f200 plus DMA enable/busy bits before each request. Two blocks are
read with a shared1024-poll budget,2us settle delays,and no transfer retries.
Completion must echo physical mode and the requested block,with kick clear and
mapped output. Programming modes2/3 cannot be generated. Existing logical capture
is unchanged. No native caller or boot profile is added by this engine work.

The29-byte map excludes the last three transfer padding bytes. The reference
check uses the first-to-last zero span,not zero count; its no-zero case has span1.
The summary names `reference_used` as the reference's29-minus-span estimate,
not the actual number of programmed entries. Require estimate>=5 plus meaningful
map evidence; all-ff/unmapped data is refused even though span arithmetic alone
could pass it. Capture completion and map validity remain separate states.
Raw map bytes are cleared after analysis and never logged by the engine.

Host tests cover fixed mode/address construction and preservation,all I/O and
cancellation boundaries,guard failures,mode/address/kick corruption,poll timeout,
unmapped/empty evidence,threshold cases,fragmented zero spans and padding exclusion.
The native transport independently enforces the same two-block read-command
allowlist,ownership and quarantine.

### Physical EFUSE map trial

The opt-in `--wifi-efuse-map` extends `--wifi-mac-fixed` as the distinct
`wifi-fw-v54` capture. It reads physical mode1 blocks1e0/1f0 after the complete
v53 chain. A separate native transport state machine validates ordering, exactly
two preserved-control commands, bounded polling, delays and MAC/CORE/DMA guards.
Each USB transfer is attempted once; uncertain DMA stays quarantined. Logical
EFUSE mode0 admission remains unchanged. Modes2/3 are never generated.

Saved receipts include operation/poll counts, four MAC/CORE guards, two DMA
readings and map summary arithmetic; private map bytes are cleared and not logged.
A completed read is distinct from a valid map. This stage does not accept RF,
calibrated power, scanning, association or connectivity. On the captured Pro11,2
adapter, release `b34ed974d2fa3095` passed native map validation:24 reads,two
mode1 commands,two polls,both blocks,and all guards. Nine zero entries span
1e0–1e8; the reference span calculation yields20 used entries (an estimate,not
a general programmed-entry count). Private MAC setup and PHY/RF remain pending.

### Private factory-address plan

The next host-tested engine consumes a fresh mapped logical block0 after the
accepted physical-map check. It accepts the captured7610/version2/FAE1 header and
a nonzero unicast address at offsets4..9; locally administered addresses are
permitted, but invalid addresses refuse instead of generating a random fallback.
Historical calibration logs redact these bytes and cannot supply this input.

The reference programs1008/100c (station address/U2ME mask),1010/1014 (BSSID and
MBSS/beacon fields),then clears eight unique BSSID pairs1090..10cc. Its16-iteration
loop masks the index to7; the proposed engine performs one verified pass over
those eight pairs. It combines the two1014 operations into the same final value
while MAC is disabled. High16 bits of each BSSID table high word are preserved.
These are explicit plan differences,not claimed hardware acceptance.

Twenty targets each require MAC0/COREf000f200 and disabled/idle DMA,initial target
read,one write,exact verification,and final MAC/CORE guards:160 operations,
140 reads/20 writes. All-ones initial targets refuse. Result receipts contain
counters only,not address bytes. Every failed operation stops without retry.
The private header copy is cleared on exit. Native logical-block acquisition,
independent transport admission and scratch clearing are implemented by the
explicit trial below; physical acceptance remains pending.

### Native private-address trial

`--wifi-mac-address` extends `--wifi-efuse-map` as `wifi-fw-v55`. After the
complete v54 chain,it privately reads only logical block0 in fixed mode0 with
bounded polling and fresh MAC/CORE/DMA guards. An unmapped,malformed or invalid
header refuses address programming. The existing full logical capture is unchanged.
The native programming gate independently validates all160 ordered operations,
exact derived values,readbacks and guard observations. No operation is retried.

Private stages disable immediate USB data: descriptors contain DMA pointers,
not factory address bytes. Confirmed descriptor buffers and the private header
are cleared; uncertain DMA stays quarantined and is never dumped. Saved receipts
contain only counts and20 compact before/after guard/DMA groups,keeping the prior
initialization evidence within the bounded exit log. Native acceptance is pending.
RF/channel/power setup,scanning and connectivity remain separate work.

Native v55 release `fbedd261796dd08f` completed private mapped header acquisition
but failed address initialization at target1 (100c) readback:13 operations,
14 submissions,two writes and one verified target. The current counters cannot
distinguish transport failure from value mismatch. Both final completion markers
report failure; complete station/BSSID setup remains unaccepted. The next
comparison needs privacy-preserving failure classification, not a relaxed mask
or raw factory-address logging. Evidence and clean recovery are recorded in the
Pro11,2 unit record.

The v55 comparison now snapshots the last address USB transfer before cleanup:
operation, success, actual length, completion code, residue, stage, timeout and
quarantine. Gate failure codes distinguish admission(1), transport(2), state(3),
MAC(4), CORE(5), DMA(6), all-ones initial target(7) and exact readback mismatch(8);
zero means no gate failure. A compared readback reports address/control equality.
Only targets1/3 expose their public upper16 control bits; all factory-address
bits remain private. Comparison fields clear at each admitted operation, so a
later guard failure cannot reuse a prior comparison. The strict write sequence,
exact verification, one-attempt transport and automatic failure return are
unchanged. These diagnostics do not accept a failed initialization.

The Pro11,2 native diagnostic (`d1d3486f342b9121`) isolates the100c failure:
USB read succeeded with4 bytes,cc1,no residue/timeout/quarantine; factory-address
bits matched,while public control bits read00000000 versus requested00ff0000.
Gate failure8 stopped the unchanged strict sequence. This is a confirmed control
readback mismatch,not accepted station/BSSID initialization or evidence by itself
that those bits may be ignored. Audit captured-device U2ME semantics before
changing write values or verification masks. Prior prerequisite/header receipts,
saved failure return and offline filesystem checks passed.

### Captured station-control readback comparison

The opt-in `--wifi-mac-address-validate` requires `--wifi-mac-address` and stages
`wifi-fw-v56`. It preserves v55's writes and transport, but requires100c's exact
captured readback: the private address low16 and zero upper16. No bits are ignored;
any nonzero control readback or changed address bit refuses. Other targets,
including1014,retain exact verification. The legacy v55 expectation is unchanged.
The independent native gate and engine must select the same profile,after the
existing exact ASIC/MAC/header/map prerequisites. A separate saved profile marker
identifies this trial; successful completion still needs all160 operations and
20 verified targets. Native v56 acceptance is recorded below.

This follows two cold SrvrOS captures and four read-only samples on the same
adapter under Linux:100c upper16zero,1014 upper16=003f. Linux had enabled MAC and
different DMA state,so it is corroborating evidence rather than a disabled-state
acceptance test. Address bits were masked before saving Linux results. Upstream
[MAC initialization](https://github.com/torvalds/linux/blob/master/drivers/net/wireless/mediatek/mt76/mt76x02_mac.c)
still writes U2MEff; its
[register definition](https://github.com/torvalds/linux/blob/master/drivers/net/wireless/mediatek/mt76/mt76x02_regs.h)
places that field in bits23:16. These sources do not establish universal read-only
semantics. Preserve the reference write and scope the exact readback to this
captured device,not sibling hardware. RF/channel/connectivity remain separate.


On the captured Pro11,2 Edimax adapter,release `be58d4957e3c4d7a` (source87395a2,
session `session-00065e9b722cf305-00`) passed v56:160 ordered operations,
140 reads,20 writes and20 exact verifications. Both station/BSSID words and all
eight unique table pairs passed,including the captured100c readback and strict
1014 fields. Forty MAC0/COREf000f200 guard pairs and20 disabled/idle DMA checks
passed. Private mapped header acquisition,prior prerequisites,both completion
markers and saved return passed; no address bytes logged. This accepts the
bounded address initialization on this adapter,not RF behavior,calibrated power,
association or connectivity. Ubuntu recovery and offline filesystem/allocation
checks passed. Preserve v55 failure history; next work is the board/PHY/RF audit.

### Fixed board power and I/O prerequisites (v57)

The opt-in `--wifi-mac-board` requires `--wifi-mac-address-validate` and stages
`wifi-fw-v57`. After completed private address setup, it applies four fixed
settings from the [Linux v7.0 MT7610 initialization table](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/initvals_init.h).
Four read-only Linux samples on this adapter corroborate the target values;
the accepted v56 native snapshot supplies the exact required initial values.

| Register | Required initial value | Write and exact readback |
| --- | --- | --- |
| PWR_PIN_CFG, 1204 | 0000000a | 00000000 |
| IOCFG6, 0124 | a0140280 | a0040080 |
| LDO0, 006c | 00a647a4 | 00a647b6 |
| LDO1, 0070 | 6b006868 | 6b006464 |

Each target requires MAC disabled, CORE=f000f200, idle/disabled DMA and the
exact initial value before its single write. Exact readback and another MAC/CORE
pair follow. Success requires 32 operations, 28 reads, four writes and four
verifications. The independent native transfer admission checks the fixed address,
direction, payload, prior observations, ownership and submission count. Failure
stops without retry; uncertain USB completion retains quarantined DMA resources.
Private mapped-header bytes must already be cleared, and every prior v56 phase
must have completed. Saved receipts contain only these public register values.

This board power/I/O trial is accepted on the captured adapter as recorded below. It adds no
RF-bank, channel, rate-power, antenna, MAC-enable or DMA-configuration writes.
Linux's live enabled-MAC observations are reference evidence, not native disabled
state acceptance. Existing profiles remain unchanged. The strict collector
requires the full earlier initialization proof and all new receipts; missing,
duplicate or altered receipts cannot pass. RF and connectivity remain untested.

Native source `3af56f0`, release `69718a055422766a`, session
`session-00065e9c42605b54-00` passed all 32 operations: 28 reads, four writes and
four exact target verifications. Initial values and readbacks matched the table;
all eight MAC0/COREf000f200 pairs passed, with DMA40000050 at all four checks
(enable/busy low four bits clear). The complete earlier initialization proof,
both completion markers, saved exit log, Ubuntu recovery and offline filesystem
checks passed. This accepts these four fixed settings on this adapter only.
PHY antenna selection, RF tables/calibration, channel/regulatory setup, data
transfers, association, authentication and usable Wi-Fi remain separate work.

### Antenna prerequisite observation (v58)

`--wifi-ant-snapshot` requires `--wifi-mac-board` and stages `wifi-fw-v58`.
After all v57 prerequisites pass, it reads WLAN_FUN_CTRL80, CMB20, CSR_EE_CFG1
104, COEXCFG0 40 and COEXCFG3 4c, with MAC0/COREf000f200 and disabled/idle DMA
checks before and after. The eleven-read engine has no write callback. Native
admission checks exact read order, single submission and completed board values,
counts and guard observations, plus the existing private-address ownership chain.
All-ones reads, guard changes, failed transfers or cancellation stop the capture;
uncertain USB completion preserves quarantined resources. Five indexed values
and two boundary receipts are saved. Completion accepts observation only.

The [Linux v7.0 antenna selection](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c)
uses these registers with EEPROM22/24/42 and board band capabilities. This unit's
accepted mapped calibration has antenna17ff, cfg1ffef, NIC0fd11, NIC11004,
NIC2ffff: single antenna and a both-band hint; NIC2's diversity flags are not used
by the single-antenna branch. Linux clears antenna bits14/12, giving low07ff.
These decoded fields do not authorize channels or transmission. Fresh Linux
samples corroborate CMB low07ff and CFG1 lowffef, but WLAN changed bit4 between
live samples from separate runs. Preserve unrelated bits and obtain the cold
native starting state before designing antenna writes. No antenna/RF writes
are added in v58; physical observation completed as recorded below.

Source `1d291a7`, release `4964d0724ea58faa`, session
`session-00065e9ca514ef7a-00` completed all eleven reads and submissions, with
zero writes in the snapshot. Both boundaries showed MAC0, COREf000f200 and
DMA40000050. All earlier initialization prerequisites and both completion
markers passed; saved logs, Ubuntu recovery and offline filesystem checks passed.

| Register | Cold native observation | Derived antenna target (not yet written) |
| --- | --- | --- |
| WLAN_FUN_CTRL80 | ff000003 | ff000003 |
| CMB20 | 00c01fff | 00c007ff |
| CSR_EE_CFG1 104 | 0033ffef | 0033ffef |
| COEXCFG0 40 | 6f008847 | 6f008843 |
| COEXCFG3 4c | 00000001 | 00000019 |

The targets follow the captured single-antenna EEPROM fields and the reference
algorithm while retaining the native high/control bits. In particular, do not
copy Linux's full WLAN or CMB values: its enabled-driver state differs. Next
work is bounded antenna initialization with fresh mapped-field validation,
exact starting-state checks and readbacks. This result accepts observation only;
antenna function, RF calibration and connectivity remain untested.

### Fresh configuration and antenna setup (v59)

`--wifi-ant-init` requires `--wifi-ant-snapshot` and stages `wifi-fw-v59`.
The completed snapshot must exactly match the cold values above. The new stage
reads mapped logical EFUSE blocks20/30 only, with two mode-zero read commands,
bounded polling, four MAC/CORE guard pairs and two disabled-DMA checks. It
requires fresh antenna17ff,cfg1ffef,NIC0fd11,NIC11004 before permitting antenna
writes. These blocks contain no factory MAC address. Unmapped blocks, changed
fields or completion-control mode/address mismatches refuse; EFUSE programming
modes are never issued. Historical physical-map and private-header reads keep
their original block/mode constraints.

The fixed register plan follows the five targets in the preceding table in
order. Each has a fresh initial-value read and MAC0/COREf000f200/disabled-DMA
checks, one write, exact readback and another MAC/CORE pair. Success requires
40 operations,35 reads,five writes and five verifications. Native transport
independently admits only the next request and exact payload after completed
snapshot/board/private-address ownership and fresh configuration validation.
No retry follows a failure; uncertain completion retains quarantined DMA.

Saved receipts include configuration completion/counts, aggregate guard-error
counts, initialization counts and every before/after value. A zero guard-error
count is insufficient by itself: the collector also requires all operation
counts, exact values and the full earlier proof. Compact new receipts keep the
combined fixture within the existing 64KiB exit snapshot; old receipts remain
unchanged. The native result below accepts these five register targets only.
This initializes antenna registers;
RF-bank tables, channel selection, calibration, radio traffic and connectivity
remain separate work.


Source `eaf911c`, release `28c2ba352d59b843`, session
`session-00065e9d031419f8-00` passed the strict v59 collector on the captured
Pro11,2/Edimax adapter. Fresh mapped configuration completed with 24 reads,
two read-command writes, two polls and 26 submissions. Antenna initialization
completed all 40 operations/submissions, 35 reads, five writes and five exact
verifications, with zero guard errors. Readbacks in table order were ff000003,
00c007ff, 0033ffef, 6f008843 and 00000019. All earlier prerequisites and both
completion markers passed. Saved logs, automatic return, Ubuntu recovery and
both offline filesystem checks passed. RF-bank initialization and calibration
are next; antenna RF performance, scanning and connectivity remain untested.


### Read-only RF transport probe (v60)

`--wifi-rf-probe` requires `--wifi-ant-init`. After the complete v59 chain, this
stage sends one fixed CMD_RANDOM_READ10, sequence8 request for RF bank0,
register22 (frequency trim), with a zero value field. Linux v7.0 uses the MCU
RF address space `0x80000000 + (bank << 16 | register)` on USB; the PCI RF CSR
path is not used. See the [RF access path](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c)
and [USB MCU transport](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_usb_mcu.c).

Six direct reads check MAC0/COREf000f200/disabled DMA before and after the
exchange. Native admission requires exact completed antenna/configuration proof,
request bytes and order, and one submission only. The observed response contract
requires 16 bytes, sequence8/event0/length8 header `4a080008`, address `80000016`,
an eight-bit value, and matching trailer. These framing fields were observed on
the captured adapter in the native run below. The
first four response words and transport counts are saved on a mismatch. A failed
exchange or changed guard stops without retry and retains uncertain USB DMA.
No RF register write, calibration, channel choice or transmitter enable occurs.

Wi-Fi firmware captures now opt into a 128KiB complete startup snapshot. Expansion
refuses already lost startup bytes; overflow refuses saving a partial proof.
Ordinary desktop/battery snapshots stay at 64KiB. The larger kernel buffers add
192KiB of static storage; there is no runtime allocation or new periodic schedule.
Host collection retains its total 64MiB ceiling.

Source `5fb885e`, release `63fb5a9f9addbb21`, session
`session-00065e9d618d5674-00` passed the strict v60 collector. The one 16-byte
response contained `4a080008/80000016/00000000/4a080008`: RF0:22 returned zero.
All six boundary reads passed, with MAC0, COREf000f200 and DMA40000050 before
and after; no RF writes occurred. All preceding initialization proof and both
completion markers passed. The complete 65730-byte startup snapshot was saved,
194 bytes beyond the former 64KiB limit. Automatic return, Ubuntu recovery and
both offline filesystem checks passed. This accepts the command/response
observation only; it does not establish calibrated frequency trim or radio function.

The next work is bounded RF-table initialization, fresh
frequency-offset validation, DAC/VCO calibration and receiver setup; scanning,
802.11 association/security and network integration remain later milestones.


### Fixed central RF initialization (v61 candidate)

`--wifi-rf-central` requires `--wifi-rf-probe` and its entire initialization
chain. The v60 response must be exact and RF0:22 must still read zero. This
stage programs the 44 fixed central bank0 settings, in reference order, with
the USB MT7610 R3=73 and R21=12 values. Numeric settings and ordering follow
[Linux v7.0 central RF initialization](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/initvals_phy.h)
and its [USB/model patches](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c).
There is no caller-supplied register plan. RF0:22 frequency trim and the later
DAC reset/VCO trigger sequence are excluded; R4 is written with its trigger bit
clear. The central table includes the reference ADC/DAC startup configuration.

Each entry uses three fresh MAC0/COREf000f200/disabled-DMA guards, one sequence-
numbered CMD12 write, one CMD10 readback, and three final guards. Sequence numbers
advance from nine and wrap through one to fifteen, never zero. Each response must
have the exact 16-byte framing, current sequence, RF address and byte value. The
write echo is a candidate contract; independent readback must also match.
The native binding checks the complete preceding proof, every operation/count,
exact payload and prior readbacks before submitting. Failures stop without retry
and uncertain USB DMA stays quarantined. Ordinary desktop startup is unchanged.

Success requires 352 operations/submissions, 264 guard reads, 88 command exchanges,
44 write echoes and 44 verified readbacks with zero guard errors. Saved receipts
retain every transport result and every target/readback; failure receipts include
the current response words and guard values. The collector requires all of those,
the full earlier proof, both completion markers and a complete 128KiB-bounded
startup snapshot. Host/synthetic results do not accept RF writes on hardware.
Band-specific tables, fresh frequency offset, calibration, receiver setup,
channels, scanning and connectivity remain separate work.


Native v61 source `23cb72a`, release `d5f9b767d30876ce`, session
`session-00065e9da8109959-00` **failed** strict completion at RF0:6. The first
five entries (RF0:1–5) matched their readbacks. All twelve command exchanges
completed with sent16/received16/cc13/residue1008; six write echoes validated.
The sixth read returned `4a050008/80000006/00000000/4a050008`: exact framing and
address, but value00 instead of41. State4 stopped the engine at44 completed
operations/45 submissions,33 guard reads,12 exchanges,6 echoes and5 verifications.
No later table entry was attempted. Both overall completion markers are NO.
The strict collector rejected this run; central RF initialization is not accepted.

The current pre-write boundary remained MAC0/COREf000f200/DMA40000050. The
post-read boundary was not reached. The summary's `guard_errors=77` counts
unexecuted zero-initialized guard slots, not77 observed guard failures; use the
operation count and boundary receipts when interpreting this failed candidate.
The saved complete70943-byte snapshot, recovery checks and both offline filesystem
checks passed. Next investigate RF0:6 semantics and sequencing with bounded
observations; do not mask the mismatch or continue the table without evidence.
Calibration, radio function and connectivity remain untested.


### Bounded RF0:6 comparison (v62 candidate)

`--wifi-rf-six` requires `--wifi-rf-central`. The ordinary v61 strict engine is
unchanged. The v62 authorizer and native submission checks independently cap its
prefix at45 submissions: six writes and six reads with the first five entries
verified, stopping before the sixth entry's final guards or any later entry.
If the sixth read returns41, the strict engine is deliberately cancelled at that
boundary. Any other eight-bit value retains its state-mismatch result. Only these
exact states, with six validated write echoes, correct sixth response framing,
33 prior guard reads and all earlier initialization proof, admit the comparison.
Transport/framing failures, other cancellations or earlier mismatches refuse.

The separate comparison performs fresh MAC0/COREf000f200/disabled-DMA guards,
then five CMD10 reads in order: RF0:6, RF0:1, RF0:2, RF0:6 and RF0:22, using
sequences6–10. Each is followed by another guard triplet. RF0:1/2/22 must match
1/17/0; the two RF0:6 values are observations only. Success requires23 operations,
18 guard reads and5 exact-framed replies, with zero additional writes. No retry,
RF0:6 rewrite, later table initialization, trim, calibration or traffic is added.

The logs explicitly say `observation_only=1` and `central_complete=0`, retain the
initial sixth reply plus all five comparison replies, and distinguish checked
guards from unexecuted slots. Overall capture completion means this bounded
observation finished, never that central initialization or RF function passed.
The Linux reference writes RF0:6=41 without immediate verification, but gives no
readback exemption establishing that zero is correct. The exposed mt76 debugfs
interface on this Ubuntu adapter provides generic MMIO regidx/regval only, so
no unsupported live-driver RF access was attempted. Compare the native repeated
values and controls before changing any strict acceptance rule.

Native v62 source `c69c064`, release `f44fa8061c8014a5`, session
`session-00065e9df83b5391-00` completed the bounded observation. RF0:6 returned
zero initially and on both later reads (sequences5/6/9); interleaved RF0:1/2/22
controls returned1/17/0 with exact sequence/address/framing. All five comparison
exchanges and18 guard reads passed, totaling23 operations/submissions. The capped
central prefix again stopped with state4,45 submissions,6 echoes and5 verified
entries. Its33 reached guard reads passed;77 boundary slots were unexecuted.

The strict full-prefix collector and both completion markers passed. The complete
69763-byte exit snapshot, Ubuntu recovery and offline filesystem/allocation checks
passed. This accepts the observation only: repeated zero with correct controls
argues against a one-off response mix-up, but does not establish RF0:6 semantics
or justify accepting the central table. Next investigate the register's access
and initialization requirements before changing its readback rule or continuing
RF initialization. Calibration, scanning and connectivity remain untested.

### Direct RF-CSR comparison (v63 candidate)

`--wifi-rf-csr` requires `--wifi-rf-six` and its complete preceding proof. It
compares the MCU observations with five fixed direct RF-CSR reads, in order
RF0:6/1/2/6/22. The [Linux RF access definitions and read path](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c)
define the bank/register selector, busy bit and returned identity. The original
[vendor RF access implementation](https://github.com/GregersP/mediatek_mt7610u_sta_driver_linux-64bit/blob/95fd2aa84ad774b3a8d1636c32420d8676dcc347/phy/rlt_rf.c)
also provides direct access on USB when in-band MCU access is not selected.
This is a reference for a diagnostic hypothesis, not prior acceptance of direct
RF access on this adapter. The [vendor chip initialization/calibration source](https://github.com/GregersP/mediatek_mt7610u_sta_driver_linux-64bit/blob/95fd2aa84ad774b3a8d1636c32420d8676dcc347/chips/mt76x0.c)
describes RF0:6 as an amplitude setting and reads/modifies it during calibration;
it does not establish that zero is a valid readback of41. No vendor implementation
is imported into the diagnostic.

For each target, three fresh MAC0/COREf000f200/disabled-DMA guards precede up to32
idle polls at0500. One fixed selector write then starts a read, with RF-CSR WR
and data bits clear. Up to32 completion polls must end with the requested bank
and register identity; the three guards are repeated afterward. Controls RF0:1/2/22
must read1/17/0; both RF0:6 values are observations. These five selector writes
do not write RF register data. There are no additional central-table writes,
RF retries, calibration triggers, channel selection or traffic.

Independent native admission validates the complete v62 proof and exact current
operation/selector; transport uncertainty retains existing DMA quarantine. There
are30 guard reads,5 selectors and10–320 polls (45–355 total transfers). All-ones,
busy exhaustion, identity/control mismatch or any I/O error stops the diagnostic.
The saved log retains every USB result, final selector/identity, poll counts and
guard values. The strict collector checks their order and counts against the
full initialization prefix. Even the maximum-poll fixture fits128KiB. Completion
accepts only this comparison; central RF setup and connectivity remain pending.

Native v63 source `697111b`, release `032ade0806c417ab`, session
`session-00065e9e44b46d9c-00` passed the strict comparison collector. Direct
RF0:6/1/2/6/22 values were0/1/17/0/0, agreeing with the preceding MCU reads.
Every target needed one idle poll and one completion poll. All45 transfers,
40 reads,5 read selectors and30 guard reads passed; no RF data write was added.
The CSR words retained the requested bank/register identities. Both completion
markers and the full77073-byte startup snapshot passed, as did Ubuntu recovery
and offline filesystem/allocation checks.

This accepts direct RF read-selector transport on this captured adapter in this
bounded state, not central RF initialization. Agreement between the two paths
points away from an MCU reply-decoding error but does not distinguish an ignored
write from RF hardware semantics or missing initialization prerequisites. Next
investigate the write path and those prerequisites before relaxing any readback
rule or continuing the table. Calibration and connectivity remain untested.

### One direct RF0:6 write (v64 candidate)

`--wifi-rf-write` requires `--wifi-rf-csr` and all previous proof. Admission
requires the v62 MCU observations and v63 direct reads to reproduce the captured
zero RF0:6 profile, including the expected controls, every guard and all transfer
counts. No write occurs if that profile changes or the prerequisite is incomplete.

The fixed comparison uses the reference RF-CSR write encoding at0500: c0000641
selects bank0/register6, value41, WR and KICK. There is exactly one such write.
Three MAC0/COREf000f200/disabled-DMA guards precede bounded idle polling; the write
is followed by bounded completion polling and another guard triplet. A separate
80000600 read selector then starts an independent RF0:6 read, followed by bounded
completion polling and the final guard triplet. Each poll phase permits at most32
reads. Completion must report the correct bank/register and clear KICK; the
independent read must also clear WR. The returned byte remains an observation.
The write-completion data/WR fields are retained without treating them as an
independent RF readback. No arbitrary payload, retry, later table entry, RF0:22
trim, calibration trigger or packet traffic is added.

Success requires9 guard reads,2 selector writes and3–96 poll reads (14–107
transfers), with every USB result and poll value retained in order. Native
admission independently checks the full prerequisite and exact operation/value;
uncertain USB transfers retain DMA quarantine. The full worst-poll fixture fits
within128KiB. Completed capture accepts the experiment only. A41 independent
read would distinguish this direct write from the earlier MCU write; zero would
leave hardware semantics/prerequisites unresolved. Neither result accepts the
remaining central table or calibrated radio operation.


The 2026-10-25 native v64 comparison (source `4fa96c66`, release
`bc805e30aa1454bb`, session `session-00065e9fb28f464b-00`) completed all14
operations,12 reads and2 selectors with one poll per phase and all9 guard reads
valid. Direct write completion was `40000600`; the independent read returned
`00000600` (value0), matching the prior MCU/direct zero observations. Both capture
markers and the full79461-byte snapshot passed, with clean recovery/filesystem
checks. This accepts only the experiment. Switching the write transport did not
resolve RF0:6 readback; audit prerequisites/register semantics before another
write trial. The central table remains unaccepted; no calibration or traffic.


### Read-only RF control snapshot (v65 candidate)

`--wifi-rf-state` requires the completed v64 zero-read experiment and all prior
prerequisites. It adds14 fixed vendor register reads: MAC/CORE/disabled-DMA guards,
eight target observations, then the same three guards. Targets are WLAN_FUN_CTRL
`80`, CMB_CTRL `20`, OSC_CTRL `104`, PBF_SYS_CTRL `400`, RF_BYPASS0/1 `504/508`,
RF_SETTING0 `50c` and RF_MISC `518`. All-ones or failed reads stop the capture;
there is no write callback or caller-selected address. Native admission verifies
both the complete zero-read prefix and the precise next read, retains transfer
quarantine, and refuses replay. It adds no RF selector or data writes beyond the
existing v64 experiment. The eight values are observations, not accepted targets.

The reference [chip/clock sequence](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/init.c)
and [RF initialization](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c)
confirm the v64 CSR encoding and central-table order. They do not define a zero
readback exception for RF0:6. Read-only Linux debugfs sampling on this unit gave
four identical RF_BYPASS0/1=0, RF_SETTING0=0 and RF_MISC=5 observations. Those are
live, connected radio values: they cannot establish the proper disabled native
initialization state. WLAN GPIO input bits and CMB state also differ between the
live Linux sample and the saved native antenna setup; do not copy them wholesale.
The next snapshot fills the native bypass/control evidence gap before any new
RF data-write hypothesis. Calibration and connectivity remain pending.


The native v65 snapshot completed on2026-10-25 (source `48b79846`, release
`6e49ad417d7ea11e`, session `session-00065ea5459d3f13-00`). All14 read requests
and both guard boundaries passed. RF_BYPASS0/1 and RF_SETTING0 were all0, matching
the live Linux samples; RF_MISC was1 versus Linux5. The reference channel setup
sets RF_MISC bit2 for external A-band PA, so that live/disabled difference does
not identify a missing pre-table setting. WLAN differed only in GPIO input bit10;
CMB differed in bit21, whose semantics are not established by the pinned header.
OSC/PBF matched. Both completion markers and the full80450-byte prefix passed,
with clean Ubuntu recovery and filesystem checks. The snapshot adds observations
only. RF0:6 remains zero; investigate power/reset/register semantics without
copying the live state, retrying the write automatically or weakening acceptance.


### Live Linux RF comparison, 2026-10-25

A temporary device-matched probe called the loaded Linux RF read helper for
0:6/1/2/6/22 through the driver's MCU serialization. It returned8/1/17/8/68,
then unloaded. The adapter stayed bound, the default route remained unchanged
and gateway pings passed. No RF data write, driver reset/unbind or channel change
was performed. The first two loads stopped at unsupported interface runtime-PM
acquisition before reads; using an active-only parent-device reference resolved
the local probe issue without changing power policy.

The working Linux radio reads08 at RF0:6 after its initialization/calibration,
so41 is not a guaranteed persistent value. This does not establish the expected
early readback before calibration. The [reference initialization order](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c)
writes the central table without requiring immediate equality at each target.
Next add a separate full-table observation with fixed targets/values, complete
transport/guard checks and mismatch accounting. Preserve the original strict
engine and distinguish command completion, observed register values and actual
calibrated radio function. No automatic calibration or traffic follows a table
observation.


### Two-pass central RF table observation (v66 candidate)

`--wifi-rf-table` requires the accepted antenna setup and RF0:22 read probe.
It excludes the older central/six/CSR/write/state comparison modes. The original
strict central engine and v61–65 configurations remain unchanged. This separate
experiment follows the reference44-entry central write order, then reads all44
targets in a separate pass. It does not require immediate equality between writes.
RF0:22 trim, other RF banks/band tables, DAC/VCO calibration and packet traffic
are absent. Completed capture is explicitly an observation, not initialization
or radio-function acceptance.

Each of88 commands has three preguards and three postguards: MAC1004=0,
CORE2000=f000f200 and WPDMA208 neither all-ones nor enabled/busy in its low nibble.
The engine therefore performs616 operations:528 guard reads and88 exchanges.
Write acknowledgements require the exact16-byte header/sequence/address/value/
trailer echo; read replies require the exact envelope and one uint8 value.
Sequences start9 and wrap1..15 only after each completed exchange. Independent
native admission checks the precise next request and complete accumulated proof;
malformed/uncertain transfers stop with quarantine and are never retried.

The receipt retains all88 raw command replies,176 guard triplets and44 observed
values. It reports equal-count and a44-bit mismatch mask derived from every read.
Arbitrary readback patterns may complete an observation when their transport and
guards are valid; this does not accept those values or permit later calibration.
The full fixture is at most102527bytes, below the131072-byte startup capture cap.
The strict collector requires the complete prior initialization proof, both
completion markers and the complete untruncated saved prefix.


The native v66 observation completed on2026-10-25 (source `83152ee0`, release
`257e68678e55ba49`, session `session-00065eac64bddcbe-00`). All44 writes and44
separate reads passed command/transport checks; all616 operations,528 guard reads
and176 guard triplets passed. There were43 exact readback matches. Only index5,
RF0:6, differed:41 written/00 observed, mismatch mask0000000000000020.
Both completion markers and the complete102457-byte prefix passed, with clean
recovery and filesystem/allocation checks. This accepts the bounded observation
only; the original strict engine remains unchanged. Next observe the next fixed
reference RF bank table with this exact full central profile as a prerequisite.
Do not promote table command completion to calibrated radio acceptance or run
trim/DAC/VCO calibration automatically.


### Fixed RF bank5 observation (v67 native observation)

`--wifi-rf-bank5` requires the full central-table observation and all earlier
prerequisites. Admission validates the complete central616-operation proof,
including all command replies/guards,43 equal values and only index5 RF0:6=00.
This is an exact captured prerequisite, not acceptance of RF0:6 or a widened
strict-engine exception. The original strict engine and all earlier recipes
remain unchanged.

The fixed USB bank5 plan has68 entries from the pinned reference2G channel0
initialization table, including USB5:2=0c and the final5:127=04. It writes all68
in order, then reads all68. Each command has three preguards and three postguards,
so there are952 operations:816 guard reads and136 exchanges, with68 exact ACKs
and68 uint8 observations. Sequences begin7 after the central pass ends6, then
wrap1..15 only between completed exchanges. No caller-provided plan/address/value
or replay is admitted. Two mismatch fields cover indices0..63 and64..67 without
overshifting; every read contributes to equal/mismatch accounting.

Full central+bank5 receipts exceed128KiB. Only this explicit profile selects the
new256KiB startup capture. Ordinary64KiB and existing128KiB capture behavior is
preserved. Both growth points refuse an already discarded prefix; oversized or
truncated saved evidence cannot pass. The strict full-prefix fixtures are at
most159350bytes and retain all central and bank5 commands,448 guard triplets
and112 observed values. The larger window remains bounded and byte-verified.

Bank5 values are observations, not accepted register settings or radio function.
No bank6, RF0:22 trim, bandwidth/band selection, DAC/VCO calibration or packet
traffic follows this experiment automatically. New plan-specific wrappers share
the locally written two-pass mechanism; plan selection remains private and fixed.


Native MacBookPro11,2 source `8fbd311d`, release `242e84b15d20babc`, session
`session-00065ead199515d2-00` completed this fixed plan:952 operations/submissions,
816 guard reads,136 verified replies,68 ACKs,68 observations and68 matches.
All272 guard triplets passed; low/high mismatch fields zero. The central
prerequisite remained43/44, with only RF0:6=00. Both completion markers and the
complete159281-byte startup prefix were saved within262144 bytes. The host
collector now permits256KiB transport, preserving bytes at both prior limits
and the new boundary and refusing262145 bytes; collection never implies
hardware acceptance. Strict validation and recovery/offline filesystem checks
passed. This is native command/guard/readback evidence only; calibrated radio
operation and connectivity remain untested. Next is bounded reference bank6
initialization/readback, separate from trim, DAC/VCO and channel/traffic work.


### Fixed RF bank6 observation (v68 native observation)

`--wifi-rf-bank6` requires `--wifi-rf-bank5` and the entire earlier capture
chain. Native admission revalidates all central and bank5 command/guard proof:
central43/44 with only RF0:6=00, bank5 complete68/68 exact readbacks and zero
low/high mismatch fields. Neither source tables nor prior strict acceptance
are weakened. The fixed bank6 plan follows Linux v7.0
`mt76x0_rf_5g_channel_0_tab`:38 entries, beginning6:2=0c and ending6:65=0f.
This is initialization of the next reference bank, not channel selection.

The private plan uses the existing two-pass mechanism:38 writes then38 reads,
532 operations,456 guard reads,76 exact command replies,38 ACKs/observations
and152 guard triplets. Its first sequence is8 after bank5 ends7; sequences wrap
only between complete exchanges. Each command still checks MAC0, COREf000f200
and disabled/idle DMA before and after I/O. A prior-proof failure refuses all
new traffic; uncertain USB errors quarantine and never replay. Every uint8
readback contributes to the low38-bit mismatch field; the high field must be0.
Observed read differences are evidence, not automatic register acceptance.

The v68 recipe adds only `srvros.wifi-rf-bank6=1` to v67. Existing64KiB ordinary,
128KiB earlier capture and256KiB bank5 capture behavior remains unchanged;
v68 also uses the complete256KiB window. Saved proof must contain all prior
receipts, every new command/guard/value, exact byte count and both completion
markers. No VGA table, bandwidth/band switching, trim, DAC/VCO calibration,
scan/association/security or packet traffic follows this experiment.


Native MacBookPro11,2 source `00f04fb3`, release `7b329891c0606f1c`, session
`session-00065ead98009e52-00` completed the fixed bank6 plan:532 operations,
456 guard reads,76 verified command replies,38 ACKs and38 matching readbacks.
All152 guard triplets passed, both mismatch fields zero. Full central43/44
(RF0:6=00) and bank5 68/68 proof remained valid. Both completion markers and
the entire191063-byte startup prefix within262144 capacity were saved and
strictly verified. Ubuntu default, consumed one-shot, payload/recovery hashes
and offline filesystem/allocation checks passed. This establishes native
command/guard/readback observations only. Next is bounded reference VGA-table
initialization/readback; calibrated RF and connectivity remain untested.


### Fixed RF VGA observation (v69 native observation)

`--wifi-rf-vga` requires `--wifi-rf-bank6` and the full earlier initialization
chain. The native owner revalidates every completed bank6 command/guard and
all38 matching observations with zero mismatch fields, in addition to the
central zero-profile and exact bank5 proof. The original strict central
acceptance contract remains unchanged.

The fixed35-entry bank7 plan is the pinned Linux v7.0
`mt76x0_rf_vga_channel_0_tab`, beginning7:0=47 and ending7:74=00. It uses the
existing private two-pass mechanism:35 ordered writes then35 independent
reads,490 operations,420 guard reads,70 verified command replies,35 ACKs and
35 uint8 observations. All140 pre/post guard triplets require MAC0,
COREf000f200 and disabled/idle DMA. The first sequence is9 after bank6 ends8;
sequence wrap occurs only between complete exchanges. Prior-proof faults
refuse new I/O, uncertain transport quarantines and replay is refused.
All35 observations contribute to the low35-bit mismatch mask; high mask0.
Readback equality is recorded separately from calibrated radio acceptance.

The v69 recipe adds only `srvros.wifi-rf-vga=1` to v68, retaining the opt-in
256KiB complete startup window. The collector requires full earlier proof,
every new command/guard/value, exact prefix byte accounting and both completion
markers. Ordinary64KiB and older128KiB logging are unchanged. No bandwidth/band
selection, RF0:22 trim, DAC/VCO calibration, scan, association, security or packet
traffic follows the experiment automatically.


Native MacBookPro11,2 source `22f757bf`, release `de89daa6bdc3024e`, session
`session-00065eadd298077c-00` completed the fixed VGA plan:490 operations,
420 guard reads,70 verified command replies,35 ACKs and35 matching readbacks.
All140 guard triplets passed; both mismatch fields zero. Full central43/44
(RF0:6=00), bank5 68/68 and bank6 38/38 proof remained valid. Both completion
markers and the entire219878-byte startup prefix within262144 capacity were
saved and strictly verified. Ubuntu default, consumed one-shot, payload/recovery
hashes and offline filesystem/allocation checks passed. This establishes native
command/guard/readback observations only. Next is bounded reference bandwidth/
band initialization; frequency trim, DAC/VCO calibration, scanning and actual
Wi-Fi connectivity remain untested.


### Fixed default RF G20 observation (v70 native observation)

`--wifi-rf-g20` requires `--wifi-rf-vga` and the entire earlier initialization
chain. Native admission checks the full completed VGA command/guard proof and
all35 exact readbacks with zero masks, plus all earlier central/bank5/bank6
proof. The original strict central engine remains unchanged.

The reference `mt76x0_phy_rf_init` selects9 bandwidth entries for default20MHz
G-band and10 G-band entries from the two switch tables, in source order.
The private fixed19-target plan preserves that order across banks0/6/7, then
reads all19 targets separately. Existing single-bank plans retain their fixed
base/register representation; only the private plan gains an optional immutable
full-address table. No caller-provided plan, bank, address, count or value is
admitted. The selected plan is default RF initialization, not a channel-frequency
plan, regulatory acceptance or proof of a working tuned radio.

The new observation has266 operations/submissions,228 guard reads,38 exact
command replies,19 ACKs and19 uint8 observations, with76 pre/post guard
triplets. Each guard checks MAC0, COREf000f200 and disabled/idle DMA. The first
sequence is4 after VGA ends3, wrapping only between completed exchanges.
All19 observations contribute to the low19-bit mismatch mask; high mask0.
Prior-proof faults refuse new I/O; uncertain USB errors quarantine and never
replay. Readback equality stays separate from calibrated RF acceptance.

The v70 recipe adds only `srvros.wifi-rf-g20=1` to v69. It retains the existing
256KiB full-prefix window; ordinary64KiB and older128KiB logging are unchanged.
The collector requires all earlier proof, every new command/guard/value, exact
prefix byte count and both completion markers. No frequency-plan tuning, trim,
DAC/VCO calibration, scanning, association/security or packet traffic follows
this trial automatically.


Native MacBookPro11,2 source `b4be310b`, release `e75a52b56efa282b`, session
`session-00065eae3ac86feb-00` completed the fixed G20 plan:266 operations,
228 guard reads,38 verified command replies,19 ACKs and19 matching readbacks.
All76 guard triplets passed; both mismatch fields zero. Full central43/44
(RF0:6=00), bank5 68/68, bank6 38/38 and VGA35/35 proof remained valid.
Both completion markers and the entire235592-byte startup prefix within262144
capacity were saved and strictly verified. Ubuntu default, consumed one-shot,
payload/recovery hashes and offline filesystem/allocation checks passed.
This establishes native command/guard/readback observations only. Next is
captured-board frequency-trim auditing and the bounded reference DAC/VCO
sequence; calibrated RF, scanning and Wi-Fi connectivity remain untested.


### Fresh mapped board frequency trim (v71 native observation)

`--wifi-rf-trim` requires full G20 and all earlier proof, including19/19 exact
G20 readbacks. A fixed logical EFUSE read profile fetches only mapped blocks30
andd0, never block0/factory-address bytes and never programming mode. The
native gate checks every transfer, preserved command bits, mapped completion,
bounded polling, pre/post MAC/CORE guards and disabled DMA. It requires fresh
NICfd11/1004, base44 at3a and correction00 atdb. No fallback profile is admitted.
Local raw buffers clear on completion/failure; descriptor DMA remains retained
on uncertainty and is never logged. Diagnostics expose counters/profile success,
not raw EFUSE data. Existing physical-map/header/antenna profiles are unchanged.

Pinned [Linux v7.0 EEPROM decoding](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/eeprom.c)
and its [shared encoding helper](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_eeprom.h)
use bit7-positive signed magnitude for frequency correction and temperature,
not ordinary signed8. Frequency subtraction wraps through uint8 before the
[RF initialization](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c)
cap atbf. The offline auditor now separates this encoding from ordinary signed8
RSSI; boundary tests cover polarity, defaults, wrap and cap. This board still
decodes/caps to44 with zero correction. Its bytef8 temperature metadata decodes
+120 calibration units; earlier-8 reports are historical audit errors, not
accepted temperature calibration or a physical thermometer reading.

Only after the fresh profile succeeds does the fixed RF plan write0:22=44,
then independently read0:22. Two commands use sequences12/13 after G20 ends11:
14 operations,12 guard reads,2 verified command replies,1 ACK,1 observation and
4 guard triplets. An arbitrary uint8 observation is recorded with exact
one-bit mismatch accounting; it is not promoted to calibrated RF acceptance.
Full earlier proof, fresh-field status/count/guard checks, complete startup
prefix and both completion markers are required. v71 adds only the trim flag
to v70 and retains256KiB capture. DAC/VCO pulses, channel-frequency tuning,
scanning, association/security and traffic remain outside this trial.


Native release `4ec251f79c97532b` (source `b15867d7`) completed in session
`session-00065eaec03cebac-00`. Fresh board fields passed24 reads/2 writes/
2 polls/2 blocks/26 submissions without guard errors or raw data logging.
RF0:22 read68 after the fixed68 write; all14 operations,12 guard reads,
2 replies,1 ACK and4 guard triplets passed, both mismatch fields zero.
Full earlier proof and both completion markers passed in the entire236812-byte
startup prefix. Ubuntu default, consumed one-shot, payload/recovery hashes and
offline workspace checks passed;285files/681clusters/116directories, zero
leaks/mismatches/errors, unmounted and unarmed. Results are archived outside
build. Calibrated radio operation remains untested; bounded DAC/VCO work is next.


### Bounded DAC power-up pulses and VCO initiation (v72 native observation)

`--wifi-rf-cal` requires exact fresh-board trim and every earlier prerequisite.
The separate fixed engine follows the pinned Linux v7.0 RF-init order: read
RF0:73 and set bit7, read again and clear bit7, read again and set bit7; read
RF0:4 and set bit7 to initiate VCO calibration. Every write preserves the other
seven freshly read bits; no arbitrary register/value input is exposed. Two
final reads observe RF0:73 and RF0:4 without asserting a fixed VCO value or
calibration success. VCO bit7 self-clears in the reference description; a clear
or still-set final bit is recorded, with no polling, replay or forced clearing.

Ten commands use sequences14,15,1..8 following trim sequence13:70 operations,
60 guard reads,10 exact replies,4 ACKs,6 read observations and20 MAC/CORE/
disabled-DMA guard triplets. The native single-submission gate requires all
fresh fields and14-operation exact trim proof before any command. All prior
strict profiles remain unchanged. Opt-in v72 adds only its token to v71 and
retains256KiB complete-prefix logging and automatic save/return. This trial
adds no channel tuning, MAC enable, scanning, association/security or traffic;
calibrated radio operation remains untested.


Native release `d27f0d2f10e5ae0b` (source `cfc0cfdd`) completed in session
`session-00065eaf209e71b6-00`. All70 submissions,60 guard reads,10 replies,
4 ACKs,6 observations and20 guard triplets passed, including full earlier
proof. DAC RF0:73 followed93/13/93 with preserved lower bits and final93.
VCO RF0:4 initial30 was writtenb0 and still readb0 at the final observation;
self-clear was not observed. This establishes command/guard/observation
completion, not successful VCO calibration. Next is a bounded delayed read
observation without repeating the start pulse or forcing the bit clear.
The entire241416-byte prefix and both completion markers passed; recovery,
one-shot, hashes and clean offline filesystem/allocation checks verified.
Workspace unmounted, no reboot armed; evidence archived outside build.

### Delayed read-only VCO observation (v73 native observation)

`--wifi-rf-wait` requires the full captured v72 initiation proof, including
RF0:73 values93/93/93/13/13/93, RF0:4 initial30/writeb0 and final93/b0.
A separate fixed engine reads only RF0:4 six times, after inter-sample delays
10/40/50/100/300/500ms. Total requested delay is1000000us; USB/guard/logging
time is additional. Native delay submission is checked independently from
command submission, refuses wrong durations/order/replay, and uses the bounded
reference timer. A failed wait stops the attempt; it cannot be retried.

Commands use sequences9..14,42 operations,36 guard reads,6 exact read replies
and12 MAC/CORE/disabled-DMA guard triplets. No additional RF data write, pulse
or forced clear occurs. All six values and cumulative requested delays are
saved; first_clear is one-based or0 when never observed. Reassertion is recorded
without imposing monotonic state, and a clear bit alone does not certify radio
calibration. Full-prefix proof/counters/delay receipts/both completion markers
remain required. v73 adds only its opt-in token to v72, retains256KiB capture
and automatic save/return. No channel tuning, MAC enable, scanning or traffic.


Native release `cd775567c529231f` (source `70a3f45d`) completed in session
`session-00065eafa382c5ce-00`. All six RF0:4 reads returned30, first_clear=1,
final30: the VCO start bit cleared by the first delayed observation and stayed
clear at every later sample. Requested cumulative delays10/50/100/200/500/
1000ms exclude transport/guard/logging time; this is not an exact completion
latency measurement. All42 operations,36 guard reads,6 replies/delay receipts,
12 guard triplets, full prior proof and both completion markers passed in the
complete244743-byte prefix. Recovery/default/one-shot/hashes and offline clean
filesystem/allocation verified;289files/689clusters/118directories, no leaks/
mismatches/errors. Workspace unmounted, no reboot armed; evidence archived.
This accepts the bounded self-clear observation, not calibrated radio operation.
Next is remaining RX-path/TX-DAC and calibration/channel prerequisite auditing.

### One-chain RX-path/TX-DAC baseband setup (v74 native acceptance)

Pinned [Linux v7.0 one-chain setup](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_util.c)
uses chainmask101 for MT7610; fresh captured NICfd11 has one RX and TX stream.
The [RX-path/TX-DAC helpers](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_phy.c)
clear bits4/3 in AGC0 (2300) and bits1/0 in TXBE5 (2714) for this profile.
`--wifi-path` requires full prior proof and all six delayed VCO reads30,
first_clear1 and exact counters/delays/guards. It admits no sibling profile.

A separate fixed engine performs fresh read/modify/write/read at each target,
preserving every other bit. MAC0, COREf000f200 and disabled/idle DMA guard
triplets bracket each target. Eighteen USB transfers comprise16 reads,2 writes
and2 verified readbacks; all-ones input or any unexpected readback stops the
attempt. Every transfer is checked against the fixed operation, direction,
address, length, prior state and derived write bytes, with no replay after an
attempt. Diagnostics save18 receipts,4 guard triplets and2 before/after words.

v74 adds only its explicit token to v73 and retains256KiB complete-prefix
capture and automatic save/return. This setup does not enable the MAC or
establish calibrated radio operation. No RF data writes, channel tuning,
scanning, association/security or traffic are added by this phase.


Native release `0aaf20927414846c` (source `67e5e5af`) completed in session
`session-00065eb0051574ff-00`. AGC0 at2300 was21400 before/after clearing18;
TXBE5 at2714 was0 before/after clearing3. Required bits were already clear,
and both fixed writes/readbacks preserved all other bits. All18 USB receipts,
16 reads/2 writes/2 verified targets,4 guard triplets and full prior proof
passed. Entire247000-byte prefix and both completion markers verified.
Recovery/default/one-shot/hashes and clean offline filesystem/allocation checks
passed;291files/693clusters/119directories, no leaks/mismatches/errors.
Workspace unmounted, no reboot armed; evidence archived outside build.
Captured-unit baseband path setup is accepted; calibrated radio operation and
connectivity remain untested. Remaining calibration/channel prerequisites are
next to audit before further hardware changes.

### Channel/calibration dependency audit and read-only inventory (v75 failed capture)

The pinned [USB start sequence](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/usb.c)
starts the MAC before power-on calibration. The
[calibration function](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c)
uses channel context, timed firmware commands, TX ALC/IBI9 save/restore and
optional TSSI MAC transitions. This is separate from the accepted bounded VCO
start-bit self-clear observation. Do not transplant isolated calibration calls
into the current MAC-disabled sequence or equate MCU ACKs with calibrated RF.

| Remaining dependency | Evidence/next action |
| --- | --- |
| USB tail timing/beacon state | Read US_CYC2a4, TXOP1340 and BCN_TIME1114; TXOP583f was established earlier, US_CYC low8=1e is a reference tail requirement |
| Baseband bandwidth/band/CCA | Read CORE1, AGC0, TXBE0, TX_BAND, EXT_CCA and TX_SW before a fixed20MHz channel plan |
| Calibration save/restore | Inventory IBI9/TX_ALC; define bounded restoration and failure handling before command execution |
| Channel synthesizer/gain/power | Fixed reference tables and fresh EEPROM profile/validity still need admission; default G-band initialization is not channel tuning |
| Radio enable/calibration | Deferred until prerequisites and controlled lifecycle are defined; central RF0:6 semantics remain unresolved |
| Receive/scan/networking | USB receive framing and management parser, controlled US channel handling, then association/security and traffic remain pending |

The user's United States location is recorded; it does not establish EEPROM
power validity or authorize guessed regulatory limits. `--wifi-channel-state`
adds a fixed read-only inventory after exact accepted path2300=21400/2714=0
proof. Targets in order:2a4,1340,2004,2300,2700,132c,141c,1330,2124,13b0,
1114,238. Pre/post MAC0/COREf000f200/disabled-DMA triplets make18 total reads;
all-ones or transfer uncertainty stops the attempt. No target value is accepted
merely because it was read. The collector requires18 receipts,12 indexed target
observations,2 guard triplets, full earlier proof and both completion markers.
v75 adds only its explicit token to v74;256KiB complete logging and automatic
save/return remain. No additional RF write, tuning, MAC enable or traffic.


Native release `80a6781c0b08290c` (source `4dd830fd`) failed in session
`session-00065eb0816b56d4-00`. IBI9 at2124 returnedffffffff with a successful
USB receipt; the engine refused after12 reads (three initial guards plus nine
target reads). Eight earlier target values were observed:2a4=0,1340=583f,
2004=2,2300=21400,2700=0,132c=4,141c=ffe4,1330=601. Targets13b0/1114/238
and final guards were NOT_READ; printed zero tail rows are placeholders. No
inventory/settings acceptance follows. Original success validation refused;
separate failure validation preserved full earlier proof, exact stop and entire
249442-byte prefix with both completed=NO markers. Recovery, consumed one-shot,
hashes and clean offline filesystem/allocation verified; workspace unmounted,
no reboot armed. The next audit must establish IBI9 all-ones semantics before
changing admission; no gate relaxation or automatic retry was made.

### IBI9 comparison and separate raw observation (v76 candidate)

On the same captured adapter under Ubuntu, four interleaved debugfs samples
returned IBI9=ffffffff twelve times, alongside ASIC76100002, COREf000f200,
adjacent IBI8/10=0 and normal register values. Only the debug read selector was
changed/restored; no hardware register write, driver reset/unbind or channel
change occurred, and binding/default route remained unchanged. Linux is already
initialized/calibrated here, so these values are not native initialization
acceptance. The [reference calibration](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c)
saves IBI9, temporarily writesffffff7e and restores the saved value. All ones at
this address alone is insufficient evidence of a failed USB transfer.

The v75 strict profile and its recorded failure remain unchanged. New explicit
`--wifi-channel-observe` selects a separate read-only policy/log family. It may
record all-ones only at IBI9 index8 and emits a derived12-bit all_ones_mask;
only bit8 can appear in a completed observation. Other11 targets and all MAC/
CORE/DMA guards retain strict rejection. Counters, policy flag and native mode
must agree before any read; the original admission function refuses raw mode.
No value is promoted to accepted calibration data. Complete18 receipts,12
values,2 guard triplets and both completion markers remain required. v76 adds
only its token to v75 and keeps256KiB save/return. No additional writes or radio
enable/traffic. The independent Linux read/audit is archived outside build.


### Early VCO-clear timing variant (v77 candidate)

The native v76 run saved its failure before reaching the new inventory:
initiation completed70 operations with final RF0:4=30 (bit7 already cleared),
but the historical pending profile requiredb0 and refused delayed reads.
No wait/path/inventory ran; zero wait rows are placeholders. Earlier proof,
both failure markers, complete prefix and recovery/filesystem checks verified.

Separate `--wifi-vco-ready` requires `--wifi-channel-observe` and emits its own
boot token. Only native mode55 admits the exact captured early-clear variant:
all ten initiation values/headers/guards/counters remain strict, with final30
instead ofb0. The historical pending predicate and modes remain unchanged.
A prerequisite receipt distinguishes pending from early-clear. Either variant
still executes all six fixed delayed RF0:4 reads; all six must observe30 before
path/inventory admission. No skipped waits, retries, repeated pulses, new RF
writes, calibrated radio acceptance, MAC enable or traffic. Full earlier proof,
18 inventory receipts/postguards, IBI9-only raw mask and both saved completion
markers remain required; native validation is pending.


### v77 native observation result

On MacBookPro11,2, source `f5549279`, release `b2369fab9ef32f11`, session
`session-00065eb1475df12a-00` completed with the early-clear variant. Initiation
final RF0:4=30 and pending0/early_clear1 receipt passed. All six delayed reads
remained30 before path/inventory admission; full earlier proof passed.
All18 inventory USB reads and both MAC0/COREf000f200/DMA40000050 guard triplets
passed, with zero inventory writes. IBI9=ffffffff yields exact mask100; the
remaining11 targets were non-all-ones. Newly reached targets:13b0=2f2f1b1b,
1114=640 and238=c00020. US_CYC2a4=0 remains a pending timing prerequisite;
TXOP1340=583f. This accepts complete transport/guard observation, not target
register semantics, calibrated radio operation or connectivity. Full250364-byte
prefix, both completion markers, recovery/one-shot/hashes and offline clean
filesystem/allocation checks passed; evidence archived and no reboot armed.
Original strict modes and failed v75/v76 evidence remain unchanged.


### USB microsecond-cycle setup (v78 candidate)

After v77's complete disabled-MAC inventory,02a4 read0 while1340 already
matched583f. The pinned Linux USB initialization sets
[US_CYC_CNT low8 at02a4](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_regs.h)
to1e after hardware/beacon initialization. Separate `--wifi-usb-cycle` requires
the complete early-clear-aware inventory and selects native mode56. It requires
the exact12 captured inventory values/mask and both guards before any timing
transfer. Only then: three guard reads, fresh02a4 read, one masked write setting
low8=1e while preserving upper24, independent readback and three final guards.
Nine transfers/eight reads/one write/one verification; no retries. Native write
refuses if the fresh value differs from captured0. All-ones, failed transfer,
changed guard or readback mismatch stops the phase. MAC remains disabled;
no USB DMA change, RF write, channel tuning, scan or traffic. Original v77 and
older recipes/policies remain unchanged; native acceptance pending. Full saved
prefix/both completion markers and recovery/offline checks remain required.


### v78 native USB timing result

MacBookPro11,2 release `5c86ad12e5f185ca` (source `da8b7fce`), session
`session-00065eb17b66ef77-00`, completed the exact inventory then fixed02a4
setup: fresh0, masked low8 write1e, independent readback1e. All9 ordered USB
transfers,8 reads/1 write/1 verification and2 complete MAC0/COREf000f200/
DMA40000050 guard triplets passed. Upper24 preserved0. Full earlier proof,
251629-byte startup prefix, both success markers, recovery/one-shot/hashes and
offline clean filesystem/allocation checks passed; archived, unmounted/unarmed.
This run's initiation was pending (finalb0), with all six delayed reads30;
v77 previously exercised early-clear. Both observed timing variants retain the
same strict downstream proof. Captured USB timing setup is accepted; no channel
configuration, calibrated radio operation, MAC enable or traffic is accepted.


### USB firmware bandwidth response observation (v79 candidate)

Pinned `mt76x0_phy_set_channel` calls USB `mt76x0_phy_bbp_set_bw` before
baseband bandwidth/control-channel and band/CCA setup, RF band/synthesizer,
gain/power and context-dependent calibration. The 20MHz USB command is
CMD_FUN_SET_OP1, BW_SETTING2,value0 with a reply requested. This step selects
bandwidth only; it does not establish a channel or calibrated radio operation.

Separate `--wifi-bw-observe` requires complete v78 USB timing proof and selects
native mode57. Exactly three MAC/CORE/disabled-DMA reads, one fixed sequence15
command and three final guard reads execute. Exact16-byte framing header4a0f0008
is required; the other three uint32 reply words are recorded as opaque data.
`ack_validated=0` remains explicit: framing/sequence and transport completion
are not semantic firmware acknowledgment. No later BBP/RF/channel programming,
MAC enable or traffic follows. Every command byte is fixed, no caller-selected
payload or automatic retry. Full earlier inventory/timing proof, seven operation
receipts/two guard triplets, both completion markers and whole saved prefix
remain required. Native response semantics and bandwidth acceptance pending.


### v79 native bandwidth response evidence

MacBookPro11,2 release `4c8563c12db92578` (source `83a869fc`), session
`session-00065eb1d61ee32d-00`, completed full prior initialization/inventory/
timing proof, then the fixed bandwidth command. Sequence15 sent16/received16,
cc13/residue1008; response4a0f0008/00000002/00000000/4a0f0008. This echoes
function ID2/value0 and the framing word. All7 operations,6 guard reads and both
MAC0/COREf000f200/DMA40000050 triplets passed. Whole252903-byte prefix/both
success markers and recovery/one-shot/hashes/offline checks passed; archived,
unmounted/unarmed. The collector deliberately records ack_validated0 and does
not accept actual bandwidth or radio functionality. Next implementation may
pin this captured response as a prerequisite with tests before the next bounded
baseband phase; no follow-on BBP/RF writes occurred in this observation trial.


### Guarded20MHz baseband layout (v80 candidate)

Separate `--wifi-bbp20` requires v79's fixed bandwidth response path and adds
an exact captured response predicate: complete7-operation proof, all guards,
words4a0f0008/2/0/4a0f0008. Original framing-only observation remains unchanged;
new mode58 emits `wifi-bw-validated: exact_profile=1` before any baseband transfer.
The fixed layout groups only adjacent same-register reference field updates;
there is no intervening command or delay. MAC/DMA remain disabled.

| Register | Clear mask | Set bits | Captured before | Required after |
| --- | --- | --- | --- | --- |
| CORE1 /2004 | 18 | 0 | 2 | 2 |
| AGC0 /2300 | 7300 | 1000 | 21400 | 21400 |
| TXBE0 /2700 | 3 | 0 | 0 | 0 |
| TX_BAND /132c | 7 | 4 | 4 | 4 |
| EXT_CCA /141c | fff | 1e4 | ffe4 | f1e4 |

Each target uses three guards, fresh read, one masked write, separate readback
and three final guards.45 ordered transfers,40 reads/5 writes/5 verified,
10 complete strict guard triplets. Every native fresh value must match the
captured profile before its write; all unmasked bits are preserved. The CCA
ED_CCA upper nibblef remains unchanged. The first four values already have the
requested layout; only EXT_CCA is expected to change. No RF frequency selection,
calibration, MAC enable, scan or traffic. Whole258463-byte fixture fits262144
capacity, with strict full-prefix/both-marker checks; native validation pending.


### v80 native baseband layout result

MacBookPro11,2 release `6a9fd32642e92a7b` (source `a89af59a`), session
`session-00065eb25d7baa54-00`, passed the whole prior proof and exact captured
bandwidth response predicate. All45 transfers,40 reads/5 writes/5 verified and
10 MAC0/COREf000f200/DMA40000050 guard triplets passed. Masked readbacks were
2004:2,2300:21400,2700:0,132c:4,141c:f1e4. Only EXT_CCA changed fromffe4;
ED_CCA upper nibblef and all unmasked bits preserved. Whole258464-byte prefix,
both completion markers and recovery/one-shot/hashes/offline checks passed;
archived, unmounted/unarmed. Captured20MHz baseband layout is accepted; RF
frequency/channel selection, calibrated radio operation and connectivity are
not accepted. Original v79 framing-only observation remains unchanged. The local
return collector's boot-module variable collision was corrected without changing
proof checks or native log bytes, with namespace-preservation regression coverage.


### Batched channel prerequisites with full capture (v81)

The accepted v80 prefix is258464bytes, leaving only3680 in256KiB. New explicit
`--wifi-channel-probe` requires full v80 proof and selects512KiB complete-prefix
logging before diagnostics start. Ordinary64KiB and historical128/256KiB capture
selection remains unchanged. Prefix loss still refuses extension/save; no ring
wrap is reclassified as complete. Ubuntu transport collection is bounded512KiB
per log and preserves every byte; full proof/physical acceptance remain separate.

The new phase is read-only: fixed MAC13c8/13a0/13a8/13b4/518 observations,
then RF5:0,6:0,0:24,0:26..37. The MAC group has two guard triplets; each RF
read has two more.116 ordered operations,101 reads (96 guards+5 MAC),15 CMD10
reads at sequence1..15,32 complete MAC0/COREf000f200/DMA40000050 triplets.
MAC targets reject all-ones; RF responses require exact16-byte header/sequence/
address/uint8/trailer. No caller-selected address or RF data write. All indexed
observations and ordered receipts, complete512KiB prefix and both markers are
required. Values remain observations, not RF readiness. No frequency choice,
calibration, MAC enable, scan or traffic. The collected values will ground the
next channel-band/synthesizer and fresh gain/power plan, rather than guessing
uncaptured initial register state. Native result follows.


### v81 native prerequisite capture result

MacBookPro11,2 release `dde8fcb03f3ea0d5` (source `74d1599e`), session
`session-00065eb2f077e6da-00`, completed all116 operations:101 USB reads,
15 exact CMD10 replies and32 strict MAC0/COREf000f200/DMA40000050 guard
triplets. All prior initialization and exact BBP20/bandwidth proof passed.
Fresh MAC values (hex) were13c8=0,13a0=0,13a8=6c6c6c6c,13b4=c9540000,
518=1. RF5:0 and6:0 were0; RF0:24 was33(hex); RF0:26..37 were0.
No new data writes occurred. These values ground subsequent channel-band and
synthesizer work; they do not accept RF readiness or calibrated operation.

The full274888-byte prefix exceeded256KiB and was preserved within explicit
512KiB capture, with both completion markers. Strict ordered receipts,
indexed observations, Ubuntu default/consumed one-shot, installed/recovery
hashes and offline workspace checks passed.305files/722clusters/126directories,
zero leaks/mismatches/errors; evidence archived, unmounted/unarmed. Next scope:
bounded channel-band/synthesizer setup from the pinned reference and fresh
values. Scanning, association/security, connectivity and Wi-Fi UI remain pending.


### Fixed 2GHz band preparation (v82 candidate)

After exact v81 observations, follow pinned Linux v7.0
`mt76x0_phy_set_band(NL80211_BAND_2GHZ)` order: replay the68-entry2G bank5
table, write RF5:0=45 and6:0=44(hex), then MAC13c8=00050007 and13a0=003e0002.
The70-target RF plan performs all writes then separate reads, each command
surrounded by disabled-MAC/CORE/disabled-DMA triplets.980 ordered operations,
840 guard reads,140 exact16-byte replies,70 ACKs,70 independent observations,
280 guard triplets and complete low64/high6 mismatch accounting. Sequences
start1 after v81 ends15. Only exact70 matching readbacks admit MAC setup.

The two MAC full-word writes require fresh0 at each captured target and
independent readback:18 ordered transfers,16 reads,2 writes,2 verified and4
more guard triplets. Both native callbacks and the bounded engine reject a
changed fresh value before write. No caller-selected targets, plan, payload or
retry. The v81 prerequisite predicate checks its entire116-operation proof,
all32 guards, five captured MAC values and15 exact captured RF values.
Existing v81 observation behavior and all162 historical recipes are unchanged.

Directed host checks cover every new I/O/cancellation/guard boundary, reply
bits/lengths, both mismatch masks, completed-proof mutations and replay refusal.
Native admission tests cover980 RF and18 MAC positions,162 earlier ownership/
state faults, command packet bits and quarantine on failure. The full collector
fixture is402659bytes;4247 missing/duplicate/altered/incomplete proofs refuse,
with whole earlier-chain verification and boot-module namespace preservation.
Explicit512KiB capture remains mandatory. No synthesizer frequency, transmit
power selection, MAC enable, scanning or traffic; physical acceptance pending.


### v82 native 2GHz band setup/readback result

MacBookPro11,2 release `7e269653eebe03c3` (source `83edd6be`), session
`session-00065eb38500e72c-00`, passed all70 RF readbacks:68 bank5 replay
entries, RF5:0=45 and6:0=44(hex).980 ordered operations,840 guard reads,
140 exact replies,70 ACKs,70 matches and280 strict disabled-MAC guard
triplets passed; both mismatch masks zero. The subsequent MAC writes from
fresh0 independently read back13c8=00050007 and13a0=003e0002:18 ordered
transfers,16 reads,2 writes,2 verified and4 guard triplets. Whole prior proof
and complete402661-byte prefix/both completion markers passed.

Ubuntu default, consumed one-shot and current/recovery hashes verified;
offline workspace clean307files/728clusters/127directories with zero leaks,
mismatches or errors. Evidence archived, unmounted/unarmed. Bounded band
setup/readbacks passed on the captured adapter; synthesizer/channel tuning,
calibrated radio operation, scanning and Wi-Fi connectivity remain untested.


### Fixed channel1 synthesizer (v83 candidate)

The next explicit diagnostic applies only the pinned Linux v7.0 channel1
(2412MHz) frequency row, mapped through the declared `mt76x0_freq_item` fields
and masks. Preserve all21 reference steps: RF0:37/36/35/34/33 full writes;
32[e0,1f],31[e0,1f],30[80,7c,02],29 full,30[01],28[c0,30,0c],26/27 full,
28[03],24[03]. There is no SDM reset pulse for this row. Full masks areff.
No grouped field shortcut, caller-selected frequency or arbitrary register plan.

Each step reads its fixed target fresh, requires the captured profile at that
point, performs one full/masked write preserving other bits, then reads back
independently. Initial RF0:24 is33(hex);26..37 are0 before this phase. Later
fresh values include earlier accepted steps in the same sequence. Writes use
fresh read words, with exact masked payload validation before USB. All63 MCU
commands have strict MAC0/COREf000f200/DMA40000050 triplets before/after:
441 ordered operations,378 guard reads,63 exact replies,21 ACKs,42 observations
and126 triplets. Sequences start6 after the v82 band plan ends5, finish8.

Require whole exact v82 band/MAC proof and all prior initialization. Both
completion markers, all ordered receipts and21 indexed mask/bits/before/after
observations must fit the complete512KiB prefix. A synthetic whole-chain
fixture is458428bytes and1993 malformed proofs refuse. Source table/struct/
mask mapping and fresh-value replay independently compared to pinned reference.
Directed engine checks cover every I/O/cancellation/guard/reply-bit/length
boundary,5376 masked byte inputs, completed-proof mutations and no replay.
Native/VM checks follow before installation; physical result pending.
No VCO trigger, transmit-power selection, bandwidth/band tail, MAC enable,
scanning or traffic. Frequency-field readbacks do not establish RF readiness.


### v83 native channel1 synthesizer field/readback result

MacBookPro11,2 release `be37d545e93f1aed` (source `0ee9efb3`), session
`session-00065eb3c5cb1785-00`, passed all21 fixed fresh-read/write/readback
steps:441 ordered operations,378 guard reads,63 exact replies,21 ACKs,
42 observations and126 strict disabled-MAC guard triplets. Final RF bank0
values (hex):37=02,36=3f,35=28,34=dd,33=e2,32=42,31=42,30=02,29=28,28=30,
26=00,27=00,24=33. Unmasked bits preserved; no VCO/SDM pulse. Whole prior
proof and complete458424-byte prefix/both completion markers passed.

Ubuntu default, consumed one-shot and installed/recovery hashes verified;
offline clean309files/734clusters/128directories, zero leaks/mismatches/errors.
Evidence archived, unmounted/unarmed. Bounded field setup/readbacks passed;
PLL lock, calibrated radio and Wi-Fi connectivity remain untested. Next scope:
post-tune bandwidth/band tables and fresh PA/gain prerequisites before VCO or
MAC enable.


### Post-tune20MHz/G-band and internal-PA MAC tail (v84 candidate)

After full exact v83 synthesizer proof, replay the reference-selected19 RF
bandwidth/band entries in source order (nine20MHz then tenG-band). This is a
separate private plan with sequences9..1 after synth1 ends8; earlier G20
initialization and its predicate remain unchanged.266 operations,228 guard
reads,38 exact replies,19 ACKs,19 independent observations and76 strict
MAC0/COREf000f200/DMA40000050 triplets. Only19 exact matching readbacks with
zero mismatch masks admit the MAC tail.

Require complete same-boot mapped board-field proof (`fd11`/`1004`). The
pinned reference interprets NIC0 bit8 as internal2G PA; this profile sets it.
No external-PA branch or inferred transmit power. A fixed marker records this
admission without raw fields. MAC tail requires fresh518=1,13a8=6c6c6c6c,
13b4=c9540000(hex), then clears518 bits0c, writes13a8=63707400 and masks
13b4 with896400ff. Independent readbacks must be1,63707400,89440000.
27 ordered transfers,24 reads,3 writes/verified and6 more guard triplets.
Unmasked bits preserved; foreign fresh values refuse before write.

Independent source-table selection/order, PA interpretation and MAC masks
checked against pinned Linux v7.0. Directed engine and native admission tests
cover266 RF and27 MAC positions,196 ownership/state/PA faults, all command
bits, no USB on refusal, no replay and failure quarantine. Whole-chain fixture
495282bytes fits explicit512KiB;1380 malformed proofs refuse, with namespace
preservation.163 staging tests and166 historical recipes checked unchanged.
Build/sanitizer/VM/install checks follow; physical acceptance pending. No VCO,
calibration, MAC enable, scanning or traffic; no calibrated-radio acceptance.


### v84 native post-tune bandwidth/band and gain result

MacBookPro11,2 release `e57afcf9eb1fab7e` (source `f67ee1be`), session
`session-00065eb40f2ab69b-00`, matched all19 RF readbacks:266 operations,
228 guard reads,38 exact replies,19 ACKs,76 strict guard triplets and zero
mismatch fields. Same-boot internal2G PA admission passed. Three MAC tails
verified518 1->1,13a8 6c6c6c6c->63707400,13b4 c9540000->89440000(hex):
27 ordered transfers,24 reads,3 writes/verified and6 strict guard triplets;
all unmasked bits preserved. Whole earlier proof and complete495284-byte
prefix/both completion markers passed.

Ubuntu default, consumed one-shot and installed/recovery hashes verified;
offline clean311files/740clusters/129directories, zero leaks/mismatches/errors.
Evidence archived, unmounted/unarmed. Bounded post-tune setup/readbacks passed;
frequency lock, calibrated radio and Wi-Fi connectivity remain untested.
Next: channel-specific baseband/filter/gain prerequisites before VCO/calibration
or MAC enable.512KiB capture now has only29004 bytes of headroom; any next
candidate must budget its entire prefix before staging.


### Mapped RX gain and channel-BBP prerequisites (v85 candidate)

The reference channel-BBP table adjusts AGC8 using board LNA gain. Before
writing that adjusted table, a new explicit mode captures logical block40 in
read mode0, decodes only byte44 LNA and signed46/47 RSSI offsets, and scrubs
the raw16-byte block on success/failure. One fixed EFUSE read-select write is
required; programming modes, arbitrary addresses and raw-block logging refuse.
This software profile4 is separate from unchanged map/header/antenna/trim
profiles. Fixed ASIC/MAC and whole exact v84 ownership proof remain required.

LNAff maps to0 per pinned reference; RSSI outside[-10,10] clamps to0 with
explicit mask. AGC8 observation follows reference uint8 subtraction and7-bit
field placement from16344ef0. Outputs are observations, not permission to
write a guessed/default board profile. Capture requires one block, one select,
1..1024 polls,reads11+polls/submitted12+polls,complete phase15, strict MAC/
CORE guards andDMA40000050. All ordered nonraw receipts and busy/settle delays
are checked; raw control/data payloads are not logged.

Then CORE1 plus20 fixed channel-BBP targets are read between strict guard
triplets:27 ordered reads,zero data writes,21 indexed non-all-ones observations.
No filter/BBP/RF/MAC data write,VCO,MAC enable,scan or traffic. Host tests cover
all transfer/cancel/timeout/unmapped/command-bit boundaries, raw-output clearing,
65536 decoder inputs, late-state refusal and no replay; existing EFUSE profiles
remain strict. Native admission covers13 read-select phases/27 inventory
positions/216 ownership/state/profile faults and quarantine. Full synthetic
prefix501863bytes fits512KiB;450 malformed proofs and an unknown raw-output
family refuse, while four alternative valid LNA observations pass.164 staging
tests and168 historical recipes unchanged. Native observations pending.


### v85 native RX gain and baseband prerequisite result

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


### Channel1 baseband/filter setup (v86 candidate)

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


### v86 installation checkpoint

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


### v86 native channel1 BBP/filter result

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


### Post-channel1 VCO initiation and delayed observations (v87 candidate)

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


### v87 installation checkpoint

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


### v87 native post-channel1 VCO result

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


### Main refresh and next AGC prerequisite

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


### Merged-main v87 regression installation

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


### Native merged-main v87 regression result

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


### Fresh AGC8/9 capture and software state (v88 candidate)

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


### v88 installation checkpoint

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


### Passive receive decoder groundwork (2026-10-26)

The next radio-start lifecycle needs a bounded consumer for received bytes.
`mt76_rx_usb_view` decodes one completed main-RX endpoint transfer with byte
loads:4-byte DMA header,32-byte RXWI,14-bit MPDU length,24-byte management
header and optional2-byte L2 padding. It bounds every view within the declared
entry and reports consumed bytes; trailing USB bytes are not interpreted as
another frame. CRC/ICV/MIC errors,fragments,PN/decrypted processing,aggregates,
non-management and variable-header forms are refused. RSSI remains raw; rate
is retained as metadata. This does not validate FCE/FCS semantics or provide
transport scheduling/reassembly.

`mt76_rx_beacon` copies infrastructure beacon/probe-response observations only
after validating the entire information-element stream. It preserves binary
SSID bytes/length (including hidden/zero SSIDs),BSSID/transmitter,timestamp,
sequence,interval,capabilities and raw RSSI. Duplicate mandatory SSID/rates/DS
and duplicate RSN/WPA advertisements refuse; malformed lengths refuse without
publishing partial data. RSN/WPA booleans describe advertised element presence,
not validated security suites or connectability. Caller supplies a tuned US
channel1..11; an explicit DS channel must match it. Missing DS retains that
caller context; the packet never authorizes a channel or radio transmission.

Framing was audited against pinned Linux v7.0
[USB entry bounds](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/usb.c),
[RXWI fields](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_mac.h),
[RX padding/MPDU trim](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_mac.c)
and [main/MCU queue separation](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_txrx.c).
It is separate from MCU-response decoding and currently has no native caller.
Synthetic tests are not physical receive/scanning acceptance. Last installed
v88 gain-state payload/recovery remain unchanged; no reboot is armed.

Calibration audit: pinned USB `mt76x0u_start` starts MAC before power-on
calibration; FULL/LC temporarily overwrite TX_ALC_CFG0 and IBI9 and restore
both,then RXDCOC follows. Power-on adds R/VCO and conditional TSSI calibration,
which itself uses additional MAC/baseband/RF transitions. Future native work
must own receive DMA,stop/restoration and fresh board/power/TSSI context; do
not issue isolated FULL under the current disabled-MAC guard or reinterpret
prior IBI9=ffffffff as calibrated-radio acceptance. Next block is controlled
MAC/RX lifecycle and calibration admission,not association or automatic TX.


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

### Retained main-RX endpoint lifecycle (v89 candidate)

After whole v88 proof and exact captured AGC9=0/gain1=0, explicit
`--wifi-rx-lifecycle` configures only captured packet endpoint84/DCI9/512-byte
bulk-IN, separate from MCU85/DCI11 and command08/DCI16. Three fixed
MAC0/COREf000f200/DMA40000050 reads precede configuration; three follow stop.
One4KiB IN is posted,with a bounded10ms wait (or earlier completion),then one
exact-pointer/slot Stop Endpoint command15. Success requires output context
state3 and current/next owned TRB dequeue with DCS1. No Set Dequeue,rearm,retry,
DMA free,radio register write,MAC enable,calibration or transmit occurs. Ring
and buffer survive all outcomes until controller/host reset.

Matching ordinary completions accept onlyCC1/13 and bounded residue; stopped
CC26/27/28 are accepted as cancellation metadata only during Stop. CC27 length
is explicitly unusable and never converted into packet bytes. A successful
Stop plus stopped context can quiesce without a Transfer Event. Wrong endpoint,
slot,pointer and stale completions cannot retire another request. Late exact
completions after retirement are absorbed without rearm. Failed/uncertain
configuration or stop retains DMA and marks failure. Cleanup remains available
if earlier RF/software admission is revoked after the post.

The guest prints progress and saves ordered guard/config/post/stop/summary
receipts within the inherited1MiB full-prefix policy,then returns automatically.
Payload/SSID bytes are never logged or delivered. Even unexpected normal data
completion is not packet-reception,calibration or scanning acceptance. This
trial proves bounded endpoint ownership/retirement before later MAC/RX work.
Pinned [Linux Stop Endpoint handling](https://github.com/torvalds/linux/blob/v7.0/drivers/usb/host/xhci-ring.c)
was audited alongside the captured descriptor. Existing firmware/RF routines
and the original strict predicates retain their behavior; v1-v88 recipe bytes
remain unchanged. Native lifecycle acceptance is pending.


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


### 2026-10-26 — bounded repeated receive engine (offline)

Following native v93 beacon reception, main `a9d69554` was merged (PR199 adds
Air6,1 evidence only). PR182's filesystem scope does not overlap. New
`mt76_rx_window` is an allocation-free callback-driven engine for a fixed
channel1 receive window. The caller supplies eight distinct aligned4KiB DMA
pages and eight monotonically advancing, non-wrapping TRB identities. Only one
request can be pending. Normal CC1/13 with residue<=4096 records actual bytes;
foreign slot/DCI/TRB events are ignored, and prior TRBs cannot complete a later
request. Stopped/error/event-data completions fail the forward transaction.
Every attempted submission, including a callback failure, requires endpoint
cessation before any subsequent inspection; buffers are never freed or reused.

The count limit is eight; the window deadline is1s, with a1.1s elapsed accounting
cap, backward-clock refusal,1024 consecutive no-progress checks and a2M poll
cap. Eight immediate completions can finish by count without a clock advance;
a stalled pending request cannot spin indefinitely. Adapter callbacks themselves
must be bounded and serialized. The future native adapter must prove stable
DMA/ring/endpoint identities and the accepted MAC/calibration context before
each forward action, then disable/drain MAC and prove disabled guards in cleanup.
The helper requires exact slot/DCI9/stopped-state3/current-or-next dequeue with
DCS1 before decoding. All pages remain retained even if shutdown cannot be proven.

Only after final cessation and fresh cleanup ownership does the existing strict
parser inspect the first entry of each normally completed buffer. Zero-data
normal completions never dereference their buffers. Only non-identifying sizes,
frame-control and beacon advertisement metadata are copied; temporary identifiers
are scrubbed. No aggregate traversal, SSID/address logging, raw RSSI/FCS inference,
transmission, scan, association or networking policy is added. A complete empty
window is not reception acceptance; any failed window remains unaccepted even
if earlier items have partial metadata.

The helper has no native caller, staging flag or boot recipe in this block.
Installed v93 and recovery are unchanged; repeated native reception is not yet
accepted. Next is the native callback adapter, with actual event/ring/stop fault
fixtures and exact-stage save/refusal/return validation before a cold trial.


Validation for this offline engine:222 host scenarios and ASan/UBSan with leak
checking pass, including each submission/ownership/cleanup failure position,
foreign/stale events, zero/full transfers, malformed/unsupported/off-channel
frames, stopped completions with misleading residues, exact-stop mutations,
invalid plans/callbacks and poison DMA pointers on all no-usable-data paths.
The first frozen-clock fixture incorrectly supplied immediate completions; it
was corrected to withhold events and exercise a pending stall. That initial
failed run is retained, not counted as passing evidence. Full merged image/core,
final-source image/GPT USB build,180 staging tests, documentation checks and
OVMF GPT boot with real emulated USB keyboard input pass. These validate the
helper and boot workflow, not repeated reception on physical hardware. The
native adapter must preserve shared-event dispatch and the parent MAC cleanup
sequence; it must not stop the endpoint prematurely inside the existing
inspection hook. Installed v93, e47 recovery and accepted v92 fallback remain
unchanged; no reboot armed.


### 2026-10-26 — native repeated passive RX candidate (v94)

New selector72/v94 consumes the whole accepted prior firmware, board, channel,
AGC, reversible-probe and five-command power-on sequence. The receive-window
engine now has one-shot poll/finish phases, retaining its standalone wrapper.
Native polling runs inside the existing MAC inspection hook; the original MAC
transaction remains byte-identical. Final decoding occurs only after its parent
hold/drain/disable/filter/TXOP restoration, exact endpoint stop and all six
boundary guards. No endpoint stop or packet inspection occurs inside the active
MAC hook. Original receive parser and both older calibration helpers are unchanged.

Configure allocates a ring plus eight distinct retained4KiB pages before MAC
startup. The first poll submission acknowledges the already posted first IN;
up to seven later requests publish successive non-wrapping TRBs, one pending
at a time. Every posted descriptor must still encode its exact saved physical
page,4096-byte length, normal/IOC/ISP/Cycle bits, while future entries remain
unpublished. Current virtual mappings, enqueue/cycle, buffer, slot/DCI and all
saved page identities are rechecked. Publication and shared-event dispatch use
the existing IRQ-protected controller path. Neither buffers nor TRB identities
are reused, and no Set Dequeue or network data OUT is issued. All DMA remains
retained after failure or uncertain stop.

A normal CC1/13 event with bounded residue supplies bytes only when observed
by the polling phase. An initial completion processed before polling is tagged
early. A final completion arriving during the later parent hold or Stop does
not retroactively populate the saved item; cancellation residues are never
parsed. The1s/eight-request bounds,1.1s accounting cap,1024 frozen-clock bound and
2M absolute poll limit remain. Packet metadata contains no SSID, addresses or
raw payload/RSSI; only the first bounded entry of each observed transfer is
viewed. Empty/rejected windows can complete without accepting reception.
Scanning, association/security, Wi-Fi IP and UI remain outside this trial.

Native fixtures cover the actual selector, whole power-on/parent MAC callbacks,
real ring publication/dispatcher and exact stop proof, allocation failure at
each of nine pages, no replay/free, deadline/early/late/stale/foreign/invalid
length/stopped events, clock/ownership loss and cleanup/descriptor mutations.
An interleaved USB keyboard event is delivered during the receive window.
The full saved-log checker binds every item to count, completion/residue, phase,
parent cleanup, current TRB stop proof and whole earlier initialization chain.
Installed recovery remains unchanged until all validation and exact-stage checks.


### 2026-10-26 — v94 installed; cold native trial pending

Candidate source `34c2e8b4898966e252d7d335b7298a46cc721e0a`, release
`2530466d580891cc`, profile v94 automatic return.60 native whole-power-on/MAC/
window/ring/event/stop/allocation/shared-keyboard scenarios and230 engine cases
pass. Native and pure ASan/UBSan pass, as do the full committed-source image/core
suite and181 staging tests. The strict whole-prefix checker accepts11 outcomes
and refuses137 malformed proofs;186 historical Wi-Fi configurations and both
merged desktop configurations remain byte-identical.25 native printf arities
match their arguments. Initial compile/stale staging-fixture failures are
retained separately; only corrected complete runs are counted as passing.

The staged kernel matches the final committed-source build; independently
adding the verified firmware package reproduces the staged initramfs hash.
The window split API, unchanged receive parser and power-on runner are linked;
the original strict calibration runner remains uncalled. Exact GPT-stage VM
saved and byte-verified logs, cleanly unmounted and returned automatically.
Missing workspace refused before probing and returned. Q35 has no captured
adapter; these VM results establish workflow/refusal only.

Installed/current v94, accepted v93 fallback and e47 previous recovery match
verified durable payloads. To obtain ESP space, older accepted v92 was removed
only after full durable payload equality; its backup remains outside build.
Ubuntu default/EFI order remain unchanged, workspace unmounted and no one-shot
armed. Offline workspace:333files,816clusters,140directories; zero leaks,
bitmap mismatches or errors. Durable candidate evidence: `wifi-rx-window-34c2e8b`.

After readiness, power off, unplug only Wi-Fi10s while off, reconnect and power
on; leave workspace attached. Guest shows progress, saves results and returns
to Ubuntu automatically; no photos or commands needed. On return collect the
matching v94 session FIRST with `rx-window-native/collect_return.py`, requiring
whole prior proof, parent cleanup, exact current-TRB stop, six guards and each
item's completion/actual bytes/parser/phase. Empty/rejected data is not reception
acceptance. No physical v94 result is claimed; scan/association/security/IP/UI
and sustained receive/FCS/RSSI/RF-performance acceptance remain pending.


### 2026-10-26 — v94 returned with an empty receive window

Release `2530466d580891cc`, source `34c2e8b4`, session
`session-00065ec9814b8379-00` saved complete logs and returned automatically.
Whole earlier firmware/MAC/baseband/RF/channel/AGC, reversible-probe and five
power-on command proofs passed. The window polled18350 times over1000042us,
with one attempted request, zero normal completions and zero beacons; no early
completion or usable bytes occurred. The request remained pending, so no
second TRB was submitted and no DMA bytes were inspected. Parser statuses255
mean not inspected, not parser failure or frame reception.

The unchanged parent MAC transaction completed54 operations/46 reads/7 writes/
1 wait, hold10035us, TX1/RX7 drain rounds, MAC disable and filter/TXOP restoration.
Host wait10001us and exact endpoint stop plus all six guards passed. Stop's
CC26/4096 remaining is cancellation metadata, with DMA retained. This accepts
only the bounded empty-window lifecycle; the new successive native submissions
and repeated reception were not exercised. The earlier v93 beacon observation
remains separate evidence and does not fill this run's missing receive proof.

The strict checker validated the full1MiB-capacity startup prefix and both
completion markers. Current/e47/accepted-v93 payloads, Ubuntu default and
consumed one-shot were verified. Offline workspace:335files,823clusters,
141directories; zero leaks, bitmap mismatches or errors. Workspace unmounted,
no reboot armed. Complete raw evidence and observation records are retained
outside build in `wifi-rx-window-34c2e8b`.

Next: a longer bounded passive window to distinguish a quiet interval from a
receive-path problem, preserving exact completion/byte accounting, parent
cleanup and stop proof. Do not submit another request while the first is
pending. Scanning, association/security/IP/UI and sustained reception, FCS,
calibrated RSSI and RF performance remain untested; no cause is inferred from
this single empty observation.


### 2026-10-26 — explicit ten-second passive receive candidate (v95)

Actual main `eacb4ec4` (PR200 screenshot/worker validation) was merged, preserving
both MT76 and operation-timing Makefile test families. PR202 is Air evidence;
PR182 owns filesystem imports. Neither overlaps this window-budget scope.

Selector73/v95 is a separate explicit long-window profile. The engine admits
only a zero/default1s plan, an explicit1s plan, or10s; arbitrary/overflowing
budgets refuse before submission. The existing v94 native plan remains zero/
1s. Long native ownership requires the10s plan in both the stable adapter and
engine copy. Its absolute poll cap is20M and elapsed accounting cap10.1s;
backward clocks and1024 frozen-clock checks still refuse. The eight retained
pages/requests, one pending at a time, exact completion/byte accounting and
parent hold/drain/disable/restore/stop/six-guard sequence remain unchanged.
A final pending request cannot trigger another submission. Empty observation
is not received-packet or repeated-submission acceptance.

The long profile adds only the explicit window-long boot token and progress
label; all older recipes keep their command lines and short deadlines. There
are no RF/channel/register or parser/calibration changes. Host coverage includes
literal/default/long budget behavior, six invalid budgets, long empty/partial/
completed windows, long clock failure and budget-identity mutation, plus all
prior descriptor/ownership/stop/cancellation/shared-keyboard cases. Physical
v95 remains pending; the last native v94 observation was empty, as recorded above.


### 2026-10-26 — v95 ten-second trial installed, unarmed

Source `e9acacd5041587caf8b31f751270f7a68ab3e97b`, release
`84afc9dee029775f`, v95 automatic return.75 actual native short/long budget,
allocation, parent/power-on, shared-keyboard, event, ownership and exact-stop
scenarios pass.246 engine cases, native/pure ASan/UBSan,182 staging tests and
full merged committed-source image/core/desktop tests pass. The strict whole
prefix checker accepts11 outcomes and refuses137 malformed proofs;188 older
Wi-Fi recipes and both desktop configurations remain byte-identical.25 native
printf formats match argument counts. Original parser, parent MAC cycle,
calibration helpers and native power-on adapter remain unchanged.

The staged kernel and independently firmware-packaged initramfs match the final
committed-source products. Exact GPT-stage VM saved byte-verified diagnostics,
unmounted cleanly and returned automatically. Missing workspace refused before
probing and returned; Q35 results establish workflow only, not native reception.

Current v95, accepted v93 fallback and e47 previous recovery match durable
payloads. EFI space was recovered by first selecting verified v93 as current
while keeping Ubuntu default/e47 recovery, then removing the now-obsolete quiet
v94 only after full durable equality; v95 was installed afterward. V94's full
payload/empty-window evidence remains outside build. EFI order and Ubuntu default
are preserved; no one-shot armed. Offline workspace:335files,823clusters,
141directories; zero leaks/bitmap mismatches/errors, unmounted. Durable candidate
archive: `wifi-rx-window-long-e9acacd`.

After readiness, power off, unplug Wi-Fi10s while off, reconnect and power on;
leave workspace connected. Guest shows the up-to10s listening window, saves
results and returns automatically; no photos or commands needed. Collect the
matching v95 FIRST with `rx-window-long/collect_return.py`. Require whole earlier
proof, ten-second/count accounting, exact byte/event phase, parent cleanup,
exact stop and six guards. An empty window or stopped-event residue is not
packet reception; no repeated native submission, scan/security/IP/UI or RF
performance claim is made before those outcomes are observed.


### 2026-10-26 — v95 bounded repeated native reception observed

Release `84afc9dee029775f`, source `e9acacd5`, session
`session-00065eca15a6993a-00` completed the explicit ten-second observation:
172646 polls,10000029us, seven distinct retained4096-byte requests and six
normal CC13 completions. Each completed item had3900 remaining/196 actual
bytes; its first valid RX entry consumed192 bytes with154-byte MPDU and FC0040
(probe request). All six completions were observed during the active window,
none before it. Beacon-parser status2 means unsupported frame subtype here,
not malformed RX framing. No beacon/probe response or identifiers were logged;
no uniqueness claim is possible from this copied metadata alone.

The seventh request stayed pending and was cancelled only after the unchanged
parent MAC cleanup:54 operations,46 reads,7 writes,1 wait,10036us hold, TX1/RX7
drain rounds, MAC disable and filter/TXOP restoration. Host wait10001us, exact
endpoint stop and six guards passed; final CC26/4096 remaining supplies no
packet bytes. DMA stayed retained, no page/TRB reuse or network data OUT.
Whole prior initialization, reversible observation and five-command power-on
proof passed. This accepts bounded successive native submissions and repeated
RX framing on this release. It does not establish sustained reception, FCS,
calibrated RSSI, RF performance, access-point discovery or connectivity.

Full startup prefix and both completion markers passed the strict collector.
Ubuntu default, consumed one-shot and current/e47/accepted-v93 payload equality
were verified. Offline workspace:338 files,835 clusters,142 directories;
zero leaks, bitmap mismatches or errors. Workspace unmounted, no reboot armed.
Complete raw collection, strict proof and recovery evidence are preserved in
`wifi-rx-window-long-e9acacd` outside build. Next: bounded passive access-point
discovery, then association/security, IP integration and network UI; the
six probe requests do not substitute for beacon discovery acceptance.


### 2026-10-27 — bounded passive discovery result engine

The separate `mt76_discovery` helper consumes only an already completed and
exactly stopped receive window. Its caller must prove stable retained pages,
parent MAC cleanup and all six guards before each input access and again before
publication. Every normal CC1/13 byte count and saved parser outcome must match;
cancelled/pending entries cannot supply bytes. A failed lifecycle, changed ledger,
changed framing or lost cleanup ownership publishes no result rows. Collection
is one-shot and allocation-free, bounded to eight input transfers and eight rows.
It has no native caller, boot profile, packet submission, channel change or UI yet.

The unchanged strict first-entry parser admits only valid infrastructure beacons
or probe responses on fixed US channel1. Malformed/channel-mismatched entries,
unrelated management frames and zero-byte completions have separate counts;
a successful empty collection is not physical access-point discovery acceptance.
Rows deduplicate BSSID while keeping identical names on distinct BSSIDs separate.
Names are binary, length-bounded copies; empty/all-zero names stay hidden until
a valid visible advertisement reveals a name. Later hidden advertisements cannot
erase it. Conflicting visible names retain the first name and set a conflict flag.
Changed privacy/RSN/WPA advertisements also set a conflict flag without silently
replacing the original advertisement. Beacon/probe-response observations are
counted, not claimed to be unique radio frames. Advertised security is neither
suite validation nor authentication, and an observed BSSID is unauthenticated.

Only BSSID/name and bounded advertisement metadata survive in private memory;
no DMA pointers, payload, transmitter address or raw RSSI is retained or logged.
Future UI must escape arbitrary binary names; this helper does not expose names
through console logs or a syscall. Native ownership binding, saved diagnostic
proof, user-visible discovery and later association/security/IP remain separate.
Installed v95 and its native reception evidence are unchanged; no reboot armed.


### 2026-10-27 — explicit native private discovery candidate (v96)

Separate selector74/v96 inherits the exact v95 ten-second/eight-page receive
plan and all earlier initialization, power-on, MAC cleanup, exact stop and six
guards. It runs `mt76_discovery_collect` only after successful window finish,
while those native identities remain owned. Every collection ownership check
also rereads the live output endpoint state/dequeue and requires the same stopped
context. A failed window/ledger/guard or lost live stopped context cannot publish
private rows. No new USB/RF writes, retuning, polling, submissions, DMA reuse or
packet parser policy are added by collection.

The guest displays progress and saves only counts and row metadata: observations,
channel/interval/name length, hidden status, advertisement flags and conflict
flags. BSSID/name bytes remain private in memory; no address, name, payload or
raw RSSI is logged. The new profile requires the explicit long-window chain and
automatic return. Old v1-v95 profiles and their command lines remain unchanged.
An empty table is a completed observation, not access-point discovery acceptance.
Even nonempty metadata does not independently verify physical BSSID deduplication,
authenticated identity/security, sustained reception or RF quality. Native UI,
association/security and IP integration remain future work.

The actual native fixture exercises short/long/v96 mode inheritance, shared HID,
parent/power-on ordering, distinct TRBs, delayed/empty/error completions and stop
proof, plus damaged discovery ledgers and live context loss at every collection
check. The helper is one-shot; rejected captures leave no partial identifiers.
No physical discovery result has been collected for this new profile yet.

Local v96 validation:182 actual native window/discovery scenarios,10168 pure
discovery collections,246 receive-window cases, native ASan/UBSan,183 staging
tests and full merged image/core/desktop checks pass. The strict whole saved
proof admits15 outcomes and refuses167 malformed variants;190 older Wi-Fi
recipes and both desktop recipes are unchanged.27 native printf formats match
argument counts. Original discovery/window/parser/parent/calibration/power-on
source bytes remain unchanged. Exact-stage VM, installation and physical
discovery evidence are separate checkpoints.


### 2026-10-27 — v96 private discovery trial installed, unarmed

Source `8f447183b2df543eaa7d20910cd672b25ce21e60`, release
`6f0426100438a6bc`, v96 automatic return on main `6b6d2f4d`. All182 native
scenarios,10168 pure discovery collections,246 window cases, native ASan/UBSan,
183 staging tests, full core/image/desktop and strict15-valid/167-refusal proofs
pass.190 older Wi-Fi and both desktop recipes are unchanged. Stage kernel and
independently packaged initramfs match the committed-source products; native
discovery/window/parser/power-on symbols link, original strict calibration runner
remains uncalled.27 printf argument counts pass.

Exact GPT-stage VM saved byte-verified diagnostics, cleanly unmounted and returned
automatically. Missing workspace refused before probing and returned. These
verify workflow only, not native discovery. Initial missing v96 staging-pattern
membership, a host-test invocation without its required arguments, and an early
proof-script insertion-order error were corrected before installation; original
failure logs are preserved with the successful evidence.

Current v96, accepted v95/v93 and e47 previous payloads match durable copies.
Only obsolete Ethernet candidate `debb2ddf3e9f4243` was removed from the ESP,
after full file equality with its retained durable backup; current/previous and
accepted Wi-Fi fallbacks were kept throughout. Ubuntu default and EFI order
remain unchanged. Offline workspace:338files,835clusters,142directories;
zero leaks, bitmap mismatches or errors. Workspace unmounted; no one-shot armed.
Durable candidate evidence: `wifi-discovery-native-8f44718` outside build.

After cold-cycle readiness, power off, unplug Wi-Fi10s while off, reconnect and
power on, keeping workspace connected. Guest shows listening/collection progress,
saves results and returns automatically; no photos or commands. Collect matching
v96 FIRST with `discovery-native/collect_return.py`, including whole prior proof,
window item outcomes, discovery counts/metadata, fresh stopped-context ownership,
full prefix/completion, four retained payloads and offline filesystem/allocation.
No AP or connectivity acceptance is claimed until actual results are reviewed.


### 2026-10-27 — v96 native fixed-channel private AP table observed

Release `6f0426100438a6bc`, source `8f447183`, session
`session-00065ecbdbf4e83a-00` reached the eight-buffer count bound after26022
polls/1411525us, before the10s deadline. All eight completions were CC13 and
observed during the window; no early event, ignored event or pending final
request. Actual byte counts in order:312,276,312,196,196,196,312,276. Each
buffer/TRB stayed distinct and retained. Only the first bounded RX entry was
inspected after parent cleanup, exact endpoint stop and six guards.

Five valid channel1 beacons (interval100, privacy/RSN advertised, WPA absent)
produced two private rows: a visible three-byte name with three observations,
and a hidden empty name with two. No probe responses or name/security conflicts
were observed. One unsupported RX entry and two probe requests account for the
three unrelated inputs; no rejected or empty input. All18 fresh native discovery
ownership checks passed. No BSSID, name bytes, payload or raw signal values were
logged. Redacted row metadata does not independently establish BSSID identity/
deduplication or unique radio frames. This accepts the bounded channel1 native
AP-table observation, not a multi-channel scan, security validation or connection.

Whole earlier initialization, reversible observation and five fixed power-on
commands passed. Parent MAC cleanup completed54operations/46reads/7writes/1wait,
10031us hold, TX1/RX7 drains, MAC disable and filter/TXOP restoration. Host wait1us;
final normal completion had3820 remaining/276 actual bytes, then exact stop and
all six guards. No DMA reuse or network data OUT. Full553572-byte startup prefix within
the1MiB capacity and both completion markers passed the strict checker.

Ubuntu default, consumed one-shot and current/e47/v95/v93 payload equality were
verified. Offline workspace:340files,842clusters,143directories; zero leaks,
bitmap mismatches or errors. Workspace unmounted, no reboot armed. Full raw
collection, exact counts/metadata, strict proof and recovery evidence are kept
outside build in `wifi-discovery-native-8f44718`. Next: explicit bounded
multi-channel passive discovery with independently tested retuning and cleanup,
then network selection, association/security and IP/UI. RF0:6 remains unresolved;
no sustained reception, FCS, calibrated RSSI or RF-performance claim is added.


### 2026-10-27 — US channel frequency planning and reversible preview

Separate `mt76_channel_plan` projects all eleven US1–11/20MHz frequency rows
into21 ordered masked register operations per channel. The693-byte independent
host fixture derives from pinned Linux v7.0
[frequency rows](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/initvals_phy.h)
and [PHY operation order](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c).
None of these channels uses the reference's alternate SDM path. Channel1's
register/mask/value triples equal the unchanged accepted synthesizer helper.
This planner does not implement a regulatory database or accept another region,
channel12–14,5GHz,40/80MHz or an arbitrary RF address.

The separate allocation-free preview accepts a canonical US/20MHz plan with at
most11 distinct channels. It snapshots all13 distinct synthesizer registers and
requires their exact saved channel1 values before any write. Each step checks
disabled MAC, CORE, DMA and idle status; reads the fresh previous RF byte,
preserves unmasked bits, marks a possibly touched word before publication, writes
once and independently verifies readback. All touched originals are restored in
reverse snapshot order after success or forward failure under independent cleanup
ownership. A false write result is tested both with and without actual mutation.
Failed cleanup guards or unsafe transport ownership forbid further RF access and
leave explicit unproven restoration; such a transaction cannot pass. Cleanup
attempts other independent saved words after an individual write/read failure.
No retries, waits, MAC writes, VCO pulses, packet I/O or DMA reuse occur here.

The full eleven-channel preview has2592 bounded I/O callbacks:1860 guard reads,
488 RF reads and244 RF writes, including13 cleanup readbacks/writes. Callback
transport deadlines and exact native stopped-context/board ownership must be
provided by a future adapter. Repeating the state is refused. These observations
validate only reversible frequency fields: actual tuning still requires the
remaining RF/baseband/VCO sequence and receiver lifecycle. No native caller,
boot profile, live channel change or multi-channel scan is added in this block;
installed v96 and its native channel1 discovery acceptance remain unchanged.

Independent-reference/masked-bit, all-callback I/O/ownership/readback and forward
cancellation/cleanup tests pass:12918 transactions,59136 masked words, including
all eleven individual plans and the combined preview. ASan/UBSan pass. Initial
test compilation indentation and an overly broad test expectation of restoration
after failed cleanup guards were corrected; original failed logs are retained.
The production guard refusal was preserved. Raw primary-source retrieval was
rate-limited, so the audit uses the prior hash-pinned Linux references, separately
checked against the official source view; source hashes/provenance remain local.

Validation closeout: actual main `e7953352` merged PR204/205/206; the append-only
Makefile conflict retained both discovery and audio-volume test families. Full
merged image/core/desktop,183 staging tests and GPT/OVMF USB keyboard boot pass.
This VM checks merged-baseline boot/input, not native tuning.192 prior Wi-Fi
configurations and both desktop configurations are byte-identical, as are all
existing MT76/native RX/discovery/parent/calibration sources. No physical trial
is staged or armed. Native preview transport/ownership binding is next; channel
RF/baseband/VCO and RX lifecycle remain required before multi-channel discovery.


### 2026-10-27 — explicit native reversible register preview candidate (v97)

Selector75/v97 runs the unchanged US1–11 frequency-field preview only after
whole v96 reception/discovery, parent MAC cleanup, exact main-RX stop and six
guards. A pinned native identity covers slot, control buffers/ring, firmware and
MCU buffers/rings, current main-RX TRB/dequeue and window ring. Every callback
rechecks the live stopped context and stable identities; forward access also
requires the original complete window/discovery and board/firmware proof.
Independent cleanup uses retained safe transport after forward metadata or reply
loss. Quarantined/changed transport or lost stopped context forbids further I/O.

An independent callback gate requires the exact2558 forward operations and
masked values for the canonical eleven-channel plan. Cleanup admits only captured
touched registers, original saved bytes, reverse ordering and independent
readbacks. No arbitrary register, retry, main-RX restart/reuse, VCO pulse, network
frame or MAC enable is added. The register preview is not live tuning or a scan;
remaining RF/baseband/VCO and receive lifecycle work still applies.

Normal evidence consists of2592 callbacks:1860 fresh guard reads,488 RF reads,
244 RF writes and732 strictly matched MCU exchanges, with5185 ownership checks.
All13 originals must be saved/touched/restored, cleanup must finish, and the
original forward proof must remain valid at final publication. Each16-byte RF
reply requires exact sequence/header/address/tail, byte value, write echo, CC13
and1008 residue. Invalid replies log contract flags and transport outcome with
payload words redacted. Successful records contain only the known non-identifying
RF bytes. Guest progress, full callback trace and final restoration summary are
saved; identifiers stay private. The fixture prefix is910690 bytes under the
existing1MiB capacity. Old v1-v96 recipes retain their command lines and behavior.

Native coverage includes every732 RF-reply header mutation, address/value/tail/
length failures, OUT/IN timeout quarantine, forward proof loss with independent
cleanup, live stopped-context loss, board/transport/ledger admission and direct
callback order/value/refusal tests. Early tests exposed missing completed-window
and final-forward-proof gates and a stale packet-size fixture; these were fixed
before staging, and failure logs remain preserved. The pure preview engine is
unchanged. Native register restoration and live tuning remain unaccepted until
a matching physical capture is reviewed.

Local v97 validation:786 actual native preview scenarios plus289 prior/native
window/mode/allocation cases pass, including every732 RF reply mutation and
independent restoration. Native ASan/UBSan,12918 pure preview transactions,10168
discovery collections,246 window cases,184 staging tests and full merged core/
image/desktop pass. Strict saved-proof checks accept15 outcomes and refuse627
malformed variants;192 older Wi-Fi and both desktop configurations are unchanged.
All30 printf argument counts match. Existing pure preview, parser, window,
discovery, parent and calibration/power-on C sources remain byte-identical.
Exact-stage VM, installed recovery and native acceptance remain separate checks.


### 2026-10-27 — v97 reversible register trial installed, unarmed

Source `f836110a80cd7360f896bf70593fc7e32b513d7e`, release
`4acac674194041a5`, v97 automatic return on actual main `e7953352`.786 actual
native preview scenarios,289 native window/mode/allocation cases, native
ASan/UBSan,12918 pure preview transactions,10168 discovery collections,246 window
cases,184 staging tests and full merged core/image/desktop pass. Strict checks
accept15 whole outcomes and refuse627 malformed proofs.192 older Wi-Fi and
both desktop recipes are unchanged;30 printf argument counts pass.

Stage kernel and independently firmware-packaged initramfs match committed-source
products; native preview/discovery/window/parser/power-on symbols link and the
original strict calibration runner remains uncalled. Exact GPT-stage VM saved
byte-verified diagnostics, unmounted and returned automatically. Missing workspace
refused before probing and returned. VM results accept workflow only, not native
register restoration or tuning. Raw earlier admission/final-gate and fixture
failures remain archived with the successful local checks.

Current v97, accepted v96/v95/v93 and e47 previous recovery match durable payloads.
Only obsolete Ethernet candidate `1212d33f6ccf6bd4` was retired from the ESP after
full durable file equality; its backup remains, and all accepted Wi-Fi fallbacks
were retained. Ubuntu default and EFI order are preserved. Offline workspace:
340files,842clusters,143directories; zero leaks, bitmap mismatches or errors.
Workspace unmounted; no one-shot armed. Durable candidate/evidence archive:
`wifi-channel-native-f836110`, including independently pinned frequency references.

After readiness, power off, unplug Wi-Fi10s while off, reconnect and power on;
leave workspace connected. Guest shows progress, runs the existing channel1
observation, then previews all11 frequency plans with MAC disabled/main RX
stopped and restores the saved13 register words. It saves results and returns
automatically; no photos or commands. Collect matching v97 FIRST with
`channel-native/collect_return.py`. Require the full earlier proof,2592 callback/
732 exact RF-exchange trace,5185 ownership checks, verified saved/touched/restored
masks and full prefix/completion/recovery/offline checks. Preserve strict refusal
and raw collection on any failure; `collect_failure.py` records failure/incomplete
evidence without promoting acceptance. A register preview is not live tuning,
RF lock, new-channel reception, scanning or connectivity acceptance.


### 2026-10-27 — v97 native register preview and restoration accepted

Release `4acac674194041a5`, source `f836110a`, session
`session-00065ed40659e09b-00` saved results and returned automatically. All 11
US/20MHz plans completed 231 steps: 1860 fresh guard reads, 488 RF reads and
244 RF writes, including 13 cleanup writes/readbacks. The complete ordered
2592-callback / 732-exchange trace and 5185 ownership checks passed. All 13
originals were saved, touched and restored (each mask 8191); cleanup finished
with main RX stopped, MAC disabled and retained DMA. This accepts the bounded
frequency-field transaction and exact restoration; no VCO pulse or main-RX
restart occurred during preview. It does not accept live tuning, RF lock,
new-channel reception, scanning or connectivity.

The preceding channel1 window was quiet: 175717 polls over 10000043 us, one
submission and zero normal completions/beacons/AP rows. Pending RX cancelled
with CC26/residue4096; exact stop/six guards passed. Parent MAC cleanup completed
54 operations with 10036 us hold; all five power-on commands and their original
restoration proof passed. Earlier v96 positive discovery remains historical.
The full 908235-byte prefix fits the 1MiB capacity and contains both completion
markers. Ubuntu default/consumed one-shot and current/e47/v96/v95/v93 payload
hashes passed. Workspace unmounted; fsck and independent allocation checker
report 343 files, 856 clusters, 144 directories, zero leaks/mismatches/errors.
Raw collection, original structured collector result and complete restoration
proof are preserved in durable `wifi-channel-native-f836110` outside build.
Next: bounded live channel tuning and receiver lifecycle before multi-channel
passive discovery. No reboot armed.


### 2026-10-27 — bounded frequency/VCO excursion engine

Source `1ff74269` on actual main `c07dc402` adds a separate
`mt76_channel_tune_run` transport-neutral transaction, with no native caller or
boot profile. It requires the captured disabled-MAC/CORE/DMA/idle state and
exact channel1 frequency/VCO originals, applies one US1–11/20MHz plan, triggers
bank0R4 bit7, and waits for that trigger to clear. It then restores all touched
originals in reverse order, re-triggers channel1 and independently verifies the
saved words. Trigger clearing is a register observation, not RF lock or
calibrated tuning acceptance. Same-lifetime board/firmware, fixed-band RF/BBP
setup and stopped retained DMA remain mandatory caller prerequisites.

Each VCO attempt allows six reads with at most 100000 us requested delay
(0/10/90/900/9000/90000 us increments). These bounds are SrvrOS policy;
the pinned Linux v7.0 scan path establishes prerequisite/frequency/BBP/VCO order,
not this timeout. Busy/unknown VCO blocks cleanup RF writes. An incomplete
frequency restoration blocks its VCO pulse. Forward cancellation may admit
independent cleanup only while transport ownership remains safe; successful
cleanup never converts a forward failure into success. No receiver restart,
transmit, DMA reuse, scan UI or automatic native startup is added.

Host coverage compares all eleven plans against the existing independent
693-byte fixture and injects every normal-path I/O/ownership failure, ambiguous
write, bad readback, forward cancellation/drift, lost cleanup transport, stuck
VCO and each original-byte bit mismatch. A compiler warning and an initial
fixture that injected arbitrary register changes after the last read were
corrected before validation. Cleanup corruption/readback and lost ownership
remain covered independently; no transport or completion gate was relaxed.
Installed v97 and its native acceptance/recovery remain unchanged. Next is a
separate native stopped-RX/board/firmware/strict-response/delay binding with
fault tests and exact-stage save/return rehearsal, before a cold hardware trial.
Live tuning, receiver restart, multi-channel reception and connectivity remain
unaccepted. No reboot armed.


Offline validation for `1ff74269`: 3969 tuning transactions pass, including all
472 normal-path I/O and 476 ownership boundaries, all eleven independent plans,
stuck/delayed VCO and original-byte bit mismatches. ASan/UBSan and the full merged
image/core/desktop suite pass. All 39 prior MT76/xHCI/native-startup/staging/
reference files match accepted v97 byte-for-byte; no old recipe changed. The
uncalled helper is garbage-collected from the ordinary kernel. These tests do
not accept native retuning or reception; installed/recovery payloads are retained.

The merged ordinary GPT/OVMF USB-keyboard boot regression also passes
(`channel-tune-boot-ok`). VM evidence validates ordinary boot/input only; QEMU
does not emulate this radio. Source, tests, pinned references and recovery audit
are preserved outside build in `wifi-channel-tune-1ff74269`. Native v97 remains
installed, with all five retained payloads and Ubuntu recovery preserved;
workspace unmounted and no one-shot armed.


### 2026-10-27 — native single-channel tuning implementation

Separate selector76/v98 binds the unchanged bounded tune engine to fixed US
channel6/20MHz, after stopped channel1 discovery. It inherits v96 prerequisites
without repeating v97's eleven-plan preview. Main RX stays stopped; transport
rings/buffers and the exact live output context remain pinned. A separate
bounded event ledger admits exact guards, frequency snapshots, masked values,
VCO polling/delays, reverse touched-word restoration and channel1 re-settling.
Each RF exchange validates sixteen bytes, rotating sequence/header/address/tail,
byte value/write echo, CC13 and residue1008. Unexpected values or malformed
replies are redacted; only non-identifying diagnostics are saved.

Cleanup has independent live transport/stop ownership after forward proof loss,
but cannot bypass quarantine or changed DMA identities. Each delay checks
measured minimum elapsed time and fresh ownership after return. All callbacks,
exchanges and polls are bounded. Completion also requires fresh forward proof
at publication, so loss during otherwise successful cleanup remains failure.
Guest progress, full ordered callback trace, VCO observations and restoration
summary are saved. Busy/unknown VCO refuses restoration RF changes; incomplete
restoration suppresses the cleanup pulse. No endpoint restart, network data OUT,
DMA reuse, multi-channel receive or scan UI is enabled.

The first native pass covers every one of138 RF reply headers, wire/transport/
length/address/value faults, board/live-stop gates and delayed/stuck VCO. Added
direct callback refusal and delay-failure/frozen/backward-clock/ownership tests
pass with221 native scenarios. Local ordinary staging membership initially
omitted v98 from its recipe regex and retained an obsolete unknown-version test;
both were corrected, preserving all previous recipes. Exact native acceptance,
VM/install/recovery checks and physical trigger/restoration remain separate.


### 2026-10-27 — v98 channel6 VCO trial installed, unarmed

Source `5e2ed3daba04be8cbc583e061328ec5748818f15`, release
`83aad161d5fa476e`, v98 automatic return on actual main `e283eecb`.221 native
tune scenarios pass, including all138 RF reply headers, wire/transport faults,
independent restoration, board/live stopped-context admission, direct callback
order/value refusal and failed/frozen/backward delay/ownership cases. Normal
synthetic2/2-poll proof has460 callbacks,138 exchanges,927 ownership checks,
320 guard reads,102 RF reads and36 RF writes.786 previous native preview cases
and window/allocation regressions pass; native ASan/UBSan,3969 pure tuning
transactions, merged full image/core/desktop and185 staging tests pass.

Strict full-prefix checks accept15 earlier window/AP variants and36 target/
cleanup VCO polling combinations and refuse4081 malformed traces.194 prior
Wi-Fi recipes and both desktop configurations are unchanged;34 printf arities
match. The normal fixture prefix is616947 bytes under the1MiB capacity; all
bounded polling combinations fit. Committed-source kernel and independently
firmware-packaged initramfs match the exact stage. Tune/plan/discovery/power-on
linkage is present; the original strict calibration runner remains uncalled.
Exact GPT-stage VM saved and byte-verified logs, unmounted and returned. The
missing-workspace VM refused probing and returned. These are workflow checks,
not native tuning, receiver or RF-quality acceptance.

Current v98 and previous e47, accepted v97/v96/v95/v93 payloads match durable
backups. Only obsolete Ethernet payload `0875949083ad6ac7` was retired from ESP
after full durable equality, with its backup retained. Ubuntu default and EFI
order are preserved. Offline workspace:343 files,856 clusters,144 directories;
zero leaks, bitmap mismatches or errors. Workspace unmounted; no one-shot armed.
Durable candidate/evidence archive: `wifi-tune-native-5e2ed3d` outside build.

After readiness, power off, unplug Wi-Fi10s while off, reconnect and power on;
leave workspace connected. Guest shows progress, performs channel1 observation,
then channel6 frequency writes/VCO trigger polling and original channel1 word
restoration/re-settling with main RX stopped throughout. It saves results and
returns automatically; no photos or commands. Collect matching v98 FIRST with
`tune-native/collect_return.py`, requiring the full prior proof and exact
variable-length RF/guard/delay/cleanup trace, completion/prefix markers, all six
payload hashes and offline allocation checks. Preserve strict refusal and raw
collection on any failure; `collect_failure.py` records failure/incomplete
evidence without promoting acceptance. Trigger clearing is not RF lock, live
reception, scanning or connectivity acceptance. No receiver restart or network
data OUT is added.


### 2026-10-27 — v98 channel6 VCO transaction and restoration accepted

Release `83aad161d5fa476e`, source `5e2ed3da`, session
`session-00065ed585ba7f47-00` saved results and returned automatically. All 21
US channel6/20MHz frequency steps completed with main RX stopped. Strict proof
validated the complete ordered 478-callback / 141-exchange trace, 966 fresh
ownership checks, 332 guards, 105 RF reads and 36 RF writes. All 13 originals
were saved/touched/restored (each mask 8191). Target VCO trigger cleared after
four polls with 1000 us requested delay; channel1 restoration/re-settling cleared
after three polls with 100 us requested delay. Each measured wait and exact RF
response passed. This accepts the bounded frequency/VCO register transaction
and original channel1 word restoration/re-settling. Trigger clearing does not
prove PLL lock, RF quality, calibrated tuning or reception.

The preceding channel1 window was quiet: 173350 polls over 10000027 us, one
submission and zero normal completions/beacons/AP rows. Pending RX cancelled
with CC26/residue4096; exact stopped context/six guards and retained DMA passed.
Parent MAC cleanup completed 54 operations with 11957 us hold; five power-on
commands and saved-word restoration passed. No main-RX restart or network data
OUT occurred. Earlier positive v96 AP-table observations remain historical.
The full 616541-byte startup prefix fits the1MiB capacity and includes both
completion markers. Ubuntu default/consumed one-shot and all six current/e47/
v97/v96/v95/v93 payload hashes passed. Workspace unmounted; fsck and independent
allocation report 346 files, 868 clusters, 145 directories, zero leaks, bitmap
mismatches or errors. Full raw collection, original structured collector result,
acceptance and recovery proof are retained in `wifi-tune-native-5e2ed3d` outside
build. Next: safe bounded receiver startup at tuned channel6 before restoring
channel1, then multi-channel passive discovery. No reboot armed.


### 2026-10-27 — separate channel6 receive observer implemented

The original tuning entry retains its stopped-RX transaction. A separate
one-shot observer entry runs after target frequency/VCO verification, then
requires fresh MAC/idle/VCO and all target frequency words before restoration.
Failed observations retain independent cleanup only under proven stopped DMA;
no successful cleanup erases a forward failure. Host fault tests cover 6905
transactions, including each original and observer I/O/ownership boundary,
unsafe callbacks, drift, cancellation and independent restoration.

Opt-in selector77/v99 uses a fresh retained ring and page for one 4096-byte
channel6 receive. The original main-RX ring/eight pages are retained. Exact
Set TR Dequeue completion, live stopped context/DCS and fresh layout/ownership
are checked before ringing the endpoint. The command form, completion-before-
doorbell order and output-context check were audited against
[Linux v7.0 xHCI ring handling](https://raw.githubusercontent.com/torvalds/linux/v7.0/drivers/usb/host/xhci-ring.c);
this is a reference contract, not native acceptance of the new command.
A separately owned MAC cycle supplies a ten-second/count-one observation,
seven quiet RX drain rounds, MAC disable and saved filter/TXOP restoration.
The reused MAC helper does not admit a radio channel; the new caller requires
the captured board and fully verified US channel6/20MHz target. No calibration
or network data OUT is added during receive. Exact Stop Endpoint and four fresh
idle guards precede any tuner RF restoration. Independently owned command-ring
stop remains available after EP0 failure; unknown/quarantined RF ownership
still forbids retuning. Uncertain command/context/DMA states remain failures,
with all allocations retained.

Only a matching new TRB/slot/DCI normal event has a byte count. Early or late
observations are excluded from in-window parsing; stopped-length events never
supply usable bytes. The existing USB-entry/beacon parser receives the original
bounded USB packet and explicit channel6 argument, only after quiescence.
Identifiers stay private; logs contain counts and bounded parser/advertisement
metadata. Quiet, empty, malformed or channel-mismatched input cannot establish
beacon reception or connectivity.

Native model and sanitizer coverage passes356 scenarios: all152 RF-reply
headers;57 MAC/guard boundaries under cancellation, EP0 quarantine and changed
context; command timeout/pointer/slot/context errors; quiet/early/late/foreign/
malformed/empty and channel-mismatched packets; allocation failures; measured
clock/wait faults; and no publication after admission loss. No DMA is freed.
Initial compiler style, fake input context, parser-entry and post-quarantine MAC
fixture mistakes were corrected before staging; the hardware RF guards were
not relaxed. Saved-log validation, full merged tests, exact-stage VM, install
and native acceptance remain separate checkpoints. Installed v98 remains the
last physically accepted payload; no reboot armed.


### 2026-10-27 — v99 fresh channel6 receive candidate installed, unarmed

Source `87144160` consumes actual main `fbb6eaa5` and preserves the independent
Wi-Fi startup-prefix and Air desktop-checkpoint logger contracts. Release
`995dd64ae6135add` is installed for the next cold adapter trial; native acceptance
is pending. V98 remains the accepted frequency/VCO/restoration baseline.

The full merged image/core/desktop build passes, including both logger modes,
the desktop power fixtures and356 native receive cases. ASan/UBSan passes356
native cases and6906 pure tune/observer transactions. Staging passes187 tests;
196 older Wi-Fi recipes and desktop76/77 remain unchanged, and merged-main
76/77/78 desktop recipes match `fbb6eaa5` byte-for-byte. Forty-five diagnostic
format strings match argument counts. The strict whole-prefix collector accepts
15 prior window/AP outcomes,14 fresh receive outcomes and36 VCO polling
combinations, and rejects1979 malformed proofs. A complete synthetic prefix is
628330 bytes, below1MiB. These are synthetic validation, not new native results.

The committed-source kernel and independently firmware-packaged initramfs match
the exact stage. Exact-stage GPT VM saved/byte-verified logs, unmounted and
returned automatically; missing workspace refused probing and returned. QEMU
does not emulate this Wi-Fi adapter. Installation verified all seven new/e47/
v98/v97/v96/v95/v93 payloads against durable copies. Obsolete, unreferenced
`81e3308ffcdfe637` was removed from ESP only after full file equality to its
retained backup, keeping all accepted fallback payloads. Ubuntu remains default,
one-shot is unarmed, workspace unmounted. Offline checks report346 files,
868 clusters,145 directories and zero leaks, bitmap mismatches or errors.

Full source/reference/local/VM/proof/install records and payload are retained in
`wifi-channel-receive-8714416` outside build. Next: after cold-cycle readiness,
power off, unplug only Wi-Fi for10s, reconnect and power on while leaving the
workspace drive connected. Guest progress/results and automatic Ubuntu return
require no photos or commands. On return collect the matching v99 session FIRST;
validate fresh Set Dequeue,one IN,MAC retirement,exact stop/four guards,post-
observer target words and independent channel1 restoration. Quiet or malformed
input cannot establish reception. RF lock/quality,multi-channel discovery,
association/security and IP/UI remain pending. No reboot armed.


### 2026-10-27 — v99 native channel6 beacon and restoration accepted

Release `995dd64ae6135add`, source `87144160`, integrated main `fbb6eaa5`,
session `session-00065ed6f4d568a5-00` returned automatically. The matching saved
session passes the complete strict proof, independently replayed from its raw
627250-byte exit snapshot. Both capture-completion markers and the complete
startup prefix fit the 1MiB capacity. No photos or manual commands were needed.

After the target US channel6/20MHz frequency/VCO verification, exact Set TR
Dequeue selected a fresh retained ring/page before one 4096-byte IN. A matching
CC13 completion arrived during the receive window after 93435 us / 6863 polls,
with residue3752 and actual344 bytes. The existing parser consumed340 bytes,
decoded a304-byte management frame and accepted a channel6 beacon: interval100,
three-byte SSID, privacy and RSN advertised, DS channel present. Raw identifiers
remain private. This accepts that bounded observation, not FCS validity,
sustained RF quality, association, authorization or IP connectivity.

The separate MAC cycle completed54 operations (46 reads, seven writes, one
wait), one TX drain round and seven RX drain rounds, measured10044 us hold,
MAC disable and saved filter/TXOP restoration. Exact endpoint stop, stopped live
context, four fresh quiescence guards and retained DMA passed before the tuner
resumed. Post-observer VCO and all target-word checks passed. All13 original
channel1 words were independently restored and re-settled. Tuning proof covers
494 callbacks,154 matched RF exchanges,1000 ownership checks,336 guard reads,
118 RF reads and36 RF writes. Target and restoration triggers each cleared in
three polls /100 us requested delay. Trigger clearing is not a PLL-lock test.

The preceding channel1 window was quiet for10000060 us: one post, no normal
completion or AP rows. Exact CC26 cancellation/stop and retained old ring/eight
pages passed. No channel1 reception or multi-channel scan acceptance is inferred
from that quiet interval. Prior initialization, five power-on command acknowledg-
ments and independent restoration passed. V96 remains the latest AP-table proof.

All seven current/e47/v98/v97/v96/v95/v93 payloads match durable copies in full.
Ubuntu remains default, one-shot was consumed, and workspace is unmounted.
Offline checks report349 files,880 clusters,146 directories, zero leaks, bitmap
mismatches or errors. Full native collection, original structured strict result,
source/reference/local/VM evidence and recovery proof are retained outside build
in `wifi-channel-receive-8714416`. Next: bounded sequential passive multi-channel
discovery with fresh receive ownership and independent stop/restoration on every
failure. Selection, association/security and IP/UI follow. No repeat boot is
needed for v99; no reboot armed.


### 2026-10-27 — separate three-channel pilot implemented, validation pending

The new private collector requires exactly three independent restored visits
on US1/6/11 at20MHz. It reparses each first bounded USB entry with the explicit
visited channel after stopped/quiescent ownership checks. Six fresh physical
pages must be distinct; all tune target/observer/cleanup masks, original words
and bounded VCO delay ledgers must pass. Private BSSID rows preserve first
advertised security and revealed SSID, flag name/security/channel conflicts,
and publish only after all visits and ownership checks complete. Failure leaves
no result rows. No raw IDs, transmitter address, signal fields or DMA pointers
are copied into the result table. Hidden SSIDs remain distinct from empty reads.

Explicit selector78/v100 retains each native tune/receive state and its new
ring/page. The previous visit's exact stopped context admits the next Set
Dequeue; every RF/receiver ownership gate also rechecks older pinned records.
An independently owned endpoint Stop remains available after forward/history
loss; unproven MAC/DMA/retained ownership forbids RF restoration. Old completion
events cannot complete the current request. Per-visit progress, complete traces
and restoration summaries precede final private-row metadata. Ordinary startup
and v1–v99 profiles remain separate. The initial scan fixture's sticky fault26
was corrected by selecting the intended per-visit allocation fault before
allocation; production ownership gates were not relaxed. Local/native boundary
checks, sanitizers, merged tests, whole-log proof, exact-stage VM and physical
acceptance are separate checkpoints. V99 stays installed/accepted; no new
release is armed.

The combined native suite also exposed a fixture admission leak: the earlier
receive allocation-failure loop left its fake allocation ceiling at zero.
It now restores its caller's ceiling, and the scan tests use the actual mode78
selector; window allocation/refusal coverage includes mode78. The production
allocator and hardware admission gates are unchanged.


### 2026-10-27 — v100 three-channel passive candidate installed, unarmed

Release `a7bc67f2d057e7d8`, source `2847cb05`, consumes actual main `a3066f87`.
The fixed US1/6/11,20MHz pilot uses one new retained ring/page per visit and
independent MAC retirement, endpoint stop and channel1 restoration between
visits. The complete merged image/core/desktop suite passes, including actual
mode78, shared window allocation/refusal and the combined receive-to-scan
fixture handoff. Final ASan/UBSan passes115 native scan cases,356 older native
receive cases and215 private collector cases. Staging passes188 tests;198
v1–v99 Wi-Fi recipes and desktop76/77/78 recipes remain unchanged. Fifty-two
diagnostic format strings match argument counts.

The complete log checker accepts102 whole valid prior/RX/VCO/private-row
outcomes; its representative full prefix is778379 bytes within1MiB. It checks
the shared initialization once and independently projects all three frequency/
VCO/RX/stop/restore traces. Negative tests isolate each exact visit, plus whole
parent marker failures, and reject455 malformed proofs. Private row channel
masks partition the observed beacon visits; counts, name visibility, advertised
security and conflict metadata are constrained by those observations. Redacted
records cannot independently prove physical BSSID identity/deduplication. Initial
adjacent-marker and parent-proof extraction mistakes were corrected; the test's
invalid hold sentinel was changed to9999 us, below the10000-us requirement.
A longer measured hold is not automatically a malformed record. Original
refusals and both corrected validation receipts are retained.

The committed-source kernel and independently firmware-packaged initramfs
match the stage. Exact-stage GPT VM saved/byte-verified logs, unmounted and
returned; missing workspace refused probing and returned. QEMU validates this
workflow, not the Wi-Fi radio. Installation verified all seven current/e47/
v99/v98/v97/v96/v95 payloads against durable copies. To preserve the installer
margin, unreferenced v93 `6fcb4fe992b35b78` was removed from ESP only after full
payload equality to its retained `wifi-passive-rx-774e9e9` backup. Ubuntu and
the newer accepted recovery payloads remain retained. Offline checks report349
files,880 clusters,146 directories, zero leaks, bitmap mismatches or errors.

Full source/reference/local/VM/proof/install evidence and payload are retained
outside build in `wifi-scan-native-2847cb0`. V99 remains the latest native
acceptance. Next: after cold-cycle readiness, power off, unplug only Wi-Fi for
10 seconds, reconnect and power on; leave the workspace drive connected. Guest
progress, saved results and automatic Ubuntu return need no photos or commands.
Collect matching v100 FIRST on return; require all three independently stopped/
restored visits and complete-prefix proof before acceptance. Quiet visits do
not establish reception. Association/security, authorized IP and Wi-Fi UI remain
pending. Workspace unmounted, Ubuntu default, no reboot armed.


### 2026-10-27 Pro11,2 three-channel pilot prerequisite refusal

Release `a7bc67f2d057e7d8`, source `2847cb05`, saved session
`session-00065ed80b7c4d71-00` and returned automatically. The complete startup
prefix is535857 bytes within the1MiB snapshot, with both failure completion
markers. No scan visit or receive window ran. Post-channel baseband setup
completed189 operations; post-VCO observation completed63 operations with
six delayed samples and first_clear1. Its immediate post-trigger read was
`0x30`, followed by six delayed `0x30` reads; the existing captured-profile
gate requires immediate `0xb0`. AGC-state ownership consequently refused
before its first read (`status=1 attempted=0 reads=0`). This is a timing-profile
refusal, not evidence of a failed three-channel receive or proven RF lock.

The strict checker refused the absent scan progress marker as expected. Raw
capture, original refusal and failed run are preserved outside build in
`wifi-scan-native-2847cb0`. Seven installed payloads and Ubuntu recovery were
verified, the one-shot entry consumed, and the workspace unmounted. Offline
checks report351 files,887 clusters,147 directories, zero leaks, bitmap
mismatches or errors. V99 remains the latest native acceptance. Next: reproduce
the immediate-clear post-VCO variant in a focused prerequisite regression and
review a bounded timing contract before any candidate retry; preserve guard,
framing and delayed-sample requirements. No new acceptance or reboot armed.


### 2026-10-27 Pro11,2 scan timing retry and acceptance boundary

Source `57fede24`, release `c80f3afe5d496f36`, consumes actual main `108969f7`.
The same-unit immediate VCO read can be0xb0 or already0x30 after the exact0xb0
write acknowledgment. New `mt76_post_vco_settled_profile_valid` admits only
those two readbacks, requiring complete63-operation framing/guard ledgers,
all six measured waits totaling1s, and all six delayed0x30 samples. Only scan
mode78 selects it; the older captured-profile predicate still requires0xb0.
This does not establish RF lock, calibration quality or connectivity.

The regression fails at the actual scan AGC ownership callback against the
original gate. The fix passes16 callbacks, all256 immediate byte values,
5760 guard/word bit mutations under the established DMA idle mask, and omitted
wait/partial-state tests. Full merged image/core/desktop tests pass. ASan/UBSan
passes115 scan fault cases per timing variant in independent processes (230
total),356 older receive cases,215 private collector cases and pure AGC checks.
The initial all-bits DMA expectation was corrected to the existing idle mask;
combining both scan matrices after earlier retained-allocation tests exhausted
the fixed5MiB host fixture pool, so each timing matrix now has its own process.
Production DMA allocation/retention and guard masks were unchanged.

Whole-log proof passes102 valid cases and455 malformed refusals, plus both
complete timing variants and256 full post-VCO trace immediate-value checks.
Staging passes188 tests;198 older Wi-Fi recipes and three desktop recipes
remain unchanged. Exact-source kernel and independently packaged initramfs
match the stage. Exact-image GPT VM saved/byte-verified logs, cleanly unmounted
and returned; missing workspace refused probing and returned. VM checks do not
accept native RF operation.

Seven complete current/e47/v99/v98/v97/v96/failed-v100 payloads match durable
backups. To maintain installer capacity, v95 `84afc9dee029775f` was retired
from ESP only after full equality to its retained `wifi-rx-window-long-e9acacd`
payload; v93's earlier durable backup is also preserved. Offline workspace
checks report351 files,887 clusters,147 directories and zero leaks, bitmap
mismatches or errors. Evidence and payload survive distclean outside build in
`wifi-scan-timing-57fede2`. V99 remains latest native acceptance. The retry is
installed, unmounted and unarmed; await manual cold-cycle readiness, then save
and collect all three visit/stop/restoration traces before acceptance. No photos
or guest commands needed. Association/security and IP/UI remain pending.


### 2026-10-27 Pro11,2 bounded three-channel native result

Release `c80f3afe5d496f36`, source `57fede24` on main `108969f7`, saved session
`session-00065ed915340ee5-00` and returned automatically. Complete782503-byte
startup prefix and both successful completion markers pass. All three fixed
US1/6/11,20MHz visits used distinct retained ring/page pairs, exact Set Dequeue,
one bounded IN, MAC retirement, exact Stop/four guards and independent
restoration/re-settling of all13 original channel1 words before advancing.
Ordered proofs validate1500 tuning callbacks,465 RF exchanges and3039 ownership
checks. Target VCO polls were2/5/5 and cleanup polls3/3/3; clearing is not RF lock.

Channel1's receive window was quiet for10000012 us and stopped with CC26.
Channel6 completed after95026 us/4314 polls, CC13/residue3752/actual344,
consumed340 bytes with a304-byte beacon MPDU. Interval100, visible SSID length3,
privacy/RSN true, WPA false and DS present; names and identifiers stay private.
The copied table contains one channel6 row with one observation and channel
mask32. Channel11 had a176-byte normal completion before the enabled receive
window; it was deliberately uninspected and contributes no advertisement.
All three MAC cycles completed54 operations with46 reads,7 writes and one wait;
measured holds were10069/10070/10069 us. No network data OUT or DMA reuse.
This boot's immediate post-VCO read was0xb0; the new scan-only0x30 admission
path retains synthetic coverage plus the earlier native observation, not a
positive native execution claim.

The initial strict collection refusal was caused by a complete cached workspace
checkpoint during channel1. Reviewed `tools/wifi_scan_log.py` validates all seven
lines against the disabled-device profile, common timestamps, scheduler bounds
and original raw prefix offset before projecting only the Wi-Fi phase. Unknown
records remain for phase refusal. The original capture is unchanged; the saved
`session-b.log` is byte-identical to its592911-byte raw prefix. Original checker,
refusal and failed-run record remain archived. Six unit tests, three full
checkpoint scan proofs and96 malformed checkpoint/phase cases pass; the full
102-valid/455-refusal matrix and198 prior recipes pass again. No kernel or
hardware gate was changed for collection.

Seven complete current/e47/v99/v98/v97/v96/failed-v100 payloads match durable
backups. Ubuntu default, consumed one-shot and unmounted/unarmed state pass.
Offline checks report355 files,905 clusters,148 directories, zero errors, leaks
or bitmap mismatches. Evidence, both original/rechecked proofs and frozen source
are retained outside build in `wifi-scan-timing-57fede2`;1078 hashes verified at
the native audit before final publication. No repeat boot is needed. Next:
bounded repeated receive/drain handling per channel, preserving independent
stop/restoration and private channel-aware parsing. Sustained RX/FCS, channel11
reception, physical BSSID deduplication, association/security and IP/UI remain
pending. Battery/headphones/further trackpad latency work remain deferred.


### 2026-10-27 Pro11,2 bounded pre-enable retry candidate

Source `6b12e4f2`, release `f1015eeb7358f9c7`, consumes actual main `e4f8bf84`.
Selector79/`srvros.wifi-scan-retry=1` reserves a ring and two distinct data pages
per fixed US1/6/11 visit. If slot0 completes normally before the enabled MAC
observer window, the driver preserves its exact completion ledger uninspected
and publishes slot1 once, pointing to the second preallocated page. The ten-
second deadline starts before that publication and is never restarted. Duplicate
slot0 events cannot complete slot1; both TDs/data pages and all prior visits
remain pinned. Final parsing and collection use only the selected page after
MAC retirement, exact endpoint Stop and fresh guards; original channel1
restoration remains independent. No second post occurs for an already useful
first in-window completion or a quiet pending first request. This is one bounded
early-completion retry, not a sustained receiver or new network-data transmitter.
Old v1–v100 profiles retain their previous code path and recipe bytes.

Full merged image/core/desktop tests pass. ASan/UBSan passes232 retry cases in
independent115/69/48-case processes,115 legacy scan cases,356 legacy receive
cases,16 AGC callbacks and215 private collector cases. Cases cover first normal/
empty/full early completions, selected data versus poisoned first data, no
retry replay, duplicate first/prior events, quiet/invalid/foreign second events,
clock/frozen/deadline failure, all three visits, reserve allocation, page aliases,
identity/TD/context loss, independent stop and refused unsafe RF restoration.
The initial expanded matrix exceeded the fixed retained-DMA host pool; splitting
its processes preserved the5MiB budget and production retention policy. The
layout model was also corrected to stop generating fake DMA after deliberately
corrupting its own binding. Original refusals are retained as failures.

Saved-log checks pass110 complete retry outcomes and123 retry/visit refusals,
15 prior-window variants,455 existing malformed visit cases, three complete
checkpoint projections/96 malformed checkpoint cases and both VCO timing
variants/256 full post-VCO trace immediate values. The original raw snapshot
remains unchanged; only fully validated extra retry/checkpoint records are
projected from individual Wi-Fi proofs. Slot1 must follow all pre-observer MAC
reads and precede the poll result; selected/first/reserved identities, exact
stopped pointers, no deadline restart and all nine fresh pages are checked.
Representative full prefix is780091 bytes within1MiB, including all three
retries; maximum bounded VCO variants also fit. Staging passes189 tests;200
older Wi-Fi recipes and three desktop recipes remain unchanged. Fifty-four
format strings match their argument counts.

The exact committed-source kernel and independently packaged initramfs match
the stage. Exact-image GPT VM saved/byte-verified logs, unmounted and returned;
missing workspace refused probing and returned. QEMU does not accept native
radio operation. Seven complete current/e47/v100/v99/v98/v97/v96 payloads match
durable backups. Unreferenced failed-v100 `a7bc67f2d057e7d8` was retired from ESP
only after full equality to its preserved `wifi-scan-native-2847cb0` payload;
accepted v100 `c80f3afe5d496f36` and Ubuntu recovery remain retained. Offline
workspace checks report355 files,905 clusters,148 directories, zero errors,
leaks or bitmap mismatches. Payload, source and evidence survive distclean in
`wifi-scan-retry-6b12e4f` outside build. Installed, unmounted, unarmed. After cold
readiness, unplug Wi-Fi10s while off, reconnect and power on; leave workspace
connected. Guest progress, saved logs and automatic return require no photos or
commands. Collect v101 FIRST, checking optional second completion and cleanup
before acceptance. Association/security, sustained RX/FCS and IP/UI stay pending.

## 2026-10-27: v101 native pre-enable retry accepted

Release `f1015eeb7358f9c7`, source `6b12e4f2`, integrated main `e4f8bf84`,
saved session `session-00065edaa5a051a0-00` passed the original whole-log proof.
The full785759-byte startup/exit prefix fits1MiB, with both completion markers.
Two cached device checkpoints match their original raw prefixes byte for byte
(593137 and749349 bytes). No failure projection or relaxed native guard was needed.

Channel1 stayed quiet for10000004 us. Channel6 decoded a beacon in53262 us
(2347 polls, CC13,344 actual bytes,340 consumed,304-byte MPDU). On channel11,
the first IN completed before enable (CC13,120 bytes); it stayed uninspected.
Exactly one fresh reserved-page IN used TD1, preserving TD0 and all page pins.
The selected retry remained quiet for10000019 us, then stopped with CC26 and
zero actual bytes. No deadline reset, duplicate completion, DMA reuse, identifiers
or network data OUT occurred. All nine fresh pages were distinct. Each visit
completed54 MAC operations, independent quiescence/Stop and all13 original
channel1 word restorations. Tuning callbacks488/506/506 and exchanges153/156/156
passed exact order/value/ownership checks. The private table contains one
channel6 row. This accepts the bounded early-completion retry lifecycle; it does
not establish channel11 reception, sustained receive, FCS, physical BSSID
deduplication, association/security or IP/UI.

Independent final audit reran the raw native proof, matched committed source
hashes and all seven complete live payloads to durable backups. Ubuntu remains
default, one-shot consumed, workspace unmounted and no reboot armed. Offline
checks:359 files,924 clusters,149 directories, zero errors/leaks/bitmap mismatches.
Evidence, source, payloads and collection scripts survive distclean under
`/home/paul/.local/share/srvros/macbookpro11-2/wifi-scan-retry-6b12e4f`.
Next: bounded repeated receive with distinct retained buffers, unchanged deadlines
and independent Stop/channel1 restoration on every outcome. No repeat of this
successful trial is required.

## Repeated receive-window collection contract

`mt76_rx_window_plan` can explicitly bind `country=US` and channel1,6 or11.
An all-zero binding retains the historical channel1 behavior. Mixed/implicit
bindings, other countries and other channels are refused before callbacks.
Channel binding changes parser admission only; it never tunes or enables a radio.
The existing eight sequential INs, immutable distinct pages, bounded original
one/ten-second deadline and deferred post-stop inspection are retained.

`mt76_scan_batch_collect` consumes exactly three US1/6/11 ten-second window
snapshots with their independently completed tune/restore ledgers. It validates
all three rings and all24 pages, including unsubmitted pages, before reading any
payload. The caller must prove every native DMA identity, MAC retirement,
endpoint cessation and original-channel1 restoration on each ownership check.
Only enabled-window data belongs in these snapshots; rejecting pre-enable data
remains the native adapter's responsibility. No pre-enable buffer may be relabeled
as an enabled-window result merely to feed this helper.

Each completed transfer's first USB entry is reparsed with its tuned channel and
checked against saved metadata. Up to24 copied private advertisements are merged
by BSSID, retaining hidden-name reveals and name/security/channel conflicts.
No borrowed pointer, raw payload or signal measurement is published. Quiet,
empty, unsupported and malformed observations have independent accounting.
Ownership/ledger failure leaves the public result failed with no partial rows.
This helper neither polls hardware nor starts an endpoint, retunes, transmits,
logs identifiers or joins a network. Native repeated-receive integration and its
exact saved-log/VM/physical evidence remain the next checkpoint; host fixtures
do not accept sustained RF reception or Wi-Fi connectivity.

## Native repeated-IN trial (v102)

Source `ebbb08cb` adds an opt-in selector80/v102 adapter for the repeated-window
contract above. Per US1/6/11 visit it reserves a fresh ring and nine data pages,
retains an observed early TD uninspected and selects eight distinct pages with
a zero/one TRB offset. It validates both configured/saved plans and all retained
current/prior identities before each action. Events for earlier TDs cannot
complete the active request. One measured10s clock covers the entire window;
no retry restarts it. Every post, completion, final metadata and retained page
is recorded. Independent MAC retirement, exact Stop, four fresh guards and
original-channel1 restoration remain required on every path. Ordinary startup
and v1–v101 recipes stay unchanged; no active network data is transmitted.

Release `3337af3753372b9d`, source `ebbb08cb` with main `052ebc18`, passed
on Pro11,2 in session `session-00065edbc19533a8-00`. All24 selected receives
completed:8/8/7 beacon observations on channels1/6/11, one unrelated transfer,
and six anonymous discovery rows. Each visit completed in less than0.88s.
Channel6 rejected an early276-byte completion uninspected, then used eight fresh
pages under the original deadline. All30 distinct ring/data pages stayed retained;
all three independent MAC/endpoint stops and13-word channel1 restorations passed.
The full799953-byte capture, seven complete payload equalities and clean offline
workspace/recovery checks passed. No repeat is needed; Ubuntu is default and no
reboot is armed.

Full build/core/desktop, native fault/sanitizer, strict whole-log and saved/refusal
exact-stage VM gates precede this physical result. The
[unit record](machines/macbookpro11-2.md) records counts, source and retained
failures. Private row/count proof does not establish physical BSSID grouping,
FCS, sustained endurance, association, security or IP/UI. Bounded discovery
publication/selection and transmit/association groundwork remain next.

## Discovery snapshot and explicit selection groundwork

`mt76_selection` copies a completed, owned three-visit batch into a private
bounded24-row snapshot. Every refresh attempt advances its generation and erases
the previous snapshot before validation, including failures. Generation exhaustion
refuses further publication instead of wrapping. The serialized owner must retain
its generation across refresh/disconnect/invalidate operations; a future service
must also scope requests to that owner's lifetime. Publication rechecks ownership
after copying and exposes no rows on failure. Counts, observation totals, unique
unicast BSSIDs and US1/6/11 channel masks are validated; unused SSID bytes are
cleared. The callback remains responsible for exact native ownership and cleanup.

Selection requires an explicit generation and row index. It rejects an old
snapshot, missing row, hidden name or SSID/security/channel conflict, with no
first/open-network fallback. Success returns copied advertisement metadata and
US/20MHz binding. RSN/WPA presence does not validate cipher, AKM, key exchange or
negotiated security; association must reparse full IEs. Hidden-network credentials
and explicit SSID entry remain separate work. Binary SSID display escapes control,
non-ASCII and backslash bytes as `\xNN` with a checked full-output buffer.

The initial `a31bbe00` step provided the transport-neutral host-tested helper.
The service integration below now binds it to completed opt-in capture and a
System picker. It does not change v102's hardware acceptance or enable ordinary
Wi-Fi startup. Native UI acceptance and management TX/association remain pending.

Source `a31bbe00` (main `5a94d755`) passes2030 directed selection cases and355
batch-collector cases with selection checks on successful real collection, both
under ASan/UBSan. Full merged image/core/desktop,191 staging tests and209 prior
Wi-Fi/current Air desktop/MWAIT/C3 recipe comparisons pass. Exact staged v102
`d6d81caedd2618f8` passes GPT saved-log/return and missing-workspace refusal VMs;
its kernel and independently packaged initramfs match the committed source.
This image is not installed; accepted hardware remains `3337af3753372b9d`.

## Adapter discovery cache and System Wi-Fi page

The cache service accepts at most four independent providers after their completed
batch ownership/cleanup proof. Each provider receives an opaque boot-lifetime owner
ID that is never reused. Refresh attempts invalidate old generations and selections;
failed proof exposes no rows. Removal scrubs copied identifiers and retires the ID.
Snapshot/selection operations run serialized on CPU0, with bounded read-only presence
checks and no USB control, RF command, scan, transmission or join. Root-attached
Edimax publication occurs inside the completed v102 capture ownership phase, after
all three stops/restorations and batch collection, before that owner retires. Hub
children remain excluded until a child-port observer exists. Root disconnect, latched
connection change, reset, missing controller/device or quarantine invalidates the
cache; reconnect cannot resurrect the old owner or automatically rescan.

`SYS_WIFI` uses a versioned fixed-size snapshot/selection ABI. The snapshot enumerates
one adapter at a time with a next-owner link and bounded escaped display names,
channel/advertisement flags and selected index. It exposes no BSSID, DMA pointer,
raw IE or credential. Selection binds owner, generation and row index and only copies
a private candidate. RSN/WPA presence is not negotiated security. Snapshot reads and
selection cannot turn this diagnostic adapter into an IP provider.

System's Wi-Fi page shows cached results, hidden/conflicting/unavailable states,
explicit selection and adapter cycling. Changing adapter or generation clears the
highlight; a stale request asks for a new choice. Selection remains visibly separate
from connection. Refresh reads saved results; it does not launch a scan. Ordinary
startup still does not initialize this diagnostic Wi-Fi path. A later explicit
scan-to-desktop profile is needed for attended native UI acceptance, followed by
management TX, association/security and IP integration.


Service refresh only republishes an already-completed immutable batch. A future
hardware rescan must first remove the old service owner, then acquire fresh scan
resources and publish under a new owner after cleanup. This cache API is not a
background scan scheduler and must not be used to keep old choices live while
retuning. Contexts and presence callbacks remain pinned until removal.

Source `e4c2b1a6` (main `5a94d755`) passes the service's four-adapter/owner/refresh
fault suite, syscall ABI/user-copy/allocation boundaries and System tests using
the actual cache service. UI tests cover adapter cycling, hidden/conflict refusal,
a generation change between display and click, malformed snapshots and disconnect
without identity resurrection. A rendered populated fixture was inspected.
ASan/UBSan passes those three suites and108 native batch cases, including the
real capture publication phase, read/selection with no USB control transfers,
latched root connection-change retirement and hub/quarantine refusal.

Full image/USB-image/core/desktop,191 staging tests and209 old/current recipe
comparisons pass. Desktop System VM with e1000 DHCP exercises the real new syscall
and empty Wi-Fi page plus existing pages, sorting, pause/refresh, menus, resize and
process ABI checks. Exact staged v102 `3ed07b1dd8567913` saves/verifies logs and
returns on GPT, while missing workspace refuses and returns. Kernel and independent
firmware-packaged initramfs match the committed source. This is VM/host evidence;
native cache/UI acceptance requires a later explicit scan-to-desktop profile.
The new image is not installed or armed. Native hardware acceptance remains the
bounded receive trial `3337af3753372b9d`; connection/security/IP remain pending.

## Attended scan-to-desktop profile (v103)

`--wifi-desktop-trial` requires the complete automatic `--wifi-scan-batch` recipe.
It inherits v102's RF/USB operations byte for byte and adds only the attended
handoff token. GUI admission requires successful capture, a ready selectable
cache and a verified immutable `wifi-scan.log` containing the complete extended
startup proof. The same session then uses ordinary64KiB rolling logs, preserving
`wifi-ui-begin.log` and the radio file across an untimed desktop session. This
avoids overwriting radio proof when the user takes longer than the extended
startup buffer permits. The collector must bind the overlapping phase bytes and
run ID; a tail log is not a replacement for the complete radio proof.

Help opens the entire `wifi-trial` checklist automatically. The user inspects
System's Wi-Fi list, selects a visible network, exercises Refresh/Pause/Resume,
unplugs only Wi-Fi and confirms that its cached list disappears. Reconnecting does
not restore an old owner or rescan. Exit desktop saves logs, unmounts and returns
to Ubuntu without commands; closing Help alone does not finish. Anonymous cache
before/after reports, cumulative selection/removal counts and choice outcomes are
saved; physical acceptance still requires the user's visual report. No password,
association, security negotiation or IP operation is part of this profile.

Failure before admission refuses GUI and follows the existing automatic save/
return policy. Quiescence failure retains the logger's write and mount ownership.
Ordinary log profiles, v1–v102 Wi-Fi recipes and the separate AP diagnostic kernel
remain unchanged. This new profile requires its own matched host/VM/install/run
record and native evidence; it does not inherit v102's acceptance automatically.


v103 candidate `fe9dc6295c7318c4` (source `10f15a3b`, main `951135b0`) is
installed on Pro11,2, unarmed and awaiting the attended cold trial. Full build/
core/desktop,193 staging checks,215 recipe comparisons, five sanitizer suites
and108 native-batch cases pass. The System VM opens the complete checklist at
2x scale and exits cleanly. Exact-stage saved-log and missing-workspace VMs pass;
without the captured radio, GUI admission is correctly refused. Strict composed
radio/UI proof and collector refusal fixtures pass. No new native UI acceptance
is claimed. The matching run and durable archive are recorded in the unit guide.
A failed final workspace unmount preserves the mount and refuses automatic reboot
for this profile. Passwords supplied locally for future security testing are
excluded from this image, logs, fixtures and archive.


The first v103 native run saved its exit log and returned but did not enter the
desktop: the cache presence predicate mistakenly required the generic USB
configured flag, which this diagnostic enumeration deliberately leaves false.
Its completed firmware/MCU/RX endpoint ownership is separate. The host fixture
had forced the generic flag true. The raw failure and strict collection refusal
are retained; this run does not accept the cache or UI. The guard repair requires
actual firmware/MCU/RX configuration and firmware readiness, while preserving
root-port, connection-change, quarantine and active publication ownership gates.
The host fixture now publishes with the native generic flag false and refuses
each missing diagnostic configuration state independently. A new source-bound
image and physical UI trial are still required before acceptance.


Corrected v103 `852438c8b7cd0b7e` (source `d627337c`, main `ec7bad42`) is now
installed unarmed on Pro11,2. Full merged build/core/desktop,193 staging tests,
219 recipe comparisons, five sanitizer suites and108 native-batch cases pass.
Strict phase/collector fixtures,2x checklist/System VM, exact-stage GPT save/
return and missing-workspace refusal pass. The ordinary kernel has no isolated
AP request. The previous failed v103 remains archived; native populated cache,
selection and disconnect UI still require the attended cold trial. Passwords
remain unused. See the unit record for run, recovery and collection details.


The corrected v103 run `session-00065eeb2cd82d26-00` failed before cache publication:
channel6 polling took10150270us, exceeding the unchanged10-second budget plus
100000us allowance. A periodic log save/flush immediately precedes the failure;
I/O scheduling interference needs isolation before another trial. Cleanup was
logged and results saved, but strict collection refuses the incomplete scan and
missing radio/GUI phase files. The cache fix and populated UI remain physically
unaccepted. The deadline must not be widened or reset to turn this run into a pass.


## Isolated radio logging profile (v104)

`--wifi-log-isolation` requires the automatic attended `--wifi-desktop-trial`
profile and inherits every v103 radio operation unchanged. Before firmware/radio
capture, main requires the extended logger's bounded writer drain and a verified
boundary snapshot. Periodic disk writes then remain paused while full proof
continues into the existing extended RAM buffer. The original receive deadline,
its100ms overrun guard, interrupt handling and scheduler remain unchanged.
No receiver buffer is parsed or reused because of a late poll.

A separate parked-session ownership bit permits that session alone to hand off
to `wifi-scan.log` and resume the ordinary rolling desktop logger. Generic pause
or failed quiescence does not grant the handoff. Save/readback/sync or full-prefix
failure refuses radio or GUI as appropriate; pending writes retain ownership.
Handled capture failure saves the exit log under the existing return policy.
An interrupted run may have only its boundary snapshot and remains incomplete.
The native checker requires one parked phase before firmware work and rejects
snapshot, flush or logger-cost/USB-timing records inside it. Host compositions
use an explicitly synthetic isolated variant of prior accepted RF data; these
fixtures do not accept the new profile. Native acceptance requires its own run.
