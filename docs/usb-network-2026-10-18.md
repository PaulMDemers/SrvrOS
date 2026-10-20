# USB Ethernet foundation — 2026-10-18

The IP stack now selects a network provider at boot. Existing e1000 remains the
preferred provider when both it and a USB adapter are present. `net0` is the
userspace interface name. There is no live failover or re-enumeration yet.

`srvros.usb-network=ecm` enables the first USB provider: CDC Ethernet Control
Model (ECM). It is enabled in the v21 and current v22 desktop profiles. It discovers the
control/data interfaces, validates their descriptors, reads the advertised MAC,
sets the data alternate setting and packet filter, and uses asynchronous xHCI
bulk transfers. Receive, transmit and notification buffers remain owned by the
driver until completion. Transmit copies accepted frames into an eight-frame
queue; completion deadlines and disconnects quarantine the interface without
freeing memory that the controller could still access. An exact endpoint-packet
multiple is terminated with a separate zero-length transfer.

This is **not** a claim of support for arbitrary USB Ethernet adapters. ASIX,
Realtek RTL8152/8153, RNDIS and CDC NCM need their own implementations. Capture
an adapter's VID/PID and interface descriptors before choosing a driver. The
hardware collector now saves allowlisted USB topology/interface metadata and
raw descriptors, without collecting serial-number strings or network traffic.

The provider reports link state from CDC notifications. Speed and duplex remain
unknown when the device does not provide them. The Devices tab distinguishes a
bound USB network driver from an unclaimed USB device. The Network tab uses the
same cached status API as e1000; it does not issue USB transfers while painting.

## Validation

`make test-core` includes strict descriptor/MAC/notification parsing, provider
selection, queue ownership, stale completion rejection, receive wakeup, bounded
failure, disconnect and 300 transmit/ZLP ring-wrap transactions.

```sh
tools/host-env.sh python3 tools/usb_ecm_smoke.py
tools/host-env.sh python3 tools/usb_ecm_smoke.py --soak-seconds 1800
```

The disposable VM has USB Ethernet and no PCI network card. It checks DHCP,
ICMP, UDP echo, packet-capture lengths, the existing HTTP service, concurrent USB
keyboard reports and interface quarantine after unplugging. Alternating 64- and
65-byte Ethernet frames exercise the exact-packet termination case. QEMU's
`0525:a4a2` gadget advertises RNDIS first and ECM second; configuration selection
is tested. Its notification interface-index quirk is accepted only for that
VID/PID. Physical adapter acceptance is pending.

## First physical adapter round

Attach the adapter and Ethernet cable before boot. In System > Devices, record
its vendor/product IDs and whether it is identified as USB Ethernet with an
active driver. An unclaimed device is useful inventory evidence, not a working
network provider. Its Linux descriptor capture determines the next driver work.

For a bound ECM adapter, open Terminal and run `dhcp`, then `net`. Check the
assigned address/router in System > Network before trying Network Utility.
The GUI does not configure DHCP itself yet. Use a known reachable LAN peer for
the first ping, then DNS and HTTP. Preserve the session logs and note cable/link
behavior. Re-enumeration after unplugging is a separate milestone; this driver
quarantines a disconnected interface for the rest of the boot.

Protocol reference: the original USB-IF [ECM 1.2 specification](https://home.codevoid.de/paste/ECM120.pdf).
The [QEMU 9.2 USB network device](https://raw.githubusercontent.com/qemu/qemu/v9.2.0/hw/usb/dev-network.c)
provides the VM interoperability reference. No Linux driver implementation was
copied into this driver.

## Multiple adapters and diagnostics

The registry accepts up to four supported providers before IP initialization.
Each has a boot-local adapter ID, driver name, MAC and link state exposed through
an appended, size-negotiated `srv_net_status_info` inventory. System > Network
lists these below the active connection properties; `ifconfig` prints them too.
Network Utility names the active adapter in its results and copied report.
Unclaimed USB devices remain in Devices, not in the supported-provider inventory.

Only the boot-selected provider owns `net0`, addresses, ARP, DHCP and sockets.
e1000 has priority over ECM; equal priorities retain discovery order. Inactive
adapters have no IP configuration and are not polled for application traffic.
Link-up on an inactive adapter is not connectivity through it. Quarantined USB
adapters report disconnected even if their last notification said connected.
There is no live failover, adapter switching, simultaneous IP interfaces or
hotplug re-registration. IDs can change between boots; use MAC and USB topology
for physical identification. A second e1000 instance is not implemented by its
singleton driver. Multiple ECM instances can register independently.

Use `tools/host-env.sh python3 tools/usb_ecm_smoke.py --second-usb` for two ECM
adapters on separate emulated networks. The regression exercises active traffic,
inactive inventory and zero inactive application traffic, then unplugging the
active adapter without silently selecting the other one. System/Network Utility
host fixtures cover multiple adapters and immutable diagnostic report identity.
`desktop_network_smoke.py --with-usb` covers mixed e1000/ECM inventory, the
e1000 GUI, copied adapter attribution and link changes. ARP/DHCP/DNS/ICMP
timeout loops block on bounded RX waits so an unreachable peer cannot keep an
IRQ-masked caller runnable indefinitely and starve timer/UI progress.
Physical chipset support still requires captured descriptors and native tests.
The Pro sensor desktop's existing immutable recipe does not enable ECM; prepare
an explicit network-enabled candidate and exact-stage VM check when hardware is
available, preserving its accepted desktop recovery.
