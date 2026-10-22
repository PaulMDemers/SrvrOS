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
| Mounted storage | VM study checks `workspace_active()`; native capture binds the exact parked device | Bind the mounted volume/lifetime, preventing unmount/replacement while owned |
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
Start/Stop lifetimes each. The synthetic controller supplies completion and alarm
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

These are lifecycle and mounted-volume preservation tests, not a new concurrent
file workload or physical-input/gauge result. Prior Text/file overlap evidence
remains separate. Raw VM logs, screenshots and disposable images stay in ignored
local evidence. Nothing installs a new native payload or arms a reboot.

## Next implementation boundary

Combine the proven mailbox lifecycle with explicit mounted-volume identity and
revocation, then test Start/Stop/restart and refusal during actual asynchronous
Text file operations. Keep native admission disabled until that ownership and
normal-return proof is reviewable. A passing VM controller fixture does not widen
Air firmware/register gates or accept another machine's EC/SCI ownership.
