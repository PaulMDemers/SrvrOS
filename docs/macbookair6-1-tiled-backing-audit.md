# Air6,1 tiled framebuffer backing audit

Initial source-audit checkpoint: 2026-11-03, base `8f47f196`. The starting-point
sections below preserve that earlier audit. The subsequent isolated pilot and
native results are recorded later in this guide; current accepted version-3
readout is `86257762f155128f` / `169f3808`, following version 2 at
`8dc0947f9c7fbd56` / `1b0d37dd` and version 1 at
`e98aed0c5f8d06cf` / `baaa94b5`. The prerequisite audit uses base `5634cd9d`
and grants no ownership. Policy,
limits, recovery and ordinary startup are unchanged.

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

## Inherited fence/swizzle prerequisite audit (2026-11-03)

Current integration base is `5634cd9d`; native input remains the complete
`baaa94b5` / `e98aed0c5f8d06cf` receipt. The host-only
[path audit](../tools/haswell_path_audit.py) strictly re-decodes that receipt and
never accesses MMIO. Its output cannot grant hardware authority or new physical
acceptance. PR301 at `5287fa13` remains an independent, unmerged Broadwell scope.

The Intel Haswell Memory Views volume, pp24–26, distinguishes CPU/aperture,
display and render/media swizzle controls. The register volume, p503, defines
**both** display bits 14:13: `00` disables display swizzling, `01` enables it,
and the other values are reserved. Native `DISP_ARB_CTL=0x16661056` has field
`00`. The existing bit-13 observation is correct, but interpreting only that
bit would miss reserved value `10`. Host fixtures now cover all four values.
This settles the sampled display-side field, not other access paths.

Pinned Linux's [initial-plane setup](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/i915_initial_plane.c)
sets `preserve_bios_swizzle` for an inherited nonlinear object. Its
[fence/swizzle detector](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/gt/intel_ggtt_fencing.c)
then selects the display field instead of deriving a new policy from DIMM
configuration. Its initializer writes multiple controls when enabling swizzle.
That policy-selection path does not prove firmware configured every access path
consistently, nor authorize copying those writes into this read-only diagnostic.
CPU `TILECTL` and GT `ARB_MODE` remain unobserved. Preserve their full words in
any later experiment; the Linux TILECTL bit-1 prefetch label and the PRM's generic
two-bit swizzle description must not be silently conflated.

The exact [PCI-ID definitions](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/include/drm/intel/pciids.h)
and [device info](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/i915_pci.c)
map `0a26` to Haswell ULT GT3 and the Gen7 feature path. With a native aperture,
Linux consequently models 32 fences, not the Valleyview/Cherryview exceptions.
This is exact-Haswell corroboration, not a copied Broadwell gate. The Intel
volume's explicit 16-row table still disagrees; this audit grants no upper-row
read permission or complete uniqueness. Native FENCE0 covers 16 MiB, larger than
the observed 4,325,376-byte surface. Both ten- and eleven-bit pitch decoders agree
on 5,632 for this word. Neither agreement nor excess coverage proves allocation
size, the unobserved PTEs outside the visible footprint, exclusion or lifetime.

Pinned [uncore access code](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/intel_uncore.c)
uses the render-forcewake table `0..0x3ffff` for non-Valleyview Gen6/7.
`ARB_MODE=0x4030` lies inside it. A Linux-style logical read can therefore
perform wake-control writes; a naked read without ownership is not an equivalent
replacement. Defer GT access rather than infer a valid zero or take forcewake.
`TILECTL=0x101000` lies outside that table and is defined by the pinned register
header. This makes one raw CPU/aperture word the smaller next read-only candidate,
not a blanket always-on assertion or register permission for other offsets.

### Proposed next read-only experiment

Extend only the isolated exact-Air6,1 pilot with one raw `TILECTL` observation
per pass; require agreement and retain full raw values. Keep the complete two
GGTT vectors, endpoint/fence rechecks, existing identity/D0/decode/firmware/RAM
exclusion and poisoned-window retirement rules. Do not read GT `0x4030`, DIMM
controls or extra fences, and do not map backing pixels or take ownership.

Retain the 1,536-PTE cap, 100 ms pass/gap bounds, ten-map ceiling and 64 KiB tail.
One extra page per pass requires ten maps for the native two-PTE-page layout.
A layout needing three PTE pages would require twelve maps and must refuse this
extension; the ceiling must not increase. These two CPU-control observations
would not bracket every PTE access or establish full-path equivalence.

Implementation still needs explicit offset admission, versioned additional raw
receipts, actual-adapter/refusal/retirement tests, independent decoding and a
source-matched maximum-tail proof. Then perform a matched build, exact-stage VM
refusal/save and missing-workspace checks, local USB/recovery preflight and a
new saved run record before any automated native boot. No such boot is staged,
installed or armed by this audit; the accepted payload remains installed.

## Opt-in raw CPU TILECTL capture (version 2)

The isolated pilot now implements the smaller experiment with
`--haswell-backing-capture --haswell-tilectl --laptop-probe-volume UUID`.
The modifier requires the dedicated profile, selects `ram-usb-v2-hsw` and adds
`srvros.hsw-cpu-control=read-only`. The original flag alone preserves version 1.
Normal startup and other diagnostic profiles do not acquire this read permission.

Version 2 admits exactly the aligned DWORD at BAR0+`0x101000` through the existing
kernel-only RO/NX/non-global window. Neighbouring words, GT `0x4030`, extra fences
and backing pixels stay outside the permission. Each pass reads this word after
the final fence recheck, then rechecks the ordinary endpoint and verifies final
window retirement. The two raw CPU words must agree and must not be all ones.
This is a pair of observations, not a continuous swizzle or lifetime guarantee.
No interpretation of TILECTL bit 1 or full-path equivalence is granted.

The unchanged ten-map ceiling is explicit. Before any window mapping, version 2
refuses a derived vector requiring more than two GGTT pages. Native and aligned
maximum-capacity layouts need five page mappings per pass, ten total. The same
1,536-PTE, 100 ms pass/gap and 64 KiB receipt bounds apply. Map/read/clock/change
and poisoned-retirement failures retain their existing refusal behavior.

All summary/metadata/vector lines carry the matching version. Version-2 metadata
is `<50Q16Q3Q5I`, 572 bytes: the version-1 layout plus the raw TILECTL DWORD.
Its window-read count is `PTE count + 97`, and its verified mapping count is
`2*(PTE pages + 3)`. Version-1 records retain their original 568-byte layout and
counts. Mixed versions, missing/changed/raw-all-ones CPU observations, truncated
records and false mapping/read counts refuse. CPU words remain raw-only in the
independent path audit. Actual adapter producers cover both versions and the
1,536-entry maximum, with unchanged raw tail retention checked independently.


## Native version-2 CPU control result (2026-11-03)

Release `8dc0947f9c7fbd56` / source `1b0d37dd` retains two matching raw
`TILECTL=0x00000000` observations alongside complete matching 1,056-entry vectors,
1,153 reads per pass and verified retirement of all ten temporary mappings.
Readout brackets are 383/376 us. The captured display swizzle field is also `00`.
This accepts bounded CPU/display observations; GT `ARB_MODE` remains unread,
upper-fence coverage remains unresolved, and neither full-path consistency nor
real ownership/pinning/lifetime is inferred. No policy or limit changed.
See the [native unit closeout](machines/macbookair6-1.md#native-raw-cpu-tilectl-observation-accepted-2026-11-03)
for exact session, storage integrity and recovery evidence. No new boot is armed.

## Explicit Linux-model 32-fence observation (version 3, 2026-11-04)

The exact `0a26` Linux PCI/device-info chain and pinned fence initializer above
independently justify a separately opted-in read experiment over 32 rows. Its
Gen6+ low/high register macros cover `0x100000..0x1000ff`; rows16–31 stay in the
already admitted fence page, outside the pinned Haswell render-forcewake range.
The Intel16/Linux32 reference discrepancy remains unresolved. This experiment
observes values under Linux's exact-model assumption; reading zero upper rows
cannot prove register existence, an absolute table size or hardware uniqueness.
No Broadwell register/model gate is copied or widened.

Stage with `--haswell-backing-capture --haswell-tilectl --haswell-linux-fences`
and the local `--laptop-probe-volume UUID`. Both earlier flags are required. The
new `ram-usb-v3-hsw` profile adds `srvros.hsw-fences=linux-model-32`; versions1/2
remain sixteen-row profiles. The same single fence page and existing CPU/PTE
windows are used. Exactly32 high/low/high fence pairs are retained and compared
in each pass and rechecked after the full vector. GT `0x4030`, forcewake, extra
page offsets, backing pixels, power policy and assurance bits remain excluded.
The ten-map, 100ms pass/gap, 1536-PTE and 64KiB tail limits remain unchanged.

Version3 summary explicitly says `fences=32 upper=linux-model-32`. Metadata is
`<50Q32Q3Q5I`, 700 bytes; reads equal `PTE count+193`, with the same CPU/raw fields
and mapping formula as version2. Changed/partial/unavailable upper rows, mixed
versions/counts/coverage labels, malformed receipts or uncertain cleanup refuse.
The decoder may report a single candidate **within the Linux model**, but always
retains `unique_fence_proven=false`, unresolved reference count, no pin and no
hardware authority. Overlapping valid rows remove even model-conditional
uniqueness. Neither a matching snapshot nor a conditional candidate establishes
allocation/scanout lifetime, firmware exclusion or full-path consistency.

Actual adapter producers cover all three versions and maximum version3 vectors;
independent decoding and maximum-tail retention apply to the full raw32-row
records. Native acceptance requires a newly matched, staged/VM-tested image,
local recovery/storage preflight and complete saved capture. The accepted
version2 observations remain historical evidence, not advance acceptance of
this new profile. GT and actual ownership/lifetime remain later prerequisites.


## Native version-3 Linux-model32 result (2026-11-04)

Release `86257762f155128f` / source `169f3808` records two matching complete
32-row observations: fence0=`0x00fff02b00000001`, rows1–31 zero. Fence0 remains
the only candidate within the pinned Linux model; zero upper values do not
resolve the Intel16/Linux32 discrepancy or prove global hardware uniqueness.
Two matching 1,056-entry GGTT vectors, raw CPU TILECTL zero twice, 1,249 reads
per pass and ten verified retired mappings complete the read-only receipt.
Pass brackets are396/391 us. No GT access, forcewake, policy or limits changed.

The [unit closeout](machines/macbookair6-1.md#native-linux-model32-fence-observation-accepted-2026-11-04)
records matching session, unchanged prior files, clean allocation and recovery.
This accepts sampled observations only. GT/full-path swizzle, real backing
allocation and scanout lifetime, firmware exclusion and ownership stay open;
the original RC6 timing failure and package-C3 zero are unchanged. The next
bounded step is auditing those remaining prerequisites before any control change.


## Remaining access and lifetime contract (2026-11-04)

The host path audit now consumes the receipt version: version1 still proposes
raw CPU-control observation; version2 identifies the Linux-model32 readout;
version3 stops proposing already completed captures. Its explicit prerequisite
ledger always denies GT read/write admission and native ownership. This is an
evidence report, not an executable hardware permission or new test recipe.

The pinned [uncore implementation](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/intel_uncore.c)
selects the Gen6 render range for Haswell's Gen7 path, encompassing `0x4030`.
Normal reads can acquire forcewake. Haswell's acquisition uses the MT request,
HSW acknowledgement and a GT thread-status wait to avoid a spurious zero read;
release clears the request. Therefore neither a raw zero nor Linux's logical
read API is a no-write consistency proof. Any later acquisition needs its own
exact-model request/ack/thread-status, concurrency, deadline and retirement
contract. No such owner or extra MMIO allowlist is added here.

Pinned [initial-plane adoption](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/i915_initial_plane.c)
creates a preallocated region object, reserves conflicting GGTT space when
relocating, pins a VMA and conditionally pins a fence. Those allocator/lifetime
actions are separate from observing PTEs. Copying this path would also introduce
binding/publication behavior outside our read-only experiment.

The native 16 MiB fence covers a 4,325,376-byte observed surface; the extra
12,451,840 bytes are not a captured allocation. Our vector observes only the
surface footprint. Reading the whole fence would require 4,096 PTEs, beyond
the unchanged 1,536-PTE cap, and would still not establish ownership. Firmware
reservation excludes ordinary RAM allocation, but is not firmware exclusion,
a GGTT lease, a fence pin or proof of inactive DMA. Matching snapshots cannot
replace cache/publication, matched scanout completion or a restore lifetime.

The next bounded implementation should be a host-only inherited-layout and
reservation **proposal** for the exact captured footprint, with collision,
alias, partial-range, stale-generation and uncertain-retirement refusal tests.
It must remain separate from native managed GGTT ranges and grant no assurance
bits. Coordinate any shared reservation mechanism with the actual PR301 API
before integration; its Broadwell DMA readout does not admit Air6,1 registers.
A GT forcewake owner, native DMA/resource binding and real scanout adoption
remain separate work. No new reboot can resolve those software contracts merely
by repeating stable snapshots. Power policy, existing limits and the unresolved
RC6/package-C3 results remain unchanged.


## Host-only exact layout and synthetic reservation proposal (2026-11-04)

[haswell_layout_proposal.py](../tools/haswell_layout_proposal.py) consumes a
strict, complete version3 raw receipt and admits only the captured1366×768,
5632-byte pitch, zero-offset,1056-page shape with matching raw CPU/display
controls. It requires a sole Linux-model fence0 candidate, firmware-reserved
stolen candidates and consecutive, nonaliased physical pages. It preserves the
input receipt hash; older/incomplete/overlapping or changed layouts refuse.
This is a proposal, not native allocation or hardware admission.

The report keeps separate intervals: surface GGTT `[0,4325376)`, full protected
fence `[0,16777216)`, and the1056 observed physical pages. All extra fence space
stays protected from simulated GGTT reuse without inventing physical pages or
ownership for it. `native_managed_ranges` is empty and pin/ownership/hardware
and physical-acceptance flags remain false.

The separate `SyntheticReservations` host model exercises half-open GPU ranges
and exact physical-page sets. Partial GPU overlaps and physical aliases refuse
independently; adjacency is allowed. Local object tokens bind a finite catalog,
slot and monotonically increasing generation. Reader tokens are nontransferable;
close blocks new holds/work, drains existing work and retains the range until
all readers release. Uncertain retirement permanently quarantines its ranges
and readers in that catalog; there is no reset/reclaim method. Fresh catalogs
cannot validate old tokens. Eight simulated claims/readers and1536 input pages
bound host exercises only; no native capacity or policy is changed.

PR301 at `a803af45` already has an independent `scanout_space` API: audited
managed imports start unavailable, protected inherited fences override grants,
and exposed leases require restoration/translation/reader proofs. This tool does
not implement or replace that allocator, import its synthetic grants into native
state, or translate a simulation result into its proof mask. Shared integration
must wait for the actual merged API and an independently audited Haswell owner.

Run the proposal against an already collected raw exit log:

```sh
python3 tools/haswell_layout_proposal.py --exit-log /absolute/path/exit.log \
  --output /absolute/path/host-layout-proposal.json
```

`make test-haswell-backing` includes positive proposal replay plus partial
collision, exact-physical alias, aliased-but-valid receipt, full-fence protection,
foreign/forged/stale tokens, reader retention, slot reuse, capacity and quarantine
regressions. A completed simulated lifetime says nothing about device readers.
No MMIO, PTE publication, native reservation, firmware exclusion or hardware
completion callback exists here. Before native adoption, the remaining work is
exact-model DMA/resource inventory, exclusive allocation and firmware ownership,
GT-access protocol, publication/invalidation and matched route/restore retirement.
