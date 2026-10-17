# Bounded USB transfers and shell responsiveness

The preceding laptop batch `ade82408bfb8320f` passed all five rounds, all 75
expected output files and offline filesystem checking without repair. Its
validated payload is backed up independently as `bf8a6d244459c66b`. The earlier
intermittent USB status-wrapper failure remains unexplained. This increment
addresses transfer overhead; it does not claim to fix that failure.

## Observed bottleneck

An exFAT mutation holds scheduler I/O ownership until its data and metadata have
been flushed and the clean flag published. Keyboard/console service threads can
run, but ordinary application threads cannot. Keyboard reports can therefore
be retained while the shell waits seconds to echo a character. Removing this
restriction without replacement synchronization would expose partially updated
filesystem state and shared cache/process structures to concurrent operations.

The transfer path also did unnecessary work: the block cache split every large
request into individual sectors, and a full-sector write fetched the old sector
before overwriting all of it. USB then issued one READ(10) or WRITE(10) per sector.

## Change

USB opts into aligned block-cache transfers capped at 4 KiB, matching the existing
pinned controller buffer. Other devices retain their current cache path. Read
misses are combined up to that limit, stopping before a cached sector. Complete
sector writes bypass the unnecessary preread. Partial first/last sectors retain
read-modify-write behavior. Partition aliases continue to use the parent cache.

The USB driver encodes the sector count in each READ(10)/WRITE(10) command and
splits larger direct requests to fit its buffer. The count layout is consistent
with the [Linux SCSI disk command builder](https://github.com/torvalds/linux/blob/master/drivers/scsi/sd.c).
At 512-byte sectors, an aligned 8 KiB driver read or write now uses two commands
instead of sixteen. This command reduction is verified by the transport model;
it is not a claim of an eightfold end-to-end speedup.

Writes remain synchronous and write-through. Before a batched write, all cached
sectors in its range are invalidated; only a successfully completed write
publishes replacement cache entries. An ambiguous failure may have changed a
prefix, so the entire attempted range stays invalid and later batches are not
submitted. Failed reads publish no cache entries. Flush ordering, failure
quarantine, command validation, deadlines and indefinite diagnostic halts remain
in force. Cache service checkpoints still account for each processed sector,
and slow USB waits retain their existing service yields.

## Emulator measurements

`tools/workspace_latency_smoke.py` measures host time from a QMP USB-key event to
serial echo, using a disposable disk throttled to 200 write IOPS. It now takes
three independent active-write samples by default (`--write-rounds 1..10`).

| Workload | Before | Batched candidate |
| --- | --- | --- |
| Idle key echo, ten samples | 4.7–9.1 ms | 4.1–7.3 ms |
| Key echo during a 512 KiB write | 5,050.5 ms, one sample | 483.0, 482.5, 1,906.7 ms |
| Key echo during 20,000 output lines | 24.1 ms, one sample | 35.6 ms, one sample |

All injected keys were retained, the persisted 512 KiB file matched exactly,
and the independently formatted exFAT image checked clean. These sparse emulator
samples show reduced storage stalls, but not acceptable interactive latency
under all write loads. They do not measure the laptop display or establish a
percentile performance guarantee. Evidence is retained in
`build/review/batched-usb-baseline*` and `build/review/batched-usb-latency*`.

Host regressions cover 512–4096-byte sectors, aligned/unaligned boundaries,
multi-batch requests, exact untouched bytes, command counts, cache hits splitting
read batches, eviction, failed-read publication, partial failed writes, partition
coherence, range rejection, and retained I/O service windows. The full build and
all 21 host test groups passed; focused tests were rerun after the additional
failure/service assertions. Physical validation of this candidate is pending.

The full five-round QEMU automatic cycle passed: all five reports and all 75
output files independently verified, a clean filesystem, and return through the
GRUB default entry. Consumed-request replay and forced write failure stayed
halted for the observation period beyond the old 15-second window. Separate USB
integration tests passed read-only enforcement, writes, exact persistence after
reboot, and keyboard input. A stalled-transfer injection quarantined the disk
within the existing deadline while keyboard input continued; the read-only disk
image remained unchanged. Logs use `build/review/batched-usb-{build,cycle,integration,timeout}.log`,
with retained serial evidence in `build/review/batched-usb-evidence/` and payload
hashes in `build/review/batched-usb-payload.json`.

## Next synchronization step

Keep the existing exclusion until replacement ownership is tested. Audit VFS
lookup/node lifetimes, exFAT readers and mutators, shared open-file state, process
exit, and block-cache ownership before admitting arbitrary user threads during
disk waits. Introduce waitable ownership at those boundaries, with tests for
concurrent readers/writers, close/exit, cancellation and unmount. Then repeat
the throttled key-to-echo workload and require the shell to echo before the
background writer finishes, rather than merely proving that keys were retained.
