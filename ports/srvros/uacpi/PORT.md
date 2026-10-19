# uACPI namespace evaluation

Upstream: https://github.com/uACPI/uACPI
Version: 7.0.0, commit `686a9a52d9ad72690406d7cada82801377d5e2fa`
License: MIT (retained in `ports/upstream/uacpi/LICENSE`).
Tarball: https://api.github.com/repos/uACPI/uACPI/tarball/686a9a52d9ad72690406d7cada82801377d5e2fa
Downloaded archive SHA-256: `26fc8f2fd7fedac1356732d9aa3f58620142d3aa9b1da69c6817293040e732fd`.
Only unmodified `include/`, `source/`, LICENSE and README.md are vendored.

The first adapter is `tools/acpi_namespace_probe.c`, a single-threaded offline
host tool. It builds synthetic RSDP/XSDT/FADT pointers around explicitly supplied
DSDT/SSDT copies, validates their checksums and evaluates selected objects.
Physical mappings are limited to these owned buffers and explicit snapshots. SystemIO accesses and uncaptured PCI/MMIO accesses are denied; there is no
`/dev/mem`, port I/O or host firmware access. `--region ADDRESS:FILE` and
`--pci SEG:BUS:DEV.FN:FILE` explicitly provide private copies of previously
captured memory/configuration state. Writes change those in-memory copies only;
the files and hardware are never modified.
`UACPI_REDUCED_HARDWARE` compiles out fixed-register/event machinery and
`UACPI_NATIVE_MMIO` prevents operation-region access through table mappings.
These are restrictions of the offline adapter, not claims about laptop hardware.

The tool loads module-level AML, but does not call namespace initialization,
enter ACPI mode, install an EC handler or run `_REG`/all `_INI` methods. Its
Darwin profile disables other vendor `_OSI` strings; selecting it is an explicit
probe option, not yet native OS policy. Each evaluated object is named on the
command line. Loop depth/time and host runtime are bounded. A denied access is
reported as incomplete; values from a denied access must not be treated as hardware readings.

Native SCI/GPE ownership, scheduler synchronization, EC query dispatch and the
battery provider remain separate work. Offline resource evaluation cannot prove
that battery sampling, suspend, power-off or firmware initialization works.

## Offline ECDT/namespace agreement

`--ec-contract ECDT.dat FACP.dat` validates the supported resource layout and,
after explicitly requested methods, compares the ECDT device path, HID, UID,
GPE and CRS with the loaded namespace. It also reports optional Global Lock
requirements. No controller access is added. The
[native EC runtime gate](../../../docs/native-ec-runtime.md) records the supported
subset, captured Air result, initialization side effects and remaining ownership
work. `make test-acpi-ec-resources test-acpi-namespace` covers malformed tables,
namespace disagreements, type errors and private snapshot behavior. Logged AML
errors during evaluation now contribute to failure, including APIs that return
success. Ordinary desktop startup and the restricted native probe are unchanged.

## Isolated native probe

`ports/srvros/uacpi/kernel/probe.c` is linked with the same upstream interpreter
and the same reduced-hardware/native-MMIO restrictions. `srvros.aml=probe` opts
into a single boot-time run. It copies known ACPI tables, lazily snapshots only
firmware-described ACPI reclaimable/NVS ranges, and copies registered PCI
configuration data on first use. Every AML memory/PCI write targets a private
copy. Arbitrary physical mappings and all SystemIO are denied. Copy and
interpreter heaps each have a 4 MiB budget; each copy is at most 1 MiB. Allocation
headers count toward the interpreter budget and preserve heap alignment.

This is a single-threaded probe adapter, not the eventual native ACPI runtime.
Blocking semaphores, long sleeps and asynchronous work are explicitly unsupported
and make the result incomplete. It does not call namespace initialization,
register an EC handler, enable SCI/GPEs or enter ACPI mode. It evaluates the
known MacBook resource paths and `PINI` only in the isolated namespace; changes
to GNVS/PCI copies are discarded. Diagnostic error/denial/leak counts accompany
the result. Firmware snapshots and the interpreter are released afterwards.

Host tests compile our own ASL fixture and cover resource values, `_OSI`, private
snapshot writes, missing objects, bad checksums, denied I/O and loop deadlines.
`tools/aml_probe_smoke.py` runs the native adapter in QEMU; `--fixture` adds our
synthetic EC/SMBus resource contract. Neither test proves a physical battery read.

## Deferred-work foundations (no live firmware binding)

`runtime/work_queue.c` implements the intrusive queue needed by the pinned
uACPI 7.0 deferred-work contract. Items are allocated before scheduling, so
enqueue/dequeue take constant time and allocate nothing. Taking an invocation
detaches and clears its item before the callback runs: the callback may requeue
or free that item. Completion touches only queue state, and idleness includes
in-flight callbacks as well as pending work. Duplicate scheduling, including
placing one pending item on another queue, is rejected without changing either
queue. GPE/notification work types and callback contexts remain attached to
their individual invocations.

`runtime/work_worker.c` now binds this queue to an ordinary CPU0 kernel worker.
Enqueue is IRQ-safe and allocation-free; callbacks execute outside the lock and
yield between every invocation. Drain includes pending and in-flight work,
including callback requeues. Finite monotonic deadlines, worker self-wait refusal,
idle-only stop admission and explicit stopped publication protect teardown.
Stop timeout leaves a stop request pending; callers must wait for success before
freeing worker storage. Lifecycle ownership excludes new producers and waiters.
This remains separate from the pinned `uacpi_kernel_*` callbacks: the isolated
probe continues to deny asynchronous work, and ordinary boot starts no worker.

Host queue/worker fault tests and ASan/UBSan cover duplicate enqueue, finite/zero
waits, overflow, unavailable scheduler/clock, IRQ flag restoration, callback
reschedule/free, yielding and stop/restart. `acpiworktest` exercises the actual
ISR registry using software interrupt 47, eight enqueue attempts with one pending
item, a blocked callback/drain timeout, 1,000 self-rescheduling callbacks with
alternating work types, self-free and stopped teardown. `tools/acpi_work_smoke.py`
runs four successive worker lifetimes in QEMU. This changes no I/O APIC route
and is not a level-triggered hardware SCI test.

The future uACPI completion callback must first synchronize installed interrupt
producers, then call drain. Queue idleness alone is not interrupt quiescence.
SCI ownership, full runtime initialization and EC integration remain required;
see the [integration plan](../../../docs/macbook-air-laptop-services-2026-10-18.md#live-acpi-runtime-integration-plan).

`make check-acpi-runtime-build` separately compiles the full, event-enabled
upstream sources plus the queue with SrvrOS's freestanding x86-64 flags, then
combines them with a relocatable link. It passes without changing native kernel
inputs. The combined object's remaining undefined symbols are the expected
uACPI kernel callbacks and `memcmp`/`memcpy`/`memmove`/`memset`; no unexpected
compiler runtime dependency appeared. This is a compile feasibility check,
not a linked OS adapter or a runtime test. Its log and symbol inventory are
retained under `build/review/acpi-runtime-*`.

## Physical probe limitation

The MacBook's first combined-device trial stopped after the native probe's
FADT-length compatibility warning, before workspace logging or desktop startup.
The warning is not itself fatal; that trial did not localize the stall.
Normal desktop profiles from v22 omit `srvros.aml=probe`. The subsequent
explicit native diagnostic completed, as recorded below. Keep this resource
probe separate from normal desktop startup and live firmware ownership.

The October 19 investigation adds stage and per-object entry markers. Captured
DSDT/SSDT injection in QEMU reproduced a false heap-corruption halt at 4,096
blocks; removing that arbitrary ceiling lets the diagnostic return. The native
stall cause was not yet confirmed by the replay alone. `tools/aml_probe_smoke.py --tables <capture>/acpi`
validates input checksums and records table hashes, phase output and failure
registers. QEMU's generated tables and unavailable firmware memory make this a
stress/replay diagnostic, not a faithful machine model or successful battery
probe; `RETURNED` deliberately differs from the synthetic fixture's `PASS`.

A separate v24 staged profile (`--laptop-probe-volume UUID --aml-capture`) starts
workspace logging before the isolated probe. It returns to Ubuntu only after
error/denial/leak-free completion and successful log flush/unmount; a failure
halts for inspection. This is explicitly requested diagnostic execution and
remains absent from v22 desktop and v23 idle profiles. Progress markers improve
stall localization but are not a watchdog: a kernel fault or stalled interpreter
can still require manual recovery. No live ACPI/EC/SCI ownership is added.

The subsequent native v24 capture `4e5f438c4069752b` completed in 5.13 seconds
with zero errors/denials/leaks and four private snapshot writes. EC/SMBus/SBS
resource evaluation succeeded and the logged capture returned to Ubuntu with
clean exFAT. This clears the isolated probe stall gate after the heap fixes;
it does not enable live runtime ownership or accept battery readings. Normal
desktop startup continues to omit this diagnostic.

## Kernel synchronization foundation

`kernel/src/semaphore.c` now provides CPU0 counting semaphores with IRQ-safe,
allocation-free signal; nonblocking/finite/infinite waits; monotonic timeout
checks; closed-state wakeup and explicit drained teardown. Owners must exclude
new callers before freeing storage. Waits are ordinary kernel-worker work, not
an I/O service or cancellable userspace path. This is not an SMP lock. The
current 100 Hz timer governs scheduling granularity; finite waits round upward
and never report timeout before the monotonic deadline.

The scheduler supplies nonzero lifetime identities, including distinct IDs when
a worker slot is reused. Worker publication/identity allocation is protected
against local IRQ preemption. Host checks cover lost-wakeup boundaries, IRQ flag
restoration, counter/deadline overflow, spurious wakes, failure and close/drain.
The explicit `semaphoretest` monitor diagnostic and `tools/semaphore_smoke.py`
exercise real blocked workers, timeout and four successive worker lifetimes.
These primitives are not yet bound to a live uACPI adapter; the isolated probe's
single-threaded callbacks and hardware restrictions remain unchanged.

## Shared interrupt foundation

CPU0 now has a fixed-capacity shared IRQ callback registry. It rejects duplicate
registrations, reserved legacy lines, stale removal tokens and mutation inside
dispatch. Callbacks run with IRQs masked and cannot sleep or yield; all handlers
on a shared line run even after one claims it. Removing a handler does not mask
hardware: the owner must first disable its source and mask its route. No SCI
handler is registered by this change. Unknown IRQ logging is bounded to its first
occurrence per line; this prevents a log flood, not an interrupt storm.

The I/O APIC now exposes checked masked preparation, readback and mask/unmask
operations for installed legacy IRQ vectors. Preparation refuses active or
pending routes, uses explicit polarity/trigger mode and stays masked. Selector
and window accesses are protected from local IRQ preemption. Register range,
write order, readback failure, alternate GSI bases and existing legacy routes
are host-tested. Source ownership, FADT/MADT SCI resolution, repeated level
assertion and full initialization rollback remain adapter work. No new route
is enabled at startup.

## QEMU SCI fixture

`scitest arm|arm-fail|status|stop` and `tools/acpi_sci_smoke.py` exercise a real
emulated level-triggered SCI, separate from uACPI. The fixture requires the Q35
host bridge/LPC IDs, BOCHS FADT, SCI 9 and the expected PM/GPE register layout;
it refuses other identities before port I/O. It also refuses pre-owned enabled
fixed/GPE sources or an active route. It is never invoked by normal startup and
cannot match the MacBook's hardware profile. This is a VM test, not a portable
ACPI mode policy.

QMP power-down requests set the emulated fixed power-button event. The first
IRQ deliberately leaves the level asserted through EOI; the second delivery
quiesces the source and queues its clear/re-enable on the worker. A second
registered observer confirms shared dispatch. Three lifetimes each passed eight
power events with exactly sixteen IRQs and zero errors. Each lifetime also
injects a failed start after preparation/mode entry and verifies cleanup before
restarting. Stop disables the source, masks the route, removes callbacks, drains
and stops work, restores the saved masked route and original ACPI mode. A later
power event causes no callback. Host checks cover the identity gate and masked
route restoration, including unsafe snapshot refusal.

This fixture follows QEMU's [ICH9 power event wiring](https://github.com/qemu/qemu/blob/master/hw/acpi/ich9.c)
and [fixed-event status/enable behavior](https://github.com/qemu/qemu/blob/master/hw/acpi/core.c).
The full event-enabled uACPI kernel adapter is still not linked into the normal
OS. Native SCI/EC ownership and battery sampling remain unaccepted.

## Separate full-runtime VM kernel

`make acpi-runtime-vm-kernel` links the full, event-enabled upstream sources and
`runtime/qemu_adapter.c` into **build/acpi-runtime/srvros.elf**. It does not replace
the normal kernel, ISO or hardware staging input. The normal kernel retains its
reduced-hardware probe and a weak `acpiruntime` refusal stub. The separate image
replaces that stub and refuses isolated-probe invocation. Verify both builds;
`tools/acpi_runtime_smoke.py` copies the alternate ELF only into its disposable
ISO. Ordinary hardware profiles do not start this runtime.

The adapter requires the strict Q35 fixture identity/register gate, then maps
only inventoried ACPI tables, RSDP and the fixture's FACS. Port mappings are
limited to the known Q35 PM/GPE/SMI ranges; PCI configuration and operation-region
MMIO are denied. It binds CPU0 IRQ registration, semaphores, stable thread IDs,
IRQ-preserving locks and the deferred worker. Interpreter allocations have an
8 MiB budget and leak accounting. It starts with `NO_ACPI_MODE`, loads the
namespace, installs a fixed power event and enters ACPI mode explicitly. It
**does not run namespace initialization, EC `_REG` or finalize AML GPE users**.
This is a bounded integration fixture, not the laptop runtime adapter.

The VM test passed three clean lifetimes plus failed IRQ-install rollback on
each. Thirty QMP power events caused deferred evaluation of a synthetic AML
method with a mutex and blocking sleep. Concurrent monitor evaluation contended
for that AML mutex (one contended semaphore wait per round) and produced ordered
counter results. Stop while work was pending drained it before freeing the
namespace. Each clean reset had zero errors, denials or remaining interpreter
allocations, restored masked routing/mode, and accepted no post-stop callback.
An intentional permanently blocked worker then exercised failure: teardown
halted with 162,283 bytes retained instead of freeing live state. This expected
negative VM result must never be reported as a native halt acceptance.

Run the normal `aml_probe_smoke.py --fixture` separately to preserve isolation.
The next runtime work is synthetic EC operation-region/query/SMBus integration,
then firmware-specific ownership and initialization policy. An ordinary kernel
worker is deliberately not exempt from the storage ownership gate. Battery/AC
values, native Global Lock policy and suspend remain unimplemented services.

## Synthetic EC, queries and SBS integration

The separate VM kernel now includes `runtime/ec_fixture.c` and a synthetic AML
EC device. Its in-memory controller implements the byte protocol consumed by
the real `ec_transport`, `ec_smbus` and `sbs_sample` engines. It has no native
port binding. `_REG` connects the real uACPI EmbeddedControl region handler and
performs a read/write handshake. The handler rejects callers outside the chosen
ordinary worker or with interrupts masked. Query exchange releases its controller
lease before invoking `_Qxx`; a query method can therefore read the EC or execute
a complete AML SMBus transaction without recursive controller ownership.

The fixture queues queries while a complete SBS sample owns the controller and
runs them after that sample finishes. All EC methods, query methods, notification
callbacks and sampling run on one worker. This matters because captured Air
firmware holds `SMB0.SMTX` across several EC accesses: merely locking each byte
would let a direct driver corrupt an in-progress AML bank transaction. The VM
also injects 32 queries, processes at most eight per work invocation, and yields
between batches. The real emulated fixed SCI supplies one query notification;
its EC status/reply bytes are modeled. This is not an emulated physical EC GPE.

`tools/acpi_ec_smoke.py` passes two full clean EC lifetimes plus stop during
delayed setup. Each full lifetime verifies `_REG`, rejects a foreign caller
before I/O, completes the 15-word SBS sample, defers two queries injected during
the sample, and runs 35 query methods and 18 notification callbacks. Clean
disconnect executes a sleeping AML method with an EC write/read handshake on
the owning worker, then requires a detached handler and zero interpreter
allocations. A final separate lifetime injects a stalled sample: current validity
is cleared, the transport is poisoned, retries are refused and the last good
sample survives. The ensuing failed disconnect must halt with the namespace
retained. The `--disconnect-timeout` variant instead leaves `_REG` sleeping
beyond the five-second drain deadline and requires a bounded halt without
namespace reset. It uses seven consecutive one-second sleeps because the pinned
interpreter caps an individual Sleep at two seconds and the adapter bounds
AML loops to one second. Neither guard is changed by this test. Values such as 86% and
12 V are synthetic, not laptop readings.

The first EC reset exposed an ordering defect in relying solely on generic
namespace destruction: `_REG` disconnect tried to access names already removed.
The adapter now masks sources, drains scheduled work, removes the notification
handler and explicitly disconnects the region while its namespace still exists,
then resets uACPI. Delayed setup remains owned until completion, so stop cannot
miss a handler that is being installed. The failed capture is retained locally.

A subsequent fixture made disconnect perform EC I/O. It reproduced a second
teardown defect: the lifecycle caller was outside the owning worker and its
access was denied. Upstream logs `_REG` execution errors but can still return
success from handler removal; blindly freeing the namespace would hide that
failure. Teardown now closes external admission, drains existing work, removes
the notification handler on the lifecycle caller, and schedules region removal
on the EC worker. Notification removal stays outside that worker because the
pinned upstream implementation waits for all deferred work. A bounded drain
then verifies the fixture's disconnect handshake before namespace reset. A
poisoned controller fails verification and halts with state retained; it is
never silently reset to make the test pass.

The handshake is a synthetic assertion, not a general firmware completion
contract. Native work still needs an audited initialization owner/worker handoff,
real ECDT/resource binding, `_OSI` and Global Lock policy, live GPE/query ownership,
register permissions and hardware acceptance. Native `_REG` errors and worker
timeouts must remain visible, including where upstream returns success after
logging an AML error. Do not widen this VM fixture's port gate to boot a Mac.


## Namespace initialization on the EC worker

The explicit `acpiruntime start-fixture` mode requires a checksummed, uniquely
identified `SRVROS/INITTEST` DSDT in the Q35 table inventory. It uses the pinned
uACPI table-installation callback to replace only the interpreter's DSDT view.
Neither firmware tables nor the ordinary kernel are modified. Missing fixtures
are refused. Normal `start` retains the generated Q35 namespace and all previous
I/O restrictions. QEMU [appends user tables while its FADT references the generated
DSDT](https://github.com/qemu/qemu/blob/v10.0.0/hw/i386/acpi-build.c); merely passing
another DSDT on its command line does not replace that FADT reference.

`acpiruntime ec initialize` explicitly connects the modeled EC on its worker,
then calls `uacpi_namespace_initialize` on that same worker. The synthetic DSDT
and EC methods verify that EC `_REG` has connected before initialization, then
root `_INI` precedes `_SB._INI` and EC `_STA` precedes EC `_INI`. The latter methods read the controller and sleep before
publishing readiness. Sampling and query/Notify work then use the established
worker ownership. This does not finalize AML GPE users or initialize native
Mac firmware; it is an isolated initialization contract test.

`tools/acpi_ec_smoke.py --namespace-init` covers two clean initialized lifetimes,
query/SBS overlap, delayed setup teardown and poisoned disconnect containment.
The `--namespace-denied` variant deliberately keeps QEMU's generated DSDT. Its
HPET/hotplug initialization needs regions outside this adapter's allowlist. The
API still returns success after logging method failures, so the adapter checks
error/denial deltas and refuses to publish a ready service. The VM verifies
sampling refusal and clean teardown without widening permissions. The original
SCI/mutex/failure suite also verifies `start-fixture` rejects an absent fixture.

Before native use, namespace loading itself still needs an early EC binding and
initialization-owner policy, not just worker ownership after loading. Native
permissions, `_OSI`, Global Lock, GPE finalization and watchdog/rollback policy
remain unresolved. Do not infer a native initialization pass from this small
synthetic namespace.


## Early EC binding during namespace load (Q35 only)

`acpiruntime start-early` requires the explicit `SRVROS/INITTEST` fixture DSDT
and runs namespace loading on the same ordinary worker used for EC transactions.
Before loading, it installs the modeled EC handler at the namespace root, so the
synthetic EC region inherits it as the table is interpreted. The test SSDT's
module-level code reads `0x42`, writes `0x3c`, observes `_REG` still disconnected
and sleeps. Subsequent `ec initialize` verifies those results, runs `_REG` from
the handler's actual root scope, then initializes `_STA`/`_INI` on that worker.
Queries and complete SBS transactions keep their existing ownership after load.
The handler's scope and the EC device node are tracked separately for registration
and teardown. This root binding is a synthetic mechanism test, not a native
resource match or authorization to bind every laptop EC region.

The lifecycle caller waits at most five seconds for the load worker. It publishes
runtime readiness only after the call and logged error/denial checks succeed;
EC/battery readiness additionally requires the fixture's early-access and
initialization assertions. Access denial can return API success after an AML
error, so that path is refused and the partial namespace is cleaned up with
balanced region detach and no heap retained.

A deadline is not cancellation. The timeout fixture sleeps repeatedly in
module-level AML past the deadline and halts visibly with the live namespace/worker
retained. `start-early-fail` refuses SCI installation; the pinned interpreter
then invokes reset internally. Reset's wait-for-work callback recognizes that
it is running on the loading worker and halts before namespace teardown rather
than wait for itself or pretend cleanup completed. Failure halts flush the
console and disable interrupts before halting, so another worker cannot continue
behind a frozen monitor. The timeout test waits past the injected AML sleep and
checks both the absence of worker completion and the CPU interrupt flag.
This is failure containment,
not recovery from every interpreter failure. Native integration still needs a
lifecycle design that can handle interpreter-initiated reset safely.

Reproduce in disposable VMs, sequentially after `make acpi-runtime-vm-kernel`:

```sh
python3 tools/acpi_ec_smoke.py --early-load
python3 tools/acpi_ec_smoke.py --early-denied
python3 tools/acpi_ec_smoke.py --early-timeout
python3 tools/acpi_ec_smoke.py --early-fail
```

The positive mode also exercises two initialized lifetimes, SCI queries, SBS
samples, foreign-caller refusal, delayed setup teardown and poisoned-disconnect
containment. Existing `--namespace-init`, `--namespace-denied`,
`--disconnect-timeout` and `acpi_runtime_smoke.py` remain regression paths.
Ordinary hardware images do not link this adapter; native ECDT resource binding,
permissions, `_OSI`, Global Lock and GPE ownership still require separate work.
