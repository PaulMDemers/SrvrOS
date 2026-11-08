# Pro11,2 Wi-Fi completion plan

November 8. Unit-local implementation plan,not a hardware acceptance claim.
Source307b911e with integrationa1661937 consumed;origin/main63036859 fetched.
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
