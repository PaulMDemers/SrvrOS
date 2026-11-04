# Filesystem layout and root contract

Updated November 3, 2026. Global activation follows the explicit namespace proof
in [PR308](https://github.com/PaulMDemers/SrvrOS/pull/308).

## PR332 merge review and next bounded milestone

User authorized review and merge if ready. Reviewed `eba6b83b` against unchanged
main `89c210db`; no blocking finding. The CLI must reach stage=flush with all
12,289 accepted bytes and no offset-restore error; the host additionally checks
ATA write/flush failure on the third target. Open refusal cannot pass. Scope
validation confines both filters to distinct fixture images and leaves installed
root unfiltered. Calibration and reopen carry no filters; default modes preserve
their existing behavior. No kernel/process/block/driver implementation changes.

Revalidated all 79 preserved final image/log hashes, raw healthy bytes, the
flush case's unacknowledged payload, source/target hashes, failed-attempt output,
explicit reboot refusal and both restart readbacks. The five-boot proof at
`4c368c23` remains applicable: `eba6b83b` changes only documentation and builds
identical guest payloads. The existing 11-case canonical/CLI proof also remains
applicable. Fresh fsync/namespace/usercopy/I/O/helper/CLI/scope and documentation
checks pass. Unchanged runtime evidence was inspected rather than relabeled as
a new VM run. Review record and fresh host log are retained outside build at
`.local/filesystem-evidence/pr332-review/`.

PR317 at `c9f4f306` remains draft, with independent terminal xHCI stop/drain and
network callback retirement. No overlap or dependency is established for this
proof-only merge. Review VMs 61610, 6233 and 85869 remain running and untouched;
no new VM, physical trial or hosted Actions run was needed for this review.
The destination-writeback qualification target is complete.

Next proposed bounded milestone: **installed-root managed USB workspace salvage
and failed-retirement proof**. Current guest proofs use raw AHCI mounts; host
coverage already exercises managed workspace refusal. Start by reviewing the
prepared disposable USB source and write-fault placement, coordinating transport
and lifetime assumptions with PR317. Boot from installed root, use the actual
`workspace mount/sync/unmount` controller, retain dirty source data through CLI
salvage to healthy `/home`, and verify failed sync/retirement both while the
reader is held and after deliberate final close. Check healthy-copy bytes after
controlled restart and preserve `/fat` and `/work` compatibility. No physical
unplug/reset, forced retirement, automatic repair or backing reclamation is
included. This is proposed work, not implementation started by the merge review.

## Installed-root destination-writeback qualification — verified

User authorized this bounded proof on main `10890eed`, with PR317 coordination
and preservation of review VMs 61610, 6233 and 85869. The isolated filesystem
checkout owns guest sysabi proof, a fixture-owned third target, host calibration/
fault rules, scope guards and documentation. No process/syscall/block/driver
implementation changes were needed. Manual GPT mounts remain separate from
managed USB workspace retirement.

### Reviewed fault placement

A separate installed EFI calibration boot exclusively creates and closes the
same empty target pathname with the CLI's flags/mode; QMP records 50 target
writes. Each real case starts from fresh images. Counted blkdebug write states
admit all creation writes and the fsync dirty-marker write. The write case fails
the next allocation write (ATA 0x35). The flush case activates a flush-only EIO
on that allocation write; ordinary writes continue, then the payload barrier
fails (ATA 0xea). This avoids counting redundant flushes and rejects an open
refusal as proof of writeback failure. The host probes require successful writes/
flush before the boundary and the intended error afterward. qemu-io's flush
reports failure through exit status without printing errno. Rules and events
were reviewed against the [QEMU guide](https://www.qemu.org/docs/master/devel/testing/blkdebug.html)
and [blkdebug implementation](https://github.com/qemu/qemu/blob/master/block/blkdebug.c).

The healthy system disk is never filtered. Source and third target must be
three distinct owned images; only first-boot fault cases accept the two owned
configs. Calibration/reopen cannot carry faults. Eight unittest methods cover
21 scope scenarios, including ordinary/later-boot, external/same-image and
missing-filter refusals before launching QEMU; the positive graph checks unique
boot priorities and the healthy disk's unfiltered path.

### Guest outcome and restart

Both cases use the actual `/bin/salvage` with the independently retained dirty
source reader. After all 12,289 bytes are accepted, the CLI reports stage=flush,
EIO, close EIO and restore-error=0, and explicitly retains an unacknowledged
artifact. The parent verifies fd, offset 37 and every source byte, then explicitly
copies to healthy `/home/.dirty-salvage-copy`. Source errors remain sticky until
deliberate last-reader close after healthy acknowledgment. No automatic retry,
repair or kernel fault hook is introduced.

The failed target remains dirty with its committed empty directory entry.
In the flush case the raw image also contains the full payload: visible bytes
do not establish an acknowledged copy. Both uncertain target images are retained
without repair or allocation-clean claims. Normal global reboot correctly refuses
the failed disks; the harness QMP-stops only its disposable VM, then starts fresh
EFI with reversed system/work order and no filters. The healthy copy reads back
exactly and is cleaned up. Work is byte-identical throughout; the failed target
is byte-identical across restart. Protected image regions and healthy allocation
checks remain clean. This is controlled cold-restart persistence after healthy
file fsync/close, not successful global flush or physical power-loss acceptance.

### Integration and evidence

Initial `bc5dde19` passes calibration plus both case pairs (five EFI lifetimes).
Fresh ordinary installed-root canonical/CLI proof `proof-s4aavezy` passes all 11
cases, including `/fat` and `/work`. Main `89c210db`, including PR330's host
publication checks, then merged cleanly. Combined `4c368c23` preserves identical
guest artifacts and passes a fresh five-boot proof `proof-etsium6m`, focused
fsync/namespace/usercopy/I/O/helper/CLI/scope checks and documentation validation.
Installation host discovery runs 31 tests with one opt-in exhaustive I/O matrix
skipped (30 pass); it is not claimed as a fresh exhaustive PR330 rerun.

Independent checks verify raw healthy bytes, isolated command graphs, target ATA
error types and the flush case's unacknowledged payload. Evidence is in
`build/review/destination-salvage/`; complete final proof images, receipts, rules,
commands and serial logs are independently cloned and SHA-256 checked outside
build at `.local/filesystem-evidence/destination-salvage-4c368c23/`. APFS clones
are separate writable files, never hard links. Initial failed rule-probe
expectations and `proof-wjm7h64c` (duplicate boot priority, rejected before guest
boot) remain recorded; neither counts as a pass.

PR317's owner reports independent terminal xHCI stop/drain and generic network
callback retirement work, without process/syscall/block/installer overlap. Its
hardware/provider audit stays separate and draft; scope feedback does not claim
independent review of this proof. All task VMs stopped; review VMs 61610, 6233 and
85869, primary checkout and physical media remain untouched. No hosted Actions,
physical trial or merge was requested. The bounded qualification is complete;
delivery is a scoped draft PR for independent review.

Reproduce after the matched build:

```sh
make HOST_CC='python3 build/tooling/darwin-host-cc.py' test-file-fsync test-dirty-salvage-scope
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd \
  python3 tools/destination_salvage_smoke.py --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

## PR329 merge review and next bounded milestone

User authorized review and merge if ready. Reviewed `05905122` against actual
main `c90f3522`; no blocking finding. Traced `process_file_flush` through retained
content writeback and the exFAT mutation boundary: successful outer mutation
flushes data/metadata, writes the clean marker and flushes that marker on the
backing parent. Failed transactions cannot return success. Clean/unlinked
content does not need an unrelated global device barrier. Global sync and reboot
still retain their existing all-device behavior.

Fresh exact-head review proof `build/review/dirty-salvage/proof-qhb4hf4j/` passes
both installed EFI boots with the isolated work EIO, exact 12,289-byte healthy
copy after controlled cold restart, unchanged work hash/protected regions and
clean allocation checks. Independently checked raw bytes, command graphs and
explicit global reboot refusal. Fresh ordinary installed-root canonical/CLI
proof `build/review/installed-canonical/proof-0cqpa1bt/` passes all 11 cases,
including `/fat` and `/work` compatibility. Focused fsync/namespace/usercopy/I/O,
helper/CLI, seven scope guards and documentation checks pass. Revalidated the
49 sanitized salvage cases and recorded source hashes; previous threadstress
and sanitized syscall evidence remain applicable to unchanged code.

PR317 at `e2457b1b` remains draft/unmerged. Its owner confirms no current overlap
or new relevant concern; hardware stop/drain/provider retirement remains open
there. That is scope feedback, not independent approval of this flush contract.
Review VMs 61610, 6233 and 85869 stay running; all task VMs stop normally or through
the documented owned-VM restart. Review receipts/logs are backed up outside build
under `.local/filesystem-evidence/pr329-review/`. No hosted Actions or physical
trial was run. The current dirty-source salvage milestone is complete.

Next proposed bounded milestone: **installed-root salvage destination-writeback
failure qualification**. Existing guest proof covers failed-source destinations
and exclusive conflicts; destination write/flush failures currently have host
coverage only. First review fault placement on a third disposable target image.
Prove the real CLI reports failure without losing the caller's retained dirty
reader or offset, then copies to a separate healthy volume and verifies exact
bytes after controlled restart. Preserve uncertain artifacts and the global
reboot guard; coordinate any process/driver changes with PR317. No automatic
repair, retry or failed-volume retirement is included. This is proposed work,
not begun by the merge review.

## Installed-root dirty-source salvage — verified

User authorized mechanism review and installed-root proof on main `1a5a6dee`,
with explicit PR317 coordination and review-VM preservation. Reviewed a host-only
QEMU `blkdebug` one-shot write EIO, attached solely to a distinct fixture-owned
work image. The probe verifies reads succeed, first write fails without changing
bytes, subsequent write succeeds and a separate healthy image remains writable.
The rule uses write_aio/iotype=write/once=on per the
[QEMU blkdebug guide](https://www.qemu.org/docs/master/devel/testing/blkdebug.html).
Seven scope tests reject boot-disk, external-image/config, normal-mode and later
boot attachment; the positive graph test allows exactly one filtered work drive.
No kernel/process/driver fault hook is used.

The guest opens preseeded `/work/bin/true` without create/truncate and holds an
independent reader, stages 12,289 binary bytes in shared process RAM, then hits
the first work write with explicit fsync. AHCI reports task-file EIO and retains
its offline/pinned state. Source fsync/writer close error, global sync error,
failed-work mutation refusal and readable retained data are required. The actual
`/bin/salvage` command first refuses a destination on failed work, then copies
through the inherited reader to `/home/.dirty-salvage-copy`; an exclusive conflict
also fails. Parent offset 37 and all held bytes survive every child. Source errors
stay sticky until deliberate final-reader close after acknowledged healthy copy.

### Demonstrated fsync fix

The first guest run correctly refused healthy-copy success: `syscall_fsync`
flushed the requested content, then called `block_flush_all`, so the unrelated
offline work disk vetoed healthy-file acknowledgment. The actual syscall host
regression fails baseline and passes the fix. File fsync now returns its own
content-writeback result: the current exFAT transaction already completes data,
metadata and clean-marker barriers on its backing volume before success. Clean
descriptions have no deferred writeback, and unlinked RAM content has no remaining
disk file. No stale pathname/mount lookup or new process lifetime state is added.
Global SYS_SYNC and the reboot guard retain their system-wide barrier behavior.
Future asynchronous backends will require an explicit file-scoped durability
contract. The host regression covers clean/read descriptions, content errors,
invalid descriptors and unchanged global-sync failure, normally and under
ASan/UBSan. Namespace/usercopy/I/O admission and helper/CLI tests pass.

PR317's owner confirmed no active syscall/process/AHCI overlap and gave scope
feedback without claiming full durability review. Only production change is the
bounded syscall body/comment; no AHCI, block or process implementation change.
The existing backend failure contract and actual guest readback support the
synchronous durability boundary.

### Restart and persistence evidence

After successful salvage, normal guest reboot correctly reports storage flush
failure because work remains offline. The proof records that refusal, stops only
its own disposable VM through QMP, and starts a fresh EFI VM with reversed disk
order and no filter. This is a controlled cold restart after healthy-file fsync/
close acknowledgment, not a successful normal global flush or arbitrary physical
power-loss qualification. The healthy copy reads back exactly and is cleaned up.
Both boots preserve the entire work-image hash and protected system regions.
Healthy allocation checks report zero leaks/FAT/bitmap errors. Manual GPT `/work`
is not the managed USB controller; raw unmount after releasing references is
fixture teardown, not repaired-work or managed clean-retirement success.

Initial corrected proof `4bbbf2c7` passes. Main `c90f3522` (Haswell/host-compatibility
PR324 and SrvrPorts `ca749517`) subsequently merged cleanly; matched `ee6316a2`
passes the mechanism probe and both dirty-source boots again. Full threadstress,
49 ASan/UBSan host salvage cases and focused syscall/helper/scope tests pass on
that combined source. The default installed-root canonical/CLI proof also passes
all 11 cases at `bb7c4e4a`, before the nonoverlapping main update.

Failed bring-up runs remain in `build/review/dirty-salvage/`: signed-size compile
failure and stale-build refusal; `proof-m08bcofe` exposes the global fsync defect;
`proof-y_fld3g2` proves the fix but times out in an unrelated GUI save path that
also requests global sync. The final dedicated proof deliberately does not count
that GUI step as successful. Passing evidence is `proof-uy53gpes/`, with exact
commands, serial logs, rule, images, receipt, restart record and offline checks;
`independent-review.json`, `tested-build.json`, host/sanitizer/thread logs are in
the parent directory. Backups survive build cleanup at
`.local/filesystem-evidence/dirty-salvage-ee6316a2/`.

Reproduce after the matched build:

```sh
make HOST_CC='python3 build/tooling/darwin-host-cc.py' test-file-fsync test-dirty-salvage-scope
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd \
  python3 tools/dirty_salvage_smoke.py --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

All task VMs stopped; review VMs 61610, 6233 and 85869 and primary checkout remain
untouched. No automatic retry/repair, forced kernel unmount or production reboot
bypass was introduced. The bounded dirty-source salvage target is complete.

## Salvage command — installed-root proof complete

`salvage --fd NUMBER DESTINATION` is the first command-line consumer of
`srv_salvage_copy`. It borrows an inherited seekable descriptor and exclusively
creates the explicit destination. There is no source-path option: the launcher
must arrange descriptor inheritance while retaining its own source reference.
The command never reopens that pathname or overwrites an existing destination.
Descriptor syntax is decimal 0 through INT_MAX. Exit status is 0 for acknowledged
copy, 1 for helper failure and 2 for usage errors. Failures report primary stage,
error, close/restore errors and byte count; uncertain destination artifacts remain.
Process exit releases only the child's inherited reference; the caller owns its
original descriptor. Callers must obey the helper's serialized-offset contract.

The command is packaged at `/fat/bin/salvage`, exposed through `/bin` and
`/usr/bin`. CLI host tests pass normally and under ASan/UBSan for syntax, bounds,
helper arguments and success/failure statuses. Helper tests also pass. Its 49-case
normal/sanitized dirty-data fault evidence remains applicable: every recorded
helper/process/backend source hash is unchanged. The installed-root proof below
covers clean inherited descriptors and is not a new guest fault-injection claim.

Added optional `--salvage-consumer` to the installed-root harness, requiring
`--canonical-paths` and retaining existing defaults. On a clean identity-selected
GPT boot, sysabi holds an open `/work` source, spawns `/bin/salvage` with that fd,
and verifies a 4,099-byte binary copy into `/home`. The parent's shared offset
remains 37, `fcntl` confirms its fd still exists, and a following read returns the
expected source byte. A second child refuses the existing destination with
EEXIST while bytes and offset remain unchanged. A child with an inherited pipe
refuses ESPIPE without creating a destination; parent pipe ownership is intact.
The reversed-disk second boot independently reads both copies through canonical
and `/fat` names, then performs checked cleanup.

Initial source `27b49c9c` passes all 11 cases. Main then advanced to `59928ec9`
(PR326 profile chooser); it merged cleanly and matched source `da1e8d44` passes
all 11 again with the new desktop binaries. This includes full namespace and
home/work persistence, Text/theme restoration and nine no-write recovery refusals.
Four offline allocation checks are clean. Independent image inspection confirms
exact binary destination/system and source/work placement after boot 1 and
absence after boot 2 cleanup; every negative disk hash is unchanged.

Evidence: `build/review/installed-canonical/proof-i1htf98c/` retains receipt,
commands, disks, serial logs and allocation checks. The earlier passing run is
`proof-ms2r7bd0`. `build/review/salvage-cli/` holds host/sanitizer/provenance logs,
matched builds, `tested-build.json` and `independent-review.json`; a backup of
receipts/logs/manifests is retained at
`.local/filesystem-evidence/salvage-cli-da1e8d44/` outside build cleanup.
Reproduce after `sh tools/macos/build.sh -j2 all`:

```sh
make HOST_CC='cc -arch x86_64' test-salvage-cli test-salvage
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd \
  python3 tools/installed_root_smoke.py --canonical-paths --salvage-consumer \
  --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

Coordinated scope with PR317 and the installation owner. No kernel, process,
driver or installer-policy edits were needed. All proof VMs stopped; primary
checkout and review VMs 61610, 6233 and 85869 were preserved. This bounded command
and disposable EFI/GPT proof are complete; application recovery UI, physical
media, automatic repair and guest dirty-writeback fault injection remain separate.

### PR328 merge review and next bounded milestone

User authorized review/merge if ready. Actual main remains `59928ec9`. Review of
the eight-file diff confirms strict descriptor parsing, helper-only source access,
exclusive destination and explicit status/error reporting. The parent/child proof
checks descriptor usability and shared offset after success/refusal, rejects a
nonseekable inherited pipe and verifies persisted bytes on reordered boot.
Revalidated the archived 11-case receipt checksum, all recovery disk-hash pairs
and independent offline image inspection. Only documentation changed after the
qualified `da1e8d44` build. CLI/helper host tests, CLI sanitizer and six provenance
tests pass again; matched build hashes equal the qualified images, whitespace
and 888 local links pass. No submitted correction or unresolved thread was present.

PR317's owner confirms its bounded command-ring/removal cohort has no filesystem,
libc or userspace changes and identifies no overlap blocker. This coordination
is not claimed as independent full-diff approval. Review VMs remain live and
untouched. New review evidence is under `build/review/salvage-cli/review-*`.

Next proposed filesystem milestone: **installed-root dirty-source command salvage
proof**. Start by selecting a test-only fault mechanism that targets just the
disposable `/work` backend. A parent must retain a reader across an actual failed
writeback and invoke the command with that inherited descriptor; verify error
propagation/failed-workspace state, parent ownership/offset, durable healthy-volume
copy and reboot readback. Include a destination failure that reports no successful
salvage while the parent reader stays usable. Reuse the existing host contract and
installed-root harness. Coordinate any necessary driver/process test seam with
PR317 before editing; keep normal driver behavior unchanged. No physical media,
automatic repair, forced unmount or recovery UI is implied. This is a proposal,
not work begun by this merge review.

## Userspace salvage helper — implemented and verified

`srv_salvage_copy(source_fd, destination, result)` in `<srvros/salvage.h>` copies
the complete seekable source into a newly and exclusively created 0600 file.
It never closes the source. The caller must serialize access to that descriptor
and its shared-offset duplicates. The helper records the current offset, rewinds,
copies with bounded stack storage, then attempts to restore the offset on every
exit after capture. Success requires destination fsync, close and restoration.
If restoration itself fails, the helper reports failure and `restore_error`;
it cannot guarantee a restored offset in that exceptional case.

Short reads/writes and EINTR from open/read/write/seek/fsync are handled.
A zero-byte write is an I/O error. Destination close is attempted once, including
when it returns EINTR, because repeating an ambiguous close can hit a reused fd.
The optional result preserves the first error/stage, separate close/restore errors,
acknowledged bytes written and whether a destination descriptor was acquired.
The helper never removes uncertain artifacts, retries failed storage, repairs a
volume or changes source ownership. Even a fully written file is not acknowledged
as successful when cleanup or restoration fails. Callers retain the source until
verified success or an explicit discard decision.

The helper is compiled into the userspace library automatically. `make test-salvage`
runs focused tests for short I/O, interruption at each retryable boundary, empty
source, nonseekable/rewind/open/read/write/zero-write/flush/close/restore failures,
source ownership/position and primary plus secondary errors. Normal and
ASan/UBSan executions pass. Source restoration is checked from a nonzero offset.

The actual-source salvage fixture now invokes this implementation through POSIX
adapters to real process file operations. Clean source `2a5889b0` passes all 49
salvage cases normally and under ASan/UBSan, preserving the dirty reader through
healthy-volume flush/close/remount verification and restoring offset 37 on every
storage result. All 52 writeback cases, 134 existing cases and both negative
controls pass with the new integration. Adapters do not qualify full libc syscall
or usercopy behavior; controlled short/EINTR/restore errors are covered by the
focused helper test instead.

Coordinated with PR317's owner; no production process/kernel/driver edits were
needed. Consumed main `4b5ddb7c` (installation recovery PR325); every recorded
fault-suite source hash still matches. Matched build passes. An ordinary Q35/
VirtIO desktop reaches its first application frame in 4.31 seconds; running state
and screenshot are retained. This smoke checks the linked build, not guest salvage
fault injection. The task VM (29735) was stopped through its own QMP socket;
review VMs 61610, 6233 and 85869 and primary checkout were preserved.

Evidence: `build/review/salvage-helper/` contains focused host/sanitizer logs,
`final-normal/`, `final-sanitize/`, `writeback/`, `legacy/`, `import/`, `negative/`,
build logs and `tested-build.json`. Desktop evidence is under
`.local/macos-vm/salvage-helper/session-20261104T005425.521885Z/`.
Receipts/logs/build manifest are backed up outside build cleanup at
`.local/filesystem-evidence/salvage-helper-2a5889b0/`. Reproduction:

```sh
make HOST_CC='cc -arch x86_64' test-salvage
CC='cc -arch x86_64' python3 tools/test_exfat_import.py --workspace-salvage --sanitize --output build/review/salvage-helper/sanitize
```

The bounded helper target is complete. Application integration, recovery UI,
physical power-loss and automated retry/repair remain separate work.

### PR327 integration review

User authorized review, integration and merge if ready. Actual main remains
`4b5ddb7c`; no conflict or overlapping production process/driver change is present.
Revalidated every fault-suite source hash and all 49 normal + 49 sanitized salvage
results, 52 writeback cases and 134 existing cases. Focused helper normal and
ASan/UBSan checks and both negative controls pass again. Matched build reproduces
the qualified images; whitespace and all 880 local documentation links pass.
No submitted review correction or unresolved thread was present.

Review confirms exclusive creation, caller-owned source lifetime, serialized
shared-offset precondition, first-error preservation, secondary cleanup errors,
one-shot close and restoration failure reporting. The source is never closed,
and success is withheld for any flush, close or restore failure. The API does not
promise source offset recovery when seek itself fails or automatic removal of
uncertain destination artifacts. Existing exact-source host and desktop evidence
remains applicable; no new VM is needed for this documentation-only review update.

PR317's active audit concerns driver DMA/reset/repeated initialization; no libc
salvage, content, close or workspace policy changes were planned. Coordination is
not represented as independent full-diff approval. The bounded helper is ready
for integration; application recovery/UI adoption remains separate optional work.
Review logs are retained under `build/review/salvage-helper/review-*`.

## Retained dirty-data salvage — verified

User authorized this explicit salvage-copy proof on main `0cba30a1`.
Coordinated with the PR317 audit owner, who confirmed no overlapping content,
close or workspace edits and reiterated the held-description lifetime requirement.
No production `process.c` change, new salvage API or recovery policy was needed.

The fixture stages a 2,049-byte binary pattern through real process file writes,
then faults `/work` writeback, latches managed workspace failure, and closes the
writer with its expected I/O error. An existing reader retains the dirty content.
The copy reads only that held description through actual process read/seek calls,
using 127-byte chunks; it never reopens the failed source pathname. Destination
`/home/salvage` is explicitly opened through namespace resolution with exclusive
create, written through actual process file writes, flushed and closed through
the real description-close handler. Transport and unrelated PTY/scheduler wake
branches are host adapters; no full syscall/usercopy or guest fault claim is made.

On success, the source reader remains held through destination fsync/close,
healthy-volume unmount/remount and exact byte readback. Retiring `/fat` invalidates
its aliases as designed; explicit activation restores the view after remount.
`/home/salvage` and `/fat/home/salvage` then name the verified file. Only after all
these checks does the fixture release the source reader, expecting its sticky
close error. The failed workspace remains failed and refuses managed retirement.

The baseline copy has 29 destination writes and 17 flushes. Fault injection at
every boundary must report copy failure. Destination conflict and allocation
refusal also fail with zero destination writes/flushes and unchanged destination
image. After every failed attempt, the original reader is still held, dirty and
failed; seek/read verifies every source byte again. The failed workspace disk
and its transport counters remain unchanged throughout the attempt. An unrelated
system anchor file stays readable and unchanged, and aliases keep their backing
identity. A transport failure may leave an incomplete or even fully published
destination artifact; it never constitutes acknowledged salvage success.

Cleanup on a failed test attempt explicitly closes the source only after proving
it remains usable. That is fixture teardown, not a recommended automatic error
path. The proof does not promise source survival after last close; callers must
keep the source reader alive until a verified copy or an explicit discard choice.

Clean source `e86776ec` passes **49/49** cases normally and with ASan/UBSan:
one durable-copy control, 46 transport faults, destination conflict and allocation
refusal. The 52-case writeback matrix, one existing process writeback case and
133 exFAT/namespace cases also pass with the extended adapters. Both existing
negative controls still reject lost failure-latch and swallowed close-error
variants. The initial missing PTY adapter link failure is retained as bring-up
evidence; PTY callbacks now trap if reached in this regular-file-only proof.

Reproduce after host tool preparation:

```sh
CC='cc -arch x86_64' python3 tools/test_exfat_import.py --workspace-salvage --output build/review/workspace-salvage/normal
CC='cc -arch x86_64' python3 tools/test_exfat_import.py --workspace-salvage --sanitize --output build/review/workspace-salvage/sanitize
```

Evidence is in `build/review/workspace-salvage/`: `final-normal/`,
`final-sanitize/`, `writeback/`, `negative/`, `legacy-process/`, `import/` and their
logs. Receipts retain exact source hashes and compiler commands. Backups outside
build cleanup are in `.local/filesystem-evidence/workspace-salvage-e86776ec/`.
All three review VMs (61610, 6233, 85869) and the primary checkout remain untouched;
this proof starts no VM. Physical media, USB admission, power loss, automated
retry/repair and product recovery UI remain outside this host qualification.
No further production implementation is required for the authorized proof.

### PR323 merge review and next bounded milestone

User authorized review/merge if ready. Main remains `0cba30a1`. Revalidated
all receipt source hashes and 49 normal + 49 sanitized salvage results, 52
writeback cases and 134 existing cases. Both negative controls pass again;
matched build reproduces the qualified production image hashes, whitespace
checks and all 877 local documentation links pass. No submitted correction or
unresolved review thread was present. Review confirms no source-path reopen,
exclusive destination creation and source lifetime through destination verification;
failure teardown remains explicit and is not a success acknowledgment.

Coordinated with PR317's owner: its current work concerns PMM/allocator scans and
poisoning, with no writeback/content/workspace policy changes planned. This is
scope coordination, not independent full-diff approval. PR323 changes only seven
fixture/tool/documentation files and introduces no production runtime change.
Review VMs remain live and untouched. Review logs are retained under
`build/review/workspace-salvage/review-*`.

Next proposed filesystem milestone: **descriptor-preserving userspace salvage
helper**. Turn the proven sequence into a reusable helper taking a caller-owned,
seekable source descriptor and an explicit exclusive destination. Handle short
reads/writes and interruptions; flush/close the destination before acknowledging
success, preserve source ownership and restore its offset on every result.
Return clear failure information without automatically deleting uncertain
artifacts. Verify with the same healthy-volume and injected-failure fixtures,
including destination conflict and source usability after failure. Reuse existing
file operations; no kernel recovery API, forced unmount, automatic repair or
application-wide recovery UI is implied. This is the next proposal, not work
begun by the merge review.

## Workspace writeback failure — qualified

User authorized deterministic writeback-failure qualification on main `805519b2`.
Coordinated with the PR317 audit owner, who confirmed no overlapping content,
close, writeback or exFAT/VFS edits. No production change was necessary: this
milestone extends the actual-source host fixture and records its failure contract.

The fixture links actual `process.c`, `workspace.c`, exFAT, VFS, namespace and block
claim code. Two independent in-memory exFAT devices provide `/fat` and `/work`;
active `/home`, `/usr`, `/bin` and `/var` bindings target the system volume.
It adopts the already-mounted work device into the managed workspace controller;
USB discovery/UUID admission remains covered by the existing workspace suite.
It prepares real process content and independent writer/reader descriptions, then
uses real global sync and description-close handlers. Transport, allocation and
scheduler admission use deterministic host adapters, not a guest fault injector.

The tested failure-state contract is:

| Failure | Observable result and retirement policy |
| --- | --- |
| Clean refusal before mutation | Sync errors; disk bytes unchanged, content stays dirty/retryable and exFAT writable. Explicit content retry can persist later. A managed workspace sync failure still latches its conservative failure state; content recovery does not silently clear it. |
| exFAT mutation write/flush failure | Sync/close errors; content remains dirty with sticky error while a reader holds it, exFAT is quarantined from further writes. Subsequent mutation attempts perform no transport I/O. |
| Last retained-reader close after failure | Reports I/O error and releases the now-unreferenced RAM buffer. This is error propagation, not a guarantee that edited bytes survive the final close. No false clean-persistence result is asserted. |
| Workspace device barrier failure | Managed sync errors and retains its failure latch and mount. This controller latch is distinct from an exFAT mutation quarantine; the underlying exFAT writable flag is not itself cleared by this barrier-only case. |
| Managed retirement after failure | Workspace sync/unmount stay false, the volume remains attached and write callbacks are not revoked. Raw driver unmount used for fixture teardown is explicitly not managed clean retirement. |

Clean source `d4cb4487` passes **52/52** cases normally and with ASan/UBSan.
The clean replacement has 17 writes and seven flushes; the runner injects one
failure at each boundary through both sync and writer-close paths (48 cases),
plus sync/close controls, a device barrier failure and a clean allocation refusal.
Every case verifies unchanged complete system-volume bytes, no system writes or
flushes, alias identity/readability, and balanced filesystem/I/O scopes. Clean
controls also prove managed retirement and remount byte readback. The retained
reader checks verify edited bytes before final close; both close handlers return
the expected error when the writer and then last reader retire.

The existing real process writeback test (one case) and complete exFAT/namespace
suite (133 cases) pass with the extended adapter. Two temporary-source negative
controls are rejected: removing the workspace failure latch and swallowing the
last reader's close error. No production source is modified by these controls.
The initial bridge prototype/root-fixture setup failures are retained separately;
they are fixture bring-up failures, not passing acceptance evidence.

Reproduce on this ARM64 macOS host with matched x86_64 host execution:

```sh
CC='cc -arch x86_64' python3 tools/test_exfat_import.py --workspace-writeback --output build/review/workspace-writeback/normal
CC='cc -arch x86_64' python3 tools/test_exfat_import.py --workspace-writeback --sanitize --output build/review/workspace-writeback/sanitize
python3 tools/test_workspace_writeback_negative.py --record build/review/workspace-writeback/normal/result.json --output build/review/workspace-writeback/negative
```

Local evidence is under `build/review/workspace-writeback/`: `final-normal/`,
`final-sanitize/`, `legacy-process/`, `import/`, `negative/`, plus associated logs.
Result receipts include source hashes and compiler commands. A backup outside
build cleanup is retained in `.local/filesystem-evidence/workspace-writeback-d4cb4487/`.
Review VMs 61610, 6233 and 85869 and the primary checkout remain untouched; this
milestone starts no VM. No physical power-loss, arbitrary media corruption,
full syscall/usercopy, USB transport or automatic recovery claim is added.
Main subsequently advanced to `a8e1fca1` (PR320 guest profile transfer). That
merge changes no process/workspace/exFAT/VFS or fixture source in these receipts;
all recorded positive-suite source hashes still match. It is consumed here
without altering profile policy. The final matched build is recorded separately
from the `a9508644` build whose production image hashes matched the earlier
filesystem runtime. No new VM proof is claimed for this host fault qualification.

No further production implementation is required for this qualification target.

### PR322 merge review and next bounded milestone

User authorized review and merge if ready. Integrated actual main `b9ac7435`
(PR321 profile-aware installation plans); it changes none of the code recorded
in the fault-proof receipts. Rechecked every recorded source hash, all 52 normal
and 52 sanitized results, 134 existing cases and both negative-control failures.
Both negative controls and six build-provenance tests pass again against the
merged tree. The matched build, whitespace and all 877 local links pass.
No submitted correction or unresolved review thread was present.

The PR317 audit owner confirms no new content/close/workspace/exFAT/VFS policy
overlap and no overlap-based blocker. That coordination is not an independent
full-diff approval. This review covers the six fixture/tool/documentation files;
no production source is changed. New review logs are retained under
`build/review/workspace-writeback/merge-review-*`. Prior positive receipts remain
exact-source evidence, rather than being relabeled as tests of later revisions.

Next proposed filesystem milestone: **explicit salvage of retained dirty data**.
With an injected `/work` writeback failure and an existing reader still holding
the dirty content, copy those bytes through existing file operations to an
explicit destination on a healthy volume. Fsync and close that destination,
remount it and verify exact bytes before releasing the final failing-work reader.
Prove destination errors cannot report successful salvage and preserve the
original held content; the failed workspace must remain failed. Start with a
focused actual-source proof and coordinate any process changes with its audit
owner. No new recovery API, automatic retry, repair, or forced unmount is implied.
This closes the practical evidence gap before the documented final-close RAM
release; it is a proposal, not work begun by this merge review.

## Workspace lifecycle — verified

User authorized the installed-root workspace lifecycle proof on main `c7be6574`.
Coordinated the additive harness mode with the installation owner and declared
the process audit boundary. No kernel, syscall, process, installer policy or
profile change was needed. This is a fixture/proof milestone: it adds sysabi
holder/verification modes and optional `--workspace-lifecycle`, which requires
`--canonical-paths`; existing installed-boot defaults remain unchanged.

Clean matched source `9ba7e944` passes the build and all 11 installed-root cases.
On each of the normal and reversed-disk boots, independent file, directory and
cwd holders each cause `unmount /work` to report busy. The file is dirty while
held; release performs fsync/close. Directory release closes its descriptor;
cwd release changes to `/`. Control is an explicit `/home` marker, adding no
extra `/work` reference. Each holder has a bounded 60-second timeout.

After every release, sync and unmount succeed. System aliases are checked while
`/work` is absent, then the correct other GPT partition remounts and the exact
file bytes plus `/home`/`/usr`/`/fat` identities and workspace isolation are checked.
All six independent refusal/release cycles and 12 absent/remounted checks pass.
The fixture file is removed after verification. The existing full namespace,
long-path, two-boot home/work marker persistence, canonical Text/theme restoration
and nine no-write recovery cases pass. Four offline allocation checks report
zero leaks, FAT/bitmap mismatches or errors; protected disk regions are unchanged.

Reproduce after a matched build:

```sh
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd \
  python3 tools/installed_root_smoke.py --canonical-paths --workspace-lifecycle \
  --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

Six build-provenance host tests, whitespace and documentation checks pass.
Evidence is in `build/review/installed-canonical/proof-q8wzmgqx/` and
`build/review/workspace-lifecycle/`; receipt SHA256 is
`4370536d27cd2a4cc72afe7b3bb48d320bd12f44d2524f55a38f0b324f78373e`.
The independent review counts all expected READY/RELEASED/refusal/verification
markers, rejects failure markers, compares every recovery disk hash and checks
allocation results. Receipts, commands, logs, matched manifest and independent
checks are also retained at
`.local/filesystem-evidence/workspace-lifecycle-9ba7e944/` outside build cleanup.

All proof VMs stopped; review VMs 61610, 6233 and 85869 and primary checkout were
preserved. This covers disposable GPT/AHCI mounted-volume reference lifetime,
not physical hot unplug, automatic USB workspace admission, failed-flush injection
or power-loss qualification. No further implementation is required for this target.

### PR319 merge review and next bounded milestone

The user authorized review and merge if ready. Actual main remains `c7be6574`.
The five-file diff changes only sysabi test modes, optional harness behavior and
this document. Rechecked the archived receipt SHA256, exact source and all six
READY/refusal/RELEASED cycles, 12 alias/persistence checks and nine complete disk
hash comparisons. No review correction or unresolved thread was present.
Six provenance tests, 871 local documentation links and whitespace checks pass.
A clean build of the review head reproduces all three image hashes from tested
`9ba7e944`; changes since that run are documentation only. The passing VM proof
therefore remains applicable without another run. Existing review VMs remain live.

Next proposed filesystem milestone: **failed-writeback workspace retirement
contract and fault proof**. Use the actual process/exFAT/VFS path with a disposable
`/work` backend and deterministic write/flush failures. Establish the expected
sync/close error, retained or quarantined state and safe mount-retirement behavior;
verify that failures cannot be reported as clean persistence and leave the
selected system volume and `/home`/`/usr`/`/fat` aliases intact. Reuse existing
fault adapters and coordinate process cleanup with its audit owner. Begin with
an explicit failure-state contract and focused reproducer; change production
code only for demonstrated gaps. No automatic repair/retry, physical media or
new backend is implied. This is the next proposal, not work begun by this review.

## Installed-root canonical paths — verified

November 3, 2026: canonical paths work end to end on an identity-selected
installed-root boot, preserving `/fat` and independent `/work`. Coordinated with
the installation owner of [PR315](https://github.com/PaulMDemers/SrvrOS/pull/315):
this proof reuses their published harness unchanged. No production filesystem
fix or competing installer implementation was needed. PR315 landed as
`500a0e37`; this is independent filesystem acceptance evidence.

Clean tested source `a85cc716` combines main `56e1f950` (PR313/PR314 included)
and published PR315. Six build-provenance host tests and a matched
`sh tools/macos/build.sh -j2 all` pass. The command is:

```sh
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd \
  python3 tools/installed_root_smoke.py --canonical-paths \
  --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

All 11 VM cases pass. No ISO, host kernel or host initrd supplies the OS; GPT
installation images provide loader, kernel, bootstrap and applications. The
bootstrap contains only `/init`. Firmware templates are host supplied.

| Installed-root acceptance | Result |
| --- | --- |
| Boot order and root identity | System selects `ahci0p2`, then `ahci1p2` after disk reversal; selection precedes aliases and init readiness |
| Canonical namespace on both boots | Full `sysabi --namespace` passes: `/bin`/`/usr`/`/fat` inode identity, logical cwd/dirfd, raw listing, shared content, guards, rename/unlink, spawn, 1023-byte backing paths and expansion-overflow refusal |
| Independent workspace | `/work` mounts the other GPT data partition (`ahci1p2`, then `ahci0p2`); guest isolation checks and clean unmount pass |
| Persistence | Distinct `/home` and `/work` markers survive boot, reopen through global/legacy aliases, then checked cleanup; offline images confirm separate storage |
| Desktop | `/bin/deskd` saves and reopens `/home/installed-proof.txt`; saved Text bytes and theme preference survive reordered boot; screenshot inspected |
| Integrity | Four offline system/work allocation checks report zero leaks, FAT/bitmap mismatches and errors; protected regions outside both data partitions remain unchanged |
| Recovery | Nine missing/duplicate/wrong-role/type/format/invalid-map/recovery/pending cases halt before namespace activation, with both disk hashes unchanged |

Evidence: `build/review/installed-canonical/proof-zj4uzynn/receipt.json` has SHA256
`7694ffd8a1ddd91729a25fdf6430dad852a66799de2795b22a043261088426cc`.
Commands, full serial logs, disk images, allocation checks and screenshots remain
in that directory. `build/review/filesystem-installed/` contains host/build logs,
`tested-build.json`, `independent-review.json` and `reopened.png`. Receipt, logs,
commands, matched manifest, independent checks and PNG are also retained under
`.local/filesystem-evidence/installed-canonical-a85cc716/` to survive build cleanup.
The independent checks verify ordering, markers, command boot inputs, both
reordered mount devices, every recovery hash comparison and allocation results.

The primary checkout and review VMs (61610, 6233, 85869) were preserved; every
VM created by this proof was stopped. This accepts disposable QEMU EFI/GPT
installation behavior. Physical media, power-loss qualification, authenticated
identity, GPT repair and profile-session activation remain outside this proof.
No further filesystem implementation is required for this target.

### PR318 merge review and next bounded filesystem milestone

The user authorized review and merge of PR318 after PR315 landed. Reviewed actual
main `500a0e37` and the single-file PR318 diff. PR315's final harness removes the
unreliable early Text startup-log wait and waits for window publication instead;
Text save/reopen responses, clipboard length and exact disk readback remain
required. Its clean reviewed source `57fcf482` passes all 11 cases. See the
[final installation review](disk-installation.md) and
[portable review receipt](assets/screenshots/installed-canonical-review-manifest.json).
The earlier `a85cc716` receipt above remains its own passing historical run,
not a claim to have tested the later harness revision.

Archived receipt hashes and independent checks were revalidated for PR318.
No unresolved review thread or requested correction was present. This PR changes
only this acceptance document; it introduces no runtime or harness change.
Documentation link validation and whitespace checks pass against merged PR315.
No further VM run is needed for this documentation-only reconciliation.

Next proposed filesystem milestone: **installed-root workspace unmount/remount
lifetime proof**. On two disposable GPT disks, hold an open file, directory and
cwd under `/work`, verify busy unmount refusal without data loss, then release
the references, sync, unmount and remount. Check persisted bytes and confirm
`/home`, `/usr` and `/fat` still refer to the selected system volume throughout.
Reuse existing process/workspace ownership and the installed-boot harness;
coordinate any needed process changes with its audit owner. This closes the
guest lifecycle evidence gap beyond the current clean-unmount proof. It adds
no automatic USB selection, profile policy or new filesystem backend. This is
a proposal for the next milestone, not implementation begun by this review.

## Dependency integration — verified for merge

The user authorized dependency review, integration and merge of
[PR313](https://github.com/PaulMDemers/SrvrOS/pull/313). The PR252 owner reviewed
full-head integration (`badfeb37`), ports provenance, lifetime/exclusion and the
two compatibility corrections below, with no remaining source-review blocker.
No submitted review requested changes and no unresolved review thread was found
on PR313, PR252 or SrvrPorts PR3. Use a history-preserving PR313 merge: it contains
both PR252 and PR308; do not independently land PR252's obsolete tracked ports tree.

[SrvrPorts PR3](https://github.com/PaulMDemers/SrvrPorts/pull/3) merged first as
`5e994f53`. Its tree is identical to tested head `0f51ba9`, now an ancestor of
ports main. The OS gitlink pins that actual merge. Four transferred code files
match PR252 byte-for-byte; its provenance addition is retained alongside newer
cross-repository links. The owner confirmed no unpublished fixes or newer head.

Combined verification used clean OS source `421591d6` and ports `0f51ba9`,
including main `9d642c75` (installed-root identity PR312 and portable profiles
PR309, after installation PR311). Main subsequently advanced to `e224ac2c`
(PR316: device documentation and an independent host audit tool); that merge
changes no runtime sources, and its eight host tests pass. Final build manifests
and image-hash comparison are retained with the evidence below to connect the
final documentation/gitlink integration to the tested matched images.

Full threadstress found two compatibility regressions before acceptance:

- The proof helper's stale 160-byte bound rejected legacy long-path mkdir.
  `3563dd30` uses `SRV_PATH_MAX` (1024) throughout namespace translation and
  projected listing, propagates `ENAMETOOLONG` for alias expansion overflow,
  and clears refused outputs. Guest coverage creates a 1023-byte backing path,
  checks listing/identity, and rejects overflowing open/stat/mkdir/chdir.
- `06d5a416` restores existing rename type/cycle error precedence before the
  attachment busy guard, while retaining the guard before mutation or I/O.
  Tests cover EISDIR, ENOTDIR, EINVAL and valid protected-root EBUSY.

The owner accepted both corrections without changes to owner-span lifetime,
checked-copy/descriptor protections, private-image admission, or process exit
exclusion. Spawn/exec still have no outer filesystem owner scope. The two failed
threadstress logs (`thread.log`, `thread-fixed.log`) remain as failure evidence;
subsequent full runs pass.

| Combined check | Result |
| --- | --- |
| Real exFAT/VFS and process writeback at `421591d6` | 133/133 normal, 133/133 ASan/UBSan, writeback pass |
| Namespace guest at `421591d6` | Long paths, alias guards, `/fat` compatibility, independent `/work`, two-boot persistence and cleanup pass |
| Full threadstress at `421591d6` | Pass, including path-limits and rename |
| Installed-root guest at `421591d6` | Two successful identity-selected boots with reversed disk order; nine recovery refusals preserve disk hashes |
| Activation and recovery ordering | Aliases activate on both selected-root boots; no activation on any recovery refusal |
| Ordinary desktop at `421591d6` | VirtIO Q35/TCG first application frame 4.33 seconds, running QMP state and screenshot retained |
| Focused host regressions after both fixes | Lifetime, process/service/loader, fault cleanup, checked usercopy, syscall I/O/listing, replacement/file changes, pipe rights/message, mappings, socket transfer/aliases/restart, PTY, PMM/VMM, shell, uv and uACPI pass |
| Workspace host contract | Identity/marker/duplicate/dirty refusal, mount holds/stale tokens, ownership, busy/flush/unmount/revoke and sync-failure checks pass |
| Network and libuv guests at `06d5a416` | netabi, uv and libuv pass; later main merges do not change their runtime implementation |
| New main host contracts | Installed-root selector, clean-mount no-write refusal, 21 installation Python cases, storage, portable-profile header and 13 Python cases pass |

Local ignored evidence: `build/review/filesystem-integration/` holds
`host-final.log`, `socket-host.log`, `new-main-host.log`, `current-exfat/`,
`current-exfat-sanitize/`, `current-writeback/`, `current-thread.log`,
`current-namespace/20261103T233037.982713Z/`, guest network/libuv logs and the
`tested-421591d6-build.json` manifest. Installed-root receipt and all positive /
negative logs are in `build/review/installed-root/proof-5aolw6w9/`.
Ordinary desktop evidence is in
`.local/macos-vm/filesystem-integrated-main/session-20261103T233242.983475Z/`.
The earlier disposable installed-disk proof at `06d5a416` also passed, including
post-boot allocation checks with zero leaks/errors.

### Final current-main profile integration

Main advanced again to `4c4c37a5` (desktop-profile PR314). Merge `3cd9ce86`
preserves both appended Makefile test groups; this was the only conflict.
The selected SrvrApps pin is `3cf32af2`. Source review confirms profile selection
retains explicit `/fat` and `/work` homes and owns session/environment data across
launch. Kernel/shared ABI sources and the kernel image hash are unchanged from
`421591d6`.

On clean matched source `3cd9ce86`, profile session/startup, preferences,
portable-profile/image, command-job, lifecycle-wait and desktop-session host
checks pass. The four-boot profile guest passes edits, preference persistence,
export/import, independent installation identities, Terminal environment/cwd and
offline allocation checks. The two-boot long-path namespace/persistence suite and
full threadstress pass again. Ordinary desktop first frame is 4.32 seconds.
The task desktop VM was stopped after retaining its running-state evidence.

Evidence: `profile-host-final.log` (profile/command/lifecycle checks),
`profile-session-host.log`, `profile-build.log`, `profile-guest.log`,
`profile-thread.log`, `profile-namespace/20261103T233903.642148Z/` and
`tested-3cd9ce86-build.json` under `build/review/filesystem-integration/`;
`build/review/profile-desktop/proof-bm81r8zn/receipt.json`; and desktop session
`.local/macos-vm/filesystem-profile-main/session-20261103T233958.865517Z/`.
The initial host compile attempts are retained: Darwin requires
`-D_DARWIN_C_SOURCE` for `O_NOFOLLOW`; the desktop-session fixture additionally
uses the documented `-Wno-array-bounds` for its inactive inline-storage branch.
No production warning policy was changed. Final documentation-only build hashes
are compared with this tested source before merge. Documentation link validation
passes with 864 local links and zero errors.

The primary checkout and existing review VMs (61610, 6233, 85869) were preserved;
task VMs were stopped. No physical installation or reboot was performed.
SrvrOS Actions is disabled; SrvrPorts Actions is enabled with no workflows.
No CI setting was changed. Earlier checkpoint statements about disabled Actions
refer to SrvrOS, not a verified SrvrPorts policy.

## Global activation milestone

Review: [PR313](https://github.com/PaulMDemers/SrvrOS/pull/313).

Branch `device/apple-silicon/filesystem-global` builds on main `a936baeb`, proof
`88ea5524`, and full PR252 `badfeb37`. The PR252 owner supplied a read-only
lifetime/exclusion handoff and recommended full integration, not a partial
cherry-pick. Dependency merge `c0773799` preserves main's graphics syscalls
156–161 and assigns the incoming pipe-message syscall 162. The five incoming
ports changes initially moved to SrvrPorts commit `0f51ba9`,
[PR3](https://github.com/PaulMDemers/SrvrPorts/pull/3); no tracked ports tree was
reintroduced. This branch is separate from the unchanged PR308 proof branch.

Boot atomically attaches `/bin` → `/fat/bin`, `/usr` → `/fat`, `/var` →
`/fat/var`, and `/home` → `/fat/home` after the backing volume and boot log
layout exist. Missing/conflicting targets refuse activation and retain legacy
paths. `/fat` stays available; an independently mounted `/work` stays independent.
Existing `/etc` remains the initramfs tree. Profiles retains HOME/provider policy;
this change does not select a new user identity, workspace, or root filesystem.

Path-consuming syscalls copy and normalize logical paths, resolve their backing
names, and perform guards/backend operations/publication in one masked-IRQ,
scheduler I/O owner span. Monitor `ls`, `cat`, `write` and `elf` use the same admission and view; admin
mountpoint names stay raw. Eligibility in `syscall_io_safe` is unchanged. Executable
paths are prepared under ordinary admission before entering PR252's private-child
image scope. Unix socket names retain their separate registry and path contract.

Content objects, locks, dirty-size queries, mutation guards and unmount references
use canonical backing paths. Cwd and directory handles separately retain logical
paths, so `/home/..` is `/` and fchdir/openat preserve the attachment view. Root
readdir and native indexed listing include new paths; native listing retains its
short-buffer truncation contract. Active attachment targets and their ancestors
cannot be removed or renamed through filesystem syscalls. VFS retirement removes
affected bindings before node slots can be reused; mounting a replacement at the
same raw name does not reactivate aliases. Explicit activation is required.

Host validation covers the 133 real exFAT/VFS cases normally and with ASan/UBSan,
process content writeback, logical cwd/dirfd, shared alias content/locks, protected
roots, raw listing compatibility, loader/private-image admission and PR252 lifetime,
I/O-admission and checked-usercopy regressions. The guest test is
`sysabi --namespace`; `tools/namespace_smoke.py` uses two disposable disks and two
boots for `/fat`/`/work` compatibility and persistence. Matched build and guest
results are recorded below.

Known bounds remain: 1023-byte canonical backing paths (1024-byte buffers), no symlinks or nonempty directory
rename, and the existing exFAT sidecar/recovery limitations. Indexed flat projected
listing rescans the raw node table and is quadratic for a complete scan; directory
root enumeration uses a direct four-binding accessor. No cache or second lock was
introduced. This milestone does not provide physical-device acceptance.

## Global activation verification

Implementation `b5e6d153` (preceded by `ecec98e3`) was built with a clean tracked
checkout and pinned submodules, including SrvrPorts `0f51ba9`. Documentation-only
follow-ups do not change the tested binaries.

| Check | Result |
| --- | --- |
| Real exFAT/VFS import and namespace suite | 133/133 pass on x86_64; 133/133 pass with ASan/UBSan |
| Real process/backend writeback | Pass, including refusal/retry integration |
| Process and syscall regressions | Output/content/locks, cwd/dirfd, service/private-image preparation, foreign-owner refusal, loader, lifetime, fault cleanup, I/O eligibility, replacement, guarded file changes, checked usercopy, VFS time and native list compatibility pass |
| PR252 transferred ports and pipe-message tests | uv bounds/lifetime, uACPI namespace-root/table-install/GAS, pipe-message pass |
| Workspace host contract | UUID/marker/duplicate/dirty checks, mount holds, stale tokens, ownership, busy/flush/unmount/revoke and sync-failure propagation pass |
| Matched build | `sh tools/macos/build.sh -j2 all` passes at `b5e6d153`; source and image hashes recorded in `build/macos-build.json` |
| Global guest namespace | Monitor listings/read/write/ELF/run, raw syscall and libc aliases, dirty shared reads, exclusive-create/native-write guards, cwd/fchdir/openat, protected roots, metadata, rename/unlink, exec/spawn and separate `/work` data pass |
| Two-boot persistence | Independent system/work copies, fsync/close, global sync, `/work` unmount and checked guest reboot; reopen through `/home`, `/fat/home`, `/usr/home` and `/work` passes, followed by verified cleanup and a second checked reboot |
| Ordinary desktop | Q35/TCG with VirtIO, matched images: desktop and first application frame at 4.31 seconds; QMP running state and screenshot retained |
| Documentation | 840 local inline links checked, zero errors before the final evidence additions |

Evidence is local and ignored: `build/review/filesystem-layout/global-*.log`,
`global-exfat/result.json`, `global-exfat-sanitize/result.json`, and
`global-process-writeback/result.json` under that directory; two-boot guest logs,
manifest and disposable disks under
`build/review/namespace-smoke/20261103T231056.210739Z/`; desktop manifest, serial
log and PNG under
`.local/macos-vm/filesystem-global/session-20261103T231118.231509Z/`.
The initial harness startup-prompt race and its attempted `/fat` unmount are
retained as failed runs. The latter correctly refused init's live log reference;
the final harness uses the ordinary checked reboot flush, without overriding it.

Host tests use the task's x86_64 Darwin compiler wrapper; workspace/sync fixtures
also need `-D_DARWIN_C_SOURCE` for the host `cfmakeraw` declaration. This is not a
claim that every `test-core` target is qualified on macOS. The guest `/work` disk
is an explicit second AHCI volume, not physical USB workspace acceptance.
Task VMs are stopped; the three existing review VMs were preserved. Actions
remain disabled. No installation, physical reboot or merge was performed.

## First checkpoint history

## Target, baseline and ownership

Base: freshly fetched `origin/main` at `a936baeb` (includes HiDPI PR303).
Branch: `device/apple-silicon/filesystem-layout`, isolated ordinary checkout
`SrvrOS-filesystem`. The integration checkout and existing review VMs/disks are
not build inputs or test targets. No physical installation or reboot is authorized
by this checkpoint; Actions remain disabled.

Own VFS namespace/mount semantics and this document. Inspected fetched PR heads:
PR252 `badfeb37` owns process, descriptors and libc lifetime; PR19 `993e920a`
owns Wi-Fi; PR301 `4c9fc184` owns native display; PR306 `5028871b` plus SrvrPorts
PR2 owns SQLite packaging. Their actual file diffs were checked. This proof adds
an explicit namespace helper and tests without editing process, syscall, libc,
boot selection or those owners' documentation. Generic Makefile test wiring is
additive. There is no competing account system, installer or filesystem backend.

Required evidence: canonical resolution and rejection boundaries; one live object
through new/legacy names; write/readback, metadata and clean unmount/remount through
real exFAT/VFS; preservation of existing file contents; full matched build and
ordinary disposable VM boot. Evidence and limitations are recorded below.

## First checkpoint source facts (historical)

- `kernel/src/main.c:mount_exfat_volume` tries whole `ahci0`, otherwise `virtio0`,
  then GPT partitions in order 2, 1, 3 through 32, then initramfs `/srvros.exfat`.
  The selected filesystem mounts at `/fat`. This is discovery order, not an
  installed-root identity or authenticated boot selection policy.
- `kernel/src/workspace.c` separately selects a unique USB exFAT serial and checks
  `srvros-work/.workspace`; it mounts at `/work`. Duplicate serials are refused.
  A serial is not a security identity. The workspace must not silently become the
  OS root or replace another user's HOME.
- VFS is a flat table of stable node addresses, with initramfs directories
  synthesized at boot. It has 32,768 total slots. exFAT supports four mounts,
  8,192 files and 2,048 directories per mount, and paths of at most 1023 bytes
  including the mount prefix (1024-byte buffer; components are at most 255 bytes). Capacity includes internal
  metadata and temporary replacement files; free bytes alone do not admit a write.
- exFAT imports a constrained ASCII entry subset, rejects unsupported layouts
  and case collisions, and does not implement general Unicode or arbitrary
  upstream exFAT extension preservation. VFS lookup itself is case-sensitive.
- Mode, uid/gid, inode, link count and times are SrvrOS path-keyed `.srvros/meta`
  records, replaced using `.srvros/meta.tmp`. Default ownership is root. Imported
  inode collisions are remapped in the live VFS: inode numbers are not portable
  profile IDs or guaranteed stable identifiers across installations.
- `link`, `symlink` and `readlink` are unsupported. Generated exact-allocation
  aliases are a disk-image compatibility convention, not general hard links.
  Nonempty directory rename is unsupported. Regular-file replacement has explicit
  publication/open-target handling in process/exFAT; PR252 retains that lifetime
  ownership. Do not add a second node or content cache for a namespace alias.
- The backend flushes dirty state before mutation, orders writes and flushes,
  and clears dirty state only after completion. Uncertain failures quarantine
  writes; dirty imports skip sidecar replay. This is not a journal, crash-atomic
  rename, or automatic recovery. Copy-on-write replacement needs spare clusters.
  Clean remount tests do not prove physical power-loss recovery.

See [capacity](filesystem-capacity.md), [hardening](filesystem-hardening-2026-10-17.md)
and [the storage audit](filesystem-audit-2026-10-26.md) for detailed bounds,
refusal and fault-injection evidence.

## Practical layout, staged on implemented facilities

The table defines the contract to consume, not paths globally activated by this
checkpoint. Do not install empty pseudo-filesystem directories to imply support.

| Logical path | Backing and policy | Availability / transition |
| --- | --- | --- |
| `/` | Existing initramfs bootstrap and recovery tree | Remains the current VFS root; no new root mount replaces it |
| `/bin` | Selected system volume's `bin` | Proof mapping `/bin` → `/fat/bin`; retain `/fat/bin` |
| `/usr` | Selected system volume root; currently `bin`, `lib`, `include`, `share` as packaged | Proof mapping `/usr` → `/fat`, so `/usr/bin` and `/bin` use the same objects; this compatibility view also exposes other existing volume directories |
| `/etc` | Existing initramfs configuration/defaults | Preserve existing `/etc`; do not hide it with a binding. Persistent machine configuration/overlay is a later explicit contract, not implemented here |
| `/var` | System volume `var`, including current boot logs | Proof mapping `/var` → `/fat/var` after the backing directory exists |
| `/home` | Explicitly selected home-provider directory | Proof uses `/fat/home`; the legacy workspace alternative is `/work/srvros-work`. Profiles owner defines user subdirectories and HOME/config/data/cache policy |
| `/tmp` | No general tmpfs is implemented | A disposable disk-backed scratch directory may be created by a test; automatic clearing, sticky-bit enforcement and a global `/tmp` service are not claimed |
| `/dev` | Existing terminal/device descriptor APIs | Do not invent a general devfs mount or device-node creation capability |
| `/proc` | Existing process diagnostic interfaces | No procfs tree is added; use implemented process APIs/tools |
| `/fat`, `/work` | Existing storage/mount compatibility names | Continue to work; no move, copy, rename or automatic data migration |

System binaries, libraries and packaged resources are **release-managed**;
`var` and the chosen home tree are **mutable state**. Release-managed does not yet
mean kernel-enforced immutable: `/fat` is writable and a second alias cannot
protect the same object from writes through its old path. Read-only mounts,
credential checks and upgrade staging must precede an enforced immutable-system
claim. `/etc` defaults are backed by the boot archive, not a writable overlay.

## Volume and installation interface

For this checkpoint keep the existing exFAT image format and `/fat` mount. The
installation owner can consume that unchanged kernel/image contract immediately.
A bootable installation experiment must record which disk/partition was selected
and verify it after boot; successful probing of the first volume is insufficient
evidence of stable selection when multiple candidates exist.

The required next installed-root interface is explicit selection of a block
volume with a stable identifier plus format/version and role. Resolve exactly
one match before mount; refuse ambiguity or mismatch, and enter explicit recovery
rather than falling back to writing an unrelated volume. Keep boot/recovery
payload selection separate from mutable home/workspace selection. This selector
is a specified dependency, not implemented by the namespace proof. The
installation owner owns boot configuration and image tooling; this owner owns
mount validation/namespace attachment. No new boot parameter is asserted here.

Profiles supplies the stable profile identity, home-provider choice and relative
profile content paths. Filesystem supplies the selected backing directory,
capacity/error results and eventual credentials-aware operations. Before either
owner implements enforcement, agree on process credentials (effective uid/gid,
supplementary groups), creation ownership/umask, directory search permission,
read/write/execute checks, sticky directories, descriptor rights and metadata
persistence. Current root identity and owner-mode `access()` checks do not provide
multi-user isolation. Never serialize host paths, live handles, local inode
numbers or machine credentials as portable profile identity.

## Format decision

| Option | What it solves | What SrvrOS would still need |
| --- | --- | --- |
| Keep current exFAT for this proof | Existing bootable images, host interoperability, working bounded read/write/flush and compatibility | Real permission enforcement, links, fuller rename semantics, stronger recovery, wider paths/names; sidecar remains necessary |
| Add ext2 | Native inode ownership/modes and Unix directory/link representation | Entire driver/import/mutation integration, tooling and recovery tests; choosing ext2 does not add journaling |
| Add ext4 with a deliberately bounded feature set | Native Unix metadata plus journal-based metadata recovery | Driver, feature gating, journal replay and flush ordering, boot/tooling and failure testing; journaled metadata alone does not guarantee application data consistency |

Decision: retain exFAT for the first namespace/installation experiments, without
selecting it as the final durable multi-user root. Evaluate a native Unix format
when a backend milestone can prove the missing semantics and recovery. There is
no justified new repository or format migration in this checkpoint.

Primary sources consulted: Microsoft's [exFAT specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification)
for allocation, entry sets and volume flags; Linux's [ext2 format documentation](https://www.kernel.org/doc/html/latest/filesystems/ext2.html)
for inode/ownership and directory representation, and [ext4 journal documentation](https://www.kernel.org/doc/html/latest/filesystems/ext4/journal.html)
for transaction/replay guarantees and their data-mode limits. These describe the
formats, not implemented SrvrOS support.

## Namespace proof interface and migration boundary

The Linux man-pages project's [pathname resolution documentation](https://man7.org/linux/man-pages/man7/path_resolution.7.html)
distinguishes traversed directory components, missing final names for creation,
dot components and trailing-slash directory requirements. The eventual public
resolver must preserve those distinctions across a mount boundary. The helper
below accepts already-canonical storage operations only; its rejection of dots
and trailing slashes is an internal precondition, not the desired application API.
Direct retrieval of the Open Group Issue 8 pathname/rename pages returned HTTP
403 during research; no new POSIX conformance claim relies on those unread pages.

`vfs_namespace` is an explicit caller-owned view with at most eight copied
source/target mappings and, initially, 160-byte proof buffers. Integration review
corrected those buffers to the shared 1024-byte ABI limit. Bind to an existing directory;
refuse root replacement, occupied public names, overlapping public prefixes and
recursive/chained aliases. No duplicate VFS node, inode, mount or storage claim is
created. Resolve a canonical absolute path once, then pass the returned storage
path to the existing VFS/exFAT operation. Lookup returns the original VFS pointer.
Legacy canonical paths pass through without translation.

This low-level API deliberately rejects relative paths, `.`/`..`, duplicate or
trailing slashes, oversize input/expansion and traversal through absent/non-directory
parents. It is not a POSIX pathname parser, chroot or security sandbox. In
particular a caller cannot escape an exported subtree through dot components;
legacy paths remain accessible by design. Bindings do not cache target pointers;
unmounted targets fail lookup/resolution until present again. A view is a mapping
to a selected path, not a mount-generation pin: callers must discard/rebind it
when changing selected volumes, rather than transparently substituting a new disk.

The caller must provide filesystem exclusion across resolve and operation,
and preserve the canonical storage path in lifetime bookkeeping. The sequential host proof does not establish concurrent resolve/operation safety.
The exFAT mutation scope is internal; a supported exclusion handoff with the
process/storage owners is still needed before runtime activation. No global
aliases, syscall ABI, directory enumeration or process path normalization change
is enabled. Before activation, coordinate with PR252 to route open/exec/stat,
create/delete/rename, cwd/dirfd, getdents, metadata, statfs and unmount/open-prefix
checks through one identity contract. Pair that with logical directory enumeration
and dot-component traversal tests. Translating only `vfs_lookup` would leave
writes, cache keys and unmount checks inconsistent and is intentionally avoided.

Migration is additive: retain `/fat` and `/work`, attach tested views after explicit
volume selection, and move application defaults only after the complete process
interface passes. Do not rename existing directories or copy user data merely to
change the public namespace. Missing homes require an explicit provider decision;
do not create a replacement home on the fallback RAM image silently.

## Verification and next action

The bounded proof is complete and published as draft [PR308](https://github.com/PaulMDemers/SrvrOS/pull/308).
Implementation commit: `6686ae18fc709a07b636b97fac9e4161a2f3b9da` on base
`a936baeb`. The later evidence update changes only this document.

| Check | Result and scope |
| --- | --- |
| Real exFAT/VFS import suite | 132 modes pass: 123 existing plus nine namespace cases; normal run and ASan/UBSan using `cc -arch x86_64` on ARM64 macOS |
| Namespace proof | Paths, bounds, binding rejection, attachment lifecycle, pointer/inode identity, persistence, rename/delete, backend quarantine and legacy workspace provider pass |
| Existing data | Original 1,100-byte fixture file remains exact after mutations and remount; mode/mtime and newly written data survive clean unmount/remount |
| Image generator | Six tests pass |
| Focused Make targets | `test-vfs-namespace test-vfs-time check-docs` pass using the documented Darwin x86_64 wrapper; 824 local Markdown links at the implementation checkpoint, 825 after the evidence update |
| Matched `all` build | Pass from clean implementation commit and pinned submodules; `build/macos-build.json` records source and hashes |
| Disposable ordinary boot | Q35/TCG, VirtIO, matched kernel/initramfs/disk: desktop and first application frame at 4.31 seconds; QMP reports running and screenshot retained |
| Cleanup | Task VM stopped through its own QMP socket; original review PIDs 61610, 6233 and 85869 preserved |

Reproduce sequentially in an isolated checkout with the compiler wrapper from
[macOS guidance](macos-development.md):

```sh
CC='cc -arch x86_64' python3 tools/test_exfat_import.py
CC='cc -arch x86_64' python3 tools/test_exfat_import.py --sanitize --output build/review/filesystem-layout/final-sanitize
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-vfs-namespace test-vfs-time check-docs
sh tools/macos/build.sh -j2 all
python3 tools/macos/run.py --headless --state-dir .local/macos-vm/filesystem-layout
```

The launcher leaves its guest running; inspect its recorded PID/QMP socket and
stop only that disposable instance after readiness. Local receipts live under
`build/review/filesystem-layout/`; the Make namespace receipt is under
`build/review/vfs-namespace/`. VM manifest, serial log and screenshot remain in
`.local/macos-vm/filesystem-layout/session-20261103T224228.262439Z/`.
The initial documentation check ran before submodule checkout completed and
reported missing dependency links; the complete-checkout rerun passed.

The final sanitizer receipt names the clean implementation commit. The ordinary
132-mode run preceded that commit and used identical implementation; focused
Make checks and the matched image/VM used the committed tree. Build hashes:

- Kernel: `ac1198e5655ff216b57fab9c1f6e182476c93b214333a6739e3ebf578cad5185`
- Initramfs: `f31a586d1073314577ce4864afd3b30af19458aed048b0bc6689942c7599baff`
- exFAT template: `c972e6c49ba978df9476b0e9cfc54108a8f263fdf5b07eea1e891318c68cee7d`

The VM verifies ordinary bootability, not use of new global paths or a clean
shutdown/remount. Persistence evidence comes from the real backend's disposable
host fixture. No physical power-loss, native hardware, permission isolation,
concurrent resolve/operation or global application path activation is accepted.
No physical boot is armed and no task-owned VM is left running.

Next required platform step: agree the exclusion and canonical-storage identity
handoff with PR252, then implement complete process/namespace activation and
logical directory enumeration as a separate milestone. Installation experiments
can consume the existing `/fat` image contract immediately, with explicit
selected-volume evidence; profiles can consume the provider boundary above.
This draft is delivered for review, with no merge requested.
