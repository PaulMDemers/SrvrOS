# USB Wi-Fi bring-up: captured Edimax adapter

Initial scope, October 21: `7392:a711` revision `0100`, Edimax EW-7711MAC,
attached to MacBookPro11,2. Linux binds `mt76x0u`; SrvrOS currently has no Wi-Fi
radio driver, association/authentication service or Wi-Fi configuration UI.
Ethernet support is preserved. Native identity, logical calibration capture and
firmware upload/readiness are accepted for the captured adapter; radio operation
and Wi-Fi connectivity remain unimplemented/unaccepted. See the latest milestone
at the end of this guide.

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
frequency offset is68, temperature offset−8, 5GHz low/mid/high LNA gains12/12/12,
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
