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
containment and retain memory rather than claim successful retirement. An
unattended capture resets the machine after containment; attended diagnostics
retain the halt.

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

New unattended stages add `-auto-return` to the recipe and
`srvros.capture-failure=reboot` to the command line. Before risky battery work,
the capture pauses logging, writes a nonfinal checkpoint and unmounts USB.
Cached readers then run while sampling, followed by stop/restart. A normal
return remounts the same workspace, saves the final result and unmounts again,
including when the result is a failure. A contained live-runtime fault resets
without interpreter cleanup or new filesystem writes. Its pre-probe checkpoint
survives; no final report means failure, and the precise last fault may be
available only in serial output. This capture does not exercise concurrent USB
logging during the battery service.

Exact-stage Q35 must refuse native access, save that failed result and actually
reset. An injected VM setup deadline separately verifies the contained reset
path. Existing recipes without the suffix retain their original behavior.
`--hold-on-failure` deliberately stages an attended diagnostic; tell the user
before boot when their input or manual recovery is required. Guided AC
transitions remain attended by default.
Collection requires a fresh valid sample, readers observed during sampling, both
exact event restorations for every initialized lifetime, balanced retirement and
the final clean capture marker. Read latency is an observed wall-clock maximum
that can include preemption; it is not desktop/input latency acceptance.

Long-lived desktop operation, input/storage responsiveness under heavy I/O,
power cost, gauge calibration and other Macs are separate acceptance gates.
In particular, do not promote the worker into a storage exemption merely to
avoid a deadline during a workload: audit and measure the scheduling interaction.

## Physical checkpoint and next gate

On October 20, Air7,2 release `d9e6f4f75da4e871` (artifact `a24836a`) completed
the bounded service test in session `session-00065e4922d1be3d-00`. Five fresh
samples, 49,344 cached reads including 10,368 during sampling, and an observed
18-microsecond maximum read passed. Immediate cancellation, restart and in-flight
stop with a timed-out short wait followed by clean join passed. Both initialized
lifetimes restored the retained event enables and retired without reported
runtime errors, denied accesses or remaining heap. The matching completed exit,
consumed one-shot entry, new Ubuntu boot and clean exFAT were verified.

This capture parked USB throughout runtime and initialized in approximately four
seconds per lifetime. It also logged an unhandled SCI warning after the first
setup; the current log prints only the first occurrence, so one warning does not
establish a single interrupt. Before desktop enablement, quantify claimed and
unclaimed SCI deliveries across setup, active sampling and retirement, then
exercise bounded service operation alongside storage and cached readers.
Retain exact native access gates and existing deadlines. Do not mask the warning
by claiming every interrupt, and do not attribute the successful run solely to
the blocking reader fix when USB logging also changed.

The physical success path is accepted for this bounded test. Automatic return
after a contained fault remains VM-tested only. Gauge calibration, continuous
operation and userspace/UI integration are still pending.

### Interrupt accounting and storage exclusion

The runtime now emits `battery-sci` snapshots at ready, retirement and fault.
Each snapshot separates setup, active operation and retirement, with delivered,
handled, unclaimed and in-flight callback counts. First/last unclaimed timestamps
are monotonic microseconds. Counters reset with each runtime lifetime and carry
the service epoch (zero for the direct diagnostic). The IRQ bridge preserves
uACPI's claim result and performs no new event-register access or logging.
An interrupt that faults inside its callback remains counted as in-flight.
These counters cover the installed bridge, not interrupts arriving after removal.

The disposable `service-sci-unclaimed` VM fixture delivers software interrupts
through vector 41 without setting a hardware source. It verifies repeated
unclaimed deliveries remain visible in the counters although the generic IRQ
warning prints once. This tests accounting, not the cause of a native SCI.

Two VM fixtures interleave a separate thread at an EC transport status read.
That thread holds the real scheduler I/O scope, yields through its normal service
windows, and reads the battery cache. They do not access a disk or flash drive:

| Fixture | Exclusion | Expected behavior |
| --- | --- | --- |
| `service-io-short` | 20 ms | Sampling resumes; five samples and lifecycle retirement pass |
| `service-io-deadline` | 250 ms | EC transport expires; containment retains live state, no service pass |

Both behaviors were reproduced. The short hold measured 20.311 ms; the long hold
measured 250.234 ms and ended with transport deadline error 1. Cache reads worked
inside both scopes. The failing case is a regression test for truthful
containment, **not** acceptance of battery/storage coexistence. The current
scheduler excludes the ordinary EC worker during storage ownership; a paused
transaction still consumes its wall-clock deadline.

Desktop enablement therefore needs a separate scheduling design. Before granting
any I/O eligibility, audit the worker and owner across setup, sampling, queries
and retirement: allocator/mapping use, console logging, blocking dependencies
and all platform callbacks. The shared ACPI worker also serves other runtime
fixtures; do not make every caller eligible as a shortcut. Preserve deadlines
and storage isolation, then require progress and clean retirement under the
same long-hold reproducer followed by actual USB I/O and input testing.
