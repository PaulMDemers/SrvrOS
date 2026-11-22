# Bounded DHCP identity comparisons

`wifi-dhcp-identity5` records comparisons against the current bounded DHCP
transaction. It stores only counters and masks, never addresses, XID, packet
bytes, private equality digests, key material or borrowed packet pointers.
The temporary TX diagnostic owner is cleared before the DHCP call returns; it
is separate from the poll/worker/provider admission owners and grants no authority.

The network layer observes construction before publication and observes RX
only after the existing worker-owner check, before Ethernet/IP/UDP filtering.
Thus a server-port ACK can be observed here even if a later admission gate
rejects it. TX is construction intent, not USB/radio/AP/server receipt. Existing
packet/USB witnesses and protocol rejection codes remain independent evidence.
Rendering occurs with the existing HTTP report after local native closure,
never in the live DHCP window. No deadlines, waits, clock reads, retransmissions,
parser acceptance or key/DMA retirement policy change.

| Mask bit | Comparison |
| --- | --- |
| 1 | Bounded framing/options, IPv4 and present UDP checksum, positive TTL; client parameter/client-ID options have expected shapes |
| 2 | Ethernet source equals current station |
| 4 | Transaction ID equals current DHCP XID |
| 8 | BOOTP client hardware address equals current station |
| 16 | Client identifier equals Ethernet hardware type/current station |
| 32 | Client requested address equals selected Offer (absent for Discover), or server yiaddr equals selected Offer |
| 64 | Selected server option matches (absent for Discover) |
| 128 | Client selecting BOOTP fields: zero hops/seconds/addresses and broadcast flag |
| 256 | Client IPv4 source zero and destination broadcast |
| 512 | Ethernet destination is current station or broadcast |

All bits (`1023`) classify a Discover/Request as matching the current client's
selecting transaction. These are field comparisons, not cryptographic packet
identities or proof that a DHCP server received our Request. Other clients,
malformed candidates and server messages have separate counts/last masks.
Server source/relay binding and ACK admission remain the production parser's
responsibility; a mask cannot override it. UDP checksum zero is permitted for
IPv4, matching the existing parser.

The decoder checks canonical schemas, finite masks/counters, ordered epoch and
original DHCP-counter bindings. Absent historical witnesses yield no acceptance.
A fresh collector must save this observer before full success proof and qualify
its missing/malformed companion handling for the selected source; the old closed
hardware collectors must not be edited or rerun to manufacture these new facts.

Native cases32/34 reproduce two Discovers, a late Offer, a Request, an authenticated
client-port echo, one unrelated multicast IPv4 Ethernet rejection, and absent
ACK through the actual native HTTP/CCMP/provider path and actual RF inverse.
Case34 changes the echo's XID, which must not count as our Request. Case33 delays
a valid ACK350ms after Request and must complete both HTTP lifetimes. The AP
independently checks client-ID, XID, requested/server options, selecting flags,
source/destination and IPv4 checksum. Group noise uses a valid PN above the
synthetic M3 RSC; no production replay check is relaxed. Guard-page malformed
and truncated-span tests verify the comparison helper reads only bounded input.
HCD/AP/registers/scheduler/child remain modeled; physical acceptance is pending.

All 35 native HTTP cases pass normally and with ASan/UBSan at `26e25ced`.
Final source `0db97fbb` changes only the sidecar test and unit record; every
native runtime/fixture input is byte-identical, so those results carry forward.
The guard-page comparison suite and seven native-trace decoder tests pass. Shared native/HTTP/packet/empty-stack/workspace regressions and actual
guest ELF/socket/IRQ-worker HTTP/DNS pass separately. These tests reproduce the
no-ACK failure and reject a wrong-XID client echo; they do not identify the
physical loss's cause or accept a new hardware image. The installed8f/d81
hardware attempt remains closed failed and its hub service was never reached.

The first cold comparison run, source `04b7b381` / release `79235e8eda6bc8e4`,
saved the observer despite full protocol refusal. It records two valid client
Discovers, one received current-field/checksum-matched Discover (mask1023),
zero Requests and no server Offer/ACK candidates. It therefore cannot localize
the earlier Request/ACK failure; it instead records an earlier no-Offer outcome.
USB TX success and matching client fields still do not prove server receipt.
The prepared HTTP child also exits with its pre-GO deadline status24. Full
calibration/child lifetime orchestration needs separate native-path audit before
another hardware candidate; no timeout or admission relaxation is accepted.
