# Filesystem capacity and allocation

The filesystem scale implementation replaces the old static 1,024-file / 256-directory
exFAT tables and 4,096-node VFS table. Native acceptance is tracked separately in
the [Air7,2 record](machines/macbookair7-2.md); an installed older release retains
its older limits.

| Resource | Budget | Allocation |
| --- | ---: | --- |
| exFAT files per volume | 8,192 | Stable blocks of 64 file records and paths |
| exFAT directories per volume | 2,048, including mount root | Stable blocks of 64 directory records |
| Mounted exFAT volumes | 4 | Independent tables and metadata scratch |
| VFS nodes across all filesystems | 32,768 | Bootstrap block of 256, then stable blocks of 256 |
| exFAT path | 159 bytes plus terminator | Unchanged |

These are admission budgets, not preallocated arrays or a promise that every
combination fits available RAM. `vfs_free_nodes()` reports unused budget;
`vfs_reserve_nodes()` attempts the corresponding memory allocation. Before disk
mutation, exFAT reserves file/directory blocks, VFS nodes, metadata replacement
nodes and serialization scratch. An allocation refusal at that stage performs
no disk writes. Later I/O/buffer failures retain the existing dirty/quarantine
and recovery policy; this is not a transactional filesystem.

Pointers to live VFS nodes, exFAT private file records and stored paths stay
stable across growth. Deleted file and directory slots can be reused, including
holes below the high-water mark. Unmount unregisters VFS nodes before freeing
exFAT blocks and scratch. VFS blocks remain allocated until VFS initialization,
because callers can inspect a retired node's `used` flag; repeated mount/unmount
reuses that bounded high-water allocation. Kernel heap initialization precedes
VFS imports so larger boot archives can allocate nodes safely.

The VFS keeps a sorted dense list of live slot IDs. Indexed enumeration is
constant-time and preserves slot order; prefix removal compacts it once. Path
and inode lookup remain linear. exFAT still rewrites the complete metadata sidecar
on each ordinary mutation. The scratch buffer grows on demand to a worst-case
bound derived from the node/path budgets, and cannot move while a nested sidecar
write references it. The sidecar and exFAT disk formats are unchanged.

Use [workspace admission and verified archival](ubuntu-device-agent-setup.md#workspace-log-capacity-and-verified-archival)
even with the larger budget. Retention never deletes unknown files automatically.
Linux preflight reads limits from the checkout: validate and install the matching
payload before relying on increased capacity on physical media.

## Validation

Run these sequentially in one checkout:

```sh
tools/host-env.sh make test-core test-exfat-hardening test-exfat-scale
tools/host-env.sh python3 tools/hardware_test_smoke.py --release RELEASE --scale-files 5000
```

`RELEASE` is a staged v8/v9 automated-test payload. The VM command uses disposable
media, exercises save/remount/reopen and automatic return, checks preserved files,
and retains the consumed-request and read-only-failure negative cases. It does
not install or reboot the host.

The host scale fixture imports 5,000 files, grows/reuses 96 files and 320
directories, verifies retained references and identities, edits/renames/remounts,
and checks exact contents with an independent parser and `fsck.exfat`. Allocation
faults cover mount rollback/retry and mutation refusal before writes. The hardening
suite additionally reaches both budgets, serializes a large metadata sidecar,
reuses directory holes at exhaustion, and injects write/flush/power-cut failures.

## Transaction-local read reuse

exFAT uses four fixed 128 KiB read-copy slots (512 KiB total) only while an outer
mutation owns storage. Exact or contained reads can reuse these copies; larger
requests, image mounts, ordinary reads and quarantined volumes bypass them.
Every overlapping write invalidates before media access. Complete successful
writes may patch and restore the owned copies only if no intervening cache or
transaction activity occurred. Failed/partial or reentrant writes leave affected
copies invalid. An exhausted generation disables caching instead of wrapping.
Outer completion discards the copies, including on failure.

This shortens transaction work without releasing ownership over intermediate
filesystem state. It introduces no delayed writes and changes no flush, dirty
marker, quarantine or teardown rules. It does not bound an individual device
command's latency or prove a native endurance improvement by itself.

`make test-exfat-read-cache` executes the production volume paths for contained
reads, overlap/partial failures, transaction boundaries, reentrancy and exhaustion.
`make test-exfat-read-reuse` compares cached and cache-disabled host drivers on a
1200-file,128 KiB-cluster fixture. It requires identical whole disk hashes and
write/flush counts, and fewer read bytes. The disabling define is host-test-only;
it is not a native runtime setting. Read counts include mounting. Existing
crash/growth/retirement sweeps still apply to the cached production path.


## Allocation-bitmap transfer spans

Allocation and freeing publish only the bitmap byte interval containing changed
bits. Contiguous allocation uses the allocated run; fragmented allocation uses
the ascending selected clusters; freeing computes the minimum and maximum bit
across the entire chain, including descending chains. Bytes between changed
bits retain their previously read values. The block layer preserves neighbouring
bytes with its existing partial-sector read/modify/write path. Full bitmap reads,
FAT-before-bitmap ordering, mutation ownership, dirty markers, flush boundaries,
failed-write quarantine and alias retirement stay unchanged.

`make test-exfat-bitmap-span` covers crossing bytes, the last valid bit,
fragmented/descending chains, untouched padding, invalid bounds and partial-write
quarantine. `make test-exfat-bitmap-io` compares the real driver with a host-only
full-cluster comparator. The1200-file128KiB-cluster fixture requires identical
complete disk hashes and write/flush call counts for create, overwrite, rename,
delete and replacement. Requested write bytes fall59.96–74.94%; these are block
API bytes, not physical transport-sector counts or a native latency guarantee.
Existing crash, growth and retirement sweeps also apply to narrow writes.

The logger saves the four longest completed mutations of at least100ms as
`exfat-cost` records, with start/end times, operation and success. Four
`exfat-cost-phase` rows per record attribute actual block API calls, requested
bytes and callback wall time to metadata, allocation, data and freeing (phases
0–3). Flush callback time is separate. Cache hits cause no block read and add no
read call; partial-sector work inside block callbacks is included in callback
wall time, not counted as another filesystem read. The remaining elapsed time
includes CPU, waits and nested work outside those callbacks. Records contain no
paths, allocate no memory and are printed after a CPU0 IRQ-protected snapshot,
outside mutation ownership. Match intervals against owner/gate records before
attributing a delay. This is observation, not a permission to release ownership
mid-publication or an SMP synchronization mechanism. `make test-exfat-cost`
checks accounting, failure retention and the four-record bound.

The automatic VM workload can add a preserved empty archive with
`tools/audio_io_stress_smoke.py --air72 --accel kvm --populated-home --metadata-files 1200 --write-iops 100`.
This enlarges the real serialized sidecar without allocating one128KiB data
cluster per fixture file. It preserves the original live PCM/disk workload,
limits and existing Home fixtures, checks every archive file after return, and
is forbidden with an exact staged native profile. Add `--read-iops 100` for
combined read/write throttling. This remains disposable VM evidence.


## Verified append writeback

Whole-buffer flushes can reuse the existing extent only for an actual append.
Under the existing owner, compare every committed prefix byte against media.
Changed prefixes, shared extents, incomplete ValidDataLength and old EOFs inside
a device sector retain whole-file copy-on-write. Prefix-read or resource failure
refuses instead of treating missing evidence as permission for replacement.
Global sync still visits every live dirty content object; fsync/close still wait
for durability. No caller dirty hint, new syscall or process admission is used.

Eligible appends write only the tail, starting at a device-sector boundary so
old payload sectors remain untouched on torn writes. New clusters and FAT links
are initialized under the dirty marker; a NoFatChain stream ignores its prepared
links until publication. Payload is flushed, the old tail link is flushed if
needed, then the checksummed new length/flags and metadata sidecar are durably
published. Later appends can traverse the resulting fragmented FAT chain. Old
prefix clusters are retained; uncertain failures keep quarantine and offline
recovery requirements. An `append` cost label identifies a verified prefix.
This does not make directory/checksum writes crash-atomic: a torn publication
can lose a name during repair, as it can with the existing replacement path.

`make test-exfat-append` compares actual drivers with append enabled/disabled,
checks exact contents/remount/fsck, changed/alignment/alias fallbacks, failed
prefix reads, original payload sectors and unrelated files across every write,
flush and half-write interruption. Torn-name loss must reproduce on COW too;
it is not accepted as an atomic rename guarantee. Test media is disposable.
Native performance and full endurance require their own matching trial.
