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

## Dormant native backend implementation

`xhci_wifi_fence5.inc` now supplies an explicit private singleton binding with
no staging flag or ordinary startup caller. `wifi_fence5_bind(device,dma,epoch,
owner_context,owner_predicate)` captures original command-ring identity/cycle,
actual endpoint geometry and immutable physical mappings. Admission requires the
captured Intel8086:8c31 controller,7392:a711/0100 probe-verified device,the actual
512-byte bulk TX8/RX9 contexts,63-position ledger,free pages and an observed empty
raw event frontier with no pending old command. The supplied independently
verified owner predicate must enforce unit/parent/mapping ownership and freeze
both ledger directions across a fence. It must recognize its own retained async
command,not call the historical blanket pending-command refusal predicate.

`wifi_fence5_begin(direction)` and `wifi_fence5_step()` wrap the production engine
under short CPU0/IRQ serialization. One step performs one engine action;WAIT
steps only inspect latched completion state. The worker must separately dispatch
shared raw events between steps. Every false return requires publication closure
and retained ownership. Publication writes a real immutable command TRB and rings
the native command doorbell without calling the synchronous command helper.
The command cycle/ownership bit is made valid last after body writes and release
fences. Validated raw command/FSE TRBs and the contemporaneous stopped endpoint
context/dequeue are retained separately for deferred receipts.
Global command exclusivity lasts through Stop,context/drain,Set Dequeue,final
context/drain and the actual ledger commit. No permanent `sync_users` reference
is held. Success releases command exclusivity after the engine returns IDLE;
command records and tombstones remain retained.

The shared dispatcher routes command completions before its transfer-only
return,and the synchronous waiter defensively invokes the same command owner.
Unknown command debt during any active fence phase,wrong pointer/slot/CC,
changed shadow TRB,duplicate or old completion fails closed. Raw completions
receive identity only from the retained unique-position publication record.
Two command slots plus six unchanged cleanup slots must remain before a round.
No command-address wrap is admitted while the binding is retained;the ordinary
synchronous helper also refuses alias reuse under this binding. This is bounded
command lifetime support,not sustained command-ring wrap acceptance.

The pure-TX NoOp mode uses `mt76_dma5_init_noop_ring`:only the real TX ring exists
in the ledger;RX ring and all data-page addresses remain zero. It grants no data
page or RX authority. Actual slot0/RX9/MCU11/16 output contexts are captured and
must remain unchanged. Only TX fences are admitted. Normal data-mode geometry
remains distinct and requires both real directions.

The scoped transfer hook returns false outside an active fence,allowing the
original NoOp/data owner to validate ordinary completions. Inside the fence it
accepts exactly one independently checked Stop FSE before successful Stop command
completion. For the fully consumed63-position boundary,that is CC27,length0,
ED0 and actual Stopped output dequeue equal to either Link63/current DCS or the
following invalid base/opposite DCS. Other target transfer debt fails. This is
based on the primary xHCI1.2 Stop Endpoint rules and table4-2,not an invented FSE
command flag. Historical WPA terminal phases and their receipts stay unchanged.
Frontier callbacks consume at most one raw event per step,dispatch unrelated
transfer events normally,and require a real empty observation within the engine's
unchanged32-event cumulative budget. They do not infer empty from a running poll.

`wifi_fence5_retire()` is deliberately terminal-only:the external DMA ledger must
already be retired,the controller must actually be stopped with RUN/INTE/reset
clear and HCH set/CNR clear,and the event frontier must be empty with callback/
poll references drained. It does not accept a caller's hardware-retired boolean,
clear uncertain pending commands,free DMA,reset the controller or erase records.
`xhci_reclaim` refuses an unretired binding. `xhci_stop` can still halt without an
artificial permanent sync reference;uncertain active fences remain pinned.
A first isolated NoOp child may therefore retain its backend context until
terminal halt and must not claim live context release or persistent service.

The actual-backend fixture includes the real controller/shared dispatcher with
mock MMIO. Its63 completed bufferless ledger positions prepare the boundary;
that setup is software input,not physical transfer evidence. It covers command
publication,completion consumed by the shared worker,strict FSE/context variants,
Set-Dequeue/final-frontier commit,global exclusion,reserve/alias refusal,missing/
duplicate/wrong/late command and transfer debt,shadow/epoch/slot/sibling-context
changes,deadlines and retained terminal-stop behavior. Separate native NoOp
transport must prove63 actual TRBs plus the first opposite-cycle reused TRB
before any hardware acceptance. Ordinary data,CCMP,persistence and TCP/HTTP remain
outside this dormant backend's acceptance scope.

### Software qualification of the implemented binding

The actual-controller backend fixture passes GCC C11 `-O2 -Wall -Wextra -Werror`
and a separate `-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined` build
with fatal ASan/UBSan and leak detection. The final sanitizer run includes the
frozen NoOp producer/shared hook,retained raw receipts,cycle-valid-last command
publication,prepublication alias guards and automatic checked terminal-retirement
handoff from generic reclaim. Both include the real `xhci.c`/native backend and
production `mt76_fence5`/DMA core with the existing controller fixture's mocked
MMIO/device environment. No shared staging/build/VM or hardware action occurred.
The parent must still qualify the matched full kernel/image,old controller paths,
exact-stage VM and native evidence. These software results accept no radio data,
physical descriptor wrap or ordinary persistent connection.

### November10 Stop publication ordering correction

The [HTTP/native Stop audit](macbookpro11-2-wifi-stop-audit.md) corrects the
synchronization point above. Retain exact raw FSE identity when it arrives; the
controller may still expose Running/old dequeue then. Validate final Stopped
context, matching dequeue and DCS only after the exact successful Stop Command
Completion. Missing, duplicated or reversed FSE/command events still fault and
pin ownership. This applies to the native fence and outer stream/NoOp/calibration
Stop families; raw notification alone releases no DMA.

`wifi_fence5_try_begin` distinguishes an unrelated nonempty event frontier from
invalid ownership/geometry. Retry that frontier only inside the original caller
budget, without publishing a command first. While bound, every shared consumer now retains
unexplained command debt as a fault, including before a round. A non-fence command
completion delegates only from the actual synchronous waiter with its exact
published pointer; pending/sync counters alone grant no delegation. Selected
source regressions and independent review are recorded in the unit audit;
physical validation of these corrections remains pending.
