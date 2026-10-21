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
The cached-service owner and EC worker are explicitly eligible in storage service
windows only after platform validation and mapping have completed and while the
runtime remains engaged. Other ACPI workers and the direct diagnostic retain
ordinary scheduling. During idle intervals the owner sleeps and checks the
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

Native capture `a5281d842c8b6160` / `6938813`, session
`session-00065e497cc34d56-00`, passed the bounded service test with these counters.
Epoch 2 recorded ten active deliveries (five handled/five unclaimed), and epoch 3
recorded two (one handled/one unclaimed). All setup and retirement counters were
zero; no callbacks remained in flight. Cached readers reached 48,768 calls with
an observed maximum of 18 microseconds, and both lifetimes retired cleanly.
This narrows the warning to active operation in this run; it does not establish
why those deliveries were unclaimed or guarantee behavior over a longer run.

The disposable `service-sci-unclaimed` VM fixture delivers software interrupts
through vector 41 without setting a hardware source. It verifies repeated
unclaimed deliveries remain visible in the counters although the generic IRQ
warning prints once. This tests accounting, not the cause of a native SCI.

The original ordinary-thread implementation was reproduced with a separate
thread holding the scheduler I/O scope at an EC transport status read. A 20 ms
hold passed; a 250 ms hold paused the worker long enough to expire its 100 ms
transport deadline. This was containment evidence, not coexistence acceptance.
The baseline serial log is retained in local scheduling-repair evidence.

### Audited storage eligibility

The service now opts its owner and worker into the existing I/O service windows.
The predicate requires the service owner, service mode and an engaged runtime.
It remains active through retirement. Platform preflight runs before engagement.
The explicit worker API leaves all existing ordinary-worker callers unchanged.

| Dependency | Audited behavior |
| --- | --- |
| Page tables and addresses | Native preflight completes HPTC mapping before engagement; runtime uses kernel/HHDM or existing mappings, never user memory |
| Allocation | Kernel heap operations mask IRQs; interpreter allocation accounting is also serialized |
| PCI configuration | CF8/CFC transactions have IRQ protection; ECAM uses established mappings |
| Console and boot log | Serialized kernel paths; deferred console service is already I/O eligible, with no dependency on an application presenter |
| Queues, semaphores and IRQ registry | IRQ-serialized; runtime waits depend on its eligible owner/worker, IRQs or timer |
| Platform callbacks | Bounded EC/SBS/AML and exact resource gates; no VFS, filesystem or storage callbacks |
| Worker retirement | Stopped publication and return to the thread's terminal state retain masked IRQs, preventing reclamation while its eligibility callback can still reference it; readiness scans skip retired threads before calling predicates |

This audit applies to the current CPU0 scheduler. SMP execution would require a
new synchronization audit. It does not permit arbitrary ACPI callbacks to run
during storage ownership. Transport, query and setup deadlines are unchanged.

The VM fixtures require progress **before** the I/O owner releases its scope.
They record cache reads, observed read latency, EC command progress and sample
generation. The interleave handshake releases the worker once the scope has
started, so it tests scheduler eligibility rather than forcing the worker to
wait until the scope ends.

| Fixture | Workload | Required result |
| --- | --- | --- |
| `service-io-short` | 20 ms scheduler scope | Five samples and clean lifecycle retirement |
| `service-io-long` | 250 ms scheduler scope | EC commands advance inside the scope; service passes |
| `service-io-sustained` | Five-second scheduler scope | EC commands and sample generation advance inside the scope |
| `service-usb-io` | Five seconds of disposable QEMU USB exFAT writes and flushes | Concurrent sample progress, service retirement, clean unmount, offline exact payload verification and clean exFAT |
| `service-filesystem` | 60 seconds of 256 KiB+37-byte file growth/shrink, rename, empty-file truncation, directory rename/deletion and flushes | At least ten generation advances inside the I/O scope, clean retirement/unmount, exact retained tree, clean offline exFAT and allocation graph |
| `service-filesystem-write-failure` | Reject the third raw write during replacement of a committed file; keep sampling in a five-second I/O scope | Existing file preserved, subsequent writes refused, service retires; dirty volume retained and offline recovery rehearsed on the disposable image |

The USB fixture creates its own image and synthetic workspace identity. It never
uses the laptop flash drive. VM evidence does not accept native storage/input
coexistence, native SCI behavior over sustained operation, or desktop enablement.
The v34 and v36 captures below add bounded native read and scratch-data
write/flush evidence. Ordinary filesystem metadata mutation on hardware still
needs a separate recovery contract: a contained native fault resets without
filesystem reentry and could interrupt an allocation or directory update. Do not
reuse that reset policy on a writable user workspace merely because the VM
workload passed. The next physical filesystem step needs a disposable target
whose failures cannot alter existing workspace files, retained pre-test bytes,
and verified post-return recovery before ordinary desktop integration.


## Native read-only USB overlap capture (v34)

Use `--battery-usb-read` with `--laptop-probe-volume` to stage the automatic
read-overlap test. It uses the separate native runtime kernel and cannot be
combined with the other capture modes or `--hold-on-failure`. Keep AC and USB
connected; no keypress or AC transition is requested.

The capture first saves a nonfinal checkpoint, unmounts the validated workspace
and revokes writes on every USB alias. The workspace remembers only the device
that was successfully validated and unmounted; remounting invalidates that
parked identity. The diagnostic reads the raw USB parent's first 4096 bytes
directly, bypassing block caches, and compares every subsequent read against
that baseline. It does not log the bytes or write during the runtime lifetime.

During the five-sample lifecycle test, the reader holds repeated 250 ms scheduler
I/O scopes. Cached status and USB command counts are captured inside those
scopes; passing requires observed sampling, real read commands and sample
generation advancement before exclusion ends. The final `battery-usb-read`
record is emitted only after both initialized lifetimes have retired. The
collector requires this proof and the existing complete service proof.

Normal return remounts the same workspace and saves the result; a contained
runtime fault resets without filesystem reentry. The VM tests require an
unchanged whole-disk hash and clean exFAT after both successful overlap and
injected deadline reset. Exact-stage native Q35 refusal must save a failed
result and return, and missing-workspace rehearsal must also reset.

This is read-only USB coexistence evidence. Native write/flush coexistence,
human input latency, sustained SCI behavior and ordinary desktop enablement
remain separate acceptance gates.

### Physical acceptance — October 20

MacBookAir7,2 release `b73c1ae3e4643dc9` / artifact `0cb9f2e`, session
`session-00065e4a0a43b75e-00`, passed the automatic read-overlap capture:

- 103,094 uncached 4096-byte USB reads matched the baseline; 153 occurred while
  sampling. Five sample-generation advances occurred inside 58 I/O scopes;
  the maximum observed scope duration was 289,851 microseconds.
- Five fresh samples, 106,806 cached reads (921 while sampling), maximum observed
  cached read time 18 microseconds. Immediate cancel, restart, in-flight stop
  and short stop timeout followed by clean join passed.
- Both initialized lifetimes restored retained enables and balanced attachments,
  with zero heap, errors and denied accesses at retirement. SCI recorded 12
  active deliveries: six handled/six unclaimed, none during setup/retirement or
  in flight afterward. The unclaimed source remains unresolved.
- Completed exit, new Ubuntu boot, consumed one-shot and installed hashes were
  verified. Offline exFAT was clean (178 directories, 858 files). The exit is
  saved before final unmount/reset; those host checks corroborate return.

Accepted payload and hashed evidence are preserved outside build cleanup.
This physically validates the scheduling repair for bounded USB reads with the
workspace unmounted and writes revoked. It does not accept native writes/flushes,
human input responsiveness, long-running SCI behavior, ordinary desktop service,
gauge accuracy or suspend/resume.

## Native scratch write/flush overlap capture (v36)

`--battery-usb-write` is a separate automatic laptop-probe profile. Version 35
is reserved by the Pro11,2 CPU-load proposal. This test exercises USB transport
writes and SYNCHRONIZE CACHE alongside the battery service; it does not exercise
filesystem metadata mutations during battery ownership.

Before entering the native runtime, the capture pauses logging and creates a
new `battery-io-<16 hex digits>.bin` file in the validated workspace. An existing
name is refused. It flushes the file, checks the filesystem's allocation graph,
and requires an exclusive single-cluster 4096-byte file. The extent must resolve
to a bounded, sector-aligned range on that same USB parent. The workspace is then
flushed/unmounted, and every public USB write alias remains revoked.

A private workspace capability retains only the selected range and transport
callbacks. Callers cannot supply an offset or length. Each cycle writes a
sequence-dependent 4096-byte pattern, flushes the device and reads it back
uncached. Any write, flush or readback failure poisons the capability and blocks
remount for this boot; a partial write cannot be retried as success. All aliases'
clean write-through cache entries are invalidated, including on failure. Normal
service retirement revokes the capability before remount and final logging.
A contained firmware fault resets without filesystem reentry; it can leave
incomplete scratch contents, but no filesystem metadata is written during the
runtime. Missing final evidence remains a failure.

The existing five-sample lifecycle test requires write cycles while sampling
and generation progress **inside** the storage scopes. Collection requires
complete service retirement, the scratch preparation/revocation records, at
least three transport commands per completed cycle, and valid overlap counts.
The Ubuntu collector also checks the final scratch file against the recorded
cycle count, saves a verified `battery-scratch.bin` copy, and records its hash.
Missing, replaced, malformed or mismatched payloads do not produce write proof.
Scratch files are retained for evidence; they are not user documents or deleted
automatically.

Host tests cover partition bounds/cache invalidation, exclusive extent refusal,
existing-file refusal, revoked aliases, partial-write/flush/read/mismatch failures
and blocked remount. Disposable 4 KiB and 128 KiB cluster images verify malformed
allocation refusal and unchanged metadata/other files after sector-boundary
scratch tears. VM success verifies the final pattern, a clean offline filesystem
and identical disk bytes everywhere outside the 4096-byte range. An injected
setup deadline exercises automatic reset after writes and checks the same disk
isolation. These remain VM/host evidence until a matching native run is collected.

Stage with the native kernel and rehearse the exact immutable payload using
`hardware_boot_smoke.py --probe-check`, including missing-workspace return,
before installation. Native Q35 refusal must save a failed report and reset.
Keep AC and USB connected; no human input is requested. Do not infer desktop
responsiveness, concurrent filesystem updates or sustained native service
acceptance from this bounded scratch test.

The first native v36 attempt, release `5792b6ece45b85dc` / source `e2d779b`,
**failed preparation before entering the battery runtime**. Session
`session-00065e4a6f889c64-00` records eight leaked allocated clusters, first 114,
from the SrvrOS allocation checker. No scratch capability was granted. The failed
exit was saved and the machine returned automatically; Ubuntu's read-only offline
exFAT check reports clean. A later independent scan confirmed eight actual orphan allocations; exfatprogs
1.3.2 ordinary read-only checking did not report them. Rehearsed orphan recovery
preserved every existing file and all orphan bytes, and the unchanged SrvrOS
checker now passes. The guard was correct. Native write/flush overlap remains
untested by that failed attempt; the accepted bounded read-overlap result was
unchanged. The subsequent successful retry is described below. Evidence and recovery details are in
the [Air unit record](machines/macbookair7-2.md).

After recovery, the same immutable release passed on Air7,2 in session
`session-00065e4c30fcddac-00`: 1,894 write/flush/readback cycles, 5,682 commands,
62 cycles during sampling, and three sample-generation advances inside I/O
scopes. Five fresh samples and 5,414 cached reads completed (574 during sampling,
maximum 18 microseconds). The service retired cleanly through cancellation,
restart and short stop-timeout coverage; the scratch capability was revoked.
The collector verified the final 4096-byte file against the reported sequence.
A completed exit, new Ubuntu boot, consumed one-shot and payload/recovery hashes
were verified. Both offline checks passed: fsck clean, and the SrvrOS checker
reported zero leaks/FAT mismatches/errors. This accepts bounded native transport
write/flush coexistence. It does not accept ordinary filesystem metadata updates,
human input or sustained desktop operation. Six active SCI deliveries remained
unclaimed; none occurred during setup/retirement or remained in flight.


### Filesystem recovery checks during the VM workload

The injected metadata-write failure preserved the committed payload and blocked
subsequent writes, but initially failed SrvrOS's post-recovery check. The checker
incorrectly required every bitmap-free cluster's FAT entry to be zero. The
[exFAT allocation contract (sections 5.1 and 7.1.5)](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#51-cluster-heap-sub-region)
uses the bitmap to determine ownership. [Linux's free-cluster implementation](https://github.com/torvalds/linux/blob/v6.17/fs/exfat/fatent.c)
clears bitmap bits without clearing old FAT links; exfatprogs recovery can leave
the same state.

The checker now validates the allocation bits of referenced clusters and still
rejects allocated orphan clusters, overlaps and invalid live chains. It ignores
FAT contents of unreferenced bitmap-free clusters. No repair or allocation change
is performed. `fat_bitmap_mismatches` now counts referenced clusters missing from
the bitmap. Regressions cover stale end markers, self-links and apparent links to
live files in free clusters, while missing live allocation bits and actual
orphans still refuse scratch admission. This correction is separate from the
eight real orphan clusters recovered on the Air: their rejection was correct.

## Isolated filesystem image trial (v38)

The native capture uses a 16 MiB exFAT filesystem inside the prepared file
`/work/srvros-work/battery-fs.img`. This is a disposable volume, not desktop Home.
`tools/battery_filesystem_image.py` creates it exclusively, with a diagnostic
identity and marker; it refuses an existing file. Retain each used image with its
run evidence before preparing another. The kernel refuses an image containing a
previous workload, so rebooting the same used fixture cannot count as a fresh run.

Admission checks the outer allocation graph, initialized file length, exclusive
ownership and physical contiguity (either NoFatChain or a contiguous FAT chain).
The outer workspace is flushed and unmounted, and public USB write aliases stay
revoked. A private block view maps only the proven file data range. Every callback
checks its identity, lease state, alignment and bounds; arithmetic overflow and
out-of-range requests are refused without transport access. An open or poisoned
lease prevents outer remount. No outer allocation or directory update occurs
during the battery lifetime. This does not authorize ordinary desktop filesystem
mutation while native EC/SCI ownership is active.

Filesystem traffic starts only after the service reaches ACTIVE; unsupported
platform refusal and setup deadlines therefore leave the prepared inner image
unchanged. The workload grows/shrinks and renames files, truncates an empty file, renames and
deletes directories, verifies contents and flushes for at least 60 seconds while
the cached service samples. Completion requires at least ten sample generations,
I/O progress during sampling and clean service retirement before unmounting and
revoking the image view. The automatic capture then remounts the outer workspace
to save logs and returns to Ubuntu. Contained native faults reset without trying
to repair the inner filesystem. Missing completion is failure; a failed transport
poisons the lease and may prevent saving the final log.

Host tests run the real filesystem and adapter against a disposable image,
interrupting every write, power-cut and flush boundary. They compare every byte
outside the image before and after, check the outer allocation graph and rehearse
inner recovery. The modeled runtime cases `service-isolated-filesystem` and
`service-isolated-auto-deadline` add USB, battery progress and setup-reset coverage.
`service-isolated-active-deadline` stalls the third sample after filesystem writes
start, then verifies reset, byte-for-byte outer isolation and recovery on a copy
of the retained interrupted image.
Exact-stage Q35 native refusal must also revoke the image, save an unsuccessful
report and reset. None of these fixtures accepts physical operation.

Collection copies the completed inner image, verifies its retained payload and
anchor, and checks the copied filesystem with both exfatprogs and the kernel
allocation checker. Preserve the original failed image before any offline repair.
The fixed prepared filename is single-use: collect/archive it before replacing it
for another run, and do not reinterpret older session logs using a newer image.

Air7,2 physically accepted this bounded workload in release `c473b63c44d32cd0`
(source `8822cba`), session `session-00065e4ddc1d41d5-00`: 25 cycles over
61.51 seconds, 18,754 commands and 21 sample-progress scopes. Collection verified
the final contents, clean inner/outer allocation graphs, clean service retirement
and image revocation, and return to Ubuntu. The 870 previous non-sidecar files
were unchanged; the outer inode sidecar was updated for session logging.
The maximum filesystem scope lasted 7.303 seconds, so this is not interactive
latency acceptance. SCI unclaimed deliveries, long-lived desktop/input coexistence
and gauge accuracy remain open. See the [unit record](machines/macbookair7-2.md).

### Operation boundaries for the follow-up trial

The follow-up diagnostic executes one filesystem operation per scheduler storage
scope, then releases exclusion and sleeps the reader before the next operation.
A cycle has 18 ordered steps: prepare directory/pattern, write large draft, verify
large draft, shrink, verify short draft, regrow, remove old payload, rename draft,
verify payload, write empty-file seed, truncate it, verify empty, create temporary
directory, rename it, verify old name absent, remove directory, verify removal,
and flush. Each individual mutation retains the existing filesystem transaction
and storage exclusion; no transaction is suspended halfway through a mutation.
The pattern and final payload size are unchanged from the accepted v38 workload.

Reports add `steps`, `max_step_us` and zero-based `slowest_step`. The collector
requires exactly 18 operations per completed cycle and one scope per operation;
legacy complete-cycle reports remain readable with their original predicates.
The minute-long trial finishes a cycle before retirement, never accepting a
half-written next payload. Wrong cycle identities and failed operations cannot
advance the continuation. The host suite checks clean stop/revocation at all 19
boundaries as well as the existing transport fault/cut matrix.

This bounds the work unit, not USB completion or battery-worker wall time. Large
file writes and verification still use existing atomic APIs. Measure their times
before selecting a smaller transfer/transaction design; do not interpret the
whole-cycle 7.303-second hardware measurement as a measured input stall. Ordinary
desktop startup and scheduler admission remain unchanged. Physical acceptance of
this follow-up is recorded below.

Air7,2 release `9eb84deab70d240c` / source `89edb98`, session
`session-00065e4e5218f777-00`, completed 25 cycles / 450 operations over 60.77
seconds with verified contents and clean retirement/revocation. Maximum scope
was 3,210,080 us and maximum operation 3,210,076 us (step 1, initial large draft
write), compared with the prior whole-cycle maximum 7,303,034 us. This accepts
the operation boundaries and isolated data-integrity test, not desktop latency.
The large atomic write remains the next measured bottleneck; retain existing
transaction/transport failure guarantees when reducing it. Outer/inner offline
checks passed and the accepted evidence is archived outside `build`.


The contiguous FAT batching follow-up (`39b152d`, PR #10) preserves FAT values,
copy-on-write publication and all flush barriers while grouping adjacent entries
into bounded writes. Native release `2ca59bc8678efc17`, session
`session-00065e4f02718396-00`, completed 34 cycles / 612 operations over 61.13
seconds with 16,954 commands and sampling progress in 17 scopes. Maximum scope
was 2,721,271 us, versus 3,210,080 us in the prior operation-stepped trial; the
slowest step remains initial large creation. Cached reads topped out at 18 us.
Payload, inner/outer allocation, unrelated files and clean retirement passed.
This is a single-run comparison and bounded isolated-workload acceptance;
desktop responsiveness, ordinary native battery startup and gauge accuracy
remain unaccepted. The 24 handled / 24 unclaimed active SCI deliveries persist.
