# Battery lifecycle with mounted Home

The Air's accepted v47 capture runs the real Text file worker alongside native
SMBus completion events, but its outer workspace is parked. An exclusive image
lease bounds which storage can change. Ordinary mounted Home removes that bound:
apps, settings and logging can retain open files or asynchronous work. Native
Home monitoring and automatic startup remain disabled.

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

## Next implementation boundary

Combine the held mailbox lifecycle with actual asynchronous Text file operations,
including Start/Stop/restart, cancellation and refusal. Keep native admission disabled until that ownership and
normal-return proof is reviewable. A passing VM controller fixture does not widen
Air firmware/register gates or accept another machine's EC/SCI ownership.

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

1. Prove mounted controls and real asynchronous file work together, including
   cancellation, close, restart and volume release. Preserve per-session ownership.
2. Establish the native mounted normal-return/refusal path and bounded event
   behavior without weakening terminal-fault storage containment. Integrate a
   desktop-session service owner; enable normal startup only after acceptance.
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
