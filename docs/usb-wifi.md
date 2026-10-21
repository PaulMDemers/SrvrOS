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

## Primary references

- [Linux device table and initialization](https://github.com/torvalds/linux/blob/master/drivers/net/wireless/mediatek/mt76/mt76x0/usb.c)
- [Linux MCU firmware loader](https://github.com/torvalds/linux/blob/master/drivers/net/wireless/mediatek/mt76/mt76x0/usb_mcu.c)

These are protocol research references, not imported driver code. Check each
file's license before any code reuse; the SrvrOS root license does not relicense
third-party sources or firmware. Firmware packaging is not part of this change.
