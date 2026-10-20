# Cached battery and AC service

The kernel service builds on the bounded Air7,2 battery diagnostic. It is linked
only into the separate full-runtime image. Ordinary desktop boot still returns
the disabled stub; there is no userspace syscall or battery widget yet.
See the [native runtime gate](native-ec-runtime.md) and
[machine acceptance record](machines/macbookair7-2.md) for physical evidence.

## Lifetime and ownership

`battery_service_start` admits an asynchronous lifecycle. Its return value means
the owner thread was created, not that firmware initialization succeeded.
The status progresses through STARTING to ACTIVE or UNAVAILABLE. Diagnostic and
service admission share one CPU0 owner; a second start cannot replace live state.
An increasing epoch distinguishes new lifetimes, each starting with an empty
cache. Refusal or thread-creation failure releases admission without publishing
invented readings.

The owner uses the existing EC/AML worker for namespace setup, battery reads,
query handling and disconnection. Native firmware/resource gates, Global Lock,
foreign-event preservation, transport deadlines and query budgets are unchanged.
The lifecycle owner and EC worker are ordinary kernel threads, not storage
service exemptions. During idle intervals the owner sleeps and checks the
independent query watchdog at 50 ms intervals.

`battery_service_stop` closes sample publication and requests retirement.
In-flight hardware work completes under its existing bounds; it is not freed or
reused early. Zero timeout only requests/polls. A timed-out wait returns false
with ownership retained; callers may wait again. Waiters follow their captured
epoch, so an already-retired lifetime is not confused with a new start. STOPPED
is published only after worker, SCI, interpreter, ACPI mode and retained event
restoration have completed. Unsafe live-runtime failures keep the existing
containment halt and retain memory rather than claim successful retirement.

## Cached readings

`battery_service_status` copies the snapshot with IRQs briefly masked and
calculates its age. It never starts EC/AML work, waits for a sample, allocates or
touches storage. This is a kernel contract; a later versioned userspace interface
should expose deliberate fields rather than copying internal structs wholesale.

Sampling starts two seconds after the previous operation completes. Firmware
notifications remain serialized through the worker; they do not recursively
start battery transactions or create an unbounded refresh queue. Each sample
brackets its battery words with AC reads. A differing AC value or invalid decoded
sample is rejected and preserves the last good reading. Transport/interpreter
failures still use containment; they are not converted into harmless cache misses.

| Flag | Meaning |
| --- | --- |
| VALID | A good observation exists in this epoch; it may be old |
| FRESH | ACTIVE, last attempted update accepted, and observation at most six seconds old |
| STALE | A valid observation exists but is no longer current |
| AC_PRESENT | AC state belonging to that cached observation |
| SAMPLING | The worker has begun a sample operation; readers still use the cache |

When no observation exists, age is `UINT64_MAX` with neither VALID nor FRESH. Clock reversal,
stopping, stopped state, rejected updates and expired age cannot appear fresh.
The last good numeric fields remain available with STALE. Consumers must inspect
flags instead of treating an old value or a zero-initialized percentage as live.
Sample generation, attempt/rejection counts, timestamps, capacity units and raw
SBS words remain available for diagnostics. SOC is explicitly relative to full
capacity; absolute SOC/calibration remains separate work.

## Validation and bounded capture

The pure host cache tests cover freshness boundaries, invalid sample retention,
incoherent AC, replayed generations, clock reversal, stop publication exclusion
and epoch reset. VM service fixtures exercise concurrent cached readers,
duplicate admission, immediate stop, repeated initialization/retirement,
stopping an executing sample, stale-to-fresh recovery, native platform refusal
and independent query-deadline containment.

The service reader fixture sleeps through a scheduler-backed timed wait.
`timer_wait_ticks` uses `HLT` without blocking its thread: using that between
reader batches kept the bootstrap thread runnable and consumed CPU time needed
by initialization. A CPU-bound VM setup fixture reproduces the deadline with
the old reader and completes with blocking waits, under the same five-second
setup bound. This models scheduling pressure, not native firmware contents.
Initialization logs its load/contract/region/initialize stages and elapsed time;
containment also reports the worker's progress. Native confirmation remains a
separate hardware gate.

The v33 laptop-probe profile provides a bounded automatic service test:

```sh
tools/host-env.sh make kernel initramfs acpi-battery-kernel
python3 tools/hardware_boot.py stage --laptop-probe-volume "$SRVROS_USB_UUID" --battery-service
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check
```

It runs
cached readers while sampling, stops and restarts the runtime, saves evidence to
the local prepared USB workspace, and returns only after complete retirement and
unmount. Exact-stage Q35 uses the native image and must refuse rather than
substitute a modeled battery. Existing v31/v32 recipes remain unchanged.
Collection requires a fresh valid sample, readers observed during sampling, both
exact event restorations for every initialized lifetime, balanced retirement and
the final clean capture marker. Read latency is an observed wall-clock maximum
that can include preemption; it is not desktop/input latency acceptance.

Long-lived desktop operation, input/storage responsiveness under heavy I/O,
power cost, gauge calibration and other Macs are separate acceptance gates.
In particular, do not promote the worker into a storage exemption merely to
avoid a deadline during a workload: audit and measure the scheduling interaction.
