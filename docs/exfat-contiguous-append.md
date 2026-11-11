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

Unnecessary unpublished FAT links, tail attachment and invalid NoFatChain FAT
initialization/clearing are omitted. Every allocation bitmap update remains.
Directory/system allocation still initializes its chain. Both the data
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


## Deferring invalid contiguous-file FAT work

Contiguous file allocation and release now leave the stream's ignored FAT cells
untouched. The freshly read allocation bitmap, exact extent, alias ownership and
all existing validators remain authoritative. This follows the same format
rule above; stale free FAT values do not authorize an allocation or a chain.
First-fit allocation order, directory initialization, fragmented allocation,
command sizes, transport ownership and timeouts are unchanged.

Whenever append needs a real chain, its new contiguous extent is fully
materialized, including the final EOC, before publication. Existing old-prefix
links and the final tail attachment retain their ordering and barriers. A single
new cluster also needs an explicit EOC: allocation no longer supplies it as a
side effect. Every required FAT write still quarantines on failure. The
512-byte bounded writer preserves adjacent cells and sector boundaries.

`make test-exfat-nofat` tests poisoned FAT, exact bitmap ownership, directory EOC,
full-chain terminals across boundaries, allocation failure and required-write
quarantine. It also runs the actual process/VFS/exFAT populated screenshot save
against a host-only reproduction of the old eager FAT work. All65 screenshot
writes,65 foreign syncs, logical files, reads, prefix proofs and flushes match.
The sequential cases remove393 source write callbacks/1572 bytes; the overlapping
case removes304 callbacks/1240 bytes. These are source callback counts, not
physical command counts or a claim of native latency improvement. Required
terminal work can move into append preparation, so per-phase totals are checked
alongside complete-save totals rather than assuming every phase decreases.

Both ordinary and eager-FAT append comparisons retain complete prefix checks,
negative topologies and write/tear/flush fault replay with stale free FAT.
The current lazy path has131 remaining fault cuts versus the historical143
above; only omitted invalid FAT writes remove boundaries. The81 inherited
strict allocation-repair failures remain recorded and compared, never called
clean. No validator, barrier or recovery requirement is relaxed. Existing
`links_avoided` telemetry still counts adjacency links, not deferred FAT work.


## Air6,1 lazy-FAT native observation (2026-11-11)

Release `c7d2293b66b7999b`, compiled `72567e32` (PR467), completes the original
1800-second1% audio/disk/apps/three-screenshot workload and automatic Ubuntu
return. Independent parsing validates all16 original contracts plus the queued
wait companion, exact BMPs/binary receipt, actual file hashes and full offline
allocation. All850 prior files are preserved except the expected metadata
sidecar;859 files and4231 allocated clusters have zero leaks/mismatches/errors.
This accepts bounded native functionality, not a performance improvement.

Screenshot totals8.049986/9.633137/17.522316 seconds are slower overall than the
preceding7.479590/7.281154/10.530490-second cold run. This is not an isolated
causal comparison; the source-work reduction does not establish a latency fix.
Private-prelude wait still occupies96.36/96.25/98.90% of raw write-call wall,
while ordinary gates are tiny and cleanup peaks at66.079ms. A separate global
content call lasts3.584277 seconds, including3.117253 seconds of payload-write
callbacks. It contains352.037ms16KiB payload and355.361ms512-byte allocation
status waits. These software observations do not distinguish device/firmware
from controller delivery, or identify the owner of each private wait. Completion
tails/shared commit waiting remain open; no integrity check or limit is relaxed.
