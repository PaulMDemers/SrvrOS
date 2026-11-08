# Pro11,2 ordinary provider RX integration

November 8, software investigation at f2cc9ac0 runtime foundation. This document
and `tools/test_xhci_wifi_rx5.c` are preparatory software evidence,not native
ordinary-stack acceptance. Parent owns production edits and physical trials.

## Existing path and missing connection

`xhci_wifi_provider5.inc:wifi_provider5_receive` already binds the original
post-M4 owner and exact generation,requires the provider's own receive buffer,
and calls `wifi_wpa5_poll(MT76_WPA5_DATA)`. That consumes one validated native
USB event,borrows the original reusable DMA page,copies the raw snapshot,
normalizes/decrypts using the exclusive original CCMP context,commits replay
state once,copies Ethernet to the provider buffer,releases the DMA borrow and
posts the next receive position. This path needs no second key owner or
additional plaintext queue for its present single-consumer serialized contract.

The present data window constructs `mt76_ip5_io.poll=wifi_data5_poll`,which polls
directly into the private diagnostic's receive buffer. Thus v132/v133 exercise
provider TX but bypass `net_device_poll` and `mt76_net5.poll` on RX. The production
provider poll already invokes `net_handle_ethernet_frame`,but is not driven by
that diagnostic. Its existence alone is not native ordinary RX acceptance.

The reusable path masks IRQ around the short native ownership transaction.
`wifi_wpa5_dispatch` handles at most one event per call;it does not wait for RX.
Provider callbacks remain nonblocking,may execute with IRQ disabled,and must
not log,perform EP0/register traffic,allocate or wait for a connection event.

## New integration fixture

The fixture includes the unmodified actual native WPA/join/reuse owner and real
provider registry. Only its private IP-oracle call boundary is substituted:
`rx5_ip_run` copies the existing IO contract and changes its poll to
`net_device_poll(1,false)`. Production `mt76_net5.c` is separately compiled with
`net_handle_ethernet_frame=rx5_deliver`. The test sink copies already authenticated
Ethernet back to the strict private IP oracle. The historical fixture's original
asserting stack sink remains intact. No production packet parser,key,completion,
provider or hardware gate is replaced. USB/RF/AP/entropy are the existing mock
boundary with independent synthetic encrypted AP vectors.

This proves the native receive callback can deliver the six strict diagnostic
responses through real provider/registry callbacks. The independent IP oracle
checks their DHCP/ARP/DNS/exact256-byte echo contents. The test checks one replay
domain advances per delivery,duplicate reprocessing cannot publish or alter PN,
all DMA borrows are released before delivery,exact epoch/span refusal before
USB consumption,padding/strict sole metadata-bit admission,MIC/peer rejection,
and disconnect during delivery refuses early context release and recursive poll.
For quiet MIC/peer rejection cases only,the mocked clock advances100ms per empty
poll to reach the unchanged production2s deadline without slow repeated snapshots.
It does not link the production Ethernet/IP/TCP stack or prove native receive
interrupt wake behavior,persistence or UI networking. The delivery-symbol rename
must be visible in the build rule;linking the unmodified legacy assert sink is
not this test.

## Minimal production integration sequence

1. Preserve all historical flags/checkers. Add a separate opt-in ordinary-stack
   receive trial,not a silent reinterpretation of v133 receipts.
2. Keep one original secured CCMP owner. Replace the diagnostic data-window
   consumer for the new trial with bounded production network initialization/
   worker polling. Bind registry/provider before network selection. Do not start
   both the private IP oracle and production DHCP consumer on the same RX stream.
3. Production `net_device_poll` delivers through the existing native receive
   callback into `net_handle_ethernet_frame`. Add aggregate delivery/drop/callback
   counters and an explicit native receipt. Native checks must distinguish this
   route from private IP diagnostic receive;mock test counters are not receipts.
4. Ordinary stack sends are not limited to six exact diagnostic requests. Remove
   those assumptions only in a separately owned persistent transport extension:
   `wifi_provider5_submit` currently limits data_sends<6 and sends=4+index;
   `wifi_data5_owned` limits PN<=6;raw arrays/snapshots and event transcripts have
   immutable48/11-position bounds. These must not simply be widened or wrapped.
   Bind persistent TX/RX with independently checked native wrap/stop/event debt,
   cycle and Set-TR-Dequeue proofs before running arbitrary DHCP/TCP traffic.
5. Retain original CCMP replay/MIC/peer/length admission and one-time PN reservation.
   Stop publication before erasure;drain callbacks/loans and actual hardware/event
   debt before detach. Disconnect from within a stack callback must pin context
   until callback return. Old generation completions must never touch current
   queue state. Provider permanent registry retirement differs from connection
   detach/reconnect;do not retire the boot-stable adapter on each disconnect.
6. Qualify full actual-owner/provider/production-stack fixtures,then a serial
   matched build,sanitizers,exact-stage VM,storage/recovery and cold native trial.
   Require production DHCP/DNS/echo plus native ordinary RX receipt and full
   teardown. Follow with multi-packet TCP/HTTP and sustained transport acceptance.

## Local qualification

Final fixture passes GCC GNU11 `-O2 -Wall -Wextra -Werror` and a separate
`-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined` build with fatal
ASan/UBSan and leak detection enabled. Both compile with
`-DSRVROS_MT76_NET5_HOST_TEST -I kernel/include -I shared/include`.
Compile `kernel/src/drivers/mt76_net5.c` separately with
`-Dnet_handle_ethernet_frame=rx5_deliver`,then link that object with the new
fixture,`mt76_dma5.c`,the exact Makefile `WPA5_SOURCES`,`mt76_join5.c`,
`mt76_mac_cycle.c` and `mt76_rx.c`. Outputs used private temporary directories;
no shared build/stage/VM/hardware run occurred. No native source was changed.
