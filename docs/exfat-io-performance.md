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
paths relative to the frozen runtime. Air6,1 installed c7/recovery c354 and USB
contents remain unchanged. Native filesystem and screenshot latency for these
passes is still unmeasured.
