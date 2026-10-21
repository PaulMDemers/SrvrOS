# MacBookAir7,2 / A1466

Updated: 2026-10-21. State: **active reference machine; partial device acceptance**.

### Active scope: native full Text capture

Starting from merged PR #28 (`529eee4`), add opt-in v41 `--battery-text`
selection, exact-stage rehearsal and strict native collection. Shared edits are
limited to the battery diagnostic entry point, boot selection/staging, collectors
and their tests; editor/compositor and native hardware gates stay as validated.
Pro PR #19 inspected at `e9320d9` reserves v39 Wi-Fi; preserve that independent
scope. The automatic trial needs no input: require 60 presented edits, five `AA`
checkpoints, clean app/service retirement and continued sampling. Rehearse the
exact native payload's unsupported-platform save/unmount/return before install.
Implementation `98bebc9` passed native/VM builds, full host core tests, 44 staging
and 13 collector tests, seven strict overlap-record tests and 393 documentation
links. The rebuilt automatic Text VM presented all 60 events, retained all five
`AA` checkpoints and final `AA`, with zero extra keyboard input. Sampling advanced
1→25 and a replacement compositor started after clean retirement. Eleven files
including exact binaries are archived outside `build`. PR #29 carries this work.
Physical acceptance remains pending exact-stage rehearsal and a native trial;
retain the installed v40 and desktop recovery. Actions remain disabled.

### Current checkpoint: native RAM desktop/battery overlap accepted

Release `ae2851607a9bf0c1`, source `1df9a24` (merged PR #26), session
`session-00065e5c8e37f13c-00` completed the automatic v40 trial and returned to
Ubuntu. The typing-study compositor ran for 60.019 seconds, generated and
presented all **60 changes**, and completed 61 frames across 5,314 loops.
Maximum loop gap was 30,009 microseconds; this includes scheduling and is not
measured physical input latency. Battery sampling advanced generation 1→23;
3,004 cached reads included 707 while sampling. The compositor exited with
status zero and the service retired with balanced attachment and zero rejected
samples, heap, errors or denied operations. Full SCI counters recorded
**23 handled / zero unclaimed** interrupts.

The collector found one matching session, complete overlap proof and no attention
items. All 901 prior data files, including the used filesystem-test image, were
byte-identical. Only workspace metadata changed, with three new matching logs.
Offline exFAT and allocation checks passed: 192 directories, 905 files, 1,684
clusters, no leaks or mismatches. A new Ubuntu boot, consumed one-shot, default
and EFI order, installed payload and accepted desktop recovery hashes verified.
Evidence and the exact payload are archived outside `build`.

Current installed is this diagnostic; previous remains accepted desktop
`b427b78f21644bd2`. No reboot is armed. This accepts the bounded RAM typing-study
and generated-event overlap, not physical typing/gesture latency, full Text-app
keyboard events, filesystem-load overlap or gauge accuracy. Ordinary desktop
battery startup remains disabled. Next validate the rich Text keyboard-event
path in RAM, then combine the isolated disposable filesystem workload; retain
separate attended input acceptance.

### Full Text input in RAM: implemented and VM validated

PR #28 builds on `3b20bfb`; runtime/compositor/Text implementation `9ee553a`.
The new monitor-only `battery-desktop-text` mode runs the real Text editor with
its file worker disabled. It retains normal rich keyboard events, editing,
clipboard IPC and surface publication. The existing v40 study profile and the
installed/accepted native payload above remain unchanged; ordinary desktop
battery startup stays off. Pro PR #19 remains `e9320d9`, with no shared-file
conflict in this change.

Seven Text VM cases passed: automatic script, injected keyboard editing, missing
program, owner deadline, early exit, unsupported platform and active-runtime
fault/reset. Both positive cases presented all 60 generated operations and five
exact `AA` editing checkpoints while sampling advanced generation 1→25. The
injected case delivered 44 keyboard events, acknowledged the final serial, and
ended with visible `ZbZb`; ordered events may share frames. Every clean-return
case subsequently started a replacement compositor. The old study, full host
core suite, strict record/corruption tests and ordinary desktop-app regression
also passed. No guest timeout or native hardware gate changed.

The initial injected VM lacked final serial records while its document was
visible. Its failed evidence is retained; draining serial between input bursts
resolved the repeat. Refreshing the older apps harness also corrected stale
folder-field coordinates, implicit `/fat` defaults after the move to Home,
and maximized restore coordinates. Screenshots/raw disposable disks established
the path mismatch; a split child exit log was replaced by checking the parent's
status for that exact PID. The refreshed suite verifies actual file contents,
clipboard editing and maximize/restore transitions. All failed attempts remain
separate from final validation.

Next add a separate native Text capture recipe and strict collection proof,
rehearse its exact stage, then run the automatic laptop trial. Full Text native
acceptance, concurrent isolated filesystem load, attended input latency and
battery gauge accuracy remain pending. No physical trial is armed by this work.

### Previous checkpoint: EC GPE rearm ordering accepted for the bounded trial

Release `54dce7fb0a41b718`, source `b7bd60c` (merged PR #24), session
`session-00065e5b4cd48eb4-00`, returned automatically. Full retired counters
recorded **24 handled / zero unclaimed** interrupts across two lifetimes,
compared with 32 handled / 32 unclaimed in the preceding query-context run.
Nine callbacks were retained and fifteen overwritten. All retained entries
observed an idle query worker; all post-EOI samples had ISR/IRR and remote-IRR
clear (LAPIC bits 4, route low `0x8029`). The pre-resume pending check eliminated
the observed duplicate-delivery pattern in this trial. This is bounded native
acceptance, not a guarantee against late arrivals or all future unclaimed IRQs.
Claims, EOI ordering, route polarity and hardware gates remain unchanged.

The isolated filesystem completed 42 cycles / 756 operations in 61,319,529
microseconds with 16,910 USB commands. Maximum operation time was 3,157,927
microseconds; this is wall time, not measured input latency. Generation 23 and
61,290 cached reads (maximum 18 microseconds), including 2,194 during sampling,
were recorded. Both lifetimes retired with restored enables, balanced attachments
and zero heap/errors/denials. The deliberate short stop wait timed out, then
retirement completed normally.

Exact inner contents/fsck/allocation (75 clusters), outer fsck/allocation (191
directories, 902 files, 1,680 clusters) and preservation of 897 unrelated files
passed. Only the disposable image/root metadata changed, with three new matching
logs. Ubuntu return, consumed one-shot, default/EFI order and recovery hashes
verified. Twenty-eight hashed evidence files and the exact payload are archived
outside `build`. Current installed is this diagnostic; accepted desktop
`b427b78f21644bd2` remains previous. No reboot is armed. The image is used and
must not rerun. Ordinary desktop battery startup remains disabled; this trial
has no physical input or gauge-accuracy acceptance.

### Implementation and local validation: RAM desktop and battery overlap

Branch `device/macbookair7-2/sci-controller`, PR #26, integration base `4daf2c2`.
Runtime `aaab764`, bulk unmap fix `054b6cf` and harness `39aa9fa` implement an
opt-in v40 `--battery-desktop` laptop probe. Ordinary desktop battery startup
remains disabled. The workspace is parked during the RAM typing-study capture;
Home, preferences, screenshots and storage load are excluded. The used filesystem
fixture is left unchanged. Pro PR #19 (`e9320d9`) retains v39 for its Wi-Fi probe;
its hardware profiles and xHCI scope are unchanged.

The independent runtime owner starts active cached sampling, observes the desktop
for one minute, checks process exit and sampling progress, then retires the
service before remounting storage and saving logs. Generated font-toggle input
requires a matching client serial and successful compositor presentation. The
study uses legacy text input, not the rich Text-app keyboard-event path. Startup
proof is printed after retirement to avoid concurrent console-writer interleaving;
its saved timestamp still identifies actual start. Maximum compositor loop gap
includes client startup and is not physical input latency.

The first VM presented all 60 events and typed `Ab`, then hit the existing EC
sample deadline during compositor cache cleanup. This failed evidence is retained.
`process_munmap` performed two linear mapping-table searches per page. Single-pass
range removal preserves full validation and frame ownership, and the repeat VM
completed cleanly. Bulk/split/unrelated-mapping regression coverage was added to
posixdemo. EC deadlines, IRQ claims, EOI ordering and native hardware gates have
not changed.

Final normal, missing-program, deadline, early-exit and unsupported-platform VM
cases passed, each followed by successful replacement compositor startup. Normal
captured generation 1→25, 60 generated/presented changes, 66 frames, and injected
`Abx` followed by Backspace produced `Ab`. Full local `make smoke` and the
software SCI regression passed. Record tests reject missing, conflicting and
non-overlapping proof. The active-runtime deadline fixture also returned automatically without unsafe
cleanup or a false success record. All six cases and exact binaries are archived
outside `build` under `local-validation/coexist-39aa9fa`. Exact native stage
rehearsal is recorded separately before installation. GitHub Actions
remain disabled. The native v40 result above accepts bounded RAM overlap; none
of this accepts physical input latency, gauge accuracy or loaded filesystem coexistence.

### Previous native query correlation accepted

Release `df04f96a16d0f850`, source `68aac71` (PR #23), session
`session-00065e5b05b8b6c9-00`, returned automatically after 45 cycles / 810
operations in 60,749,550 microseconds (18,116 USB commands; maximum operation
2,439,655 microseconds). Both lifetimes retired cleanly with restored enables,
balanced attachments and zero heap/errors/denials. The service reached generation
23; 64,685 cached reads completed, maximum 3 microseconds, including 2,263 during
sampling. The intentional short stop-wait timed out before clean retirement.

Counters recorded 32 handled / 32 unclaimed callbacks; ten were retained and
54 overwritten. All five retained unclaimed entries found a **new queued batch**
aged 5–6 microseconds, with zero transfers and no known response code. They
followed handled callbacks by 558,139–717,617 microseconds; selected EC event
status/enable was `0xa0`/zero and fixed status/enable `0x8001`/`0x20`. Each retained
post-EOI snapshot again had ISR/IRR clear and remote-IRR clear, with LAPIC bits 4
and route low `0x8029`. The query worker was not executing a transfer or AML at
these entry points. Source inspection finds GPE resume **before** the pending
check that can suspend/clear it and queue this new batch. That ordering is a
candidate cause of a latched delivery whose event is gone by callback entry;
the correlation alone does not prove the cause or justify claiming that IRQ.

Inner exact contents/fsck/allocation (75 clusters), outer fsck/allocation (190
directories, 899 files, 1,676 clusters) and preservation of 894 previous files
passed. Only the disposable image/root metadata changed, with three new logs.
New Ubuntu boot, consumed one-shot, default/EFI order and recovery hashes passed.
Twenty-nine hashed evidence files and the exact payload are archived outside
`build`. USB remounted; previous desktop `b427b78f21644bd2` retained. No reboot
armed; the used image must not rerun. Desktop/input/gauge acceptance stays pending.

### Rearm implementation scope and validation (PR #24)

Branch `device/macbookair7-2/sci-controller`, base main `68aac71`. Add a pending
check while the owned EC GPE is still suspended: queue known work directly,
otherwise resume and keep the existing post-resume pending check. Preserve
budgets, ownership, genuine handler claims, raw-handler semantics and late-arrival
coverage. Touch `battery_runtime.c`, a small `ec_query_rearm.h` ordering helper,
its host model test/Makefile target and current guides. Model known-pending and
late arrivals with a latched-interrupt fixture; run the battery VMs before a fresh
automatic native trial. A late arrival during resume can still latch a delivery;
this change does not promise to eliminate every unclaimed IRQ. Pro PR #19 at
`e9320d9` remains USB Wi-Fi/xHCI/staging work; only additive Makefile tests overlap.
Ordinary desktop battery remains off; GitHub Actions remain disabled.

Rearm implementation `3129374` (PR #24) passes the latched-delivery/late-arrival
host model, core suite, native/VM builds and six focused VMs: software-unclaimed,
normal service, foreign event, isolated filesystem, active filesystem deadline
with automatic reset, and query deadline containment. The isolated image and
all outside bytes pass verification. The ordinary kernel is byte-identical to
PR #23's full-smoke baseline; that unchanged baseline was not rerun. Docs and
whitespace checks pass. Local evidence and binary hashes are archived outside
`build`; physical ordering efficacy is accepted for the bounded trial above.

### Previous native post-EOI capture accepted

Branch `device/macbookair7-2/sci-controller`, integration base `2c90dfe` (PR #21).
Release `71b16a6e59709895`, session `session-00065e52c3a9ec0c-00`, completed the
automatic native trial and returned to Ubuntu. The workload completed 45 cycles /
810 operations in 60,719,544 microseconds, using 18,116 USB commands; maximum
operation time was 1,968,515 microseconds. Both service lifetimes retired with
restored enables, balanced attachments and zero heap/errors/denials. Generation
23 and 64,749 cached reads (maximum 18 microseconds), including 2,517 during
sampling, were recorded. The deliberate short stop-wait timeout was followed by
clean retirement; it is not an abandoned worker.

Full counters recorded 24 handled / 24 unclaimed callbacks. Ten completed
callbacks and thirty controller rows survived ring retention; 38 older callbacks
were overwritten. All snapshots were available on vector 41 / GSI 9. Before
and after each callback, LAPIC ISR/TMR were set and IRR clear; after generic EOI,
ISR and IRR were clear, TMR remained set (`lapic_bits=4`), and route low was
`0x8029`: remote-IRR and delivery-pending clear, level trigger retained. The
pre-EOI route was `0xd029` before handled callbacks and `0xc029` afterward;
unclaimed callbacks had `0xc029` at both pre-EOI points. LINT0 remained `0x10700`,
PIC masks `0xffff`, route high zero. Thus the existing acknowledgement takes
effect in every retained sample. Sequential snapshots do not establish line
voltage or exclude a subsequent interrupt.

All ten completion rows were observed with query work pending. Entry-to-post-EOI
time was 26 microseconds for each retained unclaimed callback, 33–34 for the
first lifetime's retained handled callbacks, and 368 for the second lifetime's
first handled callback (callback time 357). These are not `iret`, worker-duration
or input-latency measurements. The five retained unclaimed callbacks followed
handled callbacks by 553,381–754,688 microseconds, with EC status/enable
`0xa0`/zero and fixed status/enable `0x8001`/`0x20`; their callback duration was
15–16 microseconds. The extra-delivery cause remains unresolved. No evidence
here justifies altering EOI order, route polarity, event enables or claim policy.

Exact inner contents, fsck and allocation (75 clusters) passed. Outer fsck and
allocation passed: 189 directories, 896 files, 1,672 clusters, no leaks/mismatches.
891 existing files remain byte-identical; only the disposable image and root
metadata changed, with three new matching logs. New Ubuntu boot, consumed
one-shot, default/EFI order and installed/recovery hashes verified. Twenty-eight
hashed evidence files and an independent exact payload survive outside `build`.
The USB is remounted, previous desktop `b427b78f21644bd2` retained, no reboot
armed. The image is used and must be freshly prepared before another trial.
This accepts the isolated workload and diagnostic lifecycle, not ordinary desktop
battery startup, physical gauge accuracy or interactive input coexistence.

### Query execution correlation implementation (PR #23)

The source audit confirms `query_scheduled` spans queued work, EC query transfer,
AML evaluation and rescheduling until GPE resume. Existing observations cannot
distinguish those stages. Next add bounded passive query phase/sequence/timing
correlation to the diagnostic runtime and host/VM validators, reusing successful
reads and existing scheduling transitions. Keep output outside IRQ context; no
new EC/event reads, policy changes or sibling hardware gates. Implementation scope: add a pure query-phase observer to `sci_observe.h`,
entry snapshots in `battery_runtime.c`, software query-transfer IRQ injection
in `battery_vm_platform.c`, and matching host tests and
`tools/acpi_battery_smoke.py` record validation. No IRQ dispatcher or USB edits.
Base is main `a3aaa27`; Pro PR #19 remains separate. Native acceptance will require
a fresh isolated automatic trial after local validation. Ordinary desktop battery
remains off. Pro PR #19 at `e9320d9` concerns USB Wi-Fi inventory, xHCI and staging;
no IRQ/runtime overlap was found. Preserve both branches' Makefile test additions.
GitHub Actions remain disabled; validation is local.

Query correlation implementation `f3fa54d` (PR #23) passes host snapshot/lifecycle
checks, seven malformed-record cases, the core suite and native/VM builds.
Five focused VMs pass: software-unclaimed, normal service, foreign event,
isolated filesystem and active filesystem deadline/reset. The injected case
retains five query-transfer and five idle entry contexts; the transfer contexts
have a known batch/attempt and correctly unknown response code. The workload
checks preserve all outside bytes and verify final image contents. Full local
baseline smoke passes syscalls, 481 shell commands, shell editing, USB keyboard
and display startup. Documentation links and diff whitespace pass. Validation
is archived outside `build`; exact-stage VM and native query correlation are now
accepted above.

### Post-EOI implementation and local validation (PR #21)

Implementation `4b2560c` adds an optional token-owned IRQ completion observer
after the existing EOI and before interrupt return. It records a third controller
snapshot and entry-to-observation time/query state without changing claim, route,
mask, EOI order or EC/event-access policy. Core, registry ownership, real dispatcher
ordering, six record-corruption/unknown-state cases and native/VM builds pass.
Full baseline smoke passes syscalls, 481 shell commands, shell editing, UEFI USB
keyboard and display startup. Five focused battery VMs pass: injected-unclaimed,
normal service, foreign event, isolated filesystem and active filesystem deadline
with automatic reset. Outside bytes, retirement and recovery checks pass as
applicable. Exact-stage Q35 platform refusal, diagnostic save, unmount and return
also pass. Local validation and payload hashes are archived outside `build`;
physical acceptance is recorded above.

### Native SCI controller capture accepted

Release `7357a2eee058622e`, source `d9950fb` (merged PR #17), session
`session-00065e523d6edce6-00` completed the automatic isolated-filesystem trial.
42 cycles / 756 operations ran in 60,968,560 microseconds, with 16,910 USB commands
and a 3,669,507-microsecond maximum operation. Both service lifetimes retired
cleanly with restored enables, balanced attachment and zero heap/errors/denials;
generation 23 and 61,290 cached reads (maximum 18 microseconds) were recorded.
This workload timing does not establish desktop/input latency or observer cost.

The full counters recorded 29 handled and 29 unclaimed callbacks. Ten completed
callbacks were retained; 48 were overwritten. All twenty paired controller
snapshots were available on vector 41 / GSI 9: ISR and TMR set, IRR clear,
LINT0 `0x10700`, PIC masks `0xffff`. Each retained handled callback showed route
low `0xd029` before and `0xc029` after; each unclaimed callback showed `0xc029`
at both points. All route high words were zero. The route's remote-IRR bit was
set throughout these observations. These are sequential **pre-EOI** reads.

All five retained unclaimed callbacks still had EC status `0xa0`, enable zero,
fixed status/enable `0x8001`/`0x20`, and query work pending before and after.
They followed handled callbacks by 578,961–751,884 microseconds and lasted
15–16 microseconds including the controller reads. This is evidence of a
level-triggered APIC delivery with no enabled event in the selected register
reads; it does not establish its cause or justify swallowing the interrupt.
A checksum-validated, read-only MADT audit confirms source 9 / GSI 9, flags 13:
explicit active-high/level, matching the observed route. Do not substitute an
assumed active-low SCI configuration for this machine's firmware evidence.

Exact inner contents, fsck and allocation (75 clusters) passed. Outer fsck and
allocation passed with 188 directories, 893 files, 1,668 clusters and no leaks or
mismatches. 888 previous files are byte-identical; only the disposable image and
root metadata changed, with three new matching logs. New Ubuntu boot, consumed
one-shot, default/EFI order and recovery payload hashes were verified. Accepted
evidence contains 29 hashed files outside `build`, plus an independent exact
payload backup; accepted desktop `b427b78f21644bd2` remains the recovery entry.
The current test image is used and must not be rerun without fresh preparation.

Next bounded investigation: observe controller state after generic EOI and IRQ
return timing alongside query scheduling. The source audit finds EOI directly
after dispatch; these snapshots cannot verify its effect. Keep claim, route,
mask and event acknowledgement policy unchanged until evidence supports a fix.
Ordinary desktop battery startup and physical gauge/input acceptance remain off
or pending. Integration baseline `6c6bf9e` includes Pro's updated unit record;
Pro PR #19 at `f2af3f2` concerns USB Wi-Fi inventory and does not overlap this
scope. GitHub Actions remain disabled; validation is local.

### Controller snapshot implementation (PR #17)

Branch `device/macbookair7-2/sci-controller`, base main `bd0fe58`.
Added a bounded xAPIC vector-state read API and diagnostic-only snapshots before
and after the uACPI SCI callback. Records include ISR/IRR/TMR, LINT0 configuration, the
owned I/O APIC route and PIC masks; both snapshots precede generic EOI. No EOI,
route, mask, event acknowledgement or claim-policy change. Availability and
sequential-read limits remain explicit. Changes cover LAPIC helper/tests, battery
observer/runtime, VM record validation and documentation. Pro scope inspected
at implementation time (`ecad3df`) concerned network/UI acceptance, with battery
deferred. Host and VM validation preceded the accepted native trial above.

Implementation: paired controller rows in the existing eight-entry ring; xAPIC
reads are bounded to ISR/IRR/TMR/LINT0, and no IRQ return or acknowledgement
policy changes. Host core suite, all vector/bit combinations, observer pairing
and corrupted/truncated record fixtures pass. Source `6e7c93f` passes five local
Q35 cases: injected unclaimed, normal service, foreign event, isolated filesystem
and active filesystem deadline/reset. All available controller identities match
the fixture; twelve injected-callback snapshots report no APIC vector state.
The initial stale VM binary was rejected for missing controller records; the
final runs used the rebuilt VM kernel. The isolated test verified the inner tree
and all outside bytes, and deadline recovery returned automatically. Validation
is archived outside `build`; GitHub Actions remain disabled. Physical acceptance
is now recorded above.

Prior accepted **passive SCI observation**: release `57ac1e9954e445e9`,
source `d9d5afc` (merged PR #15), session `session-00065e5157c6e854-00`.
The isolated workload completed 43 cycles / 774 operations in 60,859,549
microseconds, with 17,311 USB commands and a 2,796,485-microsecond maximum
operation. This single run does not attribute timing variation to the observer.
Both service lifetimes retired with balanced attachments, restored enables and
zero heap/errors/denials; 62,443 cached reads completed, maximum 18 microseconds.

The ring retained ten callbacks across the two lifetimes, including five
unclaimed callbacks; 38 older callbacks were overwritten. All five retained
unclaimed callbacks had complete first-read evidence: EC mask `0x40` was clear
in status `0xa0`, EC enable was zero, and fixed status `0x8001` did not overlap
fixed enable `0x20`. Raw and filtered EC bytes agreed. Query work was pending
before and after each callback. Each followed a handled callback by
536,339–741,498 microseconds and took 4–5 microseconds. The preceding retained
handled callbacks saw EC status `0xe0`, enable `0x40`, and scheduled query work.
Total active deliveries remain 24 handled / 24 unclaimed; no setup/retirement
or in-flight deliveries remained. This narrows observed callback conditions;
it does **not** establish an interrupt source, queued-delivery cause or safety
of ignoring unclaimed interrupts. Keep the actual handler result unchanged.

Exact final image, inner fsck/allocation (75 clusters), outer fsck (187 directories,
890 files) and outer allocation (1,664 clusters, no leaks/mismatches/errors) passed.
885 existing files are byte-identical; only the disposable image and root inode
sidecar changed, with three matching new logs. New Ubuntu boot, consumed one-shot,
boot default/EFI order and recovery hashes verified. Twenty-nine hashed evidence
files and the payload are archived outside `build`. Installed is this diagnostic;
previous accepted desktop remains `b427b78f21644bd2`. No reboot is armed.

Next: bounded controller-delivery/IRQ-return observations alongside query timing.
The source audit finds EOI after dispatch and a level-triggered SCI route; there
is no demonstrated ordering repair yet. Ordinary desktop battery remains off;
input coexistence and physical gauge accuracy remain separate acceptance work.

### Passive SCI observation implementation — October 20

Branch `device/macbookair7-2/sci-observation`, base main `3c1739b`.
Investigate the repeated 24 handled / 24 unclaimed active SCI deliveries before
ordinary desktop battery integration. Touch the diagnostic battery runtime,
a pure bounded observation helper, host tests and the battery VM harness. Capture
only existing successful uACPI event reads (fixed status/enable and the EC GPE
byte), first-read values, callback result, timing and query scheduling state.
No additional event/EC reads, acknowledgements, event enable changes, native model
gates or IRQ claim-policy changes. Report outside IRQ context and retain missing
reads as unknown. Timestamps use the existing read-only PM timer. Validate bounded storage and the injected unclaimed VM case
before a native automatic capture. Implementation `2da3043` passes the host core
suite, observer unit/record-corruption tests, native/VM builds and all five focused
VM cases (injected unclaimed, normal service, foreign event, isolated filesystem
and active deadline). The native result is recorded above. Pro branch inspected at `4f9fa06`; its current
scope is post-merge desktop validation, with battery deferred.

Previous accepted **bounded USB data batching**: release `e1ba87c6133b2e9d`,
merged source `cd732ec` (PR #13, including the Pro PR #5 baseline), session
`session-00065e50be1d9e42-00`. The automatic run completed 45 cycles / 810
operations in 60,779,542 microseconds with 18,116 USB commands. Maximum storage
scope was 2,176,320 microseconds; the slowest operation was the initial large
write at 2,176,314 microseconds. Its data callbacks used 1,446,984 microseconds
in 18 calls / 270,336 bytes; flushing used 5,023 microseconds. The previous
profile below needed 66 data calls and 4,702,530 microseconds for that operation.
Throughput rose about 37% and commands per cycle fell about 19%. These are
single-run comparisons; callback wall time includes admitted services and does
not measure desktop input latency.

Both service lifetimes retired with balanced attachments, restored enables and
zero heap/errors/denials. The service reached generation 23; 64,685 cached reads
(maximum 18 microseconds), including 2,261 while sampling, completed. Active SCI
still totals 24 handled / 24 unclaimed; the cause remains unresolved. Exact final
image contents, inner fsck and its 75-cluster allocation passed. Outer fsck is
clean (186 directories / 887 files), with 1,660 clusters and zero leaks,
mismatches or errors. 882 existing files are byte-identical; only the disposable
image and root inode sidecar changed, with three new matching logs.

New Ubuntu boot, consumed one-shot, boot default/EFI order and recovery hashes
were verified. Evidence and payload are archived outside `build`. At that checkpoint installed was
`e1ba87c6133b2e9d`; accepted desktop recovery remains `b427b78f21644bd2`.
No reboot is armed. This accepts the isolated workload and runtime retirement,
not ordinary desktop battery startup, physical gauge accuracy or input latency.
The used diagnostic image cannot be rerun without fresh preparation. Next:
investigate unclaimed SCI deliveries, then establish desktop/input coexistence
with explicit service admission and lifecycle checks. Actions remains disabled.

Previous accepted **write profiling**: release `f99f6759a1ed8f68`, source
`b090d94`, session `session-00065e4ff61b5b24-00`. The automatic run completed
33 cycles / 594 operations in 61,198,562 microseconds with 16,454 USB commands.
Slowest operation: cycle 1, step 1, 4,702,530 microseconds. Data-write callbacks
used 3,737,960 microseconds (66 writes / 270,336 padded bytes); all flushes used
24,519 microseconds; only 44,808 microseconds fell outside raw I/O callbacks.
The maximum data callback was 274,351 microseconds. This is callback wall time,
including admitted services, not CPU or input latency; the higher maximum than
the preceding run is not enough to attribute a performance regression.

Both service lifetimes retired cleanly with restored event enables, balanced
attachments and zero heap/errors/denials. Generation 23 and 50,785 cached reads
(maximum 18 microseconds) were captured. Active SCI remains 24 handled / 24
unclaimed. The final image/tree and 75-cluster allocation check passed. Outer
fsck is clean (185 directories / 884 files), with 1,656 clusters and zero leaks,
mismatches or errors. 879 prior files are byte-identical; only the disposable
image and root inode sidecar changed, with three new session logs. New Ubuntu
boot, consumed one-shot and recovery hashes verified; 29 evidence files and the
payload are archived independently. At that checkpoint this profiling diagnostic was installed;
previous accepted desktop remained `b427b78f21644bd2`. No reboot is armed.

### Bounded USB data batching — October 20

Branch `device/macbookair7-2/usb-data-batching`, original base main `b090d94`.
Pro PR #5 has now landed as main `6dd6574`; combined source is `88f09c9`.
Air scope: bounded 16 KiB SCSI commands segmented into existing 4 KiB bulk calls,
block-cache batch admission, contiguous exFAT file-data grouping, isolated image
capability and host/VM fault fixtures. No xHCI DMA geometry, retry, flush, model
or scheduler-admission changes. Follow USB BOT 1.0 data-phase ordering and short
transfer rules; preserve quarantine on ambiguity. Validate local boundaries,
then compare a fresh automatic native profile. Ordinary desktop battery remains
disabled and Actions remains disabled. The native batching result is recorded above.

Integration: main `6dd6574` was merged without replacing either device's scope.
Pro GPT map validation and GPT-first USB discovery merge with the batching
implementation. The USB fixture's two conflicts were resolved by keeping GPT
scan/reset assertions, segmented transport counters and cases, and the larger
mock disk's `sizeof(disk)/512` capacity. The staging profiles merge cleanly in
this landed baseline; the earlier dry-merge conflicts are superseded. Combined
ordinary/native/Q35 builds, full baseline smoke (481 CLI commands, system calls,
editing, UEFI USB input and display), USB persistence/keyboard/timeout tests and
all five battery VM cases passed at `88f09c9`. Pro acceptance remains limited to
its own machine; the later Air result above accepts only the named isolated checks.

Local batching comparison (implementation `ca6f274`, added fixtures `d0e98d1`):
265 -> 169 image writes, all 72 flushes retained, and identical final disk bytes.
All/core builds/tests and exFAT hardening passed, including fragmented allocation.
The isolated image passed 169 write failures, 169 power cuts, 72 flush failures,
169 sector-aligned partial failures, 169 partial cuts and 19 pause/revoke points.
USB read-only/write/flush/reload/keyboard and timeout/quarantine VM checks, plus all five battery VM cases
passed. The profiled Q35 workload completed 78 cycles / 31,400 commands, compared
with 78 / 38,888 before batching. Native timing was pending at this local-validation
checkpoint; neither VM result accepts physical input latency or ordinary desktop
battery startup.

Latest accepted **contiguous FAT batching**: release **`2ca59bc8678efc17`** /
source **`39b152d`**, session **`session-00065e4f02718396-00`**. The automatic
trial completed 34 cycles / 612 operations in 61,130,508 microseconds with 16,954
USB commands and sampling progress in 17 scopes. Maximum storage scope was
2,721,271 microseconds; maximum operation 2,721,267 microseconds at step 1
(initial large draft write). Compared with the previous 25-cycle run below,
commands per cycle fell about 34% and maximum scope about 15%. These are
single-run measurements on the same USB drive, not a desktop latency guarantee.

The service reached generation 23 with 51,938 cached reads (1,423 while sampling),
maximum 18 microseconds. Both lifetimes retired with balanced attachment,
restored event enables and zero heap/errors/denials. SCI still recorded 24
handled / 24 unclaimed active deliveries; setup/retirement deliveries and
in-flight count were zero. The unclaimed cause remains open.

Exact final payload/tree, inner fsck (4 directories / 5 files) and 75-cluster
allocation checks passed. Outer fsck is clean (184 directories / 881 files),
with 1,652 clusters and zero leaks/mismatches/errors. All 876 prior files except
the disposable image and root inode sidecar are byte-identical. New Ubuntu boot,
consumed one-shot and installed/recovery hashes were verified. The payload and
29 hashed evidence files are archived outside `build`; the USB is remounted.
Accepted desktop recovery remains `b427b78f21644bd2`.

PRs #9/#10 are merged. Actions is disabled at the user's request; validation
runs locally. No reboot is armed and ordinary desktop battery startup remains
disabled. Next: measure the remaining large-write data/flush costs and reduce
blocking time while retaining failure guarantees, then establish desktop/input
coexistence. This result accepts the isolated workload and runtime retirement;
it does not accept ordinary desktop battery startup or gauge accuracy.

Previous accepted **operation-stepped isolated filesystem overlap**: release
**`9eb84deab70d240c`** / source **`89edb98`**, session
**`session-00065e4e5218f777-00`**. The trial completed 25 cycles / 450 operations
in 60,768,457 microseconds, issuing 18,754 commands, with sample progress in 21
scopes. The maximum storage scope was 3,210,080 microseconds; maximum operation
was 3,210,076 microseconds at step 1 (the initial large draft write). The prior
whole-cycle scope maximum was 7,303,034 microseconds. These single-run timings
show shorter exclusion, not accepted desktop/input latency; the remaining
three-second operation needs further investigation before desktop integration.

The service captured generation 23 and 41,561 cached reads (1,226 while sampling),
maximum 18 microseconds. Both initialized lifetimes retired cleanly with balanced
attachments, restored enables, zero heap/errors/denials, and clean image revocation.
SCI counted 24 handled / 24 unclaimed active deliveries, none during setup or
retirement; the cause remains open. Final image payload/tree and inner fsck plus
75-cluster allocation checks passed. Outer fsck is clean (183 directories,
878 files); 1,648 clusters are accounted for with zero leaks/mismatches/errors.
All 873 previous files other than the disposable image and inode sidecar are
byte-identical. New Ubuntu boot, consumed one-shot and recovery hashes verified;
the payload and 30 hashed evidence files are archived outside `build`.

At that checkpoint PR #9 was awaiting validation. It later merged using local
results after the user disabled Actions; the newer batching result is above.

Previous bounded **whole-cycle isolated filesystem overlap**: release
**`c473b63c44d32cd0`** / source **`8822cba`**, session
**`session-00065e4ddc1d41d5-00`**. The v38 trial completed 25 create/grow/shrink,
rename/delete/flush cycles over 61,510,180 microseconds, issuing 18,754 USB
commands. Sampling advanced inside 21 of 25 filesystem scopes; generation 22
was captured. Cached reads completed 14,425 times (130 while sampling), with a
17-microsecond maximum. Cancellation, restart and in-flight stop/timeout followed
by clean retirement passed. Both initialized lifetimes restored retained enables,
balanced attachments and ended with zero heap, errors and denied accesses.

The exclusive image lease was revoked before workspace remount. Collection
verified the final payload, empty file, unchanged anchor and image identity.
Inner fsck/allocation checks passed (75 clusters); outer fsck reports clean
(182 directories, 875 files), with 1,644 accounted clusters and zero leaks or FAT
mismatches. All 870 pre-existing files other than the SrvrOS inode metadata sidecar
are byte-identical; that sidecar was rewritten by normal session logging. The
new Ubuntu boot, consumed one-shot and installed/recovery hashes were verified.
The accepted payload and 33 hashed evidence files are archived outside `build`.

This accepts the bounded isolated workload, not ordinary desktop filesystem
concurrency or input responsiveness. Maximum filesystem scope duration was
7,303,034 microseconds; bound and measure that work before desktop integration.
SCI recorded 24 handled and 24 unclaimed active deliveries across both lifetimes,
with none during setup/retirement or remaining in flight; the cause remains open.
Gauge accuracy and long-lived service are untested. Ordinary desktop battery
startup remains disabled. No reboot is armed; the used fixture must be archived
before replacement and cannot be reused as a fresh trial.

Latest accepted bounded **scratch write/flush overlap**: release
**`5792b6ece45b85dc`** / artifact **`e2d779b`**, retry checkpoint **`d4a21cb`**,
session **`session-00065e4c30fcddac-00`**. It completed 1,894 write/flush/readback
cycles (5,682 transport commands), including 62 cycles during sampling and three
sample-generation advances inside 55 I/O scopes (maximum 349,839 microseconds).
Five fresh samples and 5,414 cached reads completed, 574 during sampling, maximum
read time 18 microseconds. Cancellation, restart, in-flight stop and a short stop
timeout followed by clean retirement passed. Both initialized lifetimes restored
retained enables, balanced attachments and ended with zero heap/errors/denials.
SCI recorded six handled and six unclaimed active deliveries, none during setup
or retirement and none in flight afterward; the unclaimed cause remains open.

The exclusive scratch capability was revoked before remount. Collection verified
the final 4096-byte payload against the cycle count and preserved its bytes/hash.
Completed exit, new Ubuntu boot, consumed one-shot and installed/recovery hashes
were verified. Offline fsck reports clean (181 directories, 871 files); SrvrOS's
allocation checker accounts for 1,512 clusters with zero leaks or FAT mismatches.
The accepted payload and 47 hashed evidence files are backed up outside `build`.
The earlier failed attempt of this same release remains separate evidence.
This accepts bounded transport writes/flushes during sampling, not ordinary
filesystem metadata mutation, human input, sustained desktop operation or gauge
accuracy. No new reboot is armed; ordinary desktop battery access stays disabled.

Latest accepted bounded **read** run: **`b73c1ae3e4643dc9`** / artifact **`0cb9f2e`**,
session **`session-00065e4a0a43b75e-00`**. Battery sampling progressed during
103,094 uncached USB reads; all 4096-byte reads matched their baseline. Five
sample-generation advances occurred inside storage I/O scopes, with 153 reads
observed during sampling. The cached service completed five fresh samples and
106,806 reads (921 during sampling), maximum observed read time 18 microseconds.
Both initialized lifetimes retired cleanly and restored retained event enables.
SCI counted six handled and six unclaimed active deliveries, none during setup
or retirement and none in flight afterward; the unclaimed cause remains open.
Native read overlap is accepted within this bounded test. Writes, human input,
continuous desktop operation and gauge accuracy are not accepted by this run.

Current unattended-test policy (user correction October 20): announce required
human actions before boot; tests requiring no input must return to Ubuntu on
handled failures as well as success. The `-auto-return` capture checkpoints and
unmounts USB before battery runtime entry. Normal returns remount, save the result
and reset even on failure. Contained faults disable interrupts and reset without
filesystem reentry or unsafe interpreter cleanup. Missing completion remains
failure, never a pass merely because Ubuntu returned.

**Bounded cached battery service accepted:** release **`d9e6f4f75da4e871`**,
artifact **`a24836a`**, session **`session-00065e4922d1be3d-00`** completed the
v33 automatic test. It produced five fresh samples, 49,344 cached reads (10,368
during sampling), and an observed maximum read time of 18 microseconds.
Immediate cancellation, two initialized lifetimes, restart and in-flight stop
with a short wait timeout followed by successful retirement passed. Both retained
event enables were restored in each initialized lifetime; together they handled
90 queries/92 notifications with balanced attach/detach and zero reported runtime
errors, denied accesses or remaining heap. Setup took 3.997 and 3.978 seconds.

The matching exit log contains clean retirement and `completed=yes`. Ubuntu has
a new boot ID, the one-shot entry is consumed, installed/previous payload hashes
match, and read-only exFAT checking reports clean (176 directories, 852 files).
The accepted payload and 33 hashed evidence files are preserved outside `build`.
This establishes service completion and host recovery; failure-triggered reset
has VM evidence, not a physical fault-injection result.

Acceptance is deliberately bounded: USB logging was parked during runtime.
Long-lived desktop use, concurrent storage/input responsiveness and gauge
calibration remain pending. An unhandled SCI line 9/vector 41 warning appeared
after the first setup; that initial run did not establish its count or cause.
The later counter capture above quantifies deliveries. Do not hide that warning
or infer a fault-free interrupt path from the runtime's zero error counters.
The remaining block is scheduling/USB coexistence validation before
enabling an ordinary desktop provider. Battery values remain relative SOC:
the capture's 98% is not acceptance of agreement with Ubuntu's 86% display.

Builds, workspace pause/remount logging, bootlog/cache/autotest host checks and
44 staging/collector checks passed for this candidate. VM setup-deadline injection
verified actual reset; explicit legacy halt still passed. Exact-stage UEFI/GRUB
checks verified missing-workspace reset and native Q35 refusal with USB
park/remount, saved failed report, clean exFAT and reset. Earlier failed v33 runs
remain preserved; this success does not identify their exact causes or prove
the reader-wait change alone resolved them.

## Identity and ownership

Development host: Ubuntu on the 13-inch Air, Broadwell-U, Intel HD Graphics 6000
`8086:1626`, xHCI `8086:9cb1`, Apple topcase `05ac:0291`, HDA `8086:9ca0` /
CS4208 `1013:4208` subsystem `106b:7e00`. Firmware, descriptors and routing evidence
are in the linked device guides. The shared integration baseline is now `main`,
including the completed `codex/linux-development-baseline` work. Subsequent
device work uses separate branches based on `main`.

## Active work and shared dependencies

### Large-write profiling — October 20

Branch `device/macbookair7-2/write-profiling` starts from merged main `9d9c72e`.
Pro scope inspected at `992e8b9` concerns network UI repaint/integration, with battery
work deferred. Air scope: read-only exFAT phase labels, per-operation bounded
image callback timing, diagnostic battery reporting, collector and host/VM
fixtures. Keep mutation order, flush barriers, I/O admission and model gates
unchanged while measuring allocation/data/free/metadata read/write/flush costs.
Record the slowest operation without printing inside its measured interval.
I/O durations include any admitted service execution; scope wall time is not
an input-latency measurement. After local validation, use a fresh automatic
native diagnostic to identify the next targeted change. Ordinary desktop
battery startup remains disabled. Actions remains disabled; validate locally.

Implementation `e1dce19` passed ordinary/native/Q35 builds; deterministic callback
accounting; 265 write-failure, 265 power-cut, 72 flush-failure and 19 pause/revoke
boundaries; generic exFAT hardening; collector and documentation checks. Five
Q35 cases passed: isolated workload, setup/active deadlines, generic filesystem
and write failure. The profiled VM completed 78 cycles with clean payload/tree
and allocation checks. These are VM results; native profiling is pending in
PR #12. The remaining long write and unclaimed SCI cause are still open.


### Contiguous FAT write batching — October 20

Branch `device/macbookair7-2/fat-write-batching` consumes PR #9 source
`89edb98` and native acceptance notes. PR #9 merged as `5bf63b7` after
review of its local host/VM and native evidence; this branch consumes that main. Pro scope inspected at `14e2a99` remains Ethernet readiness
and prompted cable testing; no overlapping filesystem change was found.

Bounded objective: reduce repeated writes of the same FAT sector during
contiguous allocation/free. Scope: `kernel/src/fs/exfat.c`, entry/isolated-image
host regressions and this record. Preserve the existing on-disk FAT values,
copy-on-write publication and flush/dirty ordering; fragmented chains retain
their existing traversal. First record per-operation FAT/data write counts,
then batch adjacent equal FAT entries into bounded writes. Validate neighboring
FAT entries, boundary failures, directory growth and fragmented allocation,
then run the full image fault matrix and battery filesystem VM workload.
The native comparison is recorded at the top of this file; no reboot is armed
and ordinary desktop battery startup remains disabled.

Host tracing measured 142 writes (67 FAT) for initial 256 KiB+37-byte creation
before batching, versus 78 (3 FAT) afterward. One full cycle falls from 457
to 265 writes; all 72 flushes remain. These are fixture transfer counts, not
hardware latency measurements. The change retains each FAT entry's prior value
and the existing write-blocked recovery behavior; it batches contiguous ranges
in at most 512-byte slices while leaving fragmented traversal intact.

Local range/sentinel/failure tests, all 19 pause/revocation boundaries, all 265
write failures + 265 power cuts + 72 flush faults, exFAT hardening (4K/128K,
copy-on-write recovery, directory growth, capacity and fragmented allocation),
scratch isolation and ordinary/native/modeled builds passed. Local `make smoke`
passed the full core suite, 481-command CLI workload (424 files / 5,159 clusters,
zero leaks/FAT mismatches/errors), syscall, line-editing, UEFI USB/input and
display-service checks. All six affected battery VM cases passed: sustained
isolated filesystem, isolated setup/active deadline automatic reset, filesystem
normal/write-failure, and scratch writes. The sustained run completed 75 cycles /
1,350 steps in 60,494,657 microseconds, 37,391 commands, 21 scopes with sampling
progress, and a maximum step of 499,405 microseconds (regrow, step 5). Payload,
outside bytes, allocation and clean retirement passed. Staging (35) and collector
(12) tests also passed. Functional code is `6ca3f4f`, tested after integration
with main as `880efc3`; later documentation commits do not change that code.

Repository Actions is disabled at the user's request; no hosted result is used.
PR #10 merged as `39b152d`. Its native comparison is accepted for the bounded
isolated workload above. Every subsequent trial still needs a fresh disposable
fixture, exact-stage rehearsal and independently verified recovery.


### Filesystem operation scheduling — October 20

Branch `device/macbookair7-2/filesystem-latency`, based on PR #8 source `8822cba`
and its acceptance notes. PR #8 passed CI and merged as `09207b0`; this branch
consumes that integration baseline. Pro scope inspected at `a19ce5c` remains
Ethernet bring-up; preserve its branch and profiles.

Bounded objective: split the diagnostic filesystem cycle into explicit operations,
release storage exclusion between them, measure each operation and require a
complete cycle before accepting its payload. Files: `workspace_image.c`/header,
battery runtime workload, host fault/continuation tests, VM harness and collector.
Keep each filesystem mutation atomic under existing exclusion; do not change
shared exFAT transactions, scheduler admission or ordinary desktop startup.
Host tests must reject skipped/repeated cycle identities and cover failures and
revocation between operations; VM checks must prove complete payloads, sampling
progress, step accounting and automatic fault return. Native latency acceptance
remains pending; a bounded operation count does not guarantee device completion
within a fixed time. No new physical test is armed.
Host validation passed all 19 clean pause/revocation boundaries plus 457 write
failures, 457 power cuts and 72 flush failures, preserving outer bytes and clean
post-recovery inner allocation. Collector tests cover complete step accounting,
invalid counts/timing/phase and legacy reports. Ordinary/native/modeled kernels
build with the local `tools/host-env.sh` wrapper. The sustained VM completed
73 cycles / 1,314 operations over 60,354,688 microseconds, with 54,874 transport
commands and 23 scopes containing sampling progress. The maximum operation was
532,726 microseconds (step 1, the initial large draft write); maximum scope was
532,748 microseconds. Payload, outer isolation and clean retirement passed.
Setup and active-sample deadline fixtures also passed automatic reset and
outer preservation/inner recovery; the scratch-write regression passed.
The physical comparison above accepts the operation sequence and data integrity;
it does not accept interactive latency.


### Isolated filesystem coexistence — October 20

PR #7 passed CI and merged as `76508bf`; this device branch consumes that baseline.
Pro PR #5 inspected at `304bc55`. Air scope: exclusive exFAT file-extent admission,
a diagnostic bounded block view over a prepared disposable filesystem image,
workspace lease exclusion, battery workload and VM/host isolation verification.
Avoid Pro's partition-parser and CPU-load changes. The outer workspace must be
unmounted with public writes revoked throughout the lease; all test allocations
remain inside the image's proven data extent. Validate interrupted inner writes,
byte-for-byte preservation outside that extent, lease revocation and refusal paths
before adding a native capture. Ordinary desktop startup remains unchanged.
Air reserves **v38 laptop-probe** for `--battery-filesystem`; preserve Pro's
v37 desktop network profile during integration. The isolated VM completed 332
cycles / 249,772 transport commands over 60,183,438 microseconds, with 25 progress
bursts inside I/O scopes and generation 24 at completion. Service retirement,
exclusive range revocation, offline image contents/allocation checks and exact
preservation of every byte outside the image passed. Filesystem traffic now waits for ACTIVE runtime ownership. Both the setup and
active-sample deadline fixtures reset automatically; outer bytes remained unchanged
and a separate copy of the interrupted inner image recovered successfully.
All 457 write-failure, 457 power-cut and 72 flush-failure host boundaries passed
outer isolation and post-recovery inner/outer allocation checks. The exact staged
Q35 native-refusal/automatic-return check and the bounded native trial above
passed. PR #8 publishes this work; Pro PR #5 was subsequently inspected at
`f74595d` (Ethernet recovery work). Keep that branch and its capture profiles intact.

### Sustained filesystem coexistence — October 20 (VM-first)

Base `d72d2af`, host/CI follow-up PR #7 at `dd4245d`; Pro PR #5 inspected at
`fec1a41` with the merged battery baseline. Scope: the disposable Q35 battery
model and smoke harness, exercising a minute of actual filesystem allocation,
grow/shrink, rename, deletion and flushes during sampling, plus a write-failure
case. Keep native profiles and ordinary desktop startup unchanged. Validate
sample progress inside storage scopes, clean retirement, offline file contents
and allocation accounting. Failure must retain the previous committed file and
block further writes; recovery is rehearsed only on the disposable image.
The failure fixture also exposed an overly strict shared exFAT checker: it
mistook nonzero FAT entries of bitmap-free clusters for allocation mismatches.
Scope now includes correcting that check against the exFAT allocation-bitmap
contract, with stale-free-FAT admission and actual orphan/missing-allocation
refusal regressions. This does not change allocation or repair on-disk bytes.
A real workspace trial needs a separate crash/recovery contract before enabling
concurrent filesystem metadata mutation on hardware.

Final disposable VM validation completed 134 filesystem cycles in 60,539,284
microseconds, with sample generation advancing from 1 to 25 inside the I/O
scope. The service completed 8,320 cached reads (1,472 during sampling), maximum
603 microseconds, and retired cleanly. Offline payload/tree checks, fsck and the
kernel allocation checker passed (five files, 12 clusters, zero errors/leaks).
The injected write failure preserved the old payload before and after recovery,
refused later writes, advanced sampling and retired with zero errors/denials/heap.
Recovery was performed only on a disposable VM image; the failed image is retained.
These results do not extend native acceptance beyond the v36 scratch range.


### Allocation-check discrepancy — October 20 (resolved)

Base `d72d2af` (merged PR #6), Pro PR #5 inspected at `7c9372b`.
Independent read-only allocation traversal and the actual SrvrOS checker both
found orphan clusters 114, 159, 162, 210–213 and 606. The kernel guard was correct;
`fsck.exfat -n` (1.3.2) reported clean without resolving those orphan allocations.
Their originating write or interruption is unknown. No kernel or native battery
policy change was needed.

Preserved boot/FAT and all 1,506 allocated clusters in a sparse snapshot outside
`build`. Recovery was rehearsed on a copy, verified against the unchanged device,
then applied with exfatprogs orphan rescue. All 862 existing files remained
byte-for-byte identical. Five `LOST+FOUND` files preserve all eight orphan
clusters, also copied into the durable evidence archive. The recovered volume
passes the real SrvrOS allocation checker: 867 files, 1,507 accounted clusters,
zero leaks/FAT mismatches/errors. Offline fsck reports 180 directories, 867 files.
The workspace was remounted and the unchanged automatic v36 payload
`5792b6ece45b85dc` subsequently passed the bounded trial summarized above.

Evidence is under `~/.local/share/srvros/failed-evidence/exfat-orphans-20261020`.
The snapshot contains boot/FAT and allocated data; unallocated bytes were not
captured. New host tooling exposes read-only allocation checking on an unmounted
USB partition. Disposable 4 KiB/128 KiB fixtures exercise orphan refusal,
byte-preserving rescue and renewed scratch eligibility; the kernel guard remains
unchanged. Scratch/entry tests, the full filesystem hardening suite (including
write/flush/power-cut boundaries), wrapper selection and documentation checks
pass. The later successful retry accepts bounded scratch write/flush overlap.
See the [offline workflow](../ubuntu-device-agent-setup.md#7-test-return-and-collect).

### Native scratch write/flush overlap — October 20 (preflight failed)

Bounded objective: extend coexistence testing to USB writes and cache flushes
without filesystem metadata updates during native battery ownership. Prepare a
new 4096-byte scratch file, verify its exclusive allocation, flush/unmount the
workspace, and retain a private write capability for only that file's data range.
Normal retirement revokes that capability before remount and final collection;
contained firmware faults may leave scratch contents incomplete but must not
reenter the filesystem. No general USB write aliases are enabled during runtime.

Touches block range translation/cache invalidation, exFAT allocation inspection,
workspace scratch lifetime, battery diagnostics and host/VM tests. First gate is
a tested scratch capability including refusal, partial-write and flush failures;
then wire the automatic capture with strict offline verification. Base `b6075ef`,
Pro PR #5 `7c9372b` inspected (battery work deferred on that unit). Its CPU-load
capture overlaps shared boot staging/startup files: keep its implementation separate and reconcile those hunks
at integration; do not copy or replace the Pro proposal. Ordinary desktop battery
access remains disabled. The accepted b73 read trial is collected; its evidence and recovery payload
remain preserved.


The v36 scratch capture is implemented. Block/workspace host checks and disposable
exFAT extent tests pass for 4 KiB and 128 KiB clusters, including sector-boundary
tears, bad allocation/alias refusal, partial writes, flush/read failures and
poisoned-lease remount refusal. All 46 staging/collector tests pass. Ordinary,
native and model builds pass. Positive write-overlap and injected automatic
reset VMs verify clean exFAT and unchanged disk bytes outside scratch data;
existing service, read overlap and disposable filesystem-write regressions pass.
Collection independently verifies and copies the final scratch payload.
Candidate `5792b6ece45b85dc`, artifact `e2d779b`, passed exact-stage native
Q35 refusal with saved failed result, scratch revocation, clean exFAT, unmount
and reset. Missing-workspace automatic return also passed.
The candidate is installed with previous desktop `b427b78f21644bd2` retained;
accepted read payload `b73c1ae3e4643dc9` remains on EFI. The unreferenced `a528`
EFI duplicate was removed only after its independent backup/hashes were verified.
The physical run returned automatically but **failed preflight**, session
`session-00065e4a6f889c64-00`: SrvrOS's allocation check reported eight leaked
clusters, first cluster 114. It refused the scratch capability before native
battery runtime entry; no service/write-overlap acceptance is claimed. The
matching exit saved `completed=NO`. Ubuntu's read-only offline exFAT check
reports clean (179 directories, 862 files). The new boot, consumed one-shot and
installed/recovery hashes were verified. No new reboot is armed.

The failed report and 45 hashed evidence files are retained outside `build` at
`~/.local/share/srvros/failed-evidence/5792b6ece45b85dc`. The accepted b73 read
baseline is unchanged. The later recovery above resolves this preflight refusal without bypassing the
guard. It does not accept native writes or identify the historical orphan source.
Both full CI baseline runs passed at `800d84e`, which contains the exact scratch
implementation. See the [v36 contract](../battery-service.md#native-scratch-writeflush-overlap-capture-v36)
and [integration handoff](../agent-coordination.md#october-20-battery-baseline-handoff).

### Native read-only USB overlap capture — October 20

Scope: add a distinct v34 automatic battery-service capture that checkpoints and
unmounts the validated workspace, then repeatedly reads the same USB device with
writes revoked while sampling. Compare read bytes with a pre-runtime baseline;
require sample generation progress inside actual scheduler I/O scopes. Save after
clean retirement/remount; contained faults reset without filesystem reentry.
Touches the diagnostic, workspace parked-device identity, staging/collector and
VM fixtures. Base b6075ef and Pro PR #5 at 8fc3e1c remain unchanged. No desktop
enablement or new firmware permissions. Native write/flush overlap and human input
acceptance remain separate gates. No human action is required by this trial.

Implementation and host/VM checks pass: 45 staging/collector tests, workspace
identity/write-revocation host coverage, ordinary/native/model builds, new
read-overlap and injected-reset cases, and existing service/reset regressions.
The modeled overlap issued 83,547 real USB read commands, with 57 reads while
sampling and five generation-progress bursts; all bytes matched. Whole-disk
hashes were unchanged and exFAT clean after both overlap and injected reset.
These are VM results. The exact-stage candidate `b73c1ae3e4643dc9`, artifact
`0cb9f2e`, also passed Q35 native refusal with saved failed result, clean exFAT,
unmount/reset and missing-workspace return.

The physical trial subsequently passed with no human input. Its 103,094 read
commands included 153 reads during sampling and five generation-progress bursts
inside 58 storage scopes, maximum observed duration 289,851 microseconds.
Immediate cancellation, restart, in-flight stop and short stop timeout followed
by clean join passed. The two initialized lifetimes handled 90 queries and 92
notifications, balanced attachments, and ended with zero heap/errors/denials.
Setup took 3.984 and 3.982 seconds. SCI accounting is summarized above.

The matching exit contains complete service/read proof and `completed=yes`.
A new Ubuntu boot, consumed one-shot, installed hashes and clean offline exFAT
(178 directories, 858 files) were verified. The persisted exit precedes final
unmount/reset; recovery is corroborated by those host checks. At that checkpoint current was `b73c1ae3e4643dc9`,
previous desktop is `b427b78f21644bd2`, and prior accepted `a5281d842c8b6160`
remains available. Accepted payload/evidence backups are outside `build`, under
`~/.local/share/srvros/known-good/b73c1ae3e4643dc9` and
`~/.local/share/srvros/accepted-evidence/b73c1ae3e4643dc9`.
The old unreferenced `ed93e03b2585dcaf` EFI duplicate was removed only after its
durable backup was verified. No new reboot was armed at that checkpoint.

At collection, integration remains `b6075ef`; Pro PR #5 was inspected at
`cdbefc2`. Its isolated load diagnostic changes do not overlap this native battery
scope. Next is bounded native write/flush coexistence with durable failure
recovery. Human input, sustained SCI behavior and ordinary desktop enablement
remain separate gates.


### Battery scheduling repair — October 20

Scope: audit and grant explicit I/O-service eligibility only to the separate
cached battery service's owner/worker while its validated runtime is engaged.
The generic ACPI worker remains ordinary by default; other adapters must not
inherit eligibility. Touches battery runtime, an opt-in worker admission API,
worker host tests, a scheduler readiness-scan lifetime correction, and VM overlap fixtures. Base `b6075ef` and Pro PR #5
`8fc3e1c` were fetched unchanged. No desktop startup, native resource gate,
transport deadline or scheduler-wide policy change is part of this block.
Preflight mappings precede engagement; service callbacks use kernel mappings,
serialized heap/PCI/console/queue operations and runtime-owned wait dependencies.
The repair permits EC progress before the I/O owner releases its scope. In the
final VM build the 250 ms hold measured 268.379 ms with 83 EC commands; the
five-second scope measured 5.017 seconds with 293 commands and sample generation
1→3. The disposable USB case completed 94 write/flush cycles over 5.524 seconds,
417 EC commands and generation 1→3. It retired cleanly, unmounted, verified the
4096-byte final payload offline and passed read-only exFAT checking.

The audit also closed worker reclamation gaps: stopped publication retains
masked IRQs until the scheduler marks the thread dead, and readiness scans skip
retired threads before invoking eligibility callbacks. Host regressions cover
both paths. See the [dependency audit](../battery-service.md#audited-storage-eligibility).

Ordinary/native/VM kernels and scheduler/worker/cache/SCI host tests pass.
Twelve targeted battery scenarios passed before the final scheduler scan
correction; long/sustained/actual-USB overlap, query deadline and automatic
fault reset were rerun on the final build, along with the ordinary ACPI worker
lifecycle VM. Staging/collection has 44 passing host checks. Evidence is in
local ignored `build/battery-io-fix-20261020`; the full CI matrix was not rerun
locally. CI now includes the USB fixture and its exfatprogs dependency.

The subsequent v34 trial above accepts bounded native USB-read coexistence.
Native writes/input and sustained SCI behavior remain pending. Ordinary desktop
battery access stays off.

### Earlier SCI accounting and storage scheduling block — October 20

This checkpoint preceded the scheduling repair above.

The Air agent owns the next bounded runtime change: phase-scoped SCI delivery,
claimed/unclaimed counts and timestamps, plus disposable VM tests of service
progress under the existing storage scheduling exclusion. Touches the separate
battery runtime/model, its VM harness and validation documentation; no changes
to native resource gates, IRQ acknowledgement, scheduler exemptions or ordinary
desktop startup are planned. Base `b6075ef` and Pro PR #5 `8fc3e1c` were fetched
again before work; no overlapping native EC scope was found. Preserve accepted
`d9e6f4f75da4e871` while establishing the interrupt warning's measurable scope
and whether storage exclusion can exhaust the existing operation/query budgets.
A modeled scheduling hold is not physical USB or desktop responsiveness evidence.

Implementation: ready/retired/fault SCI records now separate setup, active and
retirement callbacks, preserving claimed/unclaimed results and timestamps without
new event-register access. The VM unclaimed-dispatch fixture counts five active
deliveries in the first sampled lifetime and one after restart, while the generic
warning prints once. A 20.311 ms scheduler I/O exclusion permits full service
completion; a 250.234 ms exclusion at a transport read reproduces a sample timeout
(EC error 1) with live state retained. Cached readers work during both holds.
The latter is containment evidence, not a coexistence pass. Ordinary desktop
enablement remains blocked on an explicit worker/owner scheduling audit; do not
extend deadlines or add a blanket ACPI storage exemption.

Native/VM/ordinary builds, SCI-owner/IRQ-registry/cache host tests, 44
staging/collector checks and all seven targeted VM cases pass: the three new
fixtures, normal service, stale recovery, contained automatic reset and native
refusal. The physical capture repeated the accepted parked-USB service test
with counters; it needed no human input and used automatic failure return.
It measured the native warning, not the known-failing storage interleave.

Release **`a5281d842c8b6160`**, artifact **`6938813`**, completed that unattended
capture. Exact-stage UEFI/GRUB rehearsal passed native Q35 refusal,
saved failed-report byte verification, clean exFAT, unmount and actual reset.
The prior accepted service payload and previous desktop are preserved; the
unreferenced failed `a3df674e8d6e5c65` payload was archived and hash-verified before
EFI space was reclaimed.

The matching native session `session-00065e497cc34d56-00` has a completed exit and
strict service proof: 48,768 reads/9,728 during sampling, maximum 18 microseconds,
five fresh samples and clean retirement of both initialized lifetimes. Epoch 2
recorded ten active SCI deliveries (five handled/five unclaimed); epoch 3 recorded
two (one/one). All setup/retirement and final in-flight counters were zero.
Setup took 3.999/3.978 seconds. Both event restores, balanced attach/detach and
zero runtime errors/denials/heap were preserved. The new Ubuntu boot, consumed
one-shot and clean exFAT (177 directories/855 files) were verified. Payload and
36 hashed evidence files are backed up outside `build`. No new boot is armed.
Unclaimed interrupts remain unexplained; no continuous desktop or concurrent
storage acceptance follows from this parked-USB run.

### Cached battery service block — October 20

Following acceptance of the latest v32 transition round, this Air branch owns
the cached battery/AC provider. Scope: a pure snapshot/freshness model, an
asynchronous lifecycle owner around the existing serialized EC/AML worker,
explicit start/stop and bounded waits, and host/VM lifecycle/reader regressions.
Preserve the accepted diagnostics and exact native resource gates. Ordinary
desktop startup remains disabled until service lifetime and responsiveness have
separate evidence; UI/syscall integration follows this kernel contract.
Base remains `b6075ef`; Pro PR #5 is `8fc3e1c` (accepted paired-fan telemetry docs),
confirmed by fetch after this physical return.
No overlapping native EC implementation or new sibling acceptance is inferred.
The next checkpoint is a tested cached service with safe retirement, stale
readings during missed/invalid updates, and repeatable start/stop lifetimes.
Reserve **v33 laptop-probe** for a bounded automatic service-lifecycle capture
after those checks pass, including exact-stage refusal rehearsal and strict
collection. It exercises cached readers and stop/restart, not desktop acceptance.

Historical implementation and trial sequence (the acceptance above supersedes
the pending statuses below):

The pure cache and asynchronous lifecycle are in
place; [service contract](../battery-service.md) documents freshness, retirement
and ownership. Host cache checks also pass ASan/UBSan; native, VM and ordinary
kernel builds pass. All 21 battery VM scenarios pass, including cached readers
during sampling, invalid/incoherent update recovery, repeated lifetimes,
in-flight stop, native refusal, independent deadline containment and the earlier
transition timeout/reentry regression. The 31 staging and 11 collector tests,
375 local documentation links and whitespace checks pass. The collector also
accepts the modeled service transcript after explicitly translating fixture
port fields; that replay is parser validation only. Native v33 lifecycle
acceptance, long-lived desktop service and responsiveness under I/O remain
pending. No battery widget or userspace interface is enabled in this block.

Native candidate **`a9be2dc78d213dee`**, artifact source **`1a12f2d`**, is the
v33 automatic service trial. Its exact staged native image passes GRUB/UEFI
rehearsal: unsupported Q35 is refused, the failure log is persisted and verified
offline, exFAT is clean, and failure remains halted. The hardware test keeps AC
and USB connected and needs no input; it saves/unmounts and returns only after
complete retirement. Each initialized lifetime must restore both retained event
enables, and the collector requires the reader/sample/retirement proofs.
Accepted v32 `aa976febab59c128`, v31 `13d8083de35b51af` and previous desktop
`b427b78f21644bd2` remain backed up outside build. The unreferenced v25 payload
`06c646455f08555e` was archived and hash-verified before reclaiming EFI space.
Collect the matching v33 session before another candidate; physical result is
pending, and this does not accept persistent desktop operation.

The v33 physical trial failed during namespace/EC initialization with the
five-second operation deadline: errors/denials zero, heap 674132 retained,
EC phase/error and queries zero. Both new sessions
`session-00065e3eecadaeef-00` and `session-00065e48a9f0e993-00` lack exit records;
the latter matches the supplied halt photograph. They are failures/incomplete
evidence, not service acceptance. Current repair scope is the diagnostic reader's
scheduling: its `HLT` waits keep it runnable during worker setup. Add a slower
VM initialization fixture, compare before/after blocking reader waits, and add
setup progress/timing diagnostics while preserving native deadlines and access
gates. Pro PR #5 advanced to `8fc3e1c` (accepted paired fan telemetry docs), with
no overlapping runtime work; integration base remains `b6075ef`.

Repair validation: a VM-only 3.5-second executing setup workload reproduces the
five-second halt with the old runnable reader. Changing only the reader's waits
to scheduler-backed sleeps completes that workload in 3.578/3.565 seconds
across two initialized lifetimes. Final native/VM/ordinary builds and all 12
affected VM cases pass, including the new load regression, all service modes,
normal diagnostic, setup/query/disconnect containment and guided transitions.
The 42 staging/collector checks and 376 local documentation links pass.
Setup now logs substage timing and containment reports worker progress. Native
deadlines, firmware gates and storage scheduling eligibility are unchanged.
The physical explanation and corrected service still require a hardware retry.
Failed evidence is retained outside build; the local USB is clean after the
halt (174 directories, 846 files), and the one-shot entry was consumed.

Corrected v33 candidate **`a3df674e8d6e5c65`**, artifact source **`c8be037`**,
passes exact-stage GRUB/UEFI rehearsal with expected native refusal, persisted
log verification, clean exFAT and persistent halt. It is prepared for the same
automatic lifecycle trial, with setup timing now visible. Previous desktop
`b427b78f21644bd2` and all accepted battery/interactive/power backups remain
verified. The older unreferenced `fed370ffbe33d554` payload was archived and
hash-verified for EFI space; failed `a9be2dc78d213dee` is preserved separately
as diagnostic material. Collect this new candidate's session before further
work. No physical acceptance is implied by the repair's VM results.

- Owner: local Air7,2 device agent; branch `device/macbookair7-2/reliability-input-power`.
- Objective: read-only desktop power/frequency baseline, then bounded native
  battery/AC runtime groundwork; CPU policy changes and suspend/resume follow
  measurement and separate acceptance.
- Shared scope: power telemetry/collector first; subsequent ACPI/EC runtime,
  worker/IRQ ownership and battery interfaces require explicit coordination.
  No new native EC/SCI enablement is part of the power capture.
- Integration baseline: `main` merge `b6075ef`; PR #4 merged after review and
  both exact-head CI runs passed at `869cbc7`. Merge tree matches the tested head.
- Power checkpoint: v26 `ed93e03b2585dcaf` accepted for bounded read-only
  telemetry, 6.458 W package average / 2,083 MHz CPU0 active over 52.999 seconds.
- Current implementation block: complete the connected EC/IGPU access audit,
  then build an opt-in native battery/AC diagnostic through a separate runtime
  adapter, serialized EC/SBS worker, owned SCI/GPE path, bounded lifecycle and
  durable test reporting. Scope includes runtime/transport, build/staging tests
  and guides. Keep normal desktop startup and accepted recovery unchanged.
  Dependencies: chipset/handoff `a51c4a5`, PM1 notification `2d29126`; integration
  baseline `b6075ef`. Pro11,2 PR #5 has advanced to `203e2d1` with exact Pro
  brightness and shared staging changes. Its profiles occupy v27–v30; this
  battery capture reserves **v31**. Native ACPI code does not overlap. Shared
  staging/test hunks need integration reconciliation without changing existing
  release recipes; neither branch is silently merged into the other.
- Native battery candidate: connected captured initialization now passes the
  offline model, including ECSS, GNVS lid/AC and IGPU CLID effects. The separate
  Air-only runtime performs five bounded SBS/AC samples, explicit connection,
  owned SCI/GPE handling and clean retirement; ordinary desktop retains a
  refusal stub. The same lifecycle passes two Q35 lifetimes and six injected
  faults (denied region, EC stall, storm, setup/query/disconnect deadlines).
  The native image refuses Q35 before hardware access. Host memory policy,
  transport/SBS/event/owner tests and collector/staging tests pass. Physical
  Initial VM checkpoint only; the first physical runtime result below supersedes
  native-startup uncertainty. Battery dynamics and general firmware handoff remain pending.
  See [native runtime gate](../native-ec-runtime.md#explicit-native-battery-diagnostic).
- PM1 notification checkpoint: all 65,536 starting words and each single-bit
  command mutation pass host/sanitizer checks. The Q35 release case performs
  exactly one actual emulated PM1 write, refuses permission replay, preserves
  readable control bits and cleans up through two lifetimes. Injected SLP_EN is
  rejected before any write and halts with state retained. Firmware Pending is
  modeled; this does not accept native firmware reception. Generic PM1 writes
  remain denied. Connected Air EC `_REG` additionally writes the IGPU OpRegion's
  CLID field; its mapping needs a captured layout/per-access audit before binding.
- Chipset/handoff checkpoint: read-only Linux capture confirms missing PCI
  IDs/configuration and RCBA/HPTC; all nine live firmware hashes match. Complete
  offline initialization now passes with an exact read-only DWORD model and
  captured PCI snapshots (zero errors/denials/leaks, no PCI writes on this path).
  Q35 modeled firmware-to-OS lock notification and an owner-side operation
  deadline pass. This accepts neither native SCI nor OS-to-firmware handoff;
  connected EC `_REG`, native access policy and controller ownership remain.
- Earlier Global Lock checkpoint: Q35 full-uACPI tests pass two acquire/release
  lifetimes, firmware-held lock containment and denied PM1 handoff detection.
  Owner drain now precedes interpreter reset; a stuck worker retains heap,
  Owned/Pending bits and the masked SCI callback. Exact fixture lock-word reads
  are allowed; the later PM1 notification fixture above adds a scoped release.
  Native contention/handoff, operation ownership and live mappings are pending.
- Earlier full initialization refusal checkpoint (superseded by chipset replay): offline `--initialize` refuses errors,
  warnings and denied accesses even when uACPI returns success; skips follow-up
  queries on refusal. Twenty-five synthetic tests cover opt-in execution,
  ignored errors, malformed status and prior load faults. Captured Air replay
  with two private GPIO byte models failed on HPET chipset memory and
  missing PCI snapshots (four AML errors / 11 denials / zero remaining
  allocations). GN46 maps to input-sensing disable; native access remains gated.
  Full-runtime FACS contention and PM1 Global Lock release need their own tests.
- Pipe-restart checkpoint (`3e70395`): the sending helper blocks its process-wide
  test signal so its own yields cannot consume it. The probe verifies the handler
  ran on the reader; 256 rounds with varied yields and the full 481-command CLI
  suite pass, with clean exFAT. This corrects the test protocol, not the broader
  POSIX signal implementation. The previously failing old-head CI passed on
  retry; the new repeated regression is retained in CI.
- Memory audit checkpoint: private memory/PCI access traces and budgets pass
  21 namespace tests normally and under ASan/UBSan. Captured initialization's eight writes only change OSYS;
  one PCI read and the constrained GPIO access are recorded. Separate `_SB._INI`
  correctly refuses an additional GPIO read at `0xa74` for `GN46`; its live
  meaning/ownership remain unaccepted. Input captures remain unchanged. See the
  [runtime guide](../native-ec-runtime.md) for exact scope and counts.
- SCI ownership checkpoint (`fbdf014`): the reusable owner now prepares a masked
  route, registers its callback, enables delivery, and retires/restores the
  route with bounded rollback. Host fault tests and ASan/UBSan pass, including
  immediate delivery, partial writes and poisoned-state retention. The separate
  Q35 runtime passes three SCI lifetimes and failed-install rollback after
  actual route preparation; early EC load passes two lifetimes with queries,
  SBS samples and disconnect cleanup. Blocked-worker and interpreter self-reset
  cases halt visibly with live state retained. These are VM/host results only;
  native source ownership and live mappings remain pending.
- Query containment checkpoint: a drain now retains its attempt/time budget
  across worker batches and additional notifications. Host/sanitizer cases pass
  for finite/repeated drains, queued and AML time, clock regression and sticky
  faults. The early-load VM regression passes; continuous and zero-reply storms
  each halt at 128 attempts, and a three-second AML query fails its two-second
  cooperative budget after returning. All retain live state with interrupts off.
  CI now runs the separate SCI/early-EC and query-fault VMs sequentially and saves
  their serial evidence. Native GPE masking/rearm and a worker that cannot return
  still need the external ownership/lifecycle path; no physical result is claimed.
- Event-register checkpoint (`843b66c`): complete supported FADT event layout
  decoding and exact-width/direction IO policy pass host tests and ASan/UBSan,
  including the captured Air FADT. Q35 now restricts fixed enables to its Global
  Lock/power button and enables no GPEs. SCI lifecycle and early EC/SBS/teardown
  regressions pass under the policy. An injected early write denial is refused
  despite upstream API success, then cleans up with zero heap before a normal
  start. Full startup clears all GPE status bytes: native ownership must cover
  that domain, and cleared pending events cannot be rolled back from a snapshot.
  Live native register/NVS/FACS/PCI/GPIO permissions remain unaccepted.
- Dependencies: early worker/handler lifetime fixture merged in PR #3; actual
  captured Air firmware, legacy/extended address consistency, namespace resource
  agreement, `_OSI` policy and SCI/GPE ownership must all be resolved before use.
- Resource checkpoint: [offline EC contract audit](../native-ec-runtime.md)
  matches captured ECDT/FADT to the loaded EC Device/HID/UID/GPE/CRS, with absent
  `_GLK`, zero errors/denials/leaks and private snapshots only. Generic table
  and namespace refusal tests pass. A separate PCI0 `_INI` replay refuses the
  GPIO mapping at `0x800`. The follow-up private byte replay completes PCI0
  `_INI` with one read/write at `0xb90`, setting only bit 3. The same replay
  passes a set-only bit-3 rule and rejects a wrong mask; native GPIO effects
  and ownership still need acceptance. Eighteen namespace tests and the pure
  per-access rule's exhaustive byte transitions pass, including ASan/UBSan.
  This is not controller ownership acceptance. The production build passes and
  its PT_LOAD contents match installed `ed93e03b2585dcaf` exactly.
- First native battery trial: v31 `cc486bf6b6581626`, source `8dc2408`,
  refused platform/resource preflight before runtime initialization. Session
  `session-00065e3d0d7c3f18-00` was copied and hash-verified after return;
  exit logs were saved and the USB unmounted before the persistent halt.
  No battery samples or native EC/SCI acceptance. The screen's `saved=NO`
  combined diagnostic failure with log saving and did not describe the saved files.
- Granular native trial `339211cceb5d894d` (source `e4c8d7e`) refused
  GPE enable byte 12 at `0x189c`, value `0x40` (GPE `0x66`), before EC/runtime
  ownership. Session `session-00065e3d35a068b6-00` verified; logs saved, USB
  unmounted, persistent halt photographed. One-shot consumed on return.
- Second enabled-event refusal: `c0f2c3af0dbacc66` (source `8dc82e7`),
  session `session-00065e3d7491a8fe-00`, refused GPE enable byte 14 at `0x189e`,
  value `0x04` (GPE `0x72`), with PM1 control zero. Logs verified and USB
  unmounted before halt. The GPE66 lease and battery runtime did not start.
- Earlier bounded scope: capture the complete fixed/GPE enable and status
  snapshot before admission; audit the second source (chipset wake-alarm timer)
  and extend the existing exact-source preservation mechanism only with evidence.
  All 12 battery VM cases pass, including two-source preservation and partial
  mask/retirement failures. Host admission/lease checks pass with sanitizers;
  collector (8) and staging (29) tests pass. Runtime, platform, collector and tests are this Air branch's scope; normal desktop unchanged.
- Existing handoff implementation passed host/sanitizer and full-runtime VM
  pending-status preservation, failed mask/restore and unexpected reenable
  containment. Those are model results; neither native handoff nor battery
  readings were accepted at that checkpoint. Candidate `6a2f90f72a0e4b96` / source `ac033a5`
  passes its exact GRUB/UEFI Q35 rehearsal: expected native refusal, verified
  saved log bytes, clean exFAT and persistent halt. Next is the AC-connected
  physical trial; full event snapshot and both retirement records are required.
- Integration remains `b6075ef`; Pro PR #5 is at `fd6d2a2` with input queue residence
  diagnostics and bounded queue work. Shared startup/staging hunks require later
  integration reconciliation, not concurrent changes to the battery mechanism.
- Native trial `6a2f90f72a0e4b96` / `ac033a5` passed preflight and masked
  both retained sources, then halted during namespace load: the captured CPU
  SSDT requests PNVS length `0x53` (83 bytes), but the adapter used decimal 53.
  The photograph records this denied mapping, a later PM1 enable denial
  (`0x120` at `0x1802`), and the contained reset attempt;
  no samples or restoration completed. Matching session
  `session-00065e3db0a6917a-00` has a verified boot log, no exit log.
  Next bounded scope: correct PNVS admission/mapping/read bounds to the captured
  length, add independent boundary coverage, and rerun native staging/VM checks.
  Scope also checks the post-mode fixed enables: the later Global Lock write
  suggests firmware armed power-button bit8. Mask only this exact bit before
  namespace load, without status acknowledgement or widening permitted enables;
  unexpected sources halt. All 14 battery VM cases pass; the final fixture
  also verifies both mode cases after actually placing Q35 in legacy mode.
  Host/sanitizer mapping/read boundaries, connected captured-firmware replay,
  9 collector and 29 staging tests pass. Missing exit snapshots now receive an
  explicit incomplete-session flag. Read-only USB fsck after this halt was clean.
  Follow-up v31 `13d8083de35b51af` / source `57de926` passes the exact staged
  GRUB/UEFI Q35 rehearsal (expected native refusal, saved log bytes, clean
  exFAT, persistent halt). Next physical trial remains AC-connected and requires
  complete samples, both event restorations and clean capture/unmount.
- Recovery before this candidate: current `c0f2c3af0dbacc66`, previous
  `b427b78f21644bd2`; accepted power and interactive backups are verified outside
  build. Retain that previous release when installing the next candidate; the
  local run record owns installed/armed state.

## Active transition-test block

Air branch owns a separate guided AC-connected → battery-only → reconnected
capture, with three fresh consistent samples per phase, bounded waits, clean
retirement even when the user misses a phase, durable phase summaries and strict
collection. Scope: battery diagnostic/runtime, a pure phase checker, v32 staging,
VM fixtures, collector and guides. Preserve accepted v31 and ordinary desktop.
Baseline remains `b6075ef`; Pro PR #5 at `522c36a` now uses desktop v31 as well
as its prior profiles. The recipe prefixes remain distinct; do not rewrite
installed v31 recipes. Reserve **v32 laptop-probe** for this Air transition test;
shared main/staging integration still needs reconciliation. No new physical
acceptance or reboot is implied by source changes. Next checkpoint is host/VM
validation and an exact-stage rehearsal, then a guided physical round.

The v32 implementation brackets each battery sample with AC reads and requires
three consecutive coherent observations per phase. Unplugged acceptance requires
negative battery current; connected phases require nonnegative current. Each
phase has a 120-second deadline. Timeout retires normally, saves an incomplete
report and halts; hardware faults retain immediate containment. Final phase
proofs survive a bounded log tail, and collection requires both exact event
restorations and balanced teardown. Host phase tests (including ASan/UBSan),
30 staging tests, 10 collector tests and documentation links pass. Modeled
transitions and a real missed-unplug timeout followed by successful runtime
reentry pass. All 16 battery VM scenarios pass, including existing transport,
query, event-restoration and mode-entry containment. Exact staged **v32
`aa976febab59c128`**, artifact source `c25b32c`, passes the GRUB/UEFI rehearsal:
expected native refusal, saved refusal bytes, clean exFAT and persistent halt.
None of these results accepts actual AC changes.

That candidate is now installed for the guided round. Start connected to AC;
unplug and reconnect only when the persistent instructions request it, leaving
USB attached throughout. Successful retirement/save/unmount returns automatically;
timeout or fault halts visibly. Previous desktop `b427b78f21644bd2` is retained.
Accepted bounded diagnostic `13d8083de35b51af`, its evidence, and interactive/power
recovery copies remain outside `build`. The redundant, unreferenced failed
`6a2f90f72a0e4b96` EFI payload was archived and hash-verified before removal for
space. The local run record owns actual armed/reboot state; on return collect
the matching v32 session before another trial. PR #6 remains draft.

### Guided round collected — October 20

Two new sessions for `aa976febab59c128` were copied and hash-verified on return:

- `session-00065e3e608567ed-00` has complete three-phase proofs in `exit.log`,
  17 samples, 259 queries / 260 notifications, one attachment/detachment,
  both retained event enables restored, and zero errors/denials/heap. AC-connected
  current was 0 A; the accepted discharge sample was −1.455 A at 8.190 V;
  reconnection returned to 0 A at 8.335 V. Each phase has three consecutive
  coherent samples. The collector reports transition/runtime completion with
  no attention items. Relative SOC remained 98%; final remaining/full capacity
  6.107/6.293 Ah matches Linux after return.
- `session-00065e3e400fee10-00` has boot and periodic logs but **no exit
  snapshot**. The last saved state, around 44 seconds after startup, was waiting
  for AC unplug with AC still present. Its cause is unresolved; do not count it
  as a pass or infer a clean timeout.

The Ubuntu boot ID changed, the one-shot was consumed, and read-only exFAT
checking is clean (172 directories, 844 files). Both sessions and the exact
payload have hash-verified copies outside `build`. The user confirmed the latest
round was good and explicitly excluded earlier attempts from this acceptance.
The bounded AC-connected → discharge → reconnect milestone is **accepted on this
unit**, against the latest session only. The incomplete session remains historical
evidence and is not promoted to a pass. This does not establish calibrated gauge
accuracy or a persistent desktop service.

The v32 diagnostic payload and accepted evidence are now retained outside
`build`, alongside the v31 diagnostic and desktop recovery. No new reboot is
armed. Next is a cached battery/AC provider on the serialized worker, with explicit
lifetime, stale-data reporting and input/storage responsiveness validation before
ordinary desktop integration.

## First completed native battery diagnostic — October 20

Release `13d8083de35b51af`, artifact source `57de926`, completed five samples
in session `session-00065e3de26bb9fa-00`. Three logs were copied and hash-verified;
the collector reports complete runtime evidence with no attention flags.

- Actual mode entry enabled fixed power-button bit8; the new mask/readback path
  completed before namespace load. No PNVS mapping denial remained.
- Five samples: AC present, relative charge 98%, 8.374 V, 0 A, remaining
  6.094 Ah, full 6.265 Ah, temperature 30.45 C. Voltage/current/charge/full values
  match Linux snapshots before and after. This is comparison, not calibration.
- 75 EC queries / 76 notifications; one region attachment and one detachment;
  zero errors, denials or remaining interpreter heap. Both retained event enables
  restored with protected-write counts; diagnostic returned cleanly.
- Ubuntu boot ID changed and the one-shot entry was consumed. Read-only exFAT
  checking was clean (170 directories, 837 files). Accepted diagnostic payload
  and full evidence have durable copies outside `build`; previous desktop
  recovery `b427b78f21644bd2` remains installed.

This accepts the bounded AC-connected initialization/sample/retirement path on
this unit. It does not accept a long-running desktop service, discharge, AC
unplug/replug, suspend/resume, independent gauge calibration or other Macs.
That accepted **diagnostic** `13d8083de35b51af` remains a durable recovery copy;
the later transition block above records the next candidate. Ordinary desktop
startup still has no battery runtime.

Linux reports 86% while the diagnostic reports 98%. SrvrOS reads relative SOC
(command `0x0d`); upstream Linux SBS reads absolute SOC (`0x0e`). These use full
and design capacity respectively. Linux design capacity is 7.150 Ah, consistent
with that distinction. Raw absolute SOC has not yet been captured by SrvrOS;
do not change the relative gauge merely to force equality. Sources and limits
are in the [native runtime guide](../native-ec-runtime.md#first-physical-completion).

The subsequent guided round above completes bounded discharge and AC-change
acceptance. Next is a cached desktop provider with clearly labeled gauge/health
data. Keep EC work on the serialized worker and preserve the accepted diagnostics.

## Accepted scope and open items

| Capability | State | Evidence / limit |
| --- | --- | --- |
| Desktop boot, rendering, keyboard, trackpad, USB documents | Physically exercised | User rounds and durable workspace history in the development journal |
| Terminal backspace redraw | User validated | `5282cfde546ea2b5`, fix `c6e5538`; reported stray characters no longer appear |
| Text Ctrl+C / Ctrl+V / Ctrl+A | User validated | `5282cfde546ea2b5`; copy, paste and select-all explicitly checked |
| Held-click second-finger steering | User accepted | `d61e60a82a295d84`; counters do not independently prove every handoff |
| Brightness and audible volume adjustment | User accepted | `b427b78f21644bd2`; requested brightness 16/83 applied, three tone tests completed |
| Temperature and fan sampling | Live output confirmed | Fresh screenshot readings 83 C / 1202 RPM; logs reached 85 C, no SMC errors; calibration/power behavior not accepted |
| Two-finger physical right-click | Implemented; physical acceptance pending | Press role latches from initial contact count; one-finger press plus later steering finger remains primary |
| Context menu from trackpad taps | Physical acceptance open | Exit counts 5 primary and 1 secondary tap; user reported no visible right-click result |
| System window stability | No recurrence in latest physical round | `fed370ffbe33d554`: System and Help exited cleanly, no collector attention flags; earlier timeout cause remains unproven |
| PCM files / headphones | VM-tested / physical confirmation incomplete | Volume report does not separately accept these routes/workflows |
| Package power / CPU0 active frequency | Physical diagnostic accepted | `ed93e03b2585dcaf`; bounded Text idle capture, no CPU controls or whole-laptop power |
| Battery/AC diagnostic | Bounded sampling and AC transitions accepted | `13d8083de35b51af`: connected sampling; latest `aa976febab59c128` session: three AC/discharge/reconnect phases, both event restores and clean retirement; persistent desktop service pending |
| Suspend/resume | Not implemented | Separate ownership and acceptance milestone |
| Optional isolated AML probe | Native completion accepted | `4e5f438c4069752b`; resource queries, zero errors/denials/leaks; v22 still omits this diagnostic |
| USB Ethernet | Implemented CDC ECM, VM-tested | No actual adapter accepted |

## Latest interactive physical record

Release `5282cfde546ea2b5`, v22, source `dad7aa8`, contains the Terminal cursor
correction `c6e5538`. The user reports that the character artifact appears
resolved and explicitly validated Ctrl+C, Ctrl+V and Ctrl+A in Text. Those two
workflows are now physically accepted on this Air.

Matching session `session-00065e37be14b48c-00` has four verified logs, a saved
exit snapshot, clean desktop process exit and no collector attention flags.
There were no new screenshots in this round. Offline read-only exFAT checking
was clean (164 directories, 822 files). Evidence and the run record are local
under `build/native-desktop-trials/5282cfde546ea2b5/`.

At that interactive checkpoint installed was `5282cfde546ea2b5`; previous remained accepted recovery
`b427b78f21644bd2`, and no one-shot boot is armed. Secondary press/tap cases were
not individually confirmed in the report; power telemetry was not enabled.

## Earlier October 19 interactive round

Release `fed370ffbe33d554`, v22, source `c6e50ec` (same tree as merged baseline
`e2d172b`), was exercised on October 19. The user reported generally good
behavior, with a new Terminal backspace display defect. Session
`session-00065e37230cc95f-00` has four verified logs, a clean desktop process exit
and no collector attention flags. Two saved screenshots are retained locally in
`build/native-desktop-trials/fed370ffbe33d554/physical-after/`.

System and Help remained present in the screenshots and exited without failure.
PCM playback completed 1,152,000 bytes with zero reported underruns; the user did
not separately confirm channel separation or headphones. The Sensors screenshot
shows 67 C core/package and 1,209 RPM with fresh sample ages. The offline exFAT
check was clean (163 directories, 818 files). General success does not establish
every gesture case: the final two-finger tap counter was zero, so specific
secondary-tap acceptance stays open.

At that checkpoint installed was `fed370ffbe33d554`; previous remained accepted recovery
`b427b78f21644bd2`. The one-shot boot selection was consumed and no next boot is
armed. The Terminal screenshot's stray slash was reproduced in the host editor
and actual desktop VM: erasing padding left the output cursor beyond the edited
line. The source correction restores that cursor; the subsequent physical
retest above accepts that correction. See the [journal](../desktop-progress-2026-10-19.md).

## Earlier interactive physical record

Release `b427b78f21644bd2`, v22, built from source `5b1ed05`.
Session `session-00065e281812e807-00` has saved logs and clean desktop exit.
Local evidence is in `build/native-desktop-trials/b427b78f21644bd2/physical-after/`.
The detailed [journal](../desktop-progress-2026-10-18.md) preserves failures,
partial acceptance and the previous implementation sequence.

At this checkpoint current is `b427b78f21644bd2`, previous is `f05dfcc01c28dc85`,
and the independent held-drag backup is under
`~/.local/share/srvros/known-good/d61e60a82a295d84`. Re-read actual installed state
before any action; these are historical identifiers, not values for other Macs.
The local trial record marks this run returned/reviewed, not armed for reboot.

## Next actions

Current shared-code work is on `device/macbookair7-2/reliability-input-power`
from `main` at `3b0b43a`: physical click latching, compositor delivery deadlines,
idle/sleep scheduling and console polling. The [October 19 journal](../desktop-progress-2026-10-19.md)
records host/VM evidence. Native automated batch `54a944ae1f7849ac`
(source `686edd8`) passed all five filesystem/shell rounds with retained files
verified and a clean offline exFAT check. The following bounded console-idle capture `1fe693c9889f131f`
(source `b3f0e31`) saved 60 seconds of samples: 94.55% idle, package 80→75 C,
1,206 RPM final fan, no SMC errors. It returned automatically with a clean
offline exFAT check. Subsequent isolated AML capture `4e5f438c4069752b` (source `17c6ed3`)
completed cleanly in 5.13 seconds and returned with clean exFAT. Unattended
desktop capture `06c646455f08555e` (source `e6fd681`) subsequently measured
60 seconds with Text open: 94.15% CPU0 idle samples, one frame presentation and
76,384 framebuffer bytes during the measurement. All four logs and the 1440×900
BMP were verified, with clean automatic return and offline exFAT. At that earlier checkpoint the current boot
payload was the desktop-idle diagnostic, with accepted desktop
`b427b78f21644bd2` retained as previous. The newer interactive record above
supersedes that installed-state snapshot; power consumption still needs
physical acceptance. Timer samples measure CPU0 scheduler occupancy, not energy.
The generalized Linux collector completed with zero partial reads on this host.
Additional IRQ, semaphore and deferred-worker foundations now pass host/VM tests.
A separate full-uACPI Q35 kernel passes real SCI, contended AML mutex/sleep and
teardown tests, including worker-owned EC disconnect I/O and halting with
state retained on a poisoned controller or a disconnect deadline. It is not the
installed payload or a native EC/battery provider. Its immutable `6b7c2bc8`
artifact passed a verified 30-minute, 37-round runtime/EC soak. Later explicit
synthetic namespace initialization checks worker ownership and refuses readiness
when firmware methods log denied accesses even if the API returns success.

1. Physically accept two-finger secondary click while preserving established primary drag; host/transport tests precede this check.
2. Follow up the System timeout after the delivery fix; the native cause is not yet proven.
3. Extend the accepted console and unattended desktop idle measurements to interactive activity and actual power/frequency evidence, retaining USB/input responsiveness.
4. Battery/AC runtime, headphone route acceptance and actual USB networking.
5. Shared inventory/profile/scaling work for the other model agents, coordinated
   through the [expansion plan](../apple-hardware-expansion-plan.md).

Shared desktop preparation now includes explicit integer 2× scaling. UEFI VM
checks passed at 2880×1800/2× and the Air's 1440×900/1×, including exact capture
pixels and input/window geometry. The installed Air capture exercised the default 1× path; the additional Pro
models still require their own framebuffer and input acceptance.


Read-only [package-energy and CPU0 active-frequency telemetry](../power-telemetry.md)
has host arithmetic/service tests, VM refusal evidence and the accepted
October 19 v26 native capture. It remains opt-in. The Linux MSR capture confirms register availability
on this host but is not an SrvrOS power result. No CPU power controls have changed.

Trackpad transport/palm faults now cancel a held interaction rather than deliver
an ordinary release. The compositor also preserves queued typing on pointer
cancellation. Host fault tests cover both paths; native acceptance is pending,
and this is not claimed as the cause of earlier unexplained window closures.

The generalized Linux inventory now includes `cpu-policy.json`. The October 19
capture had no partial reads: four online logical CPUs, `intel_idle`/`menu`, and
four `intel_cpufreq`/`schedutil` policies with 500–3,200 MHz limits. CPU0 listed
POLL through C10 as enabled. These are Linux policy observations, not SrvrOS
energy or deeper-idle acceptance. The [power guide](../power-telemetry.md)
documents units, non-atomic capture and missing-field semantics.

## Latest automated power record

Release `ed93e03b2585dcaf`, source `0f2038a`, session
`session-00065e38449c2146-00` completed its 60-second Text idle capture and returned
to Ubuntu. Valid power checkpoints cover 52.999 seconds: 6.458 W average package
power and 2,083 MHz CPU0 active frequency, with 94.10% scheduler-idle samples over
the full desktop interval. See [power telemetry](../power-telemetry.md#accepted-air72-desktop-baseline)
for units, conditions and limits. Four logs and the screenshot verify; no collector
attention or rejected power samples; offline exFAT clean (165 directories,
827 files). Evidence is backed up under `~/.local/share/srvros/evidence/ed93e03b2585dcaf`.

Current installed is this automated diagnostic; previous remains accepted
`b427b78f21644bd2`. The one-shot is consumed. Interactive `5282cfde546ea2b5`
is independently backed up. No additional physical trial is armed.
