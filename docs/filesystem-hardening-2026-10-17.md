# exFAT hardening and independent recovery tests

This follows the successful physical test `064df72e6888db2d` and its
[interoperability correction](exfat-interop-2026-10-17.md). It focuses on file
operations, write ordering, and recovery; GUI work remains deferred.

## Physical flash drive

The USB volume `37D8-C5D0` had an allocation bitmap declared as 59,150 bytes for
473,192 clusters. Its required length is 59,149 bytes. Both lengths occupy the
same 128 KiB cluster.

`tools/repair_exfat_bitmap.py` corrected the length with the volume unmounted.
It verified the USB transport, volume identity, clean flags, single FAT and
unchanged allocation. It saved the original 32-byte bitmap directory record
before writing and checked the result. Only one byte in that record changed;
file contents, FAT entries and bitmap contents were untouched.

- Original record and repair plan: `build/review/usb-bitmap-repair.json`.
- Independent before/after check: `build/review/usb-bitmap-fsck.log`.
- After correction, `fsck.exfat -n` reported clean, four directories and eleven
  files, without diagnostics. Normal collection of the saved physical test
  still passed after remounting.

The checker previously printed an ERROR while exiting zero. New integration
tests require clean output as well as a successful exit code.

## Driver changes

- Empty files keep AllocationPossible set and NoFatChain clear.
- Full directories can grow: contiguous directories acquire a FAT chain, and
  non-root directory streams receive an updated length and entry checksum.
- New names are validated before allocation. ASCII case collisions are refused.
- An overwrite writes replacement data to new clusters, flushes it, publishes
  and flushes the updated directory entry, then releases the old allocation.
  New files/directories flush allocation and contents before publishing a name;
  relocated renames flush the destination before removing the source. Deletion
  persists the removed entry before releasing clusters.
- Each mutation marks and flushes the main volume dirty flag before changing
  metadata. It flushes the completed changes before clearing and flushing the
  clean marker. Nested metadata-sidecar operations share the mutation scope.
- Mutation scopes retain the scheduler's I/O owner and interrupt state, allowing
  existing I/O service work while excluding competing filesystem mutations.
- Write/flush failures latch the mount read-only. Dirty or media-error volumes,
  unsupported FAT arrangements and unsupported bitmap sizes mount without write
  access. A second mount of the same block-device object is rejected.

The volume-flag policy follows the
[Microsoft exFAT specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification),
including the main boot sector's authoritative flags and their exclusion from
the boot checksum. The backup flags can be stale.

## Reproducible verification

From the repository root:

```sh
tools/host-env.sh make -j2 all test-core
python3 -m unittest discover -s tools -p 'test_hardware*.py'
python3 -m unittest discover -s tools -p 'test_exfat_bitmap.py'
make test-exfat-hardening
tools/host-env.sh python3 tools/hardware_test_smoke.py
tools/host-env.sh python3 tools/cli_smoke.py
```

The hardening harness compiles the actual kernel exFAT and VFS sources against
a host block-device adapter. It uses disposable regular files, formatted and
checked by exfatprogs 1.3.2. Normal operations exercise 4 KiB and 128 KiB clusters:
create, grow, shrink, zero length, rename, cross-directory move, delete, directory
growth, remount/readback and forced fragmented allocation. An independent reader
checks payloads and confirms the fragmented file uses a FAT chain.

For overwrite, create, delete and rename, it injects a failure at every write and
flush boundary, then separately stops after every completed write. Dirty images
must refuse mutations on remount without changing any bytes. Only disposable
copies are repaired with `fsck.exfat -y`; they must subsequently pass a strict
read-only check and retain the unrelated anchor file and permitted old/new
operation contents. Missing flush support and a foreign I/O owner also refuse
writes without touching the image.

All 140 injected cases passed: overwrite 40, create 35, delete 34 and rename 31.
That covers 55 failed writes, 55 completed-write interruptions and 30 failed
flushes. The 16 C regression groups, 20 hardware Python tests, three bitmap
repair tests and three-scenario QEMU automatic boot cycle also passed.
The full 479-command shell regression passed, including `posixdemo` and the
final guest filesystem check: 384 files, zero leaks, zero FAT errors and zero
total errors.

Evidence: `build/exfat-hardening-build.log`,
`build/review/exfat-hardening-python.log`, `exfat-hardening.log`,
`exfat-hardening-qemu.log` and `exfat-hardening-cli.log` in `build/review/`.

## Limits and recovery

This is not a journal or a promise of atomic power-loss recovery. The fault model
stops between completed driver writes; it does not model torn sectors, arbitrary
reordering inside a flush group, or devices that falsely acknowledge a flush.
An interrupted rename can leave duplicate names pointing to the same data until
offline repair. Copy-on-write replacement also requires spare clusters, even
when the replacement has the same length.

For an interrupted physical write, stop writable testing, unmount the USB volume
in Ubuntu and preserve an image/backup before considering repair. Inspect with
`fsck.exfat -n`; do not simply clear the dirty flag. The automatic test collector
still requires a complete, matching report and completion checksum.

Filename support remains ASCII, with case-sensitive VFS lookup; collision
rejection does not implement full case-insensitive or Unicode lookup. Renaming
nonempty directories is still unsupported. Writable bitmaps must fit one cluster,
matching the current allocator. Geometry validation, multi-cluster bitmaps,
broader crash cases and durable metadata-sidecar replacement remain follow-up
work.

The previously working normal console release `ff6be76bcdfff11c` has an
independently hash-verified copy at
`~/.local/share/srvros/known-good/ff6be76bcdfff11c` before release rotation.

## Laptop release and physical test result

Installed normal console release `248a8dc15fe34246` contains the tested kernel with SHA256
`b09bb75024a4039f104f953971d18c9d88323395dc3aab4fe22b3b49b2dd8bc7`.
The matching automatic profile is `dfd1690ca43b2ec0`, with request
`70588005b8917c28` on USB UUID `37D8-C5D0`. The cycle was launched on
2026-10-17 and the user returned to Ubuntu. Normal collection validated the
complete report, completion checksum, run/build/volume identities and persisted
USB probe without repair. All guest checks passed. Physical key presses remain
explicitly untested by this automatic suite.

The 1,000-line render took 4,937 microseconds at 1440x900, pitch 6144, with one
presentation and 5,160,988 framebuffer bytes written. Four USB devices and one
keyboard were enumerated. Boot readiness, timer, memory, FPU traps, rendering,
keyboard enumeration and USB write/flush/read all reported PASS.

An unmounted `fsck.exfat -n` check after collection reported clean, four
directories and fifteen files, without diagnostics. The drive was remounted.
The installed test and normal console payload hashes were verified, and GRUB's
one-shot selection was confirmed consumed. Current is `dfd1690ca43b2ec0`;
previous is the normal console release `248a8dc15fe34246`.

The request is consumed and must not be launched again. To recollect its results:

```sh
python3 tools/hardware_test.py collect --run 70588005b8917c28
```

Evidence is in `build/hardware-tests/70588005b8917c28/`: `report.txt`,
`completion.txt`, `result.json`, `launch.log` and `offline-check.log`.
Future physical cycles require a new request via `hardware_test.py prepare`.

Preparation does not arm GRUB or reboot. The explicit console installation uses
`--release 248a8dc15fe34246`; the staging `latest` pointer now names the separately
prepared automatic profile. Installation evidence is
`build/review/exfat-hardening-install.log`; preparation evidence is
`build/review/exfat-hardening-prepare.log`.
