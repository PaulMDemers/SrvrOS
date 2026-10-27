# Cached battery and AC service

The kernel service builds on the bounded Air7,2 battery diagnostic. It is linked
only into the separate full-runtime image. Default desktop boot uses the disabled
stub. The explicit v54 ordinary Home session selects that runtime and starts the
cached service under the accepted desktop owner; unsupported profiles remain
unavailable while apps continue. A versioned read-only userspace snapshot feeds
the desktop and System display. See [ordinary Home startup](battery-home-lifecycle.md#opt-in-ordinary-home-startup)
for opt-in staging, exit/restart and validation limits. Native held-Home captures
and AC transitions have Air7,2 acceptance; v54 ordinary startup is pending.
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
armed unattended capture can reset only while the workspace is unmounted and no
scheduler I/O scope is active at failure time. Otherwise it retains the halt.
This current [reset interlock](#mounted-workspace-study-and-fatal-reset-admission)
also applies to the earlier isolated-image recipes described below.

## Cached readings

`battery_service_status` copies the snapshot with IRQs briefly masked and
calculates its age. It never starts EC/AML work, waits for a sample, allocates or
touches storage. The v1 userspace snapshot exposes deliberate fields and units rather than
copying internal structs or firmware data wholesale.

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

## Isolated filesystem write profiling

The diagnostic image records the slowest completed filesystem operation using
`battery-filesystem-profile` and four `battery-filesystem-phase` records. Phase
0 is metadata/default (including verification reads), 1 allocation, 2 file data
writes and 3 freeing old clusters. Each phase counts raw image read/write/flush
callbacks, transfer bytes, total microseconds and the longest callback. The
operation's remaining time includes filesystem CPU work, cache invalidation and
scheduler activity outside those callbacks. Callback wall time itself includes
any admitted input or power-service execution; it is neither CPU blocking time
nor a measurement of user input latency.

The report is copied while the diagnostic owns storage exclusion, and printed
only after workload completion and lease revocation. It adds no I/O, changes no
flush/order/admission rules and does not enable battery service on ordinary
startup. Failed operations do not publish a successful profile. The collector
accepts older reports without profiling; a report marked `profile=1` must contain
one matching operation header and all four consistent phase records before the
service completion summary. Native profiling completed on Air7,2 in release `f99f6759a1ed8f68`: 3.738 of
4.703 seconds in the slowest operation were data-write callbacks, versus 0.025
seconds flushing. See the machine record for acceptance limits and integrity checks.

## Bounded USB command batching

The data path uses at most 16 KiB per SCSI command while keeping
each controller bulk request at 4 KiB. Command/data/status ordering follows
[USB BOT 1.0, sections 5.3 and 6.7](https://www.usb.org/sites/default/files/usbmassbulk_10.pdf).
Intermediate full-sized requests neither send a zero-length packet nor restart
the command. A short data response ends that phase; existing status/residue
validation and quarantine policy still apply. Transfer failure or overrun stops
immediately, with no retry or later command. Service checkpoints remain between
chunks as well as after commands. xHCI's pinned buffer, timeout and DMA ownership
are unchanged.

The block cache admits at most 16 KiB, still publishing only completed sectors
and invalidating an entire failed range. exFAT groups only contiguous file-data
clusters within the advertised raw-device limit; fragmented chains retain their
existing traversal. Final partial clusters are zero-padded. Flush barriers and
copy-on-write metadata ordering stay unchanged. The diagnostic image inherits
the bounded capability; host tests compare its persisted bytes against the 4 KiB
path, then exercise write/power-cut/flush failures and sector-aligned partial-write
prefixes. Air7,2 release `e1ba87c6133b2e9d` subsequently passed 45 isolated
cycles in 60.78 seconds with 18,116 commands, clean retirement and exact final
filesystem contents. Compared with the preceding profile, the slowest operation
fell from 4.70 to 2.18 seconds and data calls from 66 to 18. Outer file hashes,
fsck and allocation graph passed. This is isolated-workload acceptance, not a
desktop latency guarantee. Unclaimed SCI deliveries, desktop coexistence and
physical gauge accuracy remain open; ordinary desktop battery startup stays off.
See the [Air record](machines/macbookair7-2.md) for the exact source and evidence.

## Passive SCI observations

The explicit battery runtime keeps an eight-entry ring of completed SCI
callbacks. It observes values returned by the event reads uACPI already issues;
it adds no event/EC register reads, W1C acknowledgements, enable writes, or claim-policy
changes. Entry/exit timestamps use the existing monotonic clock, which reads the
read-only ACPI PM timer on this host. Normal reports are printed at lifecycle
snapshots, outside IRQ handling. The existing terminal containment path also
prints retained completed entries before halt/reset; it never resumes dispatch.
Each entry retains phase, callback result, entry/duration/gap timing, previous
callback result and query scheduling state before/after dispatch.

`valid` bits 0/1 indicate first fixed-status/enable reads; bits 2/3 indicate first
EC GPE-byte status/enable reads. Missing reads are unknown, even when the printed
value is zero. EC raw values and filtered values returned to uACPI are both
retained. The EC mask in the header identifies the relevant bit within that
byte. First reads are retained across later handler clears/disables. These are
sequential observations, not an atomic event snapshot; they cover only the fixed
registers and EC GPE byte, not every potential shared interrupt source. A
zero enabled/status intersection does not prove a spurious interrupt.

The ring records its retained/overwritten counts and sequence numbers, so missing
older entries are explicit. Adjacent handled/unclaimed callbacks and short gaps
may support a queued-delivery hypothesis, but do not by themselves establish
controller or firmware causality. The existing per-phase totals and uACPI return
value remain authoritative. Host fixtures cover address/width gating, first-read
retention, unknown values, filtering, timing and ring overflow; the Q35 injected
unclaimed case checks actual dispatcher output without claiming native evidence.

Air7,2 release `57ac1e9954e445e9` accepts the observer during the isolated
filesystem workload and clean service retirement. Of ten retained callbacks,
five were unclaimed: all had complete reads, EC event bit clear and disabled,
no fixed-status/enable overlap, and pending query work. They followed handled
callbacks by 0.536–0.741 seconds. The full counters still recorded 24 handled
and 24 unclaimed deliveries; the ring overwrote 38 older callbacks. This evidence
narrows callback conditions but does not prove queued-controller delivery or
exclude another source. No claim, enable or acknowledgement policy changed.
See the [Air record](machines/macbookair7-2.md) for integrity checks and limits.

### Interrupt-controller snapshots

The next diagnostic adds paired `battery-sci-controller` records to each retained
callback. `snapshot`, `epoch` and `seq` identify the associated event; `point` is
`before` or `after` the uACPI callback. Both points precede the generic IRQ
end-of-interrupt acknowledgement. These records cannot describe post-EOI state.
The callback duration now includes the two bounded controller reads.

Availability bits are LAPIC=1, I/O APIC=2 and PIC=4. An unavailable field is unknown,
not evidence of a clear bit. `lapic_bits` encodes the selected vector's ISR=1,
IRR=2 and TMR=4; `lint0` retains its raw LVT configuration. The raw owned route
low/high words include the I/O APIC's delivery, remote-IRR, trigger and mask bits.
`pic_masks` combines master/slave mask bytes, master in the low byte; it does not
read PIC pending/in-service state. No PIC command, event acknowledgement, EOI,
mask or routing policy is changed. Reading the I/O APIC uses its serialized
register selector, without writing route fields.

The xAPIC register offsets and 16-byte bank spacing follow the
[Intel System Programming Guide, local APIC register map](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf).
Only the existing mapped xAPIC is supported; this is not x2APIC enablement.
ISR, IRR, TMR, LINT0, route and PIC masks are read sequentially. Hardware can
change between reads, so the pair is a bounded observation rather than an atomic
snapshot or proof of interrupt origin. Software `int` injection in Q35 exercises
dispatch and record validation but does not model a pending hardware APIC vector.
Native controller acceptance is limited to the matching physical run recorded
below. Existing unclaimed interrupts retain their original return value.

Air7,2 release `7357a2eee058622e` accepts this bounded controller capture during
42 isolated-filesystem cycles and two clean service lifetimes. All ten retained
callbacks had ISR/TMR set and IRR clear; the I/O APIC remote-IRR bit was set at
both observation points. Five unclaimed callbacks again had no enabled event
in the selected fixed/EC reads and pending query work. The firmware MADT override
explicitly specifies active-high/level for GSI 9, matching the observed route.
Neither snapshot occurs after generic EOI, so delivery cause and acknowledgement
behavior remain unresolved. No IRQ claim or configuration policy changed. See
the [Air acceptance record](machines/macbookair7-2.md) for counts, integrity checks
and remaining physical acceptance boundaries.

### Post-EOI observation

The diagnostic now retains a third controller row, `point=post_eoi`, and a
`battery-sci-return` row for each completed callback. A token-owned optional IRQ
completion observer runs after the existing generic PIC/LAPIC EOI sequence,
with interrupts masked, before returning through the ISR frame. It cannot alter
the handler claim. The dispatcher preserves the handler-to-EOI ordering; it does
not add acknowledgement, route or event-register writes.

`observed=1` distinguishes a completed observation from unknown state. The
entry-to-snapshot `elapsed_us` includes callback execution, other shared handlers,
any existing dispatcher logging, EOI and the third controller read. It is **not**
an `iret` timestamp, query-worker execution time, or input latency. `query` is
the scheduling flag at this observation. Register reads remain sequential;
IRR/delivery state can change immediately afterward. A clear bit at this point
does not exclude later delivery or prove that the interrupt line is deasserted.

Registration mutation and reentrant shared dispatch are rejected while callbacks
or completion are outstanding. Only handlers actually dispatched receive one
completion. Wrong-line or duplicate completion cannot call the observer, and
successful removal excludes later calls. An interrupted completion is explicitly
unknown; normal ready/retired report validation requires complete observations,
while a terminal fault record may retain an unfinished observation. No observer
is registered by ordinary desktop startup.

Air7,2 release `71b16a6e59709895` (source `2c90dfe`, PR #21) accepts this
three-point diagnostic during the isolated filesystem workload: 45 cycles / 810
operations, two clean service lifetimes and verified inner/outer filesystem
integrity. All ten retained post-EOI observations show LAPIC ISR/IRR clear and
I/O APIC remote-IRR clear (`lapic_bits=4`, route low `0x8029`). The existing EOI
therefore takes effect at these sample points. All five retained unclaimed
callbacks still follow handled callbacks while query work is pending; their
selected event status/enable pairs contain no enabled event. This does not
identify the later delivery's cause or justify changing the interrupt claim.

The pending flag spans queued and executing query work, including AML evaluation
and rescheduling. Next correlate those stages and their timing with the SCI ring
using bounded passive observations, without additional EC/event-register reads
or policy changes. The [Air record](machines/macbookair7-2.md) contains exact
counts, timing limits and preserved recovery evidence. Ordinary desktop battery
startup, interactive input coexistence and physical gauge accuracy remain
unaccepted by this automatic trial.

### Query context at SCI entry

Each new `battery-sci-query` row pairs with one event by snapshot/epoch/sequence.
It copies the diagnostic worker's passive state at SCI callback entry, before
uACPI dispatch. `valid=1` distinguishes a recorded state from missing evidence.
`cycle` counts admitted query batches within the runtime lifetime; zero means
none has started. `transfers` counts attempted query commands in that batch,
including attempts across worker rescheduling. Counters saturate rather than wrap.

| Phase | Meaning at the sampling point |
| --- | --- |
| 0 | Idle; any nonzero cycle describes the most recent completed batch |
| 1 | Queued or requeued; the same batch retains its cycle and transfer count |
| 2 | Query transport; returned code is unknown until the transfer succeeds |
| 3 | Nonzero query code dispatch/logging and AML evaluation |
| 4 | Query budget/status checks between transfers |
| 5 | GPE resume boundary, with interrupts masked |

`age_us` is wall time since batch admission; `phase_age_us` is wall time since
entry to this phase. They include scheduler delay, hardware waits and any logging;
they are not CPU cost or IRQ latency. On initial idle state both are zero. The
last successful command's `code` has an independent `code_valid` bit: a valid
zero is an empty response, while unknown is zero with validity clear. Entering
a new transfer clears code validity. A new batch resets its transfer count/code.

Updates and snapshots are serialized on CPU0 against IRQ entry. The ring stores
copies, so later worker progress cannot rewrite an older interrupt's context.
No transport/event reads, ownership, deadlines or acknowledgement decisions are
added. A phase identifies where the worker was interrupted, not why an interrupt
was delivered; bounded retention can omit earlier work. The VM unclaimed-SCI
fixture injects a software interrupt at a query-command boundary to exercise
this association. It cannot establish a physical interrupt cause.

Air7,2 release `df04f96a16d0f850` (PR #23, source `68aac71`) accepts the query
context observations with the isolated workload and clean retirement. All five
retained unclaimed callbacks observed a new queued batch just 5–6 microseconds
old, with no transfer yet. The event was already disabled/clear for the EC mask;
post-EOI controller state again cleared normally. This points toward the worker's
resume-then-recheck ordering, rather than an interrupt during query transport or
AML, in these retained samples. It is correlation, not proof of the source.

### Check pending work before GPE resume

At the end of a query batch, with the owned GPE still suspended and IRQ dispatch
excluded, check pending status first. Known pending work is queued while the
source stays disabled. Otherwise resume the GPE and retain the existing pending
check after resume. This avoids briefly enabling a source already known pending,
which can latch an interrupt before the subsequent queue operation masks and
clears it. It preserves the post-resume check for arrivals after the first read.
An arrival in that window can still latch a delivery; claims are never forced to
handled merely because this race is possible.

`ec_query_rearm.h` defines the ordering through the existing pending/resume/queue
callbacks. The runtime retains its EC/GPE access gates, raw GPE handler, query
budgets, worker ownership, stop and retirement behavior. The host fixture models
a latched interrupt surviving source clear, known GPE or EC pending work, a quiet
source, and arrivals before/during/after resume and after the final check.

Air7,2 release `54dce7fb0a41b718` (source `b7bd60c`, merged PR #24) accepts this
ordering for the bounded native workload. Full counters across both lifetimes
were **24 handled / zero unclaimed**, versus 32 / 32 in the preceding capture.
All nine retained entries found an idle query worker and normal post-EOI state.
The isolated filesystem completed 42 cycles / 756 operations, both lifetimes
retired cleanly, and exact image contents, outer allocation and unrelated-file
hashes passed. This supports the ordering fix for the observed duplicate pattern;
it does not eliminate the documented late-arrival window. Ordinary desktop
battery startup remains disabled.

## Desktop and input coexistence contract

The automatic filesystem captures exercise storage and cached readers, but do
not run the compositor concurrently. The original `main.c` battery and desktop
capture branches ran sequentially, so combining flags did not demonstrate
overlap. The v40 RAM capture implements the first stage of this contract:

1. Checkpoint and park the outer workspace before native runtime entry. Launch
   the desktop with RAM-backed state and a visible test banner; omit Home access,
   periodic workspace logging and screenshot writes while storage is parked.
   Preserve the accepted ordinary desktop and its startup behavior.
2. Start the cached battery service once, wait for ACTIVE under a deadline, and
   run the compositor alongside it. First validate this without storage load;
   then add the existing isolated disposable filesystem capability. Keep generic
   USB write aliases revoked and retain ownership until every operation completes.
3. Record compositor progress and frame/event timing alongside battery generations,
   SCI totals and lifecycle state. Synthetic VM input must assert delivery and
   application state, not only a polling counter. Native automatic progress is
   not acceptance of physical typing, gestures or end-to-end input latency.
4. Stop desktop activity and storage work, retire the battery service, then revoke
   the image capability. Only after clean retirement may the workspace remount
   and persist final logs. Exercise process failure, unavailable service, deadline
   and stop-timeout paths in VMs; preserve live-state containment and automatic
   return without unsafe filesystem writes after a runtime fault.
5. Rehearse the exact native stage, then run an automatic physical overlap trial.
   Require matched release/session, overlapping activity, complete lifecycle,
   final filesystem checks and Ubuntu recovery. Follow with a separately announced
   attended keyboard/trackpad trial before accepting interactive responsiveness.

Do not add a persistent battery widget or enable the ordinary desktop service
until this lifecycle and coexistence contract is tested. A versioned cached
userspace read interface can follow; it must expose freshness/unavailability and
must never trigger EC/AML work from the compositor. Gauge accuracy, AC transition
revalidation and suspend/resume remain separate acceptance items.

### RAM compositor capture (v40)

`hardware_boot.py stage --laptop-probe-volume <this-host-uuid>
--battery-desktop` selects the separate native runtime, automatic-return recipe
and RAM-only `deskd --coexist-capture`. v39 remains reserved for the Pro agent's
Wi-Fi probe. Ordinary desktop startup and its disabled battery stub are unchanged.
This first step uses the existing independent typing-study client and legacy text
input path; it is not yet the full Text application or a filesystem load test.

The launcher checkpoints and unmounts the workspace before starting the service.
It requires a fresh active sample, then launches the compositor under an independent
90-second deadline. The compositor requires its client to appear within five
seconds and runs a 60-second sequence of generated font-toggle messages. Each
message carries a serial that must return in a client surface commit and reach a
successful screen blit. It requires at least 55 completed presentations, tracks
loop gaps, and retires its client. Generated events are explicitly distinct from
physical keys. This mode opens no app menus, Home, preferences or screenshot jobs;
the native keyboard/mouse can still feed the study, but no input is required.

The owner requires successful process exit, at least ten additional battery
sample generations and cached reads during sampling. It reaps the compositor and
ensures no live study child remains before stopping the service. Failure to retire
retains the existing containment/reset path. Only after retirement does main
remount the workspace, save the final log and return to Ubuntu. Missing final
logs remain incomplete evidence. The collector's `battery_desktop` field checks
matching epochs, overlapping time intervals, completed client presentations,
process/lifecycle success and SCI records. The saved kernel start record is
printed after teardown to avoid interleaving with startup writers; its timestamp
still identifies admission. Loop gaps include client startup and are software
scheduling measurements, not physical input-to-display latency.

Local validation uses `tools/battery_desktop_smoke.py`: normal operation with
injected `Abx`/Backspace, missing executable, owner deadline, early UI exit and
native platform refusal. A replacement compositor must start afterward. An
additional active-runtime deadline case requires automatic containment/reset,
without a successful completion report or unsafe live-runtime cleanup. The first
VM exposed a sample timeout during large pixel-cache cleanup; `process_munmap`
now removes mappings in one table pass rather than searching twice per page.
A bulk/split/unrelated-mapping regression runs in `posixdemo`. The existing EC
100 ms transport deadline and fault containment remain unchanged.

### Accepted Air7,2 RAM overlap (2026-10-21)

Native release `ae2851607a9bf0c1` (source `1df9a24`, PR #26), session
`session-00065e5c8e37f13c-00`, passed after exact-stage VM rehearsal. The study
ran 60.019 seconds, presented all 60 generated changes and 61 frames, and
recorded a maximum loop gap of 30,009 microseconds. Sampling advanced from
generation 1 to 23 with 3,004 cached reads, 707 during sampling. Clean process
exit and service retirement, balanced attachment, zero rejection/errors/denials
and 23 handled / zero unclaimed SCI interrupts were recorded. The
30 ms maximum loop gap is software scheduling evidence, not physical latency.

Collection, old-file hashes (including the unchanged used filesystem image),
offline exFAT/allocation, Ubuntu return/default and recovery verification passed.
The machine record owns the exact installed/evidence state. This closes the first
RAM study overlap step. Rich Text keyboard-event coverage, isolated filesystem
load alongside the compositor and attended physical input acceptance remain.
The ordinary desktop battery service is still disabled.

### Text keyboard-event overlap (monitor/VM only)

The native v40 recipe remains the accepted typing study. A separate
`acpiruntime battery-desktop-text` monitor command starts the same bounded owner
with `deskd --coexist-text` and the real Text app. Opt-in v41 `--battery-text`
selects the same owner for native capture. The Text diagnostic uses its normal editor, history,
clipboard and surface publication, with its file worker disabled and no initial
file loaded. The compositor provides focus and the in-memory clipboard without
activating the global app launcher, preferences, Home or screenshot writes.
Ordinary Text and desktop startup retain their existing behavior.

Five twelve-event sequences exercise Ctrl-A, Backspace, shifted insertion,
Left/Delete, End, copy and paste. Each sequence must leave `AA`, with the caret
and anchor at byte two and no clipboard transfer pending. Every generated event
must be acknowledged by a client surface commit and presented before the next.
The sixty events finish before the end of the one-minute overlap interval,
leaving time for VM keyboard injection. Diagnostic snapshots are printed only
at shutdown, and only in this explicitly selected RAM document mode.

The injected sequence exercises casing, selection replacement, navigation,
Backspace/Delete and clipboard shortcuts through the OS keyboard-event stream.
Its final document must be `ZbZb`. Ordered keyboard events may share a frame;
`keyboard_presented` records the last acknowledged sequence prefix, not one frame
per physical event. The final document, caret/anchor, matching input serial and
successful presentation are required together. These are VM observations, not
physical input latency or trackpad acceptance.

Run sequentially after building matching ordinary/native/VM kernels and userspace:

```sh
tools/host-env.sh python3 tools/battery_desktop_smoke.py --text --no-input
tools/host-env.sh python3 tools/battery_desktop_smoke.py --text --case normal
tools/host-env.sh python3 tools/battery_desktop_smoke.py --text --case missing
tools/host-env.sh python3 tools/battery_desktop_smoke.py --text --case timeout
tools/host-env.sh python3 tools/battery_desktop_smoke.py --text --case early-exit
tools/host-env.sh python3 tools/battery_desktop_smoke.py --text --case refusal
tools/host-env.sh python3 tools/battery_desktop_smoke.py --text --case runtime-fault
```

Normal and handled-failure cases require a replacement compositor to start after
cleanup. The runtime-fault fixture requires automatic reset without a false
retirement/success report. The harness drains serial output between injected key
bursts: pausing diagnostic reads while typing produced an incomplete first run,
although its final document was visible. That failed evidence is retained; the
repeat passed without guest deadline changes. `check_text_capture` rejects wrong
or missing editing checkpoints, unfinished clipboard operations, unmatched final
input/presentation serials, file-worker activation and incomplete lifecycle proof.
The existing v40 collector continues to require the original study evidence.

The seven Text cases, original study, host core suite and ordinary desktop-app
regression passed with implementation `9ee553a`. The apps harness now selects
fixture paths explicitly after the move to Home and targets the current Places
layout. It asserts maximize/restore transitions and pairs child shutdown with
the parent-reported PID/status, avoiding an interleaved child console line.
The separate v41 native trial is accepted for the bounded RAM script below.


### Native automatic Text capture (v41)

Build `acpi-battery-kernel` and the matching `build/initramfs.tar`, then stage:

```sh
python3 tools/hardware_boot.py stage --laptop-probe-volume "$SRVROS_USB_UUID" --battery-text
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check
```

The distinct `srvros.capture=battery-text` recipe requires automatic return and
cannot be combined with another capture or `--hold-on-failure`. The exact-stage
Q35 run checks the native platform refusal, saved report, unmount and return; it
does not establish native runtime or editing acceptance. Preserve the ordinary
desktop recovery and follow the setup guide's install/run-record sequence.

No typing or clicking is required. Leave input alone for the one-minute trial,
with AC and the workspace USB connected. The outer workspace is parked during
runtime; Text has a RAM document and no file worker. Clean completion or handled
failure saves diagnostics and returns automatically. Existing unsafe-retirement
containment can reset without a final report; missing evidence remains failure.

Collection reports `battery_text` only for the exact recipe and one complete
clean-return marker, all sixty presentations, five exact `AA` checkpoints, the
final `AA` document/caret, zero extra keyboard events, continued battery sampling
and clean app/service retirement. Malformed or missing proof adds an attention
item. The v40 study remains separately selectable and separately checked. This
automatic result does not establish physical input latency, simultaneous storage
load, gauge accuracy or ordinary desktop battery support.


Air7,2 release `2fd49386c4ecf0d9` (source `01e64a9`, PR #29) passed the native
v41 trial: all 60 edits presented, five exact `AA` checkpoints and final `AA`,
zero extra keyboard events, 60.018 seconds of overlap, battery generation 1→23
and clean process/service retirement. SCI recorded 23 handled / zero unclaimed
interrupts. Collection, all prior data-file hashes, offline exFAT/allocation,
consumed one-shot and recovery checks passed. This accepts only this bounded
RAM workload on the captured Air; simultaneous filesystem load, physical input
latency and gauge accuracy remain open. See the [unit record](machines/macbookair7-2.md).

### Text with isolated filesystem load (v42)

The explicit `--battery-load` profile combines the RAM Text script with the
existing exclusive 16 MiB image workload. It uses `srvros.capture=battery-load`
and the native diagnostic kernel. Ordinary desktop startup is unchanged. The
outer workspace is unmounted and write-revoked before runtime entry; only the
leased `battery-fs.img` extent is writable. Text's own file worker stays disabled.

The diagnostic owner alternates one image operation with service observation and
process polling, releasing I/O exclusion between operations. Existing audited
userspace paths and the battery service may run during I/O; this does not widen
scheduler eligibility. After Text exits, the owner finishes the current file
cycle, with a 90-second total workload deadline. It reaps owned processes before
retiring the service and closing the image lease. Handled failure uses the same
bounded cleanup; unsafe runtime retirement uses the existing contained reset.

`battery-text-load` proof binds the owner epoch/start to the Text completion and
requires at least 59 seconds of measured common filesystem/desktop interval.
It also requires the existing Text document/presentation proof, complete image
cycles, service progress inside I/O scopes, per-operation timing, clean revocation
and exact offline payload contents. Collection records `battery_load` only after
verifying the saved image, inner filesystem and allocation graph. Outer file hashes
and offline outer allocation checks remain part of native acceptance. Max loop or
I/O durations include scheduling and are not physical input latency.

Run the disposable cases sequentially after matching native/VM builds:

```sh
tools/host-env.sh python3 tools/battery_desktop_smoke.py --load --no-input
tools/host-env.sh python3 tools/battery_desktop_smoke.py --load --case normal
tools/host-env.sh python3 tools/battery_desktop_smoke.py --load --case missing
tools/host-env.sh python3 tools/battery_desktop_smoke.py --load --case timeout
tools/host-env.sh python3 tools/battery_desktop_smoke.py --load --case early-exit
tools/host-env.sh python3 tools/battery_desktop_smoke.py --load --case refusal
tools/host-env.sh python3 tools/battery_desktop_smoke.py --load --case load-abort
tools/host-env.sh python3 tools/battery_desktop_smoke.py --load --case runtime-fault
```

`load-abort` deliberately stops after three successful filesystem operations;
it tests partial-cycle owner cleanup, not a simulated USB transport failure.
Interrupted VM images are retained, and only a separate disposable copy may be
recovered for offline inspection. Every byte outside the image extent must remain
unchanged. Clean-return cases also require a replacement compositor to start.

Before native testing, archive and verify the previous used fixture outside
`build`, provision a fresh image, snapshot all file hashes and run offline
`fsck.exfat -n` plus `check_exfat_allocation.py --uuid "$SRVROS_USB_UUID"
--battery-image`. Do not replay a used image as fresh. Then stage and rehearse:

```sh
python3 tools/hardware_boot.py stage --laptop-probe-volume "$SRVROS_USB_UUID" --battery-load
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check
```

The exact native payload must refuse unsupported Q35, leave the image unchanged,
save, unmount and return. For the physical trial, retain accepted desktop recovery,
save the durable run record, leave keys/trackpad alone and keep AC/USB connected.
No human input is required. Completion or handled failure returns automatically;
a contained reset without complete proof remains incomplete. This profile does not
accept general desktop storage access, physical input latency, battery gauge
accuracy or another model's firmware.


Air7,2 release `c3746988ed5c0d5b` (source `421a69b`, merged PR #31) passed the
native v42 trial. All 60 generated edits and five exact document checkpoints
completed during 60.017 seconds of common desktop/filesystem activity. The image
workload completed 45 cycles / 810 operations / 18,120 USB commands while battery
generation advanced 1→25. Maximum desktop loop gap was 29,998 microseconds;
the slowest filesystem operation took 4,551,918 microseconds including admitted
services. Neither metric measures physical input latency. Clean app/service/lease
retirement, 25 handled / zero unclaimed SCI interrupts, exact inner contents and
inner/outer allocation checks passed. All 906 other prior data files were unchanged;
only the intended image and workspace metadata changed, with three new logs.
The one-shot was consumed and Ubuntu recovery verified. This closes bounded
combined-load acceptance on this unit; ordinary desktop battery remains off.
See the [unit record](machines/macbookair7-2.md) for retained evidence and limits.


### Application file worker and cached presentation (v43)

`--battery-app-files` uses a fresh exclusive image and the real Text file worker.
Every twelfth generated edit saves `AA`, stats and renames the document through
`deskfs`, clears the editor, then reopens and verifies it. Five cycles leave only
`text-1.txt` through `text-5.txt` in the fixture alongside its original anchors.
The image also contains the normal `.srvros/meta` sidecar; offline verification
requires exactly the expected paths, unique inode identities, modes and link counts.
Normal open resets the caret/anchor to zero; the capture verifies that behavior.
It acknowledges the generated edit only after reopening, while still rendering
intermediate progress. This tests application syscalls and asynchronous worker
IPC; it does not mount the ordinary Home volume alongside the runtime.

The diagnostic owner retains descendant identities, including Text's worker,
until they exit before stopping the service and revoking the image. Existing
workload and retirement deadlines remain bounded. VM failure paths must prove
cleanup and a replacement compositor; unsafe native retirement retains contained
reset behavior. Missing final evidence is never acceptance.

`SYS_BATTERY_STATUS` returns version 1 of `srv_battery_status`: a bounded copy of
the cache, with explicit state, validity, freshness, sample generation/age, AC,
SOC and unit-bearing battery values. It performs no hardware work. The ordinary
kernel's disabled provider returns unavailable data without activating EC/AML.
The shared battery presentation helper labels disabled, starting, stopping,
stale and unavailable states. Only a current valid SOC becomes a live percentage;
charging/discharging also requires valid signed current. The compositor polls
once per second and updates its status label only when the text changes. System’s
Sensors page uses the same cached view; neither UI starts the service.

This is the first part of desktop battery integration. Explicit user start/stop,
ordinary workspace coexistence and attended input/AC acceptance are still pending;
automatic battery startup stays disabled. Do not infer physical acceptance from
VM screenshots. Rehearse the exact native payload before installing v43.


Air7,2 accepted the bounded v43 trial in release `27c39e5404ab573b`, source
`977a554`: 60 presented edits, five complete real-worker file cycles, 61 fresh
cached UI reads, battery generation 1→23, three retired descendants and clean
service/image teardown. SCI handled 23 deliveries with none unclaimed. Exact
image contents/metadata, inner/outer allocation, other-file preservation and
Ubuntu return passed. This accepts the isolated application path and cached view;
it does not enable ordinary Home access, lifecycle UI or automatic startup.


### Scoped asynchronous controls (in development)

`SYS_BATTERY_CONTROL` is separate from cached status. Query returns a v1 control
snapshot; Start/Stop enqueue one command and return immediately without allocation,
firmware work or waiting. Admission requires the opt-in desktop process group and
the observed battery epoch. Duplicate pending commands are busy; stale epochs and
invalid lifecycle transitions are rejected. A successful enqueue means requested,
not running or stopped. The kernel session owner dispatches commands to the existing
service outside syscall/storage work. Starting can be cancelled by Stop; STOPPING
retains ownership until the existing worker/SCI/AML teardown finishes.

System's Sensors buttons use the shared state helper and remain disabled without
a granted scope. Pending/starting/stopping/failure states remain visible, and
control state refreshes even when other System tables are paused. Scope revocation
precedes client retirement; late commands are rejected and unfinished runtime work
is retired before the session owner returns. Closing a window does not free live
firmware state. The outer workspace remains parked throughout this first study.

`acpiruntime battery-controls` drives two lifetimes through the real System action,
syscall and owner paths. `acpiruntime battery-controls-manual` exposes the same
buttons for an attended bounded session; close System to end it. These commands
require the separate runtime kernel and retain native platform gates. They do not
enable automatic startup or ordinary workspace coexistence. VM evidence and native
acceptance are recorded separately in the machine record.

The v44 `hardware_boot.py stage --laptop-probe-volume <local-volume-id>
--battery-controls` recipe runs the automatic study through the exact native
kernel and matched userspace. Automatic return is mandatory; this profile takes
no physical input. It parks the outer workspace, starts and stops twice through
System, revokes control, retires both clients and the service, then remounts only
to save the final logs and returns to Ubuntu. A contained runtime fault can reset
without a final report; missing proof is incomplete evidence. Collection requires
four accepted/dispatched requests, two fresh sample generations in each lifetime,
two stopped states, balanced retirement, revoked scope and the exact return marker.
The former disposable filesystem image is neither opened nor rewritten by v44.

Local validation uses `tools/battery_controls_smoke.py` with `--case normal`,
`manual`, `early-exit`, `refusal` and `runtime-fault`. Normal repeats the whole
session in one VM to reject stale command/epoch reuse. Manual uses actual pointer
clicks, then closes System while active; the owner must retire the live service.
The runtime-fault case leaves monitoring running until the third-sample injected
deadline and requires automatic reset without a claimed clean retirement.

Air7,2 accepted the bounded automatic v44 path in release `87524e9fe7788772`,
source `134f0d6`: four commands, two fresh lifetimes and stopped states, two retired
clients, clean service teardown and revoked scope. SCI handled four deliveries
without an unclaimed interrupt. All 913 prior data files stayed identical; only
workspace metadata and three new logs changed. Offline allocation and Ubuntu
return/recovery checks passed. This accepts the parked-workspace System action
path; ordinary workspace coexistence and attended pointer/AC accuracy remain open.


## Mounted workspace study and fatal reset admission

The opt-in VM kernel provides `acpiruntime battery-workspace` for a disposable
mounted `/work` study. It exercises the existing Text file worker against
`/work/battery-study`, with 60 edits and five save/stat/rename/reopen cycles while
cached battery samples continue. The workspace stays mounted until all three
clients and the service retire. The native backend refuses this command before
opening firmware resources; there is no native capture recipe or boot option.
This is a prerequisite study, not ordinary desktop enablement or physical acceptance.

`tools/battery_workspace_smoke.py` creates its own fresh exFAT image and never
accepts a host disk path. Run it through `tools/host-env.sh` after building matched
`all acpi-battery-kernel acpi-battery-vm-kernel` targets. Cases are `normal`,
`timeout`, `refusal`, and `runtime-fault`. Completed cases use an atomic boot-log
dump for strict proof checks, run a replacement compositor, unmount normally and
verify the exact file tree, metadata identities, anchors, partition prefix and
allocation graph offline. The fault case preserves the interrupted disposable
disk and explicitly does **not** claim clean retirement.

Contained-fault reset admission is now checked again with IRQs masked at the
point of failure. Earlier arming while parked cannot authorize reset if an
ordinary workspace is now mounted or a scheduler I/O scope is active. In either
case the runtime masks its sources, reports `battery-reset: BLOCKED`, retains
live state and halts. It does not reenter the filesystem to flush, free the live
runtime or claim successful teardown. Parked, idle fault trials may still reset
when armed. This also means an isolated-image fault during an active I/O scope
must halt, even though earlier image trials allowed reset there.

Ordinary native workspace enablement remains blocked on recoverable failure
ownership: a failed ACPI owner/worker must be quarantined without stack reuse,
queued-wakeup resurrection or freeing firmware-owned memory; the active storage
owner must finish safely before logging/unmount/return. Fatal callbacks may run
inside IRQ context, so merely killing a kernel thread is insufficient. A future
change must prove these paths with injected faults before enabling mounted Home
on hardware. Automatic battery startup remains off.


## Owner-watchdog quarantine study

The VM-only `acpiruntime battery-workspace-recovery` command adds a recoverable
boundary for a sample deadline detected by the service owner. It requires IF set,
the matching owner identity, a ready runtime, and zero SCI callbacks in flight.
It masks and reads back all fixed/GPE enables and the SCI route before asking the
scheduler to quarantine the owner and its worker. All native paths and other fatal paths outside the explicit IRQ entry study
below retain the existing containment/reset interlock. This is **not** an
exception escape mechanism from an IRQ or an arbitrary AML callback.

The CPU0 scheduler primitive validates boot-lifetime identities and both kernel
service contexts before any mutation. It refuses stale, idle, userspace, dead or
storage-owner targets and requires an idle fallback. It unlinks waits, clears
timeouts and scheduling eligibility, and assigns a distinct quarantined state.
Neither thread is runnable, wakeable, dead nor reusable. Stacks, interpreter heap,
queue entries, callback contexts and firmware resources remain retained until
reboot; there is no resume or reclaim operation. The caller performs only final
IRQ-masked accounting and yields without unwinding or restoring interrupts on
its retained stack. The adapter must establish that shared resources needed by
unrelated kernel services are not held by these threads.

The battery cache reports FAULTED and retains the epoch; start and stop are
refused. The study's ordinary owner waits for its three client descendants to
retire, crosses the stuck worker's original sleep expiry, attempts queue wakeups
and delivers a late SCI vector. It verifies unchanged worker completion, sample
generation, heap, query and IRQ counters and retained scheduler identities.
It then saves boot/exit logs through the ordinary workspace logger, unmounts,
checks reset admission again and returns automatically. Logs explicitly say
`runtime_retired=NO` and `test outcome=contained-failure`: successful containment
is not successful service teardown.

`battery-workspace-recovery-io` additionally runs an unrelated storage owner
which holds its scheduler scope for 28 seconds around the 25-second sample
deadline and flushes before and after quarantine. Ordinary recovery/logging
cannot run until this owner releases its scope. This proves scheduler ownership
and real flush continuation, not outstanding DMA or partially completed physical
media transactions. File work can remain blocked for this whole fixture; its
zero completed Text saves must not be reported as an interactive workload pass.

Use `tools/battery_workspace_smoke.py --case recovery`, `recovery-io`, or
`recovery-refusal` through the host environment. The additional
`recovery-owner-refusal` case deliberately lets the modeled battery worker own
a storage scope: quarantine must refuse and halt without restoring interrupts
after source masking, saving a false completion or resetting active storage. The harness accepts only its own
disposable image, requires automatic reset after successful containment, checks
the saved failure log against serial evidence, and verifies the exact file tree,
metadata, unchanged anchors and allocation graph offline. Remaining native
prerequisites include IRQ-origin fault handling, shared-lock/resource audits and
fault injection around real device operations. Automatic startup remains off.

### VM IRQ entry containment

`acpiruntime battery-workspace-recovery-irq` extends the disposable workspace
study to the foreign-enable guard at SCI callback entry. It does not recover
arbitrary faults inside the interpreter callback. The native backend refuses the
command before platform access; ordinary desktop startup remains unchanged.

With IRQs masked on CPU0, the guard verifies source masks and retains both
noncurrent battery service threads. Neither can be the interrupted thread or a
storage owner. It returns unclaimed through the normal IRQ registry, dispatcher
EOI and completion callbacks; the interrupted stack continues normally. Retained
threads cannot run, wake, reuse their slots or release interpreter allocations.
The service stays faulted until reset. Failure to verify masks or retain the pair
is terminal without restoring IRQs or attempting storage cleanup.

Run `tools/battery_workspace_smoke.py --case recovery-irq` through
`tools/host-env.sh`; the negative cases are `recovery-irq-target-refusal`,
`recovery-irq-mask-refusal` and `recovery-irq-refusal` (native admission).
The VM fixture injects a software SCI on an unrelated storage owner's stack. A
second handler and completion callback prove continued dispatch; successful
removal after return checks registry pending/depth accounting. A stack sentinel,
current identity and I/O ownership must survive. Both flushes, retained state
across the old sleep deadline, saved failure logs, ordinary unmount and automatic
reset are checked. This exercises the real software dispatcher/EOI path, not a
physical level-triggered SCI or outstanding device DMA. Negative fixtures inject
on the battery worker itself or refuse an enable-mask write; neither may resume
or claim clean retirement. The previous watchdog recovery remains separately
covered.

Remaining boundaries: deep uACPI callback faults, IRQs interrupting a battery
thread, native shared locks/firmware resources and physical level-triggered
behavior. Do not promote this study to native mounted Home or automatic startup.

The shared-resource check for this boundary is deliberately limited to CPU0 and
the modeled sample sleep. The runtime allocator and spinlock adapter serialize
with IRQ masking; this entry guard calls neither the allocator nor uACPI. The
worker's sleep semaphore, interpreter-owned semaphores, work items and heap remain
allocated with their stacks. Quarantine removes scheduler wait membership without
pretending to decrement semaphore waiter counts or complete outstanding work.
Normal storage uses none of those interpreter objects. The SCI token/context
stays registered, but later deliveries bypass the retained interpreter. Source
mask verification and scheduler retention occur in one IRQ-excluded interval.
These observations support the disposable fixture; they do not prove that an
arbitrary native AML callback holds no firmware Global Lock, EC transaction or
other shared resource required by unrelated hardware. Those cases still need
explicit ownership evidence and injected-fault coverage.

### Returned GPE callback failures

The VM-only `battery-workspace-recovery-callback` study exercises an actual
uACPI SCI → GPE-dispatch → raw EC handler call chain. A modeled EC status bit
causes entry through the interpreter; the queue-admission fault occurs after
GPE suspend/clear. The raw handler returns NOT_HANDLED on the reported enqueue
failure, and uACPI returns normally to the bridge. Only then does the bridge
mask sources and retain the noncurrent service pair. No upstream interpreter
code or stack unwinding mechanism is changed.

Callback-scoped spinlock accounting must balance on return, the failed query
must not be pending in the worker queue, and the interpreter must return
unclaimed. The existing same-stack storage, secondary handler, post-EOI,
registry removal, retained-state and saved-filesystem proofs still apply. A
leaked modeled spinlock, interrupted battery worker, or use outside the explicit
callback recovery scope remains terminal. Native command admission is refused
before platform access. These VM hooks are null in the native backend.

This covers a known enqueue refusal with a normal return path. GPE suspend/clear
failures, query-budget failures, unexpected indices, arbitrary interpreter
exceptions/nonreturning callbacks and native firmware lock ownership remain
outside this boundary. Runtime state, including the interrupted sample and query
budget, is retained until reboot; no clean service retirement is claimed.

Run `tools/battery_workspace_smoke.py --case recovery-callback` through the host
environment. Refusal fixtures: `recovery-callback-target-refusal`,
`recovery-callback-lock-refusal`, `recovery-callback-outside-scope`, and
`recovery-callback-refusal` (native admission). This is software SCI dispatch in
a disposable Q35 image, not physical level-triggered SCI or DMA acceptance.


### Terminal handoff after completed IRQ delivery

The VM-only `battery-workspace-recovery-target` study extends the returned-query
admission failure to an IRQ interrupting the battery owner or worker itself.
The bridge requests a token-bound IRQ exit hook and returns normally. The
architecture dispatcher sends EOI, runs every registered completion and clears
all registry bookkeeping before invoking that hook with interrupts masked.
Only then may the hook retain both battery threads and switch permanently away
from the interrupted stack. The interrupt frame and its saved FPU state remain
with that retained stack; the failed service never returns through IRET.

`irq_handler_defer_exit` accepts one request per delivery, only from that token's
handler or completion, and only for a registration with a completion callback.
Registration changes and redispatch remain excluded until `irq_handlers_finish`
consumes the request. This is an explicit terminal boundary, not a general work
queue or automatic resource cleanup. The CPU0 scheduler retention rules still
reject user threads, the idle thread, a storage owner and invalid peer states.
Native study admission remains disabled.

Run `tools/battery_workspace_smoke.py --case recovery-target-worker` and
`--case recovery-target-owner` through the host environment; native refusal is
`--case recovery-target-refusal`. The disposable fixture arms delivery only after
an unrelated storage owner has registered a second handler and completion. Proof
records must identify the interrupted battery role, drained callbacks, balanced
callback locks, a separate resumed storage identity and successful registry
removal. Existing retained-state, both-flush, saved-log, unmount/reset and offline
filesystem checks also apply. Returning to the failed target is a test failure.
The older callback study still refuses target interruption without this scope.
This does not accept native firmware locks, physical level-triggered SCI recovery,
outstanding hardware DMA, or arbitrary nonreturning interpreter faults.


### Recovery resource admission

The [firmware resource audit](battery-resource-audit.md) distinguishes retained
CPU state from an unfinished EC/SMBus protocol or firmware Global Lock. Every VM
quarantine path now requires an observed idle protocol and unowned/nonpending
Global Lock, including the AML and underlying semaphore paths. The snapshot only
refuses unsafe recovery; it grants no native admission and performs no cleanup.
Native mounted Home and ordinary automatic startup remain disabled. The
[native AML side-effect map](battery-native-aml-audit.md) identifies startup's
shared writes, all nine captured EC queries and the absence of rollback in
`_REG(3,0)`. The explicit `battery-workspace-query` VM study now exercises returned
query-admission failure with whole-batch effect receipts and orderly failed-service
retirement. It grants no native recovery or mounted Home admission. Native resource gates
remain unchanged; use the audit guide for the case matrix and remaining work.


### Air query admission

The exact Air7,2 adapter now selects the captured-firmware query policy before
any query method executes. Unknown codes, GPU/power query80 and query10's mutable
OS-dependent SMBus path are refused before AML. Native refusal remains terminal
outside the explicit parked query capture described below.
Query20/21 retain their audited shared writes, so they do not qualify as effect-free
recovery batches. QueryCF is bounded under both OS states. See the
[native policy and VM replay](battery-native-aml-audit.md#exact-air72-dispatch-policy)
for the full table, evidence and remaining parked-workspace acceptance. No new
register/firmware admission, ordinary startup or mounted Home support is enabled.


### Parked native query-return capture (v45)

`hardware_boot.py stage --laptop-probe-volume <local-volume-id> --battery-query`
selects the native battery kernel and an unattended capture. It parks the exact
workspace device before starting the service, with no filesystem/image I/O during
the runtime. Only the existing exact Air7,2 firmware/resource profile and native
query policy qualify. No QR_EC value is injected, substituted or requested outside
the normal event path. The capture waits at most 30 seconds for natural refusal.

A rejected query can return normally only after completed transport, zero attempted
shared writes in that batch, equal AML call/return counts, drained notifications,
idle work and the existing resource proof. The owner performs ordinary disconnect,
worker/interpreter retirement, ACPI-mode/event restoration and heap cleanup before
the workspace can remount. Rejection before the first sample is valid with an
empty, nonfresh cache; it does not manufacture a battery reading. The resulting
service is explicitly faulted. Partial effects, retained locks, poisoned transport
or failed disconnect retain the existing parked-media fault reset, with missing
final evidence treated as failure.

`battery-native-query: outcome=returned-failure` is the scoped success receipt.
No rejected event within the window yields `outcome=not-exercised`, clean=NO and
completed=NO: save it and return to Ubuntu, but do not accept the feature. Both
handled outcomes return automatically; no user input is requested. The collector
requires exact native restored-event addresses plus ordered transport, resource,
retirement and capture receipts. VM evidence uses separate synthetic addresses.
This does not enable automatic battery startup, mounted Home or sibling models.

`tools/battery_native_query_smoke.py` exercises the actual boot capture path with
disposable exFAT media: unknown/high/OS-state refusal, drained notifications in
both OS states, pre-sample rejection, no event, shared effects, retained lock,
poisoned transport, failed disconnect and native-kernel platform refusal. Clean
cases compare live/saved proofs and check exact tree, metadata, allocation and fsck.
Air7,2 release `a72409383258a4a0` (source `6f6381c`, PR #44) physically accepted
this narrow path. A natural query10 was refused before the first sample: completed
transport, zero AML calls/effects, drained idle worker, resource proof, ordinary
disconnect and retirement, restored foreign enables, zero heap/errors/denied. The
cache remained empty, faulted and nonfresh. Three matching logs were saved after
remount; 916 prior files stayed identical apart from the metadata sidecar. Offline
fsck/allocation and Ubuntu return/default/recovery checks passed. This establishes
safe predispatch failure handling, not support for query10, battery readings or
mounted Home. The [machine record](machines/macbookair7-2.md) owns the exact evidence.


## Percentage convention

The service exposes the battery's **relative state of charge** (SBS command
`0x0d`) directly when valid; it does not recompute percentage from capacity
snapshots or substitute absolute SOC (`0x0e`). The [SBS 1.1 specification,
sections 5.1.14–15](https://sbs-forum.org/specs/sbdat110.pdf) defines relative SOC
against current full-charge capacity, and absolute SOC against original design
capacity. Calibration and rounding can also differ from a ratio of separate
capacity reads. Do not force one OS's displayed percentage to match another
without checking its source register.

On the reference Air's accepted v52 transition trial, SrvrOS displayed 97%
and Ubuntu displayed 85%. Ubuntu's installed `acpi-sbs` reader table was verified
to request `0x0e`, consistent with [Linux's SBS driver](https://raw.githubusercontent.com/torvalds/linux/master/drivers/acpi/sbs.c).
The post-return remaining/full capacity values matched across OSes. This explains
the comparison; it does not establish calibrated accuracy or battery health.


## Ordinary desktop supervisor event waits

The lifetime-owned ordinary Home session supervisor waits on the kernel process
wait queue instead of sleeping every ten milliseconds. The wait does not reap:
it uses the same caller/target ownership predicates as `process_wait`, checks
for queued controls and invalid mount ownership atomically before sleeping, and
returns for the original absolute diagnostic deadline. Start/Stop publications
notify after mailbox admission; query/refused operations do not notify. Process
retirement already notifies this queue. Spurious process events merely cause a
fresh pass through the unchanged checks and cannot restart a deadline.

While the mount hold is live, mount/unmount/parking refuse identity changes and
registered block parent identities are stable. Successful hold release publishes
invalidation before notifying the same queue. Stale or mismatched release cannot
unpin it. Mount identity is still validated before every dispatch and before
sleep. Native service ownership checks and firmware/IRQ watchdogs are unchanged;
no wake can confer an ownership capability. Failed waits use the existing bounded
sleep fallback. Capture/study loops that inspect battery progress, legacy tree
scans and client/service retirement retain their previous behavior and limits.

Host process-event tests cover owner/foreign/stale targets without reaping,
publication at the sleep boundary, notifications after blocking, absolute timeouts
and wait refusal. Workspace tests check release notification with existing stale
hold and mount refusal cases. The ordinary Home VM's `--supervisor-controls`
option drives two actual System Stop/Start cycles, sleeps between commands,
checks four dispatched requests and three clean epochs, then retires/reopens the
desktop. The existing alarm-volume threshold remains unchanged, with fresh samples
required after each restart. The automatic reopen fixture exercises the unchanged
20-second deadline; the power fixture retains its 300-second owner watchdog.


Air6,1 release `aa576838270d623a` subsequently passes the native v78 repeat with
13 app cycles and clean battery retirement: zero supervisor timer wakes in all
covered phases, no supervisor switches during idle/recovery, and 25 event wakes
during app work. The unchanged 500 ms UI loop bound is met at 110.061 ms worst.
The [unit record](machines/macbookair6-1.md) owns physical acceptance and its
power-measurement limits; no firmware watchdog or sampling cadence changed.
