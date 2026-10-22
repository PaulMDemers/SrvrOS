# Battery recovery resource audit

Audited against the in-tree runtime and pinned uACPI sources on 2026-10-21,
starting from `3c98c42` (PR #39). This is an ownership audit, not native recovery
acceptance. Read with [battery service](battery-service.md) and the
[Air7,2 record](machines/macbookair7-2.md). Native mounted Home and automatic
battery startup remain disabled.

## Finding

The existing VM faults occur before a new sample starts EC traffic. Their clean
IRQ return and retained service stacks prove scheduling and storage continuation,
but do not prove that an interrupted firmware transaction can be abandoned.
Masking SCI also does not retire firmware ownership or complete an EC command.
The next native milestone needs those distinctions explicitly enforced.

| Resource | Current owner and lifetime | Fault implication |
| --- | --- | --- |
| Service and worker stacks, interpreter heap, queues and private locks | One CPU0 service owner and one worker; retained together on the explicit VM recovery path | Retention prevents reuse but does not release any external resource |
| EC byte exchange | `runtime.lease` and `ec_transport`, serialized on the worker; AML region access and query traffic use the same transport | A command/address/data exchange may be incomplete even if the CPU thread stops; active, failed or poisoned transport is refused |
| EC-backed SMBus bank | The sample lease encloses the sampler and its read-word transactions | Completing one EC byte does not complete the SMBus protocol; sampler/transaction activity or uncertain completion is refused |
| Firmware Global Lock | uACPI public API, AML `Acquire(\_GL)`, or a field's Lock rule; FACS Owned/Pending plus OS mutex/event state | `_GLK=0` only avoids an EC-specific lock requirement. It does not forbid other AML paths from acquiring this lock |
| Fixed/GPE enables and SCI route | Explicit service lifetime; sources masked on failure; prior route/event ownership retained | Route masking contains delivery, not firmware execution or transaction completion. Fault recovery does not restore foreign ownership |
| NVS, graphics mailbox and allowed firmware I/O side effects | Exact native profile permits particular reads and masked writes during namespace initialization/query execution | A quiet EC or free Global Lock does not prove those changes are independent of other devices or firmware |
| Workspace and USB I/O | Separate scheduler I/O owner and normal filesystem teardown | Battery recovery must never quarantine that owner, flush from a fatal IRQ context or reset while storage is still live |

## Global Lock paths that must not be conflated

In [mutex.c](../ports/upstream/uacpi/source/mutex.c), the public acquire/release
API maintains `global_lock_acquired`. AML mutex acquisition instead records
owner/depth in the `_GL` mutex and uses the same firmware handshake without
setting that API flag. The underlying one-unit semaphore also covers windows
before those higher-level fields are updated. Firmware wait state uses
`global_lock_pending` and a separate event.

The FACS exchange uses atomic compare/exchange directly on the mapped lock word.
Consequently, the adapter's denial of ordinary MMIO writes is not evidence that
FACS is never modified. The native PM1 GBL_RLS write rule already permits the
matching release notification. Do not force-clear FACS bits, release another
thread's mutex, or treat a single clear lock-word read as an ongoing ownership
lease.

The EC namespace contract rejects nonzero `_GLK` in
[battery_runtime.c](../ports/srvros/uacpi/runtime/battery_runtime.c), but that
contract is narrower than all firmware locking. Callback spinlock balance is
also narrower: it says nothing about a lock held by the interrupted worker.

## Enforced recovery admission

Every existing VM quarantine path now takes an IRQ-masked resource snapshot
before mutating scheduler retention state. The
[pure policy](../ports/srvros/uacpi/runtime/battery_recovery_resources.h) rejects:

- Unknown Global Lock state or unavailable mapped FACS/mutex.
- A live EC lease, any EC phase other than idle/done, or a poisoned transport.
- An active/waiting sampler or a started SMBus transaction that is not complete.
- Public API ownership, AML ownership/depth, an occupied/closing/waited-on Global
  Lock semaphore, or pending firmware acquisition.
- Either FACS Owned or Pending bit.

The snapshot is observational. It does not acquire, release, cancel, reset or
repair these resources. A refusal follows existing terminal containment and
never reports clean storage retirement. Its `battery-recovery-resources` record
identifies the blockers. Successful recovery reports must include exactly one
complete zero-blocker row; missing, contradictory and duplicate rows fail checks.
The VM-only command gate remains separate and mandatory. This predicate must
not be used as native admission on its own.

The guard intentionally depends on the pinned interpreter's internal `_GL`
mutex/context layout and this port's semaphore representation. An interpreter
or semaphore update must rerun this audit and the fixtures, not just compile.
Other private interpreter locks remain with the retained interpreter; this is
not a general lock unwinder or proof that arbitrary AML side effects are safe.

## Reproducible checks

Run through `tools/host-env.sh`, one VM at a time:

```sh
make test-battery-recovery-resources test-battery-desktop-records
python3 tools/battery_workspace_smoke.py --case recovery-resource-api
python3 tools/battery_workspace_smoke.py --case recovery-resource-aml
python3 tools/battery_workspace_smoke.py --case recovery-resource-ec
```

The first two VM cases acquire the real in-tree Global Lock through its public
API and through AML `Acquire(\_GL)` respectively, then sleep across the owner
watchdog. They require the distinct API/AML ownership proof and FACS Owned bit.
The third pauses the modeled worker after a real EC transport poll submits a
command, with the lease still held and the transport awaiting the address.
All three must halt without quarantine, success, log-save or automatic-reset
claims. Disposable disk bytes are retained as interrupted evidence, not declared
clean. Existing idle-protocol recovery and normal mounted Text are regressions.
These are Q35/model tests, not physical firmware contention tests.

## Remaining native acceptance work

1. Use the completed [native AML side-effect map](battery-native-aml-audit.md)
   for setup, `_REG`, `_INI`, all nine captured `_Qxx` methods and disconnect.
   It selects a controlled predispatch failure after a completed query exchange
   as the next VM experiment; shared writes and partial setup remain excluded.
2. Establish a native policy for faults during each external protocol. A stalled
   EC/SMBus exchange or firmware lock wait cannot be solved by masking SCI and
   retaining a thread. Keep these terminal unless bounded recovery is demonstrated.
3. Define which normal-return fault boundaries can reach a resource-idle point
   without forced cleanup. Preserve the current separation between callback
   return, IRQ completion and scheduler retention.
4. Validate the resulting narrow policy on disposable VMs, then an automated
   Air trial using the existing parked workspace and preserved recovery payload.
   Mounted Home remains a separate physical acceptance step after those proofs.

There is no basis here to change the installed native payload, enable background
battery startup, or claim physical SCI/controller/DMA recovery.
