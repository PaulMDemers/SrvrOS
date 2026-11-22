# Pro11,2 Wi-Fi failure research — November 11, 2026

Research baseline: SrvrOS `c8ecd707`, installed runtime `d413e338`, release
`e4e95e7d5e24fc7c`. This is a source/specification comparison, not a new hardware
result or a root-cause claim. No kernel edits, installation or reboot were made.
Fetched main `5e72e2dd` includes PR467/468 filesystem work; it is not integrated
here. PR469 filesystem, PR301 GPU and PR379 WebCore scopes remain independent.

## Sources and applicability

### Subsequent AUTH-delivery audit

The b9f78149 native return receives all five calibration replies and completes
the first production DHCP/DNS/HTTP lifetime. The second AUTH has one successful
USB OUT completion but no peer reply within the original750ms;25 unrelated
management frames pass the receive decoder. There is no recorded xHCI/ownership
fault. These observations cannot distinguish a request lost before radio TX,
an unacknowledged radio request, or reply loss/filtering after AP acknowledgement.

[Linux's MT76 TX status reader](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_mac.c#L252)
reads EXT0x1798 before read-to-pop FIFO0x1718. The
[register definitions](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_regs.h#L579)
describe valid, success, ACK-request, WCID, rate, retry and packet-ID fields.
Linux resets up to16 entries and regularly consumes TX status. This provides a
bounded diagnostic; an empty status FIFO cannot prove a packet was not sent.
SrvrOS's current TXWI uses ACK_REQ, WCIDff and packet-ID80. A USB success is
therefore not a substitute for inspecting a separately reported radio result.

The audit found no demonstrated OUT-toggle mismatch: the fresh connection
explicitly clears endpoint04 halt/toggle, then configures a fresh DCI8 ring.
Management requests carry manual sequence numbers with hardware assignment
disabled. Linux's TXWI NSEQ bit requests hardware assignment; setting it
speculatively would change the sequence contract. Repeated fixed sequence
numbers across connection generations deserve later protocol work, but are
not established as this AUTH miss's cause. Neither is USB aggregation: the
configured DMA policy leaves aggregation disabled. The current scalar logs
do not establish that every possible received frame survived filtering.

The next connection-only diagnostic drains older TX status before AUTH and
samples again after the AUTH response or unchanged timeout, before any ASSOC
publication. Each snapshot reads at most17 EXT/FIFO pairs, records saturation
and emptiness, retains raw nonsecret status words and prints only after MAC
and endpoint cleanup. There is no added wait, retransmission or status-based
success rule. A control/ownership failure retains native quarantine. Modeled
ACK, failed-ACK, no-status and saturated histories must all leave a missing
AUTH unsuccessful through the actual native connection/HTTP path.

Linux references were downloaded at **v6.12**
(`adc218676eef25575469234709c2d87185ca223a`) and **v7.0**
(`028ef9c96e96197026887c0f092424679298aae8`). The relevant response policy and
calibration order agree across those releases. Links below use v7.0. Reference
copies, retrieval failures, tag resolution and SHA-256 inventory are local in
`build/research/wifi-2026-11-11/`; raw third-party files are not committed.

| Reference | Why it matters | Limits |
| --- | --- | --- |
| [Linux MT76 USB MCU transport](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_usb_mcu.c#L37) | Command serialization, sequence matching, response waits | Shared x0/x2 transport, not a firmware timing specification |
| [Linux MT7610 PHY calibration](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c#L861) | Correct family, calibration order and channel-dependent arguments | Board/TSSI/USB branches still matter |
| [Linux firmware loader](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/usb_mcu.c#L67) | Firmware choice and startup checks | Version equality must be measured, not assumed |
| [Linux MT76 USB queues](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/usb.c#L721), [USB synchronous messaging](https://github.com/torvalds/linux/blob/v7.0/drivers/usb/core/message.c#L51), [host API](https://docs.kernel.org/driver-api/usb/usb.html) | Timeout cancellation and worker/URB lifetimes | Linux supplies cancellation machinery our diagnostic path intentionally lacks |
| [Linux xHCI transfer handling](https://github.com/torvalds/linux/blob/v7.0/drivers/usb/host/xhci-ring.c#L2234) | Stop-event ownership and ring publication | Controller quirks must match this host |
| [Intel xHCI 1.2b, document 625472](https://cdrdv2-public.intel.com/625472/625472_xHCI_Rev1_2b.pdf) | Sections 4.6.9, 4.11.2/3 and table 6-10 | Apply revision/capability gates; modern optional features do not become available on older hardware |
| [Ralink vendor MCU source mirror](https://github.com/GregersP/mediatek_mt7610u_sta_driver_linux-64bit/blob/95fd2aa84ad774b3a8d1636c32420d8676dcc347/mcu/mcu_and.c), [chip calibration](https://github.com/GregersP/mediatek_mt7610u_sta_driver_linux-64bit/blob/95fd2aa84ad774b3a8d1636c32420d8676dcc347/chips/mt76x0.c#L3045) | Independent implementation of command ACK queues, sequencing and calibration | Older GPL vendor code in a third-party mirror; not a current MediaTek specification or drop-in driver |
| [Edimax EW-7711ULC datasheet](https://www.edimax.com/edimax/mw/cufiles/files/download/datasheet/EW-7711ULC_Datasheet_English.pdf) | Actual product is USB 2.0 and 5 GHz | Generic MT7610 dual-band capability does not describe this assembled adapter |
| [Linux usbmon documentation](https://docs.kernel.org/usb/usbmon.html) | Comparative command/response timing capture | Observes driver/HCD requests, not independent electrical USB transactions |

No public MediaTek document defining the complete MT7610 calibration-response
timing and timeout recovery contract was located in this search. Generic Wi-Fi
whitepapers do not supply that missing device contract. Driver implementations
are the strongest protocol references found; their policy choices are not
guaranteed hardware limits.

## 1. MCU receive policy differs materially

Linux sends the command with a 500 ms OUT timeout, then tries up to five 300 ms
response receives. It serializes commands, rotates nonzero four-bit sequence
IDs, and checks the returned sequence/event type. Timeouts and unrelated replies
can consume receive attempts; the command is not resent by this loop. Thus the
nominal response budget can approach 1.5 seconds. This policy is present in both
examined releases. [Transport source](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x02_usb_mcu.c#L37)

SrvrOS `wifi_mcu_bulk` has one 500 ms receive deadline. `wifi_cal5_command`
requires the first completed receive to contain the exact captured 16-byte reply;
the calibration packet uses sequence IDs 1–5. Both use a 1,024-byte response
buffer. These are established implementation differences, **not established
bugs**. The latest native failure observed no relevant USB completion at all,
so relaxing reply-content validation would not address that recorded outcome.
Nor does our software bulk counter 3817 identify the four-bit firmware sequence.

Linux's bulk timeout path calls `usb_kill_urb` before returning. Repeating its
receive attempts therefore relies on canceling the previous transfer and joining
completion. SrvrOS instead quarantines the failed MCU/EP0 state and retains its
pending DMA. Adding a retry on that same buffer/ring without a cancellation
contract would introduce overlapping ownership. [Synchronous message source](https://github.com/torvalds/linux/blob/v7.0/drivers/usb/core/message.c#L51)

**Inference:** late firmware completion remains plausible and measurable. It has
not been observed. A longer production deadline alone would conceal the
distinction between latency, missing completion delivery and device failure.

## 2. The main calibration recipe is consistent with Linux

For power-on calibration on channel 161, Linux issues R(0), VCO(161), FULL(0x901),
LC(1), then RXDCOC(1), with 10–20 us, 500–700 us and 15–20 ms sleeps at the
corresponding boundaries. Temporary ALC/IBI settings are restored. SrvrOS's
five-command recipe, arguments and minimum delays agree. The selected board
profile excludes internal TSSI, so Linux's conditional TSSI branch is not simply
a missing mandatory command. Linux's PHY routine does not propagate each
calibration call's error, so a usable Linux interface alone cannot prove every
calibration command succeeded. [PHY source](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/phy.c#L861)

This comparison does not establish equivalent timing or all preceding state:
SrvrOS performs many guarded register transactions and logs before calibration.
The latest timeout follows roughly 1,906 earlier response receives. Compare the
whole command history, queue routing and interval before R, not only the five
calibration commands. Linux also prefers `mt7610e.bin` before falling back to
`mt7610u.bin`; using an image named for PCIe is therefore not itself a mismatch.
Our pinned image and the actual Linux-loaded bytes still need comparison in the
next reference capture. [Loader source](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/mt76x0/usb_mcu.c#L67)

The vendor mirror separately maintains outstanding ACKs by sequence and
distinguishes self-generated events. Its generic retry scaffolding is not a
safe recipe: this revision initializes `need_retransmit` to false even when the
parameter is true. Do not infer active retries from call arguments alone.
[Vendor command implementation](https://github.com/GregersP/mediatek_mt7610u_sta_driver_linux-64bit/blob/95fd2aa84ad774b3a8d1636c32420d8676dcc347/mcu/mcu_and.c#L604)

## 3. Endpoint snapshots and Stop events have different meanings

Intel specifies that a non-stream endpoint's output TR Dequeue Pointer is
undefined outside Halted/Stopped states. A running snapshot at the ring base
cannot prove stalled hardware. Stop writes final dequeue/cycle state before its
command completion, and may first emit a stopped transfer notification even
between TDs. TRB parameter/status publication must precede control publication;
the cycle bit publishes ownership. Short-transfer residue and capability-gated
Stop codes must be interpreted separately. [Intel specification](https://cdrdv2-public.intel.com/625472/625472_xHCI_Rev1_2b.pdf)

Linux leaves stopped TD retirement to Stop-command handling. Its producer also
uses a write barrier before publishing the first TRB's cycle bit and ringing
the endpoint. [Linux xHCI implementation](https://github.com/torvalds/linux/blob/v7.0/drivers/usb/host/xhci-ring.c#L2234)

Our previously fixed early Stop-context validation and empty-ring notification
refusals are consistent with these distinctions. They do not explain the latest
R0 wait, which has not issued Stop. The installed MCU publication instructions
were already inspected: parameter/status precede control. General compiler/DMA
publication deserves audit, but this research does not establish a new ordering
defect. No relevant event among 25 dispatched transfers is evidence about the
dispatcher, not proof that the adapter transmitted nothing on USB.

## 4. Disconnect is an explicit cancellation operation

Linux MT76 disables the RX worker and poisons its receive URBs; resume unpoisons
and resubmits them. TX shutdown waits for pending work and kills URBs when
needed, then joins associated work. This separates application completion from
USB completion and prevents callbacks from resurrecting old work. [Queue source](https://github.com/torvalds/linux/blob/v7.0/drivers/net/wireless/mediatek/mt76/usb.c#L721)

The USB API requires disconnect to synchronize outstanding URB activity before
returning. Consequently, finishing HTTP or returning all software packet loans
does not retire a still-posted raw receive. [USB host API](https://docs.kernel.org/driver-api/usb/usb.html)

For SrvrOS, audit the stop-accepting-work, worker join, outstanding raw RX, Stop
completion, endpoint rebase and key/provider retirement transitions separately.
An ownership refusal is not permission to free a retained object. The prior
paired-HTTP hold still lacks its first exact reason; the latest run stopped
before connections. Neither can be assigned to missing EOF or any one Stop
condition from this research alone.

## 5. Similar reports that do not identify our bug

- Intel-authored 8-series errata, document 328905, describe stale TRBs after
  active hub removal/reconnection and short-packet scheduling/event problems.
  The original Intel URL failed retrieval; indexed text from the
  [Intel document mirror](https://www.mouser.com/datasheet/2/612/8_series_chipset_pch_spec_update-263662.pdf)
  is a lead, not a completed stepping audit. This host reports 8086:8c31 rev05,
  HM87. The [official mobile U/Y-family update](https://www.intel.la/content/dam/www/public/us/en/documents/specification-updates/4th-gen-core-family-mobile-i-o-spec-update.pdf)
  contains related errata but is a different product scope. Do not apply LP-only
  quirks or blame an erratum without matching product, stepping and trigger.
- A recent [MT7612U patch submission](https://lkml.rescloud.iu.edu/hypermail/linux/kernel/2610.3/00635.html)
  reports long SuperSpeed TX aggregation wedging the device and subsequent MCU
  timeouts. This is a different chip, USB speed and workload; it cannot explain
  our first-R0 failure before network TX. The shared timeout message is not a
  shared diagnosis.
- [FreeBSD mtw](https://github.com/freebsd/freebsd-src/blob/main/sys/dev/usb/wlan/if_mtw.c)
  contains some MT7610 names, but the examined attachment rejects ASICs other
  than MT7601. It is not a working MT7610 reference despite those identifiers.
- The Edimax product is 5 GHz only. Demers2.4 discovery cannot be accepted with
  this adapter. This is a capability boundary, not today's calibration fault.

## Proposed experiments, in order

These are research recommendations, not an armed test or an implemented fix.

1. **Capture a successful Linux initialization on this exact adapter.** Match
   firmware hash/version, endpoints, queue selection and fixed-channel setup.
   Record OUT submission/completion, IN submission/completion and firmware
   command sequence/timing using usbmon plus available xHCI tracepoints. Reduce
   the saved analysis to MCU/control metadata; keep network/key material private.
   usbmon is an HCD observation and cannot settle an HCD-versus-wire dispute.
   [Capture limitations](https://docs.kernel.org/usb/usbmon.html)
2. **Observe lateness without converting failure to success.** A separate bounded
   diagnostic can retain the original 500 ms failure and continue observing the
   same pending TD to a Linux-comparable horizon. No command resend, new TD,
   buffer reuse, EP0 recovery or normal-path timeout increase. Record bounded
   MCU-buffer change classification, event producer/consumer/cycle observations
   and exact completion dispatch. Keeping the descriptor and DMA alive must be
   covered in actual native-path tests before any physical diagnostic.
3. **Distinguish the outcomes.** A matching late completion supports a latency
   hypothesis. Buffer changes without a dispatched completion point toward
   partial transfer or event/visibility handling, not a proved completed reply.
   A relevant event seen but rejected identifies dispatch/admission work. No
   observed buffer/event progress still does not distinguish USB NAK/firmware
   nonresponse from controller failure; compare independent evidence before reset.
4. **Compare long-history and short-history command transport.** Cover the
   actual 63-position MCU ring, Link cycles, event-ring wrap, firmware sequences
   and dispatch under the observed preceding workload. Existing injected
   timeout/delay fixtures establish containment only. Do not claim that adding
   another injected failure reproduces the spontaneous hardware cause.
5. **Return to paired HTTP only after calibration is repeatable.** Persist the
   first ownership failure before console formatting or a hold. Exercise
   cancellation racing with a final packet, an empty stopped ring, partial/short
   completion, queued callbacks, real EOF, and a second connection with old keys
   and callbacks barred. Gate buffer reuse on completed hardware cancellation.

The strongest actionable discrepancy is response-wait/cancellation policy.
The strongest missing evidence is where the first absent firmware response is
lost. Neither the calibration recipe comparison nor the errata search supplies
a justified production fix yet. Existing native successes and failed captures
retain their original source-bound status.
