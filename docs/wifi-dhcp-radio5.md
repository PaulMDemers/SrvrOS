# DHCP transmit and MAC status correlation

This diagnostic covers the two bounded native MT7610 HTTP connection contexts.
It does not change DHCP admission, retry policy, RF settings or retirement
authority. USB completion alone does not prove radio delivery; a MAC-reported
ACK does not prove that the DHCP server received or answered the packet.

## Capture contract

The original encrypted data builder assigns diagnostic TXWI packet IDs 3–5 in
the first context and 6–8 in the second to at most two Discovers and one Request.
Only the descriptor packet-ID byte changes. The encrypted MPDU, sequence,
PN, IV, MIC, rate, ACK intent and WCID retain their original values. Extra DHCP
packets retain the original ID and make the observation inconclusive.

The ID binds to the actual native lease ticket/token before the doorbell, under
the original publication IRQ guard. Its completion must match the original
packet kind, sequence and lease. No packet bytes or key material are retained.

A baseline snapshot runs after M4 completion and before data provider attachment.
The final snapshot runs after child, worker, TCP, receiver and key closure and
MAC TX/RX drain, before the native endpoint Stop commands. Each snapshot reads
EXT (`0x1798`) before the consuming FIFO (`0x1718`), up to 17 pairs, stopping at
the first empty entry. There are no FIFO reads, waits or console output inside
provider callbacks. The returned-call budget is 100 ms with no retry or delay;
the existing blocking EP0 helper itself has its existing timeout, so this is
not a new hard wall-clock bound inside that call.

Ownership is checked before and after every control read. USB read failure,
ownership loss or a returned clock overrun faults the original stream, retains
its context and forbids Stop/reuse or a second connection. Partial EXT/FIFO
receipts survive. A held failure freezes scalar diagnostics for display;
it does not perform additional hardware reads or permit unsafe disk saving.

## Interpreting results

`wifi-dhcp-radio5` records a versioned summary, scalar tag bindings, both phase
summaries and raw entries. `tools/wifi_dhcp_radio5_log.py` requires exact schemas
and matches normal records against the original packet and encrypted USB
completion receipts. Held records remain partial and cannot grant correlation.

A correlation requires both snapshots to terminate empty, exact completion
binding, one final matching ID, no baseline collision, the original WCID 255,
ACK request, nonaggregate status and rate 8192. It reports the MAC success bit
as `mac_ack_reported` or `mac_failure_reported`. Missing, duplicate, stale,
wrong-WCID, saturated or overflowed observations are inconclusive. FIFO overflow
can discard DHCP statuses during later HTTP traffic; missing is never failure.
All protocol, source/epoch, provider, endpoint and RF-parent checks remain
independent and mandatory. The observer always refuses native acceptance.

## Software validation and physical limits

The production native HTTP/CCMP/provider/USB orchestration C fixture supplies
actual doorbell metadata to an independent modeled 16-entry MAC FIFO. Cases
38–48 cover no DHCP ACK despite MAC ACK reports, no Offer with MAC failure
reports, overwrite, stale/duplicate/wrong-WCID statuses, bounded saturation,
partial control-read/owner/clock failure, baseline failure and disjoint IDs in
two complete lifetimes. The selective-status case preserves DHCP statuses for
positive matching; the overwrite case exercises the realistic loss separately.
The AP, HCD, registers, scheduler and child are modeled. These tests cannot
identify the physical no-Offer/no-ACK cause.

Qualification on source `6b8b821b` passed all 49 native cases normally
and under ASan/UBSan, pure helper/guard-page sanitizer checks, nine strict decoder
tests, all eleven saved native trace decodes, full synthetic RF-parent/legacy
composition, kernel/shared/control-timing regressions and a separate actual
guest ELF HTTP/DNS smoke. No physical acceptance follows from these models.

Before a physical run, qualify a source-specific collector that saves this
observer alongside DHCP identity and the existing full protocol/retirement
proof, plus the preparation-v2 requirement and an exact matched staged image.
Do not reuse or reseal the closed hardware evidence for a new source.

## Reference

Linux v7.0's [MT76x02 MAC implementation](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_mac.c)
reads EXT before FIFO and decodes packet ID, retry, ACK request and success.
The [register definitions](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_regs.h)
define the raw fields. [MT76 packet IDs](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76.h)
distinguish software status IDs starting at 3 from no-ACK and no-skb IDs;
the original descriptor value 128 must not be interpreted as a no-ACK request.
