# Pro11,2 Wi-Fi completion plan

November 8. Unit-local implementation plan,not a hardware acceptance claim.
Initial plan source307b911e;main63036859 consumed in merge0c2deb45.
The merged baseline and exact v134 No-Op candidate are software-qualified;older native evidence is unchanged.
Edimax7392:a711 MT7610 is5GHz-only. Target Demers5 WPA2-PSK/CCMP,US161/20MHz.
Demers2.4 requires a different compatible adapter;it is a separate hardware scope.


## Current checkpoint (November 11)

The v144 early-main-RX comparison returned on runtime5473623f/release
ad83869d32dbd00e in session00066005112a7445-00. All five calibration replies,
MAC disable/drain, exact endpoint Stop and full RF restoration pass. Six of32
preposted distinct buffers complete without exhaustion, reuse or payload
inspection. All data events were outside MCU/EP0 wait spans, so native overlap
coverage still comes only from modeled actual-C-path tests. No connection or
network OUT ran; the paired-HTTP failure remains unresolved.

Qualification passes normal/fatal67 early-owner cases, prior MCU suites and
exact-stage persistence/recovery checks. Native preservation accepts all460
prior files, four new log files,59 other EFI files and seven recoveries. The
once-only collector closed with a host file-shape/recurring-marker refusal;
the primary1782-file bank stays unchanged. Separate sealed return review passes
the corrected transport guard, unchanged full protocol ledger and all seven
journal records with7199-byte final overlap. See the unit record for hashes.
The reusable transport checker now covers valid periodic snapshots and strict
identity/capacity/prefix refusals, including this actual return.

Success supports early-main-RX/backpressure but is not a proved fix. Periodic
logging was active here and parked in the previous failure; the merged
filesystem baseline also differs. Next qualify a matched-logging, same-image
comparison before integrating early receive into the production connection
path and returning to paired HTTP. Initial-channel sequencing remains a separate
hypothesis. Maina611fd9e is integrated at46955130 (documentation only); installed
runtime remains5473623f. Ubuntu is default/unarmed; no new test is scheduled.

### Previous failed trace and implementation checkpoint

Native trace runtime58a48e04/release186c23c85bc94c89 returned automatically in
session00066003c7505e38-00 and is closed failed for Wi-Fi. First R(0) OUT completes;
the original500ms IN fails and the same retained TD has no late completion
through1.5s. Both sampled16-byte reply prefixes are zero;75 events are consumed
and dispatched with zero exact MCU events/claims. No connection starts. This
narrows the observed loss before reply parsing, without distinguishing firmware
nonresponse from endpoint/controller delivery. All seven journal records,458
prior workspace files/metadata,58 other EFI files and seven recoveries pass.
The1412-file primary and separate return review are closed; never recollect or
reseal them. Ubuntu is unarmed; MCU and paired-HTTP causes remain unresolved.

At that checkpoint, the isolated v144 comparison was implemented and in
software qualification. It retains32 distinct pages, accounts for completions
during actual MCU/EP0/Stop spans and reports queue exhaustion. Normal native-C
coverage passes67 cases including1906-receive histories; fatal/image/physical
checks remain pending. Keep initial channel sequencing separate, and do not
interpret a future passing trial as a root-cause fix without discriminating
evidence. No identical blind retry or new reboot is armed.

An independent clean cold Linux capture has15 matched calibration responses,
zero monitor drops and exact pinned-code/IVB upload observations: R0=474us,
FULL901=3454us, max16187us. The first overflowing capture remains partial.
Linux's early main-RX posting and initial channel36 power-on calibration are
additional comparison targets, not established causes. Keep all original RF,
500ms failure, quarantine and retirement gates. The diagnostic observes the
same failed TD to1.5s without promoting it to success or resubmitting anything.

Linux MCU timing capture is complete for one warm driver restart and three
reconnects:36 matched replies, no monitor drops, R0=454us and four FULL901
replies=2601–3316us. The18-file capture bank survives outside build. This is an
80MHz reference, not a matched cold20MHz or resident-firmware identity proof.
Main5e72e2dd is integrated at377e2575. A calibration-only native trace now
separates buffer snapshots, consumed events, dispatch and MCU claim, preserving
the original500ms failure while observing the same retained TD up to1.5s. Its
actual-path model cases test distinct losses without claiming a hardware cause.
That trace's qualification completed; the native return is recorded above.

External driver/specification research is recorded in the
[November 11 review](macbookpro11-2-wifi-research-2026-11-11.md). Linux's response
wait/matching and cancellation policy provide concrete comparison targets;
running output-context dequeue snapshots cannot prove a stuck controller.
The research supplies no established physical cause or production correction.
The research-only checkpoint made no runtime, install or boot changes; later
source integration/tracing and its failed native return are recorded above.
The native acceptance gates below remain unchanged.

The `d413e338` / `e4e95e7d5e24fc7c` return is closed failed for Wi-Fi in
session `000660021b689d1e-00`. The larger24×48 text is natively confirmed readable
and the user confirms both automatic return and the required cold unplug. The
HTTP held panel was not exercised. All seven independent checkpoints and full
USB logs saved;456 prior files/metadata, offline checks,57 other EFI files and
seven recovery equalities pass. Preserve the1,438-file primary and separate
three-file review; never recollect either.

The first R0 calibration command's16-byte OUT completes, but DCI11 IN reaches
500000us at sequence3817/index16 before either connection begins. No matching MCU
completion was observed; this does not prove hardware absence. The original HTTP
failure and physical MCU cause remain unresolved. Test-onlyec98aa14 adds exact
first-R0/event-wrap timeout containment and400ms delayed-response controls to the
existing real-TRB long-lifetime fixture. Normal/fatal24+8+9 cases and14 decoder
methods pass without production changes or new candidate/boot. Preserve larger
text, diagnose MCU response/completion causally, then qualify a correction before
returning to the paired HTTP workload. Main52907f8b/PR467 is inspected/deferred
and requires integration/requalification before new runtime work. Ubuntu remains
unarmed; persistent service/UI/reattach/endurance follow these earlier gates.

The following acceptance remains bound to its earlier source.

The cold native `0e750c07` / `c280927702804a27` trial passes full fixed161
calibration and two fresh authenticated connection epochs within one boot
(session `00065ff9342c1e07-00`). Each completes production DHCP/DNS and 64 exact
gateway replies, followed by key erasure, network deconfiguration, endpoint
closure and provider detach. Final RF parent restoration/provider retirement
pass; two backend tombstones remain retained. Separate acceptance review checks
all seven independent journal records and USB overlap, prior workspace bytes/
metadata, offline allocation, other EFI files and protected recovery payloads.
The primary and acceptance notes are sealed outside build. Ubuntu is unarmed.

The earlier DHCP timeout did not recur and remains unexplained. Retained DHCP
observations now show successful DORA without retries or rejections in both
cycles. This is bounded connection reuse acceptance, with no claim of sustained
reliability or unlimited reuse. Next implement and qualify actual TCP/HTTP across
connection lifetimes, then the persistent worker and desktop controls, physical
adapter reattach, cancellation/failure recovery and repeated endurance runs.
The current HTTP pair failed/incomplete; historical accepted single HTTP is preserved.
Demers2.4 remains a different-adapter scope because this Edimax is 5GHz-only.

### Main-RX comparison audit after the trace return

Source58a48e04 confirms the difference, but a direct move of `wifi_cal5_post`
before calibration is invalid. `wifi_cal5_inspection` calls the calibration
engine before `mt76_rx5_window_poll`; the latter starts with zero attempts and
would attempt to publish slot0 again. The native post callback correctly refuses
that mismatch. Its eight forward slots and eight optional cleanup slots also
have exact layout, event, stopped-dequeue and retained-buffer contracts; early
traffic cannot silently become a normal observation or cleanup receipt.

Additional constraints found in this audit:

- `wifi_cal5_owner` rejects pending MCU work for ordinary actions. A preposted
  data completion may arrive during the actual MCU OUT/IN wait or EP0 guards.
  Event admission therefore needs an exact memory-only owner for those spans;
  it must not widen forward register/publish authorization.
- `wifi_cal5_event` describes one most-recent forward TD, while Linux posts a
  queue of receives and services it asynchronously. A separate comparison needs
  per-TD completion accounting rather than assigning every event to the latest
  post. No buffer can be published twice or reclaimed on timeout.
- The running output-context dequeue pointer is not a progress oracle. Preserve
  immutable ring/page identities separately; validate final cessation only after
  the matching Stop completion. This audit does not identify that guard as the
  observed R(0) cause: it passed before the failed receive.
- The current native-C calibration fixture deliberately publishes no data RX;
  its modeled parent cannot qualify a reordered data queue. The cleanup fixture
  covers a later phase and does not supply that missing overlap coverage.

Implement the comparison as a separate opt-in bounded owner, retaining all early
pages and reporting queue exhaustion explicitly. Keep fixed161, the original
MCU deadline, response-loss observer, RF/firmware gates and ordinary profiles
unchanged. Do not combine the channel36 hypothesis with this comparison. Before
installation, tests must drive actual publication/poll/dispatch with data events
interleaved before OUT completion, during each MCU IN and during EP0 guards;
cover wrong slot/DCI/TRB, duplicate/late events, malformed completion/residue,
finite capacity, pending timeout/removal and cessation races. Failure must leave
all pending DMA retained with no speculative cleanup or network connection.
A future passing comparison would support the RX-backpressure hypothesis, not
establish a production correction or accept paired HTTP/lifecycle reliability.

## Previous checkpoint (November 10)

Startup persistence is now natively accepted for one bounded file lifetime on
cc1c3775/7bf30a800c4d68ac (session00065ff73d95cc33-00). All seven independent
checkpoints pass binary checksums and exact console overlap; terminal flags
record USB save/unmount. The clean FAT-sector pin is physically confirmed.
All449 prior file/metadata checks pass apart from one bounded marker atime,
plus offline allocation,53 other EFI files and seven protected recoveries.
The779-file primary and separate three-file acceptance notes are closed.
Ubuntu is unarmed; the consumed journal cannot be replayed. Next carry the
validated startup evidence into full calibration/two-connection testing with
a fresh file, descriptor and run record. Wi-Fi reconnect remains unaccepted.


The startup fallback saved session00065ff6e502a291-00 and identified the cause:
host preparation pinned Linux's mounted FAT state at byte65=1, while native
boot read the exact clean sector with byte65=0. Controlled direct unmount/
remount reproduces both hashes. Preparation fixcc1c3775/release7bf30a800c4d68ac
pins that clean sector; the unchanged kernel still hashes every actual byte.
All27 writer/fatal, six protocol,267 profile/42 collection and seven UEFI cases
pass, including strict dirty-state refusal and clean admission with USB absent.
Main7e8f70ae is integrated at64534a26. The new startup-clean-state record owns
installation/review/reboot/collection with449 prior files. Native independent
write/flush completion is pending. The closed768-file prior primary and separate
three-file cause review remain immutable.


The first startup journal trial6e620b4a/af216d2c053b59f6 failed admission and
halted before USB; its file stayed initial and all447 workspace files/metadata
and51 other EFI files were preserved. Runtime6a39a477/release1aa9a0b4479f5e99
keeps USB logging as fallback and emits admission reasons. All24 writer/fatal,
storage/logger,267 profile/42 collection and six exact-stage UEFI cases pass,
including a saved USB refusal transcript and dual-refusal hold. Main83c955fd
is integrated at65092a82. The startup-admission local record owns the next
no-unplug reboot and collection. Native journal cause/persistence remain open.


Startup-only runtime6e620b4a/releaseaf216d2c053b59f6 is installed. Its preallocated
internal EFI journal records seven phases through the existing AHCI driver even
with USB absent. Five host tests,21 writer scenarios, fatal sanitizers, storage/
logger regressions and267 profile/42 collection cases pass. Four exact-stage
UEFI guests cover mounted/missing/bad-marker/full-logger states and verify all
other allocated internal-disk bytes unchanged. The new startup record owns
arming and collection; no unplug cycle is required. Native startup persistence
and AHCI validation are pending, followed by calibration/reconnect investigation.


The `1423bf12` / `34ed3937df70ec64` cold trial returned without a saved
session. GRUB consumed its one-shot, and all 447 prior files and metadata
remain exactly equal. Offline storage and installed recovery checks passed.
The once-only return is closed as incomplete. The operator saw SrvrOS run
for about one second and return automatically. Workspace/logger admission
and any Wi-Fi result remain unverified.
Establish those startup checkpoints before using the MCU diagnostic to
judge calibration or same-boot reconnect. Ubuntu is unarmed.

Main8271c254 is integrated atcddfd086. Runtime1423bf12/release34ed3937df70ec64
is qualified and installed for the cold MCU observation/reconnect comparison.
The original bulk diagnostic stays unchanged; sequence-bound descriptor,
producer/Link and bounded dispatch-event companions supply missing discriminators.
Normal and fatal sanitizer checks cover24 calibration cases,6 long-lifetime and9
diagnostic groups; all19 reconnect cases pass both. Sixteen emitted fragments,
18 legacy/14 companion parser cases and20 frozen collector cases pass, including
malformed companion retention and contradictory complete-parent refusal. All six
VMs and the strict447-file clone pass. Only the fully backed failed9110 EFI
duplicate was retired; factorye47, acceptedc603 and the other protected payloads
remain verified. All closed records stay unchanged. The local run record owns
final review/arming. Calibration cause and native reconnect remain open.

The3570ee66/9110 hardware return is now closed as FAILED calibration before
either connection began (session00065ff304b616c2-00). FULL step2 receives no
accepted response by the unchanged500ms limit; the saved MCU receipt identifies
the elapsed branch, not physical causation. The127-position connection change
therefore remains physically untested. All445 prior files/metadata except the
authorized RTC-bounded marker atime survive; offline checks, seven recoveries
and Ubuntu default/unarmed pass. Do not recollect/reseal its1395-file primary.
Independent returned-source/evidence reviews pass failed-run integrity. Six
new host cases close the short fixture's ring-lifetime gap by independently
consuming real Normal/Link63 TRBs after1906 preceding IN transfers. All24+6
normal/fatal sanitizer cases,13 emitted diagnostic comparisons and18 no-skip
observer cases pass. The injected failures identify no spontaneous physical
cause. Next capture discriminating failed-TD/producer/Link/unmatched-event state
before a justified runtime correction and new hardware candidate. No runtime
change or blind retry is armed. Main8271c254/PR460 was inspected, not integrated.

The following paragraphs retain the preboot qualification checkpoint.

Correction3570ee66/release9110adaf1b1c158f is now software-qualified and installed.
Normal/native55+19, all19 connection cases in seven scoped fatal groups,
profile267/collector42, source-frozen workflow20, MCU13/Python18, matching images
and all six VM gates pass, including strict445-file clone preservation. Native
and evidence source reviews have no remaining findings. The installed payload,
factory direct fallback, acceptedc603 and all seven protected prior payloads
are verified; only the completely backed faileda9 EFI duplicate was removed.
The local record owns final review/arming; the new cold hardware result is
pending. The chronology below records the failure and implementation checkpoint,
not a replacement for this latest qualified state.


The67534219/a9 comparison was collected once as00065ff1b8c61e6e-00. Calibration
passed; the first connection passed DHCP/DNS and54 exact pings, then failed at
the connection-only two-fence cap after TX63/RX126. No connection closed or
reconnected. The actual native-owner model reproduces that finite-capacity
failure with added beacon traffic on unchanged original production code. All443
prior files survive, offline checks/recovery pass, and the1371-file primary and
separate12-file failure notes are sealed. No blind retry is armed.

Actual main72d49457 is integrated at1511c722. The correction under qualification
uses127 transfer positions only for the two-connection recipe, within each
existing4KiB ring page. Four pages/direction, TX80/RX252,64 exact pings, command
Link63/no-reuse, conservative two-fence reserve and all deadlines remain fixed.
TX cannot wrap at this geometry; RX can require one native fence. Old63-position
single-connection/HTTP recipes keep their independent cross-fence proofs.
Native owner, raw event/Stop checks, generic sealed fence guard, workload
observations and strict parser must all pin that geometry. Tests include quiet
and extra-management RX, both Link/base Stop representations, geometry/DCS
mutation, stale events and excess-traffic refusal. Software and physical
acceptance of this correction remain pending; current installed image is the
closed faileda9 candidate, Ubuntu default/unarmed.


Main `c3550c82` is now integrated at `3afff099`, preserving local changes and
recursive submodule pins. The explicit v140 implementation supports two fresh
native connection generations within one held adapter lifetime: production
DHCP/DNS and 64 exact gateway echoes per connection, checked stack deconfiguration,
fresh key/epoch/PN/DMA/fence contexts, one registered provider and final outer
parent restoration. Ten same-process native HOST cases pass, including the
captured command frontier and fault/old-frame/alias cases. Independent AP crypto,
13 raw/full-RF composition checks, boot dispatch/staging and collector units pass.
Source27042d66 passed the selected qualification, but its first v140 hardware
run FAILED RXDCOC calibration before either connection began. Runtime67534219 /
releasea9e288e50a236d01 adds deferred native MCU branch observations and passes
normal/native65, actualcal5 MCU24, seven scoped fatal groups,13 emitted traces,
20 frozen-workflow cases and all six exact-image/443-file clone VM gates. One
cold diagnostic/reconnect comparison is installed; physical validation remains
pending and the earlier failure remains immutable. This milestone does not rearm the actual HTTP worker or provide
UI/reattach/persistent service. Historical accepted images below stay unchanged.

Runtime `070089f5` / release `c603901d483d3f86` now has a second complete cold
run, collected once as `00065fef07bf175d-00`. Like the first accepted session
`00065fed77ae049a-00`, it completes production DHCP/DNS and the actual HTTP
process's exact 4,159-byte request, 3,072-byte body, genuine EOF and named
child/worker/TCP/key/provider/full-parent retirement. The repeat's DHCP receipt
accepts one Offer and ACK without retry/rejection in 153.699 ms. One selected
authenticated-data RX crossing is directly observed across one RX fence; no TX
crossing is claimed. Independent repeat protocol and storage/recovery reviews pass with no findings;
separate 12-file notes accept this bounded second cold lifetime.

Preserve the first 1,880-file primary/12-file acceptance notes and new 1,164-file
repeat primary. All 439 prior repeat-workspace bytes/metadata survive except
one marker atime in the proven RTC window. Offline allocation/fsck, installed
payload and seven recoveries pass; Ubuntu is unarmed. Do not run either closed
collector again. Runtime normal/fatal and prior six VM results were explicitly
reused for the unchanged payload; only the current 439-file clone VM and host
preparation/rollback checks were fresh. Backend tombstones remain retained.
The older213 OWNER hold, unchanged785 terminal-drain failure and776 DHCP failure
remain separate immutable failures; the last remains unexplained.

The bounded cold repeat gate is now complete with independent acceptance. Next integrate actual main `c3550c82` and
recursive submodule pins, preserving existing local commits and all accepted
payloads. PR457 changes exFAT extent retention and storage fixtures; it is not
part of either accepted runtime image. Then implement explicit separation of
adapter/provider ownership from a connection, with fresh key/PN/epoch and
child/worker lifetime per generation. Prove two connect/DHCP/DNS/exact-transfer/
disconnect generations on one present adapter before reconnect/UI claims.

Do not clear one-shot guards, reuse retired tombstones or enlarge limits.
Command-address reuse, pending idle RX and event debt require explicit proofs.
Follow with cancellation, wrong credentials/AP loss, safe removal/reattach,
async UI and longer coexistence. These cold successes do not establish warm
initialization, two in-boot connection generations or persistent reliability.
Demers2.4 still requires different compatible hardware.

## Done and preserved

Discovery/selection,matched authentication/association,four-way M3 MIC/local M4,
software CCMP with monotonic PN and authenticated receive,strict DHCP DORA/ARP/
DNS/256-byte gateway echo,asynchronous copied-provider TX and bounded four-page
RX/TX reuse have native evidence. Full key/endpoint/input/MAC/power/RF retirement
has evidence. Historical failures remain failures. Native reuse has separate
storage review:all non-sidecar bytes preserved/current metadata tree valid,but
historical metadata-field equality was not captured. Bounded original-key production RX/DHCP/DNS/ICMP and actual-process TCP/HTTP
now have the November9 evidence below; persistent service and continuous
networking remain unaccepted. Immutable evidence/recovery must survive
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
User authorizes automatic qualified trials. Current cold trials require BOTH
the Wi-Fi adapter and workspace USB disconnected for10seconds while powered off,
then reconnected to the same ports before power on.
Use reboot only after warm-start behavior is independently qualified. Attended
work opens the entire untimed checklist in the guest before requiring input.

## Deferred scope

This adapter cannot prove Demers2.4 support. Different chipsets,5GHz channels or
security modes require explicit compatibility/regulatory evidence. WPA3/enterprise,
roaming,suspend/resume,throughput optimization and new-adapter enablement are not
implied by completing this WPA2 fixed-adapter path. Trackpad latency,battery and
headphone work remain independently deferred.


## Historical checkpoint (November 8)

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


Latest November9 checkpoint: accepted2da/7026 bounded original-provider
production DHCP/DNS/64exact40byteICMP remains independent. The next merged5ca/9f
run FAILED firstjoin association after AUTH; production owner unattempted.
Its1877 native files and14 independent review notes are immutable; all prior420
files/metadata, offline checks and Ubuntu-unarmed recovery passed.

Current sourcec75cdb78/release7a4442e75476090f consumes main4b71b178 and is installed
for the next cold comparison. Firstjoin observations add no callbacks or changed
acceptance/retry/deadline policy;48 complete old/new callback scenarios match.
The new collector requires both source-specific rows and the full original
production-IP/retirement proof. Fresh496 child-mutation checks and19 directed
checks pass. Eleven fatal groups plus private firstjoin/fixture proofs and exact
staged/422-file workspace VMs pass. The initial283.38second suite's single stale
allocation-ordinal failure is preserved; semantic post-write allocation injection
and the60.60second targeted repair pass without weakening filesystem assertions.
Only failed9f's backed EFI duplicate was retired after accepted7026 fallback;
all six accepted payloads and Ubuntu/e47d remain. Collect wifi-stack5-response
FIRST once. The run record owns arming; native comparison is pending.

Finite cache-only TCP connect, one-chunk checked send and exact3072-byte HTTP
framing/body/EOF helper are software-qualified and dormant. Next wire explicit
finite syscall paths without changing legacy callers, then prove isolated init/
services, genuine prelive process readiness, PID/lifetime-bound numeric DNS answer
handoff, retained ordinary worker and actual IRQ TCP timers/terminal retirement.
No fake process, hidden wait, service logging or timeout-as-success is allowed.
Poll/close admission can wait; close0 is not TCP retirement. Prefer preserving
initramfs pathname/inode layout (e.g. an explicit quiet mode in existing httpget)
and rerun exact workspace preservation. A private unapplied frontend proposal
is retained. The new native profile needs measured credits and capture budget;
current conservative reserve33339 is only571 above mandatory32768. Connection
lifecycle/epochs, reattachment/UI, reliability and compatible2.4GHz hardware follow.
