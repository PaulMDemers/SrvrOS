# Retaining contiguous exFAT append allocation

The append path keeps an existing NoFatChain file contiguous only when its
freshly allocated suffix is itself contiguous and starts exactly after the old
allocation. The old stream must already be NoFatChain. Existing alias, valid
length, allocation size, sector alignment and complete fresh-media prefix checks
still run before allocation. An old FAT-backed file is never reclassified.

This follows [Microsoft's exFAT specification, section6.3.4.2](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#6342-nofatchain-field):
NoFatChain describes a contiguous allocation whose FAT entries are not used.
Allocation bitmap ownership, private allocation and the exact physical adjacency
are still required. A byte mismatch, read failure, alias, nonadjacent/fragmented
suffix or old FAT-backed stream retains its original refusal/replacement/linking
path. No stream geometry gate, bulk/copy limit or scheduling eligibility changes.

Only unnecessary unpublished FAT link materialization and tail attachment are
omitted. Allocator FAT initialization and bitmap writes remain. Both the data
flush and original added-link flush still execute, followed by the existing
checksummed stream length/flags publication, metadata flush/sidecar work, outer
mutation flush and clean-marker publication. A failed operation retains the
existing dirty/quarantine and uncertain-publication semantics. This is not a
crash-atomic naming guarantee or a substitute for offline allocation traversal.

The explicit existing sync diagnostic prints one fixed aggregate row:
`exfat-append-contiguous: version=1`, with snapshot time, `local_complete`,
`links_avoided`, `prefix_bytes` and `invalid`. Counts update atomically without
allocation, I/O, printing or clock reads in the append path. They describe local
append phases after their barriers; the enclosing mutation can still fail its
final flush or clean marker. They never establish operation durability, physical
bytes saved, per-file attribution or time saved. Normal profiles remain quiet.
The strict `append_contiguity` decoder checks bounded, consistent, monotonic
observations and distinguishes zero observations from an exercised path.

The deterministic fixture uses the real process/VFS/exFAT code with128KiB
clusters, controlled adjacent and nonadjacent topologies, FAT-backed/fragmented/
alias/changed-prefix fallbacks, exact flush order and prefix read coverage. Its
positive cases save three FAT writes while retaining all eight flushes. Fault
cuts must preserve original payload sectors and unrelated files; compare
publication/repair limitations with the same logical baseline cut and retain
raw failure evidence. The historical and HOST-off comparisons each exercise143
cuts. In81 cases, strict traversal still reports orphan allocation after fsck
claims clean; every such result is matched against the corresponding baseline
cut and retained as failed recovery. A clean fsck result alone does not establish
clean allocation. Runtime return values and quarantine remain separate from the
local-phase counter.

The ordinary65-write screenshot replay is deliberately not forced adjacent.
It grows the complete4196406-byte image in at most64KiB writes with foreign
global sync after each, another dirty file and shared readers/writers. At the
initial replay neither baseline nor candidate exercises the new path; both
perform13787 callbacks,904 writes,889 flushes and136314880 prefix-read bytes.
The optimization is therefore not a demonstrated fix for that workload's
admission delay. Do not bypass dirty-content sync or weaken prefix/barrier
requirements to manufacture a gain. Native use and performance remain unaccepted
until matching evidence exercises the relevant path.
