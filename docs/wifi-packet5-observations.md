# Bounded encrypted packet witnesses

`wifi-packet5` is an observation family for the native two-HTTP diagnostic.
Its summary identifies the connection epoch and count. Each frame binds index,
ticket and token to the existing encrypted/native completion ledger. Direction0
is TX and direction1 is RX. The decoder refuses malformed or unbound companions;
it never accepts a connection, DHCP reply, radio delivery or DMA retirement.

The witness stores only Ethernet byte count, structural role and DHCP option53,
sequence and existing USB publish/completion clocks. No packet bytes, addresses,
XID, credentials, keys or private fingerprints are retained. TX sequence is the
submitted header's intent; hardware/radio transmission is unproved. RX sequence
comes from the authenticated header. RX start is the receive-post time, and finish
is USB completion; neither claims an over-the-air arrival timestamp.

| Role | Structural meaning |
| --- | --- |
| 0 | Unknown or unavailable span |
| 1 | ARP EtherType with minimum span |
| 2 | Bounded IPv4, other than eligible UDP |
| 3 | Bounded UDP, other ports |
| 4 | UDP68 to67 (client traffic, including a possible AP echo) |
| 5 | UDP67 to68 (server candidate) |

Message0 means unavailable, absent or malformed BOOTP/options. Values1–8 are
option53 categories;3 is Request and5 is ACK. This parser deliberately does not
validate DHCP transaction/peer identity, addresses, checksums or network state.
The existing network parser remains the admission authority. Seeing role5/message5
therefore does not prove that DHCP accepted an ACK.

RX witnesses are collected only after the unchanged CCMP/Ethernet delivery path.
TX metadata describes the original provider loan before encryption; the row is
emitted only for its successful USB completion. No firmware reads, extra clock
reads, waits, active-window printing, key copies or gate/budget changes are added.
Formatting is deferred until the HTTP supervisor checks child/worker quiescence
and the adapter capture/connection guard has returned, avoiding additional console
work in the close-to-handoff gap. All contexts and DMA tombstones remain retained.

Native fixtures include an accepted Offer with a queued/completed Request and
missing ACK, an authenticated client-UDP echo and68 management frames, with full
failed-workload RF cleanup under the original two-second limit. A companion
fixture delivers its ACK after350ms and completes both fresh HTTP lifetimes.
The HCD, AP/registers, scheduler and child are modeled; these are software checks,
not physical acceptance or proof of the earlier hardware loss's cause.
