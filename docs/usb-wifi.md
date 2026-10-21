# USB Wi-Fi bring-up: captured Edimax adapter

Initial scope, October 21: `7392:a711` revision `0100`, Edimax EW-7711MAC,
attached to MacBookPro11,2. Linux binds `mt76x0u`; SrvrOS currently has no Wi-Fi
radio driver, association/authentication service or Wi-Fi configuration UI.
Ethernet support is preserved. No physical SrvrOS Wi-Fi result is claimed.

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
checkpoint is a documented vendor-control/firmware contract and read-only native
identity diagnostic; inventory-only groundwork does not need a reboot.

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
