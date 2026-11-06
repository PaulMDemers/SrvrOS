# Air6,1 checked2MiB ECAM mapping proposal

## Status and scope

Host-only design on integration base `4cc0408b`. The [layout audit](macbookair6-1-dma-inventory.md#ecam-layout-and-fixed-bound-feasibility-audit-2026-11-05)
shows that the existing156-bus4KiB ECAM mapping alone needs at least81 tables,
beyond the unchanged64-table/32768-entry census bounds. This proposal preserves
all39936 physical4KiB mappings, attributes and PCI config address calculations,
using78 prospective2MiB leaves. The actual backend and fresh pre-PCI gate are now
implemented in the isolated AP ELF, with no boot-dispatcher caller or hardware
activation yet. The host proposal remains a historical/input model, not a grant.

`tools/haswell_ecam_mapping_proposal.py` consumes a complete exact-unit raw
CPU/cache receipt through its existing strict decoder and a complete checksummed
MCFG through the layout decoder. It requires the captured single segment0,
buses0–155, physical `0xe0000000` / virtual `0xffffc00300000000`,156MiB geometry.
The saved native context classifies the whole range UC with uniform MTRR types.
The result is `HOST_ECAM_MAPPING_PROPOSAL_ONLY`; historical data grants no live
root, feature, cache, mapping, frame or ownership permission.

## Leaf equivalence

| Property | Existing4KiB PTE | Proposed2MiB PDE |
| --- | --- | --- |
| Present, writable, supervisor |P=1,RW=1,US=0 |same |
| Execute/global |NX=1,G=0 |same |
| Cache selector |PCD=1,PWT=0,PAT bit7=0: index2 |PCD=1,PWT=0,PAT bit12=0: index2 |
| Page-size bit |not applicable |PS bit7=1 |
| First raw leaf |`0x80000000e0000013` |`0x80000000e0000093` |
| Covered physical range |`0xe0000000` through `0xe9bfffff` |same |

The host proposal expands every large leaf into5124KiB translations and compares
address, permissions and cache selector against the original recipe. It evaluates
all39936 granules under both historical CPU contexts. Every granule inside one
large leaf must have the same resolved MTRR/PAT/effective type, and effective UC;
undefined overlap, a changed endpoint or nonuniform raw MTRR type refuses even
when the effective type alone happens to match. Addresses below1MiB refuse in
the address-based classifier because fixed-MTRR interpretation is outside its
scope. The existing target classifier remains a wrapper at `0xfed90000` and its
accepted raw receipt replays identically.

Intel SDM Vol3A12.11.9 requires care with MTRR granules inside a large page;
12.12.3 distinguishes the large-leaf PAT position. The audited primary PDF/text
and their hashes remain in local evidence; no PAT, MTRR, CR0/CR4 or EFER writer
is proposed. This equivalence design changes the representation of the existing
PCI mapping policy; it does not supply the separate VT-d full-alias/resource grant.

## Checked backend contract

The intended backend is a narrow boot-only checked operation, separate from the
generic4KiB map/ensure/unmap APIs. It must provide three distinguishable results:
clean refusal, published boot ownership, and poisoned/retained uncertainty.

1. Enter an explicit CPU0 bootstrap epoch with IRQs masked, current untagged
   kernel root, no other executing CPU/root client and all relevant mutations
   excluded. Verify fresh exact-unit identity/MCFG, cache and NX capability/state.
   The existing late diagnostic's PCI-dependent identity gate cannot simply be
   called before PCI inventory; a reviewed fresh pre-PCI gate is still required.
   Historical receipts and synthetic model guards cannot substitute for it.
2. Validate the borrowed root and existing PDPT RAM sources and complete raw
   path. Require the exact zero PDPT slot at PML4 index384 / PDPT index12. Refuse
   missing borrowed PDPT, software-marked absent entries,4KiB tables, large leaves,
   stale metadata or conflicting mappings; do not split, coalesce or overwrite
   an existing live mapping. Preserve all other borrowed slots exactly.
3. Allocate one privately owned zeroed PD frame, fill78 validated large leaves
   and leave434 entries exactly zero while it remains unreachable. Check fresh
   frame identity/tag/lifetime and verify the complete staged page and guards.
   No borrowed root, PDPT, firmware or MMIO frame may be freed. Allocation/staging
   failure before publication leaves borrowed mappings unchanged and releases
   only the proven detached owned frame.
4. Under uninterrupted exclusion, revalidate the exact borrowed path and zero
   slot, order prior table stores, publish one parent link and verify readback.
   Do not allocate, yield, call workers or expose an ECAM pointer in this interval.
5. Invalidate every one of the399364KiB virtual addresses before PCI handoff.
   This deliberately retains the original invalidation coverage; it does not
   assume one INVLPG per2MiB removes every potentially stale small-page/global
   translation. No CR4 policy toggle or unproved remote shootdown is used.
   Intel SDM Vol3A4.10.4.1 is the invalidation reference. Verify raw path, staged
   page, exclusion and invalidation completion before declaring the window ready.
6. Preserve the old public mapping-registry registration order/cap/flags for
   the39936 original4KiB records. Commit metadata only during final handoff after
   verified readiness, before exposing config pointers. Unexpected partial
   metadata publication is terminal retained failure; it cannot use the clean
   pre-handoff retirement route. The128-entry list remains intentionally partial.
7. Successful handoff owns the mapping for the rest of the boot. No generic4KiB
   unmap/protect, range reuse or after-handoff retirement is authorized. Later
   user roots inherit the upper mapping and still require checked preparation.

A deliberate retirement before metadata/PCI handoff requires exact owned link,
full private page and guard equality, verified unlink, all39936 local invalidations
and an unchanged zero parent/unreachable frame before freeing that owned PD.
Unexpected A/D bits are changes, not silently masked successes. Any uncertainty
at/after publication, readback, invalidation, metadata or retirement permanently
poisons the operation and retains its frame; no4KiB fallback or second attempt
may reuse the affected range. A future caller must surface that terminal state.
A native implementation needs real publication ordering, checked RAM access,
fault containment, frame-lifetime and invalidation evidence; the host model has
none of these hardware mechanisms.

## Model and actual-source tests

`PublicationModel` is synthetic Python state, with no real allocator, PTE store,
INVLPG or I/O callback. It models detached preparation, exact-zero admission,
single-link publication,39936 invalidations, pre-handoff retirement and permanent
handoff. Twelve test groups cover leaf geometry/attributes, malformed/foreign
receipts, changed/nonuniform/undefined cache state, every modeled guard, occupied
slots, staged-page changes, allocation/staging failures, publication/readback/
invalidation faults, foreign/A-bit-changed links, cleanup faults, terminal poison,
repeated calls and invalid fault cursors. This tests the specified protocol; it
is not proof of native rollback or exclusion.

The existing actual PCI/VMM-source fixture retains the original4KiB mapping
boundary cases. It also constructs the proposed large-leaf layout in simulated
RAM, verifies every translation including each4KiB end byte, exact effective
permissions/cache flags and original registry behavior, then runs the unchanged
census: three reachable tables,1536 entries per pass. Actual checked user-root
preparation and kernel refresh preserve the mapping and lower user half; changed
large-page PAT/cache bits refuse. Port operations abort if reached. ASan/UBSan,
cache/core/adapter/decoder, alias/layout, PCI and VMM tests pass locally.

```sh
make test-haswell-ecam-proposal
python3 tools/haswell_ecam_mapping_proposal.py --exit-log /absolute/path/accepted-cache-exit.log --mcfg /absolute/path/MCFG.dat --output /absolute/path/proposal.json
```

## Actual backend and pre-PCI gate implementation

`kernel/src/memory/ecam_boot.inc` is compiled with the AP-only VMM variant.
Prepare validates current sole-root IRQ exclusion, admitted RAM/HHDM walks,
complete borrowed root/PDPT words, exact-zero slot and registry snapshot. It
allocates one checked PAGE_TABLE frame, stages all512 entries privately, rechecks
fresh admission, publishes one ordered atomic parent link and invalidates every
original4KiB VA before reporting READY. Before a clean detached free it also
reverifies the borrowed path and zero slot. Changed/ambiguous admission, ownership,
A/D bits, readback, invalidation or metadata never turns into a free/retry.
Retire verifies unlink/full invalidation before free; handoff preserves the original
39936 registration attempts and128-entry registry behavior, then becomes permanent.
A read-only PMM observation checks both allocation bitmap and frame tag under
saved IRQ exclusion; no allocator policy changes.

`kernel/diagnostics/haswell_ecam_boot.c` supplies a real fresh gate before PCI
inventory. It requires captured Intel40651/39-bit controls, NX/1GiB support, BSP0,
the four-CPU mask and untouched parked AP entrypoints. It reads the exact five
PCI identity words by legacy config access, restoring CF8 after every selection
and never writing config data. Two complete pinned DMAR/catalog captures per
admission phase and the full pinned60-byte MCFG replace historical assumptions.
Both cache snapshots, EFER values and every2MiB leaf's per4KiB uniform UC typing
must agree. Failed fresh admission latches broken and retains any allocated
frame; it cannot be followed by cleanup under the earlier successful check.

The existing49-MSR/16-PTE context caps,100ms sample caps and64-table/32768-entry/
4096-byte census remain unchanged. **Separate new mapper accounting** caps the
whole prepare/retire-or-handoff lifetime at100ms and65536 software RAM words,
including checked HHDM walk words and firmware copies. The larger RAM-word bound
belongs to this new lifetime operation, not the alias census: it includes multiple
full verification/cleanup passes. Fixed invalidation loops cover39936 VAs for
publication and again for retirement; no deadline is restarted between calls.
Fresh gate counters permit at most49 cache MSRs per snapshot, two EFER reads,
20 PCI identity reads/40 restored CF8 selector writes and4096 firmware bytes.
The RAM-copy entry is usable only within an active admission callback and rejects
ECAM/VT-d device pages. IRQ state and all control/cache policy remain unchanged.

Actual-source fixtures exercise4KiB/2MiB/1GiB checked RAM translations and inject
every one of512 detached stores before/after an
ambiguous acknowledgement, parent-store/unlink ambiguity, source failures after
publication/unlink, invalidation/metadata failures, stale/changed borrowed paths,
A-bit/private-page changes, PMM/free failures, exhausted words and stopped/backward/
expired clocks. Successful retirement and permanent handoff also execute the
real backend. The adapter fixture executes its real port-selection/restoration
logic, firmware checks, CPU/AP topology checks, maximum49+49 cache MSRs, two EFER
reads, wrong identity/geometry/RAM/pin and changed-cache refusals. Its synthetic
DMAR pin is explicitly host-only; the native pin remains byte-identical to the
prior exact-unit firmware gate. ASan/UBSan covers both actual-source fixtures.
The new fixture also cross-compiles to ARM64 using a host-neutral topology input;
its native Limine projection keeps the same early refusal checks. Assertions stay
enabled, and unexpected unrelated user-frame routes abort through existing host
hooks. This is compile coverage, not ARM hardware or macOS runtime acceptance.
Callback-return faults are contained by the backend; native CPU exceptions remain
terminal kernel faults, not resumable recovery or proof of hardware rollback.

## Opt-in adoption and dual-file proof (v5)

The normal kernel excludes the mapper and refuses its explicit command-line token.
The AP diagnostic ELF calls prepare and immediate handoff before PCI only for
`--haswell-ecam-boot`, with all four firmware/cache/alias/pre-init flags required.
The ordinary PCI path and v1–v4 retain their mapping behavior. Clean foreign
refusal may follow that path but cannot complete the capture. READY/POISONED,
wrong geometry, changed source or failed adoption stops before PCI/USB; uncertain
frames remain live. A visible terminal screen requests a photograph and Ubuntu
restart. No fallback remap or retry follows an uncertain publication.

PCI consumes ACTIVE exactly once after fresh stable admission, exact geometry,
registry/ownership and complete borrowed-path/private-PD readback. It adopts the
same156MiB window without adding4KiB tables. Adoption shares the original100ms
lifetime and65536 checked software-word cap, with no restarted deadline. Full
raw equality is required throughout this operation; A/D changes also refuse.

After startup the logger is quiesced. The real formatter saves the complete early
root/PDPT observations (unchanged words plus canonical lossless observed deltas),
512 private-PD words, both complete128-row registry vectors, two raw CPU/cache/
EFER/topology/MCFG observations, four full pinned DMAR/catalog observations and
all operation counters into `ecam.log`. Every byte is retained; Base64 chunks have
strict sequence/extent and a33000-byte raw cap. There is no hash-only substitute.
Existing committed-data flush and readback apply. The unchanged pre-user-init
cache/alias census is then reported to `exit.log`; session, epoch, root, frame,
adoption and durable-save reference bind the files. Both must complete.

The decoder requires complete old two-pass raw equality under the unchanged
64-table/32768-entry/4096-byte caps, full cross-file CPU/cache/firmware agreement
and ECAM branch/leaf correspondence. Only the cross-epoch correspondence allows
explicit PCI-induced A/D transitions after adoption; it does not mask either
census pass or relax the mapper's own equality checks. Raw native firmware pins
remain exact. VM fixtures use a synthetic DMAR pin only in host test builds.

Actual-source host tests cover adoption, changed tables/metadata/controls,
duplicate consumption, terminal PCI refusal, log quiescence/readback/failure,
real formatter-to-decoder round trips and missing/duplicate/corrupt/cross-session
records. Maximum128-catalog/49-MSR output is46213 bytes; with the16514-byte
postamble allowance it stays within65536. The old proof plus the maximum census
and postamble also fits independently. New and old files keep the same tail cap.
No payload or native acceptance follows from these host fixtures. Exact-stage
VM, recovery/install/one-shot and matching collection are required before a
native result. Native whole-graph fit and actual VT-d resource/alias/ownership
admission remain separate and unaccepted; no VT-d data load is provided here.


## First native v5 outcome

The first native v5 trial (`56308db224d9e6ab`, implementation `2f53e475`) halted
before PCI/USB continuation during checked adoption: retained-poison state5,
35949 words and one parent attempt. No dual-file receipt was produced. The
word cap was not exhausted; the photograph lacks elapsed time and first failed
check/readback, so the cause is unresolved. All prior USB files and offline
allocation integrity verify on return. This is a failed native attempt, not
adoption or complete-root acceptance. Preserve strict comparisons and limits;
add bounded first-failure and timing observations before another physical trial.


## Bounded adoption failure diagnostics

The AP helper now latches the first failing stage/reason, checked-word count,
existing clock sample and available exact expected/observed values. Adoption
identifies geometry, sole-root/ownership, fresh gate, registry/count, complete
root/PDPT/PD readback and final progress checks. Read primitives identify bounded
RAM/HHDM sources, word-cap and clock failures. The stable adapter separately
latches its first CPU/control/catalog/MCFG/DMAR failure and the exact changed
component/byte where available. Secondary cleanup/refusal cannot overwrite it.
All comparisons, source permissions, ordering and limits remain unchanged.

The existing terminal screen prints these fixed-size records after uncertainty
has already stopped PCI/USB continuation. Detail flags mean1=expected/observed
pair,2=table/index,4=observed-only data; a zero flag means values are unavailable,
not equal. Prepared/handoff/entry/last-good clocks come from existing operation
samples, with no new timer read in the successful critical path. A rejected
clock sample is retained separately; backward/invalid/stopped clocks cannot
produce a valid elapsed claim. Display time is sampled once after failure and
labelled separately; it never restarts the operation or its100ms deadline.
No hardware reread, free, retry, fallback remap or policy change is added.
Native CPU exceptions remain terminal; this is branch-failure instrumentation,
not resumable fault handling or proof of rollback.

Actual-source regressions check first-failure preservation after poisoned retry,
raw A/D differences (still rejected), registry/ownership/geometry/source faults,
cap/clock failures and fresh-gate changed-byte/component failures. The real
terminal path executes under fixture hardware, halts via a host test hook and
asserts zero table rereads/writes/frees and only one later display-clock sample.
Backward-clock and deadline-expiry output are distinguished. Existing normal
and maximum-catalog v5 success receipts remain byte-identical in the fixture;
the raw wire schema, dual-file checks and all old census/context limits stay
unchanged. Host/VM checks do not identify or fix the earlier native adoption cause.


## Instrumented native return

The subsequent instrumented release `a85cf3e1549a1928` saved a complete valid
early raw component and matching adoption/save reference:36268 words,98684us
within100000us,37+37 cache MSRs and the pinned42-row catalog. The prior adoption
halt did not reproduce; its cause remains unknown and timing margin is only
1316us on this observation. The later census still exhausted64 tables after
5678 first-pass/zero second-pass reads, with945 partial encoded bytes. Actual
whole-trial completion is NO and the strict dual-file proof remains absent.
Do not promote the early component to complete-root or ownership acceptance.

Exact staged ELF inspection shows115.160MiB BSS, of which the static process
array is104.375MiB with embedded mapping storage. This motivates a source and
native-branch audit of static kernel/HHDM mapping pressure and deferred backing
while preserving capacity/ownership/integrity checks. The saved partial graph is
insufficient to prove attribution. The original ECAM-only graph estimate was
not a complete native-layout result; its successful compact subtree does not
make the remaining graph fit. Keep the census/lifetime limits unchanged and
also investigate startup timing headroom before another implementation/trial.


## Unique-span registry bookkeeping

The checked handoff keeps39936 attempts and fault hooks, capacity128 and complete
pre/post registry verification. Prepare excludes every old registry VA from the
whole ECAM span, handoff revalidates the snapshot and the new VAs increase
strictly. No new VA can update an old or previously appended row. The private
handoff therefore appends the available prefix directly and leaves its full
registry tail unchanged, instead of searching128 rows for every provable no-op.
Each attempt also checks the expected count. All original root/clock checks,
full invalidation, post-readback and strict comparisons remain, with no deadline
restart. Generic PCI registration is unchanged. Real-source fixtures compare
every initial count0–128, including unordered/duplicate old VAs outside the span,
and retain failure/poison coverage. Native timing improvement still needs a
matching hardware receipt.


### Census measurement first-touch preparation

The opt-in RAM-only alias census now primes its private result backing and
performs one bounded guarded graph preparation before its original two exact
snapshots. Loading a later table through HHDM can set A in an earlier mapping;
preparation exercises those source paths before observations. It uses the same
RAM-catalog, sole-root and exact-translation accessor, no PTE/device writes.
Preparation and both measured snapshots share the original100ms lifetime with
no clock restart or failed-pass retry. Each traversal retains64 tables and
32768 reads; measured snapshots retain4096 lossless bytes each. Preparation
is additional read-only work, separately counted, not extra accepted raw proof.
Graph expansion after preparation fails closed. Every measured entry, including
A/D bits, must still compare exactly. The preparation receipt shares the census
begin timestamp and records its own completion, read and table counts; the
collector validates it when present while retaining old saved-report compatibility.
Native timing/stability require a fresh complete matching capture.
