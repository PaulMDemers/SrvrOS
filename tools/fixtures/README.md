# Hardware fixtures

`apple-05ac-0291-config.bin` contains the 84-byte USB configuration descriptor
read from `/sys/bus/usb/devices/1-5/descriptors` on the target MacBookAir7,2
on 2026-10-16. The device descriptor and any strings/serial identifiers are not
included. Linux identifies the source device as Apple `05ac:0291`.

The three interfaces are non-boot HID (0), boot keyboard (1, endpoint `0x82`,
64-byte max packet, interval 8), and boot mouse/trackpad (2). The host C test
feeds this real descriptor through the kernel's selection code and checks that
it prefers the keyboard over the other two interfaces. This validates parsing
and endpoint context construction, not physical USB transport or Apple reports.

`phison-13fe-6700-config.bin` contains the 44-byte configuration descriptor
captured from the attached flash drive on 2026-10-17. It contains no serial
number or user data. Interface 0 is SCSI transparent/Bulk-Only Transport, with
IN endpoint 0x81 and OUT endpoint 0x02, 1024-byte packets and SuperSpeed burst 3.
Tests cover parsing, both endpoint context sizes, malformed descriptors,
shared HID/bulk completion routing, and timeout quarantine. These tests do not
establish that physical flash-drive I/O works; that requires a laptop boot.

`apple-05ac-0291-capture.txt` and `.done` are the unmodified report and completion
marker from physical run `69fe1189e88fb65d` on 2026-10-17. They contain bounded
raw Type 3 trackpad samples and the explicit keyboard test string `AaZz09`.
The C decoder tests replay the touch, multi-contact and physical-button samples;
the Python collector tests validate the original report and marker checksums.
See [the capture result](../../docs/trackpad-capture-2026-10-17.md#physical-result--2026-10-17)
for provenance, counters and the distinction between transport and gesture acceptance.

`apple-05ac-0291-pointer.txt` and `.done` preserve physical desktop trial
`0133f0fdf2364d7b` on 2026-10-17. The collector regression verifies their original
checksums and pointer/keyboard measurements. User confirmation of typing,
clicking, dragging and resizing is recorded in the
[pointer trial result](../../docs/trackpad-pointer-2026-10-17.md#physical-result--2026-10-17).
The report counters alone do not establish pointer feel or window behavior.

`apple-05ac-0262-config.bin` is the 84-byte configuration from the privileged
MacBookPro11,2 Linux capture on 2026-10-19, USB root port 12. Device descriptor
and strings are excluded. Interface 0 is the boot keyboard (endpoint 0x83,
10-byte maximum packet, interval 8); interface 1 is non-boot vendor HID and
interface 2 is boot mouse/trackpad. The shared host test verifies keyboard
selection, both endpoint context sizes and poll length against this fixture as
well as the Air fixture. This passes on the existing driver; it does not explain
the reported native keyboard failure or accept physical transport/reports.

`apple-05ac-0290-config.bin` is the 84-byte configuration from MacBookAir6,1's
privileged Linux inventory on 2026-10-22 (root port 5, revision 0171). Device
descriptor and strings are excluded. It is byte-identical to the 0291 fixture:
boot keyboard interface 1/EP82, raw trackpad interface 2/EP83, 64-byte packets.
The Type 3/no-mode-switch selection follows Linux v6.12
[Wellspring8 mapping](https://github.com/torvalds/linux/blob/v6.12/drivers/input/mouse/bcm5974.c).
Host tests replay synthetic packets and transport failures through this PID's
capture-only admission, and refuse other revisions, pointer/gesture modes and
incorrect descriptors. This is not native packet or pointer acceptance.
