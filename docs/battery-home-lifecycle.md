# Battery lifecycle with mounted Home

The Air's accepted v47 capture runs the real Text file worker alongside native
SMBus completion events, but its outer workspace is parked. An exclusive image
lease bounds which storage can change. Ordinary mounted Home removes that bound:
apps, settings and logging can retain open files or asynchronous work. Native
Home monitoring and automatic startup remain disabled outside the explicit diagnostic.
The later v49 held-Home desktop session is now physically accepted on Air7,2:
release `fd6f6b9d397c339f` completed three service lifetimes, all 60 generated
edits and five exact file cycles, then retired before saving and unmounting.
See the [machine record](machines/macbookair7-2.md) for native scope and limits.

## Ownership audit

| Boundary | Existing behavior | Requirement before native Home enablement |
| --- | --- | --- |
| Start/Stop request | Process-group mailbox; syscalls only enqueue, session owner dispatches | Retain scoped admission, epoch checks and revocation before client teardown |
| Mounted storage | Explicit mounted diagnostics hold the exact device, parent and generation; parked captures retain their separate image lease | Preserve the hold across file-worker activity and release only after owner retirement |
| Controller events | Exact native profile owns query10 inside explicit parked or held-Home captures; serialized with sampling | Preserve profile, bank serialization and per-transaction/query bounds in a distinct mounted scope |
| Stop | Mask sources, drain worker, verify resources, disconnect, then retire owner | Prove the same ordering across UI restart/cancel and file-worker activity |
| Query refusal | Effect-free normal-return refusal is scoped to parked or held-Home ownership; other unsafe queries are terminal | Retain the checked mounted return across desktop lifetimes; no general AML unwind assumption |
| Fatal failure | Reset is blocked with mounted Home or active storage I/O; no filesystem calls from the fatal context | Keep containment; never flush/unmount uncertain state to force automatic recovery |
| Session exit | Revoke mailbox, retire all descendants, stop service, release storage after ownership ends | Preserve all clients and open-file lifetimes, including early exit |

The service/worker scheduling predicates permit unrelated I/O because their
callbacks use audited kernel mappings and serialized resources. That is scheduling
eligibility, not a mounted-filesystem lease or general firmware recovery proof.
The ordinary desktop does not acquire native controller ownership through a
cached status read, and this study does not change that behavior.

## Explicit System-control VM study

`acpiruntime battery-workspace-controls` and its `-manual` variant are available
only to the disposable VM backend with a mounted workspace and no image lease,
other runtime owner or bound controls group. Native invocation refuses before
platform activity. The existing System UI, mailbox, syscall and owner dispatch
are reused. Native `battery-controls` keeps its parked-storage admission.

`tools/host-env.sh python3 tools/battery_workspace_controls_smoke.py --case normal`
creates its own disposable exFAT disk, mounts Home, and runs two sessions with two
Start/Stop lifetimes each, with an unmount/remount between sessions. A new
non-reused token must be acquired for the second session. The synthetic controller
supplies completion and alarm
events. Each lifetime emits a drained event summary before runtime retirement;
the strict decoder checks epoch/serial progression, event sequence, acknowledgement
and invalidation counts, resource release and final scope revocation. The
compositor must be replaceable afterward, and unmount plus offline fsck/allocation
must preserve the entire workspace image byte for byte.

Additional cases are `manual` (mouse Start/Stop/Start, then close while active),
`early-exit` (cancel after Start), `runtime-fault` (uncertain alarm acknowledgement),
`refusal` (native backend), `unmounted-refusal`, and `profile-refusal` (VM backend
without the controller-event profile). The fault case must remain halted without
claiming runtime retirement, unmount or reset. It is stopped by the host harness,
and its disposable disk is retained without filesystem-cleanup acceptance.

The CPU0 mount hold is metadata ownership, not an I/O lock. It binds a block
device, its parent and a monotonically increasing mount generation. A separate
non-reused token prevents a stale release from freeing a later owner's hold.
Acquisition refuses unhealthy storage, another hold or active I/O; normal reads,
writes and sync remain permitted afterward. Unmount, scratch allocation and
image conversion refuse while held. Host fixtures cover stale/duplicate release,
remount, identity mismatch, counter exhaustion and storage failure. A storage
failure preserves the identity so a retired owner can release it; ordinary
failed-storage policy still blocks unsafe unmount.

The explicit VM SMBus and System-control sessions retain the hold through client
and runtime retirement, including Start/Stop/restart. Active-runtime guard probes
attempt unmount and both diagnostic conversions and require all three to refuse.
Uncertain runtime failure retains the hold and halts; it never claims release.
This adds no permission to reset with mounted storage or unwind arbitrary AML.

These are lifecycle and mounted-volume preservation tests, not a new concurrent
file workload or physical-input/gauge result. Prior Text/file overlap evidence
remains separate. Raw VM logs, screenshots and disposable images stay in ignored
local evidence. Nothing installs a new native payload or arms a reboot.

## Controls during submitted Text jobs

The explicit VM-only `acpiruntime battery-workspace-controls-files` fixture uses
the same scoped userspace control syscall as System. It starts monitoring, then
issues Stop/Start/Stop/Start during the first four asynchronous Text save jobs.
Each request must be completely transmitted to the ordinary file worker before
the battery action is enqueued. Reply consumption and battery-state observation
continue independently. The shared `desk_file_job_submit` helper advances the
bounded nonblocking write side without consuming a reply; existing callers keep
using `desk_file_job_poll`, which combines submission and reply validation.

The fixture performs 60 edits and five save/stat/rename/reopen cycles over one
minute. Its third battery lifetime stays active until session exit, where the
owner revokes controls, retires descendants and the service, then releases the
mount hold. These requests originate in the explicit Text fixture; the separate
System-button test remains the evidence for UI dispatch. A submitted but
unconsumed application job is not proof that USB DMA is active at that instant.

`battery-workspace-controls-files-cancel` exits after the first write has been
submitted and Stop enqueued. Its application workload is incomplete, even when
client/runtime cleanup succeeds. The compositor may terminate Text after its
bounded close grace expires; require its explicit child-exit status plus all
owner/worker retirement receipts and report that separately from a graceful
reported exit. Never require or infer a final userspace log after termination.
Offline checks permit only an absent draft or
a prefix of the two-byte draft, preserve both anchors and require clean allocation
and consistent metadata. Unsupported/native admission must leave the disk
unchanged. Uncertain runtime acknowledgement retains live ownership and halts
without attempting mounted-storage cleanup.

Run `tools/host-env.sh python3 tools/battery_control_files_smoke.py --case normal`;
other cases are `cancel`, `profile-refusal`, `refusal`, `unmounted-refusal` and
`runtime-fault`. The harness accepts no physical disk path. Strict receipts bind
control serials/epochs, submitted writes, three service retirements, event drains,
application/file progress and final mount release. Raw logs/disks stay local.

## Explicit mounted native return boundary

The full diagnostic kernel now exposes `acpiruntime battery-home`. It acquires
the exact mounted-volume hold and starts the existing gated battery service,
without launching clients or changing ordinary startup. The native backend still
requires the captured Air7,2 firmware/resources and host-controller profile; the
same path can be rehearsed with the disposable Q35 backend. Unsupported platforms
return unavailable before runtime engagement. Unmounted or conflicting ownership
refuses before acquiring a hold.

The diagnostic waits at most 30 seconds for fresh generation 3 and host events,
with a 128-event ceiling. It checks unmount/image/scratch guards while active.
On completion it masks sources, drains queries and worker, verifies resources,
retires the service and only then releases the mount hold. The volume remains
mounted for the caller's subsequent save/unmount. No filesystem operation occurs
inside runtime cleanup, and the diagnostic itself does not reboot.

A policy-refused query may return only after a complete EC transfer and normal
worker return, with no incomplete AML or attempted effects in that query batch.
Its cache becomes non-fresh and its outcome is `query-refused`, not completion.
An acknowledged alarm followed by a refused query in the same batch cannot use
that return path. Transport/ack uncertainty, deadline or resource failure retains
runtime and mount ownership; fatal reset is still blocked with mounted storage.
This scope does not grant recovery from arbitrary AML or enable normal desktop
monitoring. The explicit no-client native boundary is accepted below; ordinary
startup and desktop-session ownership remain pending.

`tools/host-env.sh python3 tools/battery_home_smoke.py --case normal` uses only a
disposable disk. Additional cases: `alarm`, `query-refusal`, `native-refusal`,
`unmounted-refusal`, `profile-refusal`, `runtime-fault`, and `effect-refusal`.
Clean command cases require exact byte preservation, a replacement compositor, unmount,
fsck and independent allocation checks. Fault cases require visible containment
without retirement/release/reset claims; the VM host terminates them.

The boot-flow fixture `tools/battery_home_capture_smoke.py` covers `completion`,
`alarm`, `query-refusal` and `uncertain-ack`. It verifies the actual capture path,
three saved logs, exact tree/metadata and allocation, automatic return for clean
outcomes, and visible halt without storage cleanup for uncertain ownership.
Saved records remain authoritative when concurrent UART writers interleave.

For the unattended hardware capture, stage the distinct v48 recipe with
`python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --battery-home`,
then rehearse the exact payload with `tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check`
and its `--probe-missing-workspace` variant. It uses the full native diagnostic
kernel, pauses periodic logging while keeping Home mounted, and disarms fatal
reset. After normal return it saves the final log, unmounts and returns to Ubuntu
without input. Handled unavailable/query-refusal results return as failed or
incomplete evidence; a live uncertain runtime still halts. The collector accepts
only complete, held, drained, retired evidence and never promotes a missing log
or a clean platform refusal into a native pass. Ordinary profiles are unchanged.

## Next implementation boundary

Air7,2 release `71ab6b95892a9a8c` (compiled `7fac77d`) passed the explicit
mounted-return boundary: fresh generation 3, 45 natural completion events,
resource-checked retirement, mount release, three saved logs and automatic Ubuntu
return. All 925 pre-existing non-metadata files were unchanged; offline exFAT and
allocation checks passed. No desktop clients ran, periodic logging was paused,
and no native alarms occurred. This accepts the bounded mounted owner, not
concurrent desktop I/O or physical charging/gauge accuracy.

Next establish desktop-session ownership before ordinary startup enablement. Preserve the same file/client and mount
lifetimes through cancellation and degraded service. A passing VM controller
fixture does not widen Air firmware/register gates or accept another machine's
EC/SCI ownership.

## Delivery through the desktop indicator

The native backend has produced fresh cached battery samples while handling
natural SMBus completion events on Air7,2. Bounded parked-image Text/file overlap
and orderly retirement are physically accepted. VM fixtures additionally cover
alarms, cache invalidation, control lifetimes and failure containment. Neither
proves physical gauge accuracy or native alarm handling.

The desktop already polls the cached status ABI and renders percentage plus AC
text in the menu bar. System uses the same widget presentation helper for
charging/discharging details, and has scoped Start/Stop controls. Stopped,
starting, stopping, stale and unavailable states are represented. Reads perform
no hardware I/O. This is a functioning presentation foundation, not ordinary
startup enablement or a finished graphical indicator.

The remaining delivery sequence is:

1. Integrate the accepted explicit Home session owner into ordinary desktop
   startup and exit, preserving descendant/mount lifetime and terminal-fault
   containment. The bounded v49 workload has native acceptance; cancellation,
   query refusal and uncertain-event cases retain their directed VM evidence.
2. Replace diagnostic duration/event ceilings with bounded continuous-service
   accounting and logging before enabling normal startup. Validate service
   unavailable/failed states, Start/Stop/restart, session shutdown and file work;
   do not turn the 512-event study into unlimited monitoring by deleting its guard.
3. Validate real AC plug/unplug, charging/discharging, fresh-to-stale transitions,
   percentage plausibility and session shutdown with Home mounted. Announce any
   required human action before the corresponding hardware boot. An automatic
   trial does not establish those manual observations.
4. Finish the quiet menu-bar indicator: compact battery graphic, percentage,
   charging/full/low and unavailable presentation, accessible details in System,
   narrow-screen layout and redraw checks. Derive labels only from valid cached
   fields; do not invent time remaining or equate 100% with a proven charge state.
5. Run the end-to-end hardware session and document accepted behavior, degraded
   behavior and remaining model-specific limitations.

Battery readout completion does not include suspend/resume, CPU power policy,
fan control or sibling-model enablement. Those retain separate acceptance gates.

## Explicit Home desktop sessions

The full diagnostic kernel also exposes `acpiruntime battery-home-controls`,
`battery-home-controls-manual`, `battery-home-controls-files` and
`battery-home-controls-files-cancel`. These reuse the existing System control
mailbox and desktop process-group owner. The exact mounted volume is held across
Start/Stop/restart and released only after every descendant and runtime retires.
A cached battery read or a control syscall from an unrelated process cannot
acquire this ownership. Native admission still requires the captured Air profile.
These commands do not change ordinary startup or install a hardware payload.

The session has the existing 110-second outer deadline and a 512-event ceiling
across the whole session, matching the bounded file-work duration. Every normal
retirement checks a masked, idle controller with its mount still held before
resource verification and disconnect. A clean refused query leaves a failed
session; uncertain transport or effects retain ownership and halt. No reset or
filesystem cleanup occurs from that fault context.

Rehearse the same explicit path on disposable Q35 with
`tools/host-env.sh python3 tools/battery_workspace_controls_smoke.py --home`
and `tools/host-env.sh python3 tools/battery_control_files_smoke.py --home`.
The original commands without `--home` retain their VM-only admission. System
mouse controls, active close, new mount generations, submitted Text file jobs,
cancellation, unavailable profiles and uncertain acknowledgement each require
separate receipts. Compact records retain every event in the bounded saved log.
Strict Home evidence adds a masked/idle/held return and
foreign-event restoration for every service lifetime. VM evidence does not
accept physical input latency, charging accuracy or another model's firmware.

The distinct v49 `--battery-home-session` boot profile automates the combined
Text/file session. Prepare a dedicated `/battery-study` directory on the USB
volume containing only `anchor.txt` with `Untouched ordinary workspace data`
and a final newline. The test creates five `text-N.txt` files, each containing
`AA`; do not point it at an existing work directory. Record the pre-run file
hashes after preparation. Logging pauses while Home remains mounted; final
receipts are saved after client/runtime retirement and before unmount/return.
The collector checks both the native session proof and exact study-file contents.
This recipe requires its own exact-stage rehearsal and physical acceptance.
Air7,2 release `fd6f6b9d397c339f` passed that acceptance with 180 natural completion
events, no alarms and clean retirement/filesystem/Ubuntu return. Ordinary desktop
startup, physical input latency and charging/gauge accuracy remain pending.

The generated control/file study acknowledges each twelfth edit only after the
save/stat/rename/reopen checkpoint. Native release `63333d27f5e5c064` failed the
old 4.7-second checkpoint wait while the second service lifetime and file work
were still progressing; it retired safely and preserved existing files. This
is a failed workload, not acceptance. The diagnostic now budgets 15.7 seconds
for these checkpoints, retaining 4.7 seconds for other generated edits and all
ordinary desktop input/client deadlines. It allows up to 90 seconds to finish
all 60 edits, with the existing 110-second owner deadline unchanged. Completion
still requires every file cycle and at least one minute of activity.

`battery_home_session_capture_smoke.py --slow-connect --slow-storage` adds four
seconds to each fixture connection and throttles disposable USB writes. Its
receipt must prove a file checkpoint exceeded the old budget, all five exact
files, resource-checked retirement and saved automatic return. These injected
latencies are VM regression evidence, not a measurement of native disk speed.

## Continuous accounting rehearsal (VM only)

`acpiruntime battery-home-monitor` is a separate disposable-VM session. The native
backend refuses the command before acquiring the mount or starting the runtime.
It acquires the same exact Home hold, opens System, starts the service once, and
retains the scoped Start/Stop controls until System closes. Client teardown,
runtime retirement and hold release still follow the accepted ordering. Normal
startup and existing native diagnostic event/time limits are unchanged.

The rehearsal replaces successful per-event records with fixed-size cumulative
telemetry: events, alarms, acknowledgements, unknown alarms and cache invalidations.
Counters saturate with an explicit saturation flag; they are not ownership tokens
or admission limits. CPU0 serializes updates and snapshots. Dirty summaries emit
at most once per five seconds per lifetime, without catch-up bursts, plus a final
snapshot after masking/draining. Failure records remain immediate. Every event
still executes its transport acknowledgement and cache policy; log coalescing does
not coalesce device operations. Per-query attempt/time budgets and all setup,
transaction, drain and shutdown deadlines remain intact.

The fixed System/compositor pair can run beyond the diagnostic 110 seconds and
512 events. The ordinary app session has a separate VM command described below; native continuous
monitoring still needs its own physical trial. No reboot/staging option selects this
VM path. The user-space `--battery-monitor` flag only changes the fixed System
client's lifetime; it cannot acquire a battery runtime capability on its own.

Run `tools/host-env.sh python3 tools/battery_home_monitor_smoke.py --case normal`.
The disposable model supplies 32 completion events per sample and the harness
keeps the first lifetime open for over two minutes before mouse Stop/Start and
close. It verifies counts, summary spacing, runtime retirement, replacement
compositor, byte-identical workspace and offline allocation. Other cases cover
alarms, early exit, native/unmounted/profile refusal, effect-free query refusal,
uncertain acknowledgement and effects preceding a refused query. Fatal cases
must retain the mount/runtime without cleanup or reset claims. VM results do not
accept physical alarms, gauge accuracy, continuous native startup or input latency.

### Ordinary desktop ownership follow-up

The current diagnostic `desktop_tree` retains historical PIDs in a
`PROCESS_MAX_PROCESSES` array. That bounds diagnostic memory, but a desktop that
opens/closes enough apps would eventually exhaust it. Simply pruning reaped
parents or scanning the original process group is insufficient: Terminal jobs
can change process groups or create sessions, and a parent can exit between
owner polls. The group's purpose is job control, not a permanent mount owner.

The kernel now provides `process_lifetime_create/spawn/revoke/drain/status/release`.
An owner token is installed during slot reservation, before loading or publishing
the root as runnable. All existing process allocation paths inherit it; exec,
job-control changes and orphan reaping preserve it. There is no userspace setter.
`fork()` remains unsupported; a future implementation must inherit this identity.
The CPU0 process manager serializes publication with short IRQ-masked sections.

Membership includes reserved slots and final resource destruction, not just
processes visible to `ps`. Revocation blocks new reservations. Already admitted
loads remain counted; drain requests cancellation and reaps completed detached
processes. The owner polls a bounded pass until `members==0`; only then can a
revoked token be released. Timeouts retain ownership. A released token is never
reused, and token exhaustion refuses new owners. Peak simultaneous membership
is separate from lifetime app count, so sequential launches do not exhaust a
historical PID array. Kernel waits and lifetime drain claim a zombie before
preemptible destruction to prevent competing reapers from releasing it twice.
Final removal wakes waiters that slept while the zombie was already claimed.

The continuous VM System session uses this owner, drains clients before stopping
the battery service, and releases its process token before releasing the exact
Home mount hold. Its `descendants` diagnostic field now reports peak simultaneous
membership. Native bounded diagnostic owners retain their existing implementation.
The monitor command still opens the fixed System client. The separate
`battery-home-desktop` command opens ordinary apps. Ordinary startup remains unchanged.

Host coverage exercises 512 sequential allocations, orphan/group/session changes,
revocation during preparation and release, stale tokens, table limits and token
exhaustion. The full-boot `lifetimetest` fixture exercises real executable churn,
failed loads, synchronous/asynchronous spawn, `setsid`, exec and orphaned
children/grandchildren; it drains the selected lifetime while leaving an unrelated
process alive. `tools/process_lifetime_smoke.py` runs it twice and then runs the
existing PTY regression. Each pass also runs 16 pairs of competing waiters,
requiring exactly one successful reap per child. The ordinary Home VM session
below adds periodic logging and shutdown with a submitted file write under this
new owner. Native integration still requires physical evidence.

The boot desktop path in `kernel/src/main.c` currently calls
`process_run_elf_args` directly and owns logging/unmount around it. Integrate a
session wrapper there only after the membership policy is proven. It must let
unavailable battery hardware leave the desktop usable, preserve ordinary
cached-only status reads, and retain mounted storage on uncertain ownership.
Periodic workspace logging now has VM overlap coverage below; the accepted
native v49 trial paused it, so physical overlap acceptance remains pending. Finish that VM coverage before adding a native
continuous-session recipe or changing the default startup path.

During the VM rehearsal, one development payload lost its serial channel while
printing retirement logs. A repeated run retained CPU/UART/screen evidence:
`serial_ready` was zero, the CPU was in the scheduler idle worker, and the screen
showed successful retirement, mount release and the monitor prompt. This was
lost host-observation transport, not proof of a stuck battery owner. The timed-out
runs remain incomplete evidence; they do not satisfy filesystem collection.
The harness now captures registers, UART availability and a screenshot on stalled
output. Periodic and final summaries have one service-thread writer so a
preempted owner cannot emit an older periodic snapshot after the final record.
That ordering change did not fix the general UART transmit-timeout policy.
The later ordinary Home work below adds bounded TX recovery while preserving RX.

Local validation: `07c06da` passed all nine directed continuous-session cases.
After integrating Air6,1 main `1b88c8a`, source `05a50bc` passed full build/both
kernels, a repeated extended session (1,680 events across two lifetimes), accepted
Home capture, pending-write cancellation, full core and documentation checks.
This is VM/host evidence only. The prior native PR53 release remains installed;
no ordinary startup or native continuous-service trial was enabled.

The inherited-owner checkpoint `8ea35fd` integrates actual main `50b14fe` and
passes full build/core/docs, the repeated process/PTY fixture, all nine continuous
battery cases, and the existing Home capture/cancellation regressions. The latter
still use the bounded diagnostic owner, so they are regression evidence rather
than proof of ordinary logging/file overlap. Exact payloads and 99 evidence files
are verified outside `build` in
`~/.local/share/srvros/local-validation/process-lifetime-8ea35fde079a`.
No native payload was installed and no physical acceptance was claimed.


### Ordinary Home apps and periodic logs (VM only)

`acpiruntime battery-home-desktop` admits only the disposable VM backend and an
idle logger. It pins Home, starts periodic workspace snapshots and launches
`deskd --home-apps` using the inherited kernel process lifetime. This is the
normal launcher, Text, Files and Terminal with workspace Home/preferences, not
the fixed System control fixture. Battery service starts once; apps only read
cached state unless explicitly granted the existing scoped control capability.
Unavailable battery hardware must leave the ordinary desktop usable.

Exit revokes the control capability and process reservations, drains all lifetime
members (including orphan jobs and file workers), stops battery work, then
quiesces the periodic logger and saves its exit snapshot. Only after these steps
and the runtime resource checks can the exact Home hold be released. A logger
quiescence timeout retains the write and mount; it never fabricates completion.
The final snapshot uses the existing bounded storage operations. The command
returns to the monitor with Home still mounted, for inspection or explicit unmount.
Native boot admission and `main.c`'s desktop startup path are unchanged.

`tools/battery_home_desktop_smoke.py` uses a fresh disposable USB image. It opens
apps through the launcher, saves Text and Terminal output, leaves session-changing
Terminal descendants alive, checks periodic and final snapshots, then checks
retirement ordering and offline filesystem allocation. The cancellation case
requests desktop exit after a save is submitted and before its result is received.
A submitted request proves outstanding app work, not physical USB DMA timing.
Native refusal and uncertain-runtime cases must not claim clean cleanup.

This workload reproduced permanent UART loss after a bounded TX spin timeout
while desktop/battery/logging continued. TX timeout now preserves RX and uses a
single readiness check per later byte while stalled; readiness resumes output.
The timed-out bytes can still be lost: complete persisted logs and explicit
completion records remain necessary evidence. Host fixtures check TX recovery,
RX during the stall, bounded polling and preservation of the caller's IRQ state.


Source `4351910` passed the full build, both battery kernels, full core suite,
documentation checks and all five ordinary Home VM cases. Normal and submitted-save
exit peaked at ten members, retired all members and processed 424/552 battery events; the unavailable
profile remained usable with eleven peak members. Clean cases verified exact saved
files, alternating and final logs, retirement ordering and offline allocation.
The inherited-process/PTY, continuous alarm/restart and older bounded Home cancel
regressions also passed. Durable evidence is under
`~/.local/share/srvros/local-validation/home-desktop-43519102297f`.
These checks do not enable or accept ordinary native startup.

## Explicit automatic Home capture (v51)

The `--battery-home-desktop` staging option selects a separate, automatic native
capture. It starts the ordinary Home desktop with Text open and the battery
service active, measures 180 seconds, saves a screenshot under Home/Screenshots,
then exits. The logger stays active throughout and writes alternating periodic
snapshots. This is an explicit capture only: ordinary startup and the native
`acpiruntime battery-home-desktop` command remain disabled. Native controller,
firmware and resource gates are unchanged.

The inherited process lifetime covers Text, file workers and the screenshot
worker. On exit it is revoked and drained before stopping the battery service.
The runtime proves its held-source return and resource retirement before the
process lifetime is released. Periodic logging then quiesces before releasing the
mount hold. The outer capture writes the completion record into `exit.log`,
unmounts and returns to Ubuntu. A platform or query refusal is a failed capture
with a clean return when ownership permits it. Uncertain live runtime ownership
still halts without claiming cleanup or writing a final report.

No input is required; keep power and the designated USB workspace connected.
The desktop has a 240-second supervisory deadline. Completion also requires at
least 175 seconds of service observation and 30 fresh generation advances;
the desktop's independent idle measurement must cover at least 179 seconds.
These are continuity checks, not a maximum polling latency or power target.
The screenshot worker has its existing 30-second deadline. Retirement retains
the existing five-second process drain and 35-second service stop bounds.

`battery_home_desktop_capture_smoke.py` exercises the actual boot capture on a
disposable Q35 volume, including completion, profile/query refusal and uncertain
acknowledgement. `battery_home_desktop_records.check_capture` validates saved
receipts, generation progress, elapsed time, matching lifetime identity, resource
restoration and shutdown order. Physical collection must additionally bind the
log to its staged source/release and new session, verify the saved BMP and all
pre-existing files, then run offline filesystem/allocation checks. A VM pass
accepts neither physical gauge accuracy nor native AC transitions. An attended
unplug/replug and indicator check still precedes ordinary native startup.


Air7,2 v51 release `8666698f5e5d4ed5` has now passed this explicit native capture:
201.276 seconds, 74 samples, 1,110 events, 74 handled/zero unclaimed SCI,
five inherited members retired, four logs and a 1440x900 screenshot saved.
Prior data and offline allocation checks passed. The screenshot shows `AC · 97%`;
AC transitions and physical gauge accuracy still need attended acceptance.
See the [machine record](machines/macbookair7-2.md) for source/session identities,
validation and durable evidence. This does not enable ordinary startup.

## Guided Home AC transitions (v52)

`--battery-home-transitions` is a separate attended capture using the same held
Home and inherited desktop lifetime as v51. Ordinary native startup remains off.
Tell the tester before reboot: keep USB connected throughout, start with AC
connected, unplug only at the second on-screen prompt, and reconnect only at the
third. No keyboard input is required. Each instruction waits up to 120 seconds.

The desktop reads the cached battery ABI once per second. Each phase requires
three consecutive, distinct fresh generations in one epoch, with valid SOC,
voltage, current and capacity. AC phases require AC present and nonnegative
current; the unplugged phase requires AC absent and negative current. Duplicate
cache reads do not advance progress; stale, invalid or skipped samples reset the
streak. Epoch or timestamp regression fails the trial. Three phase receipts
record the raw values and actual global indicator label.

After each phase, the ordinary screenshot worker must save and verify its BMP
before the next instruction appears. Its deadline is 30 seconds. The owner has
a separate 480-second watchdog; existing process/runtime/logger retirement
bounds are unchanged. On success, or a timeout with safely retired ownership,
the capture saves logs, unmounts and returns to Ubuntu automatically. Uncertain
live controller ownership still halts without attempting unsafe storage cleanup.
A missed prompt is an incomplete trial, never a pass.

Run `tools/host-env.sh python3 tools/battery_home_desktop_capture_smoke.py --transitions`
for the disposable VM's automatic AC/discharge/AC sequence. Add `--case no-transition`
to verify a missed unplug times out and returns safely; profile/query refusal and
uncertain-ack cases remain available. Host tests exercise duplicate/stale samples,
phase ordering and missing/corrupt screenshot collection. The physical collector
requires all four logs and three distinct, verified screenshots in addition to
the existing owner/resource retirement receipts. Bind them to the exact staged
release/new session and check prior files and offline allocation after return.

The VM supplies synthetic transitions. Native AC transition behavior requires
the attended test; neither proves calibrated capacity, remaining runtime or
battery health. Suspend/resume, thermal policy and ordinary startup are separate
milestones. No firmware/resource gate changes are part of this capture.


Air7,2 v52 release `c06f94a5893a402d` passed the attended AC/battery/AC sequence:
three screenshots, 61.111 seconds, 339 events, two acknowledged alarms and
51 handled/zero unclaimed SCI interrupts, followed by clean lifetime/runtime
retirement, verified logs, unchanged prior data and clean offline allocation.
This accepts the explicit native transition/indicator path. Ordinary native
startup and calibrated gauge accuracy remain separate milestones. See the
[machine record](machines/macbookair7-2.md) for source/session identities and
why Ubuntu's absolute SOC differs from the relative SOC displayed here.
