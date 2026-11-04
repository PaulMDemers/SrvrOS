# Air6,1 read-only DMA/resource inventory

This 2026-11-04 inventory is from the actual Ubuntu MacBookAir6,1 with GPU
`8086:0a26`, revision09, subsystem `106b:011a`. It reads exported firmware and
kernel catalogs only. No VT-d register, root/context table, `/dev/mem`, PCI write,
firmware method or driver-control operation was used. No native SrvrOS DMA
translation state, pin, private GPU mapping or hardware ownership is accepted.

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

## Next bounded native prerequisite

A future isolated **firmware-only** SrvrOS capture should retain the complete
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
refuse. Table bytes require exactly one enclosing RESERVED/ACPI-reclaim/NVS
entry and exact existing kernel translations before reads. No new mappings,
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
