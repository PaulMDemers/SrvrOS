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

## Identity invalidation and serialized teardown

This checkpoint starts from `a35e2c33` and incorporates main `19a6fe05`'s
Air6,1 documentation update. Five original directed probes reproduced teardown
while a foreign storage owner, read callback, first mutation write, checker or
mount scan was active. The old API could unregister nodes and free backend
storage still used by the suspended operation. These are deterministic callback
and ownership fixtures, not claims of a reproduced native-machine crash.

Mount, read, mutation, allocation-check and exclusive-range operations now hold
the CPU0 storage owner across their full backend operation. An active-operation
count also refuses teardown reentered by the same owner. Unmount checks linked
process files/directories and current directories under that owner, closing the
window between an earlier caller busy check and teardown. Boot-time operation
without scheduler I/O services remains supported. Nested metadata operations
remain allowed; teardown is refused until a later caller retry rather than blocking
inside a callback. The monitor reports refusal as busy or not mounted.

Runtime USB command failure exposed a separate identity gap. A valid CSW with
failed-command status returned an error without quarantining the published
device, and a later write could succeed using the old geometry and mount state.
The test reproduces that sequence for read, write and flush. No runtime sense or
media-generation recovery exists to establish that the medium is unchanged, so
published command failures now invalidate the device conservatively. Startup's
bounded Unit Attention handling remains available before publication. Repeating
`usb_storage_attach` on a published storage object is refused before any probe or
context change; the old implementation accepted that unsupported republication.

A terminal `offline` state propagates through declared backing views. USB
transport/runtime-status failure and AHCI uncertain completion invalidate the
physical lineage and cached aliases. The block layer checks health before cache
hits, writes, flushes and claims, including zero-length I/O; it cannot turn old
cached bytes into a successful request after invalidation. Descriptors, claims
and DMA storage are retained. Clean owner teardown can still release its claim,
but invalidation has no runtime clear/rebind API. This is fail-closed handling of
an unknown generation, not hot-plug or generation-counter recovery. Already
copied process snapshots are not retroactively erased.

The import/lifecycle suite now has 61 passing modes, normally and with ASan/UBSan.
New cases cover foreign and same-owner teardown, mount scans, reads, first writes,
checks, raw-range validation, busy-reference rechecks, denied admission and boot
fallback. Every mode checks that operation ownership is balanced. Block tests
cover cached/uncached aliases, no transport calls after invalidation, claim
refusal/release and retained descriptors. USB read/write/flush command failures,
republication refusal and AHCI invalidation are covered by their actual-driver
host tests. Workspace scratch invalidation retains quarantine. The seven
existing synthetic image-adapter cases still pass; block/workspace/image fixtures
also pass sanitizers. Eight compiled negative controls detect removal of nested
and foreign teardown guards, the busy recheck, read pin, USB status quarantine,
republication refusal, cached-read health check and AHCI invalidation.

Evidence is retained in `build/review/storage-lifetime/`: original failures,
host/sanitizer logs, negative controls, matched build receipt and source/output
hashes. Native media removal/replacement was not performed. The registered block
registry remains boot-lifetime, with no general unregister API; device topology
must remain stable while referenced. Raw kernel callbacks and VFS pointers held
outside their owning operation are not a general reference-counted hot-plug API.

The matched build, four generator groups and recursive 372-entry image check
pass. Six sequential disposable VMs pass: aliases 16.55 s, bitmap quarantine
16.40 s, imports 16.55 s, corrupt-entry fallback 16.40 s, complete threads
40.08 s and libuv 10.06 s. `vms/result.json` records the exact commands and results.
The guest runs validate the integrated boot/storage consumers; the teardown and
runtime-failure schedules are forced by the directed host fixtures.

### Final baseline integration

After checkpoint `801ac9ca`, main `4d195e25` incorporated PR187's retained desktop
loop diagnostics. The merge was clean and all 19 tracked storage source/test
files in the evidence manifest remained unchanged. The integrated storage host
gate (61 modes plus adapters), three loop-timing tests, matched build and
372-entry generated-image check pass. All six VMs pass again: aliases 16.52 s,
quarantine 16.46 s, imports 16.45 s, corrupt-entry fallback 16.44 s, complete
threads 40.03 s and libuv 10.06 s. `integrated-*.log`,
`integrated-tested-sha256.json`, `integrated-macos-build.json` and
`integrated-vms/result.json` retain the combined baseline's evidence. No desktop
UI or native timing acceptance is inferred from these storage/consumer checks.

### Follow-on audit disposition

The next boundary is mutation failure/recovery. Source tracing confirms dirty
marking, durable-clean publication and quarantine on uncertain writes, but these
are not a journal. Namespace-operation interruption, sidecar recovery and
crash-consistent rename/replacement require a separate fault-injection pass.
The current change adds no automatic repair or online reattachment.

## Rename/replacement and sidecar checkpoint

This pass starts from `3c0c84a7`, including main `4d195e25`/PR187. PR19's
Wi-Fi/logging work was inspected and has no storage-file overlap. The existing
relocated-rename path already flushes its destination before retiring its source;
replacement publishes the destination allocation before releasing the old one.
These paths remain serialized operations with dirty-volume quarantine, not a
journal or an atomic power-loss rename protocol.

Directed tests confirmed and corrected the following sidecar defects:

- A malformed suffix could return failure after earlier permission/identity/time
  records had already been applied. Validation and application now use two
  passes over the same read buffer, with no second disk read between them.
- A record could modify a node outside the mounted filesystem. Records now must
  belong to that mount and exclude the internal sidecar directory. Printable
  path bytes, decimal overflow checks and required record-ending newlines also
  reject embedded NULs, wrapping fields and mid-record truncation before applying
  any metadata. This is structural validation, not a document checksum.
- Dirty volumes still replayed path-keyed metadata even though an interrupted
  replacement can change which inode owns a path. Quarantined imports now retain
  default metadata and unknown times without sidecar replay or recovery writes.
- Failure to read a valid temporary sidecar was interpreted as invalid content
  and caused deletion. Failed/invalid temporary files are retained on mount for
  retry or offline inspection; a later explicit sync may replace them.
- Sync and mount-time promotion deleted the committed sidecar before renaming
  the temporary file. Both now use replacement when a committed file exists.
  The old record remains until publication, and uncertain writes quarantine the
  volume. Prefix interruption tests detect the original missing-sidecar window.
- VFS `chmod` and explicit time updates hid a failed sidecar sync and returned
  success. The persistence callback now returns its result; these operations
  restore live metadata and propagate I/O failure through the existing process
  error paths. A failed persistence operation may have changed disk bytes, so
  restoration of live metadata is not a durable rollback guarantee.

The original six parser/replay probes failed, the separate temporary-read probe
failed, and two persistence-result probes failed before their corresponding fixes.
The original production source also fails the new file-backed interruption
sweep's committed-sidecar-presence assertion. Successful ordinary import and
clean temporary promotion controls passed before and after the fixes.

All 72 import/lifecycle/metadata modes pass normally and under ASan/UBSan.
The real file-backed driver passes 144 injected boundaries across rename,
replacement and mount-time promotion: every write and flush is tested for
failure/interruption, and every write has a half-write cut. Clean states pass
allocation checking; dirty states either refuse import or mount read-only without
recovery writes. Unrelated anchor data remains intact, full-write cuts retain the
committed sidecar name, and completed operations remount with source contents
and permissions. This uses a minimal synthetic volume and ordered persisted
prefixes, not exfatprogs, controller reordering or physical sector guarantees.
The storage/lease adapters, VFS timestamp tests, matched OS build, four generator
groups and recursive 372-entry image check also pass. Six sequential disposable
VMs pass: aliases 17.20 s, bitmap quarantine 16.71 s, imports 17.16 s,
corrupt-entry fallback 16.70 s, complete threads 40.77 s and libuv 10.74 s.
Main remains `4d195e25`; refreshed PR19 at `35041be0` changes separate Wi-Fi,
logging and Make targets, with no exFAT/VFS overlap.

Evidence is retained under `build/review/rename-sidecar/`, including original
failures, fixed runs, source hashes and build/VM receipts. Early sweep harness
routing mistakes are retained separately and are not counted as production
regressions. The current v1 sidecar has no document checksum or transaction
identity: whole-record truncation, external edits and offline repair can still
require metadata reconstruction. No native media, Actions or automatic repair
was used. Follow-on tracing moves to boot-region/backup checks and system-stream
validation; those remain outside the accepted import profile.

## Boot-region and system-stream checkpoint

This pass starts from `bc024735`, with main `4d195e25` included. PR19 was refreshed
at `f70ee542`; its Wi-Fi/logging and separate Make targets do not overlap the
exFAT implementation. The original driver trusted a single boot sector without
checking its region, accepted missing/duplicate or wrongly identified system
entries, ignored upcase integrity, and treated system allocations as contiguous.
Nineteen original directed probes failed; a subsequent unknown-critical-entry
probe also reproduced acceptance of unsupported metadata.

Both image and block mounts now validate the checksum across the complete boot
region, every repeated checksum word, required jump/zero/signature/usage fields
and extended-sector signatures. Checksum calculation uses the exact header
snapshot used for geometry. A candidate review probe made the header change
between reads and exposed a geometry/checksum mismatch; it now falls back to the
valid backup rather than importing the wrong directory. Read errors and damaged
sector-size bytes have deterministic coverage.

A valid primary remains authoritative. A damaged or disagreeing backup forces
read-only admission. If the primary is invalid/unreadable, the reader probes the
four supported sector sizes and accepts only a unique valid backup, read-only.
Multiple valid candidates or no valid region refuse import. Backup flags and
usage estimates do not supersede a valid primary. This is conservative access,
not repair: no boot region is rewritten and quarantined mounts do not replay
sidecars. The [Microsoft specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification)
defines boot checksums, stale backup status fields, system FAT chains and upcase
checksums/mappings; the refusal/fallback policy is SrvrOS's bounded implementation.

Root scanning now requires exactly one bitmap and one upcase descriptor, rejects
wrong bitmap identity, duplicates, misplaced system records and unknown active
critical entries, and validates exact system FAT-chain termination. The upcase
stream must have a matching checksum, complete compressed/uncompressed coverage
and mandatory ASCII mappings. The bounded accepted length is even and at most
128 KiB; filenames remain ASCII. Allocation checks follow system FAT chains, so
a valid fragmented upcase table no longer produces a false ownership conflict.
Multi-cluster bitmap graph checking remains unsupported and is explicitly refused.

The repository generator previously emitted neither checked boot regions nor a
valid upcase table. It now writes primary and backup checksum regions and a full
compressed table with ASCII case mappings and identity mappings elsewhere. The
portable fixtures use the same format policy, with independent checksum/mapping
verification. Rebuild old generated images; do not patch persistent user media
as part of consuming this change.

Follow-on statfs tracing confirmed two adjacent defects. A read callback could
unmount the volume during statistics collection, and a bitmap extending beyond
one cluster was truncated when counting free space. The operation now holds
storage ownership, and the counter follows the complete bitmap FAT chain.
Contiguous and fragmented multi-cluster statistics are tested while mutation and
full graph checking remain refused for those volumes. An earlier suspected
bitmap buffer overread was ruled out: the existing helper clamps its read size.
Its retained failed hypothesis is not counted as a confirmed memory-safety bug.

Validation: all 100 directed modes pass normally and under ASan/UBSan. The
file-backed 29-case matrix passes normally and under ASan/UBSan across all four
sector sizes, including backup fallback, damaged/conflicting copies, stale flags,
ambiguous backup refusal and multi-cluster bitmap statistics. The 144 sidecar/
rename interruption boundaries, storage/lease adapters, matched OS build, six
generator groups and recursive 372-entry check also pass. Evidence is retained
under `build/review/boot-system/`, including original failures, candidate fixes,
the ruled-out hypothesis, source hashes and build/VM receipts. Six sequential
VMs pass: aliases 16.44 s, bitmap quarantine 16.51 s, imports 16.67 s,
corrupt-entry fallback 16.53 s, complete threads 41.49 s and libuv 10.84 s.
The final remote refresh still has main `4d195e25` and PR19 `f70ee542`.

The next audit boundary is allocation exhaustion and directory-growth rollback.
Boot repair, full Unicode, extended/cross-cluster file entry sets, multi-cluster
bitmap mutation and controller/physical power-loss guarantees remain outside the
accepted profile. No native media trial, Actions dispatch or automatic repair
occurred.

## Allocation exhaustion and directory-growth checkpoint

This pass starts from `16b81077`, with main `4d195e25` included and PR19's
`f70ee542` Wi-Fi/logging changes still separate. Directed tests reproduced clean
refusals being converted into dirty quarantine: directory scanning retired an
end marker before discovering that no extension cluster or allocation buffer
was available. Scanning is now read-only until a usable slot is selected or a
new extension cluster is allocated, initialized and flushed. The tests require
byte-for-byte preservation and writable admission after those refusals.

Additional probes reproduced mutation work for impossible payload/entry widths.
Public create/write/rename admission now rejects payloads exceeding the heap and
entry sets wider than one supported cluster before dirty marking or scratch
allocation. Cluster rounding avoids addition overflow, allocator counts cannot
exceed the heap, and a failed write cannot trigger fragmented-allocation retry
on an already quarantined volume.

Consuming an end marker also exposed stale records after it. A file named
`ghost`, absent from the live namespace, appeared on remount after a new file
was inserted. Both same-cluster and later-cluster cases reproduce this. The
writer now treats the logical end as applying to the rest of the directory
chain, clears undefined trailing records before publication and retains inactive
padding only where needed to reach the selected record. Successful root and
contiguous-to-FAT directory extensions still remount and pass allocation checks.

The long-name growth test exposed a separate signed integer overflow in the
importer's hash rotation under UBSan. The rotation now shifts only the low bit
into bit 15 before adding the ASCII character; generated hash values and the
supported ASCII naming profile are unchanged.

All 108 directed modes pass normally and under ASan/UBSan. The new 193-boundary
root/subdirectory extension sweep passes normally and under ASan/UBSan. It covers
write/flush failure, persisted-prefix interruption and half writes. Completed
extensions pass remount/allocation checks. Dirty cuts either refuse import or
remain read-only without recovery writes, while independently located anchor
bytes survive. An early harness incorrectly tried to traverse a torn root FAT
chain; that failed run is retained and the anchor check now uses its known
physical extent. This does not count an expected refusal as an implementation
regression.

The existing 144 rename/sidecar boundaries, 29 boot-sector matrix cases,
storage/lease gates, matched build, six generator groups and 372-entry image
check pass. Six sequential VMs pass: aliases 16.96 s, bitmap quarantine 16.98 s,
imports 17.02 s, corrupt-entry fallback 16.63 s, complete threads 40.39 s and
libuv 10.72 s. Evidence, original failures and tested source hashes are retained
under `build/review/allocation-growth/`. No native media, Actions or automatic
repair was used.

Follow-on tracing reaches deletion/truncation retirement and low-space sidecar
updates. This checkpoint is not a general disk-space reservation planner:
failures after allocation/data/metadata changes can still quarantine the volume,
including a later sidecar allocation failure. It adds no journal, durable
rollback promise, new errno mapping or physical power-loss acceptance.



### Allocation checkpoint integration verification

Runtime checkpoint `497e2e6d` was merged cleanly with main `ee5468f6` (PR188/189).
The incoming change is confined to the Air6,1 machine record. Every tested source
and output hash is unchanged; `all` needed no recompilation and refreshed the
matched-build receipt. A fresh disposable import VM passes in 17.00 s. The full
six-VM results above remain evidence for the identical runtime artifacts.
`integrated-build.log`, `integrated-macos-build.json` and `integrated-result.json`
retain this verification under `build/review/allocation-growth/`.

## Deletion/truncation retirement and low-space sidecars

The original `91a4f594` backend fails two healthy-volume reproducers: truncate a
three-cluster file with no free clusters, or replace it with a one-cluster file
when exactly one cluster is free. Both have a committed sidecar. Length
publication eagerly invoked the VFS metadata callback, which attempted sidecar
allocation before releasing the old extent. That failure quarantined the mount
and prevented reclamation even though the retired extent would suffice.

Length publication now updates live VFS size/timestamps with its metadata
callback suppressed. The owning operation flushes the new stream, retires the
old allocation, and only then persists the sidecar. The existing recursion guard
and internal-sidecar exclusion are preserved. Directory extension also avoids a
premature nested sync; its outer create/rename operation persists metadata after
completion. No data is overwritten in place and alias ownership still prevents
freeing a surviving name's clusters.

Eight new import modes (116 total) cover full-volume truncation with an existing
or first-created sidecar, one-free-cluster shrink, deletion, empty-directory
removal, and clean refusal of chmod, utime and nonzero replacement without space.
The fixtures allocate every occupied cluster to a real published object and
pass the allocation checker before mutation. Success checks writable remounts,
contents, allocation ownership, mode and updated mtime; clean refusals compare
the entire disk and live metadata. Original failures are retained under
`build/review/retirement-sidecar/baseline/`.

The new `test-exfat-retirement` target covers 323 write/flush failure, full-prefix
cut and half-write boundaries across deletion, directory removal, zero truncate,
shrink, chmod and utime. Clean completed operations verify expected namespace,
contents/metadata and allocation graph; dirty cuts refuse import or stay
read-only without recovery writes. Unrelated anchor bytes survive every cut,
and full-write cuts retain the committed sidecar. The extension sweep now has
161 boundaries instead of 193 because it no longer performs the extra sidecar
sync; the dated 193-case evidence above remains historical.

The process unlink, shared-content and orphan tests cover failure before and
after deletion publication, open descriptions, pathname reuse and last-close.
Their macOS build initially exposed a stub declaration mismatch (`uint64_t`
versus the heap API's `size_t`); correcting that test signature restores the
existing regressions without changing process behavior. The initial compiler
failure is retained in `host.log` and the passing rerun in `process.log`.

Follow-on source tracing confirms read-all buffers are released on read/FAT
failure and outputs are published only on success; the backend scope prevents
teardown during those reads. No additional defect is confirmed by that trace.
Process writeback retry after clean space refusal remains to be characterized:
its content object currently latches any backend flush failure, while the backend
can distinguish a clean refusal from a quarantined partial mutation. Changing
that policy requires focused process/API evidence, not an assumption that every
I/O error is safe to retry.

This checkpoint adds no general space reservation, journal, durable rollback or
ENOSPC-specific syscall mapping. A later sidecar failure may still quarantine a
partially completed mutation, and allocation retained by another alias is not
available to fund metadata. Physical power loss, controller reordering and
Linux exfatprogs repair remain separate gates.

### Retirement checkpoint verification

All 116 import modes pass normally and under ASan/UBSan; all 323 retirement fault
boundaries pass normally and under ASan/UBSan. The current 161 growth boundaries,
144 rename/promotion boundaries, 29 boot matrix cases, storage/lease tests and
process open-content/orphan tests pass. The matched build, six generator groups
and recursive 372-entry image check pass. Six sequential disposable VMs pass:
aliases 17.06 s, quarantine 16.60 s, imports 16.99 s, corrupt-entry 16.68 s, threads 40.18 s, libuv 10.61 s.

Evidence and source/artifact hashes are retained under
`build/review/retirement-sidecar/`; the two original regression failures match
`91a4f594` byte-for-byte for the backend source. The compiler-stub failure is
retained separately and is not a test pass. Main remains `ee5468f6`; refreshed
PR190 (`f1587a2f`) and PR19 (`9a50f4dc`) remain separate with no exFAT overlap.
Documentation/link and whitespace checks pass. No native hardware test or
Actions dispatch was performed.

### Retirement checkpoint integrated verification

Integration `9510ce8c` includes main `4ee18f69` (PR190). The Makefile conflict
was resolved by preserving both test targets; exFAT source was unchanged.
The scheduler admission/trace/syscall gates, five loop-timing tests, six scheduler
activity tests and desktop-session fixture pass. The latter uses the documented
Darwin wrapper with `-Wno-error=array-bounds` for only
`build/tests/desktop-session-host`: Clang diagnoses the unselected inline-array
branch at index 31 after asserted dynamic growth to 32/128 slots. Both warnings
remain visible, and the original strict compile failure is retained; no desktop
implementation was changed to silence it.

The rebuilt 116-mode import suite, 323 retirement boundaries, storage/lease and
process tests pass. The matched full build, six generator groups and recursive
372-entry image check pass. A fresh six-VM pass on the integrated artifacts gives:
aliases 16.53 s, quarantine 16.44 s, imports 16.51 s, corrupt-entry 16.38 s, threads 40.02 s, libuv 10.11 s.

Commands, failures, passing reruns, build receipt and tested source/artifact
hashes are under `build/review/retirement-sidecar/integration/`. The earlier
ASan/UBSan evidence still covers the unchanged exFAT implementation and fixtures;
it is distinct from the new integrated guest evidence. This final record changes
documentation only. PR182 remains draft; the next audit question is process
writeback retry after clean space refusal.

## Process writeback retry after clean refusal

Two confirmed process defects are corrected:

- At `834fbff0`, a healthy full-volume flush poisoned the shared content object
  permanently. Reclaiming space could not help: later flushes returned the
  sticky error without entering exFAT. The public process regression and a
  bridge linking the original process source to real exFAT/VFS reproduce this.
- Once the last writer closed with a flush error, a reader could still retain
  dirty content. Global sync walked only write descriptions, skipped that
  survivor and falsely reported success. A separate regression captures this
  after the first correction, before changing the sync traversal.

The backend now returns conservative retry eligibility captured while it owns
filesystem exclusion. An admitted, clean outer operation can retry after a
capacity/heap reservation failure before dirty marking, or after refusal with no
allocation/data/directory writes and successful clean-marker restoration/flush.
Partial mutation, failed marker I/O, nested operations and invalid/unsupported or
ownership/device admission refusals do not qualify. The original boolean write
APIs retain their behavior; this is an internal process-writeback result, not a
new syscall or errno mapping. A post-return writable probe is not used.

Process writeback still reports the first failure, retaining dirty data and
shared identity for a later explicit attempt. Global sync visits each live
content object once, including reader-only survivors. Uncertain failures remain
sticky; final-close and process-exit errors are still reported, and the final
reference's buffer is released even when close fails.

Seven new import modes (123 total) test clean space refusal/retry, reservation
OOM, entry publication, dirty marking, clean-marker write/flush failure and
invalid requests. The new `test-process-writeback` bridge links real process
content code and real exFAT/VFS: fill the volume, refuse twice, free a filler,
flush the same buffer, remount and verify bytes/allocation ownership. Public
process regressions cover shared opens, readers and dup aliases, offsets,
readiness, one-attempt-per-content sync, reader-only survivors, hard failure and
exit behavior. The original-source failure is retained in
`build/review/writeback-retry/real-baseline/`; model regressions are in
`baseline.log` and `sync-survivor-baseline.log`.

Harness corrections are retained separately: the new bridge initially omitted
the Limine include path, the relocated original source also needed the kernel
source include path, and the OOM probe initially checked its allocation counter
after a readback allocation. These are not runtime defect evidence. The corrected
probe checks the failure boundary before readback.

Follow-on tracing confirms `fsync` and `fdatasync` expose process/transport flush
errors, and `block_flush_all` does not skip offline writable devices as successes.
This libc's `fflush` treats its extra fsync as best effort and `sync()` is void;
callers needing a persistence result must use explicit fsync and check close.
No additional defect is confirmed by that trace. Descriptor inheritance and
delayed-error ownership across process exit are the next bounded audit question.

### Writeback checkpoint verification

All 123 import modes, the real process/exFAT bridge and public process-output
regressions pass normally and under ASan/UBSan. The 323 retirement, 161 growth,
144 rename/promotion and 29 boot-matrix boundaries/cases pass normally, along
with storage/lease gates. The matched build, six generator groups and recursive
372-entry image check pass. Six sequential disposable VMs pass:
aliases 16.53 s, quarantine 16.47 s, imports 16.50 s, corrupt-entry 16.41 s, threads 40.53 s, libuv 10.18 s.

Evidence, original failures, harness corrections and tested hashes remain under
`build/review/writeback-retry/`. Main remains `4ee18f69`; PR19 at `e04e2dc0` has
no process/exFAT/VFS overlap. Documentation/link and whitespace checks pass.
No native install, Actions dispatch or PR merge occurred. Follow-on syscall/libc
tracing confirmed no further defect; descriptor inheritance and delayed-error
ownership across process exit are next.

## Descriptor inheritance and exit-error checkpoint (2026-10-28)

Integrated main `5a94d755` through PR230 in `647ee564` before testing. The
Air6,1 process-admission and Air7,2 desktop-runtime tips are included in main;
PR19 (`114dee34`) remains separate Wi-Fi work without process/exFAT/VFS overlap.

No additional runtime defect was confirmed. New production-helper regressions
exercise real descriptor inheritance, both exit orders, shared offsets, clean
versus hard writeback errors, explicit nonzero status, CLOEXEC/reuse, partial
clone failure and three-stream setup rollback. A child with only an inherited
reader retains pending dirty content after the parent's final writer closes;
clean errors recover through global sync, while hard errors survive until final
release. Failed setup releases acquired references without flushing parent data.

The supported profile now states the existing exit policy: redirected standard
files are explicitly flushed; other inherited descriptions can remain buffered
until explicit flush/sync or last writer close. Exit zero is not a general
persistence guarantee. A failed flush turns zero status into one, preserves an
explicit nonzero status, and does not consume an error for surviving aliases.
These host tests exercise file teardown helpers, not full asynchronous process
termination or exhaustive fork/exec interleavings. The real process/exFAT bridge
continues to test the backend's clean-refusal contract.

Follow-on tracing reached `net_hold`, `net_close` and `net_process_cleanup`:
references retained by another process survive creator cleanup. Final TCP close
can start FIN handling, while creator cleanup can retire a zero-reference
connection. Queued output, FIN completion and creator-versus-inheritor exit need
a directed network fixture before drawing a correctness conclusion. This is the
next bounded audit question, not a confirmed bug or completed network audit.

### Inheritance checkpoint verification

All directed inheritance cases pass without a production-code change. The final
process-output fixture and real process/exFAT bridge pass normally and under
ASan/UBSan. Normal storage validation passes all 123 import modes, 323 retirement,
161 growth, 144 rename/promotion and 29 boot-matrix boundaries/cases, plus storage
and lease gates. Integrated-main checks pass for process lifetime/service/events,
lifecycle waits, I/O scheduler, scheduler trace/activity and syscall I/O.

The matched OS build, six generator groups and 372-entry generated-image check
pass. Seven sequential disposable VM gates pass: aliases 16.80 s, quarantine 16.47 s, imports 16.55 s, corrupt-entry 16.50 s, threads 40.33 s, libuv 10.20 s, lifetime 22.95 s.
The lifetime gate covers two runs, 256 sequential apps, 32 competing-waiter pairs
and PTY/job-control regression. Documentation checks resolve 692 local links;
whitespace checks pass. Evidence, source/artifact hashes and VM receipts are under
`build/review/inheritance-exit/`. Earlier broader sanitizer evidence remains dated
under previous checkpoints; full Linux test-core and native hardware acceptance
are not claimed. No Actions dispatch or PR merge occurred.

## TCP ownership follow-on (2026-10-28)

The [TCP lifetime audit](tcp-lifetime-audit-2026-10-28.md) continues PR182 beyond
file writeback. It confirms and corrects creator-exit loss of retransmission/FIN
state and inherited listener backlog, missing deferred-FIN close deadlines,
ignored deferred-send failure and premature TIME-WAIT before local FIN ACK.
Eight deterministic real-stack modes cover these transitions and abort controls.
This does not reopen the completed file-writeback checkpoint or claim a complete
network audit. Established ACK bounds, sequence wrap and stale/closing handles
are the next bounded question.

## TCP ACK and handle follow-on (2026-10-28)

The [TCP audit](tcp-lifetime-audit-2026-10-28.md) now covers unsent-data ACK
rejection, outbound SYN-ACK validation and application access to protocol-owned
closing slots. It fixes acknowledgment/handle admission and final-close wakeups;
wrap and slot-reuse controls pass. Fifteen modes retain the earlier close/exit
coverage. This is network-core work; earlier storage evidence remains dated.

## TCP receive and terminal-state follow-on (2026-10-28)

The [TCP audit](tcp-lifetime-audit-2026-10-28.md) now covers reset/receive admission,
repeated SYN and descriptor ownership after terminal expiry or TIME-WAIT pressure.
Referenced errors and orderly EOF/data survive until final close. The fixture has
23 modes; seven of eight new modes fail on the original source, with duplicate/
overlap refusal as a passing control. Storage coverage remains dated separately.

## Remaining audit queue

1. Actual hot removal/replacement and a future unregister/rebind lifecycle.
   Terminal identity invalidation and CPU0 backend-operation/teardown admission
   now have directed coverage. They do not certify live device recovery or
   arbitrary retained kernel pointers.
2. TCP receive-buffer allocation failure and partial-ACK retransmission are next.
   RST/receive admission and terminal-error/EOF retention now have directed
   coverage, including timer expiry and TIME-WAIT pressure.
   Established ACK bounds, wrap and stale/closing application handles now have
   directed coverage.
   TCP final-close, deferred FIN and creator/inheritor ownership have directed
   coverage recorded in the linked lifetime audit.
   Descriptor inheritance/exit-error helpers now have directed coverage; full
   concurrent fork/exec/kill fault injection remains open.
   Clean writeback retry, deletion/truncation and low-space sidecars have directed
   coverage. Complete boot
   regions and bounded system streams now have directed validation coverage;
   automatic repair, multi-cluster bitmap mutation, full Unicode and extended
   file entry sets remain unsupported. Controller reordering, physical power
   loss, journaled rename and metadata reconstruction after offline repair remain
   unaccepted.
3. Networking/socket lifecycle, VM/syscall validation, process/resource isolation
   and remaining documentation reconciliation follow as separately bounded work.

The [overall inventory](audit-progress-2026-10-26.md) tracks what has been audited
without assigning a misleading whole-system percentage.
