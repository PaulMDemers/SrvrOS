# Pro11,2 Wi-Fi completion plan

November 8. Unit-local implementation plan,not a hardware acceptance claim.
Initial plan source307b911e;main63036859 consumed in merge0c2deb45.
The merged baseline and exact v134 No-Op candidate are software-qualified;older native evidence is unchanged.
Edimax7392:a711 MT7610 is5GHz-only. Target Demers5 WPA2-PSK/CCMP,US161/20MHz.
Demers2.4 requires a different compatible adapter;it is a separate hardware scope.

## Done and preserved

Discovery/selection,matched authentication/association,four-way M3 MIC/local M4,
software CCMP with monotonic PN and authenticated receive,strict DHCP DORA/ARP/
DNS/256-byte gateway echo,asynchronous copied-provider TX and bounded four-page
RX/TX reuse have native evidence. Full key/endpoint/input/MAC/power/RF retirement
has evidence. Historical failures remain failures. Native reuse has separate
storage review:all non-sidecar bytes preserved/current metadata tree valid,but
historical metadata-field equality was not captured. Ordinary provider RX and
continuous networking are not accepted. Immutable evidence/recovery must survive
clean builds. Do not infer general reliability from short diagnostic exchanges.

## Execution and parallel ownership

The parent integrates and owns all shared builds,staging,VMs,private credentials,
USB writes,installation and power cycles. Only one such harness runs at once.
Subagents own disjoint software/test files;standalone tests use private temporary
outputs. They report exact commands/results,limits and handoff contracts. No
subagent power cycles,reads secrets,alters hardware gates or contacts other chats.

Initial parallel work:

- RX owner: real native provider receive fixture and minimal integration design.
  Prove authenticated Ethernet reaches real net_device_receive without a second
  decrypt/replay commit or leaking DMA storage;cover callback/epoch/close faults.
- Transport owner: actual63-position reuse/wrap regression and native fence design.
  Prove old completions cannot acquire current leases;define independent stop,
  event-debt,Set-TR-Dequeue/cycle proofs and fault retention before any wrap.
  Command-ring address reuse needs its own frontier proof too;fresh command
  positions and reserved cleanup capacity bound the first hardware binding.
- Evidence owner: reusable sidecar-aware preservation tool and tests. Save raw
  baseline sidecar bytes privately,compare prior fields under explicit policy,
  hash all other files,and permit only matching-session new paths. Never exclude
  generated metadata wholesale or overwrite old failed/accepted capsules.
- Parent: review current-main deltas,coordinate contracts,maintain milestones,
  integrate/review subagent work and execute serial qualification/native trials.

## Ordered milestones and completion gates

1. Evidence and integration baseline. Review actual main changes before merge;
   reconcile rather than replace shared profile/main/staging contracts. Preserve
   current accepted capsules and payloads. Fix next-run metadata capture/proof.
   Gate:tool positive and mutation refusals;clean reviewed source/checkpoint.
2. Persistent USB/key lifetime. Replace one-shot position/six-request assumptions
   with bounded asynchronous transport. Keep four-page backpressure,exact native
   completion identity,one original key/PN owner,quiet RX and bounded pending TX.
   Wrap only with hardware-derived fence proof;all uncertainty pins resources.
   Gate:many real ring boundaries,simultaneous traffic,stop while idle/busy,
   deadlines/late events/disconnect failures,sanitizers and native sustained run.
   Develop against the RX callback contract/fixture in parallel;software fence
   fixtures alone do not accept hardware. Native ordinary-stack RX in milestone3
   requires this transport because production DHCP is not a fixed six-request flow.
3. Ordinary receive. Route already authenticated normalized Ethernet into the
   production provider and stack under original CCMP ownership,with bounded
   copied receive storage,epoch checks and callback lifetime. Preserve diagnostic
   historical modes. Gate:actual native-owner plus independent encrypted-peer
   fixture,duplicates/MIC/length/queue/close/stale-epoch refusals,then,after milestone2,native DHCP/
   DNS/echo through ordinary stack. Do not count diagnostic polling as this gate.
4. General IP/data. Drive production DHCP,DNS,ARP,UDP and TCP rather than private
   six-request oracle. Validate independent byte content/length,checksums,large
   multi-packet HTTP responses,TCP retransmission/window/close and backpressure.
   Confirm supported application behavior and report throughput/packet loss
   without promising an arbitrary target. Gate:native ordinary-stack bidirectional
   transfer and correct teardown. TCP/HTTP host fixtures are preparatory evidence.
5. Connection lifecycle. Separate adapter/provider lifetime from one connection.
   Support select/connect,wrong-credential/auth failure,cancel/disconnect,reconnect,
   fresh epoch/keys/PN,new DHCP lease,AP loss and adapter removal during idle/TX/RX.
   No stale callbacks,leases or keys may cross generations. Reattach must discover
   and initialize the same verified device afresh without registry-slot leakage.
   Gate:repeated native reconnect/removal with successful subsequent transfers
   and full resource retirement or explicit contained retention on uncertainty.
6. UI and credentials. See [connection/UI handoff](macbookpro11-2-wifi-lifecycle-design.md). Worker-driven scan/status/connect/disconnect,accurate
   async states/Refresh/removal/reattach,keyboard/trackpad usability,clear errors,
   private credential handling and no key/password/packet values in logs. Gate:
   untimed in-guest checklist with final saved exit,plus state-model regressions.
   User credentials remain private;do not publish their derived material.
7. Reliability and release. Repeated cold/warm lifetimes,longer sustained traffic,
   UI/input/storage coexistence,AP interruption and recovery;record measured
   coverage and remaining faults. Run matched build,relevant regressions/fatal
   sanitizers/stack/storage,exact-image GPT and missing-workspace VMs,then native
   collection with file/allocation/recovery proof. Review PR19 and merge only a
   coherent locally verified scope under existing integration authorization.
   Rebuild/retest merged baseline before declaring daily-use support.

A milestone is complete only with its own native evidence,not an optimistic flag.
No fixed percentage or date is a substitute for these gates. Initial parallel
outputs should reduce serial iteration;native radio trials remain inherently
serial. Avoid tiny register-only boots when a coherent bounded trial can prove
several adjacent requirements without weakening limits.

## Test/power-cycle protocol

Before physical tests,confirm local model,source/release,UUID0E53-5900,recovery,
private artifact modes and durable run record. Progress/results automatic.
Collect once first on return. Missing completion/halts fail. Keep Ubuntu default.
User authorizes automatic qualified trials. Adapter still needs Wi-Fi-only10second
cold unplug:shutdown,leave workspace drive connected,reconnect same port,power on.
Use reboot only after warm-start behavior is independently qualified. Attended
work opens the entire untimed checklist in the guest before requiring input.

## Deferred scope

This adapter cannot prove Demers2.4 support. Different chipsets,5GHz channels or
security modes require explicit compatibility/regulatory evidence. WPA3/enterprise,
roaming,suspend/resume,throughput optimization and new-adapter enablement are not
implied by completing this WPA2 fixed-adapter path. Trackpad latency,battery and
headphone work remain independently deferred.


## Current checkpoint (November 8)

All three parallel software assignments are integrated and qualified. The actual
native async backend and typed controller-only No-Op child have fatal sanitizer,
real-owner fixture,strict full-parent saved-prefix,stack/storage and exact-stage
VM coverage. Runtime4f89113a/release56a45e10afdd4efc is the first physical63+1
No-Op boundary trial. It grants no encrypted data/RX authority. Parent owns its
protected install,cold shutdown and matching collection;no concurrent harnesses.

After physical boundary evidence,transport binds the same engine to persistent
four-page encrypted traffic while RX integrates the already-authenticated copied
Ethernet contract. These are still milestone2/3 work. Command-position reuse
beyond the bounded fresh-command budget needs a separate hardware-derived proof.
The ordinary DHCP/DNS/TCP/HTTP,lifecycle/UI and sustained reliability gates above
remain open. Evidence ownership now includes exact private baseline metadata and
independent protocol/storage outcomes;old v133 historical metadata remains limited.


First v134 physical return is incomplete:old v126 passes,but a boot dispatch bug
prevented the new child from entering. The strict metadata proof independently
refuses sequential import inode remaps. Both outcomes are sealed;no new boundary
acceptance. Next bounded corrections are actual-route dispatch coverage and
atomic same-mount persisted metadata restoration without aliasing outside nodes.
The checker stays strict. Then repeat exact qualification and the cold trial.


Corrected retry checkpoint:runtime dac05a8c/release4a8f81ad6daf73b4 passes fresh
matched/relevant regressions,245 stage tests,seven fatal sanitizer groups,stack/
full-prefix/storage/desktop/exact-image VMs. The extra exact-image VM preserves
all408 actual workspace files and saved identities with the batch importer;
only marker atime changes within proven RTC bounds. Native63+1 No-Op boundary
and native metadata preservation remain pending the next cold run. Persistent
encrypted transport,ordinary RX/IP/TCP/HTTP,lifecycle/UI and sustained reliability
remain the completion milestones;this controller-only trial accepts none of them.


Native v134 now passes(source dac05a8c):64 real bufferless completions,one TX
ring fence,full parent/ledger retirement and strict408-file/metadata preservation.
192 native files and12 acceptance notes are independently sealed. This closes
the first controller-only boundary gate. Milestone2 still requires encrypted
RX/TX across actual boundaries,pending-RX cancellation and bounded command-address
reuse. Original CCMP ownership and all later ordinary-stack/lifecycle gates remain.
The next parallel contract audit targets those integration points before coding.


The next v135 candidate is a distinct original-key owner after v128. Its first
bounded gate runs70 encrypted requests (six unchanged setup requests plus64
exact256-byte echoes),real RX and TX63-position boundaries,and authenticated
traffic afterward. Four pages/direction,eight cleanup RX positions,a four-fence
limit and fresh-command/cleanup reserve bound authority. It does not reuse command
addresses or cancel quiet pending RX. Normal-provider readiness holds frames
before dequeue/token/PN while a fence is busy. Independent public AES-CCM vectors
exercise the actual new owner in host fixtures;strict full-parent saved-log and
matching native validation remain required. This is milestone2 progress,not
ordinary-stack or lifecycle acceptance. The accepted v134 payload stays installed
and unarmed while implementation is qualified.

Qualified v135 checkpoint:runtime e7f2fe70/release6065541ae03fb3c3 passes matched
focused regressions,fatal sanitizers,strict498-rejection whole-parent composition,
stack/capture/storage and exact-image VMs,including410 prior files and identities.
It is installed for the next authorized cold run;native stream acceptance remains
pending. Original-key70-request/64-echo RX+TX boundary proof is the next gate,
followed by production-stack receive and data/lifecycle milestones above. See the
unit record for immutable evidence,recovery and collect-first instructions.

The unchanged v135 retry reaches authentication and one real RX fence,but stops
at its first DHCP wait;the full native gate remains failed. The new explicit
v136 variant permits one bounded Discover retransmission with fresh PN and keeps
categorical DHCP counters. Runtime38f2d76a/release8529308170250844 passes matched
selected regressions,sanitizers,506-rejection full-prefix proof,stack/storage and
exact-image VMs including414 prior files and identities. It is installed for the
next cold trial;native70/71-request/64-echo and TX-boundary acceptance is pending.
The original no-retry profiles stay separate. Production RX/IP/TCP/HTTP,lifecycle
and broad reliability above remain required before daily-use completion.

November 9: native v136 `38f2d76a` / `8529308170250844` now accepts the bounded
original-key stream: one Discover retry, 71 encrypted requests, 64 exact echoes,
one TX and three RX real fences, complete parent/ledger retirement, and strict
414-file baseline/metadata plus offline/recovery proof. Independent reviews and
separate acceptance notes preserve the original 255-file native capsule. This
closes the first bounded encrypted RX/TX boundary gate, not unlimited command
reuse, ordinary provider/stack or lifecycle acceptance.

Next parallel implementation keeps v136 separate: a nonblocking native provider
adapter and combined actual-owner/unrenamed-production-stack encrypted fixture,
plus explicit completion/epoch-guarded production DHCP retry. Root owns subsequent
profile integration, deferred output/capture budget, exact-image qualification
and hardware. TCP/HTTP requires a verified endpoint and expected response bytes;
UI/reattachment and reliability remain later gates. Nothing new is armed.

The next software gate is now implemented and qualified on merged baseline
`a12e2835`: explicit guarded production DHCP/quiet initialization and a callback-
only asynchronous native provider. The actual encrypted-owner plus unrenamed
production-stack fixture passes DHCP/DNS/ARP/ping, 52 UDP echoes and a multi-packet
3 KiB HTTP response with close, including fatal sanitizers and fault retention.
Matched selected build/regressions, existing 252 stage cases and ordinary desktop
VM pass. It has no new boot recipe or native acceptance. The
[next native workload design](macbookpro11-2-wifi-stack5-native-design.md) scopes
finite guarded DHCP/DNS/64 gateway pings first; real process ownership, timers and
bounded TCP APIs precede HTTP. Lifecycle, reattachment, UI and reliability remain.


November9 v137 checkpoint: runtime `6a26cb80` / release `815747ea8c559ec0`
consumes actual main `e85a6846` and is qualified/installed for the first native
production DHCP/DNS/ARP and64 exact40-byte ICMP/32-byte-body trial. Native v136
71-request/64-echo acceptance is immutable. The new original-key provider bridge
passes21 actual-owner/production-stack normal/fatal cases, independent finite-
protocol checks, matched selected regressions,255 staging cases/nine actual-main
groups, strict494+12 refusal checks and exact-stage/storage/workspace-copy VMs.
Native v137 acceptance remains pending; collect its matching saved session first.

Remaining order: accept this production-stack hardware gate; add legitimate
process/timer ownership and bounded TCP/HTTP, then prove native exact multi-packet
transfer; qualify command-address reuse and persistent lifecycle, disconnection/
reattachment/fresh keys and epochs; finish selection/connect controls and reliable
UI updates; run loss, cancellation and sustained reliability checks. Host HTTP
fixtures are preparation, not native acceptance. This5GHz-only adapter cannot
accept Demers2.4; a compatible adapter is required for that separate band scope.


The first v137 production trial6a26cb80/815747 failed during the second AUTH,
before keys or the workload. The1605-file failure and11-file reviews are retained;
workspace/recovery passed, but no new network acceptance follows. Observation-
only runtime2da2f400/release7026c0d52d23d45d is now qualified/installed: deferred
AUTH categories/timing distinguish modeled loss/rejection/malformed replies with
no protocol or timeout change.24 normal/fatal owner cases,534 full-parent
mutations, matched selected regression plus explicit Python-only proof binding,
255 stage/nine main groups, storage/desktop/exact418-file workspace VMs pass.
Native comparison is pending; collect wifi-stack5-auth first once after return.
The production-IP gate, legitimate process/timer-owned TCP/HTTP, lifecycle/UI and
reliability remain in the order above. Demers2.4 still needs compatible hardware.


Latest November9 checkpoint: source2da2f400/release7026 accepts bounded original-
provider production DHCP/DNS/ARP and64 exact40-byte ICMP messages through native
RX/TX boundaries, with complete parent/ledger/provider retirement and independent
file/metadata proof. Source5ca1925e/release9f443344 integrates mainbdc14461 and is
qualified/installed for the next native regression; host70fd tooling uses explicit
read-only/work inspection. The content-addressed runtime manifest stays5ca and
host/runtime source archives are distinct. The initial wrongnamespace13-file
preparation failure is preserved. Collect wifi-stack5-inspection FIRST once.

Next TCP/HTTP software gate needs isolated init/services (svscan uses net.initialized
as readiness), a genuinely running prelive child acknowledged through existing
startup API, finite connect/send primitives and exact HTTP decoding. Bounded DNS
has no cache for child getaddrinfo; use a reviewed PID/lifetime-bound numeric-answer
handoff. The real IRQ timer and retained worker must drain terminal sockets;
this stack uses80 TIME_WAIT ticks even after EOF-first/passive close. No fake
process context, hidden wait, service file logging or timeout-as-success is allowed.
