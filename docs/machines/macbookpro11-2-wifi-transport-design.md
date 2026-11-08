# Native persistent MT7610 transport handoff

November 8. Design and software regression,not native wrap acceptance. The parent
completion plan owns milestones and all staging/hardware. This subtask changes
only this document and tools/test_mt76_dma5_native63.c.

## Current ownership and the boundary mismatch

`xhci.c` uses64 TRBs:63 data positions and a Link. `ring_push_owned` writes
position62,rewrites the Link with toggle-cycle,then immediately resets software
enqueue to0 and flips cycle. The DMA ledger instead advances cursor to63 and
refuses prepare until a separately proved `mt76_dma5_wrap` fence.

v133 never reaches that discrepancy. RX is capped at48 positions (40 forward/
eight cleanup),TX at11,with four mapped pages/direction,linear unique TD addresses
and cycle1. Frozen checks bind cursor directly to posts/sends,last token to those
counts,and retain per-position snapshots. Provider sends still require exactly
six data requests and original management offsets. Removing caps or changing
pointer comparisons to modulo63 would bypass the late-event ownership proof.

The core admits one prepared/device TD per direction and up to four retained
completed/borrowed RX pages. Quiet RX has no timeout;pending TX has750ms. It does
not supply controller commands,event-debt knowledge,keys or independent evidence.
Fence booleans are inputs,not authority. The final v133 empty-ring drain is after
full parent restoration;it cannot establish a mid-connection wrap frontier.

## Minimal persistent owner contract

Use a separate explicit profile/owner and preserve historical v130–v133 receipts
and predicates. Share helpers only after regressions cover both modes. Retain the
same verified device,station/peer/epoch and exclusive CCMP owner from handshake
through disconnect. Keep four pages/direction and initially one device TD per
direction. Use monotonically increasing64-bit logical tokens,tickets and fence
sequences;check overflow and never reset at a ring boundary. Reserve CCMP PN once
in its original owner;never roll it back or retry uncertain ciphertext.

Replace fixed per-position transcript assumptions with eight page-owner records
and a bounded categorical trace. Current descriptor records bind physical TD,
page,cycle,epoch,ticket,token,length and endpoint. Keep each published identity
until independently resolved. Snapshots refer to latest page owner,not old logical
position. Guards bind real Link/cycle and ledger cursor. No lifetime counter may
index an old bounded array. Historical packet/snapshot arrays remain unchanged.

Provider callbacks do copied admission,checked lease/copy/seal/publication only.
No command waits,control transfers,delay or console in callbacks/RX delivery.
A CPU0 serialized worker owns commands/fences. IRQ handlers validate raw events
against retained records and schedule progress;they cannot derive new tickets
from raw pointers. Service returns promptly during a fence. Full queues/retained
loans cause backpressure;uncertain publication closes the secured port.

## Native fence state machine

Per direction:RUNNING → BOUNDARY → STOPPING → DRAINING → REBASING → RUNNING.
Any uncertainty goes to FAULT_RETAINED. Disconnect closes both publications and
uses separate full-parent retirement.

1. At ledger cursor63,block every new descriptor/doorbell in that direction.
   Native enqueue may already be0/opposite-cycle after position62;that software
   state does not authorize writing position0. Keep a separate publication gate.
   Provider may fill its normal copied queue but no TD crosses the gate.
2. First qualify the simple boundary:all63 TDs completed,RX copied/released,no
   active TD and all four direction pages free. Never revoke consumer loans.
   Initially serialize both endpoint fences in one worker. Keep original keys/
   PN and other-direction work only under explicit independent owner guards.
3. Issue exact Stop Endpoint for this slot/DCI. Match command-ring pointer/type,
   completion slot/success,generation and endpoint Stopped output state. Existing
   final-stop event expectations are phase-specific;do not assume every quiet
   stop emits cc27. Capture/document exact native context/event behavior in a
   separate profile. Pending RX cc26/28 are not ordinary successful data:the
   ledger only accepts cc1/13. Pending cancellation needs a separately proved
   stopped-transfer transition;first wrap trials can avoid it by finishing and
   releasing the last normal RX before fencing.
4. While stopped/publication closed,consume and acknowledge events through an
   observed empty frontier with IRQ and polling consumers serialized. Track
   command/transfer event debt. Every event matches a retained command/TD or is
   dispatched to its independent owner. Unexpected late target TD,wrong slot/
   DCI,duplicate,residue/CC error or bounded-drain exhaustion faults and retains.
   An empty read while the endpoint runs does not prove drained ownership.
5. With pages free,prepare descriptor/Link ownership so stale cycle bits cannot
   become device-owned. Issue exact Set TR Dequeue Pointer to ring-base with
   opposite DCS. Match command identity/success and stopped-context/dequeue
   readback;account for command consumption and all target event debt. Do not
   Reset Endpoint/CLEAR_FEATURE to force a healthy wrap or reset keys/PN. Keep
   output/input context reserved-bit and unrelated-endpoint restoration gates.
6. Construct ledger fence only from those native records:epoch,new globally
   monotonic sequence,direction,ring-base|opposite-DCS,stopped/drained/dequeue-set.
   Require mt76_dma5_wrap before prepare/publish. Reconcile native enqueue/cycle
   and descriptor guards with the ledger,then publish position0 and doorbell.
   Resume without reinstalling keys and invalidate old fence proof records.

An xHCI Transfer Event carries a TRB pointer,not the software ticket. Its cycle
bit describes the event ring,not the originating TD cycle. One wrap aliases an
address;two wraps alias address AND data-TD cycle. Raw equality cannot identify
an old completion. Looking up the current ticket from that raw pointer would
conceal the stale event. The independently stopped/drained hardware frontier is
essential. Ledger old-ticket rejection only protects correctly attributed leases.

## Disconnect and faults

Close provider publication before erasure. Stop the fence worker and drain
callbacks/loans without freeing borrowed data. Submission/stop/drain/Set-Dequeue
uncertainty pins rings/pages/context and refuses a fresh epoch. A failed proof
is not retry permission. Final retirement requires original software keys erased,
MAC drains/disable,exact endpoints/shared-input/power/RF originals,event debt
empty and ledger/provider retirement. A data completion never retires context.
Reconnect/reattach needs its separate fresh verified hardware owner/epoch.

## New standalone regression

The new test links production mt76_dma5 with actual63 usable positions:

- 8064 completions,4032 RX acquire/releases,128 synthetic fences;all eight pages
  reused1008times. RX and TX outstanding concurrently;second TD per direction
  refused. Exact descriptor address/opposite-cycle progression at each boundary.
- Every missing/wrong fence field,stale global sequence and pre-boundary fence
  refuses unchanged. Held last RX loan prevents wrap;release permits it. Four
  held loans exhaust pages;returning one permits reuse of exactly that page.
- Old epoch completion refuses without mutation. Late old lease after one/two
  wraps faults and keeps current device page pinned. Test explicitly shows raw
  address alias after one wrap,and address-plus-cycle alias after two.

Run from repository root with a private temporary output directory:

```sh
p=$(mktemp -d /tmp/srvros-native63.XXXXXX)
cc -std=c11 -Wall -Wextra -Werror -Ikernel/include tools/test_mt76_dma5_native63.c kernel/src/drivers/mt76_dma5.c -o "$p/test"
"$p/test"
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer -Ikernel/include tools/test_mt76_dma5_native63.c kernel/src/drivers/mt76_dma5.c -o "$p/test-san"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$p/test-san"
```

Normal and fatal ASan/UBSan/leak runs passed November8. Fence inputs here are
synthetic. No make,kernel/staging/VM/native tests,secrets or boot/archive changes.

## Next implementation order

Ordinary provider RX first. Then worker-owned persistent profile with one directed
TX63-boundary trial and independently saved stop/drain/dequeue receipts. Next
RX63-boundary with no loan,then concurrent direction/repeated boundaries. Add
command/event/loan/callback/late-event/disconnect fault injection to actual owner
fixture before staging. Follow parent matched-build,exact-image GPT/missing-
workspace,storage/recovery workflow. Only repeated native encrypted transfer
crossing real boundaries with full retirement accepts persistence. General TCP/
HTTP and UI/lifecycle follow. Synthetic fences do not need cold hardware boots.

## Production asynchronous fence engine

The follow-up adds mt76_fence5.h/.c,a nonblocking production engine over the DMA
ledger,and test_mt76_fence5.c. There is still no native backend or hardware
acceptance. Each step executes at most one IO action;no wait,allocation,MMIO,
DMA write,key copy or doorbell. Do not bind blocking submit_command_trb to it.

States:STOP,STOP_WAIT,STOP_CONTEXT,DRAIN,REBASE,REBASE_WAIT,VERIFY,FINAL_DRAIN,
COMMIT,terminal FAULT. Submitted/completed command records must exactly match
epoch,monotonic token,physical command pointer,slot,DCI,type and dequeue. Stop
context must be Stopped with legal DCS/alignment and pointer within ring base
through Link position inclusive. That conservative structural allowance does not
accept all such native stopping behavior. Set-Dequeue context must equal exact
ring-base|opposite-DCS. Fresh increasing frontier records require original epoch,
no target late events,no command debt and at most32 scanned events per drain.
Separate empty frontiers precede rebase and commit. Backend must derive these
from independently attributed stopped/event-debt records,not ledger booleans.

Both directions must have all eight pages FREE at begin;every ledger byte stays
frozen until commit. Quiet pending RX cannot be cancelled here. First binding
must pause/drain the other direction before fencing. Self-state checksums and
callback-bracket integrity checks detect copied/tampered engine,state,IO,command
or ledger. A locally retained original ledger pointer prevents callback pointer
reassignment from faulting a replacement object. Initial ring/eight-page geometry
remains immutable across boundaries. These checks address accidental corruption,
not arbitrary malicious memory writes. Cancel/disconnect during callback prevents
commit and pins original ledger. Caller must close publication/retain ownership
on EVERY false return,including invalid stored handles. Invalid pointers are not
dereferenced merely to mark their apparent ledger faulted.

Every phase,including pending command and drainage,has750ms bound. Backward
engine or latest-ledger time refuses. Begin checks DMA last_us;commit advances
it to fence time. Fence sequence must match last_fence and have increment room;
command tokens need room for two submissions. No leases or PNs roll back. Full
key/hardware retirement remains outside this engine.

The command ring has the same pointer-alias hazard:raw command events do not
carry software token/epoch. Engine records all physical command pointers and
rejects repeats even with fresh tokens. Pointers must be aligned data positions
within one retained4KiB command-ring geometry (positions0..62),disjoint from both
DMA rings/eight data pages. Lifetime is deliberately bounded to31 fences/62
commands;begin refuses before exhausting that limit. Backend additionally must
reserve cleanup commands and forbid positions used before engine attachment.
Never reinitialize an engine to bypass this cap. Indefinite command-ring reuse
needs a separate controller ownership frontier;native backend must enforce its
smaller actual remaining-position budget.

Fixture:30 ordinary two-direction fences/60 commands plus a separate31-fence/
62-command cap refusal. It covers every submit/completion identity field,CC,
stopped context/dequeue,frontier epoch/sequence/debt/late-target/overflow,deadline/
backward clock,loan/opposite-device refusal,cancel/disconnect,callback state/IO/
command/pointer/ledger mutation,immutable page geometry,command/DMA alias and loss
of owner at commit. Failed cases do not wrap;replacement ledger stays untouched.
Normal and fatal ASan/UBSan/leak checks pass. Production freestanding O2 compile
with stack-usage passes;maximum function frame160bytes (step),submit144bytes.

Standalone test command uses the prior compiler flags with these three sources:
`tools/test_mt76_fence5.c kernel/src/drivers/mt76_fence5.c kernel/src/drivers/mt76_dma5.c`.
Parent owns build integration/matched qualification. This subtask used only
private/tmp outputs;no shared make,staging,VM,hardware,secrets or archive changes.
