# Internal metadata payload padding

Replacing the metadata sidecar previously wrote its complete allocated clusters,
including the unused tail after its serialized byte length. A roughly109KiB
sidecar on the Air6,1's128KiB clusters consequently wrote extra padding on every
replacement. A populated complete-save replay demonstrates repeated tail writes
even when all required content, prefix and durability work is retained.

The internal data writer now receives an explicit metadata-payload flag. Only
the existing metadata-sync context writing an internal metadata path supplies
that flag. File creation and private replacement still allocate the same extent,
publish the same exact logical and valid lengths, and preserve the complete
mutation and clean-marker sequence. Directory initialization supplies false.

For the final partial group, the writer rounds the meaningful byte count up to
the larger of the filesystem sector and underlying device block size. It writes
all those bytes from the initialized scratch buffer. The final sector's padding
is therefore zero, and allocation slack beyond that sector retains its previous
contents. Unsupported alignment geometry retains the complete original group.
Ordinary payloads retain their existing whole-cluster initialization.

The exFAT stream length identifies the bytes belonging to the file. The metadata
writer publishes equal DataLength and ValidDataLength covering every serialized
byte. Reads and fresh proofs remain bounded by that logical length. The
specification's stream-length and allocation rules provide the format basis for
this interpretation. [Microsoft exFAT specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification)

Prefix comparisons, full fresh file and sidecar proofs, allocation validation,
checked rename, volume flags and every existing flush remain in place. The
scratch allocation, cluster allocation/chain, caller ownership and failure/
quarantine propagation retain their previous bounds and lifetime. This changes
neither the64KiB raw-write cap,16KiB USB command cap,256KiB reserved-append window
nor the native500ms/10s/1800s+60s requirements.

## Reproduction and validation

`make test-populated-save` uses the real process/VFS/exFAT code with832 seeded
files, persisted nonzero timestamps and one-second timestamp advances per foreign
barrier. It completes65 bounded process writes of a patterned4196406-byte
screenshot-sized payload, all65 foreign global syncs and marker updates, both
fresh final proofs and rename guards. It checks all original logical files,
full allocation and independent fsck against a whole-cluster-padding baseline.
The allocator's initially free space is poisoned to expose reliance on zero
media. Additional per-barrier test readback is measured separately. The modeled
CPU0 turns and block callbacks do not establish native timing.

Frozen-source controls reduce complete-save metadata writes by2770944 bytes in
the unreserved and reserved-sequential schedules, and1700352 bytes in controlled
reserved overlap. Logical media, prefix/read bytes, write counts and all flush
counts agree. These are deterministic source-byte reductions. Native timing and
the drive/controller's completion-tail behavior remain separate evidence.

`make test-exfat-metadata-tail` covers exact payloads, poisoned slack, final
sector zeros,512/4096-byte sectors and blocks,4/128KiB clusters, contiguous,
fragmented and grouped allocation, zero/edge sizes and allocation/write failure.
The full-driver comparison additionally checks metadata publication, every
write-fail/write-cut/flush-fail/flush-cut/torn-write boundary, protected files,
independent repair and complete allocation against the original-padding variant.
Fault images and allocation outcomes are retained locally. A legacy incomplete
repair remains incomplete evidence when it matches baseline.

`SRVROS_EXFAT_METADATA_TAIL_OFF` exists only under the host-test build for the
comparative original-padding implementation. Runtime code has no policy switch
or new ABI. The [Air6,1 record](machines/macbookair6-1.md) owns frozen validation
and physical acceptance. Source `cc4f9278` passes the matched normal/native/VM
builds, full core and desktop suites, documentation checks, ordinary ABI/input/
USB/capture VMs and populated Home audio/disk/apps/screenshot VM at400writeIOPS.
The exact staged native image safely refuses foreign hardware, saves and returns
in its VM rehearsal. These checks establish no native performance acceptance.

The full-driver fault comparison covers132 failure, cut and torn-write points.
Baseline and candidate agree on exits, logical/protected file maps, allocation
before repair, fsck outcomes and logical/allocation results after repair. In72
cases strict allocation remains incomplete after repair on both implementations;
these are preserved inherited gaps, not clean recovery or a recovery fix.

With the later lazy NoFatChain FAT work on source `72567e32`, the same strict
padding comparison covers114 remaining fault cuts, including63 matched
incomplete allocation repairs. The historical132/72 counts above belong to the
eager FAT implementation. Removing invalid FAT writes removes their fault points;
it does not establish a general recovery improvement. All original logical,
publication, repair and allocation comparisons still run. See the
[contiguous allocation contract](exfat-contiguous-append.md#deferring-invalid-contiguous-file-fat-work).

## Air6,1 native observation (2026-11-10)

Release `c95f851e735b98ff`, compiled from `cc4f9278`, completed the original
1800-second1% audio/disk/apps/three-screenshot workload and automatic Ubuntu
return. All16 original contracts pass independent saved-log/binary checks;
the native lifecycle log retains bounded tails and worst records, not every
individual app cycle. Direct read-only USB hashes and offline allocation pass.

Screenshot totals are8.824182/14.902387/15.328993 seconds, compared with
9.990791/14.004349/8.644861 in the preceding cold run. These do not demonstrate
a consistent native latency improvement. The controlled source-byte reduction
remains valid; different native interleaving is a separate observation.

Shared I/O admission occupies95.71/97.02/98.49% of raw write-call wall time.
Capture3 joins the retained wait to exact live and completed content, and to
one16KiB payload WRITE(10) command. Its2.918074-second owner scope includes
2.816760 seconds of payload-write callback wall; the selected command spends
354.680ms in status wait with a20.053ms maximum sampled service gap. Live
metadata-payload writes total110880 bytes and19.927ms in that observation.
The software clocks do not distinguish media/firmware from controller delivery.
Capture1 instead selects an open syscall outside the live sync observation;
it must not inherit capture3's phase distribution. Worker cleanup peaks at
62.800ms. The next performance target is long payload completion under shared
commit ownership, preserving proofs, barriers, byte caps and all original limits.
