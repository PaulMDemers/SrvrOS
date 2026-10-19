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

## Live-runtime foundations (not linked or enabled)

`runtime/work_queue.c` implements the intrusive queue needed by the pinned
uACPI 7.0 deferred-work contract. Items are allocated before scheduling, so
enqueue/dequeue take constant time and allocate nothing. Taking an invocation
detaches and clears its item before the callback runs: the callback may requeue
or free that item. Completion touches only queue state, and idleness includes
in-flight callbacks as well as pending work. Duplicate scheduling, including
placing one pending item on another queue, is rejected without changing either
queue. GPE/notification work types and callback contexts remain attached to
their individual invocations.

This is a host-tested component, not an implementation of the kernel callbacks
yet. The future adapter must hold one IRQ-safe lock around every queue access,
run callbacks outside that lock on a CPU0 worker, and synchronize interrupt
producers before waiting for work completion. The worker must yield between
bounded batches so self-rescheduling or an event storm cannot monopolize it.
It must not free pending items or
silently drop work if its adapter violates the enqueue contract. Host checks
cover FIFO order, duplicate refusal, in-flight drain, reuse with a changed
context, and a callback that reschedules itself 999 times before freeing its
item; ASan/UBSan cover that final free. Run `make test-acpi-work`.

The queue is intentionally absent from native build inputs. The probe's denied
asynchronous-work callbacks remain unchanged. SCI registration, scheduler-backed
semaphores, thread identities, EC ownership and runtime initialization are still
required; see the [integration plan](../../../docs/macbook-air-laptop-services-2026-10-18.md#live-acpi-runtime-integration-plan).

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
The warning is not itself fatal; the stall location after it is unresolved.
Normal desktop profiles from v22 omit `srvros.aml=probe`. Keep native probe
execution explicit until this failure is diagnosed with stage-level evidence.
Host and synthetic-QEMU success do not establish completion on this firmware.

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
