# Exact screenshot gate-owner observation

A screenshot wait's longest overlapping I/O owner and global sync's longest
content commit can be different operations. Their PID alone does not join them.
The accepted Air6,1 a139 run exhibits that difference; see its
[native result](machines/macbookair6-1.md#native-reserved-append-result-2026-11-10).

When an observed wait selects an owner at outer I/O release, the scheduler
copies fixed live sync metadata under CPU0 interrupt exclusion. A parent joins
only when it is still active, has the owner's PID and began before that entire
owner interval. Active content joins only when its start also precedes the
entire owner interval. The reader never substitutes a completed maximum or a
previous call from the same PID. It performs no allocation, clock read,
transfer, logging, wait or admission change.

| State | Meaning |
| --- | --- |
| 0 | No sync observation was made, including a non-sync owner or disabled observer |
| 1 | Observer enabled, but no matching enclosing live sync |
| 2 | Enclosing live parent, without a content operation enclosing the entire scope |
| 3 | Enclosing live parent and active content, with copied cumulative I/O counters |

States0/1 are unavailable evidence, not proof of zero work. State2 can describe
device flushing or an outer scope that began before the active content. State3
identifies the current content slot/identity/inode/captured size and start time.
Its exclusive-role read/write/flush counters are cumulative from content start
through the sampled owner release; they can include an earlier I/O scope of
that same content. They do not claim this scope's exclusive callback costs,
completed writeback, durable bytes, physical media latency or CPU time.

The chosen owner and its observation are copied together into the wait's retained
maximum. Later owners, global maxima and process retirement cannot overwrite
that value. The original wall/blocked/READY/running partitions and all ordinary
versus proved-copy counts stay unchanged. Optional diagnostics confer no storage,
address-space or hardware authority.

`capture-write-gate-owner` version2 requires one exact snapshot/PID-bound
`capture-write-gate-sync` row and all six `capture-write-gate-sync-io` roles,
including explicit zeros. Missing/conflicting companions, failed/overflowing
counters, invalid clocks or parent/content containment are rejected. Version1
owner records retain their existing parser behavior. The new live sample does
not replace any existing full readback, checked rename, prefix proof, fsync,
global/device barrier, quarantine or final retirement evidence.

The extension is emitted only at the final checkpoint. Periodic snapshots keep
the version1 owner and their original byte format, without repeated correlation
rows competing with app receipts in the unchanged bounded log. The copied
observation remains in RAM through worker retirement. A halt before final can
therefore lack this extension; that is incomplete evidence, not a zero-work
observation. A production-formatter regression requires periodic output to equal
the legacy projection byte for byte.

`make test-io-scheduler test-sync-timing test-capture-write-timing` covers the
real scheduler with the real sync producer, actual producer/production formatter/
strict parser wiring, copied retention and adversarial joins. Controller and
clock backing in these host fixtures remain models. Loaded VM and native runs
must report their actual observation coverage; a non-sync maximum must not be
relabeled to manufacture a content match. Existing workload and timing limits
remain required. Attribution and any eventual latency improvement need fresh
evidence from the applicable machine.

## Native Air6,1 observation (2026-11-10)

Release `3403de90a056fca3`, compiled from `62670020`, completes the original
1800-second PCM/disk/app workload at1% volume with1055 app cycles, all three
screenshots, zero recorded underruns and clean retirement. All16 original
contracts pass a separate local raw-log reparse. The saved native lifecycle
records retain bounded tails and worst observations rather than every cycle.
Actual USB hashes, three full BMP decodes, the40-byte disk receipt, previous
files and offline allocation checks agree. See the
[unit result](machines/macbookair6-1.md#native-exact-gate-correlation-result-2026-11-10).

| Capture | Total save | BMP write phase | Raw-write admission wait | Share of raw-write call time |
| --- | --- | --- | --- | --- |
| 1 | 9.990791s | 5.367026s | 5.066478s | 95.55% |
| 2 | 14.004349s | 7.841077s | 7.491265s | 97.23% |
| 3 | 8.644861s | 5.584426s | 5.323592s | 96.54% |

Every retained longest wait selects PID7/thread20/SYS_SYNC87 and has a valid
enclosing live parent and active content observation, state3. The observed
filesystem ownership covers99.70–99.86% of aggregate admission wait wall time.
The ordinary write handlers themselves total1.374–1.386ms per screenshot.
Worker receive/allocation spans are271.578–333.039ms, and final receipt
observation takes0.737–1.824ms. Captures retire7.122/4.458/95.016ms after their
finished timestamps. Most save time is in writing and its shared storage waits;
the required verification and checked-rename phases also contribute.

At the three selected owner releases, cumulative root payload-write callback
wall times are403.267/1470.821/1171.760ms. Cumulative prefix-read callback times
are739.293/36.690/10.535ms for2555904/2555904/458752 bytes. Their enclosing
owner scopes are1314.213/1595.471/1267.778ms. Content observation begins73/80/74us
before the respective owner scope, so these are closely aligned cumulative
observations. They still do not assign every callback exclusively to the owner
scope or the same phase distribution to other waits.

Capture2 now exactly joins the independent completed-content and command
observations, using PID, parent ID614565564, content identity3570, start and
captured size3145728, with the complete retained command inside the owner
interval. Its36 root payload WRITE(10) commands total589824 bytes. Their status
callbacks total1100.147ms, with three above100ms. The selected16384-byte command
takes380.845ms, comprising8us command submission,1.038ms data transfer,
359.848ms status wait and19.945ms checkpoints, with6us residual. The status
event is observed1us before caller resumption. Its sampled service gap is at
most20.058ms across39200 checks. These observations localize a long wait before
software observes USB completion. They do not distinguish device firmware/media
behavior from controller delivery. Capture1/3 do not join that independent
global maximum and retain their own live observations.

The same capture2 content completes with four qualified appends totaling262144
bytes, committed size3145728 and live size3407872. Dirty state survives. This
provides a native active-append observation for the
[reserved-append contract](reserved-append.md). Each screenshot takes25–27
qualified copies and38–40 ordinary writes. The bounded copy path therefore
works while remaining ordinary writes still encounter long storage ownership.
The records carry content identity and length, without a direct pathname or
writer-PID binding.

The next reproduction should preserve this exact overlap and its long write
completion while auditing the exFAT commit and ownership boundaries. Prefix
proof work also needs coverage. Required pre-entry dirty contents, full prefix
and fresh-file verification, checked rename, all barriers,256KiB suffix window,
64KiB writes,16KiB commands and existing timing/runtime limits remain required.
This diagnostic run establishes attribution and bounded functional acceptance;
it establishes no causal latency improvement or underlying USB-device diagnosis.

## Commit-boundary replay and Ubuntu reference (2026-11-10)

`make test-sync-commit-boundary` exercises the real process/VFS/exFAT code with
the captured2555904-byte old prefix,3145728-byte writeback and four concurrent
64KiB copies. The live length becomes3407872 and the suffix remains dirty after
the captured prefix commits. The fifth copy refuses at the existing256KiB
window. The fixture models the independent CPU0 turn and callback clock; its
small namespace and block callbacks differ from the native USB workload.

Control and an injected359.848ms root-data callback delay produce identical
ordered source I/O and complete media hashes. The selected commit performs177
source callbacks,159 reads/2687289 bytes,10 writes/589941 bytes and eight flushes.
Its final screenshot uses65 bounded process writes and two full fresh proofs, checked
rename guards, complete allocation traversal and independent fsck. The modeled
content wall span increases from2.000ms to361.848ms, with the entire injected
delay appearing in data-write timing. These are synthetic clock results, with
no physical USB command or real scheduler wait in this fixture. The original
reserved-append schedule and payload/final-flush fault cases still pass.

The ownership audit follows `process_file_sync_all`, `content_flush_observed`,
`mutation_begin_result`, `append_file`, `mutation_end_result` and
`scheduler_io_end`. The content borrow and transaction span the prefix proof,
allocation/data writes, length/sidecar publication and final clean-marker
barrier. Waiting filesystem operations regain access after that transaction
retires. Current global sync visits each live dirty content once, including
reader-only survivors. No equivalent source-work reduction was found in this
bounded audit; a runtime latency fix remains open.

Three bounded Ubuntu references used new scratch files on this unit's actual
USB, verified aligned16KiB direct writes and fresh readback, removed scratch
data, and verified all832 existing file hashes and clean3910-cluster allocation
before and after. They retained Ubuntu recovery and the installed SrvrOS payload.

| Ubuntu reference | Main16KiB writes | Main maximum | Other-file maximum | Other-file writes above100ms |
| --- | --- | --- | --- | --- |
| Sequential4MiB with256KiB fsync groups | 256 | 4.183ms | — | — |
|64 mixed256KiB create/fsync/rename/read/unlink/volume-sync bursts | 256 | 5.445ms | 264.650ms | 3 |
| Same mixed reference with USB monitoring | 256 | 4.251ms | 274.095ms | 2 |

The two mixed references also fsync and reread the complete growing main file
after every64KiB. They finish in8.446/8.566s. Their direct-file syscall wall spans
include Linux allocation and metadata work. The references use Linux filesystem
and transport behavior, without the native30-minute load or an identical
metadata namespace/command schedule.

The USB-monitored run contains11430 fully matched CBW/data/CSW commands,1826
writes and1280 exactly16KiB writes, with zero dropped text records, pending
commands or unmatched events. The slowest write is a512-byte WRITE(10) with
CDB flags zero. Its command takes274.124ms, of which274.030ms is the submitted
status URB awaiting software completion. The maximum16KiB write status span is
19.523ms. Linux also emits one130560-byte metadata command, so this reference
does not establish matched native command sizing. Existing SrvrOS16KiB command
caps remain in effect.

Long software-visible USB write-status waits therefore occur under Linux on
the same hardware too. The native slow16KiB payload commands remain a distinct
observation. Neither reference timestamps physical packets or identifies media
firmware versus controller delivery. The evidence supports transaction-coupled
tail latency and continued work on equivalent commit traffic reduction, without
claiming a source fix or assigning every native delay to the USB medium.
