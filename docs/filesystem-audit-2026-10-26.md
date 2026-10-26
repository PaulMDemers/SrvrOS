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

## Namespace and allocation-ownership checkpoint

This pass starts from `0ba50fe9` and incorporates main `9f18d7d6`, whose only new
change is the Air6,1 stress report. PR184's worker admission, PR181's desktop
lifecycle and PR19's Wi-Fi implementation remain separate.

**Import and mount namespaces could overlap.** Duplicate and case-colliding file
names, file/directory collisions and duplicate directory entries are now refused
before VFS registration. Mounts require a bounded canonical absolute path with
no empty, dot or dot-dot components. Stacked/nested exFAT mounts, occupied target
subtrees and paths underneath an existing file are refused. Sibling names such
as `/vol` and `/vol2` remain independent. Previously, a failed import could remove
unrelated entries under its mount prefix during rollback, and parent teardown
could unregister a nested mount's nodes. Admission now establishes that the
rollback/unmount subtree belongs exclusively to this mount.

**Releasing one alias could free another file's data.** Generated images share
some exact allocations between filenames; this is a repository compatibility
convention, not a new hard-link API or a claim of general exFAT hard-link support.
Deletion, truncate, overwrite and replacement now retain an extent while another
published file still owns it. The last owner releases it. Failed overwrite
restores the old live descriptor before reclaiming an unpublished new extent.
The diagnostic checker's exact-alias exception now also requires matching
initialized length and FAT/contiguous layout, so a different chain cannot borrow
another file's alias exemption.

**Inconsistent imported ownership could remain writable.** Block mounts with a
supported single-cluster bitmap now run the allocation checker before metadata
sidecar recovery or user mutation. Partial overlaps, referenced-but-free clusters,
unreferenced allocated clusters, checker I/O/OOM failures and other checker
errors latch read-only mode. Existing dirty/unsupported-bitmap refusal remains.
Readable media stays available for inspection; this does not certify its returned
contents, repair corruption or authorize mutation through raw device interfaces.
Memory-image mounts are already immutable. No automatic disk repair is added.

The checked alias tuple is first cluster, logical/initialized size, allocation
count and FAT-chain mode. With identical chain mode and start, aliases traverse
the same volume FAT or contiguous extent. Partial overlap and file/directory or
system-area overlap are not supported aliases. The existing checker remains
bounded by the supported allocation-bitmap representation; multi-cluster bitmap
mutation and full system-stream/boot/upcase validation are still separate work.

### Verification and lifetime disposition

Evidence is retained in `build/review/exfat-ownership-audit/`:

- All thirteen initial new modes fail on the previous runtime. Failures include
  surviving-alias byte corruption, allocation inconsistencies, duplicate imports
  and removal of an unrelated VFS entry by failed-mount cleanup.
- The expanded suite has 47 passing host modes, normally and under ASan/UBSan.
  Additional fault cases cover mount allocation failure/retry, checker OOM/read
  failure, and overwrite failure before/at metadata publication. Eight compiled
  negative controls are caught, including the overwrite rollback-order defect.
- An initial fault assertion expected a clean allocation map after a failed
  metadata write. Existing quarantine deliberately blocks cleanup writes once
  publication may be uncertain; one new allocation remains for offline recovery.
  That failed assertion/debug log is retained. Separate tests now require clean
  reclamation after a pre-publication read failure and retained/quarantined state
  after a metadata-write failure, with the original aliases preserved in both.
- The generator's independent recursive entry check and existing entry-layout
  host test pass with the matched build. Dedicated guest modes exercise alias
  deletion/overwrite/replacement and bitmap-corruption read-only behavior through
  public file APIs. All six sequential VMs pass: aliases (16.52 s), quarantine
  (16.43 s), import (16.53 s), corrupt-entry/fallback (16.41 s), complete threads
  (40.35 s), and libuv (10.17 s with the 60-second completion budget). The
  `vms/result.json` receipt and source/output hash manifest identify these runs.

The normal monitor, workspace and workspace-image teardown callers check busy
prefixes. The process layer includes current directories and retained linked
file/directory descriptions; unlink/replacement promotes surviving snapshots to
process-owned memory and clears their backend node. No new open-reference defect
was established by this trace, and no competing changes to process admission were
made. The low-level unmount API still relies on caller lifetime/serialization
contracts; arbitrary new callers, hardware removal and every interleaving are
not certified by this pass. The namespace teardown defect above is corrected.

Additional disposable VM commands:

```sh
python3 tools/exfat_import_smoke.py --ownership
python3 tools/exfat_import_smoke.py --ownership-corrupt
```

### Integration verification

After publishing allocation checkpoint `28bdb374`, main advanced to `0858e8e6`
with the desktop lifecycle and UART/startup trace changes from PR181/PR185.
Integrating that baseline conflicted only in appended Makefile test targets;
both the exFAT and desktop targets were retained. The nine filesystem
implementation/test source files are unchanged from the published checkpoint.

The integrated build and all 47 host modes pass, as do four generator groups,
the 40-case shared-window host test and the independent 372-entry image check.
All six sequential VMs pass again: aliases 16.46 s, quarantine 16.39 s, import
16.45 s, corrupt-entry/fallback 16.33 s, complete threads 39.89 s, and libuv
10.06 s. Evidence is in `integrated-*.log`, `integrated-vms/result.json`,
`integrated-tested-sha256.json` and `integrated-macos-build.json` under the same
evidence directory. These are emulated filesystem/integration checks; they do
not replace the desktop agent's UI or physical-machine acceptance.

## Physical device claims and raw leases

The next checkpoint integrates main `5bcd061c` (PR184 worker-pipe admission)
and closes two reproducible admission gaps. Distinct block views of overlapping
physical bytes could mount simultaneously because exFAT compared device pointers.
The existing workspace-image adapter also allowed the parked outer volume to
mount at another path while the inner image was active: revoking write callbacks
did not reserve the storage. Both failures are reproduced against the previous
runtime in `build/review/storage-claims/baseline-corrected/`.

MBR/GPT partitions and the diagnostic image now declare their backing device and
byte offset. A bounded resolver checks every view edge, parent bounds, overflow
and cyclic chains. A CPU0, interrupt-protected claim table admits mounts by
physical root/range before reading media. Entire block views are reserved
conservatively, including any tail beyond the declared filesystem size. Disjoint
and touching ranges remain independent. Mount rollback and unmount release the
claim; monotonically increasing tokens prevent stale releases from revoking a
new owner. Table/token exhaustion refuses admission.

Scratch and image diagnostics reserve the entire raw transport after parking the
workspace. The whole-device scope matches USB write/flush callback revocation.
Preflight refuses handoff while a sibling partition is mounted, before unmount,
callback changes or scratch creation. USB write enable/disable uses the same
check. Only the designated contained image view may mount under an image lease;
a copied view descriptor is not delegated. The child mount must retire before
its parent claim can be released. Clean refusal/close permits later admission.
Failed raw I/O retains the exclusion until reboot, even after callbacks and VFS
nodes have been revoked, so another mount cannot bypass workspace quarantine.

Claims are kernel admission metadata, not protection against arbitrary kernel
code invoking a saved driver callback. View descriptors/backing mappings must
remain stable while claimed. `block_parent` retains its transport/flush meaning;
`block_raw_range` resolves physical identity for claims and raw extents. This
checkpoint does not turn every block read into an exclusive lease or change the
read-only battery diagnostic's parked-identity checks.

Verification adds four actual exFAT/VFS modes (51 total), real block/partition
range and claim tests, workspace/USB handoff tests, and seven portable synthetic
image-adapter runs: workload, two pause/close points, partial-write failure,
flush failure, bad marker and bad inner identity. Image tests compare all outer
bytes outside the leased file, validate resulting contents and test competing
mounts, clean retry and poisoned refusal. ASan/UBSan covers the import suite and
block/workspace/image adapters. Six compiled negative controls detect missing
physical overlap, partition identity, active-child retention, USB handoff checks
and scratch/image quarantine. The existing AHCI host test also passes.

The initial alias probe was placed after an existing mount and failed on the
wrong assertion; that log is retained and is not defect evidence. The corrected
probe fails on accepting the overlapping mount. Initial macOS aggregate commands
also encountered existing GNU linker flags, x86 assembly and a `cfmakeraw` host
fixture incompatibility. Focused targets were built with the documented Darwin
linker/x86 compiler wrapper; the unrelated sync-command aggregate is not claimed
as passing. Portable synthetic fixtures do not replace Linux exfatprogs fault
sweeps or native USB acceptance.

The matched OS build and independent image check (372 entry sets in 16
directories) pass. Six sequential disposable x86_64 VMs pass: aliases 16.57 s,
bitmap quarantine 16.42 s, imports 16.43 s, corrupt-entry fallback 16.40 s,
complete threads 39.75 s and libuv 10.13 s (60-second completion budget).
`build/review/storage-claims/` retains the build receipt, source/output hashes,
host/sanitizer logs, negative controls and `vms/result.json`. These guests check
integration with the boot disk and consumers; the raw USB lease defects are
exercised by the directed host adapters, not claimed as native hardware trials.

### Follow-on removal trace

USB transport failure latches `failed`; AHCI uncertain completion takes the disk
offline and keeps DMA storage pinned. The block registry has no general runtime
unregister/reuse API; these registered descriptors live until reboot. The image
view is static and its claim/mount now retires before clean reuse. No additional
registered-device use-after-free was confirmed. General hot-unplug, generation
changes, asynchronous teardown and crash-recovery acceptance remain open; adding
an unregister API requires explicit mount/claim retirement and cache invalidation.

## Remaining audit queue

1. Device removal/generation changes and caller serialization during unmount.
   Overlapping declared views and mutating raw leases now have regression-tested
   admission. Reproduce any further lifetime gap before broadening process or
   low-level hot-unplug mechanisms.
2. Mutation failure/recovery boundaries and durable metadata. Existing serialized
   replacement and dirty-volume quarantine are not a journal; interrupted rename
   and power-loss atomicity remain unproven. Multi-cluster bitmap mutation,
   boot checksum/backup recovery, upcase/system-stream integrity, full Unicode and
   cross-cluster/extended entry sets remain outside this checkpoint.
3. Networking/socket lifecycle, VM/syscall validation, process/resource isolation
   and remaining documentation reconciliation follow as separately bounded work.

The [overall inventory](audit-progress-2026-10-26.md) tracks what has been audited
without assigning a misleading whole-system percentage.
