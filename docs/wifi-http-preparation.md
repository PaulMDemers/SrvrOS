# HTTP child preparation at the connection boundary

The two-connection Wi-Fi diagnostic reserves its exclusive slot before the RF
parent runs. Reservation creates no child, worker, network scope or provider.
Both fresh children and parked workers are prepared at the existing key-free
connection boundary: after calibration/discovery/first-join cleanup, before
that connection's stream inspection. Generation two additionally requires the
unchanged first-connection closure and native handoff guards.

Previously generation one was prepared before the full RF parent. Its actual
userspace pre-GO wait has a 60-second deadline, so sufficiently slow parent work
can exhaust that budget before DHCP or GO. The recorded status24 denotes this
deadline; it does not identify the separate missing Offer/ACK cause. Moving the
preparation boundary keeps the 60-second pre-GO, 15-second HTTP and two-second
DHCP budgets unchanged. The legacy single-HTTP diagnostic is outside this fix.

Missing targets create no child or worker. Failed late preparation still uses
the original abort/retirement path and never enters a stream or next generation.
No failure latch, RF restoration criterion or native ownership check is relaxed.

New logs contain one `wifi-http-reservation5` receipt before firmware work and
two version-2 `wifi-http-reconnect5-prepare` receipts at their respective
connection boundaries. The full decoder requires first preparation after the
completed first-join parent and second preparation after first closure. Version-1
historical logs retain the original early-preparation ordering; mixed versions,
missing/duplicate/late reservations and early version-2 preparation are refused.
Synthetic qualification preserves the RF parent and explicitly marks itself;
it cannot supply physical acceptance.

Native cases35–37 cover the saved no-Offer pattern, a simulated 70-second parent
before any child/worker exists, and late first-worker failure. The no-Offer
fixture sends two Discovers, reflects only the retry with current transaction
fields, delivers one unrelated 62-byte multicast IPv4 frame and supplies no
server response. Success/failure uses production HTTP/CCMP/provider/native/RF C;
HCD, AP, registers, scheduler and child remain modeled. Separate actual httpget
source tests retain status24 at the unchanged pre-GO deadline and accept launch
after a slow parent. Neither those tests nor USB completion prove AP/server
receipt or physical acceptance of this change.

Source `27af43ef` passes all 38 native cases normally and under ASan/UBSan,
49 actual httpget cases with modeled syscalls, strict preparation/legacy decoding,
full synthetic RF composition and separate actual guest HTTP/DNS. Physical acceptance of that source itself and the separate Offer/ACK-loss
cause were unproved at that software checkpoint. A fresh
source-specific collector must require version2 for this candidate using the
preparation-order check; the generic parser's historical version1 support alone
must not authorize an old preparation layout under a new image identity.

The pinned [Linux packet-ID definitions](https://raw.githubusercontent.com/torvalds/linux/v7.0/drivers/net/wireless/mediatek/mt76/mt76.h)
identify128 as HAS_RATE, while NO_ACK is0. SrvrOS's data descriptor still
requests an ACK;128 is not evidence that ACKs were disabled. [Linux's MAC status reader](https://raw.githubusercontent.com/torvalds/linux/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_mac.c)
reads EXT before the consuming FIFO and reports success, ACK request, retry, WCID and packet ID separately from USB completion.
At that preparation-only checkpoint FIFO companions bracketed AUTH, not DHCP.
The later [DHCP radio diagnostic](wifi-dhcp-radio5.md) adds separate disjoint
packet IDs and bounded snapshots; an AP link ACK still cannot prove server receipt.
The preparation fix itself introduces no consuming reads or descriptor changes.

Native descendant `cbf807de` / release `a2b2ed09ff25187b` now independently passes
one cold channel-161/20-MHz two-HTTP run with both version-2 preparations at their
key-free boundaries, both child statuses 0, DHCP/DNS/exact bodies+EOF and full
retirement/restoration. The first DHCP cycle needs its existing Discover retry;
no waits/retries change and intermittent Offer/ACK-loss reliability remains open.
The absent-adapter exact-image verifier also now accepts reservation-only
without child allocation, separately from historical closed version-1 owners;
mixed or live-owner receipts remain refused.
