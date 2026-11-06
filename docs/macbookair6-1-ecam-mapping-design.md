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

## Next integration and native proof

The backend entrypoints are linked only into the AP diagnostic ELF; the ordinary
kernel excludes them. Neither `main` nor PCI enumeration calls them yet. Build
a distinct versioned opt-in pre-PCI dispatch/receipt profile next, including explicit
poison handling, immediate handoff/retire before PCI access, checked PCI-window
adoption that skips the old4KiB mapper only for the admitted active window, all raw fresh proofs,
strict decoding, tail retention and exact-stage VM coverage. Do not silently
change v1–v4 or claim their existing receipts exercise this backend.

A native trial then needs the normal exact-stage rehearsal, saved run record,
recovery/install/one-shot and matching return/USB integrity sequence. A three-table
synthetic ECAM subtree does not prove the remaining native graph fits. Full-root
coverage, actual VT-d admission and hardware ownership remain separate and
unaccepted. No payload is installed or reboot armed by this implementation.
