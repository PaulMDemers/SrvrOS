# Air6,1 read-only DMA/resource inventory

This 2026-11-04 inventory is from the actual Ubuntu MacBookAir6,1 with GPU
`8086:0a26`, revision09, subsystem `106b:011a`. It reads exported firmware and
kernel catalogs only. No VT-d register, root/context table, `/dev/mem`, PCI write,
firmware method or driver-control operation was used. No native SrvrOS DMA
translation state, pin, private GPU mapping or hardware ownership is accepted.

The [native firmware/catalog capture](#native-firmwarecatalog-observation-accepted-2026-11-04)
has now completed. The Ubuntu inventory and initial proposal below remain as
history; translation-state observation and native ownership remain open.
The read/resource audit below is complete; its cache/alias prerequisite is the
next read-only experiment before any VT-d register mapping.

## Firmware declaration

The exported DMAR is136 bytes, revision1, valid checksum, OEM `APPLE / HSW`.
SHA256 is `aafa826ce50c10388f2c4198de9a429ab51a5573ad1c5d2856ea32ed078b6307`.
ACPICA20251212 disassembly and independent byte/length/checksum/scope checks agree.
The host-address field is38, encoding39 bits; flags are01. These exact bytes
are this unit's evidence, not the independent Broadwell profile's firmware pin.

| Entry | Segment/scope | Declaration |
| --- | --- | --- |
| DRHD0 | segment0, explicit PCI endpoint bus0 path02:00 (`00:02.0`) | base `0xfed90000`, flags0, size0 (one4KiB page) |
| DRHD1 | segment0, include-all flag; IOAPIC/HPET scopes | base `0xfed91000`, size0; no access proposed |
| RMRR | segment0, explicit `00:02.0` endpoint | inclusive `0x8d800000..0x8f9fffff`, half-open `[0x8d800000,0x8fa00000)` |

The structures are independently checked against pinned
[ACPICA/Linux table definitions](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/include/acpi/actbl1.h).
Intel's [VT-d specification4.1](https://cdrdv2-public.intel.com/774206/vt-directed-io-spec%20.pdf),
sections8.3 and11.1, defines discovery and these firmware declarations. A table
identifies resources; it does not report current translation enablement or grant
an exclusive device/firmware owner. No native MMIO permission follows from this
Ubuntu inventory.

## Relation to accepted backing observations

The previously accepted version3 receipt observes the GGTT table candidate at
`[0x8d800000,0x8da00000)`, stolen interval `[0x8da00000,0x8fa00000)` and inherited
surface physical candidates `[0x8da00000,0x8de20000)`. All fit inside the GPU's
RMRR declaration. This corroborates the resource relationship, not an owned RAM
allocation, an identity DMA map or proof that a firmware/device reader retired.
The16MiB inherited fence remains fully protected by the
[host-only layout proposal](macbookair6-1-tiled-backing-audit.md#host-only-exact-layout-and-synthetic-reservation-proposal-2026-11-04).
No physical pages are inferred for the fence's unobserved excess.

## Ubuntu runtime and catalog distinction

Ubuntu kernel `7.0.0-42-generic` binds i915, places only `0000:00:02.0` in IOMMU
group0, reports a translated default domain and active DMAR. Its i915 startup
reports disabling stolen-memory use under active DMAR. These are Ubuntu driver
observations after initialization, not the translation state SrvrOS inherits.
An isolated group is not a no-remapping or firmware-exclusion proof.

GPU sysfs resources show BAR0 `[0xb0000000,0xb0400000)` and aperture
`[0xa0000000,0xb0000000)`, matching the accepted native shape. Ubuntu `/proc/iomem`
labels the two DRHD pages `dmar0`/`dmar1`; the exported firmware memory catalog
contains no enclosing entry for either. Its nearest reserved edges are
`[0xfed1c000,0xfed20000)` and `[0xffe10000,0xffe40000)`. Kernel-created resource
labels are not a native Limine map or a generic permission to map a gap.
Neither these edges nor this unit's RMRR/flags may be replaced by sibling values.

## Initial native capture prerequisite

At the Ubuntu inventory checkpoint, the proposed isolated **firmware-only**
SrvrOS capture was to retain the complete
current DMAR and full current Limine catalog, exact GPU identity, hashes and
resource intersections before considering VT-d reads. It should refuse changed,
malformed, ambiguous or conflicting tables/resources, retain the inherited
backing proof and save/unmount/return automatically. It needs matched host/VM,
retention, recovery and USB validation before installation. No such payload is
implemented, installed or armed by this inventory.

Only after native declaration/catalog validation should an exact Haswell
read-only VT-d protocol be considered. It must separately justify its allowed
register offsets and mapping retirement; never copy PR301's Broadwell gate or
follow a root-table pointer. Translation observations alone still cannot grant
GGTT ownership, change forcewake, publish PTEs, exclude firmware or establish
matched scanout/restore retirement. Existing integrity checks, limits and power
policy, the prior RC6 timing failure and package-C3 zero remain unchanged.

Raw firmware, Ubuntu catalogs and local verification scripts are stored in
ignored evidence with a hash-verified durable copy outside build. Only this
sanitized inventory is committed.


## Opt-in firmware-only native capture candidate

`--haswell-firmware-capture --laptop-probe-volume UUID` selects the separate
`ram-usb-v1-hfw` diagnostic ELF/profile. It omits the ordinary Intel graphics
stub and optional AML/battery/power services. The capture reads PCI identity,
then copies the complete current Limine catalog (1–128 entries) and136-byte DMAR
twice through existing firmware RAM mappings. The whole table must match this
unit's SHA256 pin; both catalogs, table addresses and raw bytes must agree.
Unknown/overlapping/overflowing/partial catalogs and duplicate/malformed tables
refuse. Table bytes require exactly one enclosing ACPI-reclaim/NVS
entry (generic reserved/device ranges refuse) and exact existing kernel translations before reads. No new mappings,
VT-d/GT/GGTT access, firmware method or writer is provided by the capture.

Each copy and inter-copy interval remains within100ms; the128-entry report bound
is specific to this new profile. Other profiles' limits stay unchanged. Two full
raw catalogs and DMAR records plus PCI/address/timing metadata are retained in
the unchanged64KiB tail. Independent decoding recomputes checksum/SHA256,
identity/catalog/timing/coverage and all matching endpoints. It reports resource
intersections, always denying DMA state, mapping permission and ownership.
A refusal is saved as failure/incomplete, never disabled-translation evidence.

The guest shows progress without input and uses ordinary log save, clean USB
unmount and Ubuntu return. Missing workspace returns without capture/save;
exact Q35 rehearsals must refuse identity before firmware copying. Dedicated
core/actual-adapter tests, independent hash/receipt/max-tail checks, matched
builds/VMs and local recovery/storage preflight are required before installation.
Implementation is not native acceptance; the local run record owns install/arm
state and collection. Prior inherited backing evidence remains unchanged.


## Native firmware/catalog observation accepted (2026-11-04)

The firmware-only capture is now physically observed on release
`6b7dbee3a9441281` / source `e53b7a63`. Both complete136-byte DMAR copies match
the unit SHA256 pin and all42 native Limine entries agree. Table address
`0x8cd64000` is inside the ACPI-reclaimable interval
`[0x8cd64000,0x8cd8f000)`. Copy brackets21/16 us and the4us inter-copy interval
satisfy the unchanged bounds. See the [unit closeout](machines/macbookair6-1.md#native-firmwarefull-catalog-observation-accepted-2026-11-04)
for exact session, storage-integrity and recovery evidence.

Both declared DRHD pages have no intersection with the complete native catalog.
Nearest reserved edges are `[0xfed1c000,0xfed20000)` and
`[0xffe10000,0xffe40000)`, matching this unit's earlier Ubuntu catalog observation.
These are observed resource prerequisites, not an enclosing memory-map grant,
a DMA-state readout or proof of exclusive firmware/device ownership. The gap
must not be treated as allocatable RAM or an arbitrary MMIO allowlist.

The next bounded step is a source audit of exact-Haswell VT-d read semantics,
unit-specific resource admission, finite sampling and mapping retirement before
any native register test. Keep this unit's complete DMAR pin, explicit GPU scope
and gap edges independent of PR301's Broadwell profile. No VT-d command/fault
acknowledgement write, root-table traversal, forcewake, private mapping or owner
assurance is granted here. No new test is staged or reboot armed by closeout.

## VT-d read/resource contract audit (2026-11-04)

Base `db95fc9b`; native input remains source `e53b7a63` / release
`6b7dbee3a9441281`. The [host contract audit](../tools/haswell_vtd_read_audit.py)
strictly re-decodes that receipt and produces a proposal with every native
read/map/ownership flag false. It performs no I/O or native admission.

### Primary register reference

Intel [VT-d4.1](https://cdrdv2-public.intel.com/774206/vt-directed-io-spec%20.pdf)
sections8.3,11.1–11.2 and11.4.1–11.4.5 identify the DRHD location/size and allow
aligned DWORD accesses to the selected registers, including halves of64-bit
registers. The candidate offsets are VER0, CAP8/12, ECAP16/20, GSTS28 and
RTADDR32/36 (decimal bytes). GSTS.TES reports enable status. RTADDR is a programmed
address/mode; a read alone does not establish hardware's loaded root table.
No lock-prefixed access or command/fault acknowledgement operation is proposed.
This reference does not imply Haswell supports every4.1 capability: retain raw
VER/CAP/ECAP/root words and interpret only version-supported fields.

### Exact resource candidate

Only the pinned DMAR's explicit segment0 GPU endpoint at `0xfed90000` is in
scope, for one4KiB page. The include-all/IOAPIC/HPET unit at `0xfed91000` stays
excluded. Validate the complete current catalog, rejecting unknown types,
overflow, malformed/duplicate/overlapping entries and every target intersection,
including an enclosing generic reserved entry. Require the exact nearest
reserved edges `[0xfed1c000,0xfed20000)` and `[0xffe10000,0xffe40000)`.
Missing/changed or sibling gap edges refuse. Relevant resource invariants are
exact; unrelated ELF/RAM placement may change only within a fully valid catalog.
This is a proposed unit-specific declaration-plus-gap rule, not a generic gap
mapping API or an already granted resource permission.

### Cache/alias prerequisite is still missing

The [Intel SDM3A](https://cdrdv2-public.intel.com/812386/253668-sdm-vol-3a.pdf),
sections12.5.2.2/table12-7 and12.12.3/table12-11, combines inherited PAT/MTRR and
page-leaf attributes to determine effective memory type. PCD alone selects a
PAT entry; it does not prove strongly uncacheable access. UC-minus can produce
write-combining behavior with an MTRR. No PAT reset value or Ubuntu cache state
may be assumed for SrvrOS.

Current VMM initialization adopts the inherited CR3. Its public translation
query returns physical addresses, not effective cache types or complete leaf
attribute evidence. Before a new VT-d alias, require this Air6,1's feature-gated,
read-only current CPU0 PAT/MTRR/CR0 context and a bounded audit of relevant
existing aliases plus the proposed/actual leaf. Undefined overlaps, unsupported
state, incompatible aliases or inability to establish effective strong UC must
refuse. No MSR, cache-policy or alias mutation is authorized by this audit.
PR301 at `69367853` has an independent architectural cache mechanism with exact
Broadwell address gates; coordinate against its eventual shared interface rather
than copying that unit's PAT values, classifier acceptance or native gates.
The immediate next test should capture cache/alias context without mapping the
VT-d page. The firmware receipt cannot satisfy that missing prerequisite.

### Future sampling and retirement contract

After those prerequisites are established, the proposed finite read sequence is
GSTS, VER, CAP high/low/high, ECAP high/low/high, RTADDR high/low/high, GSTS:
12 DWORD reads per sample, twice. Preserve every raw word; all-ones/unavailable,
torn, changed or inconsistent guards refuse. Matching brackets are observations,
not atomic whole-register or full-interval stability. Never dereference RTADDR.

Keep100ms per-pass and maximum-gap bounds, a20ms minimum gap and one-million
iteration stop for a frozen clock. At most two temporary mappings, one per pass.
Require a previously unmapped private kernel window, current kernel root,
supervisor RO/NX/non-global attributes with proven effective UC, exact page/leaf
checks before reads and fresh full pinned-firmware/catalog/PCI guards each pass.
Verify unmap before reuse. Uncertain retirement quarantines the window until
reboot; no borrowed memory or device-owned storage may be freed to hide failure.
No GT forcewake, translation enable/disable, invalidation or acknowledgement write.

A separately opted-in adapter still needs actual-adapter guard/retirement fault
tests, independent strict raw receipts, maximum retention with the unchanged64KiB
tail, matched builds and exact-stage refusal/save/unmount/return plus missing
workspace rehearsals. Preserve every raw firmware/catalog prerequisite; no
sparse or hash-only substitution. Host proposals and VM fixtures do not accept
native DMA state, firmware exclusion, a pin or scanout/restore ownership.

Eight host regressions cover exact/changed/sibling gap edges, every catalog
intersection type, partial coverage, malformed/duplicate/overflowing catalogs,
forbidden offsets, failed firmware input and nonpromotion of foreign cache
claims. Actual native-receipt replay passes the resource-candidate audit while
all cache, mapping and ownership proofs remain false. No test is staged or
reboot armed by this audit.


## Read-only Air6,1 cache/CPU-page-table context candidate

`--haswell-firmware-capture --haswell-cache-context --laptop-probe-volume UUID`
selects a separate `ram-usb-v2-hfw` profile. Version1 firmware-only behavior and
wire format remain intact. The exact Haswell gate adds GenuineIntel signature
`0x40651`,39-bit physical width, PAT/MTRR/MSR feature checks and normal CR0 caching.
The common architectural core/header/tests are reused byte-identically from
PR301 `e48abf79`; its Broadwell adapter and address/acceptance gates are excluded.

Two bounded CPU0 snapshots retain PAT, MTRRCAP, DEF_TYPE, all supported variable
pairs (at most16),11 fixed registers when supported, and raw CPU/control fields.
The common core caps each snapshot at49 MSR reads/100ms, rechecks endpoints and
refuses changed/unsupported/invalid state. There are no MSR/cache-policy writes.

CPU page-table walks inspect the known HHDM candidate for physical `0xfed90000`
and proposed private VA `0xffffc001f3250000`. Only CPU page-table RAM is read;
no target/alias data is dereferenced and no VT-d root table is traversed. Four
levels, at most16 PTE reads per context, complete raw before/after paths and
100ms whole-context bounds are explicit. Existing root/CR4/HHDM state must remain
exact; LA57, unknown table RAM, invalid physical bits, occupied private window,
wrong HHDM translation or any raw path change refuse.4KiB/2MiB/1GiB leaves are
preserved and independently decoded without treating a large leaf's PAT bit
as a physical-address bit. No PTE, control or mapping is written.

Fresh complete pinned firmware/catalog/PCI snapshots bracket the two contexts.
Every raw firmware, CPU and walk record must survive the unchanged64KiB tail.
The host decoder independently checks shapes, bounds, identities, masks, raw
agreement, ordering and RAM-only walk sources, and reports hypothetical cache
types plus any observed HHDM leaf type. An undefined MTRR overlap remains
unresolved. These observations are not an exhaustive alias census, a reservation,
a VT-d mapping permission or hardware ownership. All such grants stay false.

Actual adapter fixtures include a49-MSR/16-PTE/128-catalog-row maximum and refusal
for wrong roots/CPU/mode, non-RAM next tables, occupied windows, MSR failures and
changed raw paths. Host fixtures are synthetic and raw native firmware remains
local. Matched builds, sanitizer/max-tail proofs, exact-stage identity-refusal
and missing-workspace VM rehearsals, recovery/USB checks and a saved run record
are required before installation. Native cache/context acceptance remains pending.
