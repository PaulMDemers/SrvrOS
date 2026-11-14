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
EXT (`0x1798`) before the consuming FIFO (`0x1718`), stopping at the first empty
entry. Version2 allows at most81 pairs: the unchanged80 owned native TX
publication slots plus one empty probe, tied by a compile-time assertion. This
is a finite observation work allowance, not an asserted hardware FIFO depth or
one-status-per-packet guarantee. Version1 historical records retain their
original17-pair allowance. Neither version may credit a capped nonempty prefix. There are no FIFO reads, waits or console output inside
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
summaries and raw entries. New records use version2; the decoder separately preserves
version1 and refuses mixed versions. `tools/wifi_dhcp_radio5_log.py` requires exact schemas
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

## Version2 completeness and reliability plan

The prior17-entry assumption was not a device specification. The first native
run held more than17 statuses after a25-or-fewer-data-packet workload. Linux's
[MT76x02 status reader](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_mac.c)
checks the valid flag after consuming EXT/FIFO; its16-read counter-reset loop
must not be treated as a FIFO-depth specification. SrvrOS instead bounds work
from its existing maximum80 owned publications, with one extra empty probe.
Unexpected extra/duplicate/foreign statuses can still exhaust the allowance;
only observed empty termination permits full-prefix classification. The100ms
returned-call budget and original retention behavior on timeout/read/ownership
failure remain unchanged. No live callback reads or polling retries are added.

Native cases49–55 exercise actual doorbells/completions and post-workload
cleanup with more than17 queued statuses, a duplicate beyond entry17, all DHCP
tags present but a nonempty tail,80 valid statuses plus empty under the original
clock budget, and control/ownership/clock failure after19 complete pairs.
Original HTTP, native retention and full RF-parent tests remain required.

A reliability series uses three separately collected cold boots, each with two
fresh connection lifetimes under the same source and fixed profile. Record
protocol/cleanup/preservation separately from radio correlation; DHCP's existing
single retry remains permitted. Stop on failure or incomplete evidence. An old
version1 capped result is not reclassified or counted as a complete version2
radio sample. Three samples do not establish persistent or general reliability.
Each changed source/collector/image still needs its own qualification.

## Native result, November 13

Source `cbf807de`, release `a2b2ed09ff25187b`, completed one cold fixed channel 161 at 20 MHz
Demers5 run with two fresh encrypted DHCP/DNS/actual-process HTTP+EOF lifetimes
and full key/native/provider/RF restoration, independently reviewed. Both
preparation-v2 boundaries passed. All 489 prior files and eight prior recoveries
survived; this is bounded protocol acceptance, not general reliability.

IDs 3/4/5 and 6/7 match the original five DHCP packet/USB receipts. Every ID appears
once in its final snapshot prefix, without baseline collisions or binding
errors. Both baselines terminate empty, but both final snapshots contain 17 valid
entries and do not terminate empty. Accordingly the unchanged observer marks
all five correlations inconclusive, crediting neither MAC ACK nor failure.
The consuming reads themselves completed within their original budget. Do not
reinterpret this bounded prefix as proof that no later duplicate exists or
raise the limit simply to turn this result into a pass. Further completeness
work needs a justified bound/boundary and native-path tests before hardware.

The matching collector ran once; its sealed primary and separate independent
review remain immutable. See the current [unit handoff](machines/macbookpro11-2.md#current-handoff)
for source, session and acceptance limits.

## Version2 first physical result, November 14

Source `b4759eda` / release `2f83523f6097b854` reaches empty after24 final
EXT/FIFO pairs in32,056µs on the first connection, with three uniquely bound
DHCP IDs reporting MAC acknowledgements. Its baseline ends empty after five
pairs. This is the complete observation that the older17-entry cap could not
provide; it remains a MAC report, not server-delivery or whole-run acceptance.

The second connection fails during the four-way handshake after M2, before any
DHCP publication or radio capture. The strict whole-run observer therefore
refuses a missing radio epoch and the candidate-specific completeness result
remains false. Raw first-epoch rows survive; separate diagnostic-only projection
checks can inspect those rows without changing the primary, replacing the
collector result or granting protocol acceptance. The three-cold-boot series
stops at this failed first trial. See the unit handoff for immutable evidence,
preservation checks and the next native handshake audit. No identical retry is
scheduled and no handshake/radio/protocol timeout was widened.

## Reference

Linux v7.0's [MT76x02 MAC implementation](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_mac.c)
reads EXT before FIFO and decodes packet ID, retry, ACK request and success.
The [register definitions](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_regs.h)
define the raw fields. [MT76 packet IDs](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76.h)
distinguish software status IDs starting at 3 from no-ACK and no-skb IDs;
the original descriptor value 128 must not be interpreted as a no-ACK request.
