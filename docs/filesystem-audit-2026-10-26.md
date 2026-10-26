# Filesystem audit — imported exFAT media

The broader system audit resumes from `dfd63fef` (merged PR179). This first
checkpoint covers exFAT boot geometry, initialized file contents and preservation
of that boundary during mutation. It does not complete the filesystem or whole-OS
audit. Before publication the branch fast-forwarded to `afc8bb67` (PR180),
rebuilt, and passed the import, complete thread and libuv VMs again. The Apple
Silicon host owns this work; desktop, Air6,1 and Wi-Fi work remain
with their device branches.

Delivery: [draft PR182](https://github.com/PaulMDemers/SrvrOS/pull/182), runtime
first-checkpoint commit `a8d1617b`. Its tested source hashes match that commit; this report and the
host record distinguish pre-integration and post-PR180 builds.

## Confirmed defects and corrections

**Imported file reads exposed uninitialized allocation bytes.** The driver used
`DataLength` for every backing read and ignored `ValidDataLength`. An imported
1,100-byte file with only 520 initialized bytes returned the fixture's stale
`0xa5` tail. This affected both memory images and block devices. The driver now
retains both lengths, reads only initialized bytes and returns zero for the rest
of the logical file. Rename and replacement preserve the initialized boundary
and recompute the entry-set checksum. A successful full overwrite publishes the
new initialized length in both the disk entry and live file record.

**Mount accepted unchecked boot geometry.** Disk-controlled shifts were evaluated
before validation, and FAT/heap/volume extents were not checked against each other
or the backing medium. Malformed images reached scanning and allocation. Both
mount entry points now validate geometry first: sector and cluster shifts, FAT
capacity/location, heap extent, cluster count, root cluster, volume size and
format revision. Reads and writes use the declared volume extent. Only revision
1.00 with one FAT and active-table bit zero is accepted. Two-FAT/TexFAT images are
refused: the previous read-only fallback could still read the wrong FAT.

The import scan also rejects a valid length beyond the logical length, logical
sizes beyond the volume heap, invalid nonempty first clusters, contiguous extents
beyond the heap, and directory streams with zero, partial-cluster or partially
initialized contents. Refused imports do not retain VFS entries. Dirty/media-error
single-FAT volumes remain readable with mutations refused.

These rules follow the [Microsoft exFAT specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification),
particularly boot geometry and stream-extension length semantics. They do not
implement every format validation rule or add TexFAT support.

## Regression evidence

Evidence is retained locally under `build/review/filesystem-audit/`; raw images
and logs are ignored rather than committed. The host runner records source hashes,
compiler command, base HEAD and each case result in `result.json`.

- Before the fix: the valid control passed; 13 of the original 14 directed modes
  failed against the real exFAT/VFS implementation. Assertions observed malformed
  mounts being accepted or stale allocation bytes being returned.
- After the fix: 16 modes pass normally and under AddressSanitizer plus
  UndefinedBehaviorSanitizer. These cover both mount paths, invalid geometry
  before allocation, valid shift boundaries, dirty-volume refusal, invalid stream
  lengths/ranges, zero/partial/exact-cluster valid lengths, FAT-chain reads,
  short/long/cross-directory rename, replacement, overwrite and remount.
- Six runtime negative controls are detected: full-length backing reads, lost
  rename/replacement boundaries, stale overwrite cache, unsupported FAT selection
  and acceptance of an invalid initialized length. Each mutated source compiles
  and then fails its designated assertion. The initial negative-control compiler
  failure is retained separately and is not counted as a detected regression.
- Existing entry-layout/checksum tests pass with the ARM host-test define and
  Darwin dead-strip linker option. The unmodified Linux-oriented Make target is
  not claimed portable to macOS.
- The matched macOS `all` build and dedicated Q35 import VM pass. The guest reads
  three 8 KiB files whose valid lengths are zero, 4 KiB and 4 KiB + 7 bytes, then
  exercises rename, replacement and overwrite through libc/kernel file APIs.
  The fixture fills uninitialized storage with `0xa5`; the guest requires zeros.
  It uses temporary copies and leaves the built source disk untouched.

The complete thread suite passes. The first libuv run exceeded the harness's
10-second completion budget after `fs poll ok` and exited 3 for missing markers;
that failed log is retained. A fresh VM with `--line-wait 60` completed the full
libuv suite with exit 0. This is acceptance at the recorded 60-second budget,
not a pass at the shorter budget. Host remount cases establish on-disk
preservation; guest file tests establish the public syscall path. This is not native-hardware or
power-cut acceptance. The Linux exfatprogs hardening/power-cut campaign was not
rerun on this Mac.

## Reproduce

```sh
sh tools/macos/build.sh test-exfat-import
python3 tools/test_exfat_import.py --sanitize --output build/review/exfat-import-sanitized
sh tools/macos/build.sh -j2 all
python3 tools/exfat_import_smoke.py
python3 tools/thread_smoke.py --boot-wait 60 --run-wait 300
python3 tools/libuv_smoke.py --boot-wait 60 --line-wait 60
```

Run build and VM commands sequentially in a checkout. `test-exfat-import` also
belongs to `test-core`. The guest mode is explicit (`exfat-import`) because its
special imported-media fixtures are absent from the ordinary thread suite.

## Entry-integrity checkpoint

The next pass starts at `352f9b0d`, still on main integration base `afc8bb67`.
The earlier checksum gap is now corrected in the image packer and mount parser
together. The packer emits entry-set checksums, ASCII name hashes and allocation
flags, and represents empty files without a cluster allocation. The test-fixture
helper follows the same empty-stream contract. Unsupported names are rejected
at generation time instead of producing ambiguous disk records.

Before publishing a file or directory, both mount paths now validate its complete
entry-set checksum, require exactly one immediately following stream and the
required consecutive name records, check allocation flags/empty-stream fields,
and verify its ASCII name hash. The previous search for arbitrary stream/name
positions could parse a different layout from the one mutation code later wrote.
Imports now refuse invalid characters, embedded separators, dot/dot-dot names
and unsupported non-ASCII text instead of substituting `?` into the VFS identity.
Unused name padding is ignored; supported single- and multi-record names pass.

The supported entry set is deliberately limited to a stream and ASCII names,
with at most 19 entries contained in one cluster. Additional vendor/benign
secondary records are refused because this writer cannot preserve them during
rename. This is an explicit compatibility boundary, not a claim that such
extensions are invalid exFAT. Older generated disks with unset checksums must be
rebuilt with the updated packer and paired with the matching kernel. This change
does not repair, reformat or upgrade a user's existing data/recovery media.

The directory walker also used to accept invalid FAT successors, premature chain
termination and traversal-budget exhaustion as success. It now rejects invalid
successors and chains shorter than the declared directory data, and detects
cycles encountered during scanning using constant memory. Self-loops and a cycle
after an acyclic prefix are covered. This does not inspect links beyond a logical
end marker/declared length or validate file allocation ownership.

Evidence is retained in `build/review/exfat-entry-audit/`:

- Ten new modes failed against the previous implementation; the original sixteen
  and two positive/bounds modes passed. The later chain-prefix case extends the
  final suite to 29 modes, all passing normally and under ASan/UBSan.
- Rejected partial imports leave no VFS publication or occupied mount slot; the
  same mountpoint can immediately mount a valid fixture. Corruption checks cover
  both memory and block mounts and require no writes or flushes on refusal.
- Seven independently compiled negative controls are detected: checksum/hash,
  name character validation, allocation flags, extra secondary records, cycle
  detection and short-chain refusal.
- Four generator test groups pass (fixed hash vector, name lengths, empty/directory
  streams and invalid names). Running them with the old packer fails. An independent
  recursive check of the rebuilt disk validates all 372 sets in 16 directories.
- The matched `all` build and existing entry-layout/checksum host test pass.
  All four final VMs pass: import (16.48 s), corrupt-entry/fallback (16.35 s),
  complete threads (40.39 s), and libuv (10.22 s, 60-second completion budget).
  Receipts and logs are under `vms-final/`. The corrupt-image runner requires an explicit mount
  refusal, successful clean-initramfs fallback and a guest assertion that the
  rejected disk's directory/files are absent. A timeout alone is not a pass.
  The first refusal harness wrongly prohibited any test launch, overlooking
  normal fallback; its failed log is retained. The corrected harness verifies
  the recovery behavior explicitly.

Reproduce the additional image/refusal checks after the matched build:

```sh
python3 tools/test_exfat_generator.py --image build/srvros.exfat
python3 tools/exfat_import_smoke.py --corrupt-entry
```

The generator tests also run under `test-exfat-import`. The earlier libuv timeout
remains historical evidence; the current acceptance command uses `--line-wait 60`.
No native hardware or Linux exfatprogs power-cut campaign is claimed here.

## Remaining audit queue

1. Allocation ownership and lifetime: cross-linked extents, duplicate path
   identities, bitmap/FAT disagreement, failed import cleanup under allocation/I/O
   faults, unmount/open-file references and device removal. Trace caller busy
   guards against direct `exfat_unmount` and retained process VFS references.
2. Mutation failure/recovery boundaries and durable metadata. Existing serialized
   replacement and dirty-volume quarantine are not a journal; interrupted rename
   and power-loss atomicity remain unproven. Multi-cluster bitmap mutation,
   boot checksum/backup recovery, upcase-table integrity, full Unicode and
   cross-cluster/extended entry sets remain outside this checkpoint.
3. Networking/socket lifecycle, VM/syscall validation, process/resource isolation
   and remaining documentation reconciliation follow as separately bounded work.

The [overall inventory](audit-progress-2026-10-26.md) tracks what has been audited
without assigning a misleading whole-system percentage.
