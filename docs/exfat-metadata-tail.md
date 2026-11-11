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

Development controls reduce complete-save metadata writes by2770944 bytes in
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
and physical acceptance. Full frozen integration and native performance are
pending at this implementation checkpoint.
