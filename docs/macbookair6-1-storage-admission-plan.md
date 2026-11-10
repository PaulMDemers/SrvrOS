# Air6,1 screenshot write-admission audit

Base: `f5043c4a`, after native acceptance of PR455/release `d551c6584bed346e`.
This work changes no physical payload or power policy and arms no reboot.

## What the native evidence establishes

Three screenshots spend10.036/16.348/8.631 seconds in WRITE. Their65 raw writes
consume9.942/16.255/8.564 seconds;95.9–98.2% is admission rather than the roughly
21ms handler work. The second capture's longest gate overlaps PID7's global
sync, whose selected growing1.44MB content commit takes2.82s. Most of that commit
is data-write callback wall; prefix checking is a smaller52ms component in this
selected operation. These are software intervals, not physical completion time.

Existing admission handoff already rotates waiting threads at fully unwound I/O
boundaries. It cannot admit another mutable filesystem operation while exFAT
still owns a transaction. Global sync already visits each dirty content object
once, including reader-only survivors. Descriptor deduplication, omitting a
foreign dirty file, skipping fresh prefix comparisons, removing flushes or
changing the stream-cluster gate would not be equivalent fixes.

## Deterministic reproduction and bounded optimization

The real process/VFS/exFAT fixture uses128KiB clusters, the exact4196406-byte
image,65 ordinary writes capped at64KiB, foreign global sync after each write,
a second dirty file, independent writers and a surviving reader. Each barrier
must persist all then-visible bytes of both files. It proves full media contents,
namespace publication, readback, allocation and unchanged anchor data.

This legal worst-interleaving schedule commits140511286 accumulated payload bytes
and compares136314880 bytes of committed prefixes. It is not a reconstruction of
all native interleavings or a measured native byte count. Native wall-time
observations and host callback/byte counts must remain separate.

The [adjacent append optimization](exfat-contiguous-append.md) avoids converting
an old NoFatChain extent when the fresh allocation is exactly contiguous with it.
Controlled positive cases omit three unnecessary FAT writes while preserving
all eight flushes and every prefix byte. Nonadjacent, fragmented, old-FAT,
alias and changed-prefix cases retain the baseline callback path. The ordinary
65-write schedule exercises zero such adjacent completions; its13787callbacks,
904writes and889flushes match baseline. This is a valid local representation
optimization, not a demonstrated screenshot-delay fix.

Power-cut tests must retain both fsck and complete allocation results. Some
baseline cuts lose a checksummed filename on a torn publication or leave orphan
allocations after fsck reports clean. Matching those outcomes does not make them
clean recovery or authorize reusing quarantined media. Preserve the exact
candidate/baseline fault images and logical cut mapping.

## Next contract to design before more code or hardware

A useful next candidate must let bounded RAM-only insertion make progress during
unrelated I/O without racing the content currently being written back. That
requires a new reviewed ownership contract, rather than classifying every file
write as safe:

1. Prove a stable process/image, FD generation, write description, private content
   and pre-reserved owned capacity. Admit only an already mapped bounded copy
   with no allocation, namespace lookup, signal delivery or backend operation.
   Ordinary shared/failed/unlinked, redirected, growing-capacity and unsupported
   cases retain the current gate.
2. Give writeback an explicit stable-content snapshot/active state and generation
   discipline. A concurrent append must never move or mutate a borrowed buffer,
   disappear under a later dirty-clear, violate shared-reader visibility or
   overwrite data being committed. Every global sync still flushes all required
   dirty contents; no deferred pre-entry dirty-data exception is allowed.
   Preserve an explicit sync invocation/ownership-entry cut. The existing IDT
   handoff can already run an overlapping queued writer after final flush but
   before SYS_SYNC returns to userspace; it does not promise that every byte
   visible at userspace return is durable. Any concurrent append proof must
   retain all required pre-entry prefixes and cover dirty-generation clearing,
   cursor/O_APPEND, metadata timestamps and retryable/sticky failure transitions.
3. Exercise atomic check/publication, all mutation/close/rename/truncate/reuse and
   failure races with actual process-content code. Prove same-process exclusion,
   FSIZE/size/copy bounds, cancellation, cleanup and fallback before changing a
   syscall admission path. Existing64KiB writes,16KiB commands,500ms/10s and
   full workload/barrier requirements remain.
4. Compare the unchanged deterministic replay, matched build/core and loaded VM
   first. A native trial requires a demonstrated relevant reduction and complete
   safety/ownership evidence. Zero-path or unchanged-work results do not justify
   an unchanged-policy reboot.

This is a proposed follow-up design, not an implemented eligibility grant or a
claim that the gate is incorrect. Persistent data/ownership and integrity take
precedence over a predicted latency improvement. Screenshot child-status and
temporary-cleanup receipts, offline repair gaps and the earlier intermittent
battery failure remain independently recorded work.


## Frozen local validation checkpoint (2026-11-10)

Compiled source `f44b7967fa46d2ea419c26795e69f4ee750dec8e` passes matched images,
full core/desktop/docs,89 hardware-install tests (one skip), ordinary ABI/input/
thread boot checks, real USB persistence, ordinary screenshot/readback/Preview
and populated Home400-IOPS loaded VM. Exact-source143-cut fixtures preserve
all original payload/anchor bytes and explicitly match81 failed allocation
repairs to baseline; retained lossless candidate/baseline images are hash-checked.

The loaded VM completes22 app cycles with zero recorded audio underruns,
140.143ms maximum loop,0.979324s ready and1.830419s close maxima. All prior files,
raw BMP/binary receipt, original timing/proof/retirement contracts and new local
append observations pass independent collection. App timing uses the existing
original-console fallback for bounded saved-tail gaps; do not call it complete
saved-only per-app evidence. The new counter is valid but zero in seven saved
snapshots. No relevant performance gain or native use is claimed, and no payload
was staged, installed or rebooted for this bounded optimization.

The main admission-delay work continues with the separate immutable-prefix/
reserved-buffer design above. That implementation must first prove its opt-in
ownership, raw syscall/user-copy, ordinary fallback and explicitly partitioned
diagnostics; it does not inherit authority merely from this local validation.
