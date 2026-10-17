# Hardware descriptor fixtures

`apple-05ac-0291-config.bin` contains the 84-byte USB configuration descriptor
read from `/sys/bus/usb/devices/1-5/descriptors` on the target MacBookAir7,2
on 2026-10-16. The device descriptor and any strings/serial identifiers are not
included. Linux identifies the source device as Apple `05ac:0291`.

The three interfaces are non-boot HID (0), boot keyboard (1, endpoint `0x82`,
64-byte max packet, interval 8), and boot mouse/trackpad (2). The host C test
feeds this real descriptor through the kernel's selection code and checks that
it prefers the keyboard over the other two interfaces. This validates parsing
and endpoint context construction, not physical USB transport or Apple reports.
