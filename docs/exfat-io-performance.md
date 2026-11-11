# exFAT read and write work

The filesystem keeps synchronous publication, dirty/clean markers, fresh prefix
and final proofs, exact allocation checks and existing ownership. These passes
reduce work within that contract. They do not change USB command sizes/timeouts,
flushes, cache capacities, lease limits or desktop timing requirements.

## Bounded streaming

Stream append keeps NoFatChain for an empty contiguous allocation or an exactly
adjacent contiguous extension of an existing NoFatChain file. Other allocations
get a real FAT chain: every new link and the final EOC is explicitly written
before publication. Old-prefix conversion uses the existing bounded link writer.
All original data/link/stream publication barriers remain, even when no tail
link needs writing. Alias/client exclusions and terminal lease behavior remain.

This also corrects a missed dependency from lazy FAT allocation: the separate
stream writer still assumed allocation had initialized EOC. The old one-byte
stream fails the strict allocation check. Forced-FAT and contiguous variants now
pass with poisoned free FAT, independent fsck and identical logical files and
flush counts. The stream and stream-allocation regressions are core targets;
large files, fragments, zero-valid tails, stale leases, aliases, failed writes,
failed flushes and allocation failure after a mutation write remain covered.
The existing64KiB stream/cluster admission gate is unchanged.

## Bitmap I/O and scratch

Allocation, freeing, allocation checking and stream preflight need the declared
bitmap bits, rather than allocation-cluster slack. They read/allocate that range
rounded to the existing device-block boundary, bounded by the original single
cluster. Rounding avoids splitting a final device sector into extra requests.
Logical bit counts, first-fit order, neighbouring bits, full allocation checking
and every bitmap write are unchanged. Unsupported mutation geometry refuses.

The individual bitmap allocation gets smaller. Whole-operation peak heap can
remain unchanged when a different buffer dominates; it is measured, not inferred.
The host-only full-bitmap-write control retains a full initialized cluster of
scratch automatically; it cannot write a compact buffer as a full cluster.

## Transaction-owned FAT reads

For128KiB and larger clusters, FAT traversal reads a bounded512-byte window
through the existing four-row transaction cache. Every target link is still
interpreted and checked independently; physical adjacency is never inferred.
The final window stops at the declared FAT-entry range. Successful FAT writes
patch the owned copy, failed/partial writes invalidate it, and the cache is
retired at the same outer transaction boundary. Ordinary reads outside that
owner keep the direct-entry path. Fresh sidecar proofs remain uncached.

Full prefix-payload reads bypass the metadata cache but still read every byte
through the block layer. This removes useless payload copies and preserves FAT
and directory rows. Smaller clusters keep their single-link form: their block
cache can already keep the FAT sector hot. Using the window there suppressed
useful cache touches and caused extra later read/modify/write reads in the
initial benchmark. The geometry selection removes that measured regression;
it changes no correctness gate, transfer bound or acceptance requirement.

## Reproduction

`make test-exfat-io-stack` runs the real filesystem and block-cache code against
disposable synthetic media. The device callbacks enforce the existing16KiB cap
and512/4096-byte device alignment. Three host-only controls isolate original
bitmap/cache work, compact bitmap work and the full candidate. Growth performs
all65 saves of a4196406-byte patterned file; replacement and create/rename/delete
cases exercise repeated metadata work. Every variant must produce the identical
complete image and logical files, with equal device writes, bytes written and
flushes. Readbacks discard clean block-cache copies, then independent fsck and
strict full allocation checking run. No case may increase device reads or bytes
in the final candidate relative to the bitmap pass.

The populated128KiB-cluster/512-byte-device controls currently show:

| Workload | Before reads | Bitmap pass | Full candidate | Before read bytes | Candidate read bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
|65-save growth |12868 |11755 |10825 |179819520 |162172928 |
|16 replacements |514 |363 |363 |6289408 |4061184 |
|8 create/rename/delete rounds |642 |434 |434 |8446976 |5301248 |

Growth retains2958 device writes,27740672 bytes written and1201 flushes. The
4KiB-cluster controls and4096-byte device variants also retain exact media and
write/flush results without a read-work regression. These are deterministic
completed callback counts, not native latency. Synthetic32MiB volumes have
smaller bitmaps than the physical USB drive, and scheduler/clock adapters are
host models. The native c7 observation still has long completion tails; this
candidate needs matching physical evidence before any screenshot speed claim.

`make test-exfat-fat-window` checks exact links/terminal, complete prefix reads,
partial FAT patching, failure/reentrancy, final bounds and transaction retirement.
`make test-exfat-stream-allocation` compares valid forced-FAT and contiguous
stream implementations, with exact logical files and flush counts. The original
[contiguous append](exfat-contiguous-append.md),
[metadata payload](exfat-metadata-tail.md) and
[reserved append](reserved-append.md) contracts still apply. Inherited interrupted
write/repair limitations remain visible failures; these passes do not claim a
recovery fix or authorize dropping a proof/barrier to obtain a speedup.

## Validation and acceptance

Runtime source `e6d06f2a` passed matched ordinary/native/VM builds, all 244 default
core targets, desktop and documentation checks, stream service ownership tests,
ordinary ABI/input/thread VMs, real USB transport and ordinary capture VMs.
The populated Home KVM stress test at 400 write IOPS passed all original
evidence contracts, including screenshot readback, complete saved app timing,
commit/command attribution and clean post-playback battery retirement. It ran
60 seconds at 1% volume, with 231 disk bursts, 24 app cycles and zero recorded
audio underruns. Exact-stage UEFI also passed the expected foreign-board refusal,
save/unmount and automatic return; that is not native battery acceptance.

The first stress attempt completed its workload but was rejected for an absent
app-timing begin receipt: adjacent fixed-size log tails left a gap and serial
output interleaved. The failure is retained. An unchanged-source/image retry
passed with complete saved records; no parser, workload, cadence or limit was
changed. This does not establish lossless timing coverage in every future run.

A subsequent host-only full-bitmap-control scratch correction passes its
original five-operation exact-media/write/flush comparison. Preprocessed token
comparisons verify unchanged production, default-host and other measured control
paths relative to the frozen runtime. At that host-validation checkpoint,
Air6,1 installed c7/recovery c354 and USB contents remained unchanged.

## Air6,1 native comparison, 2026-11-11

Release `80b4e32ef314ae05`, compiled from `820eb85c`, completes the original
1800-second 1% PCM/disk/app/three-screenshot workload in session
`session-0006600400f302c0-00`. Independent raw-log/binary review passes all16
original contracts and the queued-append companion:1071 app cycles,120687us
maximum loop, zero excursions, zero recorded audio underruns and clean capture,
desktop, file/cache and battery retirement. All859 prior files survive except
the expected metadata sidecar update. Actual rehash, fsck and full allocation
verify868 files/4338 clusters, zero leaks, mismatches or errors.

| Capture | Prior c7 total, seconds | Candidate total, seconds | Prior WRITE phase, seconds | Candidate WRITE phase, seconds |
| --- | ---: | ---: | ---: | ---: |
|1 |8.049986 |7.307485 |4.562893 |4.378074 |
|2 |9.633137 |10.890831 |5.180639 |6.668082 |
|3 |17.522316 |12.560273 |14.780125 |9.959347 |

The worst capture improves about28.3%, while the middle capture worsens about
13.1%. Burst count is3895 versus3954 previously. This is one observational cold
boot comparison with different media state and interleaving; sustained throughput
improvement and isolated causal attribution remain unproved. The native selected
allocation sample reads59392 bitmap bytes versus131072 in the prior sample,
confirming reduced bitmap work on the physical geometry. These are selected
calls, not full-run native I/O counters. The unchanged64KiB stream admission
gate also leaves generic bounded-stream coverage on this128KiB-cluster drive
separate from the host stream regressions.

Private prelude wait wall is95.69/97.50/98.45% of raw capture write call wall.
It overlaps handler time for successful copies; these are not disjoint phases.
Wait maxima reach3.615494 seconds;17 ready-under-live-borrow retries exercise
the unchanged recheck. Ordinary gates total only180/165/183us and have
no retained owner. A separately bound1MiB global content call during capture3
takes3.614946 seconds, including3.532703 seconds in payload callbacks. Forty
16KiB payload status waits total3.188556 seconds; nine exceed100ms. The maximum
is353518us with10069us maximum sampled service gap. Its512-byte allocation
status maximum is1906us. Software clocks still cannot separate media/firmware
from controller delivery, and no exact command/owner identity joins an individual
private wait to that selected global call. These waits remain the next target.
The subsequent [commit-wait isolation](commit-wait-isolation.md) adds a bounded
completed-overlap companion rather than assigning an unrelated global maximum.

Installed candidate, c354 recovery, retained c7, retired2dd's independent payload,
original failed-media archive and trial backup verify. Ubuntu remains default,
the one-shot is consumed, collection is disabled and USB is unmounted. Native
logs retain bounded lifecycle tails and worst records, not every individual app
event; the VM's complete per-cycle coverage remains separate. Subjective sound
at1% was not assessed. No policy, limit, proof or barrier changed in this trial.

## Exact private-wait isolation, 2026-11-11

The [owner/status companion](commit-wait-isolation.md) now passes the native
Air6,1 1,800-second 1% workload on `4598c7e5aac15a6b` / runtime `4f4feeeb`.
The matching raw-log/binary review retains all 16 original contracts and the
append-wait proof: 4,062 verified disk bursts, 1,088 app cycles, three exact
screenshots, zero recorded underruns and clean retirement/allocation. Actual
rehash preserves all 868 prior files except the expected metadata sidecar;
877 files/4,445 clusters have zero leaks, mismatches or errors.

The exact worst private wait is 2.894034 seconds. Actual owner intervals cover
2.893501 seconds; PID7/thread20/SYS_SYNC87 supplies the longest intersection.
Its completed content separately matches the wait target and enclosing interval.
Owner status callbacks overlap 2.521687 seconds of the bracket (87.13%). The
16 KiB WRITE status subset dominates: 43 callbacks total 2.494952 seconds;
ten 512-byte WRITE statuses total only 19.214 ms. Seven content payload statuses
exceed 100 ms. The worst is 352.020 ms, with 351.745 ms before observed completion,
273 us to resume and a 10.079 ms maximum sampled service gap. That separates
this wait from a multi-second post-completion scheduling delay. Software event
observations still cannot assign the long completion interval to device media,
firmware or controller delivery. The observer retains one worst wait, not every
storage operation.

Screenshot totals are 6.405844/10.675969/26.604312 seconds versus prior
7.307485/10.890831/12.560273. The third WRITE phase takes 13.554029 seconds;
fsync and both proof phases also take seconds, while pixel transfer and cleanup
remain subsecond. No performance improvement is accepted. Next audit the long
payload status-completion path and shared commit ownership while preserving
command/request sizes, fairness, deadlines, fresh-media proofs and flushes.
All limits and integrity checks remain unchanged; no new run is armed.
