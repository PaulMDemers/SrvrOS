# Per-machine development records

Follow the [coordination guide](../agent-coordination.md) for shared ownership,
PR integration and handoff checkpoints. The [Air profile parity review](../air-profile-parity.md)
compares Air7,2/Air6,1 source support and independent acceptance.

One agent develops locally on each Ubuntu machine. Start with the
[Ubuntu device-agent guide](../ubuntu-device-agent-setup.md), then update the
matching record before the first physical trial. The
[expansion plan](../apple-hardware-expansion-plan.md) is shared planning; these
records own observed configuration and acceptance for each model/unit.

The [Apple Silicon build host](apple-silicon-build-host.md) has a separate
[macOS setup](../macos-development.md). It builds and emulates x86_64; its VM
results do not add a native ARM64 machine to the hardware support table.

| Model | Record | Current evidence |
| --- | --- | --- |
| MacBookAir7,2 / A1466 | [macbookair7-2](macbookair7-2.md) | Active reference machine; partial device acceptance, including bounded window/navigation/Close and saved-file smoke; native 30-minute PCM/disk/app endurance accepts999 open/close cycles, three screenshots, zero underruns and clean retirement at unchanged limits on source`ca66d874`; interactive/audible acceptance stays separate. Cooperative GPU completion-state observation and [BCS readiness/cancellation](../intel-reset-readiness.md) accepted. Separate [shutdown](../intel-shutdown.md) accepts the already-disabled observation branch; enabled-ring writer remains unaccepted. Source`3028016d`/release`d260c6f11f043e51` additionally accepts [already-disabled shutdown observation with persistent reporting storage](macbookair7-2.md#latest-native-shutdown-already-disabled-observation-accepted), full preservation and clean retirement; its enabled writer remains unexercised. Source`f68ab030` now [accepts bounded same-borrow disabled observation, BCS-only reset and full preservation](macbookair7-2.md#accepted-full-census-retry-reset-and-preservation-complete): core96,445/100,000us, owner267,977/300,000us, outer346,637/500,000us, matching engine/display/HDA snapshots, both borrows returned and13 mappings retired. Source`4873e6bd` additionally accepts the reset provider-boundary integration: core61,212/100,000us, owner207,311/300,000us, outer279,649/500,000us, full preservation and clean retirement; already-disabled entry only, one bounded pass. Source`ee4a15bc` also accepts bounded five-engine prepare/hold/cancel and preservation in98,274/100,000us, with both borrows returned and13 mappings retired; the1,726us margin is one-run evidence only. Source`292163ac` subsequently accepts nested-census preparation/preservation in93,246/100,000us (6,754us margin),141 guards/230 target observations and clean retirement; one bounded pass only. Source`9c788f3f` now accepts the full pre-user-init maintained preparation/PCI/BCS8-reset/cancellation/preservation lifetime in97,414/100,000us (wake280,135/500,000us), with original borrow returned and13 aliases retired; one bounded pass, no repeatability or memory grant. Source`3315df79`/release`e07848a7c00d893d` also accepts [the original GGTT entry inside the live held reset](macbookair7-2.md#current-checkpoint-native-held-interval-source-binding-accepted): six read-only DWORD loads in59us, preparation97,379/100,000us and14 aliases retired with full preservation and clean return; this is source binding only. Source`f2a6c116`/release`e83571c874bc17f1` additionally accepts [bounded post-reset BCS re-arm/child/restore](macbookair7-2.md#current-checkpoint-native-post-reset-re-arm-and-restoration-accepted):358us scope/234us child,97,758/100,000us preparation, exact mixed-state restoration and14 aliases retired with full preservation; one bounded lifetime only. Source`e81caf7f`/release`978d9be94b96243a` additionally accepts [the bounded joint HDA/GPU claim and timing retry](macbookair7-2.md#native-joint-timing-accepted): preparation91,213/100,000µs, final guard5µs, borrow return3,182µs,132 full guards, all original claim observations and preservation, HDA6/2/2/6 restoration and clean retirement. Earlier failed/refused trials remain nonpasses. All255 outstanding memory/cache/translation/reader obligations, managed backing and native tear-free remain open.  Matched pre-init HDA/BCS owner now accepts already-disabled observation/preservation on source`a720d774` with GPU11/HDA1 alias retirement in307,103/500,000us; BCS stores0 leave its enabled writer unexercised; source `c150c6ad` / release `dd91b264c73a48a6` now accepts the [state-aware already-disabled entry](../intel-ring-entry.md#native-result-already-disabled-route-accepted), with zero BCS writes/epoch 0, fresh full post-census, complete preservation and clean retirement in 97,088/150,000 us; enabled writing and managed backing remain unaccepted; source `9e67bce7` / release `74d46f42447d218e` now accepts the [original-owner zero-geometry entry/reset composition](../intel-state-aware-reset.md#native-result-original-owner-entryreset-accepted), with preparation 91,213/100,000 us, complete preservation and clean retirement; nonzero reset, remaining memory ownership and native tear-free stay open |
| MacBookAir6,1 | [macbookair6-1](macbookair6-1.md) | Native desktop/input, bounded concurrent USB work, rerun refusal and speed/acceleration persistence accepted; brightness 5–100% and bounded speaker/headphone playback accepted; parked battery sampling, AC transitions, observed alarm refresh/retirement and bounded battery/Text USB-image coexistence and short mounted Home sampling accepted; bounded Home Text/Start/Stop and three-minute continuous desktop capture and attended AC indicator updates accepted; default battery startup remains off; opt-in ordinary monitoring and a 22-minute combined battery/brightness/audio/sensor session accepted with file persistence and clean retirement; automated and attended combined reopen accepted after integrity-preserving heap optimization (four attended lifetimes, 3.60–3.79 s startup, Text/Files work and clean retirement); bounded 30-minute audio/disk/app stress with three screenshots passes 703 app cycles and clean exit under unchanged limits; read-only CPU package power/active-frequency console and battery-owned desktop idle/app/recovery phases accepted, with 13 automated app cycles, valid power/temperature/worker coverage and clean retirement; event-driven desktop worker waits accepted with zero lifecycle timeout wakes in idle/apps/recovery; measured package power remains about 4.1 W; battery supervisor event waits also accepted with zero timer wakes and no covered idle/recovery switches; read-only hardware idle residency capture accepted across 30-second idle/load/recovery phases, with valid zero core C3/C6/C7 and package C2–C10 counters; read-only idle-entry/firmware audit confirms HLT entry with unchanged permissive C10 package limit but zero deeper residency; native CPU0 topology/BSP/MONITOR prerequisite capture accepted, with support enabled; explicit shallow MWAIT trial accepts 32 returns, 9.971 ms maximum masked wait and mandatory HLT fallback under unchanged limits; bounded C3-hint trial also passes wakeup/fallback (9.968 ms maximum) but observes zero core/package C3 residency; separate explicit AP ownership accepts 96 native MONITOR/C1 mailbox exchanges across three APs with maximum measured acknowledgement 2 us, zero faults and terminal retirement; bounded coordinated C3 now records positive residency on both physical cores in all 32 rounds, maximum whole round 9.983 ms, clean HLT fallback and AP retirement; read-only package audit accepts C2 residency in all 32 short rounds (about 99.54% of those brackets), unchanged 67.584 us IRTL and 25 stable PCI snapshots; package C3 remains valid zero and its blocker unidentified; RC6 series capture completed but strict timing acceptance still fails (about 0.0932% nominal excess), with policy and limits unchanged; a separate read-only Ubuntu reference observes package C3 at 13–14%, without resolving the SrvrOS blocker or RC6 timing; native display-well endpoints show the BIOS request set; final-only receipt placement is physically accepted with all direct exit proofs retained, compact final telemetry is also physically accepted with all 15 power/idle/entry samples and source-matched stress coverage, while strict RC6 timing still fails (about 0.0966% excess); bounded native 26-control endpoints are accepted with pipe A/eDP/DDI A routing and disabled sampled fitters/other buffers, and two eight-control display-audio/IRQ endpoints now survive directly with sampled outputs disabled and all IRQs masked/disabled; display HDA now has two valid native snapshots with command/position DMA and both stream RUN bits clear while the controller is out of reset; a no-write owner dry run now physically verifies the expected seven-unknown refusal and clean software retirement, with zero hardware authority; native primary-A/framebuffer endpoints and 1,056 contiguous aperture-page mappings are observed; two native 1,056-entry Haswell GGTT vectors agree and point into firmware-reserved stolen RAM with verified temporary-window retirement; subsequent version-2/3 captures observe raw CPU TILECTL zero twice with ten maps retired, and version3 observes 32 matching fence rows under the exact Haswell Linux model (only row0 active); a native firmware-only capture also observes two matching pinned DMAR copies and all 42 boot-catalog entries, with the declared VT-d pages in a catalog gap; a subsequent read-only CPU/cache capture observes two stable PAT/MTRR/control contexts and eight matching RAM page-table walks, with the candidate HHDM and private aliases absent and the covering MTRR UC; the subsequent pre-user-init alias/NX census saved a bounded failure at the unchanged 64-table cap, so exhaustive aliases and mapping permission remain unproved; the first opt-in v5 checked ECAM trial then halted before PCI/USB during adoption (state5,35949 words), with all prior USB files preserved; the instrumented follow-up observed one valid early adoption in98.684ms but still exhausted64 census tables; deferred full-capacity process mapping storage then removed96MiB of static BSS, with one native adoption in93.775ms and a complete30-table first pass, but the strict second pass refused a raw accessed-bit change; bounded guarded first-touch preparation then passed the full native profile with93.608ms ECAM adoption and exact30-table snapshots in40.181ms including preparation, accepting boot-only mapping and sole-managed-root context with unchanged limits and no VT-d resource/read or ownership grant; the subsequent explicit window-only profile now physically completes fresh admission, exact mapping and verified retirement/claim release in77.402ms within the unchanged100ms bound, with zero target accesses; the later two-pass A-preseeded zero-load qualification also accepts exact raw graph equality and retirement in82.610/82.379ms with unchanged limits, followed by native acceptance of the guarded read-only register sampler as summarized below; translation state, resource admission, reference-count discrepancy, GT/full-path swizzle and real ownership remain unbound; actual ownership and unused-well proof remain open; ordinary boot still manages CPU0 only, and package-level idle, sustained policy and longer endurance remain pending; exact-source isolated two-pass read-only sampler accepts24 DWORD reads with identical raw samples/graphs, verified retirement and88.955/83.461ms lifetimes; no write/table-walk/ownership grant; ordinary desktop regression on runtimea9e72ecc remains historical FAILED evidence (21 excursions,752.295ms maximum loop); checked user timer-return preemption on runtimefac79995 now passes the native1800-second audio/disk/app/three-screenshot workload with1080 app cycles,56109 valid loop samples, zero excursions,130.032ms maximum loop, zero recorded audio underruns and clean file/allocation/worker retirement; a separate30-second speaker check at25% now has user-confirmed audible working sound, while unattended endurance sound remains unassessed; native write batching on runtime989846b3 also passes1800-second stress with1078 app cycles, three screenshots, zero underruns and120.142ms maximum loop; slowest screenshot improves from43.5s to23.2s (WRITE31.9s to12.7s), with remaining completion latency open; native private reserved append on runtime `51670ae6` now passes the original 1800-second workload with 1,079 app cycles, three exact screenshots, 80 proved copies, zero recorded underruns, 121.468 ms maximum loop and clean file/allocation/worker retirement; screenshot median is 11.05 s, with remaining ordinary admission latency open; runtime `4629fced` now passes the original native 1800-second workload with 1339 app cycles, three exact screenshots, zero recorded underruns, 121.488 ms maximum loop and intact files/allocation/retirement; screenshots still take 7.5–21.7 s and retained exec/fcntl gate waits remain 4.21/5.44 s, so native private-path coverage and latency acceptance remain open; runtime `126a0f18` subsequently provides valid native launch/fcntl diagnostics and 385 private handled results, but the original workload fails its ten-second Files cycle after 442 completed cycles: parent5095 exceeds4096 mappings, exec waits6.41s and legacy helper creation waits4.68s across close; two of three screenshots save, files/allocation remain intact, and full workload/performance acceptance remains failed |
| MacBookAir7,1 / A1465 | [macbookair7-1](macbookair7-1.md) | Planned; no local inventory or physical results |
| MacBookPro11,2 / A1398 | [macbookpro11-2](macbookpro11-2.md) | Native apps, input, brightness, sound and read-only CPU/two-fan telemetry and captured USB Ethernet cable recovery/UI exercised; trackpad latency work deferred |
| MacBookPro11,3 / A1398 | [macbookpro11-3](macbookpro11-3.md) | Planned; no local inventory or physical results |
| Macmini7,1 / A1347 | [macmini7-1](macmini7-1.md) | Planned; no local inventory or physical results |

Latest Air7,2 v27 trial (`cef27b1c` / `127a222d11cc79fc`) verifies a bounded
enabled-entry suboperation, then safely refuses reset/requester admission from
nonzero geometry. The full sequence remains incomplete; all alerts, return
integrity and unchanged recovery are recorded in the unit ledger. No sibling,
requester, managed-memory or native tear-free acceptance is inferred.

Use the [record template](TEMPLATE.md). Put portable observations, source
commits and acceptance summaries in Git. Keep raw inventories, images, local
mount paths, UUID/reboot state and full logs in the machine's ignored `build/`
records, with accepted recovery copies outside `build`. Do not commit secrets,
serial numbers or private documents. If there are multiple units of a model,
give each its own labeled record rather than overwriting another unit's evidence.

Before ending a work block or rebooting, record what is installed, what will run,
where evidence will be saved, whether reboot is armed, and what the next agent
turn should do. On return, collect first and clear stale planned/armed state only
after checking the host. Scripts do not provide cross-machine orchestration.

Updates to shared code should state the touched subsystem, base commit, tests,
physical evidence and remaining unknowns. Integrate through the shared branch's
owner/review process; other machines should record the new base and rerun relevant
regressions. An agent should not assume remote changes or another device's work
have landed until it fetches and inspects the actual commits.

The reference Air7,2 now has a separate [nonzero-geometry reset diagnostic](../intel-nonzero-reset.md)
physically accepted for one bounded original-owner lifetime on source `2efa8028`
/ release `562d35107d14e298`. Its
original kind1 source/epoch/borrow, maintained readiness, strict postconditions
and preservation verify on native hardware, including strict reset-default
geometry, clean retirement and offline return integrity. It does not widen the
legacy zero-geometry gate or any sibling
profile, or grant requester/managed-memory/native tear-free acceptance.

The separate [v29 original live post-reset source child](../intel-nonzero-source.md)
is locally qualified on Air7,2 source `a9418c83` / release
`451cd4d6a9f36faa`: 34-image/core, fatal sanitizer/clock matrices, historical
replays and exact-stage VM persistence/recovery pass. The matching native run
safely refuses an already-disabled kind 2/epoch 0 source before reset admission
or GGTT reads. Full retirement/media/recovery verify; this is incomplete, not
native source acceptance. No memory/claim/requester or sibling acceptance follows.

Air7,2 has locally qualified a separate [v30 original state-aware startup
reset/source](../intel-startup-source.md) image. It binds either actual original
kind 1/nonzero or kind 2/zero entry, preserving each epoch/source/borrow and all
existing limits. Old APIs/images remain strict. One native already-disabled
v30 lifetime is accepted below; enabled-route acceptance, managed backing and
tear-free remain pending. No sibling or historical receipt supplies live authority.

Frozen v30 source `82dd3da8` / release `327a401a69ca3855` passes 35-image/core,
fatal sanitizer/two-route matrices, historical replay, native object and exact
stage VM persistence/recovery checks. Its matching native session
`session-0006601f0dd19f83-00` now accepts one original already-disabled/zero-entry
reset/source lifetime: preparation 97,434/100,000 us, six original GGTT DWORDs
in 3,456 us, complete preservation/return and 14 GPU/HDA1 alias retirement.
Collector-only `c0dcda2e` corrects shared v30 census dispatch; unchanged raw bytes,
original rejection and strict replay proof are sealed. Offline media/recovery
verify; current is v30, recovery retained and no reboot armed. Enabled v30 route
and managed memory/native tear-free remain unaccepted. See the
[latest unit result](macbookair7-2.md#native-v30-original-already-disabled-resetsource-accepted).


Air7,2 now has a hardware-ready [v31 original-source private-buffer
binding](../intel-startup-buffers.md). Native code `876b500e` passes 36-image/core,
fatal adapter/hold sanitizer, nine historical native replay and isolation checks;
host-only staging fix `7c3a7531` passes 111 profile tests with all runtime bytes
unchanged. Exact release `c154f8b4a6534d11` passes EFI/MBR VM persistence/recovery.
Two full private buffers remain held through the actual source/reset/outer root,
then verify and release only after clean retirement. Transfer remains refused
with zero assurances/managed pages and all 255 obligations. Candidate is banked,
not installed or armed; native v31 and tear-free acceptance remain pending.


The subsequent [native v31 Air7,2 result](macbookair7-2.md#native-v31-original-source-private-buffers-accepted)
accepts one original kind 2 already-disabled source/private-buffer lifetime on
runtime `876b500e` / release `c154f8b4a6534d11`: two full buffers remain held through
outer retirement, verify and release exactly once (2,712 pages, DMA57→57).
Preparation is 98,198/100,000 us; full preservation/retirement and media/recovery
verify. Enabled entry and repeatability remain pending. No managed GPU page or
tear-free grant; c154 current, recovery retained, no reboot armed.


Air7,2 source `f638ae7c` / release `8d0181c0ed0a4708` now accepts one native
v32 original kind 2 [registered source-transfer lifetime](../intel-startup-transfer.md#native-original-kind-2-lifetime-accepted):
known authority refusal/check/close in45 us, preparation97,996/100,000 us,
complete source/buffer/root preservation and retirement, four padded patterns
and exactly-once2,712-page release. All1,497 prior files preserve apart from
expected metadata; three logs yield1,500files/156sessions. Raw/strict proof,
offline media/recovery/default and consumed one-shot pass. Current8d01,
acceptedc154/prior327a and recovery retained; USB unmounted, no reboot armed.
Zero transfer provider/clock/arena/PTE activity does not imply zero enclosing
wake/reset stores. Enabled route, repeatability and actual interval ownership
remain open; all255 obligations/0assurances/0managed GPU pages. No native
tear-free acceptance. Next integrate deferred main, then resolve hardware
ownership witnesses before a GPU writer; no unchanged refusal retry is needed.
