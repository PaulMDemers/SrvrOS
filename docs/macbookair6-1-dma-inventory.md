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
are required before installation.

### Native cache/context observation (2026-11-05 closeout)

PR376 release `058606de6d8652b2`, compiled source `57c6b416`, completed session
`session-00065f7f1ab4e407-00` and returned automatically. Both complete raw CPU
contexts agree: PAT=`0x0000010500070406`, MTRRCAP=`0xd0a`, DEF_TYPE=`0xc06`,
CR0=`0x80010033`, CR3=`0x8cf06000`, CR4=`0x620`, HHDM=`0xffff800000000000`.
There are10 variable pairs and11 fixed registers,37 MSR reads per snapshot and
eight PTE reads per context. Whole contexts take63/58us, including54/53us MSR
brackets; their gap is7us. All remain within the original caps.

All eight raw before/after walks agree. Both the candidate HHDM address for
`0xfed90000` and private `0xffffc001f3250000` are absent at the second level.
The observed enabled variable MTRR covering the target is UC; hypothetical PAT
indices0/2/3 classify as effective UC under this captured context. This is not
an observed mapped leaf, an exhaustive alias census or permission to create one.
Both bracketing firmware captures retain matching pinned136-byte DMAR copies,
42 complete catalog rows and the same native resource gap. The complete21,839-byte
exit receipt survives directly; no native VT-d data load, mapping or write occurred.

All598 prior USB files are preserved except expected metadata replacement; two
new logs yield600 files/1670 allocated clusters. Offline fsck, allocation and
headroom checks pass with zero leaks/mismatches/errors. Independent payload and
511 automatically archived evidence files verify; Ubuntu default, consumed
one-shot and retained console recovery verify. Raw evidence remains local and
durable outside build. This accepts only the bounded read-only context capture.

Before any VT-d window, close the resource-reservation and relevant CPU-alias/
leaf-admission proof against these exact observations and current VMM behavior.
Then require the separate bounded read/retirement contract above; no hardware
ownership, native DMA translation state or full-path swizzle follows from this
capture. Earlier RC6 timing failure and zero package-C3 residency remain open.

## VT-d resource and CPU-alias admission audit (2026-11-05)

Source baseline `379d6f8d`; accepted cache receipt remains source `57c6b416`.
The host-only `tools/haswell_vtd_admission_audit.py` consumes the full raw receipt
through the strict cache/firmware decoders. It models the prospective4KiB leaf
and reports live admission as blocked. Saved observations never supply a live
lease, complete alias census, EFER state or mapping lifetime. Unrelated appended
claims and synthetic/foreign firmware cannot promote a receipt into permission.

### Resource exclusion is distinct from an MMIO lease

`kernel/src/memory/pmm.c:pmm_init` initializes the bitmap/tags as used/reserved,
then frees only catalog USABLE entries; `pmm_free_frame_range` refuses reserved
frames. The captured hole therefore supplies no ordinary allocatable RAM page.
This explains allocator exclusion, without proving the current bitmap from a
historical receipt. PMM has no exclusive MMIO-owner/token API. The pinned DMAR
declaration and exact gap edges identify this unit's candidate page but establish
neither competing-client exclusion nor firmware/DMA ownership. The other DRHD
page remains excluded. PR301 `dc569037` adds independent Intel GPU boot-client
exclusion; it is not a VT-d physical-page lease and is not imported here.

An eventual diagnostic must keep a boot-scoped exact-page/window state with
single admission and terminal poison on uncertain publication/retirement. All
relevant callers must participate, or the test must run under explicit verified
boot exclusion before competing clients. A token local to an adapter alone is
not evidence that arbitrary existing mappers were excluded. No resource claim
mechanism is added by this audit.

### VMM lists and point lookups cannot enumerate aliases

`kernel/src/memory/vmm.c:vmm_init` adopts inherited CR3. Its128-entry
`kernel_mappings` list records only selected public kernel mappings; it does
not enumerate the bootloader tree or `vmm_map_page_in_space` mappings. The latter
uses strict absent-leaf admission, while `vmm_ensure_page_in_space` can accept an
existing PA without checking requested attributes. Neither a successful PA
lookup nor this list is an alias or cache-permission proof.

`vmm_prepare_user_kernel_mappings` and `vmm_refresh_kernel_mappings` copy upper
PML4 entries into other roots, potentially sharing lower tables. A private
in-space call does not by itself isolate a kernel VA from other address spaces.
Any census must prove its relevant-root and CPU coverage, retain raw4KiB/2MiB/
1GiB leaf encodings and include inherited/private mappings under stable root and
mapping-mutation exclusion. Missing coverage, unknown RAM, unsupported entries,
changed paths or exhausted bounds must refuse. The current two-VA capture is
intentionally incomplete; its16-PTE/context limit is unchanged and it must not
be relabeled as a complete census. No alias is removed or retyped to obtain a pass.

### Prospective leaf and retirement checks

Intel SDM Vol3A sections4.5/4.6,12.5.2.2 and12.12.3 distinguish leaf encoding,
effective memory type and enabled execute protection. A prospective fresh PTE
`0x80000000fed90019` selects4KiB PAT index3 using PWT/PCD, sets present/XD and
clears writable/user/global/PAT-leaf bits. Under the captured PAT/MTRR/CR0 state,
that encoding classifies UC and CR0.WP is set. This is an encoding model, not an
observed device leaf. The host model rejects every other PA/permission/cache/
reserved encoding, missing WP, non-UC and undefined MTRR overlap.

The cache wire format does not capture CPUID NX support or IA32_EFER.NXE.
Setting an XD entry requires the latter enabled; an NX-looking bit alone is
insufficient. Require fresh read-only feature/EFER observations and raw path
permission/cache checks before a separately authorized mapper, preserving the
existing snapshot limits and version compatibility. An old receipt never proves
current state. No EFER write or cache-policy change is proposed.

`vmm_map_page_in_space` may leave allocated intermediate tables on failure;
`vmm_unmap_page_in_space` clears a4KiB leaf and invalidates locally only when the
specified root is current. These functions alone prove neither expected leaf
ownership nor remote/global TLB retirement. Use exact raw prevalidation, a
current kernel root, supervisor RO/NX/non-global leaves, CPU/mutation exclusion,
verified clear and local invalidation before reuse. Quarantine a possibly
published window on failure; never free MMIO or borrowed CPU tables. Generic
ensure/global-map APIs do not substitute for these checks.

Next is a separately scoped bounded read-only relevant-root/alias and NX-state
diagnostic, with explicit completeness/refusal evidence and fresh guards. It must
not silently widen the16-PTE context capture or convert its known aliases into
global coverage. The read sequence and100ms/two-pass/retirement contract above
remain unchanged; actual VT-d reads and hardware ownership stay unimplemented.

## Separate bounded alias/NX diagnostic candidate

Add `--haswell-alias-context` to the explicit firmware and cache-context stage
flags for `ram-usb-v3-hfw`. Versions1/2 are unchanged, including their49-MSR and
16-PTE CPU-context caps. Version3 inserts a separate read-only census between
those cache contexts, under the same fresh pinned firmware/catalog brackets.
No VT-d target/alias data, device table, new mapping or control write is used.

The VMM observational sole-root guard requires CPU0 IRQ exclusion, current
untagged kernel CR3, matching kernel-root pointer and no registered nonkernel
root or stale used root record. It allocates and writes nothing and grants no
lease or SMP/firmware authority. Normal profile dispatch launches no AP test;
staging refuses AP, battery, graphics or other unrelated modifiers. Other roots
refuse instead of being ignored. Upper-half sharing cannot bypass this guard.

The distinct census visits the full reachable four-level CPU RAM-table graph,
including inherited/private mappings, rather than the public mapping list.
It caps unique tables at64 and loads at32768 entries per pass, with two exact
passes and100ms total. Four-KiB,2MiB and1GiB leaves are recognized, shared tables
at the same level are retained once, and cross-level cycles, physical/reserved/
large-leaf errors, unknown RAM, data changes and any exhausted bound refuse.
Both declared device pages are forbidden as CPU table sources. No leaf's target
is dereferenced. Exact translation checks precede each CPU table entry load.

Raw tables are encoded losslessly into at most4096 bytes per pass. Each page is
`<QB>` physical address/level followed by `<HQQ>` count/first/stride runs whose
counts sum to512. Values advance modulo2^64, so zeros and arithmetic mapping
runs compress without dropping any bit, including nonpresent/A/D/software bits.
An incompressible graph refuses; it never falls back to a hash or sparse subset.
Both pass payloads are emitted in512-byte Base64 chunks and must decode to
identical raw words. The independent decoder verifies all RAM sources, full graph
closure, leaf decoding and every overlapping point-walk record. Coverage means
this sole managed root's CPU page-table graph, not stale TLBs, SMM/firmware roots,
other hardware agents or an exclusive device resource claim.

Read-only CPUID extended-NX/1GiB capabilities and IA32_EFER endpoints bracket
this work; NX/LME/LMA must be enabled and stable, unknown EFER bits refuse.
Unsupported1GiB leaves refuse. Two EFER reads are separate from the unchanged
PAT/MTRR MSR budget. IRQ-enable state must restore on success and every refusal;
step/status records identify source/root/NX/census/retirement failures. No NX,
cache, interrupt-routing, power or hardware policy is changed. Ordinary boot
excludes this opt-in diagnostic.

The guest announces progress, then saves the complete receipt, unmounts and
returns automatically. Completion requires both decoded cache contexts, both raw
census passes, matching firmware/catalog guards and USB integrity; a return or
bounded refusal alone is not acceptance. The unchanged64KiB tail must retain
maximum fixtures plus the audited source postamble. Native result is pending.

### Pre-user-init capture, deferred logging (v4 candidate)

The v3 prototype's late dispatch follows `/init --system`, which creates a user
root. Its sole-root guard must refuse that state; v3 was installed but never
armed or physically tested. Do not weaken the guard or omit registered roots.
The distinct `--haswell-alias-preinit` modifier additionally requires all three
firmware/cache/alias flags and selects `ram-usb-v4-hfw`.

Version4 captures before the first user-root creation, after kernel/device/timer
bootstrap. It saves immutable full firmware/cache/census/NX results without
emitting raw records yet. Normal init and logger startup then proceed unchanged;
the late capture branch reports those results exactly once. Repeated prepare or
report attempts refuse, and a later live sole-root failure cannot alter the saved
capture. The new epoch record encloses all saved firmware/cache observations and
identifies reporting as later than preparation. Scope is explicitly pre-user-init;
no live post-init mapping admission follows. Failure remains failure, and missing
logger/readiness/completion is not accepted.

All existing64-table/32768-entry/4096-byte/100ms census and49-MSR/16-PTE context
limits remain. Versions1–3 and their dispatch stay available unchanged. Tests
must prove no raw receipt before simulated init, one complete deferred receipt
after it, immutable reporting despite a newly registered-root refusal, both raw
passes retained at maximum size, and exact-stage init/logger/return ordering.


### Native v4 result: table-bound refusal (2026-11-05)

Source `81d1013f`, release `6d59e033859033b9`, session
`session-00065f90c3adc353-00` saves the deferred pre-user-init receipt and returns
to Ubuntu, but reports `completed=NO`: census status2/step3,64 tables,
5166 first-pass reads, no second pass and882 encoded bytes. The4698us preparation
is below100ms. The failure cursor (`0x02004000`, index45, raw `0x02032023`)
and source control flow identify an attempted65th unique table, exhausting the
unchanged64-table cap. The complete graph size is unknown. No full-graph alias,
NX endpoint-pair, live admission or mapping permission is accepted.

USB preservation, offline fsck/allocation/headroom and recovery checks pass;
raw failed evidence is durably retained. See the [unit closeout](machines/macbookair6-1.md#native-pre-init-alias-census-refusal-2026-11-05).
Next audit the inherited layout and whether a complete proof can fit existing
bounds. A streaming implementation alone would not remove the32768-entry
per-pass ceiling:65 full tables already require33280 entries. Do not widen
limits or substitute a sparse/partial graph for exhaustive coverage.


## ECAM layout and fixed-bound feasibility audit (2026-11-05)

Base `7b887bbe`, after collecting the failed v4 trial. This is an offline/source
result. No kernel producer, mapping, initialization order, policy or limit changes.
Current open PR301 GPU admission and PR19 Wi-Fi work remain separate; the only
shared build-file addition here is a host-test target.

### Kernel-created mappings already exceed the census

`main` calls `pci_init` before the pre-user-init capture. PCI's
`map_ecam_windows` maps every declared bus window with `vmm_map_page`, one4KiB
page at a time, beginning at `0xffffc00300000000`. VMM's `ensure_next_table`
creates ordinary4KiB tables and refuses a preexisting large leaf; no coalescing
occurs. The kernel inherits CR3, but the capture also includes these kernel-added
mappings. Calling this only an inherited-bootloader footprint problem is incorrect.

The unit's archived60-byte MCFG (SHA256
`0e097004696cfc23012f877d7aeecf5652fe60b5f131a6775e24be8c83fe7abb`)
is checksummed revision1 and declares one segment0 window at `0xe0000000`,
buses0–155 inclusive. The current Linux-exported MCFG is byte-identical.
The native failed trial reports one MCFG allocation and successful ECAM config;
it does not retain raw MCFG or a complete table graph. Therefore the exact
81-page footprint below is a source-derived lower bound using archived firmware,
not a newly observed native raw graph or attribution of its exact failure cursor.

| Quantity | Source-derived minimum |
| --- | --- |
| PCI configuration window |156MiB /39936 distinct4KiB leaf entries |
| Leaf-table pages,512 entries each |78 |
| Upper levels, including the root |3 |
| Full root graph |81 tables /41472 entry loads per pass |
| Existing full-graph bounds |64 tables /32768 entry loads per pass |

Every distinct2MiB virtual region needs a distinct PT because its PTEs map a
different physical range. Other inherited mappings and bootstrap devices can
only increase this minimum. The128-entry public kernel mapping registry is
saturated while all39936 ECAM leaves still map successfully; its count is not
an alias census. The lower bound already exceeds both independent limits.
Streaming tables or improving raw compression alone cannot fix this diagnostic.

### Actual-source regression and reproducible host audit

`tools/test_haswell_alias_ecam.c` includes the actual PCI and VMM source and runs
the unchanged census core over simulated RAM. Port operations abort if reached;
no PCI target is read/written. It verifies every mapped physical address and
exact original P/RW/PCD/NX leaf flags, the saturated mapping registry, full
RAM-table immutability during census, successful two-pass boundary cases and
mandatory overflow refusal. It does not emulate hardware A/D changes or accept
native timing.

| Buses | Complete simulated graph | Census result |
| --- | --- | --- |
|1 |4 tables |two passes,2048 entries each |
|120 |63 tables |two passes,32256 entries each |
|122 |64 tables |two passes,32768 entries each |
|123 |65 tables |refuses at64, no second pass |
|156 |81 tables |refuses at64, no second pass |
|256 |131 tables |refuses at64, no second pass |

`tools/haswell_alias_layout_audit.py` independently parses a complete checksummed
MCFG, rejects unsupported/malformed/overlapping/out-of-width allocations and
computes the source-derived virtual PT-region lower bound. Six Python tests
cover the captured layout, exact boundary, nonzero/odd bus start, multiple windows,
maximum bus range and malformed/overlap/width refusals. Its result is always
`HOST_LAYOUT_AUDIT_ONLY`; even a layout within bounds is not native coverage,
resource reservation, mapping permission or ownership. Run locally with:

```sh
python3 tools/haswell_alias_layout_audit.py --mcfg /absolute/path/MCFG.dat --output /absolute/path/layout.json
make test-haswell-alias-layout
```

### Decision and next design

Do not rerun unchanged v4 expecting success. It cannot produce the required
complete two-pass raw graph within the fixed read/table bounds on the modeled
current ECAM layout. Skipping ECAM or other branches because their intended
physical ranges differ is not a proof of their actual leaf contents; the public
registry, source intent, sparse excerpts and hashes cannot replace required raw
coverage. No table is removed, rewritten, or hidden to make the capture pass.

Next design an explicit ECAM mapping-footprint improvement separately from the
census: this captured window's physical/virtual bases and length are2MiB-aligned,
so a carefully checked large-leaf implementation could remove78 PT pages while
preserving the mapping's physical coverage and effective permissions/cache type.
It needs absent-entry admission, large-leaf PAT-bit handling, precise failure/
rollback behavior and existing-client regressions before any runtime change.
It must refuse conflicting inherited leaves and keep ordinary mapping semantics
and all diagnostic limits intact. A smaller ECAM footprint does not promise the
remaining native graph fits, and does not close resource/leaf/retirement admission.
An earlier pre-PCI capture is a different epoch with narrower scope, not evidence
for the post-bootstrap root; the existing exact-unit gate also depends on PCI
inventory. No such boot-order/profile change is implemented by this audit.

Local actual-source/core/adapter/decoder, PCI-config and VMM tests pass, including
ASan/UBSan on the ECAM fixture. Documentation links and diff checks pass.
Raw MCFG, failure receipt, source hashes and validation logs remain local with
a hash-verified copy outside build. Ubuntu recovery and the installed release
remain unchanged; no reboot is armed and no new physical acceptance is claimed.


## Checked ECAM large-leaf design checkpoint (2026-11-05)

The [separate mapping design](macbookair6-1-ecam-mapping-design.md) now specifies
78 exact2MiB leaves, per4KiB historical cache equivalence and uniform MTRR typing,
detached single-link publication, precise pre-handoff retirement and permanent
boot ownership. Host fault tests and actual-VMM synthetic layout/root checks
pass; no native mapper or publication mechanism is implemented. A fresh pre-PCI
identity/cache/NX gate and actual-source backend fault tests remain required.
No diagnostic cap, kernel mapping, policy, physical acceptance or reboot state
changes. The native v4 table-cap failure remains the current census result.
