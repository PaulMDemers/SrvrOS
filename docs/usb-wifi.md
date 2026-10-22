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
New calibration stages use v44, Air app-file stages retain v43, and historical
installed payload bytes are unchanged. Firmware stages use the descriptive name.

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
