# Explicit storage-only write/flush comparison

`hardware_boot.py stage --laptop-probe-volume <local-UUID>
--storage-only-capture` selects a separate opt-in control. It uses the ordinary
matched kernel/initramfs, requires automatic return and refuses other diagnostic,
desktop, battery, AP and polling-policy profiles. Two brief1% speaker cues bracket
the measurements; there is no audio, desktop or battery workload in the measured
interval. Existing combined30-minute stress acceptance remains independent.

The guest shows progress through six balanced pairs, twelve samples total,
within180seconds including preparation, proofs and retirement. Every sample
writes32768sequence-dependent bytes, flushes and verifies every byte by fresh
media reads. The filesystem leg retains normal exFAT replacement/metadata and
workspace barriers. The direct leg re-proves the fresh file's exact contiguous,
exclusive range, parks the filesystem, revokes all public write aliases, claims
the whole device and performs two existing16KiB writes, flush and two16KiB reads.
It releases the claim, remounts and verifies again before any filesystem leg.
The old4096-byte battery scratch API and all transport deadlines,4KiB DMA,
polling, integrity checks and ordinary startup behavior remain unchanged.

The fixture has a unique fresh path; existing paths refuse. No public raw
length/offset API is exposed. Uncertain write, flush, readback or clock evidence
terminally invalidates the device identity and poisons filesystem reentry for
that boot. Claims and live transport state remain retained on uncertainty.
No timeout resets/retries hardware, frees pending DMA or reuses ownership.

Periodic logging is quiesced before measurement. A durable preparation checkpoint
retains fixture identity before parking. Fixed records emit only after the
comparison: sample wall phases, write-stage BOT aggregates, exact slowest CSW's
validated command and software event/service/resume clocks. Flush and readback
have independent aggregate validity. Each line stays below1024bytes and the
complete receipt below32768bytes, within the unchanged64KiB tail. No logging
writes occur inside measurement. The final normal logger saves exit.log before
clean unmount and Ubuntu return. A poisoned device cannot save a final report:
missing final evidence is failure/incomplete, never a pass.

`storage_only_result.capture_result` requires all twelve ordered receipts,
six exact leases, full byte CRCs, consistent wall/command/service coverage,
clean retirement and two quiet boundary cues. Offline collection must additionally
check exact final fixture bytes, every prior file hash, exfatprogs and independent
allocation traversal. Preserve matching failed/preparation evidence before another
trial. VM results validate mechanics only; native acceptance requires this unit's
actual return. Timings are descriptive controls: order, media state and filesystem
metadata differ, and software event observation is not hardware completion or
flash geometry. One trial does not prove a causal performance improvement.

`make test-storage-compare` exercises the real workspace/exFAT/VFS/BOT/block/claim
producer on disposable regular media at512/4096-byte sectors and4/128KiB clusters.
It asserts exact raw-byte containment, fresh readback, clean retirement,
rerun/existing-path/held/busy/clock refusal, partial write/flush/readback/deadline
quarantine, and strict receipt negative controls. Hardware service clocks are
modeled in this fixture; the actual USB command framing and completion validation
are production code.

Each512-byte payload sector and each16KiB command chunk has distinct data, in
addition to its sample sequence. A transport that aliases both reads and writes
of the second half is explicitly fault-tested and quarantined. Controller ring
checks partition exactly into event and empty observations. Empty checks between
clock samples need not produce a last-empty timestamp; a present timestamp must
be bounded by submission/observation and retain full sampled-service coverage.
These checks add no ring/clock reads and change no controller policy or limits.


## Collection paths and native baseline

The private filesystem sidecar is volume-relative `.srvros/meta`, at
`/work/.srvros/meta` in SrvrOS. `srvros-work` is Home within that volume, not the
mount root. A known sidecar update may be accounted for explicitly; every other
prior file still requires an identical hash. Do not exempt a directory tree or
silently discard an earlier failed assessment when correcting a collector path.

The [Air6,1 return record](machines/macbookair6-1.md#native-storage-only-comparison-return-2026-11-12)
accepts the quiet control in release `78cfda46ee1b63a0`: all twelve samples in
5.602029 seconds, exact final bytes, six retired leases and preserved prior files
with clean offline allocation. Measured filesystem write/flush/readback legs are
50.755–82.691 ms versus 7.807–7.822 ms for the direct data path. Mandatory raw
proof/retirement/remount work is outside those per-leg numbers; six post-raw gaps
are 817.078–833.309 ms. Filesystem write stages issue 37/56 BOT commands versus
two direct writes. Seconds-long loaded status waits do not reproduce here, and
no sustained-load or screenshot latency fix is accepted. Next diagnostics should
separate command amplification from load-dependent completion tails, with all
original limits and integrity requirements intact.

## Opt-in filesystem command attribution (v2)

Add `--storage-command-trace` to `stage --laptop-probe-volume <this-unit-UUID>
--storage-only-capture`. This selects a distinct v2 recipe with
`srvros.storage-roles=on`. The v1 recipe is unchanged. Both use the same six
balanced pairs, twelve 32 KiB samples, payload, deadlines and integrity checks;
v2 adds observation of the existing repeated writes, not a larger workload.

The generic, already-owned USB content window receives exFAT's existing callback
labels. A role hook neither starts a window nor reads a clock, issues a transfer,
adds a checkpoint or grants I/O eligibility. Its inactive path returns immediately.
SYS_SYNC still owns its original observer/lifecycle checks. Standalone host
filesystem models keep their existing role adapters; the dedicated BOT fixture
and kernel exercise the generic USB hook directly. PR19's separate metadata
restore changes are not part of this attribution change.

| Role | Meaning of the existing callback label |
| --- | --- |
| 0 | Other active filesystem metadata work |
| 1 | Allocation or free phase |
| 2 | File data phase |
| 3 | Immutable prefix equality proof |
| 4 | Sidecar metadata update |
| 5 | Sidecar equality proof |
| 6 | Unclassified work, including pre-mutation scans and direct I/O |

Final-only `storage-only-roles` masks account for all seven roles, with zero
roles implicit. Each present `storage-only-role` row records read/write/flush
commands, transferred bytes, failures/other commands, CSW count/wall time/maximum,
512-byte WRITE count/status time, and the existing over-100-ms status subsets. The strict parser requires exact command
and CSW-time sums against the original sample, the selected command's role and
maximum, payload/512-byte subset consistency and zero failures/unknown commands.
Every direct write sample remains two 16 KiB writes under role 6. Unclassified
work stays unclassified; these labels do not infer an operation or device cause.

A dry count through the actual row formats runs after measurement and before
success is reported. Rows must fit the existing 1024-byte line bound and 30720
bytes total, reserving 2048 bytes for control/lease/audio/wire lines within the
unchanged 32768-byte receipt and 64 KiB tail. Oversize evidence refuses completion;
it is never truncated or granted a larger log. The saved wire count is checked
against every actual row byte. The count uses fixed local metadata and no media,
clock, scheduler or heap operation.

Host checks compare both v1/v2 against a control using the previous inactive role
adapter: complete ordered BOT bytes, callback/clock counts and final media must
match for positive, refused and faulted cases. Existing quarantine, exclusive
range, rerun and full readback checks remain mandatory. Maximum-width actual
emitters separately verify exact decimal sizing and overflow refusal. Exact-stage
Q35 additionally requires the v2 role receipt and complete final media bytes;
native acceptance awaits the corresponding unit's matching return.

## Native v2 attribution baseline

The [Air6,1 trace return](machines/macbookair6-1.md#native-command-attribution-return-2026-11-12)
accepts release `084167c2be826f2a` for all original bounded checks plus v2 role
accounting: twelve samples in 5.714710 seconds, exact bytes and retired leases,
20133 receipt bytes, preserved files and clean offline allocation. No policy,
limit, barrier or integrity requirement changed.

The native write stages contain eight data commands transferring 128 KiB for
the 32 KiB logical file replacement, matching full-cluster initialization.
Steady 37-command samples and 56-command samples differ by +23 sidecar-update
commands, +4 allocation commands and −8 equality-proof reads. The update role
has nine reads, eleven writes and four flushes. This identifies additional
metadata work, but not which serialized field changed or why equality missed.
The next audit should inspect those decisions without bypassing necessary
metadata persistence, initialization or fresh equality checks.

Measured CSW callbacks peak at 4.185 ms, with zero recorded over-100-ms status
subsets. The earlier long loaded status waits and screenshot stalls remain
unresolved. Direct I/O timings exclude proof/retirement/remount work and do not
authorize a filesystem bypass or a throughput comparison that omits those costs.

## Stable metadata serialization and first-difference diagnostics

Sidecar records retain the same `SRVROSMETA1` format and all eight metadata
fields, but new serializations use bytewise path order. Live VFS enumeration
slots are not changed. A bounded pointer scratch is reserved before dirty
marking, sorted without recursion or I/O and released with the mount. Existing
unsorted sidecars remain readable and may need one ordinary replacement to
normalize order. Actual field/namespace changes still require persistence;
no equality result is cached and every matching byte is still read freshly.
Pending recovery names, live clients, allocation ownership, failure quarantine
and all publication/flush barriers retain their original behavior.

The real importer regression creates interleaved directory/file nodes and reuses
a VFS hole. A remount changes record order with identical per-path fields.
The legacy serializer rewrites that sidecar; stable serialization retains exact
bytes through four remounts. Fixed-clock no-op transactions retain their two
volume-flag writes and four flushes, while actual access-time, permission,
change-time and rename mutations remain persisted and independently checked.
These host results do not yet accept a native or sustained screenshot benefit.

The v2 control additionally emits paired final `exfat-sidecar` and
`exfat-sidecar-diff` counters at the existing completion timestamp. Differences
are classified by shape (length/valid length/allocation) or the **first differing
expected serialized field**: path, inode, mode, nlink, uid, gid, atime, mtime,
ctime or format. A path difference can mean ordering or a real namespace change;
this is not a complete semantic diff. No path or actual field value is logged.
Classification adds no reads, clocks, allocations or yields. Counters are
boot-wide and include preparation. Separate `storage-only-sidecar` rows retain
one bounded before/after counter delta for each original timed write: equal,
changed, shape or first-field mask/byte offset. Each filesystem write must own
one completed comparison; each raw write must own none. Fixed RAM snapshots
add no clocks, allocations, I/O or scheduler calls, and their overhead remains
inside the unchanged write interval. Per-sample deltas must reconcile with the
boot-wide counters and fit the existing dry-counted row budget. Their sum must exactly match changed comparisons; unavailable
proofs remain refusals. New candidates require the strict paired diagnostic in
addition to every original receipt and integrity check. Legacy v1/v2 receipts
remain readable. Maximum-width companion/control/lease/audio rows fit the same
2048-byte reserve; the 32 KiB receipt and 64 KiB tail are unchanged.

This serializer change leaves PR19's separate decoder/atomic-identity-restore
work untouched. It does not repair the existing offline-recovery limitations,
accept cold-boot identity restoration or establish why long loaded USB status
completions occur.


## Native canonical/per-sample return

The [Air6,1 return](machines/macbookair6-1.md#native-sidecar-remount-return-2026-11-12)
accepts the bounded functional/diagnostic checks on `998fdfb9b4f9c5a8` (source
`9c1bd01a`): twelve samples and six retired leases in 5.703998 seconds, all
original proofs, 21623 receipt bytes, prior-file preservation and clean offline
allocation. The final sidecar is canonically ordered. Three filesystem samples
compare equal; the three post-remount rewrites first differ in atime. Counts
remain 37/56 and write times remain approximately 47–79 ms; a native performance
improvement is not established. The host order-only reduction remains separately
proved. Both 1% boundary cues complete outside measurement.

Mismatch offsets align with the workspace marker atime field in the saved final
sidecar, and mount verification calls the atime-updating VFS reader. This is a
source/evidence-supported attribution of the first mismatch, not a complete
semantic diff of each intermediate expected table. File readbacks and ordinary
writes can change other timestamps too. Audit internal verification side effects
and the full field change set before deciding which traffic can safely be removed;
do not remove persistence, fresh reads or barriers just because the first field
is an access time. Cold-boot identity restoration also remains open: the prior-
record comparison finds 56 reassigned IDs through the unchanged legacy restore
path, with unique final IDs and unchanged permission/ownership fields. PR19's
separate atomic restore implementation is not part of this accepted runtime.
The long loaded USB waits and screenshot stalls remain unresolved.
