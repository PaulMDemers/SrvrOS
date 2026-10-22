# Battery lifecycle with mounted Home

The Air's accepted v47 capture runs the real Text file worker alongside native
SMBus completion events, but its outer workspace is parked. An exclusive image
lease bounds which storage can change. Ordinary mounted Home removes that bound:
apps, settings and logging can retain open files or asynchronous work. Native
Home monitoring and automatic startup remain disabled outside the explicit diagnostic.

## Ownership audit

| Boundary | Existing behavior | Requirement before native Home enablement |
| --- | --- | --- |
| Start/Stop request | Process-group mailbox; syscalls only enqueue, session owner dispatches | Retain scoped admission, epoch checks and revocation before client teardown |
| Mounted storage | VM SMBus/control studies hold the exact mounted device, parent and generation; native capture binds the parked device | Preserve the hold across file-worker activity and release only after owner retirement |
| Controller events | Exact native profile owns query10 only inside the parked capture; serialized with sampling | Preserve profile, bank serialization and per-transaction/query bounds in a distinct mounted scope |
| Stop | Mask sources, drain worker, verify resources, disconnect, then retire owner | Prove the same ordering across UI restart/cancel and file-worker activity |
| Query refusal | Normal-return native refusal is scoped to parked capture; other unsafe queries are terminal | Prove a mounted return path before enabling it; no general AML unwind assumption |
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

1. Retain the VM combined-control/file proof above through native integration:
   submitted jobs, cancellation, active-service close and mount release.
2. Extend the accepted explicit native mounted boundary to a desktop-session
   service owner, preserving terminal-fault storage containment. Normal completion
   has native evidence; query refusal and uncertain-event cases retain VM evidence.
   Enable normal startup only after desktop-session acceptance.
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
