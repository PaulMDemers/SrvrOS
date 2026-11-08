# Pro11,2 native asynchronous fence binding audit

November 8. Read-only source audit against merged baseline0c2deb45. This records
requirements for binding the software `mt76_fence5` engine;it is not an implemented
native command owner or physical acceptance. The software engine cannot grant
its own hardware/event/endpoint authority. Parent owns production integration,
serialized qualification and native trials.

## Current command and event paths

`kernel/src/drivers/xhci.c:poll_event` consumes a raw event,advances the event-ring
cursor/cycle and writes ERDP. The worker,control waits,storage waits and native
Wi-Fi polling then pass events to `dispatch_transfer_event`. That dispatcher
currently returns immediately for non-transfer events,including command
completions. A Wi-Fi-only command consumer therefore cannot reliably receive its
completion:another legitimate controller consumer may already have consumed it.

The synchronous `submit_command_trb` publishes to the global command ring,sets
`command_pending`,increments `sync_users`,rings the command doorbell and waits
inside `wait_command_completion`. The waiter consumes command completions itself,
routing other events through the transfer dispatcher. It matches command pointer
and success CC;callers sometimes additionally check returned slot/context.
`firmware_command_exact` silently ignores unmatched command pointers in the
historical synchronous wait. None of those diagnostic counters or policies is a
new asynchronous fence ownership proof.

## Minimal native integration hooks

- Add a persistent,CPU0/IRQ-serialized asynchronous command ownership record and
  a nonblocking publication sibling beside `submit_command_trb`. Preserve the
  existing synchronous command API and its storage/control/HID consumers.
- At the top of `dispatch_transfer_event`,before its transfer-only return,route
  command completion events to that command owner. This covers all ordinary
  consumers of the shared event ring. Retain transfer dispatch unchanged.
- Synchronous and asynchronous global command ownership must be mutually
  exclusive. The synchronous waiter should nevertheless explicitly recognize/
  route an async-owned completion defensively,rather than consuming it as an
  unrelated command. No completion callback waits,polls,logs or issues EP0 I/O.
- A fence completion callback reads the previously latched ownership record;
  it must not itself poll the event ring. The raw-event owner validates actual
  pointer,slot,CC and immutable published command fields before recording an
  exact one-shot result for the engine's original token/epoch.

Publication guards include controller present/operational/owned,not stopping or
reclaiming,no command failure or other command pending,unchanged original device/
slot/generation,valid command ring/cursor/cycle,available immutable command budget,
and no conflicting control or diagnostic mutation. Admit only the intended Stop
Endpoint and Set TR Dequeue operations for the independently owned RX/TX DCIs.
Validate dequeue alignment/cycle and requested ring against the actual retained
endpoint. Freeze the command record and shadow TRB before hardware publication;
set global pending before doorbell. The caller supplies no newly invented proof.

## Command-ring alias and cleanup reserve

The native command ring has63 usable positions and existing synchronous
publication wraps/toggles its producer cycle. A raw command completion carries
command TRB pointer,CC and slot;it does not carry the software token,epoch,DCI or
original command type. Reading the current ring slot and stamping its current
token onto a raw matching pointer after address reuse is unsafe attribution.
A changed command TRB,wrong slot/pointer,duplicate or unexpected completion must
fault/pin the new owner rather than silently satisfy a current fence.

For the first bounded native binding,use command positions not recycled during
that admitted ownership interval. Establish the initial event frontier with no
old command outstanding;do not assume pointers have never been used earlier in
boot. Reserve enough remaining command positions for both fence commands and
all unchanged final endpoint cleanup. Refuse before publication if that reserve
cannot be met. Do not exhaust the command ring midway through a fence and then
claim a safe ordinary close. This bounded approach is not persistent command
ring reuse acceptance.

Longer-lived operation needs a separately justified command-address retirement
and event-frontier scheme,as well as the data-ring wrap proof. The software
fence token is an identity bound by the native owner,not information returned by
hardware. A data-endpoint Stop/Set-Dequeue proof alone does not retire aliases
in the controller's command ring.

## Existing native owner blockers

`xhci_wifi_wpa5.inc:wifi_wpa5_owned` requires both `!xhci.command_pending` and
`!xhci.sync_users`. Calling that predicate unchanged while a newly asynchronous
fence command is pending would immediately fail/quarantine the Wi-Fi owner.
Introduce a narrowly scoped fence-owner predicate recognizing exactly its own
retained command record while both DMA-ledger directions are paused. Do not
remove the old checks globally or permit arbitrary controller command overlap.

The same file's `wifi_wpa5_event` accepts Stop-related transfer CC26..28 only
under `firmware_command_exact`,terminal cleanup phase4/5 and the corresponding
`stop_dci`,then emits its historical console receipt. Persistent wrap Stop events
need a separate independently checked attribution/state branch before that
historical terminal logic. Preserve old terminal behavior. New callbacks must
not emit console output or manufacture stopped-event counts. The engine's
endpoint context and drained-event frontier must come from fresh native reads
and actual bounded event consumption while stopped.

## Fault retention and terminal stop

CC failure,duplicate/mismatched event,clock reversal or deadline expiration is
uncertainty. Close publication;retain the command record,TRB,DMA pages and original
owner. Do not turn a cancellation/timeout into free memory or a reusable command
slot. A nonblocking command state is distinct from an executing callback reference.

`xhci_stop` refuses to halt while `sync_users` is nonzero. Keeping a synthetic
sync reference forever after an async timeout would therefore prevent terminal
controller halt. Explicit asynchronous lifetime/pin accounting must coexist with
terminal halt/retained-DMA cleanup;do not clear it merely to obtain a pass.
`xhci_reclaim` currently also refuses attempted firmware diagnostics,but that
historical diagnostic guard is not a sufficient lifetime contract for a future
ordinary asynchronous command owner.

## Required regression and physical evidence

Before binding,cover command completion consumed by every shared event consumer,
synchronous/asynchronous exclusion,wrong pointer/slot/CC,duplicate/late events,
changed shadow command,command capacity/cleanup reserve,timeout/removal and
terminal-stop retention. Preserve HID/storage/control completion behavior and
historical Wi-Fi terminal cleanup fixtures. Exercise actual63-position data-ring
boundaries with real Stop completion,Stopped context,event drain,Set TR Dequeue
completion/opposite cycle,post-write context and final empty frontier before
reopening publication. Software callbacks with true booleans are not native
hardware proof. Qualify matched build,sanitizers,exact-stage VM,storage/recovery
and saved cold native results under the parent workflow before acceptance.
