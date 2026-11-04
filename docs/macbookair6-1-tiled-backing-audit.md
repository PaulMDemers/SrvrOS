# Air6,1 tiled framebuffer backing audit

Audit checkpoint: 2026-11-03, integration base `8f47f196`. This is source analysis
and executable host arithmetic, not a new native capture, adapter or ownership
implementation. Installed release `9b485ca12b4c2482` / source `728bcbeb` remains
the last observed readout. Its underlying backing and seven owner assurances
remain unknown. Policy, limits, recovery and ordinary startup are unchanged.

## Captured starting point

The matching version-9 final file has two stable primary-A endpoints: control
`0x98004400`, pitch 5,632, zero surface/live surface and offsets, and 1366×768
RGB32 geometry. The CPU framebuffer spans 4,325,376 bytes at aperture address
`0xa0000000`, translating through 1,056 contiguous CPU pages with firmware
FRAMEBUFFER coverage. Those CPU addresses are not the GGTT's underlying RAM
addresses. No fence, GGC or GGTT PTE vector has yet been captured by SrvrOS on
this unit. See the [native result](machines/macbookair6-1.md#native-primary-a-and-inherited-backing-readout-2026-11-03).

## Generation-specific contract

[PR301](https://github.com/PaulMDemers/SrvrOS/pull/301) owns the shared inherited
backing capture and an exact Air7,2 adapter. The inspected head is `b576d4b3`;
it is not an Air6,1 dependency already integrated into this baseline. Its
mechanism can inform the next adapter, but its gates/encodings cannot be copied.

| Field | Air6,1 Haswell | Air7,2 Broadwell in PR301 |
| --- | --- | --- |
| GPU / subsystem | `0a26` / `106b:011a` | `1626` / `106b:011b` |
| BAR0 extent | 4 MiB | 16 MiB |
| GGTT alias in BAR0 | +2 MiB | +8 MiB |
| Raw PTE width | 4 bytes | 8 bytes |
| GGC table-size field | bits 9:8, numeric MiB | bits 7:6, exponent-based MiB |
| GGC stolen-size field | bits 7:3 | bits 15:8 |
| PTE bit 11 | Cacheability control bit 3 | Not the Haswell encoding |
| Firmware/PCI/well gates | Independent Air6,1 evidence required | Existing Air7,2 gate remains exact |

The pinned [Linux GGTT code](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/gt/intel_ggtt.c)
and [GGC definitions](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/include/drm/intel/i915_drm.h)
separate these layouts. Preserve raw host/GPU config before interpreting either
stolen or table intervals; no current Air6,1 native GGC value is supplied by this
audit. Numeric size candidates are not validation of reserved encodings or RAM
reservation.

The Haswell [Memory Views PRM](https://kiwitree.net/~lina/intel-gfx-docs/prm/hsw/intel-gfx-prm-osrc-hsw-memory-views.pdf),
pp35–36 and 46, defines a DWORD GGTT entry. Address bits 38:32 come from entry
bits 10:4; entry bit 11 is cache control, alongside bits 3:1. For a present entry,
the address candidate is `(raw & 0xfffff000) | ((raw & 0x7f0) << 28)`.
A generic pre-Haswell `0xff0` extension would turn cache bit 11 into a false
physical bit 39. Keep all four cache bits and the original raw entry. Absent or
ambiguous all-ones entries cannot establish backing identity. The pinned
[Haswell encoder definitions](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/gt/intel_gtt.h)
corroborate this distinction; do not blindly reuse a generic decode helper.

## Fence and swizzle unknowns

The [Intel Haswell register PRM](https://cdrdv2-public.intel.com/690873/intel-gfx-prm-osrc-hsw-commandreference-registers_0.pdf),
pp858–862, lists FENCE_0–15 from `0x100000`, lower/upper inclusive 4 KiB page
bounds, X/Y walk and a ten-bit pitch in 128-byte units. Its memory overview also
says sixteen. Pinned [Linux fence code](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/gt/intel_ggtt_fencing.c)
selects 32 for Gen7+ and its Gen7 pitch capacity is wider. The first 16 documented
rows cannot establish uniqueness across an unobserved upper table. Preserve the
count/pitch reference discrepancy; independently justify any expanded native
allowlist instead of treating an Air7,2 fixture as admission. This Ubuntu host
exposed neither of the checked fence-debug metadata files; that absence resolves
nothing.

A candidate X fence must cover the entire tile-aligned surface, agree with its
pitch and avoid any competing valid overlap. Disabled or Y fences, wrong pitch,
reserved/ambiguous fields, partial coverage and duplicate candidates cannot give
a unique mapping. Even a complete unique candidate is not an owner or pin.

Unlike Broadwell, Haswell's inherited swizzle state cannot be assumed absent.
The same Linux source distinguishes preserved BIOS swizzle from driver-selected
DRAM configuration and changes several controls during initialization. Copying
that initializer would violate this read-only scope. A future exact readout must
preserve relevant current controls (including the display-arbitration swizzle
bit) without enabling swizzle, forcewake, mappings of backing data or firmware
transactions. Until the control chain is captured and interpreted, CPU-detiled
view equivalence remains unknown even when GGTT entries identify RAM candidates.

## Executable host audit and bounded next capture

[haswell_backing_audit.py](../tools/haswell_backing_audit.py) derives a proposed
extent from the saved version-9 JSON and exercises separate Haswell encodings.
Its output always says host audit only and grants no hardware authority. Its
synthetic fence/PTE fixtures are not native observations.

For the captured X-tiled layout, rounding height to eight rows leaves 768 rows,
4,325,376 bytes / 1,056 GGTT entries. A proposed full vector occupies 4,224 raw
bytes and addresses BAR0-relative `0x200000` through `0x20107f`. These are alias
locations for PTE reads, not backing data to map. A 769-row fixture needs 776
padded rows / 1,067 entries; reading only the visible descriptor span would miss
part of the tiled footprint.

The next implementation should extend or factor PR301's read-only mechanism
with a separately exact Haswell adapter/record, after its shared contract is
integrated or explicitly coordinated. Do not merge a competing generic driver
or broaden its Broadwell gate. The concrete capture contract is:

1. Validate this unit's exact GPU/PCH/subsystem/firmware, existing D0/decode,
   4 MiB BAR0 and aperture shape, CPU framebuffer mappings and whole-MMIO RAM
   exclusion. Read and retain host/GPU GGC, stolen/table bounds and swizzle
   controls; refuse uncertain arithmetic or unsupported layout.
2. Admit only the independently justified fence allowlist. Read stable DWORD
   pairs without writes, preserve raw fences and mark incomplete coverage.
3. Read the entire derived GGTT extent with **32-bit** accesses through the
   +2 MiB alias, twice. The proposed shared-profile cap is 1,536 entries; this
   does not change the existing 4,096-page descriptor bound. Preserve every raw
   entry, compare every entry and recheck route, fences, config and translations.
   A hash or sparse selection cannot replace the vector.
4. Use the shared finite iteration/clock/map-retirement checks, with an explicit
   backing-readout deadline. Never substitute that deadline for the existing
   20 ms coordinated C3 bound. Failed/retained mappings refuse reuse; no timeout
   permits freeing storage still owned by hardware.
5. Use a separate read-only backing profile/receipt, preserving the unchanged
   64 KiB tail with a source-matched maximum-size test. Do not add an unbudgeted
   vector to version 9's already crowded CPU/platform profile, omit its existing
   proofs or turn its RC6 failure into a pass.
6. Rehearse exact-stage refusal/persistence and missing-workspace failure, verify
   this machine's USB/recovery and durable records, then run automatically with
   visible progress, saved complete vectors and Ubuntu return/collection.

Host fixtures cover captured extent/padding/capacity, GGC generation separation,
all address/cache bits, Gen8-width refusal, partial/overlapping/ambiguous fences
and unsupported layout. They neither implement the hardware adapter nor verify
mapping ownership, new integration's native behavior, RC6 timing or package C3.
No new physical run is prepared or armed by this source audit.

## Isolated Haswell pilot implementation

The first native adapter is now implemented in the existing Air6,1 isolated AP
ELF, independently of PR301's still-open Broadwell driver/lifetime work. This
uses the same bounded endpoint/map/unmap/read/clock contract, without changing
that driver's gates or introducing a generic display backend. Future common
factoring follows the integrated shared interface. No parent PR is silently
merged or copied into this device branch.

Stage `--haswell-backing-capture --laptop-probe-volume UUID` after a matched
ordinary/AP build. This separate profile uses `srvros.capture=haswell-backing`,
a RAM boot filesystem and the existing saved-result/clean-unmount/Ubuntu-return
flow. The ordinary Intel stub is skipped for this explicit read-only profile;
existing D0/decode is required. No AML, battery, audio, CPU-policy or desktop
startup is enabled. The diagnostic ELF is required; an ordinary image refuses.

The exact Haswell GPU/class-revision/subsystem, host/PCH, D0/command and captured
4 MiB BAR shape are checked before dependent reads. The firmware profile and
full MMIO-range exclusion must pass before binding existing read-only resource
pages. One new kernel-space-only supervisor/RO/NX/cache-disabled window is
excluded from the global mapping registry. Its address must be unoccupied;
only documented lower-fence and derived GGTT pages are admitted. Every page
switch retires the prior mapping, and final retirement verifies physical
identity and absence. Any uncertain mapping/retirement poisons reuse until reset.
Existing diagnostic resource pages remain retained until reset as before.

Each of two finite passes preserves raw host/GPU GGC and data/table/top bounds,
PCI identity/access, descriptor/pages/firmware coverage, the nine primary-A
controls, four well words and display-arbitration swizzle control. All fields
must match the endpoint recheck and the second pass. Only FENCE0–15 are admitted
from the documented Haswell range, with high/low/high reads and a second full
fence check after the PTE vector. The complete derived **DWORD** PTE vector is
read and retained for both passes, with per-word comparison, present/stolen
candidate counts and a recomputed little-endian FNV identity. PTE bit 11 stays
cache control. A candidate stolen interval is not an allocator pin; missing
firmware reservation remains explicit.

A 1,536-entry cap and independent 100 ms pass/gap bounds apply to this new
readout. Clock rollback/freeze, changed endpoints/PTEs, partial reads or failed
retirement refuse. Finite iterations do not depend on a ticking clock to finish.
The existing 20 ms C3-round bound, 32 attempts, fallback, sample capacities and
all integrity checks remain unchanged. This separate test does not rerun or
accept the original RC6/package-C3 trial.

Records use a strict `hsw-backing: v=1` summary and two metadata/vector pairs.
Metadata is a canonical base64 encoding of 568 little-endian bytes:
`<50Q16Q3Q4I` (50 endpoint words in the header's field order, 16 raw fences,
begin/end/hash, then window-read/count/present/stolen counters). Each vector
contains exactly its reported count of little-endian 32-bit raw entries. The
host requires complete vectors, byte-for-byte pass agreement, exact hashes and
counts, endpoints, clock bounds, clean window retirement and completion. Missing,
duplicate, truncated, noncanonical and false-authority records refuse.

The 64 KiB tail is unchanged. The dedicated profile keeps CPU power disabled,
so no unbounded worker activity report follows its receipt. Source-derived
maximum I/O-owner/gate/cost/device/save reports are checked against a 32 KiB
post-receipt allowance. A maximum-vector replay reserves that allowance plus
128 KiB pre-final chatter and preserves every raw record. This model is
synthetic; only the matching native saved file can accept the observation.

Both the record and decoder preserve `upper=unknown`, `swizzle=partial` and
`ownership=0`. No full fence uniqueness, complete CPU-view equivalence, firmware
exclusion, GGTT allocation, scanout ownership or well/HDA transition is inferred.

## Native pilot result (2026-11-03)

Release `e98aed0c5f8d06cf` / source `baaa94b5` retains two matching complete
1,056-entry Haswell DWORD vectors, bracketed at 371/368 us. All PTEs are present
and identify consecutive address candidates in firmware-reserved stolen RAM;
eight temporary windows retire without uncertainty. FENCE0 is the only lower
candidate, and the observed display-arbitration swizzle bit is clear. This accepts
bounded read-only observations, not full fence uniqueness, complete swizzle,
CPU-detiled equivalence, allocation lifetime, pinning or real hardware ownership.
See the [unit record](machines/macbookair6-1.md#native-haswell-full-vector-observation-accepted-2026-11-03)
for exact session and storage/recovery checks. The original RC6 timing failure
and package-C3 result are unchanged. Remaining prerequisites require further
source audit/read-only evidence before any writer or adoption can be considered.
